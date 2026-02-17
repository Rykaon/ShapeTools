#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Input/Reply.h"
#include "ShapeStyles.h"
#include "ShapeMapWidget.generated.h"

class UShapeGraphAsset;
class UShapeStateProvider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShapeClickedBP, FName, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShapeHoveredBP, FName, Key, bool, bHovered);

UCLASS(BlueprintType)
class SHAPETOOLSRUNTIME_API UShapeMapWidget : public UWidget
{
	GENERATED_BODY()

public:
	/** Shapes to render/pick (temporary: later replaced by MapDefinition). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	TArray<TObjectPtr<UShapeGraphAsset>> Shapes;

	/** Optional external state provider (BP-implemented). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|State")
	TObjectPtr<UShapeStateProvider> StateProvider;

	UPROPERTY(BlueprintAssignable, Category = "Shape|Events")
	FOnShapeClickedBP OnShapeClicked;

	UPROPERTY(BlueprintAssignable, Category = "Shape|Events")
	FOnShapeHoveredBP OnShapeHovered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|Visual")
	FShapeVisualStyle DefaultVisualStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|Visual")
	FShapeVisualStyleOverrides PerShapeVisualOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|Debug")
	FShapeDebugStyle DebugStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|Debug")
	FName SelectedKey = NAME_None;

	// --- VISUAL STYLE SETTERS --- //

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetDefaultVisualStyle(const FShapeVisualStyle& NewStyle);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetDefaultOutlineColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetDefaultOutlineThickness(float NewThickness);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetDefaultFillColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetDefaultHoverTint(FLinearColor NewTint);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetDefaultSelectedTint(FLinearColor NewTint);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetPerShapeVisualOverrides(const FShapeVisualStyleOverrides& NewOverrides);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetPerShapeVisualOverride(FName Key, const FShapeVisualStyle& Style);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void ClearPerShapeVisualOverride(FName Key);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void ClearAllPerShapeVisualOverrides();

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	bool HasPerShapeOverride(FName Key) const;

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	FShapeVisualStyle GetResolvedStyle(FName Key) const;

	FShapeVisualStyle& GetOrCreateOverrideStyle(FName Key);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetOverrideOutlineColor(FName Key, FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetOverrideOutlineThickness(FName Key, float NewThickness);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetOverrideFillColor(FName Key, FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetOverrideHoverTint(FName Key, FLinearColor NewTint);

	UFUNCTION(BlueprintCallable, Category = "Shape|Visual")
	void SetOverrideSelectedTint(FName Key, FLinearColor NewTint);

	// --- DEBUG STYLE SETTERS --- //

	UFUNCTION(BlueprintCallable, Category = "Shape|Debug")
	void SetDebugStyle(const FShapeDebugStyle& NewStyle);

	UFUNCTION(BlueprintCallable, Category = "Shape|Debug")
	void SetDebugEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Shape|Debug")
	void SetDebugLineThickness(float Thickness);

	UFUNCTION(BlueprintCallable, Category = "Shape|Debug")
	void SetDebugColors(FLinearColor Normal, FLinearColor Hover, FLinearColor Selected);

	UFUNCTION(BlueprintCallable, Category = "Shape|Selection")
	void SetSelectedKey(FName Key);

	// UWidget
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;

protected:
	TSharedPtr<class SShapeMap> MySlateWidget;

	void HandleNativeHoverChanged(FName Key, bool bHovered);
	void HandleNativeClicked(FName Key);
};