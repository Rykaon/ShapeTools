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

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	TWeakObjectPtr<UShapeGraphAsset> ShapeAsset;
};