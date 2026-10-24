"""Read-only, bounded inspection of installed skate. TOC and CAS metadata.

This writes no game assets. Frostbite container-format observations are checked
against the installed archive's bounds. Unsupported layouts fail explicitly.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import ctypes


class FormatError(ValueError):
    pass


def unpack(data, fmt, at):
    n = struct.calcsize(fmt)
    if at < 0 or at + n > len(data):
        raise FormatError(f"Read outside container: {at}+{n}/{len(data)}")
    return struct.unpack_from(fmt, data, at)


def span(data, at, n):
    if at < 0 or n < 0 or at + n > len(data):
        raise FormatError("Range outside container")
    return data[at:at+n]


def cstring(data, at):
    end = data.find(b"\0", at)
    if at < 0 or end < at or end - at > 2048:
        raise FormatError("Invalid asset name")
    return data[at:end].decode("ascii")


class Toc:
    def __init__(self, path):
        self.path = Path(path)
        if self.path.stat().st_size > 32 * 1024 * 1024:
            raise FormatError("Unexpectedly large TOC")
        raw = self.path.read_bytes()
        if raw[:4] != bytes.fromhex("00d1ce01"):
            raise FormatError("Unsupported TOC envelope")
        self.data = raw[556:]
        h = unpack(self.data, ">12I", 0)
        self.bundle_at, self.bundle_count = h[1:3]
        self.chunk_at, self.chunk_count = h[4:6]
        self.names_at, self.chunk_data_at, self.chunk_words, self.flags = h[8:12]
        span(self.data, self.bundle_at, self.bundle_count*16)
        span(self.data, self.chunk_at, self.chunk_count*20)
        if self.flags & 4:
            words, symbols, table = unpack(self.data, ">3I", 48)
            self.words = unpack(self.data, ">"+str(words)+"I", self.names_at)
            tree = unpack(self.data, ">"+str(symbols)+"I", table)
            if symbols % 2:
                raise FormatError("Unpaired Huffman node")
            self.tree = list(zip(tree[::2], tree[1::2]))

    def name(self, offset):
        if not self.flags & 4:
            return cstring(self.data, self.names_at + offset)
        result = bytearray()
        bit = offset
        while len(result) < 2048:
            node = len(self.tree)-1
            steps = 0
            while node < len(self.tree):
                if bit >= len(self.words)*32 or steps > 256:
                    raise FormatError("Invalid Huffman stream")
                branch = (self.words[bit//32] >> (bit % 32)) & 1
                node = self.tree[node][branch]
                bit += 1
                steps += 1
            ch = (~node) & 65535
            if ch == 0:
                return result.decode("ascii")
            if ch > 127:
                raise FormatError("Non-ASCII name")
            result.append(ch)
        raise FormatError("Asset name too long")

    def bundles(self):
        for index in range(self.bundle_count):
            name, size_flag, offset = unpack(self.data, ">IIQ", self.bundle_at + index*16)
            size = size_flag & 0x3fffffff
            if size_flag >> 30 != 1:
                raise FormatError("External bundle storage unsupported")
            yield self.name(name), span(self.data, offset, size)


def bundle_locations(region):
    manifest_at, manifest_size, flags_at, count, data_at, res_at, chunk_at, _, count_again = unpack(region, ">9I", 0)
    if count != count_again or data_at != res_at or data_at != chunk_at:
        raise FormatError("Unsupported bundle tables")
    flags = span(region, flags_at, count)
    cursor = data_at
    identity = None
    files = []
    for flag in flags:
        if flag:
            fmt, n = (">I", 4) if flag == 1 else (">Q", 8)
            identity, = unpack(region, fmt, cursor)
            cursor += n
            if flag == 1:
                location = dict(patch=bool((identity>>16)&255), install_chunk=(identity>>8)&255, archive=identity&255)
            else:
                location = dict(patch=bool((identity>>48)&255), install_chunk=(identity>>16)&0xffffffff, archive=identity&65535)
        if identity is None:
            raise FormatError("Missing CAS identity")
        offset, size = unpack(region, ">II", cursor)
        cursor += 8
        files.append(dict(**location, offset=offset, size=size))
    return files, span(region, manifest_at, manifest_size) if manifest_size else b""


def read_cas(root, loc):
    # Only this install group has been verified; reject other identifiers.
    if loc["patch"] or loc["install_chunk"] != 3162564249:
        raise FormatError("Install group mapping not verified")
    path = root / "Data/Win32/configurations/layout/initialinstallpackage" / ("cas_%02d.cas" % loc["archive"])
    if loc["size"] > 64*1024*1024 or loc["offset"] + loc["size"] > path.stat().st_size:
        raise FormatError("Invalid CAS range")
    with path.open("rb") as f:
        f.seek(loc["offset"])
        data = f.read(loc["size"])
    if len(data) != loc["size"]:
        raise FormatError("Truncated CAS range")
    return data


def manifest_assets(data):
    size, = unpack(data, ">I", 0)
    if size + 4 != len(data):
        raise FormatError("Manifest length mismatch")
    if data[4:8] == bytes.fromhex("d68e799d"):
        endian = "<"
    elif data[4:8] == bytes.fromhex("9d798ed6"):
        endian = ">"
    else:
        raise FormatError("Unsupported bundle manifest")
    total, ebx, resources, chunks, strings, _, _ = unpack(data, endian+"7I", 8)
    if total != ebx + resources + chunks or total > 100000:
        raise FormatError("Invalid manifest counts")
    at = 36 + total*20
    assets = []
    for index in range(ebx+resources):
        name, original_size = unpack(data, endian+"II", at)
        at += 8
        assets.append(dict(kind="ebx" if index < ebx else "resource", name=cstring(data, 4+strings+name), size=original_size))
    for index in range(resources):
        typ, = unpack(data, endian+"I", at+index*4)
        assets[ebx+index]["resource_type"] = hex(typ)
    at += resources*28
    for index in range(chunks):
        guid = span(data, at, 16).hex()
        logical_offset, logical_size = unpack(data, endian+"II", at+16)
        assets.append(dict(kind="chunk", name=guid, size=(logical_offset & 65535) | logical_size))
        at += 24
    return assets


def decode_cas_memory(root, data):
    """Decode only in RAM using the player's existing Oodle library."""
    dll = None
    out = bytearray()
    at = 0
    while at < len(data):
        header, = unpack(data, ">Q", at)
        at += 8
        expanded = (header>>32) & 0xffffff
        codec = (header>>24) & 0x7f
        size = header & 0xfffff
        if len(out) + expanded > 64*1024*1024:
            raise FormatError("Decoded size exceeds limit")
        block = span(data, at, size)
        at += size
        if codec == 0:
            if expanded != len(block):
                raise FormatError("Raw block length mismatch")
            out.extend(block)
        elif codec in (0x11, 0x15, 0x19):
            if dll is None:
                dll = ctypes.CDLL(str((root / "oo2core_9_win64.dll").resolve()))
                dll.OodleLZ_Decompress.restype = ctypes.c_ssize_t
                dll.OodleLZ_Decompress.argtypes = [ctypes.c_void_p,ctypes.c_ssize_t,ctypes.c_void_p,ctypes.c_ssize_t,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_void_p,ctypes.c_ssize_t,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ssize_t,ctypes.c_int]
            src = ctypes.create_string_buffer(block)
            dst = ctypes.create_string_buffer(expanded)
            count = dll.OodleLZ_Decompress(src,size,dst,expanded,1,0,0,None,0,None,None,None,0,3)
            if count != expanded:
                raise FormatError("Oodle decoder rejected block")
            out.extend(dst.raw)
        else:
            raise FormatError(f"Unsupported CAS codec {codec:#x}")
    return bytes(out)


def inspect(root, toc_name, pattern, limit):
    toc = Toc(root / "Data" / "Win32" / (toc_name + ".toc"))
    matches = []
    for name, region in toc.bundles():
        if pattern.lower() not in name.lower():
            continue
        files, manifest = bundle_locations(region)
        matches.append(dict(name=name, files=files[:8], files_count=len(files), inline_manifest_bytes=len(manifest)))
        if len(matches) >= limit:
            break
    return dict(toc=toc_name, bundle_count=toc.bundle_count, chunk_count=toc.chunk_count, matches=matches)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("root", type=Path)
    p.add_argument("--toc", default="customization")
    p.add_argument("--contains", default="board")
    p.add_argument("--limit", type=int, default=10)
    args = p.parse_args()
    print(json.dumps(inspect(args.root, args.toc, args.contains, args.limit), indent=2))
