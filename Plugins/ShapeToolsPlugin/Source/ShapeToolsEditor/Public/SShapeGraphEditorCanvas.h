#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UShapeGraphAsset;
class FUICommandList;
class FScopedTransaction;

class SHAPETOOLSEDITOR_API SShapeGraphEditorCanvas : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShapeGraphEditorCanvas) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UShapeGraphAsset>, ShapeAsset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetCommandList(const TSharedPtr<FUICommandList>& InCommandList) { CommandList = InCommandList; }

	bool HasSelection() const { return SelectedPointIndex != INDEX_NONE; }
	void DeleteSelection();

	void FrameViewToShape();

	// SWidget / Input
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void ToggleGridSpace();
	FText GetGridSpaceLabel() const;
	void ClearBackgroundSelection();

private:
	// -------------------------
	// References
	// -------------------------
	TWeakObjectPtr<UShapeGraphAsset> ShapeAsset;
	TSharedPtr<FUICommandList> CommandList;

	// -------------------------
	// Cached geometry (for DPI-safe cursor conversion)
	// -------------------------
	mutable FGeometry CachedPaintGeometry;
	mutable bool bHasCachedPaintGeometry = false;

	FVector2D GetMouseLocal(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;
	FVector2D LastMouseLocal = FVector2D::ZeroVector;

	// -------------------------
	// Interaction state
	// -------------------------
	bool bPanningView = false;     // RMB drag
	bool bDraggingPoint = false;   // LMB drag on vertex
	int32 DragPointIndex = INDEX_NONE;

	int32 SelectedPointIndex = INDEX_NONE;
	int32 HoveredPointIndex = INDEX_NONE;
	int32 HoveredSegmentIndex = INDEX_NONE;

	TUniquePtr<FScopedTransaction> ActiveTransaction;

	// -------------------------
	// Viewport / Spaces
	// UV (data) <-> Design (work area) <-> View (widget px)
	// -------------------------

	FVector2D ViewPanPx = FVector2D::ZeroVector; // view-space translation in widget px
	float ViewZoom = 1.0f;                       // view-space zoom

	// Background transform in DESIGN space (independent from DesignSize)
	FVector2D BgOffsetDesign = FVector2D::ZeroVector;
	float BgScale = 1.0f;

	// -------------------------
	// Grid
	// -------------------------

	bool bShowGrid = true;

	// -------------------------
	// Helpers
	// -------------------------
	// Hit tests in VIEW space (widget local px)
	int32 HitTestPoint(const FVector2D& ViewPx, float RadiusPx) const;
	int32 HitTestSegment(const FVector2D& ViewPx, float RadiusPx, float& OutT) const;

	// Space conversions
	FVector2D UVToDesign(const FVector2D& UV) const;
	FVector2D DesignToUV(const FVector2D& Design) const;

	FVector2D DesignToViewPx(const FVector2D& Design) const;
	FVector2D ViewPxToDesign(const FVector2D& ViewPx) const;
	FVector2D UVToViewPx(const FVector2D& UV) const;
	FVector2D ViewPxToUV(const FVector2D& ViewPx) const;

	mutable FSlateBrush BackgroundBrush;
	mutable TWeakObjectPtr<UTexture2D> CachedBackgroundTexture;
	bool bBackgroundSelected = false;
	bool bDraggingBackground = false;
	FVector2D BgDragStartMouseView = FVector2D::ZeroVector;
	FVector2D BgDragStartOffsetDesign = FVector2D::ZeroVector;

	// Rendering
	void DrawGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void UpdateBackgroundBrush(UTexture2D* Texture) const;
	void DrawBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	bool HitTestBackground(const FVector2D& MouseViewPx) const;
	void DrawBackgroundSelectionOutline(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	// Transactions / asset updates
	void BeginTransaction(const FText& Description);
	void EndTransaction();
	void ModifyAsset();
	void NotifyAssetChanged();
};