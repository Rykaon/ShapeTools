#include "SShapeGraphEditorCanvas.h"
#include "ShapeGraphAsset.h"

void SShapeGraphEditorCanvas::Construct(const FArguments& InArgs)
{
	ShapeAsset = InArgs._ShapeAsset;
}

int32 SShapeGraphEditorCanvas::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset || Asset->ContourUV.Num() < 2)
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
	{
		return LayerId;
	}

	TArray<FVector2D> PointsPx;
	PointsPx.Reserve(Asset->ContourUV.Num() + (Asset->bClosed ? 1 : 0));

	for (const FVector2D& UV : Asset->ContourUV)
	{
		PointsPx.Add(FVector2D(UV.X * Size.X, UV.Y * Size.Y));
	}

	if (Asset->bClosed && PointsPx.Num() > 0)
	{
		const FVector2D First = PointsPx[0];
		PointsPx.Add(First);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(),
		PointsPx,
		ESlateDrawEffect::None,
		FLinearColor::White,
		true,
		2.0f
	);

	// Draw points as small boxes
	for (const FVector2D& P : Asset->ContourUV)
	{
		const FVector2D BoxSize(6.f, 6.f);
		const FVector2D TopLeft = P - (BoxSize * 0.5f);

		const FGeometry BoxGeo = AllottedGeometry.MakeChild(
			BoxSize,
			FSlateLayoutTransform(TopLeft)
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			BoxGeo.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor::Yellow
		);
	}

	return LayerId + 2;
}