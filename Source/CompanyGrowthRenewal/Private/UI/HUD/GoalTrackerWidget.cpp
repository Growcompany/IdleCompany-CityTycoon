#include "UI/HUD/GoalTrackerWidget.h"
#include "UI/Element/GoalTrackerRowWidget.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

// DT_PanelIntro.PanelKey / AnchorName. 유니티 빌드 셰도잉을 피하려 이름을 길게 둔다
static const FName PanelIntroKey_MissionTracker(TEXT("MissionTracker"));
static const FName PanelIntroAnchor_MissionTrackerFirstRow(TEXT("IntroFirstRow"));

namespace CGRGoalTrackerDisplay
{
// 접힘 시 항상 보이는 상위 행 수
static constexpr int32 AlwaysVisibleTopCount = 3;
// 표시 대상이 이 수 미만이면 접지 않는다 — 부담이 큰 구간(언락 직후)에만 개입한다
static constexpr int32 CollapseThreshold = 6;

// 수령 완료/잠금을 걷어내고 표시 집합·완료 수·숨김 수를 함께 낸다.
// ⚠ 완료 수는 분모(Total)에서 빼서 유도하지 않는다 — 언락 전/치트 리셋 직후 GetBoardEntries 가 빈 배열을
//    주므로 뺄셈이면 빈 리스트 위에 "10/10 완료" 가 뜬다. 입력에서 Claimed 를 직접 센다
void BuildDisplaySet(
	const TArray<FGoalBoardEntry>& InAllEntries,
	FName InTrackedGoalID,
	FName InExpandedGoalID,
	bool bInRowsExpanded,
	TArray<FGoalBoardEntry>& OutVisible,
	int32& OutClaimedCount,
	int32& OutHiddenCount)
{
	OutVisible.Reset();
	OutClaimedCount = 0;
	OutHiddenCount = 0;

	TArray<FGoalBoardEntry> Eligible;
	Eligible.Reserve(InAllEntries.Num());

	for (const FGoalBoardEntry& Entry : InAllEntries)
	{
		if (Entry.State == EGoalState::Claimed)
		{
			++OutClaimedCount;
			continue;
		}

		// 잠금은 선행을 수령하면 그 자리에 등장한다 — 미리 보여줄 정보 가치가 없다
		if (Entry.State != EGoalState::Locked)
		{
			Eligible.Add(Entry);
		}
	}

	if (bInRowsExpanded || Eligible.Num() < CollapseThreshold)
	{
		OutVisible = MoveTemp(Eligible);
		return;
	}

	for (int32 Index = 0; Index < Eligible.Num(); ++Index)
	{
		const FGoalBoardEntry& Entry = Eligible[Index];
		// Claimable 을 빼면 [수령] 버튼이 행 안에 있어 수령 경로가 통째로 막힌다.
		// 추적/펼침 행도 같은 이유 — 사라지면 [안내 끄기]와 펼친 카드에 도달할 수 없다
		const bool bAlwaysVisible =
			Index < AlwaysVisibleTopCount
			|| Entry.State == EGoalState::Claimable
			|| Entry.GoalID == InTrackedGoalID
			|| Entry.GoalID == InExpandedGoalID;

		if (bAlwaysVisible)
		{
			OutVisible.Add(Entry);
		}
		else
		{
			++OutHiddenCount;
		}
	}

	// 상위 3 이 무조건 걸리므로 구조적으로 불가능하다 — 상수를 바꿔 깨졌을 때 빈 목록 대신 전량을 그린다
	if (OutVisible.Num() == 0 && Eligible.Num() > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoalTracker] 표시 집합이 비었다 — 전량 노출로 폴백 (표시 대상 %d)"), Eligible.Num());
		OutVisible = MoveTemp(Eligible);
		OutHiddenCount = 0;
	}
}
}

UGoalBoardSubsystem* UGoalTrackerWidget::GetBoard() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UGoalBoardSubsystem>() : nullptr;
}

void UGoalTrackerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (FoldButton)
	{
		FoldButton->OnClicked.AddDynamic(this, &UGoalTrackerWidget::HandleFoldClicked);
	}
	if (UnfoldButton)
	{
		UnfoldButton->OnClicked.AddDynamic(this, &UGoalTrackerWidget::HandleUnfoldClicked);
	}
	if (MoreButton)
	{
		MoreButton->OnClicked.AddDynamic(this, &UGoalTrackerWidget::HandleMoreClicked);
	}

	// 재부착 자가치유 — NativeDestruct 가 보드 구독을 이미 끊어 놨으므로 여기서 스스로 복원해야 한다(InitTracker 는 idempotent)
	InitTracker();
}

void UGoalTrackerWidget::NativeDestruct()
{
	if (FoldButton)
	{
		FoldButton->OnClicked.RemoveDynamic(this, &UGoalTrackerWidget::HandleFoldClicked);
	}
	if (UnfoldButton)
	{
		UnfoldButton->OnClicked.RemoveDynamic(this, &UGoalTrackerWidget::HandleUnfoldClicked);
	}
	if (MoreButton)
	{
		MoreButton->OnClicked.RemoveDynamic(this, &UGoalTrackerWidget::HandleMoreClicked);
	}
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->OnGoalBoardChanged.RemoveAll(this);
		Board->OnTrackedGoalChanged.RemoveAll(this);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IntroTimer);
	}
	// 코치마크는 뷰포트에 따로 붙어 있어 트래커가 내려가도 살아남는다 — 앵커 잃은 딤이 화면에 남지 않게 같이 걷는다
	if (UPanelIntroOverlayWidget* Overlay = IntroOverlay.Get())
	{
		Overlay->RemoveFromParent();
	}
	Super::NativeDestruct();
}

void UGoalTrackerWidget::InitTracker()
{
	UGoalBoardSubsystem* Board = GetBoard();
	if (!Board)
	{
		UE_LOG(LogTemp, Error, TEXT("[GoalTracker] GoalBoardSubsystem 없음 — 미션 트래커를 그릴 수 없습니다"));
		return;
	}

	// 레벨마다 재생성되므로 중복 구독 가드 필수
	Board->OnGoalBoardChanged.RemoveAll(this);
	Board->OnTrackedGoalChanged.RemoveAll(this);
	Board->OnGoalBoardChanged.AddUObject(this, &UGoalTrackerWidget::HandleBoardChanged);
	Board->OnTrackedGoalChanged.AddUObject(this, &UGoalTrackerWidget::HandleTrackedChanged);

	RefreshAll();
	TryScheduleIntro();
}

void UGoalTrackerWidget::TryScheduleIntro()
{
	UGameInstance* GI = GetGameInstance();
	UPanelIntroSubsystem* IntroMgr = GI ? GI->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	UGoalBoardSubsystem* Board = GetBoard();
	if (!IntroMgr || !Board || !GetWorld() || !IntroMgr->ShouldPlay(PanelIntroKey_MissionTracker))
	{
		return;
	}
	// 접힌 상태(미니 칩)면 설명할 목록이 화면에 없다 — 본 것으로 치지 않고 다음 재부착 때 다시 노린다
	if (Board->IsTrackerFolded())
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		IntroTimer, this, &UGoalTrackerWidget::TryPlayIntro, IntroPlayDelay, false);
}

void UGoalTrackerWidget::TryPlayIntro()
{
	UGameInstance* GI = GetGameInstance();
	UPanelIntroSubsystem* IntroMgr = GI ? GI->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!IntroMgr || !TableMgr || !IntroMgr->ShouldPlay(PanelIntroKey_MissionTracker))
	{
		return;
	}

	// 대기 중 전량 수령/체인 재점화로 목록이 비었을 수 있다 — 가리킬 카드가 없으면 설명도 없다
	UGoalTrackerRowWidget* FirstRow = GetFirstVisibleRow();
	if (!FirstRow)
	{
		return;
	}

	// 스텝 2가 카드 안의 [안내하기]/[수령]을 설명하므로 카드가 펼쳐진 상태여야 한다.
	// 오버레이엔 스텝 전환 훅이 없어 시작 시점에 한 번에 연다 (스텝 1의 구멍은 목록 전체라 이질감이 없다)
	if (ExpandedGoalID.IsNone())
	{
		ExpandedGoalID = FirstRow->GetGoalID();
		RefreshAll();
	}

	TSubclassOf<UUserWidget> OverlayClass = TableMgr->GetWidgetClass(EWidgetType::PanelIntroOverlay);
	if (!OverlayClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PanelIntro] EWidgetType::PanelIntroOverlay 미등록 — 미션 목록 안내 생략"));
		return;
	}
	UPanelIntroOverlayWidget* Overlay = CreateWidget<UPanelIntroOverlayWidget>(GetOwningPlayer(), OverlayClass);
	if (!Overlay)
	{
		return;
	}

	Overlay->AddToViewport(9100); // 미션 가이드(9000) 위
	Overlay->OnIntroFinished.AddUObject(this, &UGoalTrackerWidget::HandleIntroFinished);
	// 행은 런타임 생성이라 WidgetTree 조회로 못 찾는다 — StartIntro 전에 걸어야 첫 스텝부터 먹는다
	FPanelIntroAnchorResolver Resolver;
	Resolver.BindUObject(this, &UGoalTrackerWidget::ResolveIntroAnchor);
	Overlay->SetAnchorResolver(Resolver);

	if (!Overlay->StartIntro(PanelIntroKey_MissionTracker, this))
	{
		Overlay->RemoveFromParent();
		return;
	}
	IntroOverlay = Overlay;

	// 딤 2겹을 플래그로 협상하지 않고 가시성으로 배제한다 (패널 인트로 설계 §3.3)
	TArray<UUserWidget*> FoundOverlays;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundOverlays, UMissionGuideOverlayWidget::StaticClass(), false);
	for (UUserWidget* Found : FoundOverlays)
	{
		if (Found && Found->GetVisibility() != ESlateVisibility::Collapsed)
		{
			HiddenMissionOverlay = Found;
			Found->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
	}
}

void UGoalTrackerWidget::HandleIntroFinished()
{
	if (UWidget* Mission = HiddenMissionOverlay.Get())
	{
		// 미션 오버레이 루트는 HitTestInvisible(입력 통과) — Visible 로 되돌리면 화면을 먹는다
		Mission->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	HiddenMissionOverlay.Reset();
	IntroOverlay.Reset();
}

UWidget* UGoalTrackerWidget::ResolveIntroAnchor(FName AnchorName) const
{
	if (AnchorName == PanelIntroAnchor_MissionTrackerFirstRow)
	{
		return GetFirstVisibleRow();
	}
	// 나머지 이름은 오버레이가 패널 WidgetTree 에서 직접 찾는다
	return nullptr;
}

UGoalTrackerRowWidget* UGoalTrackerWidget::GetFirstVisibleRow() const
{
	// 풀은 표시 순서대로 채워지고 남는 행만 뒤에서 Collapsed 된다
	for (UGoalTrackerRowWidget* Row : RowPool)
	{
		if (Row && Row->GetVisibility() != ESlateVisibility::Collapsed)
		{
			return Row;
		}
	}
	return nullptr;
}

void UGoalTrackerWidget::RefreshAll()
{
	UGoalBoardSubsystem* Board = GetBoard();
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Board || !RowBox)
	{
		return;
	}

	TArray<FGoalBoardEntry> AllEntries;
	Board->GetBoardEntries(AllEntries);

	// 총 미션 수의 소유자는 DT — 행이 늘어도 카운트가 어긋나거나 음수가 되지 않는다
	const int32 Total = TableMgr ? TableMgr->GetAllGoalData().Num() : AllEntries.Num();
	const FName TrackedID = Board->GetTrackedGoalID();
	const bool bRowsExpanded = Board->AreRowsExpanded();

	TArray<FGoalBoardEntry> Entries;
	int32 ClaimedCount = 0;
	int32 HiddenCount = 0;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		AllEntries, TrackedID, ExpandedGoalID, bRowsExpanded,
		Entries, ClaimedCount, HiddenCount);

	// 리스트에서 사라진 행의 ID 가 남아 있으면 다음 펼침이 죽은 ID 를 기준으로 돈다
	if (!ExpandedGoalID.IsNone() && !Entries.ContainsByPredicate(
		[this](const FGoalBoardEntry& E) { return E.GoalID == ExpandedGoalID; }))
	{
		ExpandedGoalID = NAME_None;
	}

	int32 ClaimableCount = 0;
	for (const FGoalBoardEntry& E : Entries)
	{
		if (E.State == EGoalState::Claimable)
		{
			++ClaimableCount;
		}
	}

	const bool bFolded = Board->IsTrackerFolded();
	if (ExpandedRoot)
	{
		ExpandedRoot->SetVisibility(bFolded ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (FoldedRoot)
	{
		FoldedRoot->SetVisibility(bFolded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	const FText CountText = FText::FromString(FString::Printf(TEXT("%d/%d"), ClaimedCount, Total));
	if (HeaderCountText)
	{
		HeaderCountText->SetText(FText::Format(NSLOCTEXT("GoalTracker", "HeaderCount", "{0} 완료"), CountText));
	}
	if (FoldedCountText)
	{
		FoldedCountText->SetText(CountText);
	}
	if (ClaimBadgeRoot)
	{
		ClaimBadgeRoot->SetVisibility(ClaimableCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ClaimBadgeText)
	{
		// 0 일 때 비우지 않으면 배지 루트가 WBP 배치 실수로 안 숨겨졌을 때 옛 숫자가 남는다
		ClaimBadgeText->SetText(ClaimableCount > 0 ? FText::AsNumber(ClaimableCount) : FText::GetEmpty());
	}

	if (MoreButton)
	{
		// 숨긴 게 없고 펼침 상태도 아니면 버튼 자체가 의미 없다
		const bool bShowMore = HiddenCount > 0 || bRowsExpanded;
		MoreButton->SetVisibility(bShowMore ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (MoreLabel)
	{
		MoreLabel->SetText(bRowsExpanded
			? NSLOCTEXT("GoalTracker", "MoreCollapse", "접기 ▲")
			: FText::Format(
				NSLOCTEXT("GoalTracker", "MoreExpand", "다른 미션 {0}개 ▼"),
				FText::AsNumber(HiddenCount)));
	}

	// 접힘 상태면 행은 그릴 필요 없다 (다음 펼침에서 다시 그린다)
	if (bFolded)
	{
		return;
	}

	// 추적 목표가 이 맵에서 안 되는 것이면 갈 곳 한 단어 — 행의 태그 칩이 "안내 중" 대신 이걸 단다.
	// 맵마다 트래커가 새로 만들어지므로 진입 즉시 정확한 값으로 다시 그려진다(살아있는 동안은 안 변한다)
	FText PlaceHint;
	if (!TrackedID.IsNone())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
			{
				PlaceHint = MissionMgr->GetTrackedGoalPlaceHint();
			}
		}
	}

	// 행 풀 — 부족하면 만들고, 남으면 Collapsed
	const TSubclassOf<UUserWidget> RowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::GoalTrackerRow) : nullptr;
	if (!RowClass)
	{
		// 매 이벤트 반복을 막는 1회 래치 — 미등록은 세션 내내 안 바뀌므로 첫 1회만 알린다
		if (!bLoggedMissingRowClass)
		{
			bLoggedMissingRowClass = true;
			UE_LOG(LogTemp, Error, TEXT("[GoalTracker] GoalTrackerRow 위젯 클래스 없음 (DT_WidgetClass 행 확인)"));
		}
		return;
	}

	// 언락 후 첫 렌더에서만. 맵 전환 재생성에는 다시 재생하지 않는다
	const bool bPlayIntro = !bFolded && Board->ConsumeEntranceOnce();

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (!RowPool.IsValidIndex(Index))
		{
			UGoalTrackerRowWidget* NewRow = CreateWidget<UGoalTrackerRowWidget>(GetOwningPlayer(), RowClass);
			if (!NewRow)
			{
				// continue 하면 RowPool.Num() 과 Index 가 갈려 아래 RowPool[Index] 가 범위 밖이 된다
				UE_LOG(LogTemp, Error, TEXT("[GoalTracker] GoalTrackerRow 생성 실패 — 등록 클래스가 UGoalTrackerRowWidget 파생인지 확인 (DT_WidgetClass)"));
				break;
			}
			// 인텐트 구독은 생성 시 1회 (행 인스턴스는 재사용되며 파괴되지 않는다)
			NewRow->OnRowTapped.AddUObject(this, &UGoalTrackerWidget::HandleRowTapped);
			NewRow->OnStartRequested.AddUObject(this, &UGoalTrackerWidget::HandleStartRequested);
			NewRow->OnStopRequested.AddUObject(this, &UGoalTrackerWidget::HandleStopRequested);
			NewRow->OnClaimRequested.AddUObject(this, &UGoalTrackerWidget::HandleClaimRequested);
			RowPool.Add(NewRow);
			if (UVerticalBoxSlot* BoxSlot = RowBox->AddChildToVerticalBox(NewRow))
			{
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			}
		}

		UGoalTrackerRowWidget* Row = RowPool[Index];
		if (!Row)
		{
			continue;
		}

		// 잠금 사유에 쓸 선행 미션 제목 — 코드가 조사를 붙이지 않도록 제목만 넘긴다
		FText PrereqTitle;
		if (Entries[Index].State == EGoalState::Locked && TableMgr)
		{
			FGoalTable PrereqRow;
			if (TableMgr->GetGoalData(Entries[Index].Row.PrereqGoalID, PrereqRow))
			{
				PrereqTitle = PrereqRow.Title;
			}
		}

		Row->SetVisibility(ESlateVisibility::Visible);
		Row->Configure(Entries[Index], Entries[Index].GoalID == TrackedID, Entries[Index].GoalID == ExpandedGoalID, PrereqTitle, PlaceHint);
		if (bPlayIntro)
		{
			Row->PlayIntro(Index);
		}
	}

	for (int32 Index = Entries.Num(); Index < RowPool.Num(); ++Index)
	{
		if (RowPool[Index])
		{
			RowPool[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGoalTrackerWidget::HandleFoldClicked()
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		ExpandedGoalID = NAME_None; // 접으면 펼친 행도 닫는다 (다시 펼칠 때 리스트부터 보여야 함)
		Board->SetTrackerFolded(true);
	}
}

void UGoalTrackerWidget::HandleUnfoldClicked()
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->SetTrackerFolded(false);
	}
}

void UGoalTrackerWidget::HandleMoreClicked()
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		const bool bWasExpanded = Board->AreRowsExpanded();
		// 접을 때는 펼친 행도 닫는다 — 사라질 행이 펼쳐진 채 남지 않게 (헤더 [접기] 와 동형)
		if (bWasExpanded)
		{
			ExpandedGoalID = NAME_None;
		}
		Board->SetRowsExpanded(!bWasExpanded);
	}
}

void UGoalTrackerWidget::HandleBoardChanged()
{
	RefreshAll();
}

void UGoalTrackerWidget::HandleTrackedChanged(FName /*TrackedID*/)
{
	RefreshAll();
}

void UGoalTrackerWidget::HandleRowTapped(FName GoalID)
{
	ExpandedGoalID = (ExpandedGoalID == GoalID) ? NAME_None : GoalID;
	RefreshAll();

	// 펼친 카드가 화면 아래로 나가지 않도록 뷰로 끌어온다
	if (RowScrollBox && ExpandedGoalID != NAME_None)
	{
		for (UGoalTrackerRowWidget* Row : RowPool)
		{
			if (Row && Row->GetGoalID() == ExpandedGoalID)
			{
				RowScrollBox->ScrollWidgetIntoView(Row, /*bAnimateScroll=*/true);
				break;
			}
		}
	}
}

void UGoalTrackerWidget::HandleStartRequested(FName GoalID)
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->SetTrackedGoal(GoalID);
	}
}

void UGoalTrackerWidget::HandleStopRequested(FName /*GoalID*/)
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->SetTrackedGoal(NAME_None);
	}
}

void UGoalTrackerWidget::HandleClaimRequested(FName GoalID)
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->ClaimGoal(GoalID); // 성공 시 내부에서 브로드캐스트 → RefreshAll 이 뒤따른다
	}
}
