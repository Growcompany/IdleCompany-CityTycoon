// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Factory/BrickFactory.h"

#include "UI/Panel/InGameLayerWidget.h"
#include "Data/FactoryUpgradeConfig.h"
#include "Data/FactoryUpgradeData.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"

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
}

// Called when the game starts or when spawned
void ABrickFactory::BeginPlay()
{
	Super::BeginPlay();

	UpdateProductionValues();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->OnHQLevelUp.AddDynamic(this, &ABrickFactory::HandleHQLevelUp);
		}
	}

	// HQ 레벨업 시점을 놓친 세이브 보정 — 연출 없이 조용히
	EvaluateAutoCollectionUnlock(false);

	RestartAutoCollectionTimer();

	// 낮엔 additive 조명을 숨겨 fill 절감 — day/night 전환 구독. 못 찾으면 항상 표시.
	if (ATimeCycleManager* Clock = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass())))
	{
		CachedTimeCycle = Clock;
		Clock->OnSunRise.AddDynamic(this, &ABrickFactory::HandleSunRise);
		Clock->OnSunSet.AddDynamic(this, &ABrickFactory::HandleSunSet);
		const int32 Cur = Clock->GetCurrentTime().ToSeconds();
		const int32 Rise = Clock->GetSunRiseTime().ToSeconds();
		const int32 Set = Clock->GetSunSetTime().ToSeconds();
		bLightVisible = (Cur >= Set) || (Cur < Rise);
	}

	SetupBodyNightEmissive();
	BuildNightLights();
}

void ABrickFactory::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Steam/Ambience 사운드 컴포넌트가 World에 분리 spawn 되어 있어 Actor 종료 시 명시적 정리 필요
	StopSteamSound();
	StopAmbienceSound();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->OnHQLevelUp.RemoveDynamic(this, &ABrickFactory::HandleHQLevelUp);
		}
	}

	if (ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		Clock->OnSunRise.RemoveDynamic(this, &ABrickFactory::HandleSunRise);
		Clock->OnSunSet.RemoveDynamic(this, &ABrickFactory::HandleSunSet);
	}

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
}

void ABrickFactory::HandleSunRise() { ApplyNightVisibility(false); }
void ABrickFactory::HandleSunSet()  { ApplyNightVisibility(true); }

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
	// Factory 모드로 전환
	AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(PC);
	MainPC->GoToFactoryMode();

	bIsInteracting = true;

	UE_LOG(LogTemp, Log, TEXT("Pressed on ABrickFactory"));

	// 연기 VFX 시작 (Continuous 타입이므로 인터랙션 동안 유지)
	StartSmokeVFX();

	// 트레일러/디버그 간격 오버라이드 우선 (치트 FactoryMax [Amount] [SpeedSec]), 없으면 강화 곡선값
	float CurrentInterval = (TrailerSpawnIntervalOverride > 0.0f)
		? TrailerSpawnIntervalOverride
		: GetUpgradeValue(EFactoryUpgradeType::HoldProductionSpeed);
	if (CurrentInterval <= 0.0f)
	{
		CurrentInterval = BrickSpawnInterval;
	}

	// 홀드 동안 포인터 아래 라디얼 게이지 — 생산 주기와 동기, OnEndInteract 에서 종료
	if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->BeginHoldPulse(CurrentInterval);
		}
	}

	// 첫 벽돌도 한 주기를 채워야 생산 — 게이지(한 바퀴=벽돌 1개)와 정확히 일치, 즉시 생성 없음
	GetWorld()->GetTimerManager().SetTimer(
		BrickSpawnTimerHandle, this, &ABrickFactory::SpawnBrick, CurrentInterval, true);
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

	if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->EndHoldPulse();
		}
	}

	// 벽돌 생산 타이머 중지
	GetWorld()->GetTimerManager().ClearTimer(BrickSpawnTimerHandle);

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

void ABrickFactory::SpawnBrick()
{
	// 1) SpawnPoints 배열에서 랜덤 선택
	FVector SpawnLocation = GetActorLocation();
	if (SpawnPoints.Num() > 0)
	{
		int32 Idx = FMath::RandRange(0, SpawnPoints.Num() - 1);
		if (SpawnPoints[Idx])
		{
			SpawnLocation = SpawnPoints[Idx]->GetComponentLocation();
		}
	}

	// 2) VFX 위치를 벽돌 스폰 위치로 이동 + 비활성화면 재시작
	if (CachedSmokeVFX)
	{
		CachedSmokeVFX->SetWorldLocation(SpawnLocation);

		if (!CachedSmokeVFX->IsActive())
		{
			CachedSmokeVFX->Activate(true);
		}
	}

	// 3) 월드 → 스크린 좌표 변환
	FVector2D ScreenPos;
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		GetWorld()->GetFirstPlayerController(),
		SpawnLocation,
		ScreenPos, true);
	// 생산 틱마다(최대 초당 5회) 찍히던 로그 — 필요할 때만 켜도록 Verbose
	UE_LOG(LogTemp, Verbose, TEXT("[ABrick] ProjectWorldLocationToWidgetPosition ScreenPos : %s"), *ScreenPos.ToString());

	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	check(UIManager);

	UInGameLayerWidget* InGameUI = UIManager->GetInGameLayer();
	check(InGameUI);

	// 업그레이드된 생산량 적용
	int32 ProductionAmount = FMath::RoundToInt(GetUpgradeValue(EFactoryUpgradeType::HoldProductionAmount));

	InGameUI->SpawnBrickCollectAt(ScreenPos, EResourceType::Brick, ProductionAmount);

	// 생산 순간과 동기화된 바운스 (재생 중이면 PlayBounce 내부 IsPlaying 가드가 스킵 — 연타 안전)
	// 생산틱 시각 강조는 홀드 게이지 한 바퀴 완료 팝이 담당
	PlayBounce();
}


void ABrickFactory::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	//UE_LOG(LogTemp, Log, TEXT("BrickFactory_Clicked"));
}

void ABrickFactory::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SpawnPoints.Empty();

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
	}
}


// Factory 업그레이드 시스템 — Count=1 단건 / Count>1 벌크. 사이드이펙트(차감/타이머/브로드캐스트)는 마지막 1회로 게이트.
bool ABrickFactory::UpgradeFactory(EFactoryUpgradeType UpgradeType, int32 Count)
{
	// BrickStock 은 상태 표시 전용 슬롯 — 업그레이드 호출 자체를 차단
	if (UpgradeType == EFactoryUpgradeType::BrickStock)
	{
		return false;
	}

	// 자동 카테고리는 언락되지 않았으면 업그레이드 불가
	if ((UpgradeType == EFactoryUpgradeType::AutoCollection ||
		 UpgradeType == EFactoryUpgradeType::AutoCollectionCapacity) &&
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
	Count = FMath::Clamp(Count, 1, 50);
	const int32 CurrentLevel = UpgradeDataPtr->Level;

	// DT 단일 진실 — 비용 곡선/상한/자원타입. 미로드 시 FFactoryUpgradeConfig 폴백(단건 경로와 동일한 곡선).
	FFactoryCostCurve CostCurve;
	int32 MaxLevel = 0;
	EResourceType CostType = EResourceType::Money;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FFactoryUpgradeDefinition Def;
	if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(UpgradeType, Def))
	{
		CostCurve = Def.GetCostCurve();
		MaxLevel = Def.MaxLevel;
		CostType = Def.CostResourceType;
	}
	else
	{
		CostCurve = FFactoryUpgradeConfig::GetFallbackCostCurve(UpgradeType);
		MaxLevel = FFactoryUpgradeConfig::GetMaxLevel(UpgradeType);
	}

	// MaxLevel 가드(0=무제한) — 잔여로 Count 클램프
	if (MaxLevel > 0)
	{
		const int32 Remaining = MaxLevel - CurrentLevel;
		if (Remaining <= 0)
		{
			return false;
		}
		Count = FMath::Min(Count, Remaining);
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
	UpgradeDataPtr->CurrentValue = FFactoryUpgradeConfig::CalculateUpgradeValue(UpgradeType, UpgradeDataPtr->Level);

	// 자동 수집 타이머 재설정 — 벌크여도 마지막 값 기준 1회
	if (UpgradeType == EFactoryUpgradeType::AutoCollection)
	{
		RestartAutoCollectionTimer();
	}

	UE_LOG(LogTemp, Log, TEXT("[BrickFactory] Upgraded %d x%d to Level %d, Value: %.2f, Cost: %lld"),
		(int32)UpgradeType, Count, UpgradeDataPtr->Level, UpgradeDataPtr->CurrentValue, TotalCost);

	UpdateProductionValues();

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

		if ((Type == EFactoryUpgradeType::AutoCollection ||
			 Type == EFactoryUpgradeType::AutoCollectionCapacity) &&
			!FactoryData.bAutoCollectionUnlocked)
		{
			continue;
		}

		// BrickStock은 업그레이드 대상 아님
		if (Type == EFactoryUpgradeType::BrickStock)
		{
			continue;
		}

		FFactoryUpgradeDefinition Def;
		int32 MaxLevel = FFactoryUpgradeConfig::GetMaxLevel(Type);
		int64 Cost = FFactoryUpgradeConfig::CalculateUpgradeCost(Type, Level);

		if (TableMgr && TableMgr->GetFactoryUpgradeDefinition(Type, Def))
		{
			MaxLevel = Def.MaxLevel;
			Cost = Def.CalculateUpgradeCost(Level);
		}

		if (MaxLevel > 0 && Level >= MaxLevel)
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

float ABrickFactory::GetUpgradeValue(EFactoryUpgradeType UpgradeType) const
{
	const FFactoryUpgradeData* UpgradeDataPtr = FactoryData.UpgradeData.Find(UpgradeType);
	return UpgradeDataPtr ? UpgradeDataPtr->CurrentValue : 0.0f;
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

	RestartAutoCollectionTimer();

	// 열려 있는 공장 패널이 잠금 슬롯을 즉시 갱신하도록
	OnFactoryUpgraded.Broadcast(EFactoryUpgradeType::AutoCollection);

	UE_LOG(LogTemp, Log, TEXT("[BrickFactory] Auto collection unlocked (interval %.3fs)"),
		FMath::Max(MinAutoCollectionInterval, GetUpgradeValue(EFactoryUpgradeType::AutoCollection)));
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

void ABrickFactory::RestartAutoCollectionTimer()
{
	UWorld* TimerWorld = GetWorld();
	if (!TimerWorld)
	{
		return;
	}

	FTimerManager& TimerMgr = TimerWorld->GetTimerManager();
	TimerMgr.ClearTimer(AutoCollectionTimerHandle);

	if (!FactoryData.bAutoCollectionUnlocked)
	{
		return;
	}

	// 강화 곡선 바닥이 0.001초라 하한을 여기서 강제하지 않으면 초당 1000틱 타이머가 된다
	const float Interval = FMath::Max(MinAutoCollectionInterval, GetUpgradeValue(EFactoryUpgradeType::AutoCollection));

	TimerMgr.SetTimer(AutoCollectionTimerHandle, this, &ABrickFactory::AutoCollectResources, Interval, true);
}

bool ABrickFactory::IsAutoCollectionUnlocked() const
{
	return FactoryData.bAutoCollectionUnlocked;
}

int64 ABrickFactory::GetAutoCollectedAmount() const
{
	return FactoryData.AutoCollectedAmount;
}

void ABrickFactory::CollectAutoResources()
{
	if (FactoryData.AutoCollectedAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrickFactory] No auto resources to collect"));
		return;
	}

	// 실제 리소스 지급 (수동 수집이므로 저장 O)
	if (UResourceItemManager* ResourceMgr = GetWorld()->GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResourceMgr->StoreResource(EResourceType::Brick, FactoryData.AutoCollectedAmount, true);
		UE_LOG(LogTemp, Log, TEXT("[BrickFactory] Collected %lld auto resources and saved"), FactoryData.AutoCollectedAmount);
	}

	FactoryData.AutoCollectedAmount = 0;
}

void ABrickFactory::AutoCollectResources()
{
	if (!FactoryData.bAutoCollectionUnlocked)
	{
		return;
	}

	// 최대 용량 확인
	int32 MaxCapacity = FMath::RoundToInt(GetUpgradeValue(EFactoryUpgradeType::AutoCollectionCapacity));
	if (MaxCapacity <= 0)
	{
		MaxCapacity = 100; // 기본 용량
	}

	if (FactoryData.AutoCollectedAmount >= MaxCapacity)
	{
		return;
	}

	// 생산량만큼 자동 수집
	int32 ProductionAmount = FMath::RoundToInt(GetUpgradeValue(EFactoryUpgradeType::HoldProductionAmount));
	if (ProductionAmount <= 0)
	{
		ProductionAmount = 1;
	}

	FactoryData.AutoCollectedAmount = FMath::Min(FactoryData.AutoCollectedAmount + ProductionAmount, MaxCapacity);

	// 가득 참 '전환' 1회 알림 (이미 가득이면 상단 early-return 이라 여기 도달 = 방금 가득)
	if (FactoryData.AutoCollectedAmount >= MaxCapacity)
	{
		if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(
				NSLOCTEXT("Factory", "AutoCollectFull", "벽돌 공장 보관함이 가득 찼습니다"),
				3.0f,
				ENotificationType::Warning);
		}
		PlayBounce();
	}
}

void ABrickFactory::UpdateProductionValues()
{
	// 현재 업그레이드 레벨에 따라 값 재계산
	for (auto& Pair : FactoryData.UpgradeData)
	{
		EFactoryUpgradeType Type = Pair.Key;
		FFactoryUpgradeData& Data = Pair.Value;

		Data.CurrentValue = FFactoryUpgradeConfig::CalculateUpgradeValue(Type, Data.Level);
	}
}

FFactorySaveData ABrickFactory::GetFactoryData() const
{
	return FactoryData;
}

void ABrickFactory::SetFactoryData(const FFactorySaveData& InData)
{
	FactoryData = InData;
	UpdateProductionValues();

	// 로드된 세이브가 이미 HQ 조건을 넘겼는데 잠겨 있으면 조용히 해금
	EvaluateAutoCollectionUnlock(false);

	RestartAutoCollectionTimer();

	UE_LOG(LogTemp, Log, TEXT("[BrickFactory] Factory data loaded for %s"), *FactoryID.ToString());
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
	MainMeshComponent->SetRelativeScale3D(FVector(Pulse));
}

void ABrickFactory::Wooble_TimelineFinished()
{
	// Timeline 종료 시 Scale을 원래대로 복원
	if (MainMeshComponent)
	{
		MainMeshComponent->SetRelativeScale3D(FVector(1.0f));
	}
	SetActorTickEnabled(false);
}

void ABrickFactory::StartSmokeVFX()
{
	if (!BrickSmokeVFX)
	{
		return;
	}

	// 스폰 위치 결정
	FVector SpawnLocation = GetActorLocation();
	if (SpawnPoints.Num() > 0 && SpawnPoints[0])
	{
		SpawnLocation = SpawnPoints[0]->GetComponentLocation();
	}

	if (!CachedSmokeVFX)
	{
		CachedSmokeVFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			BrickSmokeVFX,
			SpawnLocation,
			FRotator::ZeroRotator,
			FVector(1.0f),
			false,
			true,
			ENCPoolMethod::None,
			true
		);

		if (CachedSmokeVFX)
		{
			CachedSmokeVFX->SetFloatParameter(FName("Duration"), 1.0f);
			CachedSmokeVFX->SetFloatParameter(FName("Scale"), 10.0f);
		}
	}
	else
	{
		CachedSmokeVFX->SetWorldLocation(SpawnLocation);
		CachedSmokeVFX->Activate(true);
	}

	StartSteamSound(SpawnLocation);
	StartAmbienceSound(SpawnLocation);
}

void ABrickFactory::StopSmokeVFX()
{
	if (CachedSmokeVFX)
	{
		CachedSmokeVFX->Deactivate();
	}

	StopSteamSound();
	StopAmbienceSound();
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
