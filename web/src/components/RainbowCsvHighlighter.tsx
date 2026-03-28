import { Theme } from '../types';

export const RainbowCsvHighlighter = ({
  content,
  isDark,
  theme,
}: {
  content: string;
  isDark: boolean;
  theme: Theme;
}) => {
  const lines = content.split('\n');
  const colors = isDark
    ? ['#ff5555', '#50fa7b', '#f1fa8c', '#bd93f9', '#ff79c6', '#8be9fd', '#ffb86c']
    : ['#e45649', '#50a14f', '#c18401', '#4078f2', '#a626a4', '#0184bc', '#986801'];

  return (
    <div
      style={{
        margin: 0,
        padding: '15px 0',
        fontSize: '0.9em',
        fontFamily: 'monospace',
        lineHeight: '1.5em',
        flex: 1,
        overflow: 'auto',
      }}
    >
      <div style={{ display: 'inline-block', minWidth: '100%' }}>
        {lines.map((line, lineIdx) => {
          // Simple CSV split, doesn't handle escaped commas but good for "rainbow" effect
          const cells = line.split(',');
          return (
            <div
              key={lineIdx}
              style={{
                display: 'flex',
                minWidth: '100%',
                paddingRight: '15px',
                boxSizing: 'border-box',
              }}
            >
              <div
                style={{
                  minWidth: '40px',
                  paddingLeft: '15px',
                  paddingRight: '10px',
                  textAlign: 'right',
                  color: isDark ? '#858585' : '#999999',
                  userSelect: 'none',
                  position: 'sticky',
                  left: 0,
                  backgroundColor: theme.surface,
                  zIndex: 1,
                }}
              >
                {lineIdx + 1}
              </div>
              <div
                style={{
                  paddingLeft: '10px',
                  whiteSpace: 'pre',
                  flex: 1,
                  minWidth: 'fit-content',
                }}
              >
                {cells.map((cell, cellIdx) => {
                  const renderedCell = cell.split(/([ \t]+)/).flatMap((part, i) => {
                    if (/^[ \t]+$/.test(part)) {
                      return part.split('').map((char, j) => (
                        <span
                          key={`${i}-${j}`}
                          className="whitespace-marker"
                          data-marker={char === '\t' ? '→' : '·'}
                          data-marker-type={char === '\t' ? 'tab' : 'space'}
                        >
                          {char}
                        </span>
                      ));
                    }
                    return [<span key={i}>{part}</span>];
                  });

                  return (
                    <span key={cellIdx}>
                      <span style={{ color: colors[cellIdx % colors.length] }}>{renderedCell}</span>
                      {cellIdx < cells.length - 1 && <span style={{ color: theme.muted }}>,</span>}
                    </span>
                  );
                })}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
};
