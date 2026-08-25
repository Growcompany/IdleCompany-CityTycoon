// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/BuildOpenWidget.h"

#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "CommonActivatableWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "CommonTextBlock.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/ProjectOperationManager.h"
#include "Enum/WidgetType.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"
#include "UI/Panel/FactoryPanelInteractionPolicy.h"
#include "Entity/Factory/BrickFactory.h"
#include "Data/FactorySaveData.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Panel/InGameLayerWidget.h"
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

	if (CollectAllButton)
	{
		CollectAllButton->OnClicked().AddUObject(this, &UBuildOpenWidget::OnCollectAllButtonClicked);
	}

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

	if (InventoryOpenButton)
	{
		InventoryOpenButton->OnClicked().AddUObject(this, &UBuildOpenWidget::OnInventoryOpenButtonClicked);
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

	if (InventoryOpenButton)
	{
		InventoryOpenButton->OnClicked().Clear();
	}

	if (CollectAllButton)
	{
		CollectAllButton->OnClicked().Clear();
	}

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

	// CollectAll — 도트(이진)와 금액(정량)이 매니저 조회를 공유한다. 둘 다 미배치면 조회 자체를 건너뛴다.
	if (IsValid(CollectAllAlertMark) || IsValid(CollectAllAmountText))
	{
		UGameInstance* CollectGI = GetGameInstance();
		UProjectOperationManager* OpMgr = CollectGI
			? CollectGI->GetSubsystem<UProjectOperationManager>()
			: nullptr;

		// 수거 가능한 수익이 하나라도 있으면 초록 도트 (가용 신호)
		if (IsValid(CollectAllAlertMark))
		{
			const bool bHasRevenue = IsValid(OpMgr) && OpMgr->HasAnyStoredRevenue();
			if (bHasRevenue)
			{
				CollectAllAlertMark->SetMark(EAlertMarkSize::Large, EAlertMarkColor::Green);
				CollectAllAlertMark->Show();
			}
			else
			{
				CollectAllAlertMark->Hide();
			}
		}

		// 금액 표시 — 2초 주기 폴링 타이머(AlertMarkPollHandle)가 이미 돌고 있어 새 타이머가 필요 없다.
		// GetTotalStoredRevenue 는 HasAnyStoredRevenue 와 같은 순회라 도트와 대상 집합이 갈리지 않는다.
		if (IsValid(CollectAllAmountText))
		{
			const double TotalStored = IsValid(OpMgr) ? OpMgr->GetTotalStoredRevenue() : 0.0;
			// 매니저가 이미 건물별 max(0, trunc) 를 적용해 합계는 정수값이다 — 임계 리터럴을 다시 쓰지 않고
			// int64 로 좁혀 파생시킨다. 표시하는 값과 판정하는 값이 같은 식이어야 둘이 갈릴 수 없다.
			const int64 DisplayAmount = static_cast<int64>(TotalStored);
			if (DisplayAmount > 0)
			{
				CollectAllAmountText->SetText(UGlobalUtilFunctions::AbbreviateNumber(DisplayAmount));
				CollectAllAmountText->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				// 1원(수거 단위) 미만엔 수거할 것이 없으므로 세 표면이 모두 침묵한다 —
				// 점 꺼짐/숫자 숨김/수거 0원. 여기서 굳이 소수 잔액을 "친절하게" 보여주면
				// 화면은 수거 가능하다고 말하지만 눌러도 0 원인 거짓 약속이 된다.
				CollectAllAmountText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	RefreshWorldMapLock();
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

UWidget* UBuildOpenWidget::GetGachaButtonWidget() const
{
	return GachaBtn;
}

UWidget* UBuildOpenWidget::GetHeadquartersButtonWidget() const
{
	return HeadquartersBtn;
}

UWidget* UBuildOpenWidget::GetCollectAllButtonWidget() const
{
	return CollectAllButton;
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
				PlayerCamera->FocusOnActor(Factory, 0.35f);
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
	UIBase->PushPromptClass(MenuPanel.Get());

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
	UIBase->PushPromptClass(ShopPanel.Get());

	// UI 모드 전환
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		PC->GoToUIMode();
	}
}

void UBuildOpenWidget::OnInventoryOpenButtonClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ButtonClick);
		}
	}

	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	if (!UIBase) return;

	if (UIBase->GetPromptStackCount() > 0) return;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;
	TSubclassOf<UUserWidget> HubClass = TableMgr->GetWidgetClass(EWidgetType::InventoryHub);
	if (!HubClass) return;

	// 컨텍스트는 InventoryHubWidget::ResolveContext(Auto) 가 GetMapName 으로 추론 (MainMap 진입 시 자동)
	UIBase->PushPromptClass(HubClass.Get());

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

	UIManager->GetUIBase()->PushPromptClass(GachaCls.Get());

	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->GoToUIMode();
	}
}

bool UBuildOpenWidget::IsWorldMapUnlocked()
{
	// 월드맵은 1막(게임·IT·금융) 완주와 함께 열린다 — 그 마지막인 Finance 의 해금 레벨을 그대로 쓴다.
	// 제조 3종을 대리로 쓰던 구 판정은 제조가 Lv20 으로 밀리면서 월드맵까지 끌고 갔다.
	// 매니저가 없으면 열어 둔다 — 잠금이 진행을 막는 쪽으로 실패하지 않게.
	return !IsValid(CachedSaveMgr) || CachedSaveMgr->IsIndustryUnlocked(ECompanyType::Finance);
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
		// 여기 도달 = IsWorldMapUnlocked 가 CachedSaveMgr 유효를 이미 통과시킨 것
		const int32 ReqLevel = CachedSaveMgr->GetIndustryRequiredHQLevel(ECompanyType::Finance);
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(
				FText::Format(NSLOCTEXT("BuildOpen", "WorldMapLocked", "본사 Lv.{0} 달성 시 금융업과 함께 열립니다"), ReqLevel),
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

	UCGGameInstance* GameInstance = UCGGameInstance::GetInstance();
	if (GameInstance)
	{
		GameInstance->TransitionToLevel(TEXT("WorldMap"));
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
	UIBase->PushPromptClass(HQPanel.Get());

	// UI 모드 전환
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		PC->GoToUIMode();
	}
}

void UBuildOpenWidget::OnCollectAllButtonClicked()
{
	// InGameLayer 가 수거 + 코인 플라이아웃 + 토스트까지 일괄 처리 (단일 버블 클릭 경로와 공유)
	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr) return;

	if (UInGameLayerWidget* InGameLayer = UIMgr->GetInGameLayer())
	{
		InGameLayer->ExecuteAllVaultCollection();
		// CollectAllStoredRevenue 는 동기 — 금액/도트를 폴링(최대 2초) 기다리지 않고 즉시 비운다.
		// 코인 플라이아웃만 비동기라 연출은 그대로 이어진다.
		RefreshAlertMarks();
	}
}
