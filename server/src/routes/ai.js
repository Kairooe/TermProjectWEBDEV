const express = require('express');
const http    = require('http');

const router = express.Router();

const OLLAMA_HOST    = process.env.OLLAMA_HOST || 'localhost';
const OLLAMA_PORT    = parseInt(process.env.OLLAMA_PORT || '11434', 10);
const OLLAMA_TIMEOUT = 120000; // 120 s — LLM inference can be slow

// POST /api/ai/generate — proxies to local Ollama, buffers full response before
// returning to caller. Buffering (rather than piping) ensures:
//   1. A proper Content-Length header is sent so the ESP32 knows when the body ends.
//   2. A socket timeout can abort the Ollama request cleanly on a hung inference.
router.post('/generate', (req, res) => {
  const body = JSON.stringify(req.body);

  const options = {
    hostname: OLLAMA_HOST,
    port:     OLLAMA_PORT,
    path:     '/api/generate',
    method:   'POST',
    headers: {
      'Content-Type':   'application/json',
      'Content-Length': Buffer.byteLength(body),
    },
  };

  const proxy = http.request(options, (ollamaRes) => {
    let chunks = [];
    ollamaRes.on('data', chunk => chunks.push(chunk));
    ollamaRes.on('end', () => {
      const data = Buffer.concat(chunks);
      res.status(ollamaRes.statusCode)
         .set('Content-Type', 'application/json')
         .send(data);
    });
  });

  // Kill the request if Ollama hasn't responded within the timeout.
  // Without this the proxy hangs until Cloudflare drops the tunnel connection.
  proxy.setTimeout(OLLAMA_TIMEOUT, () => {
    proxy.destroy();
    console.error('[ai/generate] Ollama timed out after', OLLAMA_TIMEOUT / 1000, 's');
    if (!res.headersSent) res.status(504).json({ error: 'Ollama timeout' });
  });

  proxy.on('error', (err) => {
    console.error('[ai/generate] Ollama error:', err.message);
    if (!res.headersSent) res.status(502).json({ error: 'Ollama unreachable' });
  });

  proxy.write(body);
  proxy.end();
});

module.exports = router;
