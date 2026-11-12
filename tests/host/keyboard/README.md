These MIT-licensed files are unchanged test-only snapshots from sandbox-kernel
32fd0d26005b147d79da787d120976e79399eeea (the commit in sdk/PINNED):
core/hidboot.c, core/usbkbd.c, include/{hidboot,usbkbd,input,usblog}.h.
Copyright (c) 2026 Lewis Moten; see the repository LICENSE.

They let test_music.py drive the pinned kernel's real boot-report decoder and
multi-keyboard union into GMOS's production event reader and handlers without
USB hardware or a sibling checkout. They are never linked into GMOS.ELF.
The report fixtures model a receiver with a report ID and an ordinary 8-byte
boot keyboard, including simultaneous held M and hot removal. They are synthetic,
not captured reports from the living-room hardware.
