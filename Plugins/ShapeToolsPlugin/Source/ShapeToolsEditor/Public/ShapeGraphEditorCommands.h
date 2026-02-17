#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * Command set for the Shape Graph Asset Editor (Blender-like shortcuts).
 * Lives in the Editor module.
 */
class SHAPETOOLSEDITOR_API FShapeGraphEditorCommands : public TCommands<FShapeGraphEditorCommands>
{
public:
	FShapeGraphEditorCommands();

	virtual void RegisterCommands() override;

public:
	// View
	TSharedPtr<FUICommandInfo> FrameView;
	TSharedPtr<FUICommandInfo> ToggleBackgroundLock;
	TSharedPtr<FUICommandInfo> ResetBackground;

	// Edit
	TSharedPtr<FUICommandInfo> ToggleClosed;
	TSharedPtr<FUICommandInfo> DeleteSelection;

	// Undo/Redo
	TSharedPtr<FUICommandInfo> Undo;
	TSharedPtr<FUICommandInfo> Redo;

	// Help
	TSharedPtr<FUICommandInfo> ShowShortcuts;
};