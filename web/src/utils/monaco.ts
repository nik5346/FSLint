import { loader } from '@monaco-editor/react';

/**
 * Centralized Monaco configuration and theme definition.
 * Sets up the custom fslint-report language for syntax highlighting of validation reports.
 */
export const configureMonaco = () => {
  loader.init().then((monaco) => {
    // Register a custom language for FSLint validation reports if not already registered
    if (
      !monaco.languages.getLanguages().some((lang: { id: string }) => lang.id === 'fslint-report')
    ) {
      monaco.languages.register({ id: 'fslint-report' });

      // Define tokens for the report language
      monaco.languages.setMonarchTokensProvider('fslint-report', {
        tokenizer: {
          root: [
            // Status markers
            [/\[✓ PASS\]/, 'status-pass'],
            [/\[✗ FAIL\]/, 'status-fail'],
            [/\[⚠ WARN\]/, 'status-warn'],

            // Headers and sections
            [/^--[-]+$/, 'separator'],
            [/^[A-Z][A-Za-z ]+$/, 'header'],

            // Box drawing characters
            [/[┌┐└┘├┤┬┴┼─│]/, 'box-drawing'],

            // Numbers and paths
            [/\d+/, 'number'],
            [/[a-zA-Z0-9_/.-]+\.[a-z]{2,4}/, 'path'],
          ],
        },
      });
    }

    // Define the themes
    monaco.editor.defineTheme('fslint-dark', {
      base: 'vs-dark',
      inherit: true,
      rules: [
        { token: 'status-pass', foreground: '4caf50', fontStyle: 'bold' },
        { token: 'status-fail', foreground: 'f44336', fontStyle: 'bold' },
        { token: 'status-warn', foreground: 'ff9800', fontStyle: 'bold' },
        { token: 'header', foreground: '569cd6', fontStyle: 'bold' },
        { token: 'separator', foreground: '808080' },
        { token: 'box-drawing', foreground: 'd4d4d4' },
      ],
      colors: {
        'editor.background': '#2a2a2a',
      },
    });

    monaco.editor.defineTheme('fslint-light', {
      base: 'vs',
      inherit: true,
      rules: [
        { token: 'status-pass', foreground: '2e7d32', fontStyle: 'bold' },
        { token: 'status-fail', foreground: 'c62828', fontStyle: 'bold' },
        { token: 'status-warn', foreground: 'ef6c00', fontStyle: 'bold' },
        { token: 'header', foreground: '005a9e', fontStyle: 'bold' },
        { token: 'separator', foreground: 'a0a0a0' },
        { token: 'box-drawing', foreground: '333333' },
      ],
      colors: {
        'editor.background': '#ffffff',
      },
    });
  });
};

/**
 * Shared editor options for Monaco instances.
 */
export const commonEditorOptions = {
  readOnly: true,
  renderWhitespace: 'none' as const,
  fontSize: 13,
  lineHeight: 21,
  minimap: { enabled: true },
  scrollBeyondLastLine: false,
  automaticLayout: true,
  contextmenu: false,
  wordWrap: 'off' as const,
  scrollBeyondLastColumn: 2,
  fontFamily: "'JetBrains Mono', 'Fira Code', 'Cascadia Code', 'Source Code Pro', monospace",
  fontVariantNumeric: 'tabular-nums',
  textRendering: 'optimizeSpeed',
};
