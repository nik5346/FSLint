import { useState, useEffect, useCallback } from 'react';
import { FSLintModule, FileNode } from '../types';

export const useFSLint = () => {
  const [module, setModule] = useState<FSLintModule | null>(null);
  const [output, setOutput] = useState<string>('');
  const [isReady, setIsReady] = useState(false);
  const [isProcessing, setIsProcessing] = useState(false);
  const [currentWorkDir, setCurrentWorkDir] = useState<string | null>(null);
  const [fileTree, setFileTree] = useState<FileNode | null>(null);

  useEffect(() => {
    const script = document.createElement('script');
    script.src = 'FSLint-cli-wasm.js';
    script.async = true;
    script.onload = async () => {
      try {
        const mod = await window.createFSLintModule({
          print: (text: string) => setOutput((prev) => prev + text + '\n'),
          printErr: (text: string) => setOutput((prev) => prev + 'Error: ' + text + '\n'),
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
          const err = e as { errno?: number; name?: string };
          if (err.errno === 2 || err.name === 'ErrnoError') {
            try {
              module.FS.mkdir(current);
            } catch (me) {
              const mkdirErr = me as { errno?: number };
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

  const getFileTree = useCallback(
    (path: string): FileNode | null => {
      if (!module) return null;
      const stack = module.stackSave();
      const ptr = module.stackAlloc(path.length * 4 + 1);
      module.stringToUTF8(path, ptr, path.length * 4 + 1);
      const jsonPtr = module._get_file_tree_json(ptr);
      const jsonStr = module.UTF8ToString(jsonPtr);
      module.stackRestore(stack);

      try {
        return JSON.parse(jsonStr) as FileNode;
      } catch (e) {
        console.error('Failed to parse file tree JSON:', e);
        return null;
      }
    },
    [module],
  );

  const listVFS = useCallback(
    (path: string, indent = '') => {
      if (!module) return;
      try {
        const entries = module.FS.readdir(path);
        for (const entry of entries) {
          if (entry === '.' || entry === '..') continue;
          const fullPath = path === '/' ? `/${entry}` : `${path}/${entry}`;
          const stat = module.FS.stat(fullPath);
          if (module.FS.isDir(stat.mode)) {
            console.log(`${indent}[DIR] ${entry}`);
            listVFS(fullPath, indent + '  ');
          } else {
            console.log(`${indent}[FILE] ${entry}`);
          }
        }
      } catch (e) {
        console.error(`Failed to list VFS at ${path}:`, e);
      }
    },
    [module],
  );

  const processItems = useCallback(
    async (files: File[]) => {
      if (!module || isProcessing || files.length === 0) return;

      setIsProcessing(true);
      setOutput('');
      setFileTree(null);

      if (currentWorkDir) {
        recursiveUnlink(currentWorkDir);
      }

      const timestamp = Date.now();
      const workDir = `/val_${timestamp}`;
      const oldCwd = module.FS.cwd();
      setCurrentWorkDir(workDir);

      try {
        mkdirP(workDir);
        module.FS.chdir(workDir);

        const normalizedFiles = files.map((f) => ({
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

        for (const { file, relPath } of normalizedFiles) {
          const lastSlash = relPath.lastIndexOf('/');
          if (lastSlash !== -1) {
            mkdirP(`${workDir}/${relPath.substring(0, lastSlash)}`);
          }
          const data = new Uint8Array(await file.arrayBuffer());
          module.FS.writeFile(`${workDir}/${relPath}`, data);
        }

        const isSingleArchive =
          normalizedFiles.length === 1 &&
          (normalizedFiles[0].relPath.toLowerCase().endsWith('.fmu') ||
            normalizedFiles[0].relPath.toLowerCase().endsWith('.ssp'));

        const target =
          discoveredRootRel || (normalizedFiles.length === 1 ? normalizedFiles[0].relPath : '.');

        const targetPtr = module.stackAlloc(target.length * 4 + 1);
        module.stringToUTF8(target, targetPtr, target.length * 4 + 1);
        module._run_validation(targetPtr);

        let rootPath = discoveredRootRel ? `${workDir}/${discoveredRootRel}` : workDir;
        if (isSingleArchive) {
          try {
            const entries = module.FS.readdir(workDir);
            const unpackedDir = entries.find(
              (e) => e.startsWith('model_validation_') || e.startsWith('model_cert_add_'),
            );
            if (unpackedDir) {
              rootPath = `${workDir}/${unpackedDir}`;
            }
          } catch (e) {
            console.error('Failed to find unpacked directory:', e);
          }
        }
        setFileTree(getFileTree(rootPath));
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
    [module, isProcessing, currentWorkDir, recursiveUnlink, mkdirP, getFileTree],
  );

  return {
    module,
    output,
    setOutput,
    isReady,
    isProcessing,
    fileTree,
    setFileTree,
    processItems,
  };
};
