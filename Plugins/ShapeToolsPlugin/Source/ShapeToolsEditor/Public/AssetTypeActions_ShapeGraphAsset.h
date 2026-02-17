#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_ShapeGraphAsset : public FAssetTypeActions_Base
{
public:
	explicit FAssetTypeActions_ShapeGraphAsset(EAssetTypeCategories::Type InCategory)
		: MyCategory(InCategory)
	{
	}

	// IAssetTypeActions
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return MyCategory; }

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects,
		TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;

private:
	EAssetTypeCategories::Type MyCategory;
};