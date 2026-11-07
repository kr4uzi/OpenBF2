#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Rebuild synthetic JPEG/reference using macOS sips.

Run from the repository root. Scratch files stay under out/g4d-menu-backend.
No retail images or third-party codecs are distributed.
"""
from pathlib import Path
import struct
import subprocess

scratch = Path("out/g4d-menu-backend")
scratch.mkdir(parents=True, exist_ok=True)
fixtures = Path(__file__).resolve().parent
pixels = bytearray()
for y in range(32):
    for x in range(32):
        r, g, b = ((240, 16, 32), (32, 224, 48), (16, 48, 240), (224, 208, 32))[(y // 16) * 2 + x // 16]
        pixels.extend((b, g, r))
source = scratch / "quadrants-source.bmp"
source.write_bytes(b"BM" + struct.pack("<IHHI", 54 + len(pixels), 0, 0, 54)
                  + struct.pack("<IiiHHIIIIII", 40, 32, -32, 1, 24, 0, len(pixels), 2835, 2835, 0, 0) + pixels)
subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "100", str(source),
                "--out", str(fixtures / "quadrants.jpg")], check=True)
decoded = scratch / "quadrants-reference.bmp"
subprocess.run(["sips", "-s", "format", "bmp", str(fixtures / "quadrants.jpg"), "--out", str(decoded)], check=True)
bmp = decoded.read_bytes()
assert struct.unpack_from("<ii", bmp, 18) == (32, -32)
assert struct.unpack_from("<H", bmp, 28)[0] == 24
data = bmp[struct.unpack_from("<I", bmp, 10)[0]:]
bgra = bytearray()
for i in range(0, len(data), 3):
    bgra.extend(data[i:i+3] + b"\xff")
(fixtures / "quadrants.bgra").write_bytes(bgra)
mips = bytearray()
width = 32
while width > 1:
    small = bytearray()
    for y in range(0, width, 2):
        for x in range(0, width, 2):
            for c in range(4):
                small.append((sum(bgra[((y+dy)*width+x+dx)*4+c] for dy in range(2) for dx in range(2)) + 2) // 4)
    mips.extend(small)
    bgra, width = small, width // 2
(fixtures / "quadrants-mips.bgra").write_bytes(mips)

# Convert the synthetic image to a real one-component JPEG. Remove the system
# ICC profile from the encoded file; only the synthetic pixels are distributed.
gray = fixtures / "gray.jpg"
subprocess.run(["sips", "-m", "/System/Library/ColorSync/Profiles/Generic Gray Profile.icc",
                "-s", "format", "jpeg", str(source), "--out", str(gray)], check=True)
encoded = gray.read_bytes()
stripped = bytearray(encoded[:2])
at = 2
while at < len(encoded):
    assert encoded[at] == 0xff
    marker = encoded[at+1]
    if marker == 0xda:
        stripped.extend(encoded[at:])
        break
    length = int.from_bytes(encoded[at+2:at+4], "big")
    segment = encoded[at:at+length+2]
    if not (marker == 0xe2 and segment[4:].startswith(b"ICC_PROFILE\x00")):
        stripped.extend(segment)
    at += length + 2
gray.write_bytes(stripped)
gray_bmp = scratch / "gray-reference.bmp"
subprocess.run(["sips", "-s", "format", "bmp", str(gray), "--out", str(gray_bmp)], check=True)
bmp = gray_bmp.read_bytes()
assert struct.unpack_from("<ii", bmp, 18) == (32, -32)
assert struct.unpack_from("<H", bmp, 28)[0] == 24
data = bmp[struct.unpack_from("<I", bmp, 10)[0]:]
assert all(data[i] == data[i+1] == data[i+2] for i in range(0, len(data), 3))
(fixtures / "gray.l8").write_bytes(data[::3])
