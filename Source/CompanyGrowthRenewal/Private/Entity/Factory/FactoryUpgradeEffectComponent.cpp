#include "Entity/Factory/FactoryUpgradeEffectComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr float ConcealTime = 0.45f;
	constexpr float HoldEndTime = 0.52f;
	constexpr float RevealPulseTime = 0.775f;
	constexpr float RevealSeconds = 1.50f;
	constexpr int32 MaximumPanels = 128;
	constexpr float PanelThickness = 12.f;
	constexpr float PanelDepthStep = PanelThickness / 6.f;
	constexpr float HiddenScale = 0.001f;

	constexpr int32 FloorPulseCount = 3;
	constexpr int32 FloorBeamCount = FloorPulseCount * 4;
	constexpr int32 RevealShardCount = 16;
	constexpr int32 OutlineBladeCount = 8;
	constexpr int32 FirstShard = FloorBeamCount;
	constexpr int32 FirstOutline = FirstShard + RevealShardCount;
	constexpr int32 FixedAccentCount = FirstOutline + OutlineBladeCount;
	constexpr int32 SparksPerPanel = 2;
	// About 0.1s, so a 30fps device still shows at least one overdriven spark frame.
	constexpr float SnapSparkLife = 0.055f;

	// Accent material selector: silver floor strip, gold reveal shard, white-hot blade.
	constexpr float KindStrip = 0.f;
	constexpr float KindShard = 1.f;
	constexpr float KindBlade = 2.f;

	struct FFloorPulse
	{
		float Start;
		float Life;
		float Peak;
		float Spread;
		float Width;
	};

	// Energy above 1 overdrives the accent color, so the conceal slam reads brighter than the hold pulse.
	constexpr FFloorPulse FloorPulses[FloorPulseCount] = {
		{ ConcealTime, 0.12f, 2.0f, 0.18f, 24.f },
		{ HoldEndTime, 0.12f, 1.0f, 0.18f, 24.f },
		{ RevealPulseTime, 0.16f, 1.8f, 0.42f, 32.f },
	};

	float Ease(float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	float SnapArrival(float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.f, 1.f);
		if (T < 0.72f)
		{
			return FMath::Square(T / 0.72f);
		}
		if (T < 0.85f)
		{
			return 1.f + 0.045f * FMath::Sin(HALF_PI * (T - 0.72f) / 0.13f);
		}
		return 1.f + 0.045f * (1.f - Ease((T - 0.85f) / 0.15f));
	}

	float Eject(float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.f, 1.f);
		if (T < 0.09f)
		{
			return -0.025f * FMath::Sin(PI * T / 0.09f);
		}
		const float Remaining = 1.f - (T - 0.09f) / 0.91f;
		return 1.f - Remaining * Remaining * Remaining;
	}

	FTransform BladeTransform(const FVector& Head, const FVector& Direction, const FVector& FaceHint,
		float Length, float Width, float Thickness, const FVector& MeshSize)
	{
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Direction, FaceHint).ToQuat();
		return FTransform(Rotation, Head - Rotation.GetAxisX() * Length * 0.5f,
			FVector(Length / FMath::Max(MeshSize.X, 1.f), Width / FMath::Max(MeshSize.Y, 1.f),
				Thickness / FMath::Max(MeshSize.Z, 1.f)));
	}

	FTransform HiddenTransform(const FVector& Location)
	{
		return FTransform(FQuat::Identity, Location, FVector(HiddenScale));
	}
}

UFactoryUpgradeEffectComponent::UFactoryUpgradeEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFactoryUpgradeEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	PrecacheAssets();
}

void UFactoryUpgradeEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Cancel();
	if (PanelInstances)
	{
		PanelInstances->DestroyComponent();
		PanelInstances = nullptr;
	}
	if (AccentInstances)
	{
		AccentInstances->DestroyComponent();
		AccentInstances = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UFactoryUpgradeEffectComponent::PrecacheAssets()
{
	if (!CachedMesh && !PanelMesh.IsNull())
	{
		CachedMesh = PanelMesh.LoadSynchronous();
	}
	if (!CachedMaterial && !PanelMaterial.IsNull())
	{
		CachedMaterial = PanelMaterial.LoadSynchronous();
	}
	if (!CachedAccentMaterial && !AccentMaterial.IsNull())
	{
		CachedAccentMaterial = AccentMaterial.LoadSynchronous();
	}
}

bool UFactoryUpgradeEffectComponent::PlayForBounds(USceneComponent* Attachment, FVector Center, FVector Extent)
{
	if (Extent.ContainsNaN() || Center.ContainsNaN() || Extent.GetMin() <= 0.f)
	{
		return false;
	}
	return Play(Attachment, FBox(Center - Extent, Center + Extent));
}

bool UFactoryUpgradeEffectComponent::Play(USceneComponent* Attachment, const FBox& LocalBounds)
{
	if (bPlaying)
	{
		return true;
	}
	if (!IsValid(Attachment) || Attachment->GetOwner() != GetOwner() || !LocalBounds.IsValid ||
		LocalBounds.Min.ContainsNaN() || LocalBounds.Max.ContainsNaN() || LocalBounds.GetSize().GetMin() <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryUpgradeEffect] Invalid attachment or local bounds"));
		return false;
	}

	PrecacheAssets();
	if (!CachedMesh || !CachedMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryUpgradeEffect] PanelMesh or PanelMaterial is missing on %s"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!PanelInstances)
	{
		PanelInstances = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("FactoryUpgradePanelInstances"));
		PanelInstances->SetMobility(EComponentMobility::Movable);
		PanelInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PanelInstances->SetGenerateOverlapEvents(false);
		PanelInstances->SetCanEverAffectNavigation(false);
		PanelInstances->SetCastShadow(false);
		PanelInstances->bUseAsOccluder = false;
		PanelInstances->SetupAttachment(Attachment);
		PanelInstances->RegisterComponent();
	}
	else
	{
		PanelInstances->AttachToComponent(Attachment, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	PanelInstances->SetRelativeTransform(FTransform::Identity);
	PanelInstances->SetStaticMesh(CachedMesh);
	PanelInstances->SetMaterial(0, CachedMaterial);
	PanelInstances->SetNumCustomDataFloats(1);
	PanelInstances->ClearInstances();
	BuildPanels(LocalBounds);
	if (Panels.IsEmpty() || Panels.Num() > MaximumPanels)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryUpgradeEffect] Panel count is invalid: %d"), Panels.Num());
		Panels.Reset();
		return false;
	}

	for (const FPanel& Panel : Panels)
	{
		PanelInstances->AddInstance(FTransform(Panel.Rotation, Panel.Position, Panel.Scale));
	}
	if (CachedAccentMaterial)
	{
		if (!AccentInstances)
		{
			AccentInstances = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("FactoryUpgradeAccentInstances"));
			AccentInstances->SetMobility(EComponentMobility::Movable);
			AccentInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			AccentInstances->SetGenerateOverlapEvents(false);
			AccentInstances->SetCanEverAffectNavigation(false);
			AccentInstances->SetCastShadow(false);
			AccentInstances->bUseAsOccluder = false;
			AccentInstances->SetupAttachment(Attachment);
			AccentInstances->RegisterComponent();
		}
		else
		{
			AccentInstances->AttachToComponent(Attachment, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		AccentInstances->SetRelativeTransform(FTransform::Identity);
		AccentInstances->SetStaticMesh(CachedMesh);
		AccentInstances->SetMaterial(0, CachedAccentMaterial);
		AccentInstances->SetNumCustomDataFloats(2);
		AccentInstances->ClearInstances();
		const int32 AccentCount = FixedAccentCount + Panels.Num() * SparksPerPanel;
		TArray<FTransform> HiddenAccents;
		HiddenAccents.Init(HiddenTransform(FVector::ZeroVector), AccentCount);
		AccentInstances->AddInstances(HiddenAccents, false, false, false);
		for (int32 Index = 0; Index < AccentCount; ++Index)
		{
			const float Kind = Index < FirstShard ? KindStrip : Index < FirstOutline ? KindShard : KindBlade;
			AccentInstances->SetCustomDataValue(Index, 1, Kind, false);
		}
	}
	Elapsed = 0.f;
	ActiveDuration = FMath::Clamp(Duration, 1.5f, 2.5f);
	++PlayGeneration;
	bConcealedSent = false;
	bRevealedSent = false;
	bPlaying = true;
	Render(0.f);
	PanelInstances->SetVisibility(true);
	if (AccentInstances)
	{
		AccentInstances->SetVisibility(CachedAccentMaterial != nullptr);
	}
	SetComponentTickEnabled(true);
	return true;
}

void UFactoryUpgradeEffectComponent::BuildPanels(const FBox& LocalBounds)
{
	Panels.Reset();
	const FVector Min = LocalBounds.Min - FVector(ShellPadding, ShellPadding, ShellPadding * 0.25f);
	const FVector Max = LocalBounds.Max + FVector(ShellPadding);
	const FVector Center = (Min + Max) * 0.5f;
	const FVector Size = Max - Min;
	ActiveShellBounds = FBox(Min, Max);
	constexpr int32 Rows = 3;
	const float RowHeight = Size.Z / Rows;

	auto AddWall = [this, &Center, &Min, RowHeight](float SidePosition, float Span,
		const FVector& Normal, const FVector& Tangent, bool bAcrossX)
	{
		const int32 Columns = FMath::Clamp(FMath::RoundToInt(Span / RowHeight), 4, 6);
		const float CellWidth = Span / Columns;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				const float Along = -Span * 0.5f + (Column + 0.5f) * CellWidth;
				const float Height = Min.Z + (Row + 0.5f) * RowHeight;
				const FVector Position = bAcrossX
					? FVector(Center.X + Along, SidePosition, Height)
					: FVector(SidePosition, Center.Y + Along, Height);
				const float DepthOffset = PanelDepthStep * static_cast<float>(2 * (Row & 1) + (Column & 1));
				const float Arrival = 0.02f + Row * 0.105f + Column * 0.021f;
				const float Peel = 0.59f + (2 - Row) * 0.055f + Column * 0.009f;
				AddPanel(Position + Normal * DepthOffset, Normal, Tangent, CellWidth * 1.08f, RowHeight * 1.08f,
					Arrival, Peel, static_cast<float>((Column * 3 + Row) % 5));
			}
		}
	};

	AddWall(Max.X, Size.Y, FVector::ForwardVector, FVector::RightVector, false);
	AddWall(Min.X, Size.Y, -FVector::ForwardVector, -FVector::RightVector, false);
	AddWall(Max.Y, Size.X, FVector::RightVector, -FVector::ForwardVector, true);
	AddWall(Min.Y, Size.X, -FVector::RightVector, FVector::ForwardVector, true);

	const float RoofTarget = FMath::Max(Size.X, Size.Y) / 4.f;
	const int32 RoofX = FMath::Clamp(FMath::RoundToInt(Size.X / RoofTarget), 3, 5);
	const int32 RoofY = FMath::Clamp(FMath::RoundToInt(Size.Y / RoofTarget), 3, 5);
	const float CellX = Size.X / RoofX;
	const float CellY = Size.Y / RoofY;
	for (int32 X = 0; X < RoofX; ++X)
	{
		for (int32 Y = 0; Y < RoofY; ++Y)
		{
			const float DepthOffset = PanelDepthStep * static_cast<float>(2 * (X & 1) + (Y & 1));
			const FVector Position(Min.X + (X + 0.5f) * CellX, Min.Y + (Y + 0.5f) * CellY, Max.Z + DepthOffset);
			AddPanel(Position, FVector::UpVector, FVector::ForwardVector, CellX * 1.08f, CellY * 1.08f,
				0.28f + (X + Y) * 0.012f, HoldEndTime + (X + Y) * 0.008f,
				static_cast<float>((X * 3 + Y) % 5));
		}
	}
}

void UFactoryUpgradeEffectComponent::AddPanel(const FVector& Position, const FVector& Normal,
	const FVector& Tangent, float Width, float Height, float ArrivalStart, float PeelStart, float Drift)
{
	const FVector MeshSize = CachedMesh->GetBoundingBox().GetSize();
	FPanel& Panel = Panels.AddDefaulted_GetRef();
	Panel.Position = Position;
	Panel.Normal = Normal;
	Panel.Tangent = Tangent;
	Panel.Rotation = FRotationMatrix::MakeFromXZ(Tangent, Normal).ToQuat();
	Panel.Scale = FVector(Width / FMath::Max(MeshSize.X, 1.f),
		Height / FMath::Max(MeshSize.Y, 1.f), PanelThickness / FMath::Max(MeshSize.Z, 1.f));
	Panel.Width = Width;
	Panel.Height = Height;
	Panel.ArrivalStart = ArrivalStart;
	Panel.PeelStart = PeelStart;
	Panel.Drift = Drift;
}

void UFactoryUpgradeEffectComponent::Render(float NormalizedTime)
{
	if (!PanelInstances)
	{
		return;
	}
	const bool bSparks = AccentInstances &&
		AccentInstances->GetInstanceCount() == FixedAccentCount + Panels.Num() * SparksPerPanel;
	const FVector MeshSize = CachedMesh->GetBoundingBox().GetSize();
	const FVector ShellCenter = ActiveShellBounds.GetCenter();
	for (int32 Index = 0; Index < Panels.Num(); ++Index)
	{
		const FPanel& Panel = Panels[Index];
		const float ArrivalDuration = Panel.Normal.Z > 0.5f ? 0.07f : 0.065f;
		const float ArrivalTime = FMath::Clamp((NormalizedTime - Panel.ArrivalStart) / ArrivalDuration, 0.f, 1.f);
		const float Arrival = Ease(ArrivalTime / 0.35f);
		const float Travel = SnapArrival(ArrivalTime);
		const float PeelTime = FMath::Clamp((NormalizedTime - Panel.PeelStart) / 0.21f, 0.f, 1.f);
		const float Flight = Eject(PeelTime);
		const float Fade = Ease((PeelTime - 0.65f) / 0.35f);
		const float Visibility = FMath::Max(0.001f, Arrival * (1.f - Fade));
		const float WidthSeam = (Panel.Width - Panel.Width / 1.08f + PanelGap) * (1.f - Arrival);
		const float HeightSeam = (Panel.Height - Panel.Height / 1.08f + PanelGap) * (1.f - Arrival);
		const FVector Scale(Panel.Scale.X * Visibility * (1.f - WidthSeam / Panel.Width),
			Panel.Scale.Y * Visibility * (1.f - HeightSeam / Panel.Height), Panel.Scale.Z * Visibility);
		const FVector Entry = Panel.Normal * (100.f + Panel.Width * 0.32f) -
			FVector::UpVector * (55.f + Panel.Height * 0.18f) + Panel.Tangent * (Panel.Drift - 2.f) * 18.f;
		const FVector Exit = FVector::UpVector * (70.f + Panel.Height * 0.60f + Panel.Drift * 10.f) +
			Panel.Normal * (80.f + Panel.Width * 0.70f + Panel.Drift * 12.f);
		const float FlightArc = FMath::Sin(PI * FMath::Max(0.f, Flight));
		const FVector Position = Panel.Position + Entry * (1.f - Travel) + Exit * Flight +
			Panel.Tangent * 24.f * FlightArc;
		const FQuat Tilt(Panel.Tangent, FMath::DegreesToRadians(56.f * Flight - 18.f * (1.f - Travel)));
		const FQuat PanelRotation = Tilt * Panel.Rotation;
		PanelInstances->UpdateInstanceTransform(Index, FTransform(PanelRotation, Position, Scale), false, false, true);

		const float SnapTime = Panel.ArrivalStart + ArrivalDuration * 0.72f;
		const float ArrivalFlash = FMath::Max(0.f, 1.f - FMath::Abs(NormalizedTime - SnapTime) / 0.026f) * Arrival * 1.05f;
		const float HeightRatio = FMath::Clamp((Panel.Position.Z - ActiveShellBounds.Min.Z) /
			FMath::Max(ActiveShellBounds.GetSize().Z, 1.f), 0.f, 1.f);
		const float Scan = FMath::Max(0.f, 1.f - FMath::Abs(NormalizedTime -
			(0.08f + HeightRatio * 0.31f)) / 0.04f) * Arrival * (1.f - Flight) * 0.22f;
		const float ConcealPulse = FMath::Max(0.f, 1.f - FMath::Abs(NormalizedTime - ConcealTime) / 0.028f) *
			Arrival * (1.f - Flight) * 0.95f;
		const float PeelFlash = FMath::Max(0.f, 1.f - FMath::Abs(PeelTime - 0.16f) / 0.16f) * 0.9f;
		const float Energy = FMath::Clamp(ArrivalFlash + Scan + ConcealPulse + PeelFlash, 0.f, 1.5f);
		PanelInstances->SetCustomDataValue(Index, 0, Energy, false);

		if (!bSparks)
		{
			continue;
		}
		const float SnapAge = (NormalizedTime - SnapTime) / SnapSparkLife;
		const FVector Bitangent = FVector::CrossProduct(Panel.Normal, Panel.Tangent);
		const FVector Moved = Position - Panel.Position;
		const float MovedLength = Moved.Size();
		// Walls spray from the lower seam; roof panels alternate edges so the roof does not spray one way.
		const float SeamSide = Panel.Normal.Z > 0.5f && (Index & 1) ? 1.f : -1.f;
		FVector StreakFace = Panel.Normal;
		if (Panel.Normal.Z > 0.5f)
		{
			// Roof streaks rise vertically, so their broad face turns outward from the shell.
			StreakFace = FVector(Panel.Position.X - ShellCenter.X, Panel.Position.Y - ShellCenter.Y, 0.f).GetSafeNormal();
			if (StreakFace.IsNearlyZero())
			{
				StreakFace = Panel.Tangent;
			}
		}
		for (int32 Side = 0; Side < SparksPerPanel; ++Side)
		{
			const int32 AccentIndex = FixedAccentCount + Index * SparksPerPanel + Side;
			const float Sign = Side == 0 ? -1.f : 1.f;
			FTransform SparkTransform = HiddenTransform(Panel.Position);
			float SparkEnergy = 0.f;
			if (SnapAge >= 0.f && SnapAge < 1.f)
			{
				// Grinder-like sparks spray from the seam corners the moment the panel locks in.
				const FVector Origin = Panel.Position + Panel.Tangent * (Sign * Panel.Width * 0.5f) +
					Bitangent * (SeamSide * Panel.Height * 0.5f) + Panel.Normal * (PanelThickness + 4.f);
				const FVector Direction = (Panel.Tangent * (Sign * (0.85f - 0.08f * Panel.Drift)) +
					Panel.Normal * (0.45f + 0.06f * Panel.Drift) + FVector::UpVector * 0.35f).GetSafeNormal();
				// Sized for the 80m gameplay camera: the blades leave the panel face and read against the ground.
				const float Reach = 110.f + Panel.Width * 0.55f;
				const FVector Head = Origin + Direction * Reach * (1.f - FMath::Square(1.f - SnapAge)) -
					FVector::UpVector * 120.f * SnapAge * SnapAge;
				const FVector Velocity = Direction * Reach * 2.f * (1.f - SnapAge) - FVector::UpVector * 240.f * SnapAge;
				SparkTransform = BladeTransform(Head, Velocity, Panel.Normal,
					(70.f + Panel.Width * 0.45f) * (1.f - 0.55f * SnapAge), 26.f, 6.f, MeshSize);
				SparkEnergy = 2.4f * FMath::Pow(1.f - SnapAge, 1.5f);
			}
			else if (PeelTime > 0.09f && PeelTime < 0.45f && MovedLength > 1.f)
			{
				// Each lateral edge leaves a short afterglow streak back toward the closed shell.
				const float Attack = FMath::Clamp((PeelTime - 0.09f) / 0.04f, 0.f, 1.f);
				const float Decay = 1.f - FMath::Clamp((PeelTime - 0.13f) / 0.32f, 0.f, 1.f);
				const FVector Head = Position + PanelRotation.GetAxisX() * (Sign * Panel.Width * 0.5f * Visibility);
				const float Length = FMath::Min(MovedLength, Panel.Width * 1.3f) * (0.5f + 0.5f * Decay);
				SparkTransform = BladeTransform(Head, Moved / MovedLength, StreakFace, Length, 26.f, 6.f, MeshSize);
				SparkEnergy = 1.7f * Attack * Decay * Decay;
			}
			AccentInstances->UpdateInstanceTransform(AccentIndex, SparkTransform, false, false, true);
			AccentInstances->SetCustomDataValue(AccentIndex, 0, SparkEnergy, false);
		}
	}
	PanelInstances->MarkRenderStateDirty();
	RenderAccents(NormalizedTime);
}

void UFactoryUpgradeEffectComponent::RenderAccents(float NormalizedTime)
{
	if (!AccentInstances || AccentInstances->GetInstanceCount() != FixedAccentCount + Panels.Num() * SparksPerPanel)
	{
		return;
	}
	const FVector MeshSize = CachedMesh->GetBoundingBox().GetSize();
	const FVector Center = ActiveShellBounds.GetCenter();
	const FVector Half = ActiveShellBounds.GetExtent();
	const float FloorZ = ActiveShellBounds.Min.Z + ShellPadding * 0.25f + 8.f;
	for (int32 Index = 0; Index < FloorBeamCount; ++Index)
	{
		const FFloorPulse& Pulse = FloorPulses[Index / 4];
		const int32 Side = Index % 4;
		const float Progress = FMath::Clamp((NormalizedTime - Pulse.Start) / Pulse.Life, 0.f, 1.f);
		const float Opacity = NormalizedTime >= Pulse.Start && NormalizedTime < Pulse.Start + Pulse.Life
			? FMath::Min(Progress / 0.1f, 1.f) * FMath::Square(1.f - Progress) * Pulse.Peak : 0.f;
		const float Expand = 1.f + Pulse.Spread * (1.f - FMath::Pow(1.f - Progress, 3.f));
		const bool bAlongX = Side < 2;
		const float SideSign = Side % 2 == 0 ? 1.f : -1.f;
		const FVector Position = bAlongX
			? FVector(Center.X, Center.Y + SideSign * Half.Y * Expand, FloorZ)
			: FVector(Center.X + SideSign * Half.X * Expand, Center.Y, FloorZ);
		const FQuat Rotation = bAlongX ? FQuat::Identity : FQuat(FVector::UpVector, HALF_PI);
		const float Length = bAlongX ? Half.X * 2.f * Expand : Half.Y * 2.f * Expand;
		const FVector Scale(Length / FMath::Max(MeshSize.X, 1.f), Pulse.Width / FMath::Max(MeshSize.Y, 1.f),
			5.f / FMath::Max(MeshSize.Z, 1.f));
		AccentInstances->UpdateInstanceTransform(Index,
			FTransform(Rotation, Position, Scale), false, false, true);
		AccentInstances->SetCustomDataValue(Index, 0, Opacity, false);
	}
	for (int32 Shard = 0; Shard < RevealShardCount; ++Shard)
	{
		const int32 Index = FirstShard + Shard;
		const int32 Side = Shard % 4;
		const int32 Along = Shard / 4;
		const float AlongRatio = -0.75f + Along * 0.5f;
		const float Start = 0.77f + Along * 0.008f + Side * 0.003f;
		const float Progress = FMath::Clamp((NormalizedTime - Start) / 0.16f, 0.f, 1.f);
		const float Pop = NormalizedTime >= Start && NormalizedTime < Start + 0.16f ? FMath::Sin(PI * Progress) : 0.f;
		FVector Direction;
		FVector Position;
		switch (Side)
		{
		case 0: Direction = FVector::ForwardVector; Position = Center + FVector(Half.X, Half.Y * AlongRatio, Half.Z * 0.75f); break;
		case 1: Direction = -FVector::ForwardVector; Position = Center + FVector(-Half.X, Half.Y * AlongRatio, Half.Z * 0.75f); break;
		case 2: Direction = FVector::RightVector; Position = Center + FVector(Half.X * AlongRatio, Half.Y, Half.Z * 0.75f); break;
		default: Direction = -FVector::RightVector; Position = Center + FVector(Half.X * AlongRatio, -Half.Y, Half.Z * 0.75f); break;
		}
		Position += Direction * (35.f + 85.f * Ease(Progress));
		Position.Z += 115.f * Ease(Progress) + (Shard % 3) * 12.f;
		const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(static_cast<float>(Shard * 37 % 360)));
		const FVector Scale(24.f / FMath::Max(MeshSize.X, 1.f), 14.f / FMath::Max(MeshSize.Y, 1.f),
			84.f / FMath::Max(MeshSize.Z, 1.f));
		AccentInstances->UpdateInstanceTransform(Index,
			FTransform(Rotation, Position, Scale * FMath::Max(Pop, HiddenScale)), false, false, true);
		AccentInstances->SetCustomDataValue(Index, 0, Pop * 1.5f, false);
	}

	// The finished box flashes its corner seams and roof edges just as the last roof panel locks.
	const float OutlineAttack = FMath::Clamp((NormalizedTime - (ConcealTime - 0.012f)) / 0.012f, 0.f, 1.f);
	const float OutlineDecay = 1.f - FMath::Clamp((NormalizedTime - ConcealTime) / 0.06f, 0.f, 1.f);
	const float OutlineEnergy = 2.f * OutlineAttack * FMath::Square(OutlineDecay);
	const float Grow = 1.f - FMath::Square(1.f - OutlineAttack);
	const float RoofZ = ActiveShellBounds.Max.Z;
	constexpr float OutlineOffset = 22.f;
	for (int32 Blade = 0; Blade < OutlineBladeCount; ++Blade)
	{
		const int32 Index = FirstOutline + Blade;
		if (OutlineEnergy <= 0.f)
		{
			AccentInstances->UpdateInstanceTransform(Index, HiddenTransform(Center), false, false, true);
			AccentInstances->SetCustomDataValue(Index, 0, 0.f, false);
			continue;
		}
		FTransform BladePose;
		if (Blade < 4)
		{
			const float SignX = Blade == 0 || Blade == 3 ? 1.f : -1.f;
			const float SignY = Blade < 2 ? 1.f : -1.f;
			const FVector Outward = FVector(SignX, SignY, 0.f).GetSafeNormal();
			const float Height = RoofZ - FloorZ;
			const FVector Base(Center.X + SignX * Half.X, Center.Y + SignY * Half.Y, FloorZ);
			BladePose = BladeTransform(Base + Outward * OutlineOffset + FVector::UpVector * Height * Grow,
				FVector::UpVector, Outward, FMath::Max(Height * Grow, 1.f), 40.f, 6.f, MeshSize);
		}
		else
		{
			const int32 Edge = Blade - 4;
			const bool bAlongX = Edge < 2;
			const float SideSign = Edge % 2 == 0 ? 1.f : -1.f;
			const FVector Outward = bAlongX ? FVector(0.f, SideSign, 0.f) : FVector(SideSign, 0.f, 0.f);
			const FVector Direction = bAlongX ? FVector::ForwardVector : FVector::RightVector;
			const float Length = FMath::Max((bAlongX ? Half.X : Half.Y) * 2.f * Grow, 1.f);
			const FVector EdgeCenter = (bAlongX
				? FVector(Center.X, Center.Y + SideSign * Half.Y, RoofZ)
				: FVector(Center.X + SideSign * Half.X, Center.Y, RoofZ)) +
				(Outward + FVector::UpVector) * OutlineOffset * 0.7f;
			BladePose = BladeTransform(EdgeCenter + Direction * Length * 0.5f, Direction,
				Outward + FVector::UpVector, Length, 40.f, 6.f, MeshSize);
		}
		AccentInstances->UpdateInstanceTransform(Index, BladePose, false, false, true);
		AccentInstances->SetCustomDataValue(Index, 0, OutlineEnergy, false);
	}
	AccentInstances->MarkRenderStateDirty();
}

void UFactoryUpgradeEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bPlaying)
	{
		return;
	}
	const uint32 TickGeneration = PlayGeneration;
	Elapsed += FMath::Max(0.f, DeltaTime);
	const float Phase = FMath::Clamp(Elapsed / ActiveDuration, 0.f, 1.f);
	if (!bConcealedSent && Phase >= ConcealTime)
	{
		Render(ConcealTime);
		bConcealedSent = true;
		OnConcealed.Broadcast();
		if (!bPlaying || PlayGeneration != TickGeneration) return;
	}
	Render(Phase);
	if (!bRevealedSent && Elapsed >= RevealSeconds)
	{
		bRevealedSent = true;
		OnRevealed.Broadcast();
		if (!bPlaying || PlayGeneration != TickGeneration) return;
	}
	if (Elapsed >= ActiveDuration)
	{
		Cancel();
		OnFinished.Broadcast();
	}
}

void UFactoryUpgradeEffectComponent::Cancel()
{
	bPlaying = false;
	++PlayGeneration;
	SetComponentTickEnabled(false);
	if (PanelInstances)
	{
		PanelInstances->ClearInstances();
		PanelInstances->SetVisibility(false);
	}
	if (AccentInstances)
	{
		AccentInstances->ClearInstances();
		AccentInstances->SetVisibility(false);
	}
	Panels.Reset();
	Elapsed = 0.f;
	bConcealedSent = false;
	bRevealedSent = false;
}
