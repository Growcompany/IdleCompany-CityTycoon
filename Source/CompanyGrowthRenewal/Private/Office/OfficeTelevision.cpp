#include "Office/OfficeTelevision.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/CGGameInstance.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeTVBroadcastSource.h"
#include "Office/OfficeTVNewsDesk.h"
#include "TimerManager.h"
#include "UI/Element/Office/OfficeTVScreenWidget.h"
#include "UI/Panel/OfficeMainWidget.h"
#include "UI/UIBase.h"
#include "UObject/ConstructorHelpers.h"

AOfficeTelevision::AOfficeTelevision()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Mount")));
	Bezel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bezel"));
	Bezel->SetupAttachment(GetRootComponent());
	Bezel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bezel->SetCanEverAffectNavigation(false);
	Bezel->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TVCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Bezel->SetStaticMesh(TVCube.Object);

	Screen = CreateDefaultSubobject<UWidgetComponent>(TEXT("Screen"));
	Screen->SetupAttachment(GetRootComponent());
	Screen->SetWidgetSpace(EWidgetSpace::World);
	Screen->SetDrawSize(FVector2D(1024, 576));
	Screen->SetPivot(FVector2D(0.5, 0.5));
	Screen->SetBlendMode(EWidgetBlendMode::Opaque);
	Screen->SetManuallyRedraw(true);
	Screen->SetRedrawTime(1.f);
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetCanEverAffectNavigation(false);
	Screen->SetCastShadow(false);
	Screen->SetTickWhenOffscreen(false);
}

FBox AOfficeTelevision::GetScreenBoundsLocal(const AOfficeInterior* InInterior)
{
	if (!InInterior) { return FBox(ForceInit); }
	const FBox2D FloorBounds = InInterior->GetCurrentFloorBoundsLocal();
	if (!FloorBounds.bIsValid) { return FBox(ForceInit); }
	const FBox DoorBounds = InInterior->GetRecruitmentEntranceExclusionBoundsLocal();
	const double StartX = FMath::Max(FloorBounds.Min.X + 40.0, DoorBounds.IsValid ? DoorBounds.Max.X + 24.0 : FloorBounds.Min.X);
	const double EndX = FloorBounds.Max.X - 40.0;
	const double AvailableWidth = EndX - StartX;
	if (AvailableWidth <= 20.0) { return FBox(ForceInit); }
	const double Width = FMath::Min3(AvailableWidth * 0.82, 430.0,
		FMath::Max(20.0, static_cast<double>(InInterior->GetWallHeight()) - 150.0) * 16.0 / 9.0);
	const FVector Center((StartX + EndX) * 0.5, FloorBounds.Max.Y - 7.0,
		InInterior->GetStructuralFloorZLocal() + InInterior->GetWallHeight() * 0.50);
	const FVector Extent(Width * 0.5, 2.0, Width * 9.0 / 32.0);
	return FBox(Center - Extent, Center + Extent);
}

FBox AOfficeTelevision::GetExclusionBoundsLocal(const AOfficeInterior* InInterior)
{
	const FBox Bounds = GetScreenBoundsLocal(InInterior);
	return Bounds.IsValid ? Bounds.ExpandBy(FVector(12, 18, 12)) : Bounds;
}

void AOfficeTelevision::Initialize(AOfficeInterior* InInterior)
{
	if (Interior.IsValid()) { Interior->OnFootprintChanged.RemoveAll(this); }
	Interior = InInterior;
	if (!InInterior) { return; }
	AttachToActor(InInterior, FAttachmentTransformRules::KeepRelativeTransform);
	SetActorRelativeScale3D(FVector::OneVector);
	InInterior->OnFootprintChanged.AddUObject(this, &AOfficeTelevision::UpdateLayout);
	UpdateLayout(InInterior->GetTileCount());
}

void AOfficeTelevision::BeginPlay()
{
	Super::BeginPlay();
	const TSoftObjectPtr<UMaterialInterface> FrameMaterial(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Office/Television/Materials/M_OfficeTVFrame.M_OfficeTVFrame")));
	if (UMaterialInterface* Material = FrameMaterial.LoadSynchronous())
	{
		Bezel->SetMaterial(0, Material);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeTV] Bezel material /Game/CompanyGrowth/Office/Television/Materials/M_OfficeTVFrame is missing; restore it from git."));
	}
	StageSource = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	NewsDesk = NewObject<UOfficeTVNewsDesk>(this);
	NewsDesk->Start(GetWorld());
	if (UProjectOperationManager* Operations = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr)
	{
		Operations->OnOperationUpdated.AddUniqueDynamic(this, &AOfficeTelevision::HandleOperationUpdated);
	}
	LastBroadcastTime = GetWorld()->GetAudioTimeSeconds();
	GetWorldTimerManager().SetTimer(BroadcastTimer, this, &AOfficeTelevision::RefreshBroadcast, 1.f, true, 0.2f);
}

void AOfficeTelevision::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BroadcastTimer);
	GetWorldTimerManager().ClearTimer(MotionTimer);
	if (UProjectOperationManager* Operations = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr)
	{
		Operations->OnOperationUpdated.RemoveAll(this);
	}
	if (NewsDesk) { NewsDesk->Stop(); }
	StageSource.Reset();
	if (Interior.IsValid()) { Interior->OnFootprintChanged.RemoveAll(this); }
	Interior.Reset();
	Super::EndPlay(EndPlayReason);
}

void AOfficeTelevision::UpdateLayout(FIntPoint TileCount)
{
	const FBox Bounds = GetScreenBoundsLocal(Interior.Get());
	if (!Bounds.IsValid) { SetActorHiddenInGame(true); return; }
	SetActorRelativeLocation(Bounds.GetCenter());
	SetActorRelativeRotation(FRotator(0, -90, 0));
	const FVector Size = Bounds.GetSize();
	Bezel->SetRelativeScale3D(FVector(4, Size.X + 6, Size.Z + 6) / 100.0);
	Screen->SetRelativeLocation(FVector(2.2, 0, 0));
	Screen->SetRelativeScale3D(FVector(Size.X / 1024.0));
}

void AOfficeTelevision::RefreshBroadcast()
{
	// This clock pauses with the game without speeding up under development time dilation.
	const double Now = GetWorld()->GetAudioTimeSeconds();
	const float Elapsed = static_cast<float>(Now - LastBroadcastTime);
	LastBroadcastTime = Now;
	bShowingLiveReadout = false;
	if (!Interior.IsValid()) { return; }
	TArray<FOfficeTVBroadcastFrame> Interrupts;
	TArray<FOfficeTVBroadcastFrame> Priority;
	if (NewsDesk)
	{
		NewsDesk->Tick();
		NewsDesk->TakeNews(Interrupts, Priority);
	}
	const UCGGameInstance* Game = Cast<UCGGameInstance>(GetGameInstance());
	if (!Game || !Game->IsInOfficeMap() || Game->IsVisitMode())
	{
		Playback.Reset();
		SetActorHiddenInGame(true);
		return;
	}
	if (!bScreenInitialized)
	{
		UTableManagerSubsystem* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!Tables) { return; }
		bScreenInitialized = true;
		const TSubclassOf<UUserWidget> ScreenClass = Tables->GetWidgetClass(EWidgetType::OfficeTVScreen);
		if (!ScreenClass || !ScreenClass->IsChildOf(UOfficeTVScreenWidget::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeTV] DT_WidgetClass.OfficeTVScreen is missing or has an invalid parent."));
			SetActorHiddenInGame(true);
			GetWorldTimerManager().ClearTimer(BroadcastTimer);
			return;
		}
		Screen->SetWidgetClass(ScreenClass);
		Screen->InitWidget();
	}
	UOfficeTVScreenWidget* ScreenWidget = Cast<UOfficeTVScreenWidget>(Screen->GetUserWidgetObject());
	if (!ScreenWidget) { return; }
	const auto IsSupported = [ScreenWidget](EOfficeTVProgram Program) { return ScreenWidget->SupportsProgram(Program); };
	Priority.RemoveAll([&IsSupported](const FOfficeTVBroadcastFrame& Candidate) { return !IsSupported(Candidate.Program); });
	if (!Priority.IsEmpty())
	{
		Playback.QueuePriority(Priority);
	}
	for (const FOfficeTVBroadcastFrame& Candidate : Interrupts)
	{
		if (IsSupported(Candidate.Program))
		{
			Playback.QueueInterrupt(Candidate);
		}
	}
	if (!CanWatchBroadcast())
	{
		Playback.Advance(FOfficeTVSchedule(), Elapsed, false);
		return;
	}
	FOfficeTVSchedule Schedule;
	OfficeTVBroadcastSource::Gather(GetWorld(), NewsDesk ? NewsDesk->GetBuffers() : FOfficeTVNewsBuffers(), IsSupported, Schedule);
	const bool bValidBounds = GetScreenBoundsLocal(Interior.Get()).IsValid != 0;
	const FOfficeTVBroadcastFrame* Frame = Playback.Advance(Schedule, Elapsed, bValidBounds);
	SetActorHiddenInGame(!Frame || !bValidBounds);
	if (!Frame) { return; }
	ScreenWidget->SetBroadcast(*Frame, Playback.GetShowSerial());
	bShowingLiveReadout = OfficeTVProgram::IsLiveReadout(Frame->Program);
	Screen->RequestRedraw();
	// Openings and loops redraw at frame rate only while they move; the rest of the time once a second.
	if (ScreenWidget->IsAnimating() && !GetWorldTimerManager().IsTimerActive(MotionTimer))
	{
		GetWorldTimerManager().SetTimer(MotionTimer, this, &AOfficeTelevision::PumpMotion, 1.f / 30.f, true);
	}
}

void AOfficeTelevision::HandleOperationUpdated(int32 BuildingID, const FOperationData& Data)
{
	const UCGGameInstance* Game = Cast<UCGGameInstance>(GetGameInstance());
	if (!Game || BuildingID != Game->GetCurrentManagedBuildingIndex())
	{
		return;
	}
	if (NewsDesk)
	{
		NewsDesk->NoteOperationTick(BuildingID, Data.TotalRevenueEarned);
	}
	if (bShowingLiveReadout)
	{
		RefreshBroadcast();
	}
}

void AOfficeTelevision::PumpMotion()
{
	const UOfficeTVScreenWidget* ScreenWidget = Cast<UOfficeTVScreenWidget>(Screen->GetUserWidgetObject());
	const bool bMoving = ScreenWidget && ScreenWidget->IsAnimating() && !IsHidden() && CanWatchBroadcast();
	Screen->RequestRedraw();
	if (!bMoving)
	{
		GetWorldTimerManager().ClearTimer(MotionTimer);
	}
}

bool AOfficeTelevision::CanWatchBroadcast() const
{
	const UUIManagerSubsystem* UI = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	const UUIBase* Base = UI ? UI->GetUIBase() : nullptr;
	// The ordinary office HUD is the bottom stack's base, not an obstructing menu.
	if (!Base || !UI->GetOfficeMain() || Base->GetPromptStackCount() > 0 ||
		Base->GetActivePromptWidget() || Base->GetBottomStackCount() > 1 ||
		Base->GetMainStackCount() > 1 ||
		Base->GetActiveBottomWidget() != UI->GetOfficeMain())
	{
		return false;
	}
	// Hold during the completion effect and before the result modal enters its stack as well.
	return !StageSource.IsValid() || (StageSource->GetLifecycle() != EProjectLifecycle::LaunchPending &&
		StageSource->GetLifecycle() != EProjectLifecycle::ReportPending);
}
