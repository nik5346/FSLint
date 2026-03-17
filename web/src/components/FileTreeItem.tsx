import { useState, memo } from 'react';
import { FileNode, Theme } from '../types';
import { formatBytes } from '../utils/format';

export const FileTreeItem = memo(function FileTreeItem({
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
        <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', flex: 1 }}>{node.name}</span>
        {!isDir && node.size !== undefined && (
          <span style={{ color: theme.muted, fontSize: '0.85em', flexShrink: 0 }}>
            {formatBytes(node.size)}
          </span>
        )}
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
