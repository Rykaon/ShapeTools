// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShapeToolsEditor.h"

#define LOCTEXT_NAMESPACE "FShapeToolsEditorModule"

void FShapeToolsEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	ShapeToolsAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName("ShapeTools"),
		NSLOCTEXT("ShapeTools", "ShapeToolsCategory", "Shape Tools")
	);

	TSharedRef<IAssetTypeActions> Action = MakeShared<FAssetTypeActions_ShapeGraphAsset>(ShapeToolsAssetCategory);
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

void FShapeToolsEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for (const TSharedRef<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}
	}

	RegisteredAssetTypeActions.Empty();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FShapeToolsEditorModule, ShapeToolsEditor)