// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/HQManagePanelWidget.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Enum/ResourceType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/EntityManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Building/BuildingSlotHorizonWidget.h"
#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Panel/HQLevelUpCelebrationWidget.h"
#include "CommonTextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"
#include "Enum/WidgetType.h"
#include "Data/GameSaveData.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Engine/Texture2D.h"

namespace
{
	// 조건 체크 아이콘 — 사전 틴트 텍스처 2장 스왑 (그린=충족 / 회갈=미충족). 틴트 API 없이 cpp-only로 처리
	UTexture2D* GetCondCheckIcon(bool bMet)
	{
		static TSoftObjectPtr<UTexture2D> OnTex(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/UITextures/Icons/T_CondCheck_On.T_CondCheck_On")));
		static TSoftObjectPtr<UTexture2D> OffTex(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/UITextures/Icons/T_CondCheck_Off.T_CondCheck_Off")));
		return (bMet ? OnTex : OffTex).LoadSynchronous();
	}

	// 조건 진행바 색 — 미달은 '경고'가 아니라 '진행 중'이라 액센트 블루, 달성만 그린 (라이트판 기준)
	FLinearColor GetCondBarColor(float Progress)
	{
		static const FLinearColor Pending(0.047f, 0.328f, 0.745f); // #3D9BE0
		return Progress >= 1.f ? UStatRowWidget::ColorSuccess : Pending;
	}
}

void UHQManagePanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 탭 버튼 그룹 초기화
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);

	// RailTab은 CommonButtonBase 파생 — 그룹 직접 등록 (설정창 패턴 미러)
	auto RegisterTab = [this](UIconWithButtonWidget* Tab)
	{
		if (!Tab) return;
		TabButtonGroup->AddWidget(Tab);
		Tab->SetIsSelectable(true);
	};
	RegisterTab(HQTab);
	RegisterTab(BuildingListTab);
	RegisterTab(FinancialTab);

	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UHQManagePanelWidget::OnTabSelectionChanged);

	if (LevelUpButton)
	{
		LevelUpButton->OnClicked().AddUObject(this, &UHQManagePanelWidget::OnLevelUpButtonClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UHQManagePanelWidget::OnCloseButtonClicked);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UHQManagePanelWidget::OnCloseButtonClicked);
	}

	// 기본 탭: HQ (인덱스 0)
	TabButtonGroup->SelectButtonAtIndex(0);
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(0);

	// 이 패널은 매 push마다 CreateWidget — 그룹 초기화도 NativeOnActivated에서 (파일 고유 idiom)
	SetupSortButtonGroup();

	// ===== claim 라이브 갱신 — 조건 변동원 구독 (해제는 NativeOnDeactivated에서 짝) =====

	// HQ 레벨 변경 + 빌딩 레벨 변경 (치트/외부 경로 대응)
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnHQLevelUp.AddDynamic(this, &UHQManagePanelWidget::HandleHQLevelUp);
	}

	// 돈 변동 — 패널 열린 채 돈이 차오를 때 조건/CTA 실시간 갱신 (claim 어포던스의 핵심)
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResMgr->OnResourceChanged.AddUObject(this, &UHQManagePanelWidget::HandleResourceChanged);
	}

	// 직원 고용 완료
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeHireCompleted.AddUObject(this, &UHQManagePanelWidget::HandleEmployeeHireCompleted);
	}

	// 스테이지 완료 — World 서브시스템이라 MainMap에선 null (OfficeMap에서만 유효)
	if (UWorld* World = GetWorld())
	{
		if (UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>())
		{
			StageMgr->OnStageComplete.AddDynamic(this, &UHQManagePanelWidget::HandleStageComplete);
		}
	}

	// 장식 VFX 텍스처 주입 — DT_UIVFXTexture 단일 진실 (WBP엔 빈 Image만 배치)
	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		TableMgr->ApplyUIVFXTexture(TEXT("CTAShine"), CTAShineImage);
	}

	UpdateHQInfo();
}

void UHQManagePanelWidget::NativeOnDeactivated()
{
	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	if (SortLevelBtn)    SortLevelBtn->OnPressed().RemoveAll(this);
	if (SortIncomeBtn)   SortIncomeBtn->OnPressed().RemoveAll(this);
	if (SortEmployeeBtn) SortEmployeeBtn->OnPressed().RemoveAll(this);

	if (LevelUpButton)
	{
		LevelUpButton->OnClicked().RemoveAll(this);
	}
	bCTAShineActive = false;

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UHQManagePanelWidget::OnCloseButtonClicked);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}

	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnHQLevelUp.RemoveDynamic(this, &UHQManagePanelWidget::HandleHQLevelUp);
	}

	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResMgr->OnResourceChanged.RemoveAll(this);
	}

	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeHireCompleted.RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		if (UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>())
		{
			StageMgr->OnStageComplete.RemoveDynamic(this, &UHQManagePanelWidget::HandleStageComplete);
		}
	}

	// 인계 중이 아닐 때만 원복 — 인계 중이면 UI 모드도 가림 고스트도 방금 연 관리 패널 소유다
	if (!bHandingOffToManagePanel)
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
		if (PC)
		{
			// [이동]에서 후임 관리 패널 몫으로 포커스를 등록하므로(OnBuildingMoveClicked), 인계가 아닌 닫힘이면 그 등록을 여기서 끈다
			if (APlayerCamera* Camera = Cast<APlayerCamera>(PC->GetPawn()))
			{
				Camera->ClearFocusTarget();
			}

			// UI 모드 복원 — BuildOpenWidget에서 GoToUIMode()로 전환했으므로 닫힐 때 되돌림
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}
	bHandingOffToManagePanel = false;

	Super::NativeOnDeactivated();
}

void UHQManagePanelWidget::OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
	{
		SM->PlayUISound(CGUISoundTags::TabSwitch);
	}

	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}

	// 빌딩 목록 탭 진입 시 갱신
	if (ButtonIndex == 1)
	{
		PopulateBuildingList();
	}
}

void UHQManagePanelWidget::UpdateHQInfo(bool bAnimateConditions)
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!SaveMgr || !TableMgr) return;

	CachedHQLevel = SaveMgr->GetHQLevel();

	// 다음 레벨 데이터
	const int32 NextLevel = CachedHQLevel + 1;
	bool bNextSuccess = false;
	FHQLevelData NextData = TableMgr->GetHQLevelData(NextLevel, bNextSuccess);

	// 현재 데이터 수집
	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return;

	const int32 BuildingCount = SaveData->GameData.Buildings.Num();

	int32 TotalEmployees = 0;
	int32 MaxStageCleared = 0;
	for (const auto& Pair : SaveData->GameData.OfficeDataMap)
	{
		TotalEmployees += Pair.Value.EmployeeList.Num();
		MaxStageCleared = FMath::Max(MaxStageCleared, Pair.Value.StageProgress.ProjectNumber);
	}

	int64 CurrentMoney = 0;
	int64 CurrentMarketCap = 0;
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		CurrentMoney = ResMgr->GetResourceAmount(EResourceType::Money);
		CurrentMarketCap = ResMgr->GetResourceAmount(EResourceType::MarketCap);
	}

	// 요구 티어 이상인 빌딩 수 — 조건이 없는 레벨(RequiredTier 0)이면 셀 필요도 없다
	int32 TierBuildingCount = 0;
	if (NextData.RequiredTier > 0)
	{
		if (USaveLoadManager* TierSaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			TierBuildingCount = TierSaveMgr->CountBuildingsAtTier(NextData.RequiredTier);
		}
	}

	// 경량 경로(RefreshAffordability)용 스냅샷 캐시
	bNextLevelAvailable = bNextSuccess;
	CachedNextData = NextData;
	CachedBuildingCount = BuildingCount;
	CachedTierBuildingCount = TierBuildingCount;
	CachedTotalEmployees = TotalEmployees;
	CachedMarketCap = CurrentMarketCap;

	// ===== 좌 컬럼: 메달 히어로 + Lv 전환 라인 =====
	if (LevelText)
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CachedHQLevel)));
	if (CurLevelText)
	{
		CurLevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), CachedHQLevel)));
		CurLevelText->SetVisibility(bNextSuccess ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TransArrowText)
		TransArrowText->SetVisibility(bNextSuccess ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NextLevelText)
	{
		NextLevelText->SetText(bNextSuccess
			? FText::FromString(FString::Printf(TEXT("Lv.%d"), NextLevel))
			: NSLOCTEXT("HQ", "HQMaxGrade", "최고 등급 달성"));
	}

	// ===== 현황 요약 행 — 라벨/색은 WBP 디자이너 값 그대로, 값만 갱신 =====
	if (Row_Money)
		Row_Money->SetStatValue(UGlobalUtilFunctions::AbbreviateNumber(CurrentMoney).ToString());
	if (Row_Buildings)
		Row_Buildings->SetStatValue(FString::Printf(TEXT("%d개"), BuildingCount));
	if (Row_Employees)
		Row_Employees->SetStatValue(FString::Printf(TEXT("%d명"), TotalEmployees));
	if (Row_TopStage)
		Row_TopStage->SetStatValue(FString::Printf(TEXT("%d"), MaxStageCleared));

	// ===== 레벨업 조건 체크리스트 =====
	// DT 값 0 = 조건 없음 → 행 Collapsed. "현재/목표" + 진행바/색은 StatRow가 자동 처리.
	auto SetIntCond = [bAnimateConditions](UStatRowWidget* Row, int32 Cur, int32 Target)
	{
		if (!Row) return;
		if (Target <= 0)
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Row->SetStatIcon(GetCondCheckIcon(Cur >= Target));
		Row->SetRowPlateVisible(Cur < Target); // 미충족 행만 밝은 플레이트로 부상 (v4)
		if (bAnimateConditions)
			Row->SetCurrentProgressAnimated(Cur, Target);
		else
			Row->SetCurrentProgress(Cur, Target);

		// StatRow 기본 색(빨강→주황→파랑)은 경고 톤이라 조건 행에선 블루/그린으로 덮어씀
		if (UProgressBar* Bar = Row->GetProgressBarWidget())
		{
			Bar->SetFillColorAndOpacity(GetCondBarColor(Target > 0 ? static_cast<float>(Cur) / Target : 1.f));
		}
	};

	if (bNextSuccess)
	{
		UpdateMoneyConditionRow(CurrentMoney);
		SetIntCond(Row_Cond_Buildings, BuildingCount, NextData.RequiredBuildingCount);
		SetIntCond(Row_Cond_Tier, TierBuildingCount, NextData.RequiredTierBuildingCount);
		SetIntCond(Row_Cond_Employees, TotalEmployees, NextData.RequiredEmployeeCount);
		if (Row_Cond_MarketCap)
		{
			// SetIntCond는 int32 — 시총은 int64(억~조 단위 가능)라 캐스트 시 INT32_MAX(21억) 초과분은 랩어라운드.
			// 2막 레벨 DT 값이 그 범위를 넘으면 SetIntCond를 int64/double 오버로드로 확장할 것(Money처럼 UpdateMoneyConditionRow 패턴 참고).
			SetIntCond(Row_Cond_MarketCap, static_cast<int32>(CurrentMarketCap), static_cast<int32>(NextData.RequiredMarketCap));
		}
	}
	else
	{
		if (Row_Cond_Money)         Row_Cond_Money->SetVisibility(ESlateVisibility::Collapsed);
		if (Row_Cond_Buildings)     Row_Cond_Buildings->SetVisibility(ESlateVisibility::Collapsed);
		if (Row_Cond_Tier)          Row_Cond_Tier->SetVisibility(ESlateVisibility::Collapsed);
		if (Row_Cond_Employees)     Row_Cond_Employees->SetVisibility(ESlateVisibility::Collapsed);
		if (Row_Cond_MarketCap)     Row_Cond_MarketCap->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ===== 보상 스트립 (조건 웰 하단) + 캔디 CTA 비용 =====
	if (bNextSuccess)
	{
		if (RewardTitleText)
		{
			RewardTitleText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d에 열리는 것"), NextLevel)));
			RewardTitleText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		const FText NextReward = BuildLevelRewardText(NextLevel);
		const bool bHasUnlock = !NextReward.IsEmpty();
		if (UnlockChipBox)
			UnlockChipBox->SetVisibility(bHasUnlock ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (UnlockText && bHasUnlock)
			UnlockText->SetText(NextReward);

		// Lv N+2 미래 칩 — 해금 내용은 가리고 존재만 예고 (욕망 그라데이션).
		// 이제 모든 레벨이 최소 건설 칸 +N 을 주므로 판정은 "행이 있는가"(=만렙 아닌가) 뿐이다.
		bool bFutureOk = false;
		TableMgr->GetHQLevelData(CachedHQLevel + 2, bFutureOk);
		const bool bHasFuture = bFutureOk;
		if (FutureChipBox)
			FutureChipBox->SetVisibility(bHasFuture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasFuture)
		{
			if (FutureText)
				FutureText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d ???"), CachedHQLevel + 2)));
			if (FutureCaptionText)
				FutureCaptionText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d에서 공개"), CachedHQLevel + 2)));
		}

		if (CostAmountText)
		{
			if (NextData.MoneyCost > 0)
			{
				CostAmountText->SetText(UGlobalUtilFunctions::AbbreviateNumber(NextData.MoneyCost, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil));
				CostAmountText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			else
			{
				CostAmountText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	else
	{
		if (RewardTitleText) RewardTitleText->SetVisibility(ESlateVisibility::Collapsed);
		if (UnlockChipBox)   UnlockChipBox->SetVisibility(ESlateVisibility::Collapsed);
		if (FutureChipBox)   FutureChipBox->SetVisibility(ESlateVisibility::Collapsed);
		if (CostAmountText)  CostAmountText->SetVisibility(ESlateVisibility::Collapsed);
		if (RemainText)      RemainText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UpdateOverallProgress(CurrentMoney);
	UpdateLevelUpButtonState();
}

void UHQManagePanelWidget::RefreshAffordability(int64 CurrentMoney)
{
	// 돈 의존 표시만 갱신 — OnResourceChanged는 고빈도라 전체 UpdateHQInfo 금지
	if (Row_Money)
		Row_Money->SetStatValue(UGlobalUtilFunctions::AbbreviateNumber(CurrentMoney).ToString());

	UpdateMoneyConditionRow(CurrentMoney);
	UpdateOverallProgress(CurrentMoney);
	UpdateLevelUpButtonState();
}

void UHQManagePanelWidget::UpdateMoneyConditionRow(int64 CurrentMoney)
{
	if (!Row_Cond_Money) return;

	if (!bNextLevelAvailable || CachedNextData.MoneyCost <= 0)
	{
		Row_Cond_Money->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Row_Cond_Money->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// int64 → float 정밀도 손실(±16M) 회피: 축약 문자열 + 바 직접 갱신. 보유=Floor / 비용=Ceil
	const int64 Cost = CachedNextData.MoneyCost;
	const bool bMet = CurrentMoney >= Cost;

	const FString CurStr = UGlobalUtilFunctions::AbbreviateNumber(CurrentMoney).ToString();
	const FString CostStr = UGlobalUtilFunctions::AbbreviateNumber(Cost, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil).ToString();

	// CondStatRow는 Current/Value 2-텍스트 구조 — Current를 안 채우면 WBP 기본값이 잔존하므로 직접 기입
	if (UCommonTextBlock* CurText = Cast<UCommonTextBlock>(Row_Cond_Money->GetWidgetFromName(TEXT("CurrentStatValueText"))))
	{
		CurText->SetText(FText::FromString(CurStr));
		Row_Cond_Money->SetStatValue(TEXT(" / ") + CostStr);
	}
	else
	{
		Row_Cond_Money->SetStatValue(CurStr + TEXT(" / ") + CostStr);
	}
	Row_Cond_Money->SetStatIcon(GetCondCheckIcon(bMet));
	Row_Cond_Money->SetRowPlateVisible(!bMet); // 미충족 행만 부상 (v4)

	if (UProgressBar* Bar = Row_Cond_Money->GetProgressBarWidget())
	{
		const float Raw = static_cast<float>(static_cast<double>(CurrentMoney) / static_cast<double>(Cost));
		Bar->SetPercent(FMath::Clamp(Raw, 0.f, 1.f));
		Bar->SetFillColorAndOpacity(GetCondBarColor(Raw));
	}
}

void UHQManagePanelWidget::UpdateOverallProgress(int64 CurrentMoney)
{
	const float Progress = bNextLevelAvailable ? ComputeOverallProgress(CurrentMoney) : 1.f;

	if (OverallProgressBar)
	{
		OverallProgressBar->SetPercent(Progress);
		// 레디(전 조건 충족 = 정규화 평균 1.0) = 골드 — 메달 골드와 호응 (v4)
		static const FLinearColor GoldFill(0.94f, 0.75f, 0.25f);
		OverallProgressBar->SetFillColorAndOpacity(
			Progress >= 1.f ? GoldFill : UStatRowWidget::GetProgressBarColor(Progress));
	}
	if (OverallProgressText)
	{
		OverallProgressText->SetText(bNextLevelAvailable
			? FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Progress * 100.f)))
			: NSLOCTEXT("HQ", "HQMaxLevel", "MAX"));
	}
}

float UHQManagePanelWidget::ComputeOverallProgress(int64 CurrentMoney) const
{
	// 활성 조건(목표>0)의 정규화 진행률 평균 — 이산 충족 카운트가 아니라 "거의 다 모은 돈"이 보이게
	float Sum = 0.f;
	int32 Count = 0;
	auto Acc = [&Sum, &Count](double Cur, double Target)
	{
		if (Target <= 0.0) return;
		Sum += static_cast<float>(FMath::Clamp(Cur / Target, 0.0, 1.0));
		++Count;
	};
	Acc(static_cast<double>(CurrentMoney), static_cast<double>(CachedNextData.MoneyCost));
	Acc(CachedBuildingCount, CachedNextData.RequiredBuildingCount);
	Acc(CachedTierBuildingCount, CachedNextData.RequiredTierBuildingCount);
	Acc(CachedTotalEmployees, CachedNextData.RequiredEmployeeCount);
	Acc(static_cast<double>(CachedMarketCap), static_cast<double>(CachedNextData.RequiredMarketCap));

	return Count > 0 ? Sum / Count : 1.f;
}

void UHQManagePanelWidget::UpdateLevelUpButtonState()
{
	if (!LevelUpButton) return;

	if (!bNextLevelAvailable)
	{
		LevelUpButton->SetVisibility(ESlateVisibility::Collapsed);
		bCTAShineActive = false;
		bMedalGlowActive = false;
		if (CTAShineImage)
		{
			CTAShineImage->SetRenderOpacity(0.f);
		}
		if (MedalGlowImage)
		{
			MedalGlowImage->SetRenderOpacity(0.f);
		}
		if (RemainText) RemainText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	const bool bCanLevelUp = SaveMgr && SaveMgr->CanLevelUpHQ();

	int64 CurrentMoney = 0;
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
		CurrentMoney = ResMgr->GetResourceAmount(EResourceType::Money);

	LevelUpButton->SetVisibility(ESlateVisibility::Visible);
	LevelUpButton->SetIsEnabled(bCanLevelUp);
	bCTAShineActive = bCanLevelUp;  // 레디 어포던스 = 샤인 스윕만 (펄스 금지 — v4)
	bMedalGlowActive = bCanLevelUp; // 메달 링 글로우도 같은 조건
	if (MedalGlowImage && !bCanLevelUp)
	{
		MedalGlowImage->SetRenderOpacity(0.f);
	}

	if (RemainText)
	{
		if (bCanLevelUp)
		{
			RemainText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			RemainText->SetText(FText::FromString(FString::Printf(TEXT("남은 조건 %d개"), ComputeUnmetCount(CurrentMoney))));
			RemainText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	// 버튼 내 비용 색 — 자금만 기준 (부족 시 빨강)
	if (CostAmountText && CachedNextData.MoneyCost > 0)
	{
		static const FLinearColor CostCream(0.913f, 0.863f, 0.738f); // #F5EFDF linear
		CostAmountText->SetColorAndOpacity(FSlateColor(
			CurrentMoney >= CachedNextData.MoneyCost ? CostCream : UStatRowWidget::ColorFail));
	}
}

int32 UHQManagePanelWidget::ComputeUnmetCount(int64 CurrentMoney) const
{
	int32 Unmet = 0;
	auto Check = [&Unmet](double Cur, double Target) { if (Target > 0.0 && Cur < Target) ++Unmet; };
	Check(static_cast<double>(CurrentMoney), static_cast<double>(CachedNextData.MoneyCost));
	Check(CachedBuildingCount, CachedNextData.RequiredBuildingCount);
	Check(CachedTierBuildingCount, CachedNextData.RequiredTierBuildingCount);
	Check(CachedTotalEmployees, CachedNextData.RequiredEmployeeCount);
	return Unmet;
}

void UHQManagePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 장식 VFX 전용 틱 — 대상 이미지가 WBP에 없으면 비용 0
	if (!CTAShineImage && !MedalGlowImage)
	{
		return;
	}
	VFXTimer += InDeltaTime;

	// 메달 골드 링 글로우 — 레디 동안 은은한 브리딩 (알파만, 스케일 펄스 아님)
	if (MedalGlowImage && bMedalGlowActive)
	{
		MedalGlowImage->SetRenderOpacity(0.30f + 0.16f * FMath::Sin(VFXTimer * 1.8f));
	}

	if (!CTAShineImage)
	{
		return;
	}

	// CTA 샤인 — 레벨업 가능 동안 주기 스윕 (펄스 금지 — v4)
	if (!bCTAShineActive)
	{
		CTAShineImage->SetRenderOpacity(0.f);
		return;
	}
	const float Phase = FMath::Fmod(VFXTimer, ShinePeriod);
	if (Phase < ShineTravel)
	{
		const float T = Phase / ShineTravel;
		CTAShineImage->SetRenderTranslation(FVector2D(FMath::Lerp(-ShineSweepHalf, ShineSweepHalf, T), 0.f));
		CTAShineImage->SetRenderOpacity(0.6f * FMath::Sin(T * PI)); // 진입/이탈 페이드
	}
	else
	{
		CTAShineImage->SetRenderOpacity(0.f);
	}
}

void UHQManagePanelWidget::PopulateBuildingList()
{
	if (!BuildingListScrollBox) return;
	BuildingListScrollBox->ClearChildren();

	UEntityManager* EntityMgr = GetWorld()->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> SlotClass = TableMgr->GetWidgetClass(EWidgetType::HQBuildingSlot);
	if (!SlotClass) return;

	TArray<ABuildingBaseActor*> Buildings;
	for (ABuildingBaseActor* Building : EntityMgr->GetBuildings())
	{
		if (Building) Buildings.Add(Building);
	}

	// TArray::Sort는 unstable — 동률은 BuildingIndex 오름차순으로 순서 고정
	const bool bAsc = bBuildingSortAscending;
	const EHQBuildingSortMode Mode = BuildingSortMode;
	UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>();
	UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	USaveLoadManager* TierSaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	Buildings.Sort([bAsc, Mode, OpMgr, EmpMgr, TierSaveMgr](const ABuildingBaseActor& A, const ABuildingBaseActor& B)
	{
		double KeyA = 0.0;
		double KeyB = 0.0;
		switch (Mode)
		{
		case EHQBuildingSortMode::Income:
		{
			// 카드 표시와 동일 기준 = 표시 순수익(감쇠O/진동X). 진동 스냅샷 정렬은 매초 순서가 흔들려 부적합
			KeyA = OpMgr ? OpMgr->GetBuildingDisplayNetPerSec(A.GetBuildingIndex()) : 0.0;
			KeyB = OpMgr ? OpMgr->GetBuildingDisplayNetPerSec(B.GetBuildingIndex()) : 0.0;
			break;
		}
		case EHQBuildingSortMode::Employees:
			KeyA = EmpMgr ? EmpMgr->GetEmployeeCountInBuilding(A.GetBuildingIndex()) : 0;
			KeyB = EmpMgr ? EmpMgr->GetEmployeeCountInBuilding(B.GetBuildingIndex()) : 0;
			break;
		case EHQBuildingSortMode::Level:
		default:
			KeyA = TierSaveMgr ? TierSaveMgr->GetBuildingTier(A.GetBuildingIndex()) : 0;
			KeyB = TierSaveMgr ? TierSaveMgr->GetBuildingTier(B.GetBuildingIndex()) : 0;
			break;
		}
		if (KeyA != KeyB)
		{
			return bAsc ? KeyA < KeyB : KeyA > KeyB;
		}
		return A.GetBuildingIndex() < B.GetBuildingIndex();
	});

	for (ABuildingBaseActor* Building : Buildings)
	{
		UBuildingSlotHorizonWidget* SlotWidget = CreateWidget<UBuildingSlotHorizonWidget>(this, SlotClass);
		if (!SlotWidget) continue;

		SlotWidget->SetBuildingData(Building);
		SlotWidget->OnMoveClicked.BindUObject(this, &UHQManagePanelWidget::OnBuildingMoveClicked);
		BuildingListScrollBox->AddChild(SlotWidget);

		// 카드 간 간격 — WBP 슬롯이 아니라 런타임 AddChild라 코드가 줘야 함
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(SlotWidget->Slot))
		{
			CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
	}
}

void UHQManagePanelWidget::SetupSortButtonGroup()
{
	SortButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	if (!SortButtonGroup) return;

	SortButtonGroup->SetSelectionRequired(true);

	// SetIsInteractableWhenSelected(true) 필수 — selected 버튼 재클릭 OnPressed 발화가 방향 토글의 전제 (ProductSellModal 선례)
	auto Setup = [this](UButtonWidget* Btn, void (UHQManagePanelWidget::*Handler)())
	{
		if (!Btn) return;
		Btn->SetIsSelectable(true);
		Btn->SetIsInteractableWhenSelected(true);
		SortButtonGroup->AddWidget(Btn);
		Btn->OnPressed().AddUObject(this, Handler);
	};
	Setup(SortLevelBtn,    &UHQManagePanelWidget::HandleSortLevelClicked);
	Setup(SortIncomeBtn,   &UHQManagePanelWidget::HandleSortIncomeClicked);
	Setup(SortEmployeeBtn, &UHQManagePanelWidget::HandleSortEmployeeClicked);
	SortButtonGroup->SelectButtonAtIndex(0);  // 레벨순 기본
	UpdateSortButtonTexts();
}

void UHQManagePanelWidget::HandleSortLevelClicked()
{
	if (BuildingSortMode == EHQBuildingSortMode::Level) { bBuildingSortAscending = !bBuildingSortAscending; }
	else { BuildingSortMode = EHQBuildingSortMode::Level; bBuildingSortAscending = false; }
	UpdateSortButtonTexts();
	PopulateBuildingList();
}

void UHQManagePanelWidget::HandleSortIncomeClicked()
{
	if (BuildingSortMode == EHQBuildingSortMode::Income) { bBuildingSortAscending = !bBuildingSortAscending; }
	else { BuildingSortMode = EHQBuildingSortMode::Income; bBuildingSortAscending = false; }
	UpdateSortButtonTexts();
	PopulateBuildingList();
}

void UHQManagePanelWidget::HandleSortEmployeeClicked()
{
	if (BuildingSortMode == EHQBuildingSortMode::Employees) { bBuildingSortAscending = !bBuildingSortAscending; }
	else { BuildingSortMode = EHQBuildingSortMode::Employees; bBuildingSortAscending = false; }
	UpdateSortButtonTexts();
	PopulateBuildingList();
}

void UHQManagePanelWidget::UpdateSortButtonTexts()
{
	// 오름차순 ▲ / 내림차순 ▼ — selected 버튼만 방향 표시 (ProductSellModal 컨벤션)
	auto ApplyText = [this](UButtonWidget* Btn, EHQBuildingSortMode Mode, const FString& Label)
	{
		if (!Btn) return;
		const FString Suffix = (BuildingSortMode == Mode)
			? (bBuildingSortAscending ? FString(TEXT(" ▲")) : FString(TEXT(" ▼")))
			: FString();
		Btn->SetButtonText(FText::FromString(Label + Suffix));
	};
	ApplyText(SortLevelBtn,    EHQBuildingSortMode::Level,     TEXT("레벨순"));
	ApplyText(SortIncomeBtn,   EHQBuildingSortMode::Income,    TEXT("수익순"));
	ApplyText(SortEmployeeBtn, EHQBuildingSortMode::Employees, TEXT("직원순"));
}

void UHQManagePanelWidget::OnBuildingMoveClicked(int32 BuildingIndex)
{
	UEntityManager* EntityMgr = GetWorld()->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(BuildingIndex);
	if (!Building) return;

	CloseWithAnimation();

	// 카메라를 해당 건물로 포커스
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		APlayerCamera* Camera = Cast<APlayerCamera>(PC->GetPawn());
		if (Camera)
		{
			Camera->FocusOnActor(Building, 0.35f);
		}
	}

	// 빌딩 관리 패널 열기
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager) return;

	TSubclassOf<UUserWidget> ManageClass = TableMgr->GetWidgetClass(EWidgetType::BuildingManage);
	UCommonActivatableWidget* Widget = UIManager->GetUIBase()->PushBottomClass(ManageClass.Get());

	if (UBuildingManagePanelWidget* ManagePanel = Cast<UBuildingManagePanelWidget>(Widget))
	{
		ManagePanel->SetTargetBuilding(Building);
		if (PC) PC->GoToUIMode();

		// 후임이 실제로 열렸을 때만 인계 표시 — push 실패 시엔 이 패널이 정상대로 모드/고스트를 원복해야 한다
		bHandingOffToManagePanel = true;
	}
}

void UHQManagePanelWidget::OnCloseButtonClicked()
{
	CloseWithAnimation();
}

void UHQManagePanelWidget::HandleHQLevelUp(int32 NewLevel)
{
	// 2차 연출 — 풀스크린 축하에서 복귀하면 조건행이 다음 레벨 기준으로 카운트업+펀치
	UpdateHQInfo(/*bAnimateConditions=*/true);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::RewardCompanyLevelUp);
		}
	}
}

void UHQManagePanelWidget::HandleStageComplete()
{
	UpdateHQInfo();
}

void UHQManagePanelWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type != EResourceType::Money) return;
	RefreshAffordability(NewValue);
}

void UHQManagePanelWidget::HandleEmployeeHireCompleted(const FString& EmployeeID)
{
	UpdateHQInfo();
}

FText UHQManagePanelWidget::BuildLevelRewardText(int32 Level) const
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return FText::GetEmpty();

	bool bOk = false;
	const FHQLevelData Data = TableMgr->GetHQLevelData(Level, bOk);
	if (!bOk) return FText::GetEmpty();

	// 건물 슬롯 보상 폐지(2026-08-14) — 이제 모든 레벨이 해금 하나를 갖는다(UnlockDescription 이 곧 보상).
	return Data.UnlockDescription;
}

void UHQManagePanelWidget::OnLevelUpButtonClicked()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!SaveMgr || !TableMgr) return;

	int32 OldLevel = SaveMgr->GetHQLevel();

	if (SaveMgr->TryLevelUpHQ())
	{
		int32 NewLevel = SaveMgr->GetHQLevel();

		const FText RewardText = BuildLevelRewardText(NewLevel);

		// 축하 위젯 표시
		TSubclassOf<UUserWidget> CelebClass = TableMgr->GetWidgetClass(EWidgetType::HQLevelUpCelebration);
		if (CelebClass)
		{
			UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
			if (UIMgr && UIMgr->GetUIBase())
			{
				UCommonActivatableWidget* Widget = UIMgr->GetUIBase()->PushPromptClass(CelebClass.Get());
				if (UHQLevelUpCelebrationWidget* Celeb = Cast<UHQLevelUpCelebrationWidget>(Widget))
				{
					Celeb->SetLevelUpInfo(OldLevel, NewLevel, RewardText);
				}
			}
		}
		// 패널 갱신은 OnHQLevelUp → HandleHQLevelUp(2차 연출 포함)이 처리
	}
	else
	{
		// 자금 부족 등으로 레벨업 실패 — '무반응' 방지 (claim의 이중 안전망)
		if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::PurchaseFail);
		}
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("HQ", "HQInsufficient", "자금이 부족합니다"), 2.0f, ENotificationType::Failed);
			if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
			{
				if (UResourceWidget* MoneyW = InGame->GetResourceWidget(EResourceType::Money))
				{
					MoneyW->PlayAffordShake();
				}
			}
		}
	}
}
