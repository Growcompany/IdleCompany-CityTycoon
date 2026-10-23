#pragma once

#include "CoreMinimal.h"
#include "Enum/MissionTypes.h"
#include "Manager/MissionGuidePhases.h"

// 도출형 튜토리얼(M8~M31) 래치 이름 — FGameSaveData.DerivedMissionLatches 에 그대로 저장된다
namespace TutorialDerivedLatch
{
	constexpr const TCHAR* StatPointAllocated = TEXT("StatPointAllocated");
	constexpr const TCHAR* StarEnhanceAttempted = TEXT("StarEnhanceAttempted");
	constexpr const TCHAR* SkinApplied = TEXT("SkinApplied");
	constexpr const TCHAR* PaintingPlaced = TEXT("PaintingPlaced");
	constexpr const TCHAR* PlotDialogSeen = TEXT("PlotDialogSeen");
	constexpr const TCHAR* HQIntroDone = TEXT("HQIntroDone");
	constexpr const TCHAR* MenuIntroDone = TEXT("MenuIntroDone");
	// 장비 미션 시작 때 끼고 있던 장비 기록 ― 표시 + 장비마다 EquipHad_<InstanceId>
	constexpr const TCHAR* EquipBaseline = TEXT("EquipBase");
	constexpr const TCHAR* EquipHadPrefix = TEXT("EquipHad_");
}

// 완료 판정의 값 비교만 담는다 — 월드·위젯 조회는 매니저가 하고 결과 값만 넘긴다
struct FTutorialDerivedMissionRules
{
	static constexpr double FloorRaiseWatchSeconds = 2.0;
	static constexpr double DemolishWatchSeconds = 4.0;
	static constexpr double PlotExplainMinSeconds = 2.5;
	static constexpr double HQLevelUpWatchSeconds = 2.0;

	// 레벨형=목표 레벨, 개수형=목표 개수. 0(상태·래치형)도 「하나 이상」으로 읽는다
	static int32 ResolveTarget(int64 ConditionAmount)
	{
		return static_cast<int32>(FMath::Clamp<int64>(ConditionAmount, 1, MAX_int32));
	}

	static bool AreFactoryStatsReached(int32 SpeedLevel, int32 CapacityLevel, int64 ConditionAmount)
	{
		const int32 Target = ResolveTarget(ConditionAmount);
		return SpeedLevel >= Target && CapacityLevel >= Target;
	}

	// SignalAt < 0 = 이번 세션에 신호를 못 봤다(재접속). Now < SignalAt = 맵 이동으로 월드 시계가 다시 0부터 — 둘 다 기다릴 근거가 없다
	static bool HasPresentationElapsed(double SignalAt, double Now, double MinSeconds)
	{
		return SignalAt < 0.0 || Now < SignalAt || Now - SignalAt >= MinSeconds;
	}

	static bool IsFloorRaiseDone(int32 FloorLevel, int64 ConditionAmount, double SignalAt, double Now, bool bManagePanelOpen)
	{
		return FloorLevel >= ResolveTarget(ConditionAmount)
			&& (!bManagePanelOpen || HasPresentationElapsed(SignalAt, Now, FloorRaiseWatchSeconds));
	}

	// 레벨업 축하 연출을 보고 나서 다음 미션(IT 건설)으로 넘어간다
	static bool IsHQLevelUpDone(int32 HQLevel, int64 ConditionAmount, double SignalAt, double Now)
	{
		return HQLevel >= ResolveTarget(ConditionAmount) && HasPresentationElapsed(SignalAt, Now, HQLevelUpWatchSeconds);
	}

	// 지은 순서(1 = 첫 회사)로 회사를 고른다. BuildingIndex 는 지을 때마다 커지므로 정렬 순서가 곧 건설 순서다
	static int32 ResolveCompanyIndexByOrder(const TArray<int32>& BuildingIndices, int32 Order)
	{
		TArray<int32> Owned;
		for (const int32 BuildingIndex : BuildingIndices)
		{
			if (BuildingIndex >= 0)
			{
				Owned.AddUnique(BuildingIndex);
			}
		}
		Owned.Sort();
		const int32 OrderIndex = FMath::Max(1, Order) - 1;
		return Owned.IsValidIndex(OrderIndex) ? Owned[OrderIndex] : INDEX_NONE;
	}

	static FName MakeEquipHadLatch(const FGuid& InstanceId)
	{
		return FName(*(FString(TutorialDerivedLatch::EquipHadPrefix) + InstanceId.ToString(EGuidFormats::Digits)));
	}

	// 미션 시작 때 없던 장비를 몇 개 끼고 있는가 ― 개수 비교면 같은 부위 [교체]가 늘지 않아 안내가 옛 장비로 되돌아간다
	static int32 CountNewlyWornEquipment(const TArray<FGuid>& WornNow, const TArray<FName>& Latches)
	{
		int32 NewlyWorn = 0;
		for (const FGuid& InstanceId : WornNow)
		{
			NewlyWorn += Latches.Contains(MakeEquipHadLatch(InstanceId)) ? 0 : 1;
		}
		return NewlyWorn;
	}

	static bool IsDemolishDone(bool bAnyCleared, double SignalAt, double Now)
	{
		return bAnyCleared && HasPresentationElapsed(SignalAt, Now, DemolishWatchSeconds);
	}

	static bool ShouldLatchPlotExplain(double OpenedAt, double Now)
	{
		return OpenedAt >= 0.0 && Now - OpenedAt >= PlotExplainMinSeconds;
	}

	static bool IsPlotExplainDone(bool bDialogSeen, bool bDialogOpen)
	{
		return bDialogSeen && !bDialogOpen;
	}

	static bool IsMenuTourDone(bool bMenuIntroDone, bool bPromptOpen)
	{
		return bMenuIntroDone && !bPromptOpen;
	}

	// 설명 패널 키 → 그 미션이 기다리는 래치. 다른 미션의 설명은 래치하지 않는다
	static const TCHAR* ResolveIntroLatch(EMissionConditionType Condition, FName PanelKey)
	{
		if (Condition == EMissionConditionType::ReachHQLevel && PanelKey == FName(TEXT("HQManage")))
		{
			return TutorialDerivedLatch::HQIntroDone;
		}
		if (Condition == EMissionConditionType::MenuTour && PanelKey == FName(TEXT("MenuPanel")))
		{
			return TutorialDerivedLatch::MenuIntroDone;
		}
		return nullptr;
	}

	// 월드로 끌어 놓고 [배치]를 누르는 걸음 — M2/M4 처럼 게이트·딤을 끄고 손은 프리뷰 위 드래그
	static bool IsPlacementConfirmStep(EMissionConditionType Condition, int32 StepIndex)
	{
		return (Condition == EMissionConditionType::PlaceSecondDesk
				&& (StepIndex == SecondDeskGuide::Confirm || StepIndex == SecondDeskGuide::ConfirmMore))
			|| (Condition == EMissionConditionType::RecruitGoalDiscipline && StepIndex == RecruitDisciplineGuide::ConfirmVacantDesk)
			|| (Condition == EMissionConditionType::PlaceFirstPainting && StepIndex == PaintingGuide::Confirm)
			|| ((Condition == EMissionConditionType::BuildSecondBuilding || Condition == EMissionConditionType::BuildIndustry)
				&& StepIndex == SecondBuildingGuide::PlaceBuilding);
	}

	// 재화를 써서 누르는 걸음 — 여기서 대상이 없거나 비활성이면 부족 안내(ShortfallLine)로 바꾼다
	static bool IsPurchaseStep(EMissionConditionType Condition, int32 StepIndex)
	{
		switch (Condition)
		{
		case EMissionConditionType::UpgradeFactoryStats:
			return StepIndex == FactoryStatGuide::UpgradeSpeed || StepIndex == FactoryStatGuide::UpgradeCapacity;
		case EMissionConditionType::RaiseBuildingFloor:
			return StepIndex == RaiseFloorGuide::PressBuildUp;
		case EMissionConditionType::EnhanceBuildingStat:
			return StepIndex == EnhanceStatGuide::PressEnhance;
		case EMissionConditionType::EnhanceFirstEmployee:
			return StepIndex == EmployeeGrowthGuide::PressStarAttempt;
		case EMissionConditionType::UpgradeFirstWorkstation:
			return StepIndex == DeskUpgradeGuide::PressUpgrade;
		case EMissionConditionType::PlaceFirstPainting:
			return StepIndex == PaintingGuide::PickPainting;
		case EMissionConditionType::BuildSecondBuilding:
		case EMissionConditionType::BuildIndustry:
			return StepIndex == SecondBuildingGuide::PickBuilding;
		case EMissionConditionType::ReachHQLevel:
			return StepIndex == HQLevelUpGuide::PressLevelUp;
		case EMissionConditionType::AcquireFirstCompany:
			return StepIndex == AcquireGuide::PressAcquire;
		case EMissionConditionType::SkipCompanyRecovery:
			return StepIndex == SkipRecoveryGuide::PressSkip || StepIndex == SkipRecoveryGuide::Confirm;
		default:
			return false;
		}
	}

	// 래치는 그 미션 진행분이라 다른 미션(점프·완료 후 저장)으로 새지 않게 같은 ID 에서만 되살린다
	static TArray<FName> ResolveRestoredLatches(FName SavedLatchMissionID, FName RestoredMissionID,
		bool bRestoredIsDerived, const TArray<FName>& SavedLatches)
	{
		if (!bRestoredIsDerived || RestoredMissionID.IsNone() || SavedLatchMissionID != RestoredMissionID)
		{
			return TArray<FName>();
		}
		return SavedLatches;
	}
};
