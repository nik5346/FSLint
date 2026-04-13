import React from 'react';
import { NestedModelResult, ValidationResult } from '../types';

/**
 * Props for the ModelTreeItem component.
 */
export interface ModelTreeItemProps {
  /** The nested model result node or the root validation result. */
  node: NestedModelResult | ValidationResult;
  /** Nesting depth for indentation. */
  depth: number;
  /** Whether this node is currently selected. */
  isSelected: boolean;
  /** Callback when the node is selected. */
  onSelect: (node: NestedModelResult | ValidationResult) => void;
  /** Whether this node's children are expanded. */
  isExpanded: boolean;
  /** Callback to toggle expansion. */
  onToggle: () => void;
}

/**
 * Renders a single row in the model tree.
 * @param {ModelTreeItemProps} props - The component props.
 * @returns {JSX.Element} The rendered component.
 */
export const ModelTreeItem: React.FC<ModelTreeItemProps> = ({
  node,
  depth,
  isSelected,
  onSelect,
  isExpanded,
  onToggle,
}) => {
  const hasChildren =
    'nested_models' in node && node.nested_models && node.nested_models.length > 0;

  /**
   * Gets the color corresponding to a test status.
   * @param {'PASS' | 'FAIL' | 'WARNING'} status - The test status.
   * @returns {string} The CSS color value.
   */
  const getStatusColor = (status: 'PASS' | 'FAIL' | 'WARNING') => {
    switch (status) {
      case 'PASS':
        return 'var(--status-pass)';
      case 'FAIL':
        return 'var(--status-fail)';
      case 'WARNING':
        return 'var(--status-warn)';
      default:
        return 'transparent';
    }
  };

  const status = 'overallStatus' in node ? node.overallStatus : node.status;
  const name = ('name' in node && node.name) || (node.summary && node.summary.model_name) || 'Root';

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        userSelect: 'none',
      }}
    >
      <div
        onClick={() => onSelect(node)}
        onKeyDown={(e) => (e.key === 'Enter' || e.key === ' ') && onSelect(node)}
        role="button"
        tabIndex={0}
        style={{
          display: 'flex',
          alignItems: 'center',
          padding: '4px 8px',
          paddingLeft: `${depth * 16 + 8}px`,
          cursor: 'pointer',
          backgroundColor: isSelected ? 'var(--btn-hover-bg)' : 'transparent',
          borderRadius: '4px',
          gap: '8px',
          fontSize: '14px',
        }}
      >
        <span
          onClick={(e) => {
            e.stopPropagation();
            onToggle();
          }}
          onKeyDown={(e) => {
            if (e.key === 'Enter' || e.key === ' ') {
              e.stopPropagation();
              onToggle();
            }
          }}
          role="button"
          tabIndex={0}
          aria-label={isExpanded ? `Collapse ${name}` : `Expand ${name}`}
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            width: '16px',
            height: '16px',
            visibility: hasChildren ? 'visible' : 'hidden',
            cursor: 'pointer',
            transform: isExpanded ? 'rotate(90deg)' : 'none',
            transition: 'transform 0.2s',
          }}
        >
          <svg
            width="12"
            height="12"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <polyline points="9 18 15 12 9 6" />
          </svg>
        </span>

        <span
          style={{
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            backgroundColor: getStatusColor(status),
            flexShrink: 0,
          }}
        />

        <span
          style={{
            flex: 1,
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          }}
        >
          {name}
        </span>

        <span
          style={{
            fontSize: '10px',
            padding: '2px 4px',
            borderRadius: '4px',
            backgroundColor: 'var(--border)',
            opacity: 0.8,
            fontWeight: 'bold',
          }}
        >
          {status}
        </span>
      </div>
    </div>
  );
};
