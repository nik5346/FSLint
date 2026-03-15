import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { execSync } from 'child_process'
import path from 'path'

const getVersion = () => {
  try {
    const root = path.resolve(__dirname, '..')
    const scriptPath = path.join(root, 'scripts', 'get_version.py')
    // Try 'python' first (Windows default), then 'python3' (Linux/macOS)
    let command = `python "${scriptPath}"`
    try {
      return execSync(command, { stdio: 'pipe' }).toString().trim()
    } catch {
      command = `python3 "${scriptPath}"`
      return execSync(command, { stdio: 'pipe' }).toString().trim()
    }
  } catch (e) {
    console.error('Failed to get version:', e)
    return '0.0.1'
  }
}

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  base: './',
  define: {
    __APP_VERSION__: JSON.stringify(getVersion()),
    __BUILD_YEAR__: JSON.stringify(new Date().getFullYear().toString()),
  },
  server: {
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
})
