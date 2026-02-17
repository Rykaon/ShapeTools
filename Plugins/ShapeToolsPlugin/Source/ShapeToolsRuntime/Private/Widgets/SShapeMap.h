#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "ShapeStyles.h"

class UShapeGraphAsset;
class UShapeStateProvider;

class SShapeMap : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams(FOnHoverChanged, FName Key, bool bHovered);
	DECLARE_DELEGATE_OneParam(FOnClicked, FName Key);

	SLATE_BEGIN_ARGS(SShapeMap) {}
		SLATE_ARGUMENT(TArray<UShapeGraphAsset*>, Shapes)
		SLATE_ARGUMENT(UShapeStateProvider*, StateProvider)

		SLATE_ARGUMENT(FShapeVisualStyle, DefaultVisualStyle)
		SLATE_ARGUMENT(FShapeVisualStyleOverrides, PerShapeVisualOverrides)

		SLATE_ARGUMENT(FShapeDebugStyle, DebugStyle)
		SLATE_ARGUMENT(FName, SelectedKey)

		SLATE_EVENT(FOnHoverChanged, OnHoverChanged)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetDefaultVisualStyle(const FShapeVisualStyle& InStyle);
	void SetPerShapeVisualOverrides(const FShapeVisualStyleOverrides& InOverrides);
	void SetPerShapeVisualOverride(FName Key, const FShapeVisualStyle& Style);
	void ClearPerShapeVisualOverride(FName Key);
	void ClearAllPerShapeVisualOverrides();

	void SetDebugStyle(const FShapeDebugStyle& InStyle);

	void SetSelectedKey(FName InKey);

	// Input
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	// Draw
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<TObjectPtr<UShapeGraphAsset>> Shapes;
	TObjectPtr<UShapeStateProvider> StateProvider = nullptr;
	
	FShapeVisualStyle DefaultVisualStyle;
	FShapeVisualStyleOverrides PerShapeVisualOverrides;

	FShapeDebugStyle DebugStyle;

	FName SelectedKey = NAME_None;
	FName HoveredKey = NAME_None;

	FOnHoverChanged OnHoverChanged;
	FOnClicked OnClicked;

	FVector2D ScreenToUV(const FGeometry& Geo, const FVector2D& ScreenPos) const;
	bool PointInPolygon(const FVector2D& P, const TArray<FVector2D>& Poly) const;
	FName PickKeyAtUV(const FVector2D& UV) const;
};