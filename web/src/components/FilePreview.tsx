import { useState, useEffect, useRef, useMemo } from 'react';
import { PrismLight as SyntaxHighlighter } from 'react-syntax-highlighter';
import { vscDarkPlus, prism } from 'react-syntax-highlighter/dist/esm/styles/prism';
import { FileNode, Theme, FSLintModule } from '../types';
import { createElement } from 'react-syntax-highlighter';

// Utility to strip textShadow from Prism styles to fix "ghosting"
export const stripTextShadow = (style: { [key: string]: React.CSSProperties }) => {
  const newStyle: { [key: string]: React.CSSProperties } = {};
  for (const key in style) {
    if (Object.prototype.hasOwnProperty.call(style, key)) {
      newStyle[key] = { ...style[key], textShadow: 'none' };
    }
  }
  return newStyle;
};

export const whitespaceRenderer = () => {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  return ({ rows, stylesheet, useInlineStyles }: any): any => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const transformNode = (node: any): any => {
      if (!node) return node;
      if (node.type === 'text' && typeof node.value === 'string') {
        const parts = node.value.split(/([ \t]+)/);
        if (parts.length === 1 && !/^[ \t]+$/.test(parts[0])) return node;

        return {
          type: 'element',
          tagName: 'span',
          properties: { className: [] },
          children: parts.flatMap((part: string) => {
            if (/^[ \t]+$/.test(part)) {
              return part.split('').map((char) => ({
                type: 'element',
                tagName: 'span',
                properties: {
                  className: ['whitespace-marker'],
                  'data-marker': char === '\t' ? '→' : '·',
                  'data-marker-type': char === '\t' ? 'tab' : 'space',
                },
                children: [{ type: 'text', value: char }],
              }));
            }
            return [{ type: 'text', value: part }];
          }),
        };
      }
      if (node.children) {
        return { ...node, children: node.children.map(transformNode) };
      }
      return node;
    };

    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    return rows.map((node: any, i: number) =>
      createElement({
        node: transformNode(node),
        stylesheet,
        useInlineStyles,
        key: `code-segment-${i}`,
      }),
    );
  };
};

import { RainbowCsvHighlighter } from './RainbowCsvHighlighter';
import { MarkdownContent } from './MarkdownContent';

export const FilePreview = ({
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

  const memoizedWhitespaceRenderer = useMemo(() => whitespaceRenderer(), []);

  const syntaxStyle = useMemo(() => stripTextShadow(isDark ? vscDarkPlus : prism), [isDark]);

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
                style={syntaxStyle}
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
                codeTagProps={{ style: { display: 'inline-block', minWidth: '100%' } }}
                customStyle={{
                  margin: 0,
                  padding: '15px 0',
                  fontSize: '0.9em',
                  backgroundColor: 'transparent',
                  flex: 1,
                  lineHeight: '1.5em',
                  overflow: 'auto',
                }}
                renderer={memoizedWhitespaceRenderer}
                wrapLines={true}
                lineProps={{
                  style: {
                    display: 'flex',
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
