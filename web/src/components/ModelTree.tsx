import React, { useState, useCallback } from 'react';
import { NestedModelResult, ValidationResult } from '../types';
import { ModelTreeItem } from './ModelTreeItem';

/**
 * Props for the ModelTree component.
 */
export interface ModelTreeProps {
  /** The root validation result. */
  root: ValidationResult;
  /** The currently selected logical path (or null for root). */
  selectedPath: string | null;
  /** Callback when a node is selected. */
  onSelect: (logicalPath: string | null, node: NestedModelResult | ValidationResult) => void;
}

/**
 * Container component that holds expand/collapse state for all nodes and renders a flat list of ModelTreeItem rows.
 * @param {ModelTreeProps} props - The component props.
 * @returns {JSX.Element} The rendered component.
 */
export const ModelTree: React.FC<ModelTreeProps> = ({ root, selectedPath, onSelect }) => {
  const [expandedPaths, setExpandedPaths] = useState<Set<string>>(() => {
    const initial = new Set<string>(['']);
    root.nested_models.forEach((child) => {
      initial.add(child.logical_path);
    });
    return initial;
  });

  const toggleExpand = useCallback((path: string) => {
    setExpandedPaths((prev) => {
      const next = new Set(prev);
      if (next.has(path)) {
        next.delete(path);
      } else {
        next.add(path);
      }
      return next;
    });
  }, []);

  /**
   * Recursively renders the model tree nodes.
   * @param {NestedModelResult | ValidationResult} node - The current node to render.
   * @param {number} depth - The nesting depth.
   * @param {boolean} isVisible - Whether the node should be visible.
   * @returns {React.ReactNode[]} The rendered node elements.
   */
  const renderNodes = (
    node: NestedModelResult | ValidationResult,
    depth: number,
    isVisible: boolean,
  ): React.ReactNode[] => {
    const currentPath = 'logical_path' in node ? node.logical_path : '';
    const items: React.ReactNode[] = [];

    if (isVisible) {
      items.push(
        <ModelTreeItem
          key={currentPath || 'root'}
          node={node}
          depth={depth}
          isSelected={selectedPath === (currentPath || null)}
          onSelect={(n) => onSelect(currentPath || null, n)}
          isExpanded={expandedPaths.has(currentPath)}
          onToggle={() => toggleExpand(currentPath)}
        />,
      );
    }

    if ('nested_models' in node && node.nested_models) {
      node.nested_models.forEach((child) => {
        const childVisible = isVisible && expandedPaths.has(currentPath);
        items.push(...renderNodes(child, depth + 1, childVisible));
      });
    }

    return items;
  };

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        gap: '2px',
      }}
    >
      {renderNodes(root, 0, true)}
    </div>
  );
};
