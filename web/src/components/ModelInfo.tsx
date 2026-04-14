import { useState, useEffect } from 'react';
import { ValidationResult, Theme, FSLintModule, NestedModelResult, TestResult } from '../types';

/**
 * Properties for the ModelInfo component.
 */
interface ModelInfoProps {
  /** The validation result object or nested result. */
  result: ValidationResult | NestedModelResult;
  /** The current theme object. */
  theme: Theme;
  /** Whether the dark mode is enabled. */
  isDark: boolean;
  /** The FSLint WASM module, if initialized. */
  module: FSLintModule | null;
}

/**
 * A section component with a title and content.
 * @param {Object} props - Component properties.
 * @param {string} props.title - The section title.
 * @param {Theme} props.theme - The current theme object.
 * @param {React.ReactNode} props.children - The section content.
 * @returns {JSX.Element} The rendered section.
 */
const Section = ({
  title,
  theme,
  children,
}: {
  /**
   *
   */
  title: string;
  /**
   *
   */
  theme: Theme;
  /**
   *
   */
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

/**
 * Component displaying metadata and status summary of a validated model.
 * @param {ModelInfoProps} props - Component properties.
 * @returns {JSX.Element} The rendered ModelInfo component.
 */
export const ModelInfo = ({ result, theme, isDark, module }: ModelInfoProps) => {
  const summary = result.summary;
  const overallStatus = 'overallStatus' in result ? result.overallStatus : result.status;
  const results = result.results || [];

  const [iconUrl, setIconUrl] = useState<string | null>(null);

  useEffect(() => {
    if (!summary || !('has_icon' in summary) || !summary.has_icon || !module || !result.file_tree) {
      // eslint-disable-next-line react-hooks/set-state-in-effect
      setIconUrl(null);
      return;
    }

    let url: string | null = null;
    try {
      /**
       * Recursively searches for a file in the tree matching any of the given names.
       * @param {any} node - The current node to search.
       * @param {string[]} names - The list of file names to search for.
       * @returns {string | null} The absolute path to the file, or null if not found.
       */
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
  }, [summary, module, result.file_tree]);

  /**
   * Formats a size in bytes into a human-readable string.
   * @param {number} bytes - The number of bytes.
   * @returns {string} The formatted size string.
   */
  if (!summary) {
    return (
      <div style={{ padding: '24px', color: theme.muted }}>
        Metadata summary not available for this model.
      </div>
    );
  }

  const hasSecurityFailure =
    overallStatus === 'FAIL' &&
    results.some((r: TestResult) => r.status === 'FAIL' && r.test_name.includes('[SECURITY]'));

  const statusColor =
    overallStatus === 'FAIL' ? '#ff5555' : overallStatus === 'WARNING' ? '#ffb86c' : '#50fa7b';

  /**
   * Formats a size in bytes into a human-readable string.
   * @param {number} bytes - The number of bytes.
   * @returns {string} The formatted size string.
   */
  const formatSize = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  const infoItems: {
    /**
     *
     */
    label: string;
    /**
     *
     */
    value: string;
    /**
     *
     */
    mono?: boolean;
  }[] = [
    { label: 'Model Name', value: summary.model_name },
    {
      label: summary.standard === 'SSP' ? 'SSP Version' : 'FMI Version',
      value: summary.fmi_version,
    },
    { label: 'Model Version', value: 'model_version' in summary ? summary.model_version : '' },
    { label: 'Author', value: 'author' in summary ? summary.author : '' },
    { label: 'Copyright', value: 'copyright' in summary ? summary.copyright : '' },
    { label: 'License', value: 'license' in summary ? summary.license : '' },
    { label: 'Generation Tool', value: summary.generation_tool },
    {
      label: 'Generation Date',
      value: 'generation_date_and_time' in summary ? summary.generation_date_and_time : '',
    },
    {
      label: 'Source Language',
      value: 'source_language' in summary ? summary.source_language : '',
    },
    {
      label: 'Total Size',
      value:
        'total_size' in summary ? (summary.total_size ? formatSize(summary.total_size) : '') : '',
    },
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
        {'has_icon' in summary && summary.has_icon && iconUrl && (
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
              {summary.model_name || 'Unnamed Model'}
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
              {hasSecurityFailure && (
                <span
                  title="Potential security risk detected. See Certificate tab for details."
                  style={{
                    padding: '4px 10px',
                    borderRadius: '100px',
                    fontSize: '0.85em',
                    fontWeight: 'bold',
                    backgroundColor: 'var(--status-security-bg, rgba(2, 136, 209, 0.15))',
                    color: 'var(--status-security, #0288d1)',
                    border: '1px solid var(--status-security-border, rgba(2, 136, 209, 0.3))',
                  }}
                >
                  🛡️ Security Violation
                </span>
              )}
            </div>
          </div>

          {'description' in summary && summary.description && (
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
            {infoItems.length > 0 ? (
              infoItems.map((item) => (
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
              ))
            ) : (
              <span style={{ color: theme.muted, fontStyle: 'italic' }}>
                No base information could be retrieved.
              </span>
            )}
          </div>
        </Section>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
          {summary.standard !== 'SSP' &&
            summary.standard !== 'UNKNOWN' &&
            'fmu_types' in summary && (
              <Section title="Capabilities" theme={theme}>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                  <div style={{ display: 'flex', fontSize: '0.95em' }}>
                    <span style={{ width: '220px', color: theme.muted, flexShrink: 0 }}>
                      FMU Type:
                    </span>
                    <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                      {summary.fmu_types.length > 0 ? (
                        summary.fmu_types.map((t: string) => (
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
                        ))
                      ) : (
                        <span style={{ color: theme.muted, fontStyle: 'italic' }}>
                          None detected
                        </span>
                      )}
                    </div>
                  </div>

                  <div style={{ display: 'flex', fontSize: '0.95em' }}>
                    <span style={{ width: '220px', color: theme.muted, flexShrink: 0 }}>
                      Interfaces:
                    </span>
                    <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                      {'interfaces' in summary && summary.interfaces.length > 0 ? (
                        summary.interfaces.map((intf: string) => (
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
                        ))
                      ) : (
                        <span style={{ color: theme.muted, fontStyle: 'italic' }}>
                          None detected
                        </span>
                      )}
                    </div>
                  </div>

                  <div style={{ display: 'flex', fontSize: '0.95em' }}>
                    <span style={{ width: '220px', color: theme.muted, flexShrink: 0 }}>
                      Platforms:
                    </span>
                    <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                      {'platforms' in summary && summary.platforms.length > 0 ? (
                        summary.platforms.map((p: string) => (
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
                        ))
                      ) : (
                        <span style={{ color: theme.muted, fontStyle: 'italic' }}>
                          None detected
                        </span>
                      )}
                    </div>
                  </div>
                </div>
              </Section>
            )}

          {summary.standard !== 'UNKNOWN' &&
            'layered_standards' in summary &&
            summary.layered_standards.length > 0 && (
              <Section title="Layered Standards" theme={theme}>
                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                  {summary.layered_standards.map((s: string) => (
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
