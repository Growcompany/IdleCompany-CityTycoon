#include "UI/Element/Building/TierNodeWidget.h"
#include "UI/Panel/TierRoadmapWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"

namespace
{
	// 도달 색은 오피스 밴드 게이지와 공유 (CGTierColors) — 이 값은 이 위젯 전용.
	// 값은 linear (sRGB 분수 금지).
	const FLinearColor NodeUpcoming(0.023153f, 0.034340f, 0.063010f, 1.f);   // #2A3447
}

void UTierNodeWidget::Configure(int32 Tier, ETierNodeState State, const TArray<FText>& UnlockNames)
{
	const bool bCurrent = State == ETierNodeState::Current;
	const bool bDone = State == ETierNodeState::Done;

	if (LevelText)
	{
		// 표기어는 "단계" — 밴드/상단바가 이미 그렇게 부른다(티어/T 는 내부 용어)
		LevelText->SetText(FText::Format(NSLOCTEXT("Tier", "RoadmapNodeStage", "{0}단계"), FText::AsNumber(Tier)));
	}
	// 인원 상한은 층수(빌드업) 축이라 노드가 말하지 않는다 — 해금 정보만 남긴다
	if (SeatText)
	{
		SeatText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DotFill)
	{
		// 도달=그린 통일, current 는 크기(DotSizeBox)+글로우로 구분한다
		DotFill->SetBrushTintColor(FSlateColor((bCurrent || bDone) ? CGTierColors::Reached : NodeUpcoming));
	}
	if (DotSizeBox)
	{
		// 현재 노드만 크게 — 스캔할 때 "지금 여기"가 먼저 잡히도록
		DotSizeBox->SetWidthOverride(bCurrent ? 44.f : 34.f);
		DotSizeBox->SetHeightOverride(bCurrent ? 44.f : 34.f);
	}
	if (NodeGlow)
	{
		NodeGlow->SetVisibility(bCurrent ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (UnlockText)
	{
		if (UnlockNames.Num() > 0)
		{
			FText Joined = FText::Join(FText::FromString(TEXT("\n")), UnlockNames);
			UnlockText->SetText(Joined);
			UnlockText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			UnlockText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
