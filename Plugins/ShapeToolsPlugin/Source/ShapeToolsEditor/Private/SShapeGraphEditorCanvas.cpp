#include "SShapeGraphEditorCanvas.h"

#include "ShapeGraphAsset.h"
#include "ScopedTransaction.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/ICursor.h"

static constexpr float GPointHitRadiusPx = 10.f;
static constexpr float GSegmentHitRadiusPx = 8.f;
static constexpr float GPointBoxSizePx = 6.f;

void SShapeGraphEditorCanvas::Construct(const FArguments& InArgs)
{
	ShapeAsset = InArgs._ShapeAsset;
	SetCanTick(false);
}

FVector2D SShapeGraphEditorCanvas::LocalToUV(const FGeometry& Geo, const FVector2D& LocalPx) const
{
	const FVector2D Size = Geo.GetLocalSize();
	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D(LocalPx.X / Size.X, LocalPx.Y / Size.Y);
}

FVector2D SShapeGraphEditorCanvas::UVToLocal(const FGeometry& Geo, const FVector2D& UV) const
{
	const FVector2D Size = Geo.GetLocalSize();
	return FVector2D(UV.X * Size.X, UV.Y * Size.Y);
}

int32 SShapeGraphEditorCanvas::HitTestPoint(const FGeometry& Geo, const FVector2D& LocalPx, float RadiusPx) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return INDEX_NONE;

	const float R2 = RadiusPx * RadiusPx;
	for (int32 i = 0; i < Asset->ContourUV.Num(); ++i)
	{
		const FVector2D P = UVToLocal(Geo, Asset->ContourUV[i]);
		if ((P - LocalPx).SizeSquared() <= R2)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

static float DistPointToSegmentSq(const FVector2D& P, const FVector2D& A, const FVector2D& B, float& OutT)
{
	const FVector2D AB = (B - A);
	const float Den = AB.SizeSquared();
	if (Den <= KINDA_SMALL_NUMBER)
	{
		OutT = 0.f;
		return (P - A).SizeSquared();
	}

	OutT = FVector2D::DotProduct(P - A, AB) / Den;
	OutT = FMath::Clamp(OutT, 0.f, 1.f);

	const FVector2D Closest = A + AB * OutT;
	return (P - Closest).SizeSquared();
}

int32 SShapeGraphEditorCanvas::HitTestSegment(const FGeometry& Geo, const FVector2D& LocalPx, float RadiusPx, float& OutT) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return INDEX_NONE;

	const int32 N = Asset->ContourUV.Num();
	if (N < 2) return INDEX_NONE;

	const float R2 = RadiusPx * RadiusPx;
	auto GetPt = [&](int32 Idx) { return UVToLocal(Geo, Asset->ContourUV[Idx]); };

	for (int32 i = 0; i < N - 1; ++i)
	{
		float T = 0.f;
		const float D2 = DistPointToSegmentSq(LocalPx, GetPt(i), GetPt(i + 1), T);
		if (D2 <= R2)
		{
			OutT = T;
			return i;
		}
	}

	if (Asset->bClosed)
	{
		float T = 0.f;
		const float D2 = DistPointToSegmentSq(LocalPx, GetPt(N - 1), GetPt(0), T);
		if (D2 <= R2)
		{
			OutT = T;
			return N - 1; // last->first
		}
	}

	return INDEX_NONE;
}

void SShapeGraphEditorCanvas::BeginTransaction(const FText& Description)
{
	if (!ActiveTransaction.IsValid())
	{
		ActiveTransaction = MakeUnique<FScopedTransaction>(Description);
		ModifyAsset();
	}
}

void SShapeGraphEditorCanvas::EndTransaction()
{
	ActiveTransaction.Reset();
}

void SShapeGraphEditorCanvas::ModifyAsset()
{
	if (UShapeGraphAsset* Asset = ShapeAsset.Get())
	{
		Asset->Modify();
	}
}

void SShapeGraphEditorCanvas::NotifyAssetChanged()
{
	if (UShapeGraphAsset* Asset = ShapeAsset.Get())
	{
		Asset->PostEditChange();
		Asset->MarkPackageDirty();
	}
}

void SShapeGraphEditorCanvas::DeleteSelection()
{
	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;
	if (!Asset->ContourUV.IsValidIndex(SelectedPointIndex)) return;

	const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "DeletePointTx", "Delete Shape Point"));
	Asset->Modify();

	Asset->ContourUV.RemoveAt(SelectedPointIndex);
	SelectedPointIndex = INDEX_NONE;

	NotifyAssetChanged();
	Invalidate(EInvalidateWidget::Paint);
}

FReply SShapeGraphEditorCanvas::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Handled();
}

FVector2D SShapeGraphEditorCanvas::GetMouseLocal(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	FSlateApplication& App = FSlateApplication::Get();
	const FGeometry& Geo = bHasCachedPaintGeometry ? CachedPaintGeometry : MyGeometry;

	// Prefer OS cursor position -> fixes Editor/DPI/multi-window mismatch.
	const TSharedPtr<ICursor> PlatformCursor = App.GetPlatformCursor();
	if (!PlatformCursor.IsValid())
	{
		return Geo.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	}

	const FVector2D DesktopPx = PlatformCursor->GetPosition();
	TSharedPtr<SWindow> Window = App.FindWidgetWindow(AsShared());
	if (!Window.IsValid())
	{
		return Geo.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	}

	const FVector2D WindowDesktopPx = Window->GetPositionInScreen();
	const float AppScale = App.GetApplicationScale();
	const FVector2D WindowSpaceSlate = (DesktopPx - WindowDesktopPx) / AppScale;

	return Geo.AbsoluteToLocal(WindowSpaceSlate);
}

FReply SShapeGraphEditorCanvas::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		DeleteSelection();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SShapeGraphEditorCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FGeometry& Geo = bHasCachedPaintGeometry ? CachedPaintGeometry : MyGeometry;
	const FVector2D MouseLocal = GetMouseLocal(MyGeometry, MouseEvent);

	LastMouseLocal = MouseLocal;

	FReply Reply = FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);

	// Pan: MMB
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bPanning = true;
		return Reply.CaptureMouse(SharedThis(this));
	}

	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset)
	{
		return Reply;
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Ctrl+LMB near segment -> insert point
		if (MouseEvent.IsControlDown())
		{
			float T = 0.f;
			const int32 Seg = HitTestSegment(Geo, MouseLocal, GSegmentHitRadiusPx, T);
			if (Seg != INDEX_NONE)
			{
				const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "InsertPointTx", "Insert Shape Point"));
				Asset->Modify();

				const int32 N = Asset->ContourUV.Num();
				int32 InsertIndex = Seg + 1;
				FVector2D Auv, Buv;

				if (Seg == N - 1 && Asset->bClosed)
				{
					Auv = Asset->ContourUV[N - 1];
					Buv = Asset->ContourUV[0];
					InsertIndex = N; // insert at end
				}
				else
				{
					Auv = Asset->ContourUV[Seg];
					Buv = Asset->ContourUV[Seg + 1];
				}

				const FVector2D NewUV = FMath::Lerp(Auv, Buv, T);
				Asset->ContourUV.Insert(NewUV, InsertIndex);

				SelectedPointIndex = InsertIndex;
				NotifyAssetChanged();
				Invalidate(EInvalidateWidget::Paint);
				return Reply;
			}
		}

		// Select + drag point
		const int32 Hit = HitTestPoint(Geo, MouseLocal, GPointHitRadiusPx);
		if (Hit != INDEX_NONE)
		{
			SelectedPointIndex = Hit;
			bDraggingPoint = true;
			DragPointIndex = Hit;

			BeginTransaction(NSLOCTEXT("ShapeTools", "MovePointTx", "Move Shape Point"));
			Invalidate(EInvalidateWidget::Paint);
			return Reply.CaptureMouse(SharedThis(this));
		}

		// Empty click -> deselect
		if (SelectedPointIndex != INDEX_NONE)
		{
			SelectedPointIndex = INDEX_NONE;
			Invalidate(EInvalidateWidget::Paint);
		}
		return Reply;
	}

	return FReply::Unhandled();
}

FReply SShapeGraphEditorCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && bPanning)
	{
		bPanning = false;
		return FReply::Handled()
			.SetUserFocus(SharedThis(this), EFocusCause::Mouse)
			.ReleaseMouseCapture();
	}

	if (HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDraggingPoint)
	{
		bDraggingPoint = false;
		DragPointIndex = INDEX_NONE;

		EndTransaction();
		NotifyAssetChanged();
		Invalidate(EInvalidateWidget::Paint);

		return FReply::Handled()
			.SetUserFocus(SharedThis(this), EFocusCause::Mouse)
			.ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SShapeGraphEditorCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FGeometry& Geo = bHasCachedPaintGeometry ? CachedPaintGeometry : MyGeometry;
	const FVector2D MouseLocal = GetMouseLocal(MyGeometry, MouseEvent);

	// Hover detection
	const int32 PrevHoveredPoint = HoveredPointIndex;
	const int32 PrevHoveredSeg = HoveredSegmentIndex;

	HoveredPointIndex = HitTestPoint(Geo, MouseLocal, GPointHitRadiusPx);

	float SegT = 0.f;
	HoveredSegmentIndex = (HoveredPointIndex == INDEX_NONE)
		? HitTestSegment(Geo, MouseLocal, GSegmentHitRadiusPx, SegT)
		: INDEX_NONE;

	bool bNeedsRepaint = (HoveredPointIndex != PrevHoveredPoint) || (HoveredSegmentIndex != PrevHoveredSeg);

	// Pan (MMB)
	if (bPanning && HasMouseCapture())
	{
		const FVector2D Delta = MouseLocal - LastMouseLocal;
		BgOffsetPx += Delta;

		bNeedsRepaint = true;
	}

	// Drag point
	if (bDraggingPoint && HasMouseCapture())
	{
		if (UShapeGraphAsset* Asset = ShapeAsset.Get())
		{
			if (Asset->ContourUV.IsValidIndex(DragPointIndex))
			{
				FVector2D UV = LocalToUV(Geo, MouseLocal);
				UV.X = FMath::Clamp(UV.X, 0.f, 1.f);
				UV.Y = FMath::Clamp(UV.Y, 0.f, 1.f);

				if (MouseEvent.IsControlDown())
				{
					const float Step = 0.01f;
					UV.X = FMath::Clamp(FMath::GridSnap(UV.X, Step), 0.f, 1.f);
					UV.Y = FMath::Clamp(FMath::GridSnap(UV.Y, Step), 0.f, 1.f);
				}

				Asset->ContourUV[DragPointIndex] = UV;
				bNeedsRepaint = true;
			}
		}
	}

	LastMouseLocal = MouseLocal;

	if (bNeedsRepaint)
	{
		Invalidate(EInvalidateWidget::Paint);
	}

	return FReply::Handled();
}

FReply SShapeGraphEditorCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float Wheel = MouseEvent.GetWheelDelta();
	if (FMath::IsNearlyZero(Wheel))
	{
		return FReply::Unhandled();
	}

	const FVector2D MouseLocal = GetMouseLocal(MyGeometry, MouseEvent);

	const float ZoomFactor = FMath::Pow(1.1f, Wheel);
	const float NewScale = FMath::Clamp(BgScale * ZoomFactor, 0.1f, 20.f);

	const FVector2D Before = (MouseLocal - BgOffsetPx) / BgScale;
	BgScale = NewScale;
	BgOffsetPx = MouseLocal - Before * BgScale;

	Invalidate(EInvalidateWidget::Paint);
	return FReply::Handled();
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

	// Cache geometry for consistent input hit-testing
	CachedPaintGeometry = AllottedGeometry;
	bHasCachedPaintGeometry = true;

	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset || Asset->ContourUV.Num() < 1)
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
	{
		return LayerId;
	}

	// Lines
	if (Asset->ContourUV.Num() >= 2)
	{
		TArray<FVector2D> PointsPx;
		PointsPx.Reserve(Asset->ContourUV.Num() + (Asset->bClosed ? 1 : 0));

		for (const FVector2D& UV : Asset->ContourUV)
		{
			PointsPx.Add(FVector2D(UV.X * Size.X, UV.Y * Size.Y));
		}

		if (Asset->bClosed && PointsPx.Num() > 0)
		{
			PointsPx.Add(PointsPx[0]);
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			PointsPx,
			ESlateDrawEffect::None,
			FLinearColor::White,
			true,
			0.5f
		);
	}

	// Point handles
	for (int32 i = 0; i < Asset->ContourUV.Num(); ++i)
	{
		const FVector2D UV = Asset->ContourUV[i];
		const FVector2D UVc(FMath::Clamp(UV.X, 0.f, 1.f), FMath::Clamp(UV.Y, 0.f, 1.f));
		const FVector2D Ppx(UVc.X * Size.X, UVc.Y * Size.Y);

		const FVector2D BoxSize(GPointBoxSizePx, GPointBoxSizePx);
		const FVector2D TopLeft = Ppx - (BoxSize * 0.5f);

		const FGeometry BoxGeo = AllottedGeometry.MakeChild(
			BoxSize,
			FSlateLayoutTransform(TopLeft)
		);

		const bool bSelected = (i == SelectedPointIndex);
		const bool bHovered = (i == HoveredPointIndex);

		const FLinearColor Col = bSelected ? FLinearColor::Green : (bHovered ? FLinearColor::Yellow : FLinearColor::White);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			BoxGeo.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			Col
		);
	}

	return LayerId + 2;
}