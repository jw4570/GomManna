"""Optional independent scanner test (Python + numpy + zxing-cpp).

Usage: python verify_qr_decode.py /path/to/room_invite_example
Does not modify the repository, install packages or contact a server.
"""
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

import numpy as np
import zxingcpp


def verify(executable, code=None):
    result = subprocess.run([executable] + ([code] if code else []),
                            check=True, capture_output=True, text=True)
    expected = re.search(r"^qr_payload=(.+)$", result.stderr, re.MULTILINE).group(1)
    root = ET.fromstring(result.stdout)
    width, height = map(int, root.attrib["viewBox"].split()[2:])
    assert width == height
    path = root.find("{http://www.w3.org/2000/svg}path").attrib["d"]
    modules = np.full((height, width), 255, dtype=np.uint8)
    cells = re.findall(r"M(\d+),(\d+)h1v1h-1z", path)
    assert cells and "".join(f"M{x},{y}h1v1h-1z" for x, y in cells) == path
    for x, y in cells:
        x, y = int(x), int(y)
        assert 4 <= x < width - 4 and 4 <= y < height - 4, "missing quiet zone"
        modules[y, x] = 0
    pixels = np.repeat(np.repeat(modules, 10, axis=0), 10, axis=1)
    decoded = zxingcpp.read_barcodes(pixels)
    assert len(decoded) == 1 and decoded[0].format == zxingcpp.BarcodeFormat.QRCode
    actual = decoded[0].text
    assert actual == expected, (actual, expected)
    if code:
        assert actual == "gommanna://join?code=" + code


if __name__ == "__main__":
    executable = sys.argv[1]
    for code in ["ABCD2345", "AAAAAAAA", "99999999", "HJKLMNPQ", "ZZZZZZZZ", "UGZKJA4H"]:
        verify(executable, code)
    for _ in range(14):
        verify(executable)
    print("PASS: independently decoded 20 QR SVGs with the exact invite payload and quiet zone")
