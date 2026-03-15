using UnrealBuildTool;
public class Ivory_BladeTarget : TargetRules
{
	public Ivory_BladeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("Ivory_Blade");
	}
}