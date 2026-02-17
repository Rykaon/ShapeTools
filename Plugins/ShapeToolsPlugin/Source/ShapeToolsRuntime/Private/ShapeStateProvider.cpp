#include "ShapeStateProvider.h"

void UShapeStateProvider::GetAllKeys_Implementation(TArray<FName>& OutKeys) const
{
	OutKeys.Reset();
}

bool UShapeStateProvider::GetBool_Implementation(FName Key, FName ParamName, bool bDefaultValue) const
{
	return bDefaultValue;
}