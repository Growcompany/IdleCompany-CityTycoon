// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/WorldMap/MapActionBubbleWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"

namespace MapBubbles
{
	float HeightAboveAnchor(bool bHasMiniature, float MiniaturePivotY, float DiscHeight)
	{
		return bHasMiniature ? FMath::Max(0.0f, MiniaturePivotY) : FMath::Max(0.0f, DiscHeight) * 0.5f;
	}

	FVector2D RowOffset(float InHeightAboveAnchor)
	{
		return FVector2D(0.0, -static_cast<double>(InHeightAboveAnchor) + kRowOverlapPx);
	}

	FText ClaimLabel(int32 Pending)
	{
		return FText::Format(NSLOCTEXT("WorldMap", "MapBubbleClaim", "받기 {0}"), FText::AsNumber(FMath::Max(0, Pending)));
	}

	FText SellLabel(int64 Money)
	{
		return FText::Format(NSLOCTEXT("WorldMap", "MapBubbleSell", "판매 {0}"),
			UGlobalUtilFunctions::AbbreviateNumber(FMath::Max<int64>(0, Money)));
	}

	FText ClaimAllLabel(int32 Pending)
	{
		return FText::Format(NSLOCTEXT("WorldMap", "MapClaimAll", "모두 받기 {0}"), FText::AsNumber(FMath::Max(0, Pending)));
	}

	FText ClaimAllButtonLabel()
	{
		return NSLOCTEXT("WorldMap", "MapClaimAllButton", "모두 받기");
	}

	FText ClaimAllCountLabel(int32 Pending)
	{
		return FText::Format(NSLOCTEXT("WorldMap", "MapClaimAllCount", "{0}곳"), FText::AsNumber(FMath::Max(0, Pending)));
	}

	bool ShouldShowClaimAll(int32 AllPending, bool bDrawerOpen)
	{
		return AllPending > 0 && !bDrawerOpen;
	}
}

void UMapActionBubbleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!IconImage || !LabelText)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapActionBubble] IconImage/LabelText 가 WBP 트리에 없어 말풍선이 비어 보입니다."));
	}
	bLookApplied = false;
	ApplyKindLook();
}

void UMapActionBubbleWidget::SetBubble(EMapActionBubbleKind InKind, ECountryType InCountry, const FText& Label)
{
	Kind = InKind;
	Country = InCountry;
	if (LabelText && !LabelText->GetText().EqualTo(Label))
	{
		LabelText->SetText(Label);
	}
	ApplyKindLook();
}

FText UMapActionBubbleWidget::GetLabel() const
{
	return LabelText ? LabelText->GetText() : FText::GetEmpty();
}

void UMapActionBubbleWidget::ApplyKindLook()
{
	// 금액이 바뀔 때마다 불리므로 종류가 같으면 스타일·브러시를 다시 싣지 않는다.
	if (bLookApplied && AppliedKind == Kind)
	{
		return;
	}
	const bool bClaim = Kind == EMapActionBubbleKind::Claim;
	if (const TSubclassOf<UCommonButtonStyle> KindStyle = bClaim ? ClaimStyle : SellStyle)
	{
		SetStyle(KindStyle);
	}
	if (IconImage)
	{
		UTexture2D* Icon = bClaim ? ClaimIcon.Get() : SellIcon.Get();
		if (Icon)
		{
			IconImage->SetBrushResourceObject(Icon);
		}
		const FLinearColor Tint = bClaim ? ClaimIconTint : SellIconTint;
		IconImage->SetColorAndOpacity(Tint.A > KINDA_SMALL_NUMBER ? Tint : FLinearColor::White);
		IconImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LabelText)
	{
		LabelText->SetColorAndOpacity(FSlateColor(bClaim ? ClaimInk : SellInk));
	}
	bLookApplied = true;
	AppliedKind = Kind;
}

void UMapActionBubbleWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnBubbleClicked.ExecuteIfBound(Kind, Country);
}
