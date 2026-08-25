// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Trade/MineLineWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ResourceInfo.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetSwitcher.h"
#include "CommonTextBlock.h"
#include "Engine/Texture2D.h"

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
	ApplyVisualState();
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

	// 매 frame 매니저 절대 위치 polling sync — hidden 동안 매니저 진행분 자동 흡수.
	// 매니저가 위젯보다 앞서면 catch-up, 위젯이 앞서면 그대로 유지 (frame 단위 부드러움).
	if (CachedCountry != ECountryType::None && CachedResource != EResourceType::None)
	{
		const UGameInstance* GI = GetGameInstance();
		const UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
		if (MineMgr)
		{
			FMineLineState State;
			if (MineMgr->GetLineState(CachedCountry, CachedResource, State))
			{
				if (CachedRatePerSecond > 0.0f)
				{
					const double MgrElapsed = (static_cast<double>(State.CurrentQty)
						+ static_cast<double>(State.PendingFraction)) / static_cast<double>(CachedRatePerSecond);
					if (MgrElapsed > CachedLineElapsedSec)
					{
						CachedLineElapsedSec = MgrElapsed;
						LastDisplayedWholeCount = -1;
						LastDisplayedRemainingSec = -1;
					}
				}

				if (State.bSaturated != bCachedSaturated)
				{
					bCachedSaturated = State.bSaturated;
					LastDisplayedWholeCount = -1;
					LastDisplayedRemainingSec = -1;
					bLastShownAsCompleted = false;
					ApplyState();
					ApplyVisualState();
					RefreshRecieveButtonCount(State.CurrentQty);
				}
				LastSeenManagerQty = State.CurrentQty;
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
			LastDisplayedWholeCount = CachedTargetQty;
			LastDisplayedRemainingSec = -1;
			bLastShownAsCompleted = true;
		}
		return;
	}
	bLastShownAsCompleted = false;

	// 자체 누적 — frame 단위 부드러운 progress
	CachedLineElapsedSec += static_cast<double>(InDeltaTime);

	const double ExpectedTotal = CachedLineElapsedSec * static_cast<double>(CachedRatePerSecond);
	const int64 WholeCount = FMath::Min<int64>(static_cast<int64>(FMath::FloorToDouble(ExpectedTotal)), CachedTargetQty);
	const double Frac = ExpectedTotal - FMath::FloorToDouble(ExpectedTotal);

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
		LastDisplayedWholeCount = CachedTargetQty;
		bLastShownAsCompleted = true;
		return;
	}

	// ProgressBar 는 매 frame (Slate dirty marker 가 dedup)
	SetProgressBarPercent(static_cast<float>(Frac));

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

	SyncElapsedToQuantity(State.CurrentQty);

	ApplyResourceMeta(State.Resource);
	ApplyLevelText();
	ApplyRateText();

	// 첫 표시: NativeTick 첫 frame 까지 빈 화면 방지
	const double ExpectedTotal = CachedLineElapsedSec * static_cast<double>(CachedRatePerSecond);
	const int64 WholeCount = FMath::Min<int64>(static_cast<int64>(FMath::FloorToDouble(ExpectedTotal)), CachedTargetQty);
	const double Frac = ExpectedTotal - FMath::FloorToDouble(ExpectedTotal);
	SetQuantityTexts(WholeCount, CachedTargetQty);
	SetProgressBarPercent(bCachedSaturated ? 1.0f : static_cast<float>(Frac));
	LastDisplayedWholeCount = WholeCount;
	RefreshRecieveButtonCount(State.CurrentQty);

	ApplyState();
	ApplyVisualState();
}

void UMineLineWidget::UpdateStorage(int64 CurrentQty, int64 TargetQty, bool bSaturated)
{
	const bool bWasSaturated = bCachedSaturated;
	CachedTargetQty = TargetQty;
	bCachedSaturated = bSaturated;
	LastSeenManagerQty = CurrentQty;

	// CurrentQty 기준으로 elapsed 재싱크 — 수령(0) 시 사이클 리셋, instant finish/catchup 시 점프.
	SyncElapsedToQuantity(CurrentQty);

	LastDisplayedWholeCount = -1;
	LastDisplayedRemainingSec = -1;
	if (bWasSaturated != bSaturated)
	{
		bLastShownAsCompleted = false;
		ApplyState();
	}
	ApplyVisualState();

	// Saturated/Receive 상태 — visible 안 할 때도 즉시 표시 갱신 (NativeTick 미호출 케이스 안전망).
	if (bSaturated)
	{
		SetQuantityTexts(CurrentQty, TargetQty);
		SetProgressBarPercent(1.0f);
		if (RemainingTimeText)
		{
			RemainingTimeText->SetText(FText::FromString(TEXT("[정지] 한도 도달")));
		}
		bLastShownAsCompleted = true;
		LastDisplayedWholeCount = CurrentQty;
	}
	else
	{
		SetQuantityTexts(CurrentQty, TargetQty);
	}

	RefreshRecieveButtonCount(CurrentQty);
}

void UMineLineWidget::UpdateRate(int32 RateLevel, float EffectiveRatePerMinute)
{
	CachedRateLevel = RateLevel;
	CachedRatePerSecond = (EffectiveRatePerMinute > 0.0f) ? (EffectiveRatePerMinute / 60.0f) : 0.0f;
	ApplyLevelText();
	ApplyRateText();

	// rate 변경 후에는 LastDisplayed 캐시 무효 — 다음 frame 에 새 rate 로 SetText.
	LastDisplayedRemainingSec = -1;
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
}

void UMineLineWidget::SetProgressBarPercent(float Percent01)
{
	if (!ProgressBar) return;
	ProgressBar->SetPercent(FMath::Clamp(Percent01, 0.0f, 1.0f));
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
	// "박스 + 수량" 모드 (Box) — UpgradeBtnWidget::SetCost 가 Box 분기에서 자동으로 검은 텍스트 + 박스 아이콘 적용.
	RecieveButton->SetCost(CurrentQty, EResourceType::Box, /*bCheckAfford=*/false);
}

void UMineLineWidget::ApplyVisualState()
{
	// ProgressBar fill 색은 WBP 디자이너 default 그대로 사용 (Factory 와 일관).
	// saturated 시각 구분은 BtnSwitcher (RecieveButton 노출) + "[정지]" 텍스트로 충분.
	// 회색 fill 로 덮어쓰면 100% 차 있어도 비어 보이는 역효과 발생해서 제거.
}

void UMineLineWidget::HandleClaimClicked()
{
	OnClaimRequested.Broadcast(this);
}

void UMineLineWidget::HandleInstantFinishClicked()
{
	OnInstantFinishRequested.Broadcast(this);
}
