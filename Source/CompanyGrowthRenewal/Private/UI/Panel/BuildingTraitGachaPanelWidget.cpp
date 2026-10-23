// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/BuildingTraitGachaPanelWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "UI/Element/Gacha/GachaPanelFrameWidget.h"
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
#include "Enum/LootBoxRarity.h"
#include "Table/ShopItemTable.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"
#include "Manager/SaveLoadManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

namespace
{
	TArray<int32> RevealGradeWeights(const TArray<FGachaRarityChance>& Chances)
	{
		TArray<int32> Weights;
		Weights.Init(0, static_cast<int32>(ELootBoxRarity::Mythic) + 1);
		for (const FGachaRarityChance& Chance : Chances)
		{
			const int32 Grade = static_cast<int32>(Chance.Rarity);
			if (Weights.IsValidIndex(Grade)) { Weights[Grade] = FMath::Max(0, FMath::RoundToInt(Chance.Percent * 10000.f)); }
		}
		return Weights;
	}

	bool TryGetRevealLaunch(UWidget* Hero, FVector2D LayerSize, UWidget* Context,
		FVector2D& OutPos, float& OutScale)
	{
		if (!Hero || LayerSize.X <= 0.f || !Context) { return false; }
		const FGeometry HeroGeometry = Hero->GetCachedGeometry();
		const FGeometry ViewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Context);
		if (HeroGeometry.GetLocalSize().X <= 0.f || ViewportGeometry.GetLocalSize().X <= 0.f) { return false; }
		const FVector2D HeroCenter = HeroGeometry.LocalToAbsolute(HeroGeometry.GetLocalSize() * .5f);
		const FVector2D HeroRight = HeroGeometry.LocalToAbsolute(FVector2D(HeroGeometry.GetLocalSize().X, HeroGeometry.GetLocalSize().Y * .5f));
		OutPos = ViewportGeometry.AbsoluteToLocal(HeroCenter);
		const float LaunchWidth = (ViewportGeometry.AbsoluteToLocal(HeroRight) - OutPos).Size() * 2.f;
		if (LaunchWidth <= 0.f) { return false; }
		OutScale = LaunchWidth / LayerSize.X;
		return true;
	}
}

void UBuildingTraitGachaPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 저작 프리뷰(고정 35%)가 첫 갱신 전에 보이지 않도록 숨겨두고 시작
	UGlobalUtilFunctions::InitProgressHead(EpicPityHead);
	UGlobalUtilFunctions::InitProgressHead(LegendaryPityHead);

	if (UGameInstance* GI = GetGameInstance())
	{
		TraitManager = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
		SkinManager = GI->GetSubsystem<UBuildingSkinManagerSubsystem>();
	}

	// 외부 카테고리 그룹 셋업 (특성 / 스킨) — 그룹 셋업은 NativeConstruct에서만
	CategoryGroup = NewObject<UCommonButtonGroupBase>(this);
	CategoryGroup->SetSelectionRequired(true);
	CategoryGroup->AddWidget(CategoryTraitBtn);
	CategoryGroup->AddWidget(CategorySkinBtn);
	// OnButtonBaseClicked = 매 클릭 발동(탭이 selectable 아니어도). OnSelectedButtonBaseChanged 는
	// 선택이 실제로 바뀔 때만 발동 → 탭 버튼이 selectable 아니면 침묵해서 토글이 안 먹음.
	CategoryGroup->OnButtonBaseClicked.AddDynamic(this, &UBuildingTraitGachaPanelWidget::OnCategorySelected);
	// 풀링된 패널은 마지막 카테고리를 기억하므로 하이라이트도 그쪽으로 맞춘다(클릭이 아니라 무음)
	CategoryGroup->SelectButtonAtIndex(bIsSkinCategory ? 1 : 0, /*bAllowSound*/ false);

	MakeBannerGroup({ BannerNormal, BannerAdvanced });

	if (PullButton10)
	{
		PullButton10->OnClicked().RemoveAll(this);
		PullButton10->OnClicked().AddUObject(this, &UBuildingTraitGachaPanelWidget::OnPull10Clicked);
	}
	if (PullButton30)
	{
		PullButton30->OnClicked().RemoveAll(this);
		PullButton30->OnClicked().AddUObject(this, &UBuildingTraitGachaPanelWidget::OnPull30Clicked);
	}

	UE_LOG(LogTemp, Log, TEXT("[TraitGachaPanel] NativeConstruct - TraitMgr: %s, ItemMgr: %s"),
		TraitManager ? TEXT("Valid") : TEXT("Null"),
		ItemMgr ? TEXT("Valid") : TEXT("Null"));
}

UWidget* UBuildingTraitGachaPanelWidget::GetTraitCategoryTabWidget() const
{
	return CategoryTraitBtn;
}

UWidget* UBuildingTraitGachaPanelWidget::GetSkinCategoryTabWidget() const
{
	return CategorySkinBtn;
}

UWidget* UBuildingTraitGachaPanelWidget::GetNormalBannerWidget() const
{
	return BannerNormal;
}

void UBuildingTraitGachaPanelWidget::NativeDestruct()
{
	if (CategoryGroup)
	{
		CategoryGroup->OnButtonBaseClicked.RemoveAll(this);
		// 선택 필수인 채로 비우면 엔진이 남은 탭을 재선택한다(클릭음 또는 ensure)
		CategoryGroup->SetSelectionRequired(false);
		// 재구성 때 새 그룹이 같은 탭을 받으므로 옛 그룹의 탭 바인딩을 끊는다
		CategoryGroup->RemoveAll();
		CategoryGroup = nullptr;
	}
	if (PullButton10) PullButton10->OnClicked().RemoveAll(this);
	if (PullButton30) PullButton30->OnClicked().RemoveAll(this);

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
	RefreshAll();

	// M11 — 가챠 패널 열림 신호 + [뽑기] 하이라이트 타겟 등록
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->RegisterTraitGachaPanel(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[TraitGachaPanel] NativeOnActivated"));
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

// ========== UI 갱신 함수 ==========

void UBuildingTraitGachaPanelWidget::UpdateHeroAndProbability()
{
	UpdateBannerPresentation(BannerNormal, bIsSkinCategory ? EItemType::SkinTicketNormal : EItemType::BuildingTraitTicketNormal);
	UpdateBannerPresentation(BannerAdvanced, bIsSkinCategory ? EItemType::SkinTicketAdvanced : EItemType::BuildingTraitTicketAdvanced);

	if (GachaFrame)
	{
		GachaFrame->SetTitle(FText::FromString(bIsSkinCategory ? TEXT("스킨 뽑기") : TEXT("특성 뽑기")));
	}

	bool bBoxHeroVisible = false;
	if (HeroBoxImage && TableMgr)
	{
		bool bIconFound = false;
		UObject* BoxIcon = TableMgr->GetItemIconResource(GetActiveTicketType(), bIconFound);
		HeroBoxImage->SetBrushResourceObject(bIconFound ? BoxIcon : nullptr);
		bBoxHeroVisible = bIconFound && BoxIcon;
		HeroBoxImage->SetVisibility(bBoxHeroVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* HeroBoxHost = GetWidgetFromName(TEXT("HeroBoxHost")))
	{
		HeroBoxHost->SetVisibility(bBoxHeroVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 쇼케이스 카드 스위처: 특성(0) / 스킨(1)
	if (HeroCardSwitcher)
	{
		HeroCardSwitcher->SetActiveWidgetIndex(bIsSkinCategory ? 1 : 0);
		HeroCardSwitcher->SetVisibility(bBoxHeroVisible ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
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

	RebuildProbabilityPills(bIsSkinCategory
		? (SkinManager ? SkinManager->GetSkinProbabilityTableForUI(bAdvancedSelected) : TArray<FGachaRarityChance>())
		: (TraitManager ? TraitManager->GetTraitProbabilityTableForUI(bAdvancedSelected) : TArray<FGachaRarityChance>()));
}

void UBuildingTraitGachaPanelWidget::UpdateBannerPresentation(UCommonButtonBase* Banner, EItemType Ticket)
{
	UGachaBannerButton* GachaBanner = Cast<UGachaBannerButton>(Banner);
	if (!GachaBanner) { return; }

	FShopItemTable Row;
	if (!TableMgr || !TableMgr->GetShopItemByItemType(Ticket, Row))
	{
		GachaBanner->SetBannerPresentation(FText::GetEmpty(), nullptr);
		return;
	}

	FString BannerName = Row.DisplayName.ToString();
	BannerName.RemoveFromEnd(TEXT(" 상자"));
	UObject* IconResource = Row.ResolveIconResource();
	if (!IconResource)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TraitGachaPanel] Ticket icon missing in DT_ShopItem (ItemType=%d)"), static_cast<int32>(Ticket));
	}
	GachaBanner->SetBannerPresentation(FText::FromString(BannerName), IconResource);
}

void UBuildingTraitGachaPanelWidget::UpdateGauges()
{
	// 특성/스킨 천장·마일리지 상수는 동일(60/150/50). 값만 활성 카테고리 매니저에서 읽음.
	// 문구는 매니저와 무관한 상수라 조기 반환 앞에서 — WBP 에 구워진 직원 패널 문구가 남지 않게
	SetExchangeLabel(FGachaTraitData::MileageEpicBox);
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
	const float EpicPct = FMath::Clamp((float)EpicPity / (float)FBuildingTraitPityData::HardPity, 0.f, 1.f);
	SetGauge(EpicPityBar, EpicPityHead, EpicPityLabel, EpicPct, FText::FromString(FString::Printf(TEXT("%s 천장 %d/%d"),
		*FLootBoxRarityUtility::GetKoreanName(ELootBoxRarity::Epic), EpicPity, FBuildingTraitPityData::HardPity)));

	// Legendary 대천장 (150)
	const float LgdPct = FMath::Clamp((float)LgdPity / (float)FBuildingTraitPityData::GrandPity, 0.f, 1.f);
	SetGauge(LegendaryPityBar, LegendaryPityHead, LegendaryPityLabel, LgdPct, FText::FromString(FString::Printf(TEXT("%s 천장 %d/%d"),
		*FLootBoxRarityUtility::GetKoreanName(ELootBoxRarity::Legendary), LgdPity, FBuildingTraitPityData::GrandPity)));

	// 마일리지 (합산, 150pt = Legendary 상자)
	const float MileagePct = FMath::Clamp((float)Mileage / (float)FGachaTraitData::MileageLegendaryBox, 0.f, 1.f);
	SetGauge(MileageBar, MileageHead, MileageLabel, MileagePct, FText::FromString(FString::Printf(TEXT("마일리지 %d/%d"),
		Mileage, FGachaTraitData::MileageLegendaryBox)));
	// 교환 버튼은 Epic 상자(50pt) 이상이면 활성
	if (MileageExchangeButton)
	{
		MileageExchangeButton->SetIsEnabled(Mileage >= FGachaTraitData::MileageEpicBox);
	}
}

void UBuildingTraitGachaPanelWidget::UpdateCTALabel()
{
	if (!ItemMgr || !PullButton) { return; }

	const EItemType Ticket = GetActiveTicketType();
	const int32 TicketCount = ItemMgr->GetItemCount(Ticket);
	const int32 MultiCount = GetMultiPullCount();

	// 종류(특성=기어/스킨=붓)·티어(색)는 좌측 아이콘이 표시한다. 가챠는 티켓 전용(다이아 폴백 없음).
	// 서버 응답 대기 중에는 눌러도 pending 이 막으므로 라벨로 대기를 보여준다(3상태 안내 문구 층은 후속)
	// 모자라도 누를 수 있다 — 회색 표면과 "보유/필요"로 알리고, 누르면 RequestServerPull 이 부족 안내를 띄운다
	const FString Label1 = bPullRequestInFlight ? FString(TEXT("처리 중입니다")) : FString(TEXT("뽑기"));
	ApplyPullButton(PullButton, FText::FromString(Label1), !bPullRequestInFlight, Ticket);
	PullButton->SetItemCost(TicketCount, 1);

	// 10회 단추 ― 라벨·게이트·요청이 GetMultiPullCount 한 값을 본다. "10연차" 표기는 연차(휴가) 오독으로 폐기.
	const FString LabelMulti = bPullRequestInFlight ? FString(TEXT("처리 중입니다")) : FString(TEXT("뽑기"));
	ApplyPullButton(PullButton10, FText::FromString(LabelMulti), !bPullRequestInFlight, Ticket);
	if (PullButton10) PullButton10->SetItemCost(TicketCount, MultiCount);
	ApplyPullButton(PullButton30, FText::FromString(LabelMulti), !bPullRequestInFlight, Ticket);
	if (PullButton30) PullButton30->SetItemCost(TicketCount, 30);

	UpdateTicketCounts();
}

void UBuildingTraitGachaPanelWidget::UpdateTicketCounts()
{
	if (!ItemMgr) { return; }

	// 배너 2개 = 현재 카테고리(특성/스킨)의 일반/고급 뽑기권. 카테고리 토글 시 같은 배너에 다른 티켓 수가 들어간다
	const EItemType TNormal = bIsSkinCategory ? EItemType::SkinTicketNormal : EItemType::BuildingTraitTicketNormal;
	const EItemType TAdvanced = bIsSkinCategory ? EItemType::SkinTicketAdvanced : EItemType::BuildingTraitTicketAdvanced;

	SetBannerOwnedCount(BannerNormal, TNormal);
	SetBannerOwnedCount(BannerAdvanced, TAdvanced);
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
	SetBannerIndex(0);
	if (BannerGroup) { BannerGroup->SelectButtonAtIndex(0); }

	RefreshAll();
}

void UBuildingTraitGachaPanelWidget::SetBannerIndex(int32 Index)
{
	bAdvancedSelected = (Index == 1);
}

void UBuildingTraitGachaPanelWidget::OnPullClicked()
{
	RequestServerPull(1);
}

void UBuildingTraitGachaPanelWidget::OnPull10Clicked()
{
	RequestServerPull(GetMultiPullCount());
}

void UBuildingTraitGachaPanelWidget::OnPull30Clicked()
{
	RequestServerPull(30);
}

EItemType UBuildingTraitGachaPanelWidget::GetActiveTicketType() const
{
	return bIsSkinCategory
		? UBuildingSkinManagerSubsystem::GetTicketType(bAdvancedSelected)
		: UBuildingTraitManagerSubsystem::GetTicketType(bAdvancedSelected);
}

int32 UBuildingTraitGachaPanelWidget::GetMultiPullCount() const
{
	// 1 op 상한(MaxPullsPerOp)은 30 이다 ― 이 단추는 특성·스킨 모두 10회로 고정한다
	return 10;
}

void UBuildingTraitGachaPanelWidget::RequestServerPull(int32 RequestedCount)
{
	if (bPullRequestInFlight) { return; }

	// 로컬 캐시 선판정 — 서버 왕복을 아끼는 안내일 뿐이고 실제 차감 판정은 서버가 한다
	const EItemType Ticket = GetActiveTicketType();
	if (!ItemMgr || !ItemMgr->HasItem(Ticket, RequestedCount))
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(RequestedCount > 1
				? FText::Format(NSLOCTEXT("Gacha", "NoTicketMulti", "상자가 부족합니다 ({0}개 필요)"), FText::AsNumber(RequestedCount))
				: NSLOCTEXT("Gacha", "NoTicket", "상자가 부족합니다"),
				3.0f, ENotificationType::Failed);
		}
		return;
	}

	// 대기 중에 배너를 바꿔도 연출은 요청한 티어를 따라야 한다 — 멤버를 다시 읽지 않고 값으로 잡는다
	const bool bAdvanced = bAdvancedSelected;
	const UTableManagerSubsystem* TablesAtRequest = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// ===== 스킨 카테고리 경로 =====
	if (bIsSkinCategory)
	{
		if (!SkinManager) { return; }
		const int32 MileageBefore = SkinManager->GetMileagePoints();
		TSet<int32> OwnedBefore;
		if (USaveLoadManager* Save = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			if (const USaveGame_GameData* Data = Save->GetCurrentSaveData())
			{
				for (const FBuildingSkinInstance& Skin : Data->GameData.OwnedBuildingSkins) { OwnedBefore.Add(Skin.SkinID); }
			}
		}
		const TArray<int32> GradeWeights = RevealGradeWeights(SkinManager->GetSkinProbabilityTableForUI(bAdvanced));
		const EGachaBoxType BoxType = bAdvanced ? EGachaBoxType::SkinAdvanced : EGachaBoxType::SkinNormal;
		const FGachaBoxVisualRow* BoxVisual = TablesAtRequest ? TablesAtRequest->GetGachaBoxVisual(BoxType) : nullptr;
		const FVector2D LayerSize = BoxVisual ? BoxVisual->LayerSize : FVector2D::ZeroVector;
		UWidget* Hero = GetWidgetFromName(TEXT("HeroBoxImage"));
		FVector2D LaunchPos = FVector2D::ZeroVector;
		float LaunchScale = 0.f;
		const bool bHasLaunch = TryGetRevealLaunch(Hero ? Hero : HeroCardSwitcher, LayerSize, this, LaunchPos, LaunchScale);

		bPullRequestInFlight = true;
		UpdateCTALabel();   // 대기 중 CTA 잠금

		SkinManager->RequestGachaPull(bAdvanced, RequestedCount,
			UBuildingSkinManagerSubsystem::FOnSkinPullDone::CreateWeakLambda(this,
				[this, bAdvanced, MileageBefore, OwnedBefore, GradeWeights, bHasLaunch, LaunchPos, LaunchScale](bool bOk, const TArray<FBuildingSkinGachaResult>& Results)
		{
			bPullRequestInFlight = false;

			// bOk 여도 Results 가 빌 수 있다(리플레이) — 그때는 연출 없이 라벨만 되돌린다
			if (bOk && Results.Num() > 0)
			{
				if (UGachaRevealPresentationWidget* Pres = CreateRevealWidget())
				{
					FGachaRevealAwardContext Award;
					TSet<int32> SequentialOwned = OwnedBefore;
					for (const FBuildingSkinGachaResult& Result : Results)
					{
						Award.bOwnedBefore.Add(Result.bDuplicate || SequentialOwned.Contains(Result.ResultSkinID));
						if (!Result.bDuplicate) { SequentialOwned.Add(Result.ResultSkinID); }
					}
					Award.GradeWeights = GradeWeights;
					Award.MileageBalance = SkinManager->GetMileagePoints();
					Award.MileageGained = Award.MileageBalance - MileageBefore;
					if (bHasLaunch) { Pres->SetLaunchFrom(LaunchPos, LaunchScale); }
					Pres->SetupPullSkin(Results, bAdvanced, Award);
				}
			}
			UpdateCTALabel();
		}));
		return;
	}

	// ===== 특성 카테고리 경로 =====
	if (!TraitManager) { return; }
	const int32 MileageBefore = TraitManager->GetMileagePoints();
	TSet<FName> OwnedBefore = TraitManager->GetInventory().EncyclopediaUnlocked;
	const TArray<int32> GradeWeights = RevealGradeWeights(TraitManager->GetTraitProbabilityTableForUI(bAdvanced));
	const EGachaBoxType BoxType = bAdvanced ? EGachaBoxType::TraitAdvanced : EGachaBoxType::TraitNormal;
	const FGachaBoxVisualRow* BoxVisual = TablesAtRequest ? TablesAtRequest->GetGachaBoxVisual(BoxType) : nullptr;
	const FVector2D LayerSize = BoxVisual ? BoxVisual->LayerSize : FVector2D::ZeroVector;
	UWidget* Hero = GetWidgetFromName(TEXT("HeroBoxImage"));
	FVector2D LaunchPos = FVector2D::ZeroVector;
	float LaunchScale = 0.f;
	const bool bHasLaunch = TryGetRevealLaunch(Hero ? Hero : HeroCardSwitcher, LayerSize, this, LaunchPos, LaunchScale);

	bPullRequestInFlight = true;
	UpdateCTALabel();

	TraitManager->RequestGachaPull(bAdvanced, RequestedCount,
		UBuildingTraitManagerSubsystem::FOnTraitPullDone::CreateWeakLambda(this,
			[this, bAdvanced, MileageBefore, OwnedBefore, GradeWeights, bHasLaunch, LaunchPos, LaunchScale](bool bOk, const TArray<FBuildingTraitGachaResult>& Results)
	{
		bPullRequestInFlight = false;

		if (bOk && Results.Num() > 0)
		{
			if (UGachaRevealPresentationWidget* Pres = CreateRevealWidget())
			{
				FGachaRevealAwardContext Award;
				TSet<FName> SequentialOwned = OwnedBefore;
				for (const FBuildingTraitGachaResult& Result : Results)
				{
					Award.bOwnedBefore.Add(SequentialOwned.Contains(Result.ResultTraitID));
					SequentialOwned.Add(Result.ResultTraitID);
				}
				Award.GradeWeights = GradeWeights;
				Award.MileageBalance = TraitManager->GetMileagePoints();
				Award.MileageGained = Award.MileageBalance - MileageBefore;
				if (bHasLaunch) { Pres->SetLaunchFrom(LaunchPos, LaunchScale); }
				Pres->SetupPullTrait(Results, bAdvanced, Award);
			}
		}
		UpdateCTALabel();
	}));
}

UGachaRevealPresentationWidget* UBuildingTraitGachaPanelWidget::CreateRevealWidget()
{
	auto* Pres = Cast<UGachaRevealPresentationWidget>(CreateOverlayWidget(EWidgetType::GachaRevealPresentation));
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
	if (auto* Prob = Cast<UTraitGachaProbabilityWidget>(CreateOverlayWidget(EWidgetType::TraitGachaProbabilityPanel)))
	{
		// AddToViewport(=NativeConstruct) 전에 축을 넣어야 첫 빌드부터 현재 카테고리 표가 나온다
		Prob->SetCategory(bIsSkinCategory);
		Prob->AddToViewport(1000);
	}
}
