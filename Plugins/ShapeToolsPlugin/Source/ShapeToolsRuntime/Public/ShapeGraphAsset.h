#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShapeGraphAsset.generated.h"

UENUM()
enum class EShapeGridSpace : uint8
{
	Design,
	UV
};

UCLASS(BlueprintType)
class SHAPETOOLSRUNTIME_API UShapeGraphAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UShapeGraphAsset();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (MakeEditWidget = false))
	TArray<FVector2D> ContourUV;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	bool bClosed = true;

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	UPROPERTY(EditAnywhere, Category = "Editor|Canvas", meta = (ClampMin = "1", UIMin = "1"))
	FVector2D CanvasDesignSize = FVector2D(1920.f, 1080.f);

	UPROPERTY(EditAnywhere, Category = "Editor|Canvas")
	EShapeGridSpace GridSpace = EShapeGridSpace::Design;

	UPROPERTY(EditAnywhere, Category = "Editor|Canvas", meta = (ClampMin = "1", UIMin = "1"))
	float GridStepDesignPx = 50.f;

	UPROPERTY(EditAnywhere, Category = "Editor|Canvas", meta = (ClampMin = "0.0001", ClampMax = "1.0", UIMin = "0.001", UIMax = "0.25"))
	float GridStepUV = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Editor|Background")
	TSoftObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY(EditAnywhere, Category = "Editor|Background")
	FVector2D BackgroundOffsetPx = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Editor|Background", meta = (ClampMin = "0.001", UIMin = "0.01", UIMax = "20.0"))
	float BackgroundScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Editor|Background", meta = (ClampMin = "-180.0", ClampMax = "180.0", UIMin = "-180.0", UIMax = "180.0"))
	float BackgroundRotation = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Editor|Background", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float BackgroundOpacity = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Editor|Background")
	bool bBackgroundLocked = false;
#endif
};