#pragma once

#include "CoreMinimal.h"
#include "Data/FactorySaveData.h"
#include "Enum/MissionTypes.h"

// 가이드 페이즈 의미 (DT MentorLines 인덱스와 1:1). 매니저 switch 와 GuideGestureRules 가 공유한다.
namespace BuildGuide
{
	constexpr int32 PressBuild = 0;    // [건설] 버튼 잔상
	constexpr int32 PickIndustry = 1;  // 빌드 모달 — '게임' 산업 타일 클릭
	constexpr int32 PickBuilding = 2;  // 선택 가능한 건물 카드 목록
	constexpr int32 PlaceBuilding = 3; // 배치 확정 — 배치 바 [배치] 링 타겟 + 프리뷰 위 드래그 손(제스처 채널), 딤 0
}
namespace BrickGuide
{
	constexpr int32 ObserveLaborers = 0;
	constexpr int32 OpenFactory = 1;
	constexpr int32 AddLaborer = 2;
	constexpr int32 ExpandFactory = 3;
	constexpr int32 WatchExpansion = 4;
	constexpr int32 BoostLaborers = 5;
	constexpr int32 CollectBricks = 6;

	constexpr int64 FirstDeliveryBricks = 3;

	inline int32 PhaseAfterDelivery(int32 Phase, int64 Bricks, int32 LaborerCount, int32 LaborerCapacity, int32 ExpansionLevel)
	{
		if (Bricks < FirstDeliveryBricks) return Phase;
		if (ExpansionLevel > 1 && Phase <= ExpandFactory) return BoostLaborers;
		if (LaborerCapacity > 0 && LaborerCount >= LaborerCapacity && Phase <= AddLaborer) return ExpandFactory;
		return Phase == ObserveLaborers ? OpenFactory : Phase;
	}

	inline int32 PhaseAfterPurchase(int32 Phase, EFactoryUpgradeType Type, int32 LaborerCount, int32 LaborerCapacity)
	{
		if (Phase == AddLaborer && Type == EFactoryUpgradeType::LaborerCount
			&& LaborerCapacity > 0 && LaborerCount >= LaborerCapacity) return ExpandFactory;
		if (Phase == ExpandFactory && Type == EFactoryUpgradeType::FactoryExpansion) return WatchExpansion;
		return Phase;
	}

	inline int32 PhaseAfterExpansionWatch(int32 Phase, double ElapsedSeconds)
	{
		return Phase == WatchExpansion && ElapsedSeconds >= 2.0 ? BoostLaborers : Phase;
	}

	inline int32 PhaseAfterBoost(int32 Phase, double HeldSeconds)
	{
		return Phase == BoostLaborers && HeldSeconds >= 1.25 ? CollectBricks : Phase;
	}

	inline bool CanComplete(int32 Phase, int64 Bricks, int64 Target)
	{
		return Phase == CollectBricks && Bricks >= Target;
	}
}
namespace EnterOfficeGuide
{
	constexpr int32 ClickBuilding = 0; // 지은 회사 빌딩 스포트라이트 — 눌러서 관리 패널 열기 (MainMap)
	constexpr int32 PressEnter = 1;    // ManagePanel [입장] 버튼 하이라이트 (MainMap) → OfficeMap 진입으로 완료
}
// M5 RecruitEmployees — 채용 모달 → 원하는 부서 선택 → 채용 → 입장/확인.
// 완료는 페이즈가 아니라 빌딩 인원수 도달로 판정한다 — ×1 반복 경로로도 통과해 소프트락이 안 생긴다.
namespace RecruitGuide
{
	constexpr int32 OpenRecruit       = 0;  // 오피스 [채용] 도크 버튼 → 채용 패널 열기
	constexpr int32 PickJob           = 1;  // 여섯 부서 중 원하는 부서를 직접 선택
	constexpr int32 PressPull         = 2;  // 채용 패널 [일괄 채용]
	constexpr int32 PressConfirm      = 3;  // 사원증 리빌 [확인] (자동 착석)
	constexpr int32 PressCloseRecruit = 4;  // 채용 완료 뒤 모달 종료 대기
}
// M4 PlaceDesks — 카운트형. 책상은 확정 후에도 배치 모드가 유지되는 연속 배치라,
// 한 개 놓았다고 버튼 유도(OpenPlacement)로 되돌리면 이미 열려 있는 버튼을 다시 누르라는 문구가 된다.
// 모드가 살아 있는 동안은 PlaceMore 로 남은 개수를 안내하고, 모드를 나가야 PollGuideProgress 가 버튼 유도로 되돌린다.
namespace DeskGuide  { constexpr int32 OpenPlacement = 0; constexpr int32 PickDesk = 1; constexpr int32 Confirm = 2; constexpr int32 PlaceMore = 3; }
// M6 LaunchFirstInHouseProject — [새 프로젝트] 기획 보드 → 기획안 선택 → 개발 스트립 관찰 → [출시](즉시 완료, 운영은 3분 백그라운드)
namespace InHouseGuide
{
	constexpr int32 OpenBoard   = 0;
	constexpr int32 PickInHouse = 1;
	constexpr int32 WatchStrip  = 2;  // 개발 중 요구점수·배속 안내
	constexpr int32 PressLaunch = 3;
	constexpr int32 DiscardFailed = 4; // 내부 테스트 미달 ― [폐기] 후 다시 착수(추가 개발은 다이아 부족)
}
// M7 CollectFirstRevenue — 사무실 [뒤로] → 수입/운영 설명 → 수익 유입 관찰 (cross-map)
namespace CollectGuide
{
	constexpr int32 ExitOffice   = 0;  // 사무실 [뒤로]
	// 유입(수입 칩)과 운영 진행 게이지를 순서대로 설명한다.
	constexpr int32 Explain      = 1;
	// 수익이 실시간으로 들어오는 것을 지켜보는 단계. 수거 버튼이 없어져 스포트라이트 타겟이 없다.
	constexpr int32 WatchIncome = 2;
}

// ===== 상태 도출형 걸음 (M8~M31 bDerivedGuide, 성장 도움말 공용) =====
// 인덱스 = DT_Mission MentorLines/GuideLines 줄 인덱스(성장 도움말은 DT_Goal StepLines). 0 은 항상 장소 이동.
// StepCount = 그 조건 행의 CSV 줄 수 — 줄이 모자라면 빈 말풍선이 된다.
// 필수 튜토리얼이 대상을 더 깊이 지목하는 조건은 성장 도움말 번호가 달라 Goal 하위 네임스페이스로 따로 둔다.

namespace FactoryStatGuide // M2 UpgradeFactoryStats
{
	constexpr int32 ExitOffice      = 0;
	constexpr int32 OpenFactory     = 1;
	constexpr int32 UpgradeSpeed    = 2;
	constexpr int32 UpgradeCapacity = 3;
	constexpr int32 ClosePanel      = 4;
	constexpr int32 StepCount       = 5;
}
namespace RaiseFloorGuide // M9 / G1 RaiseBuildingFloor
{
	constexpr int32 ExitOffice      = 0;
	constexpr int32 TapCompany      = 1;
	constexpr int32 OpenEnhanceTab  = 2;
	constexpr int32 PressBuildUp    = 3;
	constexpr int32 CloseOtherPanel = 4;
	constexpr int32 WatchFloorRise  = 5; // 필수 튜토리얼 전용 — 빌드업 뒤 패널을 닫아 층 상승을 보여 준다
	constexpr int32 StepCount       = 6;
}
namespace EnhanceStatGuide // M10 EnhanceBuildingStat
{
	constexpr int32 ExitOffice     = 0;
	constexpr int32 TapCompany     = 1;
	constexpr int32 OpenEnhanceTab = 2;
	constexpr int32 PressEnhance   = 3;
	constexpr int32 StepCount      = 4;
}
namespace TraitGuide // M11 / G10 EquipFirstTrait
{
	constexpr int32 ExitOffice    = 0;
	constexpr int32 OpenGacha     = 1;
	constexpr int32 PickCategory  = 2;
	constexpr int32 PressPull     = 3;
	constexpr int32 ConfirmReveal = 4; // 연출이 끝나야 [확인]이 생긴다 — 그전은 기다림
	constexpr int32 CloseGacha    = 5; // 뽑기 창이 관리 패널을 가린다
	constexpr int32 OpenTraitTab  = 6; // 회사 탭 → [특성] 탭
	constexpr int32 EquipCard     = 7;
	constexpr int32 StepCount     = 8;
	namespace Goal { constexpr int32 ExitOffice = 0; constexpr int32 OpenGacha = 1; constexpr int32 OpenTraitTab = 2; constexpr int32 StepCount = 3; }
}
namespace SkinGuide // M12 / G8 EquipFirstSkin
{
	constexpr int32 ExitOffice    = 0;
	constexpr int32 OpenGacha     = 1;
	constexpr int32 PickCategory  = 2;
	constexpr int32 PressPull     = 3;
	constexpr int32 ConfirmReveal = 4;
	constexpr int32 CloseGacha    = 5;
	constexpr int32 OpenSkinTab   = 6; // 회사 탭 → [스킨] 탭
	constexpr int32 ApplySkin     = 7;
	constexpr int32 StepCount     = 8;
	namespace Goal { constexpr int32 ExitOffice = 0; constexpr int32 OpenGacha = 1; constexpr int32 OpenSkinTab = 2; constexpr int32 StepCount = 3; }
}
namespace SecondDeskGuide // M13·M23 PlaceSecondDesk
{
	constexpr int32 EnterOffice   = 0; // 회사 탭 / [입장]
	constexpr int32 OpenPlacement = 1;
	constexpr int32 PickDesk      = 2;
	constexpr int32 Confirm       = 3;
	constexpr int32 ConfirmMore   = 4; // 여러 개 중 마지막 하나 ― 「하나 더」
	constexpr int32 StepCount     = 5;
}
namespace RecruitDisciplineGuide // M14·M24 / G14·G16 RecruitGoalDiscipline
{
	constexpr int32 EnterOffice     = 0;
	constexpr int32 CapacityFull    = 1; // 사무실이면 [뒤로], 도시면 회사 지목
	constexpr int32 OpenEnhanceTab  = 2;
	constexpr int32 PressBuildUp    = 3;
	constexpr int32 PlaceVacantDesk = 4;
	constexpr int32 OpenRecruit     = 5;
	constexpr int32 PickDiscipline  = 6;
	constexpr int32 PressPull       = 7;
	constexpr int32 PickVacantDesk  = 8; // 필수 튜토리얼만 — 성장 도움말(DT_Goal)은 4번 한 줄로 끝난다
	constexpr int32 ConfirmVacantDesk = 9;
	constexpr int32 StepCount       = 10;
}
namespace EmployeeGrowthGuide // M15·M16·M17 / G12·G22·G9 — LevelUp·AllocateStatPoint·EnhanceFirstEmployee 공용
{
	constexpr int32 EnterOffice       = 0;
	constexpr int32 OpenEmployee      = 1;
	constexpr int32 PressGrowth       = 2;
	constexpr int32 PressStarAttempt  = 3; // 별 목표: 스타포스 창의 [강화]
	constexpr int32 PickReadyEmployee = 3; // 그 외: 선택 직원이 성장 불가라 다른 직원 안내
	constexpr int32 StepCount         = 4;
}
namespace EquipmentGuide // M18 EquipFirstEquipment
{
	constexpr int32 EnterOffice   = 0;
	constexpr int32 OpenEmployee  = 1;
	constexpr int32 PickSlot      = 2;
	constexpr int32 OpenGacha     = 3;
	constexpr int32 PressPull     = 4;
	constexpr int32 ConfirmReveal = 5;
	constexpr int32 CloseGacha    = 6; // 닫혀야 직원 패널이 돌아온다
	constexpr int32 Equip         = 7;
	constexpr int32 StepCount     = 8;
}
namespace DeskUpgradeGuide // M19 / G13·G18 Upgrade·PromoteFirstWorkstation
{
	constexpr int32 EnterOffice  = 0;
	constexpr int32 PickDesk     = 1;
	constexpr int32 PressUpgrade = 2;
	constexpr int32 ClosePanel   = 3; // 열린 책상이 강화 불가 — 닫고 준비된 책상으로
	constexpr int32 StepCount    = 4;
}
namespace PaintingGuide // M20 / G7 PlaceFirstPainting
{
	constexpr int32 EnterOffice    = 0;
	constexpr int32 OpenDecoration = 1;
	constexpr int32 PickPainting   = 2;
	constexpr int32 Confirm        = 3;
	constexpr int32 StepCount      = 4;
	namespace Goal { constexpr int32 EnterOffice = 0; constexpr int32 OpenDecoration = 1; constexpr int32 StepCount = 2; }
}
namespace LaunchGoalGuide // M21 / G15·G17·G20 LaunchGoalProject
{
	namespace Goal
	{
		constexpr int32 DiscardFailed = 6;
		constexpr int32 CloseForPreparation = 7;
		constexpr int32 PickPreparationDesk = 8;
		constexpr int32 UpgradePreparationDesk = 9;
		constexpr int32 ClosePreparationPanel = 10;
		constexpr int32 PreparationAlternatives = 11;
		constexpr int32 ReducePreparationBulk = 12;
	}
	constexpr int32 EnterOffice = 0;
	constexpr int32 OpenBoard   = 1;
	constexpr int32 PickProject = 2;
	constexpr int32 PressStart  = 3;
	constexpr int32 PressLaunch = 4;
	constexpr int32 WaitRunning = 5; // 앞 프로젝트 운영 중 — 기다림
	constexpr int32 WatchDevelop = 6; // 필수 튜토리얼만 ― 목표 프로젝트 개발 중(관찰)
	constexpr int32 PressStartRisk = 7; // 필수 튜토리얼만 ― 카드가 미달 경고로 「그래도 개발」일 때
	constexpr int32 DiscardFailed = 8; // 필수 튜토리얼만 ― 내부 테스트 미달 창의 [폐기]
	constexpr int32 StepCount   = 9;
}
namespace SecondBuildingGuide // M22·M30 / G3·G6·G19 BuildSecondBuilding·BuildOnClearedPlot·BuildIndustry
{
	constexpr int32 ExitOffice    = 0;
	constexpr int32 OpenBuild     = 1;
	constexpr int32 PickIndustry  = 2;
	constexpr int32 PickBuilding  = 3;
	constexpr int32 PlaceBuilding = 4;
	constexpr int32 StepCount     = 5;
	namespace Goal { constexpr int32 ExitOffice = 0; constexpr int32 OpenBuild = 1; constexpr int32 InModal = 2; constexpr int32 StepCount = 3; }
}
namespace AcquireGuide // M25 / G4·G11 AcquireFirstCompany·AcquireFirstPlot
{
	constexpr int32 ExitOffice   = 0;
	constexpr int32 TapCompany   = 1; // 가장 싼 미인수 회사 — 필수 튜토리얼에서만 지목
	constexpr int32 PressAcquire = 2;
	constexpr int32 StepCount    = 3;
	namespace Goal { constexpr int32 ExitOffice = 0; constexpr int32 FindTarget = 1; constexpr int32 StepCount = 2; }
}
namespace SkipRecoveryGuide // M26 SkipCompanyRecovery
{
	constexpr int32 ExitOffice = 0;
	constexpr int32 TapCompany = 1;
	constexpr int32 PressSkip  = 2;
	constexpr int32 Confirm    = 3; // 확인 팝업이 있을 때만
	constexpr int32 ConfirmResult = 4; // 인수 결과 창이 남아 있을 때 ― 그 [확인]이 관리 창을 연다
	constexpr int32 StepCount  = 5;
}
namespace DemolishGuide // M27 / G5 DemolishFirstCompany
{
	constexpr int32 ExitOffice    = 0;
	constexpr int32 TapCompany    = 1;
	constexpr int32 PressDemolish = 2;
	constexpr int32 Confirm       = 3;
	constexpr int32 WatchDemolish = 4; // 철거 연출 — 기다림
	constexpr int32 StepCount     = 5;
	namespace Goal { constexpr int32 ExitOffice = 0; constexpr int32 FindTarget = 1; constexpr int32 StepCount = 2; }
}
namespace PlotExplainGuide // M28 ExplainPlot
{
	constexpr int32 ExitOffice = 0;
	constexpr int32 TapPlot    = 1;
	constexpr int32 Explain    = 2; // 잠긴 [인수] 옆 설명 ― 최소 열람 시간 동안
	constexpr int32 CloseDialog = 3;
	constexpr int32 StepCount  = 4;
}
namespace HQLevelUpGuide // M29 ReachHQLevel (필수 튜토리얼 — 성장 도움말 G2 는 공용 판정 번호를 쓴다)
{
	constexpr int32 ExitOffice    = 0;
	constexpr int32 OpenHQ        = 1;
	constexpr int32 OpenGrowthTab = 2;
	constexpr int32 Explain       = 3; // PanelIntro HQManage 순차 — 그동안 미션 오버레이는 그리지 않는다
	constexpr int32 PressLevelUp  = 4; // 조건 미달로 꺼져 있으면 부족 문구
	constexpr int32 WatchLevelUp  = 5; // 축하 연출 — 대상 없음
	constexpr int32 StepCount     = 6;
}
namespace MenuTourGuide // M33 MenuTour
{
	constexpr int32 ExitOffice   = 0;
	constexpr int32 OpenMenu     = 1;
	constexpr int32 ExplainMenu  = 2; // PanelIntro MenuPanel
	constexpr int32 CloseMenu    = 3;
	constexpr int32 StepCount    = 4;
}

// 도출형 조건의 걸음 수(= DT_Mission 줄 수). 0 = 도출형 걸음표가 없는 조건
inline int32 GetDerivedStepCount(EMissionConditionType Condition)
{
	switch (Condition)
	{
	case EMissionConditionType::UpgradeFactoryStats:       return FactoryStatGuide::StepCount;
	case EMissionConditionType::RaiseBuildingFloor:        return RaiseFloorGuide::StepCount;
	case EMissionConditionType::EnhanceBuildingStat:       return EnhanceStatGuide::StepCount;
	case EMissionConditionType::EquipFirstTrait:           return TraitGuide::StepCount;
	case EMissionConditionType::EquipFirstSkin:            return SkinGuide::StepCount;
	case EMissionConditionType::PlaceSecondDesk:           return SecondDeskGuide::StepCount;
	case EMissionConditionType::RecruitGoalDiscipline:     return RecruitDisciplineGuide::StepCount;
	case EMissionConditionType::LevelUpFirstEmployee:
	case EMissionConditionType::AllocateEmployeeStatPoint:
	case EMissionConditionType::EnhanceFirstEmployee:      return EmployeeGrowthGuide::StepCount;
	case EMissionConditionType::EquipFirstEquipment:       return EquipmentGuide::StepCount;
	case EMissionConditionType::UpgradeFirstWorkstation:
	case EMissionConditionType::PromoteFirstWorkstation:   return DeskUpgradeGuide::StepCount;
	case EMissionConditionType::PlaceFirstPainting:        return PaintingGuide::StepCount;
	case EMissionConditionType::LaunchGoalProject:         return LaunchGoalGuide::StepCount;
	case EMissionConditionType::BuildSecondBuilding:
	case EMissionConditionType::BuildOnClearedPlot:
	case EMissionConditionType::BuildIndustry:             return SecondBuildingGuide::StepCount;
	case EMissionConditionType::AcquireFirstCompany:
	case EMissionConditionType::AcquireFirstPlot:          return AcquireGuide::StepCount;
	case EMissionConditionType::SkipCompanyRecovery:       return SkipRecoveryGuide::StepCount;
	case EMissionConditionType::DemolishFirstCompany:      return DemolishGuide::StepCount;
	case EMissionConditionType::ExplainPlot:               return PlotExplainGuide::StepCount;
	case EMissionConditionType::ReachHQLevel:              return HQLevelUpGuide::StepCount;
	case EMissionConditionType::MenuTour:                  return MenuTourGuide::StepCount;
	default:                                               return 0;
	}
}
