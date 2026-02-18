#include "ShapeGraphAsset.h"

UShapeGraphAsset::UShapeGraphAsset()
{
	SetFlags(RF_Transactional);
}

#if WITH_EDITOR
#include "UObject/UnrealType.h"

void UShapeGraphAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UShapeGraphAsset, ContourUV) || PropName == NAME_None)
	{
		for (FVector2D& UV : ContourUV)
		{
			UV.X = FMath::Clamp(UV.X, 0.f, 1.f);
			UV.Y = FMath::Clamp(UV.Y, 0.f, 1.f);
		}
	}

	BackgroundScale = FMath::Max(BackgroundScale, 0.001f);
	BackgroundRotation = FMath::Clamp(BackgroundRotation, -180.f, 180.f);
	BackgroundOpacity = FMath::Clamp(BackgroundOpacity, 0.f, 100.f);
}
#endif