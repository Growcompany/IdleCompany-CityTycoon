// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Factory/BrickFactory.h"

#include "Entity/Factory/BrickLaborer.h"
#include "Entity/Factory/FactoryUpgradeEffectComponent.h"
#include "Entity/Ambient/VacantPlotDressingManager.h"
#include "Components/BoxComponent.h"
#include "Data/BrickLaborerRules.h"
#include "Player/Components/PlotPlacementRules.h"
#include "Table/BuildingData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "Data/FactoryUpgradeConfig.h"
#include "Data/FactoryUpgradeData.h"
#include "Data/FactoryOfflineGainPolicy.h"
#include "Data/TutorialDerivedMissionRules.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/QuestSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Player/PlayerCamera.h"
#include "Player/MainMapPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include <Manager/UIManagerSubsystem.h>
#include <Manager/ResourceItemManager.h>
#include <Kismet/GameplayStatics.h>
#include <Blueprint/WidgetLayoutLibrary.h>
#include "Enum/ResourceType.h"
#include "Util/CoordinateUtils.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Sound/SoundWave.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TimeCycle/TimeCycleManager.h"

// Sets default values
ABrickFactory::ABrickFactory()
{
	// 부모 AInteractableBaseActor의 Timeline(PlayWobble) 진행을 위해 Tick 필요
	PrimaryActorTick.bCanEverTick = true;

	Tags.Add(FName("Interactable"));
	Tags.Add(FName("Factory"));

	// Factory 데이터 초기화
	FactoryData = FFactorySaveData(FactoryID);
	UpgradeEffect = CreateDefaultSubobject<UFactoryUpgradeEffectComponent>(TEXT("UpgradeEffect"));

	// 야간 조명 ISM — 루트에 붙여 공장 재배치 시 조명이 따라오게 한다
	LightISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LightISM"));
	LightISM->SetupAttachment(RootComponent);
	LightISM->SetMobility(EComponentMobility::Movable);
	LightISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LightISM->SetCanEverAffectNavigation(false);
	LightISM->SetCastShadow(false);
	LightISM->bUseAsOccluder = false;
	// 컬 거리에 걸려 인스턴스가 통째로 사라졌다 나타나는 점멸 방지 (BuildingBaseActor 비콘 ISM 과 동일)
	LightISM->bNeverDistanceCull = true;
	LightISM->bAllowCullDistanceVolume = false;

	// 엔진 기본 구체(엔진 콘텐츠 = 항상 쿠킹). 머티리얼은 런타임 로드(BuildNightLights).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LightMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (LightMeshFinder.Succeeded())
	{
		LightISM->SetStaticMesh(LightMeshFinder.Object);
	}

	// 잡부 기본값은 BP 가 아니라 여기서 준다.
	// BP CDO 에 꽂아 두면 네이티브 부모가 바뀔 때마다 재컴파일로 조용히 리셋된다(2026-10-02 실측).
	// 경로 문자열만 들고 있는 소프트 참조라 하드 의존이 생기지 않는다 — 쿠킹은 FunnyWorker/ 폴더 등록으로 커버.
	LaborerClass = TSoftClassPtr<ABrickLaborer>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/BP_BrickLaborer.BP_BrickLaborer_C")));
	WalkAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/AS_Walk.AS_Walk")));
	RunAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/Moving/AS_Run_Medium.AS_Run_Medium")));
	CarryWalkAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/Anims/AS_CarryWalk.AS_CarryWalk")));
	CarryRunAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/Anims/AS_CarryRun.AS_CarryRun")));
}

// Called when the game starts or when spawned
void ABrickFactory::BeginPlay()
{
	UpdateAppearanceTransform();
	Super::BeginPlay();

	UpdateProductionValues();
	UpgradeEffect->OnConcealed.AddUObject(this, &ABrickFactory::ApplyPendingFactoryAppearance);
	UpgradeEffect->OnRevealed.AddUObject(this, &ABrickFactory::HandleUpgradeRevealed);
	UpgradeEffect->OnFinished.AddUObject(this, &ABrickFactory::HandleUpgradeEffectFinished);
	RefreshFactoryAppearance(false);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->OnHQLevelUp.AddDynamic(this, &ABrickFactory::HandleHQLevelUp);
			// 접속 중 재고는 안 쌓는다 — 오프라인 경과분만 여기서 한 번 정산한다
			SaveMgr->OnOfflineGainsApplied.AddUObject(this, &ABrickFactory::SettleOfflineBricks);
		}
	}

	// HQ 레벨업 시점을 놓친 세이브 보정 — 연출 없이 조용히
	EvaluateAutoCollectionUnlock(false);

	// 낮엔 additive 조명을 숨겨 fill 절감 — day/night 전환 구독. 못 찾으면 항상 표시.
	if (ATimeCycleManager* Clock = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass())))
	{
		CachedTimeCycle = Clock;
		Clock->OnVisualTimeChanged.AddDynamic(this, &ABrickFactory::HandleVisualTimeChanged);
		const int32 Cur = Clock->GetVisualTime().ToSeconds();
		const int32 Rise = Clock->GetSunRiseTime().ToSeconds();
		const int32 Set = Clock->GetSunSetTime().ToSeconds();
		bLightVisible = (Cur >= Set) || (Cur < Rise);
	}

	SetupBodyNightEmissive();
	BuildNightLights();

	RefreshLaborerCrew();
}

void ABrickFactory::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UpgradeEffect)
	{
		UpgradeEffect->Cancel();
		UpgradeEffect->OnConcealed.RemoveAll(this);
		UpgradeEffect->OnRevealed.RemoveAll(this);
		UpgradeEffect->OnFinished.RemoveAll(this);
	}
	GetWorldTimerManager().ClearTimer(HoldSmokeTimerHandle);
	GetWorldTimerManager().ClearTimer(BoostStreakHideTimerHandle);
	ClearSmokeVFX();
	// Steam/Ambience 사운드 컴포넌트가 World에 분리 spawn 되어 있어 Actor 종료 시 명시적 정리 필요
	StopSteamSound();
	StopAmbienceSound();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->OnHQLevelUp.RemoveDynamic(this, &ABrickFactory::HandleHQLevelUp);
			SaveMgr->OnOfflineGainsApplied.RemoveAll(this);
		}
	}

	if (ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		Clock->OnVisualTimeChanged.RemoveDynamic(this, &ABrickFactory::HandleVisualTimeChanged);
	}

	// 잡부는 Owner 만 걸고 따로 spawn 된 액터다 — UE 는 Owner 로 수명을 잇지 않아
	// 공장이 사라지면 월드에 멈춰 선 채로 남는다(Tick 이 OwnerFactory 무효로 조기 반환).
	for (ABrickLaborer* Laborer : Laborers)
	{
		if (IsValid(Laborer))
		{
			Laborer->Destroy();
		}
	}
	Laborers.Reset();

	Super::EndPlay(EndPlayReason);
}

void ABrickFactory::SetupBodyNightEmissive()
{
	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (MeshComp && MeshComp != LightISM && MeshComp->GetStaticMesh())
		{
			BodyMID = MeshComp->CreateAndSetMaterialInstanceDynamic(0);
			break;
		}
	}

	if (!BodyMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryLight] 본체 MID 생성 실패 — 야간 발광 없음"));
	}
}

void ABrickFactory::ApplyNightVisibility(bool bNight)
{
	bLightVisible = bNight;
	if (LightISM)
	{
		LightISM->SetVisibility(bNight, true);
	}
	if (BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("EmissiveFactor"), bNight ? NightEmissiveTint : FLinearColor::Black);
		UE_LOG(LogTemp, Log, TEXT("[FactoryLight] 본체 EmissiveFactor -> %s"),
			*(bNight ? NightEmissiveTint : FLinearColor::Black).ToString());
	}

	// 잡부는 Unlit 머티리얼이라 밤에도 안 어두워진다 — 공장이 받은 밤 비트를 그대로 내려 준다.
	// 30명이 각자 시계를 구독하지 않게 여기 한 곳에서만 전달한다.
	for (ABrickLaborer* Laborer : Laborers)
	{
		if (IsValid(Laborer))
		{
			Laborer->SetNightMode(bNight);
		}
	}
}

void ABrickFactory::HandleVisualTimeChanged()
{
	if (const ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		ApplyNightVisibility(Clock->IsVisualNight());
	}
}

namespace
{
	// 소켓 번호순으로 이어 런을 만들기 위한 수집 항목
	struct FLightSocketEntry
	{
		int32 Order = 0;
		FVector LocalLoc = FVector::ZeroVector;
		FTransform WorldXform;
	};
}

void ABrickFactory::BuildNightLights()
{
	if (!LightISM || !LightISM->GetStaticMesh())
	{
		return;
	}

	// 조명 머티리얼은 런타임 로드 — 에셋이 C++ 이후 생성돼도 재시작 없이 반영(CLAUDE.md 런타임 로드 패턴)
	if (UMaterialInterface* LightMat = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Environment/Beacon/MI_FactoryLight.MI_FactoryLight"))).LoadSynchronous())
	{
		LightISM->SetMaterial(0, LightMat);
	}

	LightISM->ClearInstances();

	// BP 가 메시를 어느 컴포넌트에 물렸는지에 의존하지 않도록 전체 스캔
	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);

	TArray<FLightSocketEntry> Entries;
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp || MeshComp == LightISM)
		{
			continue;
		}
		UStaticMesh* SM = MeshComp->GetStaticMesh();
		if (!SM)
		{
			continue;
		}

		for (UStaticMeshSocket* Socket : SM->Sockets)
		{
			if (!Socket)
			{
				continue;
			}
			const FString SockName = Socket->SocketName.ToString();
			if (!SockName.StartsWith(TEXT("Light")))
			{
				continue;
			}

			FLightSocketEntry Entry;
			Entry.Order = FCString::Atoi(*SockName.RightChop(5)); // "Light" 뒤 숫자 = 건물 윤곽을 따라가는 순서
			Entry.LocalLoc = Socket->RelativeLocation;
			Entry.WorldXform = MeshComp->GetSocketTransform(Socket->SocketName, RTS_World);
			Entries.Add(Entry);
		}
	}

	if (Entries.Num() == 0)
	{
		return;
	}

	Entries.Sort([](const FLightSocketEntry& A, const FLightSocketEntry& B) { return A.Order < B.Order; });

	auto AddLight = [this](const FTransform& Base, const FVector& WorldLoc, bool bGround)
	{
		FVector P = WorldLoc;
		if (bGround)
		{
			P.Z += GroundLightZLift;
		}
		// bWorldSpace=true 로 넣어도 ISM 내부에서 컴포넌트 상대좌표로 저장 → 공장 이전 시 자동 추종
		LightISM->AddInstance(
			FTransform(Base.GetRotation(), P, Base.GetScale3D() * (bGround ? GroundLightScale : LightScale)),
			/*bWorldSpace=*/true);
	};

	int32 Filled = 0;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FLightSocketEntry& Cur = Entries[i];
		AddLight(Cur.WorldXform, Cur.WorldXform.GetLocation(), Cur.LocalLoc.Z < GroundSocketZThreshold);

		if (LightSpacing <= 0.f || i + 1 >= Entries.Num())
		{
			continue;
		}

		// 층이 바뀌면 런이 끊긴 것 — 건물을 가로지르는 조명 선이 생기지 않게 한다
		const FLightSocketEntry& Next = Entries[i + 1];
		if (FMath::Abs(Next.LocalLoc.Z - Cur.LocalLoc.Z) > LayerZTolerance)
		{
			continue;
		}

		const FVector Start = Cur.WorldXform.GetLocation();
		const FVector End = Next.WorldXform.GetLocation();
		const int32 Steps = FMath::Max(1, FMath::RoundToInt((End - Start).Size() / LightSpacing));
		const bool bGroundRun = Cur.LocalLoc.Z < GroundSocketZThreshold;
		for (int32 k = 1; k < Steps; ++k)
		{
			AddLight(Cur.WorldXform, FMath::Lerp(Start, End, static_cast<float>(k) / Steps), bGroundRun);
			++Filled;
		}
	}

	ApplyNightVisibility(bLightVisible);

	UE_LOG(LogTemp, Log, TEXT("[FactoryLight] %s sockets=%d filled=%d total=%d night=%d"),
		*GetName(), Entries.Num(), Filled, LightISM->GetInstanceCount(), bLightVisible ? 1 : 0);
}

void ABrickFactory::OnInteract_Implementation(APlayerController* PC)
{
	if (bIsRelocating) return;
	// Factory 모드로 전환 — 인터페이스 호출자가 메인맵 PC 라는 보장이 없다(오피스/방문 맵)
	AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(PC);
	if (MainPC)
	{
		MainPC->GoToFactoryMode();
	}

	bIsInteracting = true;

	UE_LOG(LogTemp, Log, TEXT("Pressed on ABrickFactory"));

	// 짧은 굴뚝 분출과 건물 외곽 속도선을 홀드에 맞춘다.
	StartSmokeVFX();
	StartBoostStreaks();

	// 홀드는 더 이상 벽돌을 직접 뱉지 않는다 — 잡부 왕복을 2배속으로 돌린다 [확정 2026-10-02]
	ApplyLaborerPace(true);
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyFactoryBoostStarted();
	}

	// 홀드 중 배수 배지는 최초 누름점에 머문다.
	if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->BeginFactoryBoost(HoldSpeedMultiplier);
		}
	}
}

void ABrickFactory::OnEndInteract_Implementation(APlayerController* PC)
{
	if (!bIsInteracting)
	{
		return;
	}

	bIsInteracting = false;

	UE_LOG(LogTemp, Log, TEXT("[ABrickFactory]- OnEndInteract_Implementation - Released"));

	// 연기 VFX 중지
	StopSmokeVFX();
	StopBoostStreaks();

	if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->EndFactoryBoost();
		}
	}

	// 가속 해제 — 홀드를 떼면 즉시 평소 속도로
	ApplyLaborerPace(false);

	// 상태 체크 후 Normal로 복귀
	AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(PC);
	if (MainPC && MainPC->GetCurrentInputMode() == EInputMode::Factory)
	{
		MainPC->GoToNormalMode();
	}
}


// Called every frame
void ABrickFactory::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABrickFactory::HarvestBricksAt(const FVector& WorldLocation, int32 Amount)
{
	if (Amount <= 0) return;

	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UInGameLayerWidget* InGameUI = UIManager ? UIManager->GetInGameLayer() : nullptr;
	if (!InGameUI)
	{
		if (UResourceItemManager* ResourceManager = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr)
		{
			ResourceManager->CollectFactoryBricks(Amount);
		}
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	FVector2D ScreenPos = FVector2D::ZeroVector;
	// 풀스크린 EffectCanvas와 같은 뷰포트 기준 좌표를 사용한다.
	const bool bSourceProjected = PC && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC, WorldLocation, ScreenPos, false);

	InGameUI->SpawnBrickCollectAt(ScreenPos, EResourceType::Brick, Amount, bSourceProjected);

	// 바운스는 "공장이 벽돌을 뱉었다"는 신호였다 — 이제 생산 주체가 잡부라 공장이 들썩일 이유가 없다.
	// 30명이 쉴 새 없이 배달하면 공장이 상시 진동하게 된다.
}


void ABrickFactory::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	//UE_LOG(LogTemp, Log, TEXT("BrickFactory_Clicked"));
}

void ABrickFactory::OnConstruction(const FTransform& Transform)
{
	UpdateAppearanceTransform();
	Super::OnConstruction(Transform);

	SpawnPoints.Empty();
	LaborerDoors.Empty();

	// 루트 밑에 붙어 있는 모든 SceneComponent 수집
	TArray<USceneComponent*> All;
	RootComponent->GetChildrenComponents(true, All);

	for (USceneComponent* Comp : All)
	{
		if (!Comp) continue;

		// 이름이 "BrickSpawnPoint" 로 시작하는 컴포넌트만 골라 담기
		if (Comp->GetName().StartsWith(TEXT("BrickSpawnPoint")))
		{
			SpawnPoints.Add(Comp);
		}
		else if (Comp->GetName().StartsWith(TEXT("LaborerDoor")))
		{
			LaborerDoors.Add(Comp);
		}
	}

	// 컴포넌트 수집 순서는 보장되지 않는다 — 입구/출구가 매번 뒤바뀌지 않도록 이름순으로 고정
	LaborerDoors.Sort([](const USceneComponent& A, const USceneComponent& B)
	{
		return A.GetName() < B.GetName();
	});
}

FVector ABrickFactory::GetLaborerEntryPoint() const
{
	return GetLaborerDoorTransform(0).GetLocation();
}

FVector ABrickFactory::GetLaborerExitPoint() const
{
	return GetLaborerDoorTransform(1).GetLocation();
}

FTransform ABrickFactory::GetLaborerDoorTransform(int32 DoorIndex) const
{
	const int32 DoorCount = GetLaborerDoorCount();
	if (DoorCount <= 0)
	{
		return GetActorTransform();
	}
	const int32 Index = FMath::Clamp(DoorIndex, 0, DoorCount - 1);
	if (MainMeshComponent && MainMeshComponent->DoesSocketExist(TEXT("LaborerDoor_0")))
	{
		return MainMeshComponent->GetSocketTransform(
			FName(*FString::Printf(TEXT("LaborerDoor_%d"), Index)));
	}
	if (LaborerDoors.IsValidIndex(Index) && LaborerDoors[Index])
	{
		return LaborerDoors[Index]->GetComponentTransform();
	}
	return GetActorTransform();
}

int32 ABrickFactory::GetLaborerDoorCount() const
{
	if (MainMeshComponent && MainMeshComponent->DoesSocketExist(TEXT("LaborerDoor_0")))
	{
		int32 Count = 1;
		while (MainMeshComponent->DoesSocketExist(FName(*FString::Printf(TEXT("LaborerDoor_%d"), Count))))
		{
			++Count;
		}
		return Count;
	}
	return LaborerDoors.Num();
}

FTransform ABrickFactory::GetPlacementFootprintTransform(bool bCommitted) const
{
	const FTransform Placement = bCommitted && bIsRelocating ? RelocationOriginalTransform : GetActorTransform();
	const FQuat PlacementRotation = FRotator(0.f, Placement.Rotator().Yaw, 0.f).Quaternion();
	const FVector Center = Placement.GetLocation() + PlacementRotation.RotateVector(
		FVector(PlacementFootprintOffset.X, PlacementFootprintOffset.Y, 0.f));
	return FTransform(PlacementRotation, Center);
}

FVector ABrickFactory::GetPlacementFootprintCenter(bool bCommitted) const
{
	return GetPlacementFootprintTransform(bCommitted).GetLocation();
}

FVector2D ABrickFactory::GetPlacementFootprintHalfExtent(bool bWorldAxes, bool bCommitted) const
{
	const double HalfSide = FPlotPlacementRules::GetPlotCellSize(ResolvePlotBaseCellSize(GetWorld())) * FactoryScale;
	if (!bWorldAxes) return FVector2D(HalfSide);
	const double YawRadians = FMath::DegreesToRadians(static_cast<double>(GetPlacementFootprintTransform(bCommitted).Rotator().Yaw));
	const double AABBHalfSide = HalfSide * (FMath::Abs(FMath::Cos(YawRadians)) + FMath::Abs(FMath::Sin(YawRadians)));
	return FVector2D(AABBHalfSide);
}

bool ABrickFactory::BeginRelocation()
{
	if (bIsRelocating) return false;
	OnEndInteract_Implementation(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	if (UpgradeEffect && UpgradeEffect->IsPlaying())
	{
		UpgradeEffect->Cancel();
		RefreshFactoryAppearance(false);
	}
	RelocationOriginalTransform = GetActorTransform();
	bIsRelocating = true;
	ClearSmokeVFX();
	for (ABrickLaborer* Laborer : Laborers)
	{
		if (!IsValid(Laborer)) continue;
		Laborer->SetActorHiddenInGame(true);
		Laborer->SetActorTickEnabled(false);
	}
	return true;
}

void ABrickFactory::RotatePlacementQuarterTurn()
{
	const FVector ReservedCenter = GetPlacementFootprintCenter();
	AddActorWorldRotation(FRotator(0.f, 90.f, 0.f));
	AddActorWorldOffset(ReservedCenter - GetPlacementFootprintCenter());
}

void ABrickFactory::FinishRelocation(bool bCommit, FName DestinationPlotId)
{
	if (!bIsRelocating) return;
	if (bCommit)
	{
		FactoryData.OwningPlotId = DestinationPlotId;
		FactoryData.PlacementTransform = GetActorTransform();
	}
	else
	{
		SetActorTransform(RelocationOriginalTransform);
	}
	bIsRelocating = false;
	ReCalcBoxExtent();
	for (ABrickLaborer* Laborer : Laborers)
	{
		if (!IsValid(Laborer)) continue;
		Laborer->RefreshPath();
		Laborer->SetActorHiddenInGame(false);
		Laborer->SetActorTickEnabled(true);
	}
	if (bCommit)
	{
		if (UGameInstance* FactoryGameInstance = GetGameInstance())
		{
			if (USaveLoadManager* SaveManager = FactoryGameInstance->GetSubsystem<USaveLoadManager>())
			{
				SaveManager->RequestDeferredSave();
			}
		}
	}
}

float ABrickFactory::GetCameraFocusDistanceCm() const
{
	const UGameInstance* FactoryGameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = FactoryGameInstance
		? FactoryGameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const int32 DisplayLevel = CurrentAppearanceLevel > 0
		? CurrentAppearanceLevel : GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion);
	const FFactoryAppearanceRow* Appearance = TableMgr ? TableMgr->FindFactoryAppearance(DisplayLevel) : nullptr;
	if (!Appearance || !FMath::IsFinite(Appearance->CameraFocusDistanceCm) || Appearance->CameraFocusDistanceCm <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] Missing camera focus distance at factory level %d"), DisplayLevel);
		return 0.f;
	}
	return Appearance->CameraFocusDistanceCm;
}

int32 ABrickFactory::GetDesiredLaborerCount() const
{
	return FBrickLaborerRules::LaborerCountFromValue(
		GetUpgradeValue(EFactoryUpgradeType::LaborerCount), GetLaborerCapacity());
}

int32 ABrickFactory::GetLaborerCapacity() const
{
	return GetLaborerCapacityAtLevel(GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion));
}

int32 ABrickFactory::GetLaborerCapacityAtLevel(int32 ExpansionLevel) const
{
	const UGameInstance* FactoryGameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = FactoryGameInstance
		? FactoryGameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const FFactoryAppearanceRow* Appearance = TableMgr ? TableMgr->FindFactoryAppearance(ExpansionLevel) : nullptr;
	if (!Appearance || Appearance->LaborerCapacity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] Missing laborer capacity at factory level %d"), ExpansionLevel);
		return 0;
	}
	return Appearance->LaborerCapacity;
}

namespace BrickLaborerDefaults
{
	// BP/레벨에 None 이 직렬화돼 있으면 C++ 생성자 기본값을 덮어써 버린다(2026-10-02 실측).
	// 그 경우 조용히 멈추지 말고 이 경로로 자가 복구하고, 데이터가 어긋났다는 사실은 경고로 남긴다.
	static const TCHAR* LaborerBP = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/BP_BrickLaborer.BP_BrickLaborer_C");
	static const TCHAR* Walk = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/AS_Walk.AS_Walk");
	static const TCHAR* Run = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/Moving/AS_Run_Medium.AS_Run_Medium");
	static const TCHAR* CarryWalk = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/Anims/AS_CarryWalk.AS_CarryWalk");
	static const TCHAR* CarryRun = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/Anims/AS_CarryRun.AS_CarryRun");

	template <typename T>
	T* ResolveOrFallback(const TSoftObjectPtr<T>& Soft, const TCHAR* FallbackPath, const TCHAR* Label)
	{
		if (!Soft.IsNull())
		{
			if (T* Loaded = Soft.LoadSynchronous())
			{
				return Loaded;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] %s 가 비어 있어 기본 경로로 복구한다 — BP/레벨 값 확인 필요"), Label);
		return Cast<T>(FSoftObjectPath(FallbackPath).TryLoad());
	}

	UAnimSequence* ResolveCarryOrFallback(const TSoftObjectPtr<UAnimSequence>& Soft,
		const TCHAR* FallbackPath, const TCHAR* Label)
	{
		if (!Soft.IsNull())
		{
			if (UAnimSequence* Loaded = Soft.LoadSynchronous())
			{
				return Loaded;
			}
		}
		if (UAnimSequence* Fallback = Cast<UAnimSequence>(FSoftObjectPath(FallbackPath).TryLoad()))
		{
			return Fallback;
		}
		static bool bWarnedMissingCarryClip = false;
		if (!bWarnedMissingCarryClip)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] %s carry clip is unavailable; laborers will use normal gait"), Label);
			bWarnedMissingCarryClip = true;
		}
		return nullptr;
	}
}

void ABrickFactory::SpawnLaborer(int32 LaneIndex, int32 LaneCount)
{
	UClass* LoadedClass = LaborerClass.IsNull() ? nullptr : LaborerClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BrickFactory] LaborerClass 가 비어 있어 기본 경로로 복구한다 — BP/레벨 값 확인 필요"));
		LoadedClass = Cast<UClass>(FSoftObjectPath(BrickLaborerDefaults::LaborerBP).TryLoad());
	}
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BrickFactory] 잡부 BP 를 기본 경로에서도 못 찾았다: %s"),
			BrickLaborerDefaults::LaborerBP);
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABrickLaborer* Laborer = GetWorld()->SpawnActor<ABrickLaborer>(
		LoadedClass, GetLaborerEntryPoint(), FRotator::ZeroRotator, Params);
	if (!Laborer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] 잡부 스폰 실패 (class=%s)"), *LoadedClass->GetName());
		return;
	}

	Laborer->InitLaborer(this, LaneIndex, LaneCount);
	// 밤에 스폰된 신입만 혼자 환하게 서 있지 않도록 현재 밤 상태를 바로 물려준다
	Laborer->SetNightMode(bLightVisible);
	Laborer->SetActorHiddenInGame(bIsRelocating);
	Laborer->SetActorTickEnabled(!bIsRelocating);
	Laborers.Add(Laborer);
}

void ABrickFactory::RefreshLaborerCrew()
{
	if (!GetWorld())
	{
		return;
	}


	Laborers.RemoveAll([](const ABrickLaborer* L) { return !IsValid(L); });

	const int32 Desired = GetDesiredLaborerCount();

	for (int32 i = Laborers.Num(); i < Desired; ++i)
	{
		SpawnLaborer(i, Desired);
	}

	// 줄어든 경우: 초과분은 지금 들고 있는 벽돌을 배달한 뒤 스스로 빠진다
	for (int32 i = Desired; i < Laborers.Num(); ++i)
	{
		if (IsValid(Laborers[i]))
		{
			Laborers[i]->RequestRetireAfterCycle();
		}
	}

	// 왕복 시간은 **거리 ÷ 걷는 속도**로 정한다. 곡선에서 바로 뽑으면 62m 를 1.2초에 가게 된다.
	const float LegTime = GetLaborerLegSeconds();

	for (int32 i = 0; i < Laborers.Num(); ++i)
	{
		if (IsValid(Laborers[i]))
		{
			Laborers[i]->SetCycleDuration(LegTime);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BrickFactory] 잡부 %d/%d명 · 문 %d개 · 경로 %.0fcm · 한 구간 %.1f초(평속 %.0fcm/s)"),
		Laborers.Num(), Desired, GetLaborerDoorCount(),
		GetLaborerPathLength(), LegTime, LaborerWalkSpeed);

	// 배속·운반량은 평소/홀드에 따라 다르므로 한 곳에서 같이 계산한다
	ApplyLaborerPace(bIsInteracting);
}

float ABrickFactory::GetLaborerPathLength() const
{
	const float Straight = FVector::Dist2D(GetLaborerEntryPoint(), GetLaborerExitPoint());
	return FMath::Max(100.f, Straight * PathLengthFudge);
}

float ABrickFactory::GetLaborerLegSeconds() const
{
	return FBrickLaborerRules::LegSeconds(GetLaborerPathLength(), LaborerWalkSpeed);
}

void ABrickFactory::ApplyLaborerPace(bool bHolding)
{
	// 이동 속도와 생산율을 따로 계산한다.
	// 속도는 사람처럼 보이는 범위(걷기/뛰기)로 묶고, 곡선이 요구하는 나머지 배수는 1회 운반량이 가져간다.
	const float SpeedMul = bHolding ? FMath::Max(1.f, HoldSpeedMultiplier) : 1.f;
	const float ProdScale = bHolding ? HoldProductionScale : IdleProductionScale;

	float CurveCycle = GetUpgradeValue(EFactoryUpgradeType::HoldProductionSpeed);
	if (CurveCycle <= 0.f)
	{
		CurveCycle = BrickSpawnInterval;
	}

	const float LegTime = GetLaborerLegSeconds();
	// 1인당 능력치 — 인원수로 나누지 않는다. 인원과 능력이 곱으로 쌓인다.
	const int32 PerLaborerOutput = FMath::Max(1, FMath::RoundToInt(GetUpgradeValue(EFactoryUpgradeType::HoldProductionAmount)));
	const int32 Amount = FBrickLaborerRules::HarvestPerTrip(
		PerLaborerOutput, LegTime, CurveCycle, SpeedMul, ProdScale);

	// 지면 속도에 맞춘 클립과 재생 속도 — 속도만 올리면 발이 끌리고, 클립만 올리면 제자리걸음이 된다
	const float GroundSpeed = GetLaborerPathLength() * SpeedMul / FMath::Max(LegTime, KINDA_SMALL_NUMBER);
	const bool bRun = (SpeedMul > 1.01f);
	UAnimSequence* Clip = bRun
		? BrickLaborerDefaults::ResolveOrFallback(RunAnim, BrickLaborerDefaults::Run, TEXT("RunAnim"))
		: BrickLaborerDefaults::ResolveOrFallback(WalkAnim, BrickLaborerDefaults::Walk, TEXT("WalkAnim"));
	UAnimSequence* CarryClip = bRun
		? BrickLaborerDefaults::ResolveCarryOrFallback(CarryRunAnim, BrickLaborerDefaults::CarryRun, TEXT("CarryRunAnim"))
		: BrickLaborerDefaults::ResolveCarryOrFallback(CarryWalkAnim, BrickLaborerDefaults::CarryWalk, TEXT("CarryWalkAnim"));
	const float RefSpeed = bRun ? RunClipRefSpeed : WalkClipRefSpeed;
	const float PlayRate = FBrickLaborerRules::ClipPlayRate(GroundSpeed, RefSpeed);

	for (ABrickLaborer* Laborer : Laborers)
	{
		if (!IsValid(Laborer))
		{
			continue;
		}
		Laborer->SetSpeedMultiplier(SpeedMul);
		Laborer->SetHarvestAmount(Amount);
		Laborer->SetGait(Clip, PlayRate, GroundSpeed, CarryClip);
	}
}

void ABrickFactory::OnLaborerEntered(int32 Amount)
{
	if (bIsRelocating) return;
	// 굴뚝(BrickSpawnPoint)에서 벽돌이 튀어나오는 기존 연출이 그대로 수확이다 — 별도 적재지점을 두지 않는다
	FVector SpawnLocation = GetActorLocation();
	if (SpawnPoints.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, SpawnPoints.Num() - 1);
		if (SpawnPoints[Index])
		{
			SpawnLocation = SpawnPoints[Index]->GetComponentLocation();
		}
	}

	PulseSmokeVFX();
	HarvestBricksAt(SpawnLocation, Amount);
}

void ABrickFactory::PulseSmokeVFX()
{
	if (!BrickSmokeVFX || !GetWorld() || SpawnPoints.IsEmpty())
	{
		return;
	}

	// 잡부가 여럿이면 매 입장마다 터뜨려 사실상 상시 분출이 된다 — 재점화에 최소 간격을 둔다
	const float Now = GetWorld()->GetTimeSeconds();
	const float Interval = bIsInteracting ? HoldSmokeInterval : SmokeMinIntervalSeconds;
	if (Now - LastSmokePulseTime < FMath::Max(0.3f, Interval) - KINDA_SMALL_NUMBER)
	{
		return;
	}
	LastSmokePulseTime = Now;

	if (CachedSmokeVFX.Num() != SpawnPoints.Num()) ClearSmokeVFX();
	CachedSmokeVFX.SetNum(SpawnPoints.Num());
	for (int32 Index = 0; Index < SpawnPoints.Num(); ++Index)
	{
		USceneComponent* Chimney = SpawnPoints[Index];
		if (!IsValid(Chimney)) continue;
		UNiagaraComponent* Smoke = CachedSmokeVFX[Index];
		if (!IsValid(Smoke))
		{
			Smoke = UNiagaraFunctionLibrary::SpawnSystemAttached(
				BrickSmokeVFX, Chimney, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, false, false, ENCPoolMethod::None, false);
			CachedSmokeVFX[Index] = Smoke;
		}
		if (!IsValid(Smoke)) continue;
		Smoke->SetFloatParameter(FName("Duration"), SmokePulseSeconds);
		Smoke->SetFloatParameter(FName("Scale"), bIsInteracting ? HoldSmokeScale : SmokePulseScale);
		Smoke->SetVariableFloat(TEXT("User.Smoke_Particle_Life"), FMath::Max(0.1f, SmokeParticleLifetime));
		Smoke->Activate(true);
	}

	// 짧은 분출 후 모든 굴뚝을 함께 멈춘다.
	GetWorld()->GetTimerManager().SetTimer(
		SmokePulseTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			for (UNiagaraComponent* Smoke : CachedSmokeVFX)
			{
				if (IsValid(Smoke)) Smoke->Deactivate();
			}
		}),
		SmokePulseSeconds, false);
}

void ABrickFactory::ClearSmokeVFX()
{
	GetWorldTimerManager().ClearTimer(SmokePulseTimerHandle);
	for (UNiagaraComponent* Smoke : CachedSmokeVFX)
	{
		if (IsValid(Smoke)) Smoke->DestroyComponent();
	}
	CachedSmokeVFX.Reset();
}

void ABrickFactory::NotifyLaborerRetired(ABrickLaborer* Laborer)
{
	Laborers.Remove(Laborer);
}


// Factory 업그레이드 시스템 — Count=1 단건 / Count>1 벌크. 사이드이펙트(차감/타이머/브로드캐스트)는 마지막 1회로 게이트.
bool ABrickFactory::UpgradeFactory(EFactoryUpgradeType UpgradeType, int32 Count)
{
	// 오프라인 수집 상한만 자동화 해금을 요구한다.
	if (FactoryUpgradeUnlockPolicy::IsAutoUnlockType(UpgradeType) &&
		!FactoryData.bAutoCollectionUnlocked)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] Auto collection not unlocked yet"));
		return false;
	}

	FFactoryUpgradeData* UpgradeDataPtr = FactoryData.UpgradeData.Find(UpgradeType);
	if (!UpgradeDataPtr)
	{
		return false;
	}

	// 하드 가드 — UI가 어떤 값을 넘겨도 액터가 스스로 상한(50)을 지킨다 (비용 오버플로/레벨 폭주 방지)
	if (Count <= 0)
	{
		return false;
	}
	Count = FMath::Min(Count, GetAllowedUpgradeCount(UpgradeType));
	if (Count <= 0)
	{
		return false;
	}
	const int32 CurrentLevel = UpgradeDataPtr->Level;

	// DT 단일 진실 — 비용 곡선/상한/자원타입. 미로드 시 FFactoryUpgradeConfig 폴백(단건 경로와 동일한 곡선).
	FFactoryCostCurve CostCurve;
	const int32 MaxLevel = GetUpgradeMaxLevel(UpgradeType);
	if (MaxLevel == INDEX_NONE)
	{
		return false;
	}
	EResourceType CostType = EResourceType::Money;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FFactoryUpgradeDefinition Def;
	if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(UpgradeType, Def))
	{
		CostCurve = Def.GetCostCurve();
		CostType = Def.CostResourceType;
	}
	else
	{
		if (UpgradeType == EFactoryUpgradeType::FactoryExpansion)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] FactoryExpansion definition is missing"));
			return false;
		}
		CostCurve = FFactoryUpgradeConfig::GetFallbackCostCurve(UpgradeType);
	}

	// MaxLevel 가드(0=무제한) — 잔여로 Count 클램프
	if (MaxLevel > 0 || UpgradeType == EFactoryUpgradeType::LaborerCount)
	{
		const int32 Remaining = MaxLevel - CurrentLevel;
		if (Remaining <= 0)
		{
			return false;
		}
		Count = FMath::Min(Count, Remaining);
	}

	if (UpgradeType == EFactoryUpgradeType::FactoryExpansion)
	{
		const FFactoryAppearanceRow* Appearance = TableMgr ? TableMgr->FindFactoryAppearance(CurrentLevel + Count) : nullptr;
		if (!Appearance || !Appearance->Mesh.LoadSynchronous())
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] Factory appearance is unavailable; purchase cancelled"));
			return false;
		}
	}

	// 합산 비용 — 패널 표시와 동일 곡선으로 청구해 "표시=청구" 정합
	const int64 TotalCost = CostCurve.BulkCost(CurrentLevel, Count);

	UResourceItemManager* ResourceManager = GetWorld()->GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceManager)
	{
		return false;
	}

	// 비용 자원 타입은 DT 단일 진실 — 하드코딩 금지 (현재 강화 슬롯은 전부 Money)
	if (ResourceManager->GetResourceAmount(CostType) < TotalCost)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] Not enough resource (type=%d). Need: %lld"),
			static_cast<int32>(CostType), TotalCost);
		return false;
	}

	// bShouldSave=false — 저장은 패널 OnUpgradeSuccess 의 지연 저장 1회로 통합 (홀드 10Hz 경로라 즉시 저장=히칭)
	ResourceManager->SpendResource(CostType, TotalCost, /*bShouldSave=*/false);
	// 지출 플로팅 — 홀드 10Hz 연타는 InGameLayer 스폰 지점의 배치 게이트가 합산
	if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->SpawnSpendPopup(CostType, TotalCost);
		}
	}

	// 레벨 일괄 증가 (벌크도 1회 누적)
	UpgradeDataPtr->Level += Count;

	// 업그레이드 값 계산
	UpgradeDataPtr->CurrentValue = GetUpgradeValueAtLevel(UpgradeType, UpgradeDataPtr->Level);

	UE_LOG(LogTemp, Log, TEXT("[BrickFactory] Upgraded %d x%d to Level %d, Value: %.2f, Cost: %lld"),
		(int32)UpgradeType, Count, UpgradeDataPtr->Level, UpgradeDataPtr->CurrentValue, TotalCost);

	UpdateProductionValues();

	// 인력·배달량·왕복 주기는 강화 값에서 파생 — 홀드 연타 경로라 값이 바뀐 이 지점에서만 갱신
	if (UpgradeType == EFactoryUpgradeType::HoldProductionAmount ||
		UpgradeType == EFactoryUpgradeType::HoldProductionSpeed ||
		UpgradeType == EFactoryUpgradeType::LaborerCount ||
		UpgradeType == EFactoryUpgradeType::FactoryExpansion)
	{
		RefreshLaborerCrew();
	}
	if (UpgradeType == EFactoryUpgradeType::FactoryExpansion)
	{
		RefreshFactoryAppearance(true);
	}

	OnFactoryUpgraded.Broadcast(UpgradeType);
	return true;
}

bool ABrickFactory::CanUpgradeAny() const
{
	UResourceItemManager* ResMgr = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UResourceItemManager>()
		: nullptr;
	if (!ResMgr) return false;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();

	const int64 CurrentMoney = ResMgr->GetResourceAmount(EResourceType::Money);

	for (const TPair<EFactoryUpgradeType, FFactoryUpgradeData>& Pair : FactoryData.UpgradeData)
	{
		const EFactoryUpgradeType Type = Pair.Key;
		const int32 Level = Pair.Value.Level;
		if (GetAllowedUpgradeCount(Type) <= 0)
		{
			continue;
		}

		if (FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Type) &&
			!FactoryData.bAutoCollectionUnlocked)
		{
			continue;
		}

		FFactoryUpgradeDefinition Def;
		const int32 MaxLevel = GetUpgradeMaxLevel(Type);
		int64 Cost = FFactoryUpgradeConfig::CalculateUpgradeCost(Type, Level);

		if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(Type, Def))
		{
			Cost = Def.CalculateUpgradeCost(Level);
		}
		else if (Type == EFactoryUpgradeType::FactoryExpansion)
		{
			continue;
		}

		if (MaxLevel == INDEX_NONE || ((MaxLevel > 0 || Type == EFactoryUpgradeType::LaborerCount) && Level >= MaxLevel))
		{
			continue;
		}

		if (CurrentMoney >= Cost)
		{
			return true;
		}
	}
	return false;
}

int32 ABrickFactory::GetUpgradeLevel(EFactoryUpgradeType UpgradeType) const
{
	const FFactoryUpgradeData* UpgradeDataPtr = FactoryData.UpgradeData.Find(UpgradeType);
	return UpgradeDataPtr ? UpgradeDataPtr->Level : 0;
}

int32 ABrickFactory::GetUpgradeMaxLevel(EFactoryUpgradeType UpgradeType) const
{
	const UGameInstance* FactoryGameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = FactoryGameInstance
		? FactoryGameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFactoryUpgradeDefinition Definition;
	int32 MaxLevel = FFactoryUpgradeConfig::GetMaxLevel(UpgradeType);
	if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(UpgradeType, Definition))
	{
		MaxLevel = Definition.MaxLevel;
	}
	else if (UpgradeType == EFactoryUpgradeType::FactoryExpansion)
	{
		return INDEX_NONE;
	}
	if (UpgradeType == EFactoryUpgradeType::LaborerCount)
	{
		const int32 Capacity = GetLaborerCapacity();
		if (Capacity <= 0)
		{
			return INDEX_NONE;
		}
		const int32 CapacityLevel = Capacity - 1;
		MaxLevel = MaxLevel > 0 ? FMath::Min(MaxLevel, CapacityLevel) : CapacityLevel;
	}
	return MaxLevel;
}

int32 ABrickFactory::GetAllowedUpgradeCount(EFactoryUpgradeType UpgradeType) const
{
	if (!FactoryData.UpgradeData.Contains(UpgradeType)
		|| (FactoryUpgradeUnlockPolicy::IsAutoUnlockType(UpgradeType) && !FactoryData.bAutoCollectionUnlocked))
	{
		return 0;
	}

	const int32 CurrentLevel = GetUpgradeLevel(UpgradeType);
	const int32 MaxLevel = GetUpgradeMaxLevel(UpgradeType);
	if (MaxLevel == INDEX_NONE)
	{
		return 0;
	}
	int32 Allowed = MaxLevel > 0 || UpgradeType == EFactoryUpgradeType::LaborerCount
		? FMath::Clamp(MaxLevel - CurrentLevel, 0, 50) : 50;

	const UGameInstance* GameInstance = GetGameInstance();
	const UMissionManagerSubsystem* Missions = GameInstance
		? GameInstance->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
	if (!Missions || !Missions->HasActiveMission())
	{
		return Allowed;
	}

	switch (Missions->GetActiveConditionType())
	{
	case EMissionConditionType::CollectBricks:
	{
		if (Missions->GetActiveMissionID() != TEXT("M1_CollectBricks"))
		{
			break;
		}
		const FFactorySaveData InitialData;
		const int32 InitialExpansionLevel = InitialData.UpgradeData.FindChecked(EFactoryUpgradeType::FactoryExpansion).Level;
		const int32 InitialCapacity = GetLaborerCapacityAtLevel(InitialExpansionLevel);
		if (UpgradeType == EFactoryUpgradeType::LaborerCount)
		{
			Allowed = FMath::Min(Allowed, FMath::Max(0, InitialCapacity - GetDesiredLaborerCount()));
		}
		else if (UpgradeType == EFactoryUpgradeType::FactoryExpansion)
		{
			Allowed = InitialCapacity > 0 && GetDesiredLaborerCount() >= InitialCapacity
				? FMath::Min(Allowed, FMath::Max(0, InitialExpansionLevel + 1 - CurrentLevel)) : 0;
		}
		else
		{
			Allowed = 0;
		}
		break;
	}
	case EMissionConditionType::UpgradeFactoryStats:
	{
		if (Missions->GetActiveMissionID() != TEXT("M2_UpgradeFactory"))
		{
			break;
		}
		FMissionTable Mission;
		if (!Missions->GetActiveMission(Mission))
		{
			return 0;
		}
		if (UpgradeType == EFactoryUpgradeType::HoldProductionSpeed
			|| UpgradeType == EFactoryUpgradeType::HoldProductionAmount)
		{
			const int32 Target = FTutorialDerivedMissionRules::ResolveTarget(Mission.ConditionAmount);
			Allowed = FMath::Min(Allowed, FMath::Max(0, Target - CurrentLevel));
		}
		else
		{
			Allowed = 0;
		}
		break;
	}
	default:
		break;
	}
	return Allowed;
}

float ABrickFactory::GetUpgradeValue(EFactoryUpgradeType UpgradeType) const
{
	const FFactoryUpgradeData* UpgradeDataPtr = FactoryData.UpgradeData.Find(UpgradeType);
	return UpgradeDataPtr ? UpgradeDataPtr->CurrentValue : 0.0f;
}

float ABrickFactory::GetUpgradeValueAtLevel(EFactoryUpgradeType UpgradeType, int32 Level) const
{
	if (UpgradeType != EFactoryUpgradeType::FactoryExpansion)
	{
		return FFactoryUpgradeConfig::CalculateUpgradeValue(UpgradeType, Level);
	}
	UGameInstance* GameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFactoryUpgradeDefinition Definition;
	if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(UpgradeType, Definition))
	{
		const FFactoryAppearanceRow* Appearance = TableMgr->FindFactoryAppearance(FMath::Min(Level, Definition.MaxLevel));
		if (Appearance)
		{
			return static_cast<float>(Appearance->LaborerCapacity);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] Missing factory appearance value at level %d"), Level);
	return 0.f;
}

void ABrickFactory::UnlockAutoCollection()
{
	ApplyAutoCollectionUnlock(true);
}

void ABrickFactory::ApplyAutoCollectionUnlock(bool bAllowCelebration)
{
	if (FactoryData.bAutoCollectionUnlocked)
	{
		return;
	}

	FactoryData.bAutoCollectionUnlocked = true;
	bPendingAutoUnlockCelebration = bAllowCelebration;

	// 열려 있는 공장 패널이 잠금 슬롯을 즉시 갱신하도록
	OnFactoryUpgraded.Broadcast(EFactoryUpgradeType::LaborerCount);

	UE_LOG(LogTemp, Log, TEXT("[BrickFactory] 자동(오프라인) 카테고리 해금 — 획득 상한 %d개"),
		GetAutoCollectionCapacity());
}

bool ABrickFactory::TryGetAutoUnlockRequiredHQLevel(int32& OutRequiredHQLevel) const
{
	OutRequiredHQLevel = 0;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr || !FactoryUpgradeUnlockPolicy::TryResolveRequiredHQLevel(
		TableMgr->GetAllFactoryUpgradeDefinitions(), OutRequiredHQLevel))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BrickFactory] Invalid auto-unlock RequiredHQLevel data; keeping auto slots locked"));
		return false;
	}

	return true;
}

void ABrickFactory::EvaluateAutoCollectionUnlock(bool bAllowCelebration)
{
	if (FactoryData.bAutoCollectionUnlocked)
	{
		return;
	}

	int32 RequiredHQLevel = 0;
	if (!TryGetAutoUnlockRequiredHQLevel(RequiredHQLevel))
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	if (!SaveMgr)
	{
		return;
	}

	if (SaveMgr->GetHQLevel() < RequiredHQLevel)
	{
		return;
	}

	ApplyAutoCollectionUnlock(bAllowCelebration);
}

void ABrickFactory::HandleHQLevelUp(int32 NewLevel)
{
	int32 RequiredHQLevel = 0;
	if (TryGetAutoUnlockRequiredHQLevel(RequiredHQLevel) && NewLevel >= RequiredHQLevel)
	{
		ApplyAutoCollectionUnlock(true);
	}
}

bool ABrickFactory::ConsumeAutoUnlockCelebration()
{
	const bool bPending = bPendingAutoUnlockCelebration;
	bPendingAutoUnlockCelebration = false;
	return bPending;
}

bool ABrickFactory::IsAutoCollectionUnlocked() const
{
	return FactoryData.bAutoCollectionUnlocked;
}

int32 ABrickFactory::GetAutoCollectionCapacity() const
{
	const int32 Capacity = FMath::RoundToInt(GetUpgradeValue(EFactoryUpgradeType::AutoCollectionCapacity));
	return Capacity > 0 ? Capacity : 100;
}

float ABrickFactory::GetOfflineBricksPerSecond() const
{
	// 자리를 비운 동안에도 잡부는 평소 속도로 일했다고 본다 — 적립 속도를 따로 강화하지 않는 이유.
	float CurveCycle = GetUpgradeValue(EFactoryUpgradeType::HoldProductionSpeed);
	if (CurveCycle <= 0.f)
	{
		CurveCycle = BrickSpawnInterval;
	}

	const int32 Count = GetDesiredLaborerCount();
	const int32 PerLaborerOutput = FMath::Max(1, FMath::RoundToInt(GetUpgradeValue(EFactoryUpgradeType::HoldProductionAmount)));
	const float LegTime = GetLaborerLegSeconds();
	const int32 PerTrip = FBrickLaborerRules::HarvestPerTrip(
		PerLaborerOutput, LegTime, CurveCycle, /*SpeedMultiplier=*/1.f, IdleProductionScale);

	return FBrickLaborerRules::BricksPerSecond(PerTrip, Count, LegTime, 1.f);
}

void ABrickFactory::SettleOfflineBricks(float /*TotalGained*/, float OfflineSeconds)
{
	// 관찰용 — 액터가 스폰되기 전에 정산이 돌면 이 로그가 아예 안 찍힌다(오프라인 벽돌 유실 판정 단서)
	UE_LOG(LogTemp, Verbose, TEXT("[BrickFactory] 오프라인 정산 수신 (액터 생존) %.0f초"), OfflineSeconds);

	if (!FactoryData.bAutoCollectionUnlocked)
	{
		return;
	}

	const int32 MaxGain = GetAutoCollectionCapacity();
	const int64 Gained = FactoryOfflineGainPolicy::SettledBricks(
		GetOfflineBricksPerSecond(), OfflineSeconds, MaxGain,
		USaveLoadManager::MinOfflineSettleSeconds);
	if (Gained <= 0)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* ResourceMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] 오프라인 벽돌 %lld개 지급 실패: ResourceItemManager 없음"), Gained);
		return;
	}

	ResourceMgr->StoreResource(EResourceType::Brick, Gained, true);

	// 수령 시점이 아니라 정산 시점에 보고한다 — 수동 수령 단계가 없어졌으므로
	if (UQuestSubsystem* QuestMgr = GI->GetSubsystem<UQuestSubsystem>())
	{
		QuestMgr->ReportCounter(EQuestCounter::BrickCollect, Gained);
	}

	// 정산 모달이 벽돌 행을 그릴 수 있게 보고에 싣는다 (보류→UI 준비 후 1회 flush)
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->AddPendingOfflineBrickGain(Gained);
	}

	UE_LOG(LogTemp, Log, TEXT("[BrickFactory] 오프라인 %.0f초 · 초당 %.2f개 · 지급 %lld (상한 %d)"),
		OfflineSeconds, GetOfflineBricksPerSecond(), Gained, MaxGain);
}

void ABrickFactory::UpdateProductionValues()
{
	// 현재 업그레이드 레벨에 따라 값 재계산
	for (auto& Pair : FactoryData.UpgradeData)
	{
		EFactoryUpgradeType Type = Pair.Key;
		FFactoryUpgradeData& Data = Pair.Value;

		Data.CurrentValue = GetUpgradeValueAtLevel(Type, Data.Level);
	}
}

FFactorySaveData ABrickFactory::GetFactoryData() const
{
	FFactorySaveData Saved = FactoryData;
	Saved.PlacementTransform = bIsRelocating ? RelocationOriginalTransform : GetActorTransform();
	return Saved;
}

void ABrickFactory::SetFactoryData(const FFactorySaveData& InData)
{
	FactoryData = InData;
	if (!FactoryData.OwningPlotId.IsNone())
	{
		SetActorTransform(FactoryData.PlacementTransform);
	}
	UpdateProductionValues();

	// 로드된 세이브가 이미 HQ 조건을 넘겼는데 잠겨 있으면 조용히 해금
	if (UpgradeEffect) UpgradeEffect->Cancel();
	RefreshFactoryAppearance(false);
	EvaluateAutoCollectionUnlock(false);

	// 잡부 수는 저장하지 않는다 — 로드된 강화 레벨에서 매번 다시 세운다
	RefreshLaborerCrew();
	if (!FactoryData.OwningPlotId.IsNone() && GetWorld())
	{
		for (TActorIterator<AVacantPlotDressingManager> It(GetWorld()); It; ++It)
		{
			It->RefreshAllPlots();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BrickFactory] Factory data loaded for %s"), *FactoryID.ToString());
}

void ABrickFactory::ReCalcBoxExtent() const
{
	if (!MainMeshComponent || !MainMeshComponent->GetStaticMesh() || !BoxComponent)
	{
		return;
	}
	const FBox MeshBounds = MainMeshComponent->GetStaticMesh()->GetBoundingBox();
	const FBox RootBounds = MeshBounds.TransformBy(MainMeshComponent->GetRelativeTransform());
	const FVector Padding(FMath::Max(0.f, BoundGap * 100.f));
	const FVector2D ReservedHalfExtent = GetPlacementFootprintHalfExtent(false);
	const FVector ReservedCenter = GetActorTransform().InverseTransformPosition(GetPlacementFootprintCenter());
	const FVector BoxScale = BoxComponent->GetComponentScale().GetAbs();
	BoxComponent->SetRelativeLocation(FVector(ReservedCenter.X, ReservedCenter.Y, RootBounds.GetCenter().Z));
	BoxComponent->SetRelativeRotation(FRotator::ZeroRotator);
	BoxComponent->SetBoxExtent(FVector(
		ReservedHalfExtent.X / FMath::Max(UE_DOUBLE_SMALL_NUMBER, BoxScale.X),
		ReservedHalfExtent.Y / FMath::Max(UE_DOUBLE_SMALL_NUMBER, BoxScale.Y),
		(RootBounds.GetExtent().Z + Padding.Z) / FMath::Max(UE_DOUBLE_SMALL_NUMBER, BoxScale.Z)));
	// 공유(아키타입) 바디셋업에서 갈라진 뒤 SetBoxExtent 는 물리 모양을 옛 크기로 다시 만든다 — 트레이스가 새 크기를 보게 재생성.
	if (BoxComponent->IsPhysicsStateCreated())
	{
		BoxComponent->RecreatePhysicsState();
	}
	if (NavBlocker && NavBlocker->GetAttachParent())
	{
		NavBlocker->SetRelativeLocation(MeshBounds.GetCenter());
		NavBlocker->SetRelativeRotation(FRotator::ZeroRotator);
		NavBlocker->SetBoxExtent(MeshBounds.GetExtent() + Padding);
	}
	if (ReCalcBoxExtentDelegate) ReCalcBoxExtentDelegate->Broadcast();
}

void ABrickFactory::UpdateAppearanceTransform(float Pulse)
{
	if (!MainMeshComponent) return;
	const float Scale = FactoryScale * Pulse;
	// 외형의 로컬 원점이 치우쳐 있어도 지면의 예약 중심을 기준으로 확대/축소한다.
	const FVector Anchor(PlacementFootprintOffset.X, PlacementFootprintOffset.Y, 0.f);
	MainMeshComponent->SetRelativeLocation(Anchor * (1.f - Scale));
	MainMeshComponent->SetRelativeScale3D(FVector(Scale));
}

void ABrickFactory::RefreshFactoryAppearance(bool bAnimate)
{
	UGameInstance* GameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const int32 DesiredLevel = GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion);
	const FFactoryAppearanceRow* Appearance = TableMgr ? TableMgr->FindFactoryAppearance(DesiredLevel) : nullptr;
	if (!Appearance || !MainMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] Missing factory appearance at level %d"), DesiredLevel);
		return;
	}
	if (bAnimate && UpgradeEffect && UpgradeEffect->IsPlaying())
	{
		// 한 연출의 교체 대상을 고정한다. 이후 구매는 종료 시 최신 레벨 하나로 합친다.
		return;
	}
	UStaticMesh* TargetMesh = Appearance->Mesh.LoadSynchronous();
	if (!TargetMesh) return;
	if (bAnimate && bAppearanceInitialized && DesiredLevel == CurrentAppearanceLevel) return;

	PendingAppearanceLevel = DesiredLevel;
	PendingAppearanceMesh = TargetMesh;
	if (bAnimate && bAppearanceInitialized && UpgradeEffect)
	{
		FBox WrapBounds = TargetMesh->GetBoundingBox();
		if (UStaticMesh* CurrentMesh = MainMeshComponent->GetStaticMesh())
		{
			WrapBounds += CurrentMesh->GetBoundingBox();
		}
		if (UpgradeEffect->Play(MainMeshComponent, WrapBounds))
		{
			PlayUpgradeSound(TEXT("Factory_UpgradeWrap"));
			return;
		}
	}
	ApplyPendingFactoryAppearance();
}

void ABrickFactory::ApplyPendingFactoryAppearance()
{
	if (!PendingAppearanceMesh || !MainMeshComponent) return;
	MainMeshComponent->SetStaticMesh(PendingAppearanceMesh);
	MainMeshComponent->EmptyOverrideMaterials();
	CurrentAppearanceLevel = PendingAppearanceLevel;
	bAppearanceInitialized = true;
	PendingAppearanceMesh = nullptr;
	RebuildAppearanceSpawnPoints();
	ReCalcBoxExtent();
	for (ABrickLaborer* Laborer : Laborers)
	{
		if (!IsValid(Laborer)) continue;
		Laborer->SetCycleDuration(GetLaborerLegSeconds());
		Laborer->RefreshPath();
	}
	if (!Laborers.IsEmpty()) ApplyLaborerPace(bIsInteracting);
	if (BodyMID)
	{
		SetupBodyNightEmissive();
		BuildNightLights();
		ApplyNightVisibility(bLightVisible);
	}
	if (UpgradeEffect && UpgradeEffect->IsPlaying())
	{
		PlayUpgradeSound(TEXT("Factory_UpgradeConceal"));
	}
}

void ABrickFactory::RebuildAppearanceSpawnPoints()
{
	ClearSmokeVFX();
	UStaticMesh* BodyMesh = MainMeshComponent ? MainMeshComponent->GetStaticMesh() : nullptr;
	if (!BodyMesh) return;
	TArray<FName> SocketNames;
	for (const UStaticMeshSocket* MeshSocket : BodyMesh->Sockets)
	{
		if (MeshSocket && MeshSocket->SocketName.ToString().StartsWith(TEXT("BrickSpawnPoint")))
		{
			SocketNames.Add(MeshSocket->SocketName);
		}
	}
	SocketNames.Sort(FNameLexicalLess());
	SpawnPoints.Reset();
	for (int32 Index = 0; Index < SocketNames.Num(); ++Index)
	{
		if (!AppearanceSpawnPoints.IsValidIndex(Index))
		{
			USceneComponent* Point = NewObject<USceneComponent>(this);
			Point->SetupAttachment(MainMeshComponent);
			Point->SetMobility(EComponentMobility::Movable);
			Point->RegisterComponent();
			AppearanceSpawnPoints.Add(Point);
		}
		USceneComponent* Point = AppearanceSpawnPoints[Index];
		Point->AttachToComponent(MainMeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketNames[Index]);
		Point->SetRelativeTransform(FTransform::Identity);
		// 같은 상대 변환이어도 교체된 메시의 소켓 위치는 다시 계산해야 한다.
		Point->UpdateComponentToWorld();
		SpawnPoints.Add(Point);
	}
	if (SpawnPoints.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] No chimney sockets on %s"), *GetNameSafe(BodyMesh));
	}
}

void ABrickFactory::HandleUpgradeRevealed()
{
	if (!SpawnPoints.IsEmpty())
	{
		LastSmokePulseTime = -1000.f;
		PulseSmokeVFX();
	}
	PlayUpgradeSound(TEXT("Factory_UpgradeReveal"));
}

void ABrickFactory::HandleUpgradeEffectFinished()
{
	if (GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion) != CurrentAppearanceLevel)
	{
		RefreshFactoryAppearance(true);
	}
}

void ABrickFactory::PlayUpgradeSound(FName SoundId)
{
	UGameInstance* GameInstance = GetGameInstance();
	USoundManagerSubsystem* SoundMgr = GameInstance ? GameInstance->GetSubsystem<USoundManagerSubsystem>() : nullptr;
	if (!SoundMgr) return;
	bool bFound = false;
	const FGameSFXTable SoundData = SoundMgr->GetGameSFXData(SoundId, bFound);
	if (!bFound || SoundData.Sound.IsNull()) return;
	if (USoundBase* Sound = SoundData.Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, GetActorLocation(),
			SoundMgr->GetFinalVolume(ESoundCategory::SFX, SoundData.VolumeMultiplier), SoundData.PitchMultiplier);
	}
}

void ABrickFactory::PlayBounce()
{
	// Timeline이 이미 재생 중이면 스킵 (부드러운 애니메이션 유지)
	if (Wooble_Timeline.IsPlaying())
	{
		return;
	}

	if (!MainMeshComponent)
	{
		return;
	}

	// Timeline 재생 시작 (Material 파라미터 사용 안 함)
	Wooble_Timeline.PlayFromStart();
	SetActorTickEnabled(true);
}

void ABrickFactory::Wooble_TimelineUpdate(float Value)
{
	if (!MainMeshComponent)
	{
		return;
	}

	// Sine Wave로 부드러운 Pulse 효과 (0→1→0)
	float SineWave = FMath::Sin(Value * PI);

	// BounceIntensity만큼 크기 증가 (예: 0.05면 ±5%)
	float Pulse = 1.0f + (SineWave * BounceIntensity);

	// 모든 축에 동일하게 적용 (균등 확대/축소)
	UpdateAppearanceTransform(Pulse);
}

void ABrickFactory::Wooble_TimelineFinished()
{
	// Timeline 종료 시 Scale을 원래대로 복원
	if (MainMeshComponent)
	{
		UpdateAppearanceTransform();
	}
	SetActorTickEnabled(false);
}

void ABrickFactory::TickHoldSmoke()
{
	if (!bIsInteracting || SpawnPoints.IsEmpty()) return;
	PulseSmokeVFX();
}

void ABrickFactory::StartSmokeVFX()
{
	LastSmokePulseTime = -1000.f;
	TickHoldSmoke();
	GetWorldTimerManager().SetTimer(HoldSmokeTimerHandle, this, &ABrickFactory::TickHoldSmoke,
		FMath::Max(0.3f, HoldSmokeInterval), true);
	const FVector SoundLocation = SpawnPoints.Num() > 0 && SpawnPoints[0]
		? SpawnPoints[0]->GetComponentLocation() : GetActorLocation();
	StartSteamSound(SoundLocation);
	StartAmbienceSound(SoundLocation);
}

void ABrickFactory::StopSmokeVFX()
{
	GetWorldTimerManager().ClearTimer(HoldSmokeTimerHandle);
	GetWorldTimerManager().ClearTimer(SmokePulseTimerHandle);
	for (UNiagaraComponent* Smoke : CachedSmokeVFX)
	{
		if (IsValid(Smoke)) Smoke->Deactivate();
	}
	StopSteamSound();
	StopAmbienceSound();
}

void ABrickFactory::StartBoostStreaks()
{
	if (BoostStreakMaterial.IsNull() || BoostStreakMesh.IsNull() || !MainMeshComponent) return;
	UMaterialInterface* StreakMaterial = BoostStreakMaterial.LoadSynchronous();
	if (!StreakMaterial) return;

	GetWorldTimerManager().ClearTimer(BoostStreakHideTimerHandle);
	if (!BoostStreakISM)
	{
		UStaticMesh* PlaneMesh = BoostStreakMesh.LoadSynchronous();
		if (!PlaneMesh) return;
		BoostStreakISM = NewObject<UInstancedStaticMeshComponent>(this, TEXT("BoostStreakISM"));
		BoostStreakISM->SetupAttachment(RootComponent);
		BoostStreakISM->SetMobility(EComponentMobility::Movable);
		BoostStreakISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoostStreakISM->SetCanEverAffectNavigation(false);
		BoostStreakISM->SetCastShadow(false);
		BoostStreakISM->bUseAsOccluder = false;
		BoostStreakISM->SetStaticMesh(PlaneMesh);
		BoostStreakISM->RegisterComponent();
		BoostStreakMID = UMaterialInstanceDynamic::Create(StreakMaterial, this);
		BoostStreakISM->SetMaterial(0, BoostStreakMID);
	}

	// 본체 메시 경계만 사용한다. 굴뚝 스폰점/충돌 박스/발광 인스턴스는 범위에 섞지 않는다.
	const FBoxSphereBounds MeshBounds = MainMeshComponent->Bounds;
	const FVector HalfSize = MeshBounds.BoxExtent;
	const FVector Center = MeshBounds.Origin;
	const float Height = FMath::Max(100.f, BoostStreakTravelHeight);
	const int32 Count = FMath::Clamp(BoostStreakCount, 4, 16);
	BoostStreakISM->ClearInstances();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = 2.f * PI * (Index + 0.35f) / Count;
		const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		const FVector Tangent(-Radial.Y, Radial.X, 0.f);
		const float EdgeScale = 1.f / FMath::Max(FMath::Abs(Radial.X), FMath::Abs(Radial.Y));
		const FVector Position = Center + FVector(Radial.X * (HalfSize.X * EdgeScale + 100.f),
			Radial.Y * (HalfSize.Y * EdgeScale + 100.f), -HalfSize.Z + 140.f + Height * 0.5f);
		const FQuat Rotation = FRotationMatrix::MakeFromXY(Tangent, FVector::UpVector).ToQuat();
		BoostStreakISM->AddInstance(FTransform(Rotation, Position,
			FVector(FMath::Max(1.f, BoostStreakWidth) / 100.f, Height / 100.f, 1.f)), true);
	}
	BoostStreakMID->SetScalarParameterValue(TEXT("StartTime"), GetWorld()->GetTimeSeconds());
	BoostStreakMID->SetScalarParameterValue(TEXT("StopTime"), -1.f);
	BoostStreakISM->SetVisibility(true);
}

void ABrickFactory::StopBoostStreaks()
{
	if (!BoostStreakISM || !BoostStreakMID) return;
	BoostStreakMID->SetScalarParameterValue(TEXT("StopTime"), GetWorld()->GetTimeSeconds());
	GetWorldTimerManager().SetTimer(BoostStreakHideTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (BoostStreakISM) BoostStreakISM->SetVisibility(false);
		}), 0.24f, false);
}

void ABrickFactory::StartSteamSound(const FVector& Location)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>();
	if (!SoundMgr) return;

	bool bSuccess = false;
	FGameSFXTable SoundData = SoundMgr->GetGameSFXData(FName("Factory_Steam"), bSuccess);
	if (!bSuccess || SoundData.Sound.IsNull()) return;
	USoundBase* Sound = SoundData.Sound.LoadSynchronous();
	if (!Sound) return;

	// Looping 강제 — 에디터 SoundWave 설정 누락에 의존하지 않음. Factory_Steam 은 이 곳에서만 쓰임.
	if (USoundWave* SoundWave = Cast<USoundWave>(Sound))
	{
		SoundWave->bLooping = true;
	}

	// 이전 인스턴스가 살아있으면 즉시 stop — 연속 클릭 시 중첩 방지
	if (IsValid(CachedSteamAudio))
	{
		CachedSteamAudio->Stop();
		CachedSteamAudio = nullptr;
	}

	// DT 배율만 쓰면 마스터/SFX 볼륨·음소거가 무시된다 — 최종 볼륨은 SoundManager 경유
	CachedSteamAudio = UGameplayStatics::SpawnSoundAtLocation(
		GetWorld(), Sound, Location, FRotator::ZeroRotator,
		SoundMgr->GetFinalVolume(ESoundCategory::SFX, SoundData.VolumeMultiplier), SoundData.PitchMultiplier);
}

void ABrickFactory::StopSteamSound()
{
	if (IsValid(CachedSteamAudio))
	{
		// 0.3s fade out — VFX Deactivate 와 자연스럽게 동기화
		CachedSteamAudio->FadeOut(0.3f, 0.0f);
		CachedSteamAudio = nullptr;
	}
}

void ABrickFactory::StartAmbienceSound(const FVector& Location)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>();
	if (!SoundMgr) return;

	bool bSuccess = false;
	FGameSFXTable SoundData = SoundMgr->GetGameSFXData(FName("Factory_Ambience"), bSuccess);
	if (!bSuccess || SoundData.Sound.IsNull()) return;
	USoundBase* Sound = SoundData.Sound.LoadSynchronous();
	if (!Sound) return;

	// Looping 은 SFX_Factory_Ambience.uasset 의 Looping 속성으로 설정해야 함.
	// 런타임에 SoundWave->bLooping 수정 시 공유 자산이 변경되어 다른 사용처에 부작용 leak

	if (IsValid(CachedAmbienceAudio))
	{
		CachedAmbienceAudio->Stop();
		CachedAmbienceAudio = nullptr;
	}

	// 위와 동일 — 설정 볼륨/음소거 반영
	CachedAmbienceAudio = UGameplayStatics::SpawnSoundAtLocation(
		GetWorld(), Sound, Location, FRotator::ZeroRotator,
		SoundMgr->GetFinalVolume(ESoundCategory::SFX, SoundData.VolumeMultiplier), SoundData.PitchMultiplier);
}

void ABrickFactory::StopAmbienceSound()
{
	if (IsValid(CachedAmbienceAudio))
	{
		CachedAmbienceAudio->FadeOut(0.3f, 0.0f);
		CachedAmbienceAudio = nullptr;
	}
}
