#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShapeGraphAsset.generated.h"

UCLASS(BlueprintType)
class SHAPETOOLSRUNTIME_API UShapeGraphAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (MakeEditWidget = false))
	TArray<FVector2D> ContourUV;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	bool bClosed = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Editor|Background")
	TSoftObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY(EditAnywhere, Category = "Editor|Background")
	FVector2D BackgroundOffsetPx = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Editor|Background", meta = (ClampMin = "0.01", ClampMax = "100.0"))
	float BackgroundScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Editor|Background")
	bool bBackgroundLocked = false;
#endif
};