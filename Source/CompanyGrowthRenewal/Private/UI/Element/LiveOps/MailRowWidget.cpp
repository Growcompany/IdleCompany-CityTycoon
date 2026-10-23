// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/LiveOps/MailRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/ListView.h"
#include "Data/MailboxRules.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"

namespace
{
	constexpr double MailDaySeconds = 86400.0;

	// 발신 표기는 글로벌 메일 키의 유무로 갈린다 (스펙 §7.6)
	FString MailSenderLabel(const FMailEntry& Entry)
	{
		return Entry.SourceKey.IsEmpty() ? TEXT("시스템") : TEXT("운영팀");
	}

	FString MailExpireLabel(const FMailEntry& Entry, int64 NowUnix, bool bServerTimeValid)
	{
		if (!bServerTimeValid || NowUnix <= 0)
		{
			// 권위 시각이 없으면 로컬 시계로 만료를 지어내지 않는다 (이벤트 배너 D-day 와 같은 규칙)
			return TEXT("-");
		}

		if (Entry.ExpireUnix == 0)
		{
			// ExpireUnix 0 = 만료 없음 (FMailboxRules::IsExpired 와 같은 분기)
			return TEXT("보관");
		}
		const int32 Days = FMath::CeilToInt(static_cast<double>(Entry.ExpireUnix - NowUnix) / MailDaySeconds);
		return Days > 0 ? FString::Printf(TEXT("D-%d"), Days) : TEXT("오늘 만료");
	}
}

void UMailRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ClaimButton)
	{
		ClaimButton->OnClicked().AddUObject(this, &UMailRowWidget::HandleClaimClicked);
	}

	if (FirstRewardCard)
	{
		// 카드 기본값(밀집 그리드 144)은 행보다 크다 — 배치처가 컨텍스트 규격을 주입한다
		FirstRewardCard->SetCardSize(FVector2D(96.f, 96.f), FVector2D(96.f, 96.f));
		FirstRewardCard->SetLabelFontSize(24);
	}

	PlateSizer.Bind(RowPlateBG, RowPlateLine);
}

void UMailRowWidget::NativeDestruct()
{
	if (ClaimButton)
	{
		ClaimButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMailRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	PlateSizer.Update();
}

void UMailRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Item = Cast<UMailRowItem>(ListItemObject);
	if (!Item)
	{
		return;
	}

	const FMailEntry& Entry = Item->Entry;

	if (TitleText)
	{
		TitleText->SetText(Entry.Title);
	}
	if (SubText)
	{
		SubText->SetText(FText::FromString(FString::Printf(
			TEXT("%s · %s"), *MailSenderLabel(Entry),
			*MailExpireLabel(Entry, Item->NowUnix, Item->bServerTimeValid))));
	}
	if (BodyText)
	{
		BodyText->SetText(Entry.Body);
	}
	ApplyBodyVisibility();

	if (FirstRewardCard)
	{
		if (Entry.Rewards.Num() > 0)
		{
			// 카드는 첫 보상의 첫 표시 항목만 얹는다 — 나머지는 RewardBox 칩이 받는다
			FirstRewardCard->SetIconResource(CGRewardChip::ResolveIcon(GetGameInstance(), Entry.Rewards[0]));
			const TArray<CGRewardChip::FDisplayEntry> FirstDisplay = CGRewardChip::BuildDisplayEntries({ Entry.Rewards[0] });
			FirstRewardCard->SetQuantity(FirstDisplay.Num() > 0 ? static_cast<int32>(FirstDisplay[0].Quantity) : 0);
			// 카드가 클릭을 먹으면 행 탭(아코디언)이 죽는다
			FirstRewardCard->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			FirstRewardCard->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (RewardBox && WidgetTree)
	{
		CGRewardChip::RenderIcons(*WidgetTree, GetGameInstance(), *RewardBox, Entry.Rewards);
	}

	if (ClaimButton)
	{
		// 수령 즉시 행이 사라지므로 "완료" 상태가 없다 — 라벨은 WBP 의 "받기" 그대로다
		const bool bClaimable = Item->bServerTimeValid && !FMailboxRules::IsExpired(Entry, Item->NowUnix);
		// 버튼 면과 커스텀 라벨은 별도 속성이므로 같은 상태로 함께 갱신한다.
		const TSubclassOf<UCommonButtonStyle> ClaimStyle = bClaimable ? ClaimStyle_Gold : ClaimStyle_Gray;
		if (ClaimStyle)
		{
			ClaimButton->SetStyle(ClaimStyle);
		}
		const UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass());
		const UWidgetBlueprintGeneratedClass* TreeClass = WidgetClass ? WidgetClass->FindWidgetTreeOwningClass() : nullptr;
		const UWidgetTree* AuthoredTree = TreeClass ? TreeClass->GetWidgetTreeArchetype() : nullptr;
		const UButtonWidget* AuthoredButton = AuthoredTree
			? Cast<UButtonWidget>(AuthoredTree->FindWidget(ClaimButton->GetFName())) : nullptr;
		if (AuthoredButton)
		{
			// 풀 재사용 시 골드 라벨은 인스턴스의 이전 상태가 아니라 WBP 원본에서 복원한다.
			ClaimButton->SetTextColor(bClaimable ? AuthoredButton->GetTextColor()
				: FSlateColor(FLinearColor::FromSRGBColor(FColor(27, 35, 48))));
			ClaimButton->SetTextOutlineEnabled(false);
		}
		ClaimButton->SetIsEnabled(bClaimable);
	}
}

void UMailRowWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (!Item || Item->Entry.Body.IsEmpty())
	{
		// 본문이 없는 우편은 펼칠 것이 없다 — 빈 띠만 생긴다
		return;
	}

	Item->bExpanded = !Item->bExpanded;
	ApplyBodyVisibility();

	// 행 높이가 바뀌었으니 리스트가 다시 재야 한다
	if (UListView* OwningList = Cast<UListView>(GetOwningListView()))
	{
		OwningList->RequestRefresh();
	}
}

void UMailRowWidget::ApplyBodyVisibility()
{
	if (BodyText)
	{
		BodyText->SetVisibility(Item && Item->bExpanded && !Item->Entry.Body.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMailRowWidget::HandleClaimClicked()
{
	if (Item)
	{
		OnClaimRequested.Broadcast(Item);
	}
}
