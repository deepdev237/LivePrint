using UnrealBuildTool;

public class LiveBPEditor : ModuleRules
{
	public LiveBPEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// UE 5.5 compatibility settings
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		
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
				"ApplicationCore",
				"AssetRegistry", // For asset tracking
				"ToolkitApplication", // For asset editor integration
				// Rendering
				"RenderCore",
				"RHI",
				// Serialization
				"Json",
				"JsonUtilities",
				// Concert/MUE dependencies - using correct module names for UE 5.5
				"Concert",
				"ConcertSyncClient",
				"ConcertSyncCore",
				"ConcertTransport",
				"ConcertMain"
			}
		);
	}
}
