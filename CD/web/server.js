/**
 * server.js — HTTP Server for Arithmetic Expression Compiler
 *
 * Runs without external dependencies (pure Node.js standard library).
 * Serves static web assets and provides POST /analyze by invoking
 * the compiled native C binary (`aec.exe` or `./aec`) with `--json`.
 *
 * Usage:
 *   node web/server.js
 * Open: http://localhost:8080/
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const PORT = 8080;
const WEB_DIR = __dirname;
const ROOT_DIR = path.resolve(__dirname, '..');

// Find binary: check for aec.exe (Windows) or aec (Unix)
function getBinaryPath() {
  const isWin = process.platform === 'win32';
  const binName = isWin ? 'aec.exe' : 'aec';
  const localBin = path.join(ROOT_DIR, binName);
  if (fs.existsSync(localBin)) return localBin;
  return path.join(ROOT_DIR, isWin ? 'aec.exe' : 'aec');
}

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json',
  '.png': 'image/png',
  '.svg': 'image/svg+xml'
};

const server = http.createServer((req, res) => {
  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  // API Endpoint: POST /analyze
  if (req.url === '/analyze' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      let expr = '';
      const trimmed = (body || '').trim();
      try {
        const parsed = JSON.parse(trimmed || '{}');
        if (typeof parsed === 'object' && parsed !== null && parsed.expr !== undefined) {
          expr = String(parsed.expr);
        } else if (typeof parsed === 'string') {
          expr = parsed;
        } else {
          expr = trimmed;
        }
      } catch (e) {
        const match = trimmed.match(/"?expr"?\s*[:=]\s*"?([^"}\n\r&]+)"?/);
        if (match) {
          expr = match[1].trim();
        } else {
          expr = trimmed;
        }
      }

      const binPath = getBinaryPath();
      if (!fs.existsSync(binPath)) {
        res.writeHead(500, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
          valid: false,
          errors: [{ message: `Compiler binary not found at ${binPath}. Please run 'make' first.`, line: 1, col: 1 }]
        }));
        return;
      }

      // Spawn native C compiler with --json mode and pipe expr into stdin
      const child = spawn(binPath, ['--json']);
      let stdout = '';
      let stderr = '';

      child.stdout.on('data', data => { stdout += data; });
      child.stderr.on('data', data => { stderr += data; });

      child.on('close', code => {
        const output = stdout.trim();
        res.writeHead(200, { 'Content-Type': 'application/json' });
        if (output.startsWith('{') && output.endsWith('}')) {
          res.end(output);
        } else {
          // Wrap if output wasn't pure JSON
          res.end(JSON.stringify({
            valid: false,
            errors: [{ message: stderr.trim() || output || 'Execution failed', line: 1, col: 1 }]
          }));
        }
      });

      child.on('error', err => {
        res.writeHead(500, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
          valid: false,
          errors: [{ message: 'Failed to execute binary: ' + err.message, line: 1, col: 1 }]
        }));
      });

      child.stdin.write(expr + '\n');
      child.stdin.end();
    });
    return;
  }

  // Static File Serving
  let reqPath = req.url.split('?')[0];
  if (reqPath === '/' || reqPath === '') reqPath = '/index.html';

  const filePath = path.join(WEB_DIR, reqPath);

  // Security check: stay within WEB_DIR
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
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    res.writeHead(200, { 'Content-Type': contentType });
    fs.createReadStream(filePath).pipe(res);
  });
});

server.listen(PORT, () => {
  console.log(`=======================================================`);
  console.log(` Arithmetic Expression Compiler — Web Fallback Server  `);
  console.log(`=======================================================`);
  console.log(`Server running at:  http://localhost:${PORT}/`);
  console.log(`Native C Binary:    ${getBinaryPath()}`);
  console.log(`Press Ctrl+C to stop.`);
});
