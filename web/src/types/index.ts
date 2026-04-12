/**
 * Defines the theme colors for the application.
 */
export interface Theme {
  /** Background color. */
  bg: string;
  /** Surface color for cards or panels. */
  surface: string;
  /** Border color. */
  border: string;
  /** Main text color. */
  text: string;
  /** Muted text color. */
  muted: string;
  /** Terminal background color. */
  termBg: string;
  /** Terminal text color. */
  termText: string;
  /** Terminal button hover color. */
  termBtnHover: string;
  /** Icon hover color. */
  iconHover: string;
  /** Button border color. */
  buttonBorder: string;
  /** Button hover background color. */
  buttonHoverBg: string;
}

/**
 * Interface for the Emscripten WASM module of FSLint.
 */
export interface FSLintModule {
  /** The Emscripten virtual file system. */
  FS: {
    /** Writes a file to the virtual FS. */
    writeFile: (path: string, data: Uint8Array) => void;
    /** Deletes a file from the virtual FS. */
    unlink: (path: string) => void;
    /** Creates a directory in the virtual FS. */
    mkdir: (path: string) => void;
    /** Deletes a directory from the virtual FS. */
    rmdir: (path: string) => void;
    /** Lists the contents of a directory. */
    readdir: (path: string) => string[];
    /** Gets information about a path. */
    stat: (path: string) => {
      /**
       *
       */
      mode: number;
    };
    /** Checks if a mode corresponds to a file. */
    isFile: (mode: number) => boolean;
    /** Checks if a mode corresponds to a directory. */
    isDir: (mode: number) => boolean;
    /** Changes the current working directory. */
    chdir: (path: string) => void;
    /** Gets the current working directory. */
    cwd: () => string;
    /** Reads a file from the virtual FS. */
    readFile: (
      path: string,
      opts?: {
        /**
         *
         */
        encoding?: string; /**
         *
         */
        flags?: string;
      },
    ) => Uint8Array | string;
  };
  /** The main entry point for running the validation logic. */
  _run_validation: (path: number) => number;
  /** Allocates space on the Emscripten stack. */
  stackAlloc: (size: number) => number;
  /** Saves the current Emscripten stack position. */
  stackSave: () => number;
  /** Restores the Emscripten stack to a previous position. */
  stackRestore: (stack: number) => void;
  /** Converts a JavaScript string to a UTF-8 string on the Emscripten stack. */
  stringToUTF8: (str: string, outPtr: number, maxBytes: number) => void;
  /** Converts a pointer to a UTF-8 string on the Emscripten stack to a JavaScript string. */
  UTF8ToString: (ptr: number) => string;
}

/**
 * Represents a node in the file explorer tree.
 */
export interface FileNode {
  /** The name of the file or directory. */
  name: string;
  /** The relative path from the FMU root. */
  path: string;
  /** Whether this is a file or a directory. */
  kind: 'file' | 'directory';
  /** Whether the file is binary. */
  isBinary: boolean;
  /** The size of the file in bytes. */
  size?: number;
  /** Children nodes if this is a directory. */
  children?: FileNode[];
}

/**
 * Summary information about the validated model.
 */
export interface ModelSummary {
  /** The standard used (e.g., 'FMI'). */
  standard: string;
  /** The model name. */
  model_name: string;
  /** The FMI version. */
  fmi_version: string;
  /** The model version. */
  model_version: string;
  /** The model's unique identifier (GUID or UUID). */
  guid: string;
  /** The tool used to generate the FMU. */
  generation_tool: string;
  /** When the FMU was generated. */
  generation_date_and_time: string;
  /** The author of the model. */
  author: string;
  /** Copyright information. */
  copyright: string;
  /** License information. */
  license: string;
  /** Brief description of the model. */
  description: string;
  /** List of supported platforms. */
  platforms: string[];
  /** List of supported interfaces (e.g., Co-Simulation). */
  interfaces: string[];
  /** List of supported layered standards. */
  layered_standards: string[];
  /** Whether the FMU includes an icon. */
  has_icon: boolean;
  /** List of supported FMU types. */
  fmu_types: string[];
  /** The language used for the source code. */
  source_language: string;
  /** The total recursive size of the FMU. */
  total_size: number;
}

/**
 * Represents an individual test result.
 */
export interface TestResult {
  /** The name of the test. */
  test_name: string;
  /** The status of the test. */
  status: 'PASS' | 'FAIL' | 'WARNING';
  /** Detailed messages about the test outcome. */
  messages: string[];
}

/**
 * Represents a hierarchical test result for nested models.
 */
export interface NestedModelResult {
  /** The segment name of the nested model (e.g. "inner.fmu"). */
  name: string;
  /** The full logical path from the validation root (e.g. "inner.fmu/even_inner.fmu"). */
  logical_path: string;
  /** The validation status of this model. */
  status: 'PASS' | 'FAIL' | 'WARNING';
  /** Metadata summary for this model, if available. */
  summary?: Pick<
    ModelSummary,
    'model_name' | 'standard' | 'fmi_version' | 'guid' | 'generation_tool'
  >;
  /** Individual test results for this model, if available. */
  results?: TestResult[];
  /** Results of further nested models. */
  nested_models?: NestedModelResult[];
}

/**
 * The complete validation result returned by the checker.
 */
export interface ValidationResult {
  /** The text-based validation report. */
  report: string;
  /** The overall validation status. */
  overallStatus: 'PASS' | 'FAIL' | 'WARNING';
  /** Metadata summary. */
  summary: ModelSummary;
  /** List of individual test results. */
  results: TestResult[];
  /** Hierarchical test results for nested models. */
  nested_models: NestedModelResult[];
  /** The file tree of the FMU contents. */
  file_tree?: FileNode;
}

declare global {
  /** The version of the application. */
  const __APP_VERSION__: string;
  /** The year the application was built. */
  const __BUILD_YEAR__: string;
  /** Extended window object with FSLint-specific functions. */
  interface Window {
    /** Global function to initialize the WASM module. */
    createFSLintModule: (config: {
      /** Callback for normal output. */
      print: (text: string) => void;
      /** Callback for error output. */
      printErr: (text: string) => void;
      /** Callback to resolve file paths for assets. */
      locateFile?: (path: string, prefix: string) => string;
    }) => Promise<FSLintModule>;
    /** Standard directory picker API. */
    showDirectoryPicker?: (options?: {
      /** Selection mode. */
      mode?: 'read' | 'readwrite';
    }) => Promise<FileSystemDirectoryHandle>;
  }

  /** Base interface for modern file system handles. */
  interface FileSystemHandle {
    /** The handle kind. */
    readonly kind: 'file' | 'directory';
    /** The name of the entry. */
    readonly name: string;
  }

  /** Handle for a file. */
  interface FileSystemFileHandle extends FileSystemHandle {
    /** The handle kind is 'file'. */
    readonly kind: 'file';
    /** Retrieves the File object. */
    getFile(): Promise<File>;
  }

  /** Handle for a directory. */
  interface FileSystemDirectoryHandle extends FileSystemHandle {
    /** The handle kind is 'directory'. */
    readonly kind: 'directory';
    /** Retrieves an iterator for directory contents. */
    values(): AsyncIterableIterator<FileSystemHandle>;
  }
}
