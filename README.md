# vgm2s98

Converts [VGM](https://vgmrips.net/wiki/VGM_Specification) (Video Game Music) files to [S98](http://www.vesta.dti.ne.jp/~tsato/soft/s98/) format.

Both formats are sample-accurate chip-register-write logs. VGM is the more common interchange format; S98 is primarily used with Japanese PC-88/PC-98 players.

## Supported chips

| Chip | VGM command | S98 device |
|---|---|---|
| SN76489 | 0x50 | SN76489 (16) |
| YM2413 (OPLL) | 0x51 | OPLL (6) |
| YM2612 (OPN2) | 0x52/0x53 | OPN2 (3) |
| YM2151 (OPM) | 0x54 | OPM (5) |
| YM2203 (OPN) | 0x55 | OPN (2) |
| YM2608 (OPNA) | 0x56/0x57 | OPNA (4) |
| YM2610 (OPNB) | 0x58/0x59 | OPNA (4) |
| YM3812 (OPL2) | 0x5A | OPL (7) |
| YM3526 (OPL) | 0x5B | OPL2 (8) |
| AY-3-8910 | 0xA0 | AY8910 (15) |

PCM data blocks and stream commands are not converted (S98 has no equivalent).

## Metadata

GD3 tags from the VGM (title, game, artist, year, etc.) are mapped to S98 v3 `[S98]` key=value tags.

If the VGM header contains a non-zero **Volume Modifier** (header offset `0x7C`, added in VGM 1.60), it is stored as the S98 tag `vgm_volume_modifier`. The effective gain factor is `2 ^ (value / 32)` — for example, `+32` doubles the volume, `-32` halves it. S98 has no native gain field, so this tag is informational only.

## Building

### CMake (recommended)

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### GCC one-liner

```bash
g++ -std=c++11 -O2 -o vgm2s98 vgm2s98.cpp vgm_reader.cpp s98_writer.cpp -lm
```

### MSVC

```bat
cl /std:c++11 /O2 /Fe:vgm2s98.exe vgm2s98.cpp vgm_reader.cpp s98_writer.cpp
```

## Usage

```
vgm2s98 <input.vgm> <output.s98>
```

Progress and diagnostic messages are written to stderr.
