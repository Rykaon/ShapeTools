#include "ShapeGraphAssetEditorToolkit.h"

#include "ShapeGraphAsset.h"
#include "SShapeGraphEditorCanvas.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"

const FName FShapeGraphAssetEditorToolkit::TabId_Canvas(TEXT("ShapeGraph_Canvas"));
const FName FShapeGraphAssetEditorToolkit::TabId_Details(TEXT("ShapeGraph_Details"));

void FShapeGraphAssetEditorToolkit::OpenEditor(const TArray<UObject*>& InObjects)
{
	for (UObject* Obj : InObjects)
	{
		if (UShapeGraphAsset* Asset = Cast<UShapeGraphAsset>(Obj))
		{
			TSharedRef<FShapeGraphAssetEditorToolkit> Editor = MakeShared<FShapeGraphAssetEditorToolkit>();
			Editor->InitEditor(Asset);
		}
	}
}

void FShapeGraphAssetEditorToolkit::InitEditor(UShapeGraphAsset* InAsset)
{
	EditingAsset = InAsset;

	// Details panel
	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bAllowSearch = true;

	DetailsView = PropModule.CreateDetailView(Args);
	DetailsView->SetObject(EditingAsset);

	// Tabs
	const TSharedRef<FTabManager::FLayout> Layout =
		FTabManager::NewLayout("ShapeGraphAssetEditor_Layout_v0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(TabId_Canvas, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(TabId_Details, ETabState::OpenedTab)
				->SetHideTabWell(true)
				->SetSizeCoefficient(0.30f)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	InitAssetEditor(
		EToolkitMode::Standalone,
		TSharedPtr<IToolkitHost>(),
		GetToolkitFName(),
		Layout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		EditingAsset
	);

	RegenerateMenusAndToolbars();
}

FName FShapeGraphAssetEditorToolkit::GetToolkitFName() const
{
	return FName("ShapeGraphAssetEditor");
}

FText FShapeGraphAssetEditorToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("ShapeTools", "ShapeGraphAssetEditorName", "Shape Graph Editor");
}

FString FShapeGraphAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("ShapeGraph");
}

FLinearColor FShapeGraphAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);
}

void FShapeGraphAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EditingAsset);
}

TSharedRef<SDockTab> FShapeGraphAssetEditorToolkit::SpawnTab_Canvas(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SNew(SShapeGraphEditorCanvas)
				.ShapeAsset(EditingAsset)
		];
}

TSharedRef<SDockTab> FShapeGraphAssetEditorToolkit::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			DetailsView.ToSharedRef()
		];
}

void FShapeGraphAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(TabId_Canvas, FOnSpawnTab::CreateSP(this, &FShapeGraphAssetEditorToolkit::SpawnTab_Canvas))
		.SetDisplayName(NSLOCTEXT("ShapeTools", "CanvasTab", "Canvas"));

	InTabManager->RegisterTabSpawner(TabId_Details, FOnSpawnTab::CreateSP(this, &FShapeGraphAssetEditorToolkit::SpawnTab_Details))
		.SetDisplayName(NSLOCTEXT("ShapeTools", "DetailsTab", "Details"));
}

void FShapeGraphAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(TabId_Canvas);
	InTabManager->UnregisterTabSpawner(TabId_Details);

	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}