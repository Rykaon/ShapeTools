#include "ShapeMapWidget.h"
#include "ShapeGraphAsset.h"
#include "ShapeStateProvider.h"
#include "Widgets/SShapeMap.h"

TSharedRef<SWidget> UShapeMapWidget::RebuildWidget()
{
	MySlateWidget =
		SNew(SShapeMap)
		.Shapes(Shapes)
		.StateProvider(StateProvider)
		.DefaultVisualStyle(DefaultVisualStyle)
		.PerShapeVisualOverrides(PerShapeVisualOverrides)
		.DebugStyle(DebugStyle)
		.SelectedKey(SelectedKey)
		.OnHoverChanged(SShapeMap::FOnHoverChanged::CreateUObject(this, &UShapeMapWidget::HandleNativeHoverChanged))
		.OnClicked(SShapeMap::FOnClicked::CreateUObject(this, &UShapeMapWidget::HandleNativeClicked));

	return MySlateWidget.ToSharedRef();
}

// --- VISUAL STYLE SETTERS --- //

void UShapeMapWidget::SetDefaultVisualStyle(const FShapeVisualStyle& NewStyle)
{
	DefaultVisualStyle = NewStyle;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDefaultVisualStyle(DefaultVisualStyle);
	}
}

void UShapeMapWidget::SetDefaultOutlineColor(FLinearColor NewColor)
{
	DefaultVisualStyle.OutlineColor = NewColor;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDefaultVisualStyle(DefaultVisualStyle);
	}
}

void UShapeMapWidget::SetDefaultOutlineThickness(float NewThickness)
{
	DefaultVisualStyle.OutlineThickness = FMath::Max(0.f, NewThickness);

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDefaultVisualStyle(DefaultVisualStyle);
	}
}

void UShapeMapWidget::SetDefaultFillColor(FLinearColor NewColor)
{
	DefaultVisualStyle.FillColor = NewColor;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDefaultVisualStyle(DefaultVisualStyle);
	}
}

void UShapeMapWidget::SetDefaultHoverTint(FLinearColor NewTint)
{
	DefaultVisualStyle.HoverTint = NewTint;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDefaultVisualStyle(DefaultVisualStyle);
	}
}

void UShapeMapWidget::SetDefaultSelectedTint(FLinearColor NewTint)
{
	DefaultVisualStyle.SelectedTint = NewTint;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDefaultVisualStyle(DefaultVisualStyle);
	}
}

void UShapeMapWidget::SetPerShapeVisualOverrides(const FShapeVisualStyleOverrides& NewOverrides)
{
	PerShapeVisualOverrides = NewOverrides;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetPerShapeVisualOverrides(PerShapeVisualOverrides);
	}
}

void UShapeMapWidget::SetPerShapeVisualOverride(FName Key, const FShapeVisualStyle& Style)
{
	if (Key.IsNone())
	{
		return;
	}

	PerShapeVisualOverrides.Map.FindOrAdd(Key) = Style;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetPerShapeVisualOverride(Key, Style);
	}
}

void UShapeMapWidget::ClearPerShapeVisualOverride(FName Key)
{
	if (Key.IsNone())
	{
		return;
	}

	if (PerShapeVisualOverrides.Map.Remove(Key) > 0)
	{
		if (MySlateWidget.IsValid())
		{
			MySlateWidget->ClearPerShapeVisualOverride(Key);
		}
	}
}

void UShapeMapWidget::ClearAllPerShapeVisualOverrides()
{
	if (PerShapeVisualOverrides.Map.Num() == 0)
	{
		return;
	}

	PerShapeVisualOverrides.Map.Reset();

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->ClearAllPerShapeVisualOverrides();
	}
}

bool UShapeMapWidget::HasPerShapeOverride(FName Key) const
{
	return !Key.IsNone() && PerShapeVisualOverrides.Map.Contains(Key);
}

FShapeVisualStyle UShapeMapWidget::GetResolvedStyle(FName Key) const
{
	if (!Key.IsNone())
	{
		if (const FShapeVisualStyle* Found = PerShapeVisualOverrides.Map.Find(Key))
		{
			return *Found;
		}
	}
	return DefaultVisualStyle;
}

FShapeVisualStyle& UShapeMapWidget::GetOrCreateOverrideStyle(FName Key)
{
	FShapeVisualStyle* Existing = PerShapeVisualOverrides.Map.Find(Key);
	if (Existing)
	{
		return *Existing;
	}

	FShapeVisualStyle& NewStyle = PerShapeVisualOverrides.Map.Add(Key, DefaultVisualStyle);
	return NewStyle;
}

void UShapeMapWidget::SetOverrideOutlineColor(FName Key, FLinearColor NewColor)
{
	if (Key.IsNone()) return;

	FShapeVisualStyle& Style = GetOrCreateOverrideStyle(Key);
	Style.OutlineColor = NewColor;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetPerShapeVisualOverride(Key, Style);
	}
}

void UShapeMapWidget::SetOverrideOutlineThickness(FName Key, float NewThickness)
{
	if (Key.IsNone()) return;

	FShapeVisualStyle& Style = GetOrCreateOverrideStyle(Key);
	Style.OutlineThickness = FMath::Max(0.f, NewThickness);

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetPerShapeVisualOverride(Key, Style);
	}
}

void UShapeMapWidget::SetOverrideFillColor(FName Key, FLinearColor NewColor)
{
	if (Key.IsNone()) return;

	FShapeVisualStyle& Style = GetOrCreateOverrideStyle(Key);
	Style.FillColor = NewColor;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetPerShapeVisualOverride(Key, Style);
	}
}

void UShapeMapWidget::SetOverrideHoverTint(FName Key, FLinearColor NewTint)
{
	if (Key.IsNone()) return;

	FShapeVisualStyle& Style = GetOrCreateOverrideStyle(Key);
	Style.HoverTint = NewTint;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetPerShapeVisualOverride(Key, Style);
	}
}

void UShapeMapWidget::SetOverrideSelectedTint(FName Key, FLinearColor NewTint)
{
	if (Key.IsNone()) return;

	FShapeVisualStyle& Style = GetOrCreateOverrideStyle(Key);
	Style.SelectedTint = NewTint;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetPerShapeVisualOverride(Key, Style);
	}
}

// --- DEBUG STYLE SETTERS --- //

void UShapeMapWidget::SetDebugStyle(const FShapeDebugStyle& NewStyle)
{
	DebugStyle = NewStyle;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDebugStyle(DebugStyle);
	}
}

void UShapeMapWidget::SetDebugEnabled(bool bEnabled)
{
	DebugStyle.bEnabled = bEnabled;
	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDebugStyle(DebugStyle);
	}
}

void UShapeMapWidget::SetDebugLineThickness(float Thickness)
{
	DebugStyle.LineThickness = FMath::Max(0.f, Thickness);
	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDebugStyle(DebugStyle);
	}
}

void UShapeMapWidget::SetDebugColors(FLinearColor Normal, FLinearColor Hover, FLinearColor Selected)
{
	DebugStyle.NormalColor = Normal;
	DebugStyle.HoverColor = Hover;
	DebugStyle.SelectedColor = Selected;
	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDebugStyle(DebugStyle);
	}
}

void UShapeMapWidget::SetSelectedKey(FName Key)
{
	if (SelectedKey == Key) return;

	SelectedKey = Key;

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetSelectedKey(SelectedKey);
	}
}

void UShapeMapWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (MySlateWidget.IsValid())
	{
		MySlateWidget->SetDebugStyle(DebugStyle);
		MySlateWidget->SetDefaultVisualStyle(DefaultVisualStyle);
		MySlateWidget->SetPerShapeVisualOverrides(PerShapeVisualOverrides);
		MySlateWidget->SetSelectedKey(SelectedKey);
	}
}

void UShapeMapWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MySlateWidget.Reset();
}

void UShapeMapWidget::HandleNativeHoverChanged(FName Key, bool bHovered)
{
	OnShapeHovered.Broadcast(Key, bHovered);
}

void UShapeMapWidget::HandleNativeClicked(FName Key)
{
	OnShapeClicked.Broadcast(Key);
}