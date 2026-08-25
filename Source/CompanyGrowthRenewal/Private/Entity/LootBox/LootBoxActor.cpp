// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/LootBox/LootBoxActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Table/LootBoxData.h"
#include "Table/BuildingSkinData.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Sound/SoundBase.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DataTable.h"

ALootBoxActor::ALootBoxActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	// 닫힌 상자 메시
	ClosedMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClosedMesh"));
	ClosedMeshComponent->SetupAttachment(RootScene);
	ClosedMeshComponent->SetVisibility(true);

	// 열린 상자 메시
	OpenedMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OpenedMesh"));
	OpenedMeshComponent->SetupAttachment(RootScene);
	OpenedMeshComponent->SetVisibility(false);

	// 보상 프리뷰용 Sphere (처음엔 숨김)
	RewardSphereComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RewardSphere"));
	RewardSphereComponent->SetupAttachment(RootScene);
	RewardSphereComponent->SetVisibility(false);
	RewardSphereComponent->SetRelativeScale3D(FVector(0.1f)); // 작은 크기로 시작

	// Sphere Mesh 설정 (기본 구체)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		RewardSphereComponent->SetStaticMesh(SphereMeshFinder.Object);
	}

	// Spot Light 설정 (Sphere를 밝게 비춤)
	RewardLightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("RewardLight"));
	RewardLightComponent->SetupAttachment(RootScene); // RootScene에 붙임 (회전 독립적)
	RewardLightComponent->SetRelativeLocation(FVector(200.0f, 0.0f, 53.0f)); // 초기 위치 (Sphere가 6,6,50일 때 기준)
	RewardLightComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // -X 방향 (Sphere를 향하도록)
	RewardLightComponent->SetIntensityUnits(ELightUnits::Candelas); // 광도 단위를 Candelas로 설정
	RewardLightComponent->SetIntensity(3.0f); // 밝기 (Candelas)
	RewardLightComponent->SetAttenuationRadius(500.0f); // 영향 범위
	RewardLightComponent->SetLightColor(FLinearColor::White); // 순백색
	RewardLightComponent->SetInnerConeAngle(30.0f); // 내부 원뿔 각도 (집중된 중심)
	RewardLightComponent->SetOuterConeAngle(50.0f); // 외부 원뿔 각도 (넓은 페이드)
	RewardLightComponent->SetSourceRadius(0.0f); // 광원 반경 (0 = 점광원)
	RewardLightComponent->SetSoftSourceRadius(1000.0f); // 부드러운 광원 반경 (그림자 소프트닝)
	RewardLightComponent->SetCastShadows(false); // 성능 최적화
	RewardLightComponent->SetVisibility(false); // 초기엔 꺼짐

	// VFX 컴포넌트들
	ClosedVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ClosedVFX"));
	ClosedVFXComponent->SetupAttachment(RootScene);
	ClosedVFXComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	ClosedVFXComponent->bAutoActivate = true;

	OpenVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("OpenVFX"));
	OpenVFXComponent->SetupAttachment(RootScene);
	OpenVFXComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	OpenVFXComponent->bAutoActivate = false;

	OpenedVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("OpenedVFX"));
	OpenedVFXComponent->SetupAttachment(RootScene);
	OpenedVFXComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	OpenedVFXComponent->bAutoActivate = false;

	PillarVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PillarVFX"));
	PillarVFXComponent->SetupAttachment(RootScene);
	PillarVFXComponent->bAutoActivate = false;

	// 오디오
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	AudioComponent->SetupAttachment(RootScene);
	AudioComponent->bAutoActivate = false;

	// Curve 에셋 로드
	static ConstructorHelpers::FObjectFinder<UCurveFloat> PositionCurveFinder
		(TEXT("/Script/Engine.CurveFloat'/Game/CompanyGrowth/Resources/Curve/LootBox_Position.LootBox_Position'"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> ScaleCurveFinder
		(TEXT("/Script/Engine.CurveFloat'/Game/CompanyGrowth/Resources/Curve/LootBox_Scale.LootBox_Scale'"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> AlphaCurveFinder
		(TEXT("/Script/Engine.CurveFloat'/Game/CompanyGrowth/Resources/Curve/LootBox_Alpha.LootBox_Alpha'"));

	if (PositionCurveFinder.Succeeded())
	{
		PositionCurve = PositionCurveFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] Failed to load PositionCurve"));
	}

	if (ScaleCurveFinder.Succeeded())
	{
		ScaleCurve = ScaleCurveFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] Failed to load ScaleCurve"));
	}

	if (AlphaCurveFinder.Succeeded())
	{
		AlphaCurve = AlphaCurveFinder.Object;
	}
}

void ALootBoxActor::BeginPlay()
{
	Super::BeginPlay();

	// 자동으로 태그 추가 (GameMode에서 찾을 수 있도록)
	Tags.AddUnique(FName("MainLootBox"));

	// TableManager 가져오기
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	}

	// 런타임 시작 시 항상 Closed 상태로 초기화
	ClosedMeshComponent->SetVisibility(true);
	OpenedMeshComponent->SetVisibility(false);
	RewardSphereComponent->SetVisibility(false);
	if (RewardLightComponent)
	{
		RewardLightComponent->SetVisibility(false);
	}

	// 시작 시 비주얼 설정 적용 (메시/VFX 에셋만 로드)
	ApplyVisualConfig();

	// 에디터 Preview 상태와 무관하게 런타임에서는 항상 Closed VFX만 활성화
	// (에디터에서 Opened 프리뷰 시 활성화된 VFX를 명시적으로 정리)
	if (ClosedVFXComponent)
	{
		ClosedVFXComponent->SetVisibility(true);
		ClosedVFXComponent->Activate(true);
	}
	if (OpenVFXComponent)
	{
		OpenVFXComponent->DeactivateImmediate(); // 즉시 제거
		OpenVFXComponent->SetVisibility(false);
	}
	if (OpenedVFXComponent)
	{
		OpenedVFXComponent->DeactivateImmediate(); // 즉시 제거
		OpenedVFXComponent->SetVisibility(false);
	}
	if (PillarVFXComponent)
	{
		PillarVFXComponent->DeactivateImmediate();
		PillarVFXComponent->SetVisibility(false);
	}
}

void ALootBoxActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 애니메이션 업데이트
	if (bIsAnimating)
	{
		UpdateAnimation(DeltaTime);
	}

	// 입력 대기 중일 때 Sphere 부드럽게 회전 (지구본처럼)
	if (bWaitingForInput && RewardSphereComponent)
	{
		FRotator RotationDelta(0.0f, 45.0f * DeltaTime, 0.0f); // 초당 45도 (8초/바퀴)
		RewardSphereComponent->AddRelativeRotation(RotationDelta);
	}

	// SpotLight의 Z축(높이)만 Sphere를 따라가도록 업데이트 (X, Y, 회전은 고정)
	if ((bIsAnimating || bWaitingForInput) && RewardSphereComponent && RewardLightComponent)
	{
		FVector SphereLocation = RewardSphereComponent->GetRelativeLocation();
		// Z축만 Sphere를 따라감, X와 Y는 고정 위치 유지 (X=200, Y=0)
		FVector LightLocation = FVector(200.0f, 0.0f, SphereLocation.Z + 3.0f);
		RewardLightComponent->SetRelativeLocation(LightLocation);
	}
}

#if WITH_EDITOR
void ALootBoxActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// Rarity 또는 ChestType 변경 시 LootBoxRowName 자동 생성
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ALootBoxActor, Rarity) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ALootBoxActor, ChestType))
	{
		// ChestType 문자열 생성
		FString TypeStr = (ChestType == ELootBoxType::Square) ? TEXT("Square") : TEXT("Sphere");

		// Rarity 문자열 생성
		FString RarityStr;
		switch (Rarity)
		{
		case ELootBoxRarity::Common:
			RarityStr = TEXT("Common");
			break;
		case ELootBoxRarity::Unusual:
			RarityStr = TEXT("Unusual");
			break;
		case ELootBoxRarity::Rare:
			RarityStr = TEXT("Rare");
			break;
		case ELootBoxRarity::Epic:
			RarityStr = TEXT("Epic");
			break;
		case ELootBoxRarity::Legendary:
			RarityStr = TEXT("Legendary");
			break;
		case ELootBoxRarity::Mythic:
			RarityStr = TEXT("Mythic");
			break;
		default:
			RarityStr = TEXT("Common");
			break;
		}

		// "Square_Epic" 형태로 RowName 조합
		LootBoxRowName = FName(*(TypeStr + TEXT("_") + RarityStr));

		// 비주얼 업데이트
		ApplyVisualConfig();
	}
	// RowName 직접 변경 시에도 프리뷰 업데이트
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ALootBoxActor, LootBoxRowName))
	{
		ApplyVisualConfig();
	}
	// PreviewState 변경 시 (Closed/Opened 상태 전환)
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ALootBoxActor, PreviewState))
	{
		ApplyVisualConfig();
	}
}
#endif

void ALootBoxActor::SetLootBoxVisual(FName LootBoxID)
{
	if (LootBoxID == NAME_None || LootBoxID == LootBoxRowName)
	{
		return; // 동일한 상자면 변경 안 함
	}

	// LootBoxRowName 변경
	LootBoxRowName = LootBoxID;

	// 비주얼 다시 적용
	ApplyVisualConfig();

	// 상자가 열려있지 않으면 닫힌 상태로 리셋
	if (!bIsOpened)
	{
		ClosedMeshComponent->SetVisibility(true);
		OpenedMeshComponent->SetVisibility(false);
		RewardSphereComponent->SetVisibility(false);
	}
}

void ALootBoxActor::ApplyVisualConfig()
{
	// TableManager가 없으면 가져오기 (에디터/런타임 모두 대응)
	if (!TableManager)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			UGameInstance* GameInstance = World->GetGameInstance();
			if (GameInstance)
			{
				TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
			}
		}
	}

#if WITH_EDITOR
	// 에디터에서 GameInstance 없을 때: DataTable 직접 로드
	if (!TableManager)
	{
		// DataTable 직접 로드
		static UDataTable* LootBoxTable = nullptr;
		if (!LootBoxTable)
		{
			LootBoxTable = LoadObject<UDataTable>(nullptr,
				TEXT("/Game/CompanyGrowth/Table/DT_LootBox.DT_LootBox"));
		}

		if (LootBoxTable && LootBoxRowName != NAME_None)
		{
			FLootBoxTable* RowData = LootBoxTable->FindRow<FLootBoxTable>(LootBoxRowName, TEXT(""));
			if (RowData)
			{
				CurrentConfig = *RowData;

				// 비주얼 직접 적용
				ApplyVisualConfigDirect();
				return;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] RowName '%s' not found in DataTable"),
					*LootBoxRowName.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] Failed to load DT_LootBox in Editor mode"));
		}
		return;
	}
#endif

	if (!TableManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("LootBoxActor: TableManager is still nullptr! Visual config cannot be applied."));
		return;
	}

	if (LootBoxRowName == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("LootBoxActor: LootBoxRowName is not set!"));
		return;
	}

	// TableManager에서 설정 가져오기
	bool bSuccess = false;
	CurrentConfig = TableManager->GetLootBoxData(LootBoxRowName, bSuccess);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("LootBoxActor: Failed to get config for RowName='%s'"), *LootBoxRowName.ToString());
		return;
	}

	// 메시 설정 (TSoftObjectPtr 사용하므로 LoadSynchronous 필요)
	if (CurrentConfig.ClosedMesh.ToSoftObjectPath().IsValid())
	{
		UStaticMesh* LoadedMesh = CurrentConfig.ClosedMesh.LoadSynchronous();
		if (LoadedMesh)
		{
			ClosedMeshComponent->SetStaticMesh(LoadedMesh);
		}
	}

	if (CurrentConfig.OpenedMesh.ToSoftObjectPath().IsValid())
	{
		UStaticMesh* LoadedMesh = CurrentConfig.OpenedMesh.LoadSynchronous();
		if (LoadedMesh)
		{
			OpenedMeshComponent->SetStaticMesh(LoadedMesh);
		}
	}

	// 재질 설정
	if (CurrentConfig.ChestMaterial.ToSoftObjectPath().IsValid())
	{
		UMaterialInterface* LoadedMaterial = CurrentConfig.ChestMaterial.LoadSynchronous();
		if (LoadedMaterial)
		{
			ClosedMeshComponent->SetMaterial(0, LoadedMaterial);
			OpenedMeshComponent->SetMaterial(0, LoadedMaterial);
		}
	}

	// VFX 설정
	if (CurrentConfig.ClosedVFX.ToSoftObjectPath().IsValid())
	{
		UNiagaraSystem* LoadedVFX = CurrentConfig.ClosedVFX.LoadSynchronous();
		if (LoadedVFX)
		{
			ClosedVFXComponent->SetAsset(LoadedVFX);
			ClosedVFXComponent->Activate(true);
		}
	}

	if (CurrentConfig.OpenVFX.ToSoftObjectPath().IsValid())
	{
		UNiagaraSystem* LoadedVFX = CurrentConfig.OpenVFX.LoadSynchronous();
		if (LoadedVFX)
		{
			OpenVFXComponent->SetAsset(LoadedVFX);
		}
	}

	if (CurrentConfig.OpenedVFX.ToSoftObjectPath().IsValid())
	{
		UNiagaraSystem* LoadedVFX = CurrentConfig.OpenedVFX.LoadSynchronous();
		if (LoadedVFX)
		{
			OpenedVFXComponent->SetAsset(LoadedVFX);
		}
	}

	// PillarVFX 설정 (이전 VFX 먼저 정리!)
	if (PillarVFXComponent)
	{
		// 1. 이전 VFX 완전히 제거
		PillarVFXComponent->DeactivateImmediate();
		PillarVFXComponent->SetVisibility(false);
		PillarVFXComponent->SetAsset(nullptr); // 이전 에셋 제거

		// 2. 새 VFX 설정 (있는 경우에만)
		if (CurrentConfig.PillarVFX.ToSoftObjectPath().IsValid())
		{
			UNiagaraSystem* LoadedVFX = CurrentConfig.PillarVFX.LoadSynchronous();
			if (LoadedVFX)
			{
				PillarVFXComponent->SetAsset(LoadedVFX);
			}
		}
	}

	// 사운드 설정
	if (CurrentConfig.OpenSound.ToSoftObjectPath().IsValid())
	{
		USoundBase* LoadedSound = CurrentConfig.OpenSound.LoadSynchronous();
		if (LoadedSound)
		{
			AudioComponent->SetSound(LoadedSound);
		}
	}

#if WITH_EDITOR
	// 에디터에서 PreviewState에 따라 메시 및 VFX 가시성 제어
	if (PreviewState == ELootBoxPreviewState::Closed)
	{
		// Closed 상태: 닫힌 메시만 표시
		ClosedMeshComponent->SetVisibility(true);
		OpenedMeshComponent->SetVisibility(false);
		RewardSphereComponent->SetVisibility(false);
		RewardLightComponent->SetVisibility(false);

		// VFX: Closed만 활성화 (나머지는 완전히 끄기)
		if (ClosedVFXComponent)
		{
			ClosedVFXComponent->SetVisibility(true);
			ClosedVFXComponent->Activate(true);
		}
		if (OpenVFXComponent)
		{
			OpenVFXComponent->Deactivate();
			OpenVFXComponent->SetVisibility(false);
		}
		if (OpenedVFXComponent)
		{
			OpenedVFXComponent->Deactivate();
			OpenedVFXComponent->SetVisibility(false);
		}
		if (PillarVFXComponent)
		{
			PillarVFXComponent->Deactivate();
			PillarVFXComponent->SetVisibility(false);
		}
	}
	else // ELootBoxPreviewState::Opened
	{
		// Opened 상태: 열린 메시 + 보상 Sphere 표시
		ClosedMeshComponent->SetVisibility(false);
		OpenedMeshComponent->SetVisibility(true);
		RewardSphereComponent->SetVisibility(true);
		RewardLightComponent->SetVisibility(true);

		// Sphere 위치 및 크기 설정 (완성된 상태)
		RewardSphereComponent->SetRelativeLocation(FVector(6.0f, 6.0f, 200.0f));
		RewardSphereComponent->SetRelativeScale3D(FVector(1.0f));

		// SpotLight 위치 (X=200, Y=0 고정, Z만 Sphere 높이 따라감)
		RewardLightComponent->SetRelativeLocation(FVector(200.0f, 0.0f, 200.0f + 3.0f));

		// VFX: Opened 관련 활성화 (나머지는 완전히 끄기)
		if (ClosedVFXComponent)
		{
			ClosedVFXComponent->Deactivate();
			ClosedVFXComponent->SetVisibility(false);
		}
		if (OpenVFXComponent)
		{
			OpenVFXComponent->Deactivate();
			OpenVFXComponent->SetVisibility(false);
		}
		if (OpenedVFXComponent)
		{
			OpenedVFXComponent->SetVisibility(true);
			OpenedVFXComponent->Activate(true);
		}
		if (PillarVFXComponent)
		{
			PillarVFXComponent->SetVisibility(true);
			PillarVFXComponent->Activate(true);
		}
	}
#endif
}

void ALootBoxActor::ApplyVisualConfigDirect()
{
	// CurrentConfig를 사용해서 직접 비주얼 적용 (에디터 전용)
	// 메시 설정
	if (CurrentConfig.ClosedMesh.ToSoftObjectPath().IsValid())
	{
		UStaticMesh* LoadedMesh = CurrentConfig.ClosedMesh.LoadSynchronous();
		if (LoadedMesh)
		{
			ClosedMeshComponent->SetStaticMesh(LoadedMesh);
		}
	}

	if (CurrentConfig.OpenedMesh.ToSoftObjectPath().IsValid())
	{
		UStaticMesh* LoadedMesh = CurrentConfig.OpenedMesh.LoadSynchronous();
		if (LoadedMesh)
		{
			OpenedMeshComponent->SetStaticMesh(LoadedMesh);
		}
	}

	// 재질 설정
	if (CurrentConfig.ChestMaterial.ToSoftObjectPath().IsValid())
	{
		UMaterialInterface* LoadedMaterial = CurrentConfig.ChestMaterial.LoadSynchronous();
		if (LoadedMaterial)
		{
			ClosedMeshComponent->SetMaterial(0, LoadedMaterial);
			OpenedMeshComponent->SetMaterial(0, LoadedMaterial);
		}
	}

	// VFX 설정
	if (CurrentConfig.ClosedVFX.ToSoftObjectPath().IsValid())
	{
		UNiagaraSystem* LoadedVFX = CurrentConfig.ClosedVFX.LoadSynchronous();
		if (LoadedVFX)
		{
			ClosedVFXComponent->SetAsset(LoadedVFX);
			ClosedVFXComponent->Activate(true);
		}
	}

	if (CurrentConfig.OpenVFX.ToSoftObjectPath().IsValid())
	{
		UNiagaraSystem* LoadedVFX = CurrentConfig.OpenVFX.LoadSynchronous();
		if (LoadedVFX)
		{
			OpenVFXComponent->SetAsset(LoadedVFX);
		}
	}

	if (CurrentConfig.OpenedVFX.ToSoftObjectPath().IsValid())
	{
		UNiagaraSystem* LoadedVFX = CurrentConfig.OpenedVFX.LoadSynchronous();
		if (LoadedVFX)
		{
			OpenedVFXComponent->SetAsset(LoadedVFX);
		}
	}

	// PillarVFX 설정 (이전 VFX 먼저 정리!)
	if (PillarVFXComponent)
	{
		// 1. 이전 VFX 완전히 제거
		PillarVFXComponent->DeactivateImmediate();
		PillarVFXComponent->SetVisibility(false);
		PillarVFXComponent->SetAsset(nullptr); // 이전 에셋 제거

		// 2. 새 VFX 설정 (있는 경우에만)
		if (CurrentConfig.PillarVFX.ToSoftObjectPath().IsValid())
		{
			UNiagaraSystem* LoadedVFX = CurrentConfig.PillarVFX.LoadSynchronous();
			if (LoadedVFX)
			{
				PillarVFXComponent->SetAsset(LoadedVFX);
			}
		}
	}

	// 사운드 설정
	if (CurrentConfig.OpenSound.ToSoftObjectPath().IsValid())
	{
		USoundBase* LoadedSound = CurrentConfig.OpenSound.LoadSynchronous();
		if (LoadedSound)
		{
			AudioComponent->SetSound(LoadedSound);
		}
	}

	// PreviewState에 따라 메시 및 VFX 가시성 제어
	if (PreviewState == ELootBoxPreviewState::Closed)
	{
		// Closed 상태: 닫힌 메시만 표시
		ClosedMeshComponent->SetVisibility(true);
		OpenedMeshComponent->SetVisibility(false);
		RewardSphereComponent->SetVisibility(false);
		RewardLightComponent->SetVisibility(false);

		// VFX: Closed만 활성화 (나머지는 완전히 끄기)
		ClosedVFXComponent->SetVisibility(true);
		ClosedVFXComponent->Activate(true);

		OpenVFXComponent->Deactivate();
		OpenVFXComponent->SetVisibility(false);

		OpenedVFXComponent->Deactivate();
		OpenedVFXComponent->SetVisibility(false);

		PillarVFXComponent->Deactivate();
		PillarVFXComponent->SetVisibility(false);
	}
	else // ELootBoxPreviewState::Opened
	{
		// Opened 상태: 열린 메시 + 보상 Sphere 표시
		ClosedMeshComponent->SetVisibility(false);
		OpenedMeshComponent->SetVisibility(true);
		RewardSphereComponent->SetVisibility(true);
		RewardLightComponent->SetVisibility(true);

		// Sphere 위치 및 크기 설정 (완성된 상태)
		RewardSphereComponent->SetRelativeLocation(FVector(6.0f, 6.0f, 200.0f)); // 적절한 높이
		RewardSphereComponent->SetRelativeScale3D(FVector(1.0f)); // 최대 크기

		// SpotLight 위치 (X=200, Y=0 고정, Z만 Sphere 높이 따라감)
		RewardLightComponent->SetRelativeLocation(FVector(200.0f, 0.0f, 200.0f + 3.0f));

		// VFX: Opened 관련 활성화 (나머지는 완전히 끄기)
		ClosedVFXComponent->Deactivate();
		ClosedVFXComponent->SetVisibility(false);

		OpenVFXComponent->Deactivate(); // 순간 효과는 비활성화
		OpenVFXComponent->SetVisibility(false);

		OpenedVFXComponent->SetVisibility(true);
		OpenedVFXComponent->Activate(true);

		PillarVFXComponent->SetVisibility(true);
		PillarVFXComponent->Activate(true);
	}
}

void ALootBoxActor::OpenLootBox(int32 RewardSkinID)
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxActor] Opening LootBox with RewardSkinID: %d"), RewardSkinID);

	// 상자를 닫힌 상태로 리셋 (이전 애니메이션 정리)
	if (bIsOpened || bIsAnimating)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxActor] Resetting previous animation state"));
		bIsAnimating = false;
		AnimationTime = 0.0f;
	}

	bIsOpened = true;
	CurrentRewardSkinID = RewardSkinID;

	// 닫힌 상자 숨기기
	ClosedMeshComponent->SetVisibility(false);
	ClosedVFXComponent->Deactivate();

	// 열린 상자 표시
	OpenedMeshComponent->SetVisibility(true);
	OpenVFXComponent->Activate(true);

	// 사운드 재생
	if (AudioComponent && AudioComponent->Sound)
	{
		AudioComponent->Play();
	}

	// RewardSphere에 Material 적용
	if (TableManager && RewardSphereComponent)
	{
		bool bSuccess = false;
		FBuildingSkinData SkinData = TableManager->GetBuildingSkinData(RewardSkinID, bSuccess);
		if (bSuccess && SkinData.SkinMaterial.ToSoftObjectPath().IsValid())
		{
			UMaterialInterface* Material = SkinData.SkinMaterial.LoadSynchronous();
			if (Material)
			{
				RewardSphereComponent->SetMaterial(0, Material);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] Failed to load material"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] Failed to get BuildingSkinData for ID: %d"), RewardSkinID);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] TableManager or RewardSphereComponent is null"));
	}

	// RewardSphere 초기화 (위치, 크기)
	if (RewardSphereComponent)
	{
		RewardSphereComponent->SetRelativeLocation(FVector(6.0f, 6.0f, 50.0f));
		RewardSphereComponent->SetRelativeScale3D(FVector(0.1f));
		RewardSphereComponent->SetVisibility(true);
	}

	// Spot Light 켜기 (Sphere를 밝게 비춤)
	if (RewardLightComponent)
	{
		// SpotLight의 Z축만 Sphere 높이를 따라감 (X=200, Y=0 고정)
		FVector SphereLocation = RewardSphereComponent->GetRelativeLocation();
		FVector LightLocation = FVector(200.0f, 0.0f, SphereLocation.Z + 3.0f);
		RewardLightComponent->SetRelativeLocation(LightLocation);
		RewardLightComponent->SetVisibility(true);
	}

	// 애니메이션 시작
	bIsAnimating = true;
	AnimationTime = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[LootBoxActor] Opening animation started"));
}

void ALootBoxActor::UpdateAnimation(float DeltaTime)
{
	AnimationTime += DeltaTime;

	const float ShakeDuration = CurrentConfig.ShakeDuration;

	// Reward Sphere Curve의 최대 시간을 전체 애니메이션 길이로 사용
	float RewardAnimDuration = 3.0f; // 기본값
	if (PositionCurve || ScaleCurve)
	{
		float MinTime, MaxTime;
		if (PositionCurve)
		{
			PositionCurve->GetTimeRange(MinTime, MaxTime);
			RewardAnimDuration = MaxTime;
		}
		else if (ScaleCurve)
		{
			ScaleCurve->GetTimeRange(MinTime, MaxTime);
			RewardAnimDuration = MaxTime;
		}
	}

	// 전체 지속시간 = Shake + Sphere 애니메이션
	const float TotalDuration = ShakeDuration + RewardAnimDuration;

	// Phase 1: 흔들림 (0.0 ~ ShakeDuration)
	if (AnimationTime <= ShakeDuration)
	{
		float ShakeProgress = AnimationTime / ShakeDuration;
		UpdateShakeAnimation(ShakeProgress);
	}
	// Phase 2: 전환 (ShakeDuration 시점에만 한 번)
	else if (AnimationTime > ShakeDuration && AnimationTime <= ShakeDuration + 0.01f)
	{
		UE_LOG(LogTemp, Log, TEXT("[LootBoxActor] Phase 2: Opening (Duration: %.2fs)"), RewardAnimDuration);

		// 메시 교체
		ClosedMeshComponent->SetVisibility(false);
		OpenedMeshComponent->SetVisibility(true);

		// VFX 전환
		if (ClosedVFXComponent)
		{
			ClosedVFXComponent->Deactivate();
			ClosedVFXComponent->SetVisibility(false);
		}

		if (OpenVFXComponent)
		{
			OpenVFXComponent->SetVisibility(true);
			OpenVFXComponent->Activate(true);
		}

		if (OpenedVFXComponent)
		{
			OpenedVFXComponent->SetVisibility(true);
			OpenedVFXComponent->Activate(true);
		}

		if (PillarVFXComponent)
		{
			PillarVFXComponent->SetVisibility(true);
			PillarVFXComponent->Activate(true);
		}
	}
	// Phase 3: 상자 떠오름 + Sphere 애니메이션 (동시 진행)
	else if (AnimationTime <= TotalDuration)
	{
		// 상자 떠오름 (Sphere와 같은 시간동안 진행)
		float Progress = (AnimationTime - ShakeDuration) / RewardAnimDuration;
		UpdateRiseAnimation(Progress);
	}
	// Phase 4: 완료 (입력 대기)
	else
	{
		if (bIsAnimating) // 한 번만 실행
		{
			bIsAnimating = false;
			bWaitingForInput = true; // 입력 대기 상태
			UE_LOG(LogTemp, Log, TEXT("[LootBoxActor] Animation Complete (%.2fs) - Waiting for input"), AnimationTime);
		}
	}

	// Reward Sphere 애니메이션 (Phase 2 이후부터 시작, Phase 3과 동시 진행)
	if (AnimationTime > ShakeDuration && AnimationTime <= TotalDuration)
	{
		UpdateRewardAnimation(DeltaTime);
	}
}

void ALootBoxActor::UpdateShakeAnimation(float Progress)
{
	// Sin 파형으로 좌우 흔들림
	// Progress: 0 ~ 1
	float Frequency = 20.0f; // 흔들림 빈도
	float ShakeAngle = FMath::Sin(Progress * PI * Frequency) * CurrentConfig.ShakeRotationDegrees;

	// 각도가 점점 줄어들도록 (감쇠)
	float Damping = 1.0f - Progress;
	ShakeAngle *= Damping;

	FRotator NewRotation = FRotator(0.0f, ShakeAngle, 0.0f);
	ClosedMeshComponent->SetRelativeRotation(NewRotation);

	// 위아래 진동 (선택 사항)
	float BounceHeight = FMath::Sin(Progress * PI * 15.0f) * 3.0f * Damping;
	FVector NewLocation = FVector(0.0f, 0.0f, BounceHeight);
	ClosedMeshComponent->SetRelativeLocation(NewLocation);
}

void ALootBoxActor::UpdateRiseAnimation(float Progress)
{
	// Progress: 0 ~ 1
	// Ease Out 곡선 사용
	float EasedProgress = 1.0f - FMath::Pow(1.0f - Progress, 2.0f);

	float RiseHeight = EasedProgress * CurrentConfig.RiseHeight;
	FVector NewLocation = FVector(0.0f, 0.0f, RiseHeight);
	OpenedMeshComponent->SetRelativeLocation(NewLocation);
}

void ALootBoxActor::UpdateRewardAnimation(float DeltaTime)
{
	if (!RewardSphereComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] RewardSphereComponent is null!"));
		return;
	}

	// Reward 애니메이션 시작 시점 기준으로 시간 계산 (ShakeDuration 이후부터)
	float RewardAnimTime = AnimationTime - CurrentConfig.ShakeDuration;

	// Curve가 설정되지 않았으면 기본값 사용
	if (!PositionCurve && !ScaleCurve)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxActor] PositionCurve and ScaleCurve are both null"));
		return;
	}

	// Position: Z축으로 떠오름 + XY offset 유지
	if (PositionCurve)
	{
		float PositionValue = PositionCurve->GetFloatValue(RewardAnimTime);
		FVector NewLocation = FVector(6.0f, 6.0f, 50.0f + PositionValue);
		RewardSphereComponent->SetRelativeLocation(NewLocation);
	}

	// Scale: 커지기
	if (ScaleCurve)
	{
		float ScaleValue = ScaleCurve->GetFloatValue(RewardAnimTime);
		FVector NewScale = FVector(ScaleValue);
		RewardSphereComponent->SetRelativeScale3D(NewScale);
	}

	// Alpha: Fade In (Material Parameter로 적용)
	if (AlphaCurve)
	{
		float AlphaValue = AlphaCurve->GetFloatValue(RewardAnimTime);

		// Dynamic Material Instance를 생성하여 Opacity 파라미터 설정
		UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(RewardSphereComponent->GetMaterial(0));
		if (!DynMaterial)
		{
			// Dynamic Material이 없으면 생성
			UMaterialInterface* BaseMaterial = RewardSphereComponent->GetMaterial(0);
			if (BaseMaterial)
			{
				DynMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
				RewardSphereComponent->SetMaterial(0, DynMaterial);
			}
		}

		if (DynMaterial)
		{
			DynMaterial->SetScalarParameterValue(FName("Opacity"), AlphaValue);
		}
	}
}

void ALootBoxActor::CompleteReward()
{
	if (!bWaitingForInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxActor] CompleteReward called but not waiting for input"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LootBoxActor] User input received - Completing reward"));
	bWaitingForInput = false;

	// 리셋 및 UI 복원
	OnOpenAnimationFinished();
}

void ALootBoxActor::OnOpenAnimationFinished_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxActor] Resetting to closed state"));

	// 델리게이트 브로드캐스트 (UI 복원)
	OnAnimationFinished.Broadcast();

	// 상자를 닫힌 상태로 리셋 (다음 열기를 위해)
	bIsOpened = false;
	bIsAnimating = false;
	bWaitingForInput = false;

	// 비주얼을 닫힌 상태로 되돌림
	ClosedMeshComponent->SetVisibility(true);
	ClosedMeshComponent->SetRelativeLocation(FVector::ZeroVector);
	ClosedMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);

	OpenedMeshComponent->SetVisibility(false);
	OpenedMeshComponent->SetRelativeLocation(FVector::ZeroVector);

	RewardSphereComponent->SetVisibility(false);

	// Spot Light 끄기
	if (RewardLightComponent)
	{
		RewardLightComponent->SetVisibility(false);
	}

	// 오디오 정리
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}

	// VFX 정리 (DeactivateImmediate로 즉시 제거)
	if (OpenVFXComponent)
	{
		OpenVFXComponent->DeactivateImmediate();  // 즉시 제거
	}
	if (OpenedVFXComponent)
	{
		OpenedVFXComponent->DeactivateImmediate();
	}
	if (PillarVFXComponent)
	{
		PillarVFXComponent->DeactivateImmediate();  // 즉시 제거
	}
	if (ClosedVFXComponent)
	{
		ClosedVFXComponent->SetVisibility(true);
		ClosedVFXComponent->Activate(true);
	}
}
