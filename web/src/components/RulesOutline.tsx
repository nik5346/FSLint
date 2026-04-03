import { useEffect, useRef } from 'react';
import { Theme } from '../types';

/**
 * Component displaying a table of contents outline for the validation rules.
 * @param props - Component properties.
 * @param props.headers - List of headers to display.
 * @param props.theme - The current theme object.
 * @param props.activeLine - The currently active (scrolled to) line number.
 * @returns The rendered RulesOutline component.
 */
export const RulesOutline = ({
  headers,
  theme,
  activeLine,
}: {
  headers: {
    level: number;
    text: string;
    line: number;
  }[];
  theme: Theme;
  activeLine?: number;
}) => {
  const activeRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (activeRef.current) {
      activeRef.current.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
  }, [activeLine]);

  return (
    <div style={{ paddingBottom: '20px' }}>
      <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
        {headers.map((header, i) => {
          const isActive = activeLine === header.line;
          return (
            <div
              key={i}
              ref={isActive ? activeRef : null}
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
                position: 'relative',
                padding: '6px 12px',
                paddingLeft: (header.level - 1) * 12 + 12,
                cursor: 'pointer',
                borderRadius: '6px',
                marginTop: header.level === 1 && i !== 0 ? '12px' : '0',
                fontSize:
                  header.level === 1 ? '0.95rem' : header.level === 2 ? '0.85rem' : '0.8rem',
                color: isActive ? '#007bff' : header.level === 3 ? theme.muted : theme.text,
                fontWeight:
                  isActive || header.level === 1 ? '700' : header.level === 2 ? '600' : '400',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap',
                backgroundColor: isActive ? 'rgba(0, 123, 255, 0.1)' : 'transparent',
                transition: 'all 0.2s ease',
              }}
              onMouseEnter={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = theme.iconHover;
                  e.currentTarget.style.color = theme.text;
                }
              }}
              onMouseLeave={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.color = header.level === 3 ? theme.muted : theme.text;
                }
              }}
            >
              {header.text}
            </div>
          );
        })}
      </div>
    </div>
  );
};
