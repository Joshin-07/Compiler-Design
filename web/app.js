const input = document.getElementById('expr-input');
const btn = document.getElementById('analyze-btn');
const output = document.getElementById('output');

async function analyze() {
  const expr = input.value.trim();
  try {
    const res = await fetch('/analyze', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ expr })
    });
    const data = await res.json();
    output.textContent = data.output || 'No output received.';
  } catch (err) {
    output.textContent = 'Error: ' + err.message;
  }
}

btn.addEventListener('click', analyze);
input.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') analyze();
});
