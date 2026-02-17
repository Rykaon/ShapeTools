#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ShapeStateProvider.generated.h"

/**
 * Blueprint-friendly provider: plugin users implement this in BP to supply state.
 * No custom struct required.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SHAPETOOLSRUNTIME_API UShapeStateProvider : public UObject
{
	GENERATED_BODY()

public:
	/** Return all keys known by the provider. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Shape|State")
	void GetAllKeys(TArray<FName>& OutKeys) const;

	/** Generic bool query (e.g. ParamName="O2Enabled"). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Shape|State")
	bool GetBool(FName Key, FName ParamName, bool bDefaultValue) const;
};