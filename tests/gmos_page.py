# SPDX-License-Identifier: MIT
"""The games page on the host, as GMOS.ELF builds it on the board: game folders
(each with a .GAM), the library index lines, input events, title screens read
from each game's files, and the 640x480 frame. Used by test_gmos.py and
tools/render_list.py."""
import ctypes as C
import pathlib, struct, subprocess, zlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/host'
W, H = 640, 480
TW, TH = 194, 121
NONE, MOVED, CHOSEN, QUIT, RESYNC = range(5)
KEYS = dict(esc=1, backspace=14, tab=15, q=16, lbracket=26, rbracket=27, enter=28, lshift=42, space=57,
            home=102, up=103, pgup=104, left=105, right=106, end=107, down=108, pgdn=109)
STARTER, SANDBOX, BIZARRE, CD, OTHER = range(5)
GRID, DETAILS = range(2)
_lib = None

def lib():
    global _lib
    if _lib is None:
        subprocess.run(['make', '-s', '-C', str(ROOT), 'host'], check=True)
        c = C.CDLL(str(OUT / 'libgmos.so'))
        c.gm_gam_title.argtypes = [C.c_char_p, C.c_uint32, C.c_char_p]
        c.gm_menu_bytes.restype = C.c_uint
        c.gm_add.argtypes = [C.c_void_p, C.c_char_p, C.c_char_p, C.c_char_p, C.c_uint32]; c.gm_add.restype = C.c_bool
        c.gm_library_line.argtypes = [C.c_void_p, C.c_char_p, C.c_uint32]
        c.gm_finish.argtypes = [C.c_void_p]
        c.gm_event.argtypes = [C.c_void_p, C.c_uint16, C.c_uint16, C.c_int32]
        c.gm_tab_size.argtypes = [C.c_void_p, C.c_uint]; c.gm_tab_size.restype = C.c_uint
        c.gm_draw.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p, C.c_void_p]
        c.gm_card_rect.argtypes = [C.c_uint] + [C.POINTER(C.c_uint32)] * 4
        c.gm_title_picture.argtypes = [C.c_void_p, C.c_void_p, C.c_char_p, C.c_void_p, C.c_void_p]
        c.gm_thumb_put.argtypes = [C.c_uint, C.c_void_p, C.c_void_p]
        c.gm_thumb_get.argtypes = [C.c_uint]; c.gm_thumb_get.restype = C.c_void_p
        c.gmos_menu_state.argtypes = [C.c_void_p, C.c_int]
        c.gmos_menu_message.argtypes = [C.c_void_p, C.c_char_p]
        c.gmos_tab_game.argtypes = [C.c_void_p, C.c_uint, C.c_uint]
        c.gmos_menu_pic.argtypes = [C.c_void_p, C.c_uint, C.c_int]
        c.gm_crc32.argtypes = [C.c_uint32, C.c_char_p, C.c_uint32]; c.gm_crc32.restype = C.c_uint32
        _lib = c
    return _lib

def gam_of(folder):
    return next((f for f in sorted(pathlib.Path(folder).iterdir()) if f.suffix.upper() == '.GAM'), None)

class Page:
    """A games page: add game folders (or bare titles), index lines, then finish()."""
    def __init__(self):
        self.c = lib(); self.c.gm_thumb_clear()
        self.m = C.create_string_buffer(self.c.gm_menu_bytes())
        self.paths = []
    def add_folder(self, folder, dir_name=None):
        folder = pathlib.Path(folder); gam = gam_of(folder)
        if not gam: return False
        data = gam.read_bytes()[:65536]; t = C.create_string_buffer(21)
        if self.c.gm_gam_title(data[:512], len(data[:512]), t) < 0: return False
        self.paths.append(folder)
        return self.c.gm_add(self.m, (dir_name or folder.name).upper()[:8].encode(), gam.name.upper().encode(), t.value, zlib.crc32(data))
    def add(self, dir_name, title, gam='', crc=0):
        self.paths.append(None)
        return self.c.gm_add(self.m, dir_name.encode(), (gam or dir_name + '.GAM').encode(), title.encode(), crc)
    def index(self, text):
        return sum(self.c.gm_library_line(self.m, l, len(l)) for l in text.encode().split(b'\n'))
    def finish(self): self.c.gm_finish(self.m); return self
    def key(self, name, value=1):
        code = KEYS[name] if isinstance(name, str) else name
        return self.c.gm_event(self.m, 1, code, value)
    def keys(self, *names): return [self.key(n) for n in names]
    def state(self, what): return self.c.gmos_menu_state(self.m, what)
    tab = property(lambda s: s.state(0)); pos = property(lambda s: s.state(1)); view = property(lambda s: s.state(2))
    button = property(lambda s: s.state(3)); selected = property(lambda s: s.state(4))
    def message(self, text): self.c.gmos_menu_message(self.m, text.encode())
    def title_picture(self, i):
        """Game i's title screen (320x200 indices, 6-bit palette) read from its folder, as the board reads it; None if it has none."""
        if self.paths[i] is None: return None
        px, pal = (C.c_uint8 * 64000)(), (C.c_uint8 * 768)()
        gam = gam_of(self.paths[i]).name.encode()
        if self.c.gm_title_picture(C.cast(self.c.gmos_host_read, C.c_void_p), C.c_char_p(str(self.paths[i]).encode()), gam, px, pal): return None
        return px, pal
    def load_page(self):
        """What the board does between key presses: every card on the page gets its title screen."""
        for i in self.page_games():
            pic = self.title_picture(i)
            self.c.gmos_menu_pic(self.m, i, 1 if pic else -1)
            if pic: self.c.gm_thumb_put(i, *pic)
    def page_games(self):
        """The game indexes of the cards on the selected card's page."""
        t = self.tab; first = self.pos // 6 * 6
        return [g for g in (self.c.gmos_tab_game(self.m, t, k) for k in range(first, first + 6)) if g >= 0]
    def render(self, load=True):
        if load and self.view == GRID: self.load_page()
        big = pal = None
        if self.view == DETAILS and self.selected >= 0:
            pic = self.title_picture(self.selected)
            if pic: big, pal = pic
        buf = (C.c_uint32 * (W * H))()
        self.c.gm_draw(buf, W, self.m, big, pal)
        return list(buf)

def crc(px): return zlib.crc32(struct.pack(f'<{W*H}I', *px))

def png(path, px, w=W, h=H):
    raw = b''.join(b'\0' + b''.join(struct.pack('>I', p)[1:] for p in px[y*w:(y+1)*w]) for y in range(h))
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    pathlib.Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
