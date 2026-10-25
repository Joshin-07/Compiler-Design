const http = require('http');
const fs = require('fs');
const path = require('path');
const os = require('os');
const { execFile, spawn } = require('child_process');

const PORT = 8080;
const WEB_DIR = __dirname;
const ROOT_DIR = path.resolve(__dirname, '..');
const BIN_PATH = path.join(os.tmpdir(), `arithmetic-compiler-${process.pid}${process.platform === 'win32' ? '.exe' : ''}`);

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8'
};

const server = http.createServer((req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  if (req.url === '/analyze' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      let expr = '';
      try {
        const parsed = JSON.parse(body || '{}');
        expr = parsed.expr !== undefined ? String(parsed.expr) : body.trim();
      } catch (e) {
        expr = body.trim();
      }

      const child = spawn(BIN_PATH, []);
      let stdout = '';
      let stderr = '';

      child.stdout.on('data', d => { stdout += d; });
      child.stderr.on('data', d => { stderr += d; });

      child.on('close', () => {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ output: stdout.trim() || stderr.trim() }));
      });

      child.on('error', err => {
        res.writeHead(500, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ output: 'Error: ' + err.message }));
      });

      child.stdin.write(expr + '\n');
      child.stdin.end();
    });
    return;
  }

  let reqPath = req.url.split('?')[0];
  if (reqPath === '/' || reqPath === '') reqPath = '/index.html';
  const filePath = path.join(WEB_DIR, reqPath);

  if (!filePath.startsWith(WEB_DIR)) {
    res.writeHead(403);
    res.end('Access Denied');
    return;
  }

  fs.stat(filePath, (err, stats) => {
    if (err || !stats.isFile()) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('404 Not Found');
      return;
    }
    const ext = path.extname(filePath).toLowerCase();
    res.writeHead(200, { 'Content-Type': MIME_TYPES[ext] || 'text/plain' });
    fs.createReadStream(filePath).pipe(res);
  });
});

execFile('gcc', ['main.c', '-o', BIN_PATH], { cwd: ROOT_DIR }, (error, stdout, stderr) => {
  if (error) {
    if (error.code === 'ENOENT') {
      console.error('gcc not found, install MinGW/gcc');
    } else {
      console.error(`gcc compilation failed: ${stderr || error.message}`);
    }
    process.exitCode = 1;
    return;
  }

  server.listen(PORT, () => {
    console.log(`Server running at http://localhost:${PORT}/`);
  });
});
