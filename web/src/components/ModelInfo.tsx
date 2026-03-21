import { useState, useEffect } from 'react';
import { ValidationResult, Theme, FSLintModule } from '../types';

interface ModelInfoProps {
  result: ValidationResult;
  theme: Theme;
  isDark: boolean;
  module: FSLintModule | null;
}

const Section = ({
  title,
  theme,
  children,
}: {
  title: string;
  theme: Theme;
  children: React.ReactNode;
}) => (
  <div style={{ marginBottom: '24px' }}>
    <h3
      style={{
        fontSize: '1em',
        fontWeight: 'bold',
        marginBottom: '12px',
        color: theme.muted,
        textTransform: 'uppercase',
        letterSpacing: '0.05em',
      }}
    >
      {title}
    </h3>
    {children}
  </div>
);

export const ModelInfo = ({ result, theme, isDark, module }: ModelInfoProps) => {
  const { summary, overallStatus } = result;

  const statusColor =
    overallStatus === 'FAIL' ? '#ff5555' : overallStatus === 'WARNING' ? '#ffb86c' : '#50fa7b';

  const [iconUrl, setIconUrl] = useState<string | null>(null);

  useEffect(() => {
    if (!summary.hasIcon || !module || !result.file_tree) {
      // eslint-disable-next-line react-hooks/set-state-in-effect
      setIconUrl(null);
      return;
    }

    let url: string | null = null;
    try {
      // Find the model icon in the file tree to get its absolute path
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const findPath = (node: any, names: string[]): string | null => {
        if (names.includes(node.name) && node.kind === 'file') return node.path;
        if (node.children) {
          for (const child of node.children) {
            const path = findPath(child, names);
            if (path) return path;
          }
        }
        return null;
      };

      const iconPath = findPath(result.file_tree, [
        'model.svg',
        'icon.svg',
        'model.png',
        'icon.png',
      ]);
      if (iconPath) {
        const data = module.FS.readFile(iconPath) as Uint8Array;
        const type = iconPath.toLowerCase().endsWith('.svg') ? 'image/svg+xml' : 'image/png';
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const blob = new Blob([data as any], { type });
        url = URL.createObjectURL(blob);
        setIconUrl(url);
      }
    } catch (e) {
      console.error('Failed to load FMU icon:', e);
    }

    return () => {
      if (url) URL.revokeObjectURL(url);
    };
  }, [summary.hasIcon, module, result.file_tree]);

  const infoItems: { label: string; value: string; mono?: boolean }[] = [
    { label: 'Model Name', value: summary.modelName },
    {
      label: summary.standard === 'SSP' ? 'SSP Version' : 'FMI Version',
      value: summary.fmiVersion,
    },
    { label: 'Model Version', value: summary.modelVersion },
    { label: 'Author', value: summary.author },
    { label: 'Copyright', value: summary.copyright },
    { label: 'License', value: summary.license },
    { label: 'Generation Tool', value: summary.generationTool },
    { label: 'Generation Date', value: summary.generationDateAndTime },
  ].filter((item) => item.value);

  return (
    <div
      style={{
        padding: '24px',
        height: '100%',
        overflowY: 'auto',
        display: 'flex',
        flexDirection: 'column',
        gap: '24px',
      }}
    >
      <div
        style={{
          display: 'flex',
          gap: '32px',
          alignItems: 'flex-start',
          flexWrap: 'wrap',
        }}
      >
        {summary.hasIcon && iconUrl && (
          <div
            style={{
              width: '128px',
              height: '128px',
              backgroundColor: isDark ? '#333' : '#f0f0f0',
              borderRadius: '12px',
              display: 'flex',
              overflow: 'hidden',
              border: `1px solid ${theme.border}`,
              flexShrink: 0,
            }}
          >
            <img
              src={iconUrl}
              alt="FMU Icon"
              style={{ width: '100%', height: '100%', objectFit: 'contain' }}
            />
          </div>
        )}

        <div style={{ flex: 1, minWidth: '300px' }}>
          <div style={{ marginBottom: '20px' }}>
            <h2 style={{ fontSize: '1.8em', margin: '0 0 8px 0', fontWeight: 'bold' }}>
              {summary.modelName || 'Unnamed Model'}
            </h2>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span
                style={{
                  padding: '4px 10px',
                  borderRadius: '100px',
                  fontSize: '0.85em',
                  fontWeight: 'bold',
                  backgroundColor: statusColor + '22',
                  color: statusColor,
                  border: `1px solid ${statusColor}44`,
                }}
              >
                {overallStatus === 'PASS'
                  ? 'VALID'
                  : overallStatus === 'WARNING'
                    ? 'VALID WITH WARNINGS'
                    : 'INVALID'}
              </span>
            </div>
          </div>

          {summary.description && (
            <p style={{ margin: '0', lineHeight: '1.6', color: theme.text, fontSize: '1.05em' }}>
              {summary.description}
            </p>
          )}
        </div>
      </div>

      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(550px, 1fr))',
          gap: '48px',
        }}
      >
        <Section title="Base Information" theme={theme}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {infoItems.map((item) => (
              <div key={item.label} style={{ display: 'flex', fontSize: '0.95em' }}>
                <span style={{ width: '220px', color: theme.muted, flexShrink: 0 }}>
                  {item.label}:
                </span>
                <span
                  style={{
                    color: theme.text,
                    fontFamily: item.mono ? 'monospace' : 'inherit',
                    wordBreak: 'break-all',
                  }}
                >
                  {item.value}
                </span>
              </div>
            ))}
          </div>
        </Section>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
          {summary.standard !== 'SSP' && (
            <Section title="Capabilities" theme={theme}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                <div style={{ display: 'flex', fontSize: '0.95em' }}>
                  <span style={{ width: '220px', color: theme.muted, flexShrink: 0 }}>
                    FMU Type:
                  </span>
                  <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    {summary.fmuTypes.map((t) => (
                      <span
                        key={t}
                        style={{
                          padding: '2px 8px',
                          borderRadius: '6px',
                          backgroundColor: theme.buttonHoverBg,
                          fontSize: '0.85em',
                          border: `1px solid ${theme.border}`,
                        }}
                      >
                        {t}
                      </span>
                    ))}
                  </div>
                </div>

                <div style={{ display: 'flex', fontSize: '0.95em' }}>
                  <span style={{ width: '220px', color: theme.muted, flexShrink: 0 }}>
                    Interfaces:
                  </span>
                  <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    {summary.interfaces.map((intf) => (
                      <span
                        key={intf}
                        style={{
                          padding: '2px 8px',
                          borderRadius: '6px',
                          backgroundColor: theme.buttonHoverBg,
                          fontSize: '0.85em',
                          border: `1px solid ${theme.border}`,
                        }}
                      >
                        {intf}
                      </span>
                    ))}
                    {summary.interfaces.length === 0 && (
                      <span style={{ color: theme.muted, fontStyle: 'italic' }}>None detected</span>
                    )}
                  </div>
                </div>

                <div style={{ display: 'flex', fontSize: '0.95em' }}>
                  <span style={{ width: '220px', color: theme.muted, flexShrink: 0 }}>
                    Platforms:
                  </span>
                  <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    {summary.platforms.map((p) => (
                      <span
                        key={p}
                        style={{
                          padding: '2px 8px',
                          borderRadius: '6px',
                          backgroundColor: theme.buttonHoverBg,
                          fontSize: '0.85em',
                          border: `1px solid ${theme.border}`,
                        }}
                      >
                        {p}
                      </span>
                    ))}
                    {summary.platforms.length === 0 && (
                      <span style={{ color: theme.muted, fontStyle: 'italic' }}>None detected</span>
                    )}
                  </div>
                </div>
              </div>
            </Section>
          )}

          {summary.layeredStandards.length > 0 && (
            <Section title="Layered Standards" theme={theme}>
              <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                {summary.layeredStandards.map((s) => (
                  <span
                    key={s}
                    style={{
                      padding: '4px 12px',
                      borderRadius: '6px',
                      backgroundColor: theme.buttonHoverBg,
                      fontSize: '0.9em',
                      border: `1px solid ${theme.border}`,
                    }}
                  >
                    {s}
                  </span>
                ))}
              </div>
            </Section>
          )}
        </div>
      </div>
    </div>
  );
};
