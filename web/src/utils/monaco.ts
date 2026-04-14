import { Monaco } from '@monaco-editor/react';

/**
 * Shared editor options for Monaco instances.
 */
export const commonEditorOptions = {
  readOnly: true,
  renderWhitespace: 'all' as const,
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

/**
 * Registers custom languages and themes in Monaco.
 * @param {Monaco} monaco - The Monaco editor instance.
 */
export const handleBeforeMount = (monaco: Monaco) => {
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
          [/^[A-Z][A-Za-z0-9 ]+:/, 'header'],

          // Box drawing characters
          [/[┌┐└┘├┤┬┴┼─│]/, 'box-drawing'],

          // Security markers
          [/\[SECURITY\]/, 'status-security'],

          // Overall results and outcomes
          [/Overall Status: PASS[ED]*/, 'status-pass'],
          [/Overall Status: FAIL[ED]*/, 'status-fail'],
          [/Overall Status: WARN[ED]*/, 'status-warn'],
          [/Result: PASS[ED]*/, 'status-pass'],
          [/Result: FAIL[ED]*/, 'status-fail'],
          [/Result: WARN[ED]*/, 'status-warn'],
          [/MODEL VALIDATION PASSED/, 'status-pass'],
          [/MODEL VALIDATION FAILED/, 'status-fail'],

          // Numbers and paths
          [/\d+/, 'number'],
          [/[a-zA-Z0-9_/.-]+\.[a-z]{2,4}/, 'path'],
        ],
      },
    });
  }

  // Register CSV language if not already registered
  if (!monaco.languages.getLanguages().some((l: { id: string }) => l.id === 'csv')) {
    monaco.languages.register({ id: 'csv' });
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
      { token: 'status-security', foreground: '4fc3f7', fontStyle: 'bold' },
    ],
    colors: {
      'editor.background': '#2a2a2a',
      'editor.foreground': '#f0f0f0',
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
      { token: 'status-security', foreground: '0288d1', fontStyle: 'bold' },
    ],
    colors: {
      'editor.background': '#ffffff',
      'editor.foreground': '#111418',
    },
  });
};
