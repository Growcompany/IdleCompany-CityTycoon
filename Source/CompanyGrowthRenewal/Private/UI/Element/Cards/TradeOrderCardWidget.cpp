#include "UI/Element/Cards/TradeOrderCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UTradeOrderCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AcceptButton)
	{
		AcceptButton->OnClicked().AddUObject(this, &UTradeOrderCardWidget::OnAcceptClicked);
	}
	if (DeclineButton)
	{
		DeclineButton->OnClicked().AddUObject(this, &UTradeOrderCardWidget::OnDeclineClicked);
	}

	if (bHasCachedData)
	{
		ApplyOrderData();
	}
}

void UTradeOrderCardWidget::NativeDestruct()
{
	if (AcceptButton)
	{
		AcceptButton->OnClicked().RemoveAll(this);
	}
	if (DeclineButton)
	{
		DeclineButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UTradeOrderCardWidget::SetOrder(const FTradeOrder& InOrder, const FText& ProductName, const TSoftObjectPtr<UTexture2D>& ProductIcon, int64 HaveQty, bool bSlotFull)
{
	CachedOrder = InOrder;
	CachedProductName = ProductName;
	CachedProductIcon = ProductIcon;
	CachedHaveQty = HaveQty;
	bCachedSlotFull = bSlotFull;
	bHasCachedData = true;

	if (IsConstructed())
	{
		ApplyOrderData();
	}
}

void UTradeOrderCardWidget::ApplyOrderData()
{
	const FTradeOrder& O = CachedOrder;

	// DT_TradeOrderTierDisplay 단일 진실. 매핑 없으면 default 폴백 (멤버 변수의 Normal 값)
	FText TierLabel = FText::FromString(TEXT("[일반]"));
	FLinearColor TierColor = TierColor_Normal;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			TableMgr->GetTradeOrderTierDisplay(O.Tier, TierLabel, TierColor);
		}
	}

	if (TierText)
	{
		TierText->SetText(TierLabel);
	}
	if (TierBorder)
	{
		TierBorder->SetBrushColor(TierColor);
	}

	// 산업별 StageImage 로드 — Board 가 DT_Project_*.Icon 을 주입.
	// 이미 로드된 에셋이면 LoadSynchronous 는 사실상 무료.
	if (IconImage && !CachedProductIcon.IsNull())
	{
		if (UTexture2D* Tex = CachedProductIcon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(Tex);
		}
	}

	if (ProductText)
	{
		ProductText->SetText(CachedProductName.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("Product_%d"), O.ProjectIndex))
			: CachedProductName);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("x %s"),
			*UGlobalUtilFunctions::AbbreviateNumber(O.RequestedQuantity, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));
	}

	if (RemainingTimeText)
	{
		const FDateTime Now = FDateTime::UtcNow();
		const FTimespan Remaining = (O.ExpireTime > Now) ? (O.ExpireTime - Now) : FTimespan::Zero();
		RemainingTimeText->SetText(FText::FromString(FormatDuration(Remaining)));
	}

	// 표시 수치는 "현재 재고 / 원래 요구량"으로 일관 — 수락 전/후 동일 정보
	// 재고가 요구량보다 많으면 "50/30"으로 여유분까지 그대로 노출 (정보 손실 없음).
	// 바만 1.0으로 클램프해서 시각적으로 100% 이상 안 보이게.
	const int64 DisplayDone = CachedHaveQty;

	const float Progress01 = (O.RequestedQuantity > 0)
		? FMath::Clamp(static_cast<float>(CachedHaveQty) / static_cast<float>(O.RequestedQuantity), 0.0f, 1.0f)
		: 0.0f;

	// 진행 텍스트/바 모두 DisplayDone 기반 — 수락 전 재고 프리뷰, 수락 후 실제 납품량
	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(FString::Printf(TEXT("%s/%s"),
			*UGlobalUtilFunctions::AbbreviateNumber(DisplayDone, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString(),
			*UGlobalUtilFunctions::AbbreviateNumber(O.RequestedQuantity, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));
	}

	// 분리형 진행 텍스트 ("12" + "/30") — ProgressBar 위에 오버레이로 표시되는 WBP 지원
	if (CurrentStatValueText)
	{
		CurrentStatValueText->SetText(UGlobalUtilFunctions::AbbreviateNumber(DisplayDone, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	}

	if (StatValueText)
	{
		StatValueText->SetText(FText::FromString(FString::Printf(TEXT("/%s"),
			*UGlobalUtilFunctions::AbbreviateNumber(O.RequestedQuantity, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));
	}

	// ProgressBar도 항상 표시 — 미수락은 0% 빈 바가 "이만큼 필요하다"는 시각적 단서 제공
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Progress01);
		ProgressBar->SetVisibility(ESlateVisibility::Visible);
	}

	if (RewardText)
	{
		RewardText->SetText(FText::FromString(
			FString::Printf(TEXT("보상 x%.1f"), O.RewardMultiplier)));
	}

	if (DiamondBonusText)
	{
		if (O.DiamondBonus > 0)
		{
			DiamondBonusText->SetText(FText::FromString(
				FString::Printf(TEXT("Diamond +%d"), O.DiamondBonus)));
			DiamondBonusText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			DiamondBonusText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	const bool bAccepted = O.bAccepted;
	const bool bCanAccept = !bAccepted && !bCachedSlotFull;

	// AcceptButton: 수락 후 사라짐
	if (AcceptButton)
	{
		AcceptButton->SetVisibility(bAccepted ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		AcceptButton->SetIsEnabled(bCanAccept);
	}

	// DeclineButton: "거절"(회색) ↔ "포기"(빨강) 단일 버튼 상태머신
	if (DeclineButton)
	{
		if (bAccepted)
		{
			DeclineButton->SetButtonText(FText::FromString(TEXT("포기")));
			DeclineButton->SetTextColor(FSlateColor(DeclineTextColor_Abandon));
		}
		else
		{
			DeclineButton->SetButtonText(FText::FromString(TEXT("거절")));
			DeclineButton->SetTextColor(FSlateColor(DeclineTextColor_Idle));
		}
		DeclineButton->SetIsEnabled(true);
	}

	// StatusText: 정보 전달용으로 의미 변경 (액션 아님)
	if (StatusText)
	{
		FString Status;
		if (bAccepted)
		{
			if (O.RemainingQuantity <= 0)
			{
				Status = TEXT("[완료]");
			}
			else
			{
				Status = FString::Printf(TEXT("재고 %s / %s"),
					*UGlobalUtilFunctions::AbbreviateNumber(CachedHaveQty, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString(),
					*UGlobalUtilFunctions::AbbreviateNumber(O.RemainingQuantity, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString());
			}
		}
		else if (bCachedSlotFull)
		{
			Status = TEXT("[슬롯 가득참]");
		}

		if (Status.IsEmpty())
		{
			StatusText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			StatusText->SetText(FText::FromString(Status));
			StatusText->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void UTradeOrderCardWidget::UpdateRemainingTime()
{
	if (!RemainingTimeText || !bHasCachedData) return;

	const FDateTime Now = FDateTime::UtcNow();
	const FTimespan Remaining = (CachedOrder.ExpireTime > Now)
		? (CachedOrder.ExpireTime - Now)
		: FTimespan::Zero();
	RemainingTimeText->SetText(FText::FromString(FormatDuration(Remaining)));
}

void UTradeOrderCardWidget::OnAcceptClicked()
{
	if (!CachedOrder.bAccepted && !bCachedSlotFull)
	{
		OnAcceptRequested.Broadcast(CachedOrder.OrderId);
	}
}

void UTradeOrderCardWidget::OnDeclineClicked()
{
	// 수락/미수락 상관없이 공통 진입점 — 매니저가 bAccepted 보고 분기
	OnDismissRequested.Broadcast(CachedOrder.OrderId);
}

FString UTradeOrderCardWidget::FormatDuration(const FTimespan& Duration) const
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
