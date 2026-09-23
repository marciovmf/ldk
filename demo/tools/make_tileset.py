#!/usr/bin/env python3
import json
import sys
from pathlib import Path
from PIL import Image

EXPECTED_SUFFIXES = [
    'n', 'e', 's', 'w',
    'ne', 'nw', 'es', 'sw',
    'ns', 'ew',
    'nes', 'new', 'nsw', 'esw',
    'nesw',
]


def fail(message: str) -> None:
    print(f'error: {message}', file=sys.stderr)
    raise SystemExit(1)


def resolve(base: Path, value: str) -> Path:
    p = Path(value)
    return p if p.is_absolute() else (base / p)


def load_rgba(path: Path, size: int) -> Image.Image:
    if not path.exists():
        fail(f'missing image: {path}')
    image = Image.open(path).convert('RGBA')
    if image.size != (size, size):
        fail(f'image must be {size}x{size}: {path} (got {image.size[0]}x{image.size[1]})')
    return image


def load_mask(path: Path, size: int) -> Image.Image:
    if not path.exists():
        fail(f'missing cutout image: {path}')
    image = Image.open(path).convert('L')
    if image.size != (size, size):
        fail(f'cutout must be {size}x{size}: {path} (got {image.size[0]}x{image.size[1]})')
    return image


def apply_mask(full: Image.Image, mask: Image.Image) -> Image.Image:
    out = full.copy()
    r, g, b, a = out.split()
    # new alpha = full alpha * mask luminance / 255
    alpha = Image.new('L', full.size)
    alpha_px = alpha.load()
    a_px = a.load()
    m_px = mask.load()
    width, height = full.size
    for y in range(height):
        for x in range(width):
            alpha_px[x, y] = (a_px[x, y] * m_px[x, y]) // 255
    out.putalpha(alpha)
    return out


def main() -> int:
    if len(sys.argv) != 3:
        print('usage: make_tileset.py config_file output_location', file=sys.stderr)
        return 1

    config_path = Path(sys.argv[1]).resolve()
    output_dir = Path(sys.argv[2]).resolve()

    if not config_path.exists():
        fail(f'config file not found: {config_path}')

    config = json.loads(config_path.read_text(encoding='utf-8'))
    config_dir = config_path.parent
    tile_size = int(config.get('tile_size', 160))
    cutout_map = config.get('cutouts', {})
    tiles = config.get('tiles', [])

    if sorted(cutout_map.keys()) != sorted(EXPECTED_SUFFIXES):
        fail('config.cutouts must define exactly these suffixes: ' + ', '.join(EXPECTED_SUFFIXES))

    masks = {}
    for suffix in EXPECTED_SUFFIXES:
        masks[suffix] = load_mask(resolve(config_dir, cutout_map[suffix]), tile_size)

    if not isinstance(tiles, list) or not tiles:
        fail('config.tiles must be a non-empty array')

    output_dir.mkdir(parents=True, exist_ok=True)

    for tile in tiles:
        if not isinstance(tile, dict):
            fail('each tile entry must be an object')
        name = tile.get('name')
        full_value = tile.get('full')
        full_only = bool(tile.get('full_only', False))
        if not name or not isinstance(name, str):
            fail('tile entry missing string field: name')
        if not full_value or not isinstance(full_value, str):
            fail(f'tile {name!r} missing string field: full')

        full = load_rgba(resolve(config_dir, full_value), tile_size)
        full.save(output_dir / f'{name}.png')

        if full_only:
            continue

        for suffix in EXPECTED_SUFFIXES:
            out = apply_mask(full, masks[suffix])
            out.save(output_dir / f'{name}_cut_{suffix}.png')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
