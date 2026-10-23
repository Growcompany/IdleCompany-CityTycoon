#include "Manager/OverseasDioramaSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Entity/OverseasDiorama/OverseasDioramaStage.h"

namespace
{
	const FVector kDioStageLocation(0.0, 0.0, 250000.0);
	constexpr int32 kDioLiveMaxWidth = 1600;
	constexpr int32 kDioLiveMaxHeight = 1440;

	FIntPoint ClampLiveSize(FIntPoint SizePx)
	{
		return FIntPoint(FMath::Clamp(SizePx.X, 1, kDioLiveMaxWidth), FMath::Clamp(SizePx.Y, 1, kDioLiveMaxHeight));
	}
}

bool UOverseasDioramaSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (!OuterWorld || !Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	return OuterWorld->WorldType == EWorldType::Game || OuterWorld->WorldType == EWorldType::PIE;
}

void UOverseasDioramaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UOverseasDioramaSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindManagers();
}

void UOverseasDioramaSubsystem::Deinitialize()
{
	UnbindManagers();
	if (Stage)
	{
		Stage->StopLive();
		Stage->Destroy();
		Stage = nullptr;
	}
	LiveCountry = ECountryType::None;
	PendingCaptures.Reset();
	Miniatures.Reset();
	Super::Deinitialize();
}

TStatId UOverseasDioramaSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOverseasDioramaSubsystem, STATGROUP_Tickables);
}

bool UOverseasDioramaSubsystem::BuildSpec(ECountryType Country, FOverseasDioramaSpec& OutSpec) const
{
	if (SpecProviderForTest)
	{
		return SpecProviderForTest(Country, OutSpec);
	}
	const UWorld* OwnerWorld = GetWorld();
	FDioramaSpecInputs Inputs;
	if (!OwnerWorld || !OverseasDiorama::GatherInputs(OwnerWorld->GetGameInstance(), Country, Inputs))
	{
		return false;
	}
	OutSpec = OverseasDiorama::AssembleSpec(Inputs);
	return true;
}

AOverseasDioramaStage* UOverseasDioramaSubsystem::EnsureStage()
{
	if (Stage)
	{
		return Stage;
	}
	UWorld* OwnerWorld = GetWorld();
	if (!OwnerWorld)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Stage = OwnerWorld->SpawnActor<AOverseasDioramaStage>(AOverseasDioramaStage::StaticClass(), kDioStageLocation, FRotator::ZeroRotator, Params);
	return Stage;
}

UTexture* UOverseasDioramaSubsystem::GetMiniature(ECountryType Country, FDioramaStillInfo& OutInfo) const
{
	const FDioramaMiniatureEntry* Entry = Miniatures.Find(Country);
	if (!Entry || !Entry->bCaptured)
	{
		return nullptr;
	}
	OutInfo = Entry->Info;
	return Entry->Target;
}

void UOverseasDioramaSubsystem::Enqueue(ECountryType Country)
{
	if (Country != ECountryType::None)
	{
		PendingCaptures.AddUnique(Country);
	}
}

void UOverseasDioramaSubsystem::RequestMiniatures(const TArray<ECountryType>& Countries)
{
	for (ECountryType Country : Countries)
	{
		FOverseasDioramaSpec Spec;
		if (!BuildSpec(Country, Spec) || !Spec.HasAny())
		{
			continue;
		}
		const FDioramaMiniatureEntry* Entry = Miniatures.Find(Country);
		if (!Entry || !Entry->bCaptured || Entry->LookHash != Spec.GetLookHash())
		{
			Enqueue(Country);
		}
	}
}

void UOverseasDioramaSubsystem::MarkDirty(ECountryType Country)
{
	if (Country == ECountryType::None)
	{
		return;
	}
	if (Country == LiveCountry)
	{
		bLiveDirty = true;
	}
	if (Miniatures.Contains(Country) || Country == LiveCountry)
	{
		Enqueue(Country);
	}
}

void UOverseasDioramaSubsystem::Tick(float DeltaTime)
{
	if (IsLiveView())
	{
		if (bLiveDirty && Stage)
		{
			bLiveDirty = false;
			FOverseasDioramaSpec Spec;
			if (BuildSpec(LiveCountry, Spec) && Spec.HasAny())
			{
				Stage->BuildFor(Spec, false);
			}
		}
		return;
	}
	ProcessOneCapture();
}

void UOverseasDioramaSubsystem::ProcessOneCapture()
{
	while (PendingCaptures.Num() > 0)
	{
		const ECountryType Country = PendingCaptures[0];
		PendingCaptures.RemoveAt(0);

		FOverseasDioramaSpec Spec;
		if (!BuildSpec(Country, Spec) || !Spec.HasAny())
		{
			continue;
		}
		FDioramaMiniatureEntry& Entry = Miniatures.FindOrAdd(Country);
		const uint32 Hash = Spec.GetLookHash();
		if (Entry.bCaptured && Entry.LookHash == Hash)
		{
			continue;
		}
		AOverseasDioramaStage* ActiveStage = EnsureStage();
		if (!ActiveStage)
		{
			return;
		}
		ActiveStage->BuildFor(Spec, true);
		UTextureRenderTarget2D* Target = Entry.Target;
		if (!ActiveStage->CaptureStill(Target, Entry.Info))
		{
			return;
		}
		Entry.Target = Target;
		Entry.LookHash = Hash;
		Entry.bCaptured = true;
		OnMiniatureUpdated.Broadcast(Country);
		return;
	}
}

UTextureRenderTarget2D* UOverseasDioramaSubsystem::BeginLiveView(ECountryType Country, FIntPoint SizePx, EDioramaModule Focus)
{
	FOverseasDioramaSpec Spec;
	if (!BuildSpec(Country, Spec) || !Spec.HasAny())
	{
		return nullptr;
	}
	AOverseasDioramaStage* ActiveStage = EnsureStage();
	if (!ActiveStage)
	{
		return nullptr;
	}

	const FIntPoint Size = ClampLiveSize(SizePx);
	if (!LiveTarget)
	{
		LiveTarget = NewObject<UTextureRenderTarget2D>(this);
		LiveTarget->RenderTargetFormat = RTF_RGBA16f;
		LiveTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		LiveTarget->InitAutoFormat(Size.X, Size.Y);
		LiveTarget->UpdateResourceImmediate(true);
	}
	else if (LiveTarget->SizeX != Size.X || LiveTarget->SizeY != Size.Y)
	{
		LiveTarget->ResizeTarget(Size.X, Size.Y);
	}

	const bool bSameCountry = ActiveStage->IsLive() && LiveCountry == Country;
	if (!bSameCountry)
	{
		ActiveStage->BuildFor(Spec, false);
		ActiveStage->FocusModule(Focus, true);
	}
	else
	{
		ActiveStage->FocusModule(Focus, false);
	}
	if (!ActiveStage->IsLive())
	{
		ActiveStage->StartLive(LiveTarget);
	}
	LiveCountry = Country;
	bLiveDirty = false;
	return LiveTarget;
}

void UOverseasDioramaSubsystem::SetLiveViewSize(FIntPoint SizePx)
{
	if (!LiveTarget || SizePx.X <= 0 || SizePx.Y <= 0)
	{
		return;
	}
	const FIntPoint Size = ClampLiveSize(SizePx);
	if (LiveTarget->SizeX != Size.X || LiveTarget->SizeY != Size.Y)
	{
		LiveTarget->ResizeTarget(Size.X, Size.Y);
	}
}

void UOverseasDioramaSubsystem::FocusModule(EDioramaModule Module)
{
	if (Stage && IsLiveView())
	{
		Stage->FocusModule(Module, false);
	}
}

void UOverseasDioramaSubsystem::PlayLiveIntro()
{
	if (Stage && IsLiveView())
	{
		Stage->PlayIntro();
	}
}

void UOverseasDioramaSubsystem::EndLiveView()
{
	if (!IsLiveView())
	{
		return;
	}
	if (Stage)
	{
		Stage->StopLive();
	}
	const ECountryType Ended = LiveCountry;
	LiveCountry = ECountryType::None;
	bLiveDirty = false;
	// 라이브 동안 미뤄 둔 그 나라 지도 그림을 나간 뒤 다시 찍는다.
	Enqueue(Ended);
}

void UOverseasDioramaSubsystem::BindManagers()
{
	if (bBoundManagers)
	{
		return;
	}
	const UWorld* OwnerWorld = GetWorld();
	UGameInstance* GI = OwnerWorld ? OwnerWorld->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}
	if (UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
	{
		FactoryMgr->OnLineStarted.AddDynamic(this, &UOverseasDioramaSubsystem::HandleFactoryLineStarted);
		FactoryMgr->OnLineCompleted.AddDynamic(this, &UOverseasDioramaSubsystem::HandleFactoryLineCompleted);
		FactoryMgr->OnLineClaimed.AddDynamic(this, &UOverseasDioramaSubsystem::HandleFactoryLineClaimed);
		FactoryMgr->OnUpgradeChanged.AddDynamic(this, &UOverseasDioramaSubsystem::HandleFactoryUpgradeChanged);
	}
	if (UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
	{
		MineMgr->OnLineCreated.AddDynamic(this, &UOverseasDioramaSubsystem::HandleMineLineCreated);
		MineMgr->OnLineSaturated.AddDynamic(this, &UOverseasDioramaSubsystem::HandleMineLineSaturated);
		MineMgr->OnResourceClaimed.AddDynamic(this, &UOverseasDioramaSubsystem::HandleMineResourceClaimed);
		MineMgr->OnLineRemoved.AddDynamic(this, &UOverseasDioramaSubsystem::HandleMineLineRemoved);
		MineMgr->OnUpgradeChanged.AddDynamic(this, &UOverseasDioramaSubsystem::HandleMineUpgradeChanged);
	}
	bBoundManagers = true;
}

void UOverseasDioramaSubsystem::UnbindManagers()
{
	if (!bBoundManagers)
	{
		return;
	}
	bBoundManagers = false;
	const UWorld* OwnerWorld = GetWorld();
	UGameInstance* GI = OwnerWorld ? OwnerWorld->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}
	if (UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
	{
		FactoryMgr->OnLineStarted.RemoveAll(this);
		FactoryMgr->OnLineCompleted.RemoveAll(this);
		FactoryMgr->OnLineClaimed.RemoveAll(this);
		FactoryMgr->OnUpgradeChanged.RemoveAll(this);
	}
	if (UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
	{
		MineMgr->OnLineCreated.RemoveAll(this);
		MineMgr->OnLineSaturated.RemoveAll(this);
		MineMgr->OnResourceClaimed.RemoveAll(this);
		MineMgr->OnLineRemoved.RemoveAll(this);
		MineMgr->OnUpgradeChanged.RemoveAll(this);
	}
}

void UOverseasDioramaSubsystem::HandleFactoryLineStarted(ECountryType Country, FWorldFactoryLineState LineState) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleFactoryLineCompleted(ECountryType Country, int32 LineId) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleFactoryLineClaimed(ECountryType Country, int32 LineId, int64 FinalQty) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleFactoryUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 NewLevel) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleMineLineCreated(ECountryType Country, FMineLineState LineState) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleMineLineSaturated(ECountryType Country, EResourceType Resource, bool bSaturated) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleMineResourceClaimed(ECountryType Country, EResourceType Resource, int64 ClaimedQty) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleMineLineRemoved(ECountryType Country, EResourceType Resource) { MarkDirty(Country); }
void UOverseasDioramaSubsystem::HandleMineUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 NewLevel) { MarkDirty(Country); }
