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
#include "Input/Events.h"
#include "InputCoreTypes.h"

static constexpr float GPointHitRadiusPx = 10.f;
static constexpr float GSegmentHitRadiusPx = 8.f;
static constexpr float GPointBoxSizePx = 6.f;

static float DistSq(const FVector2D& A, const FVector2D& B)
{
	return (A - B).SizeSquared();
}

static int32 GetAnySelectedVertexIndex(const FShapeSelection& Sel)
{
	for (int32 Idx : Sel.Vertices) return Idx;
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

static FVector2D ClampDeltaDesignToCanvas(
	const FVector2D& RawDeltaDesign,
	const TMap<int32, FVector2D>& InitialPosDesign,
	const FVector2D& CanvasSize)
{
	float MinDx = RawDeltaDesign.X;
	float MaxDx = RawDeltaDesign.X;
	float MinDy = RawDeltaDesign.Y;
	float MaxDy = RawDeltaDesign.Y;

	// On veut que pour tout point: 0<= (P0 + D) <= Canvas
	// => D >= -P0  et  D <= Canvas - P0
	float LoX = -BIG_NUMBER, HiX = BIG_NUMBER;
	float LoY = -BIG_NUMBER, HiY = BIG_NUMBER;

	for (const auto& KV : InitialPosDesign)
	{
		const FVector2D P0 = KV.Value;

		LoX = FMath::Max(LoX, -P0.X);
		HiX = FMath::Min(HiX, CanvasSize.X - P0.X);

		LoY = FMath::Max(LoY, -P0.Y);
		HiY = FMath::Min(HiY, CanvasSize.Y - P0.Y);
	}

	const float ClampedX = FMath::Clamp(RawDeltaDesign.X, LoX, HiX);
	const float ClampedY = FMath::Clamp(RawDeltaDesign.Y, LoY, HiY);

	return FVector2D(ClampedX, ClampedY);
}

static FVector2D ClampDeltaUVTo01(
	const FVector2D& DesiredDeltaUV,
	const TMap<int32, FVector2D>& InitialUV)
{
	float Dx = DesiredDeltaUV.X;
	float Dy = DesiredDeltaUV.Y;

	for (const TPair<int32, FVector2D>& KV : InitialUV)
	{
		const FVector2D P0 = KV.Value;

		if (P0.X + Dx < 0.f) Dx = 0.f - P0.X;
		if (P0.X + Dx > 1.f) Dx = 1.f - P0.X;

		if (P0.Y + Dy < 0.f) Dy = 0.f - P0.Y;
		if (P0.Y + Dy > 1.f) Dy = 1.f - P0.Y;
	}

	return FVector2D(Dx, Dy);
}

static float ComputeMaxUniformScaleKeepingInCanvas(
	const FVector2D& Pivot,
	const TMap<int32, FVector2D>& InitialPositions,
	const FVector2D& CanvasSize)
{
	const float MinX = 0.f, MinY = 0.f;
	const float MaxX = CanvasSize.X, MaxY = CanvasSize.Y;

	float MaxScale = BIG_NUMBER;

	for (const TPair<int32, FVector2D>& KV : InitialPositions)
	{
		const FVector2D P0 = KV.Value;
		const FVector2D O = P0 - Pivot;

		// X constraints
		if (!FMath::IsNearlyZero(O.X))
		{
			// Pivot + O.X * s in [MinX..MaxX]
			const float S1 = (MinX - Pivot.X) / O.X;
			const float S2 = (MaxX - Pivot.X) / O.X;

			const float SMin = FMath::Min(S1, S2);
			const float SMax = FMath::Max(S1, S2);

			// s must be within [SMin..SMax], and we assume s >= 0
			MaxScale = FMath::Min(MaxScale, SMax);
		}
		else
		{
			// O.X == 0 => must already be inside
			if (Pivot.X < MinX || Pivot.X > MaxX) return 0.f;
		}

		// Y constraints
		if (!FMath::IsNearlyZero(O.Y))
		{
			const float S1 = (MinY - Pivot.Y) / O.Y;
			const float S2 = (MaxY - Pivot.Y) / O.Y;

			const float SMax = FMath::Max(S1, S2);
			MaxScale = FMath::Min(MaxScale, SMax);
		}
		else
		{
			if (Pivot.Y < MinY || Pivot.Y > MaxY) return 0.f;
		}
	}

	// on empêche scale négatif (flip) pour l’instant
	MaxScale = FMath::Max(0.f, MaxScale);
	return MaxScale;
}

static bool AreAllPointsInsideCanvasAfterRotate(
	const FVector2D& Pivot,
	const TMap<int32, FVector2D>& InitialPositions,
	const FVector2D& CanvasSize,
	float DeltaDeg,
	float Eps = 0.01f) // tolérance anti-float
{
	const float MinX = 0.f - Eps;
	const float MinY = 0.f - Eps;
	const float MaxX = CanvasSize.X + Eps;
	const float MaxY = CanvasSize.Y + Eps;

	for (const TPair<int32, FVector2D>& KV : InitialPositions)
	{
		const FVector2D O = KV.Value - Pivot;
		const FVector2D P = Pivot + O.GetRotated(DeltaDeg);

		if (P.X < MinX || P.X > MaxX || P.Y < MinY || P.Y > MaxY)
		{
			return false;
		}
	}
	return true;
}

static FVector2D SnapDeltaDesignToGrid(
	const FVector2D& DeltaDesign,
	const UShapeGraphAsset* Asset)
{
	if (!Asset) return DeltaDesign;

	// Si ton asset a GridSpace + GridStepDesignPx + GridStepUV + CanvasDesignSize
	if (Asset->GridSpace == EShapeGridSpace::Design)
	{
		const float Step = FMath::Max(Asset->GridStepDesignPx, 1.f);
		return FVector2D(
			FMath::GridSnap(DeltaDesign.X, Step),
			FMath::GridSnap(DeltaDesign.Y, Step)
		);
	}
	else // UV grid
	{
		const float StepUV = FMath::Clamp(Asset->GridStepUV, 0.0001f, 1.f);
		const FVector2D Canvas = Asset->CanvasDesignSize;

		// Snap en UV puis reconvertir en Design => snap homogène
		const FVector2D DeltaUV(
			(Canvas.X > KINDA_SMALL_NUMBER) ? (DeltaDesign.X / Canvas.X) : 0.f,
			(Canvas.Y > KINDA_SMALL_NUMBER) ? (DeltaDesign.Y / Canvas.Y) : 0.f
		);

		const FVector2D SnappedUV(
			FMath::GridSnap(DeltaUV.X, StepUV),
			FMath::GridSnap(DeltaUV.Y, StepUV)
		);

		return FVector2D(SnappedUV.X * Canvas.X, SnappedUV.Y * Canvas.Y);
	}
}

static FVector2D SnapDeltaUVByPivotToActiveGrid(
	const FVector2D& RawDeltaUV,
	const UShapeGraphAsset* Asset,
	const FVector2D& PivotUV)
{
	if (!Asset) return RawDeltaUV;

	// Pivot target (absolute)
	const FVector2D TargetPivotUV = PivotUV + RawDeltaUV;

	if (Asset->GridSpace == EShapeGridSpace::Design)
	{
		const FVector2D Canvas = Asset->CanvasDesignSize;
		const float StepPx = FMath::Max(Asset->GridStepDesignPx, 1.f);

		const FVector2D TargetPivotDesign(TargetPivotUV.X * Canvas.X, TargetPivotUV.Y * Canvas.Y);

		const FVector2D SnappedPivotDesign(
			FMath::GridSnap(TargetPivotDesign.X, StepPx),
			FMath::GridSnap(TargetPivotDesign.Y, StepPx)
		);

		const FVector2D SnappedPivotUV(
			(Canvas.X > KINDA_SMALL_NUMBER) ? (SnappedPivotDesign.X / Canvas.X) : 0.f,
			(Canvas.Y > KINDA_SMALL_NUMBER) ? (SnappedPivotDesign.Y / Canvas.Y) : 0.f
		);

		return SnappedPivotUV - PivotUV; // Delta snapped
	}
	else // UV grid
	{
		const float StepUV = FMath::Clamp(Asset->GridStepUV, 0.0001f, 1.f);

		const FVector2D SnappedPivotUV(
			FMath::GridSnap(TargetPivotUV.X, StepUV),
			FMath::GridSnap(TargetPivotUV.Y, StepUV)
		);

		return SnappedPivotUV - PivotUV;
	}
}

static FVector2D SnapDeltaDesignByPivotToActiveGrid(
	const FVector2D& RawDeltaDesign,
	const UShapeGraphAsset* Asset,
	const FVector2D& PivotDesign)
{
	if (!Asset) return RawDeltaDesign;

	// Target pivot (absolute) in Design
	const FVector2D TargetPivotDesign = PivotDesign + RawDeltaDesign;

	if (Asset->GridSpace == EShapeGridSpace::Design)
	{
		const float StepPx = FMath::Max(Asset->GridStepDesignPx, 1.f);

		const FVector2D SnappedPivotDesign(
			FMath::GridSnap(TargetPivotDesign.X, StepPx),
			FMath::GridSnap(TargetPivotDesign.Y, StepPx)
		);

		return SnappedPivotDesign - PivotDesign;
	}
	else // UV grid
	{
		const float StepUV = FMath::Clamp(Asset->GridStepUV, 0.0001f, 1.f);
		const FVector2D Canvas = Asset->CanvasDesignSize;

		// Design -> UV
		const FVector2D TargetPivotUV(
			(Canvas.X > KINDA_SMALL_NUMBER) ? (TargetPivotDesign.X / Canvas.X) : 0.f,
			(Canvas.Y > KINDA_SMALL_NUMBER) ? (TargetPivotDesign.Y / Canvas.Y) : 0.f
		);

		const FVector2D SnappedPivotUV(
			FMath::GridSnap(TargetPivotUV.X, StepUV),
			FMath::GridSnap(TargetPivotUV.Y, StepUV)
		);

		// UV -> Design
		const FVector2D SnappedPivotDesign(
			SnappedPivotUV.X * Canvas.X,
			SnappedPivotUV.Y * Canvas.Y
		);

		return SnappedPivotDesign - PivotDesign;
	}
}

void SShapeGraphEditorCanvas::Construct(const FArguments& InArgs)
{
	ShapeAsset = InArgs._ShapeAsset;
	SetCanTick(false);
}

void SShapeGraphEditorCanvas::SetHUD(const FString& L1, const FString& L2)
{
	HUDLine1 = L1;
	HUDLine2 = L2;
	bShowHUD = !L1.IsEmpty();
}

/* --------------------------
   Asset read/write
   -------------------------- */

void SShapeGraphEditorCanvas::SyncPivotModeFromAsset()
{
	if (!ShapeAsset.IsValid())
	{
		return;
	}

	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset)
	{
		return;
	}

	PivotMode = Asset->PivotMode;

	Invalidate(EInvalidateWidget::Paint);
}

FVector2D SShapeGraphEditorCanvas::GetVertexDesignPosition(int32 Index) const
{
	if (!ShapeAsset.IsValid()) return FVector2D::ZeroVector;
	if (!ShapeAsset->ContourUV.IsValidIndex(Index)) return FVector2D::ZeroVector;

	const FVector2D UV = ShapeAsset->ContourUV[Index];
	return UVToDesign(UV);
}

void SShapeGraphEditorCanvas::SetVertexDesignPosition(int32 Index, const FVector2D& NewDesign)
{
	if (!ShapeAsset.IsValid()) return;
	if (!ShapeAsset->ContourUV.IsValidIndex(Index)) return;

	ShapeAsset->Modify();

	const FVector2D NewUV = DesignToUV(NewDesign);
	ShapeAsset->ContourUV[Index] = NewUV;
}

void SShapeGraphEditorCanvas::SetBackgroundOffsetDesign(const FVector2D& OffsetDesign)
{
	if (!ShapeAsset.IsValid()) return;
	ShapeAsset->Modify();
	ShapeAsset->BackgroundOffsetPx = OffsetDesign;
}

void SShapeGraphEditorCanvas::SetBackgroundScale(float Scale)
{
	if (!ShapeAsset.IsValid()) return;
	ShapeAsset->Modify();
	ShapeAsset->BackgroundScale = Scale;
}

void SShapeGraphEditorCanvas::SetBackgroundRotationDeg(float Deg)
{
	if (!ShapeAsset.IsValid()) return;
	ShapeAsset->Modify();
	ShapeAsset->BackgroundRotation = Deg;
}

void SShapeGraphEditorCanvas::HandleAssetPropertyChanged(FName PropertyName)
{
	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	// Si background devient locké -> auto-deselect + stop drag
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UShapeGraphAsset, bBackgroundLocked))
	{
		if (Asset->bBackgroundLocked)
		{
			Selection.bBackgroundSelected = false;
			bDraggingBackground = false;

			// reset intents si besoin
			bDragIntentBackground = false;
		}
	}

	// Si texture background retirée -> pas de sélection possible
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UShapeGraphAsset, BackgroundTexture))
	{
		if (!Asset->BackgroundTexture.Get())
		{
			Selection.bBackgroundSelected = false;
			bDraggingBackground = false;
		}
	}

	Invalidate(EInvalidateWidget::Paint);
}

// -------------------------
// Input Space (DPI-safe cursor)
// -------------------------

FVector2D SShapeGraphEditorCanvas::GetCursorLocalPx(const FGeometry& MyGeometry) const
{
	FSlateApplication& App = FSlateApplication::Get();
	const FGeometry& Geo = bHasCachedPaintGeometry ? CachedPaintGeometry : MyGeometry;

	const TSharedPtr<ICursor> PlatformCursor = App.GetPlatformCursor();
	if (!PlatformCursor.IsValid())
	{
		return Geo.AbsoluteToLocal(App.GetCursorPos());
	}

	const FVector2D DesktopPx = PlatformCursor->GetPosition();
	const TSharedPtr<SWindow> Window = App.FindWidgetWindow(AsShared());
	if (!Window.IsValid())
	{
		return Geo.AbsoluteToLocal(App.GetCursorPos());
	}

	const FVector2D WindowDesktopPx = Window->GetPositionInScreen();
	const float AppScale = App.GetApplicationScale();
	const FVector2D WindowSpaceSlate = (DesktopPx - WindowDesktopPx) / AppScale;

	return Geo.AbsoluteToLocal(WindowSpaceSlate);
}

FVector2D SShapeGraphEditorCanvas::GetMouseLocalPx(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	FSlateApplication& App = FSlateApplication::Get();
	const FGeometry& Geo = bHasCachedPaintGeometry ? CachedPaintGeometry : MyGeometry;

	// Prefer platform cursor when available (most reliable during captures / DPI)
	const TSharedPtr<ICursor> PlatformCursor = App.GetPlatformCursor();
	if (PlatformCursor.IsValid())
	{
		const TSharedPtr<SWindow> Window = App.FindWidgetWindow(AsShared());
		if (Window.IsValid())
		{
			const FVector2D DesktopPx = PlatformCursor->GetPosition();
			const FVector2D WindowDesktopPx = Window->GetPositionInScreen();
			const float AppScale = App.GetApplicationScale();

			// Convert desktop pixels -> Slate window space (DPI aware), then -> local
			const FVector2D WindowSpaceSlate = (DesktopPx - WindowDesktopPx) / AppScale;
			return Geo.AbsoluteToLocal(WindowSpaceSlate);
		}

		// Window not found: fall back to Slate cursor pos
		return Geo.AbsoluteToLocal(App.GetCursorPos());
	}

	// No platform cursor: fall back to event screen space (still OK)
	return Geo.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
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
	if (!Selection.bBackgroundSelected) return;

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

void SShapeGraphEditorCanvas::DrawPivot(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	if (Selection.Vertices.Num() < 2 && !Selection.bBackgroundSelected)
	{
		return;
	}

	const FVector2D PivotDesign = ComputePivotDesign();
	const FVector2D PivotView = DesignToViewPx(PivotDesign);

	const float Cross = 7.f;
	TArray<FVector2D> L;

	// Horizontal
	L.Reset();
	L.Add(PivotView + FVector2D(-Cross, 0.f));
	L.Add(PivotView + FVector2D(+Cross, 0.f));
	FSlateDrawElement::MakeLines(
		OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), L,
		ESlateDrawEffect::None, FLinearColor(0.2f, 0.9f, 1.f, 1.f), true, 1.5f);

	// Vertical
	L.Reset();
	L.Add(PivotView + FVector2D(0.f, -Cross));
	L.Add(PivotView + FVector2D(0.f, +Cross));
	FSlateDrawElement::MakeLines(
		OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), L,
		ESlateDrawEffect::None, FLinearColor(0.2f, 0.9f, 1.f, 1.f), true, 1.5f);

	// Petit carré au centre
	const FVector2D BoxSize(4.f, 4.f);
	const FVector2D TopLeft = PivotView - BoxSize * 0.5f;

	const FGeometry BoxGeo = AllottedGeometry.MakeChild(
		BoxSize,
		FSlateLayoutTransform(TopLeft));

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		BoxGeo.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.f, 0.f, 0.f, 0.75f));
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
	if (Selection.Vertices.Num() == 0) return;

	TArray<int32> Indices = Selection.Vertices.Array();
	Indices.Sort(TGreater<int32>());

	const FScopedTransaction Tx(NSLOCTEXT("ShapeTools", "DeletePointsTx", "Delete Shape Points"));
	Asset->Modify();

	for (int32 Idx : Indices)
	{
		if (Asset->ContourUV.IsValidIndex(Idx))
		{
			Asset->ContourUV.RemoveAt(Idx);
		}
	}

	Selection.Clear();
	NotifyAssetChanged();
	Invalidate(EInvalidateWidget::Paint);
}

/* --------------------------
   Selection ops
   -------------------------- */

void SShapeGraphEditorCanvas::ClearSelection()
{
	Selection.Clear();
}

void SShapeGraphEditorCanvas::SelectVertexExclusive(int32 Index)
{
	Selection.Clear();
	Selection.Vertices.Add(Index);
}

void SShapeGraphEditorCanvas::AddVertex(int32 Index)
{
	Selection.Vertices.Add(Index);
}

void SShapeGraphEditorCanvas::RemoveVertex(int32 Index)
{
	Selection.Vertices.Remove(Index);
}

void SShapeGraphEditorCanvas::ToggleVertex(int32 Index)
{
	if (Selection.Vertices.Contains(Index)) Selection.Vertices.Remove(Index);
	else Selection.Vertices.Add(Index);
}

void SShapeGraphEditorCanvas::SelectBackgroundExclusive()
{
	Selection.Clear();
	Selection.bBackgroundSelected = true;
}

void SShapeGraphEditorCanvas::ToggleBackground()
{
	Selection.bBackgroundSelected = !Selection.bBackgroundSelected;
}

void SShapeGraphEditorCanvas::BeginBoxSelect(const FGeometry& Geo, const FVector2D& MouseViewPx)
{
	bBoxSelecting = true;
	BoxStartDesign = ViewPxToDesign(MouseViewPx);
	BoxEndDesign = BoxStartDesign;

	Invalidate(EInvalidateWidget::Paint);
}

void SShapeGraphEditorCanvas::UpdateBoxSelect(const FGeometry& Geo, const FVector2D& MouseViewPx)
{
	if (!bBoxSelecting) return;
	BoxEndDesign = ViewPxToDesign(MouseViewPx);
	Invalidate(EInvalidateWidget::Paint);
}

void SShapeGraphEditorCanvas::EndBoxSelect(bool bAdditive)
{
	if (!bBoxSelecting) return;
	bBoxSelecting = false;

	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	const FVector2D Min(
		FMath::Min(BoxStartDesign.X, BoxEndDesign.X),
		FMath::Min(BoxStartDesign.Y, BoxEndDesign.Y)
	);
	const FVector2D Max(
		FMath::Max(BoxStartDesign.X, BoxEndDesign.X),
		FMath::Max(BoxStartDesign.Y, BoxEndDesign.Y)
	);

	if (!bAdditive)
	{
		Selection.Vertices.Reset();
		Selection.bBackgroundSelected = false;
	}

	for (int32 i = 0; i < Asset->ContourUV.Num(); ++i)
	{
		const FVector2D P = GetVertexDesignPosition(i);
		if (P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y)
		{
			Selection.Vertices.Add(i);
		}
	}

	FSlateApplication::Get().ReleaseAllPointerCapture();
	Invalidate(EInvalidateWidget::Paint);
}

/* --------------------------
   Pivot
   -------------------------- */

FVector2D SShapeGraphEditorCanvas::ComputePivotDesign() const
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return FVector2D::ZeroVector;

	const EShapePivotMode EffectiveMode = Asset->PivotMode;

	if (Selection.HasVertices())
	{
		if (EffectiveMode == EShapePivotMode::MedianPoint)
		{
			FVector2D Sum(0, 0);
			int32 Count = 0;

			for (int32 Idx : Selection.Vertices)
			{
				// safety
				if (!Asset->ContourUV.IsValidIndex(Idx)) continue;

				Sum += GetVertexDesignPosition(Idx);
				++Count;
			}

			return (Count > 0) ? (Sum / float(Count)) : FVector2D::ZeroVector;
		}
		else
		{
			FVector2D Min(+BIG_NUMBER, +BIG_NUMBER);
			FVector2D Max(-BIG_NUMBER, -BIG_NUMBER);
			bool bAny = false;

			for (int32 Idx : Selection.Vertices)
			{
				if (!Asset->ContourUV.IsValidIndex(Idx)) continue;

				const FVector2D P = GetVertexDesignPosition(Idx);
				Min.X = FMath::Min(Min.X, P.X);
				Min.Y = FMath::Min(Min.Y, P.Y);
				Max.X = FMath::Max(Max.X, P.X);
				Max.Y = FMath::Max(Max.Y, P.Y);
				bAny = true;
			}

			return bAny ? ((Min + Max) * 0.5f) : FVector2D::ZeroVector;
		}
	}

	// Background seul
	if (Selection.bBackgroundSelected)
	{
		UTexture2D* Texture = Asset->BackgroundTexture.Get();
		if (Texture)
		{
			const FVector2D TexSize((float)Texture->GetSizeX(), (float)Texture->GetSizeY());
			const float Scale = FMath::Max(Asset->BackgroundScale, 0.001f);
			const FVector2D SizeDesign = TexSize * Scale;
			const FVector2D TopLeft = Asset->BackgroundOffsetPx; // rename conseillé: OffsetDesign
			return TopLeft + SizeDesign * 0.5f;
		}
	}

	return FVector2D::ZeroVector;
}

/* --------------------------
   Modal transform
   -------------------------- */

void SShapeGraphEditorCanvas::BeginTransform(EShapeTransformMode Mode, const FGeometry& Geo, const FVector2D& MouseViewPx)
{
	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;
	if (TransformSession.bActive) return;

	if (Selection.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Canvas] BeginTransform refused: Selection empty"));
		return;
	}

	SyncPivotModeFromAsset();

	TransformSession.Reset();
	TransformSession.bActive = true;
	TransformSession.Mode = Mode;
	UpdateTransformModifiers();

	TransformSession.MouseStartDesign = ViewPxToDesign(MouseViewPx);
	TransformSession.PivotDesign = ComputePivotDesign();

	if (Mode == EShapeTransformMode::Rotate)
	{
		const FVector2D A = TransformSession.MouseStartDesign - TransformSession.PivotDesign;
		TransformSession.RotateStartAngleRad = FMath::Atan2(A.Y, A.X);
		TransformSession.RotateLastAngleRad = TransformSession.RotateStartAngleRad;
		TransformSession.RotateAccumulatedDeltaRad = 0.f;
	}

	for (int32 Idx : Selection.Vertices)
	{
		TransformSession.InitialVertexDesignPositions.Add(Idx, GetVertexDesignPosition(Idx));
	}

	TransformSession.bInitialBackgroundSelected = Selection.bBackgroundSelected;
	if (Selection.bBackgroundSelected)
	{
		TransformSession.InitialBackgroundOffsetDesign = Asset->BackgroundOffsetPx;
		TransformSession.InitialBackgroundScale = Asset->BackgroundScale;
		TransformSession.InitialBackgroundRotationDeg = Asset->BackgroundRotation;
	}

	switch (Mode)
	{
	case EShapeTransformMode::Translate:
		BeginTransaction(NSLOCTEXT("ShapeTools", "Tx_MoveSelection", "Move Selection"));
		break;
	case EShapeTransformMode::Scale:
		BeginTransaction(NSLOCTEXT("ShapeTools", "Tx_ScaleSelection", "Scale Selection"));
		break;
	case EShapeTransformMode::Rotate:
		BeginTransaction(NSLOCTEXT("ShapeTools", "Tx_RotateSelection", "Rotate Selection"));
		break;
	default:
		break;
	}

	FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);
}

void SShapeGraphEditorCanvas::UpdateTransform(const FGeometry& Geo, const FVector2D& MouseViewPx)
{
	if (!TransformSession.bActive) return;

	UpdateTransformModifiers();

	const FVector2D MouseDesign = ViewPxToDesign(MouseViewPx);
	bShowHUD = true;

	switch (TransformSession.Mode)
	{
	case EShapeTransformMode::Translate: ApplyTranslate(MouseDesign); break;
	case EShapeTransformMode::Scale:     ApplyScaleUniform(MouseDesign); break;
	case EShapeTransformMode::Rotate:    ApplyRotate(MouseDesign); break;
	default: break;
	}

	switch (TransformSession.Mode)
	{
	case EShapeTransformMode::Translate: HUDLine1 = TEXT("Move"); break;
	case EShapeTransformMode::Scale:     HUDLine1 = TEXT("Scale"); break;
	case EShapeTransformMode::Rotate:    HUDLine1 = TEXT("Rotate"); break;
	default:                             HUDLine1.Reset(); break;
	}

	Invalidate(EInvalidateWidget::Paint);
}

void SShapeGraphEditorCanvas::ConfirmTransform()
{
	if (!TransformSession.bActive) return;

	bShowHUD = false;
	HUDLine1.Reset();
	HUDLine2.Reset();

	TransformSession.Reset();
	EndTransaction();
	NotifyAssetChanged();

	FSlateApplication::Get().ReleaseAllPointerCapture();
	Invalidate(EInvalidateWidget::Paint);

	if (HasMouseCapture())
	{
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
}

void SShapeGraphEditorCanvas::CancelTransform()
{
	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;
	if (!TransformSession.bActive) return;

	bShowHUD = false;
	HUDLine1.Reset();
	HUDLine2.Reset();

	Asset->Modify();

	for (const TPair<int32, FVector2D>& KV : TransformSession.InitialVertexDesignPositions)
	{
		SetVertexDesignPosition(KV.Key, KV.Value);
	}

	if (TransformSession.bInitialBackgroundSelected)
	{
		SetBackgroundOffsetDesign(TransformSession.InitialBackgroundOffsetDesign);
		SetBackgroundScale(TransformSession.InitialBackgroundScale);
		SetBackgroundRotationDeg(TransformSession.InitialBackgroundRotationDeg);
	}

	TransformSession.Reset();
	EndTransaction();
	NotifyAssetChanged();

	FSlateApplication::Get().ReleaseAllPointerCapture();
	Invalidate(EInvalidateWidget::Paint);

	if (HasMouseCapture())
	{
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
}

/* --------------------------
   Apply transforms (Design)
   -------------------------- */

void SShapeGraphEditorCanvas::ApplyTranslate(const FVector2D& MouseDesign)
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	const FVector2D RawDelta = MouseDesign - TransformSession.MouseStartDesign;
	FVector2D Delta = RawDelta;

	// Ctrl = snap pivot to active grid (Design/UV)
	if (TransformSession.bCtrlDown)
	{
		Delta = SnapDeltaDesignByPivotToActiveGrid(RawDelta, Asset, TransformSession.PivotDesign);
	}

	if (TransformSession.InitialVertexDesignPositions.Num() > 0)
	{
		Delta = ClampDeltaDesignToCanvas(
			Delta,
			TransformSession.InitialVertexDesignPositions,
			Asset->CanvasDesignSize
		);

		HUDLine2 = FString::Printf(TEXT("Design: %.1f, %.1f"), Delta.X, Delta.Y);
	}

	for (const TPair<int32, FVector2D>& KV : TransformSession.InitialVertexDesignPositions)
	{
		SetVertexDesignPosition(KV.Key, KV.Value + Delta);
	}

	// background libre MAIS suit le delta clampé (cohérence visuelle)
	if (Selection.bBackgroundSelected)
	{
		SetBackgroundOffsetDesign(TransformSession.InitialBackgroundOffsetDesign + Delta);
	}

	bShowHUD = true;
	HUDLine1 = TEXT("Move");
	HUDLine2 = FString::Printf(TEXT("Design: %.1f, %.1f"), Delta.X, Delta.Y);
}

void SShapeGraphEditorCanvas::ApplyScaleUniform(const FVector2D& MouseDesign)
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	const FVector2D Pivot = TransformSession.PivotDesign;

	const float StartDist = (TransformSession.MouseStartDesign - Pivot).Size();
	const float CurrDist = (MouseDesign - Pivot).Size();
	float ScaleFactor = (StartDist > KINDA_SMALL_NUMBER) ? (CurrDist / StartDist) : 1.f;

	// Ctrl = scale snap (ex: pas de 0.05)
	if (TransformSession.bShiftDown)
	{
		const float Step = 0.05f;
		ScaleFactor = FMath::GridSnap(ScaleFactor, Step);
		ScaleFactor = FMath::Max(0.f, ScaleFactor);
	}

	// clamp max scale pour rester dans canvas
	if (TransformSession.InitialVertexDesignPositions.Num() > 0)
	{
		const float MaxScale = ComputeMaxUniformScaleKeepingInCanvas(
			Pivot,
			TransformSession.InitialVertexDesignPositions,
			Asset->CanvasDesignSize
		);

		ScaleFactor = FMath::Clamp(ScaleFactor, 0.f, MaxScale);

		HUDLine2 = FString::Printf(TEXT("Scale: %.4f"), ScaleFactor);
	}

	for (const TPair<int32, FVector2D>& KV : TransformSession.InitialVertexDesignPositions)
	{
		const FVector2D Offset = KV.Value - Pivot;
		SetVertexDesignPosition(KV.Key, Pivot + Offset * ScaleFactor);
	}

	// background libre mais suit le facteur clampé pour rester “couplé”
	if (Selection.bBackgroundSelected)
	{
		SetBackgroundScale(TransformSession.InitialBackgroundScale * ScaleFactor);

		const FVector2D BgOffset = TransformSession.InitialBackgroundOffsetDesign - Pivot;
		SetBackgroundOffsetDesign(Pivot + BgOffset * ScaleFactor);
	}

	bShowHUD = true;
	HUDLine1 = TEXT("Scale");
	HUDLine2 = FString::Printf(TEXT("Factor: %.3f"), ScaleFactor);
}

void SShapeGraphEditorCanvas::ApplyRotate(const FVector2D& MouseDesign)
{
	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset) return;

	const FVector2D Pivot = TransformSession.PivotDesign;

	// Angle courant autour du pivot
	const FVector2D B = MouseDesign - Pivot;
	const float CurrAngleRad = FMath::Atan2(B.Y, B.X);

	// Delta continu (pas de wrap)
	const float StepRad = FMath::FindDeltaAngleRadians(TransformSession.RotateLastAngleRad, CurrAngleRad);
	TransformSession.RotateAccumulatedDeltaRad += StepRad;
	TransformSession.RotateLastAngleRad = CurrAngleRad;

	float DeltaDeg = FMath::RadiansToDegrees(TransformSession.RotateAccumulatedDeltaRad);

	// Shift = angle snap
	if (TransformSession.bShiftDown)
	{
		DeltaDeg = FMath::GridSnap(DeltaDeg, 5.f);
	}

	bShowHUD = true;
	HUDLine1 = TEXT("Rotate");
	HUDLine2 = FString::Printf(TEXT("Angle: %.2f°%s"),
		DeltaDeg,
		TransformSession.bShiftDown ? TEXT(" (snap)") : TEXT(""));

	// Clamp rotation to keep points inside canvas (binary search)
	if (TransformSession.InitialVertexDesignPositions.Num() > 0)
	{
		const auto& Init = TransformSession.InitialVertexDesignPositions;

		// Anchor check: 0 must be valid, otherwise clamping is ill-defined
		const bool bZeroValid = AreAllPointsInsideCanvasAfterRotate(Pivot, Init, Asset->CanvasDesignSize, 0.f);
		if (bZeroValid)
		{
			const bool bDesiredValid = AreAllPointsInsideCanvasAfterRotate(Pivot, Init, Asset->CanvasDesignSize, DeltaDeg);
			if (!bDesiredValid)
			{
				// We search in [0 .. DeltaDeg] (or [DeltaDeg .. 0] if negative)
				float Lo = 0.f;
				float Hi = DeltaDeg;

				if (DeltaDeg < 0.f)
				{
					Lo = DeltaDeg;
					Hi = 0.f;
				}

				// Invariant: Hi (closer to 0) should remain valid, Lo may be invalid depending on sign
				// Ensure the endpoint at 0 is our "valid" side:
				// We'll converge toward the largest magnitude angle that is still valid.
				float Best = 0.f;

				for (int32 Iter = 0; Iter < 14; ++Iter) // a bit more precision
				{
					const float Mid = (Lo + Hi) * 0.5f;
					if (AreAllPointsInsideCanvasAfterRotate(Pivot, Init, Asset->CanvasDesignSize, Mid))
					{
						Best = Mid;

						// Move toward desired (increase magnitude)
						if (DeltaDeg >= 0.f) Lo = Mid;
						else                 Hi = Mid;
					}
					else
					{
						// Move back toward 0
						if (DeltaDeg >= 0.f) Hi = Mid;
						else                 Lo = Mid;
					}
				}

				DeltaDeg = Best;
				HUDLine2 = FString::Printf(TEXT("Angle: %.2f° (clamped)"), DeltaDeg);
			}
		}
		else
		{
			// If 0° is invalid, do not attempt to clamp rotation (or force to 0).
			// Safer UX: allow rotation but you'll still be out-of-bounds anyway.
			// Alternatively: DeltaDeg = 0.f;
			HUDLine2 = TEXT("Angle clamp skipped (invalid baseline)");
		}
	}

	// Apply to vertices
	for (const TPair<int32, FVector2D>& KV : TransformSession.InitialVertexDesignPositions)
	{
		const FVector2D Offset = KV.Value - Pivot;
		SetVertexDesignPosition(KV.Key, Pivot + Offset.GetRotated(DeltaDeg));
	}

	// Background is free (but you currently rotate it too; keep if you want parity)
	if (Selection.bBackgroundSelected)
	{
		SetBackgroundRotationDeg(TransformSession.InitialBackgroundRotationDeg + DeltaDeg);
	}
}

// -------------------------
// Slate input
// -------------------------

void SShapeGraphEditorCanvas::UpdateTransformModifiers()
{
	const FModifierKeysState Mods = FSlateApplication::Get().GetModifierKeys();
	TransformSession.bCtrlDown = Mods.IsControlDown();
	TransformSession.bShiftDown = Mods.IsShiftDown();
}

FReply SShapeGraphEditorCanvas::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Handled().SetUserFocus(SharedThis(this), InFocusEvent.GetCause());
}

FReply SShapeGraphEditorCanvas::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	const TCHAR Ch = InCharacterEvent.GetCharacter();

	if (!TransformSession.bActive)
	{
		if (Ch == 'g' || Ch == 'G')
		{
			BeginTransform(EShapeTransformMode::Translate, MyGeometry, GetCursorLocalPx(MyGeometry));
			return FReply::Handled();
		}
		if (Ch == 's' || Ch == 'S')
		{
			BeginTransform(EShapeTransformMode::Scale, MyGeometry, GetCursorLocalPx(MyGeometry));
			return FReply::Handled();
		}
		if (Ch == 'r' || Ch == 'R')
		{
			BeginTransform(EShapeTransformMode::Rotate, MyGeometry, GetCursorLocalPx(MyGeometry));
			return FReply::Handled();
		}
	}

	return SCompoundWidget::OnKeyChar(MyGeometry, InCharacterEvent);
}

FReply SShapeGraphEditorCanvas::OnPreviewKeyDown(
	const FGeometry& MyGeometry,
	const FKeyEvent& InKeyEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("[Canvas] PreviewKeyDown: %s  HasKeyboardFocus=%d  HasUserFocus=%d"),
		*InKeyEvent.GetKey().ToString(),
		HasKeyboardFocus() ? 1 : 0,
		HasUserFocus(FSlateApplication::Get().GetUserIndexForKeyboard()) ? 1 : 0);
	return OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SShapeGraphEditorCanvas::OnKeyDown(
	const FGeometry& MyGeometry,
	const FKeyEvent& InKeyEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("[Canvas] KeyDown: %s"), *InKeyEvent.GetKey().ToString());

	const FKey Key = InKeyEvent.GetKey();

	// Modal confirm/cancel
	if (TransformSession.bActive)
	{
		if (Key == EKeys::Escape)
		{
			CancelTransform();
			return FReply::Handled();
		}
		if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
		{
			ConfirmTransform();
			return FReply::Handled();
		}
	}

	// Local hotkeys (avant CommandList)
	if (!TransformSession.bActive)
	{
		if (Key == EKeys::G)
		{
			const FVector2D CursorLocal = GetCursorLocalPx(MyGeometry);
			BeginTransform(EShapeTransformMode::Translate, MyGeometry, CursorLocal);
			if (TransformSession.bActive)
			{
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
			return FReply::Handled();
		}

		if (Key == EKeys::S)
		{
			const FVector2D CursorLocal = GetCursorLocalPx(MyGeometry);
			BeginTransform(EShapeTransformMode::Scale, MyGeometry, CursorLocal);
			if (TransformSession.bActive)
			{
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
			return FReply::Handled();
		}

		if (Key == EKeys::R)
		{
			const FVector2D CursorLocal = GetCursorLocalPx(MyGeometry);
			BeginTransform(EShapeTransformMode::Rotate, MyGeometry, CursorLocal);
			if (TransformSession.bActive)
			{
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
			return FReply::Handled();
		}
	}

	// CommandList ensuite (ton toolkit peut binder d'autres trucs)
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SShapeGraphEditorCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D MouseView = GetMouseLocalPx(MyGeometry, MouseEvent);
	LastMouseLocalPx = MouseView;

	// Focus clavier explicite (UE5.4)
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::Mouse);

	FReply Reply = FReply::Handled();

	UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset)
	{
		return Reply;
	}

	// Modal confirm/cancel
	if (TransformSession.bActive)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			ConfirmTransform();
			return Reply;
		}
		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			CancelTransform();
			return Reply;
		}
		return Reply;
	}

	// RMB pan
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bPanningView = true;
		return Reply.CaptureMouse(SharedThis(this));
	}

	// LMB
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Ctrl+LMB near segment -> insert point (kept)
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

				// Selection: pick inserted vertex
				SelectVertexExclusive(InsertIndex);

				NotifyAssetChanged();
				Invalidate(EInvalidateWidget::Paint);
				return Reply;
			}
		}

		// Threshold intent setup (Blender-like)
		bLMBDown = true;
		bDragIntentVertex = false;
		bDragIntentEmpty = false;
		bDragIntentBackground = false;
		LMBDownHitVertexIndex = INDEX_NONE;
		LMBDownMouseViewPx = MouseView;

		const bool bShift = MouseEvent.IsShiftDown();

		// Vertex hit ?
		const int32 Hit = HitTestPoint(MouseView, GPointHitRadiusPx);
		if (Hit != INDEX_NONE)
		{
			LMBDownHitVertexIndex = Hit;
			bDragIntentVertex = true;

			// Blender-like:
			// - Shift+click toggles membership
			// - Click on already-selected vertex keeps the whole selection (for group drag)
			// - Click on non-selected vertex selects it exclusively
			if (bShift)
			{
				ToggleVertex(Hit);

				// Si tu toggles OFF le dernier vertex, tu peux garder une selection vide, c'est ok.
				// Si tu toggles ON, la selection inclut Hit et tu pourras drag un groupe.
			}
			else
			{
				if (!Selection.Vertices.Contains(Hit))
				{
					SelectVertexExclusive(Hit);
				}
				// else: keep selection unchanged
			}

			Invalidate(EInvalidateWidget::Paint);
			return Reply; // pas de capture, pas de transaction encore (threshold)
		}

		// Background hit ? (if not locked)
		if (!Asset->bBackgroundLocked && HitTestBackground(MouseView))
		{
			bDragIntentBackground = true;

			if (bShift) ToggleBackground();
			else SelectBackgroundExclusive();

			Invalidate(EInvalidateWidget::Paint);
			return Reply; // no capture yet
		}

		// Empty click
		bDragIntentEmpty = true;

		if (!bShift)
		{
			ClearSelection();
		}

		Invalidate(EInvalidateWidget::Paint);
		return Reply; // box starts only after threshold
	}

	return FReply::Unhandled();
}

FReply SShapeGraphEditorCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// RMB pan end
	if (HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bPanningView)
	{
		bPanningView = false;
		return FReply::Handled()
			.SetUserFocus(SharedThis(this), EFocusCause::Mouse)
			.ReleaseMouseCapture();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Reset threshold intent flags
		bLMBDown = false;
		bDragIntentVertex = false;
		bDragIntentEmpty = false;
		bDragIntentBackground = false;
		LMBDownHitVertexIndex = INDEX_NONE;

		// End selection drag
		if (bDraggingSelection && HasMouseCapture())
		{
			bDraggingSelection = false;
			DragAnchorIndex = INDEX_NONE;
			DragInitialUV.Reset();

			EndTransaction();
			NotifyAssetChanged();
			Invalidate(EInvalidateWidget::Paint);

			bShowHUD = false;
			HUDLine1.Reset();
			HUDLine2.Reset();

			return FReply::Handled()
				.SetUserFocus(SharedThis(this), EFocusCause::Mouse)
				.ReleaseMouseCapture();
		}

		// End background drag
		if (bDraggingBackground && HasMouseCapture())
		{
			bDraggingBackground = false;

			EndTransaction();
			NotifyAssetChanged();
			Invalidate(EInvalidateWidget::Paint);

			bShowHUD = false;
			HUDLine1.Reset();
			HUDLine2.Reset();

			return FReply::Handled()
				.SetUserFocus(SharedThis(this), EFocusCause::Mouse)
				.ReleaseMouseCapture();
		}

		// End box select
		if (bBoxSelecting && HasMouseCapture())
		{
			EndBoxSelect(MouseEvent.IsShiftDown());

			bShowHUD = false;
			HUDLine1.Reset();
			HUDLine2.Reset();

			return FReply::Handled()
				.SetUserFocus(SharedThis(this), EFocusCause::Mouse)
				.ReleaseMouseCapture();
		}

		// Click release without drag
		bShowHUD = false;
		HUDLine1.Reset();
		HUDLine2.Reset();

		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	return FReply::Unhandled();
}

FReply SShapeGraphEditorCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D MouseView = GetMouseLocalPx(MyGeometry, MouseEvent);

	const float ThresholdSq = DragStartThresholdPx * DragStartThresholdPx;

	// Threshold start (no capture yet)
	if (bLMBDown && !HasMouseCapture() && !TransformSession.bActive)
	{
		if (DistSq(MouseView, LMBDownMouseViewPx) >= ThresholdSq)
		{
			// 1) Drag selection (vertex intent)
			if (bDragIntentVertex && LMBDownHitVertexIndex != INDEX_NONE)
			{
				if (UShapeGraphAsset* Asset = ShapeAsset.Get())
				{
					bDraggingSelection = true;
					DragAnchorIndex = LMBDownHitVertexIndex;

					DragInitialUV.Reset();
					DragStartMouseUV = ViewPxToUV(LMBDownMouseViewPx);

					// snapshot UV for all selected vertices
					for (int32 Idx : Selection.Vertices)
					{
						if (Asset->ContourUV.IsValidIndex(Idx))
						{
							DragInitialUV.Add(Idx, Asset->ContourUV[Idx]);
						}
					}

					DragPivotDesign = ComputePivotDesign();

					BeginTransaction(NSLOCTEXT("ShapeTools", "MoveSelectionTx", "Move Selection"));
					return FReply::Handled().CaptureMouse(SharedThis(this));
				}
			}

			// 2) Drag background (background intent)
			if (bDragIntentBackground)
			{
				if (UShapeGraphAsset* Asset = ShapeAsset.Get())
				{
					if (!Asset->bBackgroundLocked)
					{
						bDraggingBackground = true;
						BgDragStartMouseViewPx = LMBDownMouseViewPx;
						BgDragStartOffsetDesign = Asset->BackgroundOffsetPx;

						BeginTransaction(NSLOCTEXT("ShapeTools", "MoveBackgroundTx", "Move Background"));
						return FReply::Handled().CaptureMouse(SharedThis(this));
					}
				}
			}

			// 3) Box select (empty intent)
			if (bDragIntentEmpty)
			{
				BeginBoxSelect(MyGeometry, LMBDownMouseViewPx);
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
		}
	}

	// Modal transform update
	if (TransformSession.bActive)
	{
		UpdateTransform(MyGeometry, MouseView);
		return FReply::Handled();
	}

	// Pan (RMB)
	if (bPanningView && HasMouseCapture())
	{
		const FVector2D Delta = MouseView - LastMouseLocalPx;
		ViewPanPx += Delta;
		LastMouseLocalPx = MouseView;

		Invalidate(EInvalidateWidget::Paint);
		return FReply::Handled();
	}

	// Hover
	const int32 PrevHoveredPoint = HoveredPointIndex;
	const int32 PrevHoveredSeg = HoveredSegmentIndex;

	HoveredPointIndex = HitTestPoint(MouseView, GPointHitRadiusPx);

	float SegT = 0.f;
	HoveredSegmentIndex = (HoveredPointIndex == INDEX_NONE)
		? HitTestSegment(MouseView, GSegmentHitRadiusPx, SegT)
		: INDEX_NONE;

	const bool bNeedsRepaint = (HoveredPointIndex != PrevHoveredPoint) || (HoveredSegmentIndex != PrevHoveredSeg);

	// Drag selection (multi)
	if (bDraggingSelection && HasMouseCapture())
	{
		if (UShapeGraphAsset* Asset = ShapeAsset.Get())
		{
			const FVector2D MouseUV = ViewPxToUV(MouseView);
			FVector2D DeltaUV = MouseUV - DragStartMouseUV;

			if (MouseEvent.IsControlDown())
			{
				const FVector2D PivotUV = DesignToUV(DragPivotDesign);

				DeltaUV = SnapDeltaUVByPivotToActiveGrid(
					DeltaUV,
					Asset,
					PivotUV
				);
			}

			// Clamp group 0..1
			DeltaUV = ClampDeltaUVTo01(DeltaUV, DragInitialUV);

			for (const TPair<int32, FVector2D>& KV : DragInitialUV)
			{
				const int32 Idx = KV.Key;
				if (!Asset->ContourUV.IsValidIndex(Idx)) continue;
				Asset->ContourUV[Idx] = KV.Value + DeltaUV;
			}

			// HUD
			bShowHUD = true;
			HUDLine1 = TEXT("Move");
			HUDLine2 = FString::Printf(TEXT("UV: %.4f, %.4f"), DeltaUV.X, DeltaUV.Y);

			Invalidate(EInvalidateWidget::Paint);
			return FReply::Handled();
		}
	}

	// Box select update
	if (bBoxSelecting && HasMouseCapture())
	{
		UpdateBoxSelect(MyGeometry, MouseView);
		return FReply::Handled();
	}

	// Background drag (free)
	if (bDraggingBackground && HasMouseCapture())
	{
		if (UShapeGraphAsset* Asset = ShapeAsset.Get())
		{
			if (!Asset->bBackgroundLocked)
			{
				const FVector2D DeltaView = MouseView - BgDragStartMouseViewPx;
				const FVector2D DeltaDesign = DeltaView / ViewZoom;

				Asset->BackgroundOffsetPx = BgDragStartOffsetDesign + DeltaDesign;

				// HUD
				bShowHUD = true;
				HUDLine1 = TEXT("Move Background");
				HUDLine2 = FString::Printf(TEXT("Design: %.1f, %.1f"), DeltaDesign.X, DeltaDesign.Y);

				NotifyAssetChanged();
				Invalidate(EInvalidateWidget::Paint);
				return FReply::Handled();
			}
		}
	}

	LastMouseLocalPx = MouseView;

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

	const FVector2D MouseView = GetMouseLocalPx(MyGeometry, MouseEvent);

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
	// Base
	int32 CurrentLayer = SCompoundWidget::OnPaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	// Cache geometry for DPI-safe input
	CachedPaintGeometry = AllottedGeometry;
	bHasCachedPaintGeometry = true;

	// Background / Grid
	DrawBackground(AllottedGeometry, OutDrawElements, CurrentLayer++);
	DrawBackgroundSelectionOutline(AllottedGeometry, OutDrawElements, CurrentLayer++);
	DrawGrid(AllottedGeometry, OutDrawElements, CurrentLayer++);

	const UShapeGraphAsset* Asset = ShapeAsset.Get();
	if (!Asset || Asset->ContourUV.Num() < 1)
	{
		return CurrentLayer;
	}

	// Shape lines
	if (Asset->ContourUV.Num() >= 2)
	{
		TArray<FVector2D> PointsPx;
		PointsPx.Reserve(Asset->ContourUV.Num() + (Asset->bClosed ? 1 : 0));

		for (const FVector2D& UV : Asset->ContourUV)
		{
			PointsPx.Add(UVToViewPx(UV));
		}

		if (Asset->bClosed && PointsPx.Num() > 0)
		{
			const FVector2D FirstPoint = PointsPx[0];
			PointsPx.Add(FirstPoint);
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			CurrentLayer++,
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

		const bool bSelected = Selection.Vertices.Contains(i);
		const bool bHovered = (i == HoveredPointIndex);

		const FLinearColor Col = bSelected ? FLinearColor::Green : (bHovered ? FLinearColor::Yellow : FLinearColor::White);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			CurrentLayer,
			BoxGeo.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			Col
		);
	}
	CurrentLayer++;

	DrawPivot(AllottedGeometry, OutDrawElements, CurrentLayer);
	CurrentLayer += 2;

	// Box selection overlay (au-dessus de tout)
	if (bBoxSelecting)
	{
		const FVector2D A = DesignToViewPx(BoxStartDesign);
		const FVector2D B = DesignToViewPx(BoxEndDesign);

		const FVector2D Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		const FVector2D Max(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		const FVector2D Size = Max - Min;

		// Fill
		{
			const FGeometry FillGeo = AllottedGeometry.MakeChild(
				Size,
				FSlateLayoutTransform(Min)
			);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				CurrentLayer,
				FillGeo.ToPaintGeometry(),
				FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor(0.2f, 0.6f, 1.f, 0.08f)
			);
		}

		// Outline
		{
			TArray<FVector2D> Poly;
			Poly.Reserve(5);
			Poly.Add(Min);
			Poly.Add(FVector2D(Max.X, Min.Y));
			Poly.Add(Max);
			Poly.Add(FVector2D(Min.X, Max.Y));
			Poly.Add(Min);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				CurrentLayer + 1,
				AllottedGeometry.ToPaintGeometry(),
				Poly,
				ESlateDrawEffect::None,
				FLinearColor(0.2f, 0.6f, 1.f, 0.9f),
				true,
				1.25f
			);
		}

		CurrentLayer += 2;
	}

	// HUD overlay (top-left)
	if (bShowHUD && (!HUDLine1.IsEmpty() || !HUDLine2.IsEmpty()))
	{
		const FVector2D Pad(10.f, 10.f);
		const FVector2D Pos = Pad;

		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 12);

		// Background panel
		const FVector2D PanelSize(260.f, 44.f);
		const FGeometry PanelGeo = AllottedGeometry.MakeChild(PanelSize, FSlateLayoutTransform(Pos));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			CurrentLayer,
			PanelGeo.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor(0.f, 0.f, 0.f, 0.45f)
		);

		// Text 1
		FSlateDrawElement::MakeText(
			OutDrawElements,
			CurrentLayer + 1,
			AllottedGeometry.ToPaintGeometry(Pos + FVector2D(8.f, 6.f), AllottedGeometry.GetLocalSize()),
			FText::FromString(HUDLine1),
			Font,
			ESlateDrawEffect::None,
			FLinearColor::White
		);

		// Text 2
		FSlateDrawElement::MakeText(
			OutDrawElements,
			CurrentLayer + 2,
			AllottedGeometry.ToPaintGeometry(Pos + FVector2D(8.f, 24.f), AllottedGeometry.GetLocalSize()),
			FText::FromString(HUDLine2),
			Font,
			ESlateDrawEffect::None,
			FLinearColor(0.85f, 0.9f, 1.f, 1.f)
		);

		CurrentLayer += 3;
	}

	return CurrentLayer;
}