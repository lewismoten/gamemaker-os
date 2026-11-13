// Starts and stops /sd/GAMES/G 50 times (each 100 ms apart), then logs heap and stats. Used by tests/js/profile_snd.sh.
const g = document.createElement('game');
g.style.position = 'absolute'; g.style.left = 0; g.style.top = 0; g.style.width = 320; g.style.height = 200;
document.body.appendChild(g);
g.src = '/sd/GAMES/G';
let n = 0;
function cycle() {
  if (n === 50) { console.log('APP done ' + n + ' stats ' + JSON.stringify(sys.stats()) + ' mem ' + JSON.stringify(sys.memory())); sys.exit(0); return; }
  g.play(); n++;
  setTimeout(() => { g.stop(); if (n === 1 || n === 25 || n === 50) console.log('APP n ' + n + ' mem ' + JSON.stringify(sys.memory())); setTimeout(cycle, 20); }, 100);
}
cycle();
