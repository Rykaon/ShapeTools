#include "ShapeGraphAssetEditorToolkit.h"

#include "ShapeGraphAsset.h"
#include "SShapeGraphEditorCanvas.h"
#include "ShapeGraphEditorCommands.h"
#include "ShapeToolsEditorStyle.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"

#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#include "Editor.h"
#include "ScopedTransaction.h"

struct FPropertyChangedEvent;

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

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bLockable = false;
	Args.bUpdatesFromSelection = false;
	Args.NotifyHook = nullptr;

	DetailsView = PropertyEditorModule.CreateDetailView(Args);
	DetailsView->SetObject(InAsset);

	// Bind changement de propriété (dans Details)
	if (!DetailsChangedHandle.IsValid())
	{
		DetailsChangedHandle = DetailsView->OnFinishedChangingProperties().AddRaw(
			this,
			&FShapeGraphAssetEditorToolkit::OnDetailsFinishedChangingProperties
		);
	}

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

void FShapeGraphAssetEditorToolkit::OnClose()
{
	if (DetailsView.IsValid() && DetailsChangedHandle.IsValid())
	{
		DetailsView->OnFinishedChangingProperties().Remove(DetailsChangedHandle);
		DetailsChangedHandle.Reset();
	}

	DetailsView.Reset();
	CanvasWidget.Reset();
	EditingAsset = nullptr;

	FAssetEditorToolkit::OnClose();
}

void FShapeGraphAssetEditorToolkit::OnAnyObjectPropertyChanged(
	UObject* ObjectBeingModified,
	FPropertyChangedEvent& Event)
{
	if (!EditingAsset) return;
	if (ObjectBeingModified != EditingAsset.Get()) return;
	if (!CanvasWidget.IsValid()) return;
	if (!Event.Property) return;

	CanvasWidget->HandleAssetPropertyChanged(Event.Property->GetFName());
}

void FShapeGraphAssetEditorToolkit::OnDetailsFinishedChangingProperties(const FPropertyChangedEvent& Event)
{
	if (!EditingAsset) return;
	if (!CanvasWidget.IsValid()) return;

	if (Event.Property)
	{
		CanvasWidget->HandleAssetPropertyChanged(Event.Property->GetFName());
	}
	else
	{
		// fallback: refresh général
		CanvasWidget->HandleAssetPropertyChanged(NAME_None);
	}
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
	FMenuBuilder MenuBuilder(true, ToolkitCommands);

	// ---- Helpers ----
	auto AddRow = [](FMenuBuilder& MenuBuilder, const FText& Label, const FText& Chord, const FText& Tooltip)
		{
			const FSlateFontInfo LabelFont = FAppStyle::GetFontStyle("NormalFont");
			const FSlateFontInfo ChordFont = FAppStyle::GetFontStyle("SmallFont"); // plus discret

			MenuBuilder.AddWidget(
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.65f)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 2.f)) // respiration
				[
					SNew(STextBlock)
						.Text(Label)
						.Font(LabelFont)
						.ColorAndOpacity(FSlateColor::UseForeground())
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 2.f)) // respiration
				[
					SNew(STextBlock)
						.Text(Chord.IsEmpty() ? FText::FromString(TEXT("-")) : Chord)
						.Font(ChordFont)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				],
				Tooltip
			);
		};

	auto AddCommandRow = [&AddRow](FMenuBuilder& MenuBuilder, const TSharedPtr<FUICommandInfo>& Cmd)
		{
			if (!Cmd.IsValid()) return;

			AddRow(
				MenuBuilder,
				Cmd->GetLabel(),
				Cmd->GetInputText(),
				Cmd->GetDescription()
			);
		};
	// ------------------------------

	// Navigation
	MenuBuilder.BeginSection("ShapeToolsNav", NSLOCTEXT("ShapeTools", "NavHeader", "Navigation"));
	{
		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Nav_Pan", "Pan view"),
			NSLOCTEXT("ShapeTools", "Nav_PanChord", "RMB + Drag"),
			NSLOCTEXT("ShapeTools", "Nav_PanTT", "Pan the view (camera).")
		);

		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Nav_Zoom", "Zoom"),
			NSLOCTEXT("ShapeTools", "Nav_ZoomChord", "Mouse Wheel"),
			NSLOCTEXT("ShapeTools", "Nav_ZoomTT", "Zoom in/out centered on cursor.")
		);

		AddCommandRow(MenuBuilder, Cmds.FrameView); // A
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	// Selection
	MenuBuilder.BeginSection("ShapeToolsSel", NSLOCTEXT("ShapeTools", "SelHeader", "Selection"));
	{
		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Sel_ClickVertex", "Select vertex"),
			NSLOCTEXT("ShapeTools", "Sel_ClickVertexChord", "LMB on vertex"),
			NSLOCTEXT("ShapeTools", "Sel_ClickVertexTT", "Select a vertex.")
		);

		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Sel_ToggleVertex", "Toggle vertex in selection"),
			NSLOCTEXT("ShapeTools", "Sel_ToggleVertexChord", "Shift + LMB"),
			NSLOCTEXT("ShapeTools", "Sel_ToggleVertexTT", "Add/remove vertex from selection.")
		);

		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Sel_Clear", "Clear selection"),
			NSLOCTEXT("ShapeTools", "Sel_ClearChord", "LMB on empty"),
			NSLOCTEXT("ShapeTools", "Sel_ClearTT", "Clear current selection.")
		);

		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Sel_Box", "Box select"),
			NSLOCTEXT("ShapeTools", "Sel_BoxChord", "LMB + Drag"),
			NSLOCTEXT("ShapeTools", "Sel_BoxTT", "Rectangle selection.")
		);

		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Sel_BoxAdd", "Additive box select"),
			NSLOCTEXT("ShapeTools", "Sel_BoxAddChord", "Shift + LMB + Drag"),
			NSLOCTEXT("ShapeTools", "Sel_BoxAddTT", "Add vertices to current selection.")
		);

		MenuBuilder.AddMenuSeparator();

		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Sel_Bg", "Select background"),
			NSLOCTEXT("ShapeTools", "Sel_BgChord", "LMB on background"),
			NSLOCTEXT("ShapeTools", "Sel_BgTT", "Select background (if unlocked).")
		);

		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Sel_BgToggle", "Toggle background selection"),
			NSLOCTEXT("ShapeTools", "Sel_BgToggleChord", "Shift + LMB"),
			NSLOCTEXT("ShapeTools", "Sel_BgToggleTT", "Toggle background selection.")
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	// Edition
	MenuBuilder.BeginSection("ShapeToolsEdit", NSLOCTEXT("ShapeTools", "EditHeader", "Edition"));
	{
		AddRow(
			MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_Insert", "Insert vertex"),
			NSLOCTEXT("ShapeTools", "Edit_InsertChord", "Ctrl + LMB near segment"),
			NSLOCTEXT("ShapeTools", "Edit_InsertTT", "Insert a vertex on the nearest segment.")
		);

		AddCommandRow(MenuBuilder, Cmds.DeleteSelection); // Delete
		AddCommandRow(MenuBuilder, Cmds.ToggleClosed);    // C

		MenuBuilder.AddMenuSeparator();

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_G", "Move (modal)"),
			NSLOCTEXT("ShapeTools", "Edit_GChord", "G"),
			NSLOCTEXT("ShapeTools", "Edit_GTT", "Start Move mode (confirm with LMB/Enter, cancel with RMB/Escape).")
		);

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_S", "Scale (modal)"),
			NSLOCTEXT("ShapeTools", "Edit_SChord", "S"),
			NSLOCTEXT("ShapeTools", "Edit_STT", "Start Scale mode (uniform). Shift = step scale.")
		);

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_R", "Rotate (modal)"),
			NSLOCTEXT("ShapeTools", "Edit_RChord", "R"),
			NSLOCTEXT("ShapeTools", "Edit_RTT", "Start Rotate mode. Shift = angle snap (5°).")
		);

		MenuBuilder.AddMenuSeparator();

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_Confirm", "Confirm modal transform"),
			NSLOCTEXT("ShapeTools", "Edit_ConfirmChord", "LMB / Enter"),
			NSLOCTEXT("ShapeTools", "Edit_ConfirmTT", "Confirm current modal transform.")
		);

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_Cancel", "Cancel modal transform"),
			NSLOCTEXT("ShapeTools", "Edit_CancelChord", "RMB / Escape"),
			NSLOCTEXT("ShapeTools", "Edit_CancelTT", "Cancel current modal transform.")
		);

		MenuBuilder.AddMenuSeparator();

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_SnapMove", "Snap (Move)"),
			NSLOCTEXT("ShapeTools", "Edit_SnapMoveChord", "Ctrl"),
			NSLOCTEXT("ShapeTools", "Edit_SnapMoveTT", "Snap pivot to active grid while moving.")
		);

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_SnapScale", "Step (Scale)"),
			NSLOCTEXT("ShapeTools", "Edit_SnapScaleChord", "Shift"),
			NSLOCTEXT("ShapeTools", "Edit_SnapScaleTT", "Discrete step scaling while scaling.")
		);

		AddRow(MenuBuilder,
			NSLOCTEXT("ShapeTools", "Edit_SnapRotate", "Angle snap (Rotate)"),
			NSLOCTEXT("ShapeTools", "Edit_SnapRotateChord", "Shift"),
			NSLOCTEXT("ShapeTools", "Edit_SnapRotateTT", "Snap rotation angle to 5° steps.")
		);

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
					auto GetGridLabel = [this]() -> FText
						{
							const UShapeGraphAsset* A = EditingAsset.Get();
							if (!A) return FText::FromString(TEXT("Grid"));

							return (A->GridSpace == EShapeGridSpace::Design)
								? FText::FromString(TEXT("PX"))
								: FText::FromString(TEXT("UV"));
						};

					auto GetGridTooltip = [this]() -> FText
						{
							const UShapeGraphAsset* A = EditingAsset.Get();
							if (!A) return FText::FromString(TEXT("Toggle grid space"));

							const float Step = (A->GridSpace == EShapeGridSpace::Design) ? A->GridStepDesignPx : A->GridStepUV;

							return (A->GridSpace == EShapeGridSpace::Design)
								? FText::Format(NSLOCTEXT("ShapeTools", "GridTooltipPX", "Grid space: PX\nStep: {0} px\nClick to switch to UV"),
									FText::AsNumber((int32)Step))
								: FText::Format(NSLOCTEXT("ShapeTools", "GridTooltipUV", "Grid space: UV\nStep: {0}\nClick to switch to PX"),
									FText::AsNumber(Step));
						};

					ToolbarBuilder.AddWidget(
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ContentPadding(FMargin(6, 2))
						.ToolTipText_Lambda(GetGridTooltip)
						.OnClicked_Lambda([this]()
							{
								if (UShapeGraphAsset* A = EditingAsset.Get())
								{
									const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "ToggleGridSpaceTx", "Toggle Grid Space"));
									A->Modify();

									A->GridSpace = (A->GridSpace == EShapeGridSpace::Design) ? EShapeGridSpace::UV : EShapeGridSpace::Design;

									A->PostEditChange();
									A->MarkPackageDirty();

									if (CanvasWidget.IsValid())
									{
										CanvasWidget->Invalidate(EInvalidateWidget::Paint);
									}
								}
								return FReply::Handled();
							})
						[
							SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									// Icône "grid" (si elle n'existe pas dans ton build, elle s'affichera vide sans casser)
									SNew(SImage)
										.Image(FShapeToolsEditorStyle::Get().GetBrush("ShapeTools.Icons.Grid"))
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text_Lambda(GetGridLabel) // affiche PX / UV
								]
						]
					);

					auto GetStepTooltip = [this]() -> FText
						{
							const UShapeGraphAsset* A = EditingAsset.Get();
							if (!A) return FText::FromString(TEXT("Grid step"));

							const float Step = (A->GridSpace == EShapeGridSpace::Design) ? A->GridStepDesignPx : A->GridStepUV;

							return (A->GridSpace == EShapeGridSpace::Design)
								? FText::Format(NSLOCTEXT("ShapeTools", "StepTooltipPX", "Grid step: {0} px\nOpen presets/custom"),
									FText::AsNumber((int32)Step))
								: FText::Format(NSLOCTEXT("ShapeTools", "StepTooltipUV", "Grid step: {0}\nOpen presets/custom"),
									FText::AsNumber(Step));
						};

					ToolbarBuilder.AddWidget(
						SNew(SComboButton)
						.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.HasDownArrow(true)
						.ContentPadding(FMargin(6, 2))
						.ToolTipText_Lambda(GetStepTooltip)
						.ButtonContent()
						[
							SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									// Icône "menu/options"
									SNew(SImage)
										.Image(FAppStyle::GetBrush("Icons.Settings"))
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text_Lambda([this]()
											{
												const UShapeGraphAsset* A = EditingAsset.Get();
												if (!A) return FText::FromString(TEXT("Step"));

												const float Step = (A->GridSpace == EShapeGridSpace::Design) ? A->GridStepDesignPx : A->GridStepUV;
												return (A->GridSpace == EShapeGridSpace::Design)
													? FText::Format(NSLOCTEXT("ShapeTools", "StepLabelPX", "{0}px"), FText::AsNumber((int32)Step))
													: FText::Format(NSLOCTEXT("ShapeTools", "StepLabelUV", "{0}"), FText::AsNumber(Step));
											})
								]
						]
						.OnGetMenuContent_Lambda([this]()
							{
								FMenuBuilder Menu(true, nullptr);

								auto ApplyStep = [this](float NewStep)
									{
										if (UShapeGraphAsset* A = EditingAsset.Get())
										{
											const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "GridStepTx", "Change Grid Step"));
											A->Modify();

											if (A->GridSpace == EShapeGridSpace::Design)
											{
												A->GridStepDesignPx = FMath::Max(NewStep, 1.f);
											}
											else
											{
												A->GridStepUV = FMath::Clamp(NewStep, 0.0001f, 1.f);
											}

											A->PostEditChange();
											A->MarkPackageDirty();

											if (CanvasWidget.IsValid())
											{
												CanvasWidget->Invalidate(EInvalidateWidget::Paint);
											}
										}
									};

								Menu.BeginSection("GridStepPresets", NSLOCTEXT("ShapeTools", "GridStepPresets", "Presets"));
								{
									const UShapeGraphAsset* A = EditingAsset.Get();
									const bool bPX = A && (A->GridSpace == EShapeGridSpace::Design);

									const TArray<float> PresetsPX = { 1.f, 5.f, 10.f, 25.f, 50.f, 100.f };
									const TArray<float> PresetsUV = { 0.001f, 0.005f, 0.01f, 0.02f, 0.05f };
									const TArray<float>& Presets = bPX ? PresetsPX : PresetsUV;

									for (float P : Presets)
									{
										Menu.AddMenuEntry(
											bPX
											? FText::Format(NSLOCTEXT("ShapeTools", "PresetPX", "{0} px"), FText::AsNumber((int32)P))
											: FText::AsNumber(P),
											FText(),
											FSlateIcon(),
											FUIAction(FExecuteAction::CreateLambda([ApplyStep, P]() { ApplyStep(P); }))
										);
									}
								}
								Menu.EndSection();

								Menu.BeginSection("GridStepCustom", NSLOCTEXT("ShapeTools", "GridStepCustom", "Custom"));
								{
									Menu.AddWidget(
										SNew(SNumericEntryBox<float>)
										.MinValue(0.0001f)
										.MaxValue(100000.f)
										.Value_Lambda([this]() -> TOptional<float>
											{
												const UShapeGraphAsset* A = EditingAsset.Get();
												if (!A) return TOptional<float>();

												return (A->GridSpace == EShapeGridSpace::Design) ? A->GridStepDesignPx : A->GridStepUV;
											})
										.OnValueCommitted_Lambda([ApplyStep](float NewValue, ETextCommit::Type)
											{
												ApplyStep(NewValue);
											}),
										NSLOCTEXT("ShapeTools", "StepEntry", "Step")
									);
								}
								Menu.EndSection();

								return Menu.MakeWidget();
							})
					);

					auto GetPivotLabel = [this]() -> FText
						{
							if (UShapeGraphAsset* A = EditingAsset.Get())
							{
								switch (A->PivotMode)
								{
								case EShapePivotMode::MedianPoint:       return FText::FromString(TEXT("Pivot: Median"));
								case EShapePivotMode::BoundingBoxCenter: return FText::FromString(TEXT("Pivot: BBox"));
								default: break;
								}
							}
							return FText::FromString(TEXT("Pivot"));
						};

					auto GetPivotTooltip = []() -> FText
						{
							return NSLOCTEXT("ShapeTools", "PivotModeTooltip",
								"Pivot Mode used for Move/Scale/Rotate transforms.\n"
								"Median: average of selected vertices.\n"
								"BBox: center of selection bounds.");
						};

					auto MakePivotMenu = [this]() -> TSharedRef<SWidget>
						{
							FMenuBuilder MenuBuilder(true, nullptr);

							auto SetPivotMode = [this](EShapePivotMode NewMode)
								{
									if (UShapeGraphAsset* A = EditingAsset.Get())
									{
										const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "SetPivotModeTx", "Set Pivot Mode"));
										A->Modify();

										A->PivotMode = NewMode;

										A->PostEditChange();
										A->MarkPackageDirty();

										if (CanvasWidget.IsValid())
										{
											CanvasWidget->Invalidate(EInvalidateWidget::Paint);
										}
									}
								};

							auto IsPivotMode = [this](EShapePivotMode Mode) -> bool
								{
									if (UShapeGraphAsset* A = EditingAsset.Get())
									{
										return A->PivotMode == Mode;
									}
									return false;
								};

							MenuBuilder.AddMenuEntry(
								NSLOCTEXT("ShapeTools", "PivotMedian", "Median Point"),
								NSLOCTEXT("ShapeTools", "PivotMedian_TT", "Pivot = average of selected vertices."),
								FSlateIcon(),
								FUIAction(
									FExecuteAction::CreateLambda([SetPivotMode]() { SetPivotMode(EShapePivotMode::MedianPoint); }),
									FCanExecuteAction(),
									FIsActionChecked::CreateLambda([IsPivotMode]() { return IsPivotMode(EShapePivotMode::MedianPoint); })
								),
								NAME_None,
								EUserInterfaceActionType::RadioButton
							);

							MenuBuilder.AddMenuEntry(
								NSLOCTEXT("ShapeTools", "PivotBBox", "Bounding Box Center"),
								NSLOCTEXT("ShapeTools", "PivotBBox_TT", "Pivot = center of the selection bounds."),
								FSlateIcon(),
								FUIAction(
									FExecuteAction::CreateLambda([SetPivotMode]() { SetPivotMode(EShapePivotMode::BoundingBoxCenter); }),
									FCanExecuteAction(),
									FIsActionChecked::CreateLambda([IsPivotMode]() { return IsPivotMode(EShapePivotMode::BoundingBoxCenter); })
								),
								NAME_None,
								EUserInterfaceActionType::RadioButton
							);

							return MenuBuilder.MakeWidget();
						};

					ToolbarBuilder.AddWidget(
						SNew(SComboButton)
						.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ContentPadding(FMargin(6, 2))
						.ToolTipText_Lambda(GetPivotTooltip)
						.OnGetMenuContent_Lambda(MakePivotMenu)
						.ButtonContent()
						[
							SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SImage)
										.Image(FShapeToolsEditorStyle::Get().GetBrush("ShapeTools.Icons.Pivot"))
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text_Lambda(GetPivotLabel)
								]
						]
					);

					ToolbarBuilder.AddSeparator();

					ToolbarBuilder.AddWidget(
						SNew(SComboButton)
						.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.HasDownArrow(true)
						.ContentPadding(FMargin(6, 2))
						.ToolTipText(NSLOCTEXT("ShapeTools", "ShortcutsTooltip", "Show shortcuts"))
						.ButtonContent()
						[
							SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SImage)
										.Image(FShapeToolsEditorStyle::Get().GetBrush("ShapeTools.Icons.Shortcuts"))
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(6, 0, 0, 0)
								[
									// Optionnel : petit label comme tes autres boutons
									SNew(STextBlock)
										.Text(FText::FromString(TEXT("Keys")))
								]
						]
						.OnGetMenuContent_Lambda([this]()
							{
								return BuildShortcutsMenuWidget();
							})
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
	if (CanvasWidget.IsValid())
	{
		CanvasWidget->FrameViewToShape();
	}
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

	if (EditingAsset->bBackgroundLocked)
	{
		if (CanvasWidget.IsValid())
		{
			CanvasWidget->Invalidate(EInvalidateWidget::Paint);
		}
	}
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