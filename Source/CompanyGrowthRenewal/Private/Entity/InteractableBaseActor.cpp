// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/InteractableBaseActor.h"

#include "Manager/EntityManager.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"

#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetMathLibrary.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"

#include "Core/CGGameInstance.h"
#include "Table/InteractableInfo.h"

AInteractableBaseActor::AInteractableBaseActor()
{
	// Tick은 Wobble Timeline 진행 시에만 켜짐 (PlayWobble/PlayBounce에서 enable, TimelineFinished에서 disable)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);

	// Static Mesh 컴포넌트 생성 및 기본 콜리전 프로파일 설정
	MainMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MainMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	MainMeshComponent->SetGenerateOverlapEvents(true);
	MainMeshComponent->SetCanEverAffectNavigation(true);

	// Overlap 영역 검사용 Box 컴포넌트 생성
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);

	// Box Component의 콜리전 응답 채널 세팅
	FCollisionResponseContainer collisionResponse;
	collisionResponse.SetAllChannels(ECR_Ignore);

	// 클릭 감지용: GameTraceChannel1 (에디터에서 "BuildingClick"으로 이름 설정)
	collisionResponse.SetResponse(ECC_GameTraceChannel1, ECR_Block);

	// 건물끼리 Overlap 체크 (배치 가능 여부 판단)
	collisionResponse.SetResponse(ECC_WorldDynamic, ECR_Overlap);

	// PlayerCamera Collision과의 Overlap (PC 모드용)
	collisionResponse.SetResponse(ECC_Pawn, ECR_Overlap);

	BoxComponent->SetCollisionResponseToChannels(collisionResponse);

	// NavBlocker
	NavBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("NavBlocker"));
	NavBlocker->SetupAttachment(MainMeshComponent);

	NavBlocker->PrimaryComponentTick.bCanEverTick = false;
	NavBlocker->SetMobility(EComponentMobility::Movable);
	NavBlocker->SetShouldUpdatePhysicsVolume(true);
	NavBlocker->SetCanEverAffectNavigation(true);
	NavBlocker->bDynamicObstacle = true;

	// NavMesh 설정
	NavBlocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NavBlocker->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	NavBlocker->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);

	// NavBlocker 시각화 설정
	NavBlocker->SetHiddenInGame(true);          // 게임에선 숨기기
	NavBlocker->SetVisibility(true);             // 가시성 활성화
	NavBlocker->ShapeColor = FColor::Red;        // 빨간색으로 표시
	NavBlocker->SetLineThickness(3.0f);          // 선 두께 설정

	// 디버그용 렌더링 활성화
	NavBlocker->bDrawOnlyIfSelected = false;     // 선택하지 않아도 보이게
	NavBlocker->SetCollisionProfileName(TEXT("OverlapAll")); // 충돌 프로파일 설정

	// Root에 붙이기
	MainMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	BoxComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	// 생성자에서 필요한 것들 로드
	static ConstructorHelpers::FObjectFinder<UCurveFloat> Wooble_CurveObj(
		TEXT("/Script/Engine.CurveFloat'/Game/CompanyGrowth/Resources/Curve/Woodle_Curve.Woodle_Curve'")
	);
	if (Wooble_CurveObj.Succeeded())

	{
		Wooble_Curve = Wooble_CurveObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterial> ShapeDrawMatObj(
		TEXT("/Game/CompanyGrowth/Environment/Materials/M_ShapeDraw.M_ShapeDraw")
	);
	if (ShapeDrawMatObj.Succeeded())
	{
		ShapeDrawMaterial = ShapeDrawMatObj.Object;
	}
}

void AInteractableBaseActor::SetInteractableInfo(const FInteractableInfo& InInfo)
{
	InteractableRowName = InInfo.RowName;  // RowName 저장 (예: "Building_1")
	Name = InInfo.Name;
	InteractableType = InInfo.InteractableType;

	BoundGap = InInfo.BoundGap;

	UCGGameInstance* CGGameInstance = GetGameInstance<UCGGameInstance>();
	EntityManager = GetWorld()->GetSubsystem<UEntityManager>();

	UE_LOG(LogTemp, Warning, TEXT("[InteractableBase] SetInteractableInfo - World: %s, EntityManager: %p"),
		GetWorld() ? *GetWorld()->GetName() : TEXT("NULL"), EntityManager);

	RegisterWithEntityManager();

	InitializeStaticMesh(InteractableRowName);

	if (const UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				InitializeWithInteractableInfo();
			}));
	}
}


void AInteractableBaseActor::InitializeWithInteractableInfo()
{
	ReCalcBoxExtent();

	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				CheckOverlappingActor();

				// Placement Mode일경우 EntityManager에 등록하지 않음
				const FName targetTag = TEXT("PlacementMode");
				if (ActorHasTag(targetTag))
				{
					UnregisterWithEntityManager();
				}
			}));
	}
}

void AInteractableBaseActor::CheckOverlappingActor()
{
	// Check if actor is overlapped with other actors
	// @ CHECK : Remove when using PCG
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, StaticClass());
	const FName targetTag = TEXT("PlacementMode");
	bool isDestroy = false;
	for (const AActor* actor : OverlappingActors)
	{
		if (actor->ActorHasTag(targetTag) == false)
		{
			const bool isOverlapped = FVector::Distance(GetActorLocation(), actor->GetActorLocation()) < 5;
			if (isOverlapped)
			{
				isDestroy = true;
				break;
			}
		}
	}

	if (isDestroy)
	{
		Destroy();
	}
}

// Called when the game starts or when spawned
void AInteractableBaseActor::BeginPlay()
{
	Super::BeginPlay();

	FOnTimelineFloat TimelineProgress;
	TimelineProgress.BindUFunction(this, FName("Wooble_TimelineUpdate"));
	FOnTimelineEvent TimelineFinished;
	TimelineFinished.BindUFunction(this, FName("Wooble_TimelineFinished"));

	Wooble_Timeline.AddInterpFloat(Wooble_Curve, TimelineProgress);
	Wooble_Timeline.SetTimelineFinishedFunc(TimelineFinished);

	// 초기에 NavMesh 비/활성화
	UpdateNavBlockerState();
}

void AInteractableBaseActor::Wooble_TimelineUpdate(float scaleValue)
{
	MainMeshComponent->SetScalarParameterValueOnMaterials("Wobble", scaleValue);
}

void AInteractableBaseActor::Wooble_TimelineFinished()
{
	MainMeshComponent->SetScalarParameterValueOnMaterials("Wobble", 0.f);
	SetActorTickEnabled(false);
}

void AInteractableBaseActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Wooble_Timeline.IsPlaying())
	{
		Wooble_Timeline.TickTimeline(DeltaSeconds);
	}
}

void AInteractableBaseActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ReCalcBoxExtent();
}

void AInteractableBaseActor::PlayWobble()
{
	FVector temVec = GetActorLocation();
	temVec.Normalize();

	FVector actorCenter = GetActorLocation();  // Actor 중심점

	MainMeshComponent->SetVectorParameterValueOnMaterials("Wobble Vector", temVec);
	MainMeshComponent->SetVectorParameterValueOnMaterials("Wobble Center", actorCenter);  // 추가

	Wooble_Timeline.PlayFromStart();
	SetActorTickEnabled(true);
}

void AInteractableBaseActor::EndWooble()
{
	MainMeshComponent->SetVectorParameterValueOnMaterials("Wobble Vector", FVector(0, 0, 0));
	// 타임라인 루프 해제하고 중지
	Wooble_Timeline.SetLooping(false);
	Wooble_Timeline.Stop();
	SetActorTickEnabled(false);
}

void AInteractableBaseActor::ReCalcBoxExtent() const
{
	if (MainMeshComponent == nullptr || BoxComponent == nullptr)
	{
		return;
	}

	FVector boundsMin, boundsMax;
	MainMeshComponent->GetLocalBounds(boundsMin, boundsMax);

	boundsMax /= 100;
	boundsMax.X = FMath::RoundToFloat(boundsMax.X);
	boundsMax.Y = FMath::RoundToFloat(boundsMax.Y);
	boundsMax.Z = FMath::RoundToFloat(boundsMax.Z);
	boundsMax *= 100; //Step
	// 각 축을 독립적으로 처리 (직사각형 건물 지원)
	boundsMax.X = std::max(boundsMax.X, 100.0);
	boundsMax.Y = std::max(boundsMax.Y, 100.0);
	boundsMax.Z = std::max(boundsMax.Z, 100.0);

	float boundGap = BoundGap * 100;
	boundsMax += FVector(boundGap, boundGap, boundGap);

	BoxComponent->SetBoxExtent(boundsMax);
	BoxComponent->SetWorldRotation(UKismetMathLibrary::MakeRotFromX(FVector(1, 0, 0)));

	if (NavBlocker)
	{
		NavBlocker->SetBoxExtent(boundsMax);
	}

	if (ReCalcBoxExtentDelegate != nullptr)
	{
		ReCalcBoxExtentDelegate->Broadcast();
	}
}

void AInteractableBaseActor::CalcTemporaryDistanceFromLocation(FVector InLocation)
{
	DistSquared = FVector::DistSquared(GetActorLocation(), InLocation);
}

void AInteractableBaseActor::UpdateNavBlockerState()
{
	if (NavBlocker)
	{
		// Building 태그뿐만 아니라 다른 조건들도 확인
		bool bShouldAffectNav = Tags.Contains("Building") || Tags.Contains("Obstacle") ||
			Tags.Contains("Interactable");
		// PlacementMode일 때는 NavMesh 영향 끄기
		if (Tags.Contains("PlacementMode"))
		{
			bShouldAffectNav = false;
		}
		NavBlocker->SetCanEverAffectNavigation(bShouldAffectNav);
	}
}

void AInteractableBaseActor::DetachNavBlockerForMove()
{
	if (NavBlocker)
	{
		NavBlocker->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AInteractableBaseActor::AttachNavBlockerToNewLocation()
{
	if (NavBlocker && MainMeshComponent)
	{
		NavBlocker->SetWorldLocation(GetActorLocation());
		NavBlocker->AttachToComponent(MainMeshComponent,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}