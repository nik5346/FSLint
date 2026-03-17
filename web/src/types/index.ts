export interface Theme {
  bg: string;
  surface: string;
  border: string;
  text: string;
  muted: string;
  termBg: string;
  termText: string;
  termBtnHover: string;
  iconHover: string;
  buttonBorder: string;
  buttonHoverBg: string;
}

export interface FSLintModule {
  FS: {
    writeFile: (path: string, data: Uint8Array) => void;
    unlink: (path: string) => void;
    mkdir: (path: string) => void;
    rmdir: (path: string) => void;
    readdir: (path: string) => string[];
    stat: (path: string) => { mode: number };
    isFile: (mode: number) => boolean;
    isDir: (mode: number) => boolean;
    chdir: (path: string) => void;
    cwd: () => string;
    readFile: (path: string, opts?: { encoding?: string; flags?: string }) => Uint8Array | string;
  };
  _is_binary: (path: number) => number;
  _get_file_tree_json: (path: number) => number;
  _run_validation: (path: number) => void;
  stackAlloc: (size: number) => number;
  stackSave: () => number;
  stackRestore: (stack: number) => void;
  stringToUTF8: (str: string, outPtr: number, maxBytes: number) => void;
  UTF8ToString: (ptr: number) => string;
}

export interface FileNode {
  name: string;
  path: string;
  kind: 'file' | 'directory';
  isBinary: boolean;
  size?: number;
  children?: FileNode[];
}

declare global {
  const __APP_VERSION__: string;
  const __BUILD_YEAR__: string;
  interface Window {
    createFSLintModule: (config: {
      print: (text: string) => void;
      printErr: (text: string) => void;
      locateFile?: (path: string, prefix: string) => string;
    }) => Promise<FSLintModule>;
    showDirectoryPicker?: (options?: {
      mode?: 'read' | 'readwrite';
    }) => Promise<FileSystemDirectoryHandle>;
  }

  interface FileSystemHandle {
    readonly kind: 'file' | 'directory';
    readonly name: string;
  }

  interface FileSystemFileHandle extends FileSystemHandle {
    readonly kind: 'file';
    getFile(): Promise<File>;
  }

  interface FileSystemDirectoryHandle extends FileSystemHandle {
    readonly kind: 'directory';
    values(): AsyncIterableIterator<FileSystemHandle>;
  }
}
