import { useState, useEffect, useRef, useMemo } from 'react';

interface FSLintModule {
  FS: {
    writeFile: (path: string, data: Uint8Array) => void;
    unlink: (path: string) => void;
    mkdir: (path: string) => void;
    rmdir: (path: string) => void;
    readdir: (path: string) => string[];
    stat: (path: string) => { mode: number };
    isFile: (mode: number) => boolean;
    isDir: (mode: number) => boolean;
    chdir: (path: string) => void;
    cwd: () => string;
  };
  callMain: (args: string[]) => void;
}

declare global {
  interface Window {
    createFSLintModule: (config: {
      print: (text: string) => void;
      printErr: (text: string) => void;
      locateFile?: (path: string, prefix: string) => string;
    }) => Promise<FSLintModule>;
  }
}

async function getFilesFromEntry(entry: FileSystemEntry): Promise<File[]> {
  if (entry.isFile) {
    return new Promise((resolve, reject) => {
      (entry as FileSystemFileEntry).file(
        (file) => {
          const path = entry.fullPath.startsWith('/')
            ? entry.fullPath.substring(1)
            : entry.fullPath;
          Object.defineProperty(file, 'webkitRelativePath', {
            value: path,
          });
          resolve([file]);
        },
        (err) => reject(err),
      );
    });
  } else if (entry.isDirectory) {
    const dirReader = (entry as FileSystemDirectoryEntry).createReader();
    const entries = await new Promise<FileSystemEntry[]>((resolve, reject) => {
      const allEntries: FileSystemEntry[] = [];
      const readEntries = () => {
        dirReader.readEntries(
          (results) => {
            if (results.length === 0) {
              resolve(allEntries);
            } else {
              allEntries.push(...results);
              readEntries();
            }
          },
          (err) => reject(err),
        );
      };
      readEntries();
    });
    const files = await Promise.all(entries.map((e) => getFilesFromEntry(e)));
    return files.flat();
  }
  return [];
}

function App() {
  const [module, setModule] = useState<FSLintModule | null>(null);
  const [output, setOutput] = useState<string>('');
  const [isReady, setIsReady] = useState(false);
  const [isProcessing, setIsProcessing] = useState(false);
  const [copied, setCopied] = useState(false);
  const [isDark, setIsDark] = useState(true);
  const outputEndRef = useRef<HTMLPreElement>(null);
  const folderInputRef = useRef<HTMLInputElement>(null);

  const theme = useMemo(
    () => ({
      bg: isDark ? '#1a1a1a' : '#eceef1',
      surface: isDark ? '#2a2a2a' : '#ffffff',
      border: isDark ? '#555' : '#b0b6bf',
      text: isDark ? '#f0f0f0' : '#111418',
      muted: isDark ? '#9a9a9a' : '#555b66',
      termBg: isDark ? '#0d0d0d' : '#dde1e7',
      termText: isDark ? '#f0f0f0' : '#111418',
      termBtnHover: isDark ? '#2a2a2a' : '#c4c8ce',
      iconHover: isDark ? '#2e2e2e' : '#d4d8de',
      buttonBorder: isDark ? '#666' : '#b0b6bf',
      buttonHoverBg: isDark ? '#383838' : '#d4d8de',
    }),
    [isDark],
  );

  useEffect(() => {
    document.body.style.backgroundColor = theme.bg;
    document.body.style.color = theme.text;
  }, [theme.bg, theme.text]);

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

  useEffect(() => {
    if (outputEndRef.current) {
      outputEndRef.current.scrollTop = outputEndRef.current.scrollHeight;
    }
  }, [output]);

  useEffect(() => {
    if (folderInputRef.current) {
      folderInputRef.current.setAttribute('webkitdirectory', '');
      folderInputRef.current.setAttribute('directory', '');
    }
  }, []);

  const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const files = event.target.files;
    if (files && files.length > 0) {
      await processItems(Array.from(files));
    }
    event.target.value = '';
  };

  const onDrop = async (event: React.DragEvent<HTMLElement>) => {
    event.preventDefault();
    const items = event.dataTransfer.items;
    if (items && items.length > 0) {
      const files: File[] = [];
      for (let i = 0; i < items.length; i++) {
        const entry = items[i].webkitGetAsEntry();
        if (entry) {
          const entryFiles = await getFilesFromEntry(entry);
          files.push(...entryFiles);
        }
      }
      if (files.length > 0) {
        await processItems(files);
      }
    }
  };

  const recursiveUnlink = (path: string) => {
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
  };

  const listVFS = (path: string, indent = '') => {
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
  };

  const mkdirP = (fullPath: string) => {
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
          // 2 is ENOENT
          try {
            module.FS.mkdir(current);
          } catch (me) {
            const mkdirErr = me as { errno?: number };
            if (mkdirErr.errno !== 17) throw me; // 17 is EEXIST
          }
        } else {
          throw err;
        }
      }
    }
  };

  const processItems = async (files: File[]) => {
    if (!module || isProcessing || files.length === 0) return;

    setIsProcessing(true);
    setOutput('');

    const timestamp = Date.now();
    const workDir = `/val_${timestamp}`;
    const oldCwd = module.FS.cwd();

    try {
      mkdirP(workDir);
      module.FS.chdir(workDir);

      const normalizedFiles = files.map((f) => ({
        file: f,
        relPath: (f.webkitRelativePath || f.name).replace(/\\/g, '/'),
      }));

      // Root discovery
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

      // If no mandatory file found, use the shortest common prefix
      if (discoveredRootRel === '' && normalizedFiles.length > 1) {
        const firstParts = normalizedFiles[0].relPath.split('/');
        if (firstParts.length > 1) {
          discoveredRootRel = firstParts[0];
        }
      }

      console.log(`Working directory: ${workDir}`);
      console.log(`Discovered root relative path: "${discoveredRootRel}"`);

      for (const { file, relPath } of normalizedFiles) {
        const lastSlash = relPath.lastIndexOf('/');
        if (lastSlash !== -1) {
          mkdirP(`${workDir}/${relPath.substring(0, lastSlash)}`);
        }
        const data = new Uint8Array(await file.arrayBuffer());
        module.FS.writeFile(`${workDir}/${relPath}`, data);
      }

      console.log('Reconstructed VFS structure:');
      listVFS(workDir);

      const target =
        discoveredRootRel || (normalizedFiles.length === 1 ? normalizedFiles[0].relPath : '.');
      console.log(`Executing main with target: "${target}"`);

      module.callMain([target]);
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
        recursiveUnlink(workDir);
      } catch (e) {
        console.error('Cleanup failed:', e);
      }
      setIsProcessing(false);
    }
  };

  const handleCopy = () => {
    navigator.clipboard.writeText(output).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  return (
    <div
      style={{
        position: 'fixed',
        inset: 0,
        display: 'flex',
        flexDirection: 'column',
        padding: '20px',
        boxSizing: 'border-box',
        gap: '20px',
        overflow: 'hidden',
        backgroundColor: theme.bg,
        color: theme.text,
        transition: 'background-color 0.2s, color 0.2s',
        ['--btn-hover-bg' as string]: theme.buttonHoverBg,
        ['--term-btn-hover-bg' as string]: theme.termBtnHover,
      }}
    >
      <style>{`
        .icon-btn {
          background-color: transparent;
          outline: none;
          -webkit-tap-highlight-color: transparent;
        }
        .icon-btn:hover {
          background-color: var(--btn-hover-bg) !important;
        }
        .icon-btn:focus-visible {
          outline: 2px solid var(--btn-hover-bg);
        }
        .icon-btn:not(:hover):not(:active) {
          background-color: transparent !important;
        }
        .copy-btn {
          background-color: transparent;
          outline: none;
          -webkit-tap-highlight-color: transparent;
        }
        .copy-btn:hover:not(:disabled) {
          background-color: var(--term-btn-hover-bg) !important;
          opacity: 1 !important;
        }
        .copy-btn:not(:hover) {
          background-color: transparent !important;
        }
        .copy-btn:focus-visible {
          outline: none;
        }
      `}</style>
      <header
        style={{
          display: 'flex',
          alignItems: 'stretch',
          gap: '20px',
          flexShrink: 0,
        }}
      >
        <img
          src="banner.svg"
          alt="FSLint Banner"
          style={{
            height: '80px',
            width: 'auto',
            flexShrink: 0,
            pointerEvents: 'none',
            filter: isDark ? 'none' : 'invert(1)',
          }}
        />
        <div
          onDragOver={(e) => e.preventDefault()}
          onDrop={onDrop}
          style={{
            border: `2px dashed ${theme.border}`,
            borderRadius: '8px',
            padding: '20px',
            textAlign: 'center',
            cursor: isReady && !isProcessing ? 'default' : 'wait',
            opacity: isReady && !isProcessing ? 1 : 0.6,
            background: theme.surface,
            color: theme.text,
            font: 'inherit',
            flex: 1,
            transition: 'background-color 0.2s, border-color 0.2s',
          }}
        >
          <input
            id="fileInput"
            type="file"
            style={{ display: 'none' }}
            onChange={handleFileChange}
            disabled={!isReady || isProcessing}
          />
          <input
            id="folderInput"
            ref={folderInputRef}
            type="file"
            style={{ display: 'none' }}
            onChange={handleFileChange}
            disabled={!isReady || isProcessing}
          />
          {isProcessing ? (
            <p style={{ margin: 0 }}>Processing...</p>
          ) : (
            <div>
              <p style={{ margin: 0 }}>Drag & drop an FMU/SSP file or folder here</p>
              <div
                style={{
                  marginTop: '10px',
                  display: 'flex',
                  gap: '10px',
                  justifyContent: 'center',
                }}
              >
                <label
                  htmlFor="fileInput"
                  style={{
                    textDecoration: 'underline',
                    cursor: 'pointer',
                  }}
                >
                  Select File
                </label>
                <span>or</span>
                <label
                  htmlFor="folderInput"
                  style={{
                    textDecoration: 'underline',
                    cursor: 'pointer',
                  }}
                >
                  Select Folder
                </label>
              </div>
            </div>
          )}
        </div>

        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '8px',
            flexShrink: 0,
            alignSelf: 'stretch',
          }}
        >
          <a
            href="https://github.com/nik5346/FSLint"
            target="_blank"
            rel="noopener noreferrer"
            title="View on GitHub"
            style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              width: '34px',
              borderRadius: '6px',
              border: `1px solid ${theme.buttonBorder}`,
              backgroundColor: 'transparent',
              color: theme.text,
              textDecoration: 'none',
              cursor: 'pointer',
              transition: 'background-color 0.15s, border-color 0.15s',
              flexShrink: 0,
            }}
            onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = theme.buttonHoverBg)}
            onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = 'transparent')}
          >
            <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
              <path d="M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12" />
            </svg>
          </a>

          <button
            onClick={() => setIsDark((d) => !d)}
            title={isDark ? 'Switch to light mode' : 'Switch to dark mode'}
            className="icon-btn"
            style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              width: '34px',
              borderRadius: '6px',
              border: `1px solid ${theme.buttonBorder}`,
              color: theme.text,
              cursor: 'pointer',
              transition: 'background-color 0.15s, border-color 0.15s',
              flexShrink: 0,
            }}
          >
            {isDark ? (
              <svg
                width="16"
                height="16"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="2"
                strokeLinecap="round"
                strokeLinejoin="round"
              >
                <circle cx="12" cy="12" r="5" />
                <line x1="12" y1="1" x2="12" y2="3" />
                <line x1="12" y1="21" x2="12" y2="23" />
                <line x1="4.22" y1="4.22" x2="5.64" y2="5.64" />
                <line x1="18.36" y1="18.36" x2="19.78" y2="19.78" />
                <line x1="1" y1="12" x2="3" y2="12" />
                <line x1="21" y1="12" x2="23" y2="12" />
                <line x1="4.22" y1="19.78" x2="5.64" y2="18.36" />
                <line x1="18.36" y1="5.64" x2="19.78" y2="4.22" />
              </svg>
            ) : (
              <svg
                width="16"
                height="16"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="2"
                strokeLinecap="round"
                strokeLinejoin="round"
              >
                <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z" />
              </svg>
            )}
          </button>
        </div>
      </header>

      <div
        style={{
          position: 'relative',
          flex: 1,
          minHeight: 0,
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        <button
          onClick={handleCopy}
          disabled={!output}
          title={copied ? 'Copied!' : 'Copy to clipboard'}
          className="copy-btn"
          style={{
            position: 'absolute',
            top: '6px',
            right: '6px',
            zIndex: 1,
            padding: '5px',
            color: theme.termText,
            border: 'none',
            borderRadius: '6px',
            cursor: output ? 'pointer' : 'default',
            opacity: output ? 0.6 : 0.2,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            transition: 'background-color 0.15s, opacity 0.15s',
          }}
        >
          {copied ? (
            <svg
              width="16"
              height="16"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="2"
              strokeLinecap="round"
              strokeLinejoin="round"
            >
              <polyline points="20 6 9 17 4 12" />
            </svg>
          ) : (
            <svg
              width="16"
              height="16"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="2"
              strokeLinecap="round"
              strokeLinejoin="round"
            >
              <rect x="9" y="2" width="6" height="4" rx="1" ry="1" />
              <path d="M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2" />
            </svg>
          )}
        </button>
        <pre
          ref={outputEndRef}
          style={{
            flex: 1,
            minHeight: 0,
            backgroundColor: theme.termBg,
            color: theme.termText,
            padding: '15px',
            borderRadius: '4px',
            overflowY: 'auto',
            margin: 0,
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-all',
            fontFamily: 'monospace',
            fontVariantNumeric: 'tabular-nums',
            textRendering: 'optimizeSpeed',
          }}
        >
          {output}
        </pre>
      </div>

      <footer
        style={{
          fontSize: '0.8em',
          color: theme.muted,
          textAlign: 'center',
          flexShrink: 0,
          transition: 'color 0.2s',
        }}
      >
        FSLint Core runs in WebAssembly using Emscripten. All processing is done locally in your
        browser.
      </footer>
    </div>
  );
}

export default App;
