#!/usr/bin/env python3
"""Extract + decrypt the Real Steel XBLA XEX into a flat image for the recompiler.

Usage:
  python3 tools/extract_xex.py <container_or_xex_path> [outdir]

The container is the XBLA package (LIVE/XZP). We locate the embedded "XEX2"
header, parse the XEX2 optional headers + security info (Xenia-verified
layout), decrypt the image payload with the retail XEX key (AES-128-CBC,
session key = retail-key-decrypt of the stored aes_key), and write:

  <outdir>/image.bin      flat image, mapped 1:1 at the XEX load address
  <outdir>/manifest.txt   base=.. entry=.. image_size=..

Run the recompiler with:
  real-steel-recomp --flat data/rs/image.bin --base 0x82000000 \\
      --entry 0x82088ab8 --text 0x77bc0:0x500000  [-o out.s]
"""

import os
import struct
import sys

try:
    from Crypto.Cipher import AES
except ImportError:
    sys.exit("pycryptodome is required: pip install pycryptodome")

RETAIL_KEY = bytes(
    [0x20, 0xB1, 0x85, 0xA5, 0x9D, 0x28, 0xFD, 0xC3,
     0x40, 0x58, 0x3F, 0xBB, 0x08, 0x96, 0xBF, 0x91])


def be32(b, o):
    return struct.unpack_from('>I', b, o)[0]


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    src = sys.argv[1]
    outdir = sys.argv[2] if len(sys.argv) > 2 else "data/rs"
    os.makedirs(outdir, exist_ok=True)

    data = open(src, 'rb').read()
    x = data.find(b'XEX2')
    if x < 0:
        sys.exit('no XEX2 header found in %s' % src)

    # Trim to the XEX.
    d = data[x:]
    header_size = be32(d, 0x08)
    sec_off = be32(d, 0x10)
    header_count = be32(d, 0x14)

    opts = {}
    for i in range(header_count):
        k, v = be32(d, 0x18 + i * 8), be32(d, 0x1C + i * 8)
        opts[k] = v

    sec = d[sec_off:]
    image_size = be32(sec, 0x04)
    image_flags = be32(sec, 0x10C)
    load_addr = be32(sec, 0x110)
    page_desc_count = be32(sec, 0x180)
    page_size = 0x10000 if not (image_flags & 0x10000000) else 0x1000
    total = sum((be32(sec, 0x184 + i * 0x18) & 0xFFFFFFF) * page_size
                for i in range(page_desc_count))

    entry = opts.get(0x10100, 0)          # ENTRY_POINT
    image_base = opts.get(0x10201, 0)     # IMAGE_BASE_ADDRESS
    pe_name = opts.get(0x183FF, 0)        # ORIGINAL_PE_NAME (offset)

    name = ''
    if pe_name:
        nl = be32(d, pe_name)
        if 0 < nl < 0x400:
            name = d[pe_name + 4:pe_name + 4 + nl].split(b'\0')[0].decode('ascii', 'replace')

    # Decrypt: session key from security block, then AES-128-CBC over the
    # payload (image bytes only, header_size..end), big image zero-padded.
    aes_key = sec[0x150:0x160]
    session = AES.new(RETAIL_KEY, AES.MODE_ECB).decrypt(aes_key)
    payload = d[header_size:]
    dec = AES.new(session, AES.MODE_CBC, bytes(16)).decrypt(payload)

    img = bytearray(total)
    img[:len(dec)] = dec
    with open(os.path.join(outdir, 'image.bin'), 'wb') as f:
        f.write(img)

    with open(os.path.join(outdir, 'manifest.txt'), 'w') as f:
        f.write(f'base={image_base:#x}\n')
        f.write(f'entry={entry:#x}\n')
        f.write(f'image_size={total:#x}\n')
        f.write(f'page_size={page_size:#x}\n')
        f.write(f'pe_name={name}\n')

    print(f'XEX @ {x:#x}  header_size={header_size:#x}  base={image_base:#x}  '
          f'entry={entry:#x}  total_image={total:#x}  PE={name!r}')
    print(f'wrote {outdir}/image.bin ({len(img)} bytes) + manifest.txt')


if __name__ == '__main__':
    main()