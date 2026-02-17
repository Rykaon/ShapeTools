#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"

class UShapeGraphAsset;
class IDetailsView;
class SShapeGraphEditorCanvas;

class SHAPETOOLSEDITOR_API FShapeGraphAssetEditorToolkit
	: public FAssetEditorToolkit
	, public FGCObject
{
public:
	static void OpenEditor(const TArray<UObject*>& InObjects);

	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FShapeGraphAssetEditorToolkit"); }

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

private:
	void InitEditor(UShapeGraphAsset* InAsset);

	TSharedRef<SDockTab> SpawnTab_Canvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

	// Commands / Toolbar
	void BindCommands();
	void ExtendToolbar();

	TSharedRef<SWidget> BuildShortcutsMenuWidget() const;

	void Command_Undo();
	void Command_Redo();
	void Command_Delete();
	void Command_Frame();
	void Command_ToggleClosed();
	void Command_ToggleBgLock();
	void Command_ResetBg();

	bool CanDelete() const;

private:
	TObjectPtr<UShapeGraphAsset> EditingAsset = nullptr;
	TSharedPtr<IDetailsView> DetailsView;

	TSharedPtr<FUICommandList> ToolkitCommands;
	TSharedPtr<SShapeGraphEditorCanvas> CanvasWidget;

	static const FName TabId_Canvas;
	static const FName TabId_Details;
};