import { useState, useEffect, useRef, useMemo, memo } from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { PrismLight as SyntaxHighlighter } from 'react-syntax-highlighter';
import { vscDarkPlus, prism } from 'react-syntax-highlighter/dist/esm/styles/prism';
import clike from 'react-syntax-highlighter/dist/esm/languages/prism/clike';
import cpp from 'react-syntax-highlighter/dist/esm/languages/prism/cpp';
import markdown from 'react-syntax-highlighter/dist/esm/languages/prism/markdown';
import json from 'react-syntax-highlighter/dist/esm/languages/prism/json';
import bash from 'react-syntax-highlighter/dist/esm/languages/prism/bash';
import markup from 'react-syntax-highlighter/dist/esm/languages/prism/markup';
import python from 'react-syntax-highlighter/dist/esm/languages/prism/python';
import yaml from 'react-syntax-highlighter/dist/esm/languages/prism/yaml';
import javascript from 'react-syntax-highlighter/dist/esm/languages/prism/javascript';
import typescript from 'react-syntax-highlighter/dist/esm/languages/prism/typescript';
import css from 'react-syntax-highlighter/dist/esm/languages/prism/css';
import log from 'react-syntax-highlighter/dist/esm/languages/prism/log';

SyntaxHighlighter.registerLanguage('markup', markup);
SyntaxHighlighter.registerLanguage('clike', clike);
SyntaxHighlighter.registerLanguage('cpp', cpp);
SyntaxHighlighter.registerLanguage('c++', cpp);
SyntaxHighlighter.registerLanguage('c', cpp);
SyntaxHighlighter.registerLanguage('markdown', markdown);
SyntaxHighlighter.registerLanguage('md', markdown);
SyntaxHighlighter.registerLanguage('json', json);
SyntaxHighlighter.registerLanguage('bash', bash);
SyntaxHighlighter.registerLanguage('sh', bash);
SyntaxHighlighter.registerLanguage('xml', markup);
SyntaxHighlighter.registerLanguage('xsd', markup);
SyntaxHighlighter.registerLanguage('ssd', markup);
SyntaxHighlighter.registerLanguage('html', markup);
SyntaxHighlighter.registerLanguage('htm', markup);
SyntaxHighlighter.registerLanguage('svg', markup);
SyntaxHighlighter.registerLanguage('python', python);
SyntaxHighlighter.registerLanguage('py', python);
SyntaxHighlighter.registerLanguage('yaml', yaml);
SyntaxHighlighter.registerLanguage('yml', yaml);
SyntaxHighlighter.registerLanguage('javascript', javascript);
SyntaxHighlighter.registerLanguage('js', javascript);
SyntaxHighlighter.registerLanguage('typescript', typescript);
SyntaxHighlighter.registerLanguage('ts', typescript);
SyntaxHighlighter.registerLanguage('css', css);
SyntaxHighlighter.registerLanguage('log', log);

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
  _is_binary: (path: number) => number;
  _get_file_tree_json: (path: number) => number;
  _run_validation: (path: number) => void;
  stackAlloc: (size: number) => number;
  stackSave: () => number;
  stackRestore: (stack: number) => void;
  stringToUTF8: (str: string, outPtr: number, maxBytes: number) => void;
  UTF8ToString: (ptr: number) => string;
}

interface FileNode {
  name: string;
  path: string;
  kind: 'file' | 'directory';
  isBinary: boolean;
  children?: FileNode[];
}

declare global {
  const __APP_VERSION__: string;
  const __BUILD_YEAR__: string;
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

const RainbowCsvHighlighter = ({
  content,
  isDark,
  theme,
}: {
  content: string;
  isDark: boolean;
  theme: Theme;
}) => {
  const lines = content.split('\n');
  const colors = isDark
    ? ['#ff5555', '#50fa7b', '#f1fa8c', '#bd93f9', '#ff79c6', '#8be9fd', '#ffb86c']
    : ['#e45649', '#50a14f', '#c18401', '#4078f2', '#a626a4', '#0184bc', '#986801'];

  return (
    <div
      style={{
        margin: 0,
        padding: '15px 0',
        fontSize: '0.9em',
        fontFamily: 'monospace',
        lineHeight: '1.5em',
        flex: 1,
        overflow: 'auto',
      }}
    >
      {lines.map((line, lineIdx) => {
        // Simple CSV split, doesn't handle escaped commas but good for "rainbow" effect
        const cells = line.split(',');
        return (
          <div
            key={lineIdx}
            style={{
              display: 'flex',
              width: 'fit-content',
              minWidth: '100%',
              paddingRight: '15px',
            }}
          >
            <div
              style={{
                minWidth: '40px',
                paddingLeft: '15px',
                paddingRight: '10px',
                textAlign: 'right',
                color: isDark ? '#858585' : '#999999',
                userSelect: 'none',
                position: 'sticky',
                left: 0,
                backgroundColor: theme.surface,
                zIndex: 1,
              }}
            >
              {lineIdx + 1}
            </div>
            <div style={{ paddingLeft: '10px', whiteSpace: 'pre' }}>
              {cells.map((cell, cellIdx) => (
                <span key={cellIdx}>
                  <span style={{ color: colors[cellIdx % colors.length] }}>{cell}</span>
                  {cellIdx < cells.length - 1 && <span style={{ color: theme.muted }}>,</span>}
                </span>
              ))}
            </div>
          </div>
        );
      })}
    </div>
  );
};

const extractHeaders = (text: string) => {
  const lines = text.split('\n');
  const headers: { level: number; text: string; line: number }[] = [];
  lines.forEach((line, index) => {
    const match = line.match(/^(#{1,6})\s+(.+)$/);
    if (match) {
      headers.push({
        level: match[1].length,
        text: match[2].trim(),
        line: index + 1,
      });
    }
  });
  return headers;
};

const MarkdownContent = ({
  content,
  theme,
  isDark,
}: {
  content: string;
  theme: Theme;
  isDark: boolean;
}) => {
  return (
    <div className="markdown-body">
      <style>{`
        .markdown-body { font-size: 0.9em; }
        .markdown-body table { border-collapse: collapse; width: 100%; margin: 1em 0; }
        .markdown-body th, .markdown-body td { border: 1px solid ${theme.border}; padding: 8px; text-align: left; }
        .markdown-body th { background-color: ${theme.bg}; }
        .markdown-body code { background-color: ${theme.bg}; padding: 2px 4px; border-radius: 4px; }
        .markdown-body pre { background-color: transparent !important; padding: 0 !important; margin: 1em 0 !important; border-radius: 4px; overflow: hidden; }
        .markdown-body blockquote { border-left: 4px solid ${theme.border}; padding-left: 16px; color: ${theme.muted}; }
      `}</style>
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={{
          /* eslint-disable @typescript-eslint/no-explicit-any */
          h1: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h1 id={line ? `line-${line}` : undefined}>{children}</h1>;
          },
          h2: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h2 id={line ? `line-${line}` : undefined}>{children}</h2>;
          },
          h3: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h3 id={line ? `line-${line}` : undefined}>{children}</h3>;
          },
          h4: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h4 id={line ? `line-${line}` : undefined}>{children}</h4>;
          },
          h5: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h5 id={line ? `line-${line}` : undefined}>{children}</h5>;
          },
          h6: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h6 id={line ? `line-${line}` : undefined}>{children}</h6>;
          },
          code({ inline, className, children, ...props }: any) {
            const match = /language-(\w+)/.exec(className || '');
            return !inline && match ? (
              <SyntaxHighlighter
                style={isDark ? vscDarkPlus : prism}
                language={match[1]}
                PreTag="div"
                {...props}
              >
                {String(children).replace(/\n$/, '')}
              </SyntaxHighlighter>
            ) : (
              <code className={className} {...props}>
                {children}
              </code>
            );
          },
          /* eslint-enable @typescript-eslint/no-explicit-any */
        }}
      >
        {content}
      </ReactMarkdown>
    </div>
  );
};

const RulesOutline = ({
  headers,
  theme,
}: {
  headers: { level: number; text: string; line: number }[];
  theme: Theme;
}) => {
  return (
    <div style={{ fontSize: '0.9em' }}>
      {headers.map((header, i) => (
        <div
          key={i}
          role="button"
          tabIndex={0}
          onClick={() => {
            const el = document.getElementById(`line-${header.line}`);
            if (el) el.scrollIntoView({ behavior: 'smooth' });
          }}
          onKeyDown={(e) => {
            if (e.key === 'Enter' || e.key === ' ') {
              const el = document.getElementById(`line-${header.line}`);
              if (el) el.scrollIntoView({ behavior: 'smooth' });
            }
          }}
          style={{
            padding: '4px 8px',
            cursor: 'pointer',
            borderRadius: '4px',
            marginLeft: (header.level - 1) * 12,
            color: header.level === 1 ? theme.text : theme.muted,
            fontWeight: header.level === 1 ? 'bold' : 'normal',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          }}
          onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = theme.iconHover)}
          onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = 'transparent')}
        >
          {header.text}
        </div>
      ))}
    </div>
  );
};

const FileTreeItem = memo(function FileTreeItem({
  node,
  isSelected,
  selectedFile,
  setSelectedFile,
  theme,
  level = 0,
}: {
  node: FileNode;
  isSelected: boolean;
  selectedFile: string | null;
  setSelectedFile: (path: string) => void;
  theme: Theme;
  level?: number;
}) {
  const [isOpen, setIsOpen] = useState(true);
  const isDir = node.kind === 'directory';

  return (
    <>
      <div
        role="button"
        tabIndex={0}
        onClick={() => (isDir ? setIsOpen(!isOpen) : setSelectedFile(node.path))}
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            if (isDir) {
              setIsOpen(!isOpen);
            } else {
              setSelectedFile(node.path);
            }
          }
        }}
        style={{
          padding: '4px 8px',
          paddingLeft: level * 12 + 8,
          cursor: 'pointer',
          borderRadius: '4px',
          backgroundColor: isSelected ? theme.buttonHoverBg : 'transparent',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          fontSize: '0.9em',
          whiteSpace: 'nowrap',
        }}
        onMouseEnter={(e) =>
          !isSelected && (e.currentTarget.style.backgroundColor = theme.iconHover)
        }
        onMouseLeave={(e) => !isSelected && (e.currentTarget.style.backgroundColor = 'transparent')}
      >
        {isDir ? (
          <>
            <svg
              width="10"
              height="10"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="3"
              strokeLinecap="round"
              strokeLinejoin="round"
              style={{
                transform: isOpen ? 'rotate(90deg)' : 'none',
                transition: 'transform 0.1s',
                flexShrink: 0,
              }}
            >
              <polyline points="9 18 15 12 9 6"></polyline>
            </svg>
            <svg
              width="14"
              height="14"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="2"
              strokeLinecap="round"
              strokeLinejoin="round"
              style={{ flexShrink: 0 }}
            >
              <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path>
            </svg>
          </>
        ) : (
          <>
            <div style={{ width: 10, flexShrink: 0 }} />
            <svg
              width="14"
              height="14"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="2"
              strokeLinecap="round"
              strokeLinejoin="round"
              style={{ flexShrink: 0 }}
            >
              <path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"></path>
              <polyline points="13 2 13 9 20 9"></polyline>
            </svg>
          </>
        )}
        <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{node.name}</span>
      </div>
      {isDir &&
        isOpen &&
        node.children?.map((child) => (
          <FileTreeItem
            key={child.path}
            node={child}
            isSelected={child.path === selectedFile}
            selectedFile={selectedFile}
            setSelectedFile={setSelectedFile}
            theme={theme}
            level={level + 1}
          />
        ))}
    </>
  );
});

const FilePreview = ({
  selectedFile,
  node,
  module,
  theme,
  isDark,
}: {
  selectedFile: string | null;
  node: FileNode | null | undefined;
  module: FSLintModule | null;
  theme: Theme;
  isDark: boolean;
}) => {
  const [imageUrl, setImageUrl] = useState<string | null>(null);
  const [htmlContent, setHtmlContent] = useState<string>('');
  const [copied, setCopied] = useState(false);
  const [viewMode, setViewMode] = useState<'render' | 'code'>('render');
  const htmlBlobUrls = useRef<string[]>([]);

  useEffect(() => {
    let url: string | null = null;

    if (selectedFile && module) {
      const ext = selectedFile.split('.').pop()?.toLowerCase();
      const isImage = ext === 'png' || ext === 'svg' || ext === 'jpg' || ext === 'jpeg';
      const isPdf = ext === 'pdf';

      if (isImage || isPdf) {
        try {
          const data = module.FS.readFile(selectedFile) as Uint8Array;
          let type = '';
          if (ext === 'svg') type = 'image/svg+xml';
          else if (isPdf) type = 'application/pdf';
          else type = `image/${ext}`;

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

  const ext = selectedFile?.split('.').pop()?.toLowerCase();
  const isStaticImage = ext === 'png' || ext === 'jpg' || ext === 'jpeg';
  const isSvg = ext === 'svg';
  const isPdf = ext === 'pdf';
  const isHtml = ext === 'html' || ext === 'htm';
  const isMarkdown = ext === 'md';
  const canToggle = isSvg || isHtml || isMarkdown;

  const isBinaryResult = node?.isBinary ?? false;

  const data = useMemo(() => {
    if (!selectedFile || !module) return null;
    if (isBinaryResult && !isStaticImage && !isPdf) return null;
    try {
      return module.FS.readFile(selectedFile) as Uint8Array;
    } catch (e) {
      console.error('Failed to read file:', e);
      return null;
    }
  }, [selectedFile, module, isBinaryResult, isStaticImage, isPdf]);

  const content = useMemo(() => {
    if (!data || (isBinaryResult && (isStaticImage || isPdf))) return '';
    return new TextDecoder().decode(data);
  }, [data, isBinaryResult, isStaticImage, isPdf]);

  useEffect(() => {
    // Cleanup old blob URLs
    htmlBlobUrls.current.forEach(URL.revokeObjectURL);
    htmlBlobUrls.current = [];

    if (!selectedFile || !module || !isHtml || viewMode !== 'render') {
      // eslint-disable-next-line react-hooks/set-state-in-effect
      setHtmlContent('');
      return;
    }

    const dir = selectedFile.substring(0, selectedFile.lastIndexOf('/'));
    let processed = content;

    const resolve = (base: string, rel: string) => {
      const stack = base.split('/').filter(Boolean);
      const parts = rel.split('/').filter(Boolean);
      for (const part of parts) {
        if (part === '.') continue;
        if (part === '..') stack.pop();
        else stack.push(part);
      }
      return (selectedFile?.startsWith('/') ? '/' : '') + stack.join('/');
    };

    // Find all src and href attributes
    const matches = Array.from(processed.matchAll(/(src|href)=["']([^"']+)["']/g));
    for (const match of matches) {
      const [full, attr, relPath] = match;
      if (
        relPath.startsWith('http') ||
        relPath.startsWith('data:') ||
        relPath.startsWith('#') ||
        relPath.startsWith('mailto:') ||
        relPath.startsWith('javascript:')
      ) {
        continue;
      }

      const resolvedPath = resolve(dir, relPath);
      try {
        const fileData = module.FS.readFile(resolvedPath) as Uint8Array;
        const subExt = resolvedPath.split('.').pop()?.toLowerCase();
        let type = 'application/octet-stream';
        if (subExt === 'svg') type = 'image/svg+xml';
        else if (subExt === 'png') type = 'image/png';
        else if (subExt === 'jpg' || subExt === 'jpeg') type = 'image/jpeg';
        else if (subExt === 'css') type = 'text/css';
        else if (subExt === 'js') type = 'application/javascript';

        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const blob = new Blob([fileData as any], { type });
        const url = URL.createObjectURL(blob);
        htmlBlobUrls.current.push(url);
        // Replace all occurrences of this exact match
        processed = processed.split(full).join(`${attr}="${url}"`);
      } catch {
        // Skip if file doesn't exist
      }
    }
    setHtmlContent(processed);

    return () => {
      htmlBlobUrls.current.forEach(URL.revokeObjectURL);
      htmlBlobUrls.current = [];
    };
  }, [selectedFile, content, isHtml, module, viewMode]);

  if (isBinaryResult && !isStaticImage && !isSvg && !isPdf) {
    return (
      <div
        style={{
          padding: '40px',
          textAlign: 'center',
          color: theme.muted,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: '12px',
        }}
      >
        <svg
          width="48"
          height="48"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.5"
          strokeLinecap="round"
          strokeLinejoin="round"
        >
          <path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"></path>
          <polyline points="13 2 13 9 20 9"></polyline>
        </svg>
        <span>Binary file cannot be displayed</span>
      </div>
    );
  }

  if (!selectedFile || !module) return null;

  const handleCopy = () => {
    navigator.clipboard.writeText(content).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  const getLanguage = (filename: string, text: string) => {
    const ext = filename.split('.').pop()?.toLowerCase();
    if (ext === 'xml' || ext === 'xsd' || ext === 'ssd' || ext === 'svg') return 'markup';
    if (ext === 'html' || ext === 'htm') return 'markup';
    if (ext === 'cpp' || ext === 'hpp' || ext === 'c' || ext === 'h') return 'cpp';
    if (ext === 'json') return 'json';
    if (ext === 'sh' || ext === 'bash') return 'bash';
    if (ext === 'md' || ext === 'markdown') return 'markdown';
    if (ext === 'csv') return 'csv';
    if (ext === 'py') return 'python';
    if (ext === 'yaml' || ext === 'yml') return 'yaml';
    if (ext === 'js' || ext === 'javascript') return 'javascript';
    if (ext === 'ts' || ext === 'typescript') return 'typescript';
    if (ext === 'css') return 'css';
    if (ext === 'log') return 'log';

    // Content based fallback
    const start = text.trim().substring(0, 100).toLowerCase();
    if (
      start.startsWith('<?xml') ||
      start.includes('<modeldescription') ||
      start.includes('<systemstructure')
    )
      return 'markup';
    if (start.startsWith('<!doctype html') || start.includes('<html')) return 'markup';

    return 'text';
  };

  if (isStaticImage || isPdf) {
    return (
      <div
        style={{ position: 'relative', display: 'flex', flexDirection: 'column', height: '100%' }}
      >
        <div
          style={{
            flex: 1,
            overflowY: 'auto',
            padding: isStaticImage ? '20px' : '0',
            display: 'flex',
            justifyContent: 'center',
            alignItems: 'center',
          }}
        >
          {imageUrl ? (
            isStaticImage ? (
              <img
                src={imageUrl}
                alt={selectedFile}
                style={{ width: '100%', height: '100%', objectFit: 'contain' }}
              />
            ) : (
              <iframe
                src={imageUrl}
                title="PDF Preview"
                style={{ width: '100%', height: '100%', border: 'none' }}
              />
            )
          ) : (
            <div style={{ color: '#ff5555' }}>Failed to load {isStaticImage ? 'image' : 'PDF'}</div>
          )}
        </div>
      </div>
    );
  }

  return (
    <div style={{ position: 'relative', display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div
        style={{
          position: 'absolute',
          top: '6px',
          right: '12px',
          zIndex: 10,
          display: 'flex',
          gap: '4px',
        }}
      >
        {canToggle && (
          <button
            onClick={() => setViewMode(viewMode === 'render' ? 'code' : 'render')}
            title={viewMode === 'render' ? 'Show Code' : 'Show Preview'}
            className="copy-btn"
            style={{
              padding: '5px',
              color: theme.text,
              border: 'none',
              borderRadius: '6px',
              cursor: 'pointer',
              opacity: 0.6,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              transition: 'background-color 0.15s, opacity 0.15s',
            }}
          >
            {viewMode === 'render' ? (
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
                <polyline points="16 18 22 12 16 6"></polyline>
                <polyline points="8 6 2 12 8 18"></polyline>
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
                <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
                <circle cx="12" cy="12" r="3"></circle>
              </svg>
            )}
          </button>
        )}
        <button
          onClick={handleCopy}
          title={copied ? 'Copied!' : 'Copy to clipboard'}
          className="copy-btn"
          style={{
            padding: '5px',
            color: theme.text,
            border: 'none',
            borderRadius: '6px',
            cursor: 'pointer',
            opacity: 0.6,
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
      </div>

      <div
        style={{
          flex: 1,
          overflowY: viewMode === 'render' && canToggle ? 'auto' : 'hidden',
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        {viewMode === 'render' && canToggle ? (
          <div
            style={{
              flex: 1,
              display: 'flex',
              justifyContent: isMarkdown ? 'flex-start' : 'center',
              padding: '20px',
              backgroundColor: isHtml ? '#fff' : 'transparent',
              minHeight: '200px',
            }}
          >
            {isSvg ? (
              imageUrl ? (
                <img
                  src={imageUrl}
                  alt={selectedFile}
                  style={{ width: '100%', height: '100%', objectFit: 'contain' }}
                />
              ) : (
                <div style={{ color: '#ff5555' }}>Failed to load SVG</div>
              )
            ) : isMarkdown ? (
              <MarkdownContent content={content} theme={theme} isDark={isDark} />
            ) : (
              <iframe
                srcDoc={htmlContent || content}
                title="Preview"
                style={{
                  width: '100%',
                  flex: 1,
                  border: 'none',
                  backgroundColor: '#fff',
                }}
              />
            )}
          </div>
        ) : (
          (() => {
            const language = getLanguage(selectedFile, content);
            if (language === 'csv') {
              return <RainbowCsvHighlighter content={content} isDark={isDark} theme={theme} />;
            }
            return (
              <SyntaxHighlighter
                language={language}
                style={isDark ? vscDarkPlus : prism}
                showLineNumbers={true}
                lineNumberStyle={{
                  minWidth: '40px',
                  paddingLeft: '15px',
                  paddingRight: '10px',
                  textAlign: 'right',
                  color: isDark ? '#858585' : '#999999',
                  userSelect: 'none',
                  position: 'sticky',
                  left: 0,
                  backgroundColor: theme.surface,
                  zIndex: 1,
                }}
                codeTagProps={{ style: { display: 'block', minWidth: '100%' } }}
                customStyle={{
                  margin: 0,
                  padding: '15px 0',
                  fontSize: '0.9em',
                  backgroundColor: 'transparent',
                  flex: 1,
                  lineHeight: '1.5em',
                  overflow: 'auto',
                }}
                wrapLines={true}
                lineProps={{
                  style: {
                    display: 'flex',
                    width: 'fit-content',
                    minWidth: '100%',
                    paddingRight: '15px',
                  },
                }}
              >
                {content}
              </SyntaxHighlighter>
            );
          })()
        )}
      </div>
    </div>
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
  const fileMap = useMemo(() => {
    const map = new Map<string, FileNode>();
    const traverse = (node: FileNode) => {
      map.set(node.path, node);
      node.children?.forEach(traverse);
    };
    if (fileTree) traverse(fileTree);
    return map;
  }, [fileTree]);
  const [selectedFile, setSelectedFile] = useState<string | null>(null);
  const [rulesText, setRulesText] = useState<string>('');
  const [explorerWidth, setExplorerWidth] = useState(300);
  const [rulesWidth, setRulesWidth] = useState(250);
  const [isResizing, setIsResizing] = useState(false);
  const [isResizingRules, setIsResizingRules] = useState(false);

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

    // Inject global styles to fix SyntaxHighlighter line numbers
    const styleId = 'fslint-global-styles';
    let styleEl = document.getElementById(styleId);
    if (!styleEl) {
      styleEl = document.createElement('style');
      styleEl.id = styleId;
      document.head.appendChild(styleEl);
    }
    styleEl.textContent = `
      .react-syntax-highlighter-line-number {
        color: ${isDark ? '#858585' : '#999999'} !important;
        position: sticky !important;
        left: 0 !important;
        background-color: ${theme.surface} !important;
      }
    `;
  }, [theme.bg, theme.text, theme.surface, isDark]);

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

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      if (isResizing) {
        // 64 is sidebar width, 20 is main padding
        const newWidth = e.clientX - 64 - 20;
        if (newWidth > 150 && newWidth < 800) {
          setExplorerWidth(newWidth);
        }
      } else if (isResizingRules) {
        const newWidth = e.clientX - 64 - 20;
        if (newWidth > 150 && newWidth < 600) {
          setRulesWidth(newWidth);
        }
      }
    };
    const handleMouseUp = () => {
      setIsResizing(false);
      setIsResizingRules(false);
    };

    if (isResizing || isResizingRules) {
      window.addEventListener('mousemove', handleMouseMove);
      window.addEventListener('mouseup', handleMouseUp);
    }
    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, [isResizing, isResizingRules]);

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

      const isSingleArchive =
        normalizedFiles.length === 1 &&
        (normalizedFiles[0].relPath.toLowerCase().endsWith('.fmu') ||
          normalizedFiles[0].relPath.toLowerCase().endsWith('.ssp'));

      const target =
        discoveredRootRel || (normalizedFiles.length === 1 ? normalizedFiles[0].relPath : '.');
      console.log(`Executing validation with target: "${target}"`);

      const targetPtr = module.stackAlloc(target.length * 4 + 1);
      module.stringToUTF8(target, targetPtr, target.length * 4 + 1);
      module._run_validation(targetPtr);

      // After execution, build the tree
      let rootPath = discoveredRootRel ? `${workDir}/${discoveredRootRel}` : workDir;
      if (isSingleArchive) {
        // Find extracted directory (it starts with model_validation_ or model_cert_add_)
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
          justify-content: center;
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
          title="File Tree"
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
            <rect x="9" y="3" width="6" height="6" rx="1"></rect>
            <rect x="3" y="15" width="6" height="6" rx="1"></rect>
            <rect x="15" y="15" width="6" height="6" rx="1"></rect>
            <path d="M12 9v6"></path>
            <path d="M12 15H6"></path>
            <path d="M12 15h6"></path>
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
              multiple
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
              display: 'flex',
              backgroundColor: theme.border,
              borderRadius: '4px',
              border: `1px solid ${theme.border}`,
              overflow: 'hidden',
            }}
          >
            <div
              style={{
                width: rulesWidth,
                backgroundColor: theme.surface,
                overflowY: 'auto',
                padding: '10px',
                flexShrink: 0,
              }}
            >
              <RulesOutline headers={extractHeaders(rulesText)} theme={theme} />
            </div>
            {/* eslint-disable-next-line jsx-a11y/no-static-element-interactions */}
            <div
              onMouseDown={(e) => {
                e.preventDefault();
                setIsResizingRules(true);
              }}
              style={{
                width: '4px',
                cursor: 'col-resize',
                backgroundColor: isResizingRules ? '#007bff' : theme.border,
                transition: 'background-color 0.2s',
                zIndex: 10,
              }}
            />
            <div
              style={{
                flex: 1,
                minHeight: 0,
                overflowY: 'auto',
                padding: '0 20px',
                backgroundColor: theme.surface,
              }}
            >
              <MarkdownContent content={rulesText} theme={theme} isDark={isDark} />
            </div>
          </div>
        )}

        {activeTab === 'explorer' && fileTree && (
          <div
            style={{
              flex: 1,
              minHeight: 0,
              display: 'flex',
              backgroundColor: theme.border,
              borderRadius: '4px',
              border: `1px solid ${theme.border}`,
              overflow: 'hidden',
            }}
          >
            <div
              style={{
                width: explorerWidth,
                backgroundColor: theme.surface,
                overflowY: 'auto',
                padding: '10px',
                flexShrink: 0,
              }}
            >
              {fileTree.name.startsWith('model_validation_') ||
              fileTree.name.startsWith('model_cert_add_') ? (
                fileTree.children?.map((child) => (
                  <FileTreeItem
                    key={child.path}
                    node={child}
                    isSelected={child.path === selectedFile}
                    selectedFile={selectedFile}
                    setSelectedFile={setSelectedFile}
                    theme={theme}
                  />
                ))
              ) : (
                <FileTreeItem
                  node={fileTree}
                  isSelected={selectedFile === fileTree.path}
                  selectedFile={selectedFile}
                  setSelectedFile={setSelectedFile}
                  theme={theme}
                />
              )}
            </div>
            {/* eslint-disable-next-line jsx-a11y/no-static-element-interactions */}
            <div
              onMouseDown={(e) => {
                e.preventDefault();
                setIsResizing(true);
              }}
              style={{
                width: '4px',
                cursor: 'col-resize',
                backgroundColor: isResizing ? '#007bff' : theme.border,
                transition: 'background-color 0.2s',
                zIndex: 10,
              }}
            />
            <div
              style={{
                flex: 1,
                backgroundColor: theme.surface,
                minWidth: 0,
              }}
            >
              <FilePreview
                key={selectedFile}
                selectedFile={selectedFile}
                node={selectedFile ? fileMap.get(selectedFile) : null}
                module={module}
                theme={theme}
                isDark={isDark}
              />
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
              backgroundColor: theme.surface,
              borderRadius: '4px',
              border: `1px solid ${theme.border}`,
              overflow: 'hidden',
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
                right: '12px',
                zIndex: 1,
                padding: '5px',
                color: theme.text,
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
                backgroundColor: 'transparent',
                color: theme.text,
                padding: '15px',
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
            flexWrap: 'wrap',
          }}
        >
          <span>FSLint v{__APP_VERSION__}</span>
          <span>•</span>
          <span>Copyright © {__BUILD_YEAR__} FSLint Contributors</span>
          <span>•</span>
          <span>
            FSLint Core runs in WebAssembly using Emscripten. All processing is done locally in your
            browser.
          </span>
        </footer>
      </main>
    </div>
  );
}

export default App;
