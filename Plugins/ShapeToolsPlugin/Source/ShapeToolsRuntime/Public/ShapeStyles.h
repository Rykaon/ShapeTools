#pragma once
#include "CoreMinimal.h"
#include "ShapeStyles.generated.h"

USTRUCT(BlueprintType)
struct SHAPETOOLSRUNTIME_API FShapeVisualStyle
{
	GENERATED_BODY()

	/** Base fill color (if using C++ fill later) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FLinearColor FillColor = FLinearColor::Transparent;

	/** Outline thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	float OutlineThickness = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FLinearColor OutlineColor = FLinearColor::White;

	/** Hover modifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FLinearColor HoverTint = FLinearColor(1.f, 1.f, 0.f, 1.f);

	/** Selected modifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FLinearColor SelectedTint = FLinearColor(0.f, 1.f, 1.f, 1.f);
};

USTRUCT(BlueprintType)
struct SHAPETOOLSRUNTIME_API FShapeVisualStyleOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	TMap<FName, FShapeVisualStyle> Map;
};

USTRUCT(BlueprintType)
struct SHAPETOOLSRUNTIME_API FShapeDebugStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float LineThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FLinearColor NormalColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FLinearColor HoverColor = FLinearColor(1.f, 1.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FLinearColor SelectedColor = FLinearColor(0.f, 1.f, 1.f, 1.f);
};