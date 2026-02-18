#include "ShapeGraphEditorCommands.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/InputChord.h"

#define LOCTEXT_NAMESPACE "ShapeTools_ShapeGraphEditorCommands"

FShapeGraphEditorCommands::FShapeGraphEditorCommands()
	: TCommands<FShapeGraphEditorCommands>(
		TEXT("ShapeGraphEditor"),
		LOCTEXT("ShapeGraphEditorCommands", "Shape Graph Editor"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FShapeGraphEditorCommands::RegisterCommands()
{
	// Blender-like defaults (tweak later)
	UI_COMMAND(FrameView, "Frame", "Frame view / reset view", EUserInterfaceActionType::Button, FInputChord(EKeys::A));
	UI_COMMAND(ToggleBackgroundLock, "Lock Background", "Toggle background pan/zoom lock", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::L));
	UI_COMMAND(ResetBackground, "Reset Background", "Reset background offset/scale", EUserInterfaceActionType::Button, FInputChord(EKeys::R));

	UI_COMMAND(ToggleClosed, "Toggle Closed", "Toggle contour closed/open", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::C));
	UI_COMMAND(DeleteSelection, "Delete", "Delete selected point", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));

	// Standard UE chords (works everywhere)
	UI_COMMAND(Undo, "Undo", "Undo last action", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Z));
	UI_COMMAND(Redo, "Redo", "Redo last undone action", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Y));

	UI_COMMAND(ShowShortcuts, "Shortcuts", "Show shortcuts list", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE