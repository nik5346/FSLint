self.addEventListener('install', () => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  const scopePath = new URL(self.registration.scope).pathname;
  const fmuPrefix = scopePath + 'fmu-contents';

  if (url.pathname.startsWith(fmuPrefix)) {
    event.respondWith(
      (async () => {
        const allClients = await self.clients.matchAll({ type: 'window' });
        // The main client is the one that DOES NOT contain 'fmu-contents' in its URL
        const client = allClients.find((c) => !c.url.includes('fmu-contents'));

        if (!client) {
          return new Response('Main client not found', { status: 404 });
        }

        const filePath = decodeURIComponent(url.pathname.substring(fmuPrefix.length));

        return new Promise((resolve) => {
          const messageChannel = new MessageChannel();
          messageChannel.port1.onmessage = (msgEvent) => {
            if (msgEvent.data.error) {
              resolve(new Response(msgEvent.data.error, { status: 404 }));
            } else {
              const { data, mimeType } = msgEvent.data;
              const headers = new Headers({
                'Content-Type': mimeType,
                'Cache-Control': 'no-cache',
                'Cross-Origin-Resource-Policy': 'same-origin',
              });

              // Apply security headers to nested HTML to allow them to be embedded
              if (mimeType === 'text/html') {
                headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
                headers.set('Cross-Origin-Opener-Policy', 'same-origin');
              }

              resolve(new Response(data, { headers }));
            }
          };

          client.postMessage(
            {
              type: 'GET_FMU_FILE',
              path: filePath,
            },
            [messageChannel.port2],
          );
        });
      })(),
    );
  }
});
