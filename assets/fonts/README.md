# Fonts

This folder should contain TrueType fonts used by Monolith.

## Current Requirements

- `DejaVuSans.ttf` (or another .ttf font) is expected at 14pt for window titles.

## How to get a font

On most Linux systems you can copy one from the system:

```bash
cp /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf .
```

Or download DejaVu Sans from https://dejavu-fonts.github.io/

## Future Plans

We may move to bitmap fonts or embed a default font later to reduce external dependencies.
