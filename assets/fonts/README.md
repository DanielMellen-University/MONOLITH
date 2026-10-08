# Fonts

`DejaVuSans.ttf` is the interface font for every window, menu and app. MONOLITH loads it at 14 pt, and the Settings text size option scales it.

MONOLITH opens `assets/fonts/DejaVuSans.ttf` relative to the current directory, so run the app from the repository root. If the file is not found it tries:

1. `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`
2. `/usr/share/fonts/dejavu/DejaVuSans.ttf`
3. `/usr/share/fonts/truetype/DejaVuSans.ttf`

If none exist, MONOLITH prints a warning and text does not render. Installing `fonts-dejavu` provides the system copy.

DejaVu fonts are free software, distributed under the DejaVu Fonts License (based on the Bitstream Vera license). Project page: https://dejavu-fonts.github.io/
