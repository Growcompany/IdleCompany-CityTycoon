#include "Entity/Ambient/AmbientAircraftFlyby.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CGGameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "TimerManager.h"

AAmbientAircraftFlyby::AAmbientAircraftFlyby()
{
	PrimaryActorTick.bStartWithTickEnabled = false;

	Speed = 2000.f;
	bLoop = true;
	StartDistance = 0.f;
	MeshYawOffset = FRotator(0.f, 90.f, 0.f);
	BobAmplitude = 0.f;
	RollAmplitude = 0.f;
	BankAmplitude = 4.f;
	LookAheadDistance = 1000.f;
	MaxYawRate = 60.f;
	MeshComp->SetCastShadow(false);
	Tags.AddUnique(FName(TEXT("Interactable")));

	ClickBoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("ClickBoxComp"));
	ClickBoxComp->SetupAttachment(MeshComp);
	ClickBoxComp->SetVisibility(false);
	ClickBoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClickBoxComp->SetCollisionObjectType(ECC_WorldDynamic);
	ClickBoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickBoxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	ClickBoxComp->SetGenerateOverlapEvents(false);
	ClickBoxComp->SetCanEverAffectNavigation(false);

	RotorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotorMeshComp"));
	RotorMeshComp->SetupAttachment(MeshComp);
	RotorMeshComp->SetMobility(EComponentMobility::Movable);
	RotorMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RotorMeshComp->SetGenerateOverlapEvents(false);
	RotorMeshComp->SetCanEverAffectNavigation(false);
	RotorMeshComp->SetCastShadow(false);

	TailRotorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TailRotorMeshComp"));
	TailRotorMeshComp->SetupAttachment(MeshComp);
	TailRotorMeshComp->SetMobility(EComponentMobility::Movable);
	TailRotorMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TailRotorMeshComp->SetGenerateOverlapEvents(false);
	TailRotorMeshComp->SetCanEverAffectNavigation(false);
	TailRotorMeshComp->SetCastShadow(false);
}

void AAmbientAircraftFlyby::BeginPlay()
{
	Super::BeginPlay();
	MeshComp->SetHiddenInGame(true, true);
	SetActorTickEnabled(false);
	ClickBoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 빈 경로는 베이스 무버가 이미 경고한다.
	if (!Path || Path->GetSplineLength() <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (!Path->IsClosedLoop() || !MeshComp->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AmbientAircraftFlyby] '%s'에 닫힌 Path와 항공기 MeshComp 메시가 필요합니다."), *GetName());
		return;
	}
	const FBoxSphereBounds MeshBounds = MeshComp->GetStaticMesh()->GetBounds();
	ClickBoxComp->SetRelativeLocation(MeshBounds.Origin);
	ClickBoxComp->SetBoxExtent(MeshBounds.BoxExtent + FVector(70.f));

	UpdateActivation();
	GetWorldTimerManager().SetTimer(ActivationTimer, this,
		&AAmbientAircraftFlyby::UpdateActivation, 0.25f, true);
}

void AAmbientAircraftFlyby::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ActivationTimer);
	Super::EndPlay(EndPlayReason);
}

void AAmbientAircraftFlyby::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (RotorMeshComp->GetStaticMesh())
	{
		// 쿼터니언으로 로컬 수직축을 회전해 장시간 운항에서도 각도 누적을 피한다.
		const float DeltaDegrees = FMath::Fmod(RotorDegreesPerSecond * DeltaTime, 360.f);
		RotorMeshComp->AddLocalRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(DeltaDegrees)));
	}
	if (TailRotorMeshComp->GetStaticMesh())
	{
		// 꼬리 프로펠러는 자체 전방축을 중심으로 회전한다.
		const float DeltaDegrees = FMath::Fmod(TailRotorDegreesPerSecond * DeltaTime, 360.f);
		TailRotorMeshComp->AddLocalRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(DeltaDegrees)));
	}
}

void AAmbientAircraftFlyby::OnInteract_Implementation(APlayerController* /*InstigatingPC*/)
{
}

void AAmbientAircraftFlyby::OnEndInteract_Implementation(APlayerController* InstigatingPC)
{
	const AMainMapPlayerController* MainMapPC = Cast<AMainMapPlayerController>(InstigatingPC);
	if (!IsValid(MainMapPC) || MainMapPC->GetWorld() != GetWorld() || !MainMapPC->IsLocalController() ||
		MainMapPC->GetCurrentInputMode() != EInputMode::Normal ||
		!bIsActive || IsHidden() || !MeshComp->IsVisible() || MeshComp->bHiddenInGame ||
		ClickBoxComp->GetCollisionEnabled() != ECollisionEnabled::QueryOnly)
	{
		return;
	}

	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance || GameInstance->IsVisitMode())
	{
		return;
	}

	if (UResourceItemManager* ResourceManager = GameInstance->GetSubsystem<UResourceItemManager>())
	{
		const int64 PaidAmount = ResourceManager->CollectAmbientAircraftBonus(GetFName());
		if (PaidAmount > 0)
		{
			if (UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>())
			{
				UIManager->ShowFundsToast(PaidAmount);
			}
		}
	}
}

void AAmbientAircraftFlyby::UpdateActivation()
{
	const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* PlayerPawn = PlayerController && PlayerController->IsLocalController()
		? PlayerController->GetPawn() : nullptr;
	bool bShouldBeActive = false;
	if (PlayerPawn)
	{
		const float DistanceSquared = FVector::DistSquared2D(
			PlayerPawn->GetActorLocation(), GetActorLocation());
		const float ActiveRadius = bIsActive
			? FMath::Max(DeactivationRadiusCm, ActivationRadiusCm) : ActivationRadiusCm;
		bShouldBeActive = DistanceSquared <= FMath::Square(ActiveRadius);
	}

	if (bShouldBeActive != bIsActive)
	{
		bIsActive = bShouldBeActive;
		MeshComp->SetHiddenInGame(!bIsActive, true);
		SetActorTickEnabled(bIsActive);
	}

	const ECollisionEnabled::Type DesiredCollision = bIsActive && !IsHidden() &&
		MeshComp->IsVisible() && !MeshComp->bHiddenInGame
		? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
	if (ClickBoxComp->GetCollisionEnabled() != DesiredCollision)
	{
		ClickBoxComp->SetCollisionEnabled(DesiredCollision);
	}
}
