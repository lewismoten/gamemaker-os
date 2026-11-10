"""A small CMF song built in code (no game data): one FM instrument on channel 0 and a second on channel 1,
two overlapping notes, a velocity change, a rest, then the end marker. Used by test_audio.py."""
def make_cmf():
    inst = bytearray()
    for i in range(16):
        # SBI: modulator/carrier pairs of AM-VIB-EG-KSR-MULT, KSL-TL, AR-DR, SL-RR, wave, then feedback/connection
        r = [0x21, 0x21, 0x10 if i == 0 else 0x1c, 0x00, 0xf3, 0xf3, 0x77 if i == 0 else 0x57, 0x77, 0, 0, 0x06 if i == 0 else 0x01] + [0] * 5
        if i == 1: r[0] = 0x61  # a vibrato on the modulator
        inst += bytes(r)
    music = bytes([0, 0x90, 60, 100, 40, 0x91, 67, 90, 60, 0x80, 60, 0, 30, 0x81, 67, 0, 20, 0x90, 72, 127, 90, 0x80, 72, 0, 10, 0xff, 0x2f, 0])
    head = bytearray(40)
    head[0:4] = b"CTMF"; head[4:6] = (0x0101).to_bytes(2, "little")
    head[6:8] = (40).to_bytes(2, "little"); head[8:10] = (40 + len(inst)).to_bytes(2, "little")
    head[10:12] = (48).to_bytes(2, "little"); head[12:14] = (146).to_bytes(2, "little")
    head[36:38] = (16).to_bytes(2, "little")
    return bytes(head) + bytes(inst) + music
