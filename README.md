![studworks](https://raw.githubusercontent.com/nathaneltitane/studworks/main/studworks.svg)

[![Donate](https://img.shields.io/badge/Paypal-2f343f.svg?style=for-the-badge&logo=paypal&label=Donate)](https://www.paypal.com/donate?hosted_button_id=ZW3CDCANHJCWJ)<br>
Show your appreciation with a PayPal donation!

---

[![Star](https://img.shields.io/github/stars/nathaneltitane/studworks.svg?style=for-the-badge&logo=github&label=Star&color=2f343f)](https://github.com/nathaneltitane/studworks)<br>
Useful? Star the repository - it helps others find it.

---

[[ StudWorks // Project Page ]](https://github.com/nathaneltitane/studworks)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;[ Version // 2026-10-28 ]

---

### NOTICE

2026-10-28 ↴

- Cutaway: cut into the model around a clicked face to see inside it, with a cube, sphere or cylinder shaped cut - from View > Cutaway, the model is never changed
- Cutaway: click for the default size or drag to size it, move and resize it with the gizmo knobs, push in and pull out by one plate, Esc leaves it
- Cutaway: faces lined with a light shade, its color and 90% to 99% transparency set in Preferences
- Cutaway: cut parts show solid surfaces in their own color where the cut passes through them, like a sawn brick instead of a hollow shell
- Projection: new model views and tabs open with the last chosen projection, set in Preferences to change only the active view or all model views at once
- Instructions: Model Projection in the Document settings renders the step and cover models in perspective or orthographic, whatever the model view shows
- Turntable: after an idle time the model turns full screen in perspective around the X, Y or Z axis set in Preferences on the background color, any input ends it
- Preferences: Views page grouped into Model, Cutaway, Turntable, Base Grid, Parts, Preview and Minifigure Builder without repeating View or Preview, every slider shows its value on its left and reaches the right edge
- Preferences: units inside their boxes and named by the count (1 stud, 6 studs), turntable idle and turn durations in seconds, grid line spacing in a spin box
- Preferences: the Files group sets the Author of new models, "Author" until it is set
- Preview: the panel, its options and its preferences are named Preview, since it shows models as well as parts
- Parts panel: a floating panel ends on whole thumbnail or list rows and scrolls by whole rows, the bottom row is never cut
- Part categories: turn each category on or off with its checkbox, sub parts and aliases are no longer listed
- Preview: 'Keep Last Selection' keeps showing the last selected part
- Minifigure Builder: no longer crashes at startup when the parts library cannot be loaded
- Panels: the right column shrinks again, value fields without limits no longer size for the largest possible number
- Minifigure Builder: the template buttons sit in pairs with Add to Model below, so the panel can shrink
- Views: a model opened from the Open dialog, recent files or a drop shows like one opened directly, sized and fitted to its loaded parts before it is first drawn, also when it is reopened, and stays fitted as the window resizes until the camera is moved
- Instructions: padding, margin, border and background for every page element, the background fills the whole box
- Instructions: notices, free text added to any page with a right-click
- Instructions: pointers from an element border to a part, step, notice or other element - eight anchors or the nearest, line or callout wedge, arrowhead or dot tip
- Instructions: settings, page layouts, notices and pointers saved in the model file as '!SW INSTRUCTION' meta commands
- Code: views, preferences, the parts library and projects split into smaller files by task - camera, input, categories, shortcuts, cache, loading, file and import - with no change in behavior

2026-10-27 ↴

- Packages: .deb, .rpm and AppImage for x86_64 and arm64, Windows installer and universal macOS disk image, all with Qt and the parts library included
- One release on GitHub, rebuilt from every change and tagged with its date version
- Versions follow the release date everywhere: 'studworks-2026-10-27-x86_64.rpm', the About dialog and the Windows file version
- Lighting styles for the default shading: Standard, Studio, Matte and Toon - also from the command line with '--lighting'
- Selection outline width, set in Preferences or with 'Increase Outline Width' and 'Decrease Outline Width'
- Cursors and toolbar icons redrawn from the Blender icon set, following the light or dark theme - move and rotate cursors show their axis
- Instructions: place, drag and lock every page element, page grid with snapping, page numbers and colors and fonts for every element
- Instructions: Print panel inside the instructions window replaces the print dialog and print preview, the window opens maximized
- Preferences: label and control columns lined up on every page, mouse shortcuts recorded by clicking, mouse tools grouped like the keyboard commands
- Menus: new Steps menu, Instructions and Properties in the File menu, Image under Export, Preferences at the bottom of the Edit menu
- Parts panel snaps its categories to the longest name when docked on top or bottom
- Terminal panel: 'Open Model Folder' button
- Traditional Chinese, Russian and Ukrainian added to the language choices
- About dialog shows the LEGO® trademark notice

2026-10-26 ↴

- LeoCAD+ is now StudWorks, its own project built on [LeoCAD](https://github.com/leozide/leocad)
- Code split into modules: application, core, library, rendering, input_output and interface
- Settings, part palettes and minifigure templates are copied from LeoCAD on the first start
- Models keep LeoCAD's file format and open in either program
- Set inventories use your own free Rebrickable API key, asked for once
- Light and camera properties follow the part layout: named rows, values with their names and units, one keyframe per value

2026-10-25 ↴

- Default names use a three digit number and no number sign: 'Group 001', 'Minifigure 001', 'Submodel 001.ldr', 'Camera 001', 'Spot Light 001'
- Duplicated submodels take the next free number instead of 'Copy of': 'Car.ldr' becomes 'Car 001.ldr'
- Panels share one look: headers, buttons, text boxes and spacing follow the options (gear) button
- Properties: part, color, position, rotation and transform rows below the list - color swatch spans the part and color rows
- Timeline: steps a part is added and removed in, edited below the timeline - an empty removal step reads 'Never'
- Model tabs fully outlined and lined up with the view options button

2026-10-24 ↴

- Instructions: paper and screen presets, metric or imperial units and margins, orientation-based step order
- Instructions: cover page, bill of materials, blank page breaks, step dividers, document font and full page background
- Instructions: steps per page set for each page, parts list counts, sort order and border
- Minifigure Builder panel: build a minifigure, dock the builder and add several variations to the model
- Preview: optional square viewport that sizes the panel to fit
- Models menu: 'Configure Minifigure...', Preferences and Instructions moved to the Edit menu

---

### Welcome to [StudWorks](https://github.com/nathaneltitane/studworks)

StudWorks is a free CAD program for creating virtual LEGO® models with the [LDraw](https://www.ldraw.org) parts library, built on [LeoCAD](https://github.com/leozide/leocad).
It keeps LeoCAD's file format, so models open in either program, and copies LeoCAD's settings on the first start.

Everything LeoCAD does, StudWorks does too - with a reworked interface and a set of modeling and instructions features on top:

![Main window](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/interface-main-window.png)

- **Interface** ↴
  - File, Edit, View, Part, Steps, Models and Help menus, with clean labels and unique mnemonics
  - View menu with Toolbars, Viewpoints, Cutaway, Cameras, Projection and Shading submenus
  - Options (gear) menus on the model view and every panel for grids, preview, parts and colors
  - Theme-aware toolbar icons redrawn from the Blender icon set
  - Preferences pages with lined up label and control columns: General, Rendering, Views, Colors, Instructions, Categories, Keyboard and Mouse

  ![Toolbars](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/interface-toolbars.png)

  <details>
  <summary>Menus</summary>

  ![Menus](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/interface-menus.png)

  ![View submenus](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/interface-menu-view.png)

  ![Options menus](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/interface-menu-options.png)

  </details>

  <details>
  <summary>Preferences and About</summary>

  ![Preferences](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/preferences.png)

  ![About](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/about.png)

  </details>

- **Panels** ↴
  - Uniform panel headers, buttons and text boxes, all sized from the options (gear) button
  - Collapsible color groups, square color tiles and a color options menu
  - Properties panel with compact position, rotation and transform rows and a spanning color swatch
  - Dockable Minifigure Builder panel, optional embedded terminal panel and a square preview
  - Panels dock side by side in any area
  - Parts panel snaps to whole thumbnail tiles and rows, docked or floating, and its categories to the longest name
  - Part categories turned on and off with a checkbox
  - Theme-aware Blender icons and cursors, with axis cursors for moving and rotating

  ![Preview, Parts, Minifigure Builder, Timeline, Colors and Properties panels](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/panels.png)

- **Modeling** ↴
  - MLCad ROTSTEP step views shown and edited in the Timeline
  - Vertical XZ and YZ height grids with their own z axis lines, grid origin z axis line, toggles for each x, y and z axis line, custom rotation snap angle and camera orbit snapping
  - Normal to Face view command, first part at the origin option
  - Insert color options and the selected part color as the current color
  - Separate rendering settings for the model view, preview and parts list
  - Standard, Studio, Matte and Toon lighting styles and an adjustable selection outline width
  - Cube, sphere and cylinder cutaways to see inside a model, with solid cut surfaces, a shaded cut and resize knobs
  - Idle turntable showing the model full screen
  - Perspective or orthographic projection for the active view or all views at once

  ![Cube, sphere and cylinder cutaways](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/modeling-cutaway.png)

  ![Standard, Studio, Matte and Toon lighting](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/modeling-lighting.png)

  ![XZ and YZ height grids](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/modeling-height-grids.png)

  ![Perspective and orthographic projection](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/modeling-projection.png)

  ![Idle turntable](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/modeling-turntable.png)

- **Instructions** ↴
  - Page setup with paper and screen presets, units, margins and orientation
  - Cover page, bill of materials, blank page breaks and per page step layouts
  - Step dividers, document font, model projection, parts list options and full page backgrounds
  - Placement panel to place, drag and lock every page element, with a snapping page grid
  - Page numbers, colors and fonts for every element and a built-in Print panel
  - Padding, margin, border and background for every page element
  - Notices and pointers to emphasize parts, steps and notices
  - Instructions saved in the model file

  ![Cover page, step page and bill of materials](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/instructions-pages.png)

  ![Instructions window](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/instructions-window.png)

  ![Properties, Grid, Placement, Print and Editor panels](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/instructions-panels.png)

- **Models** ↴
  - Model properties with part IDs, thumbnails, counts and BrickLink or CSV export
  - Consistent default names and numbered duplicates

  ![Model properties and submodels](https://raw.githubusercontent.com/nathaneltitane/studworks/main/documentation/screenshots/models.png)

---

### TL;DR

  - Install ↴
    - Download a package from the [latest release](https://github.com/nathaneltitane/studworks/releases/latest), rebuilt from every change - every package carries the date version of the release:

      | System | Package |
      | --- | --- |
      | Debian, Ubuntu | studworks_2026-10-28_amd64.deb / arm64.deb |
      | Fedora, Rocky Linux, RHEL, openSUSE | studworks-2026-10-28-x86_64.rpm / aarch64.rpm |
      | Any Linux | studworks-linux-2026-10-28-x86_64.appimage / aarch64.appimage |
      | Windows | studworks-windows-2026-10-28-x64.exe |
      | macOS | studworks-macos-2026-10-28-universal.dmg |

    - The .deb and .rpm packages install to '/opt/studworks', the AppImage runs from anywhere after 'chmod +x' - all three carry their own Qt libraries and the LDraw parts library, and only need the base system libraries.
    - Linux packages are tested on Debian 12 and 13, Ubuntu 22.04 and 24.04, Rocky Linux 9, Fedora and openSUSE Tumbleweed.
    - The macOS disk image is not notarized: on first start, right click StudWorks and choose Open.

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
    - Start 'build/release/studworks' and point Preferences to an [LDraw parts library](https://library.ldraw.org/updates?latest) or set 'STUDWORKS_LIBRARY' if none is found - up to date video card drivers are recommended for the OpenGL rendering.

  - Package ↴
    - Run 'packaging/deb.sh' on Debian or Ubuntu, 'packaging/rpm.sh' on an RPM distribution or 'packaging/appimage.sh' on any Linux, with an optional build directory - all three download the parts library when it is missing.

---

### Credits:

StudWorks is based on [LeoCAD](https://github.com/leozide/leocad) by Leonardo Zide and its contributors - see 'documentation/CREDITS.txt'.

### License:

StudWorks is free software released under the GNU General Public License version 2, the same license as LeoCAD - see 'LICENSE.md'.

LEGO® is a trademark of the LEGO® Group, which does not sponsor, authorize or endorse this project.

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

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/studworks?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=STUDWORKS)](https://github.com/nathaneltitane/studworks)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/pixtrm?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=PIXTRM)](https://github.com/nathaneltitane/pixtrm)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/nathaneltitane?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=NATHANEL%20%2b%20TITANE)](https://github.com/nathaneltitane/nathaneltitane)

[![GitHub Repo stars](https://img.shields.io/github/stars/nathaneltitane/pewpewprints?style=for-the-badge&logo=gnubash&logoColor=ffffff&label=PEW%21%20PEW%21%20PRINTS)](https://github.com/nathaneltitane/pewpewprints)

---

[[ StudWorks // Project Page ]](https://github.com/nathaneltitane/studworks)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;[ Version // 2026-10-28 ]

---

[![Donate](https://img.shields.io/badge/Paypal-2f343f.svg?style=for-the-badge&logo=paypal&label=Donate)](https://www.paypal.com/donate?hosted_button_id=ZW3CDCANHJCWJ)<br>
Show your appreciation with a PayPal donation!

---

[![Star](https://img.shields.io/github/stars/nathaneltitane/studworks.svg?style=for-the-badge&logo=github&label=Star&color=2f343f)](https://github.com/nathaneltitane/studworks)<br>
Useful? Star the repository - it helps others find it.
