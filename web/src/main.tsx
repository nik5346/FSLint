import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);

if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    /**
     * Registers the service worker for the application.
     */
    navigator.serviceWorker.register('./sw.js').catch((err) => {
      console.error('Service Worker registration failed:', err);
    });
  });
}
