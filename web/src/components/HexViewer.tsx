import { useMemo, useState, useRef, useLayoutEffect } from 'react';
import { Theme } from '../types';

/**
 * Properties for the HexViewer component.
 */
interface HexViewerProps {
  /** The binary data to display. */
  data: Uint8Array;
  /** The current theme object. */
  theme: Theme;
  /** Whether dark mode is active. */
  isDark: boolean;
}

/**
 * A virtualized hex viewer component for displaying binary data.
 * @param {HexViewerProps} props - Component properties.
 * @returns {JSX.Element} The rendered HexViewer component.
 */
export const HexViewer = ({ data, theme, isDark }: HexViewerProps) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const [scrollTop, setScrollTop] = useState(0);
  const [containerHeight, setContainerHeight] = useState(0);

  const rowHeight = 21;
  const bytesPerRow = 16;
  const totalRows = Math.ceil(data.length / bytesPerRow);
  const totalHeight = totalRows * rowHeight;

  useLayoutEffect(() => {
    /**
     * Updates the container height state from the DOM element.
     */
    const updateHeight = () => {
      if (containerRef.current) {
        setContainerHeight(containerRef.current.clientHeight);
      }
    };

    updateHeight();
    const observer = new ResizeObserver(updateHeight);
    if (containerRef.current) observer.observe(containerRef.current);
    return () => observer.disconnect();
  }, []);

  /**
   * Updates the scroll top state on scroll events.
   * @param {React.UIEvent<HTMLDivElement>} e - The scroll event.
   */
  const handleScroll = (e: React.UIEvent<HTMLDivElement>) => {
    setScrollTop(e.currentTarget.scrollTop);
  };

  const visibleRows = useMemo(() => {
    const startRow = Math.max(0, Math.floor(scrollTop / rowHeight) - 5);
    const endRow = Math.min(totalRows, Math.ceil((scrollTop + containerHeight) / rowHeight) + 5);

    const rows = [];
    for (let i = startRow; i < endRow; i++) {
      const start = i * bytesPerRow;
      const end = Math.min(start + bytesPerRow, data.length);
      const rowData = data.slice(start, end);
      rows.push({
        index: i,
        offset: start,
        data: rowData,
      });
    }
    return rows;
  }, [scrollTop, containerHeight, data, totalRows]);

  /**
   * Formats a byte offset into an 8-character hex string.
   * @param {number} offset - The byte offset.
   * @returns {string} The formatted hex offset.
   */
  const formatOffset = (offset: number) => offset.toString(16).padStart(8, '0').toUpperCase();
  /**
   * Formats a byte into a 2-character hex string.
   * @param {number} byte - The byte value.
   * @returns {string} The formatted hex byte.
   */
  const formatHex = (byte: number) => byte.toString(16).padStart(2, '0').toUpperCase();
  /**
   * Returns the ASCII representation of a byte, or a dot if it's not printable.
   * @param {number} byte - The byte value.
   * @returns {string} The ASCII character or '.'.
   */
  const formatAscii = (byte: number) =>
    byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : '.';

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        backgroundColor: theme.surface,
        color: theme.text,
        fontFamily: 'monospace',
        fontSize: '13px',
      }}
    >
      <div
        style={{
          display: 'flex',
          padding: '5px 10px',
          borderBottom: `1px solid ${theme.border}`,
          backgroundColor: isDark ? '#333' : '#f5f5f5',
          fontWeight: 'bold',
          color: theme.muted,
        }}
      >
        <span style={{ width: '80px', marginRight: '20px' }}>Offset</span>
        <div style={{ display: 'flex', gap: '8px', marginRight: '20px' }}>
          {Array.from({ length: bytesPerRow }).map((_, i) => (
            <span key={i} style={{ width: '20px', textAlign: 'center' }}>
              {i.toString(16).toUpperCase()}
            </span>
          ))}
        </div>
        <span>ASCII</span>
      </div>
      <div
        ref={containerRef}
        onScroll={handleScroll}
        style={{
          flex: 1,
          overflow: 'auto',
          position: 'relative',
        }}
      >
        <div style={{ height: totalHeight, position: 'relative' }}>
          {visibleRows.map((row) => (
            <div
              key={row.index}
              style={{
                position: 'absolute',
                top: row.index * rowHeight,
                left: 0,
                right: 0,
                height: rowHeight,
                display: 'flex',
                padding: '0 10px',
                whiteSpace: 'pre',
                alignItems: 'center',
              }}
            >
              <span style={{ width: '80px', color: theme.muted, marginRight: '20px' }}>
                {formatOffset(row.offset)}
              </span>
              <div style={{ display: 'flex', gap: '8px', marginRight: '20px' }}>
                {Array.from({ length: bytesPerRow }).map((_, i) => (
                  <span
                    key={i}
                    style={{
                      width: '20px',
                      display: 'inline-block',
                      textAlign: 'center',
                      color: i < row.data.length ? theme.text : 'transparent',
                    }}
                  >
                    {i < row.data.length ? formatHex(row.data[i]) : '  '}
                  </span>
                ))}
              </div>
              <div style={{ display: 'flex', color: theme.muted }}>
                {Array.from({ length: bytesPerRow }).map((_, i) => (
                  <span
                    key={i}
                    style={{
                      width: '10px',
                      display: 'inline-block',
                      textAlign: 'center',
                      visibility: i < row.data.length ? 'visible' : 'hidden',
                    }}
                  >
                    {i < row.data.length ? formatAscii(row.data[i]) : '.'}
                  </span>
                ))}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
