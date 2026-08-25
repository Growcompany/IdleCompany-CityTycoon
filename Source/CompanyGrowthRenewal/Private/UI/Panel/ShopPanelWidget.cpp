#include "UI/Panel/ShopPanelWidget.h"
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

	UTabButtonWidget* Tabs[] = { DailyTab, WeeklyTab, DiamondTab, MileageTab };
	for (UTabButtonWidget* Tab : Tabs)
	{
		if (Tab && Tab->GetButton())
		{
			TabButtonGroup->AddWidget(Tab->GetButton());
			Tab->GetButton()->SetIsSelectable(true);
		}
	}

	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UShopPanelWidget::OnTabSelectionChanged);
	TabButtonGroup->SelectButtonAtIndex(0);
	CurrentTab = EShopTab::Daily;

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UShopPanelWidget::OnCloseButtonClicked);
	}
	if (ManualResetButton)
	{
		ManualResetButton->OnClicked().AddUObject(this, &UShopPanelWidget::OnManualResetClicked);
	}
}

void UShopPanelWidget::NativeDestruct()
{
	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UShopPanelWidget::OnCloseButtonClicked);
	}
	if (ManualResetButton)
	{
		ManualResetButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UShopPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

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
	int32 Index = 0;
	switch (Tab)
	{
	case EShopTab::Weekly:  Index = 1; break;
	case EShopTab::Diamond: Index = 2; break;
	case EShopTab::Mileage: Index = 3; break;
	default: break;
	}
	if (TabButtonGroup)
	{
		TabButtonGroup->SelectButtonAtIndex(Index);
	}
}

EShopTab UShopPanelWidget::TabForIndex(int32 Index)
{
	switch (Index)
	{
	case 1: return EShopTab::Weekly;
	case 2: return EShopTab::Diamond;
	case 3: return EShopTab::Mileage;
	default: return EShopTab::Daily;
	}
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

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::ShopItemCard);
	if (!CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShopPanel] ShopItemCard widget class not found in DT_WidgetClass"));
		return;
	}

	const TArray<FName> RowNames = TableMgr->GetShopItemRowsForTab(CurrentTab);
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

		ItemGrid->AddChildToUniformGrid(Card, Index / 4, Index % 4);
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
		FText::Format(NSLOCTEXT("Shop", "PurchaseMsg", "{0}을(를) 구매하시겠습니까?"), Row.DisplayName),
		FSimpleDelegate::CreateUObject(this, &UShopPanelWidget::ConfirmPurchase, RowName));
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
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			FShopItemTable Row;
			if (TableMgr->GetShopItemRow(RowName, Row))
			{
				if (Row.Currency == EShopCurrency::Money && UIE_Resource_Money_1)
				{
					UIE_Resource_Money_1->PlayBump();
				}
				else if (Row.Currency == EShopCurrency::Diamond && UIE_Resource_Diamond_1)
				{
					UIE_Resource_Diamond_1->PlayBump();
				}
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

void UShopPanelWidget::OnManualResetClicked()
{
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ShowConfirmDialog(
			NSLOCTEXT("Shop", "ResetTitle", "일일 상점 초기화"),
			NSLOCTEXT("Shop", "ResetMsg", "다이아 50개로 일일 상점 구매 한도를 초기화할까요?"),
			FSimpleDelegate::CreateUObject(this, &UShopPanelWidget::ConfirmManualReset));
	}
}

void UShopPanelWidget::ConfirmManualReset()
{
	UGameInstance* GI = GetGameInstance();
	UShopManagerSubsystem* ShopMgr = GI->GetSubsystem<UShopManagerSubsystem>();
	USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>();

	if (ShopMgr && ShopMgr->ManualResetDaily())
	{
		if (SM)
		{
			SM->PlayUISound(CGUISoundTags::PurchaseSuccess);
		}
		if (UIE_Resource_Diamond_1)
		{
			UIE_Resource_Diamond_1->PlayBump();
		}
	}
	else
	{
		if (SM)
		{
			SM->PlayUISound(CGUISoundTags::PurchaseFail);
		}
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Shop", "NotEnoughDia", "다이아가 부족합니다"), 3.0f, ENotificationType::Failed);
		}
	}
}

void UShopPanelWidget::RefreshCurrencyChips()
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		return;
	}

	if (UIE_Resource_Money_1)
	{
		UIE_Resource_Money_1->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Money));
	}
	if (UIE_Resource_Diamond_1)
	{
		UIE_Resource_Diamond_1->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Diamond));
	}
}

void UShopPanelWidget::UpdateInfoText()
{
	if (ManualResetButton)
	{
		ManualResetButton->SetVisibility(CurrentTab == EShopTab::Daily ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!InfoText)
	{
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
