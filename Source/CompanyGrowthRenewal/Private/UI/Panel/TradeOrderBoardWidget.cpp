#include "UI/Panel/TradeOrderBoardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/TradeOrderCardWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "Manager/TradeOrderManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/WorldMapManager.h"
#include "Table/ProjectDataTable.h"
#include "Enum/WidgetType.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UTradeOrderBoardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Designer 미리보기 전용. 런타임에선 NativeConstruct가 덮어씀.
	if (!IsDesignTime()) return;

	if (CardAreaSizeBox)
	{
		CardAreaSizeBox->SetHeightOverride(
			bPreviewCollapsed ? CollapsedCardAreaHeight : ExpandedCardAreaHeight);
	}
	if (CollapseBtn)
	{
		CollapseBtn->SetButtonText(
			FText::FromString(bPreviewCollapsed ? TEXT("+") : TEXT("-")));
	}
}

void UTradeOrderBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		TradeOrderMgr = GI->GetSubsystem<UTradeOrderManager>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
		WorldMapMgr = GI->GetSubsystem<UWorldMapManager>();
	}

	if (CollapseBtn)
	{
		CollapseBtn->OnClicked().AddUObject(this, &UTradeOrderBoardWidget::OnCollapseBtnClicked);
	}

	if (TradeOrderMgr)
	{
		TradeOrderMgr->OnOrderGenerated.AddDynamic(this, &UTradeOrderBoardWidget::HandleOrderGenerated);
		TradeOrderMgr->OnOrderExpired.AddDynamic(this, &UTradeOrderBoardWidget::HandleOrderExpired);
		TradeOrderMgr->OnOrderCompleted.AddDynamic(this, &UTradeOrderBoardWidget::HandleOrderCompleted);
		TradeOrderMgr->OnOrderAccepted.AddDynamic(this, &UTradeOrderBoardWidget::HandleOrderAccepted);
		TradeOrderMgr->OnOrderDismissed.AddDynamic(this, &UTradeOrderBoardWidget::HandleOrderDismissed);
		TradeOrderMgr->OnComboChanged.AddDynamic(this, &UTradeOrderBoardWidget::HandleComboChanged);
	}

	if (UWorld* World = GetWorld())
	{
		// 매초 남은 시간 텍스트만 갱신. 목록/상태 변경은 Manager 델리게이트가 담당하므로
		// 여기서 전체 RefreshBoard를 돌리면 TextBlock/카드가 매초 파괴/재생성되어 깜빡임.
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			FTimerDelegate::CreateUObject(this, &UTradeOrderBoardWidget::TickCards),
			1.0f,
			true
		);
	}

	// 초기 상태: Expanded, 카드 영역 즉시 1150으로 세팅
	bCollapsed = false;
	bAnimating = false;
	AnimElapsed = 0.0f;

	if (CardAreaSizeBox)
	{
		CardAreaSizeBox->SetHeightOverride(ExpandedCardAreaHeight);
	}
	if (CollapseBtn)
	{
		CollapseBtn->SetButtonText(FText::FromString(TEXT("-")));
	}

	RefreshBoard();
}

void UTradeOrderBoardWidget::NativeDestruct()
{
	if (CollapseBtn)
	{
		CollapseBtn->OnClicked().RemoveAll(this);
	}

	if (TradeOrderMgr)
	{
		TradeOrderMgr->OnOrderGenerated.RemoveDynamic(this, &UTradeOrderBoardWidget::HandleOrderGenerated);
		TradeOrderMgr->OnOrderExpired.RemoveDynamic(this, &UTradeOrderBoardWidget::HandleOrderExpired);
		TradeOrderMgr->OnOrderCompleted.RemoveDynamic(this, &UTradeOrderBoardWidget::HandleOrderCompleted);
		TradeOrderMgr->OnOrderAccepted.RemoveDynamic(this, &UTradeOrderBoardWidget::HandleOrderAccepted);
		TradeOrderMgr->OnOrderDismissed.RemoveDynamic(this, &UTradeOrderBoardWidget::HandleOrderDismissed);
		TradeOrderMgr->OnComboChanged.RemoveDynamic(this, &UTradeOrderBoardWidget::HandleComboChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	Super::NativeDestruct();
}

void UTradeOrderBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAnimating || !CardAreaSizeBox) return;

	const float Duration = bAnimatingToExpand ? ExpandDuration : CollapseDuration;

	// 지속시간 0 이하면 즉시 완료
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		CardAreaSizeBox->SetHeightOverride(
			bAnimatingToExpand ? ExpandedCardAreaHeight : CollapsedCardAreaHeight);
		bAnimating = false;
		OnAnimationFinished();
		return;
	}

	AnimElapsed += InDeltaTime;
	const float Progress = FMath::Clamp(AnimElapsed / Duration, 0.0f, 1.0f);

	// 방향별 easing
	const float Eased = bAnimatingToExpand
		? EaseOutBack(Progress, BackEaseOvershoot)
		: EaseInQuad(Progress);

	const float FromH = bAnimatingToExpand ? CollapsedCardAreaHeight : ExpandedCardAreaHeight;
	const float ToH = bAnimatingToExpand ? ExpandedCardAreaHeight : CollapsedCardAreaHeight;
	const float CurrentH = FMath::Lerp(FromH, ToH, Eased);

	CardAreaSizeBox->SetHeightOverride(CurrentH);

	if (Progress >= 1.0f)
	{
		bAnimating = false;
		OnAnimationFinished();
	}
}

void UTradeOrderBoardWidget::RefreshBoard()
{
	RefreshStats();
	RefreshCards();
}

void UTradeOrderBoardWidget::RefreshStats()
{
	if (!TradeOrderMgr) return;

	const int32 Combo = TradeOrderMgr->GetComboCount();
	const float ComboMul = TradeOrderMgr->GetComboMultiplier();
	const int32 AcceptedCount = TradeOrderMgr->GetAcceptedCount();
	const int32 MaxSlots = TradeOrderMgr->GetMaxAcceptedSlots();

	if (ComboText)
	{
		if (Combo >= 2)
		{
			ComboText->SetStatInfo(
				FString::Printf(TEXT("%d연속"), Combo),
				FString::Printf(TEXT("x%.1f"), ComboMul));
			ComboText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ComboText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (SlotCountText)
	{
		SlotCountText->SetStatInfo(
			TEXT("수락"),
			FString::Printf(TEXT("%d/%d"), AcceptedCount, MaxSlots));
	}

	// 미수락 긴급 주문 카운트 — 0건이면 숨김
	if (UrgentText)
	{
		int32 UrgentCount = 0;
		for (const FTradeOrder& O : TradeOrderMgr->GetUnacceptedOrders())
		{
			if (O.Tier == ETradeOrderTier::Urgent) ++UrgentCount;
		}

		if (UrgentCount > 0)
		{
			UrgentText->SetStatInfo(
				TEXT("긴급"),
				FString::Printf(TEXT("%d건"), UrgentCount));
			UrgentText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			UrgentText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 미열람 신규 주문 존재 시 보드 헤더 도트 표시 (Collapsed 상태에서도 보임).
	// Expanded 시 RefreshCards가 MarkOrdersAsViewed를 호출해 bViewed=true로 전환 → 다음 Refresh에 자동 숨김.
	if (IsValid(AlertMark))
	{
		bool bHasUnviewed = false;
		for (const FTradeOrder& O : TradeOrderMgr->GetUnacceptedOrders())
		{
			if (!O.bViewed) { bHasUnviewed = true; break; }
		}
		if (bHasUnviewed)
		{
			AlertMark->SetColor(EAlertMarkColor::Red);
			AlertMark->Show();
		}
		else
		{
			AlertMark->Hide();
		}
	}
}

void UTradeOrderBoardWidget::RefreshCards()
{
	if (!TradeOrderMgr || !TradeOrderCardScrollBox) return;

	// Collapsed 확정 상태에서는 카드 유지 불필요
	if (bCollapsed)
	{
		TradeOrderCardScrollBox->ClearChildren();
		return;
	}

	TradeOrderCardScrollBox->ClearChildren();

	// 섹션 헤더 제거 — 보드 헤더 AlertMark 가 "신규 주문 있음" 집계 알림을 담당.
	// Accepted를 앞에 두어 시선 상단에 "내가 받은 것"이 먼저 오도록 유지.
	const TArray<FTradeOrder> Accepted = TradeOrderMgr->GetAcceptedOrders();
	for (const FTradeOrder& O : Accepted)
	{
		if (UWidget* Card = CreateOrderCard(O))
		{
			TradeOrderCardScrollBox->AddChild(Card);
		}
	}

	const TArray<FTradeOrder> Unaccepted = TradeOrderMgr->GetUnacceptedOrders();
	TArray<int32> IdsToMarkViewed;
	IdsToMarkViewed.Reserve(Unaccepted.Num());

	for (const FTradeOrder& O : Unaccepted)
	{
		if (UWidget* Card = CreateOrderCard(O))
		{
			TradeOrderCardScrollBox->AddChild(Card);
		}

		// 이번 렌더가 유저에게 첫 노출인 경우만 "확인됨"으로 전환 예정.
		// 카드는 이미 O.bViewed=false 스냅샷으로 생성되어 [신규] 뱃지가 보이고,
		// 다음 RefreshCards부터는 true로 바뀐 상태로 조회되어 뱃지가 안 뜸.
		if (!O.bViewed)
		{
			IdsToMarkViewed.Add(O.OrderId);
		}
	}

	if (IdsToMarkViewed.Num() > 0)
	{
		TradeOrderMgr->MarkOrdersAsViewed(IdsToMarkViewed);
	}

	// 빈 상태 메시지는 WBP에 고정 TextBlock으로 두고 Visibility 토글하는 편이 낫지만,
	// 그건 B안 범위라 일단 생략. ScrollBox가 비어있으면 그대로 비어 보이도록 둠.
}

void UTradeOrderBoardWidget::TickCards()
{
	if (!TradeOrderCardScrollBox || bCollapsed) return;

	const int32 Count = TradeOrderCardScrollBox->GetChildrenCount();
	for (int32 i = 0; i < Count; ++i)
	{
		if (UTradeOrderCardWidget* Card = Cast<UTradeOrderCardWidget>(
			TradeOrderCardScrollBox->GetChildAt(i)))
		{
			Card->UpdateRemainingTime();
		}
	}
}

void UTradeOrderBoardWidget::ToggleCollapse()
{
	bCollapsed = !bCollapsed;
	ApplyCollapsedState();
}

void UTradeOrderBoardWidget::ApplyCollapsedState()
{
	// 펼치기일 때는 카드를 애니 시작 "전"에 populate해서
	// 높이가 자라나는 동안 카드가 위에서 서서히 드러나도록 함
	if (!bCollapsed)
	{
		RefreshCards();
	}

	// 애니메이션 상태 리셋
	bAnimatingToExpand = !bCollapsed;
	AnimElapsed = 0.0f;
	bAnimating = true;

	RefreshStats();

	if (CollapseBtn)
	{
		CollapseBtn->SetButtonText(FText::FromString(bCollapsed ? TEXT("+") : TEXT("-")));
	}
}

void UTradeOrderBoardWidget::OnAnimationFinished()
{
	// 접기 완료 → 카드 메모리 정리
	if (bCollapsed && TradeOrderCardScrollBox)
	{
		TradeOrderCardScrollBox->ClearChildren();
	}
	// 펼치기 완료 시엔 추가 처리 없음 (카드는 이미 populate됨)
}

void UTradeOrderBoardWidget::OnCollapseBtnClicked()
{
	ToggleCollapse();
}

// ── Easing 함수 ──

float UTradeOrderBoardWidget::EaseOutBack(float T, float Overshoot)
{
	// https://easings.net/#easeOutBack
	// f(t) = 1 + c3*(t-1)^3 + c1*(t-1)^2  where c1=overshoot, c3=c1+1
	const float C1 = Overshoot;
	const float C3 = C1 + 1.0f;
	const float TMinusOne = T - 1.0f;
	return 1.0f + C3 * TMinusOne * TMinusOne * TMinusOne + C1 * TMinusOne * TMinusOne;
}

float UTradeOrderBoardWidget::EaseInQuad(float T)
{
	return T * T;
}

// ── 매니저 델리게이트 핸들러 ──

void UTradeOrderBoardWidget::HandleOrderGenerated(FTradeOrder Order)
{
	RefreshBoard();
}

void UTradeOrderBoardWidget::HandleOrderExpired(int32 OrderId)
{
	RefreshBoard();
}

void UTradeOrderBoardWidget::HandleOrderCompleted(int32 OrderId)
{
	RefreshBoard();
}

void UTradeOrderBoardWidget::HandleOrderAccepted(int32 OrderId)
{
	RefreshBoard();
}

void UTradeOrderBoardWidget::HandleOrderDismissed(int32 OrderId, bool bWasAccepted)
{
	RefreshBoard();
}

void UTradeOrderBoardWidget::HandleComboChanged(int32 NewCombo, float NewMultiplier)
{
	RefreshBoard();
}

// ── 카드 생성 ──

UWidget* UTradeOrderBoardWidget::CreateOrderCard(const FTradeOrder& Order)
{
	if (!TableMgr) return nullptr;

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::TradeOrderCard);
	if (!CardClass) return nullptr;

	UTradeOrderCardWidget* Card = CreateWidget<UTradeOrderCardWidget>(this, CardClass);
	if (!Card) return nullptr;

	const FIntPoint Key = MakeProductKey(Order.CompanyType, Order.ProjectIndex);
	const int64 Have = WorldMapMgr ? WorldMapMgr->GetProductAmount(Key) : 0;
	const bool bSlotFull = TradeOrderMgr
		&& TradeOrderMgr->GetAcceptedCount() >= TradeOrderMgr->GetMaxAcceptedSlots();

	// DT_Project_* lookup 1회 → 이름/아이콘 동시 주입 (단일 진실, 호출 횟수도 절감)
	FText ProductName;
	TSoftObjectPtr<UTexture2D> ProductIcon;
	if (TableMgr)
	{
		bool bOk = false;
		const FProjectData Data = TableMgr->GetProjectData(Order.CompanyType, Order.ProjectIndex, bOk);
		if (bOk)
		{
			ProductName = Data.ProjectName;
			ProductIcon = Data.Icon;
		}
	}
	if (ProductName.IsEmpty())
	{
		ProductName = FText::FromString(FString::Printf(TEXT("Product_%d"), Order.ProjectIndex));
	}

	Card->SetOrder(Order, ProductName, ProductIcon, Have, bSlotFull);
	Card->OnAcceptRequested.AddDynamic(this, &UTradeOrderBoardWidget::HandleCardAcceptRequested);
	Card->OnDismissRequested.AddDynamic(this, &UTradeOrderBoardWidget::HandleCardDismissRequested);

	return Card;
}

void UTradeOrderBoardWidget::HandleCardAcceptRequested(int32 OrderId)
{
	if (TradeOrderMgr)
	{
		TradeOrderMgr->AcceptOrder(OrderId);
	}
}

void UTradeOrderBoardWidget::HandleCardDismissRequested(int32 OrderId)
{
	if (TradeOrderMgr)
	{
		TradeOrderMgr->DismissOrder(OrderId);
	}
}

FString UTradeOrderBoardWidget::FormatDuration(const FTimespan& Duration) const
{
	if (Duration.GetTicks() <= 0)
	{
		return TEXT("만료");
	}

	const int32 TotalSec = static_cast<int32>(Duration.GetTotalSeconds());
	const int32 Hours = TotalSec / 3600;
	const int32 Minutes = (TotalSec % 3600) / 60;

	if (Hours >= 1)
	{
		return FString::Printf(TEXT("%dh %dm"), Hours, Minutes);
	}
	return FString::Printf(TEXT("%dm"), FMath::Max(1, Minutes));
}

