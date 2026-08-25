// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeLayerWidget.h"
#include "UI/Panel/TierRoadmapWidget.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Common/CoinFlyoutContainerWidget.h"
#include "UI/Element/Chat/BubbleContainerWidget.h"
#include "UI/Element/Effects/CatchRingWidget.h"
#include "UI/Element/Common/GestureHintWidget.h"
#include "UI/Element/Common/CatchHintRules.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Player/Components/InteractableInputHandler.h"
#include "GameFramework/Pawn.h"
#include "Table/ResourceInfo.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EngineUtils.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "UI/Element/Common/DiamondProgressWidget.h"
#include "UI/Element/Common/RevenueRateChipWidget.h"
#include "CommonTextBlock.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Data/ProjectBoardData.h"
#include "Table/CompanyInfoTable.h"
#include "Table/BuildableCardTable.h"
#include "Data/GameSaveData.h"
#include "Data/EntitySaveData.h"
#include "Components/OverlaySlot.h"
#include "Enum/WidgetType.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"

void UOfficeLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	RecruitmentMgr = GI->GetSubsystem<URecruitmentManagerSubsystem>();
	CurrentBuildingIndex = GI->GetCurrentManagedBuildingIndex();

	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();

	// BindWidget된 리소스 위젯들을 TMap에 등록
	if (UIE_Resource_Money)
	{
		ResourceWidgets.Add(EResourceType::Money, UIE_Resource_Money);
		if (RMgr)
		{
			UIE_Resource_Money->SetValue(RMgr->GetResourceAmount(EResourceType::Money));
		}
	}

	if (UIE_Resource_Diamond)
	{
		ResourceWidgets.Add(EResourceType::Diamond, UIE_Resource_Diamond);
		if (RMgr)
		{
			UIE_Resource_Diamond->SetValue(RMgr->GetResourceAmount(EResourceType::Diamond));
		}
	}

	if (UIE_Resource_Employee)
	{
		ResourceWidgets.Add(EResourceType::Employee, UIE_Resource_Employee);
		UpdateEmployeeChip();
	}

	// 회사 플레이트 블록 = 빌딩 레벨 로드맵 진입점
	if (CompanyPlateButton)
	{
		CompanyPlateButton->OnClicked.AddDynamic(this, &UOfficeLayerWidget::HandleCompanyPlateClicked);
	}

	// UIManagerSubsystem을 통한 리소스 변경 구독
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UIResourceChangedHandle = UIMgr->OnUIResourceChanged.AddUObject(
			this, &UOfficeLayerWidget::HandleResourceChanged);
	}

	// OfficeManager 업무공간 변경 구독
	if (UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>())
	{
		WorkstationChangedHandle = OfficeMgr->OnWorkstationCountChanged.AddUObject(
			this, &UOfficeLayerWidget::HandleWorkstationCountChanged);
	}

	// 고용/해고로 로스터가 바뀌면 인원 칩 분자를 즉시 갱신
	if (UEmployeeManager* ChipEmpMgr = GI->GetSubsystem<UEmployeeManager>())
	{
		RosterChangedHandle = ChipEmpMgr->OnEmployeeRosterChanged.AddUObject(
			this, &UOfficeLayerWidget::UpdateEmployeeChip);
	}

	// 티어 승급 구독 — 매니저가 UI 를 직접 호출하지 않도록 상주 레이어가 당겨 쓴다.
	// OfficeStageProgressManager 는 WorldSubsystem 이라 오피스 맵에서만 존재한다.
	if (UOfficeStageProgressManager* TierStageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>())
	{
		TierStageMgr->OnTierUnlocked.AddDynamic(this, &UOfficeLayerWidget::HandleTierUnlocked);

		// 집중 모드도 같은 자세 — OfficeMain 을 경유하면 레이어보다 늦게 생성될 때 첫 상태를 놓친다.
		TierStageMgr->OnLifecycleChanged.AddDynamic(this, &UOfficeLayerWidget::HandleLifecycleChanged);

		// 개발중에 오피스로 재진입/로드된 경우 — 전환 이벤트가 이미 지나갔으므로 현재 상태를 당겨 스냅
		SetFocusLocked(TierStageMgr->IsFocusLocked());
		FocusFadeAlpha = bFocusLocked ? 1.0f : 0.0f;
		ApplyFocusFade(FocusFadeAlpha);
	}

	// ========== 빈 좌석 버블 컨테이너 생성 (InGameLayerWidget 패턴 미러) ==========
	if (!BubbleContainer)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			BubbleContainer = CreateWidget<UBubbleContainerWidget>(PC);
			if (BubbleContainer && InGameCanvas)
			{
				UCanvasPanelSlot* BubbleSlot = InGameCanvas->AddChildToCanvas(BubbleContainer);
				if (BubbleSlot)
				{
					BubbleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
					BubbleSlot->SetOffsets(FMargin(0.0f));
					BubbleSlot->SetZOrder(-1);
				}
			}
		}
	}

	if (BubbleContainer)
	{
		BubbleContainer->OnBubbleAction.BindUObject(this, &UOfficeLayerWidget::HandleBubbleAction);
	}

	// 진입 즉시 1회 평가 (이후는 NativeTick 폴링 + 책상 수 변경 델리게이트)
	EvaluateAllWorkstationBubbles();

	// M4 진입 유도 — 매니저가 책상 버블/액터를 하이라이트 타겟으로 조회
	if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		M->RegisterOfficeLayer(this);
	}

	// 직원 수익 코인 연출 구독 (직원 머리 위 → 돈 아이콘)
	if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
	{
		OpMgr->OnIncomeCoinRequested.AddDynamic(this, &UOfficeLayerWidget::OnIncomeCoinRequestedReceived);
	}

	// 상단 밴드 초기 채우기 (배너/트렌드/티어). 이후는 OfficeMain 라이프사이클 → RefreshTopBar.
	RefreshTopBar();

	UE_LOG(LogTemp, Log, TEXT("[OfficeLayerWidget] NativeConstruct - BuildingIndex: %d"), CurrentBuildingIndex);
}

void UOfficeLayerWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	BindButtonEvents();

	// 직원은 입장 후 순차 스폰이라 여기선 보통 0 — 첫 틱(1초)이 실제 요율을 채운다.
	// 그래도 0 을 명시해야 이전 사무실의 잔상값이 남지 않는다.
	if (RevenueRateChip)
	{
		RevenueRateChip->SetRate(0.0);
		RevenueChipTimer = 0.0f;
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeLayerWidget] NativeOnActivated"));
}

void UOfficeLayerWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	UnbindButtonEvents();
	UE_LOG(LogTemp, Log, TEXT("[OfficeLayerWidget] NativeOnDeactivated"));
}

void UOfficeLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 배정 변경(WorkstationInfoWidget) 전용 델리게이트가 없어 주기적 전체 재평가
	BubbleReevalTimer += InDeltaTime;
	if (BubbleReevalTimer >= BubbleReevalInterval)
	{
		BubbleReevalTimer = 0.0f;
		EvaluateAllWorkstationBubbles();
	}

	// [Perf] 캐치 대기 직원 스캔(전체 액터 순회)은 ~6Hz로만, 링 위치 투영은 매 틱(카메라 추적).
	// 텔레그래프가 ~3초라 0.15s 스캔 지연은 무해하고, 종료는 RefreshCatchRings의 매틱 재확인으로 즉시 반영.
	CatchScanAccum += InDeltaTime;
	if (CatchScanAccum >= 0.15f)
	{
		CatchScanAccum = 0.0f;
		ScanAwaitingCatchWorkers();
	}
	RefreshCatchRings();

	RevenueChipTimer += InDeltaTime;
	if (RevenueChipTimer >= RevenueChipInterval)
	{
		RevenueChipTimer = 0.0f;
		RefreshRevenueRateChip();
	}

	// 집중 모드 페이드 — 하단 도크/미션 트래커와 같은 보간(멱등). 히트테스트는 SetFocusLocked 가 이미 끊었다.
	{
		const float Target = bFocusLocked ? 1.0f : 0.0f;
		if (!FMath::IsNearlyEqual(FocusFadeAlpha, Target, 0.001f))
		{
			FocusFadeAlpha = FMath::FInterpTo(FocusFadeAlpha, Target, InDeltaTime, FocusFadeSpeed);
			// FInterpTo 는 목표에 정확히 안 닿음 — 도착 프레임만 스냅
			if (FMath::IsNearlyEqual(FocusFadeAlpha, Target, 0.001f))
			{
				FocusFadeAlpha = Target;
			}
			ApplyFocusFade(FocusFadeAlpha);
		}
	}
}

void UOfficeLayerWidget::HandleLifecycleChanged(EProjectLifecycle OldState, EProjectLifecycle NewState)
{
	SetFocusLocked(NewState == EProjectLifecycle::Developing || NewState == EProjectLifecycle::LaunchPending);
}

void UOfficeLayerWidget::SetFocusLocked(bool bLocked)
{
	if (bFocusLocked == bLocked)
	{
		return;
	}
	bFocusLocked = bLocked;

	// 히트테스트는 페이드를 기다리지 않는다 — 사라지는 0.2초 동안 눌리면 잠금이 그대로 뚫린다.
	// Collapsed 가 아니라 HitTestInvisible 이라야 상단 밴드 레이아웃이 안 흔들린다.
	const ESlateVisibility Vis = bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible;
	if (BackButton)
	{
		BackButton->SetVisibility(Vis);
	}
	if (CodexButton)
	{
		CodexButton->SetVisibility(Vis);
	}
}

void UOfficeLayerWidget::ApplyFocusFade(float Alpha)
{
	const float Opacity = 1.0f - Alpha;
	if (BackButton)
	{
		BackButton->SetRenderOpacity(Opacity);
	}
	if (CodexButton)
	{
		CodexButton->SetRenderOpacity(Opacity);
	}
}

void UOfficeLayerWidget::RefreshRevenueRateChip()
{
	// 미배치(WBP 트리 paste 전)면 조용히 스킵
	if (!RevenueRateChip)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 지급 주체를 그대로 센다 — 요율식은 GetCurrentIncomePerSecond 단독 소유라 표시와 지급이 갈릴 수 없다.
	// 앉아 쉬는 직원은 요율 0 을 돌려주므로 합이 곧 지갑 증가 속도다.
	double RatePerSec = 0.0;
	for (TActorIterator<AOfficeworker> It(World); It; ++It)
	{
		const AOfficeworker* Worker = *It;
		if (!Worker || Worker->bIsPortraitMode || !Worker->BehaviorComponent) continue;
		RatePerSec += static_cast<double>(Worker->BehaviorComponent->GetCurrentIncomePerSecond());
	}

	RevenueRateChip->SetRate(RatePerSec);
}

void UOfficeLayerWidget::BindButtonEvents()
{
	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &UOfficeLayerWidget::OnBackButtonClicked);
	}
	if (CodexButton)
	{
		CodexButton->OnClicked().AddUObject(this, &UOfficeLayerWidget::OnCodexBtnClicked);
	}
}

void UOfficeLayerWidget::UnbindButtonEvents()
{
	if (BackButton)
	{
		BackButton->OnClicked().Clear();
	}
	if (CodexButton)
	{
		CodexButton->OnClicked().Clear();
	}
}

void UOfficeLayerWidget::NativeDestruct()
{
	// 회사 플레이트 클릭 해제 (NativeConstruct 의 AddDynamic 쌍)
	if (CompanyPlateButton)
	{
		CompanyPlateButton->OnClicked.RemoveDynamic(this, &UOfficeLayerWidget::HandleCompanyPlateClicked);
	}

	// UIManagerSubsystem 구독 해제
	if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->OnUIResourceChanged.Remove(UIResourceChangedHandle);
	}

	// OfficeManager 업무공간 구독 해제
	if (UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>())
	{
		OfficeMgr->OnWorkstationCountChanged.Remove(WorkstationChangedHandle);
	}

	if (UEmployeeManager* ChipEmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		ChipEmpMgr->OnEmployeeRosterChanged.Remove(RosterChangedHandle);
	}

	// 티어 승급 구독 해제 (NativeConstruct 의 AddDynamic 쌍)
	if (UOfficeStageProgressManager* TierStageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr)
	{
		TierStageMgr->OnTierUnlocked.RemoveDynamic(this, &UOfficeLayerWidget::HandleTierUnlocked);
		TierStageMgr->OnLifecycleChanged.RemoveDynamic(this, &UOfficeLayerWidget::HandleLifecycleChanged);
	}

	// 버블 클릭 델리게이트 해제
	if (BubbleContainer)
	{
		BubbleContainer->OnBubbleAction.Unbind();
	}

	if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		M->UnregisterOfficeLayer(this);
	}

	// 코인 연출 중이면 정리
	if (CurrentCoinFlyout)
	{
		CurrentCoinFlyout->OnCoinArrived.Unbind();
		CurrentCoinFlyout->OnAllCoinsComplete.Unbind();
		CurrentCoinFlyout->RemoveFromParent();
		CurrentCoinFlyout = nullptr;
		bSuppressMoneyUpdate = false;
	}

	// 수익 코인 구독 해제 + 스트리밍 컨테이너 정리
	if (UCGGameInstance* CoinGI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (UProjectOperationManager* OpMgr = CoinGI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->OnIncomeCoinRequested.RemoveAll(this);
		}
	}
	if (StreamCoinFlyout)
	{
		StreamCoinFlyout->OnStreamCoinArrived.Unbind();
		StreamCoinFlyout->RemoveFromParent();
		StreamCoinFlyout = nullptr;
	}

	// 캐치 링 풀 델리게이트 해제 (GetOrCreateCatchRing 의 AddUObject 쌍)
	for (UCatchRingWidget* PooledRing : CatchRingPool)
	{
		if (PooledRing)
		{
			PooledRing->OnCaught.RemoveAll(this);
		}
	}

	UnbindButtonEvents();
	Super::NativeDestruct();
	UE_LOG(LogTemp, Log, TEXT("[OfficeLayerWidget] NativeDestruct"));
}

// 인원 칩 = 고용 게이트와 같은 술어(로스터 / 인원 상한). 갱신 지점이 갈라지지 않게 여기로 모은다.
void UOfficeLayerWidget::UpdateEmployeeChip()
{
	UResourceWidget** FoundWidget = ResourceWidgets.Find(EResourceType::Employee);
	if (!FoundWidget || !*FoundWidget) { return; }

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!EmpMgr) { return; }

	// 표시 경로 — 상한 0 로그는 게이트가 남긴다
	(*FoundWidget)->SetValueWithMax(
		EmpMgr->GetEmployeeCountInBuilding(CurrentBuildingIndex),
		EmpMgr->GetBuildingEmployeeCapacity(CurrentBuildingIndex, /*bLogIfZero*/ false));
}

void UOfficeLayerWidget::HandleWorkstationCountChanged()
{
	// 책상이 추가/제거되면 버블도 즉시 재평가
	EvaluateAllWorkstationBubbles();
}

void UOfficeLayerWidget::HandleCompanyPlateClicked()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) { return; }
	UTableManagerSubsystem* RoadmapTableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!RoadmapTableMgr || !UIMgr || !UIMgr->GetUIBase()) { return; }

	TSubclassOf<UUserWidget> Cls = RoadmapTableMgr->GetWidgetClass(EWidgetType::TierRoadmap);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeLayer] TierRoadmap 위젯 미등록(DT_WidgetClass)"));
		return;
	}

	// 오피스에서는 현재 관리 중인 빌딩이 곧 이 오피스다
	UCommonActivatableWidget* W = UIMgr->GetUIBase()->PushPromptClass(Cls.Get());
	UTierRoadmapWidget* Roadmap = Cast<UTierRoadmapWidget>(W);
	if (!Roadmap) { return; }

	Roadmap->ConfigureForBuilding(GI->GetCurrentManagedBuildingIndex());

	// 오피스는 Normal 모드라 팝오버 뒤로 월드 탭/카메라 드래그가 살아 있다 — 여기서 잡고 복원 책임을 로드맵에 넘긴다
	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
	{
		PC->GoToUIMode();
		Roadmap->SetOwnsInputMode(true);
	}
}

void UOfficeLayerWidget::HandleTierUnlocked(int32 NewTier)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) { return; }

	UTableManagerSubsystem* TierTableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TierTableMgr || !UIMgr) { return; }

	ECompanyType CompanyType = ECompanyType::None;
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		CompanyType = SaveMgr->GetBuildingCompanyType(GI->GetCurrentManagedBuildingIndex());
	}

	bool bOk = false;
	const FTierUnlockData TierData = TierTableMgr->GetTierUnlockData(NewTier, bOk);

	// 승급이 주는 것은 강화 슬롯 해금뿐 — 인원 상한은 층수(빌드업)가 올린다
	FString Msg = FString::Printf(TEXT("%d단계 달성"), NewTier);
	if (bOk)
	{
		for (EBuildingEnhancementType UnlockedType : TierData.UnlockedEnhancements)
		{
			FBuildingEnhancementDefinition Def;
			if (!TierTableMgr->GetEnhancementDefinition(UnlockedType, Def)) { continue; }

			// 이 산업에 안 나오는 슬롯은 해금됐다고 말하지 않는다
			if (!UBuildingEnhancementHelper::IsEnhancementVisibleForCompanyType(Def.Category, CompanyType))
			{
				continue;
			}
			// 줄표는 U+2015 — U+2014 em-dash 는 NEXON 폰트 글리프가 없어 박스로 렌더된다
			Msg += FString::Printf(TEXT(" ― %s 해금"), *Def.DisplayName.ToString());
		}
	}

	// 빌딩이 늘면 모달은 진행 차단이 되므로 토스트 1줄만
	UIMgr->ShowNotification(FText::FromString(Msg), 3.0f, ENotificationType::Normal);

	// 밴드 뱃지가 이 빌딩의 티어를 말하므로 같은 이벤트에서 다시 그려야 stale 이 안 남는다
	RefreshTopBar();
}

void UOfficeLayerWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	// 코인 연출 중이면 Money 업데이트 억제
	if (Type == EResourceType::Money && bSuppressMoneyUpdate)
	{
		return;
	}

	if (UResourceWidget** FoundWidget = ResourceWidgets.Find(Type))
	{
		if (*FoundWidget)
		{
			(*FoundWidget)->SetValue(NewValue);
		}
	}
}

void UOfficeLayerWidget::OnBackButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeLayerWidget] Back button clicked"));

	// 집중 모드 방어선 — 히트테스트 차단이 뚫려도 레벨 전환까지 가지 않게 한다(월드 입력 2중 가드 미러).
	if (bFocusLocked)
	{
		return;
	}

	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIManager || !UIManager->GetUIBase()) return;

	// 스택 개수 확인
	int32 StackCount = UIManager->GetUIBase()->GetBottomStackCount();

	if (StackCount <= 1)
	{
		// 스택이 1개 이하면 MainMap으로 이동
		if (UCGGameInstance* CGGameInst = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
		{
			CGGameInst->TransitionToLevel(TEXT("MainMap_TheRiverwalkCity"), 0);
		}
	}
	else
	{
		// 스택에 여러 개가 있으면 하나씩 제거
		UIManager->GetUIBase()->PopBottomWidget();
	}
}

void UOfficeLayerWidget::OnCodexBtnClicked()
{
	if (bFocusLocked)
	{
		return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;
	UTableManagerSubsystem* TblMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TblMgr || !UIMgr || !UIMgr->GetUIBase()) return;

	TSubclassOf<UUserWidget> CodexClass = TblMgr->GetWidgetClass(EWidgetType::CodexPanel);
	if (!CodexClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeLayerWidget] CodexPanel 위젯 미등록(DT_WidgetClass)"));
		return;
	}
	UIMgr->GetUIBase()->PushPromptClass(CodexClass.Get());
}

// 현재 오피스 건물 아이콘을 프로필 액자에 적용 — 성공 시 true, 실패 시 호출부가 산업 글리프 폴백.
// 오피스맵엔 건물 액터가 없어 세이브 데이터로 BuildingIndex → InteractableName(=카드 RowName) 해석.
static bool TryApplyBuildingProfileIcon(UCGGameInstance* GI, UTableManagerSubsystem* TblMgr, UImage* IconImage)
{
	const int32 BuildingIdx = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIdx == INDEX_NONE) return false;

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData) return false;

	for (const FBuildingEntitySaveData& BS : SaveData->GameData.Buildings)
	{
		if (BS.BuildingIndex != BuildingIdx) continue;

		FBuildableCardTable BuildableInfo;
		if (!TblMgr->GetBuildableInfo(BS.InteractableName, BuildableInfo)) return false;
		if (BuildableInfo.UIIcon.IsNull()) return false;

		UTexture2D* IconTex = BuildableInfo.UIIcon.LoadSynchronous();
		if (!IconTex) return false;

		IconImage->SetBrushFromTexture(IconTex);
		// WBP 브러시의 웜크림 틴트는 단색 글리프용 — 풀컬러 건물 아이콘엔 해제, 여백도 축소(64→80px)
		IconImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
		if (UOverlaySlot* IconSlot = Cast<UOverlaySlot>(IconImage->Slot))
		{
			IconSlot->SetPadding(FMargin(12.f));
		}
		return true;
	}
	return false;
}

void UOfficeLayerWidget::RefreshTopBar()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) { return; }
	const ECompanyType Industry = GI->GetCurrentBuildingCompanyType();

	// ===== 회사 배너 (산업 시그니처색/산업명 = DT, 아이콘 = 현재 건물, 회사명/레벨 = PlayFab/HQ) =====
	if (UTableManagerSubsystem* TblMgr = GI->GetSubsystem<UTableManagerSubsystem>())
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TblMgr->GetCompanyInfo(Industry, bFound);
		if (bFound)
		{
			if (CompanyPlate) { CompanyPlate->SetColorAndOpacity(Info.AccentColor); }
			if (CompanyIndustryText) { CompanyIndustryText->SetText(Info.DisplayName); }
			// 산업은 텍스트가 이미 말하므로 아이콘은 건물 정체성 담당 — 해석 실패 시에만 글리프
			if (CompanyProfileIcon && !TryApplyBuildingProfileIcon(GI, TblMgr, CompanyProfileIcon)
				&& !Info.GlyphIcon.IsNull())
			{
				if (UTexture2D* Glyph = Info.GlyphIcon.LoadSynchronous())
				{
					CompanyProfileIcon->SetBrushFromTexture(Glyph);
				}
			}
		}
	}
	if (CompanyNameText)
	{
		FString Name;
		if (UPlayFabManagerSubsystem* PlayFabMgr = GI->GetSubsystem<UPlayFabManagerSubsystem>())
		{
			if (PlayFabMgr->IsLoggedIn())
			{
				Name = PlayFabMgr->GetUserInfo().DisplayName;
			}
		}
		CompanyNameText->SetText(FText::FromString(Name.IsEmpty() ? TEXT("내 회사") : Name));
	}
	// 뱃지는 플레이트가 가리키는 대상(= 이 회사가 들어있는 건물)의 티어. HQ 레벨은 다른 축이라 여기 오면 범주 오류.
	// 마름모 하나가 단계(숫자)와 진행(둘레 게이지)을 다 말하므로 별도 칩/핍을 두지 않는다.
	const int32 BandBuildingIndex = GI->GetCurrentManagedBuildingIndex();
	int32 BandTier = 1;
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		BandTier = SaveMgr->GetBuildingTier(BandBuildingIndex);
	}
	if (CompanyLevelText)
	{
		CompanyLevelText->SetText(FText::AsNumber(BandTier));
	}
	if (CompanyLevelProgress)
	{
		// 테두리 자체가 진행 게이지 ― 크기 변화 0. 티어 게이트가 사라져 "막힘" 상태는 존재하지 않는다
		bool bAtMax = false;
		CompanyLevelProgress->SetPercent(
			UTierRoadmapWidget::ComputeTierProgress(BandBuildingIndex, BandTier, bAtMax));
		CompanyLevelProgress->SetFillColor(CGTierColors::Reached);
	}
}

void UOfficeLayerWidget::PlayStoredRevenueCollection(int64 CollectedAmount)
{
	if (CollectedAmount <= 0 || !UIE_Resource_Money) return;

	// 이미 연출 중이면 무시
	if (CurrentCoinFlyout) return;

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!RMgr) return;

	// 아이콘 스크린 위치 계산
	UIE_Resource_Money->CalculateIconScreenPos();
	FVector2D TargetPos = UIE_Resource_Money->GetCachedIconScreenPos();

	// 누적 수익은 DT Money 아이콘/색을 사용하는 전용 무음 FundsToast로 표시한다.
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ShowFundsToast(CollectedAmount);
	}

	// 현재 Money (이미 수집 완료된 상태) → OldMoney 역산
	int64 CurrentMoney = RMgr->GetResourceAmount(EResourceType::Money);
	CoinAnimOldMoney = CurrentMoney - CollectedAmount;
	CoinAnimCollectedAmount = CollectedAmount;

	// Money 위젯 텍스트를 OldMoney로 되돌림 (시각적)
	bSuppressMoneyUpdate = true;
	UIE_Resource_Money->SetValue(CoinAnimOldMoney);

	// 코인 수 계산: Amount / 500, 최소 5개 ~ 최대 20개
	CoinAnimTotalCoins = FMath::Clamp(static_cast<int32>(CollectedAmount / 500), 5, 20);
	CoinAnimArrivedCoins = 0;

	// 코인 텍스처 로드 (ResourceInfo DataTable에서 Money 아이콘)
	UTexture2D* CoinTexture = nullptr;
	if (TableMgr)
	{
		bool bSuccess = false;
		FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bSuccess);
		if (bSuccess && !ResInfo.Icon.IsNull())
		{
			CoinTexture = ResInfo.Icon.LoadSynchronous();
		}
	}

	// CoinFlyoutContainerWidget 생성 + Viewport에 추가 (ZOrder 999로 최상위)
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	CurrentCoinFlyout = CreateWidget<UCoinFlyoutContainerWidget>(PC);
	if (!CurrentCoinFlyout) return;

	// 뷰포트 직접 부착은 팝업 위에 그려짐 → InGameCanvas 부착 (스트리밍 코인보다 위)
	CurrentCoinFlyout->SetVisibility(ESlateVisibility::HitTestInvisible);
	bool bOneShotAttached = false;
	if (InGameCanvas)
	{
		if (UCanvasPanelSlot* OneShotSlot = InGameCanvas->AddChildToCanvas(CurrentCoinFlyout))
		{
			OneShotSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			OneShotSlot->SetOffsets(FMargin(0.0f));
			OneShotSlot->SetZOrder(61);
			bOneShotAttached = true;
		}
	}
	if (!bOneShotAttached)
	{
		CurrentCoinFlyout->AddToViewport(999);
	}

	// 콜백 바인딩
	CurrentCoinFlyout->OnCoinArrived.BindUObject(this, &UOfficeLayerWidget::OnCoinArrivedCallback);
	CurrentCoinFlyout->OnAllCoinsComplete.BindUObject(this, &UOfficeLayerWidget::OnAllCoinsCompleteCallback);

	// 연출 시작
	CurrentCoinFlyout->StartFlyout(TargetPos, CoinAnimTotalCoins, CoinTexture);

	UE_LOG(LogTemp, Log, TEXT("[OfficeLayerWidget] PlayStoredRevenueCollection - Amount: %lld, Coins: %d"),
		CollectedAmount, CoinAnimTotalCoins);
}

void UOfficeLayerWidget::OnCoinArrivedCallback(int32 CoinIndex)
{
	CoinAnimArrivedCoins++;

	// 도착한 비율에 따라 Money 카운터 점진적으로 증가
	float Ratio = static_cast<float>(CoinAnimArrivedCoins) / static_cast<float>(CoinAnimTotalCoins);
	int64 DisplayMoney = CoinAnimOldMoney + FMath::RoundToInt64(CoinAnimCollectedAmount * Ratio);

	if (UIE_Resource_Money)
	{
		UIE_Resource_Money->SetValue(DisplayMoney);
	}
}

void UOfficeLayerWidget::OnIncomeCoinRequestedReceived(FVector WorldPos, int64 Amount, int32 EmployeeID)
{
	if (!UIE_Resource_Money) return;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	// 직원 월드 위치 → Absolute 스크린 좌표 (ScoreOrb 시작점 패턴)
	FVector2D ViewportPos;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldPos, ViewportPos, true))
	{
		return;   // 화면 밖 — 코인 생략 (재화는 이미 적립, 다음 착지 갱신에 합류)
	}
	FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	FVector2D StartAbs = ViewportGeo.LocalToAbsolute(ViewportPos);

	// 타겟: 돈 아이콘 (매 스폰 재계산 — 리사이즈/이동 대응)
	UIE_Resource_Money->CalculateIconScreenPos();
	FVector2D TargetAbs = UIE_Resource_Money->GetCachedIconScreenPos();

	// 코인 텍스처 1회 로드 캐시
	if (!bMoneyPresentationCached)
	{
		bMoneyPresentationCached = true;
		CachedCoinTexture = nullptr;
		CachedCoinColor = FLinearColor::White;

		if (TableMgr)
		{
			bool bResourceInfoOk = false;
			const FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bResourceInfoOk);
			if (bResourceInfoOk)
			{
				CachedCoinColor = ResInfo.UIColor;
				if (!ResInfo.Icon.IsNull())
				{
					CachedCoinTexture = ResInfo.Icon.LoadSynchronous();
				}
			}
		}
	}

	// 상주 스트리밍 컨테이너 (1회 생성) — 뷰포트 직접 부착은 팝업 위에 그려져 InGameCanvas 에 부착
	if (!StreamCoinFlyout)
	{
		StreamCoinFlyout = CreateWidget<UCoinFlyoutContainerWidget>(PC);
		if (!StreamCoinFlyout) return;
		StreamCoinFlyout->SetVisibility(ESlateVisibility::HitTestInvisible);
		bool bAttached = false;
		if (InGameCanvas)
		{
			if (UCanvasPanelSlot* CoinCanvasSlot = InGameCanvas->AddChildToCanvas(StreamCoinFlyout))
			{
				CoinCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CoinCanvasSlot->SetOffsets(FMargin(0.0f));
				CoinCanvasSlot->SetZOrder(60);   // 레이어 HUD 위, 프롬프트(팝업)는 UIBase 스택이라 항상 이 위
				bAttached = true;
			}
		}
		if (!bAttached)
		{
			StreamCoinFlyout->AddToViewport(998);   // 캔버스 미바인딩 폴백
		}
		StreamCoinFlyout->OnStreamCoinArrived.BindUObject(this, &UOfficeLayerWidget::OnStreamCoinArrivedCallback);
	}

	// 수익 텍스트는 매 틱(직원별 귀속 표시), 코인은 컨테이너가 알아서 샘플링(스로틀)
	StreamCoinFlyout->SpawnIncomeText(StartAbs, Amount, CachedCoinTexture, CachedCoinColor);
	StreamCoinFlyout->SpawnStreamCoin(StartAbs, TargetAbs, CachedCoinTexture, EmployeeID);
}

void UOfficeLayerWidget::OnStreamCoinArrivedCallback()
{
	// 일괄 수집 연출(suppression) 중엔 그 연출이 표시를 소유
	if (bSuppressMoneyUpdate || !UIE_Resource_Money) return;

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			// 코인 착지 순간에만 실제 보유액으로 갱신 — 수익은 조용히 쌓이고 코인이 나르는 인과
			UIE_Resource_Money->SetValue(RMgr->GetResourceAmount(EResourceType::Money));
		}
	}
}

void UOfficeLayerWidget::OnAllCoinsCompleteCallback()
{
	bSuppressMoneyUpdate = false;

	// 실제 현재 Money 값으로 복원 (다른 소스에서 변경되었을 수 있으므로)
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (GI)
	{
		UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
		if (RMgr && UIE_Resource_Money)
		{
			UIE_Resource_Money->SetValue(RMgr->GetResourceAmount(EResourceType::Money));
		}
	}

	CurrentCoinFlyout = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[OfficeLayerWidget] Coin flyout animation completed"));
}

UWidget* UOfficeLayerWidget::GetBackButtonWidget() const
{
	return BackButton;
}

// ========== 빈 좌석 버블 시스템 ==========

void UOfficeLayerWidget::EvaluateAllWorkstationBubbles()
{
	if (!BubbleContainer) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UOfficeManager* OfficeMgr = World->GetSubsystem<UOfficeManager>();
	if (!OfficeMgr) return;

	for (AWorkstationActorBase* Workstation : OfficeMgr->GetPlacedWorkstations())
	{
		if (!Workstation) continue;

		const EBubbleType Type = EvaluateBubbleTypeForWorkstation(Workstation);
		// 액터 UniqueID = 세션 내 안정 키. 빈 자리면 EmptySeat, 아니면 None(제거)
		BubbleContainer->UpdateBubbleForAnchor(
			static_cast<int32>(Workstation->GetUniqueID()), Workstation, Type);
	}
}

EBubbleType UOfficeLayerWidget::EvaluateBubbleTypeForWorkstation(AWorkstationActorBase* Workstation) const
{
	if (!Workstation) return EBubbleType::None;

	// 미배정(직원 없는) 좌석이 하나라도 있으면 빈 좌석 버블 표시
	return (Workstation->FindEmptyAssignmentSlot() != INDEX_NONE)
		? EBubbleType::EmptySeat
		: EBubbleType::None;
}

void UOfficeLayerWidget::HandleBubbleAction(int32 Key, EBubbleType Type)
{
	if (Type != EBubbleType::EmptySeat || !BubbleContainer) return;

	AWorkstationActorBase* Workstation = Cast<AWorkstationActorBase>(BubbleContainer->GetBubbleAnchorActor(Key));
	if (!Workstation) return;

	// 기존 책상 클릭과 동일 경로 재사용 (InteractableInputHandler::OpenWorkstationPanel)
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn) return;

	if (UInteractableInputHandler* Handler = Pawn->FindComponentByClass<UInteractableInputHandler>())
	{
		Handler->OpenWorkstationPanel(Workstation);
	}
}

// ========== 농땡이 캐치 펄스 링 ==========

UCatchRingWidget* UOfficeLayerWidget::GetOrCreateCatchRing(int32 PoolIndex)
{
	if (CatchRingPool.IsValidIndex(PoolIndex) && CatchRingPool[PoolIndex])
	{
		return CatchRingPool[PoolIndex];
	}

	if (!InGameCanvas) return nullptr;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return nullptr;

	// 무인 런타임 위젯이라 WBP/DT 등록 없이 StaticClass 직접 생성 (SelectionChevron 선례)
	UCatchRingWidget* Ring = CreateWidget<UCatchRingWidget>(PC, UCatchRingWidget::StaticClass());
	if (!Ring) return nullptr;

	UCanvasPanelSlot* RingSlot = InGameCanvas->AddChildToCanvas(Ring);
	if (RingSlot)
	{
		RingSlot->SetAutoSize(true);
		RingSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		// 월드 추적 오버레이는 HUD 크롬 뒤로 — BubbleContainer(-1)와 동일. 미설정 시 후순위 Add 라 크롬 위를 덮음.
		RingSlot->SetZOrder(-1);
	}

	// 재사용 풀이라 중복 바인딩 가드 후 배선 (매 재배정마다 재바인딩하지 않도록 생성 시 1회).
	Ring->OnCaught.RemoveAll(this);
	Ring->OnCaught.AddUObject(this, &UOfficeLayerWidget::HandleCatchRingCaught);

	// 인덱스 슬롯 보장(순차 Add 만 — PoolIndex 는 항상 Num() 와 일치하게 호출).
	CatchRingPool.Add(Ring);
	return Ring;
}

UGestureHintWidget* UOfficeLayerWidget::GetOrCreateCatchHint(int32 PoolIndex)
{
	if (CatchHintPool.IsValidIndex(PoolIndex) && CatchHintPool[PoolIndex]) return CatchHintPool[PoolIndex];
	if (!InGameCanvas) return nullptr;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UTableManagerSubsystem* TblMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> Cls = TblMgr ? TblMgr->GetWidgetClass(EWidgetType::GestureHint) : nullptr;
	if (!PC || !Cls) return nullptr;

	UGestureHintWidget* Hint = CreateWidget<UGestureHintWidget>(PC, Cls);
	if (!Hint) return nullptr;

	Hint->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* HintSlot = InGameCanvas->AddChildToCanvas(Hint))
	{
		HintSlot->SetAutoSize(true);
		HintSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		HintSlot->SetZOrder(-1); // 링과 같은 층 — 나중에 추가돼 링 위에 그려진다
	}

	CatchHintPool.SetNum(FMath::Max(CatchHintPool.Num(), PoolIndex + 1));
	CatchHintPool[PoolIndex] = Hint;
	return Hint;
}

void UOfficeLayerWidget::HideCatchHint(int32 PoolIndex)
{
	if (!CatchHintPool.IsValidIndex(PoolIndex) || !CatchHintPool[PoolIndex]) return;
	if (CatchHintPool[PoolIndex]->GetVisibility() != ESlateVisibility::Collapsed)
	{
		CatchHintPool[PoolIndex]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOfficeLayerWidget::PlaceCatchHint(int32 PoolIndex, UCatchRingWidget* InRing, AOfficeworker* InWorker)
{
	if (!InRing || !InWorker || !InWorker->BehaviorComponent) { HideCatchHint(PoolIndex); return; }

	const EFatigueSlackPhase Phase = InWorker->BehaviorComponent->GetFatigueSlackPhase();
	const FName Key = CatchHintRules::ResolveKey(Phase);

	UPanelIntroSubsystem* Intro = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	const bool bGraduated = !Intro || Key.IsNone() || Intro->IsHintGraduated(Key, CatchHintRules::GraduationCount);
	if (bGraduated || InRing->GetVisibility() != ESlateVisibility::Visible) { HideCatchHint(PoolIndex); return; }

	UGestureHintWidget* Hint = GetOrCreateCatchHint(PoolIndex);
	if (!Hint) return;

	Hint->SetGesture(EGestureHintKind::Tap);
	Hint->SetLabel(CatchHintRules::ResolveLabel(Phase));

	const UCanvasPanelSlot* RingSlot = Cast<UCanvasPanelSlot>(InRing->Slot);
	if (UCanvasPanelSlot* HintSlot = Cast<UCanvasPanelSlot>(Hint->Slot))
	{
		HintSlot->SetPosition((RingSlot ? RingSlot->GetPosition() : FVector2D::ZeroVector) + CatchHintRules::HintOffset);
	}

	// 링의 투명 버튼이 탭을 계속 받아야 하므로 힌트는 히트테스트에서 빠진다
	if (Hint->GetVisibility() != ESlateVisibility::HitTestInvisible) Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UOfficeLayerWidget::ProjectWorkerAnchorToCanvas(const AOfficeworker* InWorker, FVector2D& OutCanvasLocal) const
{
	OutCanvasLocal = FVector2D::ZeroVector;
	if (!InWorker || !InGameCanvas) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return false;

	// 직원 몸통 아래쪽 — GetCenterLocation(허리) 에서 더 내림. 링 박스가 220px 라 위로 커서, 앵커를 낮춰야 머리 위로 안 뜨고 몸에 걸침.
	const FVector AnchorWorld = InWorker->GetCenterLocation() + FVector(0.0f, 0.0f, -30.0f);

	// BubbleContainer 의 검증된 변환: 3D 앵커 -> Viewport 로컬(논리 픽셀, DPI 보정) -> Absolute -> CanvasGeo.AbsoluteToLocal.
	FVector2D ViewportPos;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, AnchorWorld, ViewportPos, true))
	{
		return false;
	}

	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(World);
	const FVector2D AbsolutePos = ViewportGeo.LocalToAbsolute(ViewportPos);

	const FGeometry CanvasGeo = InGameCanvas->GetCachedGeometry();
	OutCanvasLocal = CanvasGeo.AbsoluteToLocal(AbsolutePos);

	const FVector2D CanvasSize = CanvasGeo.GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return false;
	}

	constexpr float OffscreenMargin = 120.0f;
	if (OutCanvasLocal.X < -OffscreenMargin || OutCanvasLocal.X > CanvasSize.X + OffscreenMargin ||
		OutCanvasLocal.Y < -OffscreenMargin || OutCanvasLocal.Y > CanvasSize.Y + OffscreenMargin)
	{
		return false;
	}

	return true;
}

void UOfficeLayerWidget::ScanAwaitingCatchWorkers()
{
	// [Perf] 전체 액터 순회는 여기서만(스로틀 호출). 보통 0~소수만 텔레그래프 상태.
	AwaitingCatchWorkers.Reset();

	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AOfficeworker> It(World); It; ++It)
	{
		AOfficeworker* W = *It;
		if (!W || W->bIsPortraitMode || !W->BehaviorComponent) continue;
		if (!W->BehaviorComponent->IsAwaitingCatch()) continue;
		AwaitingCatchWorkers.Add(W);
	}
}

void UOfficeLayerWidget::PlaceCatchRing(UCatchRingWidget* InRing, AOfficeworker* InWorker)
{
	if (!InRing) return;

	FVector2D CanvasLocal;
	const bool bOnScreen = ProjectWorkerAnchorToCanvas(InWorker, CanvasLocal);

	const ESlateVisibility DesiredVis = bOnScreen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (InRing->GetVisibility() != DesiredVis)
	{
		InRing->SetVisibility(DesiredVis);
	}

	if (bOnScreen)
	{
		if (UCanvasPanelSlot* RingSlot = Cast<UCanvasPanelSlot>(InRing->Slot))
		{
			RingSlot->SetPosition(CanvasLocal);
		}
	}
}

void UOfficeLayerWidget::RefreshCatchRings()
{
	if (!InGameCanvas) return;

	// 인덱스 점유 맵 — 파열 잔상 재생 중인 링은 앵커 직원이 이미 캐치 해제됐어도 재배정 대상에서 빼야 한다.
	TArray<bool, TInlineAllocator<8>> Claimed;
	Claimed.Init(false, CatchRingPool.Num());

	TArray<AOfficeworker*, TInlineAllocator<8>> BurstingWorkers;

	// 1) 파열 중인 링 유지 — 클릭 프레임에 회수하면 잔상이 한 프레임도 안 보인다.
	for (int32 i = 0; i < CatchRingPool.Num(); ++i)
	{
		UCatchRingWidget* PooledRing = CatchRingPool[i];
		if (!PooledRing || !PooledRing->IsBurstActive()) continue;

		Claimed[i] = true;
		HideCatchHint(i); // 캐치됐으니 손은 즉시 사라진다

		AOfficeworker* BurstWorker = PooledRing->GetWorker();
		if (!BurstWorker || BurstWorker->bIsPortraitMode)
		{
			// 앵커 소실 — 위치를 못 잡으므로 그냥 숨기고 점유만 유지(다음 틱 파열 종료 후 정상 회수).
			if (PooledRing->GetVisibility() != ESlateVisibility::Collapsed)
			{
				PooledRing->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}

		BurstingWorkers.Add(BurstWorker);
		PlaceCatchRing(PooledRing, BurstWorker);
	}

	// 2) 캐시된 대기 직원에게 미점유 링 배정 — 위치는 매 틱 재투영해 카메라 추적 유지.
	int32 NextFree = 0;
	for (const TWeakObjectPtr<AOfficeworker>& WPtr : AwaitingCatchWorkers)
	{
		AOfficeworker* W = WPtr.Get();
		if (!W || W->bIsPortraitMode || !W->BehaviorComponent) continue;
		// 스캔(≤0.15s 전) 이후 텔레그래프가 끝났을 수 있으니 재확인 — 캐치 윈도우 종료 즉시 숨김.
		if (!W->BehaviorComponent->IsAwaitingCatch()) continue;
		// 파열 링이 이미 이 직원을 물고 있으면 중복 링 금지.
		if (BurstingWorkers.Contains(W)) continue;

		while (Claimed.IsValidIndex(NextFree) && Claimed[NextFree]) ++NextFree;

		UCatchRingWidget* Ring = GetOrCreateCatchRing(NextFree);
		if (!Ring) continue;

		if (Claimed.IsValidIndex(NextFree))
		{
			Claimed[NextFree] = true;
		}
		else
		{
			// GetOrCreateCatchRing 이 풀 끝에 새로 추가한 항목 — 점유 맵 길이를 맞춘다.
			Claimed.Add(true);
		}
		const int32 RingIndex = NextFree;
		++NextFree;

		Ring->SetWorker(W);
		PlaceCatchRing(Ring, W);
		PlaceCatchHint(RingIndex, Ring, W);
	}

	// 3) 미점유 링은 숨김(파괴 대신 재사용) + 앵커 해제
	for (int32 i = 0; i < CatchRingPool.Num(); ++i)
	{
		if (!CatchRingPool[i]) continue;
		if (Claimed.IsValidIndex(i) && Claimed[i]) continue;

		CatchRingPool[i]->SetWorker(nullptr);
		HideCatchHint(i);
		if (CatchRingPool[i]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			CatchRingPool[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UOfficeLayerWidget::HandleCatchRingCaught(EFatigueSlackPhase CaughtPhase)
{
	UWorld* CatchWorld = GetWorld();
	const double Now = CatchWorld ? CatchWorld->GetTimeSeconds() : 0.0;

	// 연타 감쇠 — 0.8초 이상 조용했으면 원음(1.0), 연타 중이면 ×0.7(하한 0.3). 피치 ±6% 랜덤으로 기계적 반복감 제거.
	CatchSoundVolume = (Now - LastCatchSoundTime >= 0.8) ? 1.0f : FMath::Max(0.3f, CatchSoundVolume * 0.7f);
	LastCatchSoundTime = Now;

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISoundWithParams(CGUISoundTags::Catch, CatchSoundVolume, FMath::FRandRange(0.94f, 1.06f));
		}
	}

	// IncrementHint 는 호출마다 풀세이브라 졸업 후에는 부르지 않는다
	const FName HintKey = CatchHintRules::ResolveKey(CaughtPhase);
	if (UPanelIntroSubsystem* Intro = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr)
	{
		if (!HintKey.IsNone() && !Intro->IsHintGraduated(HintKey, CatchHintRules::GraduationCount))
		{
			Intro->IncrementHint(HintKey);
		}
	}
}
