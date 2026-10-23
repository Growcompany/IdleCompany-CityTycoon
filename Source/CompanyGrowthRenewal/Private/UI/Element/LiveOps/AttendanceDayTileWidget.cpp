// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/LiveOps/AttendanceDayTileWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Common/UIIconContentBounds.h"
#include "Utils/FWidgetAnimationUtils.h"

// 탭 프레스 플래시 (R33) — 밝은 테 한 벌을 켜 둔다. 불투명도로는 못 낸다:
// RoundedBox 아웃라인 색은 InTint 를 안 타서(SlateCore DrawElementTypes ET_RoundedBox) RenderOpacity 가 안 실린다
static constexpr float GAttendanceHaloFlashDuration = 0.15f;
// 빛띠 = 시안 @keyframes sweep{0%,55%{left:-60%}100%{left:130%}} ease-in-out. 위치는 칸 폭 비율, 기준 = WBP 의 빛띠 왼쪽 0
static constexpr float GAttendanceSweepHold = 0.55f;
static constexpr double GAttendanceSweepFrom = -0.6;
static constexpr double GAttendanceSweepTo = 1.3;

ESlateVisibility UAttendanceDayTileWidget::Show(bool bVisible)
{
	return bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}

FAttendanceTileVisibility FAttendanceTileVisibility::Resolve(EAttendanceTileState State, bool bMilestone)
{
	const bool bToday = State == EAttendanceTileState::Today;
	const bool bClaimed = State == EAttendanceTileState::ClaimedToday || State == EAttendanceTileState::Done;

	FAttendanceTileVisibility Out;
	// 오늘은 특별 칸이어도 오늘 판이 이긴다 — 기능색(하늘)이 "지금 받을 칸"을 말하고 특별함은 왕관 배지가 남는다
	Out.bPlateToday = bToday;
	Out.bPlateNormal = State == EAttendanceTileState::Future && !bMilestone;
	Out.bPlateGold = State == EAttendanceTileState::Future && bMilestone;
	Out.bPlateDone = bClaimed && !bMilestone;
	Out.bPlateGoldDone = bClaimed && bMilestone;
	// 왕관 날짜는 상태 무관 상시 — 수령 후에도 7열 척추가 지워지지 않는다
	Out.bBadgeMilestone = bMilestone;
	Out.bBadgeToday = !bMilestone && bToday;
	Out.bBadgeDefault = !bMilestone && !bToday;
	Out.bTodayRing = bToday;
	Out.bTodaySweep = bToday;
	Out.bSeal = bClaimed;
	Out.bIconDimmed = bClaimed;
	return Out;
}

void UAttendanceDayTileWidget::SetDayView(const FAttendanceDayView& Day)
{
	CurrentDay = Day.Day;
	CacheAuthoredLooks();

	// 머리띠 한 줄 「12일」 — 숫자와 「일」이 한 글꼴 한 크기라 어긋나지 않는다 (스펙 ⑥)
	const FText DayLabel = FText::Format(NSLOCTEXT("Attendance", "TileDay", "{0}일"), Day.Day);
	DayText->SetText(DayLabel);
	MilestoneDayText->SetText(DayLabel);
	// 오늘 칸은 날짜 대신 「오늘」 — 칸 자리가 일차를 말한다 (스펙 ⑥)
	DayTodayText->SetText(NSLOCTEXT("Attendance", "TileToday", "오늘"));

	if (UObject* Icon = Day.PrimaryIcon.Get())
	{
		RewardCard->SetIconResource(Icon);
		RewardCard->SetLabel(Day.PrimaryLabel);
		RewardCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// 아이콘을 못 찾으면 WBP 에 구워진 플레이스홀더가 정확한 수량과 함께 보이는 오표시가 난다
		UE_LOG(LogTemp, Warning, TEXT("출석 %d일차 보상 아이콘 해석 실패 ― 카드를 숨깁니다 (행 %s)"),
			Day.Day, Day.bRowFound ? TEXT("있음") : TEXT("없음"));
		RewardCard->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 둘째 보상은 첫 보상 옆에 나란히 ― 28일차 잭팟이 유일한 소비자다(DT 규약)
	ExtraRewardBox->SetVisibility(Show(Day.bHasExtra));
	UObject* ExtraIcon = Day.bHasExtra ? Day.ExtraIcon.Get() : nullptr;
	// 칸은 재사용된다 — 비우지 않으면 앞서 그린 날의 둘째 보상이 접힌 박스 안에 남는다
	ExtraRewardIcon->SetBrushResourceObject(ExtraIcon);
	ExtraRewardIcon->SetVisibility(Show(ExtraIcon != nullptr));
	ExtraRewardQty->SetText(Day.bHasExtra ? Day.ExtraLabel : FText::GetEmpty());
	ApplyIconLayout(Day);

	// 메아리 도중 창이 닫히면 도장이 이동·기울기 중간값으로 얼어붙는다 — 새 상태를 그리기 전에 제자리로 되돌린다
	ResetSealPose();

	const FAttendanceTileVisibility Vis = FAttendanceTileVisibility::Resolve(Day.State, Day.bMilestone);
	TilePlate->SetVisibility(Show(Vis.bPlateNormal));
	TilePlateToday->SetVisibility(Show(Vis.bPlateToday));
	TilePlateGold->SetVisibility(Show(Vis.bPlateGold));
	TilePlateDone->SetVisibility(Show(Vis.bPlateDone));
	TilePlateGoldDone->SetVisibility(Show(Vis.bPlateGoldDone));
	DayBadge->SetVisibility(Show(Vis.bBadgeDefault));
	DayBadgeToday->SetVisibility(Show(Vis.bBadgeToday));
	MilestoneBadge->SetVisibility(Show(Vis.bBadgeMilestone));
	MilestoneCrown->SetVisibility(Show(Vis.bBadgeMilestone));
	TodayRing->SetVisibility(Show(Vis.bTodayRing));
	// 프레스 플래시는 입력이 켜는 층이다 — 새 상태를 그릴 때는 무조건 꺼야 다음 칸이 켜진 채로 물려받지 않는다
	TodayHaloFlash->SetVisibility(ESlateVisibility::Collapsed);
	bPressFlashPlaying = false;
	PressFlashElapsed = 0.f;
	DoneMark->SetVisibility(Show(Vis.bSeal));

	const bool bWasSweeping = bSweepActive;
	bSweepActive = Vis.bTodaySweep && !FMotionGate::IsReduced();
	TodaySweep->SetVisibility(Show(bSweepActive));
	if (bSweepActive && !bWasSweeping)
	{
		// 새로 켜질 때만 처음부터 — 새로고침마다 되감으면 지나가던 빛이 칸 왼쪽으로 튄다
		SweepElapsed = 0.f;
		PlaceSweep(FVector2D(GetCachedGeometry().GetLocalSize()).X);
	}

	ApplyStateInks(Vis);

	// 수령 연출은 RefreshAll 없이 SetDayView 만 부른다 — 여기서 잠가야 0.46s 동안 오늘 칸에 재탭이 안 물린다
	SetTappable(bTappable && Day.State == EAttendanceTileState::Today);
}

void UAttendanceDayTileWidget::ApplyIconLayout(const FAttendanceDayView& Day)
{
	if (IconBoxWidth <= 0.f || IconBoxHeight <= 0.f)
	{
		return;
	}
	const FVector2D FullBox(IconBoxWidth, IconBoxHeight);
	if (!Day.bHasExtra)
	{
		RewardCard->SetCardSize(FullBox, FullBox);
		return;
	}
	// 보상 둘 = 상자를 반씩. 카드 높이는 그대로 둬야 수량 줄이 다른 칸과 같은 기준선에 선다
	const FVector2D PairBox(IconBoxWidth * 0.5f, IconBoxHeight * PairBoxHeightRatio);
	RewardCard->SetCardSize(FVector2D(PairBox.X, FullBox.Y), PairBox);
	// null 리소스로 맞추면 Apply 가 여백 정보를 못 읽는다 — 아이콘이 없으면 SetDayView 가 이미 접었다
	if (UObject* ExtraIcon = Day.ExtraIcon.Get())
	{
		FUIIconContentFit::Apply(ExtraRewardIcon, ExtraIcon, PairBox);
	}
}

void UAttendanceDayTileWidget::CacheAuthoredLooks()
{
	if (bAuthoredLooksCached)
	{
		return;
	}
	bAuthoredLooksCached = true;
	AuthoredQtyInk = RewardCard->GetLabelColor();
	AuthoredExtraQtyInk = ExtraRewardQty->GetColorAndOpacity();
	AuthoredIconTint = RewardCard->GetIconTint();
	AuthoredExtraIconTint = ExtraRewardIcon->GetColorAndOpacity();
}

void UAttendanceDayTileWidget::ApplyStateInks(const FAttendanceTileVisibility& Vis)
{
	const bool bClaimed = Vis.bIconDimmed;
	DayText->SetColorAndOpacity(bClaimed ? DayInkDone : DayInkNormal);
	MilestoneDayText->SetColorAndOpacity(bClaimed ? DayInkMilestoneDone : DayInkMilestone);
	DayTodayText->SetColorAndOpacity(DayInkToday);
	RewardCard->SetLabelColor(bClaimed ? QtyInkDone : AuthoredQtyInk);
	ExtraRewardQty->SetColorAndOpacity(bClaimed ? QtyInkDone : AuthoredExtraQtyInk);
	RewardCard->SetIconTint(bClaimed ? IconDimTint : AuthoredIconTint);
	ExtraRewardIcon->SetColorAndOpacity(bClaimed ? IconDimTint : AuthoredExtraIconTint);
}

void UAttendanceDayTileWidget::PlayStampEcho(bool bReduceMotion)
{
	bEchoPlaying = true;
	bEchoReduced = bReduceMotion;
	EchoElapsed = 0.f;
	ApplySealPose(0.f);
}

void UAttendanceDayTileWidget::ApplySealPose(float T01)
{
	const FStampDropPose Pose = FWidgetAnimationUtils::StampDrop(T01, SealDropPx, SealExtraTiltDeg, bEchoReduced);
	DoneMark->SetRenderTranslation(Pose.Offset);
	DoneMark->SetRenderTransformAngle(Pose.AngleDeg);
	DoneMark->SetRenderOpacity(Pose.Opacity);
}

void UAttendanceDayTileWidget::ResetSealPose()
{
	bEchoPlaying = false;
	EchoElapsed = 0.f;
	DoneMark->SetRenderTranslation(FVector2D::ZeroVector);
	DoneMark->SetRenderTransformAngle(0.f);
	DoneMark->SetRenderOpacity(1.f);
}

void UAttendanceDayTileWidget::PlaceSweep(double InTileWidth)
{
	const float Phase = SweepElapsed / FMath::Max(SweepPeriod, UE_KINDA_SMALL_NUMBER);
	const float Travel = Phase < GAttendanceSweepHold
		? 0.f
		: FMath::SmoothStep(0.f, 1.f, (Phase - GAttendanceSweepHold) / (1.f - GAttendanceSweepHold));
	const double Left = FMath::Lerp(GAttendanceSweepFrom, GAttendanceSweepTo, static_cast<double>(Travel));
	TodaySweep->SetRenderTranslation(FVector2D(InTileWidth * Left, 0.0));
}

void UAttendanceDayTileWidget::TickSweep(const FGeometry& MyGeometry, float InDeltaTime)
{
	if (!bSweepActive)
	{
		return;
	}
	if (FMotionGate::IsReduced())
	{
		// 설정은 창이 열린 채로도 바뀐다 — 다음 SetDayView 를 기다리지 않고 바로 끈다
		bSweepActive = false;
		TodaySweep->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SweepElapsed = FMath::Fmod(SweepElapsed + InDeltaTime, FMath::Max(SweepPeriod, UE_KINDA_SMALL_NUMBER));
	PlaceSweep(FVector2D(MyGeometry.GetLocalSize()).X);
}

void UAttendanceDayTileWidget::SetTappable(bool bInTappable)
{
	bTappable = bInTappable;
	// 탭 가능한 칸만 히트테스트를 연다 — 나머지 칸이 Visible 이면 웰 드래그/닫기가 칸에 먹힌다
	SetVisibility(bInTappable ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible);
	// 프레스 플래시는 여기서 끄지 않는다 — 탭이 같은 스택에서 잠금까지 내려와 한 프레임도 못 그린다(R30). 소멸은 틱이 맡는다
}

FReply UAttendanceDayTileWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bTappable)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	bPressFlashPlaying = true;
	PressFlashElapsed = 0.f;
	TodayHaloFlash->SetVisibility(ESlateVisibility::HitTestInvisible);
	OnTapped.Broadcast(CurrentDay);
	return FReply::Handled();
}

void UAttendanceDayTileWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 감속 설정에서도 남긴다: 이동 없는 밝기 피드백이 탭이 먹혔다는 유일한 신호다
	if (bPressFlashPlaying)
	{
		PressFlashElapsed += InDeltaTime;
		if (PressFlashElapsed >= GAttendanceHaloFlashDuration)
		{
			bPressFlashPlaying = false;
			PressFlashElapsed = 0.f;
			TodayHaloFlash->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	TickSweep(MyGeometry, InDeltaTime);

	if (!bEchoPlaying)
	{
		return;
	}

	EchoElapsed += InDeltaTime;
	const float T = FMath::Clamp(EchoElapsed / FMath::Max(SealDropDuration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
	ApplySealPose(T);
	if (T >= 1.f)
	{
		ResetSealPose();
	}
}
