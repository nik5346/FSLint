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

export async function getFilesFromHandle(
  handle: FileSystemDirectoryHandle,
  path = handle.name,
): Promise<File[]> {
  const files: File[] = [];
  for await (const entry of handle.values()) {
    const entryPath = `${path}/${entry.name}`;
    if (entry.kind === 'file') {
      const file = await (entry as FileSystemFileHandle).getFile();
      Object.defineProperty(file, 'webkitRelativePath', {
        value: entryPath,
      });
      files.push(file);
    } else if (entry.kind === 'directory') {
      const subFiles = await getFilesFromHandle(entry as FileSystemDirectoryHandle, entryPath);
      files.push(...subFiles);
    }
  }
  return files;
}

export async function getFilesFromEntry(entry: FileSystemEntry): Promise<File[]> {
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
          resolve([file]);
        },
        (err) => reject(err),
      );
    });
  } else if (entry.isDirectory) {
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
    const files = await Promise.all(entries.map((e) => getFilesFromEntry(e)));
    return files.flat();
  }
  return [];
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
