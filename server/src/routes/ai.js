const express = require('express');
const http    = require('http');

const router = express.Router();

const OLLAMA_HOST = process.env.OLLAMA_HOST || 'localhost';
const OLLAMA_PORT = parseInt(process.env.OLLAMA_PORT || '11434', 10);

// POST /api/ai/generate — proxies to local Ollama, streams response back to ESP32
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
    res.status(ollamaRes.statusCode);
    ollamaRes.pipe(res);
  });

  proxy.on('error', (err) => {
    console.error('[ai/generate] Ollama unreachable:', err.message);
    res.status(502).json({ error: 'Ollama unreachable' });
  });

  proxy.write(body);
  proxy.end();
});

module.exports = router;
