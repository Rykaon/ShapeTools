#include "ShapeGraphAssetEditorToolkit.h"

#include "ShapeGraphAsset.h"
#include "SShapeGraphEditorCanvas.h"
#include "ShapeGraphEditorCommands.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"

#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

#include "Editor.h"
#include "ScopedTransaction.h"

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
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bLockable = false;
	DetailsArgs.bUpdatesFromSelection = false;

	DetailsView = PropModule.CreateDetailView(DetailsArgs);
	DetailsView->SetObject(EditingAsset);

	// Canvas
	CanvasWidget = SNew(SShapeGraphEditorCanvas)
		.ShapeAsset(EditingAsset);

	// Commands
	BindCommands();
	CanvasWidget->SetCommandList(ToolkitCommands);

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

	ExtendToolbar();
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
	check(CanvasWidget.IsValid());

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			CanvasWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FShapeGraphAssetEditorToolkit::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	check(DetailsView.IsValid());

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
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

void FShapeGraphAssetEditorToolkit::BindCommands()
{
	FShapeGraphEditorCommands::Register();
	const FShapeGraphEditorCommands& Cmds = FShapeGraphEditorCommands::Get();

	ToolkitCommands = GetToolkitCommands();

	ToolkitCommands->MapAction(
		Cmds.Undo,
		FExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::Command_Undo)
	);

	ToolkitCommands->MapAction(
		Cmds.Redo,
		FExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::Command_Redo)
	);

	ToolkitCommands->MapAction(
		Cmds.DeleteSelection,
		FExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::Command_Delete),
		FCanExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::CanDelete)
	);

	ToolkitCommands->MapAction(
		Cmds.FrameView,
		FExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::Command_Frame)
	);

	ToolkitCommands->MapAction(
		Cmds.ToggleClosed,
		FExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::Command_ToggleClosed),
		FCanExecuteAction::CreateLambda([this] { return EditingAsset != nullptr; }),
		FIsActionChecked::CreateLambda([this] { return EditingAsset && EditingAsset->bClosed; })
	);

	ToolkitCommands->MapAction(
		Cmds.ToggleBackgroundLock,
		FExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::Command_ToggleBgLock),
		FCanExecuteAction::CreateLambda([this] { return EditingAsset != nullptr; }),
		FIsActionChecked::CreateLambda([this] { return EditingAsset && EditingAsset->bBackgroundLocked; })
	);

	ToolkitCommands->MapAction(
		Cmds.ResetBackground,
		FExecuteAction::CreateSP(this, &FShapeGraphAssetEditorToolkit::Command_ResetBg)
	);
}

TSharedRef<SWidget> FShapeGraphAssetEditorToolkit::BuildShortcutsMenuWidget() const
{
	const FShapeGraphEditorCommands& Cmds = FShapeGraphEditorCommands::Get();

	auto AddCommandRow = [](FMenuBuilder& MenuBuilder, const TSharedPtr<FUICommandInfo>& Cmd)
		{
			if (!Cmd.IsValid()) return;

			const FText Label = Cmd->GetLabel();
			const FText Desc = Cmd->GetDescription();
			const FText Chord = Cmd->GetInputText();

			MenuBuilder.AddWidget(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.65f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Label)
				]
				+ SHorizontalBox::Slot().FillWidth(0.35f).HAlign(HAlign_Right).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Chord.IsEmpty() ? FText::FromString(TEXT("-")) : Chord)
				],
				Desc
			);
		};

	FMenuBuilder MenuBuilder(true, ToolkitCommands);

	MenuBuilder.BeginSection("ShapeToolsMouse", NSLOCTEXT("ShapeTools", "MouseHeader", "Mouse"));
	{
		MenuBuilder.AddWidget(
			SNew(STextBlock).Text(NSLOCTEXT("ShapeTools", "Mouse_SelectDrag", "LMB on point: Select + Drag")),
			FText()
		);

		MenuBuilder.AddWidget(
			SNew(STextBlock).Text(NSLOCTEXT("ShapeTools", "Mouse_Insert", "Ctrl + LMB near segment: Insert point")),
			FText()
		);

		MenuBuilder.AddWidget(
			SNew(STextBlock).Text(NSLOCTEXT("ShapeTools", "Mouse_Pan", "MMB Drag: Pan (background view)")),
			FText()
		);

		MenuBuilder.AddWidget(
			SNew(STextBlock).Text(NSLOCTEXT("ShapeTools", "Mouse_Zoom", "Mouse Wheel: Zoom (background view)")),
			FText()
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.BeginSection("ShapeToolsShortcuts", NSLOCTEXT("ShapeTools", "ShortcutsHeader", "Keyboard"));
	{
		AddCommandRow(MenuBuilder, Cmds.FrameView);
		AddCommandRow(MenuBuilder, Cmds.ToggleBackgroundLock);
		AddCommandRow(MenuBuilder, Cmds.ResetBackground);

		MenuBuilder.AddMenuSeparator();

		AddCommandRow(MenuBuilder, Cmds.ToggleClosed);
		AddCommandRow(MenuBuilder, Cmds.DeleteSelection);

		MenuBuilder.AddMenuSeparator();

		AddCommandRow(MenuBuilder, Cmds.Undo);
		AddCommandRow(MenuBuilder, Cmds.Redo);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FShapeGraphAssetEditorToolkit::ExtendToolbar()
{
	TSharedPtr<FExtender> Extender = MakeShared<FExtender>();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		ToolkitCommands,
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
			{
				const FShapeGraphEditorCommands& Cmds = FShapeGraphEditorCommands::Get();

				ToolbarBuilder.BeginSection("ShapeTools");
				{
					ToolbarBuilder.AddToolBarButton(Cmds.FrameView);
					ToolbarBuilder.AddToolBarButton(Cmds.ToggleClosed);

					ToolbarBuilder.AddSeparator();

					ToolbarBuilder.AddWidget(
						SNew(SComboButton)
						.ToolTipText(NSLOCTEXT("ShapeTools", "ShortcutsTooltip", "Show shortcuts"))
						.HasDownArrow(false)
						.ButtonContent()
						[
							SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Menu"))
						]
						.MenuContent()
						[
							BuildShortcutsMenuWidget()
						]
					);
				}
				ToolbarBuilder.EndSection();
			})
	);

	AddToolbarExtender(Extender);
}

void FShapeGraphAssetEditorToolkit::Command_Undo()
{
	if (GEditor) GEditor->UndoTransaction();
}

void FShapeGraphAssetEditorToolkit::Command_Redo()
{
	if (GEditor) GEditor->RedoTransaction();
}

bool FShapeGraphAssetEditorToolkit::CanDelete() const
{
	return CanvasWidget.IsValid() && CanvasWidget->HasSelection();
}

void FShapeGraphAssetEditorToolkit::Command_Delete()
{
	if (CanvasWidget.IsValid())
	{
		CanvasWidget->DeleteSelection();
	}
}

void FShapeGraphAssetEditorToolkit::Command_Frame()
{
	// TODO: later, frame background/viewport. For now we can no-op.
}

void FShapeGraphAssetEditorToolkit::Command_ToggleClosed()
{
	if (!EditingAsset) return;

	const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "ToggleClosedTx", "Toggle Closed"));
	EditingAsset->Modify();
	EditingAsset->bClosed = !EditingAsset->bClosed;
	EditingAsset->PostEditChange();
	EditingAsset->MarkPackageDirty();
}

void FShapeGraphAssetEditorToolkit::Command_ToggleBgLock()
{
	if (!EditingAsset) return;

	const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "ToggleBgLockTx", "Toggle Background Lock"));
	EditingAsset->Modify();
	EditingAsset->bBackgroundLocked = !EditingAsset->bBackgroundLocked;
	EditingAsset->PostEditChange();
	EditingAsset->MarkPackageDirty();
}

void FShapeGraphAssetEditorToolkit::Command_ResetBg()
{
	if (!EditingAsset) return;

	const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "ResetBgTx", "Reset Background"));
	EditingAsset->Modify();
	EditingAsset->BackgroundOffsetPx = FVector2D::ZeroVector;
	EditingAsset->BackgroundScale = 1.0f;
	EditingAsset->PostEditChange();
	EditingAsset->MarkPackageDirty();
}