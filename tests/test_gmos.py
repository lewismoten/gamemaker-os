# SPDX-License-Identifier: MIT
"""Host tests for the pure parts of GMOS.ELF: the .GAM title reader, the games
list and its input handling, and the 640x480 layout, rendered to PNG files in
build/host/ for a person to look at. GMOS_GAMES may name a folder of game
folders (each with a .GAM) to title real games too."""
import ctypes as C
import os, pathlib, struct, subprocess, sys, unittest, zlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'build/host'
W,H=640,480

class Game(C.Structure): _fields_=[('dir',C.c_char*9),('title',C.c_char*21)]
class Menu(C.Structure): _fields_=[('games',Game*128),('count',C.c_uint),('sel',C.c_uint),('top',C.c_uint),('message',C.c_char*49)]
NONE,MOVED,CHOSEN,QUIT,RESYNC=range(5)
UP,DOWN,ENTER,ESC,Q,SPACE=103,108,28,1,16,57

def lib():
    subprocess.run(['make','-s','-C',str(ROOT),'host'],check=True)
    c=C.CDLL(str(OUT/'libgmos.so'))
    c.gm_gam_title.argtypes=[C.c_char_p,C.c_uint32,C.c_char_p]
    c.gm_add.argtypes=[C.POINTER(Menu),C.c_char_p,C.c_char_p];c.gm_add.restype=C.c_bool
    c.gm_sort.argtypes=[C.POINTER(Menu)];c.gm_event.argtypes=[C.POINTER(Menu),C.c_uint16,C.c_uint16,C.c_int32]
    c.gm_draw.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Menu)]
    return c
C_LIB=None
def c():
    global C_LIB
    if C_LIB is None: C_LIB=lib()
    return C_LIB

def gam3(desc,scenes=3):
    d=bytearray(b'GM'+struct.pack('<h',300)+bytes([0,8])+b'RSD'*84)[:256]
    d+=struct.pack('<3H',scenes,20,20)
    scene=bytearray(283);scene[0:2]=struct.pack('<H',1);scene[23:23+len(desc)]=desc
    return bytes(d+scene*max(scenes,1)+b'\x01\x00\x00')
def gam_old(desc):
    rec=bytearray(330);rec[6:6+len(desc)]=desc;return bytes(rec*20)
def title(data):
    out=C.create_string_buffer(21);n=c().gm_gam_title(data,len(data),out);return n,out.value.decode('ascii')

def png(path,px):
    raw=b''.join(b'\0'+b''.join(struct.pack('>I',p)[1:] for p in px[y*W:(y+1)*W]) for y in range(H))
    def chunk(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',W,H,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b''))

def render(m):
    buf=(C.c_uint32*(W*H))();c().gm_draw(buf,W,C.byref(m));return list(buf)

def menu(*games):
    m=Menu()
    for d,t in games: c().gm_add(C.byref(m),d.encode(),t.encode())
    c().gm_sort(C.byref(m));return m

class Titles(unittest.TestCase):
    def test_version_3(self):
        self.assertEqual(title(gam3(b'ROLL THE DICE')),(13,'ROLL THE DICE'))
        self.assertEqual(title(gam3(b"Kit's Shop")),(10,"Kit's Shop"))
        self.assertEqual(title(gam3(b'  \xc4\xc4 BOXED \xc4\xc4  ')),(5,'BOXED'))         # frame characters around it are trimmed
        self.assertEqual(title(gam3(b'Caf\x82 \x01'))[1],'Cafe ?')
        self.assertEqual(title(gam3(b'A'*20)),(20,'A'*20))                               # all 20 bytes, no NUL
        self.assertEqual(title(gam3(b'')),(0,''))                                        # empty: the folder name is shown
    def test_inner_frame_is_a_space(self):
        self.assertEqual(title(gam3(b'Caf\x82 \xb3 Two'))[1],'Cafe   Two')
    def test_old_format(self):
        self.assertEqual(title(gam_old(b'OLD GAME')),(8,'OLD GAME'))
        self.assertEqual(title(gam_old(b'B'*15+b'tail')),(15,'B'*15))                     # 15 bytes in old files
    def test_not_a_game(self):
        for data in (b'',b'GM',gam3(b'X')[:304],gam3(b'X',scenes=0),b'x'*329):
            with self.subTest(len(data)): self.assertEqual(title(data)[0],-1)
    def test_real_games(self):
        folder=os.environ.get('GMOS_GAMES')
        if not folder: self.skipTest('set GMOS_GAMES to a folder of game folders')
        for g in sorted(pathlib.Path(folder).iterdir()):
            for f in g.glob('*.[gG][aA][mM]'):
                n,t=title(f.read_bytes()[:512]);self.assertGreaterEqual(n,0,f);print(f'{g.name}: {t}')

class Lists(unittest.TestCase):
    def test_sort_and_fallback(self):
        m=menu(('ZED','zebra'),('APPLE','Apple'),('EMPTY',''),('B2','banana'),('B1','banana'))
        self.assertEqual([(g.dir.decode(),g.title.decode()) for g in m.games[:m.count]],
                         [('APPLE','Apple'),('B1','banana'),('B2','banana'),('EMPTY','EMPTY'),('ZED','zebra')])
    def test_moves_and_scroll(self):
        m=menu(*[(f'G{i:02d}',f'Game {i:02d}') for i in range(12)]);ev=lambda code,v=1,t=1: c().gm_event(C.byref(m),t,code,v)
        self.assertEqual(ev(UP),NONE);self.assertEqual(m.sel,0)                         # top stays at the top
        for i in range(8): self.assertEqual(ev(DOWN),MOVED)
        self.assertEqual((m.sel,m.top),(8,0));ev(DOWN);self.assertEqual((m.sel,m.top),(9,1))   # nine rows show
        self.assertEqual(ev(DOWN,2),MOVED);self.assertEqual(ev(DOWN,0),NONE)            # repeats move, releases do not
        ev(DOWN);self.assertEqual(ev(DOWN),NONE);self.assertEqual((m.sel,m.top),(11,3))
        for i in range(11): ev(UP)
        self.assertEqual((m.sel,m.top),(0,0))
        self.assertEqual(ev(ENTER),CHOSEN);self.assertEqual(ev(ENTER,2),NONE)            # no repeat for Enter
        self.assertEqual(ev(ESC),QUIT);self.assertEqual(ev(Q),QUIT);self.assertEqual(ev(SPACE),NONE)
        self.assertEqual(ev(3,0,0),RESYNC);self.assertEqual(ev(0,0,0),NONE)              # SYN_DROPPED, SYN_REPORT
        self.assertEqual(ev(0,100,3),NONE)                                              # axes do nothing yet
    def test_empty_list(self):
        m=menu();self.assertEqual(c().gm_event(C.byref(m),1,ENTER,1),NONE);self.assertEqual(c().gm_event(C.byref(m),1,DOWN,1),NONE)
    def test_full_list(self):
        m=Menu();self.assertTrue(all(c().gm_add(C.byref(m),b'D',b'T') for _ in range(128)));self.assertFalse(c().gm_add(C.byref(m),b'D',b'T'))

def rows_with(px,color,x0=0,x1=W):
    return [y for y in range(H) if any(px[y*W+x]==color for x in range(x0,x1))]

class Layout(unittest.TestCase):
    @classmethod
    def setUpClass(cls): OUT.mkdir(parents=True,exist_ok=True)
    def test_games_list(self):
        m=menu(('DICE','ROLL THE DICE'),('KITSHOP',"KIT'S SHOP"),('SNDBOARD','SOUND BOARD'))
        c().gm_event(C.byref(m),1,DOWN,1);px=render(m);png(OUT/'games-list.png',px)
        self.assertEqual(px[10*W+10],0x1f3a93);self.assertEqual(px[65*W+10],0xffcc33)   # title bar, accent line
        sel=rows_with(px,0xffcc33,24,30);self.assertEqual((min(y for y in sel if y>70),max(y for y in sel if y<430)),(84+38,84+38+33))
        title=rows_with(px,0xf2f4ff,150,500);self.assertTrue(any(18<=y<46 for y in title))   # "GAME-MAKER OS" drawn
        dark=rows_with(px,0x0b1a3c,40,300);self.assertTrue(any(84+38<y<84+76 for y in dark))  # dark text on the highlight
        self.assertEqual(px[470*W+600],0x142a66)
    def test_scrolled_and_chosen(self):
        m=menu(*[(f'G{i:02d}',f'Game number {i}') for i in range(14)])
        for _ in range(11): c().gm_event(C.byref(m),1,DOWN,1)
        m.message=b'Chose Game number 19'[:20];px=render(m);png(OUT/'games-list-scrolled.png',px)
        self.assertEqual((m.sel,m.top),(11,3))
    def test_empty(self):
        px=render(menu());png(OUT/'games-list-empty.png',px)
        self.assertTrue(rows_with(px,0xf2f4ff,100,540))

class Build(unittest.TestCase):
    def test_font_source_is_current(self):
        self.assertEqual(subprocess.run([sys.executable,str(ROOT/'tools/make_font.py'),'--check']).returncode,0)
    def test_arm_program(self):
        cc=os.environ.get('TOOLCHAIN','/Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/arm-none-eabi-')+'gcc'
        if not pathlib.Path(cc).exists(): self.skipTest('no Arm GNU toolchain')
        subprocess.run(['make','-s','-C',str(ROOT),'all'],check=True)
        d=(ROOT/'build/GMOS.ELF').read_bytes()
        self.assertEqual(d[:7],b'\x7fELF\x01\x01\x01');self.assertEqual(struct.unpack_from('<HH',d,16),(2,40))   # ELF32 LE, ET_EXEC, ARM
        self.assertEqual(struct.unpack_from('<I',d,24)[0],0x08000000)

if __name__=='__main__': unittest.main()
