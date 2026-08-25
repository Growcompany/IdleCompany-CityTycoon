// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class CompanyGrowthRenewal : ModuleRules
{
	public CompanyGrowthRenewal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] { "CompanyGrowthRenewal/Public" });
        PrivateIncludePaths.AddRange(new string[] { "CompanyGrowthRenewal/Private" });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" , "UMG", "CommonUI",
            "ProceduralMeshComponent",
            "NavigationSystem",
            "AIModule",
            "AsyncLoadingScreen",
            "ImageWrapper",
            "Niagara",
            "NiagaraUIRenderer",
            // GameplayTags (UI 사운드 / 향후 도메인 통합용)
            "GameplayTags",
            // UDeveloperSettings (CGDevSettings — Project Settings 개발 시작 모드)
            "DeveloperSettings",
            // PlayFab SDK
            "PlayFab",
            "PlayFabCpp",
            "PlayFabCommon",
            // HTTP/JSON (Firebase REST API)
            "HTTP",
            "Json",
            "JsonUtilities"
        });

		// Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] {"CoreUObject", "Engine", "Slate", "SlateCore" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Android: UPL 로 매니페스트 후처리 (WRITE_EXTERNAL_STORAGE 에 maxSdkVersion 부여 → 부팅 저장소 권한 게이트 회피)
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "CGR_UPL.xml"));
		}

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
