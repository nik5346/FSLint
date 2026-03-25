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
    <div style={{ fontSize: '0.9em' }}>
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
              padding: '4px 8px',
              cursor: 'pointer',
              borderRadius: '4px',
              marginLeft: (header.level - 1) * 12,
              color: isActive ? '#007bff' : header.level === 1 ? theme.text : theme.muted,
              fontWeight: isActive || header.level === 1 ? 'bold' : 'normal',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap',
              backgroundColor: isActive ? theme.iconHover : 'transparent',
            }}
            onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = theme.iconHover)}
            onMouseLeave={(e) =>
              (e.currentTarget.style.backgroundColor = isActive ? theme.iconHover : 'transparent')
            }
          >
            {header.text}
          </div>
        );
      })}
    </div>
  );
};
