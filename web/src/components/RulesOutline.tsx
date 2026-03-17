import { Theme } from '../types';

export const RulesOutline = ({
  headers,
  theme,
}: {
  headers: { level: number; text: string; line: number }[];
  theme: Theme;
}) => {
  return (
    <div style={{ fontSize: '0.9em' }}>
      {headers.map((header, i) => (
        <div
          key={i}
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
            color: header.level === 1 ? theme.text : theme.muted,
            fontWeight: header.level === 1 ? 'bold' : 'normal',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          }}
          onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = theme.iconHover)}
          onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = 'transparent')}
        >
          {header.text}
        </div>
      ))}
    </div>
  );
};
