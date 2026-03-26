import { useMemo } from 'react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { PrismLight as SyntaxHighlighter } from 'react-syntax-highlighter';
import { vscDarkPlus, prism } from 'react-syntax-highlighter/dist/esm/styles/prism';
import { Theme } from '../types';
import { whitespaceRenderer, stripTextShadow } from './FilePreview';

export const MarkdownContent = ({
  content,
  theme,
  isDark,
}: {
  content: string;
  theme: Theme;
  isDark: boolean;
}) => {
  const memoizedWhitespaceRenderer = useMemo(() => whitespaceRenderer(), []);

  const syntaxStyle = useMemo(() => stripTextShadow(isDark ? vscDarkPlus : prism), [isDark]);

  return (
    <div className="markdown-body">
      <style>{`
        .markdown-body { font-size: 0.9em; }
        .markdown-body table { border-collapse: collapse; width: 100%; margin: 1em 0; }
        .markdown-body th, .markdown-body td { border: 1px solid ${theme.border}; padding: 8px; text-align: left; }
        .markdown-body th { background-color: ${theme.bg}; }
        .markdown-body code { background-color: ${theme.bg}; color: ${theme.text}; padding: 2px 4px; border-radius: 4px; }
        .markdown-body pre { background-color: transparent !important; padding: 0 !important; margin: 0.4em 0 !important; border-radius: 4px; overflow: hidden; }
        .markdown-body blockquote { border-left: 4px solid ${theme.border}; padding-left: 16px; color: ${theme.muted}; }
      `}</style>
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={{
          /* eslint-disable @typescript-eslint/no-explicit-any */
          h1: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h1 id={line ? `line-${line}` : undefined}>{children}</h1>;
          },
          h2: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h2 id={line ? `line-${line}` : undefined}>{children}</h2>;
          },
          h3: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h3 id={line ? `line-${line}` : undefined}>{children}</h3>;
          },
          h4: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h4 id={line ? `line-${line}` : undefined}>{children}</h4>;
          },
          h5: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h5 id={line ? `line-${line}` : undefined}>{children}</h5>;
          },
          h6: ({ children, ...props }: any) => {
            const line = (props as any).node?.position?.start.line;
            return <h6 id={line ? `line-${line}` : undefined}>{children}</h6>;
          },
          code({ inline, className, children, ...props }: any) {
            const match = /language-(\w+)/.exec(className || '');
            const isBnf = match && match[1] === 'bnf';
            return !inline && match ? (
              <SyntaxHighlighter
                style={isBnf ? {} : syntaxStyle}
                language={isBnf ? 'text' : match[1]}
                PreTag="div"
                renderer={memoizedWhitespaceRenderer}
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
