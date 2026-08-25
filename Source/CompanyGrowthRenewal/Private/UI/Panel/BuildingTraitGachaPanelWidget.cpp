// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/BuildingTraitGachaPanelWidget.h"
#include "Components/Image.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Panel/TraitGachaProbabilityWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "UI/UIBase.h"
#include "Enum/LootBoxRarity.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"

void UBuildingTraitGachaPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 저작 프리뷰(고정 35%)가 첫 갱신 전에 보이지 않도록 숨겨두고 시작
	UGlobalUtilFunctions::InitProgressHead(EpicPityHead);
	UGlobalUtilFunctions::InitProgressHead(LegendaryPityHead);
	UGlobalUtilFunctions::InitProgressHead(MileageHead);

	// 서브시스템 캐시
	if (UGameInstance* GI = GetGameInstance())
	{
		TraitManager = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
		SkinManager = GI->GetSubsystem<UBuildingSkinManagerSubsystem>();
		ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
		UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	}

	// 닫기 버튼 바인딩
	if (UIE_CloseButton && !UIE_CloseButton->OnCloseClicked.IsBound())
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UBuildingTraitGachaPanelWidget::OnCloseButtonClicked);
	}

	// 외부 카테고리 그룹 셋업 (특성 / 스킨, 스킨=비활성 스텁) — 그룹 셋업은 NativeConstruct에서만
	CategoryGroup = NewObject<UCommonButtonGroupBase>(this);
	CategoryGroup->SetSelectionRequired(true);
	CategoryGroup->AddWidget(CategoryTraitBtn);
	CategoryGroup->AddWidget(CategorySkinBtn);
	// 스킨 가챠 구현됨 — 더 이상 비활성 스텁 아님(클릭 시 OnCategorySelected 가 스킨 모드로 전환)
	// OnButtonBaseClicked = 매 클릭 발동(탭이 selectable 아니어도). OnSelectedButtonBaseChanged 는
	// 선택이 실제로 바뀔 때만 발동 → 탭 버튼이 selectable 아니면 침묵해서 토글이 안 먹음.
	CategoryGroup->OnButtonBaseClicked.AddDynamic(this, &UBuildingTraitGachaPanelWidget::OnCategorySelected);
	CategoryGroup->SelectButtonAtIndex(0);

	// 내부 배너 그룹 셋업 (일반 / 고급)
	BannerGroup = NewObject<UCommonButtonGroupBase>(this);
	BannerGroup->SetSelectionRequired(true);
	BannerGroup->AddWidget(BannerNormal);
	BannerGroup->AddWidget(BannerAdvanced);
	BannerGroup->OnButtonBaseClicked.AddDynamic(this, &UBuildingTraitGachaPanelWidget::OnBannerSelected);
	BannerGroup->SelectButtonAtIndex(0);
	bAdvancedSelected = false;

	// CTA + 링크 버튼 바인딩
	if (PullButton)
	{
		PullButton->OnClicked().RemoveAll(this);
		PullButton->OnClicked().AddUObject(this, &UBuildingTraitGachaPanelWidget::OnPullClicked);
	}
	if (PullButton10)
	{
		PullButton10->OnClicked().RemoveAll(this);
		PullButton10->OnClicked().AddUObject(this, &UBuildingTraitGachaPanelWidget::OnPull10Clicked);
	}
	if (MileageExchangeButton)
	{
		MileageExchangeButton->OnClicked().RemoveAll(this);
		MileageExchangeButton->OnClicked().AddUObject(this, &UBuildingTraitGachaPanelWidget::OnMileageExchangeClicked);
	}
	if (ProbabilityInfoButton)
	{
		ProbabilityInfoButton->OnClicked().RemoveAll(this);
		ProbabilityInfoButton->OnClicked().AddUObject(this, &UBuildingTraitGachaPanelWidget::OnProbabilityInfoClicked);
	}

	UE_LOG(LogTemp, Log, TEXT("[TraitGachaPanel] NativeConstruct - TraitMgr: %s, ItemMgr: %s"),
		TraitManager ? TEXT("Valid") : TEXT("Null"),
		ItemMgr ? TEXT("Valid") : TEXT("Null"));
}

void UBuildingTraitGachaPanelWidget::NativeDestruct()
{
	// 닫기 버튼 해제
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}

	// 그룹 해제 (양쪽)
	if (CategoryGroup)
	{
		CategoryGroup->OnButtonBaseClicked.RemoveAll(this);
	}
	if (BannerGroup)
	{
		BannerGroup->OnButtonBaseClicked.RemoveAll(this);
	}

	// CTA + 링크 버튼 해제
	if (PullButton) PullButton->OnClicked().RemoveAll(this);
	if (MileageExchangeButton) MileageExchangeButton->OnClicked().RemoveAll(this);
	if (ProbabilityInfoButton) ProbabilityInfoButton->OnClicked().RemoveAll(this);

	Super::NativeDestruct();
}

void UBuildingTraitGachaPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 매니저 델리게이트 구독 (오버레이 재뽑기로 게이지/티켓이 바뀌면 동기화)
	if (TraitManager)
	{
		OnTraitGachaCompletedHandle = TraitManager->OnTraitGachaCompleted.AddUObject(
			this, &UBuildingTraitGachaPanelWidget::HandleTraitGachaCompleted);
	}
	// 양쪽 매니저 모두 구독 — 핸들러가 현재 카테고리 기준으로 게이지/CTA 를 갱신하므로 안전.
	if (SkinManager)
	{
		OnSkinGachaCompletedHandle = SkinManager->OnSkinGachaCompleted.AddUObject(
			this, &UBuildingTraitGachaPanelWidget::HandleSkinGachaCompleted);
	}
	if (ItemMgr)
	{
		OnItemChangedHandle = ItemMgr->OnItemChanged.AddUObject(
			this, &UBuildingTraitGachaPanelWidget::HandleItemChanged);
	}

	// 그룹 상태는 건드리지 않고 데이터만 갱신
	UpdateHeroAndProbability();
	UpdateGauges();
	UpdateCTALabel();

	// M11 — 가챠 패널 열림 신호 + [뽑기] 하이라이트 타겟 등록
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->RegisterTraitGachaPanel(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[TraitGachaPanel] NativeOnActivated - BuildingIndex: %d"), ContextBuildingIndex);
}

void UBuildingTraitGachaPanelWidget::NativeOnDeactivated()
{
	// 매니저 델리게이트 해제
	if (TraitManager)
	{
		TraitManager->OnTraitGachaCompleted.Remove(OnTraitGachaCompletedHandle);
	}
	if (SkinManager)
	{
		SkinManager->OnSkinGachaCompleted.Remove(OnSkinGachaCompletedHandle);
	}
	if (ItemMgr)
	{
		ItemMgr->OnItemChanged.Remove(OnItemChangedHandle);
	}

	// M11 — 가챠 패널 닫힘 → [뽑기] 하이라이트 타겟 해제
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->UnregisterTraitGachaPanel(this);
	}

	// MainMap에서 PushPromptClass로 열렸으므로 닫힐 때 Normal 모드로 복원 (BuildingManagePanel 패턴)
	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

	Super::NativeOnDeactivated();

	UE_LOG(LogTemp, Log, TEXT("[TraitGachaPanel] NativeOnDeactivated"));
}

UWidget* UBuildingTraitGachaPanelWidget::GetPullButtonWidget() const
{
	return PullButton;
}

UWidget* UBuildingTraitGachaPanelWidget::GetSkinCategoryButtonWidget() const
{
	return CategorySkinBtn;
}

UWidget* UBuildingTraitGachaPanelWidget::GetCloseButtonWidget() const
{
	return UIE_CloseButton;
}

// ========== 컨텍스트 ==========

void UBuildingTraitGachaPanelWidget::SetContext(int32 BuildingIndex, bool bOpenTraitTab)
{
	ContextBuildingIndex = BuildingIndex;

	// 특성 탭(인덱스 0) 유지. 그룹 셋업은 NativeConstruct에서만 하므로 여기선 인덱스 보장만 필요 시 처리.
	if (bOpenTraitTab && CategoryGroup)
	{
		CategoryGroup->SelectButtonAtIndex(0);
	}

	UE_LOG(LogTemp, Log, TEXT("[TraitGachaPanel] SetContext - BuildingIndex: %d, OpenTraitTab: %d"),
		BuildingIndex, bOpenTraitTab ? 1 : 0);
}

// ========== UI 갱신 함수 ==========

void UBuildingTraitGachaPanelWidget::UpdateHeroAndProbability()
{
	// 쇼케이스 카드 스위처: 특성(0) / 스킨(1)
	if (HeroCardSwitcher)
	{
		HeroCardSwitcher->SetActiveWidgetIndex(bIsSkinCategory ? 1 : 0);
	}

	if (HeroTitleText)
	{
		const TCHAR* Category = bIsSkinCategory ? TEXT("스킨") : TEXT("특성");
		const TCHAR* Tier = bAdvancedSelected ? TEXT("고급") : TEXT("일반");
		HeroTitleText->SetText(FText::FromString(FString::Printf(TEXT("%s %s 뽑기"), Tier, Category)));
	}

	// 설명도 카테고리마다 갈린다 — 코드가 안 채우면 WBP baked 특성 문구가 스킨 탭에 그대로 남는다
	if (HeroDescText)
	{
		HeroDescText->SetText(FText::FromString(bIsSkinCategory
			? TEXT("건물 외관을 바꾸는 스킨을 획득합니다. 60회에 에픽, 150회에 레전드가 확정됩니다")
			: TEXT("건물에 장착할 특성을 획득합니다. 60회에 에픽, 150회에 레전드가 확정됩니다")));
	}

	if (ProbabilityPillRow)
	{
		ProbabilityPillRow->ClearChildren();
		const TArray<FGachaRarityChance> Rows = bIsSkinCategory
			? (SkinManager ? SkinManager->GetSkinProbabilityTableForUI(bAdvancedSelected) : TArray<FGachaRarityChance>())
			: (TraitManager ? TraitManager->GetTraitProbabilityTableForUI(bAdvancedSelected) : TArray<FGachaRarityChance>());
		for (const FGachaRarityChance& Row : Rows)
		{
			UTextBlock* Pill = NewObject<UTextBlock>(this);
			if (PillFont.FontObject) { Pill->SetFont(PillFont); }
			Pill->SetText(FText::FromString(FString::Printf(TEXT("%s %.0f%%"),
				*FLootBoxRarityUtility::GetKoreanName(Row.Rarity), Row.Percent)));
			Pill->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityColor(Row.Rarity)));
			ProbabilityPillRow->AddChildToHorizontalBox(Pill);
		}
	}
}

void UBuildingTraitGachaPanelWidget::UpdateGauges()
{
	// 특성/스킨 천장·마일리지 상수는 동일(60/150/50). 값만 활성 카테고리 매니저에서 읽음.
	if (bIsSkinCategory && !SkinManager) { return; }
	if (!bIsSkinCategory && !TraitManager) { return; }

	const int32 EpicPity = bIsSkinCategory
		? SkinManager->GetPullsSinceEpic(bAdvancedSelected)
		: TraitManager->GetPullsSinceEpic(bAdvancedSelected);
	const int32 LgdPity = bIsSkinCategory
		? SkinManager->GetPullsSinceLegendary(bAdvancedSelected)
		: TraitManager->GetPullsSinceLegendary(bAdvancedSelected);
	const int32 Mileage = bIsSkinCategory
		? SkinManager->GetMileagePoints()
		: TraitManager->GetMileagePoints();

	// Epic 하드 천장 (60)
	if (EpicPityLabel)
	{
		EpicPityLabel->SetText(FText::FromString(FString::Printf(TEXT("%s 천장 %d/%d"),
			*FLootBoxRarityUtility::GetKoreanName(ELootBoxRarity::Epic), EpicPity, FBuildingTraitPityData::HardPity)));
	}
	if (EpicPityBar)
	{
		const float EpicPct = FMath::Clamp((float)EpicPity / (float)FBuildingTraitPityData::HardPity, 0.f, 1.f);
		EpicPityBar->SetPercent(EpicPct);
		UGlobalUtilFunctions::UpdateProgressHead(EpicPityBar, EpicPityHead, EpicPct);
	}

	// Legendary 대천장 (150)
	if (LegendaryPityLabel)
	{
		LegendaryPityLabel->SetText(FText::FromString(FString::Printf(TEXT("%s 천장 %d/%d"),
			*FLootBoxRarityUtility::GetKoreanName(ELootBoxRarity::Legendary), LgdPity, FBuildingTraitPityData::GrandPity)));
	}
	if (LegendaryPityBar)
	{
		const float LgdPct = FMath::Clamp((float)LgdPity / (float)FBuildingTraitPityData::GrandPity, 0.f, 1.f);
		LegendaryPityBar->SetPercent(LgdPct);
		UGlobalUtilFunctions::UpdateProgressHead(LegendaryPityBar, LegendaryPityHead, LgdPct);
	}

	// 마일리지 (합산, 150pt = Legendary 상자)
	if (MileageLabel)
	{
		MileageLabel->SetText(FText::FromString(FString::Printf(TEXT("마일리지 %d/%d"),
			Mileage, FGachaTraitData::MileageLegendaryBox)));
	}
	if (MileageBar)
	{
		const float MileagePct = FMath::Clamp((float)Mileage / (float)FGachaTraitData::MileageLegendaryBox, 0.f, 1.f);
		MileageBar->SetPercent(MileagePct);
		UGlobalUtilFunctions::UpdateProgressHead(MileageBar, MileageHead, MileagePct);
	}
	// 교환 버튼은 Epic 상자(50pt) 이상이면 활성
	if (MileageExchangeButton)
	{
		MileageExchangeButton->SetIsEnabled(Mileage >= FGachaTraitData::MileageEpicBox);
	}
}

void UBuildingTraitGachaPanelWidget::UpdateCTALabel()
{
	if (!ItemMgr || !PullButton) { return; }

	const EItemType Ticket = bIsSkinCategory
		? (bAdvancedSelected ? EItemType::SkinTicketAdvanced : EItemType::SkinTicketNormal)
		: (bAdvancedSelected ? EItemType::BuildingTraitTicketAdvanced : EItemType::BuildingTraitTicketNormal);

	const int32 TicketCount = ItemMgr->GetItemCount(Ticket);

	// 종류(특성=기어/스킨=붓)·티어(색)는 좌측 아이콘이, 보유 개수는 상단 티켓 칩이 표시 — 버튼엔 액션만. 가챠는 티켓 전용(다이아 폴백 없음).
	const bool bHasOne = TicketCount > 0;
	PullButton->SetButtonText(FText::FromString(bHasOne ? FString(TEXT("×1 뽑기")) : FString(TEXT("뽑기권 부족"))));
	PullButton->SetIsEnabled(bHasOne);
	PullButton->SetCount(0);   // 별도 카운트 칸 숨김 — 비용은 ButtonText 한 줄로 통일

	if (PullButton10)
	{
		// 10연 = 티켓 10장 (OnPull10Clicked 의 HasItem(Ticket,10) 과 동일 게이팅). "10연차" 표기는 연차(휴가) 오독으로 폐기.
		PullButton10->SetButtonText(FText::FromString(TEXT("×10 뽑기")));
		PullButton10->SetIsEnabled(TicketCount >= 10);
		PullButton10->SetCount(0);   // 별도 카운트 칸 숨김
	}

	// 좌측 아이콘 = 현재 카테고리/티어 뽑기권. 1x·10x 동일 티켓.
	if (TableMgr)
	{
		bool bIconFound = false;
		UTexture2D* TicketIcon = TableMgr->GetItemIcon(Ticket, bIconFound);
		UTexture2D* IconToSet = bIconFound ? TicketIcon : nullptr;
		PullButton->SetIcon(IconToSet);
		if (PullButton10) { PullButton10->SetIcon(IconToSet); }
	}

	UpdateTicketCounts();
}

void UBuildingTraitGachaPanelWidget::UpdateTicketCounts()
{
	if (!ItemMgr) { return; }

	// 배너 2개 = 현재 카테고리(특성/스킨)의 일반/고급 뽑기권. 카테고리 토글 시 같은 배너에 다른 티켓 수가 들어간다
	const EItemType TNormal = bIsSkinCategory ? EItemType::SkinTicketNormal : EItemType::BuildingTraitTicketNormal;
	const EItemType TAdvanced = bIsSkinCategory ? EItemType::SkinTicketAdvanced : EItemType::BuildingTraitTicketAdvanced;

	// 배너 BindWidget 타입은 UCommonButtonBase 계약 유지 — 실제 위젯만 캐스팅해서 수량을 넣는다
	auto SetBannerCount = [this](UCommonButtonBase* Banner, EItemType Ticket)
	{
		if (UGachaBannerButton* B = Cast<UGachaBannerButton>(Banner))
		{
			B->SetOwnedCount(ItemMgr->GetItemCount(Ticket));
		}
	};

	SetBannerCount(BannerNormal, TNormal);
	SetBannerCount(BannerAdvanced, TAdvanced);
}

// ========== 델리게이트 핸들러 ==========

void UBuildingTraitGachaPanelWidget::HandleTraitGachaCompleted(const FBuildingTraitGachaResult& Result)
{
	// 오버레이 재뽑기로 천장/마일리지/티켓이 바뀌면 패널 다시 동기화
	UpdateGauges();
	UpdateCTALabel();
}

void UBuildingTraitGachaPanelWidget::HandleSkinGachaCompleted(const FBuildingSkinGachaResult& Result)
{
	// 스킨 오버레이 재뽑기 동기화 (게이지/CTA 는 현재 카테고리 기준으로 갱신)
	UpdateGauges();
	UpdateCTALabel();
}

void UBuildingTraitGachaPanelWidget::HandleItemChanged(EItemType ItemType, int32 NewCount, int32 Delta)
{
	if (ItemType == EItemType::BuildingTraitTicketNormal
		|| ItemType == EItemType::BuildingTraitTicketAdvanced
		|| ItemType == EItemType::SkinTicketNormal
		|| ItemType == EItemType::SkinTicketAdvanced)
	{
		UpdateCTALabel();
	}
}

// ========== 버튼 이벤트 ==========

void UBuildingTraitGachaPanelWidget::OnCategorySelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	// 인덱스 0=특성, 1=스킨. 클릭 경로(OnButtonBaseClicked)라 선택 하이라이트는 수동 동기화.
	if (CategoryGroup) { CategoryGroup->SelectButtonAtIndex(ButtonIndex); }

	const bool bNewSkin = (ButtonIndex == 1);
	if (bNewSkin == bIsSkinCategory) { return; } // 동일 카테고리 재클릭 → no-op
	bIsSkinCategory = bNewSkin;

	// 카테고리 전환 시 배너는 일반(0)으로 리셋 — 새 카테고리의 천장/티켓 기준으로 다시 보여줌
	bAdvancedSelected = false;
	if (BannerGroup) { BannerGroup->SelectButtonAtIndex(0); }

	UpdateHeroAndProbability();
	UpdateGauges();
	UpdateCTALabel();
}

void UBuildingTraitGachaPanelWidget::OnBannerSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	bAdvancedSelected = (ButtonIndex == 1);
	// 클릭 핸들러(OnButtonBaseClicked) 경유라 선택 하이라이트는 수동 동기화. 우리 핸들러는
	// 선택 변경 델리게이트에 안 묶여 있어 재진입 없음.
	if (BannerGroup) { BannerGroup->SelectButtonAtIndex(ButtonIndex); }
	UpdateHeroAndProbability();
	UpdateGauges();
	UpdateCTALabel();
}

void UBuildingTraitGachaPanelWidget::OnPullClicked()
{
	// ===== 스킨 카테고리 경로 =====
	if (bIsSkinCategory)
	{
		if (!SkinManager) { return; }
		if (!SkinManager->CanExecuteGachaPull(bAdvancedSelected))
		{
			if (UIMgr) { UIMgr->ShowNotification(NSLOCTEXT("SkinGacha", "NoTicket", "뽑기권이 부족합니다"), 3.0f, ENotificationType::Failed); }
			return;
		}
		FBuildingSkinGachaResult SkinResult;
		if (SkinManager->ExecuteGachaPull(bAdvancedSelected, SkinResult))
		{
			if (UGachaRevealPresentationWidget* Pres = CreateRevealWidget())
			{
				Pres->SetupPullSkin({ SkinResult }, bAdvancedSelected);
			}
		}
		return;
	}

	// ===== 특성 카테고리 경로 =====
	if (!TraitManager) { return; }
	if (!TraitManager->CanExecuteGachaPull(bAdvancedSelected))
	{
		if (UIMgr) { UIMgr->ShowNotification(NSLOCTEXT("TraitGacha", "NoTicket", "뽑기권이 부족합니다"), 3.0f, ENotificationType::Failed); }
		return;
	}

	FBuildingTraitGachaResult Result;
	if (TraitManager->ExecuteGachaPull(bAdvancedSelected, Result))
	{
		if (UGachaRevealPresentationWidget* Pres = CreateRevealWidget())
		{
			Pres->SetupPullTrait({ Result }, bAdvancedSelected);
		}
	}
}

void UBuildingTraitGachaPanelWidget::OnPull10Clicked()
{
	// ===== 스킨 카테고리 경로 =====
	if (bIsSkinCategory)
	{
		if (!SkinManager) { return; }
		const EItemType SkinTicket = bAdvancedSelected ? EItemType::SkinTicketAdvanced : EItemType::SkinTicketNormal;
		if (!ItemMgr || !ItemMgr->HasItem(SkinTicket, 10))
		{
			if (UIMgr) { UIMgr->ShowNotification(NSLOCTEXT("SkinGacha", "NoTicket10", "뽑기권이 부족합니다 (10장 필요)"), 3.0f, ENotificationType::Failed); }
			return;
		}
		TArray<FBuildingSkinGachaResult> SkinResults;
		if (SkinManager->ExecuteGachaPullMulti(bAdvancedSelected, 10, SkinResults) && SkinResults.Num() > 0)
		{
			if (UGachaRevealPresentationWidget* Pres = CreateRevealWidget())
			{
				Pres->SetupPullSkin(SkinResults, bAdvancedSelected);
			}
		}
		return;
	}

	// ===== 특성 카테고리 경로 =====
	if (!TraitManager) { return; }

	const EItemType Ticket = bAdvancedSelected
		? EItemType::BuildingTraitTicketAdvanced
		: EItemType::BuildingTraitTicketNormal;
	if (!ItemMgr || !ItemMgr->HasItem(Ticket, 10))
	{
		if (UIMgr) { UIMgr->ShowNotification(NSLOCTEXT("TraitGacha", "NoTicket10", "뽑기권이 부족합니다 (10장 필요)"), 3.0f, ENotificationType::Failed); }
		return;
	}

	TArray<FBuildingTraitGachaResult> Results;
	if (TraitManager->ExecuteGachaPullMulti(bAdvancedSelected, 10, Results) && Results.Num() > 0)
	{
		if (UGachaRevealPresentationWidget* Pres = CreateRevealWidget())
		{
			Pres->SetupPullTrait(Results, bAdvancedSelected);
		}
	}
}

UGachaRevealPresentationWidget* UBuildingTraitGachaPanelWidget::CreateRevealWidget()
{
	// 특성/스킨 공용 연출 위젯 생성 + 패널 위 뷰포트 오버레이로 표시(같은 스택 push면 패널 deactivate→가려짐)
	if (!TableMgr) { return nullptr; }
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::GachaRevealPresentation);
	if (!Cls) { return nullptr; }
	APlayerController* PC = GetOwningPlayer();
	if (!PC) { return nullptr; }
	UGachaRevealPresentationWidget* Pres = CreateWidget<UGachaRevealPresentationWidget>(PC, Cls.Get());
	if (Pres) { Pres->AddToViewport(1000); }
	return Pres;
}

void UBuildingTraitGachaPanelWidget::OnMileageExchangeClicked()
{
	// STUB — 마일리지 상자 교환 picker(상자 종류 + 트레이트 선택)는 분해 상점 UI와 함께 후속
	if (UIMgr)
	{
		UIMgr->ShowNotification(NSLOCTEXT("TraitGacha", "MileageSoon", "마일리지 상자 교환은 후속 업데이트에서 제공됩니다"), 3.0f, ENotificationType::Warning);
	}
}

void UBuildingTraitGachaPanelWidget::OnProbabilityInfoClicked()
{
	if (!TableMgr) { return; }
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::TraitGachaProbabilityPanel);
	if (!Cls) { return; }

	// PushPromptClass(PromptStack)로 띄우면 같은 스택의 가챠 패널이 deactivate→가려짐.
	// 확률 정보는 패널 '위에' 떠야 하므로 뷰포트 오버레이로(UIBase 전체 위, ZOrder 큼). 닫기=RemoveFromParent.
	APlayerController* PC = GetOwningPlayer();
	if (!PC) { return; }
	if (UTraitGachaProbabilityWidget* Prob = CreateWidget<UTraitGachaProbabilityWidget>(PC, Cls.Get()))
	{
		// AddToViewport(=NativeConstruct) 전에 축을 넣어야 첫 빌드부터 현재 카테고리 표가 나온다
		Prob->SetCategory(bIsSkinCategory);
		Prob->AddToViewport(1000);
	}
}

void UBuildingTraitGachaPanelWidget::OnCloseButtonClicked()
{
	// PushPromptClass로 열리므로 DeactivateWidget으로 닫기
	DeactivateWidget();
}
