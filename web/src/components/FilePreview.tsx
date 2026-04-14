import { useState, useEffect, useMemo, useCallback } from 'react';
import Editor, { Monaco } from '@monaco-editor/react';
import { HexViewer } from './HexViewer';
import { FileNode, Theme, FSLintModule } from '../types';

import { MarkdownContent } from './MarkdownContent';
import { decodeText } from '../utils/file';
import { handleBeforeMount as commonHandleBeforeMount, commonEditorOptions } from '../utils/monaco';

/**
 * Detects the most likely separator for a CSV file based on consistency and frequency.
 * @param {string} text - The sample text from the CSV file.
 * @returns {string} The detected separator (',', ';', '\t', or '|').
 */
const detectSeparator = (text: string) => {
  const candidates = [',', ';', '\t', '|'];
  const lines = text.split('\n').slice(0, 20);

  if (lines.length === 0) return ',';

  let bestSeparator = ',';
  let maxScore = -1;

  for (const sep of candidates) {
    const counts = lines.map((line) => line.split(sep).length - 1);
    const average = counts.reduce((a, b) => a + b, 0) / counts.length;

    if (average === 0) continue;

    // Consistency: how much do the counts vary from the average?
    const variance = counts.reduce((a, b) => a + Math.pow(b - average, 2), 0) / counts.length;
    const score = average / (variance + 1); // Higher average, lower variance is better

    if (score > maxScore) {
      maxScore = score;
      bestSeparator = sep;
    }
  }

  return bestSeparator;
};

/**
 * Component for previewing various file types (images, PDFs, HTML, Markdown, CSV, Hex).
 * @param {Object} props - Component properties.
 * @param {string | null} props.selectedFile - The path of the currently selected file.
 * @param {FileNode | null | undefined} props.node - The file node data from the file tree.
 * @param {FSLintModule | null} props.module - The FSLint WASM module for reading files.
 * @param {Theme} props.theme - The current theme object.
 * @param {boolean} props.isDark - Whether dark mode is active.
 * @returns {JSX.Element | null} The rendered FilePreview component or null if no file is selected.
 */
export const FilePreview = ({
  selectedFile,
  node,
  module,
  theme,
  isDark,
}: {
  /**
   *
   */
  selectedFile: string | null;
  /**
   *
   */
  node: FileNode | null | undefined;
  /**
   *
   */
  module: FSLintModule | null;
  /**
   *
   */
  theme: Theme;
  /**
   *
   */
  isDark: boolean;
}) => {
  const [imageUrl, setImageUrl] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);
  const [viewMode, setViewMode] = useState<'render' | 'code' | 'hex'>('render');
  const [monaco, setMonaco] = useState<Monaco | null>(null);

  const ext = selectedFile?.split('.').pop()?.toLowerCase();
  const isStaticImage = ext === 'png' || ext === 'jpg' || ext === 'jpeg';
  const isSvg = ext === 'svg';
  const isPdf = ext === 'pdf';
  const isHtml = ext === 'html' || ext === 'htm';
  const isMarkdown = ext === 'md';

  const isBinaryResult = node?.isBinary ?? false;

  const canRender = isStaticImage || isSvg || isPdf || isHtml || isMarkdown;
  const canHex = isBinaryResult;

  const [lastSelectedFile, setLastSelectedFile] = useState<string | null>(null);

  if (selectedFile !== lastSelectedFile) {
    setLastSelectedFile(selectedFile);
    if (canRender) {
      setViewMode('render');
    } else if (canHex) {
      setViewMode('hex');
    } else {
      setViewMode('code');
    }
  }

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

  const data = useMemo(() => {
    if (!selectedFile || !module) return null;
    try {
      return module.FS.readFile(selectedFile) as Uint8Array;
    } catch (e) {
      console.error('Failed to read file:', e);
      return null;
    }
  }, [selectedFile, module]);

  const content = useMemo(() => {
    if (!data) return '';
    if (viewMode === 'hex') return '';
    // If it's a known binary format that we render (image, pdf), don't try to decode as text unless we are in code mode
    if (isBinaryResult && (isStaticImage || isPdf) && viewMode === 'render') return '';
    return decodeText(data);
  }, [data, isBinaryResult, isStaticImage, isPdf, viewMode]);

  /**
   * Configures the Monaco editor before it is mounted.
   * Registers the CSV language and custom themes.
   * @param {Monaco} monaco - The Monaco editor instance.
   */
  const handleBeforeMount = useCallback((monaco: Monaco) => {
    commonHandleBeforeMount(monaco);

    const darkColors = [
      '#ff5555',
      '#50fa7b',
      '#f1fa8c',
      '#bd93f9',
      '#ff79c6',
      '#8be9fd',
      '#ffb86c',
    ];
    const lightColors = [
      '#e45649',
      '#50a14f',
      '#c18401',
      '#4078f2',
      '#a626a4',
      '#0184bc',
      '#986801',
    ];

    // Re-define themes with CSV rules included
    monaco.editor.defineTheme('fslint-dark', {
      base: 'vs-dark',
      inherit: true,
      rules: [
        { token: 'status-pass', foreground: '4caf50', fontStyle: 'bold' },
        { token: 'status-fail', foreground: 'f44336', fontStyle: 'bold' },
        { token: 'status-warn', foreground: 'ff9800', fontStyle: 'bold' },
        { token: 'header', foreground: '569cd6', fontStyle: 'bold' },
        { token: 'separator', foreground: '808080' },
        { token: 'box-drawing', foreground: 'd4d4d4' },
        ...Array.from({ length: 10 }).map((_, i) => ({
          token: `csv-col-${i}`,
          foreground: darkColors[i % darkColors.length],
        })),
      ],
      colors: {
        'editor.background': '#2a2a2a',
        'editor.foreground': '#f0f0f0',
        'editor.lineHighlightBackground': '#333333',
        'editorLineNumber.foreground': '#858585',
        'editorLineNumber.activeForeground': '#c6c6c6',
      },
    });

    monaco.editor.defineTheme('fslint-light', {
      base: 'vs',
      inherit: true,
      rules: [
        { token: 'status-pass', foreground: '2e7d32', fontStyle: 'bold' },
        { token: 'status-fail', foreground: 'c62828', fontStyle: 'bold' },
        { token: 'status-warn', foreground: 'ef6c00', fontStyle: 'bold' },
        { token: 'header', foreground: '005a9e', fontStyle: 'bold' },
        { token: 'separator', foreground: 'a0a0a0' },
        { token: 'box-drawing', foreground: '333333' },
        ...Array.from({ length: 10 }).map((_, i) => ({
          token: `csv-col-${i}`,
          foreground: lightColors[i % lightColors.length],
        })),
      ],
      colors: {
        'editor.background': '#ffffff',
        'editor.foreground': '#111418',
        'editor.lineHighlightBackground': '#f3f3f3',
        'editorLineNumber.foreground': '#999999',
        'editorLineNumber.activeForeground': '#333333',
      },
    });
  }, []);

  useEffect(() => {
    if (!monaco) return;

    const isCsv = selectedFile?.toLowerCase().endsWith('.csv');
    if (!isCsv) return;

    const separator = detectSeparator(content);
    const escapedSeparator = separator.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

    monaco.languages.setMonarchTokensProvider('csv', {
      tokenizer: {
        root: [[/^/, { token: '', next: '@column0' }]],
        // Generate rules for each column (up to 10 for rainbow effect)
        ...Object.fromEntries(
          Array.from({ length: 10 }).map((_, i) => [
            `column${i}`,
            [
              ...(i > 0 ? [[/^/, { token: '', next: '@column0' }]] : []), // Reset to first column at start of line, but NOT in column0 to avoid infinite loops
              [/"/, { token: `csv-col-${i}`, next: `@quotedColumn${i}` }], // Start of quoted field
              [new RegExp(`[^${escapedSeparator}"]+`), `csv-col-${i}`], // Unquoted field content
              [
                new RegExp(escapedSeparator),
                { token: 'delimiter', next: i < 9 ? `@column${i + 1}` : '@column0' },
              ], // Delimiter
              [/$/, { token: '', next: '@root' }], // End of line
            ],
          ]),
        ),
        // Quoted field states
        ...Object.fromEntries(
          Array.from({ length: 10 }).map((_, i) => [
            `quotedColumn${i}`,
            [
              [/^/, { token: '', next: '@column0' }], // Reset at start of line
              [/""/, `csv-col-${i}`], // Escaped quote
              [/"/, { token: `csv-col-${i}`, next: `@column${i}` }], // End of quoted field
              [/[^"]+/, `csv-col-${i}`], // Quoted field content
            ],
          ]),
        ),
      },
    });
  }, [monaco, content, selectedFile]);

  if (!selectedFile || !module) return null;

  /**
   * Copies the current file content (or hex representation) to the clipboard.
   */
  const handleCopy = () => {
    if (viewMode === 'hex' && data) {
      // Safety limit for large files to avoid hanging the UI
      const limit = 1024 * 1024; // 1MB
      const slice = data.length > limit ? data.slice(0, limit) : data;
      const hex = Array.from(slice)
        .map((b) => b.toString(16).padStart(2, '0'))
        .join(' ');
      const textToCopy = data.length > limit ? `${hex}\n... (truncated, file too large)` : hex;

      navigator.clipboard.writeText(textToCopy).then(() => {
        setCopied(true);
        setTimeout(() => setCopied(false), 2000);
      });
      return;
    }
    navigator.clipboard.writeText(content).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  /**
   * Determines the Monaco language ID based on the file extension and content.
   * @param {string} filename - The file name.
   * @param {string} text - The file content.
   * @returns {string} The language ID.
   */
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
        {canRender && (
          <button
            onClick={() =>
              setViewMode(viewMode === 'render' ? (canHex ? 'hex' : 'code') : 'render')
            }
            title={viewMode === 'render' ? (canHex ? 'Show Hex' : 'Show Code') : 'Show Preview'}
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
          overflowY: viewMode === 'render' && canRender ? 'auto' : 'hidden',
          display: 'flex',
          flexDirection: 'column',
          minHeight: 0,
        }}
      >
        {viewMode === 'render' && canRender ? (
          <div
            style={{
              flex: 1,
              display: 'flex',
              justifyContent: isHtml || isMarkdown ? 'flex-start' : 'center',
              padding: isHtml || isPdf ? '0' : '20px',
              backgroundColor: 'transparent',
              minHeight: isHtml || isPdf || isStaticImage || isSvg ? '100%' : '200px',
              boxSizing: 'border-box',
            }}
          >
            {isStaticImage ? (
              imageUrl ? (
                <img
                  src={imageUrl}
                  alt={selectedFile}
                  style={{ width: '100%', height: '100%', objectFit: 'contain' }}
                />
              ) : (
                <div style={{ color: '#ff5555' }}>Failed to load Image</div>
              )
            ) : isSvg ? (
              imageUrl ? (
                <img
                  src={imageUrl}
                  alt={selectedFile}
                  style={{ width: '100%', height: '100%', objectFit: 'contain' }}
                />
              ) : (
                <div style={{ color: '#ff5555' }}>Failed to load SVG</div>
              )
            ) : isPdf ? (
              imageUrl ? (
                <iframe
                  src={imageUrl}
                  title="PDF Preview"
                  style={{ width: '100%', height: '100%', border: 'none' }}
                />
              ) : (
                <div style={{ color: '#ff5555' }}>Failed to load PDF</div>
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
        ) : viewMode === 'hex' ? (
          <div
            style={{
              flex: 1,
              minHeight: 0,
              backgroundColor: isDark ? '#2a2a2a' : '#ffffff',
              color: theme.text,
            }}
          >
            {data && <HexViewer data={data} theme={theme} isDark={isDark} />}
          </div>
        ) : (
          (() => {
            const language = getLanguage(selectedFile, content);

            return (
              <div style={{ flex: 1, minHeight: 0 }}>
                <Editor
                  height="100%"
                  language={language}
                  theme={isDark ? 'fslint-dark' : 'fslint-light'}
                  beforeMount={handleBeforeMount}
                  onMount={(editor, monaco) => setMonaco(monaco)}
                  value={content}
                  options={{
                    ...commonEditorOptions,
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
