#include "UI/Element/Skyline/SkylineFloorEntryWidget.h"

#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Global/GlobalUtilFunctions.h"
#include "Utils/FWidgetAnimationUtils.h"

namespace
{
	// 슬롯 높이(88~160)보다 작게 잡는다 — 더 크면 위 슬래브를 덮으며 떨어진다
	constexpr float SlamDropPx = 46.0f;
}

float USkylineFloorEntryWidget::GetSlamOffsetY(const float T01)
{
	if (T01 <= 0.0f) return -SlamDropPx;
	if (T01 >= 1.0f) return 0.0f;
	return -SlamDropPx * (1.0f - FWidgetAnimationUtils::EaseOutQuad(T01));
}

void USkylineFloorEntryWidget::PlaySlamIn(const float DurationSeconds)
{
	if (FMotionGate::IsReduced() || DurationSeconds <= 0.0f)
	{
		SlamDuration = 0.0f;
		SetRenderTranslation(FVector2D::ZeroVector);
		return;
	}
	SlamElapsed = 0.0f;
	SlamDuration = DurationSeconds;
	SetRenderTranslation(FVector2D(0.0f, GetSlamOffsetY(0.0f)));
}

void USkylineFloorEntryWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (SlamDuration <= 0.0f) return;

	SlamElapsed += InDeltaTime;
	const float T01 = FMath::Clamp(SlamElapsed / SlamDuration, 0.0f, 1.0f);
	SetRenderTranslation(FVector2D(0.0f, GetSlamOffsetY(T01)));
	if (T01 >= 1.0f) SlamDuration = 0.0f;
}

// 장식/글자 레이어는 히트를 먹지 않는다 — 슬래브가 목록 드래그를 가로채면 열이 안 굴러간다
ESlateVisibility USkylineFloorEntryWidget::Show(bool bVisible)
{
	return bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}

void USkylineFloorEntryWidget::SetFloor(int32 Floor, float Requirement, ESkylineFloorState State,
	bool bIsBoss, bool bIsNextGate, const FText& TenantName)
{
	FloorState = State;
	bNextGate = bIsNextGate;
	bHasRequirement = Requirement > 0.0f;
	bHasTenantName = !TenantName.IsEmpty();

	if (FloorText)
	{
		FloorText->SetText(FText::Format(NSLOCTEXT("Skyline", "FloorLabel", "{0}층"), FText::AsNumber(Floor)));
	}

	if (ClearedBG) ClearedBG->SetVisibility(Show(State == ESkylineFloorState::Cleared));
	if (CurrentBG) CurrentBG->SetVisibility(Show(State == ESkylineFloorState::Current));
	if (LockedBG)  LockedBG->SetVisibility(Show(State == ESkylineFloorState::Locked));

	if (RequirementText)
	{
		// 요구치는 하단 판정 밴드가 소유한다 — 클리어·현재 슬래브에 다시 쓰면 같은 숫자가 화면에 두 번 나온다
		// (목업 NOTES 2026-09-24 "중복 금지"). 미도달 층만 자기 요구치를 말한다.
		const bool bShowRequirement = State == ESkylineFloorState::Locked && Requirement > 0.0f;
		if (bShowRequirement)
		{
			// 반올림은 판정 밴드·기술 연구소와 같은 기준, 축약만 Ceil — 문턱값을 깎아 보이면 거짓말이 된다
			RequirementText->SetText(FText::Format(
				NSLOCTEXT("Skyline", "FloorRequirement", "요구 {0}"),
				UGlobalUtilFunctions::AbbreviateNumber(FMath::RoundToInt64(Requirement),
					ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil)));
		}
	}

	if (BossTag)     BossTag->SetVisibility(Show(bIsBoss));
	if (ClearedMark) ClearedMark->SetVisibility(Show(State == ESkylineFloorState::Cleared));

	if (TenantText)
	{
		// 밴드 밖 층은 이름이 없다 — 빈 줄을 남기면 슬래브 높이가 층마다 달라진다
		const bool bHasTenant = !TenantName.IsEmpty();
		if (bHasTenant)
		{
			TenantText->SetText(TenantName);
		}
	}
	RefreshPresentation();
}

void USkylineFloorEntryWidget::SetCompactMode(bool bCompact)
{
	bCompactMode = bCompact;
	RefreshPresentation();
}

void USkylineFloorEntryWidget::RefreshPresentation()
{
	if (RequirementText) RequirementText->SetVisibility(Show(!bCompactMode && bHasRequirement && FloorState == ESkylineFloorState::Locked));
	if (TenantText) TenantText->SetVisibility(Show(!bCompactMode && bHasTenantName));
	if (GateTag) GateTag->SetVisibility(Show(!bCompactMode && bNextGate));

	if (FloorSize)
	{
		const float Height = FloorState == ESkylineFloorState::Current ? (bCompactMode ? 92.0f : 160.0f)
			: FloorState == ESkylineFloorState::Cleared ? (bCompactMode ? 56.0f : 88.0f)
			: (bCompactMode ? 66.0f : 112.0f);
		FloorSize->SetHeightOverride(Height);
	}
}
