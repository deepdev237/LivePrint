using UnrealBuildTool;

public class LiveBPCore : ModuleRules
{
	public LiveBPCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// UE 5.5 compatibility settings
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		
		bUseUnity = false;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"JsonUtilities",
				"Serialization",
				"Networking",
				"Sockets",
				"ApplicationCore", // For timers and threading
				"RHI", // For performance monitoring
				"Projects", // For project settings
				// Concert/MUE dependencies - using correct module names for UE 5.5
				"Concert",
				"ConcertSyncClient",
				"ConcertSyncCore",
				"ConcertTransport",
				"ConcertMain"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
