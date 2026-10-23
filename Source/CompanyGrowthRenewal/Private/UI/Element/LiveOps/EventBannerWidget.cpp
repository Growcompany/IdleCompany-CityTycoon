// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/LiveOps/EventBannerWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "UI/Element/Common/AlertMarkWidget.h"

namespace
{
	constexpr double BannerDaySeconds = 86400.0;

	// 종료가 10년 이상 남은 정의는 "기간 없음"의 표현이다 — D-3650 을 보여주는 건 정보가 아니다
	constexpr int64 BannerAlwaysOnSeconds = 10LL * 365LL * 86400LL;

	FString BannerDDayLabel(const FLiveOpsEventDef& Def, int64 NowUnix, bool bServerTimeValid)
	{
		if (!bServerTimeValid || NowUnix <= 0)
		{
			// 권위 시각이 없으면 로컬 시계로 D-N 을 지어내지 않는다 (수령도 같은 이유로 막혀 있다)
			return TEXT("-");
		}

		const int64 Remain = Def.EndUnix - NowUnix;
		if (Remain >= BannerAlwaysOnSeconds)
		{
			return TEXT("상시");
		}
		const int32 Days = FMath::CeilToInt(static_cast<double>(Remain) / BannerDaySeconds);
		return Days > 0 ? FString::Printf(TEXT("D-%d"), Days) : TEXT("오늘 종료");
	}
}

void UEventBannerWidget::SetEvent(const FLiveOpsEventDef& InDef, bool bInClaimable, int64 InNowUnix,
	bool bInServerTimeValid)
{
	if (BannerPlate)
	{
		BannerPlate->SetColorAndOpacity(InDef.BannerColor);
	}
	if (TypeText)
	{
		TypeText->SetText(TypeDisplayName(InDef.Type));
	}
	if (DDayText)
	{
		DDayText->SetText(FText::FromString(BannerDDayLabel(InDef, InNowUnix, bInServerTimeValid)));
	}
	if (TitleText)
	{
		TitleText->SetText(InDef.Title);
	}
	if (AlertMark)
	{
		if (bInClaimable)
		{
			AlertMark->Show();
		}
		else
		{
			AlertMark->Hide();
		}
	}
}

void UEventBannerWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Item = Cast<UEventBannerItem>(ListItemObject);
	if (!Item)
	{
		return;
	}

	SetEvent(Item->Def, Item->bClaimable, Item->NowUnix, Item->bServerTimeValid);
	// 엔트리 위젯은 풀에서 재사용된다 — 링은 지금 이 아이템의 선택 상태로 다시 맞춘다
	ApplySelectRing(IsListItemSelected());
}

void UEventBannerWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	ApplySelectRing(bIsSelected);
}

void UEventBannerWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	// 행이 버튼이라 리스트 행(STableRow)이 클릭을 못 받는다 — 선택은 창이 리스트에 되돌려 세운다
	if (Item)
	{
		OnBannerClicked.Broadcast(Item);
	}
}

FText UEventBannerWidget::TypeDisplayName(ELiveOpsEventType Type) const
{
	switch (Type)
	{
	case ELiveOpsEventType::AttendanceEvent:
		return TypeNameAttendance;
	case ELiveOpsEventType::HotTimeEvent:
		return TypeNameHotTime;
	default:
		return TypeNameCollect;
	}
}

void UEventBannerWidget::ApplySelectRing(bool bRingVisible)
{
	if (SelectRing)
	{
		SelectRing->SetVisibility(bRingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
