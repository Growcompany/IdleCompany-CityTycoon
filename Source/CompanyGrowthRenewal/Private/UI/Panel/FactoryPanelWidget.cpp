// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/FactoryPanelWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Panel/FactoryPanelInteractionPolicy.h"

#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Entity/Factory/BrickFactory.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"
#include "UI/UIBase.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"
#include "Player/Components/PlacementHandler.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Enum/ResourceType.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Data/FactoryUpgradeConfig.h"
#include "Data/FactoryUpgradeData.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonTextBlock.h"
#include "Global/GlobalUtilFunctions.h"
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

	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UFactoryPanelWidget::OnCloseButtonClicked);
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UFactoryPanelWidget::OnCloseButtonClicked);
	}
	if (RelocateButton)
	{
		RelocateButton->OnClicked().RemoveAll(this);
		RelocateButton->OnClicked().AddUObject(this, &UFactoryPanelWidget::HandleRelocateRequested);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryPanel] RelocateButton is missing"));
	}

	// 배율 선택기 구독 + 세션 모드 복원 (탭이 없어 상시 표시 — 가시성 토글 없음)
	if (BulkModeSelector)
	{
		BulkModeSelector->OnModeChanged.RemoveAll(this);
		BulkModeSelector->OnModeChanged.AddUObject(this, &UFactoryPanelWidget::ApplyBulkMode);
		BulkModeSelector->SetMode(BulkMode);
	}

}

void UFactoryPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	bCloseRequested = false;
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
		ResourceManager->OnResourceChanged.RemoveAll(this);
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

	// 슬롯이 채워진 뒤 등록 — 등록 즉시 미션이 인부 추가 단계로 전진할 수 있다.
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

UWidget* UFactoryPanelWidget::GetLaborerUpgradeTarget() const
{
	return ResolveNudgeTarget(DynamicSlots, EFactoryUpgradeType::LaborerCount);
}

UWidget* UFactoryPanelWidget::GetFactoryExpansionUpgradeTarget() const
{
	return ResolveNudgeTarget(DynamicSlots, EFactoryUpgradeType::FactoryExpansion);
}

UWidget* UFactoryPanelWidget::GetUpgradeTarget(EFactoryUpgradeType Type) const
{
	return ResolveNudgeTarget(DynamicSlots, Type);
}

UWidget* UFactoryPanelWidget::GetFirstAffordableUpgradeTarget() const
{
	if (!CurrentFactory) { return nullptr; }
	for (uint8 Index = static_cast<uint8>(EFactoryUpgradeType::HoldProductionSpeed);
		Index <= static_cast<uint8>(EFactoryUpgradeType::FactoryExpansion); ++Index)
	{
		const EFactoryUpgradeType Type = static_cast<EFactoryUpgradeType>(Index);
		if (CurrentFactory->GetAllowedUpgradeCount(Type) <= 0
			|| (FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Type)
				&& !CurrentFactory->IsAutoCollectionUnlocked())) { continue; }
		const int32 MaxLevel = CurrentFactory->GetUpgradeMaxLevel(Type);
		if (MaxLevel == INDEX_NONE || ((MaxLevel > 0 || Type == EFactoryUpgradeType::LaborerCount)
			&& CurrentFactory->GetUpgradeLevel(Type) >= MaxLevel)) { continue; }
		const UUpgradeSlot* UpgradeSlot = DynamicSlots.FindRef(Type);
		UCostActionButtonWidget* Button = UpgradeSlot ? UpgradeSlot->GetUpgradeButton() : nullptr;
		if (Button && Button->CanAfford())
		{
			return Button;
		}
	}
	return nullptr;
}

void UFactoryPanelWidget::NativeDestruct()
{
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UFactoryPanelWidget::OnCloseButtonClicked);
	}
	if (RelocateButton)
	{
		RelocateButton->OnClicked().RemoveAll(this);
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

UWidget* UFactoryPanelWidget::GetCloseButtonWidget() const
{
	return PanelHeader ? PanelHeader->GetCloseButton() : nullptr;
}

void UFactoryPanelWidget::OnCloseButtonClicked()
{
	RequestClose();
}

void UFactoryPanelWidget::RequestClose()
{
	bCloseRequested = true;
	StopUpgradeHold();
	CloseWithAnimation();
}

void UFactoryPanelWidget::HandleRelocateRequested()
{
	ABrickFactory* Factory = CurrentFactory;
	if (!IsValid(Factory) || Factory->IsRelocating())
	{
		return;
	}

	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	AMainMapPlayerController* PlayerController = GameInstance
		? Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController()) : nullptr;
	APlayerCamera* PlayerCam = PlayerController ? Cast<APlayerCamera>(PlayerController->GetPawn()) : nullptr;
	UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIMgr = GameInstance ? GameInstance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	TSubclassOf<UUserWidget> PlacementClass = TableMgr
		? TableMgr->GetWidgetClass(EWidgetType::BuildPlacementPanel) : nullptr;
	if (!IsValid(PlayerCam) || !IsValid(PlayerCam->PlacementHandler) || !IsValid(UIBase)
		|| !PlacementClass || !PlacementClass->IsChildOf(UBuildPlacementPanelWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryPanel] Relocation prerequisites are missing"));
		return;
	}

	// Deactivation clears CurrentFactory and releases the panel's interaction state.
	DeactivateWidget();
	PlayerCam->StartBuildingRelocation(Factory);
	if (!Factory->IsRelocating())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryPanel] Factory relocation did not start"));
		return;
	}

	if (UBuildPlacementPanelWidget* PlacementWidget =
		Cast<UBuildPlacementPanelWidget>(UIBase->PushBottomClass(PlacementClass.Get())))
	{
		PlacementWidget->SetBuildableInfo(FBuildableCardTable(), true);
	}
	else
	{
		PlayerCam->PlacementHandler->CancelBuildingMove();
		PlayerCam->EndBuildingRelocation();
		UE_LOG(LogTemp, Warning, TEXT("[FactoryPanel] Failed to open relocation controls"));
	}
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

	// 강화 틱에는 슬롯 구조를 유지하고, 잠금 상태가 바뀌면 정의도 다시 반영한다.
	const bool bNowLocked = !CurrentFactory->IsAutoCollectionUnlocked();
	if (bNowLocked == bAutoSlotsShownAsLocked)
	{
		if (UpgradeType == EFactoryUpgradeType::FactoryExpansion || UpgradeType == EFactoryUpgradeType::LaborerCount)
		{
			UpdateAllUpgradeSlots();
			if (UpgradeType == EFactoryUpgradeType::LaborerCount && CurrentHoldUpgradeType == UpgradeType
				&& CurrentFactory->GetUpgradeLevel(UpgradeType) >= CurrentFactory->GetUpgradeMaxLevel(UpgradeType))
			{
				StopUpgradeHold();
			}
		}
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

	// 자동 강화 2슬롯은 시각만 — 같은 연출이 두 번 울리지 않게 사운드는 아래에서 1회
	if (UUpgradeSlot* AutoSlot = DynamicSlots.FindRef(EFactoryUpgradeType::LaborerCount))
	{
		AutoSlot->PlayUpgradeEffect(false);
	}
	if (UUpgradeSlot* CapacitySlot = DynamicSlots.FindRef(EFactoryUpgradeType::AutoCollectionCapacity))
	{
		CapacitySlot->PlayUpgradeEffect(false);
	}
	if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
	{
		SM->PlayUISound(CGUISoundTags::UpgradeSuccess);
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

	const TArray<FFactoryUpgradeDefinition> Defs = TableMgr->GetAllFactoryUpgradeDefinitions();
	TArray<const FFactoryUpgradeDefinition*> ValidDefs;
	ValidDefs.Reserve(Defs.Num());
	const UEnum* UpgradeTypeEnum = StaticEnum<EFactoryUpgradeType>();
	for (const FFactoryUpgradeDefinition& Def : Defs)
	{
		// UHT 의 _MAX 도 enum 값으로 등록되므로 실제 강화 타입만 슬롯으로 만든다.
		if (UpgradeTypeEnum && static_cast<int64>(Def.UpgradeType) >= UpgradeTypeEnum->GetMaxEnumValue())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[FactoryPanel] DT_FactoryUpgradeDefinition 에 enum 에 없는 행이 있어 건너뛴다 — '%s' 행 삭제 필요"),
				*Def.DisplayName.ToString());
			continue;
		}
		ValidDefs.Add(&Def);
	}

	bool bReuseSlots = DynamicSlots.Num() == ValidDefs.Num()
		&& UpgradeSlotContainer->GetChildrenCount() == ValidDefs.Num();
	if (bReuseSlots)
	{
		for (int32 Index = 0; Index < ValidDefs.Num(); ++Index)
		{
			UUpgradeSlot* ExistingSlot = DynamicSlots.FindRef(ValidDefs[Index]->UpgradeType);
			if (!IsValid(ExistingSlot) || ExistingSlot->GetClass() != SlotClass.Get()
				|| UpgradeSlotContainer->GetChildAt(Index) != ExistingSlot)
			{
				bReuseSlots = false;
				break;
			}
		}
	}

	if (!bReuseSlots)
	{
		StopUpgradeHold();
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
	}

	bAutoSlotsShownAsLocked = !CurrentFactory->IsAutoCollectionUnlocked();
	FText AutoLockText;
	if (bAutoSlotsShownAsLocked)
	{
		AutoLockText = GetFactoryAutoLockText(Defs);
	}

	for (const FFactoryUpgradeDefinition* DefPtr : ValidDefs)
	{
		const FFactoryUpgradeDefinition& Def = *DefPtr;
		UUpgradeSlot* SlotItem = bReuseSlots ? DynamicSlots.FindRef(Def.UpgradeType)
			: CreateWidget<UUpgradeSlot>(this, SlotClass);
		if (!SlotItem) continue;

		ApplySlotDefinition(SlotItem, Def);

		const bool bLocked = bAutoSlotsShownAsLocked
			&& FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Def.UpgradeType);
		SlotItem->LockConditionText = bLocked ? AutoLockText : FText::GetEmpty();

		BindSlotButton(SlotItem, Def.UpgradeType);

		if (!bReuseSlots)
		{
			UpgradeSlotContainer->AddChild(SlotItem);
			DynamicSlots.Add(Def.UpgradeType, SlotItem);
		}

		UpdateUpgradeSlot(Def.UpgradeType, SlotItem);
	}

	UE_LOG(LogTemp, Log, TEXT("[FactoryPanel] Upgrade slots %s: %d slots"),
		bReuseSlots ? TEXT("reused") : TEXT("rebuilt"), DynamicSlots.Num());
}

void UFactoryPanelWidget::ApplySlotDefinition(UUpgradeSlot* InSlot, const FFactoryUpgradeDefinition& Def)
{
	if (!InSlot) return;

	InSlot->SetRuntimePresentation(Def.DisplayName, Def.SubDescription, Def.CostResourceType);
	InSlot->ValueUnit = Def.ValueUnit;
	InSlot->bIsInteger = Def.bIsInteger;
	InSlot->Icon = Def.Icon;
	InSlot->SetIconTexture(Def.Icon.IsNull() ? nullptr : Def.Icon.LoadSynchronous());
}

void UFactoryPanelWidget::BindSlotButton(UUpgradeSlot* InSlot, EFactoryUpgradeType Type)
{
	if (!InSlot) return;
	UCostActionButtonWidget* Btn = InSlot->GetUpgradeButton();
	if (!Btn) return;

	Btn->OnClicked().RemoveAll(this);
	Btn->OnPressed().RemoveAll(this);
	Btn->OnReleased().RemoveAll(this);

	// x1 = Pressed 1회 강화 + 홀드 타이머, x10/x50 = 탭당 벌크 1회 구매 (홀드 비활성)
	Btn->OnPressed().AddWeakLambda(this, [this, Type]()
	{
		if (BulkMode == EEnhanceBulkMode::x1)
		{
			const UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
			const FName MissionBefore = Missions ? Missions->GetActiveMissionID() : NAME_None;
			OnUpgradeClicked(Type);
			if (!Missions || Missions->GetActiveMissionID() == MissionBefore)
			{
				StartUpgradeHold(Type);
			}
		}
		else
		{
			OnBulkUpgrade(Type);
		}
	});
	Btn->OnReleased().AddUObject(this, &UFactoryPanelWidget::StopUpgradeHold);
}

void UFactoryPanelWidget::UpdateUpgradeSlot(EFactoryUpgradeType Type, UUpgradeSlot* InSlot)
{
	if (!InSlot || !CurrentFactory) return;

	const int32 Level = CurrentFactory->GetUpgradeLevel(Type);
	const float CurrentValue = Type == EFactoryUpgradeType::LaborerCount
		? static_cast<float>(CurrentFactory->GetDesiredLaborerCount()) : CurrentFactory->GetUpgradeValue(Type);

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

	const int32 PurchaseMaxLevel = CurrentFactory->GetUpgradeMaxLevel(Type);
	const int32 AllowedCount = CurrentFactory->GetAllowedUpgradeCount(Type);
	const bool bMissingCapacity = PurchaseMaxLevel == INDEX_NONE;
	const bool bAtStageCapacity = Type == EFactoryUpgradeType::LaborerCount
		&& Level >= PurchaseMaxLevel && (MaxLevel <= 0 || Level < MaxLevel);
	const bool bTutorialLocked = AllowedCount == 0 && !bMissingCapacity && !bAtStageCapacity
		&& (MaxLevel <= 0 || Level < MaxLevel)
		&& !(FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Type) && !CurrentFactory->IsAutoCollectionUnlocked());
	const bool bIsLocked = bMissingCapacity || bAtStageCapacity || bTutorialLocked
		|| (FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Type) && !CurrentFactory->IsAutoCollectionUnlocked());
	if (bMissingCapacity)
	{
		InSlot->LockConditionText = NSLOCTEXT("FactoryPanel", "MissingCapacityData", "공장 데이터 확인 필요");
	}
	else if (bAtStageCapacity)
	{
		InSlot->LockConditionText = NSLOCTEXT("FactoryPanel", "ExpandLaborerCapacity", "공장 강화로 정원 확장");
	}
	else if (bTutorialLocked)
	{
		InSlot->LockConditionText = NSLOCTEXT("FactoryPanel", "TutorialUpgradeLimit", "튜토리얼 진행 후 강화 가능");
	}
	else if (!FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Type) || CurrentFactory->IsAutoCollectionUnlocked())
	{
		InSlot->LockConditionText = FText::GetEmpty();
	}

	// 벌크 모드 — 잠금 슬롯은 단건 표시. 재고는 상태 스트립이 담당해 슬롯 자체가 없다
	const bool bBulkMode = (BulkMode != EEnhanceBulkMode::x1) && !bIsLocked;

	int32 BulkCount = 1;
	int64 BulkTotalCost = 0;
	int64 BulkAvailable = 0;
	if (bBulkMode)
	{
		BulkCount = ComputeBulkCount(Type, Level, PurchaseMaxLevel, CostCurve, CostType, BulkTotalCost, BulkAvailable);
	}

	// 델타 = +N레벨 합산 효과 (Count=0 이면 1레벨 기준 유지)
	const int32 NextLevel = Level + (bBulkMode ? FMath::Max(BulkCount, 1) : 1);
	float NextValue = CurrentFactory->GetUpgradeValueAtLevel(Type, NextLevel);
	if (Type == EFactoryUpgradeType::LaborerCount)
	{
		const int32 Capacity = CurrentFactory->GetLaborerCapacity();
		NextValue = FMath::Min(NextValue, static_cast<float>(Capacity));
		InSlot->SetLevelBadgeOverride(FText::Format(
			NSLOCTEXT("FactoryPanel", "LaborerOccupancy", "{0} / {1}명"),
			FText::AsNumber(CurrentFactory->GetDesiredLaborerCount()), FText::AsNumber(Capacity)));
	}

	InSlot->UpdateInfo(Level, CurrentValue, NextValue, Cost, bIsLocked, MaxLevel,
		bBulkMode, BulkCount, bBulkMode ? BulkTotalCost : Cost, BulkAvailable);
}

void UFactoryPanelWidget::UpdateAllUpgradeSlots()
{
	// DynamicSlots 에는 강화 슬롯만 들어온다 (상태 슬롯은 RebuildUpgradeSlots 에서 제외)
	for (const TPair<EFactoryUpgradeType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		UpdateUpgradeSlot(Pair.Key, Pair.Value);
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
	if (CurrentFactory->GetAllowedUpgradeCount(Type) <= 0) return;

	const int32 CurrentLevel = CurrentFactory->GetUpgradeLevel(Type);
	const int32 PurchaseMaxLevel = CurrentFactory->GetUpgradeMaxLevel(Type);
	if (PurchaseMaxLevel == INDEX_NONE
		|| ((PurchaseMaxLevel > 0 || Type == EFactoryUpgradeType::LaborerCount) && CurrentLevel >= PurchaseMaxLevel))
	{
		return;
	}

	// DT 단일 진실 — 비용 곡선/상한/자원타입
	FFactoryCostCurve CostCurve;
	EResourceType CostType = EResourceType::Money;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FFactoryUpgradeDefinition Def;
	if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(Type, Def))
	{
		CostCurve = Def.GetCostCurve();
		CostType = Def.CostResourceType;
	}
	else
	{
		CostCurve = FFactoryUpgradeConfig::GetFallbackCostCurve(Type);
	}

	int64 TotalCost = 0;
	int64 Available = 0;
	const int32 Count = ComputeBulkCount(Type, CurrentLevel, PurchaseMaxLevel, CostCurve, CostType, TotalCost, Available);

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

	SaveFactoryData();
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyFactoryUpgradePurchased(Type, CurrentFactory->GetUpgradeLevel(Type) - CurrentLevel);
	}
	UpdateAllUpgradeSlots();
}

int32 UFactoryPanelWidget::ComputeBulkCount(EFactoryUpgradeType Type, int32 CurrentLevel, int32 MaxLevel, const FFactoryCostCurve& CostCurve,
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
	Count = CurrentFactory ? FMath::Min(Count, CurrentFactory->GetAllowedUpgradeCount(Type)) : 0;

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

void UFactoryPanelWidget::OnUpgradeSuccess(EFactoryUpgradeType UpgradeType)
{
	if (UUpgradeSlot* TargetSlot = DynamicSlots.FindRef(UpgradeType))
	{
		UpdateUpgradeSlot(UpgradeType, TargetSlot);
		TargetSlot->PlayUpgradeEffect();
	}

	SaveFactoryData();
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyFactoryUpgradePurchased(UpgradeType);
	}
	UpdateAllUpgradeSlots();
}

// ===== Hold =====

void UFactoryPanelWidget::StartUpgradeHold(EFactoryUpgradeType UpgradeType)
{
	if (!CurrentFactory || bCloseRequested) return;
	if (CurrentFactory->GetAllowedUpgradeCount(UpgradeType) <= 0) return;
	const int32 MaxLevel = CurrentFactory->GetUpgradeMaxLevel(UpgradeType);
	if (MaxLevel == INDEX_NONE || ((MaxLevel > 0 || UpgradeType == EFactoryUpgradeType::LaborerCount)
		&& CurrentFactory->GetUpgradeLevel(UpgradeType) >= MaxLevel))
	{
		return;
	}
	CurrentHoldUpgradeType = UpgradeType;
	const UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
	HoldMissionID = Missions ? Missions->GetActiveMissionID() : NAME_None;

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
	HoldMissionID = NAME_None;

	if (UUpgradeSlot* HeldSlot = DynamicSlots.FindRef(CurrentHoldUpgradeType))
	{
		HeldSlot->SetPunchSuppressed(false);
	}
}

void UFactoryPanelWidget::OnUpgradeHoldTick()
{
	if (!CurrentFactory) return;
	const UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
	if (Missions && Missions->GetActiveMissionID() != HoldMissionID)
	{
		StopUpgradeHold();
		return;
	}
	if (CurrentFactory->GetAllowedUpgradeCount(CurrentHoldUpgradeType) <= 0)
	{
		StopUpgradeHold();
		return;
	}

	if (CurrentFactory->UpgradeFactory(CurrentHoldUpgradeType))
	{
		OnUpgradeSuccess(CurrentHoldUpgradeType);
		if ((Missions && Missions->GetActiveMissionID() != HoldMissionID)
			|| CurrentFactory->GetAllowedUpgradeCount(CurrentHoldUpgradeType) <= 0)
		{
			StopUpgradeHold();
		}
	}
	else
	{
		StopUpgradeHold();
	}
}

// ===== 기타 =====

void UFactoryPanelWidget::OnResourceChanged(EResourceType Type, int64 NewValue)
{
	if (!CurrentFactory)
	{
		return;
	}

	for (const TPair<EFactoryUpgradeType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		if (UUpgradeSlot* SlotItem = Pair.Value)
		{
			if (UCostActionButtonWidget* Btn = SlotItem->GetUpgradeButton())
			{
				Btn->RefreshAffordability(Type, NewValue);
			}
		}
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
