#include "Manager/PanelIntroSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Office/OfficeRecruitmentFlowSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Data/ShippedRecord.h"
#include "Table/ProjectDataTable.h"
#include "Table/PanelIntroTable.h"
#include "Data/GameSaveData.h"
#include "Data/GrowthGoalRules.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "UI/UIBase.h"
#include "Enum/WidgetType.h"
#include "Engine/World.h"

void UPanelIntroSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 구독 대상은 InitializeDependency 로 강제 — 없으면 init 순서상 null 구독 스킵 (GDD_TUTORIAL §4 함정)
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();

	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UPanelIntroSubsystem::HandleGameDataLoaded);
	}
}

void UPanelIntroSubsystem::Deinitialize()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UPanelIntroSubsystem::HandleGameDataLoaded()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData)
	{
		return;
	}
	SeenPanelKeys = SaveData->GameData.SeenPanelIntros;
	HintCounts = SaveData->GameData.GestureHintCounts;
}

void UPanelIntroSubsystem::CollectSaveData(FGameSaveData& OutData) const
{
	OutData.SeenPanelIntros = SeenPanelKeys;
	OutData.GestureHintCounts = HintCounts;
}

bool UPanelIntroSubsystem::ShouldPlay(FName PanelKey) const
{
	if (PanelKey.IsNone() || SeenPanelKeys.Contains(PanelKey))
	{
		return false;
	}
	// 자동 안내는 후속 기획 채용과 제조업 최초 사용 맥락에만 허용한다.
	static const TSet<FName> ManufacturingKeys = {
		FName(TEXT("ManufacturingUnlock")), FName(TEXT("ManufacturingOffice")),
		FName(TEXT("ManufacturingResearch")), FName(TEXT("ManufacturingProduction"))
	};
	static const TSet<FName> AutoPlayKeys = {
		FName(TEXT("RecruitPlanningNextProject")),
		FName(TEXT("RecruitPlanningPrepareSeat"))
	};
	if (!AutoPlayKeys.Contains(PanelKey) && !ManufacturingKeys.Contains(PanelKey))
	{
		return false;
	}
	if (ManufacturingKeys.Contains(PanelKey))
	{
		const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
		const UMissionManagerSubsystem* Missions = GI ? GI->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
		USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
		if (!GI || GI->IsVisitMode() || !Missions || !Missions->IsTutorialCompleted()
			|| Missions->HasActiveMission() || !SaveMgr)
		{
			return false;
		}
		bool bManufacturingUnlocked = false;
		for (uint8 Value = 0; Value < static_cast<uint8>(ECompanyType::Max); ++Value)
		{
			const ECompanyType Company = static_cast<ECompanyType>(Value);
			bManufacturingUnlocked |= IsManufacturingType(Company)
				&& SaveMgr->IsIndustryAvailable(Company) && SaveMgr->IsIndustryUnlocked(Company);
		}
		if (!bManufacturingUnlocked) { return false; }
	}
	const UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return false;
	}
	const TArray<FPanelIntroTable>* Steps = TableMgr->GetPanelIntroSteps(PanelKey);
	return Steps && Steps->Num() > 0;
}

FName UPanelIntroSubsystem::GetRecruitDepartmentIntroKey(int32 BuildingIndex) const
{
	static const FName RecruitKey(TEXT("RecruitPlanningNextProject"));
	static const FName PrepareKey(TEXT("RecruitPlanningPrepareSeat"));
	if (BuildingIndex == INDEX_NONE || HasSeen(RecruitKey)
		|| (!ShouldPlay(RecruitKey) && !ShouldPlay(PrepareKey)))
	{
		return NAME_None;
	}

	UCGGameInstance* CGInstance = Cast<UCGGameInstance>(GetGameInstance());
	UWorld* CurrentWorld = GetWorld();
	const UMissionManagerSubsystem* MissionMgr = CGInstance ? CGInstance->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
	USaveLoadManager* SaveMgr = CGInstance ? CGInstance->GetSubsystem<USaveLoadManager>() : nullptr;
	const UTableManagerSubsystem* TableMgr = CGInstance ? CGInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const UEmployeeManager* EmployeeMgr = CGInstance ? CGInstance->GetSubsystem<UEmployeeManager>() : nullptr;
	const URecruitmentManagerSubsystem* RecruitMgr = CGInstance ? CGInstance->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	const UOfficeRecruitmentFlowSubsystem* Flow = CurrentWorld ? CurrentWorld->GetSubsystem<UOfficeRecruitmentFlowSubsystem>() : nullptr;
	const UOfficeStageProgressManager* StageMgr = CurrentWorld ? CurrentWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!MissionMgr || !MissionMgr->IsTutorialCompleted() || !SaveData || !TableMgr || !EmployeeMgr || !RecruitMgr
		|| !Flow || !StageMgr || !CGInstance || CGInstance->GetCurrentManagedBuildingIndex() != BuildingIndex
		|| CGInstance->GetCurrentBuildingCompanyType() != ECompanyType::Game)
	{
		return NAME_None;
	}
	if (const UGoalBoardSubsystem* Goals = CGInstance->GetSubsystem<UGoalBoardSubsystem>())
	{
		FGoalTable TrackedRow;
		const FName TrackedID = Goals->GetTrackedGoalID();
		if (!TrackedID.IsNone() && TableMgr->GetGoalData(TrackedID, TrackedRow)
			&& GrowthGoalRules::SuppressPlanningRecruitIntro(
				TrackedRow, Goals->GetGuideBuildingIndex(TrackedID), BuildingIndex))
		{
			return NAME_None;
		}
	}

	bool bFoundMission = false;
	const FMissionTable FirstMission = TableMgr->GetMissionData(FName(TEXT("M7_InHouseProject")), bFoundMission);
	if (!bFoundMission || FirstMission.RequiredProjectIndex <= 0
		|| !ShippedProjectRecords::Find(SaveData->GameData.ShippedProjects, BuildingIndex,
			ECompanyType::Game, FirstMission.RequiredProjectIndex))
	{
		return NAME_None;
	}

	const TArray<FProjectData> GameProjects = TableMgr->GetProjectsByCompanyType(ECompanyType::Game);
	const FProjectData* NextProject = nullptr;
	for (const FProjectData& Project : GameProjects)
	{
		if (Project.ProjectIndex > FirstMission.RequiredProjectIndex
			&& (!NextProject || Project.ProjectIndex < NextProject->ProjectIndex))
		{
			NextProject = &Project;
		}
	}
	if (!NextProject || !NextProject->IsPitchReady() || NextProject->Weight_Plan <= 0
		|| StageMgr->GetTierProgress().ClearedProjects.Contains(NextProject->ProjectIndex))
	{
		return NAME_None;
	}

	for (const FEmployeeInstance& Employee : EmployeeMgr->GetAllEmployeesRef())
	{
		if (Employee.AssignedBuildingIndex != INDEX_NONE
			&& EmployeeMgr->GetCompanyTypeForBuilding(Employee.AssignedBuildingIndex) == ECompanyType::Game
			&& GetPrimaryDisciplineSlot(Employee) == static_cast<int32>(EProductionDiscipline::Plan))
		{
			return NAME_None;
		}
	}

	if (RecruitMgr->GetFreeCapacity(BuildingIndex) > 0 && Flow->GetFreeSeatCount() > 0)
	{
		return ShouldPlay(RecruitKey) ? RecruitKey : NAME_None;
	}
	return ShouldPlay(PrepareKey) ? PrepareKey : NAME_None;
}

void UPanelIntroSubsystem::MarkSeen(FName PanelKey)
{
	if (PanelKey.IsNone())
	{
		return;
	}
	OnIntroCompleted.Broadcast(PanelKey);
	if (SeenPanelKeys.Contains(PanelKey))
	{
		return;
	}
	SeenPanelKeys.Add(PanelKey);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
	UE_LOG(LogTemp, Log, TEXT("[PanelIntro] '%s' 최초 진입 안내 완료"), *PanelKey.ToString());
}

int32 UPanelIntroSubsystem::GetHintCount(FName Key) const
{
	const int32* Found = HintCounts.Find(Key);
	return Found ? *Found : 0;
}

void UPanelIntroSubsystem::IncrementHint(FName Key)
{
	if (Key.IsNone())
	{
		return;
	}
	HintCounts.FindOrAdd(Key) += 1;
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}

void UPanelIntroSubsystem::ResetAll()
{
	SeenPanelKeys.Empty();
	HintCounts.Empty();
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
	UE_LOG(LogTemp, Log, TEXT("[PanelIntro] 전체 초기화 — 모든 패널 안내와 제스처 힌트가 다시 재생됩니다"));
}

UPanelIntroOverlayWidget* UPanelIntroSubsystem::PlayIntro(
	UUserWidget* OwnerPanel, FName PanelKey, bool bIgnoreSeen,
	const FPanelIntroAnchorResolver& AnchorResolver)
{
	if (!OwnerPanel || PanelKey.IsNone() || IsIntroPlaying())
	{
		return nullptr;
	}
	if (!bIgnoreSeen && !ShouldPlay(PanelKey))
	{
		return nullptr;
	}
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!bIgnoreSeen && (!CGBackNav::IsTopWindow(*OwnerPanel) || (GI && GI->IsLevelTransitionActive())))
	{
		return nullptr;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> OverlayClass = TableMgr
		? TableMgr->GetWidgetClass(EWidgetType::PanelIntroOverlay) : nullptr;
	if (!OverlayClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PanelIntro] EWidgetType::PanelIntroOverlay 미등록 ― '%s' 안내 생략"),
			*PanelKey.ToString());
		return nullptr;
	}

	UPanelIntroOverlayWidget* Overlay =
		CreateWidget<UPanelIntroOverlayWidget>(OwnerPanel->GetOwningPlayer(), OverlayClass);
	if (!Overlay)
	{
		return nullptr;
	}

	Overlay->AddToViewport(9100); // 미션 가이드(9000) 위
	Overlay->OnIntroFinished.AddUObject(this, &UPanelIntroSubsystem::HandleIntroFinished);
	PlayingOverlay = Overlay;
	if (AnchorResolver.IsBound())
	{
		// StartIntro 전에 걸어야 첫 스텝부터 먹는다
		Overlay->SetAnchorResolver(AnchorResolver);
	}

	if (!Overlay->StartIntro(PanelKey, OwnerPanel))
	{
		Overlay->RemoveFromParent();
		return nullptr;
	}

	// 딤 2겹을 플래그로 협상하지 않고 가시성으로 배제한다 (패널 인트로 설계 §3.3)
	TArray<UUserWidget*> FoundOverlays;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		OwnerPanel, FoundOverlays, UMissionGuideOverlayWidget::StaticClass(), false);
	for (UUserWidget* Found : FoundOverlays)
	{
		if (Found && Found->GetVisibility() != ESlateVisibility::Collapsed)
		{
			HiddenMissionOverlay = Found;
			Found->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
	}

	return Overlay;
}

bool UPanelIntroSubsystem::IsIntroPlaying() const
{
	const UPanelIntroOverlayWidget* Overlay = PlayingOverlay.Get();
	return Overlay && Overlay->IsInViewport();
}

void UPanelIntroSubsystem::HandleIntroFinished()
{
	PlayingOverlay.Reset();
	if (UUserWidget* Restored = HiddenMissionOverlay.Get())
	{
		// 미션 오버레이 루트는 HitTestInvisible(입력 통과) — Visible 로 되돌리면 화면을 먹는다
		Restored->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	HiddenMissionOverlay.Reset();
}
