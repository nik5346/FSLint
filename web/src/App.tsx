import { useState, useEffect, useRef, useMemo, useCallback } from 'react';
import Editor from '@monaco-editor/react';
import { useFSLint } from './hooks/useFSLint';
import { FileNode, NestedModelResult, ValidationResult } from './types';
import { FileTreeItem } from './components/FileTreeItem';
import { FilePreview } from './components/FilePreview';
import { ModelTree } from './components/ModelTree';
import { ModelInfo } from './components/ModelInfo';
import { RulesOutline } from './components/RulesOutline';
import { MarkdownContent } from './components/MarkdownContent';
import { extractHeaders, getFilesFromHandle, getFilesFromEntry } from './utils/file';
import { handleBeforeMount, commonEditorOptions } from './utils/monaco';

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

/**
 * The main Application component.
 * Manages the layout, tabs, theme, and file uploading/processing.
 * @returns {JSX.Element} The rendered App component.
 */
function App() {
  const {
    module,
    output,
    isReady,
    isProcessing,
    fileTree,
    setFileTree,
    validationResult,
    processItems,
    downloadCertifiedModel,
  } = useFSLint();

  const [copied, setCopied] = useState(false);
  const [isDark, setIsDark] = useState(true);
  const [activeTab, setActiveTab] = useState<'info' | 'certificate' | 'rules' | 'explorer'>(
    'rules',
  );
  const [selectedFile, setSelectedFile] = useState<string | null>(null);
  const [rulesText, setRulesText] = useState<string>('');
  const [rulesViewMode, setRulesViewMode] = useState<'render' | 'code'>('render');
  const [explorerWidth, setExplorerWidth] = useState(200);
  const [modelTreeWidth, setModelTreeWidth] = useState(190);
  const [rulesWidth, setRulesWidth] = useState(250);
  const [isResizing, setIsResizing] = useState(false);
  const [isResizingModelTree, setIsResizingModelTree] = useState(false);
  const [isResizingRules, setIsResizingRules] = useState(false);
  const [isDragging, setIsDragging] = useState(false);
  const [activeRuleLine, setActiveRuleLine] = useState<number | undefined>();
  const [selectedModelPath, setSelectedModelPath] = useState<string | null>(null);

  const outputEndRef = useRef<HTMLPreElement>(null);
  const folderInputRef = useRef<HTMLInputElement>(null);
  const dragCounter = useRef(0);
  const rulesScrollRef = useRef<HTMLDivElement>(null);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const rulesEditorRef = useRef<any>(null);

  const selectedNode = useMemo(() => {
    if (!validationResult) return null;
    if (!selectedModelPath) return validationResult;

    let found: NestedModelResult | ValidationResult | null = null;
    /**
     * Recursively finds a nested model result by its logical path.
     * @param {NestedModelResult[]} nodes - The array of nodes to search.
     */
    const findNode = (nodes: NestedModelResult[]) => {
      for (const node of nodes) {
        if (found) return;
        if (node.logical_path === selectedModelPath) {
          found = node;
          return;
        }
        if (node.nested_models) findNode(node.nested_models);
      }
    };
    findNode(validationResult.nested_models);
    return found;
  }, [validationResult, selectedModelPath]);

  const selectedFileTree = useMemo(() => {
    if (!selectedNode) return null;
    if ('file_tree' in selectedNode) {
      return (
        (selectedNode as ValidationResult).file_tree ||
        (selectedNode as NestedModelResult).file_tree
      );
    }
    return null;
  }, [selectedNode]);

  const fileMap = useMemo(() => {
    const map = new Map<string, FileNode>();
    /**
     * Recursively traverses the file tree and populates the map.
     * @param {FileNode} node - The current node to traverse.
     */
    const traverse = (node: FileNode) => {
      map.set(node.path, node);
      node.children?.forEach(traverse);
    };
    if (selectedFileTree) traverse(selectedFileTree);
    return map;
  }, [selectedFileTree]);

  const rulesHeaders = useMemo(() => extractHeaders(rulesText), [rulesText]);

  /**
   * Updates the active rule line based on the scroll position of the rules container.
   */
  const handleRulesScroll = useCallback(() => {
    if (rulesViewMode === 'code' && rulesEditorRef.current) {
      const topVisibleLine = rulesEditorRef.current.getVisibleRanges()[0]?.startLineNumber || 1;
      let currentHeaderLine = rulesHeaders[0]?.line;
      for (const header of rulesHeaders) {
        if (header.line <= topVisibleLine) {
          currentHeaderLine = header.line;
        } else {
          break;
        }
      }
      setActiveRuleLine(currentHeaderLine);
      return;
    }

    if (!rulesScrollRef.current) return;

    const container = rulesScrollRef.current;
    const scrollPos = container.scrollTop + 50; // buffer

    let currentHeaderLine = rulesHeaders[0]?.line;
    for (const header of rulesHeaders) {
      const el = document.getElementById(`line-${header.line}`);
      if (el && el.offsetTop <= scrollPos) {
        currentHeaderLine = header.line;
      } else if (el && el.offsetTop > scrollPos) {
        break;
      }
    }
    setActiveRuleLine(currentHeaderLine);
  }, [rulesHeaders, rulesViewMode]);

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
        background-color: ${isDark ? 'rgba(0, 123, 255, 0.4)' : 'rgba(0, 123, 255, 0.25)'} !important;
      }
      .whitespace-marker {
        position: relative;
      }
      .whitespace-marker::before {
        content: attr(data-marker);
        position: absolute;
        left: 0;
        top: 50%;
        transform: translateY(-50%);
        color: ${isDark ? '#fff' : '#000'};
        opacity: 0.3;
        font-size: 0.8em;
        pointer-events: none;
        user-select: none;
        white-space: pre;
      }
      .whitespace-marker[data-marker-type="tab"]::before {
        opacity: 0.3;
      }
      .react-syntax-highlighter-line-number {
        color: ${isDark ? '#858585' : '#999999'} !important;
        position: sticky !important;
        left: 0 !important;
        background-color: ${theme.surface} !important;
        z-index: 2 !important;
        display: inline-block !important;
      }
      :root {
        --status-pass: #4caf50;
        --status-fail: #f44336;
        --status-warn: #ff9800;
        --status-security: #0288d1;
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
    /**
     * Handles the mouse move event for resizing panels.
     * @param {MouseEvent} e - The mouse move event.
     */
    const handleMouseMove = (e: MouseEvent) => {
      if (isResizing) {
        const newWidth = e.clientX - 64 - 20 - modelTreeWidth - 4;
        if (newWidth > 150 && newWidth < 800) {
          setExplorerWidth(newWidth);
        }
      } else if (isResizingModelTree) {
        const newWidth = e.clientX - 64 - 20;
        if (newWidth > 100 && newWidth < 400) {
          setModelTreeWidth(newWidth);
        }
      } else if (isResizingRules) {
        const newWidth = e.clientX - 64 - 20;
        if (newWidth > 150 && newWidth < 600) {
          setRulesWidth(newWidth);
        }
      }
    };
    /**
     * Handles the mouse up event to stop resizing.
     */
    const handleMouseUp = () => {
      setIsResizing(false);
      setIsResizingModelTree(false);
      setIsResizingRules(false);
    };

    if (isResizing || isResizingModelTree || isResizingRules) {
      window.addEventListener('mousemove', handleMouseMove);
      window.addEventListener('mouseup', handleMouseUp);
    }
    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, [isResizing, isResizingModelTree, isResizingRules, modelTreeWidth]);

  useEffect(() => {
    if (validationResult) {
      // If validation failed and we don't even have a file tree, show the certificate
      if (validationResult.overallStatus === 'FAIL' && !fileTree) {
        // eslint-disable-next-line react-hooks/set-state-in-effect
        setActiveTab('certificate');
      } else {
        // Otherwise default to the info tab
        setActiveTab('info');
      }
    }
  }, [validationResult, fileTree]);

  /**
   * Resets the selected model path when the validation result changes.
   */
  useEffect(() => {
    // eslint-disable-next-line react-hooks/set-state-in-effect
    setSelectedModelPath(null);
  }, [validationResult]);

  /**
   * Handles file selection from the hidden input.
   * @param {React.ChangeEvent<HTMLInputElement>} event - The change event.
   */
  const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const files = event.target.files;
    if (files && files.length > 0) {
      setFileTree(null);
      setSelectedFile(null);
      await processItems(Array.from(files));
    }
    event.target.value = '';
  };

  /**
   * Handles folder selection, using the directory picker API if available.
   * @param {React.MouseEvent} event - The click event.
   */
  const handleFolderSelect = async (event: React.MouseEvent) => {
    event.preventDefault();
    if (window.showDirectoryPicker) {
      try {
        const handle = await window.showDirectoryPicker();
        const items = await getFilesFromHandle(handle);
        if (items.files.length > 0 || items.directories.length > 0) {
          setFileTree(null);
          setSelectedFile(null);
          await processItems(items);
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

  /**
   * Handles the drag enter event for the drop zone.
   * @param {React.DragEvent<HTMLElement>} event - The drag event.
   */
  const handleDragEnter = (event: React.DragEvent<HTMLElement>) => {
    event.preventDefault();
    dragCounter.current += 1;
    if (dragCounter.current === 1) {
      setIsDragging(true);
    }
  };

  /**
   * Handles the drag leave event for the drop zone.
   * @param {React.DragEvent<HTMLElement>} event - The drag event.
   */
  const handleDragLeave = (event: React.DragEvent<HTMLElement>) => {
    event.preventDefault();
    dragCounter.current -= 1;
    if (dragCounter.current === 0) {
      setIsDragging(false);
    }
  };

  /**
   * Handles the drop event for the drop zone.
   * @param {React.DragEvent<HTMLElement>} event - The drop event.
   */
  const onDrop = async (event: React.DragEvent<HTMLElement>) => {
    event.preventDefault();
    dragCounter.current = 0;
    setIsDragging(false);
    const items = event.dataTransfer.items;
    if (items && items.length > 0) {
      const allFiles: File[] = [];
      const allDirs: string[] = [];
      for (let i = 0; i < items.length; i++) {
        const entry = items[i].webkitGetAsEntry();
        if (entry) {
          const collected = await getFilesFromEntry(entry);
          allFiles.push(...collected.files);
          allDirs.push(...collected.directories);
        }
      }
      if (allFiles.length > 0 || allDirs.length > 0) {
        setFileTree(null);
        setSelectedFile(null);
        await processItems({ files: allFiles, directories: allDirs });
      }
    }
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
          className={`tab-btn ${activeTab === 'info' ? 'active' : ''}`}
          onClick={() => setActiveTab('info')}
          disabled={!validationResult}
          style={{
            opacity: validationResult ? 1 : 0.3,
            cursor: validationResult ? 'pointer' : 'default',
          }}
          title="Model Information"
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
            <circle cx="12" cy="12" r="10"></circle>
            <line x1="12" y1="16" x2="12" y2="12"></line>
            <line x1="12" y1="8" x2="12.01" y2="8"></line>
          </svg>
        </button>
        <button
          className={`tab-btn ${activeTab === 'certificate' ? 'active' : ''}`}
          onClick={() => setActiveTab('certificate')}
          disabled={!validationResult}
          style={{
            opacity: validationResult ? 1 : 0.3,
            cursor: validationResult ? 'pointer' : 'default',
          }}
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
          disabled={!validationResult}
          style={{
            opacity: validationResult ? 1 : 0.3,
            cursor: validationResult ? 'pointer' : 'default',
          }}
          title="Explorer"
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
            onDragEnter={handleDragEnter}
            onDragLeave={handleDragLeave}
            onDrop={onDrop}
            style={{
              border: `2px dashed ${isDragging ? '#007bff' : theme.border}`,
              borderRadius: '8px',
              padding: '20px',
              textAlign: 'center',
              cursor: isReady && !isProcessing ? 'default' : 'wait',
              opacity: isReady && !isProcessing ? 1 : 0.6,
              background: isDragging
                ? isDark
                  ? 'rgba(0, 123, 255, 0.1)'
                  : 'rgba(0, 123, 255, 0.05)'
                : theme.surface,
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
            <div
              style={{
                filter: isDragging ? 'blur(2px)' : 'none',
                transition: 'filter 0.2s',
                pointerEvents: isDragging ? 'none' : 'auto',
              }}
            >
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
          </div>

          <div
            style={{
              display: 'flex',
              alignItems: 'stretch',
              flexShrink: 0,
            }}
          >
            <button
              onClick={downloadCertifiedModel}
              disabled={
                !validationResult ||
                (validationResult.overallStatus !== 'PASS' &&
                  validationResult.overallStatus !== 'WARNING') ||
                isProcessing
              }
              title={
                validationResult?.overallStatus === 'PASS' ||
                validationResult?.overallStatus === 'WARNING'
                  ? 'Download Certified Model'
                  : 'Model must pass validation to be certified'
              }
              className="icon-btn"
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                width: '80px',
                borderRadius: '8px',
                border: `1px solid ${theme.buttonBorder}`,
                color: theme.text,
                cursor:
                  (validationResult?.overallStatus === 'PASS' ||
                    validationResult?.overallStatus === 'WARNING') &&
                  !isProcessing
                    ? 'pointer'
                    : 'default',
                transition: 'background-color 0.15s, border-color 0.15s',
                flexShrink: 0,
                opacity:
                  (validationResult?.overallStatus === 'PASS' ||
                    validationResult?.overallStatus === 'WARNING') &&
                  !isProcessing
                    ? 1
                    : 0.3,
              }}
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
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                <polyline points="7 10 12 15 17 10"></polyline>
                <line x1="12" y1="15" x2="12" y2="3"></line>
              </svg>
            </button>
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
              <RulesOutline
                headers={rulesHeaders}
                theme={theme}
                activeLine={activeRuleLine}
                onSelect={(line) => {
                  if (rulesViewMode === 'code' && rulesEditorRef.current) {
                    rulesEditorRef.current.revealLineInCenter(line);
                  } else {
                    const el = document.getElementById(`line-${line}`);
                    if (el) el.scrollIntoView({ behavior: 'smooth' });
                  }
                }}
              />
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
                position: 'relative',
                display: 'flex',
                flexDirection: 'column',
                backgroundColor: theme.surface,
                overflow: 'hidden',
              }}
            >
              <div
                style={{
                  position: 'absolute',
                  top: '6px',
                  right: '30px',
                  zIndex: 10,
                  display: 'flex',
                  gap: '4px',
                }}
              >
                <button
                  onClick={() => setRulesViewMode(rulesViewMode === 'render' ? 'code' : 'render')}
                  title={rulesViewMode === 'render' ? 'Show Code' : 'Show Preview'}
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
                  {rulesViewMode === 'render' ? (
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
                      <polyline points="16 18 22 12 16 6" />
                      <polyline points="8 6 2 12 8 18" />
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
                      <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z" />
                      <circle cx="12" cy="12" r="3" />
                    </svg>
                  )}
                </button>
                <button
                  onClick={() => {
                    navigator.clipboard.writeText(rulesText).then(() => {
                      setCopied(true);
                      setTimeout(() => setCopied(false), 2000);
                    });
                  }}
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
                ref={rulesViewMode === 'render' ? rulesScrollRef : null}
                onScroll={rulesViewMode === 'render' ? handleRulesScroll : undefined}
                style={{
                  height: '100%',
                  padding: rulesViewMode === 'render' ? '0 20px' : 0,
                  overflowY: rulesViewMode === 'render' ? 'auto' : 'hidden',
                }}
              >
                {rulesViewMode === 'render' ? (
                  <MarkdownContent content={rulesText} theme={theme} isDark={isDark} />
                ) : (
                  <Editor
                    height="100%"
                    language="markdown"
                    theme={isDark ? 'vs-dark' : 'vs-light'}
                    beforeMount={handleBeforeMount}
                    value={rulesText}
                    onMount={(editor) => {
                      rulesEditorRef.current = editor;
                      editor.onDidScrollChange(handleRulesScroll);
                      if (activeRuleLine) {
                        editor.revealLineInCenter(activeRuleLine);
                      }
                    }}
                    options={{
                      ...commonEditorOptions,
                      lineNumbers: 'on',
                    }}
                  />
                )}
              </div>
            </div>
          </div>
        )}

        {activeTab !== 'rules' && validationResult && (
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
            {/* Shared Left Sidebar: Model Tree */}
            {validationResult.nested_models && validationResult.nested_models.length > 0 && (
              <>
                <div
                  style={{
                    width: modelTreeWidth,
                    backgroundColor: theme.surface,
                    overflowY: 'auto',
                    padding: '10px',
                    flexShrink: 0,
                    borderRight: `1px solid ${theme.border}`,
                  }}
                >
                  <ModelTree
                    root={validationResult}
                    selectedPath={selectedModelPath}
                    onSelect={(path) => {
                      setSelectedModelPath(path);
                      setSelectedFile(null);
                    }}
                  />
                </div>
                {/* eslint-disable-next-line jsx-a11y/no-static-element-interactions */}
                <div
                  onMouseDown={(e) => {
                    e.preventDefault();
                    setIsResizingModelTree(true);
                  }}
                  style={{
                    width: '4px',
                    cursor: 'col-resize',
                    backgroundColor: isResizingModelTree ? '#007bff' : theme.border,
                    transition: 'background-color 0.2s',
                    zIndex: 10,
                  }}
                />
              </>
            )}

            {/* Tab-specific Content */}
            <div style={{ flex: 1, display: 'flex', minWidth: 0 }}>
              {activeTab === 'info' && selectedNode && (
                <div style={{ flex: 1, backgroundColor: theme.surface, overflowY: 'auto' }}>
                  <ModelInfo result={selectedNode} theme={theme} isDark={isDark} module={module} />
                </div>
              )}

              {activeTab === 'certificate' && selectedNode && (
                <div
                  style={{
                    position: 'relative',
                    flex: 1,
                    minHeight: 0,
                    display: 'flex',
                    flexDirection: 'column',
                    backgroundColor: theme.surface,
                    overflow: 'hidden',
                  }}
                >
                  <button
                    onClick={() => {
                      const report =
                        'overallStatus' in selectedNode
                          ? (selectedNode as ValidationResult).report
                          : (selectedNode as NestedModelResult).report;
                      // eslint-disable-next-line no-control-regex
                      const stripped = report.replace(/\x1b\[[0-9;]*m/g, '');
                      navigator.clipboard.writeText(stripped).then(() => {
                        setCopied(true);
                        setTimeout(() => setCopied(false), 2000);
                      });
                    }}
                    disabled={!selectedNode.report}
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
                      cursor: selectedNode.report ? 'pointer' : 'default',
                      opacity: selectedNode.report ? 0.6 : 0.2,
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
                  <div style={{ flex: 1, minHeight: 0 }}>
                    <Editor
                      height="100%"
                      language="fslint-report"
                      theme={isDark ? 'fslint-dark' : 'fslint-light'}
                      beforeMount={handleBeforeMount}
                      value={(
                        ('overallStatus' in selectedNode
                          ? (selectedNode as ValidationResult).report
                          : (selectedNode as NestedModelResult).report) || ''
                      ).replace(
                        // eslint-disable-next-line no-control-regex
                        /\x1b\[[0-9;]*m/g,
                        '',
                      )}
                      options={commonEditorOptions}
                    />
                  </div>
                </div>
              )}

              {activeTab === 'explorer' && selectedNode && (
                <div style={{ flex: 1, display: 'flex', minWidth: 0 }}>
                  <div
                    style={{
                      width: explorerWidth,
                      backgroundColor: theme.surface,
                      overflowY: 'auto',
                      padding: '10px',
                      flexShrink: 0,
                    }}
                  >
                    {selectedFileTree ? (
                      selectedFileTree.name.startsWith('model_validation_') ||
                      selectedFileTree.name.startsWith('model_cert_add_') ? (
                        selectedFileTree.children?.map((child) => (
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
                          node={selectedFileTree}
                          isSelected={selectedFile === selectedFileTree.path}
                          selectedFile={selectedFile}
                          setSelectedFile={setSelectedFile}
                          theme={theme}
                        />
                      )
                    ) : (
                      <div style={{ color: theme.muted, fontSize: '0.9em', textAlign: 'center' }}>
                        File tree not available for this model
                      </div>
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
                  <div style={{ flex: 1, backgroundColor: theme.surface, minWidth: 0 }}>
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
            </div>
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
