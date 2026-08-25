// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/FactoryPanelWidget.h"
#include "UI/Panel/FactoryPanelInteractionPolicy.h"

#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Entity/Factory/BrickFactory.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Enum/ResourceType.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Data/FactoryUpgradeConfig.h"
#include "Data/FactoryUpgradeData.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Enum/WidgetType.h"

namespace
{
	// 넛지 타겟 = 해당 강화 타입 슬롯의 [강화] 버튼. 버튼(Optional)이 없으면 슬롯 전체에 링.
	UWidget* ResolveNudgeTarget(const TMap<EFactoryUpgradeType, UUpgradeSlot*>& Slots, EFactoryUpgradeType Type)
	{
		UUpgradeSlot* FoundSlot = Slots.FindRef(Type);
		if (!FoundSlot)
		{
			return nullptr;
		}
		if (UWidget* Btn = FoundSlot->GetUpgradeButton())
		{
			return Btn;
		}
		return FoundSlot;
	}

	FText GetFactoryAutoLockText(const TArray<FFactoryUpgradeDefinition>& Definitions)
	{
		int32 RequiredHQLevel = 0;
		if (!FactoryUpgradeUnlockPolicy::TryResolveRequiredHQLevel(Definitions, RequiredHQLevel))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[FactoryPanel] Invalid auto-unlock RequiredHQLevel data"));
			return NSLOCTEXT(
				"FactoryPanel", "InvalidAutoUnlockCondition", "해금 조건을 불러올 수 없습니다");
		}

		return FText::Format(
			NSLOCTEXT("FactoryPanel", "HQLevelUnlockCondition", "본사 레벨 {0} 달성 시 해금"),
			FText::AsNumber(RequiredHQLevel));
	}
}

void UFactoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UFactoryPanelWidget::OnCloseButtonClicked);
	}

	// 배율 선택기 구독 + 세션 모드 복원 (탭이 없어 상시 표시 — 가시성 토글 없음)
	if (BulkModeSelector)
	{
		BulkModeSelector->OnModeChanged.AddUObject(this, &UFactoryPanelWidget::ApplyBulkMode);
		BulkModeSelector->SetMode(BulkMode);
	}
}

void UFactoryPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	CGRFactoryPanelInteraction::ConfigureWorldPassThrough(this, BackgroundBtn);

	if (!CurrentFactory)
	{
		for (TActorIterator<ABrickFactory> It(GetWorld()); It; ++It)
		{
			CurrentFactory = *It;
			break;
		}
	}

	UResourceItemManager* ResourceManager = GetWorld()->GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (ResourceManager)
	{
		ResourceManager->OnResourceChanged.AddUObject(this, &UFactoryPanelWidget::OnResourceChanged);
	}

	if (CurrentFactory)
	{
		// 재사용 위젯이라 매 오픈 재바인딩 (RemoveAll 가드 후)
		CurrentFactory->OnFactoryUpgraded.RemoveDynamic(this, &UFactoryPanelWidget::HandleFactoryUpgraded);
		CurrentFactory->OnFactoryUpgraded.AddDynamic(this, &UFactoryPanelWidget::HandleFactoryUpgraded);

		RebuildUpgradeSlots();
		TryPlayAutoUnlockCelebration();
	}

	// 슬롯이 채워진 뒤 등록 — 등록 즉시 미션이 NudgeUpgrade 로 전진하므로 순서가 뒤집히면 첫 프레임 타겟이 빈다
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->RegisterFactoryPanel(this);
	}
}

void UFactoryPanelWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->UnregisterFactoryPanel(this);
	}

	StopUpgradeHold();

	UResourceItemManager* ResourceManager = GetWorld()->GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (ResourceManager)
	{
		ResourceManager->OnResourceChanged.RemoveAll(this);
	}

	if (CurrentFactory)
	{
		CurrentFactory->OnFactoryUpgraded.RemoveDynamic(this, &UFactoryPanelWidget::HandleFactoryUpgraded);
	}

	CurrentFactory = nullptr;
}

UWidget* UFactoryPanelWidget::GetUpgradeNudgeTarget() const
{
	return ResolveNudgeTarget(DynamicSlots, EFactoryUpgradeType::HoldProductionSpeed);
}

UWidget* UFactoryPanelWidget::GetSecondUpgradeNudgeTarget() const
{
	return ResolveNudgeTarget(DynamicSlots, EFactoryUpgradeType::HoldProductionAmount);
}

void UFactoryPanelWidget::NativeDestruct()
{
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UFactoryPanelWidget::OnCloseButtonClicked);
	}

	if (BulkModeSelector)
	{
		BulkModeSelector->OnModeChanged.RemoveAll(this);
	}

	for (const TPair<EFactoryUpgradeType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		if (UUpgradeSlot* SlotItem = Pair.Value)
		{
			if (UCostActionButtonWidget* Btn = SlotItem->GetUpgradeButton())
			{
				Btn->OnClicked().RemoveAll(this);
				Btn->OnPressed().RemoveAll(this);
				Btn->OnReleased().RemoveAll(this);
			}
		}
	}

	Super::NativeDestruct();
}

void UFactoryPanelWidget::OnCloseButtonClicked()
{
	RequestClose();
}

void UFactoryPanelWidget::RequestClose()
{
	CloseWithAnimation();
}

void UFactoryPanelWidget::SetFactory(ABrickFactory* Factory)
{
	if (CurrentFactory && CurrentFactory != Factory)
	{
		CurrentFactory->OnFactoryUpgraded.RemoveDynamic(this, &UFactoryPanelWidget::HandleFactoryUpgraded);
	}

	CurrentFactory = Factory;

	if (CurrentFactory)
	{
		CurrentFactory->OnFactoryUpgraded.RemoveDynamic(this, &UFactoryPanelWidget::HandleFactoryUpgraded);
		CurrentFactory->OnFactoryUpgraded.AddDynamic(this, &UFactoryPanelWidget::HandleFactoryUpgraded);

		RebuildUpgradeSlots();
	}
}

void UFactoryPanelWidget::HandleFactoryUpgraded(EFactoryUpgradeType UpgradeType)
{
	if (!CurrentFactory)
	{
		return;
	}

	// 강화 틱마다 재빌드하면 홀드(10Hz)에서 슬롯이 통째로 다시 만들어진다 — 잠금 표시가 바뀐 순간만
	const bool bNowLocked = !CurrentFactory->IsAutoCollectionUnlocked();
	if (bNowLocked == bAutoSlotsShownAsLocked)
	{
		return;
	}

	RebuildUpgradeSlots();
	TryPlayAutoUnlockCelebration();
}

void UFactoryPanelWidget::TryPlayAutoUnlockCelebration()
{
	if (!CurrentFactory || !CurrentFactory->ConsumeAutoUnlockCelebration())
	{
		return;
	}

	// 자동 강화 2슬롯은 시각만, 재고 슬롯에서 사운드 1회 — 같은 연출이 3번 울리지 않게
	if (UUpgradeSlot* AutoSlot = DynamicSlots.FindRef(EFactoryUpgradeType::AutoCollection))
	{
		AutoSlot->PlayUpgradeEffect(false);
	}
	if (UUpgradeSlot* CapacitySlot = DynamicSlots.FindRef(EFactoryUpgradeType::AutoCollectionCapacity))
	{
		CapacitySlot->PlayUpgradeEffect(false);
	}
	if (UUpgradeSlot* StockSlot = DynamicSlots.FindRef(EFactoryUpgradeType::BrickStock))
	{
		StockSlot->PlayUpgradeEffect(true);
	}

	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ShowNotification(
			NSLOCTEXT("Factory", "AutoCollectUnlocked", "이제 벽돌이 자동으로 쌓입니다. 재고에서 수령할 수 있습니다"),
			4.0f,
			ENotificationType::Success);
	}
}

void UFactoryPanelWidget::RefreshUI()
{
	if (CurrentFactory)
	{
		UpdateAllUpgradeSlots();
		UpdateBrickStockSlot();
	}
}

// ===== DT 기반 동적 슬롯 생성 =====

void UFactoryPanelWidget::RebuildUpgradeSlots()
{
	if (!CurrentFactory || !UpgradeSlotContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryPanel] RebuildUpgradeSlots: CurrentFactory 또는 Container 없음"));
		return;
	}

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[FactoryPanel] TableManagerSubsystem 없음"));
		return;
	}

	TSubclassOf<UUserWidget> SlotClass = TableMgr->GetWidgetClass(EWidgetType::UpgradeSlot);
	if (!SlotClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[FactoryPanel] UpgradeSlot 위젯 클래스 없음 — DT_Widget 에 EWidgetType::UpgradeSlot 행 확인 필요"));
		return;
	}

	// 기존 슬롯 버튼 언바인딩
	for (const TPair<EFactoryUpgradeType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		if (UUpgradeSlot* SlotItem = Pair.Value)
		{
			if (UCostActionButtonWidget* Btn = SlotItem->GetUpgradeButton())
			{
				Btn->OnClicked().RemoveAll(this);
				Btn->OnPressed().RemoveAll(this);
				Btn->OnReleased().RemoveAll(this);
			}
		}
	}

	UpgradeSlotContainer->ClearChildren();
	DynamicSlots.Reset();

	bAutoSlotsShownAsLocked = !CurrentFactory->IsAutoCollectionUnlocked();

	const TArray<FFactoryUpgradeDefinition> Defs = TableMgr->GetAllFactoryUpgradeDefinitions();
	FText AutoLockText;
	if (bAutoSlotsShownAsLocked)
	{
		AutoLockText = GetFactoryAutoLockText(Defs);
	}

	for (const FFactoryUpgradeDefinition& Def : Defs)
	{
		UUpgradeSlot* NewSlot = CreateWidget<UUpgradeSlot>(this, SlotClass);
		if (!NewSlot) continue;

		ApplySlotDefinition(NewSlot, Def);

		const bool bLocked = bAutoSlotsShownAsLocked
			&& FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Def.UpgradeType);
		if (bLocked)
		{
			NewSlot->LockConditionText = AutoLockText;
		}

		BindSlotButton(NewSlot, Def.UpgradeType);

		UpgradeSlotContainer->AddChild(NewSlot);
		DynamicSlots.Add(Def.UpgradeType, NewSlot);

		// 초기 UI 갱신
		if (Def.bIsStatusSlot)
		{
			UpdateBrickStockSlot();
		}
		else
		{
			UpdateUpgradeSlot(Def.UpgradeType, NewSlot);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[FactoryPanel] Upgrade slots rebuilt: %d slots"), DynamicSlots.Num());
}

void UFactoryPanelWidget::ApplySlotDefinition(UUpgradeSlot* InSlot, const FFactoryUpgradeDefinition& Def)
{
	if (!InSlot) return;

	InSlot->Description = Def.DisplayName;
	InSlot->SubDescription = Def.SubDescription;
	InSlot->ValueUnit = Def.ValueUnit;
	InSlot->bIsInteger = Def.bIsInteger;
	InSlot->CostResourceType = Def.CostResourceType;

	if (!Def.Icon.IsNull())
	{
		InSlot->Icon = Def.Icon;
	}
}

void UFactoryPanelWidget::BindSlotButton(UUpgradeSlot* InSlot, EFactoryUpgradeType Type)
{
	if (!InSlot) return;
	UCostActionButtonWidget* Btn = InSlot->GetUpgradeButton();
	if (!Btn) return;

	Btn->OnClicked().RemoveAll(this);
	Btn->OnPressed().RemoveAll(this);
	Btn->OnReleased().RemoveAll(this);

	if (Type == EFactoryUpgradeType::BrickStock)
	{
		Btn->OnClicked().AddUObject(this, &UFactoryPanelWidget::OnCollectAutoResourcesClicked);
	}
	else
	{
		// x1 = Pressed 1회 강화 + 홀드 타이머, x10/x50 = 탭당 벌크 1회 구매 (홀드 비활성)
		Btn->OnPressed().AddWeakLambda(this, [this, Type]()
		{
			if (BulkMode == EEnhanceBulkMode::x1)
			{
				OnUpgradeClicked(Type);
				StartUpgradeHold(Type);
			}
			else
			{
				OnBulkUpgrade(Type);
			}
		});
		Btn->OnReleased().AddUObject(this, &UFactoryPanelWidget::StopUpgradeHold);
	}
}

void UFactoryPanelWidget::UpdateUpgradeSlot(EFactoryUpgradeType Type, UUpgradeSlot* InSlot)
{
	if (!InSlot || !CurrentFactory) return;

	const int32 Level = CurrentFactory->GetUpgradeLevel(Type);
	const float CurrentValue = CurrentFactory->GetUpgradeValue(Type);

	// DT 단일 진실 — 비용 곡선/상한/자원타입. 미로드 시 FFactoryUpgradeConfig 폴백.
	FFactoryCostCurve CostCurve;
	int32 MaxLevel = 0;
	EResourceType CostType = EResourceType::Money;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FFactoryUpgradeDefinition Def;
	if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(Type, Def))
	{
		CostCurve = Def.GetCostCurve();
		MaxLevel = Def.MaxLevel;
		CostType = Def.CostResourceType;
	}
	else
	{
		CostCurve = FFactoryUpgradeConfig::GetFallbackCostCurve(Type);
		MaxLevel = FFactoryUpgradeConfig::GetMaxLevel(Type);
	}
	const int64 Cost = CostCurve.CostAtLevel(Level);

	const bool bIsLocked = FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Type)
		&& !CurrentFactory->IsAutoCollectionUnlocked();

	// 벌크 모드 — 잠금 슬롯은 단건 표시. BrickStock 은 UpdateBrickStockSlot 경유라 여기 도달 안 함(벌크 대상 제외)
	const bool bBulkMode = (BulkMode != EEnhanceBulkMode::x1) && !bIsLocked;

	int32 BulkCount = 1;
	int64 BulkTotalCost = 0;
	int64 BulkAvailable = 0;
	if (bBulkMode)
	{
		BulkCount = ComputeBulkCount(Level, MaxLevel, CostCurve, CostType, BulkTotalCost, BulkAvailable);
	}

	// 델타 = +N레벨 합산 효과 (Count=0 이면 1레벨 기준 유지)
	const float NextValue = FFactoryUpgradeConfig::CalculateUpgradeValue(Type, Level + (bBulkMode ? FMath::Max(BulkCount, 1) : 1));

	InSlot->UpdateInfo(Level, CurrentValue, NextValue, Cost, bIsLocked, MaxLevel,
		bBulkMode, BulkCount, bBulkMode ? BulkTotalCost : Cost, BulkAvailable);
}

void UFactoryPanelWidget::UpdateAllUpgradeSlots()
{
	for (const TPair<EFactoryUpgradeType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		if (Pair.Key == EFactoryUpgradeType::BrickStock) continue;
		UpdateUpgradeSlot(Pair.Key, Pair.Value);
	}
}

void UFactoryPanelWidget::UpdateBrickStockSlot()
{
	UUpgradeSlot* StockSlot = DynamicSlots.FindRef(EFactoryUpgradeType::BrickStock);
	if (!StockSlot || !CurrentFactory) return;

	// 재고 슬롯의 두 값은 현재/최대라 '진척'이 아니다 — 델타 표기가 남은 용량으로 오독된다
	StockSlot->bShowDelta = false;

	int32 CurrentAmount = CurrentFactory->GetAutoCollectedAmount();
	int32 MaxCapacity = FMath::RoundToInt(CurrentFactory->GetUpgradeValue(EFactoryUpgradeType::AutoCollectionCapacity));
	bool bIsLocked = !CurrentFactory->IsAutoCollectionUnlocked();

	StockSlot->UpdateInfo(0, CurrentAmount, MaxCapacity, CurrentAmount, bIsLocked);

	if (UCostActionButtonWidget* CollectButton = StockSlot->GetUpgradeButton())
	{
		bool bCanCollect = !bIsLocked && CurrentAmount > 0;
		CollectButton->SetEnabled(bCanCollect);
	}
}

// ===== 업그레이드 핸들러 =====

void UFactoryPanelWidget::OnUpgradeClicked(EFactoryUpgradeType Type)
{
	if (!CurrentFactory) return;

	if (CurrentFactory->UpgradeFactory(Type))
	{
		OnUpgradeSuccess(Type);
		UE_LOG(LogTemp, Log, TEXT("[FactoryPanel] Upgraded type %d"), (int32)Type);
	}
}

void UFactoryPanelWidget::OnBulkUpgrade(EFactoryUpgradeType Type)
{
	if (!CurrentFactory) return;

	const int32 CurrentLevel = CurrentFactory->GetUpgradeLevel(Type);

	// DT 단일 진실 — 비용 곡선/상한/자원타입
	FFactoryCostCurve CostCurve;
	int32 MaxLevel = 0;
	EResourceType CostType = EResourceType::Money;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FFactoryUpgradeDefinition Def;
	if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(Type, Def))
	{
		CostCurve = Def.GetCostCurve();
		MaxLevel = Def.MaxLevel;
		CostType = Def.CostResourceType;
	}
	else
	{
		CostCurve = FFactoryUpgradeConfig::GetFallbackCostCurve(Type);
		MaxLevel = FFactoryUpgradeConfig::GetMaxLevel(Type);
	}

	int64 TotalCost = 0;
	int64 Available = 0;
	const int32 Count = ComputeBulkCount(CurrentLevel, MaxLevel, CostCurve, CostType, TotalCost, Available);

	// 1차 방어는 UpgradeBtn 의 부족 입력 삼킴 — 여기는 잔여 0/자금 부족 경로의 방어적 안내
	if (Count <= 0 || Available < TotalCost)
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	if (!CurrentFactory->UpgradeFactory(Type, Count))
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	// 벌크 1회 구매 = 이펙트 1회
	if (UUpgradeSlot* TargetSlot = DynamicSlots.FindRef(Type))
	{
		TargetSlot->PlayUpgradeEffect();
	}

	// 자금이 크게 움직였으니 전 슬롯 벌크 카운트/비용 재계산 (OnResourceChanged 도 태우지만 즉시성 확보)
	UpdateAllUpgradeSlots();
	SaveFactoryData();
}

int32 UFactoryPanelWidget::ComputeBulkCount(int32 CurrentLevel, int32 MaxLevel, const FFactoryCostCurve& CostCurve,
	EResourceType CostType, int64& OutTotalCost, int64& OutAvailable) const
{
	OutTotalCost = 0;
	OutAvailable = 0;

	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		OutAvailable = ResMgr->GetResourceAmount(CostType);
	}

	int32 Count = 1;
	switch (BulkMode)
	{
	case EEnhanceBulkMode::x10: Count = 10; break;
	case EEnhanceBulkMode::x50: Count = 50; break;
	default: break;
	}

	// MaxLevel(0=무제한) 잔여 + 액터 하드 가드(50)와 동일 상한
	if (MaxLevel > 0)
	{
		Count = FMath::Min(Count, FMath::Max(0, MaxLevel - CurrentLevel));
	}
	Count = FMath::Min(Count, 50);

	// Count=0(잔여 0)이어도 부족액 표기는 1레벨 비용 기준
	OutTotalCost = CostCurve.BulkCost(CurrentLevel, FMath::Max(Count, 1));
	return Count;
}

void UFactoryPanelWidget::ApplyBulkMode(EEnhanceBulkMode NewMode)
{
	if (BulkMode == NewMode)
	{
		return;
	}
	BulkMode = NewMode;

	// x1 → 벌크 전환 직후 유령 홀드 방지
	StopUpgradeHold();

	// 전 슬롯 벌크 카운트/비용/델타 재계산
	UpdateAllUpgradeSlots();
}

void UFactoryPanelWidget::OnCollectAutoResourcesClicked()
{
	if (!CurrentFactory) return;

	const int32 Collected = CurrentFactory->GetAutoCollectedAmount();
	CurrentFactory->CollectAutoResources();
	UpdateBrickStockSlot();

	if (Collected > 0)
	{
		UGameInstance* GI = GetGameInstance();
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
			{
				InGame->SpawnGainPopup(EResourceType::Brick, Collected);
			}
		}
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::RewardCoin);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[FactoryPanel] Auto resources collected (+%d)"), Collected);
}

void UFactoryPanelWidget::OnUpgradeSuccess(EFactoryUpgradeType UpgradeType)
{
	if (UUpgradeSlot* TargetSlot = DynamicSlots.FindRef(UpgradeType))
	{
		UpdateUpgradeSlot(UpgradeType, TargetSlot);
		TargetSlot->PlayUpgradeEffect();
	}

	// 자동 용량 업그레이드 시 BrickStock 슬롯도 갱신 (MaxCapacity 변경)
	if (UpgradeType == EFactoryUpgradeType::AutoCollectionCapacity)
	{
		UpdateBrickStockSlot();
	}

	SaveFactoryData();
}

// ===== Hold =====

void UFactoryPanelWidget::StartUpgradeHold(EFactoryUpgradeType UpgradeType)
{
	CurrentHoldUpgradeType = UpgradeType;

	// 홀드 중엔 자금 재충전마다 구매가능 펀치가 반복된다 — 홀드 구간만 억제
	if (UUpgradeSlot* HeldSlot = DynamicSlots.FindRef(UpgradeType))
	{
		HeldSlot->SetPunchSuppressed(true);
	}

	GetWorld()->GetTimerManager().SetTimer(
		UpgradeHoldTimerHandle,
		this,
		&UFactoryPanelWidget::OnUpgradeHoldTick,
		HoldRepeatInterval,
		true,
		HoldInitialDelay
	);
}

void UFactoryPanelWidget::StopUpgradeHold()
{
	GetWorld()->GetTimerManager().ClearTimer(UpgradeHoldTimerHandle);

	if (UUpgradeSlot* HeldSlot = DynamicSlots.FindRef(CurrentHoldUpgradeType))
	{
		HeldSlot->SetPunchSuppressed(false);
	}
}

void UFactoryPanelWidget::OnUpgradeHoldTick()
{
	if (!CurrentFactory) return;

	if (CurrentFactory->UpgradeFactory(CurrentHoldUpgradeType))
	{
		OnUpgradeSuccess(CurrentHoldUpgradeType);
	}
	else
	{
		StopUpgradeHold();
	}
}

// ===== 기타 =====

void UFactoryPanelWidget::OnResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type == EResourceType::Money && CurrentFactory)
	{
		UpdateAllUpgradeSlots();
	}
}

void UFactoryPanelWidget::SaveFactoryData()
{
	// 지연 저장 — 강화 홀드(10Hz) 경로에서 호출되므로 즉시 동기 저장 금지
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (USaveLoadManager* SaveManager = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveManager->RequestDeferredSave();
		}
	}
}
