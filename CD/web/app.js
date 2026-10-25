const input = document.getElementById('expr-input');
const btn = document.getElementById('analyze-btn');
const output = document.getElementById('output');
const treeBox = document.getElementById('tree-box');

/* Parses indented tree-text lines into a nested {label, children} object.
   Depth of each line = (leading spaces) / 4.  Children are consecutive
   lines at depth + 1 under a node. */
function parseTree(lines) {
  var items = [];
  for (var i = 0; i < lines.length; i++) {
    if (lines[i].trim() === '') continue;
    var sp = 0;
    while (sp < lines[i].length && lines[i][sp] === ' ') sp++;
    items.push({ depth: sp / 4, label: lines[i].trim() });
  }
  if (items.length === 0) return null;
  var pos = 0;
  function build(depth) {
    if (pos >= items.length || items[pos].depth !== depth) return null;
    var node = { label: items[pos].label, children: [] };
    pos++;
    while (pos < items.length && items[pos].depth === depth + 1) {
      var child = build(depth + 1);
      if (child) node.children.push(child);
    }
    return node;
  }
  return build(0);
}

/* Creates an SVG tree diagram from a {label, children} root object.
   Leaves are placed left-to-right with equal spacing; each parent is
   centred above its children.  Edges go from parent bottom to child top. */
function drawTree(root) {
  var HGAP = 72, VGAP = 76, PAD = 36, BH = 34;

  /* --- layout pass --- */
  var leaf = 0;
  function layout(n, d) {
    n.y = d * VGAP + PAD;
    if (n.children.length === 0) {
      n.x = leaf * HGAP + PAD; leaf++;
    } else {
      for (var i = 0; i < n.children.length; i++) layout(n.children[i], d + 1);
      var s = 0;
      for (var i = 0; i < n.children.length; i++) s += n.children[i].x;
      n.x = s / n.children.length;
    }
  }
  layout(root, 0);

  /* --- bounding box --- */
  var mx = 0, my = 0;
  function bounds(n) {
    if (n.x > mx) mx = n.x;
    if (n.y > my) my = n.y;
    for (var i = 0; i < n.children.length; i++) bounds(n.children[i]);
  }
  bounds(root);
  var W = mx + PAD, H = my + PAD;

  /* --- node colour --- */
  function col(lb) {
    if (/^[\d.]/.test(lb)) return '#22c55e';
    if ('+-*/'.indexOf(lb) >= 0 || lb === 'neg') return '#3b82f6';
    return '#f97316';
  }

  var o = '<svg width="' + W + '" height="' + H
        + '" xmlns="http://www.w3.org/2000/svg">';

  /* shadow filter */
  o += '<defs><filter id="ns"><feDropShadow dx="0" dy="2" stdDeviation="3"'
     + ' flood-color="#000" flood-opacity="0.4"/></filter></defs>';

  /* edges (drawn first so they sit behind nodes) */
  function edges(n) {
    for (var i = 0; i < n.children.length; i++) {
      var c = n.children[i];
      o += '<line x1="' + n.x + '" y1="' + (n.y + BH / 2)
         + '" x2="' + c.x + '" y2="' + (c.y - BH / 2)
         + '" stroke="#475569" stroke-width="2"/>';
      edges(c);
    }
  }
  edges(root);

  /* nodes */
  function nodes(n) {
    var fill = col(n.label);
    var tw = Math.max(n.label.length * 10 + 18, 44);
    var rx = BH / 2;
    o += '<rect x="' + (n.x - tw / 2) + '" y="' + (n.y - BH / 2)
       + '" width="' + tw + '" height="' + BH
       + '" rx="' + rx + '" ry="' + rx
       + '" fill="' + fill + '" filter="url(#ns)"/>';
    var fs = n.label.length > 2 ? 13 : 16;
    o += '<text x="' + n.x + '" y="' + (n.y + 5)
       + '" text-anchor="middle" fill="#fff"'
       + ' font-family="\'JetBrains Mono\',Consolas,monospace"'
       + ' font-size="' + fs + '" font-weight="bold">'
       + n.label + '</text>';
    for (var i = 0; i < n.children.length; i++) nodes(n.children[i]);
  }
  nodes(root);

  o += '</svg>';
  return o;
}

async function analyze() {
  var expr = input.value.trim();
  treeBox.innerHTML = '';
  try {
    var res = await fetch('/analyze', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ expr })
    });
    var data = await res.json();
    var text = data.output || 'No output received.';
    var lines = text.split('\n');

    /* Separate the four text lines from the tree lines. */
    var treeStart = -1, resultLines = [];
    for (var i = 0; i < lines.length; i++) {
      if (/^Tree\s*:/.test(lines[i])) { treeStart = i; break; }
      resultLines.push(lines[i]);
    }
    output.textContent = resultLines.join('\n').trim();

    /* Parse the indented tree and render the SVG diagram. */
    if (treeStart >= 0) {
      var root = parseTree(lines.slice(treeStart + 1));
      if (root) {
        var h = document.createElement('h3');
        h.className = 'tree-heading';
        h.textContent = 'Syntax Tree';
        treeBox.appendChild(h);
        var box = document.createElement('div');
        box.className = 'tree-svg';
        box.innerHTML = drawTree(root);
        treeBox.appendChild(box);
      }
    }
  } catch (err) {
    output.textContent = 'Error: ' + err.message;
  }
}

btn.addEventListener('click', analyze);
input.addEventListener('keydown', function (e) {
  if (e.key === 'Enter') analyze();
});
