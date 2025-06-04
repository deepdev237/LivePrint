using UnrealBuildTool;

public class LiveBPEditor : ModuleRules
{
	public LiveBPEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"EditorStyle",
				"EditorWidgets",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"WorkspaceMenuStructure",
				// Blueprint Editor
				"BlueprintGraph",
				"KismetCompiler",
				"KismetWidgets",
				"PropertyEditor",
				"GraphEditor",
				"Kismet",
				"BlueprintEditorModule",
				// LiveBP Core
				"LiveBPCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"EditorSubsystem",
				"DesktopPlatform",
				"Framework",
				"ApplicationCore",
				// Rendering
				"RenderCore",
				"RHI",
				// Serialization
				"Json",
				"JsonObjectConverter"
			}
		);
	}
}
