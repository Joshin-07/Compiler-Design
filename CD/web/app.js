/**
 * app.js — Arithmetic Expression Compiler Web UI Controller
 *
 * Connects the web interface to the C compiler front end via
 * the local Node.js server (POST /analyze calling native `aec.exe --json`).
 *
 * Zero external libraries or frameworks. Pure vanilla JS.
 */

(function () {
  'use strict';

  // --- State ---
  let debounceTimer = null;
  const historyList = [];        // In-memory only (max 10 items)

  // --- DOM Elements ---
  const exprInput = document.getElementById('exprInput');
  const analyzeBtn = document.getElementById('analyzeBtn');
  const clearBtn = document.getElementById('clearBtn');
  const colTracker = document.getElementById('colTracker');
  const errorUnderline = document.getElementById('errorUnderline');
  const engineBadge = document.getElementById('engineBadge');
  const engineText = document.getElementById('engineText');

  const statusBadge = document.getElementById('statusBadge');
  const statusText = document.getElementById('statusText');
  const resultVal = document.getElementById('resultVal');
  const parensVal = document.getElementById('parensVal');
  const postfixVal = document.getElementById('postfixVal');
  const diagnosticsArea = document.getElementById('diagnosticsArea');

  const astSvg = document.getElementById('astSvg');
  const tokenTableBody = document.getElementById('tokenTableBody');
  const tokenCount = document.getElementById('tokenCount');
  const symbolList = document.getElementById('symbolList');
  const symbolCount = document.getElementById('symbolCount');
  const historyContainer = document.getElementById('historyList');

  // Precedence modal elements
  const togglePrecedenceBtn = document.getElementById('togglePrecedenceBtn');
  const precedenceModal = document.getElementById('precedenceModal');
  const closePrecedenceBtn = document.getElementById('closePrecedenceBtn');
  const loadPrecedenceDemoBtn = document.getElementById('loadPrecedenceDemoBtn');

  // ===================================================================
  //  1. Engine Initialization
  // ===================================================================

  async function initEngine() {
    engineText.textContent = 'Checking server...';

    try {
      const resp = await fetch('/analyze', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ expr: '2+3' })
      });
      if (resp.ok) {
        setEngineMode('🖥️ Native C Engine (aec.exe)');
        runAnalysis();
        return;
      }
    } catch (_) {
      // Server not answering yet
    }

    setEngineMode('🖥️ Server (Connecting...)');
    runAnalysis();
  }

  function setEngineMode(label) {
    engineBadge.className = 'engine-badge engine-cli';
    engineText.textContent = label;
  }

  // ===================================================================
  //  2. Analysis Dispatcher
  // ===================================================================

  async function executeAnalysis(expression) {
    try {
      const resp = await fetch('/analyze', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ expr: expression })
      });
      if (!resp.ok) {
        throw new Error('Server returned HTTP ' + resp.status);
      }
      return await resp.json();
    } catch (err) {
      return {
        valid: false,
        errors: [{
          message: 'Backend unavailable. Make sure the server is running ("node web/server.js").',
          line: 1,
          col: 1
        }],
        warnings: [],
        tokens: [],
        ast: null,
        parenthesized: null,
        postfix: null,
        result: null,
        symbols: []
      };
    }
  }

  async function runAnalysis() {
    const expr = exprInput.value.trim();
    if (!expr) {
      renderEmptyState();
      return;
    }

    const data = await executeAnalysis(expr);
    renderResults(expr, data);
    addToHistory(expr, data.valid);
  }

  // ===================================================================
  //  3. Render Results
  // ===================================================================

  function renderResults(expr, data) {
    // 3.1 Status Badge
    if (data.valid) {
      statusBadge.className = 'status-pill status-valid';
      statusBadge.innerHTML = '<span class="status-icon">✓</span><span>VALID' + 
        (data.warnings && data.warnings.length ? ' (Warnings)' : '') + '</span>';
    } else {
      statusBadge.className = 'status-pill status-invalid';
      statusBadge.innerHTML = '<span class="status-icon">✗</span><span>INVALID</span>';
    }

    // 3.2 Metrics
    resultVal.textContent = data.result !== null && data.result !== undefined 
      ? data.result 
      : (data.symbols && data.symbols.length > 0 ? '(symbolic)' : '—');
    parensVal.textContent = data.parenthesized || '—';
    postfixVal.textContent = data.postfix || '—';

    // 3.3 Diagnostics & Error Caret Track
    renderDiagnostics(expr, data.errors || [], data.warnings || []);

    // 3.4 AST Tree Visualization
    renderAstTree(data.ast);

    // 3.5 Token Table
    renderTokenTable(data.tokens || []);

    // 3.6 Symbol Table
    renderSymbolTable(data.symbols || []);
  }

  function renderEmptyState() {
    statusBadge.className = 'status-pill';
    statusBadge.innerHTML = '<span>READY</span>';
    resultVal.textContent = '—';
    parensVal.textContent = '—';
    postfixVal.textContent = '—';
    diagnosticsArea.style.display = 'none';
    errorUnderline.innerHTML = '';
    astSvg.innerHTML = '<text x="50%" y="50%" text-anchor="middle" fill="#64748b" font-family="sans-serif" font-size="14">Enter an arithmetic expression above</text>';
    tokenTableBody.innerHTML = '<tr><td colspan="4" class="empty-state" style="text-align:center; padding: 20px;">No tokens generated</td></tr>';
    tokenCount.textContent = '0 tokens';
    symbolList.innerHTML = '<p class="empty-state">No variables referenced.</p>';
    symbolCount.textContent = '0 identifiers';
  }

  // ===================================================================
  //  4. Diagnostics & Caret Highlight
  // ===================================================================

  function renderDiagnostics(expr, errors, warnings) {
    diagnosticsArea.innerHTML = '';
    errorUnderline.innerHTML = '';

    const hasErrors = errors && errors.length > 0;
    const hasWarnings = warnings && warnings.length > 0;

    if (!hasErrors && !hasWarnings) {
      diagnosticsArea.style.display = 'none';
      return;
    }

    diagnosticsArea.style.display = 'flex';

    // Highlight the primary error column in the underline track
    if (hasErrors) {
      const primaryErr = errors[0];
      const col = primaryErr.col || 1;

      // Render caret pointer under the character
      // We estimate monospace character offset based on input font metrics
      const charWidth = 11.25; // standard monospace ch width at font-size 1.25rem
      const leftPx = 16 + (col - 1) * charWidth;
      
      const caret = document.createElement('span');
      caret.className = 'caret-pointer';
      caret.style.left = `${leftPx}px`;
      caret.textContent = '▲';
      caret.title = `Error at column ${col}`;
      errorUnderline.appendChild(caret);
    }

    // Render Error Banners
    errors.forEach(err => {
      const banner = document.createElement('div');
      banner.className = 'diag-banner diag-error';
      banner.innerHTML = `
        <span class="diag-icon">✗</span>
        <div>
          <span class="diag-pos">Line ${err.line || 1}:${err.col || 1}</span>
          <strong>${escapeHtml(err.message)}</strong>
        </div>
      `;
      diagnosticsArea.appendChild(banner);
    });

    // Render Warning Banners
    warnings.forEach(warn => {
      const banner = document.createElement('div');
      banner.className = 'diag-banner diag-warning';
      const msg = typeof warn === 'string' ? warn : warn.message;
      const pos = warn.line ? `Line ${warn.line}:${warn.col}` : 'Warning';
      banner.innerHTML = `
        <span class="diag-icon">⚠</span>
        <div>
          <span class="diag-pos">${pos}</span>
          <span>${escapeHtml(msg)}</span>
        </div>
      `;
      diagnosticsArea.appendChild(banner);
    });
  }

  // ===================================================================
  //  5. AST SVG Visualizer
  // ===================================================================

  function renderAstTree(rootNode) {
    astSvg.innerHTML = '';
    if (!rootNode) {
      astSvg.innerHTML = '<text x="50%" y="50%" text-anchor="middle" fill="#64748b" font-family="sans-serif" font-size="14">No syntax tree available (check errors)</text>';
      return;
    }

    // 1. Calculate subtree bounding widths
    const nodeWidth = 76;
    const nodeHeight = 52;
    const levelHeight = 84;
    const siblingGap = 28;

    function measureSubtree(node) {
      if (!node) return 0;
      if (node.type === 'BINOP') {
        const leftW = measureSubtree(node.left);
        const rightW = measureSubtree(node.right);
        node._width = Math.max(nodeWidth, leftW + rightW + siblingGap);
        node._leftW = leftW;
        node._rightW = rightW;
        return node._width;
      } else if (node.type === 'UNARY') {
        const opW = measureSubtree(node.operand);
        node._width = Math.max(nodeWidth, opW + siblingGap);
        return node._width;
      } else {
        node._width = nodeWidth;
        return node._width;
      }
    }

    measureSubtree(rootNode);

    // 2. Assign absolute (x, y) coordinates
    function assignPositions(node, x, y) {
      if (!node) return;
      node.x = x;
      node.y = y;

      if (node.type === 'BINOP') {
        const totalW = node._leftW + node._rightW + siblingGap;
        const leftCenter = x - (totalW / 2) + (node._leftW / 2);
        const rightCenter = x + (totalW / 2) - (node._rightW / 2);
        assignPositions(node.left, leftCenter, y + levelHeight);
        assignPositions(node.right, rightCenter, y + levelHeight);
      } else if (node.type === 'UNARY') {
        assignPositions(node.operand, x, y + levelHeight);
      }
    }

    // Viewport layout dimensions
    const totalTreeWidth = Math.max(rootNode._width + 80, 500);
    const maxDepth = computeDepth(rootNode);
    const totalTreeHeight = Math.max(maxDepth * levelHeight + 90, 280);

    const startX = totalTreeWidth / 2;
    const startY = 44;
    assignPositions(rootNode, startX, startY);

    astSvg.setAttribute('viewBox', `0 0 ${totalTreeWidth} ${totalTreeHeight}`);
    astSvg.setAttribute('width', totalTreeWidth);
    astSvg.setAttribute('height', totalTreeHeight);

    // 3. Render connecting lines (links)
    const linksGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    linksGroup.setAttribute('class', 'ast-links');

    function drawLinks(node) {
      if (!node) return;
      if (node.type === 'BINOP') {
        if (node.left) {
          drawLink(node.x, node.y + nodeHeight / 2, node.left.x, node.left.y - nodeHeight / 2);
          drawLinks(node.left);
        }
        if (node.right) {
          drawLink(node.x, node.y + nodeHeight / 2, node.right.x, node.right.y - nodeHeight / 2);
          drawLinks(node.right);
        }
      } else if (node.type === 'UNARY') {
        if (node.operand) {
          drawLink(node.x, node.y + nodeHeight / 2, node.operand.x, node.operand.y - nodeHeight / 2);
          drawLinks(node.operand);
        }
      }
    }

    function drawLink(x1, y1, x2, y2) {
      const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
      const midY = (y1 + y2) / 2;
      const d = `M ${x1} ${y1} C ${x1} ${midY}, ${x2} ${midY}, ${x2} ${y2}`;
      path.setAttribute('d', d);
      path.setAttribute('class', 'ast-link');
      linksGroup.appendChild(path);
    }

    drawLinks(rootNode);
    astSvg.appendChild(linksGroup);

    // 4. Render node cards
    const nodesGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    nodesGroup.setAttribute('class', 'ast-nodes');

    function drawNodes(node) {
      if (!node) return;
      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      
      let nodeClass = 'node-binop';
      let title = '';
      let sub = '';

      if (node.type === 'BINOP') {
        nodeClass = 'node-binop';
        title = node.op || '?';
        sub = 'BINOP';
      } else if (node.type === 'UNARY') {
        nodeClass = 'node-unary';
        title = '- (neg)';
        sub = 'UNARY';
      } else if (node.type === 'NUM') {
        nodeClass = 'node-num';
        title = node.value || '0';
        sub = 'NUM';
      } else if (node.type === 'ID') {
        nodeClass = 'node-id';
        title = node.name || 'id';
        sub = 'ID';
      }

      g.setAttribute('class', `ast-node ${nodeClass}`);
      g.setAttribute('transform', `translate(${node.x}, ${node.y})`);

      // Rectangle background
      const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      rect.setAttribute('x', -nodeWidth / 2);
      rect.setAttribute('y', -nodeHeight / 2);
      rect.setAttribute('width', nodeWidth);
      rect.setAttribute('height', nodeHeight);
      rect.setAttribute('class', 'ast-node-rect');
      g.appendChild(rect);

      // Main Text (operator or operand value)
      const textMain = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      textMain.setAttribute('x', 0);
      textMain.setAttribute('y', -4);
      textMain.setAttribute('class', 'ast-node-text-main');
      textMain.textContent = title;
      g.appendChild(textMain);

      // Subtitle (Type label)
      const textSub = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      textSub.setAttribute('x', 0);
      textSub.setAttribute('y', 15);
      textSub.setAttribute('class', 'ast-node-text-sub');
      textSub.textContent = sub;
      g.appendChild(textSub);

      nodesGroup.appendChild(g);

      if (node.type === 'BINOP') {
        drawNodes(node.left);
        drawNodes(node.right);
      } else if (node.type === 'UNARY') {
        drawNodes(node.operand);
      }
    }

    drawNodes(rootNode);
    astSvg.appendChild(nodesGroup);
  }

  function computeDepth(node) {
    if (!node) return 0;
    if (node.type === 'BINOP') {
      return 1 + Math.max(computeDepth(node.left), computeDepth(node.right));
    } else if (node.type === 'UNARY') {
      return 1 + computeDepth(node.operand);
    }
    return 1;
  }

  // ===================================================================
  //  6. Token & Symbol Tables
  // ===================================================================

  function renderTokenTable(tokens) {
    tokenTableBody.innerHTML = '';
    tokenCount.textContent = `${tokens.length} token${tokens.length === 1 ? '' : 's'}`;

    if (!tokens || tokens.length === 0) {
      tokenTableBody.innerHTML = '<tr><td colspan="4" class="empty-state" style="text-align:center;">No tokens</td></tr>';
      return;
    }

    tokens.forEach((tok, idx) => {
      const tr = document.createElement('tr');
      const lex = tok.type === 'EOF' ? '&lt;EOF&gt;' : escapeHtml(tok.lexeme);
      const badgeClass = 'tok-' + (tok.type || 'UNKNOWN');
      
      tr.innerHTML = `
        <td style="color: var(--text-dim);">${idx + 1}</td>
        <td><code>${lex}</code></td>
        <td><span class="tok-badge ${badgeClass}">${escapeHtml(tok.type)}</span></td>
        <td style="color: var(--text-muted);">${tok.line}:${tok.col}</td>
      `;
      tokenTableBody.appendChild(tr);
    });
  }

  function renderSymbolTable(symbols) {
    symbolList.innerHTML = '';
    const count = symbols ? symbols.length : 0;
    symbolCount.textContent = `${count} identifier${count === 1 ? '' : 's'}`;

    if (!symbols || symbols.length === 0) {
      symbolList.innerHTML = '<p class="empty-state">No variables referenced.</p>';
      return;
    }

    symbols.forEach(sym => {
      const tag = document.createElement('span');
      tag.className = 'sym-tag';
      tag.innerHTML = `<span>ID:</span> <strong>${escapeHtml(sym)}</strong>`;
      symbolList.appendChild(tag);
    });
  }

  // ===================================================================
  //  7. History (in-memory only, last 10)
  // ===================================================================

  function addToHistory(expr, isValid) {
    // Avoid duplicate if it's the exact same as the last item
    if (historyList.length > 0 && historyList[0].expr === expr) return;

    historyList.unshift({ expr, isValid, timestamp: new Date() });
    if (historyList.length > 10) historyList.pop();

    historyContainer.innerHTML = '';
    historyList.forEach(item => {
      const div = document.createElement('div');
      div.className = 'history-item';
      div.innerHTML = `
        <span class="history-expr">${escapeHtml(item.expr)}</span>
        <span class="history-status ${item.isValid ? 'valid' : 'invalid'}">
          ${item.isValid ? 'VALID' : 'INVALID'}
        </span>
      `;
      div.addEventListener('click', () => {
        exprInput.value = item.expr;
        updateColTracker();
        runAnalysis();
      });
      historyContainer.appendChild(div);
    });
  }

  // ===================================================================
  //  8. Event Listeners & UI Helpers
  // ===================================================================

  function updateColTracker() {
    const col = (exprInput.selectionStart || 0) + 1;
    colTracker.textContent = `Col: ${col}`;
  }

  // Live analysis on every keystroke (debounced 200ms)
  exprInput.addEventListener('input', () => {
    updateColTracker();
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(runAnalysis, 200);
  });

  exprInput.addEventListener('keyup', updateColTracker);
  exprInput.addEventListener('click', updateColTracker);

  exprInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      clearTimeout(debounceTimer);
      runAnalysis();
    }
  });

  analyzeBtn.addEventListener('click', () => {
    clearTimeout(debounceTimer);
    runAnalysis();
  });

  clearBtn.addEventListener('click', () => {
    exprInput.value = '';
    exprInput.focus();
    updateColTracker();
    renderEmptyState();
  });

  // Example chip buttons
  document.querySelectorAll('.chip').forEach(btn => {
    btn.addEventListener('click', () => {
      const expr = btn.getAttribute('data-expr');
      exprInput.value = expr;
      exprInput.focus();
      updateColTracker();
      runAnalysis();
    });
  });

  // Precedence Modal controls
  togglePrecedenceBtn.addEventListener('click', () => {
    precedenceModal.style.display = 'flex';
  });

  closePrecedenceBtn.addEventListener('click', () => {
    precedenceModal.style.display = 'none';
  });

  precedenceModal.addEventListener('click', (e) => {
    if (e.target === precedenceModal) precedenceModal.style.display = 'none';
  });

  loadPrecedenceDemoBtn.addEventListener('click', () => {
    precedenceModal.style.display = 'none';
    exprInput.value = '2 + 3 * 4';
    exprInput.focus();
    updateColTracker();
    runAnalysis();
  });

  document.getElementById('resetZoomBtn').addEventListener('click', () => {
    const container = document.getElementById('astContainer');
    container.scrollTo({
      left: (container.scrollWidth - container.clientWidth) / 2,
      top: 0,
      behavior: 'smooth'
    });
  });

  function escapeHtml(str) {
    if (!str) return '';
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#039;');
  }

  // Bootstrap
  initEngine();
})();
