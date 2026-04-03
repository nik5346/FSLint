import { useMemo } from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { PrismLight as SyntaxHighlighter } from 'react-syntax-highlighter';
import { vscDarkPlus, prism } from 'react-syntax-highlighter/dist/esm/styles/prism';
import { Theme } from '../types';

/**
 * Strips textShadow from Prism styles to fix "ghosting" effects.
 * @param style - The original Prism style object.
 * @returns The updated style object without text shadows.
 */
const stripTextShadow = (style: { [key: string]: React.CSSProperties }) => {
  const newStyle: { [key: string]: React.CSSProperties } = {};
  for (const key in style) {
    if (Object.prototype.hasOwnProperty.call(style, key)) {
      newStyle[key] = { ...style[key], textShadow: 'none' };
    }
  }
  return newStyle;
};

/**
 * Component for rendering Markdown content with syntax highlighting and anchor support.
 * @param props - Component properties.
 * @param props.content - The Markdown content to render.
 * @param props.theme - The current theme object.
 * @param props.isDark - Whether dark mode is active.
 * @returns The rendered MarkdownContent component.
 */
export const MarkdownContent = ({
  content,
  theme,
  isDark,
}: {
  content: string;
  theme: Theme;
  isDark: boolean;
}) => {
  const syntaxStyle = useMemo(() => stripTextShadow(isDark ? vscDarkPlus : prism), [isDark]);

  return (
    <div className="markdown-body">
      <style>{`
        .markdown-body { font-size: 0.9em; }
        .markdown-body table { border-collapse: collapse; width: 100%; margin: 1em 0; }
        .markdown-body th, .markdown-body td { border: 1px solid ${theme.border}; padding: 8px; text-align: left; }
        .markdown-body th { background-color: ${theme.bg}; }
        .markdown-body code { background-color: ${theme.bg}; color: ${theme.text}; border-radius: 4px; overflow-x: auto; max-width: 100%; display: inline-block; vertical-align: bottom; }
        .markdown-body pre { background-color: transparent !important; padding: 0 !important; margin: 0.4em 0 !important; border-radius: 4px; overflow: hidden; }
        .markdown-body blockquote { border-left: 4px solid ${theme.border}; padding-left: 16px; color: ${theme.muted}; }
      `}</style>
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={{
          /* eslint-disable @typescript-eslint/no-explicit-any */
          /**
           * Renders an H1 header with an ID for anchor links.
           * @param props - Component properties.
           * @param props.children - The child elements to render.
           * @returns The rendered H1.
           */
          h1: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h1 id={line ? `line-${line}` : undefined}>{children}</h1>;
          },
          /**
           * Renders an H2 header with an ID for anchor links.
           * @param props - Component properties.
           * @param props.children - The child elements to render.
           * @returns The rendered H2.
           */
          h2: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h2 id={line ? `line-${line}` : undefined}>{children}</h2>;
          },
          /**
           * Renders an H3 header with an ID for anchor links.
           * @param props - Component properties.
           * @param props.children - The child elements to render.
           * @returns The rendered H3.
           */
          h3: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h3 id={line ? `line-${line}` : undefined}>{children}</h3>;
          },
          /**
           * Renders an H4 header with an ID for anchor links.
           * @param props - Component properties.
           * @param props.children - The child elements to render.
           * @returns The rendered H4.
           */
          h4: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h4 id={line ? `line-${line}` : undefined}>{children}</h4>;
          },
          /**
           * Renders an H5 header with an ID for anchor links.
           * @param props - Component properties.
           * @param props.children - The child elements to render.
           * @returns The rendered H5.
           */
          h5: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h5 id={line ? `line-${line}` : undefined}>{children}</h5>;
          },
          /**
           * Renders an H6 header with an ID for anchor links.
           * @param props - Component properties.
           * @param props.children - The child elements to render.
           * @returns The rendered H6.
           */
          h6: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h6 id={line ? `line-${line}` : undefined}>{children}</h6>;
          },
          /**
           * Renders a code block with syntax highlighting.
           * @param props - Component properties.
           * @param props.inline - Whether the code is inline.
           * @param props.className - The CSS class name.
           * @param props.children - The child elements to render.
           * @returns The rendered code block.
           */
          code({ inline, className, children, ...props }: any) {
            const match = /language-(\w+)/.exec(className || '');
            const isBnf = match && match[1] === 'bnf';
            return !inline && match ? (
              <SyntaxHighlighter
                style={isBnf ? {} : syntaxStyle}
                language={isBnf ? 'text' : match[1]}
                PreTag="div"
                wrapLines={true}
                customStyle={{
                  margin: 0,
                  padding: '8px 12px',
                  backgroundColor: theme.bg,
                  color: theme.text,
                  lineHeight: '1.5em',
                }}
                codeTagProps={{ style: { color: 'inherit' } }}
                lineProps={{
                  style: {
                    display: 'flex',
                    minWidth: '100%',
                    paddingRight: '15px',
                    boxSizing: 'border-box',
                  },
                }}
                {...props}
              >
                {String(children).trim()}
              </SyntaxHighlighter>
            ) : (
              <code className={className} {...props}>
                {children}
              </code>
            );
          },
          /* eslint-enable @typescript-eslint/no-explicit-any */
        }}
      >
        {content}
      </ReactMarkdown>
    </div>
  );
};
