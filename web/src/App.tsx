import { useState, useEffect, useRef, useMemo } from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';

interface Theme {
  bg: string;
  surface: string;
  border: string;
  text: string;
  muted: string;
  termBg: string;
  termText: string;
  termBtnHover: string;
  iconHover: string;
  buttonBorder: string;
  buttonHoverBg: string;
}

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
    readFile: (path: string, opts?: { encoding?: string; flags?: string }) => Uint8Array | string;
  };
  callMain: (args: string[]) => void;
}

interface FileNode {
  name: string;
  path: string;
  kind: 'file' | 'directory';
  children?: FileNode[];
}

declare global {
  const __APP_VERSION__: string;
  interface Window {
    createFSLintModule: (config: {
      print: (text: string) => void;
      printErr: (text: string) => void;
      locateFile?: (path: string, prefix: string) => string;
    }) => Promise<FSLintModule>;
    showDirectoryPicker?: (options?: {
      mode?: 'read' | 'readwrite';
    }) => Promise<FileSystemDirectoryHandle>;
  }

  interface FileSystemHandle {
    readonly kind: 'file' | 'directory';
    readonly name: string;
  }

  interface FileSystemFileHandle extends FileSystemHandle {
    readonly kind: 'file';
    getFile(): Promise<File>;
  }

  interface FileSystemDirectoryHandle extends FileSystemHandle {
    readonly kind: 'directory';
    values(): AsyncIterableIterator<FileSystemHandle>;
  }
}

async function getFilesFromHandle(
  handle: FileSystemDirectoryHandle,
  path = handle.name,
): Promise<File[]> {
  const files: File[] = [];
  for await (const entry of handle.values()) {
    const entryPath = `${path}/${entry.name}`;
    if (entry.kind === 'file') {
      const file = await (entry as FileSystemFileHandle).getFile();
      Object.defineProperty(file, 'webkitRelativePath', {
        value: entryPath,
      });
      files.push(file);
    } else if (entry.kind === 'directory') {
      const subFiles = await getFilesFromHandle(entry as FileSystemDirectoryHandle, entryPath);
      files.push(...subFiles);
    }
  }
  return files;
}

const FileTreeItem = ({
  node,
  selectedFile,
  setSelectedFile,
  theme,
  level = 0,
}: {
  node: FileNode;
  selectedFile: string | null;
  setSelectedFile: (path: string) => void;
  theme: Theme;
  level?: number;
}) => {
  const isSelected = selectedFile === node.path;
  const isDir = node.kind === 'directory';

  return (
    <div style={{ marginLeft: level * 12 }}>
      <div
        role="button"
        tabIndex={isDir ? -1 : 0}
        onClick={() => !isDir && setSelectedFile(node.path)}
        onKeyDown={(e) => {
          if (!isDir && (e.key === 'Enter' || e.key === ' ')) {
            setSelectedFile(node.path);
          }
        }}
        style={{
          padding: '4px 8px',
          cursor: isDir ? 'default' : 'pointer',
          borderRadius: '4px',
          backgroundColor: isSelected ? theme.buttonHoverBg : 'transparent',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          fontSize: '0.9em',
          whiteSpace: 'nowrap',
        }}
        onMouseEnter={(e) => !isSelected && (e.currentTarget.style.backgroundColor = theme.iconHover)}
        onMouseLeave={(e) => !isSelected && (e.currentTarget.style.backgroundColor = 'transparent')}
      >
        {isDir ? (
          <svg
            width="14"
            height="14"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path>
          </svg>
        ) : (
          <svg
            width="14"
            height="14"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"></path>
            <polyline points="13 2 13 9 20 9"></polyline>
          </svg>
        )}
        {node.name}
      </div>
      {isDir &&
        node.children?.map((child) => (
          <FileTreeItem
            key={child.path}
            node={child}
            selectedFile={selectedFile}
            setSelectedFile={setSelectedFile}
            theme={theme}
            level={level + 1}
          />
        ))}
    </div>
  );
};

const FilePreview = ({
  selectedFile,
  module,
  theme,
}: {
  selectedFile: string | null;
  module: FSLintModule | null;
  theme: Theme;
}) => {
  const [imageUrl, setImageUrl] = useState<string | null>(null);

  useEffect(() => {
    let url: string | null = null;

    if (selectedFile && module) {
      const ext = selectedFile.split('.').pop()?.toLowerCase();
      const isImage = ext === 'png' || ext === 'svg' || ext === 'jpg' || ext === 'jpeg';

      if (isImage) {
        try {
          const data = module.FS.readFile(selectedFile) as Uint8Array;
          const type = ext === 'svg' ? 'image/svg+xml' : `image/${ext}`;
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          const blob = new Blob([data as any], { type });
          url = URL.createObjectURL(blob);
        } catch (e) {
          console.error('Failed to create object URL:', e);
        }
      }
    }

    // eslint-disable-next-line react-hooks/set-state-in-effect
    setImageUrl(url);
    return () => {
      if (url) URL.revokeObjectURL(url);
    };

  }, [selectedFile, module]);

  if (!selectedFile || !module) return null;

  const ext = selectedFile.split('.').pop()?.toLowerCase();
  const isImage = ext === 'png' || ext === 'svg' || ext === 'jpg' || ext === 'jpeg';

  if (isImage) {
    return imageUrl ? (
      <div style={{ padding: '20px', display: 'flex', justifyContent: 'center' }}>
        <img
          src={imageUrl}
          alt={selectedFile}
          style={{ maxWidth: '100%', maxHeight: '100%', objectFit: 'contain' }}
        />
      </div>
    ) : (
      <div style={{ padding: '20px', color: '#ff5555' }}>Failed to load image</div>
    );
  }

  let content: string;
  try {
    content = module.FS.readFile(selectedFile, { encoding: 'utf8' }) as string;
  } catch (e) {
    return (
      <div style={{ padding: '20px', color: '#ff5555' }}>Failed to load file: {String(e)}</div>
    );
  }

  return (
    <pre
      style={{
        margin: 0,
        padding: '15px',
        fontFamily: 'monospace',
        fontSize: '0.9em',
        whiteSpace: 'pre-wrap',
        wordBreak: 'break-all',
        color: theme.text,
      }}
    >
      {content}
    </pre>
  );
};

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
  const [activeTab, setActiveTab] = useState<'certificate' | 'rules' | 'explorer'>('certificate');
  const [currentWorkDir, setCurrentWorkDir] = useState<string | null>(null);
  const [fileTree, setFileTree] = useState<FileNode | null>(null);
  const [selectedFile, setSelectedFile] = useState<string | null>(null);
  const [rulesText, setRulesText] = useState<string>('');

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

  useEffect(() => {
    fetch('rules.md')
      .then((res) => res.text())
      .then((text) => setRulesText(text))
      .catch((err) => console.error('Failed to load rules.md:', err));
  }, []);

  const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const files = event.target.files;
    if (files && files.length > 0) {
      await processItems(Array.from(files));
    }
    event.target.value = '';
  };

  const handleFolderSelect = async (event: React.MouseEvent) => {
    event.preventDefault();
    if (window.showDirectoryPicker) {
      try {
        const handle = await window.showDirectoryPicker();
        const files = await getFilesFromHandle(handle);
        if (files.length > 0) {
          await processItems(files);
        }
      } catch (err) {
        if ((err as Error).name !== 'AbortError') {
          console.error('showDirectoryPicker failed, falling back to input:', err);
          folderInputRef.current?.click();
        }
      }
    } else {
      folderInputRef.current?.click();
    }
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

  const getFileTree = (path: string): FileNode | null => {
    if (!module) return null;
    try {
      const stat = module.FS.stat(path);
      const name = path.split('/').pop() || '/';
      const node: FileNode = {
        name,
        path,
        kind: module.FS.isDir(stat.mode) ? 'directory' : 'file',
      };

      if (node.kind === 'directory') {
        const entries = module.FS.readdir(path);
        node.children = entries
          .filter((e) => e !== '.' && e !== '..')
          .map((e) => getFileTree(path === '/' ? `/${e}` : `${path}/${e}`))
          .filter((n): n is FileNode => n !== null)
          .sort((a, b) => {
            if (a.kind !== b.kind) return a.kind === 'directory' ? -1 : 1;
            return a.name.localeCompare(b.name);
          });
      }
      return node;
    } catch {
      return null;
    }
  };

  const processItems = async (files: File[]) => {
    if (!module || isProcessing || files.length === 0) return;

    setIsProcessing(true);
    setOutput('');
    setFileTree(null);
    setSelectedFile(null);

    // Clean up previous run
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

      // After execution, build the tree
      const rootPath = discoveredRootRel ? `${workDir}/${discoveredRootRel}` : workDir;
      setFileTree(getFileTree(rootPath));
      setActiveTab('certificate');
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
        // We no longer unlink workDir here so it remains available for the explorer
      } catch (e) {
        console.error('Cleanup failed:', e);
      }
      setIsProcessing(false);
    }
  };

  const handleCopy = () => {
    // Strip ANSI codes before copying
    // eslint-disable-next-line no-control-regex
    const stripped = output.replace(/\x1b\[[0-9;]*m/g, '');
    navigator.clipboard.writeText(stripped).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  const parseAnsi = (text: string) => {
    // eslint-disable-next-line no-control-regex
    const parts = text.split(/(\x1b\[[0-9;]*m)/);
    let currentColor = '';

    return parts.map((part, i) => {
      if (part.startsWith('\x1b[')) {
        if (part === '\x1b[31m') currentColor = '#ff5555';
        else if (part === '\x1b[33m') currentColor = '#ffb86c';
        else if (part === '\x1b[0m') currentColor = '';
        return null;
      }
      return currentColor ? (
        <span key={i} style={{ color: currentColor }}>
          {part}
        </span>
      ) : (
        part
      );
    });
  };


  return (
    <div
      style={{
        position: 'fixed',
        inset: 0,
        display: 'flex',
        flexDirection: 'row',
        boxSizing: 'border-box',
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
        .tab-btn {
          width: 48px;
          height: 48px;
          display: flex;
          align-items: center;
          justifyContent: center;
          border: none;
          background: transparent;
          color: inherit;
          cursor: pointer;
          border-radius: 8px;
          transition: background-color 0.2s;
        }
        .tab-btn:hover {
          background-color: var(--btn-hover-bg);
        }
        .tab-btn.active {
          background-color: var(--btn-hover-bg);
          color: #007bff;
        }
      `}</style>

      {/* Sidebar */}
      <aside
        style={{
          width: '64px',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          padding: '20px 0',
          gap: '12px',
          borderRight: `1px solid ${theme.border}`,
          backgroundColor: theme.surface,
          flexShrink: 0,
        }}
      >
        <button
          className={`tab-btn ${activeTab === 'certificate' ? 'active' : ''}`}
          onClick={() => setActiveTab('certificate')}
          title="Validation Certificate"
        >
          <svg
            width="24"
            height="24"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
            <polyline points="14 2 14 8 20 8"></polyline>
            <line x1="16" y1="13" x2="8" y2="13"></line>
            <line x1="16" y1="17" x2="8" y2="17"></line>
            <polyline points="10 9 9 9 8 9"></polyline>
          </svg>
        </button>
        <button
          className={`tab-btn ${activeTab === 'rules' ? 'active' : ''}`}
          onClick={() => setActiveTab('rules')}
          title="Validation Rules"
        >
          <svg
            width="24"
            height="24"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"></path>
            <path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"></path>
          </svg>
        </button>
        <button
          className={`tab-btn ${activeTab === 'explorer' ? 'active' : ''}`}
          onClick={() => setActiveTab('explorer')}
          disabled={!fileTree}
          style={{ opacity: fileTree ? 1 : 0.3, cursor: fileTree ? 'pointer' : 'default' }}
          title="File Explorer"
        >
          <svg
            width="24"
            height="24"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <rect x="2" y="3" width="20" height="14" rx="2" ry="2"></rect>
            <line x1="2" y1="10" x2="22" y2="10"></line>
            <path d="M7 21h10"></path>
            <line x1="12" y1="17" x2="12" y2="21"></line>
          </svg>
        </button>
      </aside>

      <main
        style={{
          flex: 1,
          display: 'flex',
          flexDirection: 'column',
          padding: '20px',
          gap: '20px',
          minWidth: 0,
        }}
      >
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
                  <span
                    role="button"
                    tabIndex={0}
                    onClick={handleFolderSelect}
                    onKeyDown={(e) =>
                      (e.key === 'Enter' || e.key === ' ') &&
                      handleFolderSelect(e as unknown as React.MouseEvent)
                    }
                    style={{
                      textDecoration: 'underline',
                      cursor: 'pointer',
                    }}
                  >
                    Select Folder
                  </span>
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
              justifyContent: 'center',
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
              <svg
                width="16"
                height="16"
                viewBox="0 0 24 24"
                fill="currentColor"
                aria-hidden="true"
              >
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

        {activeTab === 'rules' && (
          <div
            style={{
              flex: 1,
              minHeight: 0,
              overflowY: 'auto',
              padding: '0 20px',
              backgroundColor: theme.surface,
              borderRadius: '4px',
              border: `1px solid ${theme.border}`,
            }}
          >
            <style>{`
            .markdown-body table { border-collapse: collapse; width: 100%; margin: 1em 0; }
            .markdown-body th, .markdown-body td { border: 1px solid ${theme.border}; padding: 8px; text-align: left; }
            .markdown-body th { background-color: ${theme.bg}; }
            .markdown-body code { background-color: ${theme.bg}; padding: 2px 4px; border-radius: 4px; }
            .markdown-body pre { background-color: ${theme.bg}; padding: 16px; border-radius: 4px; overflow: auto; }
            .markdown-body blockquote { border-left: 4px solid ${theme.border}; padding-left: 16px; color: ${theme.muted}; }
          `}</style>
            <div className="markdown-body">
              <ReactMarkdown remarkPlugins={[remarkGfm]}>{rulesText}</ReactMarkdown>
            </div>
          </div>
        )}

        {activeTab === 'explorer' && fileTree && (
          <div
            style={{
              flex: 1,
              minHeight: 0,
              display: 'flex',
              gap: '1px',
              backgroundColor: theme.border,
              borderRadius: '4px',
              border: `1px solid ${theme.border}`,
              overflow: 'hidden',
            }}
          >
            <div
              style={{
                width: '300px',
                backgroundColor: theme.surface,
                overflowY: 'auto',
                padding: '10px',
              }}
            >
              <FileTreeItem
                node={fileTree}
                selectedFile={selectedFile}
                setSelectedFile={setSelectedFile}
                theme={theme}
              />
            </div>
            <div
              style={{
                flex: 1,
                backgroundColor: theme.surface,
                overflowY: 'auto',
              }}
            >
              <FilePreview selectedFile={selectedFile} module={module} theme={theme} />
            </div>
          </div>
        )}

        {activeTab === 'certificate' && (
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
              {parseAnsi(output)}
            </pre>
          </div>
        )}
      </main>

      <footer
        style={{
          fontSize: '0.8em',
          color: theme.muted,
          textAlign: 'center',
          flexShrink: 0,
          transition: 'color 0.2s',
          display: 'flex',
          justifyContent: 'center',
          gap: '10px',
        }}
      >
        <span>FSLint v{__APP_VERSION__}</span>
        <span>•</span>
        <span>
          FSLint Core runs in WebAssembly using Emscripten. All processing is done locally in your
          browser.
        </span>
      </footer>
    </div>
  );
}

export default App;
