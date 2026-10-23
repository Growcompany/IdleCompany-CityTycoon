// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/AttendancePanelWidget.h"

#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Data/AttendanceRules.h"
#include "Manager/AttendanceSubsystem.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/AttendanceTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/CountdownText.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/LiveOps/AttendanceDayTileWidget.h"
#include "UI/Element/LiveOps/AttendanceMilestoneNodeWidget.h"
#include "UI/UISoundTags.h"
#include "UI/Utils/UIVFXTextureUtils.h"

ESlateVisibility UAttendancePanelWidget::Show(bool bVisible)
{
	return bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}

FText UAttendancePanelWidget::FormatTileQuantity(bool bResource, const FText& Quantity)
{
	// 공용 FormatQuantity 는 다른 화면의 재화 칩이 쓰므로 바꾸지 않는다 — 아이템은 이미 "x" 가 붙어 온다
	return bResource ? FText::Format(NSLOCTEXT("Attendance", "ResourceQty", "x{0}"), Quantity) : Quantity;
}

bool FAttendanceView::IsNewCycle() const
{
	// 권위 시각이 없으면 순환이 넘어갔는지도 모른다 — 게이트가 없으면 28칸이 통째로 비어 "지난 칸 Done 유지"가 깨진다
	return bServerTimeValid && NextDay == 1 && CycleDay == FAttendanceRules::CycleLength && !bClaimedToday;
}

EAttendanceTileState FAttendanceView::TileState(int32 Day) const
{
	if (bServerTimeValid)
	{
		if (Day == NextDay && bCanClaim)
		{
			return EAttendanceTileState::Today;
		}
		if (bClaimedToday && Day == CycleDay)
		{
			return EAttendanceTileState::ClaimedToday;
		}
	}
	return (Day <= CycleDay && !IsNewCycle()) ? EAttendanceTileState::Done : EAttendanceTileState::Future;
}

int32 FAttendanceView::ShownDay() const
{
	return bClaimedToday ? CycleDay : NextDay;
}

int32 FAttendanceView::ClaimedCount() const
{
	return IsNewCycle() ? 0 : CycleDay;
}

const FAttendanceDayView* FAttendanceView::NextMilestoneDay() const
{
	// 어느 날이 특별 칸인지는 DT_Attendance 의 bMilestone 이 정한다 — 코드에 일차를 박으면
	// 표가 바뀐 날 칸은 금색인데 안내 줄·원판만 다른 날을 세는 조용한 어긋남이 난다
	const int32 Claimed = ClaimedCount();
	for (const FAttendanceDayView& Entry : Days)
	{
		if (Entry.bRowFound && Entry.bMilestone && Entry.Day > Claimed)
		{
			return &Entry;
		}
	}
	return nullptr;
}

int32 FAttendanceView::DaysToNextMilestone() const
{
	const FAttendanceDayView* Next = NextMilestoneDay();
	return Next ? FMath::Max(0, Next->Day - ShownDay()) : 0;
}

FText FAttendanceView::MilestoneLine() const
{
	if (NextMilestoneDay() == nullptr)
	{
		// 표가 비었으면(리임포트 누락) "모두 받았다"는 거짓이다 — 줄을 접고 BuildView 경고가 원인을 말한다
		const bool bAnyMilestone = Days.ContainsByPredicate([](const FAttendanceDayView& Entry)
		{
			return Entry.bRowFound && Entry.bMilestone;
		});
		return bAnyMilestone ? NSLOCTEXT("Attendance", "AllMilestonesDone", "특별 보상을 모두 받았습니다") : FText::GetEmpty();
	}
	// 오늘 칸이 특별 칸이면 그 칸(오늘 링)이 말한다 — 「0일」은 쓰지 않는다
	const int32 Remaining = DaysToNextMilestone();
	return Remaining > 0
		? FText::Format(NSLOCTEXT("Attendance", "NextMilestone", "특별 보상까지 {0}일"), Remaining)
		: FText::GetEmpty();
}

EAttendanceNodeState FAttendanceView::MilestoneNodeState(int32 Day) const
{
	if (Day <= ClaimedCount())
	{
		return EAttendanceNodeState::Got;
	}
	const FAttendanceDayView* Next = NextMilestoneDay();
	return Next && Next->Day == Day ? EAttendanceNodeState::Next : EAttendanceNodeState::Pending;
}

bool FAttendanceView::ShowsClaimedPlate() const
{
	return bServerTimeValid && bClaimedToday;
}

double FAttendanceTrackScale::NodeCenterX(int32 Day, double TrackWidth)
{
	const double Inner = TrackWidth - 2.0 * RimPx;
	return RimPx + (Inner - NodeRadiusPx) * static_cast<double>(Day) / static_cast<double>(FAttendanceRules::CycleLength);
}

float FAttendanceTrackScale::FillPercent(int32 Claimed, double TrackWidth)
{
	// 채움은 테 안쪽 폭 위에서 그려진다 — 채움 끝이 받은 원판 중심과 같은 눈금에 선다
	const double Inner = TrackWidth - 2.0 * RimPx;
	if (Inner <= 0.0)
	{
		return 0.f;
	}
	const double Filled = (Inner - NodeRadiusPx) * static_cast<double>(Claimed) / static_cast<double>(FAttendanceRules::CycleLength);
	return static_cast<float>(FMath::Clamp(Filled / Inner, 0.0, 1.0));
}

void UAttendancePanelWidget::FillDayFromRow(FAttendanceDayView& DayView, const FAttendanceTable& Row,
	TFunctionRef<UObject*(const CGRewardChip::FDisplayEntry&)> ResolveIcon)
{
	DayView.bRowFound = true;
	DayView.bMilestone = Row.bMilestone;

	const TArray<CGRewardChip::FDisplayEntry> Entries = CGRewardChip::BuildDisplayEntries(Row.Rewards);
	if (Entries.Num() > 0)
	{
		DayView.PrimaryIcon = ResolveIcon(Entries[0]);
		DayView.PrimaryLabel = FormatTileQuantity(Entries[0].ResourceType != EResourceType::None,
			CGRewardChip::FormatQuantity(Entries[0]));
	}
	if (Entries.Num() > 1)
	{
		DayView.bHasExtra = true;
		DayView.ExtraIcon = ResolveIcon(Entries[1]);
		DayView.ExtraLabel = FormatTileQuantity(Entries[1].ResourceType != EResourceType::None,
			CGRewardChip::FormatQuantity(Entries[1]));
	}
}

void UAttendancePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ClaimButton->OnClicked().AddUObject(this, &UAttendancePanelWidget::HandleClaim);

	UGameInstance* GameInst = GetGameInstance();
	if (UTableManagerSubsystem* TableMgr = GameInst ? GameInst->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		// 텍스처만 받는다 ― ApplyUIVFXTexture 는 브러시를 통째로 갈아 저작한 틴트·ImageSize 를 지운다 (R2)
		FUIVFXTextureUtils::SetTextureKeepStyle(TableMgr, TEXT("CTAShine"), ClaimShine);
	}

	ResetClaimShine();
	BuildGridOnce();

	// 칸은 패널보다 오래 산다(BuildGridOnce 는 1회) — 창을 다시 열 때마다 다시 건다 (R33)
	for (const TObjectPtr<UAttendanceDayTileWidget>& Tile : Tiles)
	{
		if (Tile)
		{
			Tile->OnTapped.AddUObject(this, &UAttendancePanelWidget::HandleTileTapped);
		}
	}
}

void UAttendancePanelWidget::NativeDestruct()
{
	ClaimButton->OnClicked().RemoveAll(this);
	for (const TObjectPtr<UAttendanceDayTileWidget>& Tile : Tiles)
	{
		if (Tile)
		{
			Tile->OnTapped.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

void UAttendancePanelWidget::NativeOnActivated()
{
	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UAttendanceSubsystem* Attendance = GameInst->GetSubsystem<UAttendanceSubsystem>())
		{
			Attendance->OnAttendanceChanged.AddUObject(this, &UAttendancePanelWidget::HandleAttendanceChanged);
		}
		if (UGameClockSubsystem* Clock = GameInst->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnDayKeyChanged.AddUObject(this, &UAttendancePanelWidget::HandleDayKey);
		}
	}

	bReduceMotion = USettingsManagerSubsystem::IsReduceMotion(this);
	bClaimShineTickEnabled = !bReduceMotion;
	ClaimShineTime = 0.f;
	ResetClaimShine();

	Super::NativeOnActivated();   // → RefreshAll → RetargetCountdown
}

void UAttendancePanelWidget::NativeOnDeactivated()
{
	// 크로스페이드 도중 닫으면 CTA 가 반투명·접힘인 채 남는다 — 다음 오픈의 RefreshAll 을 기다리지 않고 여기서 되돌린다
	const bool bShowClaimed = bCountdownOnClaimedPlate;
	Countdown.Stop();
	bCountdownOnClaimedPlate = false;
	// 연출 도중 닫으면 중간 프레임이 남아 재오픈 때 반쯤 넘어간 버튼이 보인다
	bClaimFxPlaying = false;
	bAwaitClaimFx = false;
	bClaimShineTickEnabled = false;
	ResetClaimShine();

	// 메아리 도중 닫으면 칸 안의 도장이 떨어지던 자세로 얼어붙는다. 칸 키가 그대로면 다음 오픈의 ApplyView 가
	// SetDayView 를 건너뛰어 그 프레임이 남으므로, 키를 비워 28칸을 다시 그리게 한다(리셋은 SetDayView 가 한다)
	for (FAttendanceTileKey& TileKey : TileKeys)
	{
		TileKey = FAttendanceTileKey();
	}

	ClaimButton->SetRenderOpacity(1.f);
	ClaimButton->SetVisibility(bShowClaimed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ClaimedPlate->SetRenderOpacity(1.f);
	ClaimedPlate->SetVisibility(bShowClaimed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UAttendanceSubsystem* Attendance = GameInst->GetSubsystem<UAttendanceSubsystem>())
		{
			Attendance->OnAttendanceChanged.RemoveAll(this);
		}
		if (UGameClockSubsystem* Clock = GameInst->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnDayKeyChanged.RemoveAll(this);
		}
	}

	Super::NativeOnDeactivated();
}

void UAttendancePanelWidget::ResetClaimShine()
{
	ClaimShine->SetRenderOpacity(0.f);
	ClaimShine->SetRenderScale(FVector2D(1.f, 1.f));
	ClaimShine->SetRenderTransformAngle(0.f);
	ClaimShine->SetRenderTranslation(FVector2D::ZeroVector);
}

void UAttendancePanelWidget::BuildGridOnce()
{
	if (Tiles.Num() > 0)
	{
		return;
	}

	UGameInstance* GameInst = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GameInst ? GameInst->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> TileClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::AttendanceDayTile) : nullptr;
	if (!TileClass)
	{
		return;
	}

	CalendarGrid->ClearChildren();
	for (int32 Index = 0; Index < FAttendanceRules::CycleLength; ++Index)
	{
		UAttendanceDayTileWidget* Tile = CreateWidget<UAttendanceDayTileWidget>(this, TileClass);
		if (!Tile)
		{
			continue;
		}

		if (UUniformGridSlot* GridSlot = CalendarGrid->AddChildToUniformGrid(Tile, Index / 7, Index % 7))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Tiles.Add(Tile);
	}
	TileKeys.SetNum(Tiles.Num());
}

FAttendanceView UAttendancePanelWidget::BuildView() const
{
	FAttendanceView View;
	UGameInstance* GameInst = GetGameInstance();
	if (const UAttendanceSubsystem* Attendance = GameInst ? GameInst->GetSubsystem<UAttendanceSubsystem>() : nullptr)
	{
		View.CycleDay = Attendance->GetCycleDay();
		View.NextDay = Attendance->GetNextDay();
		View.bCanClaim = Attendance->CanClaimToday();
		// 수령 완료와 "서버 시간 대기"는 다른 상태다 — bCanClaim 하나로 묶으면 시간이 안 잡힌 것도 완료로 보인다
		View.bClaimedToday = Attendance->HasClaimedToday();
	}
	View.bServerTimeValid = IsServerTimeValidNow();

	// DT 조회와 아이콘 해석을 여기서 끝낸다 — ApplyView 가 서브시스템을 만지지 않게 하는 경계다 (R3)
	UTableManagerSubsystem* TableMgr = GameInst ? GameInst->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (TableMgr)
	{
		View.Days.SetNum(FAttendanceRules::CycleLength);
		for (int32 Index = 0; Index < FAttendanceRules::CycleLength; ++Index)
		{
			const int32 Day = Index + 1;
			FAttendanceDayView& DayView = View.Days[Index];
			DayView.Day = Day;
			DayView.State = View.TileState(Day);

			FAttendanceTable Row;
			if (!TableMgr->GetAttendanceRow(Day, Row))
			{
				// 행이 비면 빈 칸으로 남는다 — 리임포트 누락을 로그로 드러낸다
				UE_LOG(LogTemp, Warning, TEXT("DT_Attendance %d일차 행 없음 ― 칸을 숨깁니다"), Day);
				continue;
			}
			FillDayFromRow(DayView, Row,
				[GameInst](const CGRewardChip::FDisplayEntry& Entry) { return CGRewardChip::ResolveIcon(GameInst, Entry); });
		}
	}
	return View;
}

void UAttendancePanelWidget::RefreshAll()
{
	const FAttendanceView View = BuildView();
	if (bClaimFxPlaying)
	{
		// 연출 중 새 값은 끝 프레임에 싣는다 — 격자는 메아리(0.24)가, 버튼·완료판은 교차(0.30~0.45)가 꺼내 쓴다
		PendingView = View;
		ApplyView(PendingView, EAttendanceApplyScope::ExceptGrid);
		return;
	}
	ApplyView(View);
	// 타이머 시작은 부작용이라 순수 ApplyView 밖에서 한다
	RetargetCountdown(View.ShowsClaimedPlate());
}

void UAttendancePanelWidget::ApplyView(const FAttendanceView& View, EAttendanceApplyScope Scope)
{
	if (Scope == EAttendanceApplyScope::All)
	{
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			if (!View.Days.IsValidIndex(Index))
			{
				continue;
			}
			const FAttendanceDayView& DayView = View.Days[Index];
			if (!DayView.bRowFound)
			{
				// 탭 해제가 먼저다 — SetTappable 이 가시성을 SelfHitTestInvisible 로 되돌려 빈 칸을 드러낸다
				Tiles[Index]->SetTappable(false);
				Tiles[Index]->SetVisibility(ESlateVisibility::Collapsed);
				TileKeys[Index] = FAttendanceTileKey();
				continue;
			}

			const FAttendanceTileKey Key{ DayView.Day, DayView.State, DayView.bMilestone, true };
			if (!(TileKeys[Index] == Key))
			{
				TileKeys[Index] = Key;

				Tiles[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				Tiles[Index]->SetDayView(DayView);
			}

			// 표시값이 그대로여도 연출 플래그는 바뀐다 — 잠금은 키 스킵 밖에서 매번 다시 건다 (R33)
			Tiles[Index]->SetTappable(DayView.State == EAttendanceTileState::Today && View.bCanClaim && !bClaimFxPlaying);
		}
	}

	ApplyProgress(View);

	if (ClaimStyle_Gold)
	{
		ClaimButton->SetStyle(ClaimStyle_Gold);
	}
	ClaimButton->SetButtonText(NSLOCTEXT("Attendance", "ClaimCta", "출석 보상 받기"));
	ApplyClaimedTitle();
	if (bClaimFxPlaying)
	{
		// 버튼·완료판 가시성은 시퀀스가 0.30~0.45 에 넘긴다 — 여기서 건드리면 교차가 끊긴다.
		// SetIsEnabled(알파 1.0 회색 브러시)도 SetIsInteractionEnabled(알파 0.5)도 t=0 에 튀므로 히트테스트만 끈다
		ClaimButton->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		const bool bShowClaimed = View.ShowsClaimedPlate();
		ClaimButton->SetVisibility(bShowClaimed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ClaimButton->SetRenderOpacity(1.f);
		// Style2 Disabled 는 알파 1.0 의 별도 회색 브러시(Pre-flight 실측 R13)라 서버 시간 미확인을 회색 판으로 알린다
		ClaimButton->SetIsEnabled(View.bCanClaim);
		ClaimedPlate->SetVisibility(bShowClaimed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ClaimedPlate->SetRenderOpacity(1.f);
	}
	// 펄스는 1 → 0.97 → 1 로 원 크기 아래에서만 숨쉰다(R29 개정) ― ClaimRow 클립 안에서 잘리지 않는다
	ClaimButton->SetPulseEnabled(View.bCanClaim && !bReduceMotion && !bClaimFxPlaying);
	ClaimShine->SetVisibility(Show(View.bCanClaim && !bReduceMotion));
}

void UAttendancePanelWidget::ApplyClaimedTitle()
{
	ClaimedTitle->SetText(NSLOCTEXT("Attendance", "ClaimedTitle", "출석 완료"));
}

void UAttendancePanelWidget::ApplyProgress(const FAttendanceView& View)
{
	const int32 Claimed = View.ClaimedCount();
	CycleText->SetText(FText::Format(
		NSLOCTEXT("Attendance", "CycleProgress", "{0} / {1}일"), Claimed, FAttendanceRules::CycleLength));
	CycleBar->SetPercent(FAttendanceTrackScale::FillPercent(Claimed, ProgressTrackWidth));

	// 받은 뒤에도 남는다 — 다음 특별 칸까지의 거리는 수령 전후가 같다 (스펙 §8.5-4)
	const FText Line = View.MilestoneLine();
	NextMilestoneText->SetText(Line);
	NextMilestoneText->SetVisibility(Show(!Line.IsEmpty()));

	UAttendanceMilestoneNodeWidget* const Nodes[] = {
		MilestoneNode0.Get(), MilestoneNode1.Get(), MilestoneNode2.Get(), MilestoneNode3.Get() };
	constexpr int32 NodeCount = static_cast<int32>(UE_ARRAY_COUNT(Nodes));
	int32 NodeIndex = 0;
	for (const FAttendanceDayView& Entry : View.Days)
	{
		if (!Entry.bRowFound || !Entry.bMilestone)
		{
			continue;
		}
		if (NodeIndex >= NodeCount)
		{
			// 원판은 4개만 저작돼 있다 — DT 에 특별 칸이 더 생기면 WBP 에 원판을 늘려야 한다
			UE_LOG(LogTemp, Warning, TEXT("출석 특별 칸이 원판 %d개보다 많습니다 ― %d일차 원판 생략"), NodeCount, Entry.Day);
			continue;
		}
		UAttendanceMilestoneNodeWidget* Node = Nodes[NodeIndex];
		if (UCanvasPanelSlot* NodeSlot = Cast<UCanvasPanelSlot>(Node->Slot))
		{
			// 가로만 막대 눈금으로 옮긴다 — 세로 앵커·정렬·위치는 WBP 가 원판 중심을 막대 중심에 맞춰 저작한다
			FAnchors Anchors = NodeSlot->GetAnchors();
			Anchors.Minimum.X = 0.f;
			Anchors.Maximum.X = 0.f;
			NodeSlot->SetAnchors(Anchors);
			FVector2D Alignment = NodeSlot->GetAlignment();
			Alignment.X = 0.5;
			NodeSlot->SetAlignment(Alignment);
			FVector2D Position = NodeSlot->GetPosition();
			Position.X = FAttendanceTrackScale::NodeCenterX(Entry.Day, ProgressTrackWidth);
			NodeSlot->SetPosition(Position);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("MilestoneNode%d 의 부모가 캔버스가 아닙니다 ― 막대 눈금에 못 놓습니다"), NodeIndex);
		}
		Node->SetNode(Entry.Day, View.MilestoneNodeState(Entry.Day));
		Node->SetVisibility(ESlateVisibility::HitTestInvisible);
		++NodeIndex;
	}
	for (; NodeIndex < NodeCount; ++NodeIndex)
	{
		Nodes[NodeIndex]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAttendancePanelWidget::RetargetCountdown(bool bClaimedPlateVisible)
{
	if (!bClaimedPlateVisible)
	{
		Countdown.Stop();
		bCountdownOnClaimedPlate = false;
		return;
	}
	if (bCountdownOnClaimedPlate)
	{
		Countdown.Refresh();
		return;
	}
	bCountdownOnClaimedPlate = true;
	Countdown.Start(GetWorld(), ClaimedCountdown.Get(),
		[this]() -> int64
		{
			UGameInstance* TickGameInst = GetGameInstance();
			const UGameClockSubsystem* Clock = TickGameInst ? TickGameInst->GetSubsystem<UGameClockSubsystem>() : nullptr;
			return Clock ? Clock->GetSecondsUntilDayRoll() : 0;
		},
		TEXT("다음 출석까지 "));
}

void UAttendancePanelWidget::HandleClaim()
{
	// 연출 창 안의 재입력은 매니저 거부 토스트까지 가기 전에 삼킨다(버튼은 골드로 남아 있다)
	if (bAwaitClaimFx || bClaimFxPlaying)
	{
		return;
	}

	UGameInstance* GameInst = GetGameInstance();
	UAttendanceSubsystem* Attendance = GameInst ? GameInst->GetSubsystem<UAttendanceSubsystem>() : nullptr;
	if (!Attendance)
	{
		return;
	}
	PendingClaimDay = Attendance->GetNextDay();
	bAwaitClaimFx = true;
	// 거부 토스트는 매니저가 낸다 — 실패하면 연출 예약을 즉시 거둔다
	if (!Attendance->ClaimToday())
	{
		bAwaitClaimFx = false;
		PendingClaimDay = 0;
	}
}

void UAttendancePanelWidget::HandleTileTapped(int32 Day)
{
	if (bAwaitClaimFx || bClaimFxPlaying)
	{
		return;
	}

	UGameInstance* GameInst = GetGameInstance();
	const UAttendanceSubsystem* Attendance = GameInst ? GameInst->GetSubsystem<UAttendanceSubsystem>() : nullptr;
	if (!Attendance || !IsServerTimeValidNow())
	{
		return;
	}
	// 탭은 오늘 칸에서만 수령으로 들어간다 — 칸의 잠금과 별개로 여기서도 판정한다(묵은 프레임 방어)
	if (Day != Attendance->GetNextDay() || !Attendance->CanClaimToday())
	{
		return;
	}

	// 사운드·연출·매니저 호출은 전부 CTA 와 같은 한 경로다(중복 재생 없음)
	HandleClaim();
}

void UAttendancePanelWidget::HandleAttendanceChanged()
{
	UGameInstance* GameInst = GetGameInstance();
	const UAttendanceSubsystem* Attendance = GameInst ? GameInst->GetSubsystem<UAttendanceSubsystem>() : nullptr;
	if (bAwaitClaimFx && Attendance && Attendance->HasClaimedToday())
	{
		bAwaitClaimFx = false;
		PlayClaimSequence(BuildView(), PendingClaimDay);
		return;
	}
	RefreshAll();
}

void UAttendancePanelWidget::HandleDayKey(const FString& /*NewDayKey*/)
{
	// 하루가 연출 창 안에서 넘어가면 시퀀스를 끝내고 새 날 기준으로 다시 그린다
	bClaimFxPlaying = false;
	ResetClaimShine();
	RefreshAll();
}

void UAttendancePanelWidget::PlayClaimSequence(const FAttendanceView& AfterClaim, int32 ClaimedDay)
{
	bClaimFxPlaying = true;
	ClaimFxElapsed = 0.f;
	bEchoFired = false;
	bSwapFired = false;
	PendingClaimDay = ClaimedDay;

	// 격자는 0.24s 까지 옛 프레임을 들고 있다 — 오늘 칸 탭은 여기서 곧바로 닫는다 (R33)
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		// Today 칸만 건드린다 — 접힌 칸(행 없음)에 SetTappable 을 걸면 SelfHitTestInvisible 로 되살아난다
		if (Tiles[Index] && TileKeys.IsValidIndex(Index) && TileKeys[Index].bApplied
			&& TileKeys[Index].State == EAttendanceTileState::Today)
		{
			Tiles[Index]->SetTappable(false);
		}
	}

	if (UGameInstance* GameInst = GetGameInstance())
	{
		// 전용 클립이 없으면 조용히 지나간다(DT 행의 Sound 가 비어 있다) — 죽은 태그가 아니다
		if (USoundManagerSubsystem* Sound = GameInst->GetSubsystem<USoundManagerSubsystem>())
		{
			Sound->PlayUISound(CGUISoundTags::AttendanceStamp);
		}
	}

	// 수령 후 프레임을 미리 든다 — 격자는 Scope 가, 버튼·완료판은 bClaimFxPlaying 가드가 0.24/0.30 까지 붙든다
	PendingView = AfterClaim;
	ApplyView(PendingView, EAttendanceApplyScope::ExceptGrid);
}

void UAttendancePanelWidget::TickClaimSequence(float DeltaTime)
{
	ClaimFxElapsed += DeltaTime;

	// 0.24 격자 메아리 — 해당 칸이 여기서 비로소 ClaimedToday 로 넘어가고 도장이 떨어져 붙는다
	if (!bEchoFired && ClaimFxElapsed >= EchoAt)
	{
		bEchoFired = true;
		const int32 Index = PendingClaimDay - 1;
		if (Tiles.IsValidIndex(Index) && Tiles[Index] && PendingView.Days.IsValidIndex(Index))
		{
			TileKeys[Index] = FAttendanceTileKey{ PendingView.Days[Index].Day, PendingView.Days[Index].State,
				PendingView.Days[Index].bMilestone, true };
			// SetDayView 가 받은 칸 층(판·곱색·도장 제자리)을 세운 뒤 메아리가 도장만 위에서 떨어뜨린다
			Tiles[Index]->SetDayView(PendingView.Days[Index]);
			Tiles[Index]->PlayStampEcho(bReduceMotion);
		}
	}

	// 0.30 → 0.45 크로스페이드. 가시성은 여기서 한 번만 바꾸고 이후는 RenderOpacity 로 섞는다.
	// 권위 시각을 잃은 프레임이면 완료판을 띄우지 않는다 — 버튼이 그대로 남고 끝에서 비활성으로 바뀐다
	if (!bSwapFired && ClaimFxElapsed >= SwapAt)
	{
		bSwapFired = true;
		if (PendingView.ShowsClaimedPlate())
		{
			ClaimedPlate->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ClaimedPlate->SetRenderOpacity(0.f);
			RetargetCountdown(true);
		}
	}
	if (bSwapFired && ClaimedPlate->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const float Blend = FMath::Clamp((ClaimFxElapsed - SwapAt) / SwapCrossfade, 0.f, 1.f);
		ClaimButton->SetRenderOpacity(1.f - Blend);
		ClaimedPlate->SetRenderOpacity(Blend);
	}

	if (ClaimFxElapsed >= ClaimFxDuration)
	{
		bClaimFxPlaying = false;
		PendingClaimDay = 0;
		// 끝 프레임 = 들고 있던 수령 후 프레임(연출 중 새로고침이 갱신해 둔다). 매니저를 다시 읽지 않아 하네스도 같은 길을 탄다
		ApplyView(PendingView);
		RetargetCountdown(PendingView.ShowsClaimedPlate());
	}
}

void UAttendancePanelWidget::TickClaimShine(float DeltaTime)
{
	ClaimShineTime += DeltaTime;

	if (ClaimShine->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const float Phase = FMath::Fmod(ClaimShineTime, ShinePeriod);
		if (Phase < ShineTravel)
		{
			const float T = Phase / ShineTravel;
			ClaimShine->SetRenderTranslation(FVector2D(FMath::Lerp(-ShineSweepHalf, ShineSweepHalf, T), 0.f));
			ClaimShine->SetRenderOpacity(0.6f * FMath::Sin(T * PI));
		}
		else
		{
			ClaimShine->SetRenderOpacity(0.f);
		}
	}
}

void UAttendancePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bClaimShineTickEnabled)
	{
		TickClaimShine(InDeltaTime);
	}
	if (bClaimFxPlaying)
	{
		TickClaimSequence(InDeltaTime);
	}
}
