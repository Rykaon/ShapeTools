#include "SShapeGraphEditorCanvas.h"

#include "ShapeGraphAsset.h"
#include "ScopedTransaction.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/ICursor.h"
#include "Math/TransformCalculus2D.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/SlateRenderTransform.h"

static constexpr float GPointHitRadiusPx = 10.f;
static constexpr float GSegmentHitRadiusPx = 8.f;
static constexpr float GPointBoxSizePx = 6.f;

void SShapeGraphEditorCanvas::Construct(const FArguments& InArgs)
{
	ShapeAsset = InArgs._ShapeAsset;
	SetCanTick(false);
}

// -------------------------
// Input Space (DPI-safe cursor)
// -------------------------

FVector2D SShapeGraphEditorCanvas::GetMouseLocal(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	FSlateApplication& App = FSlateApplication::Get();
	const FGeometry& Geo = bHasCachedPaintGeometry ? CachedPaintGeometry : MyGeometry;

	const TSharedPtr<ICursor> PlatformCursor = App.GetPlatformCursor();
	if (!PlatformCursor.IsValid())
	{
		return Geo.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	}

	const FVector2D DesktopPx = PlatformCursor->GetPosition();
	const TSharedPtr<SWindow> Window = App.FindWidgetWindow(AsShared());
	if (!Window.IsValid())
	{
		return Geo.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	}

	const FVector2D WindowDesktopPx = Window->GetPositionInScreen();
	const float AppScale = App.GetApplicationScale();
	const FVector2D WindowSpaceSlate = (DesktopPx - WindowDesktopPx) / AppScale;

	return Geo.AbsoluteToLocal(WindowSpaceSlate);
}

// -------------------------
// Space conversions
// UV (0..1) <-> Design (work area units) <-> View (widget local px)
// -------------------------

FVector2D SShapeGraphEditorCanvas::UVToDesign(const FVector2D& UV) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return FVector2D::ZeroVector;
	const FVector2D Size = Asset ? Asset->CanvasDesignSize : FVector2D(1920.f, 1080.f);
	return FVector2D(UV.X * Size.X, UV.Y * Size.Y);
}

FVector2D SShapeGraphEditorCanvas::DesignToUV(const FVector2D& Design) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return FVector2D::ZeroVector;
	const FVector2D Size = Asset->CanvasDesignSize;
	return FVector2D(
		Size.X > KINDA_SMALL_NUMBER ? Design.X / Size.X : 0.f,
		Size.Y > KINDA_SMALL_NUMBER ? Design.Y / Size.Y : 0.f
	);
}

FVector2D SShapeGraphEditorCanvas::DesignToViewPx(const FVector2D& Design) const
{
	return (Design * ViewZoom) + ViewPanPx;
}

FVector2D SShapeGraphEditorCanvas::ViewPxToDesign(const FVector2D& ViewPx) const
{
	return (ViewPx - ViewPanPx) / ViewZoom;
}

FVector2D SShapeGraphEditorCanvas::UVToViewPx(const FVector2D& UV) const
{
	return DesignToViewPx(UVToDesign(UV));
}

FVector2D SShapeGraphEditorCanvas::ViewPxToUV(const FVector2D& ViewPx) const
{
	return DesignToUV(ViewPxToDesign(ViewPx));
}

// -------------------------
// Geometry helpers
// -------------------------

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

int32 SShapeGraphEditorCanvas::HitTestPoint(const FVector2D& ViewPx, float RadiusPx) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return INDEX_NONE;

	const float R2 = RadiusPx * RadiusPx;
	for (int32 i = 0; i < Asset->ContourUV.Num(); ++i)
	{
		const FVector2D P = UVToViewPx(Asset->ContourUV[i]);
		if ((P - ViewPx).SizeSquared() <= R2)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 SShapeGraphEditorCanvas::HitTestSegment(const FVector2D& ViewPx, float RadiusPx, float& OutT) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return INDEX_NONE;

	const int32 N = Asset->ContourUV.Num();
	if (N < 2) return INDEX_NONE;

	const float R2 = RadiusPx * RadiusPx;
	auto GetPt = [&](int32 Idx) { return UVToViewPx(Asset->ContourUV[Idx]); };

	for (int32 i = 0; i < N - 1; ++i)
	{
		float T = 0.f;
		const float D2 = DistPointToSegmentSq(ViewPx, GetPt(i), GetPt(i + 1), T);
		if (D2 <= R2)
		{
			OutT = T;
			return i;
		}
	}

	if (Asset->bClosed)
	{
		float T = 0.f;
		const float D2 = DistPointToSegmentSq(ViewPx, GetPt(N - 1), GetPt(0), T);
		if (D2 <= R2)
		{
			OutT = T;
			return N - 1;
		}
	}

	return INDEX_NONE;
}

bool SShapeGraphEditorCanvas::HitTestBackground(const FVector2D& MouseViewPx) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return false;

	UTexture2D* Texture = Asset->BackgroundTexture.Get();
	if (!Texture) return false;

	const FVector2D TexSize((float)Texture->GetSizeX(), (float)Texture->GetSizeY());
	const float Scale = FMath::Max(Asset->BackgroundScale, 0.001f);
	const FVector2D SizeDesign = TexSize * Scale;

	// Rect in VIEW before rotation
	const FVector2D TopLeftView = DesignToViewPx(Asset->BackgroundOffsetPx);
	const FVector2D SizeView = SizeDesign * ViewZoom;

	if (SizeView.X <= 1.f || SizeView.Y <= 1.f) return false;

	// Transform mouse into BG local space (account rotation)
	const FVector2D Center = TopLeftView + (SizeView * 0.5f);
	FVector2D P = MouseViewPx - Center;

	const float Rad = FMath::DegreesToRadians(Asset->BackgroundRotation);
	const float CosA = FMath::Cos(-Rad);
	const float SinA = FMath::Sin(-Rad);

	// inverse rotate
	const FVector2D Pr(
		P.X * CosA - P.Y * SinA,
		P.X * SinA + P.Y * CosA
	);

	const FVector2D Local = Pr + (SizeView * 0.5f);

	return (Local.X >= 0.f && Local.X <= SizeView.X && Local.Y >= 0.f && Local.Y <= SizeView.Y);
}

// -------------------------
// View helpers
// -------------------------

void SShapeGraphEditorCanvas::FrameViewToShape()
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset || Asset->ContourUV.Num() < 1 || !bHasCachedPaintGeometry)
	{
		return;
	}

	const FVector2D WidgetSize = CachedPaintGeometry.GetLocalSize();
	if (WidgetSize.X <= KINDA_SMALL_NUMBER || WidgetSize.Y <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Compute bounds in DESIGN space (not widget-local).
	FVector2D MinD(FLT_MAX, FLT_MAX);
	FVector2D MaxD(-FLT_MAX, -FLT_MAX);

	for (const FVector2D& UV : Asset->ContourUV)
	{
		const FVector2D D = UVToDesign(UV);
		MinD.X = FMath::Min(MinD.X, D.X);
		MinD.Y = FMath::Min(MinD.Y, D.Y);
		MaxD.X = FMath::Max(MaxD.X, D.X);
		MaxD.Y = FMath::Max(MaxD.Y, D.Y);
	}

	const FVector2D Ext = (MaxD - MinD);
	const float ExtX = FMath::Max(Ext.X, 1.f);
	const float ExtY = FMath::Max(Ext.Y, 1.f);

	const float MarginPx = 30.f;
	const FVector2D TargetPx = WidgetSize - FVector2D(MarginPx * 2.f, MarginPx * 2.f);

	const float ZoomX = TargetPx.X / ExtX;
	const float ZoomY = TargetPx.Y / ExtY;

	ViewZoom = FMath::Clamp(FMath::Min(ZoomX, ZoomY), 0.1f, 20.f);

	const FVector2D BoundsCenterD = (MinD + MaxD) * 0.5f;
	const FVector2D WidgetCenterPx = WidgetSize * 0.5f;

	// View = Design*Zoom + Pan => Pan = Center - BoundsCenter*Zoom
	ViewPanPx = WidgetCenterPx - (BoundsCenterD * ViewZoom);

	Invalidate(EInvalidateWidget::Paint);
}

// -------------------------
// Grid Helpers
// -------------------------

void SShapeGraphEditorCanvas::ToggleGridSpace()
{
	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;
	Asset->GridSpace = (Asset->GridSpace == EShapeGridSpace::Design) ? EShapeGridSpace::UV : EShapeGridSpace::Design;
	Invalidate(EInvalidateWidget::Paint);
}

FText SShapeGraphEditorCanvas::GetGridSpaceLabel() const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return FText::FromString(TEXT("NONE"));
	const EShapeGridSpace GridSpace = Asset->GridSpace;
	return (GridSpace == EShapeGridSpace::Design)
		? FText::FromString(TEXT("Grid: PX"))
		: FText::FromString(TEXT("Grid: UV"));
}

// -------------------------
// Rendering
// -------------------------

void SShapeGraphEditorCanvas::DrawGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (!bShowGrid) return;

	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	const FVector2D CanvasSize = Asset->CanvasDesignSize;
	const EShapeGridSpace GridSpace = Asset->GridSpace;
	const float DesignStep = Asset->GridStepDesignPx;
	const float UVStep = Asset->GridStepUV;

	TArray<FVector2D> Line;
	Line.Reserve(2);

	auto DrawLine = [&](const FVector2D& A, const FVector2D& B)
		{
			Line.Reset();
			Line.Add(A);
			Line.Add(B);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				Line,
				ESlateDrawEffect::None,
				FLinearColor(1.f, 1.f, 1.f, 0.08f),
				true,
				1.0f
			);
		};

	if (GridSpace == EShapeGridSpace::Design)
	{
		const float Step = FMath::Max(DesignStep, 1.f);

		for (float X = 0.f; X <= CanvasSize.X + KINDA_SMALL_NUMBER; X += Step)
		{
			DrawLine(
				DesignToViewPx(FVector2D(X, 0.f)),
				DesignToViewPx(FVector2D(X, CanvasSize.Y))
			);
		}

		for (float Y = 0.f; Y <= CanvasSize.Y + KINDA_SMALL_NUMBER; Y += Step)
		{
			DrawLine(
				DesignToViewPx(FVector2D(0.f, Y)),
				DesignToViewPx(FVector2D(CanvasSize.X, Y))
			);
		}
	}
	else // UV
	{
		const float Step = FMath::Clamp(UVStep, 0.0001f, 1.f);

		for (float U = 0.f; U <= 1.f + KINDA_SMALL_NUMBER; U += Step)
		{
			DrawLine(UVToViewPx(FVector2D(U, 0.f)), UVToViewPx(FVector2D(U, 1.f)));
		}

		for (float V = 0.f; V <= 1.f + KINDA_SMALL_NUMBER; V += Step)
		{
			DrawLine(UVToViewPx(FVector2D(0.f, V)), UVToViewPx(FVector2D(1.f, V)));
		}
	}
}

void SShapeGraphEditorCanvas::UpdateBackgroundBrush(UTexture2D* Texture) const
{
	if (CachedBackgroundTexture.Get() == Texture)
	{
		return;
	}

	CachedBackgroundTexture = Texture;

	BackgroundBrush = FSlateBrush();
	BackgroundBrush.SetResourceObject(Texture);
	BackgroundBrush.ImageSize = FVector2D((float)Texture->GetSizeX(), (float)Texture->GetSizeY());
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
}

void SShapeGraphEditorCanvas::DrawBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	UTexture2D* Texture = Asset->BackgroundTexture.LoadSynchronous();
	if (!Texture) return;

	UpdateBackgroundBrush(Texture);

	const float Scale = FMath::Max(Asset->BackgroundScale, 0.001f);
	const FVector2D TexSize((float)Texture->GetSizeX(), (float)Texture->GetSizeY());
	const FVector2D SizeDesign = TexSize * Scale;

	// Camera mapping (same as outline/points)
	const FVector2D SizeView = SizeDesign * ViewZoom;
	if (SizeView.X <= 1.f || SizeView.Y <= 1.f) return;

	const float AngleRad = FMath::DegreesToRadians(Asset->BackgroundRotation);
	const float Alpha = FMath::Clamp(Asset->BackgroundOpacity / 100.f, 0.f, 1.f);

	// Desired (unrotated) top-left in view space
	const FVector2D TopLeftView = DesignToViewPx(Asset->BackgroundOffsetPx);

	// --- Correct TopLeft so that rotation happens around center ---
	const FVector2D Half = SizeView * 0.5f;
	const float CosA = FMath::Cos(AngleRad);
	const float SinA = FMath::Sin(AngleRad);

	const FVector2D RotHalf(
		Half.X * CosA - Half.Y * SinA,
		Half.X * SinA + Half.Y * CosA
	);

	// TL' = TL + Half - Rot(Half)
	const FVector2D TopLeftCorrected = TopLeftView + Half - RotHalf;

	// Use pivot at top-left (0,0) since we corrected TL
	const FVector2D Pivot(0.f, 0.f);

	FSlateDrawElement::MakeRotatedBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(
			SizeView,
			FSlateLayoutTransform(TopLeftCorrected)
		),
		&BackgroundBrush,
		ESlateDrawEffect::None,
		AngleRad,
		Pivot,
		FSlateDrawElement::RelativeToElement,
		FLinearColor(1.f, 1.f, 1.f, Alpha)
	);
}

void SShapeGraphEditorCanvas::DrawBackgroundSelectionOutline(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (!bBackgroundSelected) return;

	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	UTexture2D* Texture = Asset->BackgroundTexture.Get();
	if (!Texture) return;

	const FVector2D TexSize((float)Texture->GetSizeX(), (float)Texture->GetSizeY());
	const float Scale = FMath::Max(Asset->BackgroundScale, 0.001f);
	const FVector2D SizeDesign = TexSize * Scale;

	// DESIGN -> VIEW
	const FVector2D TopLeftView = (Asset->BackgroundOffsetPx * ViewZoom) + ViewPanPx;
	const FVector2D SizeView = SizeDesign * ViewZoom;

	const float Rad = FMath::DegreesToRadians(Asset->BackgroundRotation);
	const FVector2D Center = TopLeftView + (SizeView * 0.5f);

	auto Rot = [&](const FVector2D& P)
		{
			const float CosA = FMath::Cos(Rad);
			const float SinA = FMath::Sin(Rad);
			return FVector2D(P.X * CosA - P.Y * SinA, P.X * SinA + P.Y * CosA);
		};

	// corners around center
	const FVector2D Half = SizeView * 0.5f;
	const FVector2D C0 = Center + Rot(FVector2D(-Half.X, -Half.Y));
	const FVector2D C1 = Center + Rot(FVector2D(Half.X, -Half.Y));
	const FVector2D C2 = Center + Rot(FVector2D(Half.X, Half.Y));
	const FVector2D C3 = Center + Rot(FVector2D(-Half.X, Half.Y));

	TArray<FVector2D> Poly;
	Poly.Reserve(5);
	Poly.Add(C0); Poly.Add(C1); Poly.Add(C2); Poly.Add(C3); Poly.Add(C0);

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Poly,
		ESlateDrawEffect::None,
		FLinearColor::Yellow,
		true,
		1.5f
	);
}

void SShapeGraphEditorCanvas::ClearBackgroundSelection()
{
	if (bBackgroundSelected)
	{
		bBackgroundSelected = false;
		bDraggingBackground = false;
		Invalidate(EInvalidateWidget::Paint);
	}
}

// -------------------------
// Transactions / Asset change
// -------------------------

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

// -------------------------
// Slate input
// -------------------------

FReply SShapeGraphEditorCanvas::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Handled();
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
	const FVector2D MouseView = GetMouseLocal(MyGeometry, MouseEvent);
	LastMouseLocal = MouseView;

	FReply Reply = FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);

	// RMB drag pan
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bPanningView = true;
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
			const int32 Seg = HitTestSegment(MouseView, GSegmentHitRadiusPx, T);
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
					InsertIndex = N;
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
		const int32 Hit = HitTestPoint(MouseView, GPointHitRadiusPx);
		if (Hit != INDEX_NONE)
		{
			SelectedPointIndex = Hit;
			bDraggingPoint = true;
			DragPointIndex = Hit;

			BeginTransaction(NSLOCTEXT("ShapeTools", "MovePointTx", "Move Shape Point"));
			Invalidate(EInvalidateWidget::Paint);
			return Reply.CaptureMouse(SharedThis(this));
		}

		// Background select (if not locked)
		if (!Asset->bBackgroundLocked && HitTestBackground(MouseView))
		{
			bBackgroundSelected = true;
			SelectedPointIndex = INDEX_NONE;

			bDraggingBackground = true;
			BgDragStartMouseView = MouseView;
			BgDragStartOffsetDesign = Asset->BackgroundOffsetPx;

			BeginTransaction(NSLOCTEXT("ShapeTools", "MoveBackgroundTx", "Move Background"));
			return Reply.CaptureMouse(SharedThis(this));
		}
		else
		{
			// click outside -> deselect background
			if (bBackgroundSelected)
			{
				bBackgroundSelected = false;
				Invalidate(EInvalidateWidget::Paint);
			}
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
	if (HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bPanningView)
	{
		bPanningView = false;
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

	if (HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDraggingBackground)
	{
		bDraggingBackground = false;
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
	const FVector2D MouseView = GetMouseLocal(MyGeometry, MouseEvent);

	// Pan (RMB)
	if (bPanningView && HasMouseCapture())
	{
		const FVector2D Delta = MouseView - LastMouseLocal;
		ViewPanPx += Delta;
		LastMouseLocal = MouseView;

		Invalidate(EInvalidateWidget::Paint);
		return FReply::Handled();
	}

	const int32 PrevHoveredPoint = HoveredPointIndex;
	const int32 PrevHoveredSeg = HoveredSegmentIndex;

	HoveredPointIndex = HitTestPoint(MouseView, GPointHitRadiusPx);

	float SegT = 0.f;
	HoveredSegmentIndex = (HoveredPointIndex == INDEX_NONE)
		? HitTestSegment(MouseView, GSegmentHitRadiusPx, SegT)
		: INDEX_NONE;

	bool bNeedsRepaint = (HoveredPointIndex != PrevHoveredPoint) || (HoveredSegmentIndex != PrevHoveredSeg);

	// Drag point (block-at-bounds behavior)
	if (bDraggingPoint && HasMouseCapture())
	{
		if (UShapeGraphAsset* Asset = ShapeAsset.Get())
		{
			if (Asset->ContourUV.IsValidIndex(DragPointIndex))
			{
				FVector2D TargetUV = ViewPxToUV(MouseView);

				if (MouseEvent.IsControlDown())
				{
					const float Step = 0.01f;
					TargetUV.X = FMath::GridSnap(TargetUV.X, Step);
					TargetUV.Y = FMath::GridSnap(TargetUV.Y, Step);
				}

				const FVector2D CurrentUV = Asset->ContourUV[DragPointIndex];
				const FVector2D DeltaUV = TargetUV - CurrentUV;

				auto CanApplyDelta = [&](const FVector2D& D)
					{
						const FVector2D NewUV = Asset->ContourUV[DragPointIndex] + D;
						return !(NewUV.X < 0.f || NewUV.X > 1.f || NewUV.Y < 0.f || NewUV.Y > 1.f);
					};

				if (CanApplyDelta(DeltaUV))
				{
					Asset->ContourUV[DragPointIndex] += DeltaUV;
					bNeedsRepaint = true;
				}
			}
		}
	}

	if (bDraggingBackground && HasMouseCapture())
	{
		if (UShapeGraphAsset* Asset = ShapeAsset.Get())
		{
			if (!Asset->bBackgroundLocked)
			{
				const FVector2D DeltaView = MouseView - BgDragStartMouseView;
				const FVector2D DeltaDesign = DeltaView / ViewZoom;

				Asset->BackgroundOffsetPx = BgDragStartOffsetDesign + DeltaDesign;
				NotifyAssetChanged();
				Invalidate(EInvalidateWidget::Paint);
				return FReply::Handled();
			}
		}
	}

	LastMouseLocal = MouseView;

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

	const FVector2D MouseView = GetMouseLocal(MyGeometry, MouseEvent);

	const float ZoomFactor = FMath::Pow(1.1f, Wheel);
	const float NewZoom = FMath::Clamp(ViewZoom * ZoomFactor, 0.1f, 20.f);

	const FVector2D DesignUnderCursor = ViewPxToDesign(MouseView);

	ViewZoom = NewZoom;
	ViewPanPx = MouseView - (DesignUnderCursor * ViewZoom);

	Invalidate(EInvalidateWidget::Paint);
	return FReply::Handled();
}

// -------------------------
// Paint
// -------------------------

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

	DrawBackground(AllottedGeometry, OutDrawElements, LayerId + 0);
	DrawBackgroundSelectionOutline(AllottedGeometry, OutDrawElements, LayerId + 1);
	DrawGrid(AllottedGeometry, OutDrawElements, LayerId + 2);

	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset || Asset->ContourUV.Num() < 1)
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
			PointsPx.Add(UVToViewPx(UV));
		}

		// IMPORTANT: avoid aliasing crash (copy temp)
		if (Asset->bClosed && PointsPx.Num() > 0)
		{
			const FVector2D FirstPoint = PointsPx[0];
			PointsPx.Add(FirstPoint);
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 3,
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
		const FVector2D Ppx = UVToViewPx(UVc);

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
			LayerId + 4,
			BoxGeo.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			Col
		);
	}

	return LayerId + 4;
}