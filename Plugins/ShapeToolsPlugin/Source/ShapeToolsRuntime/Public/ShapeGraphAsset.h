#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShapeGraphAsset.generated.h"

UCLASS(BlueprintType)
class SHAPETOOLSRUNTIME_API UShapeGraphAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShapeTools")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShapeTools")
	TArray<FVector2D> ContourUV;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShapeTools")
	bool bClosed = true;
};