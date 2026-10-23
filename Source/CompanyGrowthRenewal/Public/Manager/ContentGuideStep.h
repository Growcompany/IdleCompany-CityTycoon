#pragma once

#include "CoreMinimal.h"
#include "Enum/MissionTypes.h"
#include "Enum/ProductionDiscipline.h"
#include "Enum/CompanyType.h"

class AWorkstationActorBase;

// 「지금 할 한 걸음」 판정 입력. 출처(미션판 추적 행 / 튜토리얼 체인 행)를 지워 두 경로가 같은 판정기를 쓴다
struct FContentGuideSpec
{
	EMissionConditionType Condition = EMissionConditionType::None;
	// INDEX_NONE = 회사를 특정하지 않는다(현재 관리 중인 곳 / 늘 가던 곳으로 대체)
	int32 BuildingIndex = INDEX_NONE;
	EProductionDiscipline Discipline = EProductionDiscipline::Count;
	int32 TargetProjectIndex = 0;
	ECompanyType Industry = ECompanyType::None;
	// 레벨형은 목표 레벨 — 같은 화면 안에서 다음에 누를 버튼을 고르는 데 쓴다(성장 도움말은 0)
	int64 ConditionAmount = 0;
	// 성장 도움말이 플레이어 선택에 맡기는 대상(인수 회사·건설 카드 등)은 필수 튜토리얼에서만 지목한다
	bool bForcedTutorial = false;
};

// Resolve*Step 은 열린 화면 플래그만 받는 순수 함수 — 월드 없이 자동화 테스트로 고정한다
namespace CGRGoalGuideStep
{
bool CanGuideDeskGrowth(const AWorkstationActorBase* Desk, bool bPromotion);
int32 ResolveEmployeeGrowthStep(bool bInTargetOffice, bool bWindowOpen, bool bStarModalOpen, bool bStarGoal);
int32 ResolveRaiseFloorStep(bool bInOffice, bool bManagePanelOpen, bool bEnhancementTabActive);

// ===== 필수 튜토리얼(M8~M31) 전용 — 반환값은 MissionGuidePhases.h 의 걸음 상수 =====

// 최상단 프롬프트 종류. 흐름 밖 창은 닫기 버튼으로 안내하거나(게터가 있을 때) 대상 없이 풀어 둔다
enum class EGuidePromptKind : uint8
{
	None,
	CompanyGacha,   // 도시 [뽑기] 특성·스킨 셸
	EquipmentGacha,
	BuildModal,
	CompanyInfo,    // 도시 회사 인수 창
	ConfirmCancel,
	HQPanel,
	HQLevelUpCelebration,
	MenuPanel,
	ShopPanel,
	StarforceModal,
	Other,
};

// 이 조건의 걸음 중 하나가 이 종류의 창을 띄울 수 있는가. 같은 종류 안의 대상 구분(어느 회사·어느 확인 창)은 호출부가 한다
bool IsPromptInFlow(EMissionConditionType Condition, EGuidePromptKind Kind);

int32 ResolveFactoryStatStep(bool bInOffice, bool bFactoryPanelOpen, bool bSpeedReached, bool bCapacityReached);
// 다른 회사 패널이 열려 있으면 닫기부터, 층이 이미 올랐으면 관찰 걸음
int32 ResolveForcedRaiseFloorStep(bool bInOffice, bool bOtherCompanyPanelOpen, bool bCompanyPanelOpen,
	bool bEnhancementTabActive, bool bFloorReached);
int32 ResolveEnhanceStatStep(bool bInOffice, bool bCompanyPanelOpen, bool bEnhancementTabActive);

// M11 특성 / M12 스킨 — 두 조건은 걸음 번호가 같다
struct FCompanyGachaGuideFlags
{
	bool bInOffice = false;
	bool bRevealOpen = false;
	bool bGachaOpen = false;
	bool bWrongCategory = false;
	// 장착·적용할 뽑기 결과를 이미 가졌다 — 뽑기 걸음은 끝난 것
	bool bOwnsResult = false;
	bool bCompanyPanelOpen = false;
	bool bTargetTabActive = false;
};
int32 ResolveCompanyGachaStep(const FCompanyGachaGuideFlags& Flags);

// M13·M23 책상 / M20 그림 — 배치 확정 걸음은 배치 모드가 실제로 켜졌을 때만(게이트·딤 해제가 이 번호에 묶여 있다)
int32 ResolveOfficePlacementStep(bool bInTargetOffice, bool bCatalogOpen, bool bPlacementLive);

// M18 장비
struct FEquipmentGuideFlags
{
	bool bInTargetOffice = false;
	bool bRevealOpen = false;
	bool bGachaOpen = false;
	// 아무 직원도 끼지 않은 보유 장비가 있다 — 뽑기 없이 장착으로 간다
	bool bOwnsUnequipped = false;
	// 보유 0개인 부위가 있어 빈 칸을 누르면 패널 안 [장비 뽑기]가 뜬다. 없으면 도크 [장비]로 간다
	bool bPanelGachaReachable = false;
	bool bEmployeePanelOpen = false;
	bool bEquipmentTabActive = false;
	bool bPanelGachaVisible = false;
};
int32 ResolveEquipmentStep(const FEquipmentGuideFlags& Flags);

// M22 두 번째 회사 / M30 IT 회사 — 산업 타일 걸음은 이번 모달에서 목표 산업을 아직 안 눌렀을 때만
int32 ResolveSecondBuildingStep(bool bInOffice, bool bBuildModalOpen, bool bIndustryPending, bool bPlacementLive);
int32 ResolveAcquireStep(bool bInOffice, bool bTargetInfoOpen);
// 인수 결과 창 [확인]은 관리 창을 여는 버튼이라 「회사 눌러 관리 창 열기」 걸음으로 읽는다
int32 ResolveSkipRecoveryStep(bool bInOffice, bool bResultConfirmPending, bool bManagePanelOpen, bool bSkipConfirmOpen);
int32 ResolveDemolishStep(bool bInOffice, bool bDemolished, bool bManagePanelOpen, bool bDemolishConfirmOpen);
int32 ResolvePlotExplainStep(bool bInOffice, bool bDialogOpen, bool bExplainRead);
// 레벨이 오르면 어디에 있든 축하 관찰 — 완료가 연출 뒤로 밀려 있는 동안 다른 걸음으로 튀지 않는다
int32 ResolveHQLevelUpStep(bool bInOffice, bool bPanelOpen, bool bGrowthTabActive, bool bIntroDone, bool bLevelReached);
int32 ResolveMenuTourStep(bool bInOffice, bool bMenuOpen, bool bMenuIntroDone);
// 목표 프로젝트를 개발 중이면 [출시] 대기, 다른 프로젝트가 돌고 있으면 기다림
int32 ResolveForcedBusyProjectStep(bool bTargetProjectInProgress);
}
