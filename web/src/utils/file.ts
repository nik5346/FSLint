export const extractHeaders = (text: string) => {
  const lines = text.split('\n');
  const headers: { level: number; text: string; line: number }[] = [];
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

export interface CollectedItems {
  files: File[];
  directories: string[];
}

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
