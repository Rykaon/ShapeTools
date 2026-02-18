#include "ShapeToolsEditorStyle.h"

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FShapeToolsEditorStyle::StyleInstance;

static FString GetPluginResourcesDir()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ShapeToolsPlugin"));
	check(Plugin.IsValid());
	return Plugin->GetBaseDir() / TEXT("Resources");
}

#define IMAGE_BRUSH(RelativePath, Size) FSlateImageBrush((GetPluginResourcesDir() / RelativePath), Size)

void FShapeToolsEditorStyle::Initialize()
{
	if (StyleInstance.IsValid())
	{
		return;
	}

	StyleInstance = MakeShared<FSlateStyleSet>(GetStyleSetName());

	// Register icon brushes
	StyleInstance->Set("ShapeTools.Icons.Grid", new IMAGE_BRUSH(TEXT("T_GridIcon.png"), FVector2D(16.f, 16.f)));
	StyleInstance->Set("ShapeTools.Icons.Shortcuts", new IMAGE_BRUSH(TEXT("T_ShortcutsIcon.png"), FVector2D(16.f, 16.f)));
	StyleInstance->Set("ShapeTools.Icons.Export", new IMAGE_BRUSH(TEXT("T_ExportIcon.png"), FVector2D(16.f, 16.f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FShapeToolsEditorStyle::Shutdown()
{
	if (!StyleInstance.IsValid())
	{
		return;
	}

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	StyleInstance.Reset();
}

const ISlateStyle& FShapeToolsEditorStyle::Get()
{
	return *StyleInstance;
}

FName FShapeToolsEditorStyle::GetStyleSetName()
{
	static FName Name(TEXT("ShapeToolsEditorStyle"));
	return Name;
}