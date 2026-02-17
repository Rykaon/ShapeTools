#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "ShapeGraphAssetFactory.generated.h"

UCLASS()
class SHAPETOOLSEDITOR_API UShapeGraphAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UShapeGraphAssetFactory();

	// UFactory
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;

	virtual bool ShouldShowInNewMenu() const override { return true; }
};