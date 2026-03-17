import { useState, useEffect, useRef, useMemo } from 'react';
import { useFSLint } from './hooks/useFSLint';
import { FileNode } from './types';
import { FileTreeItem } from './components/FileTreeItem';
import { FilePreview } from './components/FilePreview';
import { RulesOutline } from './components/RulesOutline';
import { MarkdownContent } from './components/MarkdownContent';
import { extractHeaders, getFilesFromHandle, getFilesFromEntry } from './utils/file';

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
import { PrismLight as SyntaxHighlighter } from 'react-syntax-highlighter';

SyntaxHighlighter.registerLanguage('markup', markup);
SyntaxHighlighter.registerLanguage('xml', markup);
SyntaxHighlighter.registerLanguage('html', markup);
SyntaxHighlighter.registerLanguage('clike', clike);
SyntaxHighlighter.registerLanguage('markdown', markdown);
SyntaxHighlighter.registerLanguage('md', markdown);
SyntaxHighlighter.registerLanguage('cpp', cpp);
SyntaxHighlighter.registerLanguage('c++', cpp);
SyntaxHighlighter.registerLanguage('c', cpp);
SyntaxHighlighter.registerLanguage('json', json);
SyntaxHighlighter.registerLanguage('bash', bash);
SyntaxHighlighter.registerLanguage('sh', bash);
SyntaxHighlighter.registerLanguage('xsd', markup);
SyntaxHighlighter.registerLanguage('ssd', markup);
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

function App() {
  const { module, output, isReady, isProcessing, fileTree, setFileTree, processItems } =
    useFSLint();

  const [copied, setCopied] = useState(false);
  const [isDark, setIsDark] = useState(true);
  const [activeTab, setActiveTab] = useState<'certificate' | 'rules' | 'explorer'>('certificate');
  const [selectedFile, setSelectedFile] = useState<string | null>(null);
  const [rulesText, setRulesText] = useState<string>('');
  const [explorerWidth, setExplorerWidth] = useState(300);
  const [rulesWidth, setRulesWidth] = useState(250);
  const [isResizing, setIsResizing] = useState(false);
  const [isResizingRules, setIsResizingRules] = useState(false);

  const outputEndRef = useRef<HTMLPreElement>(null);
  const folderInputRef = useRef<HTMLInputElement>(null);

  const fileMap = useMemo(() => {
    const map = new Map<string, FileNode>();
    const traverse = (node: FileNode) => {
      map.set(node.path, node);
      node.children?.forEach(traverse);
    };
    if (fileTree) traverse(fileTree);
    return map;
  }, [fileTree]);

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

    const styleId = 'fslint-global-styles';
    let styleEl = document.getElementById(styleId);
    if (!styleEl) {
      styleEl = document.createElement('style');
      styleEl.id = styleId;
      document.head.appendChild(styleEl);
    }
    styleEl.textContent = `
      [class*="token"], .token, pre, code {
        text-shadow: none !important;
      }
      ::selection {
        text-shadow: none !important;
      }
      .react-syntax-highlighter-line-number {
        color: ${isDark ? '#858585' : '#999999'} !important;
        position: sticky !important;
        left: 0 !important;
        background-color: ${theme.surface} !important;
        z-index: 2 !important;
        display: inline-block !important;
      }
    `;
  }, [theme.bg, theme.text, theme.surface, isDark]);

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

  useEffect(() => {
    if (fileTree) {
      // eslint-disable-next-line react-hooks/set-state-in-effect
      setActiveTab('certificate');
    }
  }, [fileTree]);

  const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const files = event.target.files;
    if (files && files.length > 0) {
      setFileTree(null);
      setSelectedFile(null);
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
          setFileTree(null);
          setSelectedFile(null);
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
        setFileTree(null);
        setSelectedFile(null);
        await processItems(files);
      }
    }
  };

  const handleCopy = () => {
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
