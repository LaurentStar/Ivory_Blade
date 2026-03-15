using UnrealBuildTool;
public class Ivory_BladeEditorTarget : TargetRules
{
	public Ivory_BladeEditorTarget(TargetInfo Target) : base(Target)
	{
		WindowsPlatform.CompilerVersion = "14.36.32532";
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("Ivory_Blade");
	}
}