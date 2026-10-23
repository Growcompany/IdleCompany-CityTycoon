// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/BuildOpenWidget.h"

#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "CommonActivatableWidget.h"
#include "CommonTextBlock.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "Manager/AttendanceSubsystem.h"
#include "Manager/QuestSubsystem.h"
#include "Manager/MailboxSubsystem.h"
#include "Manager/LiveOpsSubsystem.h"
#include "Enum/WidgetType.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"
#include "UI/Panel/FactoryPanelInteractionPolicy.h"
#include "Entity/Factory/BrickFactory.h"
#include "Data/FactorySaveData.h"
#include "Data/WorldMapTypes.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UBuildOpenWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBuildOpenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	BuildOpenButton->OnClicked().AddUObject(this, &UBuildOpenWidget::OnBuildOpenButtonClicked);
	FactoryOpenButton->OnClicked().AddUObject(this, &UBuildOpenWidget::OnFactoryOpenButtonClicked);
	HeadquartersBtn->OnClicked().AddUObject(this, &UBuildOpenWidget::OnHeadquartersButtonClicked);

	if (MenuBtn)
	{
		MenuBtn->OnClicked.AddDynamic(this, &UBuildOpenWidget::OnMenuButtonClicked);
	}

	if (ShopBtn)
	{
		ShopBtn->OnClicked.AddDynamic(this, &UBuildOpenWidget::OnShopButtonClicked);
	}

	if (WorldMapBtn)
	{
		WorldMapBtn->OnClicked.AddDynamic(this, &UBuildOpenWidget::OnWorldMapButtonClicked);
	}

	if (GachaBtn)
	{
		GachaBtn->OnClicked.AddDynamic(this, &UBuildOpenWidget::OnGachaButtonClicked);
	}

	if (QuestBtn)
	{
		QuestBtn->OnClicked.AddDynamic(this, &UBuildOpenWidget::OnQuestButtonClicked);
	}


	// ── AlertMark: 참조 캐싱 + 델리게이트 구독 + 타이머 시작 ──
	if (UGameInstance* GI = GetGameInstance())
	{
		CachedSaveMgr = GI->GetSubsystem<USaveLoadManager>();
	}
	CachedFactory = Cast<ABrickFactory>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass()));

	if (IsValid(CachedSaveMgr))
	{
		CachedSaveMgr->OnHQLevelUp.AddDynamic(this, &UBuildOpenWidget::HandleHQLeveledUp);
	}
	if (IsValid(CachedFactory))
	{
		CachedFactory->OnFactoryUpgraded.AddDynamic(this, &UBuildOpenWidget::HandleFactoryUpgraded);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AlertMarkPollHandle,
			FTimerDelegate::CreateUObject(this, &UBuildOpenWidget::RefreshAlertMarks),
			2.0f,
			true);
	}

	RefreshAlertMarks();

	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		const EInputMode ExistingMode = PC->GetCurrentInputMode();
		const EInputMode ActivationMode = CGRFactoryPanelInteraction::ResolveOpeningInputMode(ExistingMode);
		if (ExistingMode != ActivationMode)
		{
			PC->GoToNormalMode();
		}
	}

	// Show/Hide 는 같은 RenderTransform.Translation 을 잡는 별개 플레이어라 서로 안 멈춤 →
	// 반대편을 먼저 끊지 않으면 늦게 끝나는 쪽이 최종 위치를 덮어써 도크가 중간에 앉는다.
	if (HideButtonsAnim)
	{
		StopAnimation(HideButtonsAnim);
	}
	if (ShowButtonsAnim)
	{
		PlayAnimation(ShowButtonsAnim);
	}

	// 미션 가이드가 [건설] 버튼을 하이라이트 타겟으로 쓸 수 있게 등록
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->RegisterBuildOpenWidget(this);
	}
}

void UBuildOpenWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// 필수 BindWidget도 BP 리컴파일 재인스턴싱 중 Deactivate에선 null일 수 있음 (2026-07-04 에디터 크래시 이력)
	if (BuildOpenButton) { BuildOpenButton->OnClicked().Clear(); }
	if (FactoryOpenButton) { FactoryOpenButton->OnClicked().Clear(); }
	if (HeadquartersBtn) { HeadquartersBtn->OnClicked().Clear(); }


	if (MenuBtn)
	{
		MenuBtn->OnClicked.RemoveDynamic(this, &UBuildOpenWidget::OnMenuButtonClicked);
	}

	if (ShopBtn)
	{
		ShopBtn->OnClicked.RemoveDynamic(this, &UBuildOpenWidget::OnShopButtonClicked);
	}

	if (WorldMapBtn)
	{
		WorldMapBtn->OnClicked.RemoveDynamic(this, &UBuildOpenWidget::OnWorldMapButtonClicked);
	}

	if (GachaBtn)
	{
		GachaBtn->OnClicked.RemoveDynamic(this, &UBuildOpenWidget::OnGachaButtonClicked);
	}

	if (QuestBtn)
	{
		QuestBtn->OnClicked.RemoveDynamic(this, &UBuildOpenWidget::OnQuestButtonClicked);
	}

	// AlertMark 구독/타이머 해제
	if (IsValid(CachedSaveMgr))
	{
		CachedSaveMgr->OnHQLevelUp.RemoveDynamic(this, &UBuildOpenWidget::HandleHQLeveledUp);
	}
	if (IsValid(CachedFactory))
	{
		CachedFactory->OnFactoryUpgraded.RemoveDynamic(this, &UBuildOpenWidget::HandleFactoryUpgraded);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AlertMarkPollHandle);
	}
	CachedSaveMgr = nullptr;
	CachedFactory = nullptr;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			MissionMgr->UnregisterBuildOpenWidget(this);
		}
	}

	// 슬라이드아웃 애니메이션 재생 (반대편 정리는 NativeOnActivated 주석 참조)
	if (ShowButtonsAnim)
	{
		StopAnimation(ShowButtonsAnim);
	}
	if (HideButtonsAnim)
	{
		PlayAnimation(HideButtonsAnim);
	}
}

void UBuildOpenWidget::RefreshAlertMarks()
{
	// IconWithButton 의 내장 Badge API 사용 — AlertMark 가 버튼 WBP 내부에 중첩되어 있어
	// BindWidgetOptional 로는 도달 불가, Cast 로 버튼에 위임.
	if (UIconWithButtonWidget* HQBtn = Cast<UIconWithButtonWidget>(HeadquartersBtn))
	{
		const bool bCanLevelUp = IsValid(CachedSaveMgr) && CachedSaveMgr->CanLevelUpHQ();
		if (bCanLevelUp) { HQBtn->ShowBadge(); } else { HQBtn->HideBadge(); }
	}

	if (UIconWithButtonWidget* FactoryBtn = Cast<UIconWithButtonWidget>(FactoryOpenButton))
	{
		const bool bCanUpgrade = IsValid(CachedFactory) && CachedFactory->CanUpgradeAny();
		if (bCanUpgrade) { FactoryBtn->ShowBadge(); } else { FactoryBtn->HideBadge(); }
	}

	UGameInstance* MenuGI = GetGameInstance();
	const UQuestSubsystem* Quest = MenuGI ? MenuGI->GetSubsystem<UQuestSubsystem>() : nullptr;
	// 튜토리얼 중엔 메뉴·미션을 누를 수 없다 ― 시작부터 켜진 점(출석·공지·출시 우편)이 누르지 못할 곳을 계속 가리킨다
	const UMissionManagerSubsystem* MenuMissions = MenuGI ? MenuGI->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
	const bool bTutorialActive = MenuMissions && MenuMissions->HasActiveMission();
	RefreshQuestBadge(bTutorialActive || !Quest ? 0 : Quest->GetClaimableCount());

	// 메뉴 허브 도트 — 메뉴 시트 안 5 매니저 중 하나라도 받을 것이 있으면 켠다 (타일별 판정은 시트가 한다)
	if (MenuAlertMark)
	{
		const UAttendanceSubsystem* Attendance = MenuGI ? MenuGI->GetSubsystem<UAttendanceSubsystem>() : nullptr;
		const UMailboxSubsystem* Mailbox = MenuGI ? MenuGI->GetSubsystem<UMailboxSubsystem>() : nullptr;
		const ULiveOpsSubsystem* LiveOps = MenuGI ? MenuGI->GetSubsystem<ULiveOpsSubsystem>() : nullptr;
		const bool bAnyClaimable = !bTutorialActive && (
			(Attendance && Attendance->CanClaimToday())
			|| (Quest && Quest->HasAnyClaimable())
			|| (Mailbox && Mailbox->GetUnclaimedCount() > 0)
			|| (LiveOps && (LiveOps->HasClaimableEvent() || LiveOps->HasUnreadNotice())));

		if (bAnyClaimable)
		{
			MenuAlertMark->SetMark(EAlertMarkSize::Large, EAlertMarkColor::Red);
			MenuAlertMark->Show();
		}
		else
		{
			MenuAlertMark->Hide();
		}
	}

	RefreshWorldMapLock();
}

void UBuildOpenWidget::RefreshQuestBadge(int32 ClaimableCount)
{
	if (!QuestBadge || !QuestBadgeCount)
	{
		return;
	}
	QuestBadge->SetVisibility(ClaimableCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (ClaimableCount > 0)
	{
		// 두 자리를 넘으면 배지가 알약으로 늘어난다 — 99+ 에서 멈춘다
		QuestBadgeCount->SetText(ClaimableCount > 99
			? NSLOCTEXT("BuildOpen", "QuestBadgeMax", "99+")
			: FText::AsNumber(ClaimableCount));
	}
}

void UBuildOpenWidget::OnQuestButtonClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ButtonClick);
		}
	}

	UUIManagerSubsystem* UIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!UIBase || !TableMgr) return;

	// PromptStack에 이미 위젯이 있으면 중복 push 방지
	if (UIBase->GetPromptStackCount() > 0) return;

	// 탭 힌트 없음 = 미션판 기본 탭(일일). 메뉴판 미션 타일과 같은 진입이다
	TSubclassOf<UUserWidget> QuestPanel = TableMgr->GetWidgetClass(EWidgetType::QuestPanel);
	if (!QuestPanel || !UIBase->PushPromptClass(QuestPanel.Get())) return;

	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->GoToUIMode();
	}
}

void UBuildOpenWidget::HandleHQLeveledUp(int32 /*NewLevel*/)
{
	// 레벨업 직후엔 자금 소모로 재평가 필요 — 다음 단계 조건 재체크
	RefreshAlertMarks();
}

void UBuildOpenWidget::HandleFactoryUpgraded(EFactoryUpgradeType /*UpgradeType*/)
{
	RefreshAlertMarks();
}

UWidget* UBuildOpenWidget::GetBuildOpenButtonWidget() const
{
	return BuildOpenButton;
}

UWidget* UBuildOpenWidget::GetFactoryOpenButtonWidget() const
{
	return FactoryOpenButton;
}

UWidget* UBuildOpenWidget::GetWorldMapButtonWidget() const
{
	return WorldMapBtn;
}

UWidget* UBuildOpenWidget::GetGachaButtonWidget() const
{
	return GachaBtn;
}

UWidget* UBuildOpenWidget::GetHeadquartersButtonWidget() const
{
	return HeadquartersBtn;
}

UWidget* UBuildOpenWidget::GetMenuButtonWidget() const
{
	return MenuBtn;
}

void UBuildOpenWidget::OnBuildOpenButtonClicked()
{
	UTableManagerSubsystem* tableManager = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	if (!tableManager || !UIBase) return;

	// 중복 push 방지 (다른 프롬프트 모달과 일관)
	if (UIBase->GetPromptStackCount() > 0) return;

	// B안: 빌드 모달은 중앙 프롬프트(자체 딤). enum 슬롯은 BuildPanel 재사용 (DT 행이 새 WBP 가리킴)
	TSubclassOf<UUserWidget> buildPanel = tableManager->GetWidgetClass(EWidgetType::BuildPanel);
	if (!buildPanel)
	{
		// null push 시 모달 없이 GoToUIMode 가 실행되어 입력이 잠기므로 반드시 여기서 중단
		UE_LOG(LogTemp, Error, TEXT("[BuildOpenWidget] BuildPanel widget class not found! (DT_WidgetClass 행 확인)"));
		return;
	}
	UCommonActivatableWidget* PushedModal = UIBase->PushPromptClass(buildPanel.Get());
	if (!PushedModal) return;

	// 미션 가이드 — 모달 열림 알림 ('게임' 타일 하이라이트 페이즈 진입)
	if (UMissionManagerSubsystem* MissionMgr = GetWorld()->GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyBuildModalOpened(PushedModal);
	}

	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		PC->GoToUIMode();
	}
}

void UBuildOpenWidget::OnFactoryOpenButtonClicked()
{
	UCGGameInstance* gameInstance = UCGGameInstance::GetInstance();
	UTableManagerSubsystem* tableManager = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	TSubclassOf<UUserWidget> FactoryPanel = tableManager->GetWidgetClass(EWidgetType::FactoryPanel);
	UIManager->GetUIBase()->PushBottomClass(FactoryPanel.Get());

	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		const EInputMode ExistingMode = PC->GetCurrentInputMode();
		const EInputMode OpeningMode = CGRFactoryPanelInteraction::ResolveOpeningInputMode(ExistingMode);
		if (ExistingMode != OpeningMode)
		{
			PC->GoToNormalMode();
		}

		// 공장으로 카메라 이동
		ABrickFactory* Factory = Cast<ABrickFactory>(UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass()));
		if (Factory)
		{
			APlayerCamera* PlayerCamera = Cast<APlayerCamera>(PC->GetPawn());
			if (PlayerCamera)
			{
				const float FocusDistance = Factory->GetCameraFocusDistanceCm();
				if (FocusDistance > 0.f)
				{
					PlayerCamera->FocusOnActor(Factory, 0.35f, true, FocusDistance);
				}
			}
		}
	}
}

void UBuildOpenWidget::OnMenuButtonClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ButtonClick);
		}
	}

	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager->GetUIBase();
	if (!UIBase) return;

	// PromptStack에 이미 위젯이 있으면 중복 push 방지
	if (UIBase->GetPromptStackCount() > 0) return;

	UTableManagerSubsystem* tableManager = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	TSubclassOf<UUserWidget> MenuPanel = tableManager->GetWidgetClass(EWidgetType::MenuPanel);
	if (!UIBase->PushPromptClass(MenuPanel.Get())) return;

	// UI 모드 전환
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		PC->GoToUIMode();
	}
}

void UBuildOpenWidget::OnShopButtonClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ButtonClick);
		}
	}

	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager->GetUIBase();
	if (!UIBase) return;

	if (UIBase->GetPromptStackCount() > 0) return;

	UTableManagerSubsystem* tableManager = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	TSubclassOf<UUserWidget> ShopPanel = tableManager->GetWidgetClass(EWidgetType::ShopPanel);
	if (!UIBase->PushPromptClass(ShopPanel.Get())) return;

	// UI 모드 전환
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		PC->GoToUIMode();
	}
}

void UBuildOpenWidget::OnGachaButtonClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ButtonClick);
		}
	}

	// MainMap [뽑기] → 빌딩 특성 가챠 허브 (직원 채용 가챠는 OfficeMain [채용] 별도 진입)
	UUIManagerSubsystem* UIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!UIManager || !UIManager->GetUIBase() || !TableMgr) return;

	TSubclassOf<UUserWidget> GachaCls = TableMgr->GetWidgetClass(EWidgetType::BuildingTraitGachaPanel);
	if (!GachaCls) return;

	if (!UIManager->GetUIBase()->PushPromptClass(GachaCls.Get())) return;

	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->GoToUIMode();
	}
}

bool UBuildOpenWidget::IsWorldMapUnlocked()
{
	// 매니저가 없으면 열어 둔다 — 잠금이 진행을 막는 쪽으로 실패하지 않게.
	return !IsValid(CachedSaveMgr)
		|| CachedSaveMgr->GetHQLevel() >= WorldMapConstants::WorldMapRequiredHQLevel;
}

void UBuildOpenWidget::RefreshWorldMapLock()
{
	if (!WorldMapLockImage)
	{
		return;
	}

	const bool bLocked = !IsWorldMapUnlocked();
	WorldMapLockImage->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// 딤 대상 = 자물쇠의 부모 오버레이(지구본 버튼 + "월드" 라벨). 산업 타일과 같은 잠금 언어.
	if (UPanelWidget* WorldMapGroup = WorldMapLockImage->GetParent())
	{
		WorldMapGroup->SetRenderOpacity(bLocked ? 0.45f : 1.0f);
	}
}

void UBuildOpenWidget::OnWorldMapButtonClicked()
{
	if (!IsWorldMapUnlocked())
	{
		const int32 ReqLevel = WorldMapConstants::WorldMapRequiredHQLevel;
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(
				FText::Format(NSLOCTEXT("BuildOpen", "WorldMapLocked", "본사 Lv.{0} 달성 시 열립니다"), ReqLevel),
				3.0f, ENotificationType::Failed);
		}
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ButtonClick);
		}
	}

	// 월드맵은 레벨이 아니라 MainMap 위에 얹는 풀스크린 패널이다 ― 전환·로딩 화면이 사라진다.
	UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!UIBase || !TableMgr) { return; }
	if (UIBase->GetPromptStackCount() > 0) { return; }

	if (TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::WorldMapPanel))
	{
		if (!UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls))) return;
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			PC->GoToUIMode();
		}
	}
}

void UBuildOpenWidget::OnHeadquartersButtonClicked()
{
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager->GetUIBase();
	if (!UIBase) return;

	// PromptStack에 이미 위젯이 있으면 중복 push 방지
	if (UIBase->GetPromptStackCount() > 0) return;

	UTableManagerSubsystem* tableManager = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	TSubclassOf<UUserWidget> HQPanel = tableManager->GetWidgetClass(EWidgetType::HQManagePanel);
	if (!UIBase->PushPromptClass(HQPanel.Get())) return;

	// UI 모드 전환
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		PC->GoToUIMode();
	}
}
