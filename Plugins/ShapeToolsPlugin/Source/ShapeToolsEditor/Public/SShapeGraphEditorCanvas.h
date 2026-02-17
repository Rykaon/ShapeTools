#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UShapeGraphAsset;

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

	// --- UV helpers ---
	FVector2D LocalToUV(const FGeometry& Geo, const FVector2D& LocalPx) const;
	FVector2D UVToLocal(const FGeometry& Geo, const FVector2D& UV) const;

	// --- Hit tests ---
	int32 HitTestPoint(const FGeometry& Geo, const FVector2D& LocalPx, float RadiusPx) const;
	int32 HitTestSegment(const FGeometry& Geo, const FVector2D& LocalPx, float RadiusPx, float& OutT) const;

	// --- Undo transaction ---
	void BeginTransaction(const FText& Description);
	void EndTransaction();

	void ModifyAsset();
	void NotifyAssetChanged();

private:
	TWeakObjectPtr<UShapeGraphAsset> ShapeAsset;

	TSharedPtr<FUICommandList> CommandList;

	mutable FGeometry CachedPaintGeometry;
	mutable bool bHasCachedPaintGeometry = false;

	FVector2D GetMouseLocal(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;
	FVector2D LastMouseLocal = FVector2D::ZeroVector;

	// Interaction state
	bool bPanning = false;
	bool bDraggingPoint = false;
	int32 DragPointIndex = INDEX_NONE;

	int32 SelectedPointIndex = INDEX_NONE;
	int32 HoveredPointIndex = INDEX_NONE;
	int32 HoveredSegmentIndex = INDEX_NONE;

	// Background view transform (for texture later)
	FVector2D BgOffsetPx = FVector2D::ZeroVector;
	float BgScale = 1.0f;

	// Drag transaction lifetime
	TUniquePtr<class FScopedTransaction> ActiveTransaction;
};