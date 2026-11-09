GAME-MAKER OS / BOOT ART
Reference source: lewismoten/gamemaker-os at 349c129.

Integration
The OS opens /dev/fb0, selects 960x540 RGB565 and then finds the games.
The full-screen splash is for that OS-owned interval before the list.
640x480 versions are provided for the alternate layout. The optional
overlays show the OS badge over the RGB565 bars and gray ramp.
All HDMI art is indexed pixel art. Pack #RRGGBB as
((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3), honoring surface pitch.
At 1080p, use 2x nearest-neighbor for the 960x540 art.
The test card is a standalone optional display check. Its 1 px and 2 px
checker interiors start at (736,326) and (846,326), both 84x50.
Library diagnostics is an OS-owned fallback for an empty or unreadable
library. Failures before OS launch remain with sandbox-kernel.

eInk is 250x122 logical landscape for Waveshare 2.13-inch V4.
The status layout is for eInk API page mode. Game-Maker OS draws the
entire panel, including MODE, GAMES, and GAME. In title mode the
kernel draws its own status page. If the OS fails, the kernel reclaims
the panel and restores its UP, USB, and ERR lines.
The 40x40 mark simplifies the badge to the castle and ring. The
16x16 and 32x32 favicon frames are hand-drawn on their native grids.
PBM P4 pixels are MSB first, 1 = black. Strip the header for masks.
Row strides are 12 bytes for 96, 5 for 40, and 32 for 250-wide screens.
Apply the panel rotation and native polarity in the renderer.

File inventory / dimensions / bytes / used colors
hdmi-sandbox-badge-96.png  96x96  1249 bytes
  #000000 #101428 #78B058 #80C8F8 #B87438 #E85048 #E8AC50 #F0D890 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FCF8
eink-mark-96.png  96x96  732 bytes
  #000000 #FFFFFF
eink-mark-96.pbm  96x96  1161 bytes
  #000000 #FFFFFF
eink-mark-40.png  40x40  204 bytes
  #000000 #FFFFFF
eink-mark-40.pbm  40x40  209 bytes
  #000000 #FFFFFF
eink-starting-template-250x122.png  250x122  859 bytes
  #000000 #FFFFFF
eink-starting-template-250x122.pbm  250x122  3915 bytes
  #000000 #FFFFFF
eink-starting-250x122.png  250x122  1037 bytes
  #000000 #FFFFFF
eink-starting-250x122.pbm  250x122  3915 bytes
  #000000 #FFFFFF
eink-status-template-250x122.png  250x122  283 bytes
  #000000 #FFFFFF
eink-status-template-250x122.pbm  250x122  3915 bytes
  #000000 #FFFFFF
eink-status-250x122.png  250x122  508 bytes
  #000000 #FFFFFF
eink-status-250x122.pbm  250x122  3915 bytes
  #000000 #FFFFFF
hdmi-boot-template-960x540.png  960x540  2746 bytes
  #000000 #101428 #182C78 #7898F8 #78B058 #80C8F8 #B87438 #E85048 #E8AC50 #F0D890 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FCF8
hdmi-boot-960x540.png  960x540  3014 bytes
  #000000 #101428 #182C78 #7898F8 #78B058 #80C8F8 #A0ACD0 #B87438 #E85048 #E8AC50 #F0D890 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FCF8
hdmi-boot-template-640x480.png  640x480  2516 bytes
  #000000 #101428 #182C78 #7898F8 #78B058 #80C8F8 #B87438 #E85048 #E8AC50 #F0D890 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FCF8
hdmi-boot-640x480.png  640x480  2777 bytes
  #000000 #101428 #182C78 #7898F8 #78B058 #80C8F8 #A0ACD0 #B87438 #E85048 #E8AC50 #F0D890 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FCF8
hdmi-overlay-960x540.png  960x540  3725 bytes
  #000000 #0000F8 #00FC00 #00FCF8 #101010 #101428 #202020 #303430 #404440 #505450 #606460 #707470 #78B058 #80C8F8 #888888 #989898 #A8A8A8 #B87438 #B8B8B8 #C8C8C8 #D8DCD8 #E85048 #E8AC50 #E8ECE8 #F0D890 #F80000 #F800F8 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FC00 #F8FCF8
hdmi-overlay-640x480.png  640x480  2806 bytes
  #000000 #0000F8 #00FC00 #00FCF8 #101010 #101428 #202020 #303430 #404440 #505450 #606460 #707470 #78B058 #80C8F8 #888888 #989898 #A8A8A8 #B87438 #B8B8B8 #C8C8C8 #D8DCD8 #E85048 #E8AC50 #E8ECE8 #F0D890 #F80000 #F800F8 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FC00 #F8FCF8
hdmi-library-diagnostics-960x540.png  960x540  2576 bytes
  #000000 #101428 #182C78 #7898F8 #78B058 #80C8F8 #B87438 #E85048 #E8AC50 #F0D890 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FCF8
hdmi-test-card-960x540.png  960x540  4009 bytes
  #000000 #0000F8 #00FC00 #00FCF8 #101010 #101428 #202020 #303430 #404440 #505450 #606460 #707470 #78B058 #80C8F8 #888888 #989898 #A8A8A8 #B87438 #B8B8B8 #C8C8C8 #D8DCD8 #E85048 #E8AC50 #E8ECE8 #F0D890 #F80000 #F800F8 #F86C58 #F8A470 #F8CC68 #F8D450 #F8FC00 #F8FCF8
README.txt  UTF-8 integration notes.
LICENSE.txt  Game-Maker OS MIT license, no pixel dimensions or palette.

Text rectangles / x, y, width, height
Top-left origin. Use the project 5x7 font, 2x for runtime text.
eInk uses 12 px advance and a 14 px space for its fixed labels.
For live fields use a fixed 12 px advance, including spaces.
HDMI diagnostics uses 24 px row pitch and #F8FCF8 text.
The boot message is blank in hdmi-boot-template and shown in hdmi-boot.
Use muted #A0ACD0 for the boot message, centered in its box.
eInk template variants leave even the fixed labels blank.

eInk starting
  GAME-MAKER               111,  23, 120,  14  fixed
  OS                       111,  41,  24,  14  fixed
  STARTING...              111,  77, 132,  14  fixed

eInk status
  GAME-MAKER OS             54,   6, 158,  14  fixed
  MODE:                      4,  48,  60,  14  fixed
  GAMES:                     4,  70,  72,  14  fixed
  GAME:                      4,  92,  60,  14  fixed
  Build ID                  54,  24, 144,  14  12 chars
  Mode                      82,  48, 168,  14  14 chars
  Game count                82,  70, 168,  14  14 chars
  Current game              82,  92, 168,  14  14 chars

HDMI boot 960x540
  GAME-MAKER OS            287, 272, 390,  35  fixed
  SANDBOX EDITION          391, 328, 180,  14  fixed
  Boot message             312, 375, 336,  14  28 chars

HDMI boot 640x480
  GAME-MAKER OS            127, 244, 390,  35  fixed
  SANDBOX EDITION          231, 300, 180,  14  fixed
  Boot message             152, 347, 336,  14  28 chars

HDMI overlay 960x540
  GAME-MAKER OS            376, 328, 234,  21  fixed
  SANDBOX EDITION          376, 372, 180,  14  fixed

HDMI overlay 640x480
  GAME-MAKER OS            216, 288, 234,  21  fixed
  SANDBOX EDITION          216, 332, 180,  14  fixed

HDMI library diagnostics
  GAME-MAKER OS            190,  53, 312,  28  fixed
  LIBRARY DIAGNOSTICS      191,  96, 228,  14  fixed
  Live message area        120, 151, 720, 288  60 cols x 12 rows

HDMI test card
  GAME-MAKER OS            139, 329, 312,  28  fixed
  FRAMEBUFFER TEST         140, 376, 192,  14  fixed
  1 PX                     754, 392,  48,  14  fixed
  2 PX                     864, 392,  48,  14  fixed

SPDX-License-Identifier: MIT
The Sandbox Edition badge is the project’s original MIT drawing from
sandbox-kernel scripts/make_logo.py. The color treatment and layouts
are original. Fixed labels use the
original 5x7 font shared by Game-Maker OS and sandbox-kernel (MIT).
The boot assets use no third-party logos or fonts.
The social preview uses the repo’s docs/games-list.png capture at 1:1.
Companion outputs: 1280x640 social PNG, ICO at 16/32/48/64/128/256.
