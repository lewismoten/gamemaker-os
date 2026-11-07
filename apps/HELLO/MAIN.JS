// A tiny shell for sandbox-kernel, written like a small web page: the layout
// is INDEX.HTM, and this script fills it in. Arrow keys move between buttons;
// Enter clicks.
const here = '/sd/OS/APPS/HELLO';
document.body.style.background = '#0b1020';
document.body.innerHTML = fs.readFile(here + '/INDEX.HTM');
const page = document.getElementById('page');

function about() {
  page.innerHTML =
    '<text style="color:#ffffff">This page is JavaScript on a Raspberry Pi Zero W.</text>' +
    '<text style="color:#8a96b8">Edit /OS/APPS/HELLO/MAIN.JS and INDEX.HTM to make it yours.</text>';
}

function games() {
  page.innerHTML = '<div id="grid" style="display:flex; flex-direction:row; gap:8px"></div>';
  const grid = document.getElementById('grid');
  let names = [];
  try {
    names = fs.readdir('/sd/GAMES').slice(0, 4);
  } catch (e) {
    page.textContent = 'No /GAMES folder on the card.';
    return;
  }
  for (const name of names) {
    const card = document.createElement('button');
    card.style.width = 144;
    const pic = document.createElement('img');
    pic.src = '/sd/GAMES/' + name;
    pic.style.width = 132;
    pic.style.height = 82;
    const label = document.createElement('text');
    label.textContent = name;
    card.appendChild(pic);
    card.appendChild(label);
    card.addEventListener('click', () => play(name));
    grid.appendChild(card);
  }
  grid.firstChild && grid.firstChild.focus();
}

function play(name) {
  const game = document.createElement('game');
  game.style.position = 'absolute';
  game.src = '/sd/GAMES/' + name;
  game.addEventListener('ended', () => {
    game.remove();
    games();
  });
  document.body.appendChild(game);
  game.play();
}

document.getElementById('games').addEventListener('click', games);
document.getElementById('about').addEventListener('click', about);
document.getElementById('games').focus();
about();

const clock = document.getElementById('clock');
setInterval(() => {
  const s = Math.floor(performance.now() / 1000);
  clock.textContent = 'up ' + Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0');
}, 1000);
console.log('hello OS ready');
