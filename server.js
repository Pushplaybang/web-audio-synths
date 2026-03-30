const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 3000;
const ROOT = __dirname;

const MIME_TYPES = {
  '.html': 'text/html',
  '.css': 'text/css',
  '.js': 'application/javascript',
  '.json': 'application/json',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.wav': 'audio/wav',
  '.mp3': 'audio/mpeg',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
};

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  let filePath = path.resolve(ROOT, decodeURIComponent(url.pathname).replace(/^\/+/, ''));

  // Prevent directory traversal
  if (!filePath.startsWith(path.resolve(ROOT))) {
    res.writeHead(403);
    res.end('Forbidden');
    return;
  }

  // Default to index.html for directory requests
  if (filePath.endsWith(path.sep) || filePath === path.resolve(ROOT)) {
    filePath = path.join(filePath, 'index.html');
  }

  fs.stat(filePath, (err, stats) => {
    if (err || !stats.isFile()) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('Not Found');
      return;
    }

    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    res.writeHead(200, { 'Content-Type': contentType });
    fs.createReadStream(filePath)
      .on('error', () => {
        res.writeHead(500);
        res.end('Internal Server Error');
      })
      .pipe(res);
  });
});

server.listen(PORT, () => {
  console.log(`Serving on http://localhost:${PORT}`);
});
