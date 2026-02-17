#include "ShapeGraphAssetFactory.h"
#include "ShapeGraphAsset.h"

UShapeGraphAssetFactory::UShapeGraphAssetFactory()
{
	SupportedClass = UShapeGraphAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UShapeGraphAssetFactory::FactoryCreateNew(
	UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UShapeGraphAsset>(InParent, Class, Name, Flags);
}