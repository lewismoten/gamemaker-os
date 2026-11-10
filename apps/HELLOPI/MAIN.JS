// Hello, Pi: a first JS.ELF app for OS authors. No browser or game files needed.
// Copy this folder to /OS/APPS/HELLOPI on the card. All state lives in memory:
// JS.ELF offers read-only fs.readFile/readdir, not file writes or browser fetch.
const body = document.body;
body.style.background = '#0b1020';
body.style.padding = 16;
body.style.gap = 12;

// Keep references to attached elements for this runtime's garbage collector.
const title = document.createElement('text');
title.textContent = 'Hello, Pi';
title.style.fontSize = 27;
title.style.color = '#ffffff';
body.appendChild(title);
const help = document.createElement('text');
help.textContent = 'Arrows: move  Enter: screen mode  Esc: exit';
help.style.color = '#c2c3c7';
body.appendChild(help);
const status = document.createElement('text');
status.style.color = '#ffcc33';
body.appendChild(status);

// A small canvas bounds memory use; this is a CSS/API subset, not HTML5.
const canvas = document.createElement('canvas');
canvas.width = 300;
canvas.height = 150;
// Match the layout box to the backing pixels instead of stretching to the row.
canvas.style.width = 300;
canvas.style.height = 150;
body.appendChild(canvas);
const ctx = canvas.getContext('2d');

let wide = false;
function setMode(nextWide) {
  // Both modes fit the runtime's fixed screen buffer. 960x540x16 scales
  // exactly 2x to 1080p. Catch refusal so older kernels can still show the app.
  try {
    screen.setMode(nextWide ? 960 : 640, nextWide ? 540 : 480, nextWide ? 16 : 32);
    wide = nextWide;
    status.textContent = wide ? 'Mode: 960x540, 16 bits' : 'Mode: 640x480, 32 bits';
  } catch (e) {
    status.textContent = 'Mode unavailable; keeping the current screen.';
    console.log('Hello, Pi mode refused: ' + e);
  }
}
setMode(false);

let x = 140, y = 65;
document.addEventListener('keydown', (event) => {
  // Consume the keys we handle so arrows do not also move native focus.
  if (event.key === 'Escape') {
    event.preventDefault();
    console.log('Hello, Pi exit');
    sys.exit(0);
    return; // Exit is cooperative: always return from the current callback.
  }
  if (event.key === 'Enter') {
    event.preventDefault();
    if (!event.repeat) setMode(!wide);
  } else if (event.key === 'ArrowLeft' || event.key === 'ArrowRight' ||
             event.key === 'ArrowUp' || event.key === 'ArrowDown') {
    event.preventDefault();
    // Repeated arrows keep moving, but the square stays inside the canvas.
    if (event.key === 'ArrowLeft') x = Math.max(0, x - 10);
    if (event.key === 'ArrowRight') x = Math.min(280, x + 10);
    if (event.key === 'ArrowUp') y = Math.max(0, y - 10);
    if (event.key === 'ArrowDown') y = Math.min(130, y + 10);
  }
});

function draw(now) {
  ctx.fillStyle = '#1d2b53';
  ctx.fillRect(0, 0, 300, 150);
  ctx.fillStyle = '#ffcc33';
  ctx.fillRect(x, y, 20, 20);
  // A second square sweeps across the bottom even without input. Use elapsed
  // time, not a frame counter, so slow frames do not slow the animation.
  ctx.fillStyle = '#29adff';
  ctx.fillRect(Math.floor(now / 10) % 290, 140, 10, 10);
  requestAnimationFrame(draw); // One callback per frame; no busy loop.
}
requestAnimationFrame(draw);
console.log('Hello, Pi ready');
