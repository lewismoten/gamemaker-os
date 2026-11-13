// Plays /sd/GAMES/G for 12 s of (virtual) time with scripted Enter, arrow and Space presses, then stops. Used by tests/js/profile_snd.sh.
const g = document.createElement('game');
g.style.position = 'absolute'; g.style.left = 0; g.style.top = 0; g.style.width = 320; g.style.height = 200;
document.body.appendChild(g);
g.src = '/sd/GAMES/G';
g.addEventListener('ended', () => { console.log('APP ended ticks ' + g.ticks); sys.exit(0); });
g.play();
const keys = [28, 28, 28, 106, 105, 57, 28, 103, 108];
let i = 0;
setInterval(() => { const k = keys[i++ % keys.length]; g.press(k, 1); setTimeout(() => g.press(k, 0), 60); }, 700);
setTimeout(() => { g.press(1, 1); }, 12000); // Esc: the game ends itself
