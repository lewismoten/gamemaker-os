// A game folder as <img src>, beside a second one. Used by tests/js/profile_snd.sh.
document.body.style.background = '#102030';
const im = document.createElement('img');
im.src = '/sd/GAMES/G'; im.style.position = 'absolute'; im.style.left = 20; im.style.top = 20; im.style.width = 320; im.style.height = 200;
document.body.appendChild(im);
const im2 = document.createElement('img');
im2.src = '/sd/GAMES/H'; im2.style.position = 'absolute'; im2.style.left = 360; im2.style.top = 20; im2.style.width = 160; im2.style.height = 100;
document.body.appendChild(im2);
