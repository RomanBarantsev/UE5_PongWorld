// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PingPong : ModuleRules
{
	public PingPong(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore" ,
			"UMG", 
			"Slate",
			"GeometryCollectionEngine",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Sockets",
			"Networking",
			"SlateCore",
			"Slate",
			"Json",
			"JsonUtilities",
			"HTTP"
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});
		
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
