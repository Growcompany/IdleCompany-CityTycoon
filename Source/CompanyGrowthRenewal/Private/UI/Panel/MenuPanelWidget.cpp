// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/MenuPanelWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "Components/Button.h"
#include "CommonTextBlock.h"
#include "UI/Element/Buttons/MenuTileWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/AttendanceSubsystem.h"
#include "Manager/QuestSubsystem.h"
#include "Manager/MailboxSubsystem.h"
#include "Manager/LiveOpsSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Table/MenuUnlockData.h"
#include "Player/MainMapPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UMenuPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UMenuPanelWidget::OnBackgroundClicked);
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UMenuPanelWidget::OnCancelButtonClicked);
	}
	if (SettingsBtn)
	{
		SettingsBtn->OnClicked.AddDynamic(this, &UMenuPanelWidget::OnSettingsButtonClicked);
	}

	TileRoutes.Reset();
	BindTile(AttendanceTile, EWidgetType::AttendancePanel, NAME_None);
	BindTile(QuestTile, EWidgetType::QuestPanel, NAME_None);
	BindTile(EventTile, EWidgetType::NewsPanel, NAME_None);
	BindTile(MailboxTile, EWidgetType::MailboxPanel, NAME_None);
	BindTile(AchievementTile, EWidgetType::QuestPanel, TEXT("Lifetime"));
	BindTile(CodexTile, EWidgetType::CodexPanel, NAME_None);
	BindTile(NoticeTile, EWidgetType::NewsPanel, TEXT("Notice"));
	BindTile(RankingTile, EWidgetType::RankingPanel, NAME_None);
	BindTile(ProfileTile, EWidgetType::ProfileImagePanel, NAME_None);
	// 4번째 그룹 "도전" — 목적지 화면이 DT_WidgetClass 에 아직 없으면 메뉴만 닫힌다(NativeOnDeactivated 의 null 가드)
	BindTile(SkylineTile, EWidgetType::SkylineScreen, NAME_None);
	BindTile(FusionTile, EWidgetType::FusionProjectPanel, NAME_None);
}

UWidget* UMenuPanelWidget::GetCloseButtonWidget() const
{
	return PanelHeader ? PanelHeader->GetCloseButton() : nullptr;
}

UWidget* UMenuPanelWidget::GetAttendanceTileWidget() const
{
	return AttendanceTile.Get();
}

UWidget* UMenuPanelWidget::GetMailboxTileWidget() const
{
	return MailboxTile.Get();
}

UWidget* UMenuPanelWidget::GetQuestTileWidget() const
{
	return QuestTile.Get();
}

bool UMenuPanelWidget::PlayPanelIntro()
{
	if (PanelIntroOverlay.IsValid())
	{
		return true;
	}
	UPanelIntroSubsystem* IntroMgr = (IsActivated() && GetGameInstance())
		? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	if (!IntroMgr)
	{
		return false;
	}
	// 함수 지역 static 이라 유니티 빌드 셰도잉이 없다
	static const FName IntroKey(TEXT("MenuPanel"));
	PanelIntroOverlay = IntroMgr->PlayIntro(this, IntroKey, /*bIgnoreSeen=*/true);
	if (UPanelIntroOverlayWidget* Overlay = PanelIntroOverlay.Get())
	{
		Overlay->OnIntroFinished.AddUObject(this, &UMenuPanelWidget::HandlePanelIntroFinished);
		return true;
	}
	return false;
}

void UMenuPanelWidget::HandlePanelIntroFinished()
{
	PanelIntroOverlay.Reset();
}

void UMenuPanelWidget::BindTile(UMenuTileWidget* Tile, EWidgetType Widget, FName Tab)
{
	if (!Tile)
	{
		return;
	}

	TileRoutes.Emplace(Tile, TPair<EWidgetType, FName>(Widget, Tab));
	Tile->OnClicked().AddWeakLambda(this, [this, Tile, Widget, Tab]()
	{
		if (Tile->IsLocked())
		{
			return;
		}
		OpenAfterClose(Widget, Tab);
	});
}

void UMenuPanelWidget::NativeDestruct()
{
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UMenuPanelWidget::OnBackgroundClicked);
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UMenuPanelWidget::OnCancelButtonClicked);
	}
	if (SettingsBtn)
	{
		SettingsBtn->OnClicked.RemoveDynamic(this, &UMenuPanelWidget::OnSettingsButtonClicked);
	}

	for (auto& Route : TileRoutes)
	{
		if (Route.Key)
		{
			Route.Key->OnClicked().RemoveAll(this);
		}
	}
	TileRoutes.Reset();

	Super::NativeDestruct();
}

void UMenuPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 메뉴 타일은 액션 버튼 — 선택 토글 비활성화
	for (auto& Route : TileRoutes)
	{
		Route.Key->SetIsSelectable(false);
		Route.Key->SetIsSelected(false);
	}

	UGameInstance* GameInst = GetGameInstance();
	if (!GameInst)
	{
		return;
	}

	if (UAttendanceSubsystem* Attendance = GameInst->GetSubsystem<UAttendanceSubsystem>())
	{
		Attendance->OnAttendanceChanged.AddUObject(this, &UMenuPanelWidget::RefreshBadges);
	}
	if (UQuestSubsystem* Quest = GameInst->GetSubsystem<UQuestSubsystem>())
	{
		Quest->OnQuestBoardChanged.AddUObject(this, &UMenuPanelWidget::RefreshBadges);
	}
	if (UMailboxSubsystem* Mailbox = GameInst->GetSubsystem<UMailboxSubsystem>())
	{
		Mailbox->OnMailboxChanged.AddUObject(this, &UMenuPanelWidget::RefreshBadges);
	}
	if (ULiveOpsSubsystem* LiveOps = GameInst->GetSubsystem<ULiveOpsSubsystem>())
	{
		LiveOps->OnLiveOpsContentChanged.AddUObject(this, &UMenuPanelWidget::RefreshBadges);
	}
	if (UGameClockSubsystem* Clock = GameInst->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnDayKeyChanged.AddUObject(this, &UMenuPanelWidget::HandleDayKey);
	}

	RefreshLocks();
	RefreshBadges();

	// 카운트다운은 표시 전용 1초 타이머 (데이터 폴링 아님) — 공용 FCountdownTicker
	Countdown.Start(GetWorld(), CountdownText,
		[this]() -> int64
		{
			UGameInstance* TickGameInst = GetGameInstance();
			const UGameClockSubsystem* Clock = TickGameInst ? TickGameInst->GetSubsystem<UGameClockSubsystem>() : nullptr;
			return Clock ? Clock->GetSecondsUntilDayRoll() : 0;
		},
		TEXT("일일 리셋 "));
}

void UMenuPanelWidget::NativeOnDeactivated()
{
	Countdown.Stop();

	// 인트로는 뷰포트 오버레이라 스택이 안 거둬간다 — RemoveFromParent → NativeDestruct 가 복구 신호를 쏜다
	if (UPanelIntroOverlayWidget* Intro = PanelIntroOverlay.Get())
	{
		Intro->RemoveFromParent();
	}
	PanelIntroOverlay.Reset();

	UGameInstance* GameInst = GetGameInstance();
	if (GameInst)
	{
		if (UAttendanceSubsystem* Attendance = GameInst->GetSubsystem<UAttendanceSubsystem>())
		{
			Attendance->OnAttendanceChanged.RemoveAll(this);
		}
		if (UQuestSubsystem* Quest = GameInst->GetSubsystem<UQuestSubsystem>())
		{
			Quest->OnQuestBoardChanged.RemoveAll(this);
		}
		if (UMailboxSubsystem* Mailbox = GameInst->GetSubsystem<UMailboxSubsystem>())
		{
			Mailbox->OnMailboxChanged.RemoveAll(this);
		}
		if (ULiveOpsSubsystem* LiveOps = GameInst->GetSubsystem<ULiveOpsSubsystem>())
		{
			LiveOps->OnLiveOpsContentChanged.RemoveAll(this);
		}
		if (UGameClockSubsystem* Clock = GameInst->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnDayKeyChanged.RemoveAll(this);
		}
	}

	Super::NativeOnDeactivated();

	if (GameInst && PendingOpenWidget != EWidgetType::None)
	{
		const EWidgetType WidgetToOpen = PendingOpenWidget;
		const FName Tab = PendingTab;
		PendingOpenWidget = EWidgetType::None;
		PendingTab = NAME_None;

		UUIManagerSubsystem* UIMgr = GameInst->GetSubsystem<UUIManagerSubsystem>();
		UTableManagerSubsystem* TableMgr = GameInst->GetSubsystem<UTableManagerSubsystem>();
		if (UIMgr && UIMgr->GetUIBase() && TableMgr)
		{
			if (TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(WidgetToOpen))
			{
				// 탭 힌트는 push 직전에 둔다 — 열리는 창이 NativeOnActivated 에서 소비한다
				if (!Tab.IsNone())
				{
					UIMgr->SetPendingPanelTab(Tab);
				}
				UCommonActivatableWidget* PushedWidget = nullptr;
				// 스카이라인은 HUD 를 덮는 전용 화면이라 프롬프트가 아닌 메뉴 스택으로 간다 (사이드 컨텐츠 스펙 v0.6.1 §1.3)
				if (WidgetToOpen == EWidgetType::SkylineScreen)
				{
					PushedWidget = UIMgr->GetUIBase()->PushMenuClass(TSubclassOf<UCommonActivatableWidget>(WidgetClass));
				}
				else
				{
					PushedWidget = UIMgr->GetUIBase()->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(WidgetClass));
				}
				if (PushedWidget)
				{
					return;
				}
				if (!Tab.IsNone())
				{
					UIMgr->SetPendingPanelTab(NAME_None);
				}
			}
		}
	}

	// 다음 위젯이 없으면 UI 모드 해제
	if (UWorld* OwningWorld = GetWorld())
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(OwningWorld->GetFirstPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}
}

void UMenuPanelWidget::OpenAfterClose(EWidgetType Widget, FName Tab)
{
	// 닫기 애니 후 NativeOnDeactivated 에서 push (스택 규칙: 닫기 먼저)
	PendingOpenWidget = Widget;
	PendingTab = Tab;
	ClosePanel();
}

void UMenuPanelWidget::RefreshLocks()
{
	UGameInstance* GameInst = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GameInst ? GameInst->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	USaveLoadManager* SaveMgr = GameInst ? GameInst->GetSubsystem<USaveLoadManager>() : nullptr;
	if (!TableMgr || !SaveMgr)
	{
		return;
	}

	for (auto& Route : TileRoutes)
	{
		bool bFound = false;
		const FMenuUnlockData Data = TableMgr->GetMenuUnlockData(Route.Key->GetFName(), bFound);
		const bool bLocked = bFound && SaveMgr->GetHQLevel() < Data.RequiredHQLevel;
		Route.Key->SetLocked(bLocked, bFound ? Data.LockReasonText : FText::GetEmpty());
	}
}

void UMenuPanelWidget::RefreshBadges()
{
	UGameInstance* GameInst = GetGameInstance();
	if (!GameInst)
	{
		return;
	}

	auto Badge = [](UMenuTileWidget* Tile, bool bOn)
	{
		if (!Tile || Tile->IsLocked())
		{
			return;
		}
		if (bOn) { Tile->ShowBadge(); } else { Tile->HideBadge(); }
	};

	const UAttendanceSubsystem* Attendance = GameInst->GetSubsystem<UAttendanceSubsystem>();
	const UQuestSubsystem* Quest = GameInst->GetSubsystem<UQuestSubsystem>();
	const UMailboxSubsystem* Mailbox = GameInst->GetSubsystem<UMailboxSubsystem>();
	const ULiveOpsSubsystem* LiveOps = GameInst->GetSubsystem<ULiveOpsSubsystem>();

	Badge(AttendanceTile, Attendance && Attendance->CanClaimToday());
	Badge(QuestTile, Quest && (Quest->HasClaimable(EQuestPeriod::Daily) || Quest->HasClaimable(EQuestPeriod::Weekly)));
	Badge(AchievementTile, Quest && Quest->HasClaimable(EQuestPeriod::Lifetime));
	Badge(MailboxTile, Mailbox && Mailbox->GetUnclaimedCount() > 0);
	Badge(EventTile, LiveOps && LiveOps->HasClaimableEvent());
	Badge(NoticeTile, LiveOps && LiveOps->HasUnreadNotice());
}

void UMenuPanelWidget::HandleDayKey(const FString& /*NewDayKey*/)
{
	// 날짜가 넘어가면 받을 것과 해금 조건이 동시에 바뀐다 — 잠금이 먼저여야 잠긴 타일에 뱃지가 안 남는다(활성화와 같은 순서)
	RefreshLocks();
	RefreshBadges();
}

void UMenuPanelWidget::OnBackgroundClicked()
{
	ClosePanel();
}

void UMenuPanelWidget::OnCancelButtonClicked()
{
	ClosePanel();
}

void UMenuPanelWidget::OnSettingsButtonClicked()
{
	OpenAfterClose(EWidgetType::SettingsPanel);
}

void UMenuPanelWidget::ClosePanel()
{
	CloseWithAnimation();
}
