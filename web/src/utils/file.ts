/**
 * Extracts Markdown headers from a string.
 * @param text - The Markdown text to extract headers from.
 * @returns An array of header objects.
 */
export const extractHeaders = (text: string) => {
  const lines = text.split('\n');
  const headers: {
    /** The header level (1-6). */
    level: number;
    /** The header text. */
    text: string;
    /** The line number in the source text. */
    line: number;
  }[] = [];
  lines.forEach((line, index) => {
    const match = line.match(/^(#{1,6})\s+(.+)$/);
    if (match) {
      headers.push({
        level: match[1].length,
        text: match[2].trim(),
        line: index + 1,
      });
    }
  });
  return headers;
};

/**
 * Represents a collection of files and directories.
 */
export interface CollectedItems {
  /**
   * The list of files.
   */
  files: File[];
  /**
   * The list of directory paths.
   */
  directories: string[];
}

/**
 * Recursively gets all files from a FileSystemDirectoryHandle.
 * @param handle - The directory handle to read from.
 * @param path - The current path within the directory.
 * @returns A promise that resolves to the collected items.
 */
export async function getFilesFromHandle(
  handle: FileSystemDirectoryHandle,
  path = handle.name,
): Promise<CollectedItems> {
  const result: CollectedItems = { files: [], directories: [path] };
  for await (const entry of handle.values()) {
    const entryPath = `${path}/${entry.name}`;
    if (entry.kind === 'file') {
      const file = await (entry as FileSystemFileHandle).getFile();
      Object.defineProperty(file, 'webkitRelativePath', {
        value: entryPath,
      });
      result.files.push(file);
    } else if (entry.kind === 'directory') {
      const subItems = await getFilesFromHandle(entry as FileSystemDirectoryHandle, entryPath);
      result.files.push(...subItems.files);
      result.directories.push(...subItems.directories);
    }
  }
  return result;
}

/**
 * Recursively gets all files from a FileSystemEntry (used for drag and drop).
 * @param entry - The entry to read from.
 * @returns A promise that resolves to the collected items.
 */
export async function getFilesFromEntry(entry: FileSystemEntry): Promise<CollectedItems> {
  if (entry.isFile) {
    return new Promise((resolve, reject) => {
      (entry as FileSystemFileEntry).file(
        (file) => {
          const path = entry.fullPath.startsWith('/')
            ? entry.fullPath.substring(1)
            : entry.fullPath;
          Object.defineProperty(file, 'webkitRelativePath', {
            value: path,
          });
          resolve({ files: [file], directories: [] });
        },
        (err) => reject(err),
      );
    });
  } else if (entry.isDirectory) {
    const dirPath = entry.fullPath.startsWith('/') ? entry.fullPath.substring(1) : entry.fullPath;
    const dirReader = (entry as FileSystemDirectoryEntry).createReader();
    const entries = await new Promise<FileSystemEntry[]>((resolve, reject) => {
      const allEntries: FileSystemEntry[] = [];
      /**
       * Reads entries recursively using the directory reader.
       */
      const readEntries = () => {
        dirReader.readEntries(
          (results) => {
            if (results.length === 0) {
              resolve(allEntries);
            } else {
              allEntries.push(...results);
              readEntries();
            }
          },
          (err) => reject(err),
        );
      };
      readEntries();
    });
    const subResults = await Promise.all(entries.map((e) => getFilesFromEntry(e)));
    const finalResult: CollectedItems = { files: [], directories: [dirPath] };
    for (const sub of subResults) {
      finalResult.files.push(...sub.files);
      finalResult.directories.push(...sub.directories);
    }
    return finalResult;
  }
  return { files: [], directories: [] };
}

/**
 * Decodes a Uint8Array into a string using UTF-8 or Windows-1252 fallbacks.
 * @param data - The binary data to decode.
 * @returns The decoded string.
 */
export function decodeText(data: Uint8Array): string {
  try {
    return new TextDecoder('utf-8', { fatal: true }).decode(data);
  } catch {
    try {
      return new TextDecoder('windows-1252').decode(data);
    } catch {
      return new TextDecoder('utf-8').decode(data);
    }
  }
}

/**
 * Maps file extensions to MIME types.
 */
export const mimeMap: { [key: string]: string } = {
  svg: 'image/svg+xml',
  png: 'image/png',
  jpg: 'image/jpeg',
  jpeg: 'image/jpeg',
  gif: 'image/gif',
  webp: 'image/webp',
  bmp: 'image/bmp',
  ico: 'image/x-icon',
  css: 'text/css',
  js: 'application/javascript',
  pdf: 'application/pdf',
  html: 'text/html',
  htm: 'text/html',
  txt: 'text/plain',
  xml: 'application/xml',
  xsd: 'application/xml',
};

/**
 * Performs a case-insensitive file resolution using Emscripten FS.
 * @param module - The Emscripten module.
 * @param base - The base directory.
 * @param rel - The relative path to resolve.
 * @returns The resolved path, or null if it cannot be resolved.
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export function resolveCaseInsensitive(module: any, base: string, rel: string): string | null {
  // Strip query parameters or hashes
  const cleanRel = rel.split(/[?#]/)[0].replace(/\\/g, '/').trim();
  if (!cleanRel) return null;

  const stack = base.split('/').filter(Boolean);
  const parts = cleanRel.split('/').filter(Boolean);
  for (const part of parts) {
    if (part === '.') continue;
    if (part === '..') stack.pop();
    else stack.push(part);
  }

  // Preserve absolute path if base was absolute
  const prefix = base.startsWith('/') ? '/' : '';
  let resolved = prefix + stack.join('/');

  // Recursive case-insensitive check for each component
  const components = resolved.split('/').filter(Boolean);
  let current = prefix;

  for (const component of components) {
    try {
      const next = (current === '/' ? '/' : current + '/') + component;
      module.FS.stat(next);
      current = next;
    } catch {
      try {
        const entries = module.FS.readdir(current || '/');
        const lower = component.toLowerCase();
        const found = entries.find((e: string) => e.toLowerCase() === lower);
        if (found) {
          current = (current === '/' ? '/' : current + '/') + found;
        } else {
          // If not found, just proceed with the original name and let the final stat/read fail
          current = (current === '/' ? '/' : current + '/') + component;
        }
      } catch {
        current = (current === '/' ? '/' : current + '/') + component;
      }
    }
  }
  resolved = current;

  return resolved;
}
