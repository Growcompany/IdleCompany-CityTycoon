// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/CGCheatManager.h"
#include "Core/CGGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/EmployeeManager.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Table/CountryInfoTable.h"
#include "Table/ProductRecipeTable.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Data/ShippedRecord.h"
#include "Data/ProjectBoardData.h"
#include "Table/ProjectDataTable.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/DevPresetSeeder.h"
#include "Global/CGDevSettings.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/SpawnManager.h"
#include "Manager/EntityManager.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "Player/MainMapPlayerController.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Engine/LocalPlayer.h"
#include "Manager/MineManager.h"
#include "Manager/WorldMapManager.h"
#include "Office/OfficeManager.h"
#include "Office/OfficeInterior.h"
#include "Player/OfficeCameraPawn.h"
#include "Office/DecorationActor.h"
#include "TimeCycle/TimeCycleManager.h"
#include "TimeCycle/TimeCycleCode.h"
#include "Entity/Country/CountryActor.h"
#include "Entity/Factory/BrickFactory.h"
#include "Data/FactorySaveData.h"
#include "Data/FactoryUpgradeData.h"
#include "Data/FactoryUpgradeConfig.h"
#include "Enum/ResourceType.h"
#include "Enum/ItemType.h"
#include "Enum/LootBoxRarity.h"
#include "Table/BuildingSkinData.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogCGCheat, Log, All);

void UCGCheatManager::InitCheatManager()
{
	Super::InitCheatManager();
	UE_LOG(LogCGCheat, Warning, TEXT("[CGCheat] CheatManager INITIALIZED — cheats are LIVE"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Green, TEXT("[CGCheat] CheatManager READY"));
	}
}

void UCGCheatManager::CheatPing()
{
	UE_LOG(LogCGCheat, Warning, TEXT("[CGCheat] PONG — console dispatch is working"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[CGCheat] PONG"));
	}
}

// ========== 경험치 관련 ==========

void UCGCheatManager::Exp(int32 Amount, int32 EmployeeID)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Exp] GameInstance not found"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Exp] EmployeeManager not found"));
		return;
	}

	int32 TargetID = (EmployeeID == -1) ? EmployeeMgr->GetSelectedEmployeeID() : EmployeeID;

	if (TargetID == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Exp] No employee selected. Usage: Exp [Amount] [EmployeeID]"));
		return;
	}

	FEmployeeInstance* Employee = EmployeeMgr->FindEmployee(TargetID);
	if (!Employee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Exp] Employee %d not found"), TargetID);
		return;
	}

	float OldExp = Employee->Experience;
	int32 MaxExp = EmployeeMgr->GetMaxExperienceForLevel(Employee->Level);

	EmployeeMgr->AddExperience(TargetID, static_cast<float>(Amount));

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:Exp] %s (ID:%d): %.1f -> %.1f / %d (Lv.%d)"),
		*Employee->EmployeeName, TargetID, OldExp, Employee->Experience, MaxExp, Employee->Level);

	if (Employee->Experience >= MaxExp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Exp] >>> LEVEL UP AVAILABLE!"));
	}
}

void UCGCheatManager::DistExp(int32 Amount, int32 QualityGradeValue)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:DistExp] GameInstance not found"));
		return;
	}

	int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:DistExp] No building selected"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:DistExp] EmployeeManager not found"));
		return;
	}

	EQualityGrade Grade = static_cast<EQualityGrade>(FMath::Clamp(QualityGradeValue, 0, 5));

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:DistExp] Distributing %d EXP (Grade: %s, x%.1f) to building %d"),
		Amount, *QualityGradeToAlphabetString(Grade), EmployeeMgr->GetQualityMultiplier(Grade), BuildingIndex);

	EmployeeMgr->DistributeExperienceToBuilding(BuildingIndex, static_cast<float>(Amount), Grade);
}

void UCGCheatManager::ExpStatus()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ExpStatus] GameInstance not found"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ExpStatus] EmployeeManager not found"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("========== Employee EXP Status =========="));

	const TArray<FEmployeeInstance>& EmployeeList = EmployeeMgr->GetAllEmployees();
	for (const FEmployeeInstance& Employee : EmployeeList)
	{
		int32 MaxExp = EmployeeMgr->GetMaxExperienceForLevel(Employee.Level);
		float Progress = (MaxExp > 0) ? (Employee.Experience / MaxExp * 100.0f) : 0.0f;
		bool bCanLevelUp = Employee.Experience >= MaxExp;

		UE_LOG(LogTemp, Warning, TEXT("  [%d] %s | Lv.%d | %.0f/%d (%.0f%%) | Bld:%d %s"),
			Employee.EmployeeID,
			*Employee.EmployeeName,
			Employee.Level,
			Employee.Experience,
			MaxExp,
			Progress,
			Employee.AssignedBuildingIndex,
			bCanLevelUp ? TEXT("<<< LEVEL UP!") : TEXT(""));
	}

	UE_LOG(LogTemp, Warning, TEXT("========================================="));
	UE_LOG(LogTemp, Warning, TEXT(""));
}

// ========== 자원 관련 ==========

void UCGCheatManager::Money(int64 Amount)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Money] GameInstance not found"));
		return;
	}

	UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Money] ResourceItemManager not found"));
		return;
	}

	int64 OldAmount = ResourceMgr->GetResourceAmount(EResourceType::Money);
	ResourceMgr->StoreResource(EResourceType::Money, Amount);
	int64 NewAmount = ResourceMgr->GetResourceAmount(EResourceType::Money);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:Money] %lld -> %lld (+%lld)"), OldAmount, NewAmount, Amount);
}

void UCGCheatManager::ShowResources()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ShowResources] GameInstance not found"));
		return;
	}

	UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ShowResources] ResourceItemManager not found"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("========== Resources =========="));
	UE_LOG(LogTemp, Warning, TEXT("  Money: %lld"), ResourceMgr->GetResourceAmount(EResourceType::Money));
	UE_LOG(LogTemp, Warning, TEXT("==============================="));
	UE_LOG(LogTemp, Warning, TEXT(""));
}

void UCGCheatManager::GrantRes(FString ScenarioRowName)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:GrantRes] GameInstance not found"));
		return;
	}

	UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:GrantRes] ResourceItemManager not found"));
		return;
	}

	if (ScenarioRowName.IsEmpty())
	{
		ScenarioRowName = TEXT("Default");
	}

	const FName Row(*ScenarioRowName);
	const bool bOK = ResourceMgr->GrantTestResources(Row);

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:GrantRes] scenario='%s' result=%s"),
		*ScenarioRowName, bOK ? TEXT("OK") : TEXT("FAIL (DT_TestResourceScenarios 에 해당 RowName 확인)"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f,
			bOK ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("[GrantRes] %s %s"), *ScenarioRowName, bOK ? TEXT("applied") : TEXT("FAILED")));
	}
}

// ========== 직원 관련 ==========

void UCGCheatManager::ListEmp()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ListEmp] GameInstance not found"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ListEmp] EmployeeManager not found"));
		return;
	}

	int32 SelectedID = EmployeeMgr->GetSelectedEmployeeID();

	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("========== Employees =========="));

	const TArray<FEmployeeInstance>& EmployeeList = EmployeeMgr->GetAllEmployees();
	for (const FEmployeeInstance& Employee : EmployeeList)
	{
		EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee.EnhancementLevel);
		bool bSelected = (Employee.EmployeeID == SelectedID);

		UE_LOG(LogTemp, Warning, TEXT("  %s[%d] %s | %s | +%d | Bld:%d"),
			bSelected ? TEXT(">>> ") : TEXT("    "),
			Employee.EmployeeID,
			*Employee.EmployeeName,
			*EmployeeMgr->GetRankDisplayName(Rank),
			Employee.EnhancementLevel,
			Employee.AssignedBuildingIndex);
	}

	UE_LOG(LogTemp, Warning, TEXT("==============================="));
	UE_LOG(LogTemp, Warning, TEXT(""));
}

void UCGCheatManager::EmpName(int32 RarityValue, int32 Count)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:EmpName] GameInstance not found"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:EmpName] EmployeeManager not found"));
		return;
	}

	const ELootBoxRarity Rarity = static_cast<ELootBoxRarity>(FMath::Clamp(RarityValue, 0, 5));
	const int32 SafeCount = FMath::Clamp(Count, 1, 500);

	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("========== EmpName (Rarity %d, %d개) =========="), (int32)Rarity, SafeCount);

	for (int32 i = 0; i < SafeCount; ++i)
	{
		UE_LOG(LogTemp, Warning, TEXT("  %s"), *EmployeeMgr->GenerateRandomName(Rarity));
	}

	UE_LOG(LogTemp, Warning, TEXT("=============================================="));
}

void UCGCheatManager::SetRank(int32 EnhancementLevel)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetRank] GameInstance not found"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetRank] EmployeeManager not found"));
		return;
	}

	int32 SelectedID = EmployeeMgr->GetSelectedEmployeeID();
	if (SelectedID == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetRank] No employee selected"));
		return;
	}

	FEmployeeInstance* Employee = EmployeeMgr->FindEmployee(SelectedID);
	if (!Employee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetRank] Employee not found"));
		return;
	}

	int32 OldLevel = Employee->EnhancementLevel;
	Employee->EnhancementLevel = FMath::Clamp(EnhancementLevel, 0, 12);

	EEmployeeRank NewRank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetRank] %s: +%d -> +%d (%s)"),
		*Employee->EmployeeName, OldLevel, Employee->EnhancementLevel, *EmployeeMgr->GetRankDisplayName(NewRank));

	// 강화 레벨은 유효스탯(GetEnhanceStatBonus) 파생 경로라 운영 수익 캐시도 갱신 필요
	if (Employee->EnhancementLevel != OldLevel && Employee->AssignedBuildingIndex != INDEX_NONE)
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->InvalidateStatBonusCache(Employee->AssignedBuildingIndex);
		}
	}

	// 외모 업데이트
	EmployeeMgr->UpdateOfficeworkerAppearance(SelectedID);
}

void UCGCheatManager::SetStat(int32 StatIndex, int32 Value, int32 EmployeeID)
{
	if (StatIndex < 0 || StatIndex > 5)
	{
		// 표시명 단일 진실 = GetStatDisplayName (UMETA DisplayName 바뀌어도 경고 문구 자동 반영)
		FString Legend;
		for (uint8 i = 0; i < 6; ++i)
		{
			Legend += FString::Printf(TEXT("%d=%s "), i, *UEmployeeStatsHelper::GetStatDisplayName(i).ToString());
		}
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetStat] StatIndex 는 0~5 (%s)"), *Legend.TrimEnd());
		return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetStat] GameInstance not found"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetStat] EmployeeManager not found"));
		return;
	}

	int32 TargetID = (EmployeeID == -1) ? EmployeeMgr->GetSelectedEmployeeID() : EmployeeID;
	if (TargetID == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetStat] No employee selected. Usage: SetStat [Index] [Value] [EmployeeID]"));
		return;
	}

	FEmployeeInstance* Employee = EmployeeMgr->FindEmployee(TargetID);
	if (!Employee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetStat] Employee %d not found"), TargetID);
		return;
	}

	const int32 ClampedValue = FMath::Max(0, Value);
	switch (StatIndex)
	{
	case 0: Employee->Stats.WorkSpeed   = ClampedValue; break;
	case 1: Employee->Stats.CritChance  = ClampedValue; break;
	case 2: Employee->Stats.Composure   = ClampedValue; break;
	case 3: Employee->Stats.ExpGain     = ClampedValue; break;
	case 4: Employee->Stats.Stamina     = ClampedValue; break;
	case 5: Employee->Stats.Focus       = ClampedValue; break;
	default: break;
	}

	// 운영 수익 캐시 무효화 훅은 없앴다 — 직원 스탯의 수익 기여가 2026-08-13 폐지돼(구 IncomeBonus → Focus)
	// 어떤 스탯을 바꿔도 요율이 안 변한다. 남은 축(잠재 큐브)은 자기 변경 지점에서 무효화한다.

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetStat] %s (ID:%d) %s = %d"),
		*Employee->EmployeeName, TargetID,
		*UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(StatIndex)).ToString(), ClampedValue);
}

void UCGCheatManager::StatStatus(int32 EmployeeID)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:StatStatus] GameInstance not found"));
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:StatStatus] EmployeeManager not found"));
		return;
	}

	int32 TargetID = (EmployeeID == -1) ? EmployeeMgr->GetSelectedEmployeeID() : EmployeeID;
	if (TargetID == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:StatStatus] No employee selected. Usage: StatStatus [EmployeeID]"));
		return;
	}

	FEmployeeInstance* Employee = EmployeeMgr->FindEmployee(TargetID);
	if (!Employee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:StatStatus] Employee %d not found"), TargetID);
		return;
	}

	const int32 EnhB = UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);

	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("========== %s (ID:%d) Lv.%d +%d Stats =========="),
		*Employee->EmployeeName, TargetID, Employee->Level, Employee->EnhancementLevel);

	for (uint8 i = 0; i < 6; ++i)
	{
		const int32 Base = UEmployeeStatsHelper::GetStatValueByIndex(Employee->Stats, i);
		UE_LOG(LogTemp, Warning, TEXT("  %s: %d (+%d) = %d"),
			*UEmployeeStatsHelper::GetStatDisplayName(i).ToString(), Base, EnhB, Base + EnhB);
	}

	UE_LOG(LogTemp, Warning, TEXT("  Overall: %d"), UEmployeeStatsHelper::CalculateOverall(Employee->Stats));
	UE_LOG(LogTemp, Warning, TEXT("==============================================="));
	UE_LOG(LogTemp, Warning, TEXT(""));
}

void UCGCheatManager::SetCrit(float Percent)
{
	UWorld* W = GetWorld();
	UOfficeStageProgressManager* StageMgr = W ? W->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SetCrit] UOfficeStageProgressManager 없음 (OfficeMap 에서 실행)"));
		return;
	}

	const float Chance01 = (Percent >= 0.f) ? (Percent / 100.f) : -1.f;
	StageMgr->SetCritChanceOverride(Chance01);

	if (Percent >= 0.f)
	{
		const float Applied = FMath::Clamp(Percent, 0.f, 100.f);
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SetCrit] 크리티컬 확률 = %.0f%% (강제 오버라이드)"), Applied);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				FString::Printf(TEXT("[SetCrit] 크리 %.0f%%"), Applied));
		}
	}
	else
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SetCrit] 크리티컬 확률 오버라이드 해제 (정상 복귀)"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("[SetCrit] 정상 복귀"));
		}
	}
}

// ========== 스테이지 관련 ==========

void UCGCheatManager::CompleteStep()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:CompleteStep] World not found"));
		return;
	}

	UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:CompleteStep] Not in OfficeMap or StageProgressManager not found"));
		return;
	}

	if (!StageMgr->IsStageInProgress())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:CompleteStep] No stage in progress"));
		return;
	}

	// 3개 카테고리 각각 MinimumScore 이상 되도록 일괄 보너스 부여
	const FStageProgressData& ProgData = StageMgr->GetProgressData();
	float MaxNeeded = 0.0f;
	for (int32 i = 0; i < 3 && i < ProgData.Steps.Num(); i++)
	{
		float Gap = ProgData.Steps[i].MinimumScore - ProgData.Steps[i].AcquiredScore + 10.0f;
		MaxNeeded = FMath::Max(MaxNeeded, Gap);
	}

	if (MaxNeeded > 0.0f)
	{
		// AddBonusScore는 3개 카테고리 균등 분배이므로 필요분 × 3 지급
		StageMgr->AddBonusScore(MaxNeeded * 3.0f);
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:CompleteStep] Added %.1f bonus score to clear minimum across 3 categories"), MaxNeeded * 3.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:CompleteStep] All 3 categories already meet minimum"));
	}
}

void UCGCheatManager::Stage_FillScores(int32 Percent)
{
	UWorld* World = GetWorld();
	UOfficeStageProgressManager* StageMgr = World ? World->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Stage_FillScores] Not in OfficeMap or StageProgressManager not found"));
		return;
	}
	if (!StageMgr->IsStageInProgress())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:Stage_FillScores] No stage in progress"));
		return;
	}
	StageMgr->Debug_FillScores(FMath::Clamp(Percent, 0, 200) / 100.0f);
}

void UCGCheatManager::StageStatus()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:StageStatus] World not found"));
		return;
	}

	UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:StageStatus] Not in OfficeMap"));
		return;
	}

	const FStageProgressData& Data = StageMgr->GetProgressData();

	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("========== Stage Status =========="));
	UE_LOG(LogTemp, Warning, TEXT("  Project: %s (Number: %d)"), *Data.ProjectName, Data.ProjectNumber);
	UE_LOG(LogTemp, Warning, TEXT("  Stage: %d | Step: %d"), Data.StageNumber, Data.CurrentStep);
	for (int32 i = 0; i < 3 && i < Data.Steps.Num(); i++)
	{
		const FStepRoundData& S = Data.Steps[i];
		UE_LOG(LogTemp, Warning, TEXT("  Step %d: %.1f / %.1f (min %.1f)"),
			i + 1, S.AcquiredScore, S.TargetScore, S.MinimumScore);
	}
	UE_LOG(LogTemp, Warning, TEXT("  Time: %.1f sec remaining"), Data.RemainingTime);
	UE_LOG(LogTemp, Warning, TEXT("  Quality: %s (%.2f)"),
		*QualityGradeToAlphabetString(Data.CalculateQualityGrade()),
		Data.CalculateQualityScore());
	UE_LOG(LogTemp, Warning, TEXT("  Timer Running: %s"),
		Data.bIsTimerRunning ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("=================================="));
	UE_LOG(LogTemp, Warning, TEXT(""));
}

// ========== 운영 관련 ==========

void UCGCheatManager::SetOpTime(float Seconds)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetOpTime] GameInstance not found"));
		return;
	}

	int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetOpTime] No building selected"));
		return;
	}

	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OpMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetOpTime] ProjectOperationManager not found"));
		return;
	}

	FOperationData* Op = OpMgr->GetOperationByBuildingID(BuildingIndex);
	if (!Op)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetOpTime] No active operation for building %d"), BuildingIndex);
		return;
	}

	float OldRemaining = Op->RemainingTime;
	float NewRemaining = FMath::Max(Seconds, 0.0f);
	Op->RemainingTime = NewRemaining;
	Op->ElapsedTime = Op->TotalOperationTime - NewRemaining;

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetOpTime] Building %d: %.1f sec -> %.1f sec remaining"),
		BuildingIndex, OldRemaining, NewRemaining);
}

void UCGCheatManager::OpStatus()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:OpStatus] GameInstance not found"));
		return;
	}

	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OpMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:OpStatus] ProjectOperationManager not found"));
		return;
	}

	const TArray<FOperationData>& Operations = OpMgr->GetActiveOperations();

	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("========== Operation Status =========="));
	UE_LOG(LogTemp, Warning, TEXT("  Active Operations: %d"), Operations.Num());

	for (int32 i = 0; i < Operations.Num(); i++)
	{
		const FOperationData& Op = Operations[i];
		UE_LOG(LogTemp, Warning, TEXT("  [%d] %s | Bld:%d | Quality:%s (%.2f)"),
			i, *Op.ProjectName, Op.BuildingID,
			*QualityGradeToAlphabetString(Op.QualityGrade), Op.QualityScore);
		UE_LOG(LogTemp, Warning, TEXT("       Time: %.0f/%.0f sec (%.0f sec left) | Revenue: %.1f/sec | Total: %.0f"),
			Op.ElapsedTime, Op.TotalOperationTime, Op.RemainingTime,
			Op.ActualRevenuePerSecond, Op.TotalRevenueEarned);
	}

	UE_LOG(LogTemp, Warning, TEXT("======================================"));
	UE_LOG(LogTemp, Warning, TEXT(""));
}

void UCGCheatManager::RevenueMax(int64 AmountPerBuilding)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:RevenueMax] GameInstance not found"));
		return;
	}

	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OpMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:RevenueMax] ProjectOperationManager not found"));
		return;
	}

	const int32 Filled = OpMgr->FillAllStoredRevenue(AmountPerBuilding);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:RevenueMax] %d개 건물 금고 각 %lld원 충전 — 전체수거 버튼으로 대량 수집"), Filled, AmountPerBuilding);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
			FString::Printf(TEXT("[RevenueMax] %d개 건물 각 %lld원"), Filled, AmountPerBuilding));
	}
}

void UCGCheatManager::ShowOfflineReport(int32 OfflineHours, int32 bZeroGain, int32 FakeRows)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ShowOfflineReport] GameInstance not found"));
		return;
	}

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!SaveLoadMgr || !UIMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:ShowOfflineReport] SaveLoad/UI 매니저 없음"));
		return;
	}

	const int32 Hours = FMath::Clamp(OfflineHours, 1, 24);
	const bool bZero = (bZeroGain != 0);

	// 기본 = 실제 지은 건물만(이름/아이콘 해석). FakeRows>0 일 때만 그 개수까지 가짜 인덱스로 패딩.
	TArray<int32> Indices;
	if (UWorld* World = GetWorld())
	{
		if (UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>())
		{
			for (ABuildingBaseActor* Building : EntityMgr->GetBuildings())
			{
				if (Building) { Indices.Add(Building->GetBuildingIndex()); }
			}
		}
	}
	const int32 TargetRows = FMath::Clamp(FakeRows, 0, 30);
	for (int32 i = Indices.Num(); i < TargetRows; ++i)
	{
		Indices.Add(9000 + i);  // 실제 건물과 안 겹치는 합성 인덱스 → 행 "건물 N" 폴백(레이아웃/스크롤 테스트)
	}
	if (Indices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Cheat:ShowOfflineReport] 실제 건물 0개 → 빈 모달. 테스트 행은 3번째 인자로: ShowOfflineReport 3 0 8"));
	}

	TArray<FOfflineGainEntry> Entries;
	float Total = 0.0f;
	for (int32 i = 0; i < Indices.Num(); ++i)
	{
		FOfflineGainEntry Entry;
		Entry.BuildingIndex = Indices[i];
		const float BaseRate = 800.0f + i * 520.0f;                        // 원/초 근사(빌딩별 차등)
		Entry.RawGain = BaseRate * Hours * 3600.0f * 0.4f;                 // idle rate 근사
		const bool bHasLoss = (i % 2 == 1);                                // 절반은 금고 초과 손실
		Entry.ActualGain = bZero ? 0.0f : (bHasLoss ? Entry.RawGain * 0.7f : Entry.RawGain);
		Entry.LossByVault = bZero ? Entry.RawGain : (Entry.RawGain - Entry.ActualGain);
		Entry.VaultLevel = i * 3;
		Entry.bLifespanBound = (i % 3 == 2);                               // 일부는 운영 종료 태그
		Entries.Add(Entry);
		Total += Entry.ActualGain;
	}

	// 실제 금고도 같이 채운다 — [모두 수령] 이 진짜 코인을 수거하게(표시 데이터만 주입하면 빈 금고라
	// "수거할 수익 없음" 경고가 뜬다). 금고 내용 = 모달 표시값과 정합: 일반은 ActualGain(적립분),
	// 적립0 변주는 RawGain(만석 잔량 — 히어로 0 vs 수령액은 다를 수 있음, 설계상 정상).
	if (USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData())
	{
		for (const FOfflineGainEntry& Entry : Entries)
		{
			FOfficeSaveData& Office = SaveData->GameData.OfficeDataMap.FindOrAdd(Entry.BuildingIndex);
			Office.StoredRevenue = bZero ? Entry.RawGain : Entry.ActualGain;
		}
	}

	const bool bCapReached = (Hours >= 12);
	SaveLoadMgr->DebugSetPendingOfflineReport(Total, Hours * 3600.0f, Entries, bCapReached);
	UIMgr->TryShowOfflineReport();

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:ShowOfflineReport] %d행, 총 %.0f원, %dh, 캡=%s, zero=%s"),
		Entries.Num(), Total, Hours, bCapReached ? TEXT("Y") : TEXT("N"), bZero ? TEXT("Y") : TEXT("N"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
			FString::Printf(TEXT("[ShowOfflineReport] %d행 / %dh / zero=%d"), Entries.Num(), Hours, bZero ? 1 : 0));
	}
}

// ========== 아이템 관련 ==========

void UCGCheatManager::Ticket(int32 Amount, int32 TierValue)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
	if (!ItemMgr) return;

	// Tier: 0=Normal, 1=Advanced, 2=Premium
	EItemType TicketType;
	FString TierName;
	switch (FMath::Clamp(TierValue, 0, 2))
	{
	case 0: TicketType = EItemType::RecruitTicketNormal;   TierName = TEXT("Normal");   break;
	case 1: TicketType = EItemType::RecruitTicketAdvanced;  TierName = TEXT("Advanced");  break;
	case 2: TicketType = EItemType::RecruitTicketPremium;   TierName = TEXT("Premium");   break;
	default: TicketType = EItemType::RecruitTicketNormal;   TierName = TEXT("Normal");   break;
	}

	int32 OldCount = ItemMgr->GetItemCount(TicketType);
	ItemMgr->AddItem(TicketType, Amount);
	int32 NewCount = ItemMgr->GetItemCount(TicketType);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:Ticket] %s: %d -> %d (+%d)"),
		*TierName, OldCount, NewCount, Amount);
}

void UCGCheatManager::TraitTicket(int32 Amount, int32 TierValue)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
	if (!ItemMgr) return;

	if (Amount <= 0) Amount = 30;

	// AddItem 기본 bShouldSave=true → OnItemChanged 브로드캐스트 → 가챠 패널 CTA(보유 수량) 즉시 갱신
	auto GrantOne = [&](EItemType Type, const TCHAR* Label)
	{
		const int32 Old = ItemMgr->GetItemCount(Type);
		ItemMgr->AddItem(Type, Amount);
		const int32 New = ItemMgr->GetItemCount(Type);
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:TraitTicket] %s: %d -> %d (+%d)"), Label, Old, New, Amount);
	};

	// Tier: 0=Normal 만, 1=Advanced 만, 그 외(-1 기본)=둘 다
	const bool bNormal   = (TierValue != 1);
	const bool bAdvanced = (TierValue != 0);

	if (bNormal)   GrantOne(EItemType::BuildingTraitTicketNormal,   TEXT("Normal"));
	if (bAdvanced) GrantOne(EItemType::BuildingTraitTicketAdvanced, TEXT("Advanced"));

	if (GEngine)
	{
		const TCHAR* Scope = (TierValue == 0) ? TEXT("Normal") : (TierValue == 1) ? TEXT("Advanced") : TEXT("Both");
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[TraitTicket] +%d (%s)"), Amount, Scope));
	}
}

void UCGCheatManager::SkinTicket(int32 Amount, int32 TierValue)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
	if (!ItemMgr) return;

	if (Amount <= 0) Amount = 30;

	auto GrantOne = [&](EItemType Type, const TCHAR* Label)
	{
		const int32 Old = ItemMgr->GetItemCount(Type);
		ItemMgr->AddItem(Type, Amount);
		const int32 New = ItemMgr->GetItemCount(Type);
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SkinTicket] %s: %d -> %d (+%d)"), Label, Old, New, Amount);
	};

	// Tier: 0=Normal 만, 1=Advanced 만, 그 외(-1 기본)=둘 다
	const bool bNormal   = (TierValue != 1);
	const bool bAdvanced = (TierValue != 0);

	if (bNormal)   GrantOne(EItemType::SkinTicketNormal,   TEXT("Normal"));
	if (bAdvanced) GrantOne(EItemType::SkinTicketAdvanced, TEXT("Advanced"));

	if (GEngine)
	{
		const TCHAR* Scope = (TierValue == 0) ? TEXT("Normal") : (TierValue == 1) ? TEXT("Advanced") : TEXT("Both");
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[SkinTicket] +%d (%s)"), Amount, Scope));
	}
}

void UCGCheatManager::Card(int32 Amount, int32 TierValue)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
	if (!ItemMgr) return;

	if (Amount <= 0) Amount = 20;

	// AddItem 기본 bShouldSave=true → OnItemChanged 브로드캐스트 → 직원창 명함 슬롯 수량/리롤 버튼 즉시 갱신
	auto GrantOne = [&](EItemType Type, const TCHAR* Label)
	{
		const int32 Old = ItemMgr->GetItemCount(Type);
		ItemMgr->AddItem(Type, Amount);
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Card] %s: %d -> %d (+%d)"),
			Label, Old, ItemMgr->GetItemCount(Type), Amount);
	};

	if (TierValue != 1 && TierValue != 2) GrantOne(EItemType::BusinessCardPaper, TEXT("Paper"));
	if (TierValue != 0 && TierValue != 2) GrantOne(EItemType::BusinessCardGold,  TEXT("Gold"));
	if (TierValue != 0 && TierValue != 1) GrantOne(EItemType::BusinessCardBlack, TEXT("Black"));

	if (GEngine)
	{
		const TCHAR* Scope = (TierValue == 0) ? TEXT("Paper") : (TierValue == 1) ? TEXT("Gold")
			: (TierValue == 2) ? TEXT("Black") : TEXT("All");
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[Card] +%d (%s)"), Amount, Scope));
	}
}

void UCGCheatManager::LootRoll(FString TableKey, int32 Tier, int32 Count)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:LootRoll] GameInstance not found"));
		return;
	}

	ULaunchLootManagerSubsystem* Loot = GI->GetSubsystem<ULaunchLootManagerSubsystem>();
	if (!Loot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:LootRoll] LaunchLootManagerSubsystem not found"));
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		Loot->RollAndGrant(FName(*TableKey), Tier, /*ScaleBasis=*/100000, /*ExtraRolls=*/0, /*bGrant=*/false);
	}
	UE_LOG(LogTemp, Warning, TEXT("[Cheat:LootRoll] %s Tier=%d x%d 시뮬 완료 (로그 위 참조)"), *TableKey, Tier, Count);
}

// ========== 건물 관련 ==========

void UCGCheatManager::UnlockSkins(int32 Count)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:UnlockSkins] GameInstance not found"));
		return;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!TableMgr || !SaveMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:UnlockSkins] TableManager or SaveLoadManager not found"));
		return;
	}

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:UnlockSkins] No save data"));
		return;
	}

	// 전체 스킨 목록 수집
	TArray<FBuildingSkinData> AllSkins;
	for (ELootBoxRarity Rarity : FLootBoxRarityUtility::GetAllRarities())
	{
		AllSkins.Append(TableMgr->GetBuildingSkinsByRarity(Rarity));
	}

	// 이미 보유한 SkinID Set 구축
	TSet<int32> OwnedSkinIDs;
	for (const FBuildingSkinInstance& Owned : SaveData->GameData.OwnedBuildingSkins)
	{
		OwnedSkinIDs.Add(Owned.SkinID);
	}

	// 미보유 스킨 필터링
	TArray<FBuildingSkinData> UnownedSkins;
	for (const FBuildingSkinData& Skin : AllSkins)
	{
		if (!OwnedSkinIDs.Contains(Skin.SkinID))
		{
			UnownedSkins.Add(Skin);
		}
	}

	if (UnownedSkins.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat:UnlockSkins] All skins already owned!"));
		return;
	}

	// Fisher-Yates 셔플
	for (int32 i = UnownedSkins.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		UnownedSkins.Swap(i, j);
	}

	// Count개 선택 (미보유 수보다 크면 전부)
	int32 ActualCount = FMath::Min(Count, UnownedSkins.Num());

	for (int32 i = 0; i < ActualCount; ++i)
	{
		const FBuildingSkinData& Skin = UnownedSkins[i];
		SaveData->GameData.OwnedBuildingSkins.Add(FBuildingSkinInstance(Skin.SkinID));

		UE_LOG(LogTemp, Warning, TEXT("[Cheat:UnlockSkins] Unlocked: %s (%s) ID:%d"),
			*Skin.DisplayName.ToString(),
			*FLootBoxRarityUtility::GetKoreanName(Skin.Rarity),
			Skin.SkinID);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:UnlockSkins] Total unlocked: %d / %d requested (Remaining unowned: %d)"),
		ActualCount, Count, UnownedSkins.Num() - ActualCount);
}

void UCGCheatManager::BldLightsOn(int32 bOn)
{
	UWorld* W = GetWorld();
	UEntityManager* EntityMgr = W ? W->GetSubsystem<UEntityManager>() : nullptr;
	if (!EntityMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:BldLightsOn] UEntityManager 없음 (MainMap 에서 실행)"));
		return;
	}

	const bool bForce = (bOn != 0);
	ABuildingBaseActor::SetForceWindowLightsOn(bForce);

	// 해제 시 정상 정책(운영 중만 점등) 복원용 — 강제 켜기면 불필요
	UProjectOperationManager* OpMgr = (!bForce && W->GetGameInstance())
		? W->GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr;

	int32 Count = 0;
	for (ABuildingBaseActor* B : EntityMgr->GetBuildings())
	{
		if (!B) { continue; }
		// 강제 켜기면 무조건 true, 해제면 운영 여부로 원복(InGameLayer 정책과 동일)
		const bool bActive = bForce || (OpMgr && OpMgr->HasActiveOperation(B->GetBuildingIndex()));
		B->SetWindowLightActive(bActive);
		++Count;
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:BldLightsOn] force=%s, %d개 건물 조명 %s"),
		bForce ? TEXT("ON") : TEXT("OFF"), Count, bForce ? TEXT("항상 켬 고정") : TEXT("정상(운영 기준) 복원"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[BldLightsOn] %s (%d개 건물)"), bForce ? TEXT("항상 켬") : TEXT("정상"), Count));
	}
}

// ========== 본사 레벨 관련 ==========

void UCGCheatManager::HQLevelUp()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return;

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return;

	int32 OldLevel = SaveData->GameData.HQLevel;
	SaveData->GameData.HQLevel = OldLevel + 1;

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:HQLevelUp] HQ Lv.%d -> Lv.%d (조건 무시)"),
		OldLevel, SaveData->GameData.HQLevel);

	SaveMgr->SaveGameData();
	SaveMgr->OnHQLevelUp.Broadcast(SaveData->GameData.HQLevel);
}

void UCGCheatManager::SetHQLv(int32 Level)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return;

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return;

	int32 OldLevel = SaveData->GameData.HQLevel;
	SaveData->GameData.HQLevel = FMath::Max(1, Level);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat:SetHQLv] HQ Lv.%d -> Lv.%d"),
		OldLevel, SaveData->GameData.HQLevel);

	SaveMgr->SaveGameData();
	SaveMgr->OnHQLevelUp.Broadcast(SaveData->GameData.HQLevel);
}

// ========== 채광 (UMineManager) ==========

void UCGCheatManager::MineDebug()
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	if (!MineMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineDebug] UMineManager 없음"));
		return;
	}

	const UEnum* CountryEnum = StaticEnum<ECountryType>();
	const UEnum* ResourceEnum = StaticEnum<EResourceType>();
	if (!CountryEnum || !ResourceEnum) return;

	int32 PrintedCountries = 0;
	const int32 NumEnums = CountryEnum->NumEnums() - 1;  // _MAX 제외
	for (int32 i = 0; i < NumEnums; ++i)
	{
		const ECountryType Country = static_cast<ECountryType>(CountryEnum->GetValueByIndex(i));
		const TArray<FMineLineState> Lines = MineMgr->GetMineLines(Country);
		if (Lines.Num() == 0) continue;

		const FString CountryStr = CountryEnum->GetNameStringByIndex(i);
		const int64 MaxStorage = MineMgr->GetMaxStorage(Country);
		const int32 RateLv = MineMgr->GetUpgradeLevel(Country, EMineUpgradeType::MiningRate);
		const int32 StorageLv = MineMgr->GetUpgradeLevel(Country, EMineUpgradeType::MiningStorage);

		UE_LOG(LogCGCheat, Warning, TEXT("[Mine %s] RateLv=%d StorageLv=%d MaxStorage=%lld"),
			*CountryStr, RateLv, StorageLv, MaxStorage);

		for (const FMineLineState& Line : Lines)
		{
			const float RatePerMin = MineMgr->GetEffectiveRatePerMinute(Country, Line.Resource);
			const FString ResStr = ResourceEnum->GetNameStringByValue(static_cast<int64>(Line.Resource));
			const float Pct = (MaxStorage > 0) ? 100.0f * static_cast<float>(Line.CurrentQty) / static_cast<float>(MaxStorage) : 0.0f;

			UE_LOG(LogCGCheat, Warning, TEXT("  %s: %lld/%lld (%.1f%%) Rate=%.2f/min Sat=%s"),
				*ResStr, Line.CurrentQty, MaxStorage, Pct, RatePerMin,
				Line.bSaturated ? TEXT("true") : TEXT("false"));
		}
		++PrintedCountries;
	}

	if (PrintedCountries == 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineDebug] 활성 채광 라인 없음. CountryDetail 채광 탭 진입하면 EnsureCountryLines 자동 호출됨."));
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("[MineDebug] %d countries logged"), PrintedCountries));
	}
}

void UCGCheatManager::MineFill(const FString& CountryName)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	if (!MineMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineFill] UMineManager 없음"));
		return;
	}

	const UEnum* CountryEnum = StaticEnum<ECountryType>();
	const int64 EnumValue = CountryEnum->GetValueByNameString(CountryName);
	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineFill] Country not found: %s (예: Australia, Canada, Brazil, SouthAfrica, China, Saudi)"), *CountryName);
		return;
	}

	const ECountryType Country = static_cast<ECountryType>(EnumValue);
	MineMgr->EnsureCountryLines(Country);
	MineMgr->DebugFillAllLines(Country);

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineFill] %s: 모든 라인 한도까지 채움"), *CountryName);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[MineFill] %s filled"), *CountryName));
	}
}

void UCGCheatManager::MineSetLv(const FString& CountryName, const FString& UpgradeName, int32 Level)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	if (!MineMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineSetLv] UMineManager 없음"));
		return;
	}

	const UEnum* CountryEnum = StaticEnum<ECountryType>();
	const int64 EnumValue = CountryEnum->GetValueByNameString(CountryName);
	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineSetLv] Country not found: %s"), *CountryName);
		return;
	}
	const ECountryType Country = static_cast<ECountryType>(EnumValue);

	EMineUpgradeType UpgradeType = EMineUpgradeType::None;
	if (UpgradeName.Equals(TEXT("Rate"), ESearchCase::IgnoreCase) || UpgradeName.Equals(TEXT("MiningRate"), ESearchCase::IgnoreCase))
	{
		UpgradeType = EMineUpgradeType::MiningRate;
	}
	else if (UpgradeName.Equals(TEXT("Storage"), ESearchCase::IgnoreCase) || UpgradeName.Equals(TEXT("MiningStorage"), ESearchCase::IgnoreCase))
	{
		UpgradeType = EMineUpgradeType::MiningStorage;
	}
	else
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineSetLv] UpgradeName 인식 불가: %s (Rate / Storage)"), *UpgradeName);
		return;
	}

	const int32 ClampedLevel = FMath::Max(0, Level);
	const int32 CurrentLevel = MineMgr->GetUpgradeLevel(Country, UpgradeType);
	const int32 Delta = ClampedLevel - CurrentLevel;

	if (Delta == 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineSetLv] 이미 Lv.%d"), ClampedLevel);
		return;
	}

	if (Delta > 0)
	{
		// Up: UpgradeLevelUp 반복 호출 (델리게이트 정상 트리거 + UI 갱신)
		MineMgr->EnsureCountryLines(Country);
		for (int32 i = 0; i < Delta; ++i)
		{
			if (!MineMgr->UpgradeLevelUp(Country, UpgradeType)) break;  // MaxLevel 도달
		}
	}
	else
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineSetLv] 레벨 다운은 미지원. 더 낮은 레벨로 가려면 새 세이브 필요."));
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MineSetLv] %s.%s: Lv.%d -> Lv.%d"),
		*CountryName, *UpgradeName, CurrentLevel, MineMgr->GetUpgradeLevel(Country, UpgradeType));
}

void UCGCheatManager::SeedProducts(int32 NumSlots, int64 MinQty, int64 MaxQty)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UWorldMapManager* WorldMapMgr = GI ? GI->GetSubsystem<UWorldMapManager>() : nullptr;
	if (!WorldMapMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SeedProducts] UWorldMapManager 없음 (WorldMap 레벨에서만 동작)"));
		return;
	}

	WorldMapMgr->DebugSeedRandomProducts(NumSlots, MinQty, MaxQty);

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SeedProducts] %d slots (qty %lld~%lld) 시드 완료"),
		NumSlots, MinQty, MaxQty);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[SeedProducts] %d slots seeded"), NumSlots));
	}
}

void UCGCheatManager::SeedTraits(int32 NumKinds, int32 MinQty, int32 MaxQty)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UBuildingTraitManagerSubsystem* TraitMgr = GI ? GI->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TraitMgr || !TableMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SeedTraits] Manager 없음"));
		return;
	}

	TArray<FBuildingTraitTableRow> AllTraits = TableMgr->GetAllBuildingTraits();
	if (AllTraits.Num() == 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SeedTraits] DT_BuildingTrait 비어있음"));
		return;
	}

	// 랜덤 N개 선택 (셔플 후 앞에서 NumKinds 만큼)
	const int32 N = FMath::Clamp(NumKinds, 1, AllTraits.Num());
	for (int32 i = AllTraits.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		AllTraits.Swap(i, j);
	}

	int32 TotalGranted = 0;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Qty = FMath::RandRange(MinQty, MaxQty);
		TraitMgr->GrantTrait(AllTraits[i].TraitID, Qty, false);
		TotalGranted += Qty;
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SeedTraits] %d종 %d장 시드 완료"), N, TotalGranted);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[SeedTraits] %d kinds, %d total seeded"), N, TotalGranted));
	}
}

void UCGCheatManager::TraitEffects(int32 BuildingIndex)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UBuildingTraitManagerSubsystem* TraitMgr = GI ? GI->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr;
	if (!TraitMgr) { UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:TraitEffects] TraitManager 없음")); return; }

	int32 Idx = BuildingIndex;
	if (Idx < 0)
	{
		UCGGameInstance* CGI = Cast<UCGGameInstance>(GI);
		Idx = CGI ? CGI->GetCurrentManagedBuildingIndex() : -1;
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:TraitEffects] Building %d"), Idx);

	const UEnum* TargetEnum = StaticEnum<EBuildingTraitTarget>();
	if (!TargetEnum) return;

	int32 Shown = 0;
	for (int32 i = 0; i < TargetEnum->NumEnums() - 1; ++i)
	{
		const EBuildingTraitTarget T = static_cast<EBuildingTraitTarget>(TargetEnum->GetValueByIndex(i));
		if (T == EBuildingTraitTarget::None) continue;

		const float Pct = TraitMgr->GetAggregatedTraitPercent(Idx, T);
		if (FMath::IsNearlyZero(Pct)) continue;

		UE_LOG(LogCGCheat, Warning, TEXT("  %-16s %+.2f  (factor %.4f)"),
			*GetTraitTargetDisplayName(T).ToString(), Pct, 1.0f + Pct / 100.0f);
		++Shown;
	}
	if (Shown == 0) { UE_LOG(LogCGCheat, Warning, TEXT("  (적용 중인 특성 효과 없음)")); }

	// 전사 적용은 건물 단위가 아니므로 따로 찍는다
	UE_LOG(LogCGCheat, Warning, TEXT("  [전사] 무역 판매가 %+.2f"),
		TraitMgr->GetCompanyWideTraitPercent(EBuildingTraitTarget::TradeValue));
}

void UCGCheatManager::TraitStatus()
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UBuildingTraitManagerSubsystem* TraitMgr = GI ? GI->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TraitMgr || !TableMgr) return;

	const TMap<FName, int32>& Owned = TraitMgr->GetAllOwnedTraits();
	int32 Total = 0;
	for (const auto& Pair : Owned) Total += Pair.Value;

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:TraitStatus] %d종 / %d장"), Owned.Num(), Total);
	for (const TPair<FName, int32>& Pair : Owned)
	{
		FBuildingTraitTableRow Row;
		if (TableMgr->GetBuildingTraitData(Pair.Key, Row))
		{
			UE_LOG(LogCGCheat, Warning, TEXT("  %s (R=%d) x%d"),
				*Row.DisplayName.ToString(), static_cast<int32>(Row.Rarity), Pair.Value);
		}
	}
}

// ========== 사무실 스타터 프리셋 ==========

void UCGCheatManager::ApplyStarterPreset(const FString& PresetRowName)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UCGGameInstance* CGI = Cast<UCGGameInstance>(GI);
	UOfficeManager* OfficeMgr = W ? W->GetSubsystem<UOfficeManager>() : nullptr;
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!CGI || !OfficeMgr || !SaveData)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ApplyStarterPreset] OfficeMap에서만 동작"));
		return;
	}

	const int32 BuildingIndex = CGI->GetCurrentManagedBuildingIndex();
	FOfficeSaveData* OfficeData = SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
	if (!OfficeData)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ApplyStarterPreset] OfficeData 없음 (Building %d)"), BuildingIndex);
		return;
	}

	// 1단계 시드와 동일하게 타일 크기도 프리셋 값으로 반영 (프리뷰 정확성 — 기존 확장 상태는 덮어씀)
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	bool bRowFound = false;
	const FOfficeStarterPresetRow PresetRow = TableMgr
		? TableMgr->GetStarterPresetRow(FName(*PresetRowName), bRowFound)
		: FOfficeStarterPresetRow();
	if (bRowFound)
	{
		OfficeData->TileCountX = PresetRow.TileCountX;
		OfficeData->TileCountY = PresetRow.TileCountY;
		OfficeData->StarterTileCountX = PresetRow.TileCountX;
		OfficeData->StarterTileCountY = PresetRow.TileCountY;
	}

	OfficeData->PendingStarterPreset = FName(*PresetRowName);
	if (OfficeMgr->ApplyStarterPresetToData(*OfficeData, CGI->GetCurrentBuildingCompanyType()))
	{
		// 기존 복원기 재실행으로 즉시 반영
		if (AOfficeInterior* Interior = Cast<AOfficeInterior>(
			UGameplayStatics::GetActorOfClass(W, AOfficeInterior::StaticClass())))
		{
			Interior->ApplyOfficeSaveData(*OfficeData);
		}
		OfficeMgr->ApplyOfficeSaveData(*OfficeData);
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ApplyStarterPreset] %s 적용 완료"), *PresetRowName);
	}
}

void UCGCheatManager::ExportStarterPreset()
{
	UWorld* W = GetWorld();
	UOfficeManager* OfficeMgr = W ? W->GetSubsystem<UOfficeManager>() : nullptr;
	AOfficeInterior* Interior = W ? Cast<AOfficeInterior>(
		UGameplayStatics::GetActorOfClass(W, AOfficeInterior::StaticClass())) : nullptr;
	if (!OfficeMgr || !Interior)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ExportStarterPreset] OfficeMap에서만 동작"));
		return;
	}

	FTransform Origin = Interior->GetActorTransform();
	Origin.SetScale3D(FVector::OneVector); // 2단계 적용과 동일 기준 (스케일 미반영 상대좌표)
	FString Out = TEXT("\"(");
	bool bFirst = true;
	for (const ADecorationActor* Deco : OfficeMgr->GetPlacedDecorations())
	{
		if (!Deco) continue;
		const FTransform Rel = Deco->GetActorTransform().GetRelativeTransform(Origin);
		const FVector L = Rel.GetLocation();
		if (!bFirst) Out += TEXT(",");
		bFirst = false;
		Out += FString::Printf(TEXT("(DecorationRowName=\"\"%s\"\",Slot=None,RelLocation=(X=%.0f,Y=%.0f,Z=%.0f),YawDeg=%.0f)"),
			*Deco->DecorationCardTableRowName.ToString(), L.X, L.Y, L.Z, Rel.Rotator().Yaw);
	}
	Out += TEXT(")\"");

	const FString Path = FPaths::ProjectSavedDir() / TEXT("StarterPresetExport.txt");
	FFileHelper::SaveStringToFile(Out, *Path);
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ExportStarterPreset] %d개 -> %s"),
		OfficeMgr->GetPlacedDecorations().Num(), *Path);
}

// ========== 카메라 각도 (OfficeMap 전용) ==========

void UCGCheatManager::CamPreset(int32 Index)
{
	APlayerController* PC = GetOuterAPlayerController();
	AOfficeCameraPawn* Pawn = PC ? Cast<AOfficeCameraPawn>(PC->GetPawn()) : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CamPreset] OfficeMap에서만 동작 (possess Pawn != OfficeCameraPawn)"));
		return;
	}

	// FOV는 40/30 고정 — 피치만 비교하도록
	float InP, OutP;
	FString Name;
	switch (Index)
	{
	case 0: InP = -32.f; OutP = -48.f; Name = TEXT("Cozy(~-38)");        break;
	case 1: InP = -40.f; OutP = -52.f; Name = TEXT("Balanced(~-45)");    break;
	case 2: InP = -48.f; OutP = -60.f; Name = TEXT("TopDown(~-53)");     break;
	case 3: InP = -56.f; OutP = -66.f; Name = TEXT("HardTopDown(~-60)"); break;
	default:
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CamPreset] Index는 0~3 (0코지 1밸런스 2탑다운 3하드탑다운)"));
		return;
	}

	Pawn->ApplyCameraBand(InP, OutP, 40.f, 30.f);
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CamPreset] %d -> %s (FOV 40/30 고정)"), Index, *Name);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
			FString::Printf(TEXT("CamPreset %d: %s"), Index, *Name));
	}
}

void UCGCheatManager::CamBand(float InPitch, float OutPitch, float InFOV, float OutFOV)
{
	APlayerController* PC = GetOuterAPlayerController();
	AOfficeCameraPawn* Pawn = PC ? Cast<AOfficeCameraPawn>(PC->GetPawn()) : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CamBand] OfficeMap에서만 동작"));
		return;
	}

	Pawn->ApplyCameraBand(InPitch, OutPitch, InFOV, OutFOV);
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CamBand] Pitch %.0f/%.0f  FOV %.0f/%.0f"), InPitch, OutPitch, InFOV, OutFOV);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
			FString::Printf(TEXT("CamBand P %.0f/%.0f F %.0f/%.0f"), InPitch, OutPitch, InFOV, OutFOV));
	}
}

// ========== 튜토리얼 (미션 점프) ==========

void UCGCheatManager::SkipToMission(const FString& MissionRowName)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SkipToMission] GameInstance not found"));
		return;
	}

	UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>();
	if (!MissionMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SkipToMission] MissionManager not found"));
		return;
	}

	// 점프 + 선행상태 시드 + 가이드 재구축 (RowName 없으면 매니저가 loud-fail 로그)
	MissionMgr->DevJumpToMission(FName(*MissionRowName));

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SkipToMission] → %s"), *MissionRowName);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("[Cheat] SkipToMission: %s"), *MissionRowName));
	}
}

void UCGCheatManager::ResetTutorial()
{
	// 세이브+초상화 삭제 — 다음 Play(Stop 후 재시작)에서 깨끗한 신규게임 = M1부터. 자원 시드 없음(M1 노가다 정상).
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	if (!SaveMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetTutorial] SaveLoadManager not found"));
		return;
	}

	SaveMgr->WipeAllSaveData();
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetTutorial] 세이브 삭제 — Stop 후 다시 Play 하면 M1부터"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Cyan, TEXT("[Cheat] ResetTutorial: 세이브 삭제됨 — Stop 후 다시 Play"));
	}
}

// ========== 미션판 (UGoalBoardSubsystem) ==========

void UCGCheatManager::UnlockGoalBoard()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:UnlockGoalBoard] GameInstance not found"));
		return;
	}

	UGoalBoardSubsystem* GoalMgr = GI->GetSubsystem<UGoalBoardSubsystem>();
	if (!GoalMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:UnlockGoalBoard] GoalBoardSubsystem not found"));
		return;
	}

	// UnlockBoard 는 이미 언락이면 조용히 반환 — 어느 쪽이었는지 로그로 구분
	const bool bWasUnlocked = GoalMgr->IsUnlocked();
	GoalMgr->UnlockBoard();

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:UnlockGoalBoard] %s"),
		bWasUnlocked ? TEXT("이미 언락 상태 — 변화 없음") : TEXT("언락 + 전수 재평가 + 저장"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			bWasUnlocked ? TEXT("[Cheat] UnlockGoalBoard: 이미 언락됨") : TEXT("[Cheat] UnlockGoalBoard: 미션판 언락"));
	}
}

void UCGCheatManager::CompleteGoal(const FString& GoalRowName)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CompleteGoal] GameInstance not found"));
		return;
	}

	UGoalBoardSubsystem* GoalMgr = GI->GetSubsystem<UGoalBoardSubsystem>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!GoalMgr || !TableMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CompleteGoal] GoalBoardSubsystem/TableManager not found"));
		return;
	}

	// 백엔드는 없는 RowName 을 조용히 무시하므로 여기서 먼저 걸러 유효 목록을 찍는다
	const FName GoalID(*GoalRowName);
	FGoalTable Row;
	if (!TableMgr->GetGoalData(GoalID, Row))
	{
		FString ValidIDs;
		for (const auto& Pair : TableMgr->GetAllGoalData())
		{
			if (!ValidIDs.IsEmpty())
			{
				ValidIDs += TEXT(", ");
			}
			ValidIDs += Pair.Key.ToString();
		}
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CompleteGoal] DT_Goal 에 없는 RowName: %s (가능: %s)"), *GoalRowName, *ValidIDs);
		return;
	}

	GoalMgr->DevCompleteGoal(GoalID);

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:CompleteGoal] %s 충족 처리 — 수령은 미션판에서%s"), *GoalRowName,
		GoalMgr->IsUnlocked() ? TEXT("") : TEXT(" (보드가 아직 잠김 — UnlockGoalBoard 먼저)"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("[Cheat] CompleteGoal: %s"), *GoalRowName));
	}
}

void UCGCheatManager::ResetGoals()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetGoals] GameInstance not found"));
		return;
	}

	UGoalBoardSubsystem* GoalMgr = GI->GetSubsystem<UGoalBoardSubsystem>();
	if (!GoalMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetGoals] GoalBoardSubsystem not found"));
		return;
	}

	GoalMgr->DevResetGoals();

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetGoals] 미션판 초기화 — 언락/충족/수령 전부 리셋 + 저장"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("[Cheat] ResetGoals: 미션판 초기화"));
	}
}

void UCGCheatManager::ResetPanelIntro()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetPanelIntro] GameInstance not found"));
		return;
	}

	UPanelIntroSubsystem* IntroMgr = GI->GetSubsystem<UPanelIntroSubsystem>();
	if (!IntroMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetPanelIntro] PanelIntroSubsystem not found"));
		return;
	}

	IntroMgr->ResetAll();

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:ResetPanelIntro] 패널 최초 진입 안내 초기화 — 다음 오픈부터 다시 재생 + 저장"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("[Cheat] ResetPanelIntro: 최초 진입 안내 초기화"));
	}
}

// ========== 시간대 / 낮밤 (ATimeCycleManager) ==========

ATimeCycleManager* UCGCheatManager::GetTimeCycleManager() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ATimeCycleManager* Mgr = Cast<ATimeCycleManager>(
		UGameplayStatics::GetActorOfClass(World, ATimeCycleManager::StaticClass()));

	if (!Mgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Time] TimeCycleManager 없음 (현재 맵에 배치되지 않음)"));
	}
	return Mgr;
}

void UCGCheatManager::Day()
{
	SetTime(12, 0);
}

void UCGCheatManager::Night()
{
	SetTime(0, 0);
}

void UCGCheatManager::SetTime(int32 Hour, int32 Minute)
{
	ATimeCycleManager* Mgr = GetTimeCycleManager();
	if (!Mgr)
	{
		return;
	}

	FTimeCycleCode NewTime;
	NewTime.Hours = FMath::Clamp(Hour, 0, 23);
	NewTime.Minutes = FMath::Clamp(Minute, 0, 59);
	NewTime.Seconds = 0;

	// 시간 세팅 후 사이클 정지 → 해당 시각의 낮/밤 룩으로 고정
	Mgr->SetCurrentTime(NewTime);
	Mgr->PauseCycle();

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:SetTime] %02d:%02d 고정 (사이클 정지)"),
		NewTime.Hours, NewTime.Minutes);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
			FString::Printf(TEXT("[Time] %02d:%02d 고정"), NewTime.Hours, NewTime.Minutes));
	}
}

void UCGCheatManager::TimeResume()
{
	ATimeCycleManager* Mgr = GetTimeCycleManager();
	if (!Mgr)
	{
		return;
	}

	Mgr->StartCycle();

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:TimeResume] 시간 사이클 재개 (고정 해제)"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, TEXT("[Time] 사이클 재개"));
	}
}

// ========== 백로그 수학 검증 ==========

void UCGCheatManager::CG_TestBacklogMath()
{
	UWorld* W = GetWorld(); if (!W) return;
	UProjectOperationManager* Mgr = W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr;
	if (!Mgr) { UE_LOG(LogTemp, Warning, TEXT("[BacklogMath] no manager")); return; }
	FBacklogProductEntry E; E.BasePeakRevenuePerSec = 100.0; E.OperatingCostPerSec = 10.0; E.LaunchWallClock = FDateTime::UtcNow();
	const FDateTime L = E.LaunchWallClock;
	UE_LOG(LogTemp, Warning, TEXT("[BacklogMath] peak100 cost10 halflife600: net@0=%.3f @60=%.3f @600=%.3f @3600=%.3f (expect ~90 / ~78.8 / ~40 / 0)"),
		Mgr->ComputeProductNetPerSec(E, L),
		Mgr->ComputeProductNetPerSec(E, L + FTimespan::FromSeconds(60)),
		Mgr->ComputeProductNetPerSec(E, L + FTimespan::FromSeconds(600)),
		Mgr->ComputeProductNetPerSec(E, L + FTimespan::FromSeconds(3600)));
}

void UCGCheatManager::CG_DumpBacklog()
{
	UWorld* W = GetWorld(); if (!W) return;
	UProjectOperationManager* Mgr = W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr;
	if (!Mgr) { UE_LOG(LogTemp, Warning, TEXT("[DumpBacklog] no manager")); return; }
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) { UE_LOG(LogTemp, Warning, TEXT("[DumpBacklog] no GI")); return; }
	const int32 BID = GI->GetCurrentManagedBuildingIndex();
	if (BID < 0) { UE_LOG(LogTemp, Warning, TEXT("[DumpBacklog] 관리 중 빌딩 없음")); return; }

	const FDateTime Now = FDateTime::UtcNow();
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SD) { UE_LOG(LogTemp, Warning, TEXT("[DumpBacklog] 세이브 없음")); return; }

	FOfficeSaveData* Office = SD->GameData.OfficeDataMap.Find(BID);
	if (!Office || Office->BacklogProducts.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DumpBacklog] BID=%d 백로그 비어있음"), BID);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DumpBacklog] BID=%d 백로그 %d개:"), BID, Office->BacklogProducts.Num());
	for (const FBacklogProductEntry& E : Office->BacklogProducts)
	{
		const double Elapsed = (Now - E.LaunchWallClock).GetTotalSeconds();
		const double Net = Mgr->ComputeProductNetPerSec(E, Now);
		UE_LOG(LogTemp, Warning, TEXT("  PID=%d Num=%d elapsed=%.0fs peak=%.2f cost=%.2f net=%.2f/s"),
			E.ProjectID, E.ProjectNumber, Elapsed, E.BasePeakRevenuePerSec, E.OperatingCostPerSec, Net);
	}
	UE_LOG(LogTemp, Warning, TEXT("[DumpBacklog] 빌딩 합산 순익: %.2f/s"), Mgr->ComputeBuildingNetPerSec(BID, Now));
}

// ========== 도시 회사 인수 (UCityAcquisitionManager) ==========

void UCGCheatManager::Acq_Buy(int32 Key)
{
	if (UWorld* W = GetWorld())
		if (UCityAcquisitionManager* M = W->GetSubsystem<UCityAcquisitionManager>())
		{ const bool b = M->Acquire(Key); UE_LOG(LogTemp, Warning, TEXT("[Acq] Buy %d -> %d"), Key, b ? 1 : 0); }
}

void UCGCheatManager::Acq_State(int32 Key)
{
	if (UWorld* W = GetWorld())
		if (UCityAcquisitionManager* M = W->GetSubsystem<UCityAcquisitionManager>())
		{ UE_LOG(LogTemp, Warning, TEXT("[Acq] State %d = %d"), Key, (int32)M->GetState(Key)); }
}

void UCGCheatManager::Acq_Demolish(int32 Key)
{
	if (UWorld* W = GetWorld())
		if (UCityAcquisitionManager* M = W->GetSubsystem<UCityAcquisitionManager>())
		{ M->Demolish(Key); UE_LOG(LogTemp, Warning, TEXT("[Acq] Demolish %d (state now %d)"), Key, (int32)M->GetState(Key)); }
}

void UCGCheatManager::Acq_FillPot(int32 Key, int32 Pct)
{
	if (UWorld* W = GetWorld())
		if (UCityAcquisitionManager* M = W->GetSubsystem<UCityAcquisitionManager>())
		{ M->DebugFillPot(Key, Pct); UE_LOG(LogTemp, Warning, TEXT("[Acq] FillPot %d -> pot=%lld"), Key, M->GetPotAmount(Key)); }
}

void UCGCheatManager::Acq_ClearAll()
{
	UWorld* W = GetWorld();
	UCityAcquisitionManager* M = W ? W->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!M)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Acq_ClearAll] UCityAcquisitionManager 없음 (MainMap 에서 실행)"));
		return;
	}

	const int32 Count = M->DebugClearAllCompanies();
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Acq_ClearAll] %d개 회사 Cleared 처리 (건물 숨김 + 부지 구매 가능) — 땅까지 접수하려면 Plot_OwnAll"), Count);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[Acq_ClearAll] %d개 회사 정리됨"), Count));
	}
}

// ========== 도시 부지 (USpawnManager / ACityPlotActor) ==========

void UCGCheatManager::Plot_OwnAll()
{
	UWorld* W = GetWorld();
	USpawnManager* SpawnMgr = W ? W->GetSubsystem<USpawnManager>() : nullptr;
	if (!SpawnMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Plot_OwnAll] USpawnManager 없음 (MainMap 에서 실행)"));
		return;
	}

	const int32 Count = SpawnMgr->DebugOwnAllPlots();

	// 소유 상태 영속화 + 가격 배지 재평가 (모두 소유 → 배지 사라짐). GatherOwnedPlotIds 가 IsOwned() 부지를 직렬화.
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
		if (UUIManagerSubsystem* UIM = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			if (UInGameLayerWidget* InGameLayer = UIM->GetInGameLayer())
			{
				InGameLayer->RefreshPlotPriceBadges();
			}
		}
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Plot_OwnAll] %d개 부지 소유 전환 + 저장"), Count);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[Plot_OwnAll] %d개 부지 인수됨"), Count));
	}
}

// ========== 벽돌공장 (ABrickFactory) ==========

void UCGCheatManager::FactoryMax(int32 AmountLevel, float SpeedSeconds)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UTableManagerSubsystem* TableMgr = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// DT(FFactoryUpgradeDefinition)에 MaxLevel 있으면 그 값(단일 진실 원천), 없으면 코드 폴백.
	auto MaxLevelFor = [&](EFactoryUpgradeType Type) -> int32
	{
		FFactoryUpgradeDefinition Def;
		if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(Type, Def) && Def.MaxLevel > 0)
		{
			return Def.MaxLevel;
		}
		return FFactoryUpgradeConfig::GetMaxLevel(Type);
	};

	const int32 SpeedMax = MaxLevelFor(EFactoryUpgradeType::HoldProductionSpeed);
	const int32 AmountMax = MaxLevelFor(EFactoryUpgradeType::HoldProductionAmount);
	const int32 TargetAmount = (AmountLevel > 0) ? FMath::Min(AmountLevel, AmountMax) : AmountMax;

	TArray<AActor*> Factories;
	UGameplayStatics::GetAllActorsOfClass(World, ABrickFactory::StaticClass(), Factories);

	int32 Count = 0;
	for (AActor* Actor : Factories)
	{
		ABrickFactory* Factory = Cast<ABrickFactory>(Actor);
		if (!Factory) continue;

		// GetFactoryData() 복사본의 레벨만 갈아끼우고 SetFactoryData() 로 되돌리면
		// 내부 UpdateProductionValues() 가 CurrentValue(실효 간격/개수)를 재계산한다.
		// 자동수집(AutoCollection/Capacity)은 의도적으로 안 건드림 — 0.001초 틱 알림이 트레일러 컷을 망침.
		FFactorySaveData Data = Factory->GetFactoryData();
		if (FFactoryUpgradeData* Speed = Data.UpgradeData.Find(EFactoryUpgradeType::HoldProductionSpeed))
		{
			Speed->Level = SpeedMax;
		}
		if (FFactoryUpgradeData* Amount = Data.UpgradeData.Find(EFactoryUpgradeType::HoldProductionAmount))
		{
			Amount->Level = TargetAmount;
		}
		Factory->SetFactoryData(Data);

		// SpeedSeconds>0 이면 곡선 대신 그 초를 버스트 간격으로 오버라이드 (라이브 튜닝). ≤0 이면 곡선값 사용.
		Factory->TrailerSpawnIntervalOverride = (SpeedSeconds > 0.0f) ? SpeedSeconds : -1.0f;
		++Count;

		const float EffInterval = (SpeedSeconds > 0.0f)
			? SpeedSeconds
			: Factory->GetUpgradeValue(EFactoryUpgradeType::HoldProductionSpeed);

		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:FactoryMax] %s -> 버스트 간격 %.3fs (%s) 개수 L%d(%.0f개/버스트)"),
			*Factory->GetName(), EffInterval,
			(SpeedSeconds > 0.0f) ? TEXT("오버라이드") : TEXT("곡선 만렙"),
			TargetAmount,
			Factory->GetUpgradeValue(EFactoryUpgradeType::HoldProductionAmount));
	}

	if (Count == 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:FactoryMax] 이 맵에 벽돌공장(ABrickFactory) 없음 — 공장이 배치된 맵에서 실행"));
	}
	if (GEngine)
	{
		const float ShownInterval = (SpeedSeconds > 0.0f)
			? SpeedSeconds
			: FFactoryUpgradeConfig::CalculateUpgradeValue(EFactoryUpgradeType::HoldProductionSpeed, SpeedMax);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[FactoryMax] 공장 %d개 (간격 %.3fs / 개수 L%d) — 공장 홀드하면 우수수"),
				Count, ShownInterval, TargetAmount));
	}
}

void UCGCheatManager::UnlockFactoryAuto()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> Factories;
	UGameplayStatics::GetAllActorsOfClass(World, ABrickFactory::StaticClass(), Factories);

	int32 UnlockedCount = 0;
	int32 AlreadyCount = 0;
	for (AActor* Actor : Factories)
	{
		ABrickFactory* Factory = Cast<ABrickFactory>(Actor);
		if (!Factory) continue;

		if (Factory->IsAutoCollectionUnlocked())
		{
			++AlreadyCount;
			continue;
		}

		Factory->UnlockAutoCollection();
		++UnlockedCount;

		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:UnlockFactoryAuto] %s -> 자동 수집 해금 (간격 %.3fs / 용량 %.0f)"),
			*Factory->GetName(),
			Factory->GetUpgradeValue(EFactoryUpgradeType::AutoCollection),
			Factory->GetUpgradeValue(EFactoryUpgradeType::AutoCollectionCapacity));
	}

	if (Factories.Num() == 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:UnlockFactoryAuto] 이 맵에 벽돌공장(ABrickFactory) 없음 — 공장이 배치된 맵에서 실행"));
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[UnlockFactoryAuto] 해금 %d개 (이미 해금 %d개) — 공장 패널에서 확인"),
				UnlockedCount, AlreadyCount));
	}
}

// ========== UI 디버그 (UIBase 스택 / 입력모드) ==========

void UCGCheatManager::CG_DumpUI()
{
	APlayerController* PC = GetOuterAPlayerController();
	UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	if (!UIBase)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[CG_DumpUI] UIBase 없음 — UI 레이어가 안 뜬 맵"));
		return;
	}

	UIBase->DebugPrintStackContents();

	// CommonUI ActionRouter 는 프로젝트 EInputMode 와 별개의 게이트 — Menu 잔류 시 게임 입력 전면 차단됨
	// (ECommonInputMode 의 StaticEnum 은 CommonInput 모듈 export 라 미링크 — 3값이라 수동 매핑)
	FString RouterInfo = TEXT("Router=?");
	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (UCommonUIActionRouterBase* Router = LP->GetSubsystem<UCommonUIActionRouterBase>())
		{
			const ECommonInputMode RouterMode = Router->GetActiveInputMode();
			const TCHAR* RouterModeName =
				RouterMode == ECommonInputMode::Menu ? TEXT("Menu") :
				RouterMode == ECommonInputMode::Game ? TEXT("Game") : TEXT("All");
			RouterInfo = FString::Printf(TEXT("Router=%s GameInput=%s"),
				RouterModeName, Router->CanProcessNormalGameInput() ? TEXT("OK") : TEXT("BLOCKED"));
		}
	}

	if (AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(PC))
	{
		const FString ModeName = StaticEnum<EInputMode>()->GetNameStringByValue((int64)MainPC->GetCurrentInputMode());
		UE_LOG(LogCGCheat, Warning, TEXT("[CG_DumpUI] InputMode=%s %s"), *ModeName, *RouterInfo);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan,
				FString::Printf(TEXT("[CG_DumpUI] InputMode=%s %s | Main=%d Prompt=%d Bottom=%d (상세는 Output Log)"),
					*ModeName, *RouterInfo, UIBase->GetMainStackCount(), UIBase->GetPromptStackCount(), UIBase->GetBottomStackCount()));
		}
	}
}

void UCGCheatManager::CG_UnlockInput()
{
	AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(GetOuterAPlayerController());
	if (!MainPC)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[CG_UnlockInput] MainMap 계열 PC 아님 — 복원 불가"));
		return;
	}

	MainPC->GoToNormalMode();

	// CommonUI 라우터의 잔류 입력컨피그(Menu=게임입력 차단)도 함께 리셋 — enum 복원만으론 안 풀리는 잠금 대비
	if (ULocalPlayer* LP = MainPC->GetLocalPlayer())
	{
		if (UCommonUIActionRouterBase* Router = LP->GetSubsystem<UCommonUIActionRouterBase>())
		{
			Router->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture));
		}
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[CG_UnlockInput] InputMode 강제 Normal + CommonUI 라우터 All 복원 완료"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("[CG_UnlockInput] 입력모드 Normal + 라우터 All 복원"));
	}
}

// 직원 이동 속도 / 로코모션 ─────────────────────────────────────────

namespace
{
	const TCHAR* BehaviorModeName(int32 Index)
	{
		switch (Index)
		{
		case 0: return TEXT("Idle");
		case 1: return TEXT("Stage");
		case 2: return TEXT("Operation");
		default: return TEXT("?");
		}
	}

	int32 GatherOfficeworkers(UWorld* World, TArray<AOfficeworker*>& Out)
	{
		Out.Reset();
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, AOfficeworker::StaticClass(), Found);
		for (AActor* A : Found)
		{
			if (AOfficeworker* Worker = Cast<AOfficeworker>(A))
			{
				Out.Add(Worker);
			}
		}
		return Out.Num();
	}
}

void UCGCheatManager::WorkerBolt(int32 Count)
{
	UWorld* W = GetWorld();
	if (!W) return;

	TArray<AOfficeworker*> Workers;
	const int32 Total = GatherOfficeworkers(W, Workers);
	const int32 Limit = (Count < 0) ? Total : FMath::Min(Count, Total);

	// 슬랙 루프는 Stage 전용 — 다른 모드에서 강제 진입시켜도 TickFatigueSlack 이 다음 프레임에
	// 페이즈를 지워서 아무 일도 안 일어난다. 조용히 실패하면 안 되니 명시적으로 알린다.
	int32 Applied = 0;
	int32 SkippedNotStage = 0;
	for (int32 i = 0; i < Limit; ++i)
	{
		UEmployeeBehaviorComponent* Comp = Workers[i]->BehaviorComponent;
		if (!Comp) continue;
		if (Comp->CurrentBehaviorMode != EEmployeeBehaviorMode::Stage)
		{
			++SkippedNotStage;
			continue;
		}
		Comp->ForceSlackBolt();
		++Applied;
	}

	if (Applied == 0)
	{
		UE_LOG(LogCGCheat, Warning,
			TEXT("[Cheat:WorkerBolt] 발동 0명 — 대상 %d 중 %d명이 Stage 모드가 아님. ")
			TEXT("번아웃 배회는 개발(Stage) 중에만 동작하므로 프로젝트를 진행시킨 뒤 다시 실행할 것."),
			Total, SkippedNotStage);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Yellow,
				TEXT("[WorkerBolt] 발동 0명 — Stage 모드 필요"));
		}
		return;
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:WorkerBolt] 번아웃 배회 발동 %d명 (대상 %d, Stage 아님 %d)"),
		Applied, Total, SkippedNotStage);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[WorkerBolt] 뛰쳐나감 %d명 (Stage 아님 %d)"), Applied, SkippedNotStage));
	}
}

void UCGCheatManager::BoltStatus(float StepSeconds)
{
	UWorld* W = GetWorld();
	if (!W) { return; }

	if (StepSeconds <= 0.f) { StepSeconds = 15.f; }

	TArray<AOfficeworker*> Workers;
	const int32 Total = GatherOfficeworkers(W, Workers);
	if (Total == 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:BoltStatus] 사무실에 직원 0명"));
		return;
	}

	UEmployeeManager* EmpMgr = W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;

	UE_LOG(LogCGCheat, Warning, TEXT("=== 폭주 확률 진단 (판 %.0f초 기준) ==="), StepSeconds);

	float SumExpected = 0.f;
	for (AOfficeworker* Worker : Workers)
	{
		UEmployeeBehaviorComponent* Comp = Worker ? Worker->BehaviorComponent : nullptr;
		if (!Comp) { continue; }

		// 실효 초당 확률은 컴포넌트가 계산한 값을 그대로 읽는다 — 치트가 공식을 재구현하면 본체와 어긋난다
		const float PerSec = Comp->GetEffectiveBoltChance();
		const float PerStep = 1.f - FMath::Pow(1.f - PerSec, StepSeconds);
		SumExpected += PerStep;

		int32 Composure = -1;
		if (EmpMgr)
		{
			if (FEmployeeInstance* Emp = EmpMgr->FindEmployee(Worker->GetEmployeeID()))
			{
				Composure = Emp->Stats.Composure + UEmployeeTypeHelper::GetEnhanceStatBonus(Emp->EnhancementLevel);
			}
		}

		UE_LOG(LogCGCheat, Warning,
			TEXT("  직원 %d: 유효침착성 %d · 초당 %.4f%% · 판당 %.1f%% · 모드 %d"),
			Worker->GetEmployeeID(), Composure, PerSec * 100.f, PerStep * 100.f,
			static_cast<int32>(Comp->CurrentBehaviorMode));
	}

	UE_LOG(LogCGCheat, Warning,
		TEXT("  => 판당 기대 폭주 인원: %.2f명 / %d명 (개발 모드 직원만 실제 굴림)"),
		SumExpected, Total);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan,
			FString::Printf(TEXT("[BoltStatus] 판 %.0fs · 기대 %.2f명 / %d명"), StepSeconds, SumExpected, Total));
	}
}

void UCGCheatManager::WorkerSpeed(float NewSpeed)
{
	UWorld* W = GetWorld();
	if (!W) return;

	TArray<AOfficeworker*> Workers;
	const int32 Total = GatherOfficeworkers(W, Workers);
	const bool bRestore = (NewSpeed < 0.f);

	int32 Applied = 0;
	for (AOfficeworker* Worker : Workers)
	{
		if (bRestore)
		{
			if (Worker->BehaviorComponent)
			{
				Worker->BehaviorComponent->ApplyMoveSpeed();
				++Applied;
			}
			continue;
		}

		if (UCharacterMovementComponent* Move = Worker->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = NewSpeed;
			++Applied;
		}
	}

	const FString Desc = bRestore
		? FString::Printf(TEXT("페이즈 기준값 복귀 (%d명)"), Applied)
		: FString::Printf(TEXT("MaxWalkSpeed=%.0f (%d명)"), NewSpeed, Applied);

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:WorkerSpeed] %s — 대상 %d"), *Desc, Total);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[WorkerSpeed] %s"), *Desc));
	}
}

void UCGCheatManager::WorkerSpeedStatus()
{
	UWorld* W = GetWorld();
	if (!W) return;

	TArray<AOfficeworker*> Workers;
	const int32 Total = GatherOfficeworkers(W, Workers);

	UE_LOG(LogCGCheat, Warning,
		TEXT("[Cheat:WorkerSpeedStatus] 직원 %d명 — Speed 는 실측 속도(ABP 블렌드스페이스 축 입력값)"), Total);

	for (AOfficeworker* Worker : Workers)
	{
		const UCharacterMovementComponent* Move = Worker->GetCharacterMovement();
		const UEmployeeBehaviorComponent* Comp = Worker->BehaviorComponent;
		const int32 ModeIdx = Comp ? static_cast<int32>(Comp->CurrentBehaviorMode) : -1;
		const float MaxWalk = Move ? Move->MaxWalkSpeed : -1.f;

		// 속도를 결정하는 건 모드가 아니라 슬랙 페이즈다. 페이즈는 private 이라
		// MaxWalkSpeed 가 Bolt 값인지로 역판정해 표시한다(왜 450 인지 바로 보이게).
		const TCHAR* Band = TEXT("?");
		if (Comp)
		{
			if (FMath::IsNearlyEqual(MaxWalk, Comp->MoveSpeedBolt)) { Band = TEXT("BOLT"); }
			else if (FMath::IsNearlyEqual(MaxWalk, Comp->MoveSpeedWalk)) { Band = TEXT("walk"); }
			else { Band = TEXT("override"); }
		}

		UE_LOG(LogCGCheat, Warning, TEXT("  [%d] Mode=%s  Band=%s  MaxWalkSpeed=%.0f  Speed=%.1f"),
			Worker->GetEmployeeID(), BehaviorModeName(ModeIdx), Band,
			MaxWalk, Worker->GetVelocity().Size2D());
	}
}

// ========== 프로젝트 진척 / 도감 ==========

namespace
{
	// 치트 4종 공용 컨텍스트. 오피스(WorldSubsystem) + 세이브가 둘 다 있어야 의미가 있다.
	struct FProjCheatCtx
	{
		UOfficeStageProgressManager* StageMgr = nullptr;
		UTableManagerSubsystem* TableMgr = nullptr;
		USaveGame_GameData* SaveData = nullptr;
		ECompanyType Industry = ECompanyType::Game;
		bool bOK = false;
	};

	FProjCheatCtx ResolveProjCheatCtx(UWorld* World, const TCHAR* Tag)
	{
		FProjCheatCtx Ctx;
		Ctx.StageMgr = World ? World->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
		if (!Ctx.StageMgr)
		{
			UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:%s] UOfficeStageProgressManager 없음 (OfficeMap 에서 실행)"), Tag);
			return Ctx;
		}
		UCGGameInstance* GI = UCGGameInstance::GetInstance();
		Ctx.TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr)
		{
			Ctx.SaveData = SaveMgr->GetCurrentSaveData();
		}
		if (!Ctx.TableMgr || !Ctx.SaveData)
		{
			UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:%s] TableManager/SaveData 없음"), Tag);
			return Ctx;
		}
		Ctx.Industry = GI->GetCurrentBuildingCompanyType();
		Ctx.bOK = true;
		return Ctx;
	}
}

void UCGCheatManager::Proj_Seed(int32 Count, int32 Review)
{
	FProjCheatCtx Ctx = ResolveProjCheatCtx(GetWorld(), TEXT("Proj_Seed"));
	if (!Ctx.bOK) { return; }

	FProjectTierProgress Progress = Ctx.StageMgr->GetTierProgress();
	int32 BandStart = 0, BandEnd = 0;
	FProjectTierProgress::GetTierProjectRange(Progress.CurrentTier, BandStart, BandEnd);

	int32 Added = 0;
	for (int32 Idx = 1; Idx <= BandEnd && Added < Count; ++Idx)
	{
		if (Progress.ClearedProjects.Contains(Idx)) { continue; }

		bool bRowOK = false;
		const FProjectData Row = Ctx.TableMgr->GetProjectData(Ctx.Industry, Idx, bRowOK);
		if (!bRowOK) { continue; }
		// 미저작 행은 패널이 걸러내므로 시드에서도 제외 — 안 그러면 안 보이는 이력만 쌓인다
		if (!Row.IsPitchReady()) { continue; }

		Progress.ClearedProjects.Add(Idx);

		const int32 NewReview = (Review > 0) ? FMath::Clamp(Review, 1, 40) : FMath::RandRange(12, 34);

		// 실제 출시 경로(ComputeReviewAndDiscovery)와 같은 upsert 규칙 —
		// 치트가 blind Add 를 하면 실제 플레이로는 생길 수 없는 '같은 인덱스 2행' 상태가 만들어진다.
		const ECompanyType SeedIndustry = Ctx.Industry;
		FShippedProjectRecord* Existing = Ctx.SaveData->GameData.ShippedProjects.FindByPredicate(
			[Idx, SeedIndustry](const FShippedProjectRecord& R)
			{
				return R.ProjectIndex == Idx && R.Industry == SeedIndustry;
			});

		if (Existing)
		{
			Existing->ReviewScore = FMath::Max(Existing->ReviewScore, NewReview);
		}
		else
		{
			FShippedProjectRecord Rec;
			Rec.ProjectName = Row.ProjectName.IsEmpty()
				? FString::Printf(TEXT("%s %s"), *Row.Material.ToString(), *Row.Genre.ToString())
				: Row.ProjectName.ToString();
			Rec.Industry = SeedIndustry;
			Rec.ProjectIndex = Idx;
			Rec.Genre = Row.Genre;
			Rec.Material = Row.Material;
			Rec.ReviewScore = NewReview;
			Rec.CumulativeRevenue = static_cast<int64>(Idx) * 10000;
			Ctx.SaveData->GameData.ShippedProjects.Add(Rec);
		}
		++Added;
	}

	Ctx.StageMgr->SetTierProgress(Progress);
	Ctx.StageMgr->SyncTierProgressToSave();   // 안 부르면 오피스 이탈 시 시드가 통째로 날아간다
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Proj_Seed] %d건 기록 (티어 %d, 총 개발이력 %d)"),
		Added, Progress.CurrentTier, Progress.ClearedProjects.Num());
}

void UCGCheatManager::Proj_SetTier(int32 Tier)
{
	FProjCheatCtx Ctx = ResolveProjCheatCtx(GetWorld(), TEXT("Proj_SetTier"));
	if (!Ctx.bOK) { return; }

	FProjectTierProgress Progress = Ctx.StageMgr->GetTierProgress();
	Progress.CurrentTier = FMath::Clamp(Tier, 1, TierConstants::MAX_TIER);
	Ctx.StageMgr->SetTierProgress(Progress);
	Ctx.StageMgr->SyncTierProgressToSave();
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Proj_SetTier] 현재 티어 = %d"), Progress.CurrentTier);
}

void UCGCheatManager::Proj_ClearHistory()
{
	FProjCheatCtx Ctx = ResolveProjCheatCtx(GetWorld(), TEXT("Proj_ClearHistory"));
	if (!Ctx.bOK) { return; }

	FProjectTierProgress Progress = Ctx.StageMgr->GetTierProgress();
	const int32 HadDev = Progress.ClearedProjects.Num();
	Progress.ClearedProjects.Empty();
	Progress.MilestoneRewardedTiers.Empty();
	Progress.bAllProjectsMilestoneRewarded = false;
	Ctx.StageMgr->SetTierProgress(Progress);
	Ctx.StageMgr->SyncTierProgressToSave();

	const int32 HadShip = Ctx.SaveData->GameData.ShippedProjects.Num();
	Ctx.SaveData->GameData.ShippedProjects.Empty();

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Proj_ClearHistory] 개발이력 %d건 / 출시기록 %d건 삭제"), HadDev, HadShip);
}

void UCGCheatManager::Proj_State()
{
	FProjCheatCtx Ctx = ResolveProjCheatCtx(GetWorld(), TEXT("Proj_State"));
	if (!Ctx.bOK) { return; }

	const FProjectTierProgress& Progress = Ctx.StageMgr->GetTierProgress();
	int32 Shipped = 0;
	for (const FShippedProjectRecord& Rec : Ctx.SaveData->GameData.ShippedProjects)
	{
		if (Rec.Industry == Ctx.Industry) { ++Shipped; }
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Proj_State] 산업=%s 티어=%d 개발이력=%d 출시기록=%d"),
		*UEnum::GetValueAsString(Ctx.Industry), Progress.CurrentTier,
		Progress.ClearedProjects.Num(), Shipped);

	FString Line;
	for (int32 Idx : Progress.ClearedProjects) { Line += FString::Printf(TEXT("%d "), Idx); }
	UE_LOG(LogCGCheat, Warning, TEXT("  개발한 인덱스: %s"), Line.IsEmpty() ? TEXT("(없음)") : *Line);
}

// ============================================================================
// 생산 주문서 / 공장 라인 치트
// ============================================================================

namespace CGProdCheat
{

	static FString EnumStr(const UEnum* E, int64 V)
	{
		return E ? E->GetNameStringByValue(V) : TEXT("?");
	}

	// 국가명 -> enum. 실패 시 로그 남기고 false.
	static bool ResolveCountry(const FString& Name, const TCHAR* Tag, ECountryType& Out)
	{
		const UEnum* E = StaticEnum<ECountryType>();
		const int64 V = E ? E->GetValueByNameString(Name) : INDEX_NONE;
		if (V == INDEX_NONE)
		{
			UE_LOG(LogCGCheat, Warning,
				TEXT("[Cheat:%s] Country not found: %s (예: Korea, China, Japan, Germany, USA)"), Tag, *Name);
			return false;
		}
		Out = static_cast<ECountryType>(V);
		return true;
	}

	// 자원을 정확히 그 수량으로. Set API 가 없어 델타로 맞춘다.
	static void SetExact(UResourceItemManager* ResMgr, EResourceType T, int64 Amount)
	{
		const int64 Have = ResMgr->GetResourceAmount(T);
		const int64 Delta = Amount - Have;
		if (Delta > 0)      ResMgr->StoreResource(T, Delta, /*bShouldSave=*/false);
		else if (Delta < 0) ResMgr->SpendResource(T, -Delta, /*bShouldSave=*/false);
	}

	// 주문 1건을 "제품 · 등급 · 남은수량 · 제작가능수(병목)" 한 줄로
	static FString DescribeOrder(UProductionOrderManager* OrderMgr, UTableManagerSubsystem* TableMgr,
		const FProductionOrder& Order)
	{
		FString ProductName = Order.ProductName;
		bool bProjOk = false;
		const FProjectData Proj = TableMgr->GetProjectData(Order.CompanyType, Order.ProjectIndex, bProjOk);
		if (bProjOk && !Proj.ProjectName.IsEmpty()) ProductName = Proj.ProjectName.ToString();

		EResourceType Bottleneck = EResourceType::None;
		bool bMaterialBound = false;
		const int32 MaxProducible = OrderMgr->GetMaxProducible(Order, Bottleneck, bMaterialBound);
		const UEnum* ResEnum = StaticEnum<EResourceType>();

		FString Cap;
		if (MaxProducible <= 0)
		{
			Cap = FString::Printf(TEXT("제작불가(%s 부족)"), *EnumStr(ResEnum, static_cast<int64>(Bottleneck)));
		}
		else if (bMaterialBound)
		{
			Cap = FString::Printf(TEXT("%d개까지(상한 %s)"), MaxProducible,
				*EnumStr(ResEnum, static_cast<int64>(Bottleneck)));
		}
		else
		{
			Cap = FString::Printf(TEXT("%d개(주문서 전량)"), MaxProducible);
		}

		return FString::Printf(TEXT("  #%-4d %-12s idx=%-3d %-16s 등급%s 남은%3d  -> %s"),
			Order.OrderID,
			*EnumStr(StaticEnum<ECompanyType>(), static_cast<int64>(Order.CompanyType)),
			Order.ProjectIndex, *ProductName,
			*EnumStr(StaticEnum<EQualityGrade>(), static_cast<int64>(Order.Grade)),
			Order.RemainingQuantity, *Cap);
	}
}

void UCGCheatManager::Prod_Country(const FString& CountryName)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
	if (!OrderMgr || !TableMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_Country] 매니저 없음"));
		return;
	}

	ECountryType Country;
	if (!CGProdCheat::ResolveCountry(CountryName, TEXT("Prod_Country"), Country)) return;

	bool bInfoOk = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bInfoOk);
	if (!bInfoOk)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_Country] DT_CountryInfo 행 없음: %s"), *CountryName);
		return;
	}

	FString Industries;
	for (ECompanyType C : Info.SupportedIndustries)
	{
		Industries += CGProdCheat::EnumStr(StaticEnum<ECompanyType>(), static_cast<int64>(C)) + TEXT(" ");
	}
	if (Industries.IsEmpty()) Industries = TEXT("(제한 없음 - 전 산업)");

	UE_LOG(LogCGCheat, Warning, TEXT("===== [%s] 제작 가능 목록 ====="), *CountryName);
	UE_LOG(LogCGCheat, Warning, TEXT("  지원 산업: %s"), *Industries);
	if (FactoryMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("  공장 라인: %d / %d"),
			FactoryMgr->GetActiveLineCount(Country), FactoryMgr->GetMaxLines(Country));
	}

	int32 Shown = 0;
	int32 Alive = 0;
	TMap<ECompanyType, int32> FilteredOut;   // 살아있는데 이 국가 산업과 안 맞은 것
	for (const FProductionOrder& Order : OrderMgr->GetAllOrders())
	{
		if (Order.IsConsumed()) continue;
		++Alive;
		if (Info.SupportedIndustries.Num() > 0 && !Info.SupportedIndustries.Contains(Order.CompanyType))
		{
			FilteredOut.FindOrAdd(Order.CompanyType)++;
			continue;
		}
		UE_LOG(LogCGCheat, Warning, TEXT("%s"), *CGProdCheat::DescribeOrder(OrderMgr, TableMgr, Order));
		++Shown;
	}

	// "0건" 은 두 가지 다른 상황이다 — 주문서가 없는 것과, 있는데 산업이 안 맞는 것.
	// 같은 문구로 말하면 진단이 막힌다.
	if (Shown == 0)
	{
		if (Alive == 0)
		{
			UE_LOG(LogCGCheat, Warning, TEXT("  주문서가 하나도 없습니다 - Prod_Seed 로 생성하세요"));
		}
		else
		{
			FString Breakdown;
			for (const TPair<ECompanyType, int32>& P : FilteredOut)
			{
				Breakdown += FString::Printf(TEXT("%s %d건  "),
					*CGProdCheat::EnumStr(StaticEnum<ECompanyType>(), static_cast<int64>(P.Key)), P.Value);
			}
			UE_LOG(LogCGCheat, Warning,
				TEXT("  주문서 %d건이 있지만 %s 의 지원 산업과 하나도 안 맞습니다."), Alive, *CountryName);
			UE_LOG(LogCGCheat, Warning, TEXT("  걸러진 산업: %s"), *Breakdown);
			UE_LOG(LogCGCheat, Warning,
				TEXT("  → 전 산업을 받는 China / USA 로 보거나, 해당 산업 국가로 확인하세요 (Japan=Electronics, Germany=Automobile)."));
		}
	}
	UE_LOG(LogCGCheat, Warning, TEXT("===== 표시 %d건 / 전체 %d건 ====="), Shown, Alive);
}

void UCGCheatManager::Prod_Orders()
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!OrderMgr || !TableMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_Orders] 매니저 없음"));
		return;
	}

	UE_LOG(LogCGCheat, Warning, TEXT("===== 생산 주문서 전량 ====="));
	int32 Alive = 0;
	for (const FProductionOrder& Order : OrderMgr->GetAllOrders())
	{
		if (Order.IsConsumed()) continue;
		UE_LOG(LogCGCheat, Warning, TEXT("%s"), *CGProdCheat::DescribeOrder(OrderMgr, TableMgr, Order));
		++Alive;
	}
	UE_LOG(LogCGCheat, Warning, TEXT("===== %d건 (총 잔여 %d개) ====="),
		Alive, OrderMgr->GetTotalRemainingQuantity());
}

void UCGCheatManager::Prod_Seed(int32 Count)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	if (!OrderMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_Seed] UProductionOrderManager 없음"));
		return;
	}

	const int32 Made = OrderMgr->SpawnTestOrdersFromTable(Count);
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_Seed] 주문서 %d건 생성"), Made);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[Prod_Seed] %d orders"), Made));
	}
}

void UCGCheatManager::Prod_MatFill(int64 Amount)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UResourceItemManager* ResMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!ResMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_MatFill] UResourceItemManager 없음"));
		return;
	}

	for (EResourceType T : FProductRecipeTable::GetAllRawMaterialTypes())
	{
		CGProdCheat::SetExact(ResMgr, T, Amount);
	}
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_MatFill] 원자재 10종 = %lld"), Amount);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[Prod_MatFill] all raw = %lld"), Amount));
	}
}

void UCGCheatManager::Prod_MatSet(const FString& ResourceName, int64 Amount)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UResourceItemManager* ResMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!ResMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_MatSet] UResourceItemManager 없음"));
		return;
	}

	const UEnum* ResEnum = StaticEnum<EResourceType>();
	const int64 V = ResEnum ? ResEnum->GetValueByNameString(ResourceName) : INDEX_NONE;
	if (V == INDEX_NONE)
	{
		UE_LOG(LogCGCheat, Warning,
			TEXT("[Cheat:Prod_MatSet] Resource not found: %s (IronOre/Copper/Silicon/Lithium/Oil/RareEarth/Aluminum/Wood/Gold/DiamondOre)"),
			*ResourceName);
		return;
	}

	const EResourceType T = static_cast<EResourceType>(V);
	const int64 Before = ResMgr->GetResourceAmount(T);
	CGProdCheat::SetExact(ResMgr, T, Amount);
	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Prod_MatSet] %s: %lld -> %lld"), *ResourceName, Before, Amount);
}

void UCGCheatManager::Fac_Lines(const FString& CountryName)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
	if (!FactoryMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Fac_Lines] UWorldFactoryManager 없음"));
		return;
	}

	ECountryType Country;
	if (!CGProdCheat::ResolveCountry(CountryName, TEXT("Fac_Lines"), Country)) return;

	const int32 Active = FactoryMgr->GetActiveLineCount(Country);
	const int32 Max = FactoryMgr->GetMaxLines(Country);
	UE_LOG(LogCGCheat, Warning, TEXT("===== [%s] 공장 라인 %d / %d ====="), *CountryName, Active, Max);
	UE_LOG(LogCGCheat, Warning, TEXT("  속도배율=%.2f 보상배율=%.2f"),
		FactoryMgr->GetSpeedMultiplier(Country), FactoryMgr->GetRewardMultiplier(Country));

	for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(Country))
	{
		UE_LOG(LogCGCheat, Warning, TEXT("  Line#%-3d %-16s %lld / %lld  rate=%.3f/s %s"),
			Line.LineId, *Line.ProductName.ToString(), Line.CurrentQty, Line.TargetQty,
			Line.RatePerSecond, Line.bCompleted ? TEXT("[완료]") : TEXT(""));
	}
}

void UCGCheatManager::Fac_FillLines(const FString& CountryName)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
	if (!FactoryMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Fac_FillLines] UWorldFactoryManager 없음"));
		return;
	}

	ECountryType Country;
	if (!CGProdCheat::ResolveCountry(CountryName, TEXT("Fac_FillLines"), Country)) return;

	const int32 Max = FactoryMgr->GetMaxLines(Country);
	int32 Started = 0;
	const TSoftObjectPtr<UTexture2D> NoIcon;   // nullptr 직접 전달은 TSoftObjectPtr 생성자 모호
	// 재료 차감 없는 더미 라인 - "라인 가득" UI 상태만 만들기 위한 것
	while (FactoryMgr->GetActiveLineCount(Country) < Max)
	{
		const int32 Id = FactoryMgr->StartProduction(
			Country, /*ProductIndex=*/1,
			FText::FromString(TEXT("치트 더미")), NoIcon,
			/*BaseRatePerSecond=*/0.01f, /*TargetQty=*/9999);
		if (Id <= 0) break;
		++Started;
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Fac_FillLines] %s: 더미 %d개 시작 -> %d / %d  (되돌리기 = Fac_ClearLines %s)"),
		*CountryName, Started, FactoryMgr->GetActiveLineCount(Country), Max, *CountryName);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("[Fac_FillLines] %s %d/%d"), *CountryName,
				FactoryMgr->GetActiveLineCount(Country), Max));
	}
}

void UCGCheatManager::Fac_ClearLines(const FString& CountryName)
{
	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
	if (!FactoryMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Fac_ClearLines] UWorldFactoryManager 없음"));
		return;
	}

	ECountryType Country;
	if (!CGProdCheat::ResolveCountry(CountryName, TEXT("Fac_ClearLines"), Country)) return;

	// Claim 이 원본 배열에서 라인을 지우므로 ID 로 먼저 떠 둔다 (인덱스는 매 Claim 마다 밀린다)
	TArray<int32> LineIds;
	for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(Country))
	{
		LineIds.Add(Line.LineId);
	}

	int32 Cleared = 0;
	for (int32 Id : LineIds)
	{
		FactoryMgr->InstantFinishLine(Country, Id);   // 더미는 완주에 수일 — 시간을 앞으로 당긴다
		if (FactoryMgr->ClaimLine(Country, Id)) ++Cleared;
	}

	UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Fac_ClearLines] %s: %d개 비움 -> %d / %d"),
		*CountryName, Cleared, FactoryMgr->GetActiveLineCount(Country), FactoryMgr->GetMaxLines(Country));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[Fac_ClearLines] %s cleared %d"), *CountryName, Cleared));
	}
}

// ========== 진행 상태 프리셋 (UDevPresetSeeder) ==========

void UCGCheatManager::Preset_Apply(const FString& PresetRowName)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Preset_Apply] GameInstance not found"));
		return;
	}

	UDevPresetSeeder* Seeder = GI->GetSubsystem<UDevPresetSeeder>();
	if (!Seeder)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Preset_Apply] DevPresetSeeder not found"));
		return;
	}

	FName Row(*PresetRowName);
	if (PresetRowName.IsEmpty())
	{
		const UCGDevSettings* Dev = GetDefault<UCGDevSettings>();
		Row = Dev ? Dev->ProgressPresetRow : FName(TEXT("Mid"));
	}

	Seeder->ReapplyPreset(Row);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("[Preset_Apply] %s"), *Row.ToString()));
	}
}

void UCGCheatManager::Preset_Verify(const FString& PresetRowName)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Preset_Verify] GameInstance not found"));
		return;
	}

	UDevPresetSeeder* Seeder = GI->GetSubsystem<UDevPresetSeeder>();
	if (!Seeder)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Preset_Verify] DevPresetSeeder not found"));
		return;
	}

	FName Row(*PresetRowName);
	if (PresetRowName.IsEmpty())
	{
		const UCGDevSettings* Dev = GetDefault<UCGDevSettings>();
		Row = Dev ? Dev->ProgressPresetRow : FName(TEXT("Mid"));
	}

	const int32 Failures = Seeder->VerifyPreset(Row);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f,
			Failures == 0 ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("[Preset_Verify] %s : 실패 %d건 (자세한 건 로그)"), *Row.ToString(), Failures));
	}
}

// ========== 밸런스 검증 계측 ([BALV] 한 줄 로그) ==========

namespace
{
	// 하네스 파서 계약: `[BALV] <Name> k=v k=v ...` 한 줄. 값에 공백/등호가 섞이면 키가 잘못 잘리므로
	// 숫자(또는 got/target 슬래시쌍)만 넣는다. 프로젝트명 같은 자유 문자열은 절대 싣지 않는다.
	void EmitBalv(const FString& Payload)
	{
		UE_LOG(LogTemp, Display, TEXT("[BALV] %s"), *Payload);
	}
}

void UCGCheatManager::Balance_DumpDevScore()
{
	UWorld* CheatWorld = GetWorld();
	UOfficeStageProgressManager* StageMgr = CheatWorld ? CheatWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_DumpDevScore] OfficeMap 아님 — StageProgressManager 없음"));
		return;
	}

	const FStageProgressData& Data = StageMgr->GetProgressData();
	const float TotalDuration = StageMgr->GetTotalDuration();

	// 기여 인원 = 실제로 스폰된 오피스 직원 수 (A1 의 per-emp 분모)
	TArray<AOfficeworker*> Workers;
	GatherOfficeworkers(CheatWorld, Workers);

	FString Line = FString::Printf(TEXT("DevScore proj=%d tier=%d steps=%d emp=%d"),
		Data.ProjectID, StageMgr->GetTierProgress().CurrentTier, Data.Steps.Num(), Workers.Num());

	// 출시 스텝(DisciplineSlot=NONE)은 점수축이 없어 제외 — 인덱스는 활성 직능 스텝 기준으로 다시 매긴다
	int32 ActiveIndex = 0;
	for (const FStepRoundData& Step : Data.Steps)
	{
		if (Step.DisciplineSlot == INDEX_NONE)
		{
			continue;
		}
		Line += FString::Printf(TEXT(" s%d=%.4f/%.4f w%d=%d d%d=%d"),
			ActiveIndex, Step.AcquiredScore, Step.TargetScore,
			ActiveIndex, Step.Weight,
			ActiveIndex, Step.DisciplineSlot);
		++ActiveIndex;
	}

	Line += FString::Printf(TEXT(" q=%.4f grade=%s pass=%d running=%d remain=%.2f total=%.2f elapsed=%.2f retry=%d"),
		Data.CalculateQualityScore(),
		*QualityGradeToAlphabetString(Data.CalculateQualityGrade()),
		Data.MeetsMinimumClearScore() ? 1 : 0,
		Data.bIsTimerRunning ? 1 : 0,
		Data.RemainingTime,
		TotalDuration,
		FMath::Max(0.0f, TotalDuration - Data.RemainingTime),
		Data.RetryCount);

	EmitBalv(Line);
}

void UCGCheatManager::Balance_DumpEconomy()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_DumpEconomy] GameInstance not found"));
		return;
	}

	UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!ResourceMgr || !OpMgr || !SaveLoadMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_DumpEconomy] Resource/Operation/SaveLoad 매니저 없음"));
		return;
	}

	FString Line = TEXT("Economy");

	// enum 순회 — TMap(ResourceBank) 직접 순회는 키 순서가 비보장이라 세션 간 diff 가 흔들린다
	for (int32 Raw = static_cast<int32>(EResourceType::None) + 1; Raw < static_cast<int32>(EResourceType::Count); ++Raw)
	{
		const EResourceType ResType = static_cast<EResourceType>(Raw);
		Line += FString::Printf(TEXT(" %s=%lld"),
			*EnumToString(ResType), static_cast<long long>(ResourceMgr->GetResourceAmount(ResType)));
	}

	// 빌딩 목록은 세이브가 정본 — 맵(MainMap/OfficeMap)에 따라 스폰 액터가 달라도 같은 집합을 낸다
	TArray<int32> BuildingIndices;
	if (USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData())
	{
		SaveData->GameData.OfficeDataMap.GetKeys(BuildingIndices);
	}
	BuildingIndices.Sort();

	Line += FString::Printf(TEXT(" ops=%d blds=%d"), OpMgr->GetActiveOperationCount(), BuildingIndices.Num());

	for (const int32 BuildingIndex : BuildingIndices)
	{
		const FOperationData* Op = OpMgr->GetOperationByBuildingID(BuildingIndex);
		// _earned = 운영 누적 매출(직원 드립 포함, 단조증가) — 금고 적립분과 분리해 봐야 A3/A4 귀속이 안 섞인다
		Line += FString::Printf(TEXT(" b%d_op=%d b%d_rate=%.4f b%d_stable=%.4f b%d_stored=%.2f b%d_cap=%.2f b%d_remain=%.1f b%d_earned=%.2f"),
			BuildingIndex, Op ? 1 : 0,
			BuildingIndex, Op ? Op->ActualRevenuePerSecond : 0.0f,
			BuildingIndex, OpMgr->GetVaultReferenceRatePerSecond(BuildingIndex),
			BuildingIndex, OpMgr->GetStoredRevenue(BuildingIndex),
			BuildingIndex, OpMgr->CalculateWarehouseCapacity(BuildingIndex),
			BuildingIndex, Op ? Op->RemainingTime : 0.0f,
			BuildingIndex, Op ? Op->TotalRevenueEarned : 0.0f);
	}

	// 방치 인원 2종: empUnassigned=배치 안 된 직원(세이브 기준), empIdle=오피스에서 Idle 모드로 도는 직원
	TArray<AOfficeworker*> Workers;
	int32 IdleWorkers = 0;
	if (UWorld* CheatWorld = GetWorld())
	{
		GatherOfficeworkers(CheatWorld, Workers);
		for (const AOfficeworker* Worker : Workers)
		{
			const UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
			if (Behavior && Behavior->GetBehaviorMode() == EEmployeeBehaviorMode::Idle)
			{
				++IdleWorkers;
			}
		}
	}

	Line += FString::Printf(TEXT(" emp=%d empUnassigned=%d empOffice=%d empIdle=%d"),
		EmployeeMgr ? EmployeeMgr->GetEmployeeCount() : 0,
		EmployeeMgr ? EmployeeMgr->GetUnassignedEmployees().Num() : 0,
		Workers.Num(),
		IdleWorkers);

	EmitBalv(Line);
}

void UCGCheatManager::Balance_SimOffline(int32 Seconds)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_SimOffline] GameInstance not found"));
		return;
	}

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_SimOffline] SaveLoadManager not found"));
		return;
	}

	const float SimSeconds = static_cast<float>(FMath::Max(0, Seconds));

	FOfflineGainsResult SimResult;
	if (!SaveLoadMgr->ComputeOfflineGains(SimSeconds, /*bApplyToSave=*/false, SimResult))
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_SimOffline] 세이브 데이터 없음 — 계산 불가"));
		return;
	}

	// 실지급 경로는 60초 미만을 통째로 스킵한다. dry-run 은 계산을 그대로 내므로 그 사실을 함께 실어야 오독이 없다.
	const bool bWouldSkip = (SimSeconds < 60.0f);

	// 정산은 세이브의 CurrentOperation 스냅샷을 읽는데, 그건 SaveGameData 때만 갱신된다.
	// liveOps>0 인데 blds=0 이면 "세이브가 스테일" 이라는 뜻 — 조용한 0 측정을 이 키 하나로 구분한다.
	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();

	FString Line = FString::Printf(
		TEXT("SimOffline sec=%.0f gain=%.4f clipped=%.4f elapsedAdvance=%.4f blds=%d rows=%d skipped=%d liveOps=%d"),
		SimSeconds, SimResult.TotalGained, SimResult.TotalLostToVaultCap, SimResult.ElapsedAdvance,
		SimResult.EligibleBuildings, SimResult.Entries.Num(), bWouldSkip ? 1 : 0,
		OpMgr ? OpMgr->GetActiveOperationCount() : -1);

	for (const FOfflineGainEntry& GainEntry : SimResult.Entries)
	{
		Line += FString::Printf(TEXT(" b%d_raw=%.4f b%d_gain=%.4f b%d_loss=%.4f b%d_life=%d b%d_vaultLv=%d"),
			GainEntry.BuildingIndex, GainEntry.RawGain,
			GainEntry.BuildingIndex, GainEntry.ActualGain,
			GainEntry.BuildingIndex, GainEntry.LossByVault,
			GainEntry.BuildingIndex, GainEntry.bLifespanBound ? 1 : 0,
			GainEntry.BuildingIndex, GainEntry.VaultLevel);
	}

	EmitBalv(Line);
}

// ========== Balance_GateRun — A2 게이트 통과율 N판 자동 반복 ==========

namespace
{
	// 폴링 주기. 월드 타임 기준이라 slomo 가속 시 실시간 간격이 같이 줄어든다(= 가속해도 판정 지연이 안 늘어남).
	constexpr float BalvGatePollInterval = 0.1f;
}

void UCGCheatManager::Balance_GateRun(int32 Trials, int32 ProjectIndex, int32 DirectionValue)
{
	UWorld* CheatWorld = GetWorld();
	UOfficeStageProgressManager* StageMgr = CheatWorld ? CheatWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_GateRun] OfficeMap 아님 — StageProgressManager 없음"));
		return;
	}
	if (Trials <= 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_GateRun] Trials 는 1 이상이어야 함 (입력 %d)"), Trials);
		return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	UOfficeManager* OfficeMgr = CheatWorld->GetSubsystem<UOfficeManager>();
	if (!GI || !EmpMgr || !OfficeMgr)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_GateRun] 필수 매니저 없음 (GI=%d Employee=%d Office=%d)"),
			GI ? 1 : 0, EmpMgr ? 1 : 0, OfficeMgr ? 1 : 0);
		return;
	}

	// 이미 돌고 있으면 갈아탄다 — 폴링 타이머가 두 개 붙으면 판정이 두 번 나간다.
	if (BalvGateTrialsTotal > 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_GateRun] 진행 중이던 실행을 중단하고 새로 시작"));
		BalvGateFinish(TEXT("새 실행으로 교체"));
	}

	// 착석이 A2 의 전제다. 배정된 책상이 없는 직원은 Stage 전환에서 텔레포트에 실패해 Idle 로 되돌아가고,
	// 기여 루프가 통째로 안 돌아 점수가 "조용한 0" 으로 나온다(= 측정이 아니라 거짓값).
	const int32 BuildingIdx = GI->GetCurrentManagedBuildingIndex();
	const TArray<FEmployeeInstance> Roster = EmpMgr->GetEmployeesInBuilding(BuildingIdx);
	int32 NewlySeated = 0;
	int32 AlreadySeated = 0;
	for (const FEmployeeInstance& Member : Roster)
	{
		// ⚠ bIsAssigned 는 "이 빌딩에 배치됨" 플래그지 "책상에 앉음" 이 아니다.
		// 그걸로 판정하면 전원이 '기존 착석' 으로 스킵돼 AutoSeat 가 한 번도 안 돌고, 좌석 없는 채로
		// 30판이 조용히 0점을 낸다(2026-08-02 실측 사고). 좌석 유무는 좌석 역조회로만 판정한다.
		if (OfficeMgr->FindWorkstationByEmployeeID(Member.EmployeeID) != nullptr)
		{
			++AlreadySeated;
			continue;
		}
		if (OfficeMgr->AutoSeatEmployee(Member.EmployeeID, nullptr) != nullptr)
		{
			++NewlySeated;
		}
	}

	BalvGateEmpCount = NewlySeated + AlreadySeated;
	if (BalvGateEmpCount <= 0)
	{
		UE_LOG(LogCGCheat, Warning,
			TEXT("[Cheat:Balance_GateRun] 착석 직원 0명 (빌딩 %d, 로스터 %d명, 배치된 책상 %d개) — 좌석이 없으면 기여가 0 이라 측정 불가"),
			BuildingIdx, Roster.Num(), OfficeMgr->GetPlacedWorkstations().Num());
		return;
	}
	// 전원 착석이 안 되면 그 자체가 측정 신뢰도 문제다 — 조용히 진행하지 않고 크게 남긴다.
	if (BalvGateEmpCount < Roster.Num())
	{
		UE_LOG(LogCGCheat, Warning,
			TEXT("[Cheat:Balance_GateRun] ⚠ 좌석 부족 — 로스터 %d명 중 %d명만 착석(책상 %d개). 미착석 인원은 기여하지 않는다."),
			Roster.Num(), BalvGateEmpCount, OfficeMgr->GetPlacedWorkstations().Num());
	}

	BalvGateTrialsTotal = Trials;
	BalvGateTrialIndex = 0;
	BalvGateProjectIndex = ProjectIndex;
	// EProjectDirection 는 uint8 4값 — 범위 밖 값을 그대로 캐스팅하면 튜닝 조회가 쓰레기를 문다.
	BalvGateDirectionValue = FMath::Clamp(DirectionValue, 0, 3);
	bBalvGateNeedStart = false;

	UE_LOG(LogCGCheat, Warning,
		TEXT("[Cheat:Balance_GateRun] 시작 — %d판 / proj=%d dir=%d / 착석 %d명 (신규 %d, 기존 %d)"),
		Trials, ProjectIndex, DirectionValue, BalvGateEmpCount, NewlySeated, AlreadySeated);

	CheatWorld->GetTimerManager().SetTimer(
		BalvGatePollTimerHandle, this, &UCGCheatManager::BalvGatePoll, BalvGatePollInterval, true);

	BalvGateStartTrial();
}

void UCGCheatManager::Balance_GateRunStop()
{
	if (BalvGateTrialsTotal <= 0)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_GateRunStop] 진행 중인 실행 없음"));
		return;
	}
	BalvGateFinish(TEXT("사용자 중단"));
}

void UCGCheatManager::BalvGateStartTrial()
{
	UWorld* CheatWorld = GetWorld();
	UOfficeStageProgressManager* StageMgr = CheatWorld ? CheatWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr)
	{
		BalvGateFinish(TEXT("StageProgressManager 소실"));
		return;
	}

	BalvGateTrialElapsed = 0.0f;
	bBalvGateTimerSeen = false;
	BalvGateTypingPeak = 0;
	BalvGateDeskPeak = 0;

	// LaunchDevelopFromRow 가 SelectProject → Stage 모드 통지 → 타이머 점화까지 한 번에 태운다.
	const EDevelopStartResult StartResult = StageMgr->StartSelfDevelopFromProject(
		BalvGateProjectIndex, static_cast<EProjectDirection>(BalvGateDirectionValue), FText::GetEmpty());

	// 실패를 여기서 잡지 않으면 폴링이 무한히 돈다.
	if (StartResult != EDevelopStartResult::Success)
	{
		switch (StartResult)
		{
		case EDevelopStartResult::NoIndustry:
			BalvGateFinish(TEXT("착수 실패 — 산업 미확인 (Game 산업 빌딩에서 진입했는지 확인)"));
			break;
		default:
		{
			const FString Msg = FString::Printf(TEXT("착수 실패 — 프로젝트 행 없음 (idx=%d)"), BalvGateProjectIndex);
			BalvGateFinish(*Msg);
			break;
		}
		}
	}
}

void UCGCheatManager::BalvGatePoll()
{
	UWorld* CheatWorld = GetWorld();
	UOfficeStageProgressManager* StageMgr = CheatWorld ? CheatWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr)
	{
		BalvGateFinish(TEXT("월드/StageProgressManager 소실"));
		return;
	}

	// 다음 판은 종료와 같은 프레임이 아니라 한 틱 뒤에 건다 — EndCurrentStage 가 직원을 Idle(배회)로 돌리고
	// 바로 Stage 로 다시 부르면 착석 전이가 같은 프레임에 겹쳐 상태가 꼬일 수 있다.
	if (bBalvGateNeedStart)
	{
		bBalvGateNeedStart = false;
		BalvGateStartTrial();
		return;
	}

	// 모달 대기 자동 해소. 이게 없으면 개발 ~40% 지점의 부스트 도박에서 타이머가 영구 정지한다
	// (SimultaneousTimerTick 이 bBoostGamblePending / IsEventPaused 에서 카운트다운을 건너뜀).
	// 도박은 항상 안전(false), 이벤트는 항상 첫 선택지 — 무탭 기준선을 흔들지 않기 위한 고정 정책.
	if (StageMgr->IsBoostGamblePending())
	{
		StageMgr->ResolveBoostGamble(false);
	}
	if (StageMgr->IsEventPaused())
	{
		StageMgr->HandleEventChoice(0);
	}

	const FStageProgressData& Data = StageMgr->GetProgressData();
	if (Data.bIsTimerRunning)
	{
		bBalvGateTimerSeen = true;
	}

	// 이 판에서 실제 기여 조건(책상 배정 + Typing)을 만족한 인원의 최대치를 추적한다.
	// EmployeeBehaviorComponent::SpawnScoreFloatingText 의 게이트와 같은 조건이라, 이 값이 0 이면 점수가 0인 게 당연하다.
	{
		TArray<AOfficeworker*> PollWorkers;
		GatherOfficeworkers(CheatWorld, PollWorkers);
		int32 DeskNow = 0;
		int32 TypingNow = 0;
		for (const AOfficeworker* Worker : PollWorkers)
		{
			if (!Worker || Worker->bIsPortraitMode || !Worker->BehaviorComponent)
			{
				continue;
			}
			if (Worker->GetAssignedWorkstation() == nullptr)
			{
				continue;
			}
			++DeskNow;
			if (Worker->BehaviorComponent->GetBehaviorMode() == EEmployeeBehaviorMode::Stage
				&& Worker->BehaviorComponent->GetState() == EEmployeeState::Typing)
			{
				++TypingNow;
			}
		}
		BalvGateDeskPeak = FMath::Max(BalvGateDeskPeak, DeskNow);
		BalvGateTypingPeak = FMath::Max(BalvGateTypingPeak, TypingNow);
	}

	BalvGateTrialElapsed += BalvGatePollInterval;
	// 예산은 넉넉히 — 그래도 상한을 두는 이유는 모달 해소가 실패해도 30판이 영원히 안 끝나면 안 되기 때문.
	const float TrialBudget = FMath::Max(60.0f, StageMgr->GetTotalDuration() * 3.0f + 30.0f);
	const bool bTimedOut = (BalvGateTrialElapsed > TrialBudget);

	if (!bTimedOut && !(bBalvGateTimerSeen && !Data.bIsTimerRunning))
	{
		return;
	}

	// 화면을 보는 사람이 "책상에 앉지도 않았는데 측정한다" 고 지적한 사고(2026-08-02)를 데이터로 못 숨기게,
	// 이 판에서 **실제로 책상 앞에서 타이핑한 인원 최대치**를 같이 싣는다. typing 이 0/작으면 그 판은 무효 표본이다.
	EmitBalv(FString::Printf(TEXT("GateRun idx=%d pass=%d q=%.4f grade=%s emp=%d typing=%d desk=%d timeout=%d"),
		BalvGateTrialIndex,
		Data.MeetsMinimumClearScore() ? 1 : 0,
		Data.CalculateQualityScore(),
		*QualityGradeToAlphabetString(Data.CalculateQualityGrade()),
		BalvGateEmpCount,
		BalvGateTypingPeak,
		BalvGateDeskPeak,
		bTimedOut ? 1 : 0));

	StageMgr->EndCurrentStage();
	++BalvGateTrialIndex;

	if (BalvGateTrialIndex >= BalvGateTrialsTotal)
	{
		BalvGateFinish(nullptr);
		return;
	}
	bBalvGateNeedStart = true;
}

void UCGCheatManager::BalvGateFinish(const TCHAR* AbortReason)
{
	if (UWorld* CheatWorld = GetWorld())
	{
		CheatWorld->GetTimerManager().ClearTimer(BalvGatePollTimerHandle);
	}

	if (AbortReason)
	{
		UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:Balance_GateRun] 중단 (%d/%d판): %s"),
			BalvGateTrialIndex, BalvGateTrialsTotal, AbortReason);
	}

	// 완주든 중단이든 총계를 낸다 — 하네스가 통과율 분모로 쓸 "실제 완주 판수" 를 알아야 한다.
	// (req 와 n 이 다르면 부분 실행 — 파서가 그걸 보고 판단한다.)
	EmitBalv(FString::Printf(TEXT("GateRunDone n=%d req=%d emp=%d proj=%d dir=%d"),
		BalvGateTrialIndex, BalvGateTrialsTotal, BalvGateEmpCount, BalvGateProjectIndex, BalvGateDirectionValue));

	BalvGateTrialsTotal = 0;
	BalvGateTrialIndex = 0;
	BalvGateTrialElapsed = 0.0f;
	bBalvGateTimerSeen = false;
	bBalvGateNeedStart = false;
}
