using UnrealBuildTool;

public class Ivory_Blade : ModuleRules
{
	public Ivory_Blade(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"HeadMountedDisplay",
			"AnimGraphRuntime"
		});

		// Only the combo set's IsDataValid needs this, to find mapping contexts
		// that still give a combo output action a physical key.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}
	}
}