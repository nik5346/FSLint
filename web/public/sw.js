const FMU_CONTENTS_PREFIX = '/fmu-contents';

self.addEventListener('install', () => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);

  if (url.pathname.startsWith(FMU_CONTENTS_PREFIX)) {
    event.respondWith(
      (async () => {
        // We need to find the main window client, as it has the file data
        const allClients = await self.clients.matchAll({ type: 'window' });
        const client = allClients.find((c) => !c.url.includes(FMU_CONTENTS_PREFIX));

        if (!client) {
          return new Response('Main client not found', { status: 404 });
        }

        const filePath = url.pathname.substring(FMU_CONTENTS_PREFIX.length);

        return new Promise((resolve) => {
          const messageChannel = new MessageChannel();
          messageChannel.port1.onmessage = (msgEvent) => {
            if (msgEvent.data.error) {
              resolve(new Response(msgEvent.data.error, { status: 404 }));
            } else {
              const { data, mimeType } = msgEvent.data;
              resolve(
                new Response(data, {
                  headers: {
                    'Content-Type': mimeType,
                    // Ensure the browser doesn't cache these virtual files too aggressively
                    'Cache-Control': 'no-cache'
                  },
                }),
              );
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
