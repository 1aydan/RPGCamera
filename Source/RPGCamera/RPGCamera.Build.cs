// Copyright (c) 2026. Licensed for use in your own projects.

using UnrealBuildTool;

public class RPGCamera : ModuleRules
{
	public RPGCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		// No navigation or AI dependencies: this plugin is camera-only.
	}
}
