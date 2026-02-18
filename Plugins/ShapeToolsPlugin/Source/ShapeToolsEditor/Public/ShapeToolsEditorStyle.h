#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;

class FShapeToolsEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};