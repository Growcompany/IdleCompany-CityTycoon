#include "Manager/ContentGuideStep.h"
#include "Manager/MissionGuidePhases.h"

namespace CGRGoalGuideStep
{
// 특성·스킨은 한 판정기를 쓰므로 번호가 어긋나면 한쪽 문구가 밀린다
static_assert(TraitGuide::ExitOffice == SkinGuide::ExitOffice && TraitGuide::OpenGacha == SkinGuide::OpenGacha
	&& TraitGuide::PickCategory == SkinGuide::PickCategory && TraitGuide::PressPull == SkinGuide::PressPull
	&& TraitGuide::ConfirmReveal == SkinGuide::ConfirmReveal && TraitGuide::CloseGacha == SkinGuide::CloseGacha
	&& TraitGuide::OpenTraitTab == SkinGuide::OpenSkinTab && TraitGuide::EquipCard == SkinGuide::ApplySkin,
	"TraitGuide / SkinGuide step numbers must match");
static_assert(SecondDeskGuide::EnterOffice == PaintingGuide::EnterOffice
	&& SecondDeskGuide::OpenPlacement == PaintingGuide::OpenDecoration
	&& SecondDeskGuide::PickDesk == PaintingGuide::PickPainting && SecondDeskGuide::Confirm == PaintingGuide::Confirm,
	"SecondDeskGuide / PaintingGuide step numbers must match");

bool IsPromptInFlow(EMissionConditionType Condition, EGuidePromptKind Kind)
{
	if (Kind == EGuidePromptKind::None)
	{
		return true;
	}
	switch (Condition)
	{
	case EMissionConditionType::EquipFirstTrait:
	case EMissionConditionType::EquipFirstSkin:
		return Kind == EGuidePromptKind::CompanyGacha;
	case EMissionConditionType::EquipFirstEquipment:
		return Kind == EGuidePromptKind::EquipmentGacha;
	case EMissionConditionType::BuildSecondBuilding:
	case EMissionConditionType::BuildIndustry:
		return Kind == EGuidePromptKind::BuildModal;
	case EMissionConditionType::AcquireFirstCompany:
		return Kind == EGuidePromptKind::CompanyInfo;
	case EMissionConditionType::SkipCompanyRecovery:
		return Kind == EGuidePromptKind::CompanyInfo || Kind == EGuidePromptKind::ConfirmCancel;
	case EMissionConditionType::DemolishFirstCompany:
	case EMissionConditionType::ExplainPlot:
		return Kind == EGuidePromptKind::ConfirmCancel;
	case EMissionConditionType::ReachHQLevel:
		return Kind == EGuidePromptKind::HQPanel || Kind == EGuidePromptKind::HQLevelUpCelebration;
	case EMissionConditionType::MenuTour:
		return Kind == EGuidePromptKind::MenuPanel;
	// 성장 도움말 걸음을 그대로 쓰는 사무실 조건 — 채용·기획 보드·출시·확인 창을 종류로 다 가릴 수 없어 도시 창만 밖으로 본다
	// 별 강화 창은 별 강화 미션의 것 — 다음 미션까지 남아 있으면 먼저 닫게 한다
	case EMissionConditionType::EnhanceFirstEmployee:
		return Kind == EGuidePromptKind::StarforceModal || Kind == EGuidePromptKind::ConfirmCancel || Kind == EGuidePromptKind::Other;
	case EMissionConditionType::RecruitGoalDiscipline:
	case EMissionConditionType::LevelUpFirstEmployee:
	case EMissionConditionType::AllocateEmployeeStatPoint:
	case EMissionConditionType::UpgradeFirstWorkstation:
	case EMissionConditionType::LaunchGoalProject:
		return Kind == EGuidePromptKind::ConfirmCancel || Kind == EGuidePromptKind::Other;
	default:
		return false;
	}
}

int32 ResolveFactoryStatStep(bool bInOffice, bool bFactoryPanelOpen, bool bSpeedReached, bool bCapacityReached)
{
	if (bInOffice)
	{
		return FactoryStatGuide::ExitOffice;
	}
	// 다 올렸으면 패널을 닫아야 다음 미션의 도크 버튼이 보인다
	if (bSpeedReached && bCapacityReached)
	{
		return FactoryStatGuide::ClosePanel;
	}
	if (!bFactoryPanelOpen)
	{
		return FactoryStatGuide::OpenFactory;
	}
	return bSpeedReached ? FactoryStatGuide::UpgradeCapacity : FactoryStatGuide::UpgradeSpeed;
}

int32 ResolveForcedRaiseFloorStep(bool bInOffice, bool bOtherCompanyPanelOpen, bool bCompanyPanelOpen,
	bool bEnhancementTabActive, bool bFloorReached)
{
	if (bInOffice)
	{
		return RaiseFloorGuide::ExitOffice;
	}
	if (bOtherCompanyPanelOpen)
	{
		return RaiseFloorGuide::CloseOtherPanel;
	}
	if (bFloorReached)
	{
		return RaiseFloorGuide::WatchFloorRise;
	}
	if (!bCompanyPanelOpen)
	{
		return RaiseFloorGuide::TapCompany;
	}
	return bEnhancementTabActive ? RaiseFloorGuide::PressBuildUp : RaiseFloorGuide::OpenEnhanceTab;
}

int32 ResolveEnhanceStatStep(bool bInOffice, bool bCompanyPanelOpen, bool bEnhancementTabActive)
{
	if (bInOffice)
	{
		return EnhanceStatGuide::ExitOffice;
	}
	if (!bCompanyPanelOpen)
	{
		return EnhanceStatGuide::TapCompany;
	}
	return bEnhancementTabActive ? EnhanceStatGuide::PressEnhance : EnhanceStatGuide::OpenEnhanceTab;
}

int32 ResolveCompanyGachaStep(const FCompanyGachaGuideFlags& Flags)
{
	if (Flags.bInOffice)
	{
		return TraitGuide::ExitOffice;
	}
	// 연출은 뷰포트 위에 따로 떠서 뽑기 창이 최상단 프롬프트로 남아 있다 — 연출이 먼저다
	if (Flags.bRevealOpen)
	{
		return TraitGuide::ConfirmReveal;
	}
	if (Flags.bGachaOpen)
	{
		if (Flags.bOwnsResult)
		{
			return TraitGuide::CloseGacha;
		}
		return Flags.bWrongCategory ? TraitGuide::PickCategory : TraitGuide::PressPull;
	}
	if (!Flags.bOwnsResult)
	{
		return TraitGuide::OpenGacha;
	}
	return Flags.bCompanyPanelOpen && Flags.bTargetTabActive ? TraitGuide::EquipCard : TraitGuide::OpenTraitTab;
}

int32 ResolveOfficePlacementStep(bool bInTargetOffice, bool bCatalogOpen, bool bPlacementLive)
{
	if (!bInTargetOffice)
	{
		return SecondDeskGuide::EnterOffice;
	}
	if (bPlacementLive)
	{
		return SecondDeskGuide::Confirm;
	}
	return bCatalogOpen ? SecondDeskGuide::PickDesk : SecondDeskGuide::OpenPlacement;
}

int32 ResolveEquipmentStep(const FEquipmentGuideFlags& Flags)
{
	if (!Flags.bInTargetOffice)
	{
		return EquipmentGuide::EnterOffice;
	}
	if (Flags.bRevealOpen)
	{
		return EquipmentGuide::ConfirmReveal;
	}
	if (Flags.bGachaOpen)
	{
		return Flags.bOwnsUnequipped ? EquipmentGuide::CloseGacha : EquipmentGuide::PressPull;
	}
	if (!Flags.bEmployeePanelOpen)
	{
		// 패널 안에 뽑기 진입이 없으면 [직원]으로 보내 봐야 되돌아 나온다 — 도크 [장비]로 바로 간다
		return Flags.bOwnsUnequipped || Flags.bPanelGachaReachable ? EquipmentGuide::OpenEmployee : EquipmentGuide::OpenGacha;
	}
	if (!Flags.bEquipmentTabActive)
	{
		return EquipmentGuide::PickSlot;
	}
	if (Flags.bOwnsUnequipped)
	{
		return EquipmentGuide::Equip;
	}
	if (Flags.bPanelGachaVisible)
	{
		return EquipmentGuide::OpenGacha;
	}
	return Flags.bPanelGachaReachable ? EquipmentGuide::PickSlot : EquipmentGuide::OpenGacha;
}

int32 ResolveSecondBuildingStep(bool bInOffice, bool bBuildModalOpen, bool bIndustryPending, bool bPlacementLive)
{
	if (bInOffice)
	{
		return SecondBuildingGuide::ExitOffice;
	}
	// 카드를 누르면 모달이 닫히고 배치가 시작된다 — 배치가 살아 있는 동안만 확정 걸음
	if (bPlacementLive)
	{
		return SecondBuildingGuide::PlaceBuilding;
	}
	if (!bBuildModalOpen)
	{
		return SecondBuildingGuide::OpenBuild;
	}
	return bIndustryPending ? SecondBuildingGuide::PickIndustry : SecondBuildingGuide::PickBuilding;
}

int32 ResolveAcquireStep(bool bInOffice, bool bTargetInfoOpen)
{
	if (bInOffice)
	{
		return AcquireGuide::ExitOffice;
	}
	return bTargetInfoOpen ? AcquireGuide::PressAcquire : AcquireGuide::TapCompany;
}

int32 ResolveSkipRecoveryStep(bool bInOffice, bool bResultConfirmPending, bool bManagePanelOpen, bool bSkipConfirmOpen)
{
	if (bInOffice)
	{
		return SkipRecoveryGuide::ExitOffice;
	}
	if (bResultConfirmPending)
	{
		return SkipRecoveryGuide::ConfirmResult;
	}
	if (bSkipConfirmOpen)
	{
		return SkipRecoveryGuide::Confirm;
	}
	return bManagePanelOpen ? SkipRecoveryGuide::PressSkip : SkipRecoveryGuide::TapCompany;
}

int32 ResolveDemolishStep(bool bInOffice, bool bDemolished, bool bManagePanelOpen, bool bDemolishConfirmOpen)
{
	if (bInOffice)
	{
		return DemolishGuide::ExitOffice;
	}
	if (bDemolished)
	{
		return DemolishGuide::WatchDemolish;
	}
	if (bDemolishConfirmOpen)
	{
		return DemolishGuide::Confirm;
	}
	return bManagePanelOpen ? DemolishGuide::PressDemolish : DemolishGuide::TapCompany;
}

int32 ResolvePlotExplainStep(bool bInOffice, bool bDialogOpen, bool bExplainRead)
{
	if (bInOffice)
	{
		return PlotExplainGuide::ExitOffice;
	}
	if (!bDialogOpen)
	{
		return PlotExplainGuide::TapPlot;
	}
	return bExplainRead ? PlotExplainGuide::CloseDialog : PlotExplainGuide::Explain;
}

int32 ResolveHQLevelUpStep(bool bInOffice, bool bPanelOpen, bool bGrowthTabActive, bool bIntroDone, bool bLevelReached)
{
	if (bLevelReached)
	{
		return HQLevelUpGuide::WatchLevelUp;
	}
	if (bInOffice)
	{
		return HQLevelUpGuide::ExitOffice;
	}
	if (!bPanelOpen)
	{
		return HQLevelUpGuide::OpenHQ;
	}
	if (!bGrowthTabActive)
	{
		return HQLevelUpGuide::OpenGrowthTab;
	}
	return bIntroDone ? HQLevelUpGuide::PressLevelUp : HQLevelUpGuide::Explain;
}

int32 ResolveMenuTourStep(bool bInOffice, bool bMenuOpen, bool bMenuIntroDone)
{
	if (bInOffice)
	{
		return MenuTourGuide::ExitOffice;
	}
	if (bMenuOpen)
	{
		return bMenuIntroDone ? MenuTourGuide::CloseMenu : MenuTourGuide::ExplainMenu;
	}
	if (!bMenuIntroDone)
	{
		return MenuTourGuide::OpenMenu;
	}
	return INDEX_NONE;
}

int32 ResolveForcedBusyProjectStep(bool bTargetProjectInProgress)
{
	return bTargetProjectInProgress ? LaunchGoalGuide::WatchDevelop : LaunchGoalGuide::WaitRunning;
}
}
