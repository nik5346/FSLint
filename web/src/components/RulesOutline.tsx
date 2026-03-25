import { useEffect, useRef } from 'react';
import { Theme } from '../types';

export const RulesOutline = ({
  headers,
  theme,
  activeLine,
}: {
  headers: { level: number; text: string; line: number }[];
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
      <div
        style={{
          padding: '10px 8px 15px',
          fontSize: '0.75rem',
          fontWeight: 'bold',
          color: theme.muted,
          textTransform: 'uppercase',
          letterSpacing: '0.05em',
        }}
      >
        Sections
      </div>
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
                cursor: 'pointer',
                borderRadius: '6px',
                marginLeft: (header.level - 1) * 12,
                fontSize: header.level === 1 ? '0.9rem' : '0.85rem',
                color: isActive ? '#007bff' : header.level === 1 ? theme.text : theme.muted,
                fontWeight: isActive || header.level === 1 ? '600' : '400',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap',
                backgroundColor: isActive ? 'rgba(0, 123, 255, 0.1)' : 'transparent',
                transition: 'all 0.2s ease',
                borderLeft:
                  header.level > 1
                    ? `1px solid ${isActive ? '#007bff' : theme.border}`
                    : '2px solid transparent',
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
                  e.currentTarget.style.color = header.level === 1 ? theme.text : theme.muted;
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
