// SPDX-License-Identifier: MIT
// SETTINGS: ordinary buttons + JavaScript state, no browser form widgets.
// fs is read-only. These settings intentionally reset when this app restarts.
screen.setMode(960, 540, 16);
document.body.style.background = '#101828';

// Retain labels explicitly; attached-only nodes can be collected by this spike.
const nodes = [];
function box(tag, id, x, y, width, height, text, fontSize = 18) {
  const el = document.createElement(tag);
  el.id = id;
  Object.assign(el.style, { position: 'absolute', left: x, top: y,
    width, height, fontSize, color: '#ffffff' });
  el.textContent = text;
  document.body.appendChild(el);
  nodes.push(el);
  return el;
}
box('text', 'title', 40, 32, 880, 45, 'SETTINGS', 36);
box('text', 'subtitle', 40, 92, 880, 27, 'Small controls, live state, no pretend save.');
const themeButton = box('button', 'theme', 40, 160, 360, 56, 'Theme: Night');
const hintsButton = box('button', 'hints', 40, 240, 360, 56, 'Hints: On');
const resetButton = box('button', 'reset', 40, 320, 360, 56, 'Reset defaults');
for (const button of [themeButton, hintsButton, resetButton]) {
  button.style.background = '#1d2939';
  button.style.outlineColor = '#eab44d';
}
box('text', 'preview-title', 440, 168, 480, 27, 'LIVE PREVIEW');
const preview = box('text', 'preview', 440, 224, 480, 27, 'Night theme / hints on');
const hint = box('text', 'hint', 440, 280, 480, 27, 'Enter or Space changes a setting.');
box('text', 'save-gap', 40, 424, 880, 27, 'Memory only: no file-write API; restart resets settings.');
box('text', 'controls', 40, 488, 880, 27, 'Arrows: focus   Enter/Space: activate   Reset: restore defaults');
// State is app-owned; updating it repaints the controls and the live preview.
const defaults = { paper: false, hints: true };
let state = { ...defaults };
function render() {
  const paper = state.paper;
  document.body.style.background = paper ? '#eee8dc' : '#101828';
  for (const el of document.body.children) {
    el.style.color = paper ? '#182838' : '#ffffff';
  }
  for (const button of [themeButton, hintsButton, resetButton]) {
    button.style.background = paper ? '#ffffff' : '#1d2939';
  }
  themeButton.textContent = 'Theme: ' + (paper ? 'Paper' : 'Night');
  hintsButton.textContent = 'Hints: ' + (state.hints ? 'On' : 'Off');
  preview.textContent = (paper ? 'Paper' : 'Night') + ' theme / hints ' + (state.hints ? 'on' : 'off');
  hint.textContent = state.hints ? 'Enter or Space changes a setting.' : 'Hints are off.';
}
themeButton.addEventListener('click', () => { state.paper = !state.paper; render(); });
hintsButton.addEventListener('click', () => { state.hints = !state.hints; render(); });
resetButton.addEventListener('click', () => { state = { ...defaults }; render(); });
// Native arrow focus and Enter/Space clicks; Q demonstrates the exit API.
document.addEventListener('keydown', event => {
  if (event.code === 'KeyQ' && !event.repeat) sys.exit(0);
});
render();
themeButton.focus();
console.log('SETTINGS ready');
