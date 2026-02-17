#include "Widgets/SShapeMap.h"
#include "ShapeGraphAsset.h"
#include "ShapeStateProvider.h"
#include "Rendering/DrawElements.h"

void SShapeMap::Construct(const FArguments& InArgs)
{
	Shapes.Reset();
	Shapes.Reserve(InArgs._Shapes.Num());
	for (UShapeGraphAsset* Shape : InArgs._Shapes)
	{
		Shapes.Add(Shape);
	}

	StateProvider = InArgs._StateProvider;

	DefaultVisualStyle = InArgs._DefaultVisualStyle;
	PerShapeVisualOverrides = InArgs._PerShapeVisualOverrides;

	DebugStyle = InArgs._DebugStyle;
	SelectedKey = InArgs._SelectedKey;

	OnHoverChanged = InArgs._OnHoverChanged;
	OnClicked = InArgs._OnClicked;

	SetCanTick(false);
}

void SShapeMap::SetDefaultVisualStyle(const FShapeVisualStyle& InStyle)
{
	DefaultVisualStyle = InStyle;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SShapeMap::SetPerShapeVisualOverrides(const FShapeVisualStyleOverrides& InOverrides)
{
	PerShapeVisualOverrides = InOverrides;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SShapeMap::SetPerShapeVisualOverride(FName Key, const FShapeVisualStyle& Style)
{
	if (Key.IsNone()) return;

	PerShapeVisualOverrides.Map.FindOrAdd(Key) = Style;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SShapeMap::ClearPerShapeVisualOverride(FName Key)
{
	if (Key.IsNone()) return;

	if (PerShapeVisualOverrides.Map.Remove(Key) > 0)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SShapeMap::ClearAllPerShapeVisualOverrides()
{
	if (PerShapeVisualOverrides.Map.Num() == 0) return;

	PerShapeVisualOverrides.Map.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SShapeMap::SetDebugStyle(const FShapeDebugStyle& InStyle)
{
	DebugStyle = InStyle;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SShapeMap::SetSelectedKey(FName InKey)
{
	SelectedKey = InKey;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SShapeMap::ScreenToUV(const FGeometry& Geo, const FVector2D& ScreenPos) const
{
	const FVector2D Local = Geo.AbsoluteToLocal(ScreenPos);
	const FVector2D Size = Geo.GetLocalSize();

	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
	{
		return FVector2D(-1, -1);
	}

	// UV in [0..1]
	return FVector2D(Local.X / Size.X, Local.Y / Size.Y);
}

// Ray casting point-in-polygon (works for simple polygons, assumes no self-intersection)
bool SShapeMap::PointInPolygon(const FVector2D& P, const TArray<FVector2D>& Poly) const
{
	const int32 N = Poly.Num();
	if (N < 3) return false;

	bool bInside = false;
	for (int32 i = 0, j = N - 1; i < N; j = i++)
	{
		const FVector2D& Pi = Poly[i];
		const FVector2D& Pj = Poly[j];

		const bool bIntersect =
			((Pi.Y > P.Y) != (Pj.Y > P.Y)) &&
			(P.X < (Pj.X - Pi.X) * (P.Y - Pi.Y) / (Pj.Y - Pi.Y + 1e-12f) + Pi.X);

		if (bIntersect)
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

FName SShapeMap::PickKeyAtUV(const FVector2D& UV) const
{
	// Naive: first match wins. Later we'll sort by ZOrder and use bounds.
	for (const TObjectPtr<UShapeGraphAsset>& Shape : Shapes)
	{
		if (!Shape) continue;
		if (PointInPolygon(UV, Shape->ContourUV))
		{
			return Shape->Key;
		}
	}
	return NAME_None;
}

FReply SShapeMap::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D UV = ScreenToUV(MyGeometry, MouseEvent.GetScreenSpacePosition());
	const FName NewHovered = PickKeyAtUV(UV);

	if (NewHovered != HoveredKey)
	{
		if (HoveredKey != NAME_None && OnHoverChanged.IsBound())
		{
			OnHoverChanged.Execute(HoveredKey, false);
		}

		HoveredKey = NewHovered;

		if (HoveredKey != NAME_None && OnHoverChanged.IsBound())
		{
			OnHoverChanged.Execute(HoveredKey, true);
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SShapeMap::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const FVector2D UV = ScreenToUV(MyGeometry, MouseEvent.GetScreenSpacePosition());
	const FName Picked = PickKeyAtUV(UV);

	if (Picked != NAME_None)
	{
		if (OnClicked.IsBound())
		{
			OnClicked.Execute(Picked);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

int32 SShapeMap::OnPaint(
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

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
	{
		return LayerId;
	}

	const int32 DrawLayer = LayerId + 1;

	for (const TWeakObjectPtr<UShapeGraphAsset>& ShapeWeak : Shapes)
	{
		const UShapeGraphAsset* Shape = ShapeWeak.Get();
		if (!Shape || Shape->ContourUV.Num() < 2)
		{
			continue;
		}

		// Build polyline points in local px
		TArray<FVector2D> PointsPx;
		PointsPx.Reserve(Shape->ContourUV.Num() + (Shape->bClosed ? 1 : 0));

		for (const FVector2D& UV : Shape->ContourUV)
		{
			PointsPx.Add(FVector2D(UV.X * Size.X, UV.Y * Size.Y));
		}

		// Close for drawing (safe copy)
		if (Shape->bClosed && PointsPx.Num() > 0)
		{
			const FVector2D First = PointsPx[0];
			PointsPx.Add(First);
		}

		// Decide style
		FLinearColor LineColor = FLinearColor::Transparent;
		float Thickness = 0.f;

		if (DebugStyle.bEnabled)
		{
			Thickness = DebugStyle.LineThickness;

			if (Shape->Key == SelectedKey)
			{
				LineColor = DebugStyle.SelectedColor;
			}
			else if (Shape->Key == HoveredKey)
			{
				LineColor = DebugStyle.HoverColor;
			}
			else
			{
				LineColor = DebugStyle.NormalColor;
			}
		}
		else
		{
			// Resolve per-shape override
			const FShapeVisualStyle* Override = PerShapeVisualOverrides.Map.Find(Shape->Key);
			const FShapeVisualStyle& Style = Override ? *Override : DefaultVisualStyle;

			Thickness = Style.OutlineThickness;
			LineColor = Style.OutlineColor;

			if (Shape->Key == SelectedKey)
			{
				LineColor *= Style.SelectedTint;
			}
			else if (Shape->Key == HoveredKey)
			{
				LineColor *= Style.HoverTint;
			}
		}

		// Draw if valid
		if (Thickness > 0.f && LineColor.A > 0.f)
		{
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				DrawLayer,
				AllottedGeometry.ToPaintGeometry(),
				PointsPx,
				ESlateDrawEffect::None,
				LineColor,
				true,      // antialias
				Thickness
			);
		}
	}

	return DrawLayer;
}