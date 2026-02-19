#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UShapeGraphAsset;
class FUICommandList;
class FScopedTransaction;
class UTexture2D;

enum class EShapeTransformMode : uint8
{
	None,
	Translate,
	Scale,
	Rotate
};

struct FShapeSelection
{
	TSet<int32> Vertices;
	bool bBackgroundSelected = false;

	void Clear()
	{
		Vertices.Reset();
		bBackgroundSelected = false;
	}

	bool IsEmpty() const { return Vertices.Num() == 0 && !bBackgroundSelected; }
	bool HasVertices() const { return Vertices.Num() > 0; }
};

struct FShapeTransformSession
{
	EShapeTransformMode Mode = EShapeTransformMode::None;
	bool bActive = false;

	// Snapshot (Design space)
	TMap<int32, FVector2D> InitialVertexDesignPositions;

	// Background snapshot (Design space)
	bool bInitialBackgroundSelected = false;
	FVector2D InitialBackgroundOffsetDesign = FVector2D::ZeroVector;
	float InitialBackgroundScale = 1.f;
	float InitialBackgroundRotationDeg = 0.f;

	// Modal refs (Design space)
	FVector2D PivotDesign = FVector2D::ZeroVector;
	FVector2D MouseStartDesign = FVector2D::ZeroVector;

	// Rotate continuous tracking
	float RotateStartAngleRad = 0.f;
	float RotateLastAngleRad = 0.f;
	float RotateAccumulatedDeltaRad = 0.f;

	bool bCtrlDown = false;
	bool bShiftDown = false;

	void Reset()
	{
		Mode = EShapeTransformMode::None;
		bActive = false;

		InitialVertexDesignPositions.Reset();

		bInitialBackgroundSelected = false;
		InitialBackgroundOffsetDesign = FVector2D::ZeroVector;
		InitialBackgroundScale = 1.f;
		InitialBackgroundRotationDeg = 0.f;

		PivotDesign = FVector2D::ZeroVector;
		MouseStartDesign = FVector2D::ZeroVector;

		RotateStartAngleRad = 0.f;
		RotateLastAngleRad = 0.f;
		RotateAccumulatedDeltaRad = 0.f;

		bCtrlDown = false;
		bShiftDown = false;
	}
};

class SHAPETOOLSEDITOR_API SShapeGraphEditorCanvas : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShapeGraphEditorCanvas) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UShapeGraphAsset>, ShapeAsset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetCommandList(const TSharedPtr<FUICommandList>& InCommandList) { CommandList = InCommandList; }
	void HandleAssetPropertyChanged(FName PropertyName);

	// --- Tools API
	bool HasSelection() const { return !Selection.IsEmpty(); }
	void DeleteSelection();              // multi-delete (impl cpp)
	void FrameViewToShape();

	void ToggleGridSpace();
	FText GetGridSpaceLabel() const;

	// SWidget
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	// Input
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// =====================================================================
	// References
	// =====================================================================
	TWeakObjectPtr<UShapeGraphAsset> ShapeAsset;
	TSharedPtr<FUICommandList> CommandList;

	// =====================================================================
	// Persistent editor state
	// =====================================================================
	FShapeSelection Selection;

	EShapePivotMode PivotMode = EShapePivotMode::MedianPoint;
	FShapeTransformSession TransformSession;
	void UpdateTransformModifiers();

	// View (camera) in widget-local pixels
	FVector2D ViewPanPx = FVector2D::ZeroVector;
	float ViewZoom = 1.f;

	// Grid
	bool bShowGrid = true;

	// Hover (indices)
	int32 HoveredPointIndex = INDEX_NONE;
	int32 HoveredSegmentIndex = INDEX_NONE;

	// =====================================================================
	// Cached geometry (for DPI-safe cursor conversion)
	// =====================================================================
	mutable FGeometry CachedPaintGeometry;
	mutable bool bHasCachedPaintGeometry = false;

	// =====================================================================
	// Interaction state (mouse)
	// =====================================================================
	FVector2D LastMouseLocalPx = FVector2D::ZeroVector;

	bool bPanningView = false;              // RMB drag

	// LMB drag selection (multi)
	bool bLMBDown = false;
	bool bDragIntentVertex = false;      // LMB down started on a vertex
	bool bDragIntentEmpty = false;       // LMB down started on empty
	bool bDragIntentBackground = false;  // LMB down started on background
	FVector2D DragPivotDesign = FVector2D::ZeroVector;

	FVector2D LMBDownMouseViewPx = FVector2D::ZeroVector;
	int32 LMBDownHitVertexIndex = INDEX_NONE;
	float DragStartThresholdPx = 4.f;

	// HUD overlay
	mutable FString HUDLine1;
	mutable FString HUDLine2;
	mutable bool bShowHUD = false;
	void SetHUD(const FString& L1, const FString& L2);

	bool bDraggingSelection = false;
	int32 DragAnchorIndex = INDEX_NONE;     // point under cursor when drag started (for intent)
	FVector2D DragStartMouseUV = FVector2D::ZeroVector;
	TMap<int32, FVector2D> DragInitialUV;

	// Background drag
	bool bDraggingBackground = false;
	FVector2D BgDragStartMouseViewPx = FVector2D::ZeroVector;
	FVector2D BgDragStartOffsetDesign = FVector2D::ZeroVector;

	// Rectangle selection (future / now declared)
	bool bBoxSelecting = false;
	FVector2D BoxStartDesign = FVector2D::ZeroVector;
	FVector2D BoxEndDesign = FVector2D::ZeroVector;

	// Transactions
	TUniquePtr<FScopedTransaction> ActiveTransaction;

	// =====================================================================
	// Input helpers
	// =====================================================================
	FVector2D GetMouseLocalPx(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;
	FVector2D GetCursorLocalPx(const FGeometry& MyGeometry) const;

	// =====================================================================
	// Space conversions
	// UV (0..1) <-> Design (work area px) <-> View (widget px)
	// =====================================================================
	FVector2D UVToDesign(const FVector2D& UV) const;
	FVector2D DesignToUV(const FVector2D& Design) const;

	FVector2D DesignToViewPx(const FVector2D& Design) const;
	FVector2D ViewPxToDesign(const FVector2D& ViewPx) const;

	FVector2D UVToViewPx(const FVector2D& UV) const;
	FVector2D ViewPxToUV(const FVector2D& ViewPx) const;

	// =====================================================================
	// Hit tests (VIEW space)
	// =====================================================================
	int32 HitTestPoint(const FVector2D& ViewPx, float RadiusPx) const;
	int32 HitTestSegment(const FVector2D& ViewPx, float RadiusPx, float& OutT) const;
	bool HitTestBackground(const FVector2D& MouseViewPx) const;

	// =====================================================================
	// Selection ops
	// =====================================================================
	void ClearSelection();
	void SelectVertexExclusive(int32 Index);
	void AddVertex(int32 Index);
	void RemoveVertex(int32 Index);
	void ToggleVertex(int32 Index);

	void SelectBackgroundExclusive();
	void ToggleBackground();

	// =====================================================================
	// Pivot
	// =====================================================================
	FVector2D ComputePivotDesign() const;

	// =====================================================================
	// Modal transform (G/S/R)
	// =====================================================================
	void BeginTransform(EShapeTransformMode Mode, const FGeometry& Geo, const FVector2D& MouseViewPx);
	void UpdateTransform(const FGeometry& Geo, const FVector2D& MouseViewPx);
	void ConfirmTransform();
	void CancelTransform();

	void ApplyTranslate(const FVector2D& MouseDesign);
	void ApplyScaleUniform(const FVector2D& MouseDesign);
	void ApplyRotate(const FVector2D& MouseDesign);

	// =====================================================================
	// Rectangle selection (declared, to implement in cpp)
	// =====================================================================
	void BeginBoxSelect(const FGeometry& Geo, const FVector2D& MouseViewPx);
	void UpdateBoxSelect(const FGeometry& Geo, const FVector2D& MouseViewPx);
	void EndBoxSelect(bool bAdditive);

	// =====================================================================
	// Asset read/write
	// =====================================================================
	void SyncPivotModeFromAsset();
	FVector2D GetVertexDesignPosition(int32 Index) const;
	void SetVertexDesignPosition(int32 Index, const FVector2D& NewDesign);

	void SetBackgroundOffsetDesign(const FVector2D& OffsetDesign);
	void SetBackgroundScale(float Scale);
	void SetBackgroundRotationDeg(float Deg);

	// =====================================================================
	// Rendering
	// =====================================================================
	void DrawGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	void UpdateBackgroundBrush(UTexture2D* Texture) const;
	void DrawBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawBackgroundSelectionOutline(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawPivot(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	mutable FSlateBrush BackgroundBrush;
	mutable TWeakObjectPtr<UTexture2D> CachedBackgroundTexture;

	// =====================================================================
	// Transactions / asset updates
	// =====================================================================
	void BeginTransaction(const FText& Description);
	void EndTransaction();

	void ModifyAsset();
	void NotifyAssetChanged();
};