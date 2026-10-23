// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Factory/BrickLaborer.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Entity/Factory/BrickFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogBrickLaborer, Log, All);

ABrickLaborer::ABrickLaborer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f / 30.f;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakePart = [this, Root](const TCHAR* Name) -> USkeletalMeshComponent*
	{
		USkeletalMeshComponent* Part = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Part->SetupAttachment(Root);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		// 포즈를 마스터에서 받으므로 화면 밖에서는 스킨 갱신도 돌릴 이유가 없다.
		Part->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		return Part;
	};

	Mesh = MakePart(TEXT("Mesh"));
	HeadwearMesh = MakePart(TEXT("HeadwearMesh"));
	OuterwearMesh = MakePart(TEXT("OuterwearMesh"));
	PantsMesh = MakePart(TEXT("PantsMesh"));
	ShoeMesh = MakePart(TEXT("ShoeMesh"));
	CarriedBrickMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarriedBrickMesh"));
	CarriedBrickMesh->SetupAttachment(Mesh, CarryAttachmentBone);
	CarriedBrickMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarriedBrickMesh->SetGenerateOverlapEvents(false);
	CarriedBrickMesh->SetCanEverAffectNavigation(false);
	CarriedBrickMesh->SetCastShadow(false);
	CarriedBrickMesh->SetComponentTickEnabled(false);
	CarriedBrickMesh->SetVisibility(false);
	CarriedBrickAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/SM_LaborerCarriedBricks.SM_LaborerCarriedBricks")));

	// 캐릭터 전용 2톤 툰(Unlit, 고정 광원) — FunnyOfficeworker 와 같은 자산/계약을 쓴다.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ToonFinder(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_FunnyToon"));
	ToonMaterial = ToonFinder.Succeeded() ? ToonFinder.Object : nullptr;
}

void ABrickLaborer::BeginPlay()
{
	Super::BeginPlay();
	if (CarriedBrickMesh)
	{
		CarriedBrickMesh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, CarryAttachmentBone);
		CarriedBrickMesh->SetRelativeTransform(CarryAttachmentTransform);
		UStaticMesh* BrickAsset = CarriedBrickAsset.IsNull() ? nullptr : CarriedBrickAsset.LoadSynchronous();
		if (!BrickAsset)
		{
			BrickAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
				TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/SM_LaborerCarriedBricks.SM_LaborerCarriedBricks")))
				.LoadSynchronous();
		}
		CarriedBrickMesh->SetStaticMesh(BrickAsset);
		if (!BrickAsset)
		{
			static bool bWarnedMissingBrickAsset = false;
			if (!bWarnedMissingBrickAsset)
			{
				UE_LOG(LogBrickLaborer, Warning, TEXT("Carried brick mesh is unavailable; laborers will walk empty-handed"));
				bWarnedMissingBrickAsset = true;
			}
		}
	}

	// 생성자에서 채운 배열은 BP 인스턴스로 복제될 때 CDO 컴포넌트를 가리킬 위험이 있다.
	// 인스턴스의 직접 포인터로 매번 다시 만든다 — 엉뚱한 컴포넌트에 리더를 거는 사고 방지.
	PartMeshes.Reset();
	for (USkeletalMeshComponent* Part : { Mesh, HeadwearMesh, OuterwearMesh, PantsMesh, ShoeMesh })
	{
		if (Part)
		{
			PartMeshes.Add(Part);
		}
	}

	SetActorScale3D(FVector(FMath::Max(0.1f, LaborerScale)));

	// 메시 앞축 보정 — 액터는 +X 로 진행하는데 Funny 메시는 그 축을 안 본다
	for (USkeletalMeshComponent* Part : PartMeshes)
	{
		if (Part)
		{
			Part->SetRelativeLocationAndRotation(
				FVector(0.f, 0.f, MeshZOffset), FRotator(0.f, MeshYawOffset, 0.f));
		}
	}

	ApplyLook();

	UE_LOG(LogBrickLaborer, Log, TEXT("[Laborer] BeginPlay parts=%d mids=%d night=%d body=%s loc=%s"),
		PartMeshes.Num(), PartMIDs.Num(), bNightMode ? 1 : 0,
		(Mesh && Mesh->GetSkeletalMeshAsset()) ? *Mesh->GetSkeletalMeshAsset()->GetName() : TEXT("NONE"),
		*GetActorLocation().ToCompactString());
}

void ABrickLaborer::InitLaborer(ABrickFactory* InFactory, int32 InLaneIndex, int32 InLaneCount)
{
	OwnerFactory = InFactory;
	LaneIndex = InLaneIndex;
	LaneCount = FMath::Max(1, InLaneCount);
	// 모든 진행 방향에서 파츠가 Plot 밖으로 나가지 않도록 XY 외접 반경을 한 번 산출한다.
	PathClearance = 130.f;
	for (USkeletalMeshComponent* Part : { Mesh, HeadwearMesh, OuterwearMesh, PantsMesh, ShoeMesh })
	{
		if (Part && Part->GetSkeletalMeshAsset())
		{
			const FBoxSphereBounds MeshBounds = Part->GetSkeletalMeshAsset()->GetBounds();
			const float LocalRadius = FVector2D(MeshBounds.Origin.X, MeshBounds.Origin.Y).Size()
				+ FVector2D(MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y).Size()
				+ FVector2D(Part->GetRelativeLocation().X, Part->GetRelativeLocation().Y).Size();
			PathClearance = FMath::Max(PathClearance,
				LocalRadius * Part->GetRelativeScale3D().GetAbsMax() * FMath::Max(0.1f, LaborerScale) + 10.f);
		}
	}

	// 레인별로 처음 도착할 문을 분산하고 인접한 다른 문 안쪽에서 출발한다.
	const int32 DoorCount = FMath::Max(1, InFactory ? InFactory->GetLaborerDoorCount() : 1);
	TargetDoorIndex = FMath::Max(0, LaneIndex) % DoorCount;
	FromDoorIndex = DoorCount > 1 ? (TargetDoorIndex + 1) % DoorCount : TargetDoorIndex;
	CompletedTrips = 0;
	bCarryingThisLeg = (FMath::Max(0, LaneIndex) % 2) == 0;

	// 진행도를 흩어 둔다 — 전원이 같은 자리에 겹치지 않게.
	// 황금비 소수부는 어떤 명수에서도 고르게 퍼진다.
	LegAlpha = FMath::Frac(static_cast<float>(LaneIndex) * 0.6180339887f);

	ApplyLegEndpoints();
	bHarvestedThisLeg = LegAlpha >= GetEntranceAlpha();
	AttachPartsToBody();
	ApplyGaitForCurrentLeg(false);
	UpdatePathPose();

	UE_LOG(LogBrickLaborer, Log, TEXT("[Laborer] Init lane=%d/%d door=%d start=%s end=%s alpha=%.2f"),
		LaneIndex, LaneCount, TargetDoorIndex,
		*Path.InsideFrom.ToCompactString(), *Path.InsideTo.ToCompactString(), LegAlpha);
}

void ABrickLaborer::SetCycleDuration(float InSeconds)
{
	CycleDuration = FMath::Max(0.1f, InSeconds);
}

void ABrickLaborer::RefreshPath()
{
	if (const ABrickFactory* Factory = OwnerFactory.Get())
	{
		const int32 DoorCount = FMath::Max(1, Factory->GetLaborerDoorCount());
		FromDoorIndex = FMath::Clamp(FromDoorIndex, 0, DoorCount - 1);
		TargetDoorIndex = FMath::Clamp(TargetDoorIndex, 0, DoorCount - 1);
		if (DoorCount > 1 && FromDoorIndex == TargetDoorIndex)
		{
			TargetDoorIndex = (FromDoorIndex + 1) % DoorCount;
		}
	}
	ApplyLegEndpoints();
	UpdatePathPose();
}

void ABrickLaborer::ApplyLegEndpoints()
{
	const ABrickFactory* Factory = OwnerFactory.Get();
	if (!Factory)
	{
		return;
	}

	const FTransform FromDoor = Factory->GetLaborerDoorTransform(FromDoorIndex);
	const FTransform ToDoor = Factory->GetLaborerDoorTransform(TargetDoorIndex);
	const FTransform Footprint = Factory->GetPlacementFootprintTransform();
	FBox2D BodyBounds(ForceInit);
	if (const UStaticMeshComponent* FactoryBody = Cast<UStaticMeshComponent>(Factory->GetMainMeshComponent()))
	{
		if (const UStaticMesh* FactoryMesh = FactoryBody->GetStaticMesh())
		{
			const FBox BodyBox = FactoryMesh->GetBoundingBox();
			for (const double X : { BodyBox.Min.X, BodyBox.Max.X })
			{
				for (const double Y : { BodyBox.Min.Y, BodyBox.Max.Y })
				{
					const FVector Local = Footprint.InverseTransformPositionNoScale(
						FactoryBody->GetComponentTransform().TransformPosition(FVector(X, Y, 0.f)));
					BodyBounds += FVector2D(Local.X, Local.Y);
				}
			}
		}
	}
	Path.BuildBounded(FromDoor, ToDoor, DoorEnterDepth, DoorApproachDistance, LaneIndex,
		Footprint, Factory->GetPlacementFootprintHalfExtent(false), PathClearance, BodyBounds);
}

void ABrickLaborer::AdvanceLeg()
{
	ABrickFactory* Factory = OwnerFactory.Get();

	// 큰 프레임 간격으로 입장 구간을 건너뛰어도 수확은 한 번만 처리한다.
	HarvestAtEntrance();

	if (bRetireRequested)
	{
		if (Factory)
		{
			Factory->NotifyLaborerRetired(this);
		}
		Destroy();
		return;
	}

	// 문 안쪽 회전을 마친 자리에서 다음 구간을 시작한다.
	FromDoorIndex = TargetDoorIndex;
	++CompletedTrips;
	bCarryingThisLeg = ((FMath::Max(0, LaneIndex) + CompletedTrips) % 2) == 0;
	const int32 DoorCount = FMath::Max(1, Factory ? Factory->GetLaborerDoorCount() : 1);
	const int32 DoorOffset = DoorCount > 1
		? 1 + ((FMath::Max(0, LaneIndex) + CompletedTrips - 1) % (DoorCount - 1)) : 0;
	TargetDoorIndex = (FromDoorIndex + DoorOffset) % DoorCount;
	bHarvestedThisLeg = false;
	ApplyLegEndpoints();
	ApplyGaitForCurrentLeg(true);
}

void ABrickLaborer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!OwnerFactory.IsValid())
	{
		return;
	}

	LegAlpha += (DeltaTime * SpeedMultiplier) / FMath::Max(0.01f, CycleDuration);

	while (LegAlpha >= 1.f)
	{
		LegAlpha -= 1.f;
		AdvanceLeg();
		if (!IsValid(this))
		{
			return;
		}
	}

	UpdatePathPose();
	if (LegAlpha >= GetEntranceAlpha()) HarvestAtEntrance();
}

float ABrickLaborer::GetEntranceAlpha() const
{
	const float MoveAlpha = 1.f - FMath::Clamp(InteriorTurnSeconds / CycleDuration, 0.f, 0.15f);
	const float EntryDistance = Path.Length() - Path.EntryDepth;
	return MoveAlpha * FMath::Clamp(EntryDistance / FMath::Max(Path.Length(), KINDA_SMALL_NUMBER), 0.f, 1.f);
}

void ABrickLaborer::HarvestAtEntrance()
{
	if (bHarvestedThisLeg) return;
	bHarvestedThisLeg = true;
	if (ABrickFactory* Factory = OwnerFactory.Get()) Factory->OnLaborerEntered(HarvestAmount);
}

void ABrickLaborer::UpdatePathPose()
{
	const float TurnAlpha = FMath::Clamp(InteriorTurnSeconds / CycleDuration, 0.f, 0.15f);
	const float MoveAlpha = 1.f - TurnAlpha;
	const bool bTurning = TurnAlpha > KINDA_SMALL_NUMBER && LegAlpha >= MoveAlpha;
	const float Distance = Path.Length() * FMath::Clamp(LegAlpha / MoveAlpha, 0.f, 1.f);
	FQuat Facing = GetActorQuat();
	if (bTurning)
	{
		float TurnProgress = (LegAlpha - MoveAlpha) / TurnAlpha;
		if (bEaseAtDoors) TurnProgress = FMath::SmoothStep(0.f, 1.f, TurnProgress);
		const FQuat Incoming = Path.InwardTo.Rotation().Quaternion();
		const FQuat Outgoing = (-Path.InwardTo).Rotation().Quaternion();
		Facing = FQuat::Slerp(Incoming, Outgoing, TurnProgress);
	}
	else
	{
		const FVector Direction = Path.DirectionAtDistance(Distance);
		if (!Direction.IsNearlyZero()) Facing = Direction.Rotation().Quaternion();
	}
	SetActorLocationAndRotation(Path.PositionAtDistance(Distance), Facing);
	if (Mesh)
	{
		const float GroundSpeed = Path.Length() * SpeedMultiplier / (CycleDuration * MoveAlpha);
		Mesh->SetPlayRate(bTurning ? 0.f : GaitPlayRate * GroundSpeed / GaitGroundSpeed);
	}
}


void ABrickLaborer::AttachPartsToBody()
{
	if (!Mesh)
	{
		return;
	}

	// 바디가 애니를 돌리고 파츠 4개가 그 포즈를 따른다 — FunnyOfficeworker 와 같은 구조.
	// 공장의 포즈 마스터(다른 액터)를 리더로 삼던 구조에서는 전원이 T 포즈로 섰다(2026-10-02 실측).
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	for (USkeletalMeshComponent* Part : PartMeshes)
	{
		if (!Part || Part == Mesh)
		{
			continue;
		}
		Part->SetLeaderPoseComponent(Mesh);
		Part->bUseBoundsFromLeaderPoseComponent = true;
	}
}

void ABrickLaborer::SetGait(UAnimSequence* Clip, float PlayRate, float GroundSpeed, UAnimSequence* CarryClip)
{
	NormalGaitClip = Clip;
	CarryGaitClip = CarryClip;
	GaitPlayRate = FMath::Max(0.05f, PlayRate);
	GaitGroundSpeed = FMath::Max(20.f, GroundSpeed);
	ApplyGaitForCurrentLeg(false);
	UpdatePathPose();
}

void ABrickLaborer::ApplyGaitForCurrentLeg(bool bPreservePhase)
{
	const bool bHasAttachmentBone = Mesh && Mesh->GetSkeletalMeshAsset()
		&& Mesh->GetBoneIndex(CarryAttachmentBone) != INDEX_NONE;
	if (!bHasAttachmentBone)
	{
		static bool bWarnedMissingCarryBone = false;
		if (!bWarnedMissingCarryBone)
		{
			UE_LOG(LogBrickLaborer, Warning, TEXT("Carry attachment bone %s is missing; laborers will walk empty-handed"),
				*CarryAttachmentBone.ToString());
			bWarnedMissingCarryBone = true;
		}
	}
	const bool bUseCarryClip = bCarryingThisLeg && CarryGaitClip && CarriedBrickMesh
		&& CarriedBrickMesh->GetStaticMesh() && bHasAttachmentBone;
	if (CarriedBrickMesh)
	{
		CarriedBrickMesh->SetVisibility(bUseCarryClip);
	}
	if (!Mesh || !NormalGaitClip)
	{
		return;
	}

	UAnimSequence* DesiredClip = bUseCarryClip ? CarryGaitClip.Get() : NormalGaitClip.Get();
	UAnimSingleNodeInstance* GaitInstance = Mesh->GetSingleNodeInstance();
	if (GaitInstance && GaitInstance->GetAnimationAsset() == DesiredClip)
	{
		return;
	}

	const float PreviousLength = GaitInstance ? GaitInstance->GetLength() : 0.f;
	const float Phase = bPreservePhase && PreviousLength > KINDA_SMALL_NUMBER
		? FMath::Clamp(Mesh->GetPosition() / PreviousLength, 0.f, 1.f)
		: FMath::Frac(static_cast<float>(LaneIndex) * 0.6180339887f);
	Mesh->SetAnimation(DesiredClip);
	Mesh->Play(true);
	Mesh->SetPosition(DesiredClip->GetPlayLength() * Phase, false);
}

void ABrickLaborer::SetNightMode(bool bInNight)
{
	if (bNightMode == bInNight)
	{
		return;
	}
	bNightMode = bInNight;
	ApplyLook();
}

void ABrickLaborer::ApplyLook()
{
	// M_FunnyToon 은 Unlit — 조명을 안 타므로 밤에도 재료 색 그대로 나간다.
	// 틴트를 곱으로 낮춰 어둠에 섞이게 한다(0 으로 죽이면 실루엣까지 사라진다).
	const float Scale = bNightMode ? FMath::Clamp(NightTintScale, 0.05f, 1.f) : 1.f;
	const FLinearColor Tints[] = {
		SkinTint * Scale, HeadwearTint * Scale, OuterwearTint * Scale,
		PantsTint * Scale, ShoeTint * Scale
	};

	// MID 는 한 번만 만들고 이후에는 값만 바꾼다 — 밤낮 전환마다 재생성하면 30명 × 5개가 매번 새로 생긴다.
	if (PartMIDs.Num() != PartMeshes.Num())
	{
		PartMIDs.Init(nullptr, PartMeshes.Num());
	}

	for (int32 i = 0; i < PartMeshes.Num(); ++i)
	{
		USkeletalMeshComponent* Part = PartMeshes[i];
		if (!Part || !Part->GetSkeletalMeshAsset())
		{
			continue;
		}

		// 슬롯 0 의 에셋 기본 머티리얼을 그대로 쓰면 파츠별로 셰이딩 모델이 갈린다 — 부모를 툰으로 고정한다.
		if (!PartMIDs[i] || (ToonMaterial && PartMIDs[i]->Parent != ToonMaterial))
		{
			PartMIDs[i] = ToonMaterial
				? Part->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ToonMaterial)
				: Part->CreateDynamicMaterialInstance(0);
		}
		UMaterialInstanceDynamic* MID = PartMIDs[i];
		if (!MID)
		{
			continue;
		}

		// M_FunnyToon 파라미터 계약 (FunnyOfficeworker 와 동일)
		MID->SetVectorParameterValue(TEXT("TintColor"), Tints[FMath::Min(i, 4)]);
		MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		MID->SetScalarParameterValue(TEXT("Emissive"), 0.f);
		MID->SetScalarParameterValue(TEXT("Brightness"), FMath::Max(0.05f, LookBrightness));
		MID->SetScalarParameterValue(TEXT("TexBlend"), 1.f);
		// "White" 바디 팔레트 셀의 linear ≈0.185 를 보정해야 피부 틴트가 제 색으로 나온다
		MID->SetScalarParameterValue(TEXT("TexGain"), (Part == Mesh) ? 5.4f : 1.f);

	}
}
