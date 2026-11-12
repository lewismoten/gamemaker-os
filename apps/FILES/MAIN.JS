// SPDX-License-Identifier: MIT
// FILES: read-only card browser. This small DOM API is not a browser engine.
screen.setMode(960, 540, 16);
document.body.style.background = '#101828';

// Keep explicit JS references: this runtime can collect otherwise unreferenced
// attached labels under heap pressure. Release old page references on rebuild.
const rootNodes = [];
let pageNodes = [];
function box(tag, id, x, y, width, height, text, fontSize = 18, parent = document.body) {
  const el = document.createElement(tag);
  el.id = id;
  Object.assign(el.style, { position: 'absolute', left: x, top: y,
    width, height, fontSize, color: '#ffffff' });
  el.textContent = text;
  parent.appendChild(el);
  (parent === document.body ? rootNodes : pageNodes).push(el);
  return el;
}
box('text', 'title', 32, 24, 896, 45, 'FILES', 36);
const pathLabel = box('text', 'path', 32, 80, 896, 27, '/sd');
box('text', 'size-gap', 32, 116, 896, 27, 'File sizes unavailable: no stat API.');
const content = box('div', 'content', 32, 160, 896, 280, '');
const status = box('text', 'status', 32, 448, 896, 27, '');
box('text', 'controls', 32, 494, 896, 27, 'Arrows: focus/page   Enter/Space: open   Esc: up / exit at root');

let folder = '/sd';
let names = [];
let selected = 0;
const history = [];
let textPath = '';
let lines = [];
let textPage = 0;
const CARDS = 6;
const ROWS = 12;

// Never let a card name become '..' or inject another path component.
// The binding has a 96-byte path buffer. Restrict paths to <=95 ASCII bytes
// rather than allowing silent native truncation or miscounting UTF-8 bytes.
function safePath(path) {
  return path.length <= 95 && /^[\x20-\x7e]+$/.test(path);
}
function displayName(name, limit) {
  const ascii = name.replace(/[^\x20-\x7e]/g, '?');
  return ascii.length > limit ? ascii.slice(0, limit - 3) + '...' : ascii;
}
function directory(path) {
  return fs.readdir(path).filter(name => name !== '.' && name !== '..' &&
    !name.includes('/') && !name.includes('\\')).sort();
}
function showFolder() {
  textPath = '';
  pathLabel.textContent = displayName(folder, 72);
  content.innerHTML = '';
  pageNodes = [];
  const start = Math.floor(selected / CARDS) * CARDS;
  names.slice(start, start + CARDS).forEach((name, slot) => {
    const i = start + slot;
    const path = folder + '/' + name;
    let kind = /\.txt$/i.test(name) ? 'TXT / type unconfirmed' : 'Type unconfirmed';
    if (safePath(path)) {
      try { fs.readdir(path); kind = 'Folder (readable)'; } catch (error) {
        // A failed directory probe could also mean unreadable or missing.
      }
    }
    const button = box('button', 'entry-' + i, (slot % 3) * 300,
      Math.floor(slot / 3) * 140, 280, 116, '', 18, content);
    button.style.background = '#1d2939';
    button.style.outlineColor = '#eab44d';
    box('text', '', 12, 14, 256, 27, displayName(name, 21), 18, button);
    box('text', '', 12, 50, 256, 18, kind, 9, button);
    box('text', '', 12, 78, 256, 18, 'Size unavailable', 9, button);
    button.addEventListener('focus', () => { selected = i; });
    button.addEventListener('click', () => openEntry(i));
  });
  status.textContent = names.length ? 'Items ' + (start + 1) + '-' +
    Math.min(start + CARDS, names.length) + ' of ' + names.length + ' (read-only)' : 'Empty folder. Esc: parent.';
  if (names.length) document.getElementById('entry-' + selected).focus();
  else { content.tabIndex = 0; content.focus(); }
}
function showText() {
  pathLabel.textContent = displayName(textPath, 72);
  content.innerHTML = '';
  pageNodes = [];
  // Bitmap text is ASCII and single-line: wrap and paginate in the app.
  for (let row = 0; row < ROWS; row++) {
    box('text', 'line-' + row, 8, row * 23, 880, 22,
      lines[textPage * ROWS + row] || '', 18, content);
  }
  status.textContent = 'Page ' + (textPage + 1) + '/' + Math.max(1, Math.ceil(lines.length / ROWS)) +
    ' | Read cap 256 KiB; preview first 16000 chars.';
  content.tabIndex = 0;
  content.focus();
}
function openEntry(i) {
  const path = folder + '/' + names[i];
  if (!safePath(path)) {
    status.textContent = 'Cannot open: path must be ASCII and at most 95 bytes.';
    return;
  }
  try {
    const children = directory(path);
    history.push({ folder, names, selected: i });
    folder = path;
    names = children;
    selected = 0;
    showFolder();
    return;
  } catch (error) {
    // No stat API. Try TXT only after trying a readable directory.
  }
  if (!/\.txt$/i.test(names[i])) {
    status.textContent = 'Not a readable folder. Only .TXT previews are supported.';
    return;
  }
  try {
    const text = fs.readFile(path).slice(0, 16000).replace(/\r\n?/g, '\n')
      .replace(/\t/g, '    ').replace(/[^\x20-\x7e\n]/g, '?');
    lines = [];
    for (const line of text.split('\n')) {
      if (!line.length) lines.push('');
      for (let offset = 0; offset < line.length; offset += 72) {
        lines.push(line.slice(offset, offset + 72));
      }
    }
    textPath = path;
    textPage = 0;
    showText();
  } catch (error) {
    status.textContent = 'Cannot read TXT: missing, unreadable or read error.';
  }
}
function goBack() {
  if (textPath) { lines = []; showFolder(); return; }
  if (!history.length) { sys.exit(0); return; }
  const previous = history.pop();
  folder = previous.folder;
  names = previous.names;
  selected = previous.selected;
  showFolder();
}
document.addEventListener('keydown', event => {
  if (event.code === 'KeyQ' && !event.repeat) { sys.exit(0); return; }
  if (event.key === 'Escape' || event.key === 'Backspace') {
    event.preventDefault();
    if (!event.repeat) goBack();
    return;
  }
  const moves = { ArrowLeft: -1, ArrowRight: 1, ArrowUp: -3, ArrowDown: 3 };
  if (!(event.key in moves)) return;
  event.preventDefault(); // Avoid a second move from the native spatial default.
  if (textPath) {
    const delta = moves[event.key] < 0 ? -1 : 1;
    textPage = Math.max(0, Math.min(Math.ceil(lines.length / ROWS) - 1, textPage + delta));
    showText();
  } else if (names.length) {
    selected = Math.max(0, Math.min(names.length - 1, selected + moves[event.key]));
    showFolder();
  }
});
try { names = directory(folder); showFolder(); } catch (error) {
  status.textContent = 'Cannot list card root /sd.';
}
console.log('FILES ready');
