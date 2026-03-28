import { useState, useEffect, useMemo } from 'react';
import Editor from '@monaco-editor/react';
import { FileNode, Theme, FSLintModule } from '../types';

import { RainbowCsvHighlighter } from './RainbowCsvHighlighter';
import { MarkdownContent } from './MarkdownContent';
import { decodeText } from '../utils/file';

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
  const [copied, setCopied] = useState(false);
  const [viewMode, setViewMode] = useState<'render' | 'code'>('render');

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
    return decodeText(data);
  }, [data, isBinaryResult, isStaticImage, isPdf]);

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
    if (ext === 'xml' || ext === 'xsd' || ext === 'ssd' || ext === 'svg') return 'xml';
    if (ext === 'html' || ext === 'htm') return 'html';
    if (ext === 'cpp' || ext === 'hpp' || ext === 'c' || ext === 'h') return 'cpp';
    if (ext === 'json') return 'json';
    if (ext === 'sh' || ext === 'bash') return 'shell';
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
      return 'xml';
    if (start.startsWith('<!doctype html') || start.includes('<html')) return 'html';

    return 'plaintext';
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
          minHeight: 0,
        }}
      >
        {viewMode === 'render' && canToggle ? (
          <div
            style={{
              flex: 1,
              display: 'flex',
              justifyContent: isHtml || isMarkdown ? 'flex-start' : 'center',
              padding: isHtml ? '0' : '20px',
              backgroundColor: 'transparent',
              minHeight: isHtml ? '100%' : '200px',
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
                src={`fmu-contents${selectedFile}`}
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
              <div style={{ flex: 1, minHeight: 0 }}>
                <Editor
                  height="100%"
                  language={language}
                  theme={isDark ? 'vs-dark' : 'light'}
                  value={content}
                  options={{
                    readOnly: true,
                    renderWhitespace: 'all',
                    fontSize: 13,
                    lineHeight: 21,
                    minimap: { enabled: true },
                    scrollBeyondLastLine: false,
                    automaticLayout: true,
                    contextmenu: false,
                    wordWrap: 'off',
                    scrollBeyondLastColumn: 2,
                    fontFamily: 'monospace',
                    lineNumbersMinChars: 3,
                  }}
                />
              </div>
            );
          })()
        )}
      </div>
    </div>
  );
};
