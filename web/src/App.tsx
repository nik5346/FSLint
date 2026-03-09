import { useState, useEffect, useRef } from 'react';

declare global {
  interface Window {
    createFSLintModule: any;
  }
}

function App() {
  const [module, setModule] = useState<any>(null);
  const [output, setOutput] = useState<string>('');
  const [isReady, setIsReady] = useState(false);
  const [isProcessing, setIsProcessing] = useState(false);
  const outputEndRef = useRef<HTMLPreElement>(null);

  useEffect(() => {
    const script = document.createElement('script');
    script.src = 'FSLint-cli-wasm.js';
    script.async = true;
    script.onload = async () => {
      try {
        const mod = await window.createFSLintModule({
          print: (text: string) => setOutput((prev) => prev + text + '\n'),
          printErr: (text: string) => setOutput((prev) => prev + 'Error: ' + text + '\n'),
        });
        setModule(mod);
        setIsReady(true);
        setOutput('FSLint Web ready. Drop an FMU or SSP file to validate.\n');
      } catch (err) {
        setOutput('Error initializing WASM module: ' + err + '\n');
      }
    };
    document.body.appendChild(script);
  }, []);

  useEffect(() => {
    if (outputEndRef.current) {
      outputEndRef.current.scrollTop = outputEndRef.current.scrollHeight;
    }
  }, [output]);

  const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      await processFile(file);
    }
  };

  const onDrop = async (event: React.DragEvent<HTMLDivElement>) => {
    event.preventDefault();
    const file = event.dataTransfer.files?.[0];
    if (file) {
      await processFile(file);
    }
  };

  const processFile = async (file: File) => {
    if (!module || isProcessing) return;

    setIsProcessing(true);
    setOutput(`Validating ${file.name}...\n`);

    try {
      const arrayBuffer = await file.arrayBuffer();
      const data = new Uint8Array(arrayBuffer);

      // Write file to virtual filesystem
      module.FS.writeFile(file.name, data);

      // Call main with the filename
      module.callMain([file.name]);

      // Optional: Clean up file from FS
      module.FS.unlink(file.name);
    } catch (err) {
      setOutput((prev) => prev + 'Error during validation: ' + err + '\n');
    } finally {
      setIsProcessing(false);
    }
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100vh',
      padding: '20px',
      boxSizing: 'border-box',
      gap: '20px'
    }}>
      <header>
        <h1 style={{ margin: 0 }}>FSLint Web</h1>
        <p style={{ color: '#888' }}>Validate FMU (1.0, 2.0, 3.0) and SSP (1.0, 2.0) files in your browser.</p>
      </header>

      <div
        onDragOver={(e) => e.preventDefault()}
        onDrop={onDrop}
        style={{
          border: '2px dashed #444',
          borderRadius: '8px',
          padding: '40px',
          textAlign: 'center',
          backgroundColor: '#252525',
          cursor: isReady && !isProcessing ? 'pointer' : 'wait',
          opacity: isReady && !isProcessing ? 1 : 0.6
        }}
        onClick={() => !isProcessing && document.getElementById('fileInput')?.click()}
      >
        <input
          id="fileInput"
          type="file"
          style={{ display: 'none' }}
          onChange={handleFileChange}
          disabled={!isReady || isProcessing}
        />
        {isProcessing ? (
          <p>Processing...</p>
        ) : (
          <p>Drag & drop an FMU/SSP file here, or click to select</p>
        )}
      </div>

      <pre
        ref={outputEndRef}
        style={{
          flex: 1,
          backgroundColor: '#000',
          color: '#00ff00',
          padding: '15px',
          borderRadius: '4px',
          overflowY: 'auto',
          margin: 0,
          whiteSpace: 'pre-wrap',
          wordBreak: 'break-all',
          fontFamily: 'monospace'
        }}
      >
        {output}
      </pre>

      <footer style={{ fontSize: '0.8em', color: '#666', textAlign: 'center' }}>
        FSLint Core runs in WebAssembly using Emscripten. All processing is done locally in your browser.
      </footer>
    </div>
  );
}

export default App;
