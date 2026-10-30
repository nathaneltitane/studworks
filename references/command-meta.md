# Meta Commands

Meta commands are line type 0 lines of an LDraw file, written as `0 <COMMAND> <parameters>` with the command in capitals ([LDraw File Format](https://www.ldraw.org/article/218.html)). New official commands start with `!`, unofficial ones should start with `!` too and be based on the name of the program that defines them, so StudWorx writes its own commands under the `!SW` marker.

This list holds every meta command StudWorx handles, sorted by tool or context, official ones first. The handling column reads:

| Handling | Meaning |
| --- | --- |
| Read | interpreted when a model or the parts library is loaded |
| Write | written when a model is saved |
| Keep | not interpreted, saved back unchanged at its place in the model |
| Ignore | skipped when the parts library is loaded |

## LDraw Official

### File Format

From the [LDraw File Format](https://www.ldraw.org/article/218.html).

| Command | Handling | Use |
| --- | --- | --- |
| `0 // <comment>` | Keep | comment, the bare `0 <comment>` form is deprecated |
| `0 STEP` | Read, Write | ends a building step |
| `0 WRITE <message>` | Keep | message shown when the line is reached |
| `0 PRINT <message>` | Keep | same as `WRITE` |
| `0 CLEAR` | Keep | clears the screen in viewers |
| `0 PAUSE` | Keep | pauses viewers until a key press |
| `0 SAVE` | Keep | saves the screen in viewers |

### Header

From the [File Header Specification](https://www.ldraw.org/article/398.html).

| Command | Handling | Use |
| --- | --- | --- |
| `0 <description>` | Read, Write | first line of a model, its description |
| `0 Name: <file name>` | Read, Write | model file name |
| `0 Author: <name>` | Read, Write | model author |
| `0 !LDRAW_ORG <type> <update>` | Keep | official library part type |
| `0 !LICENSE <license>` | Keep | file license |
| `0 !HELP <text>` | Keep | usage notes |
| `0 BFC CERTIFY <CW\|CCW>` | Read | see [Back Face Culling](#back-face-culling) |
| `0 !CATEGORY <category>` | Keep | part category |
| `0 !KEYWORDS <words>` | Keep | part search words |
| `0 !CMDLINE <options>` | Keep | viewer command line options |
| `0 !PREVIEW <color> <matrix>` | Keep | part preview orientation |
| `0 !HISTORY <date> <author> <text>` | Keep | file change history |
| `0 ~Moved to <file>` | Keep | moved part description |

### Multi-Part Documents

From the [MPD Language Extension](https://www.ldraw.org/article/47.html).

| Command | Handling | Use |
| --- | --- | --- |
| `0 FILE <file name>` | Read, Write | starts a submodel |
| `0 NOFILE` | Read | ends a submodel |
| `0 !DATA <file name>` | Read, Write | starts an embedded file, such as a texture image |
| `0 !: <data>` | Read, Write | base64 line of an embedded file |

### Colours

From the [Colour Definition Language Extension](https://www.ldraw.org/article/299.html), read from `LDConfig.ldr` and models.

| Command | Handling | Use |
| --- | --- | --- |
| `0 !COLOUR <name> CODE <code> VALUE <#RRGGBB> EDGE <#RRGGBB\|code>` | Read | color definition |
| `ALPHA <0-255>` | Read | transparency |
| `LUMINANCE <0-255>` | Read | glow |
| `CHROME`, `PEARLESCENT`, `RUBBER`, `MATTE_METALLIC`, `METAL` | Read | finish |
| `MATERIAL <GLITTER\|SPECKLE> ...` | Read | material parameters |

### Texture Mapping

From the [Texture Mapping Language Extension](https://www.ldraw.org/texmap-spec.html), read from parts.

| Command | Handling | Use |
| --- | --- | --- |
| `0 !TEXMAP START <method> <parameters> <image> [GLOSSMAP <image>]` | Read | maps a texture on the following lines |
| `0 !TEXMAP NEXT <method> <parameters> <image> [GLOSSMAP <image>]` | Read | maps a texture on the next line only |
| `PLANAR`, `CYLINDRICAL`, `SPHERICAL` | Read | projection method |
| `0 !TEXMAP FALLBACK` | Read | starts the geometry used without texture support |
| `0 !TEXMAP END` | Read | ends the mapping |
| `0 !: <line>` | Read | textured geometry line inside a mapping |

### Back Face Culling

From the [BFC Language Extension](https://www.ldraw.org/article/415.html), read from parts.

| Command | Handling | Use |
| --- | --- | --- |
| `0 BFC CW`, `0 BFC CCW` | Read | winding of the following polygons |
| `0 BFC INVERTNEXT` | Read | inverts the winding of the next subfile |
| `0 BFC CERTIFY <CW\|CCW>` | Read | certifies the file, with its winding |
| `0 BFC NOCERTIFY`, `0 BFC CLIP`, `0 BFC NOCLIP` | Ignore | culling is not switched per file |

### Avatars

| Command | Handling | Use |
| --- | --- | --- |
| `0 !AVATAR CATEGORY "<category>"` | Keep | avatar parts, see [Avatar Library](https://www.ldraw.org/article/346.html) |

## StudWorx

StudWorx commands take the form `0 !SW <PROPERTY> <OBJECT> <arguments>`. An animated property writes one `<PROPERTY>_KEY` line per key, with the step before the values, as in `0 !SW POSITION_KEY CAMERA 3 0 -80 40`. Matrices are written as 9 numbers of a 3 by 3 rotation, positions and vectors as 3 numbers.

### Part

Written after the type 1 line of the part they apply to.

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW STEP_HIDE PART <step>` | Read, Write | step that hides the part |
| `0 !SW HIDDEN PART` | Read, Write | part hidden in the editor |
| `0 !SW PIVOT PART <x> <y> <z> <matrix>` | Read, Write | custom pivot point |
| `0 !SW POSITION_KEY PART <step> <x> <y> <z>` | Read, Write | animated position |
| `0 !SW ORIENTATION_KEY PART <step> <matrix>` | Read, Write | animated orientation |

### Camera

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW FOV CAMERA <degrees> ZNEAR <near> ZFAR <far>` | Read, Write | field of view and clipping planes |
| `0 !SW POSITION CAMERA <x> <y> <z>` | Read, Write | camera position, `POSITION_KEY` when animated |
| `0 !SW TARGET_POSITION CAMERA <x> <y> <z>` | Read, Write | target position, `TARGET_POSITION_KEY` when animated |
| `0 !SW UP_VECTOR CAMERA <x> <y> <z>` | Read, Write | up direction, `UP_VECTOR_KEY` when animated |
| `0 !SW HIDDEN CAMERA` | Read, Write | camera hidden in the editor |
| `0 !SW ORTHOGRAPHIC CAMERA` | Read, Write | orthographic projection |
| `0 !SW LABEL CAMERA <name>` | Read, Write | camera name, ends the camera |

### Light

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW SHADOWLESS LIGHT` | Read, Write | light casts no shadows |
| `0 !SW POSITION LIGHT <x> <y> <z>` | Read, Write | light position |
| `0 !SW ORIENTATION LIGHT <matrix>` | Read, Write | light direction |
| `0 !SW COLOR_RGB LIGHT <red> <green> <blue>` | Read, Write | light color, 0 to 1 |
| `0 !SW BLENDER_RADIUS LIGHT <size>` | Read, Write | Blender point and spot radius |
| `0 !SW BLENDER_ANGLE LIGHT <degrees>` | Read, Write | Blender sun angle |
| `0 !SW BLENDER_POWER LIGHT <watts>` | Read, Write | Blender power |
| `0 !SW AREA_SIZE_X LIGHT <size>` | Read, Write | area width |
| `0 !SW AREA_SIZE_Y LIGHT <size>` | Read, Write | area height |
| `0 !SW POVRAY_POWER LIGHT <power>` | Read, Write | POV-Ray power |
| `0 !SW POVRAY_FADE_DISTANCE LIGHT <distance>` | Read, Write | POV-Ray fade distance |
| `0 !SW POVRAY_FADE_POWER LIGHT <power>` | Read, Write | POV-Ray fade power |
| `0 !SW SPOT_CONE_ANGLE LIGHT <degrees>` | Read, Write | spot cone |
| `0 !SW SPOT_PENUMBRA_ANGLE LIGHT <degrees>` | Read, Write | spot penumbra |
| `0 !SW POVRAY_SPOT_TIGHTNESS LIGHT <tightness>` | Read, Write | POV-Ray spot tightness |
| `0 !SW AREA_SHAPE LIGHT <RECTANGLE\|SQUARE\|DISK\|ELLIPSE>` | Read, Write | area shape |
| `0 !SW POVRAY_AREA_GRID_X LIGHT <count>` | Read, Write | POV-Ray area grid columns |
| `0 !SW POVRAY_AREA_GRID_Y LIGHT <count>` | Read, Write | POV-Ray area grid rows |
| `0 !SW TYPE LIGHT <POINT\|SPOT\|DIRECTIONAL\|AREA>` | Read, Write | light type |
| `0 !SW LABEL LIGHT <name>` | Read, Write | light name, ends the light |

Each light property takes the `_KEY` form when animated.

### Model

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW COMMENT MODEL <text>` | Read, Write | model comment, one line each |
| `0 !SW AUTHOR MODEL <name>` | Read | author, written as `0 Author:` |
| `0 !SW DESCRIPTION MODEL <text>` | Read | description, written as the first line |
| `0 !SW ASSET_SOURCE MODEL <base64 path>` | Read, Write | folder searched for embedded file sources |
| `0 !SW ASSET_SOURCE_PRIORITY MODEL SOURCE` | Read, Write | search the asset sources before the model folder |

### POV-Ray

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW HEADER_FILE_INCLUDE POV_RAY <path>` | Read, Write | file included before the scene |
| `0 !SW FOOTER_FILE_INCLUDE POV_RAY <path>` | Read, Write | file included after the scene |
| `0 !SW FLOOR_AXIS POV_RAY <0\|1\|2>` | Read, Write | floor axis, x, y or z |
| `0 !SW FLOOR_COLOR_RGB POV_RAY <red> <green> <blue>` | Read, Write | floor color |
| `0 !SW FLOOR_AMBIENT POV_RAY <value>` | Read, Write | floor ambient |
| `0 !SW FLOOR_DIFFUSE POV_RAY <value>` | Read, Write | floor diffuse |
| `0 !SW EXCLUDE_FLOOR POV_RAY` | Read, Write | no floor |
| `0 !SW EXCLUDE_BACKGROUND POV_RAY` | Read, Write | no background |
| `0 !SW NO_REFLECTION POV_RAY` | Read, Write | no reflections |
| `0 !SW NO_SHADOWS POV_RAY` | Read, Write | no shadows |
| `0 !SW USE_LGEO POV_RAY` | Read, Write | use the LGEO library |

### Instruction

Instructions settings, written in the main model header. Models are quoted file names, `-` for none. Steps are `STEP <model> <step>`, pages `<STEPS|COVER|BILL_OF_MATERIALS|BLANK> <model> <step> <rank>` and page elements `<element> <model> <step>`, with the element `NONE`, `STEP_NUMBER`, `PARTS_LIST`, `COVER_TITLE`, `COVER_AUTHOR`, `BILL_OF_MATERIALS`, `STEP_IMAGE`, `COVER_IMAGE`, `PAGE_NUMBER`, `NOTICE` or `POINTER`. Values are `TRUE`/`FALSE`, numbers, `#RRGGBBAA` colors or text to the end of the line, with `\\`, `\"` and `\n` escaped.

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW PROPERTY INSTRUCTION <name> <value>` | Read, Write | document property, such as `STEP_NUMBER_BORDER_WIDTH 3` |
| `0 !SW PAGE_PROPERTY INSTRUCTION STEP <model> <step> <name> <value>` | Read, Write | property of the page starting at the step |
| `0 !SW STEPS_PER_PAGE INSTRUCTION <rows> <columns>` | Read, Write | steps per page |
| `0 !SW PAGE_STEPS_PER_PAGE INSTRUCTION STEP <model> <step> <rows> <columns>` | Read, Write | steps per page from the step on |
| `0 !SW BLANK_PAGE INSTRUCTION STEP <model> <step>` | Read, Write | blank page before the step |
| `0 !SW OFFSET INSTRUCTION <element> <model> <step> <x> <y>` | Read, Write | element moved from its place |
| `0 !SW LOCK INSTRUCTION <element> <model> <step> <x> <y>` | Read, Write | element locked at a position |
| `0 !SW NOTICE INSTRUCTION <id> PAGE <page> POSITION <x> <y> TEXT <text>` | Read, Write | notice on a page |
| `0 !SW POINTER INSTRUCTION <id> PAGE <page> SOURCE <element> ANCHOR <AUTO\|0-7> STYLE <DEFAULT\|LINE\|WEDGE> TIP <DEFAULT\|ARROWHEAD\|DOT> TARGET <POINT\|ELEMENT\|PART> <element> PART <index> POSITION <x> <y>` | Read, Write | pointer from an element border to a point, element or part |

### Group

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW BEGIN GROUP <name>` | Read, Write | starts a group, groups nest |
| `0 !SW END GROUP` | Read, Write | ends the innermost group |

### Synthesized Path

Flexible parts such as hoses and cables, built from control points.

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW BEGIN SYNTH_PATH` | Read, Write | starts the control points of the next part |
| `0 !SW CONTROL_POINT SYNTH_PATH <x> <y> <z> <matrix> <scale>` | Read, Write | control point |
| `0 !SW END SYNTH_PATH` | Read, Write | ends the control points |

### Clipboard

| Command | Handling | Use |
| --- | --- | --- |
| `0 !SW PROJECT CLIPBOARD <uuid>` | Read, Write | source project of copied parts, clipboard only |

## LeoCAD

LeoCAD commands take the form `0 !LEOCAD <OBJECT> <PROPERTY> <arguments>`. They are read as the matching StudWorx commands. A model opened with them asks to migrate, a migrated model is saved with StudWorx commands and a kept model is saved with LeoCAD commands.

| LeoCAD | StudWorx |
| --- | --- |
| `0 !LEOCAD PIECE <PROPERTY>` | `0 !SW <PROPERTY> PART` |
| `0 !LEOCAD <OBJECT> ROTATION`, `ROTATION_KEY` | `0 !SW ORIENTATION <OBJECT>`, `ORIENTATION_KEY` |
| `0 !LEOCAD LIGHT COLOR`, `COLOR_KEY` | `0 !SW COLOR_RGB LIGHT`, `COLOR_RGB_KEY` |
| `0 !LEOCAD <OBJECT> NAME <name>` | `0 !SW LABEL <OBJECT> <name>` |
| `0 !LEOCAD CAMERA HIDDEN ORTHOGRAPHIC NAME <name>` | `0 !SW HIDDEN CAMERA`, `0 !SW ORTHOGRAPHIC CAMERA`, `0 !SW LABEL CAMERA <name>` |
| `0 !LEOCAD LIGHT TYPE <type> NAME <name>` | `0 !SW TYPE LIGHT <type>`, `0 !SW LABEL LIGHT <name>` |
| `0 !LEOCAD POV_RAY HEADER_INCLUDE_FILE`, `FOOTER_INCLUDE_FILE` | `0 !SW HEADER_FILE_INCLUDE POV_RAY`, `FOOTER_FILE_INCLUDE` |
| `0 !LEOCAD SYNTH BEGIN`, `CONTROL_POINT`, `END` | `0 !SW BEGIN SYNTH_PATH`, `CONTROL_POINT`, `END` |
| `0 !LEOCAD GROUP BEGIN <name>`, `END` | `0 !SW BEGIN GROUP <name>`, `END` |

Every other LeoCAD property keeps its name, as in `0 !LEOCAD PIECE PIVOT` read as `0 !SW PIVOT PART`.

## MLCad

From the [list of known unofficial meta commands](https://wiki.ldraw.org/wiki/List_of_Known_Unofficial_META_Commands).

| Command | Handling | Use |
| --- | --- | --- |
| `0 ROTSTEP <x> <y> <z> [REL\|ABS\|ADD]` | Read, Write | ends a step and sets its view rotation |
| `0 ROTSTEP END` | Read, Write | ends a step and resets the view rotation |
| `0 BACKGROUND <image>` | Keep | step background |
| `0 BUFEXCHG <A-H> <STORE\|RETRIEVE>` | Keep | buffer exchange |
| `0 GHOST <line>` | Keep | ghosted line |
| `0 GROUP <count> <name>` | Keep | MLCad group |
| `0 MLCAD BTG <name>` | Keep | belongs to group |
| `0 MLCAD HIDE <line>` | Keep | hidden line |
| `0 MLCAD SKIP_BEGIN`, `0 MLCAD SKIP_END` | Keep | skipped lines |
| `0 MLCAD ARROW_BEGIN`, `0 MLCAD ARROW_END` | Keep | arrows |
| `0 ROTATION CENTER`, `CONFIG`, `AXLE` | Keep | rotation centers |
| `0 MLCAD SPRING`, `FLEXHOSE`, `RUBBER_BELT` | Keep | generated flexible parts |

## LPub

| Command | Handling | Use |
| --- | --- | --- |
| `0 !LPUB <command>`, `0 LPUB <command>` | Keep | instruction layout, parts lists, callouts and placement |

## LSynth

| Command | Handling | Use |
| --- | --- | --- |
| `0 SYNTH BEGIN <type> <color>` | Keep | starts a synthesized part |
| `0 SYNTH END` | Keep | ends a synthesized part |
| `0 SYNTH INSIDE`, `OUTSIDE`, `CROSS` | Keep | constraint side |
| `0 SYNTH SHOW`, `HIDE` | Keep | constraint visibility |
| `0 SYNTHESIZED BEGIN`, `0 SYNTHESIZED END` | Keep | generated lines |

The StudWorx `SYNTH_PATH` object is named apart from these commands, which describe a different format.

## L3P

| Command | Handling | Use |
| --- | --- | --- |
| `0 L3P IFPOV`, `IFNOTPOV`, `ELSEPOV`, `ENDPOV` | Keep | POV-Ray only and non POV-Ray lines |

## LDLite

| Command | Handling | Use |
| --- | --- | --- |
| `0 COLOR`, `0 COLOUR`, `0 COLORNAME`, `0 COLOURNAME` | Keep | color definitions |
| `0 TRANSLATE`, `0 ROTATE`, `0 SCALE`, `0 TRANSFORM`, `0 MATRIX` | Keep | transformations |
| `0 POINT` | Keep | point |
| `0 CMDLINE` | Keep | command line options |

## Blender

The Blender LDraw import add-on handles these commands when its options in Preferences ask for it. The step StudWorx hands to Blender is written with LeoCAD meta commands, the form the add-on reads cameras, lights and groups from.

| Command | Option | Use |
| --- | --- | --- |
| `0 BFC` | BFC | back face culling |
| `0 CLEAR` | CLEAR Command | hides earlier parts in the timeline |
| `0 !LEOCAD GROUP`, `0 GROUP` | GROUP Command | collections from groups |
| `0 !LEOCAD CAMERA` | Import Cameras | cameras |
| `0 !LEOCAD LIGHT` | Import Lights | lights |
| `0 PRINT`, `0 WRITE` | PRINT/WRITE Command | messages printed to the console |
| `0 STEP` | STEP Command, STEP Groups | timeline keys and step collections |
| `0 !TEXMAP`, `0 !DATA` | TEXMAP and DATA Command | textures |
| `0 PAUSE`, `0 SAVE` | PAUSE Command, SAVE Command | not implemented |
