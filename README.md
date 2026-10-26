![studworks](https://raw.githubusercontent.com/nathaneltitane/studworks/main/studworks.svg)

[![Donate](https://img.shields.io/badge/Paypal-2f343f.svg?style=for-the-badge&logo=paypal&label=Donate)](https://www.paypal.com/donate?hosted_button_id=ZW3CDCANHJCWJ)<br>
Show your appreciation with a PayPal donation!

---

[![Star](https://img.shields.io/github/stars/nathaneltitane/studworks.svg?style=for-the-badge&logo=github&label=Star&color=2f343f)](https://github.com/nathaneltitane/studworks)<br>
Useful? Star the repository - it helps others find it.

---

[[ StudWorks // Project Page ]](https://github.com/nathaneltitane/studworks)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;[ Version // 2026-10-26 ]

---

### NOTICE

10-26-2026 ↴

- LeoCAD+ is now StudWorks, its own project built on [LeoCAD](https://github.com/leozide/leocad)
- Code split into modules: app, core, library, render, io and ui
- Settings, part palettes and minifigure templates are copied from LeoCAD on the first start
- Models keep LeoCAD's file format and open in either program
- Set inventories use your own free Rebrickable API key, asked for once
- Light and camera properties follow the piece layout: named rows, values with their names and units, one keyframe per value

10-25-2026 ↴

- Default names use a three digit number and no number sign: 'Group 001', 'Minifigure 001', 'Submodel 001.ldr', 'Camera 001', 'Spot Light 001'
- Duplicated submodels take the next free number instead of 'Copy of': 'Car.ldr' becomes 'Car 001.ldr'
- Panels share one look: headers, buttons, text boxes and spacing follow the options (gear) button
- Properties: piece, color, position, rotation and transform rows below the list - color swatch spans the piece and color rows
- Timeline: steps a piece is added and removed in, edited below the timeline - an empty removal step reads 'Never'
- Model tabs fully outlined and lined up with the view options button

10-24-2026 ↴

- Instructions: paper and screen presets, metric or imperial units and margins, orientation-based step order
- Instructions: cover page, bill of materials, blank page breaks, step dividers, document font and full page background
- Instructions: steps per page set for each page, pieces list counts, sort order and border
- Minifigure Builder panel: build a minifigure, dock the builder and add several variations to the model
- Piece preview: optional square viewport that sizes the panel to fit
- Models menu: 'Configure Minifigure...', Preferences and Instructions moved to the Edit menu

---

### Welcome to [StudWorks](https://github.com/nathaneltitane/studworks)

StudWorks is a free CAD program for creating virtual LEGO® models with the [LDraw](https://www.ldraw.org) parts library, built on [LeoCAD](https://github.com/leozide/leocad).
It keeps LeoCAD's file format, so models open in either program, and copies LeoCAD's settings on the first start.

Everything LeoCAD does, StudWorks does too - with a reworked interface and a set of modeling and instructions features on top:

- **Panels** ↴
  - Uniform panel headers, buttons and text boxes, all sized from the options (gear) button
  - Collapsible color groups, square color tiles and a color options menu
  - Properties panel with compact position, rotation and transform rows and a spanning color swatch
  - Dockable Minifigure Builder panel, optional embedded terminal panel and a square piece preview
  - Panels dock side by side in any area

- **Modeling** ↴
  - MLCad ROTSTEP step views shown and edited in the Timeline
  - Vertical XZ and YZ height grids, custom rotation snap angle and camera orbit snapping
  - Normal to Face view command, first piece at the origin option
  - Insert color options and the selected piece color as the current color
  - Separate rendering settings for the model view, piece preview and pieces list

- **Instructions** ↴
  - Page setup with paper and screen presets, units, margins and orientation
  - Cover page, bill of materials, blank page breaks and per page step layouts
  - Step dividers, document font, pieces list options and full page backgrounds

- **Models** ↴
  - Model properties with piece IDs, thumbnails, counts and BrickLink or CSV export
  - Consistent default names and numbered duplicates

---

### TL;DR

  - Prepare ↴
    - Install a C++ compiler, Qt 5 (widgets, OpenGL, print support, network, xml and concurrent modules) and zlib development packages.
    - Optional: KDE Frameworks KParts and Konsole for the embedded terminal panel.

  - Build ↴

    ```
    git clone https://github.com/nathaneltitane/studworks
    mkdir studworks-build && cd studworks-build
    qmake ../studworks/studworks.pro
    make -j"$(nproc)"
    ```

  - Run ↴
    - Start 'build/release/studworks' and point Preferences to an [LDraw parts library](https://library.ldraw.org/updates?latest) if none is found.

---

### Credits:

StudWorks is based on [LeoCAD](https://github.com/leozide/leocad) by Leonardo Zide and its contributors - see 'documentation/CREDITS.txt'.

### License:

StudWorks is free software released under the GNU General Public License version 2, the same license as LeoCAD - see 'documentation/COPYING.txt'.

LEGO® is a trademark of the LEGO Group, which does not sponsor, authorize or endorse this project.

### Repositories:

[LeoCAD](https://github.com/leozide/leocad) as the application on which StudWorks is built.

[LDraw](https://www.ldraw.org) for the parts library and file format.

### Reports:

[Submit bug report or feature request](https://github.com/nathaneltitane/studworks/issues)

### Projects:

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/dextop?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=DEXTOP)](https://github.com/nathaneltitane/dextop)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/frobulator?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=FROBULATOR)](https://github.com/nathaneltitane/frobulator)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/gutengrab?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=GutenGrab)](https://github.com/nathaneltitane/gutengrab)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/l2cu?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=L²CU)](https://github.com/nathaneltitane/l2cu)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/terminal?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=TERMINAL)](https://github.com/nathaneltitane/terminal)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/mechablocks?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=MECHA%20//%20BLOCKS)](https://github.com/nathaneltitane/mechablocks)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/pixtrm?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=PIXTRM)](https://github.com/nathaneltitane/pixtrm)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/nathaneltitane?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=NATHANEL%20%2b%20TITANE)](https://github.com/nathaneltitane/nathaneltitane)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/pewpewprints?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=PEW%21%20PEW%21%20PRINTS)](https://github.com/nathaneltitane/pewpewprints)

---

[[ StudWorks // Project Page ]](https://github.com/nathaneltitane/studworks)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;[ Version // 2026-10-26 ]

---

[![Donate](https://img.shields.io/badge/Paypal-2f343f.svg?style=for-the-badge&logo=paypal&label=Donate)](https://www.paypal.com/donate?hosted_button_id=ZW3CDCANHJCWJ)<br>
Show your appreciation with a PayPal donation!

---

[![Star](https://img.shields.io/github/stars/nathaneltitane/studworks.svg?style=for-the-badge&logo=github&label=Star&color=2f343f)](https://github.com/nathaneltitane/studworks)<br>
Useful? Star the repository - it helps others find it.
