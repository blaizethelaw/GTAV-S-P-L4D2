"""Run the native read-only skate parser audit; emit counts, never game assets."""
from pathlib import Path
import argparse
import subprocess

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('harness', type=Path)
    p.add_argument('skate_root', type=Path)
    p.add_argument('--toc', default='Data/Win32/levels/game/dingolevel_ftue_island/dingolevel_ftue_island.toc')
    p.add_argument('--bundle', default='win32/levels/game/dingolevel_ftue_island/dingolevel_ftue_island')
    args = p.parse_args()
    return subprocess.run([str(args.harness.resolve()), str(args.skate_root.resolve()), args.toc, args.bundle], check=False).returncode

if __name__ == '__main__':
    raise SystemExit(main())
