"""Read Source archives in place; emit metadata only. Never extract game files."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import struct
import zlib


class Vpk:
    def __init__(self, path):
        self.path = Path(path)
        self.entries = {}
        with self.path.open('rb') as f:
            magic, version, tree_size = struct.unpack('<III', f.read(12))
            if magic != 0x55AA1234 or version not in (1, 2):
                raise ValueError('Unsupported VPK')
            self.header_size = 12 if version == 1 else 28
            f.seek(self.header_size)
            tree = f.read(tree_size)
        self.data_start = self.header_size + tree_size
        cursor = 0
        def string():
            nonlocal cursor
            end = tree.index(0, cursor)
            s = tree[cursor:end].decode('utf-8', errors='surrogateescape')
            cursor = end + 1
            return s
        while extension := string():
            while directory := string():
                while name := string():
                    crc, preload, archive, offset, size, end = struct.unpack_from('<IHHIIH', tree, cursor)
                    cursor += 18
                    if end != 0xffff:
                        raise ValueError('Bad directory entry terminator')
                    prefix = b'' + tree[cursor:cursor+preload]
                    cursor += preload
                    full = ('' if directory == ' ' else directory + '/') + name
                    if extension != ' ':
                        full += '.' + extension
                    self.entries[full.lower()] = (crc, archive, offset, size, prefix)
        if cursor != tree_size:
            raise ValueError('Directory tree trailing data')

    def read(self, name):
        crc, archive, offset, size, prefix = self.entries[name.lower().replace('\\', '/')]
        if archive == 0x7fff:
            path, offset = self.path, offset + self.data_start
        else:
            path = self.path.with_name(self.path.name.removesuffix('_dir.vpk') + f'_{archive:03d}.vpk')
        with path.open('rb') as f:
            f.seek(offset)
            data = prefix + f.read(size)
        if len(data) != len(prefix) + size or (zlib.crc32(data) & 0xffffffff) != crc:
            raise ValueError('VPK data size/CRC mismatch')
        return data


def model_metadata(archive, path):
    mdl = archive.read(path)
    vvd = archive.read(path[:-4] + '.vvd')
    vtx = archive.read(path[:-4] + '.dx90.vtx')
    def i(data, offset):
        return struct.unpack_from('<i', data, offset)[0]
    body_count, body_offset = i(mdl, 232), i(mdl, 236)
    result = {'path': path, 'mdl_version': i(mdl, 4), 'checksum': i(mdl, 8),
              'bytes': [len(mdl), len(vvd), len(vtx)], 'vertices_lod0': i(vvd, 16),
              'vvd_fixups': i(vvd, 48), 'vtx_version': i(vtx, 0), 'bodyparts': []}
    tex_count, tex_offset = i(mdl, 204), i(mdl, 208)
    def text(data, offset):
        return data[offset:data.index(0, offset)].decode('utf-8', errors='replace')
    result['materials'] = [text(mdl, tex_offset+n*64+i(mdl, tex_offset+n*64)) for n in range(tex_count)]
    result['material_dirs'] = [text(mdl, i(mdl, i(mdl, 216)+n*4)) for n in range(i(mdl, 212))]
    for b in range(body_count):
        bp = body_offset+b*16
        vb = i(vtx, 32)+b*8
        models = []
        for m in range(i(mdl, bp+4)):
            mp = bp+i(mdl, bp+12)+m*148
            vm = vb+i(vtx, vb+4)+m*8
            vl = vm+i(vtx, vm+4)
            mesh_count = i(mdl, mp+72)
            meshes = []
            for n in range(mesh_count):
                mesh = mp+i(mdl, mp+76)+n*116
                vtmesh = vl+i(vtx, vl+4)+n*9
                group = vtmesh+i(vtx, vtmesh+4)
                meshes.append({'material': i(mdl, mesh), 'num_vertices': i(mdl, mesh+8),
                               'vertex_offset': i(mdl, mesh+12), 'strip_groups': i(vtx, vtmesh),
                               'first_group': list(struct.unpack_from('<6iB', vtx, group))})
            models.append({'name': text(mdl, mp), 'vertices': i(mdl, mp+80),
                           'vertex_index': i(mdl, mp+84), 'meshes': meshes})
        result['bodyparts'].append({'name': text(mdl, bp+i(mdl,bp)), 'models': models})
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archive', type=Path)
    parser.add_argument('--contains', nargs='*')
    parser.add_argument('--model')
    args = parser.parse_args()
    archive = Vpk(args.archive)
    if args.model:
        print(json.dumps(model_metadata(archive, args.model), indent=2))
    else:
        names = [n for n in archive.entries if not args.contains or any(p.lower() in n for p in args.contains)]
        print(json.dumps({'count': len(archive.entries), 'matches': names}, indent=2))


if __name__ == '__main__':
    main()
