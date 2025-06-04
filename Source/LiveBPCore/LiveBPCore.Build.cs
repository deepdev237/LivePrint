using UnrealBuildTool;

public class LiveBPCore : ModuleRules
{
	public LiveBPCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
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
				"SlateCore",
				// Concert/MUE modules
				"Concert",
				"ConcertSyncCore",
				"ConcertTransport",
				"MultiUserClientLibrary"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"JsonObjectConverter",
				"Serialization",
				"Networking",
				"Sockets",
				"ApplicationCore", // For timers and threading
				"RHI" // For performance monitoring
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
