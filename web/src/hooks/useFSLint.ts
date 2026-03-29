import { useState, useEffect, useCallback } from 'react';
import { FSLintModule, FileNode, ValidationResult } from '../types';
import { CollectedItems, resolveCaseInsensitive, mimeMap } from '../utils/file';

/**
 * Custom hook for interacting with the FSLint WASM module.
 * @returns {Object} An object containing the FSLint state and processing functions.
 */
export const useFSLint = () => {
  const [module, setModule] = useState<FSLintModule | null>(null);
  const [output, setOutput] = useState<string>('');
  const [isReady, setIsReady] = useState(false);
  const [isProcessing, setIsProcessing] = useState(false);
  const [currentWorkDir, setCurrentWorkDir] = useState<string | null>(null);
  const [fileTree, setFileTree] = useState<FileNode | null>(null);
  const [validationResult, setValidationResult] = useState<ValidationResult | null>(null);

  useEffect(() => {
    /**
     * Handles incoming messages from the service worker.
     * @param {MessageEvent} event - The message event.
     */
    const handleMessage = (event: MessageEvent) => {
      if (event.data && event.data.type === 'GET_FMU_FILE' && module) {
        const filePath = event.data.path;
        const port = event.ports[0];

        try {
          // The selectedFile path in the explorer is relative to the workDir.
          // The Service Worker sends paths like '/model_validation_.../documentation/index.html'
          // module.FS.readFile expects absolute path in the virtual FS.
          const resolvedPath = resolveCaseInsensitive(module, '', filePath);
          if (!resolvedPath) throw new Error('File not found');

          const data = module.FS.readFile(resolvedPath) as Uint8Array;
          const ext = resolvedPath.split('.').pop()?.toLowerCase() || '';
          const mimeType = mimeMap[ext] || 'application/octet-stream';

          port.postMessage({ data, mimeType });
        } catch (err) {
          port.postMessage({ error: (err as Error).message });
        }
      }
    };

    navigator.serviceWorker.addEventListener('message', handleMessage);
    return () => {
      navigator.serviceWorker.removeEventListener('message', handleMessage);
    };
  }, [module]);

  useEffect(() => {
    const script = document.createElement('script');
    script.src = 'FSLint-cli-wasm.js';
    script.async = true;

    /**
     * Initializes the WASM module when the script is loaded.
     */
    script.onload = async () => {
      try {
        const mod = await window.createFSLintModule({
          /**
           * Callback for standard output.
           * @param {string} text - The text to print.
           */
          print: (text: string) => setOutput((prev) => prev + text + '\n'),
          /**
           * Callback for error output.
           * @param {string} text - The text to print.
           */
          printErr: (text: string) => setOutput((prev) => prev + 'Error: ' + text + '\n'),
          /**
           * Locates the WASM file for Emscripten.
           * @param {string} path - The path to the file.
           * @param {string} prefix - The path prefix.
           * @returns {string} The resolved file path.
           */
          locateFile: (path: string, prefix: string) => {
            if (path.endsWith('.wasm')) return prefix + 'FSLint-cli-wasm.wasm';
            return prefix + path;
          },
        });
        setModule(mod);
        setIsReady(true);
        setOutput('');
      } catch (err) {
        setOutput('Error initializing WASM module: ' + err + '\n');
      }
    };
    document.body.appendChild(script);
  }, []);

  /**
   * Recursively unlinks (deletes) a path from the virtual FS.
   * @param {string} path - The path to delete.
   */
  const recursiveUnlink = useCallback(
    (path: string) => {
      if (!module) return;
      try {
        const stat = module.FS.stat(path);
        if (module.FS.isDir(stat.mode)) {
          const entries = module.FS.readdir(path);
          for (const entry of entries) {
            if (entry !== '.' && entry !== '..') {
              recursiveUnlink(`${path}/${entry}`);
            }
          }
          module.FS.rmdir(path);
        } else {
          module.FS.unlink(path);
        }
      } catch {
        // Ignore
      }
    },
    [module],
  );

  /**
   * Equivalent to `mkdir -p` in the virtual FS.
   * @param {string} fullPath - The path to create.
   */
  const mkdirP = useCallback(
    (fullPath: string) => {
      if (!module) return;
      const parts = fullPath.split('/').filter(Boolean);
      let current = '';
      for (const part of parts) {
        current += '/' + part;
        try {
          const stat = module.FS.stat(current);
          if (!module.FS.isDir(stat.mode)) {
            throw new Error(`Path ${current} exists but is not a directory`);
          }
        } catch (e) {
          const err = e as {
            /** The error number. */
            errno?: number;
            /** The error name. */
            name?: string;
          };
          if (err.errno === 2 || err.name === 'ErrnoError') {
            try {
              module.FS.mkdir(current);
            } catch (me) {
              const mkdirErr = me as {
                /** The error number. */
                errno?: number;
              };
              if (mkdirErr.errno !== 17) throw me;
            }
          } else {
            throw err;
          }
        }
      }
    },
    [module],
  );

  /**
   * Processes the collected files and runs the FSLint validation.
   * @param {CollectedItems | File[]} items - The items to validate.
   * @returns {Promise<void>} A promise that resolves when processing is complete.
   */
  const processItems = useCallback(
    async (items: CollectedItems | File[]) => {
      if (!module || isProcessing) return;

      const collected = Array.isArray(items) ? { files: items, directories: [] } : items;
      if (collected.files.length === 0 && collected.directories.length === 0) return;

      setIsProcessing(true);
      setOutput('');
      setFileTree(null);
      setValidationResult(null);

      if (currentWorkDir) {
        recursiveUnlink(currentWorkDir);
      }

      const timestamp = Date.now();
      const workDir = `/model_validation_${timestamp}`;
      const oldCwd = module.FS.cwd();
      setCurrentWorkDir(workDir);

      try {
        mkdirP(workDir);
        module.FS.chdir(workDir);

        const normalizedFiles = collected.files.map((f) => ({
          file: f,
          relPath: (f.webkitRelativePath || f.name).replace(/\\/g, '/'),
        }));

        let discoveredRootRel = '';
        for (const f of normalizedFiles) {
          if (
            f.relPath.endsWith('modelDescription.xml') ||
            f.relPath.endsWith('SystemStructure.ssd')
          ) {
            const lastSlash = f.relPath.lastIndexOf('/');
            discoveredRootRel = lastSlash === -1 ? '' : f.relPath.substring(0, lastSlash);
            break;
          }
        }

        if (discoveredRootRel === '' && normalizedFiles.length > 1) {
          const firstParts = normalizedFiles[0].relPath.split('/');
          if (firstParts.length > 1) {
            discoveredRootRel = firstParts[0];
          }
        }

        // 1. Create all explicitly discovered directories
        for (const dirPath of collected.directories) {
          mkdirP(`${workDir}/${dirPath}`);
        }

        // 2. Create parent directories and write files
        for (const { file, relPath } of normalizedFiles) {
          const lastSlash = relPath.lastIndexOf('/');
          if (lastSlash !== -1) {
            mkdirP(`${workDir}/${relPath.substring(0, lastSlash)}`);
          }
          const data = new Uint8Array(await file.arrayBuffer());
          module.FS.writeFile(`${workDir}/${relPath}`, data);
        }

        const targetPath =
          discoveredRootRel || (normalizedFiles.length === 1 ? normalizedFiles[0].relPath : '.');
        const target = targetPath === '.' ? workDir : `${workDir}/${targetPath}`;

        const stack = module.stackSave();
        const targetPtr = module.stackAlloc(target.length * 4 + 1);
        module.stringToUTF8(target, targetPtr, target.length * 4 + 1);
        const resultPtr = module._run_validation(targetPtr);
        const resultJson = module.UTF8ToString(resultPtr);
        module.stackRestore(stack);

        try {
          const result = JSON.parse(resultJson) as ValidationResult;
          setValidationResult(result);
          setOutput(result.report);
          if (result.file_tree) {
            setFileTree(result.file_tree);
          }
        } catch (e) {
          console.error('Failed to parse validation result JSON:', e);
          setOutput((prev) => prev + 'Error: Failed to parse validation result JSON\n');
        }
      } catch (err) {
        let errorMessage: string;
        if (err instanceof Error) {
          errorMessage = `${err.name}: ${err.message}\nStack: ${err.stack}`;
        } else if (typeof err === 'object' && err !== null) {
          errorMessage = Object.entries(err)
            .map(([k, v]) => `${k}: ${JSON.stringify(v)}`)
            .join(', ');
        } else {
          errorMessage = String(err);
        }
        setOutput((prev) => prev + 'Error during validation: ' + errorMessage + '\n');
      } finally {
        try {
          module.FS.chdir(oldCwd);
        } catch (e) {
          console.error('Cleanup failed:', e);
        }
        setIsProcessing(false);
      }
    },
    [module, isProcessing, currentWorkDir, recursiveUnlink, mkdirP],
  );

  return {
    module,
    output,
    setOutput,
    isReady,
    isProcessing,
    fileTree,
    setFileTree,
    validationResult,
    processItems,
  };
};
