// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Trade/WorldFactoryLineWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Manager/WorldFactoryManager.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UWorldFactoryLineWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	// 자식 버튼의 PreConstruct 가 라벨을 저작 잉크로 되돌린 뒤라 잠김 상태를 다시 입힌다
	ApplyButtonLabelInks();
}

void UWorldFactoryLineWidget::ApplyButtonLabelInks()
{
	PaintButtonLabel(InstantFinishButton, InstantFinishLabel, bInstantFinishLocked);
	PaintButtonLabel(RecieveButton, ReceiveLabel, bReceiveLocked);
}

void UWorldFactoryLineWidget::PaintButtonLabel(UCostActionButtonWidget* Button, TObjectPtr<UTextBlock>& LabelCache, bool bLocked)
{
	if (!LabelCache && Button && Button->WidgetTree)
	{
		LabelCache = Cast<UTextBlock>(Button->WidgetTree->FindWidget(TEXT("BtnText")));
	}
	if (LabelCache && Button->bOverrideTextStyle)
	{
		LabelCache->SetColorAndOpacity(bLocked ? DisabledLabelInk : Button->BtnTextColor);
	}
}

void UWorldFactoryLineWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TierBorder)
	{
		TierBorder->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (InstantFinishButton)
	{
		InstantFinishButton->OnClicked().AddUObject(this, &UWorldFactoryLineWidget::HandleInstantFinishClicked);
	}
	if (RecieveButton)
	{
		RecieveButton->OnClicked().AddUObject(this, &UWorldFactoryLineWidget::HandleRecieveClicked);
	}

	ApplyState();
	RefreshInstantFinishCost();
}

void UWorldFactoryLineWidget::NativeDestruct()
{
	if (InstantFinishButton)
	{
		InstantFinishButton->OnClicked().RemoveAll(this);
	}
	if (RecieveButton)
	{
		RecieveButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UWorldFactoryLineWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	InstantFinishRefreshElapsed += InDeltaTime;
	if (InstantFinishRefreshElapsed >= 1.f)
	{
		InstantFinishRefreshElapsed = 0.f;
		RefreshInstantFinishCost();
	}

	// 매니저 polling sync — broadcast 누락 / hidden->visible 안전망.
	// catchup 발화 시점에 위젯이 hidden 이라 OnLineProgressed 가 UI 갱신 못 한 경우 visible 직후 자동 보정.
	if (CachedCountry != ECountryType::None && LineId != 0)
	{
		const UGameInstance* GI = GetGameInstance();
		const UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
		if (FactoryMgr)
		{
			FWorldFactoryLineState State;
			if (FactoryMgr->GetLineState(CachedCountry, LineId, State))
			{
				const bool bManagerJustCompleted = State.bCompleted && !bLastSeenManagerCompleted;
				// 매 frame 매니저 절대 위치 sync — 매니저가 위젯보다 앞서면 catch-up.
				if (State.LineElapsedSec > CachedLineElapsedSec)
				{
					CachedLineElapsedSec = State.LineElapsedSec;
					LastDisplayedWholeCount = -1;
					LastDisplayedRemainingSec = -1;
				}

				if (State.CurrentQty != LastSeenManagerQty || State.bCompleted != bLastSeenManagerCompleted)
				{
					LastSeenManagerQty = State.CurrentQty;
					bLastSeenManagerCompleted = State.bCompleted;
					LastDisplayedWholeCount = -1;
					LastDisplayedRemainingSec = -1;

					// 매니저가 완료 상태인데 위젯이 아직 Producing 이면 → broadcast 누락 안전망.
					if (State.bCompleted && CurrentState == EWorldFactoryLineState::Producing)
					{
						SetCompleted(State.CurrentQty);
					}
					else
					{
						RefreshRecieveButtonCount(CachedTargetQty);
					}
				}
				if (bManagerJustCompleted)
				{
					OnLineCompleted.Broadcast(this);
				}
			}
		}
	}

	if (CurrentState != EWorldFactoryLineState::Producing) return;
	if (CachedRatePerSecond <= 0.0f || CachedTargetQty <= 0) return;

	// 자체 누적 — 매니저 TickInternal 과 같은 모델 (DeltaTime 누적)
	CachedLineElapsedSec += static_cast<double>(InDeltaTime);

	const double ExpectedTotal = CachedLineElapsedSec * static_cast<double>(CachedRatePerSecond);
	const int64 WholeCount = FMath::Min<int64>(static_cast<int64>(FMath::FloorToDouble(ExpectedTotal)), CachedTargetQty);

	if (WholeCount >= CachedTargetQty)
	{
		// 위젯 자체 시계로 완료 도달 — 매니저 broadcast 기다리지 않고 즉시 BtnSwitcher 전환.
		// SetCompleted 가 idempotent (이미 Completed 면 같은 결과) 이라 broadcast 와 race 무관.
		if (CurrentState == EWorldFactoryLineState::Producing)
		{
			SetCompleted(CachedTargetQty);
		}
		return;
	}
	bLastShownAsCompleted = false;

	// ProgressBar 는 매 프레임 부드럽게 갱신 (Slate dirty marker 가 dedup)
	const double Frac = ExpectedTotal - FMath::FloorToDouble(ExpectedTotal);
	SetLineProgress(WholeCount, Frac);

	// 카운트 텍스트는 정수 단위 변경 시에만
	if (WholeCount != LastDisplayedWholeCount)
	{
		SetQuantityTexts(WholeCount, CachedTargetQty);
		LastDisplayedWholeCount = WholeCount;
	}

	// 남은 시간: 1초 단위 표시 → 정수 초 단위 변경 시에만
	const double RemainingSec = (static_cast<double>(CachedTargetQty) - ExpectedTotal) / static_cast<double>(CachedRatePerSecond);
	const int32 RemainingSecInt = FMath::Max(static_cast<int32>(RemainingSec), 0);
	if (RemainingSecInt != LastDisplayedRemainingSec)
	{
		const FTimespan Remain = FTimespan::FromSeconds(RemainingSecInt);
		if (RemainingTimeText)
		{
			RemainingTimeText->SetText(FText::FromString(
				FString::Printf(TEXT("남은 %s"), *FormatDuration(Remain))));
		}
		ApplyStatus(CountryDrawer::FactoryRemainingLabel(Remain), false);
		LastDisplayedRemainingSec = RemainingSecInt;
	}
}

void UWorldFactoryLineWidget::SetLineNumber(int32 InNumber)
{
	if (LineNumberText)
	{
		LineNumberText->SetText(FText::AsNumber(InNumber));
	}
}

void UWorldFactoryLineWidget::SetProductionData(UTexture2D* IconTexture, const FText& ProductName,
	float RatePerSecond, int64 CurrentQty, int64 TargetQty, const FTimespan& Remaining,
	double InLineElapsedSec)
{
	if (IconImage && IconTexture)
	{
		IconImage->SetBrushFromTexture(IconTexture);
	}
	if (ProductText)
	{
		ProductText->SetText(ProductName);
	}
	if (ProductionRateText)
	{
		ProductionRateText->SetText(FText::FromString(
			FString::Printf(TEXT("x %.1f/s"), RatePerSecond)));
	}
	if (RemainingTimeText)
	{
		RemainingTimeText->SetText(FText::FromString(
			FString::Printf(TEXT("남은 %s"), *FormatDuration(Remaining))));
	}
	ApplyStatus(CountryDrawer::FactoryRemainingLabel(Remaining), false);

	CachedTargetQty = TargetQty;
	CachedRatePerSecond = RatePerSecond;
	CachedLineElapsedSec = InLineElapsedSec;

	// 캐시 초기화 — 새 라인 데이터로 NativeTick 이 첫 SetText 강제하도록
	LastDisplayedWholeCount = -1;
	LastDisplayedRemainingSec = -1;
	bLastShownAsCompleted = false;
	LastSeenManagerQty = CurrentQty;
	bLastSeenManagerCompleted = false;

	// 첫 표시: WholeCount/Frac 즉시 계산해서 한 번 박아둠 (다음 NativeTick 까지 빈 화면 방지)
	const double ExpectedTotal = CachedLineElapsedSec * static_cast<double>(RatePerSecond);
	const int64 WholeCount = FMath::Min<int64>(static_cast<int64>(FMath::FloorToDouble(ExpectedTotal)), TargetQty);
	const double Frac = ExpectedTotal - FMath::FloorToDouble(ExpectedTotal);
	SetQuantityTexts(WholeCount, TargetQty);
	SetLineProgress(WholeCount, Frac);
	LastDisplayedWholeCount = WholeCount;
	RefreshRecieveButtonCount(TargetQty);  // 수령 시 받을 최종 수량 = TargetQty

	CurrentState = EWorldFactoryLineState::Producing;
	ApplyState();
	RefreshInstantFinishCost();
}

void UWorldFactoryLineWidget::UpdateProgress(int64 CurrentQty, const FTimespan& Remaining, double InLineElapsedSec)
{
	// 매니저 시계 동기화 — PlayFab catchup 같은 큰 시간 점프 흡수.
	CachedLineElapsedSec = InLineElapsedSec;
	LastSeenManagerQty = CurrentQty;

	// hidden 상태 안전망 — NativeTick 이 안 돌아 stale 표시되는 케이스 대비 즉시 한 번 paint.
	if (CurrentState == EWorldFactoryLineState::Producing && CachedTargetQty > 0)
	{
		const int64 WholeCount = FMath::Min<int64>(CurrentQty, CachedTargetQty);
		SetQuantityTexts(WholeCount, CachedTargetQty);
		LastDisplayedWholeCount = WholeCount;

		if (CachedRatePerSecond > 0.0f)
		{
			const double ExpectedTotal = CachedLineElapsedSec * static_cast<double>(CachedRatePerSecond);
			const double Frac = ExpectedTotal - FMath::FloorToDouble(ExpectedTotal);
			SetLineProgress(WholeCount, Frac);
		}

		if (RemainingTimeText)
		{
			RemainingTimeText->SetText(FText::FromString(
				FString::Printf(TEXT("남은 %s"), *FormatDuration(Remaining))));
		}
		ApplyStatus(CountryDrawer::FactoryRemainingLabel(Remaining), false);
	}
}

void UWorldFactoryLineWidget::SetCompleted(int64 FinalQty)
{
	SetQuantityTexts(FinalQty, CachedTargetQty);
	SetProgressBarPercent(1.0f);

	if (RemainingTimeText)
	{
		RemainingTimeText->SetText(FText::FromString(TEXT("완료")));
	}
	ApplyStatus(CountryDrawer::CompletedStatus(), true);

	// NativeTick 의 풀 표시 분기 진입을 명시적으로 막아 SetText 중복 안 일어나게
	bLastShownAsCompleted = true;
	LastDisplayedWholeCount = FinalQty;
	LastSeenManagerQty = FinalQty;

	CurrentState = EWorldFactoryLineState::Completed;
	ApplyState();
	RefreshRecieveButtonCount(FinalQty);
	RefreshInstantFinishCost();
}

void UWorldFactoryLineWidget::RefreshInstantFinishCost()
{
	if (!InstantFinishButton && !RecieveButton) return;
	const UGameInstance* GI = GetGameInstance();
	const UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
	const int64 Cost = FactoryMgr && CachedCountry != ECountryType::None && LineId != 0
		? FactoryMgr->GetInstantFinishCost(CachedCountry, LineId) : 0;
	const bool bPending = FactoryMgr && FactoryMgr->IsInstantFinishPending(CachedCountry, LineId);
	const bool bAvailable = Cost > 0 && !bPending;
	if (RecieveButton) RecieveButton->SetLockedState(bPending);
	if (InstantFinishButton)
	{
		InstantFinishButton->SetCostVisible(bAvailable);
		InstantFinishButton->SetLockedState(!bAvailable);
		if (bAvailable)
		{
			InstantFinishButton->SetCost(Cost, EResourceType::Diamond);
			CountryDrawer::StyleChipCount(InstantFinishButton, /*bIncoming=*/false);
		}
	}
	bInstantFinishLocked = !bAvailable;
	bReceiveLocked = bPending;
	ApplyButtonLabelInks();
}

void UWorldFactoryLineWidget::ApplyState()
{
	if (BtnSwitcher)
	{
		BtnSwitcher->SetActiveWidgetIndex(
			CurrentState == EWorldFactoryLineState::Producing ? 0 : 1);
	}
}

void UWorldFactoryLineWidget::RefreshRecieveButtonCount(int64 CurrentQty)
{
	if (!RecieveButton) return;
	// "박스 + 수량" 모드 (Box) — UpgradeBtnWidget::SetCost 가 Box 분기에서 자동으로 검은 텍스트 + 박스 아이콘 적용.
	RecieveButton->SetCost(CurrentQty, EResourceType::Box, /*bCheckAfford=*/false);
	CountryDrawer::StyleChipCount(RecieveButton, /*bIncoming=*/true);
}

void UWorldFactoryLineWidget::SetQuantityTexts(int64 Current, int64 Target)
{
	if (CurrentStatValueText)
	{
		CurrentStatValueText->SetText(FText::FromString(
			FString::Printf(TEXT("%lld"), Current)));
	}
	if (StatValueText)
	{
		StatValueText->SetText(FText::FromString(
			FString::Printf(TEXT("/%lld"), Target)));
	}
	if (BarText)
	{
		BarText->SetText(CountryDrawer::BarCountLabel(Current, Target));
	}
}

void UWorldFactoryLineWidget::SetProgressBarPercent(float Percent01)
{
	if (!ProgressBar) return;
	ProgressBar->SetPercent(FMath::Clamp(Percent01, 0.0f, 1.0f));
}

void UWorldFactoryLineWidget::SetLineProgress(int64 WholeCount, double UnitFraction01)
{
	SetProgressBarPercent(CountryDrawer::LineProgress01(WholeCount, CachedTargetQty, UnitFraction01));
}

void UWorldFactoryLineWidget::ApplyStatus(const FText& Status, bool bDone)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
		StatusText->SetColorAndOpacity(bDone ? StatusDoneInk : StatusInk);
	}
	if (ProgressBar)
	{
		ProgressBar->SetFillColorAndOpacity(bDone ? GaugeColorCompleted : GaugeColorProducing);
	}
}

void UWorldFactoryLineWidget::HandleInstantFinishClicked()
{
	OnInstantFinishRequested.Broadcast(this);
}

void UWorldFactoryLineWidget::HandleRecieveClicked()
{
	OnClaimRequested.Broadcast(this);
}

FString UWorldFactoryLineWidget::FormatDuration(const FTimespan& Duration)
{
	if (Duration.GetTicks() <= 0)
	{
		return TEXT("0s");
	}

	const int32 TotalSec = static_cast<int32>(Duration.GetTotalSeconds());
	const int32 Min = TotalSec / 60;
	const int32 Sec = TotalSec % 60;
	if (Min >= 1)
	{
		return FString::Printf(TEXT("%dm %ds"), Min, Sec);
	}
	return FString::Printf(TEXT("%ds"), Sec);
}
