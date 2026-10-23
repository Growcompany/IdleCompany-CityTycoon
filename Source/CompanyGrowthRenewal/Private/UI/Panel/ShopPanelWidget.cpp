#include "UI/Panel/ShopPanelWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "Utils/EquipmentTicketShopRules.h"
#include "Manager/ItemInventoryManager.h"
#include "CommonTextBlock.h"
#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "TimerManager.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Cards/ShopItemCardWidget.h"
#include "UI/UISoundTags.h"
#include "Manager/ShopManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Table/ShopItemTable.h"
#include "Utils/FWidgetAnimationUtils.h"

void UShopPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 탭 그룹 — 모든 셋업은 NativeConstruct에서 (CLAUDE.md 규칙)
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);

	// 레일 표시 순서. 등록된 것만 TabOrder 에 쌓아 인덱스↔탭을 항상 일치시킨다
	struct FTabEntry { UTabButtonWidget* Widget; EShopTab Tab; };
	const FTabEntry Entries[] = {
		{ PackageTab,  EShopTab::Package },
		{ DailyTab,    EShopTab::Daily   },
		{ WeeklyTab,   EShopTab::Weekly  },
		{ DiamondTab,  EShopTab::Diamond },
		{ MileageTab,  EShopTab::Mileage },
	};

	TabOrder.Reset();
	for (const FTabEntry& Entry : Entries)
	{
		if (Entry.Widget && Entry.Widget->GetButton())
		{
			TabButtonGroup->AddWidget(Entry.Widget->GetButton());
			Entry.Widget->GetButton()->SetIsSelectable(true);
			// 전환음은 OnTabSelectionChanged 가 낸다 — 버튼 자체 클릭음을 끊어 두 발을 막는다
			if (UButtonWidget* TabBtn = Cast<UButtonWidget>(Entry.Widget->GetButton())) TabBtn->SetPlayClickSound(false);
			TabOrder.Add(Entry.Tab);
		}
	}

	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UShopPanelWidget::OnTabSelectionChanged);

	// 기본 진입 탭은 계속 일일 (패키지가 레일 맨 위여도 첫 화면은 바뀌지 않는다)
	const int32 DefaultIndex = FMath::Max(IndexForTab(EShopTab::Daily), 0);
	TabButtonGroup->SelectButtonAtIndex(DefaultIndex);
	CurrentTab = TabForIndex(DefaultIndex);

	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UShopPanelWidget::OnCloseButtonClicked);
	}
}

void UShopPanelWidget::NativeDestruct()
{
	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UShopPanelWidget::OnCloseButtonClicked);
	}
	Super::NativeDestruct();
}

void UShopPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (auto* PC = Cast<AMainMapPlayerController>(GetOwningPlayer())) PC->GoToUIMode();

	UGameInstance* GI = GetGameInstance();
	if (UShopManagerSubsystem* ShopMgr = GI->GetSubsystem<UShopManagerSubsystem>())
	{
		ShopMgr->CheckAndPerformResets();
		ShopMgr->OnShopStockChanged.AddUObject(this, &UShopPanelWidget::HandleStockChanged);
	}
	if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
	{
		ResourceMgr->OnResourceChanged.AddUObject(this, &UShopPanelWidget::HandleResourceChanged);
	}
	if (URecruitmentManagerSubsystem* RecruitMgr = GI->GetSubsystem<URecruitmentManagerSubsystem>())
	{
		RecruitMgr->OnMileageChanged.AddUObject(this, &UShopPanelWidget::HandleMileageChanged);
	}

	RebuildGrid();
	RefreshCurrencyChips();
	UpdateInfoText();

	// 패널 인트로 (페이드+라이즈)
	PanelIntroElapsed = 0.f;
	SetRenderOpacity(0.f);
	SetRenderTranslation(FVector2D(0.f, PanelIntroRise));

	// 리셋 카운트다운 30초 주기 갱신
	GetWorld()->GetTimerManager().SetTimer(InfoTextTimerHandle, this, &UShopPanelWidget::UpdateInfoText, 30.0f, true);
}

void UShopPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 배경 패턴 슬로우 드리프트 — 14° 눕힌 결에 수직(v축, 좌하향)으로 가라앉는 흐름.
	// v를 타일 주기(256)로 랩하면 회전 공간에서도 정확히 한 타일 변위라 이음새 없음.
	// 슬롯 사방 -480/-560 확장 + PixelSnapping=Disabled(서브픽셀 이동)가 전제
	if (UWidget* Pattern = GetWidgetFromName(TEXT("BGPattern")))
	{
		constexpr float AngleRad = 14.f * UE_PI / 180.f;
		const float CosA = FMath::Cos(AngleRad);
		const float SinA = FMath::Sin(AngleRad);

		const FVector2D T = Pattern->GetRenderTransform().Translation;
		float V = -T.X * SinA + T.Y * CosA;
		V = FMath::Fmod(V + 8.f * InDeltaTime, 256.f);
		Pattern->SetRenderTranslation(FVector2D(-V * SinA, V * CosA));
	}

	if (PanelIntroElapsed >= 0.f)
	{
		PanelIntroElapsed += InDeltaTime;
		const float T = FMath::Clamp(PanelIntroElapsed / PanelIntroDuration, 0.f, 1.f);
		const float Eased = FWidgetAnimationUtils::EaseOutQuad(T);
		SetRenderOpacity(Eased);
		SetRenderTranslation(FVector2D(0.f, PanelIntroRise * (1.f - Eased)));
		if (T >= 1.f)
		{
			PanelIntroElapsed = -1.f;
			SetRenderOpacity(1.f);
			SetRenderTranslation(FVector2D::ZeroVector);
		}
	}
}

void UShopPanelWidget::NativeOnDeactivated()
{
	RefillTicketType = EItemType::None;
	RefillRequiredCount = 0;
	UGameInstance* GI = GetGameInstance();
	if (UShopManagerSubsystem* ShopMgr = GI->GetSubsystem<UShopManagerSubsystem>())
	{
		ShopMgr->OnShopStockChanged.RemoveAll(this);
	}
	if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
	{
		ResourceMgr->OnResourceChanged.RemoveAll(this);
	}
	if (URecruitmentManagerSubsystem* RecruitMgr = GI->GetSubsystem<URecruitmentManagerSubsystem>())
	{
		RecruitMgr->OnMileageChanged.RemoveAll(this);
	}
	GetWorld()->GetTimerManager().ClearTimer(InfoTextTimerHandle);

	// UI 입력 모드 복원 (CLAUDE.md 규칙)
	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		if (PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

	Super::NativeOnDeactivated();
}

void UShopPanelWidget::SetInitialTab(EShopTab Tab)
{
	const int32 Index = IndexForTab(Tab);
	if (TabButtonGroup && Index != INDEX_NONE)
	{
		TabButtonGroup->SelectButtonAtIndex(Index);
	}
}

EShopTab UShopPanelWidget::TabForIndex(int32 Index) const
{
	return TabOrder.IsValidIndex(Index) ? TabOrder[Index] : EShopTab::Daily;
}

int32 UShopPanelWidget::IndexForTab(EShopTab Tab) const
{
	return TabOrder.IndexOfByKey(Tab);
}

void UShopPanelWidget::OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	CurrentTab = TabForIndex(ButtonIndex);

	if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
	{
		SM->PlayUISound(CGUISoundTags::TabSwitch);
	}

	RebuildGrid();
	UpdateInfoText();
}

void UShopPanelWidget::RebuildGrid()
{
	if (!ItemGrid)
	{
		return;
	}

	ItemGrid->ClearChildren();
	Cards.Empty();

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	// 패키지는 카드가 3단이라 더 크고 항목도 적다 — 전용 카드 + 3열
	const bool bPackageTab = (CurrentTab == EShopTab::Package);
	const int32 Columns = bPackageTab ? 3 : 4;
	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(
		bPackageTab ? EWidgetType::ShopPackageCard : EWidgetType::ShopItemCard);
	if (!CardClass)
	{
		// 전용 카드가 아직 등록 전이면 공용 카드로 떨어진다 — 탭이 비는 것보다 낫다
		CardClass = TableMgr->GetWidgetClass(EWidgetType::ShopItemCard);
	}
	if (!CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShopPanel] ShopItemCard widget class not found in DT_WidgetClass"));
		return;
	}

	TArray<FName> RowNames = TableMgr->GetShopItemRowsForTab(CurrentTab);
	if (RefillTicketType != EItemType::None) RowNames.StableSort([this, TableMgr](FName A, FName B)
	{
		FShopItemTable RowA, RowB;
		TableMgr->GetShopItemRow(A, RowA); TableMgr->GetShopItemRow(B, RowB);
		const int32 ScoreA = RowA.Item == RefillTicketType ? (RowA.Quantity == RefillRequiredCount ? 2 : 1) : 0;
		const int32 ScoreB = RowB.Item == RefillTicketType ? (RowB.Quantity == RefillRequiredCount ? 2 : 1) : 0;
		return ScoreA > ScoreB;
	});
	int32 Index = 0;
	for (const FName& RowName : RowNames)
	{
		FShopItemTable Row;
		if (!TableMgr->GetShopItemRow(RowName, Row))
		{
			continue;
		}

		UShopItemCardWidget* Card = CreateWidget<UShopItemCardWidget>(this, CardClass);
		if (!Card)
		{
			continue;
		}

		Card->InitCard(RowName, Row);
		Card->OnPurchaseRequested.AddUObject(this, &UShopPanelWidget::HandlePurchaseRequested);

		if (UUniformGridSlot* GridSlot = ItemGrid->AddChildToUniformGrid(Card, Index / Columns, Index % Columns))
		{
			// 이름이 두 줄인 카드가 칸을 키워도 같은 탭의 카드 높이를 맞춘다(버튼은 카드 바닥에 붙어 있다)
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Card->PlayEntrance(0.05f + Index * 0.045f);
		Cards.Add(Card);
		++Index;
	}

	RefreshAllCardStates();
}

void UShopPanelWidget::RefreshAllCardStates()
{
	UShopManagerSubsystem* ShopMgr = GetGameInstance()->GetSubsystem<UShopManagerSubsystem>();
	if (!ShopMgr)
	{
		return;
	}

	for (UShopItemCardWidget* Card : Cards)
	{
		if (Card)
		{
			Card->RefreshState(ShopMgr->GetRemainingCount(Card->GetRowName()), ShopMgr->CanAfford(Card->GetRowName()));
		}
	}
}

void UShopPanelWidget::HandlePurchaseRequested(FName RowName)
{
	UGameInstance* GI = GetGameInstance();
	UShopManagerSubsystem* ShopMgr = GI->GetSubsystem<UShopManagerSubsystem>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!ShopMgr || !TableMgr || !UIMgr)
	{
		return;
	}
	FShopItemTable TicketRow;
	if (TableMgr->GetShopItemRow(RowName, TicketRow) && EquipmentTicketShopRules::IsTicket(TicketRow.Item))
	{
		FEquipmentTicketShopRequest Request;
		FEquipmentTicketShopQuote Quote;
		FName Error;
		if (!ShopMgr->PrepareEquipmentTicketPurchase(RowName, Request, Quote, Error) || !Quote.bCanAfford || Quote.RemainingCount == 0)
		{
			UIMgr->ShowNotification(NSLOCTEXT("Shop", "TicketUnavailable", "재화 또는 구매 가능 횟수를 확인해 주세요."), 3.f, ENotificationType::Failed);
			RefreshAllCardStates();
			return;
		}
		const auto* Items = GI->GetSubsystem<UItemInventoryManager>();
		const int32 Held = Items ? Items->GetItemCount(Quote.Item) : 0;
		bool bCurrencyFound = false;
		const FResourceInfo Currency = TableMgr->GetResourceInfo(Quote.Currency == EShopCurrency::Money ? EResourceType::Money : EResourceType::Diamond, bCurrencyFound);
		if (Quote.Price > 0 && !bCurrencyFound) { UE_LOG(LogTemp, Warning, TEXT("Ticket shop currency display missing")); return; }
		const FText Cost = Quote.Price == 0 ? NSLOCTEXT("Shop", "TicketFree", "무료")
			: FText::Format(NSLOCTEXT("Shop", "TicketCurrencyCost", "{0} {1}개 사용"), Currency.DisplayName, FText::AsNumber(Quote.Price));
		UIMgr->ShowConfirmDialog(NSLOCTEXT("Shop", "TicketPurchaseTitle", "상자 구매 확인"),
			FText::Format(NSLOCTEXT("Shop", "TicketPurchaseQuote", "{0} {1}개\n{2}\n보유 {3}개 → {4}개"),
				GetItemTypeDisplayName(Quote.Item), FText::AsNumber(Quote.Quantity), Cost, FText::AsNumber(Held), FText::AsNumber(static_cast<int64>(Held) + Quote.Quantity)),
			FSimpleDelegate::CreateUObject(this, &ThisClass::ConfirmEquipmentTicketPurchase, Request),
			FSimpleDelegate(), EConfirmDialogIcon::Info);
		return;
	}

	if (!ShopMgr->CanAfford(RowName))
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::PurchaseFail);
		}
		UIMgr->ShowNotification(NSLOCTEXT("Shop", "NotEnough", "재화가 부족합니다"), 3.0f, ENotificationType::Failed);
		return;
	}

	FShopItemTable Row;
	if (!TableMgr->GetShopItemRow(RowName, Row))
	{
		return;
	}

	UIMgr->ShowConfirmDialog(
		NSLOCTEXT("Shop", "PurchaseTitle", "구매 확인"),
		FText::Format(NSLOCTEXT("Shop", "PurchaseMsg", "{0}|hpp(을,를) 구매하시겠습니까?"), Row.DisplayName),
		FSimpleDelegate::CreateUObject(this, &UShopPanelWidget::ConfirmPurchase, RowName),
		FSimpleDelegate(), EConfirmDialogIcon::Info);
}

void UShopPanelWidget::ConfirmPurchase(FName RowName)
{
	UGameInstance* GI = GetGameInstance();
	UShopManagerSubsystem* ShopMgr = GI->GetSubsystem<UShopManagerSubsystem>();
	USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>();

	if (ShopMgr && ShopMgr->PurchaseItem(RowName))
	{
		if (SM)
		{
			SM->PlayUISound(CGUISoundTags::PurchaseSuccess);
		}

		// 구매 펀치 + 결제 재화 칩 범프
		for (UShopItemCardWidget* Card : Cards)
		{
			if (Card && Card->GetRowName() == RowName)
			{
				Card->PlayPurchasePunch();
				break;
			}
		}
	}
	else
	{
		if (SM)
		{
			SM->PlayUISound(CGUISoundTags::PurchaseFail);
		}
	}
	// 카드 상태는 OnShopStockChanged/OnResourceChanged 경유로 갱신됨
}

void UShopPanelWidget::RefreshCurrencyChips()
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		return;
	}

	if (UIE_Resource_Money)
	{
		UIE_Resource_Money->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Money));
	}
	if (UIE_Resource_Diamond)
	{
		UIE_Resource_Diamond->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Diamond));
	}
}

void UShopPanelWidget::UpdateInfoText()
{
	if (!InfoText)
	{
		return;
	}

	if (RefillTicketType != EItemType::None && CurrentTab == EShopTab::Diamond)
	{
		const auto* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
		InfoText->SetText(FText::Format(NSLOCTEXT("Shop", "EquipmentReturn", "{0} · 보유 {1}개 / 필요 {2}개 · 닫으면 이전 뽑기로 돌아갑니다"),
			GetItemTypeDisplayName(RefillTicketType), FText::AsNumber(Items ? Items->GetItemCount(RefillTicketType) : 0), FText::AsNumber(RefillRequiredCount)));
		InfoText->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
	switch (CurrentTab)
	{
	case EShopTab::Daily:
	{
		if (UShopManagerSubsystem* ShopMgr = GetGameInstance()->GetSubsystem<UShopManagerSubsystem>())
		{
			const FTimespan Remain = ShopMgr->GetTimeUntilDailyReset();
			InfoText->SetText(FText::Format(NSLOCTEXT("Shop", "DailyReset", "다음 초기화까지 {0}시간 {1}분"),
				FText::AsNumber(Remain.GetHours()), FText::AsNumber(Remain.GetMinutes())));
		}
		InfoText->SetVisibility(ESlateVisibility::HitTestInvisible);
		break;
	}
	case EShopTab::Weekly:
		InfoText->SetText(NSLOCTEXT("Shop", "WeeklyReset", "매주 월요일 00:00 초기화"));
		InfoText->SetVisibility(ESlateVisibility::HitTestInvisible);
		break;
	case EShopTab::Mileage:
	{
		URecruitmentManagerSubsystem* RecruitMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>();
		InfoText->SetText(FText::Format(NSLOCTEXT("Shop", "MileageHeld", "보유 마일리지 {0}pt (프리미엄 채용 1회당 1pt 적립)"),
			FText::AsNumber(RecruitMgr ? RecruitMgr->GetMileagePoints() : 0)));
		InfoText->SetVisibility(ESlateVisibility::HitTestInvisible);
		break;
	}
	default:
		InfoText->SetVisibility(ESlateVisibility::Collapsed);
		break;
	}
}

void UShopPanelWidget::HandleStockChanged()
{
	RefreshAllCardStates();
	UpdateInfoText();
}

void UShopPanelWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type == EResourceType::Money || Type == EResourceType::Diamond)
	{
		RefreshCurrencyChips();
		RefreshAllCardStates();
	}
}

void UShopPanelWidget::HandleMileageChanged(int32 NewPoints)
{
	if (CurrentTab == EShopTab::Mileage)
	{
		UpdateInfoText();
		RefreshAllCardStates();
	}
}

void UShopPanelWidget::OnCloseButtonClicked()
{
	CloseWithAnimation();
}

UWidget* UShopPanelWidget::GetCloseButtonWidget() const
{
	return PanelHeader ? PanelHeader->GetCloseButton() : nullptr;
}

void UShopPanelWidget::SetEquipmentTicketContext(EItemType TicketType, int32 RequiredCount)
{
	if (!EquipmentTicketShopRules::IsTicket(TicketType)) return;
	RefillTicketType = TicketType;
	RefillRequiredCount = RequiredCount;
	SetInitialTab(EShopTab::Diamond);
	RebuildGrid();
	UpdateInfoText();
}

void UShopPanelWidget::ConfirmEquipmentTicketPurchase(FEquipmentTicketShopRequest Request)
{
	auto* Shop = GetGameInstance()->GetSubsystem<UShopManagerSubsystem>();
	if (!Shop) return;
	const auto Result = Shop->PurchaseEquipmentTickets(Request);
	if (auto* Sound = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>()) Sound->PlayUISound(Result.bCommitted ? CGUISoundTags::PurchaseSuccess : CGUISoundTags::PurchaseFail);
	if (Result.bCommitted)
	{
		for (auto* Card : Cards) if (Card && Card->GetRowName() == Request.RowName) { Card->PlayPurchasePunch(); break; }
		// 지출 눌림이 칩 쪽에서 나가므로 여기서 확대를 걸지 않는다
	}
	else if (auto* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (Result.ErrorCode == TEXT("SaveFailed"))
		{
			// The shared confirmation closes its current dialog after invoking this callback.
			GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Request]()
			{
				if (!IsActivated()) return;
				if (auto* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
					UIManager->ShowConfirmDialog(NSLOCTEXT("Shop", "TicketRetryTitle", "구매 저장 실패"),
						NSLOCTEXT("Shop", "TicketRetry", "재화는 사용되지 않았습니다. 같은 구매를 다시 시도할까요?"),
						FSimpleDelegate::CreateUObject(this, &ThisClass::ConfirmEquipmentTicketPurchase, Request),
						FSimpleDelegate(), EConfirmDialogIcon::Info);
			}));
		}
		else UI->ShowNotification(NSLOCTEXT("Shop", "TicketChanged", "상품 조건 또는 보유 재화가 변경되었습니다. 구매 내용을 다시 확인해 주세요."), 3.f, ENotificationType::Failed);
	}
	RefreshAllCardStates(); RefreshCurrencyChips(); UpdateInfoText();
}
