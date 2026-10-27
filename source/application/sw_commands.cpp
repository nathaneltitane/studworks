#include "sw_global.h"
#include "sw_commands.h"

const swCommand gCommands[] =
{
	// SW_FILE_NEW
	{
		QT_TRANSLATE_NOOP("Action", "File.New"),
		QT_TRANSLATE_NOOP("Menu", "&New"),
		QT_TRANSLATE_NOOP("Status", "Create a new model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>New Model</b></p><p>Create a new, empty model. If the current model has unsaved changes, StudWorks asks whether to save them before replacing it.</p>"),
		"Ctrl+N"
	},
	// SW_FILE_OPEN
	{
		QT_TRANSLATE_NOOP("Action", "File.Open"),
		QT_TRANSLATE_NOOP("Menu", "&Open..."),
		QT_TRANSLATE_NOOP("Status", "Open an existing model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Open Model</b></p><p>Choose an existing model file to work on. If the current model has unsaved changes, save or discard those changes before opening the chosen file.</p>"),
		"Ctrl+O"
	},
	// SW_FILE_MERGE
	{
		QT_TRANSLATE_NOOP("Action", "File.Merge"),
		QT_TRANSLATE_NOOP("Menu", "&Merge..."),
		QT_TRANSLATE_NOOP("Status", "Merge the contents of another file with the current one"),
		"",
		""
	},
	// SW_FILE_RELOAD
	{
		QT_TRANSLATE_NOOP("Action", "File.Reload"),
		QT_TRANSLATE_NOOP("Menu", "&Reload"),
		QT_TRANSLATE_NOOP("Status", "Reload the current model from disk"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Reload Model</b></p><p>Read the current model file again from disk, replacing the model in StudWorks. If the model has unsaved changes, StudWorks asks before discarding them.</p>"),
		""
	},
	// SW_FILE_TOGGLE_WATCH_FILE_CHANGES
	{
		QT_TRANSLATE_NOOP("Action", "File.ToggleWatchFileChanges"),
		QT_TRANSLATE_NOOP("Menu", "Watch for File Changes"),
		QT_TRANSLATE_NOOP("Status", "Toggle asking to reload the model when another application changes its file"),
		"",
		""
	},
	// SW_FILE_SAVE
	{
		QT_TRANSLATE_NOOP("Action", "File.Save"),
		QT_TRANSLATE_NOOP("Menu", "&Save"),
		QT_TRANSLATE_NOOP("Status", "Save the current model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Save Model</b></p><p>Save the model and its submodels to disk. The first save asks for a filename. Use Save As to save a separate copy under another name.</p>"),
		"Ctrl+S"
	},
	// SW_FILE_SAVE_AS
	{
		QT_TRANSLATE_NOOP("Action", "File.SaveAs"),
		QT_TRANSLATE_NOOP("Menu", "Save &As..."),
		QT_TRANSLATE_NOOP("Status", "Save the current model with a new name"),
		"",
		""
	},
	// SW_FILE_SAVE_IMAGE
	{
		QT_TRANSLATE_NOOP("Action", "File.SaveImage"),
		QT_TRANSLATE_NOOP("Menu", "&Image..."),
		QT_TRANSLATE_NOOP("Status", "Export an image of the current view"),
		"",
		""
	},
	// SW_FILE_IMPORT_LDD
	{
		QT_TRANSLATE_NOOP("Action", "File.Import.LDD"),
		QT_TRANSLATE_NOOP("Menu", "&LEGO® Digital Designer..."),
		QT_TRANSLATE_NOOP("Status", "Import a file in LEGO® Digital Designer LXF format"),
		"",
		""
	},
	// SW_FILE_IMPORT_INVENTORY
	{
		QT_TRANSLATE_NOOP("Action", "File.Import.Inventory"),
		QT_TRANSLATE_NOOP("Menu", "Set &Inventory..."),
		QT_TRANSLATE_NOOP("Status", "Import all pieces from an official set"),
		"",
		""
	},
	// SW_FILE_EXPORT_3DS
	{
		QT_TRANSLATE_NOOP("Action", "File.Export.3DS"),
		QT_TRANSLATE_NOOP("Menu", "3D &Studio..."),
		QT_TRANSLATE_NOOP("Status", "Export the current model in 3D Studio 3DS format"),
		"",
		""
	},
	// SW_FILE_EXPORT_COLLADA
	{
		QT_TRANSLATE_NOOP("Action", "File.Export.COLLADA"),
		QT_TRANSLATE_NOOP("Menu", "&COLLADA..."),
		QT_TRANSLATE_NOOP("Status", "Export the current model in COLLADA DAE format"),
		"",
		""
	},
	// SW_FILE_EXPORT_HTML
	{
		QT_TRANSLATE_NOOP("Action", "File.Export.HTML"),
		QT_TRANSLATE_NOOP("Menu", "&HTML..."),
		QT_TRANSLATE_NOOP("Status", "Create an HTML page for the current model"),
		"",
		""
	},
	// SW_FILE_EXPORT_BRICKLINK
	{
		QT_TRANSLATE_NOOP("Action", "File.Export.BrickLink"),
		QT_TRANSLATE_NOOP("Menu", "&BrickLink..."),
		QT_TRANSLATE_NOOP("Status", "Export a list of pieces used in BrickLink XML format"),
		"",
		""
	},
	// SW_FILE_EXPORT_CSV
	{
		QT_TRANSLATE_NOOP("Action", "File.Export.CSV"),
		QT_TRANSLATE_NOOP("Menu", "&CSV..."),
		QT_TRANSLATE_NOOP("Status", "Export a list of pieces used in comma delimited file format"),
		"",
		""
	},
	// SW_FILE_EXPORT_POVRAY
	{
		QT_TRANSLATE_NOOP("Action", "File.Export.POVRay"),
		QT_TRANSLATE_NOOP("Menu", "&POV-Ray..."),
		QT_TRANSLATE_NOOP("Status", "Export the current model in POV-Ray format"),
		"",
		""
	},
	// SW_FILE_EXPORT_WAVEFRONT
	{
		QT_TRANSLATE_NOOP("Action", "File.Export.Wavefront"),
		QT_TRANSLATE_NOOP("Menu", "&Wavefront..."),
		QT_TRANSLATE_NOOP("Status", "Export the current model in Wavefront OBJ format"),
		"",
		""
	},
	// SW_FILE_RENDER_POVRAY
	{
		QT_TRANSLATE_NOOP("Action", "File.Render.POVRay"),
		QT_TRANSLATE_NOOP("Menu", "&POV-Ray Render..."),
		QT_TRANSLATE_NOOP("Status", "Render the current model using POV-Ray"),
		"",
		""
	},
	// SW_FILE_RENDER_BLENDER
	{
		QT_TRANSLATE_NOOP("Action", "File.Render.Blender"),
		QT_TRANSLATE_NOOP("Menu", "&Blender Render..."),
		QT_TRANSLATE_NOOP("Status", "Render the current model using Blender"),
		"",
		""
	},

	// SW_FILE_RENDER_OPEN_IN_BLENDER
	{
		QT_TRANSLATE_NOOP("Action", "File.Render.OpenInBlender"),
		QT_TRANSLATE_NOOP("Menu", "&Open in Blender..."),
		QT_TRANSLATE_NOOP("Status", "Open the current model in Blender"),
		"",
		""
	},
	// SW_FILE_INSTRUCTIONS
	{
		QT_TRANSLATE_NOOP("Action", "File.Instructions"),
		QT_TRANSLATE_NOOP("Menu", "Instr&uctions..."),
		QT_TRANSLATE_NOOP("Status", "Configure instructions layout"),
		"",
		""
	},
	// SW_FILE_PRINT
	{
		QT_TRANSLATE_NOOP("Action", "File.Print"),
		QT_TRANSLATE_NOOP("Menu", "&Print..."),
		QT_TRANSLATE_NOOP("Status", "Show the Print panel of the instructions window"),
		"",
		""
	},
	// SW_FILE_RECENT1
	{
		QT_TRANSLATE_NOOP("Action", "File.Recent1"),
		QT_TRANSLATE_NOOP("Menu", "&Recent 1"),
		QT_TRANSLATE_NOOP("Status", "Open this model"),
		"",
		""
	},
	// SW_FILE_RECENT2
	{
		QT_TRANSLATE_NOOP("Action", "File.Recent2"),
		QT_TRANSLATE_NOOP("Menu", "&Recent 2"),
		QT_TRANSLATE_NOOP("Status", "Open this model"),
		"",
		""
	},
	// SW_FILE_RECENT3
	{
		QT_TRANSLATE_NOOP("Action", "File.Recent3"),
		QT_TRANSLATE_NOOP("Menu", "&Recent 3"),
		QT_TRANSLATE_NOOP("Status", "Open this model"),
		"",
		""
	},
	// SW_FILE_RECENT4
	{
		QT_TRANSLATE_NOOP("Action", "File.Recent4"),
		QT_TRANSLATE_NOOP("Menu", "&Recent 4"),
		QT_TRANSLATE_NOOP("Status", "Open this model"),
		"",
		""
	},
	// SW_FILE_EXIT
	{
		QT_TRANSLATE_NOOP("Action", "File.Exit"),
		QT_TRANSLATE_NOOP("Menu", "E&xit"),
		QT_TRANSLATE_NOOP("Status", "Quit the application; prompts to save model"),
		"",
		""
	},
	// SW_EDIT_UNDO
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Undo"),
		QT_TRANSLATE_NOOP("Menu", "&Undo"),
		QT_TRANSLATE_NOOP("Status", "Undo the last action"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Undo</b></p><p>Reverse the most recent model edit, such as inserting, moving, painting, or deleting an object. Repeat Undo to move backward through the edit history.</p>"),
		"Ctrl+Z"
	},
	// SW_EDIT_REDO
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Redo"),
		QT_TRANSLATE_NOOP("Menu", "&Redo"),
		QT_TRANSLATE_NOOP("Status", "Redo the previously undone action"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Redo</b></p><p>Reapply an edit that was reversed with Undo. Making a new edit after Undo replaces the redo history with the new sequence of edits.</p>"),
		"Ctrl+Y"
	},
	// SW_EDIT_CUT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Cut"),
		QT_TRANSLATE_NOOP("Menu", "Cu&t"),
		QT_TRANSLATE_NOOP("Status", "Cut the selection and put it on the Clipboard"),
		"",
		"Ctrl+X"
	},
	// SW_EDIT_COPY
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Copy"),
		QT_TRANSLATE_NOOP("Menu", "&Copy"),
		QT_TRANSLATE_NOOP("Status", "Copy the selection and put it on the Clipboard"),
		"",
		"Ctrl+C"
	},
	// SW_EDIT_PASTE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Paste"),
		QT_TRANSLATE_NOOP("Menu", "&Paste"),
		QT_TRANSLATE_NOOP("Status", "Insert Clipboard contents in the current step"),
		"",
		"Ctrl+V"
	},
	// SW_EDIT_PASTE_STEPS
	{
		QT_TRANSLATE_NOOP("Action", "Edit.PasteSteps"),
		QT_TRANSLATE_NOOP("Menu", "Paste with Steps"),
		QT_TRANSLATE_NOOP("Status", "Insert Clipboard contents in their original steps"),
		"",
		""
	},
	// SW_EDIT_FIND
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Find"),
		QT_TRANSLATE_NOOP("Menu", "&Find"),
		QT_TRANSLATE_NOOP("Status", "Find piece"),
		"",
		"Ctrl+F"
	},
	// SW_EDIT_FIND_NEXT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.FindNext"),
		QT_TRANSLATE_NOOP("Menu", "Find Ne&xt"),
		QT_TRANSLATE_NOOP("Status", "Find next piece"),
		"",
		"F3"
	},
	// SW_EDIT_FIND_PREVIOUS
	{
		QT_TRANSLATE_NOOP("Action", "Edit.FindPrevious"),
		QT_TRANSLATE_NOOP("Menu", "Find Pre&vious"),
		QT_TRANSLATE_NOOP("Status", "Find previous piece"),
		"",
		"Shift+F3"
	},
	// SW_EDIT_FIND_ALL
	{
		QT_TRANSLATE_NOOP("Action", "Edit.FindAll"),
		QT_TRANSLATE_NOOP("Menu", "Find All"),
		QT_TRANSLATE_NOOP("Status", "Find all pieces that match the search criteria"),
		"",
		""
	},
	// SW_EDIT_REPLACE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Replace"),
		QT_TRANSLATE_NOOP("Menu", "&Replace"),
		QT_TRANSLATE_NOOP("Status", "Replace piece"),
		"",
		""
	},
	// SW_EDIT_REPLACE_NEXT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.ReplaceNext"),
		QT_TRANSLATE_NOOP("Menu", "Replace Next"),
		QT_TRANSLATE_NOOP("Status", "Replace next piece"),
		"",
		""
	},
	// SW_EDIT_REPLACE_ALL
	{
		QT_TRANSLATE_NOOP("Action", "Edit.ReplaceAll"),
		QT_TRANSLATE_NOOP("Menu", "Replace All"),
		QT_TRANSLATE_NOOP("Status", "Replace all pieces that match the search criteria"),
		"",
		""
	},
	// SW_EDIT_SELECT_ALL
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectAll"),
		QT_TRANSLATE_NOOP("Menu", "Select &All"),
		QT_TRANSLATE_NOOP("Status", "Select all pieces in the model"),
		"",
		"Ctrl+A"
	},
	// SW_EDIT_SELECT_NONE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectNone"),
		QT_TRANSLATE_NOOP("Menu", "Select &None"),
		QT_TRANSLATE_NOOP("Status", "Deselect everything"),
		"",
		""
	},
	// SW_EDIT_SELECT_INVERT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectInvert"),
		QT_TRANSLATE_NOOP("Menu", "Select &Invert"),
		QT_TRANSLATE_NOOP("Status", "Invert the current selection set"),
		"",
		"Ctrl+I"
	},
	// SW_EDIT_SELECT_BY_NAME
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectByName"),
		QT_TRANSLATE_NOOP("Menu", "Select by Na&me..."),
		QT_TRANSLATE_NOOP("Status", "Select objects by name"),
		"",
		""
	},
	// SW_EDIT_SELECT_SINGLE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectSingle"),
		QT_TRANSLATE_NOOP("Menu", "Single Selection"),
		QT_TRANSLATE_NOOP("Status", "Select one piece at a time"),
		"",
		""
	},
	// SW_EDIT_SELECT_PIECE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectPiece"),
		QT_TRANSLATE_NOOP("Menu", "Piece Selection"),
		QT_TRANSLATE_NOOP("Status", "Select all pieces of the same type"),
		"",
		""
	},
	// SW_EDIT_SELECT_COLOR
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectColor"),
		QT_TRANSLATE_NOOP("Menu", "Color Selection"),
		QT_TRANSLATE_NOOP("Status", "Select all pieces of the same color"),
		"",
		""
	},
	// SW_EDIT_SELECT_PIECE_COLOR
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SelectPieceColor"),
		QT_TRANSLATE_NOOP("Menu", "Piece and Color Selection"),
		QT_TRANSLATE_NOOP("Status", "Select all pieces of the same type and color"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_RELATIVE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformRelative"),
		QT_TRANSLATE_NOOP("Menu", "Relative Transforms"),
		QT_TRANSLATE_NOOP("Status", "Move and rotate objects relative to the one that has focus"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_ABSOLUTE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformAbsolute"),
		QT_TRANSLATE_NOOP("Menu", "Absolute Transforms"),
		QT_TRANSLATE_NOOP("Status", "Move and rotate objects in absolute coordinates"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_TOGGLE_RELATIVE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformToggleRelative"),
		QT_TRANSLATE_NOOP("Menu", "Toggle Relative Transforms"),
		QT_TRANSLATE_NOOP("Status", "Toggle moving and rotating objects relative to the one that has focus"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_SEPARATELY
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformSeparately"),
		QT_TRANSLATE_NOOP("Menu", "Rotate Separately"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected pieces separately"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_TOGETHER
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformTogether"),
		QT_TRANSLATE_NOOP("Menu", "Rotate Together"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected pieces together"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_TOGGLE_SEPARATE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformToggleSeparate"),
		QT_TRANSLATE_NOOP("Menu", "Toggle Separate Transforms"),
		QT_TRANSLATE_NOOP("Status", "Toggle rotating selected pieces separately"),
		"",
		""
	},
	// SW_EDIT_SNAP_MOVE_TOGGLE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Snap.Toggle"),
		QT_TRANSLATE_NOOP("Menu", "Move Snap Enabled"),
		QT_TRANSLATE_NOOP("Status", "Toggle movement snapping"),
		"",
		""
	},
	// SW_EDIT_SNAP_MOVE_XY0
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY0"),
		QT_TRANSLATE_NOOP("Menu", "None"),
		QT_TRANSLATE_NOOP("Status", "Do not snap movement along the XY plane"),
		"",
		"0"
	},
	// SW_EDIT_SNAP_MOVE_XY1
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY1"),
		QT_TRANSLATE_NOOP("Menu", "1/20 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 1/20 stud"),
		"",
		"1"
	},
	// SW_EDIT_SNAP_MOVE_XY2
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY2"),
		QT_TRANSLATE_NOOP("Menu", "1/4 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 1/4 stud"),
		"",
		"2"
	},
	// SW_EDIT_SNAP_MOVE_XY3
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY3"),
		QT_TRANSLATE_NOOP("Menu", "1 Flat"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 1 flat"),
		"",
		"3"
	},
	// SW_EDIT_SNAP_MOVE_XY4
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY4"),
		QT_TRANSLATE_NOOP("Menu", "1/2 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 1/2 stud"),
		"",
		"4"
	},
	// SW_EDIT_SNAP_MOVE_XY5
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY5"),
		QT_TRANSLATE_NOOP("Menu", "1 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 1 stud"),
		"",
		"5"
	},
	// SW_EDIT_SNAP_MOVE_XY6
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY6"),
		QT_TRANSLATE_NOOP("Menu", "2 Studs"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 2 studs"),
		"",
		"6"
	},
	// SW_EDIT_SNAP_MOVE_XY7
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY7"),
		QT_TRANSLATE_NOOP("Menu", "3 Studs"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 3 studs"),
		"",
		"7"
	},
	// SW_EDIT_SNAP_MOVE_XY8
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY8"),
		QT_TRANSLATE_NOOP("Menu", "4 Studs"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 4 studs"),
		"",
		"8"
	},
	// SW_EDIT_SNAP_MOVE_XY9
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.XY9"),
		QT_TRANSLATE_NOOP("Menu", "8 Studs"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the XY plane to 8 studs"),
		"",
		"9"
	},
	// SW_EDIT_SNAP_MOVE_Z0
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z0"),
		QT_TRANSLATE_NOOP("Menu", "None"),
		QT_TRANSLATE_NOOP("Status", "Do not snap movement along the Z axis"),
		"",
		"Ctrl+Shift+0"
	},
	// SW_EDIT_SNAP_MOVE_Z1
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z1"),
		QT_TRANSLATE_NOOP("Menu", "1/20 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 1/20 stud"),
		"",
		"Ctrl+Shift+1"
	},
	// SW_EDIT_SNAP_MOVE_Z2
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z2"),
		QT_TRANSLATE_NOOP("Menu", "1/4 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 1/4 stud"),
		"",
		"Ctrl+Shift+2"
	},
	// SW_EDIT_SNAP_MOVE_Z3
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z3"),
		QT_TRANSLATE_NOOP("Menu", "1 Flat"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 1 flat"),
		"",
		"Ctrl+Shift+3"
	},
	// SW_EDIT_SNAP_MOVE_Z4
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z4"),
		QT_TRANSLATE_NOOP("Menu", "1/2 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 1/2 stud"),
		"",
		"Ctrl+Shift+4"
	},
	// SW_EDIT_SNAP_MOVE_Z5
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z5"),
		QT_TRANSLATE_NOOP("Menu", "1 Stud"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 1 stud"),
		"",
		"Ctrl+Shift+5"
	},
	// SW_EDIT_SNAP_MOVE_Z6
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z6"),
		QT_TRANSLATE_NOOP("Menu", "1 Brick"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 1 brick"),
		"",
		"Ctrl+Shift+6"
	},
	// SW_EDIT_SNAP_MOVE_Z7
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z7"),
		QT_TRANSLATE_NOOP("Menu", "2 Bricks"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 2 bricks"),
		"",
		"Ctrl+Shift+7"
	},
	// SW_EDIT_SNAP_MOVE_Z8
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z8"),
		QT_TRANSLATE_NOOP("Menu", "4 Bricks"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 4 bricks"),
		"",
		"Ctrl+Shift+8"
	},
	// SW_EDIT_SNAP_MOVE_Z9
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapMove.Z9"),
		QT_TRANSLATE_NOOP("Menu", "8 Bricks"),
		QT_TRANSLATE_NOOP("Status", "Snap movement along the Z axis to 8 bricks"),
		"",
		"Ctrl+Shift+9"
	},
	// SW_EDIT_SNAP_ANGLE_TOGGLE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Toggle"),
		QT_TRANSLATE_NOOP("Menu", "Rotation Snap Enabled"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to fixed intervals"),
		"",
		""
	},
	// SW_EDIT_SNAP_ANGLE0
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle0"),
		QT_TRANSLATE_NOOP("Menu", "None"),
		QT_TRANSLATE_NOOP("Status", "Do not snap rotations"),
		"",
		"Shift+0"
	},
	// SW_EDIT_SNAP_ANGLE1
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle1"),
		QT_TRANSLATE_NOOP("Menu", "1 Degree"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 1 degree"),
		"",
		"Shift+1"
	},
	// SW_EDIT_SNAP_ANGLE2
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle2"),
		QT_TRANSLATE_NOOP("Menu", "5 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 5 degrees"),
		"",
		"Shift+2"
	},
	// SW_EDIT_SNAP_ANGLE3
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle3"),
		QT_TRANSLATE_NOOP("Menu", "15 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 15 degrees"),
		"",
		"Shift+3"
	},
	// SW_EDIT_SNAP_ANGLE4
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle4"),
		QT_TRANSLATE_NOOP("Menu", "22.5 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 22.5 degrees"),
		"",
		"Shift+4"
	},
	// SW_EDIT_SNAP_ANGLE5
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle5"),
		QT_TRANSLATE_NOOP("Menu", "30 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 30 degrees"),
		"",
		"Shift+5"
	},
	// SW_EDIT_SNAP_ANGLE6
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle6"),
		QT_TRANSLATE_NOOP("Menu", "45 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 45 degrees"),
		"",
		"Shift+6"
	},
	// SW_EDIT_SNAP_ANGLE7
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle7"),
		QT_TRANSLATE_NOOP("Menu", "60 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 60 degrees"),
		"",
		"Shift+7"
	},
	// SW_EDIT_SNAP_ANGLE8
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle8"),
		QT_TRANSLATE_NOOP("Menu", "90 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 90 degrees"),
		"",
		"Shift+8"
	},
	// SW_EDIT_SNAP_ANGLE9
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Angle9"),
		QT_TRANSLATE_NOOP("Menu", "180 Degrees"),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to 180 degrees"),
		"",
		"Shift+9"
	},
	// SW_EDIT_SNAP_ANGLE_CUSTOM
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.Custom"),
		QT_TRANSLATE_NOOP("Menu", "Custom..."),
		QT_TRANSLATE_NOOP("Status", "Snap rotations to a custom angle"),
		"",
		""
	},
	// SW_EDIT_SNAP_ORBIT_TOGGLE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.SnapAngle.ToggleOrbit"),
		QT_TRANSLATE_NOOP("Menu", "Camera Snap Enabled"),
		QT_TRANSLATE_NOOP("Status", "Toggle orbiting the camera in steps of the rotation snap angle"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Transform"),
		QT_TRANSLATE_NOOP("Menu", "Transform"),
		QT_TRANSLATE_NOOP("Status", "Apply transform to selected objects"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformAbsoluteTranslation"),
		QT_TRANSLATE_NOOP("Menu", "Absolute Translation"),
		QT_TRANSLATE_NOOP("Status", "Switch to absolute translation mode when applying transforms"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_RELATIVE_TRANSLATION
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformRelativeTranslation"),
		QT_TRANSLATE_NOOP("Menu", "Relative Translation"),
		QT_TRANSLATE_NOOP("Status", "Switch to relative translation mode when applying transforms"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_ABSOLUTE_ROTATION
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformAbsoluteRotation"),
		QT_TRANSLATE_NOOP("Menu", "Absolute Rotation"),
		QT_TRANSLATE_NOOP("Status", "Switch to absolute rotation mode when applying transforms"),
		"",
		""
	},
	// SW_EDIT_TRANSFORM_RELATIVE_ROTATION
	{
		QT_TRANSLATE_NOOP("Action", "Edit.TransformRelativeRotation"),
		QT_TRANSLATE_NOOP("Menu", "Relative Rotation"),
		QT_TRANSLATE_NOOP("Status", "Switch to relative rotation mode when applying transforms"),
		"",
		""
	},
	// SW_EDIT_ACTION_INSERT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Insert"),
		QT_TRANSLATE_NOOP("Menu", "Insert"),
		QT_TRANSLATE_NOOP("Status", "Add new pieces to the model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Insert Piece Tool</b></p><p>Choose a piece in the Pieces dock and a color in Colors, then use this tool in the model view to place the piece. Movement snapping controls placement increments. Use the preview to check placement before inserting the piece.</p>"),
		""
	},
	// SW_EDIT_ACTION_POINT_LIGHT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.PointLight"),
		QT_TRANSLATE_NOOP("Menu", "Point Light"),
		QT_TRANSLATE_NOOP("Status", "Add new point light sources to the model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Point Light Tool</b></p><p>Use the model view to place a light that emits in all directions from a point. Select the light afterward to edit its color, shadow settings, and renderer-specific power in Properties.</p>"),
		""
	},
	// SW_EDIT_ACTION_SPOTLIGHT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.SpotLight"),
		QT_TRANSLATE_NOOP("Menu", "Spotlight"),
		QT_TRANSLATE_NOOP("Status", "Add new spotlights to the model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Spotlight Tool</b></p><p>Use the model view to create a directed cone of light. In Properties, adjust its position and target, cone angle, penumbra, and renderer-specific power.</p>"),
		""
	},
	// SW_EDIT_ACTION_DIRECTIONAL_LIGHT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.DirectionalLight"),
		QT_TRANSLATE_NOOP("Menu", "Directional Light"),
		QT_TRANSLATE_NOOP("Status", "Add new directional light sources to the model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Directional Light Tool</b></p><p>Create a light with parallel rays, such as sunlight. Its direction determines the illumination. Select it to adjust its transform, color, and renderer-specific settings in Properties.</p>"),
		""
	},
	// SW_EDIT_ACTION_AREA_LIGHT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.AreaLight"),
		QT_TRANSLATE_NOOP("Menu", "Area Light"),
		QT_TRANSLATE_NOOP("Status", "Add new area light sources to the model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Area Light Tool</b></p><p>Create a light that emits over an area. Select it to set the area shape and size in Properties, along with color and renderer-specific settings.</p>"),
		""
	},
	// SW_EDIT_ACTION_CAMERA
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Camera"),
		QT_TRANSLATE_NOOP("Menu", "Camera"),
		QT_TRANSLATE_NOOP("Status", "Create a new camera"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Camera Tool</b></p><p>Create a camera in the model view. Select the camera to edit its position, target, projection, and field of view in Properties. To look through the camera, choose it from View &gt; Cameras.</p>"),
		""
	},
	// SW_EDIT_ACTION_SELECT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Select"),
		QT_TRANSLATE_NOOP("Menu", "Select"),
		QT_TRANSLATE_NOOP("Status", "Select objects (hold the CTRL key down or drag the mouse to select multiple objects)"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Select Tool</b></p><p>Pick objects in the model view or drag a selection rectangle. Selection Mode controls whether matching piece types or colors are included. The focused object provides the reference for relative transforms.</p>"),
		""
	},
	// SW_EDIT_ACTION_MOVE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Move"),
		QT_TRANSLATE_NOOP("Menu", "Move"),
		QT_TRANSLATE_NOOP("Status", "Move selected objects"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Move Tool</b></p><p>Select the objects to move, then drag in the model view. Use the manipulator to constrain movement to an axis or plane. Movement Snap sets the increments, and Transform Options selects the coordinate reference.</p>"),
		""
	},
	// SW_EDIT_ACTION_ROTATE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Rotate"),
		QT_TRANSLATE_NOOP("Menu", "Rotate"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected pieces"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Rotate Tool</b></p><p>Select objects, then drag a rotation handle in the model view. Rotation Snap sets the angle increments. Transform Options chooses relative or model axes and whether pieces rotate together or separately.</p>"),
		""
	},
	// SW_EDIT_ACTION_DELETE
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Delete"),
		QT_TRANSLATE_NOOP("Menu", "Delete"),
		QT_TRANSLATE_NOOP("Status", "Delete objects"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Eraser Tool</b></p><p>Use this tool to remove objects by clicking them in the model view. This is different from deleting the current selection as a single command. Undo restores an object removed by the tool.</p>"),
		""
	},
	// SW_EDIT_ACTION_PAINT
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Paint"),
		QT_TRANSLATE_NOOP("Menu", "Paint"),
		QT_TRANSLATE_NOOP("Status", "Change piece color"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Paint Tool</b></p><p>Choose the replacement color in the Colors dock, then click pieces in the model view to recolor them. To recolor the current selection together, use the color button above the swatches.</p>"),
		""
	},
	// SW_EDIT_ACTION_COLOR_PICKER
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.ColorPicker"),
		QT_TRANSLATE_NOOP("Menu", "Color Picker"),
		QT_TRANSLATE_NOOP("Status", "Get piece color"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Color Picker Tool</b></p><p>Click a piece in the model view to make its color the current color. You can then use that color for insertion or painting; sampling it does not recolor the piece.</p>"),
		""
	},
	// SW_EDIT_ACTION_ZOOM
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Zoom"),
		QT_TRANSLATE_NOOP("Menu", "Zoom"),
		QT_TRANSLATE_NOOP("Status", "Zoom in or out"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Zoom Tool</b></p><p>Drag in the model view to move the viewpoint closer to or farther from the model. This changes the view rather than resizing pieces. Use Zoom Region to frame a particular area.</p>"),
		""
	},
	// SW_EDIT_ACTION_PAN
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Pan"),
		QT_TRANSLATE_NOOP("Menu", "Pan"),
		QT_TRANSLATE_NOOP("Status", "Pan the current view"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Pan Tool</b></p><p>Drag to shift the view sideways or vertically without moving the model. Use this to reposition the model on screen while keeping the viewing direction.</p>"),
		""
	},
	// SW_EDIT_ACTION_ROTATE_VIEW
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.RotateView"),
		QT_TRANSLATE_NOOP("Menu", "Rotate View"),
		QT_TRANSLATE_NOOP("Status", "Rotate the current view"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Orbit Tool</b></p><p>Drag to orbit the view around its center and inspect the model from another direction. This rotates the viewpoint rather than the selected pieces; use Rotate for object edits.</p>"),
		""
	},
	// SW_EDIT_ACTION_ROLL
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.Roll"),
		QT_TRANSLATE_NOOP("Menu", "Roll"),
		QT_TRANSLATE_NOOP("Status", "Roll the current view"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Roll Tool</b></p><p>Drag to turn the view around its viewing axis, tilting what appears upright on screen. Model pieces keep their positions and orientations.</p>"),
		""
	},
	// SW_EDIT_ACTION_ZOOM_REGION
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Tool.ZoomRegion"),
		QT_TRANSLATE_NOOP("Menu", "Zoom Region"),
		QT_TRANSLATE_NOOP("Status", "Zoom into a region of the screen"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Zoom Region Tool</b></p><p>Drag a rectangle around the area you want to inspect. The view zooms to frame that region without changing model geometry.</p>"),
		""
	},
	// SW_EDIT_CANCEL
	{
		QT_TRANSLATE_NOOP("Action", "Edit.Cancel"),
		QT_TRANSLATE_NOOP("Menu", "Cancel Action"),
		QT_TRANSLATE_NOOP("Status", "Cancel current mouse action"),
		"",
		"Esc"
	},
	// SW_VIEW_PREFERENCES
	{
		QT_TRANSLATE_NOOP("Action", "View.Preferences"),
		QT_TRANSLATE_NOOP("Menu", "P&references..."),
		QT_TRANSLATE_NOOP("Status", "Change program settings"),
		"",
		""
	},
	// SW_VIEW_ZOOM_IN
	{
		QT_TRANSLATE_NOOP("Action", "View.ZoomIn"),
		QT_TRANSLATE_NOOP("Menu", "Zoom In"),
		QT_TRANSLATE_NOOP("Status", "Zoom in"),
		"",
		"+"
	},
	// SW_VIEW_ZOOM_OUT
	{
		QT_TRANSLATE_NOOP("Action", "View.ZoomOut"),
		QT_TRANSLATE_NOOP("Menu", "Zoom Out"),
		QT_TRANSLATE_NOOP("Status", "Zoom out"),
		"",
		"-"
	},
	// SW_VIEW_ZOOM_EXTENTS
	{
		QT_TRANSLATE_NOOP("Action", "View.ZoomExtents"),
		QT_TRANSLATE_NOOP("Menu", "Zoom E&xtents"),
		QT_TRANSLATE_NOOP("Status", "Fit all pieces in the current view"),
		"",
		""
	},
	// SW_VIEW_LOOK_AT
	{
		QT_TRANSLATE_NOOP("Action", "View.LookAt"),
		QT_TRANSLATE_NOOP("Menu", "Look At"),
		QT_TRANSLATE_NOOP("Status", "Rotate view so selected pieces are at center"),
		"",
		""
	},
	// SW_VIEW_MOVE_FORWARD
	{
		QT_TRANSLATE_NOOP("Action", "View.MoveForward"),
		QT_TRANSLATE_NOOP("Menu", "Move Forward"),
		QT_TRANSLATE_NOOP("Status", "Move the current view forward"),
		"",
		""
	},
	// SW_VIEW_MOVE_BACKWARD
	{
		QT_TRANSLATE_NOOP("Action", "View.MoveBackward"),
		QT_TRANSLATE_NOOP("Menu", "Move Backward"),
		QT_TRANSLATE_NOOP("Status", "Move the current view backward"),
		"",
		""
	},
	// SW_VIEW_MOVE_LEFT
	{
		QT_TRANSLATE_NOOP("Action", "View.MoveLeft"),
		QT_TRANSLATE_NOOP("Menu", "Move Left"),
		QT_TRANSLATE_NOOP("Status", "Move the current view to the left"),
		"",
		""
	},
	// SW_VIEW_MOVE_RIGHT
	{
		QT_TRANSLATE_NOOP("Action", "View.MoveRight"),
		QT_TRANSLATE_NOOP("Menu", "Move Right"),
		QT_TRANSLATE_NOOP("Status", "Move the current view to the right"),
		"",
		""
	},
	// SW_VIEW_MOVE_UP
	{
		QT_TRANSLATE_NOOP("Action", "View.MoveUp"),
		QT_TRANSLATE_NOOP("Menu", "Move Up"),
		QT_TRANSLATE_NOOP("Status", "Move the current view up"),
		"",
		""
	},
	// SW_VIEW_MOVE_DOWN
	{
		QT_TRANSLATE_NOOP("Action", "View.MoveDown"),
		QT_TRANSLATE_NOOP("Menu", "Move Down"),
		QT_TRANSLATE_NOOP("Status", "Move the current view down"),
		"",
		""
	},
	// SW_VIEW_VIEWPOINT_FRONT
	{
		QT_TRANSLATE_NOOP("Action", "View.Viewpoint.Front"),
		QT_TRANSLATE_NOOP("Menu", "&Front"),
		QT_TRANSLATE_NOOP("Status", "View model from the front"),
		"",
		"F"
	},
	// SW_VIEW_VIEWPOINT_BACK
	{
		QT_TRANSLATE_NOOP("Action", "View.Viewpoint.Back"),
		QT_TRANSLATE_NOOP("Menu", "&Back"),
		QT_TRANSLATE_NOOP("Status", "View model from the back"),
		"",
		"B"
	},
	// SW_VIEW_VIEWPOINT_TOP
	{
		QT_TRANSLATE_NOOP("Action", "View.Viewpoint.Top"),
		QT_TRANSLATE_NOOP("Menu", "&Top"),
		QT_TRANSLATE_NOOP("Status", "View model from the top"),
		"",
		"T"
	},
	// SW_VIEW_VIEWPOINT_BOTTOM
	{
		QT_TRANSLATE_NOOP("Action", "View.Viewpoint.Bottom"),
		QT_TRANSLATE_NOOP("Menu", "B&ottom"),
		QT_TRANSLATE_NOOP("Status", "View model from the bottom"),
		"",
		"O"
	},
	// SW_VIEW_VIEWPOINT_LEFT
	{
		QT_TRANSLATE_NOOP("Action", "View.Viewpoint.Left"),
		QT_TRANSLATE_NOOP("Menu", "&Left"),
		QT_TRANSLATE_NOOP("Status", "View model from the left"),
		"",
		"L"
	},
	// SW_VIEW_VIEWPOINT_RIGHT
	{
		QT_TRANSLATE_NOOP("Action", "View.Viewpoint.Right"),
		QT_TRANSLATE_NOOP("Menu", "&Right"),
		QT_TRANSLATE_NOOP("Status", "View model from the right"),
		"",
		"R"
	},
	// SW_VIEW_VIEWPOINT_HOME
	{
		QT_TRANSLATE_NOOP("Action", "View.Viewpoint.Home"),
		QT_TRANSLATE_NOOP("Menu", "&Home"),
		QT_TRANSLATE_NOOP("Status", "View model from the default position"),
		"",
		"H"
	},
	// SW_VIEW_CAMERA_NONE
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.None"),
		QT_TRANSLATE_NOOP("Menu", "None"),
		QT_TRANSLATE_NOOP("Status", "Do not use a camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA1
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera01"),
		QT_TRANSLATE_NOOP("Menu", "Camera 1"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA2
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera02"),
		QT_TRANSLATE_NOOP("Menu", "Camera 2"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA3
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera03"),
		QT_TRANSLATE_NOOP("Menu", "Camera 3"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA4
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera04"),
		QT_TRANSLATE_NOOP("Menu", "Camera 4"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA5
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera05"),
		QT_TRANSLATE_NOOP("Menu", "Camera 5"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA6
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera06"),
		QT_TRANSLATE_NOOP("Menu", "Camera 6"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA7
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera07"),
		QT_TRANSLATE_NOOP("Menu", "Camera 7"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA8
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera08"),
		QT_TRANSLATE_NOOP("Menu", "Camera 8"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA9
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera09"),
		QT_TRANSLATE_NOOP("Menu", "Camera 9"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA10
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera10"),
		QT_TRANSLATE_NOOP("Menu", "Camera 10"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA11
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera11"),
		QT_TRANSLATE_NOOP("Menu", "Camera 11"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA12
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera12"),
		QT_TRANSLATE_NOOP("Menu", "Camera 12"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA13
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera13"),
		QT_TRANSLATE_NOOP("Menu", "Camera 13"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA14
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera14"),
		QT_TRANSLATE_NOOP("Menu", "Camera 14"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA15
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera15"),
		QT_TRANSLATE_NOOP("Menu", "Camera 15"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_CAMERA16
	{
		QT_TRANSLATE_NOOP("Action", "View.Cameras.Camera16"),
		QT_TRANSLATE_NOOP("Menu", "Camera 16"),
		QT_TRANSLATE_NOOP("Status", "Use this camera"),
		"",
		""
	},
	// SW_VIEW_TIME_FIRST
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.First"),
		QT_TRANSLATE_NOOP("Menu", "First"),
		QT_TRANSLATE_NOOP("Status", "Go to the first step of the model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>First Step</b></p><p>Display the model at its first building step. Step navigation changes which pieces are visible; it does not move pieces to a different step.</p>"),
		"Alt+Up"
	},
	// SW_VIEW_TIME_PREVIOUS
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.Previous"),
		QT_TRANSLATE_NOOP("Menu", "Previous"),
		QT_TRANSLATE_NOOP("Status", "Go to the previous step"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Previous Step</b></p><p>Display the model one building step earlier. Pieces introduced in later steps are hidden so you can inspect the earlier stage of construction.</p>"),
		"Alt+Left"
	},
	// SW_VIEW_TIME_NEXT
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.Next"),
		QT_TRANSLATE_NOOP("Menu", "Next"),
		QT_TRANSLATE_NOOP("Status", "Go to the next step"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Next Step</b></p><p>Display the next building step to reveal the next stage of the model. Use the Timeline to change which step introduces a piece.</p>"),
		"Alt+Right"
	},
	// SW_VIEW_TIME_LAST
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.Last"),
		QT_TRANSLATE_NOOP("Menu", "Last"),
		QT_TRANSLATE_NOOP("Status", "Go to the last step of the model"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Last Step</b></p><p>Display the last building step of the current model. Use this to return to the completed model after inspecting earlier steps.</p>"),
		"Alt+Down"
	},
	// SW_VIEW_TIME_INSERT_BEFORE
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.InsertBefore"),
		QT_TRANSLATE_NOOP("Menu", "Insert Before"),
		QT_TRANSLATE_NOOP("Status", "Insert a new step before the current step"),
		"",
		""
	},
	// SW_VIEW_TIME_INSERT_AFTER
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.InsertAfter"),
		QT_TRANSLATE_NOOP("Menu", "Insert After"),
		QT_TRANSLATE_NOOP("Status", "Insert a new step after the current step"),
		"",
		""
	},
	// SW_VIEW_TIME_DELETE
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.Delete"),
		QT_TRANSLATE_NOOP("Menu", "Remove Step"),
		QT_TRANSLATE_NOOP("Status", "Remove current step"),
		"",
		""
	},
	// SW_VIEW_TIME_ADD_KEYS
	{
		QT_TRANSLATE_NOOP("Action", "View.Time.AddKeys"),
		QT_TRANSLATE_NOOP("Menu", "Add Keys"),
		QT_TRANSLATE_NOOP("Status", "Toggle adding new animation keys"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Add Keys</b></p><p>Enable this when an edit should create a new animation key at the current step. For example, moving a piece at a later step can give it a different position there. The diamond controls in Properties show and toggle keys for individual properties.</p>"),
		""
	},
	// SW_VIEW_SPLIT_HORIZONTAL
	{
		QT_TRANSLATE_NOOP("Action", "View.SplitHorizontal"),
		QT_TRANSLATE_NOOP("Menu", "Split &Horizontal"),
		QT_TRANSLATE_NOOP("Status", "Split the current view horizontally"),
		"",
		""
	},
	// SW_VIEW_SPLIT_VERTICAL
	{
		QT_TRANSLATE_NOOP("Action", "View.SplitVertical"),
		QT_TRANSLATE_NOOP("Menu", "Split &Vertical"),
		QT_TRANSLATE_NOOP("Status", "Split the current view vertically"),
		"",
		""
	},
	// SW_VIEW_REMOVE_VIEW
	{
		QT_TRANSLATE_NOOP("Action", "View.RemoveView"),
		QT_TRANSLATE_NOOP("Menu", "Re&move View"),
		QT_TRANSLATE_NOOP("Status", "Remove the current view"),
		"",
		""
	},
	// SW_VIEW_RESET_VIEWS
	{
		QT_TRANSLATE_NOOP("Action", "View.ResetViews"),
		QT_TRANSLATE_NOOP("Menu", "Rese&t Views"),
		QT_TRANSLATE_NOOP("Status", "Reset all views"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_STANDARD
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Standard"),
		QT_TRANSLATE_NOOP("Menu", "Standard"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Standard Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_TOOLS
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Tools"),
		QT_TRANSLATE_NOOP("Menu", "Tools"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Tools Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_TIME
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Time"),
		QT_TRANSLATE_NOOP("Menu", "Time"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Time Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_PARTS
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Parts"),
		QT_TRANSLATE_NOOP("Menu", "Pieces"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Pieces Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_COLORS
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Colors"),
		QT_TRANSLATE_NOOP("Menu", "Colors"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Colors Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_PROPERTIES
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Properties"),
		QT_TRANSLATE_NOOP("Menu", "Properties"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Properties Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_TIMELINE
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Timeline"),
		QT_TRANSLATE_NOOP("Menu", "Timeline"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Timeline Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_PREVIEW
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Preview"),
		QT_TRANSLATE_NOOP("Menu", "Preview"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Preview Toolbar"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_TERMINAL
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.Terminal"),
		QT_TRANSLATE_NOOP("Menu", "Terminal"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Terminal panel"),
		"",
		""
	},
	// SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER
	{
		QT_TRANSLATE_NOOP("Action", "View.Toolbars.MinifigureBuilder"),
		QT_TRANSLATE_NOOP("Menu", "Minifigure Builder"),
		QT_TRANSLATE_NOOP("Status", "Toggle the Minifigure Builder Panel"),
		"",
		""
	},
	// SW_VIEW_FULL_SCREEN
	{
		QT_TRANSLATE_NOOP("Action", "View.FullScreen"),
		QT_TRANSLATE_NOOP("Menu", "&Full Screen"),
		QT_TRANSLATE_NOOP("Status", "Toggle fullscreen mode"),
		"",
		""
	},
	// SW_VIEW_CLOSE_CURRENT_TAB
	{
		QT_TRANSLATE_NOOP("Action", "View.CloseCurrentTab"),
		QT_TRANSLATE_NOOP("Menu", "Close &Tab"),
		QT_TRANSLATE_NOOP("Status", "Close current tab"),
		"",
		"Ctrl+W"
	},
	// SW_VIEW_SHADE_WIREFRAME
	{
		QT_TRANSLATE_NOOP("Action", "View.Shade.Wireframe"),
		QT_TRANSLATE_NOOP("Menu", "&Wireframe"),
		QT_TRANSLATE_NOOP("Status", "Display the scene as wireframe"),
		"",
		""
	},
	// SW_VIEW_SHADE_FLAT
	{
		QT_TRANSLATE_NOOP("Action", "View.Shade.Flat"),
		QT_TRANSLATE_NOOP("Menu", "&Flat Shading"),
		QT_TRANSLATE_NOOP("Status", "Display the scene without any shading or lights"),
		"",
		""
	},
	// SW_VIEW_SHADE_DEFAULT_LIGHTS
	{
		QT_TRANSLATE_NOOP("Action", "View.Shade.DefaultLights"),
		QT_TRANSLATE_NOOP("Menu", "&Default Lights"),
		QT_TRANSLATE_NOOP("Status", "Display the scene with the default lights"),
		"",
		""
	},
	// SW_VIEW_LIGHTING_STANDARD
	{
		QT_TRANSLATE_NOOP("Action", "View.Lighting.Standard"),
		QT_TRANSLATE_NOOP("Menu", "&Standard Lighting"),
		QT_TRANSLATE_NOOP("Status", "Use a single light with soft highlights for the default lights"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Standard Lighting</b></p><p>Light pieces drawn with the Default Lights shading mode using one light above the camera with a soft highlight. This is the classic look.</p>"),
		""
	},
	// SW_VIEW_LIGHTING_STUDIO
	{
		QT_TRANSLATE_NOOP("Action", "View.Lighting.Studio"),
		QT_TRANSLATE_NOOP("Menu", "St&udio Lighting"),
		QT_TRANSLATE_NOOP("Status", "Use key, fill and rim lights for the default lights"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Studio Lighting</b></p><p>Light pieces drawn with the Default Lights shading mode using a key light, a fill light from the camera and a rim light that brightens silhouette edges, for a softer, more photographic look.</p>"),
		""
	},
	// SW_VIEW_LIGHTING_MATTE
	{
		QT_TRANSLATE_NOOP("Action", "View.Lighting.Matte"),
		QT_TRANSLATE_NOOP("Menu", "&Matte Lighting"),
		QT_TRANSLATE_NOOP("Status", "Use the default lights without highlights"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Matte Lighting</b></p><p>Light pieces drawn with the Default Lights shading mode without specular highlights, so colors stay flat and true while surfaces keep their shading.</p>"),
		""
	},
	// SW_VIEW_LIGHTING_TOON
	{
		QT_TRANSLATE_NOOP("Action", "View.Lighting.Toon"),
		QT_TRANSLATE_NOOP("Menu", "&Toon Lighting"),
		QT_TRANSLATE_NOOP("Status", "Use banded cel shading for the default lights"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Toon Lighting</b></p><p>Light pieces drawn with the Default Lights shading mode in a few flat brightness bands with a sharp highlight, for a cartoon look that pairs well with edge lines.</p>"),
		""
	},
	// SW_VIEW_PROJECTION_PERSPECTIVE
	{
		QT_TRANSLATE_NOOP("Action", "View.Projection.Perspective"),
		QT_TRANSLATE_NOOP("Menu", "&Perspective"),
		QT_TRANSLATE_NOOP("Status", "Set the current camera to use a perspective projection"),
		"",
		""
	},
	// SW_VIEW_PROJECTION_ORTHOGRAPHIC
	{
		QT_TRANSLATE_NOOP("Action", "View.Projection.Orthographic"),
		QT_TRANSLATE_NOOP("Menu", "&Orthographic"),
		QT_TRANSLATE_NOOP("Status", "Set the current camera to use an orthographic projection"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_VIEW_SPHERE
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleViewSphere"),
		QT_TRANSLATE_NOOP("Menu", "View Sphere"),
		QT_TRANSLATE_NOOP("Status", "Toggle the view sphere"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_AXIS_ICON
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleAxisIcon"),
		QT_TRANSLATE_NOOP("Menu", "Axis Icon"),
		QT_TRANSLATE_NOOP("Status", "Toggle axis icon"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_GRID
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleGrid"),
		QT_TRANSLATE_NOOP("Menu", "Base Grid"),
		QT_TRANSLATE_NOOP("Status", "Toggle grid"),
		"",
		""
	},
	// SW_VIEW_FADE_PREVIOUS_STEPS
	{
		QT_TRANSLATE_NOOP("Action", "View.FadePreviousSteps"),
		QT_TRANSLATE_NOOP("Menu", "Fade Previous Steps"),
		QT_TRANSLATE_NOOP("Status", "Toggle fading previous model steps"),
		"",
		""
	},
	// SW_VIEW_STEP_VIEW_EDIT
	{
		QT_TRANSLATE_NOOP("Action", "View.StepView.Edit"),
		QT_TRANSLATE_NOOP("Menu", "Step View..."),
		QT_TRANSLATE_NOOP("Status", "Set the view rotation of the current step"),
		"",
		""
	},
	// SW_VIEW_STEP_VIEW_REMOVE
	{
		QT_TRANSLATE_NOOP("Action", "View.StepView.Remove"),
		QT_TRANSLATE_NOOP("Menu", "Remove Step View"),
		QT_TRANSLATE_NOOP("Status", "Remove the view rotation of the current step"),
		"",
		""
	},
	// SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW
	{
		QT_TRANSLATE_NOOP("Action", "View.StepView.ToggleFollow"),
		QT_TRANSLATE_NOOP("Menu", "Follow Step Views"),
		QT_TRANSLATE_NOOP("Status", "Toggle turning the view to each step's view rotation when changing steps"),
		"",
		""
	},
	// SW_VIEW_NORMAL_TO_FACE
	{
		QT_TRANSLATE_NOOP("Action", "View.NormalToFace"),
		QT_TRANSLATE_NOOP("Menu", "Normal to Face"),
		QT_TRANSLATE_NOOP("Status", "Click a face to align the view straight on to it"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Normal to Face</b></p><p>Click a face of any piece, including pieces inside submodels, to point the camera straight at it. The model is not changed. Press Esc to cancel.</p>"),
		""
	},
	// SW_VIEW_GRID_TOGGLE_HEIGHT_XZ
	{
		QT_TRANSLATE_NOOP("Action", "View.Grid.ToggleHeightXZ"),
		QT_TRANSLATE_NOOP("Menu", "XZ Height Grid"),
		QT_TRANSLATE_NOOP("Status", "Toggle the vertical XZ height grid"),
		"",
		""
	},
	// SW_VIEW_GRID_TOGGLE_HEIGHT_YZ
	{
		QT_TRANSLATE_NOOP("Action", "View.Grid.ToggleHeightYZ"),
		QT_TRANSLATE_NOOP("Menu", "YZ Height Grid"),
		QT_TRANSLATE_NOOP("Status", "Toggle the vertical YZ height grid"),
		"",
		""
	},
	// SW_VIEW_GRID_TOGGLE_HEIGHT_BEHIND
	{
		QT_TRANSLATE_NOOP("Action", "View.Grid.ToggleHeightBehindModel"),
		QT_TRANSLATE_NOOP("Menu", "Height Grids Behind Model"),
		QT_TRANSLATE_NOOP("Status", "Toggle drawing the height grids behind the model instead of through the origin axes"),
		"",
		""
	},
	// SW_VIEW_TERMINAL_TOGGLE_FOLLOW_MODEL
	{
		QT_TRANSLATE_NOOP("Action", "View.Terminal.ToggleFollowModel"),
		QT_TRANSLATE_NOOP("Menu", "Follow Model Folder"),
		QT_TRANSLATE_NOOP("Status", "Toggle changing the terminal to the folder of each model that is opened"),
		"",
		""
	},
	// SW_VIEW_TERMINAL_GO_TO_MODEL_FOLDER
	{
		QT_TRANSLATE_NOOP("Action", "View.Terminal.GoToModelFolder"),
		QT_TRANSLATE_NOOP("Menu", "Go to Model Folder"),
		QT_TRANSLATE_NOOP("Status", "Change the terminal to the folder of the current model"),
		"",
		""
	},
	// SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER
	{
		QT_TRANSLATE_NOOP("Action", "View.Terminal.OpenModelFolder"),
		QT_TRANSLATE_NOOP("Menu", "Open Model Folder"),
		QT_TRANSLATE_NOOP("Status", "Open the model folder"),
		QT_TRANSLATE_NOOP("WhatsThis", "<p><b>Open Model Folder</b></p><p>Open the folder of the current model in the file manager. The button is unavailable until the model is saved to a file.</p>"),
		""
	},
	// SW_VIEW_COLORS_TOGGLE_SOLID
	{
		QT_TRANSLATE_NOOP("Action", "View.Colors.ToggleSolid"),
		QT_TRANSLATE_NOOP("Menu", "Solid Colors"),
		QT_TRANSLATE_NOOP("Status", "Expand or collapse the solid colors"),
		"",
		""
	},
	// SW_VIEW_COLORS_TOGGLE_TRANSLUCENT
	{
		QT_TRANSLATE_NOOP("Action", "View.Colors.ToggleTranslucent"),
		QT_TRANSLATE_NOOP("Menu", "Translucent Colors"),
		QT_TRANSLATE_NOOP("Status", "Expand or collapse the translucent colors"),
		"",
		""
	},
	// SW_VIEW_COLORS_TOGGLE_SPECIAL
	{
		QT_TRANSLATE_NOOP("Action", "View.Colors.ToggleSpecial"),
		QT_TRANSLATE_NOOP("Menu", "Special Colors"),
		QT_TRANSLATE_NOOP("Status", "Expand or collapse the special colors"),
		"",
		""
	},
	// SW_PIECE_INSERT_COLOR_CURRENT
	{
		QT_TRANSLATE_NOOP("Action", "Piece.InsertColor.Current"),
		QT_TRANSLATE_NOOP("Menu", "Insert with Current Color"),
		QT_TRANSLATE_NOOP("Status", "Insert new pieces with the current color"),
		"",
		""
	},
	// SW_PIECE_INSERT_COLOR_SELECTED
	{
		QT_TRANSLATE_NOOP("Action", "Piece.InsertColor.Selected"),
		QT_TRANSLATE_NOOP("Menu", "Insert with Selected Piece Color"),
		QT_TRANSLATE_NOOP("Status", "Insert new pieces with the color of the selected piece"),
		"",
		""
	},
	// SW_PIECE_INSERT_COLOR_LAST_INSERTED
	{
		QT_TRANSLATE_NOOP("Action", "Piece.InsertColor.LastInserted"),
		QT_TRANSLATE_NOOP("Menu", "Insert with Last Inserted Color"),
		QT_TRANSLATE_NOOP("Status", "Insert new pieces with the color of the last inserted piece"),
		"",
		""
	},
	// SW_VIEW_PIECES_TOGGLE_LOCK_COLOR
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.ToggleLockColor"),
		QT_TRANSLATE_NOOP("Menu", "Lock Pieces Color"),
		QT_TRANSLATE_NOOP("Status", "Toggle keeping the pieces list in a fixed color instead of following the insert color"),
		"",
		""
	},
	// SW_FILE_TOGGLE_OPEN_MOST_RECENT
	{
		QT_TRANSLATE_NOOP("Action", "File.ToggleOpenMostRecent"),
		QT_TRANSLATE_NOOP("Menu", "Open Most Recent File on Startup"),
		QT_TRANSLATE_NOOP("Status", "Toggle opening the most recently used file when StudWorks starts"),
		"",
		""
	},
	// SW_FILE_TOGGLE_RESTORE_TAB_LAYOUT
	{
		QT_TRANSLATE_NOOP("Action", "File.ToggleRestoreTabLayout"),
		QT_TRANSLATE_NOOP("Menu", "Restore Tab Layout"),
		QT_TRANSLATE_NOOP("Status", "Toggle restoring saved model tabs and view layouts when opening a model"),
		"",
		""
	},
	// SW_EDIT_TOGGLE_FIXED_DIRECTION_KEYS
	{
		QT_TRANSLATE_NOOP("Action", "Edit.ToggleFixedDirectionKeys"),
		QT_TRANSLATE_NOOP("Menu", "Fixed Direction Keys"),
		QT_TRANSLATE_NOOP("Status", "Toggle moving pieces along fixed model axes instead of view-relative directions"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_MESH_LOD
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleMeshLOD"),
		QT_TRANSLATE_NOOP("Menu", "Mesh LOD"),
		QT_TRANSLATE_NOOP("Status", "Toggle using simpler meshes for distant pieces"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_HIGHLIGHT_NEW_PIECES
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleHighlightNewPieces"),
		QT_TRANSLATE_NOOP("Menu", "Highlight New Pieces"),
		QT_TRANSLATE_NOOP("Status", "Toggle highlighting pieces added in the current step"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_GRID_STUDS
	{
		QT_TRANSLATE_NOOP("Action", "View.Grid.ToggleStuds"),
		QT_TRANSLATE_NOOP("Menu", "Grid Studs"),
		QT_TRANSLATE_NOOP("Status", "Toggle drawing studs on the base grid"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_GRID_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.Grid.ToggleLines"),
		QT_TRANSLATE_NOOP("Menu", "Grid Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle drawing lines on the base grid"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_GRID_ORIGIN
	{
		QT_TRANSLATE_NOOP("Action", "View.Grid.ToggleOrigin"),
		QT_TRANSLATE_NOOP("Menu", "Grid Origin Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle drawing the base grid origin lines"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_AUTOMATE_EDGE_COLOR
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleAutomateEdgeColor"),
		QT_TRANSLATE_NOOP("Menu", "Automate Edge Color"),
		QT_TRANSLATE_NOOP("Status", "Toggle automatically computing piece edge colors"),
		"",
		""
	},
	// SW_VIEW_HIGHLIGHT_STYLE_OUTLINE
	{
		QT_TRANSLATE_NOOP("Action", "View.HighlightStyle.Outline"),
		QT_TRANSLATE_NOOP("Menu", "Outline Highlight"),
		QT_TRANSLATE_NOOP("Status", "Highlight selected and new pieces with outlines"),
		"",
		""
	},
	// SW_VIEW_HIGHLIGHT_STYLE_TINT
	{
		QT_TRANSLATE_NOOP("Action", "View.HighlightStyle.TintAndEdges"),
		QT_TRANSLATE_NOOP("Menu", "Tint and Edges Highlight"),
		QT_TRANSLATE_NOOP("Status", "Highlight selected and new pieces with tinted faces and colored edges"),
		"",
		""
	},
	// SW_VIEW_SELECTION_OUTLINE_WIDTH_INCREASE
	{
		QT_TRANSLATE_NOOP("Action", "View.HighlightStyle.IncreaseOutlineWidth"),
		QT_TRANSLATE_NOOP("Menu", "Increase Outline Width"),
		QT_TRANSLATE_NOOP("Status", "Make the selection outline thicker"),
		"",
		""
	},
	// SW_VIEW_SELECTION_OUTLINE_WIDTH_DECREASE
	{
		QT_TRANSLATE_NOOP("Action", "View.HighlightStyle.DecreaseOutlineWidth"),
		QT_TRANSLATE_NOOP("Menu", "Decrease Outline Width"),
		QT_TRANSLATE_NOOP("Status", "Make the selection outline thinner"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_PLAIN
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.Plain"),
		QT_TRANSLATE_NOOP("Menu", "Plain Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the plain style"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_THIN_LINES_LOGO
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.ThinLinesLogo"),
		QT_TRANSLATE_NOOP("Menu", "Thin Lines Logo Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the thin lines logo style"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_OUTLINE_LOGO
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.OutlineLogo"),
		QT_TRANSLATE_NOOP("Menu", "Outline Logo Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the outline logo style"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_SHARP_TOP_LOGO
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.SharpTopLogo"),
		QT_TRANSLATE_NOOP("Menu", "Sharp Top Logo Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the sharp top logo style"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_ROUNDED_TOP_LOGO
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.RoundedTopLogo"),
		QT_TRANSLATE_NOOP("Menu", "Rounded Top Logo Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the rounded top logo style"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_FLATTENED_LOGO
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.FlattenedLogo"),
		QT_TRANSLATE_NOOP("Menu", "Flattened Logo Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the flattened logo style"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_HIGH_CONTRAST
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.HighContrast"),
		QT_TRANSLATE_NOOP("Menu", "High Contrast Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the high contrast style"),
		"",
		""
	},
	// SW_VIEW_STUD_STYLE_HIGH_CONTRAST_LOGO
	{
		QT_TRANSLATE_NOOP("Action", "View.StudStyle.HighContrastLogo"),
		QT_TRANSLATE_NOOP("Menu", "High Contrast with Logo Studs"),
		QT_TRANSLATE_NOOP("Status", "Draw studs with the high contrast with logo style"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_EDGE_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleEdgeLines"),
		QT_TRANSLATE_NOOP("Menu", "Edge Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle edge lines in model views"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_CONDITIONAL_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.ToggleConditionalLines"),
		QT_TRANSLATE_NOOP("Menu", "Conditional Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle conditional lines in model views"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PREVIEW_SYNCHRONIZED_RENDERING
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.ToggleSyncRendering"),
		QT_TRANSLATE_NOOP("Menu", "Preview Uses Model View Settings"),
		QT_TRANSLATE_NOOP("Status", "Toggle using the model view rendering settings for the pieces preview"),
		"",
		""
	},
	// SW_VIEW_PREVIEW_SHADING_WIREFRAME
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.Shade.Wireframe"),
		QT_TRANSLATE_NOOP("Menu", "Preview Wireframe"),
		QT_TRANSLATE_NOOP("Status", "Display the pieces preview as wireframe"),
		"",
		""
	},
	// SW_VIEW_PREVIEW_SHADING_FLAT
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.Shade.Flat"),
		QT_TRANSLATE_NOOP("Menu", "Preview Flat Shading"),
		QT_TRANSLATE_NOOP("Status", "Display the pieces preview without any shading or lights"),
		"",
		""
	},
	// SW_VIEW_PREVIEW_SHADING_DEFAULT_LIGHTS
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.Shade.DefaultLights"),
		QT_TRANSLATE_NOOP("Menu", "Preview Default Lights"),
		QT_TRANSLATE_NOOP("Status", "Display the pieces preview with the default lights"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PREVIEW_EDGE_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.ToggleEdgeLines"),
		QT_TRANSLATE_NOOP("Menu", "Preview Edge Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle edge lines in the pieces preview"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PREVIEW_CONDITIONAL_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.ToggleConditionalLines"),
		QT_TRANSLATE_NOOP("Menu", "Preview Conditional Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle conditional lines in the pieces preview"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PIECES_SYNCHRONIZED_RENDERING
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.ToggleSyncRendering"),
		QT_TRANSLATE_NOOP("Menu", "Pieces Use Model View Settings"),
		QT_TRANSLATE_NOOP("Status", "Toggle using the model view rendering settings for the pieces list"),
		"",
		""
	},
	// SW_VIEW_PIECES_SHADING_WIREFRAME
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.Shade.Wireframe"),
		QT_TRANSLATE_NOOP("Menu", "Pieces Wireframe"),
		QT_TRANSLATE_NOOP("Status", "Display the pieces list as wireframe"),
		"",
		""
	},
	// SW_VIEW_PIECES_SHADING_FLAT
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.Shade.Flat"),
		QT_TRANSLATE_NOOP("Menu", "Pieces Flat Shading"),
		QT_TRANSLATE_NOOP("Status", "Display the pieces list without any shading or lights"),
		"",
		""
	},
	// SW_VIEW_PIECES_SHADING_DEFAULT_LIGHTS
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.Shade.DefaultLights"),
		QT_TRANSLATE_NOOP("Menu", "Pieces Default Lights"),
		QT_TRANSLATE_NOOP("Status", "Display the pieces list with the default lights"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PIECES_EDGE_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.ToggleEdgeLines"),
		QT_TRANSLATE_NOOP("Menu", "Pieces Edge Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle edge lines in the pieces list"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PIECES_CONDITIONAL_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.ToggleConditionalLines"),
		QT_TRANSLATE_NOOP("Menu", "Pieces Conditional Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle conditional lines in the pieces list"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_SYNCHRONIZED_RENDERING
	{
		QT_TRANSLATE_NOOP("Action", "View.MinifigureBuilder.ToggleSyncRendering"),
		QT_TRANSLATE_NOOP("Menu", "Minifigure Builder Uses Model View Settings"),
		QT_TRANSLATE_NOOP("Status", "Toggle using the model view rendering settings in the minifigure builder preview"),
		"",
		""
	},
	// SW_VIEW_MINIFIGURE_BUILDER_SHADING_WIREFRAME
	{
		QT_TRANSLATE_NOOP("Action", "View.MinifigureBuilder.Shade.Wireframe"),
		QT_TRANSLATE_NOOP("Menu", "Minifigure Builder Wireframe"),
		QT_TRANSLATE_NOOP("Status", "Display the minifigure builder preview as a wireframe"),
		"",
		""
	},
	// SW_VIEW_MINIFIGURE_BUILDER_SHADING_FLAT
	{
		QT_TRANSLATE_NOOP("Action", "View.MinifigureBuilder.Shade.Flat"),
		QT_TRANSLATE_NOOP("Menu", "Minifigure Builder Flat Shading"),
		QT_TRANSLATE_NOOP("Status", "Display the minifigure builder preview with flat shading"),
		"",
		""
	},
	// SW_VIEW_MINIFIGURE_BUILDER_SHADING_DEFAULT_LIGHTS
	{
		QT_TRANSLATE_NOOP("Action", "View.MinifigureBuilder.Shade.DefaultLights"),
		QT_TRANSLATE_NOOP("Menu", "Minifigure Builder Default Lights"),
		QT_TRANSLATE_NOOP("Status", "Display the minifigure builder preview with the default lights"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_EDGE_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.MinifigureBuilder.ToggleEdgeLines"),
		QT_TRANSLATE_NOOP("Menu", "Minifigure Builder Edge Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle edge lines in the minifigure builder preview"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_CONDITIONAL_LINES
	{
		QT_TRANSLATE_NOOP("Action", "View.MinifigureBuilder.ToggleConditionalLines"),
		QT_TRANSLATE_NOOP("Menu", "Minifigure Builder Conditional Lines"),
		QT_TRANSLATE_NOOP("Status", "Toggle conditional lines in the minifigure builder preview"),
		"",
		""
	},
	// SW_VIEW_PIECES_TOGGLE_SNAP_TO_TILES
	{
		QT_TRANSLATE_NOOP("Action", "View.Pieces.ToggleSnapToTiles"),
		QT_TRANSLATE_NOOP("Menu", "Snap Panel Width to Tiles"),
		QT_TRANSLATE_NOOP("Status", "Toggle snapping the pieces panel width to whole thumbnail tiles and the categories to the longest name"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PREVIEW_VIEW_SPHERE
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.ToggleViewSphere"),
		QT_TRANSLATE_NOOP("Menu", "Preview View Sphere"),
		QT_TRANSLATE_NOOP("Status", "Toggle the view sphere in the pieces preview"),
		"",
		""
	},
	// SW_VIEW_TOGGLE_PREVIEW_AXIS_ICON
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.ToggleAxisIcon"),
		QT_TRANSLATE_NOOP("Menu", "Preview Axis Icon"),
		QT_TRANSLATE_NOOP("Status", "Toggle the axis icon in the pieces preview"),
		"",
		""
	},
	// SW_FILE_TOGGLE_LOAD_PIECES_BEFORE_SHOWING
	{
		QT_TRANSLATE_NOOP("Action", "File.ToggleLoadPiecesBeforeShowing"),
		QT_TRANSLATE_NOOP("Menu", "Load All Pieces Before Showing"),
		QT_TRANSLATE_NOOP("Status", "Toggle waiting for every piece to load before showing a model"),
		"",
		""
	},
	// SW_VIEW_PREVIEW_TOGGLE_SQUARE
	{
		QT_TRANSLATE_NOOP("Action", "View.Preview.ToggleSquareViewport"),
		QT_TRANSLATE_NOOP("Menu", "Square Viewport"),
		QT_TRANSLATE_NOOP("Status", "Keep the piece preview at a 1:1 ratio"),
		"",
		""
	},
	// SW_PIECE_INSERT
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Insert"),
		QT_TRANSLATE_NOOP("Menu", "&Insert"),
		QT_TRANSLATE_NOOP("Status", "Add a new piece to the model"),
		"",
		"Insert"
	},
	// SW_PIECE_TOGGLE_FIRST_AT_ORIGIN
	{
		QT_TRANSLATE_NOOP("Action", "Piece.ToggleFirstPieceAtOrigin"),
		QT_TRANSLATE_NOOP("Menu", "Insert First Piece at Origin"),
		QT_TRANSLATE_NOOP("Status", "Toggle placing the first piece of an empty model at 0, 0, 0"),
		"",
		""
	},
	// SW_PIECE_DELETE
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Delete"),
		QT_TRANSLATE_NOOP("Menu", "&Delete"),
		QT_TRANSLATE_NOOP("Status", "Delete selected objects"),
		"",
		"Delete"
	},
	// SW_PIECE_DUPLICATE
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Duplicate"),
		QT_TRANSLATE_NOOP("Menu", "&Duplicate"),
		QT_TRANSLATE_NOOP("Status", "Create a copy of the selected pieces"),
		"",
		"Ctrl+D"
	},
	// SW_PIECE_PAINT_SELECTED
	{
		QT_TRANSLATE_NOOP("Action", "Piece.PaintSelected"),
		QT_TRANSLATE_NOOP("Menu", "&Paint Selected"),
		QT_TRANSLATE_NOOP("Status", "Change the color of the selected pieces"),
		"",
		""
	},
	// SW_PIECE_RESET_PIVOT_POINT
	{
		QT_TRANSLATE_NOOP("Action", "Piece.ResetPivotPoint"),
		QT_TRANSLATE_NOOP("Menu", "Reset &Pivot Point"),
		QT_TRANSLATE_NOOP("Status", "Reset the pivot point of the selected pieces to their origin"),
		"",
		""
	},
	// SW_PIECE_REMOVE_KEY_FRAMES
	{
		QT_TRANSLATE_NOOP("Action", "Piece.RemoveKeyFrames"),
		QT_TRANSLATE_NOOP("Menu", "Remove &Keyframes"),
		QT_TRANSLATE_NOOP("Status", "Remove all keyframes from the selected pieces"),
		"",
		""
	},
	// SW_PIECE_CONTROL_POINT_INSERT
	{
		QT_TRANSLATE_NOOP("Action", "Piece.ControlPoint.Insert"),
		QT_TRANSLATE_NOOP("Menu", "Insert Control Point"),
		QT_TRANSLATE_NOOP("Status", "Insert a new control point"),
		"",
		""
	},
	// SW_PIECE_CONTROL_POINT_REMOVE
	{
		QT_TRANSLATE_NOOP("Action", "Piece.ControlPoint.Remove"),
		QT_TRANSLATE_NOOP("Menu", "Remove Control Point"),
		QT_TRANSLATE_NOOP("Status", "Remove the selected control point"),
		"",
		""
	},
	// SW_PIECE_TRAIN_TRACK_FOCUS_NEXT
	{
		QT_TRANSLATE_NOOP("Action", "Piece.TrainTrack.FocusNext"),
		QT_TRANSLATE_NOOP("Menu", "Focus Next Connection"),
		QT_TRANSLATE_NOOP("Status", "Focus the next connection of the currently selected track"),
		"",
		""
	},
	// SW_PIECE_TRAIN_TRACK_FOCUS_PREVIOUS
	{
		QT_TRANSLATE_NOOP("Action", "Piece.TrainTrack.FocusPrevious"),
		QT_TRANSLATE_NOOP("Menu", "Focus Previous Connection"),
		QT_TRANSLATE_NOOP("Status", "Focus the previous connection of the currently selected track"),
		"",
		""
	},
	// SW_PIECE_TRAIN_TRACK_ROTATE
	{
		QT_TRANSLATE_NOOP("Action", "Piece.TrainTrack.Rotate"),
		QT_TRANSLATE_NOOP("Menu", "Rotate Train Track"),
		QT_TRANSLATE_NOOP("Status", "Rotate the focused train track around its last connection"),
		"",
		""
	},
	// SW_PIECE_MOVE_PLUS_X
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Move.PlusX"),
		QT_TRANSLATE_NOOP("Menu", "Move +X"),
		QT_TRANSLATE_NOOP("Status", "Move selected objects along the X axis"),
		"",
		"Down"
	},
	// SW_PIECE_MOVE_MINUS_X
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Move.MinusX"),
		QT_TRANSLATE_NOOP("Menu", "Move -X"),
		QT_TRANSLATE_NOOP("Status", "Move selected objects along the X axis"),
		"",
		"Up"
	},
	// SW_PIECE_MOVE_PLUS_Y
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Move.PlusY"),
		QT_TRANSLATE_NOOP("Menu", "Move +Y"),
		QT_TRANSLATE_NOOP("Status", "Move selected objects along the Y axis"),
		"",
		"Right"
	},
	// SW_PIECE_MOVE_MINUS_Y
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Move.MinusY"),
		QT_TRANSLATE_NOOP("Menu", "Move -Y"),
		QT_TRANSLATE_NOOP("Status", "Move selected objects along the Y axis"),
		"",
		"Left"
	},
	// SW_PIECE_MOVE_PLUS_Z
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Move.PlusZ"),
		QT_TRANSLATE_NOOP("Menu", "Move +Z"),
		QT_TRANSLATE_NOOP("Status", "Move selected objects along the Z axis"),
		"",
		"PgUp"
	},
	// SW_PIECE_MOVE_MINUS_Z
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Move.MinusZ"),
		QT_TRANSLATE_NOOP("Menu", "Move -Z"),
		QT_TRANSLATE_NOOP("Status", "Move selected objects along the Z axis"),
		"",
		"PgDown"
	},
	// SW_PIECE_ROTATE_PLUS_X
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Rotate.PlusX"),
		QT_TRANSLATE_NOOP("Menu", "Rotate +X"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected objects around the X axis"),
		"",
		"Shift+Down"
	},
	// SW_PIECE_ROTATE_MINUS_X
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Rotate.MinusX"),
		QT_TRANSLATE_NOOP("Menu", "Rotate -X"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected objects around the X axis"),
		"",
		"Shift+Up"
	},
	// SW_PIECE_ROTATE_PLUS_Y
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Rotate.PlusY"),
		QT_TRANSLATE_NOOP("Menu", "Rotate +Y"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected objects around the Y axis"),
		"",
		"Shift+Right"
	},
	// SW_PIECE_ROTATE_MINUS_Y
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Rotate.MinusY"),
		QT_TRANSLATE_NOOP("Menu", "Rotate -Y"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected objects around the Y axis"),
		"",
		"Shift+Left"
	},
	// SW_PIECE_ROTATE_PLUS_Z
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Rotate.PlusZ"),
		QT_TRANSLATE_NOOP("Menu", "Rotate +Z"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected objects around the Z axis"),
		"",
		"Shift+PgUp"
	},
	// SW_PIECE_ROTATE_MINUS_Z
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Rotate.MinusZ"),
		QT_TRANSLATE_NOOP("Menu", "Rotate -Z"),
		QT_TRANSLATE_NOOP("Status", "Rotate selected objects around the Z axis"),
		"",
		"Shift+PgDown"
	},
	// SW_PIECE_MINIFIGURE_BUILDER
	{
		QT_TRANSLATE_NOOP("Action", "Piece.MinifigureBuilder"),
		QT_TRANSLATE_NOOP("Menu", "&Minifigure Builder"),
		QT_TRANSLATE_NOOP("Status", "Show the Minifigure Builder panel to build a minifigure and add it to the model"),
		"",
		""
	},
	// SW_PIECE_ARRAY
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Array"),
		QT_TRANSLATE_NOOP("Menu", "A&rray..."),
		QT_TRANSLATE_NOOP("Status", "Make copies of the selected pieces"),
		"",
		""
	},
	//  SW_PIECE_VIEW_SELECTED_MODEL
	{
		QT_TRANSLATE_NOOP("Action", "Piece.ViewSelectedModel"),
		QT_TRANSLATE_NOOP("Menu", "Open Selected Model"),
		QT_TRANSLATE_NOOP("Status", "Open the model referenced by the selected piece in a new tab"),
		"",
		""
	},
	// SW_PIECE_MOVE_SELECTION_TO_MODEL
	{
		QT_TRANSLATE_NOOP("Action", "Piece.MoveSelectionToModel"),
		QT_TRANSLATE_NOOP("Menu", "Move to New Model..."),
		QT_TRANSLATE_NOOP("Status", "Move the selected pieces to a new model and replace them with a reference to the model"),
		"",
		""
	},
	// SW_PIECE_INLINE_SELECTED_MODELS
	{
		QT_TRANSLATE_NOOP("Action", "Piece.InlineSelectedModels"),
		QT_TRANSLATE_NOOP("Menu", "Inline Selected Models"),
		QT_TRANSLATE_NOOP("Status", "Insert the contents of the selected model references into the current model"),
		"",
		""
	},
	// SW_PIECE_EDIT_SELECTED_SUBMODEL
	{
		QT_TRANSLATE_NOOP("Action", "Piece.EditSelectedSubmodel"),
		QT_TRANSLATE_NOOP("Menu", "Edit Selected Submodel"),
		QT_TRANSLATE_NOOP("Status", "Edit the currently selected submodel in-place"),
		"",
		""
	},
	// SW_PIECE_EDIT_END_SUBMODEL
	{
		QT_TRANSLATE_NOOP("Action", "Piece.EditEndSubmodel"),
		QT_TRANSLATE_NOOP("Menu", "End Submodel Editing"),
		QT_TRANSLATE_NOOP("Status", "End in-place submodel editing"),
		"",
		""
	},
	// SW_PIECE_GROUP
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Group"),
		QT_TRANSLATE_NOOP("Menu", "&Group..."),
		QT_TRANSLATE_NOOP("Status", "Group selected pieces together"),
		"",
		"Ctrl+G"
	},
	// SW_PIECE_UNGROUP
	{
		QT_TRANSLATE_NOOP("Action", "Piece.Ungroup"),
		QT_TRANSLATE_NOOP("Menu", "&Ungroup"),
		QT_TRANSLATE_NOOP("Status", "Ungroup selected group"),
		"",
		"Ctrl+U"
	},
	// SW_PIECE_GROUP_ADD
	{
		QT_TRANSLATE_NOOP("Action", "Piece.GroupAdd"),
		QT_TRANSLATE_NOOP("Menu", "&Add to Group"),
		QT_TRANSLATE_NOOP("Status", "Add focused piece to selected group"),
		"",
		""
	},
	// SW_PIECE_GROUP_REMOVE
	{
		QT_TRANSLATE_NOOP("Action", "Piece.GroupRemove"),
		QT_TRANSLATE_NOOP("Menu", "Re&move from Group"),
		QT_TRANSLATE_NOOP("Status", "Remove focused piece from group"),
		"",
		""
	},
	// SW_PIECE_GROUP_EDIT
	{
		QT_TRANSLATE_NOOP("Action", "Piece.GroupEdit"),
		QT_TRANSLATE_NOOP("Menu", "&Edit Groups..."),
		QT_TRANSLATE_NOOP("Status", "Edit groups"),
		"",
		""
	},
	// SW_PIECE_HIDE_SELECTED
	{
		QT_TRANSLATE_NOOP("Action", "Piece.HideSelected"),
		QT_TRANSLATE_NOOP("Menu", "&Hide Selected"),
		QT_TRANSLATE_NOOP("Status", "Hide selected objects"),
		"",
		"Ctrl+H"
	},
	// SW_PIECE_HIDE_UNSELECTED
	{
		QT_TRANSLATE_NOOP("Action", "Piece.HideUnselected"),
		QT_TRANSLATE_NOOP("Menu", "Hide &Unselected"),
		QT_TRANSLATE_NOOP("Status", "Hide objects that are not selected"),
		"",
		""
	},
	// SW_PIECE_UNHIDE_SELECTED
	{
		QT_TRANSLATE_NOOP("Action", "Piece.UnhideSelected"),
		QT_TRANSLATE_NOOP("Menu", "&Unhide Selected"),
		QT_TRANSLATE_NOOP("Status", "Show hidden objects that are selected"),
		"",
		""
	},
	// SW_PIECE_UNHIDE_ALL
	{
		QT_TRANSLATE_NOOP("Action", "Piece.UnhideAll"),
		QT_TRANSLATE_NOOP("Menu", "U&nhide All"),
		QT_TRANSLATE_NOOP("Status", "Show all hidden objects"),
		"",
		""
	},
	// SW_PIECE_SHOW_EARLIER
	{
		QT_TRANSLATE_NOOP("Action", "Piece.ShowEarlier"),
		QT_TRANSLATE_NOOP("Menu", "Show Earlier"),
		QT_TRANSLATE_NOOP("Status", "Show selected pieces one step earlier"),
		"",
		""
	},
	// SW_PIECE_SHOW_LATER
	{
		QT_TRANSLATE_NOOP("Action", "Piece.ShowLater"),
		QT_TRANSLATE_NOOP("Menu", "Show Later"),
		QT_TRANSLATE_NOOP("Status", "Show selected pieces one step later"),
		"",
		""
	},
	// SW_MODEL_NEW
	{
		QT_TRANSLATE_NOOP("Action", "Model.New"),
		QT_TRANSLATE_NOOP("Menu", "New Submodel..."),
		QT_TRANSLATE_NOOP("Status", "Create a new submodel"),
		"",
		""
	},
	// SW_MODEL_PROPERTIES
	{
		QT_TRANSLATE_NOOP("Action", "Model.Properties"),
		QT_TRANSLATE_NOOP("Menu", "Proper&ties..."),
		QT_TRANSLATE_NOOP("Status", "Display the properties of the current submodel"),
		"",
		""
	},
	// SW_MODEL_LIST
	{
		QT_TRANSLATE_NOOP("Action", "Model.List"),
		QT_TRANSLATE_NOOP("Menu", "Submodels..."),
		QT_TRANSLATE_NOOP("Status", "Show a list of all submodels"),
		"",
		""
	},
	// SW_MODEL_01
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model01"),
		QT_TRANSLATE_NOOP("Menu", "Model 1"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_02
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model02"),
		QT_TRANSLATE_NOOP("Menu", "Model 2"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_03
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model03"),
		QT_TRANSLATE_NOOP("Menu", "Model 3"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_04
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model04"),
		QT_TRANSLATE_NOOP("Menu", "Model 4"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_05
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model05"),
		QT_TRANSLATE_NOOP("Menu", "Model 5"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_06
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model06"),
		QT_TRANSLATE_NOOP("Menu", "Model 6"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_07
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model07"),
		QT_TRANSLATE_NOOP("Menu", "Model 7"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_08
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model08"),
		QT_TRANSLATE_NOOP("Menu", "Model 8"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_09
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model09"),
		QT_TRANSLATE_NOOP("Menu", "Model 9"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_10
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model10"),
		QT_TRANSLATE_NOOP("Menu", "Model 10"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_11
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model11"),
		QT_TRANSLATE_NOOP("Menu", "Model 11"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_12
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model12"),
		QT_TRANSLATE_NOOP("Menu", "Model 12"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_13
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model13"),
		QT_TRANSLATE_NOOP("Menu", "Model 13"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_14
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model14"),
		QT_TRANSLATE_NOOP("Menu", "Model 14"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_15
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model15"),
		QT_TRANSLATE_NOOP("Menu", "Model 15"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_16
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model16"),
		QT_TRANSLATE_NOOP("Menu", "Model 16"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_17
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model17"),
		QT_TRANSLATE_NOOP("Menu", "Model 17"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_18
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model18"),
		QT_TRANSLATE_NOOP("Menu", "Model 18"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_19
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model19"),
		QT_TRANSLATE_NOOP("Menu", "Model 19"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_20
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model20"),
		QT_TRANSLATE_NOOP("Menu", "Model 20"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_21
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model21"),
		QT_TRANSLATE_NOOP("Menu", "Model 21"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_22
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model22"),
		QT_TRANSLATE_NOOP("Menu", "Model 22"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_23
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model23"),
		QT_TRANSLATE_NOOP("Menu", "Model 23"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_24
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model24"),
		QT_TRANSLATE_NOOP("Menu", "Model 24"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_25
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model25"),
		QT_TRANSLATE_NOOP("Menu", "Model 25"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_26
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model26"),
		QT_TRANSLATE_NOOP("Menu", "Model 26"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_27
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model27"),
		QT_TRANSLATE_NOOP("Menu", "Model 27"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_28
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model28"),
		QT_TRANSLATE_NOOP("Menu", "Model 28"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_29
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model29"),
		QT_TRANSLATE_NOOP("Menu", "Model 29"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_30
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model30"),
		QT_TRANSLATE_NOOP("Menu", "Model 30"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_31
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model31"),
		QT_TRANSLATE_NOOP("Menu", "Model 31"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_32
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model32"),
		QT_TRANSLATE_NOOP("Menu", "Model 32"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_33
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model33"),
		QT_TRANSLATE_NOOP("Menu", "Model 33"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_34
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model34"),
		QT_TRANSLATE_NOOP("Menu", "Model 34"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_35
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model35"),
		QT_TRANSLATE_NOOP("Menu", "Model 35"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_36
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model36"),
		QT_TRANSLATE_NOOP("Menu", "Model 36"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_37
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model37"),
		QT_TRANSLATE_NOOP("Menu", "Model 37"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_38
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model38"),
		QT_TRANSLATE_NOOP("Menu", "Model 38"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_39
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model39"),
		QT_TRANSLATE_NOOP("Menu", "Model 39"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_MODEL_40
	{
		QT_TRANSLATE_NOOP("Action", "Model.Model40"),
		QT_TRANSLATE_NOOP("Menu", "Model 40"),
		QT_TRANSLATE_NOOP("Status", "Switch to this submodel"),
		"",
		""
	},
	// SW_HELP_WHATS_THIS
	{
		QT_TRANSLATE_NOOP("Action", "Help.WhatsThis"),
		QT_TRANSLATE_NOOP("Menu", "&What's This?"),
		QT_TRANSLATE_NOOP("Status", "Click a control to read its contextual help"),
		"",
		"Shift+F1"
	},
	// SW_HELP_HOME_PAGE
	{
		QT_TRANSLATE_NOOP("Action", "Help.HomePage"),
		QT_TRANSLATE_NOOP("Menu", "&Home Page"),
		QT_TRANSLATE_NOOP("Status", "Open StudWorks' home page on the internet using your default web browser"),
		"",
		""
	},
	// SW_HELP_BUG_REPORT
	{
		QT_TRANSLATE_NOOP("Action", "Help.BugReport"),
		QT_TRANSLATE_NOOP("Menu", "Report a &Bug"),
		QT_TRANSLATE_NOOP("Status", "Open StudWorks' bug report form on your default web browser"),
		"",
		""
	},
	// SW_HELP_ABOUT
	{
		QT_TRANSLATE_NOOP("Action", "Help.About"),
		QT_TRANSLATE_NOOP("Menu", "&About..."),
		QT_TRANSLATE_NOOP("Status", "Display program version number and system information"),
		"",
		""
	},
	// SW_TIMELINE_INSERT_BEFORE
	{
		"",
		QT_TRANSLATE_NOOP("Menu", "Insert Step Before"),
		QT_TRANSLATE_NOOP("Status", "Insert a new step before the current step"),
		"",
		""
	},
	// SW_TIMELINE_INSERT_AFTER
	{
		"",
		QT_TRANSLATE_NOOP("Menu", "Insert Step After"),
		QT_TRANSLATE_NOOP("Status", "Insert a new step after the current step"),
		"",
		""
	},
	// SW_TIMELINE_DELETE
	{
		"",
		QT_TRANSLATE_NOOP("Menu", "Remove Step"),
		QT_TRANSLATE_NOOP("Status", "Remove current step"),
		"",
		""
	},
	// SW_TIMELINE_MOVE_SELECTION
	{
		"",
		QT_TRANSLATE_NOOP("Menu", "Move Selection Here"),
		QT_TRANSLATE_NOOP("Status", "Move the selected pieces into this step"),
		"",
		""
	},
	// SW_TIMELINE_MOVE_SELECTION_BEFORE
	{
		"",
		QT_TRANSLATE_NOOP("Menu", "Move Selection Before"),
		QT_TRANSLATE_NOOP("Status", "Move the selected pieces into a new step before this"),
		"",
		""
	},
	// SW_TIMELINE_MOVE_SELECTION_AFTER
	{
		"",
		QT_TRANSLATE_NOOP("Menu", "Move Selection After"),
		QT_TRANSLATE_NOOP("Status", "Move the selected pieces into a new step after this"),
		"",
		""
	},
	// SW_TIMELINE_SET_CURRENT
	{
		"",
		QT_TRANSLATE_NOOP("Menu", "Set Current Step"),
		QT_TRANSLATE_NOOP("Status", "View the model at this point in the timeline"),
		"",
		""
	}
};

SW_ARRAY_SIZE_CHECK(gCommands, SW_COMMAND_COUNT);

const char* gToolNames[] =
{
	QT_TRANSLATE_NOOP("Mouse", "NewPiece"),            // swTool::Insert
	QT_TRANSLATE_NOOP("Mouse", "NewPointLight"),       // swTool::PointLight
	QT_TRANSLATE_NOOP("Mouse", "NewSpotLight"),        // swTool::SpotLight
	QT_TRANSLATE_NOOP("Mouse", "NewDirectionalLight"), // swTool::DirectionalLight
	QT_TRANSLATE_NOOP("Mouse", "NewAreaLight"),        // swTool::AreaLight
	QT_TRANSLATE_NOOP("Mouse", "NewCamera"),           // swTool::Camera
	QT_TRANSLATE_NOOP("Mouse", "Select"),              // swTool::Select
	QT_TRANSLATE_NOOP("Mouse", "Move"),                // swTool::Move
	QT_TRANSLATE_NOOP("Mouse", "Rotate"),              // swTool::Rotate
	QT_TRANSLATE_NOOP("Mouse", "Delete"),              // swTool::Eraser
	QT_TRANSLATE_NOOP("Mouse", "Paint"),               // swTool::Paint
	QT_TRANSLATE_NOOP("Mouse", "ColorPicker"),         // swTool::ColorPicker
	QT_TRANSLATE_NOOP("Mouse", "Zoom"),                // swTool::Zoom
	QT_TRANSLATE_NOOP("Mouse", "Pan"),                 // swTool::Pan
	QT_TRANSLATE_NOOP("Mouse", "Orbit"),               // swTool::RotateView
	QT_TRANSLATE_NOOP("Mouse", "Roll"),                // swTool::Roll
	QT_TRANSLATE_NOOP("Mouse", "ZoomRegion")           // swTool::ZoomRegion
};

SW_ARRAY_SIZE_CHECK(gToolNames, swTool::Count);
