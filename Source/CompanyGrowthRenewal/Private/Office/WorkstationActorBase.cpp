// Fill out your copyright notice in the Description page of Project Settings.

#include "Office/WorkstationActorBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"
#include "NavAreas/NavArea_Null.h"
#include "Engine/DataTable.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Table/WorkstationTable.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalAssetCache.h"
#include "Core/CGGameInstance.h"
#include "Enum/ResourceType.h"
#include "Enum/PrimitiveShapeType.h"
#include "Materials/MaterialParameterCollectionInstance.h"

AWorkstationActorBase::AWorkstationActorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// 루트 컴포넌트
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	// 책상 메시 (NavMesh 차단용)
	DeskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeskMesh"));
	DeskMesh->SetupAttachment(RootScene);
	DeskMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DeskMesh->SetCollisionObjectType(ECC_WorldStatic);
	DeskMesh->SetCollisionResponseToAllChannels(ECR_Block);
	DeskMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);  // 클릭 감지
	DeskMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore); // 배치 모드 전용 채널 (미리보기만 반응)
	DeskMesh->SetCanEverAffectNavigation(true);

	// BoxComponent 생성 (배치 시 Overlap 충돌 감지용)
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetupAttachment(RootScene);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	BoxComponent->SetGenerateOverlapEvents(true);
	BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	// 클릭 감지 폴백 — 데스크 SM에 simple collision이 없어도 책상 바운드 박스로 클릭을 받음
	BoxComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

	// NavBlocker — 직원 AI 가 책상/의자를 가로질러 걷지 않도록 NavMesh 에 "구멍"(통행 불가)을 내는 동적 장애물.
	// bDynamicObstacle + AreaClassOverride(NavArea_Null) → 영역 모디파이어가 박스 형상만큼 navmesh 를 통행 불가로 만든다.
	// (기본 NavArea_Obstacle 은 비용만 올릴 뿐 구멍이 아니다.) area 모디파이어는 콜리전 응답과 무관하므로
	// OverlapAll 로 두어 물리 차단을 없앤다 → 의자에 텔레포트로 앉은 직원이 밀려나지 않는다.
	NavBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("NavBlocker"));
	NavBlocker->SetupAttachment(RootScene);
	NavBlocker->SetMobility(EComponentMobility::Movable);
	NavBlocker->SetCanEverAffectNavigation(true);
	NavBlocker->bDynamicObstacle = true;
	NavBlocker->SetAreaClassOverride(UNavArea_Null::StaticClass());
	NavBlocker->SetCollisionProfileName(TEXT("OverlapAll"));
	NavBlocker->SetHiddenInGame(true);

	// 하이라이트 SMC 생성
	HighlightSMC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HighlightMesh"));
	HighlightSMC->SetupAttachment(RootScene);
	HighlightSMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightSMC->SetVisibility(false);
	HighlightSMC->SetCastShadow(false);
}

void AWorkstationActorBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeWorkstation();

	// GlobalAssetCache 캐시 및 하이라이트 SMC 머티리얼 설정
	GlobalAssetCache = UCGGameInstance::GetInstance()->GetGlobalAssetCache();
	if (GlobalAssetCache && HighlightSMC)
	{
		HighlightSMC->SetStaticMesh(GlobalAssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Cube));
		HighlightSMC->SetMaterial(0, GlobalAssetCache->GetPlaceableMaterial());
	}
}

FVector AWorkstationActorBase::GetBubbleAnchorPosition() const
{
	// 책상 상단보다 살짝 위(앉은 직원 머리 높이 근처)에서 버블이 뜨도록 보정
	constexpr float BubbleAnchorZOffset = 110.0f;

	if (DeskMesh)
	{
		const FBoxSphereBounds WorldBounds = DeskMesh->Bounds;
		return FVector(
			WorldBounds.Origin.X,
			WorldBounds.Origin.Y,
			WorldBounds.Origin.Z + WorldBounds.BoxExtent.Z + BubbleAnchorZOffset);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, BubbleAnchorZOffset);
}

// ========== 초기화 ==========

void AWorkstationActorBase::InitializeWorkstation()
{
	// 슬롯 상태 초기화
	for (uint8 i = 0; i < static_cast<uint8>(EWorkstationSlot::Max); ++i)
	{
		EWorkstationSlot Slot = static_cast<EWorkstationSlot>(i);
		if (Slot != EWorkstationSlot::Chair && !SlotStates.Contains(Slot))
		{
			FSlotSkinState NewState;
			// 노트북은 항상 활성화
			if (Slot == EWorkstationSlot::Laptop)
			{
				NewState.bIsActive = true;
			}
			SlotStates.Add(Slot, NewState);
		}
	}

	// 고유 ID 생성
	if (InstanceID.IsEmpty())
	{
		InstanceID = FGuid::NewGuid().ToString();
	}

	// BoxComponent 크기 재계산 — 클릭 인터랙션 볼륨(GameTraceChannel1 Block은 생성자에서 설정)
	RecalcBoxExtent();

	UE_LOG(LogTemp, Log, TEXT("[WorkstationActorBase] Initialized: %s (ID: %s)"),
		*WorkstationTypeID.ToString(), *InstanceID);
}

void AWorkstationActorBase::RecalcBoxExtent()
{
	if (!DeskMesh || !BoxComponent) return;

	// DeskMesh를 부모(RootScene) 공간으로 투영한 바운드 — CalcLocalBounds는 컴포넌트 스케일(0.16)을 무시해 박스가 ~6배 커짐. GetActorBounds는 BoxComponent까지 포함해 재귀적으로 커지므로 회피.
	FBoxSphereBounds MeshBounds = DeskMesh->CalcBounds(DeskMesh->GetRelativeTransform());
	FVector BoundExtent = MeshBounds.BoxExtent;

	// BoxComponent 크기 및 위치 설정
	BoxComponent->SetBoxExtent(BoundExtent);
	BoxComponent->SetRelativeLocation(MeshBounds.Origin);

	// Overlap 정보 즉시 업데이트 (스폰 직후 충돌 감지용)
	BoxComponent->UpdateOverlaps();

	// NavBlocker 를 책상+의자+모니터(Highlight 제외) 모든 비주얼 메시의 월드 바운드 합집합으로 맞춘다.
	// MC->Bounds 는 이미 월드 공간(실제 스케일 반영)이라 상대변환/스케일 수동계산 오차가 없다.
	if (NavBlocker)
	{
		FBox WorldBox(ForceInit);
		TArray<UStaticMeshComponent*> Meshes;
		GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* MC : Meshes)
		{
			if (MC && MC != HighlightSMC && MC->GetStaticMesh())
			{
				WorldBox += MC->Bounds.GetBox();
			}
		}

		if (WorldBox.IsValid)
		{
			FVector WorldExtent = WorldBox.GetExtent();
			WorldExtent.Z = FMath::Max(WorldExtent.Z, 90.f);  // 바닥 슬랩을 확실히 관통(과하지 않게)
			const float WS = FMath::Max(NavBlocker->GetComponentScale().X, KINDA_SMALL_NUMBER);
			NavBlocker->SetBoxExtent(WorldExtent / WS);  // 부모 스케일을 상쇄 → 월드 크기 = WorldExtent
			NavBlocker->SetWorldLocation(FVector(WorldBox.GetCenter().X, WorldBox.GetCenter().Y, GetActorLocation().Z));
			RefreshNavObstacle();
		}
	}
}

void AWorkstationActorBase::RefreshNavObstacle()
{
	if (!NavBlocker) return;

	// 런타임에 resize/move 한 동적 장애물은 navmesh 에 자동 재반영되지 않는다 —
	// nav 관련성을 토글(off→on)해 강제 재등록(현재 박스 크기/위치로 다시 스탬프)시킨다.
	NavBlocker->SetCanEverAffectNavigation(false);
	NavBlocker->SetCanEverAffectNavigation(true);

	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		NavSys->AddDirtyArea(NavBlocker->Bounds.GetBox(), ENavigationDirtyFlag::All);
	}
}

void AWorkstationActorBase::RestoreFromSaveData(const FWorkstationSaveData& SaveData)
{
	WorkstationTypeID = SaveData.WorkstationTypeID;
	SetActorTransform(SaveData.Transform);
	CurrentSetupLevel = SaveData.ComputerSetupLevel;
	SlotStates = SaveData.SlotStates;

	// 레벨 적용 (슬롯 활성화)
	ApplySetupLevel();

	// 스킨 복원
	for (const auto& Pair : SlotStates)
	{
		if (Pair.Value.bIsActive && !Pair.Value.CurrentSkinID.IsNone())
		{
			SetSlotSkin(Pair.Key, Pair.Value.CurrentSkinID);
		}
	}

	RecalculateStats();

	UE_LOG(LogTemp, Log, TEXT("[WorkstationActorBase] Restored: %s (Level: %d)"),
		*WorkstationTypeID.ToString(), static_cast<int32>(CurrentSetupLevel));
}

FWorkstationSaveData AWorkstationActorBase::GetSaveData() const
{
	FWorkstationSaveData SaveData;
	SaveData.WorkstationType = GetWorkstationType();
	SaveData.WorkstationTypeID = WorkstationTypeID;
	SaveData.Transform = GetActorTransform();
	SaveData.ComputerSetupLevel = CurrentSetupLevel;
	SaveData.SlotStates = SlotStates;

	return SaveData;
}

// ========== 컴퓨터 세팅 레벨 ==========

bool AWorkstationActorBase::TryUpgradeSetupLevel()
{
	FComputerSetupLevelData NextLevelData;
	if (!GetNextSetupLevelData(NextLevelData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] No valid next setup level from %d"),
			static_cast<int32>(CurrentSetupLevel));
		return false;
	}

	FString ValidationError;
	if (!ValidateAndLoadSetupMeshes(NextLevelData, ValidationError))
	{
		UE_LOG(LogTemp, Error, TEXT("[WorkstationActorBase] Setup Level%d rejected before payment: %s"),
			static_cast<int32>(NextLevelData.Level) + 1, *ValidationError);
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UResourceItemManager* ResourceManager = GameInstance
		? GameInstance->GetSubsystem<UResourceItemManager>()
		: nullptr;
	if (!ResourceManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[WorkstationActorBase] Resource manager is unavailable"));
		return false;
	}

	int64 Withdrawn = 0;
	if (!ResourceManager->ExtractResource(
		EResourceType::Diamond,
		static_cast<int64>(NextLevelData.UpgradeCost),
		Withdrawn,
		false) || Withdrawn != NextLevelData.UpgradeCost)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Not enough Diamond for setup Level%d (cost=%d)"),
			static_cast<int32>(NextLevelData.Level) + 1, NextLevelData.UpgradeCost);
		return false;
	}

	CurrentSetupLevel = NextLevelData.Level;
	ApplySetupLevel();
	RecalculateStats();
	SaveWorkstationData();

	UE_LOG(LogTemp, Log, TEXT("[WorkstationActorBase] Upgraded setup to Level%d for %d Diamond"),
		static_cast<int32>(CurrentSetupLevel) + 1, NextLevelData.UpgradeCost);
	return true;
}

bool AWorkstationActorBase::GetCurrentSetupLevelData(FComputerSetupLevelData& OutData) const
{
	const FComputerSetupLevelData* LevelData = FindSetupLevelData(CurrentSetupLevel);
	if (!LevelData)
	{
		return false;
	}

	OutData = *LevelData;
	return true;
}

bool AWorkstationActorBase::GetNextSetupLevelData(FComputerSetupLevelData& OutData) const
{
	EComputerSetupLevel NextLevel = CurrentSetupLevel;
	if (!TryGetNextComputerSetupLevel(CurrentSetupLevel, NextLevel))
	{
		return false;
	}

	const FComputerSetupLevelData* LevelData = FindSetupLevelData(NextLevel);
	if (!LevelData || LevelData->Level != NextLevel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Missing or mismatched setup row for Level%d"),
			static_cast<int32>(NextLevel) + 1);
		return false;
	}

	OutData = *LevelData;
	return true;
}

bool AWorkstationActorBase::IsMaxLevel() const
{
	return IsMaxComputerSetupLevel(CurrentSetupLevel);
}

bool AWorkstationActorBase::GetMaxVariantData(FComputerSetupLevelData& OutData) const
{
	EComputerSetupLevel Other = CurrentSetupLevel;
	if (!TryGetMaxVariantCounterpart(CurrentSetupLevel, Other))
	{
		return false;
	}

	const FComputerSetupLevelData* LevelData = FindSetupLevelData(Other);
	if (!LevelData || LevelData->Level != Other)
	{
		return false;
	}

	OutData = *LevelData;
	return true;
}

bool AWorkstationActorBase::CanSwapMaxVariant() const
{
	FComputerSetupLevelData Unused;
	return GetMaxVariantData(Unused);
}

bool AWorkstationActorBase::TrySwapMaxVariant()
{
	EComputerSetupLevel Other = CurrentSetupLevel;
	if (!TryGetMaxVariantCounterpart(CurrentSetupLevel, Other))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Setup Level%d has no appearance variant"),
			GetComputerSetupLevelNumber(CurrentSetupLevel));
		return false;
	}

	const FComputerSetupLevelData* OtherData = FindSetupLevelData(Other);
	if (!OtherData || OtherData->Level != Other)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Missing or mismatched variant row for the max level"));
		return false;
	}

	FString ValidationError;
	if (!ValidateAndLoadSetupMeshes(*OtherData, ValidationError))
	{
		UE_LOG(LogTemp, Error, TEXT("[WorkstationActorBase] Variant rejected before payment: %s"), *ValidationError);
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UResourceItemManager* ResourceManager = GameInstance
		? GameInstance->GetSubsystem<UResourceItemManager>()
		: nullptr;
	if (!ResourceManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[WorkstationActorBase] Resource manager is unavailable"));
		return false;
	}

	// 비용은 전환 대상 행이 소유한다 — 두 변형이 같은 값을 들고 있어도 방향별로 다르게 둘 수 있다
	const int32 SwapCost = OtherData->VariantSwapCost;
	int64 Withdrawn = 0;
	if (!ResourceManager->ExtractResource(
		EResourceType::Diamond,
		static_cast<int64>(SwapCost),
		Withdrawn,
		false) || Withdrawn != SwapCost)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Not enough Diamond to swap appearance (cost=%d)"), SwapCost);
		return false;
	}

	CurrentSetupLevel = Other;
	ApplySetupLevel();
	RecalculateStats();
	SaveWorkstationData();

	UE_LOG(LogTemp, Log, TEXT("[WorkstationActorBase] Swapped max-level appearance for %d Diamond"), SwapCost);
	return true;
}

// ========== 스킨 설정 ==========

bool AWorkstationActorBase::SetSlotSkin(EWorkstationSlot Slot, FName SkinID)
{
	// 의자는 별도 함수 사용
	if (Slot == EWorkstationSlot::Chair)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Use SetChairSkin for chairs"));
		return false;
	}

	// 슬롯 활성화 확인
	if (!IsSlotActive(Slot))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Slot %d is not active"), static_cast<int32>(Slot));
		return false;
	}

	// 스킨 데이터 찾기
	FWorkstationItemSkinData* SkinData = FindSkinData(SkinID, Slot);
	if (!SkinData)
	{
		UE_LOG(LogTemp, Error, TEXT("[WorkstationActorBase] Skin not found: %s"), *SkinID.ToString());
		return false;
	}

	// 모니터의 경우 타입 확인
	if (Slot == EWorkstationSlot::Monitor)
	{
		if (SkinData->MonitorType != CurrentMonitor1Type)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Monitor type mismatch. Required: %d, Skin: %d"),
				static_cast<int32>(CurrentMonitor1Type), static_cast<int32>(SkinData->MonitorType));
			return false;
		}
	}

	// 메시 적용
	if (UStaticMesh* Mesh = SkinData->Mesh.LoadSynchronous())
	{
		SetSlotMesh(Slot, Mesh);
	}

	// 상태 업데이트
	if (FSlotSkinState* State = SlotStates.Find(Slot))
	{
		State->CurrentSkinID = SkinID;
	}

	// 즉시 저장
	SaveWorkstationData();

	UE_LOG(LogTemp, Log, TEXT("[WorkstationActorBase] Slot %d skin set to: %s and saved"),
		static_cast<int32>(Slot), *SkinID.ToString());

	return true;
}

FName AWorkstationActorBase::GetSlotSkinID(EWorkstationSlot Slot) const
{
	if (const FSlotSkinState* State = SlotStates.Find(Slot))
	{
		return State->CurrentSkinID;
	}
	return NAME_None;
}

bool AWorkstationActorBase::IsSlotActive(EWorkstationSlot Slot) const
{
	if (Slot == EWorkstationSlot::Chair)
	{
		return GetChairCount() > 0;
	}

	if (const FSlotSkinState* State = SlotStates.Find(Slot))
	{
		return State->bIsActive;
	}
	return false;
}

void AWorkstationActorBase::SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh)
{
	if (UStaticMeshComponent* MeshComp = GetMeshComponentForSlot(Slot))
	{
		MeshComp->SetStaticMesh(NewMesh);
	}
}

void AWorkstationActorBase::SetSlotVisibility(EWorkstationSlot Slot, bool bVisible)
{
	if (UStaticMeshComponent* MeshComp = GetMeshComponentForSlot(Slot))
	{
		MeshComp->SetVisibility(bVisible);
	}
}

UStaticMeshComponent* AWorkstationActorBase::GetMeshComponentForSlot(EWorkstationSlot Slot) const
{
	// 베이스 클래스에서는 nullptr 반환 - 자식에서 오버라이드
	return nullptr;
}

// ========== 스탯 계산 ==========

void AWorkstationActorBase::RecalculateStats()
{
	WorkSpeedBonusRate = 0.0f;

	if (const FComputerSetupLevelData* LevelData = FindSetupLevelData(CurrentSetupLevel))
	{
		WorkSpeedBonusRate = LevelData->WorkSpeedBonusRate;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[WorkstationActorBase] Work speed +%.1f%%"),
		WorkSpeedBonusRate * 100.0f);
}

FWorkstationItemSkinData* AWorkstationActorBase::FindSkinData(FName SkinID, EWorkstationSlot Slot) const
{
	if (!WorkstationItemSkinTable || SkinID.IsNone())
	{
		return nullptr;
	}

	FWorkstationItemSkinData* SkinData = WorkstationItemSkinTable->FindRow<FWorkstationItemSkinData>(SkinID, TEXT(""));
	if (SkinData && SkinData->TargetSlot == Slot)
	{
		return SkinData;
	}

	return nullptr;
}

const FComputerSetupLevelData* AWorkstationActorBase::FindSetupLevelData(EComputerSetupLevel Level) const
{
	UGameInstance* GameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableManager = GameInstance
		? GameInstance->GetSubsystem<UTableManagerSubsystem>()
		: nullptr;
	if (!TableManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationActorBase] Table manager is unavailable"));
		return nullptr;
	}

	return TableManager->FindWorkstationSetupLevelData(Level);
}

bool AWorkstationActorBase::ValidateAndLoadSetupMeshes(
	const FComputerSetupLevelData& LevelData,
	FString& OutError) const
{
	if (!LevelData.IsValidConfiguration(&OutError))
	{
		return false;
	}

	auto LoadRequired = [&OutError](
		const TSoftObjectPtr<UStaticMesh>& MeshReference,
		bool bRequired,
		const TCHAR* EquipmentName)
	{
		if (!bRequired)
		{
			return true;
		}
		if (MeshReference.LoadSynchronous())
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Failed to load %s mesh: %s"),
			EquipmentName, *MeshReference.ToSoftObjectPath().ToString());
		return false;
	};

	return LoadRequired(LevelData.LaptopMesh, LevelData.LaptopPlacement != ELaptopPlacement::None, TEXT("laptop"))
		&& LoadRequired(LevelData.PrimaryMonitorMesh, LevelData.PrimaryMonitorType != EPrimaryMonitorType::None, TEXT("primary monitor"))
		&& LoadRequired(LevelData.VerticalMonitorMesh,
			LevelData.SecondMonitorType == ESecondMonitorType::Vertical, TEXT("vertical monitor"))
		&& LoadRequired(LevelData.DualMonitorMesh,
			LevelData.SecondMonitorType == ESecondMonitorType::Curved, TEXT("curved twin monitor"))
		&& LoadRequired(LevelData.KeyboardMesh, LevelData.bKeyboardActive, TEXT("keyboard"))
		&& LoadRequired(LevelData.MouseMesh, LevelData.bMouseActive, TEXT("mouse"))
		&& LoadRequired(LevelData.ComputerMesh, LevelData.bComputerActive, TEXT("computer"));
}

bool AWorkstationActorBase::AttachToSocket(USceneComponent* Component, FName SocketName)
{
	if (!Component || !DeskMesh)
	{
		return false;
	}

	if (!DeskMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[WorkstationActorBase] Socket %s not found, keeping default position"), *SocketName.ToString());
		return false;
	}

	// 소켓에 부착
	Component->AttachToComponent(DeskMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	UE_LOG(LogTemp, Verbose, TEXT("[WorkstationActorBase] Attached %s to socket %s"), *Component->GetName(), *SocketName.ToString());

	return true;
}

void AWorkstationActorBase::SaveWorkstationData()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;

	if (USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveLoadMgr->SaveGameData();
	}
}

void AWorkstationActorBase::SetHighlight(bool bEnabled)
{
	if (!HighlightSMC)
	{
		return;
	}

	if (bEnabled)
	{
		// 바운드 계산
		FVector BoundOrigin, BoundExtent;
		GetActorBounds(false, BoundOrigin, BoundExtent);

		// 위치: 바운드 중심 (살짝 위로)
		HighlightSMC->SetWorldLocation(FVector(BoundOrigin.X, BoundOrigin.Y, BoundExtent.Z + 1.0f));

		// 크기: 바운드에 맞게 (1.05배로 살짝 크게)
		HighlightSMC->SetWorldScale3D((BoundExtent / 50.0f) * 1.05f);

		// MPC 파라미터 설정 (A=1.0이면 초록색)
		if (GlobalAssetCache)
		{
			UMaterialParameterCollection* CropoutMPC = GlobalAssetCache->GetCropoutMPC();
			if (CropoutMPC)
			{
				UMaterialParameterCollectionInstance* MPCInst = GetWorld()->GetParameterCollectionInstance(CropoutMPC);
				if (MPCInst)
				{
					FLinearColor TargetColor = FLinearColor(BoundOrigin);
					TargetColor.A = 1.0f;  // 초록색
					MPCInst->SetVectorParameterValue("Target Position", TargetColor);
				}
			}
		}

		HighlightSMC->SetVisibility(true);
	}
	else
	{
		HighlightSMC->SetVisibility(false);
	}
}
