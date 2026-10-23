// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Trade/MineLineWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ResourceInfo.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetSwitcher.h"
#include "CommonTextBlock.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UMineLineWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	// 자식 버튼의 PreConstruct 가 라벨을 저작 잉크로 되돌린 뒤라 잠김 상태를 다시 입힌다
	ApplyButtonLabelInks();
}

void UMineLineWidget::ApplyButtonLabelInks()
{
	PaintButtonLabel(InstantFinishButton, InstantFinishLabel, bInstantFinishLocked);
	PaintButtonLabel(RecieveButton, ReceiveLabel, bReceiveLocked);
}

void UMineLineWidget::PaintButtonLabel(UCostActionButtonWidget* Button, TObjectPtr<UTextBlock>& LabelCache, bool bLocked)
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

void UMineLineWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RecieveButton)
	{
		RecieveButton->OnClicked().AddUObject(this, &UMineLineWidget::HandleClaimClicked);
	}
	if (InstantFinishButton)
	{
		InstantFinishButton->OnClicked().AddUObject(this, &UMineLineWidget::HandleInstantFinishClicked);
	}

	ApplyState();
	RefreshRecieveButtonCount(FMath::Max<int64>(0, LastSeenManagerQty));
	RefreshInstantFinishCost();
}

void UMineLineWidget::NativeDestruct()
{
	if (RecieveButton)
	{
		RecieveButton->OnClicked().RemoveAll(this);
	}
	if (InstantFinishButton)
	{
		InstantFinishButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UMineLineWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	InstantFinishRefreshElapsed += InDeltaTime;
	if (InstantFinishRefreshElapsed >= 1.f)
	{
		InstantFinishRefreshElapsed = 0.f;
		RefreshInstantFinishCost();
	}

	// 매니저의 wall-clock 결과를 표시 기준으로 사용한다. 강화 후에는 로컬 시계가 앞설 수도 있다.
	bool bSyncedToManager = false;
	int64 ManagerQty = 0;
	double ManagerFraction = 0.0;
	if (CachedCountry != ECountryType::None && CachedResource != EResourceType::None)
	{
		const UGameInstance* GI = GetGameInstance();
		const UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
		if (MineMgr)
		{
			FMineLineState State;
			if (MineMgr->GetLineState(CachedCountry, CachedResource, State))
			{
				bSyncedToManager = true;
				ManagerQty = State.CurrentQty;
				ManagerFraction = State.PendingFraction;
				if (CachedRatePerSecond > 0.0f)
				{
					const double MgrElapsed = (static_cast<double>(State.CurrentQty)
						+ static_cast<double>(State.PendingFraction)) / static_cast<double>(CachedRatePerSecond);
					CachedLineElapsedSec = MgrElapsed;
				}

				const bool bQuantityChanged = State.CurrentQty != LastSeenManagerQty;
				const bool bSaturationChanged = State.bSaturated != bCachedSaturated;
				LastSeenManagerQty = State.CurrentQty;
				bCachedSaturated = State.bSaturated;
				if (bQuantityChanged || bSaturationChanged)
				{
					LastDisplayedWholeCount = -1;
					LastDisplayedRemainingSec = -1;
					bLastShownAsCompleted = false;
					ApplyState();
					RefreshRecieveButtonCount(State.CurrentQty);
				}
				if (bQuantityChanged)
				{
					OnQuantityChanged.Broadcast(this);
				}
			}
		}
	}

	// Saturated / 무효 상태 — 한 번만 풀 표시 박아두고 SetText 중복 회피.
	if (bCachedSaturated || CachedRatePerSecond <= 0.0f || CachedTargetQty <= 0)
	{
		if (!bLastShownAsCompleted)
		{
			SetQuantityTexts(CachedTargetQty, CachedTargetQty);
			SetProgressBarPercent(1.0f);
			if (RemainingTimeText)
			{
				RemainingTimeText->SetText(FText::FromString(TEXT("[정지] 한도 도달")));
			}
			ApplyStatus(CountryDrawer::FullStatus(), true);
			LastDisplayedWholeCount = CachedTargetQty;
			LastDisplayedRemainingSec = -1;
			bLastShownAsCompleted = true;
		}
		return;
	}
	bLastShownAsCompleted = false;

	// 매니저가 아직 없는 경우에만 로컬 시계로 진행한다.
	if (!bSyncedToManager)
	{
		CachedLineElapsedSec += static_cast<double>(InDeltaTime);
	}

	const double ExpectedTotal = bSyncedToManager
		? static_cast<double>(ManagerQty) + ManagerFraction
		: CachedLineElapsedSec * static_cast<double>(CachedRatePerSecond);
	const int64 WholeCount = bSyncedToManager
		? ManagerQty
		: FMath::Min<int64>(static_cast<int64>(FMath::FloorToDouble(ExpectedTotal)), CachedTargetQty);
	const double Frac = bSyncedToManager
		? ManagerFraction
		: ExpectedTotal - FMath::FloorToDouble(ExpectedTotal);

	if (WholeCount >= CachedTargetQty)
	{
		// 위젯이 매니저보다 먼저 한도 도달 추정 — 매니저 broadcast 가 곧 따라옴.
		// 그 사이 100% 표시.
		SetQuantityTexts(CachedTargetQty, CachedTargetQty);
		SetProgressBarPercent(1.0f);
		if (RemainingTimeText)
		{
			RemainingTimeText->SetText(FText::FromString(TEXT("[정지] 한도 도달")));
		}
		ApplyStatus(CountryDrawer::FullStatus(), true);
		LastDisplayedWholeCount = CachedTargetQty;
		bLastShownAsCompleted = true;
		return;
	}

	// ProgressBar 는 매 frame (Slate dirty marker 가 dedup)
	SetLineProgress(WholeCount, Frac);

	// 카운트 텍스트는 정수 변경 시에만 — 1개 만들어질 때 0->1, 1->2, ... 갱신
	if (WholeCount != LastDisplayedWholeCount)
	{
		SetQuantityTexts(WholeCount, CachedTargetQty);
		LastDisplayedWholeCount = WholeCount;
	}

	// 남은 시간: 1초 단위
	const double RemainingSec = (static_cast<double>(CachedTargetQty) - ExpectedTotal) / static_cast<double>(CachedRatePerSecond);
	const int32 RemainingSecInt = FMath::Max(static_cast<int32>(RemainingSec), 0);
	if (RemainingSecInt != LastDisplayedRemainingSec)
	{
		SetRemainingTimeText(static_cast<double>(RemainingSecInt));
		ApplyStatus(CountryDrawer::MineFullEtaLabel(FTimespan::FromSeconds(RemainingSecInt)), false);
		LastDisplayedRemainingSec = RemainingSecInt;
	}
}

void UMineLineWidget::SetLineState(ECountryType Country, const FMineLineState& State, int64 TargetQty,
	int32 RateLevel, float EffectiveRatePerMinute)
{
	CachedCountry = Country;
	CachedResource = State.Resource;
	CachedTargetQty = TargetQty;
	CachedRatePerSecond = (EffectiveRatePerMinute > 0.0f) ? (EffectiveRatePerMinute / 60.0f) : 0.0f;
	CachedRateLevel = RateLevel;
	bCachedSaturated = State.bSaturated;
	LastSeenManagerQty = State.CurrentQty;

	// 캐시 초기화 — 새 데이터로 다음 NativeTick 이 첫 SetText 강제하도록
	LastDisplayedWholeCount = -1;
	LastDisplayedRemainingSec = -1;
	bLastShownAsCompleted = false;

	CachedLineElapsedSec = CachedRatePerSecond > 0.0f
		? (static_cast<double>(State.CurrentQty) + static_cast<double>(State.PendingFraction))
			/ static_cast<double>(CachedRatePerSecond)
		: 0.0;

	ApplyResourceMeta(State.Resource);
	ApplyLevelText();
	ApplyRateText();

	// 첫 표시: NativeTick 첫 frame 까지 빈 화면 방지
	SetQuantityTexts(State.CurrentQty, CachedTargetQty);
	if (bCachedSaturated)
	{
		SetProgressBarPercent(1.0f);
		ApplyStatus(CountryDrawer::FullStatus(), true);
	}
	else
	{
		SetLineProgress(State.CurrentQty, State.PendingFraction);
		ApplyProducingStatus(static_cast<double>(State.CurrentQty) + static_cast<double>(State.PendingFraction));
	}
	LastDisplayedWholeCount = State.CurrentQty;
	RefreshRecieveButtonCount(State.CurrentQty);

	ApplyState();
	RefreshInstantFinishCost();
}

void UMineLineWidget::UpdateStorage(int64 CurrentQty, int64 TargetQty, bool bSaturated)
{
	const bool bWasSaturated = bCachedSaturated;
	const bool bQuantityChanged = CurrentQty != LastSeenManagerQty;
	CachedTargetQty = TargetQty;
	bCachedSaturated = bSaturated;
	LastSeenManagerQty = CurrentQty;

	// CurrentQty 기준으로 elapsed 재싱크 — 수령(0) 시 사이클 리셋, instant finish/catchup 시 점프.
	SyncElapsedToQuantity(CurrentQty);

	LastDisplayedWholeCount = -1;
	LastDisplayedRemainingSec = -1;
	if (bWasSaturated != bSaturated || bQuantityChanged)
	{
		bLastShownAsCompleted = false;
		ApplyState();
	}

	// Saturated/Receive 상태 — visible 안 할 때도 즉시 표시 갱신 (NativeTick 미호출 케이스 안전망).
	if (bSaturated)
	{
		SetQuantityTexts(CurrentQty, TargetQty);
		SetProgressBarPercent(1.0f);
		if (RemainingTimeText)
		{
			RemainingTimeText->SetText(FText::FromString(TEXT("[정지] 한도 도달")));
		}
		ApplyStatus(CountryDrawer::FullStatus(), true);
		bLastShownAsCompleted = true;
		LastDisplayedWholeCount = CurrentQty;
	}
	else
	{
		SetQuantityTexts(CurrentQty, TargetQty);
		SetLineProgress(CurrentQty, 0.0);
		ApplyProducingStatus(static_cast<double>(CurrentQty));
	}

	RefreshRecieveButtonCount(CurrentQty);
	RefreshInstantFinishCost();
}

void UMineLineWidget::UpdateRate(int32 RateLevel, float EffectiveRatePerMinute)
{
	CachedRateLevel = RateLevel;
	CachedRatePerSecond = (EffectiveRatePerMinute > 0.0f) ? (EffectiveRatePerMinute / 60.0f) : 0.0f;
	ApplyLevelText();
	ApplyRateText();

	// rate 변경 후에는 LastDisplayed 캐시 무효 — 다음 frame 에 새 rate 로 SetText.
	LastDisplayedRemainingSec = -1;
	RefreshInstantFinishCost();
}

void UMineLineWidget::RefreshInstantFinishCost()
{
	if (!InstantFinishButton && !RecieveButton) return;
	const UGameInstance* GI = GetGameInstance();
	const UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	const int64 Cost = MineMgr && CachedCountry != ECountryType::None && CachedResource != EResourceType::None
		? MineMgr->GetInstantFinishCost(CachedCountry, CachedResource) : 0;
	const bool bPending = MineMgr && MineMgr->IsInstantFinishPending(CachedCountry, CachedResource);
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

void UMineLineWidget::SyncElapsedToQuantity(int64 CurrentQty)
{
	// CurrentQty 가 N 이면 "다음 N+1 만들기 시작" 시점으로 elapsed 맞춤 → ProgressBar 0% 부터 시작.
	if (CachedRatePerSecond > 0.0f)
	{
		CachedLineElapsedSec = static_cast<double>(CurrentQty) / static_cast<double>(CachedRatePerSecond);
	}
	else
	{
		CachedLineElapsedSec = 0.0;
	}
}

void UMineLineWidget::ApplyResourceMeta(EResourceType Resource)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	bool bSucc = false;
	const FResourceInfo Info = TableMgr->GetResourceInfo(Resource, bSucc);
	if (!bSucc) return;

	if (ProductText && !Info.DisplayName.IsEmpty())
	{
		ProductText->SetText(Info.DisplayName);
	}
	if (IconImage && !Info.Icon.IsNull())
	{
		if (UTexture2D* IconTex = Info.Icon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(IconTex);
		}
	}
}

void UMineLineWidget::ApplyLevelText()
{
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), CachedRateLevel)));
	}
}

void UMineLineWidget::ApplyRateText()
{
	// ProductionRateText 는 분당 채광 속도 정적 표시 — RemainingTimeText 와 정보 중복이라 비워둠.
	// 디자이너가 슬롯 활용을 원하면 여기서 "x N/min" 같은 보조 정보 채울 수 있음.
	if (ProductionRateText)
	{
		ProductionRateText->SetText(FText::GetEmpty());
	}
}

void UMineLineWidget::SetQuantityTexts(int64 Current, int64 Target)
{
	if (CurrentStatValueText)
	{
		CurrentStatValueText->SetText(UGlobalUtilFunctions::AbbreviateNumber(Current, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	}
	if (StatValueText)
	{
		StatValueText->SetText(FText::FromString(FString::Printf(TEXT("/%s"),
			*UGlobalUtilFunctions::AbbreviateNumber(Target, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));
	}
	if (BarText)
	{
		BarText->SetText(CountryDrawer::BarCountLabel(Current, Target));
	}
}

void UMineLineWidget::SetProgressBarPercent(float Percent01)
{
	if (!ProgressBar) return;
	ProgressBar->SetPercent(FMath::Clamp(Percent01, 0.0f, 1.0f));
}

void UMineLineWidget::SetLineProgress(int64 WholeCount, double UnitFraction01)
{
	// 1개 단위가 아니라 한도 대비 전체 비율 — 한 개가 캐질 때마다 0 으로 되돌아가지 않게
	SetProgressBarPercent(CountryDrawer::LineProgress01(WholeCount, CachedTargetQty, UnitFraction01));
}

void UMineLineWidget::ApplyStatus(const FText& Status, bool bFull)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
		StatusText->SetColorAndOpacity(bFull ? StatusDoneInk : StatusInk);
	}
	if (ProgressBar)
	{
		ProgressBar->SetFillColorAndOpacity(bFull ? GaugeColorSaturated : GaugeColorNormal);
	}
}

void UMineLineWidget::ApplyProducingStatus(double ExpectedTotal)
{
	const double RemainingSec = CachedRatePerSecond > 0.0f
		? (static_cast<double>(CachedTargetQty) - ExpectedTotal) / static_cast<double>(CachedRatePerSecond)
		: 0.0;
	ApplyStatus(CountryDrawer::MineFullEtaLabel(FTimespan::FromSeconds(FMath::Max(FMath::FloorToDouble(RemainingSec), 0.0))), false);
}

void UMineLineWidget::SetRemainingTimeText(double RemainingSec)
{
	if (!RemainingTimeText) return;

	const int32 Total = FMath::Max(static_cast<int32>(RemainingSec), 0);
	if (Total < 60)
	{
		RemainingTimeText->SetText(FText::FromString(FString::Printf(TEXT("%ds"), Total)));
	}
	else if (Total < 3600)
	{
		RemainingTimeText->SetText(FText::FromString(
			FString::Printf(TEXT("%dm %ds"), Total / 60, Total % 60)));
	}
	else
	{
		const int32 Hrs = Total / 3600;
		const int32 RemMins = (Total % 3600) / 60;
		RemainingTimeText->SetText(FText::FromString(
			FString::Printf(TEXT("%dh %dm"), Hrs, RemMins)));
	}
}

void UMineLineWidget::ApplyState()
{
	if (BtnSwitcher)
	{
		BtnSwitcher->SetActiveWidgetIndex(bCachedSaturated ? 1 : 0);
	}
}

void UMineLineWidget::RefreshRecieveButtonCount(int64 CurrentQty)
{
	if (!RecieveButton) return;
	// 받을 것 = 이 라인의 자원 ― 칩 아이콘도 그 자원(spec §2.3 받기 칩 = 받을 물건 아이콘)
	RecieveButton->SetCost(CurrentQty, CachedResource != EResourceType::None ? CachedResource : EResourceType::Box, /*bCheckAfford=*/false);
	CountryDrawer::StyleChipCount(RecieveButton, /*bIncoming=*/true);
}

void UMineLineWidget::SetLineNumber(int32 InNumber)
{
	if (LineNumberText)
	{
		LineNumberText->SetText(FText::AsNumber(InNumber));
	}
}

void UMineLineWidget::HandleClaimClicked()
{
	OnClaimRequested.Broadcast(this);
}

void UMineLineWidget::HandleInstantFinishClicked()
{
	OnInstantFinishRequested.Broadcast(this);
}
