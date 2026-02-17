#include "AssetTypeActions_ShapeGraphAsset.h"
#include "ShapeGraphAsset.h"
#include "ShapeGraphAssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_ShapeGraphAsset"

FText FAssetTypeActions_ShapeGraphAsset::GetName() const
{
	return LOCTEXT("ShapeGraphAssetName", "Shape Graph");
}

FColor FAssetTypeActions_ShapeGraphAsset::GetTypeColor() const
{
	return FColor(120, 200, 255);
}

UClass* FAssetTypeActions_ShapeGraphAsset::GetSupportedClass() const
{
	return UShapeGraphAsset::StaticClass();
}

void FAssetTypeActions_ShapeGraphAsset::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FShapeGraphAssetEditorToolkit::OpenEditor(InObjects);
}

#undef LOCTEXT_NAMESPACE