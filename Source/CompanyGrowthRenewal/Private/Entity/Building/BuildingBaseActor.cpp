// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Building/BuildingDefaultSkin.h"

#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Player/BuildingLandCameraShake.h"
#include "Player/PlayerCamera.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Util/InstancedMeshRefresh.h"
#include "Utils/MobileFacadeMaterialTuning.h"
#include "Utils/BuildingGroundContact.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Manager/EntityManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SoundManagerSubsystem.h"

#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Panel/FactoryPanelWidget.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/UIBase.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Materials/MaterialInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimeCycle/TimeCycleManager.h"
#include "Table/BuildingSkinData.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/TutorialEnhancementGateRules.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"

ABuildingBaseActor::ABuildingBaseActor()
{
	// 부모 AInteractableBaseActor의 Timeline(PlayWobble) 진행을 위해 Tick 필요
	PrimaryActorTick.bCanEverTick = true;

	// 부모클래스의 MainMeshComponent를 Base_Module처럼 이용

	Body_Module = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Body_Module"));
	Body_Module->SetupAttachment(RootComponent);
	Body_Module->SetMobility(EComponentMobility::Movable);
	Body_Module->bUseAsOccluder = false;
	Body_Module->bNeverDistanceCull = true;
	Body_Module->bAllowCullDistanceVolume = false;

	Top_Module = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Top_Module"));
	Top_Module->SetupAttachment(RootComponent);
	Top_Module->SetMobility(EComponentMobility::Movable);
	Top_Module->bUseAsOccluder = false;
	Top_Module->bNeverDistanceCull = true;
	Top_Module->bAllowCullDistanceVolume = false;

	Top_Empty_Module = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Top_Empty_Module"));
	Top_Empty_Module->SetupAttachment(RootComponent);
	Top_Empty_Module->SetMobility(EComponentMobility::Movable);
	Top_Empty_Module->bUseAsOccluder = false;
	Top_Empty_Module->bNeverDistanceCull = true;
	Top_Empty_Module->bAllowCullDistanceVolume = false;

	// 옥상 항공장애등 ISM (Top_Module 패턴 미러)
	BeaconISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BeaconISM"));
	BeaconISM->SetupAttachment(RootComponent);
	BeaconISM->SetMobility(EComponentMobility::Movable);
	BeaconISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconISM->SetCanEverAffectNavigation(false);
	BeaconISM->bUseAsOccluder = false;
	BeaconISM->bNeverDistanceCull = true;
	BeaconISM->bAllowCullDistanceVolume = false;
	BeaconISM->SetCastShadow(false);

	// 엔진 기본 구체(엔진 콘텐츠 = 항상 쿠킹)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeaconMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (BeaconMeshFinder.Succeeded())
	{
		BeaconISM->SetStaticMesh(BeaconMeshFinder.Object);
	}

	// 비콘 머티리얼은 런타임 로드(UpdateRooftopBeacons) — 에셋이 C++ 이후 생성되어도 재시작 없이 반영(CLAUDE.md 런타임 로드 패턴).

	// 가림 고스트 박스. 머티리얼은 런타임(SetOccluderGhost)에 주입. 기본 숨김.
	GhostBoxSMC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostBoxSMC"));
	GhostBoxSMC->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GhostCubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (GhostCubeFinder.Succeeded())
	{
		GhostBoxSMC->SetStaticMesh(GhostCubeFinder.Object);
	}
	GhostBoxSMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostBoxSMC->SetCanEverAffectNavigation(false);
	GhostBoxSMC->SetCastShadow(false);
	GhostBoxSMC->SetVisibility(false);

	GroundContactSMC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundContactSMC"));
	GroundContactSMC->SetupAttachment(MainMeshComponent);
	GroundContactSMC->SetMobility(EComponentMobility::Movable);
	GroundContactSMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundContactSMC->SetGenerateOverlapEvents(false);
	GroundContactSMC->SetCanEverAffectNavigation(false);
	GroundContactSMC->SetCastShadow(false);
	GroundContactSMC->bUseAsOccluder = false;
	GroundContactSMC->bUseAttachParentBound = true;
	GroundContactSMC->SetVisibility(false);

	// MainMeshComponent 설정
	if (MainMeshComponent)
	{
		// NavMesh는 나중에 필요하면 활성화
		//MainMeshComponent->SetCanEverAffectNavigation(true);

		// 클릭 감지를 위한 충돌 설정
		MainMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// Occlusion Query 비활성화 (Flickering 방지)
		MainMeshComponent->bUseAsOccluder = false;
		MainMeshComponent->bNeverDistanceCull = true;
		MainMeshComponent->bAllowCullDistanceVolume = false;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> BuildingTableObj(
		TEXT("/Script/Engine.DataTable'/Game/CompanyGrowth/Table/Building/BuildingDataTable.BuildingDataTable'")
	);
	if (BuildingTableObj.Succeeded())
	{
		BuildingDataTable = BuildingTableObj.Object;
	}

	// 건물 완성/업그레이드 시 VFX 로드
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BuildCompleteVFXObj(
		TEXT("/Script/Niagara.NiagaraSystem'/Game/F_ToonSmokeAndDust/Fx/NS_Ground_Jump_Fx.NS_Ground_Jump_Fx'")
	);
	if (BuildCompleteVFXObj.Succeeded())
	{
		BuildCompleteVFX = BuildCompleteVFXObj.Object;
	}

}

void ABuildingBaseActor::BeginDestroy()
{
	Super::BeginDestroy();

	if (EntityManager != nullptr)
	{
		EntityManager->RemoveBuilding(this);
	}
}

void ABuildingBaseActor::SetInteractableInfo(const FInteractableInfo& InInfo)
{
	// 새 건물 생성 시 사용
	Super::SetInteractableInfo(InInfo);

	BuildingType = InInfo.BuildingType;

	// 새로 생성된 건물은 Build 태그 (나중에 BuildSuccess에서 Building으로 변경)
	Tags.Add("Build");

	int32 DefaultSkinID = CGBuildingDefaultSkin::FindDefaultSkinID(InInfo.Name);
	if (DefaultSkinID == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] No default skin for %s; using skin 100"),
			*InInfo.Name.ToString());
		DefaultSkinID = 100;
	}

	// 배치 프리뷰부터 종류별 외형을 적용하며, 저장은 배치 완료 경로에서 처리한다.
	ApplySkin(DefaultSkinID, false);
	if (GroundContactSMC) { GroundContactSMC->SetVisibility(false); }
}

void ABuildingBaseActor::InitializeFromSaveData(const FInteractableInfo& InInfo, const FBuildingEntitySaveData& LoadData)
{
	// 저장된 건물 데이터를 먼저 복원 (Super 호출 전에!)
	BuildingIndex = LoadData.BuildingIndex;

	Body_Module_Copies = LoadData.BuildingData.Body_Module_Copies;
	Floor_Height_BodyModuleScale = LoadData.BuildingData.Floor_Height_BodyModuleScale;
	UV_Layout_Selection = LoadData.BuildingData.UV_Layout_Selection;
	Walls_Between_Windows_Switch = LoadData.BuildingData.Walls_Between_Windows_Switch;

	// 건물 강화 데이터 복원 (TMap 통째로 복사 — 타입 개별 분기 불필요)
	EnhancementLevels = LoadData.BuildingData.EnhancementLevels;

	// 회사 타입 복원
	CompanyType = LoadData.BuildingData.CompanyType;

	// 소속 도시 블록(부지) PlotId 복원 — GetBuildingsOnPlot 점유맵이 로드 직후에도 정확하도록
	OwningPlotId = LoadData.PlotId;

	UE_LOG(LogTemp, Log, TEXT("[BuildingBaseActor] Restored building data - Floors: %d, Height: %.2f, CompanyType: %s"),
		Body_Module_Copies, Floor_Height_BodyModuleScale, *CompanyTypeToString(CompanyType));

	// 복원된 데이터로 초기화 (ConstructVisuals가 올바른 층수로 1번만 호출됨)
	Super::SetInteractableInfo(InInfo);

	BuildingType = InInfo.BuildingType;

	// 로드된 데이터는 이미 완성된 건물
	Tags.Add("Building");
	if (GroundContactSMC && !bOccluderGhosted && !bIsFloating)
	{
		GroundContactSMC->SetVisibility(GroundContactSMC->GetStaticMesh() != nullptr);
	}

	// 조명 ID 먼저 멤버에 복원 — ApplySkin 내부의 emissive 재적용이 이 값을 참조함
	if (LoadData.BuildingData.AppliedLightID != 0)
	{
		AppliedLightID = LoadData.BuildingData.AppliedLightID;
	}

	// 저장된 스킨이 있으면 복원 (저장하지 않음) — ApplySkin이 내부에서 조명도 재적용
	if (LoadData.BuildingData.AppliedSkinID != 0)
	{
		ApplySkin(LoadData.BuildingData.AppliedSkinID, false);
	}
	else if (AppliedLightID != 0)
	{
		// 스킨은 default 유지 + 조명만 변경된 세이브 — ApplySkin 우회 경로이므로 emissive 직접 적용
		ApplyLight(AppliedLightID, false);
	}
}

FBuildingEntitySaveData ABuildingBaseActor::GetBuildingSaveData() const
{
	FBuildingEntitySaveData SaveData;

	// 기본 정보 저장
	SaveData.BuildingIndex = BuildingIndex;
	SaveData.InteractableName = InteractableRowName;
	SaveData.Location = GetActorLocation();
	SaveData.Rotation = GetActorRotation();
	SaveData.Scale = GetActorScale();

	// Building 전용 데이터 저장
	SaveData.BuildingData.Body_Module_Copies = Body_Module_Copies;
	SaveData.BuildingData.Floor_Height_BodyModuleScale = Floor_Height_BodyModuleScale;
	SaveData.BuildingData.UV_Layout_Selection = UV_Layout_Selection;
	SaveData.BuildingData.Walls_Between_Windows_Switch = Walls_Between_Windows_Switch;
	SaveData.BuildingData.AppliedSkinID = AppliedSkinID;
	SaveData.BuildingData.AppliedLightID = AppliedLightID;

	// 건물 강화 데이터 저장 (TMap 통째로 저장)
	SaveData.BuildingData.EnhancementLevels = EnhancementLevels;

	// 회사 타입 저장
	SaveData.BuildingData.CompanyType = CompanyType;

	// 소속 도시 블록(부지) PlotId 저장 (NAME_None = 부지 미소속)
	SaveData.PlotId = OwningPlotId;

	return SaveData;
}

FBox ABuildingBaseActor::GetBuildingWorldBounds() const
{
	FBox WorldBox(ForceInit);
	UStaticMeshComponent* const BodyMeshes[] = { MainMeshComponent, Body_Module, Top_Module, Top_Empty_Module };
	for (UStaticMeshComponent* Comp : BodyMeshes)
	{
		if (!Comp || !Comp->GetStaticMesh())
		{
			continue;
		}
		const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Comp);
		if (Instances && Instances->GetInstanceCount() == 0)
		{
			continue;
		}
		// 인스턴스 재적층 직후에는 렌더용 Bounds 캐시가 아직 이전 프레임일 수 있다.
		WorldBox += Comp->CalcBounds(Comp->GetComponentTransform()).GetBox();
	}
	return WorldBox;
}

FVector2D ABuildingBaseActor::GetPlacementFootprintHalfExtent(bool bWorldAxes) const
{
	const FBuildingData* Row = BuildingDataTable && !BuildingID.IsNone()
		? BuildingDataTable->FindRow<FBuildingData>(BuildingID, TEXT("GetPlacementFootprintHalfExtent"))
		: nullptr;
	const FVector2D LocalHalfExtent(
		BuildingFootprintCellSize * FMath::Max(1, Row ? Row->FootprintWidthCells : 1) * 0.5,
		BuildingFootprintCellSize * FMath::Max(1, Row ? Row->FootprintDepthCells : 1) * 0.5);
	if (!bWorldAxes)
	{
		return LocalHalfExtent;
	}

	const double YawRadians = FMath::DegreesToRadians(GetActorRotation().Yaw);
	const double AbsCos = FMath::Abs(FMath::Cos(YawRadians));
	const double AbsSin = FMath::Abs(FMath::Sin(YawRadians));
	return FVector2D(
		LocalHalfExtent.X * AbsCos + LocalHalfExtent.Y * AbsSin,
		LocalHalfExtent.X * AbsSin + LocalHalfExtent.Y * AbsCos);
}

FVector ABuildingBaseActor::GetPlacementFootprintCenter() const
{
	const FBox WorldBounds = GetBuildingWorldBounds();
	if (!WorldBounds.IsValid)
	{
		return GetActorLocation();
	}
	const FVector VisualCenter = WorldBounds.GetCenter();
	return FVector(VisualCenter.X, VisualCenter.Y, GetActorLocation().Z);
}

void ABuildingBaseActor::SetOccluderGhost(bool bOn)
{
	if (bOccluderGhosted == bOn)
	{
		return;
	}

	if (!bOn)
	{
		// 재계산 세터 3개가 고스트 중이면 조기 반환하므로, 플래그를 먼저 내려야 복원이 먹는다
		bOccluderGhosted = false;
		if (GhostBoxSMC)
		{
			GhostBoxSMC->SetVisibility(false);
		}
		UMeshComponent* const BodyMeshes[] = { MainMeshComponent, Body_Module, Top_Module, Top_Empty_Module };
		for (UMeshComponent* Comp : BodyMeshes)
		{
			if (Comp)
			{
				Comp->SetVisibility(true, true);
			}
		}
		if (GroundContactSMC)
		{
			GroundContactSMC->SetVisibility(!bIsFloating && !Tags.Contains("Build")
				&& GroundContactSMC->GetStaticMesh() != nullptr);
		}

		// 스냅샷이 아니라 출처에서 재계산 — 고스트 중 낮/밤이 바뀌었어도 맞는 값이 된다
		ApplyNightVisibility(bGlowVisible);
		return;
	}

	// 머티리얼을 못 얻으면 고스트를 켜지 않는다 — 건물만 사라진 채 남는 게 최악
	static const TSoftObjectPtr<UMaterialInterface> GhostMatPath(
		FSoftObjectPath(TEXT("/Game/CompanyGrowth/Resources/Materials/M_BuildingGhost.M_BuildingGhost")));
	UMaterialInterface* GhostMat = GhostMatPath.LoadSynchronous();
	if (!GhostMat || !GhostBoxSMC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] SetOccluderGhost: M_BuildingGhost 로드 실패 - 고스트 생략"));
		return;
	}

	const FBox WorldBox = GetBuildingWorldBounds();
	if (!WorldBox.IsValid)
	{
		return;
	}

	if (!GhostBoxMID)
	{
		GhostBoxMID = UMaterialInstanceDynamic::Create(GhostMat, this);
		GhostBoxSMC->SetMaterial(0, GhostBoxMID);
	}

	const FVector Origin = WorldBox.GetCenter();
	const FVector Extent = WorldBox.GetExtent();

	// M_BuildingGhost 는 높이를 ObjectPositionWS(=박스 바운드 중심) 기준으로 재므로 전체높이가 아닌 하프높이를 넣는다.
	// 바운드가 퇴화(Extent.Z≈0)하면 셰이더가 0 으로 나눠 inf/NaN 이 되므로 하한을 둔다.
	const float FadeHeightCm = FMath::Max(Extent.Z, 1.0f);
	GhostBoxMID->SetScalarParameterValue(TEXT("FadeHeight"), FadeHeightCm);
	GhostBoxMID->SetScalarParameterValue(TEXT("Opacity"), 0.f);

	// 엔진 Cube 는 한 변 100cm(하프 50) 기준이라 하프익스텐트를 50 으로 나눈다
	GhostBoxSMC->SetWorldLocation(Origin);
	GhostBoxSMC->SetWorldScale3D(FVector(Extent.X / 50.0f, Extent.Y / 50.0f, Extent.Z / 50.0f));

	bOccluderGhosted = true;
	if (GroundContactSMC) { GroundContactSMC->SetVisibility(false); }

	UMeshComponent* const BodyMeshes[] = { MainMeshComponent, Body_Module, Top_Module, Top_Empty_Module };
	for (UMeshComponent* Comp : BodyMeshes)
	{
		if (Comp)
		{
			Comp->SetVisibility(false, true);
		}
	}
	if (BeaconISM)
	{
		BeaconISM->SetVisibility(false, true);
	}

	GhostBoxSMC->SetVisibility(true);
}

void ABuildingBaseActor::SetGhostOpacity(float Opacity)
{
	if (!bOccluderGhosted || !GhostBoxMID)
	{
		return;
	}
	GhostBoxMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Opacity, 0.f, 1.f));
}

float ABuildingBaseActor::Interact()
{
	return 0.0f;
}

bool ABuildingBaseActor::IsRequireBuild() const
{
	return Tags.Contains("Build");
}

// 창문 조명 색상 팔레트 (따뜻한 실내 조명 색상들)
static const TArray<FLinearColor> WindowLightColors = {
	FLinearColor(1.0f, 0.9f, 0.7f, 1.0f),    // 따뜻한 노랑
	FLinearColor(1.0f, 0.85f, 0.6f, 1.0f),   // 연한 오렌지
	FLinearColor(1.0f, 0.95f, 0.85f, 1.0f),  // 따뜻한 흰색
	FLinearColor(1.0f, 0.8f, 0.5f, 1.0f),    // 오렌지
	FLinearColor(1.0f, 0.92f, 0.75f, 1.0f),  // 크림색
	FLinearColor(0.95f, 0.9f, 0.8f, 1.0f),   // 연한 베이지
	FLinearColor(1.0f, 0.88f, 0.7f, 1.0f),   // 밝은 살구색
	FLinearColor(0.98f, 0.95f, 0.9f, 1.0f),  // 소프트 화이트
	FLinearColor(1.0f, 0.82f, 0.55f, 1.0f),  // 황금색
	FLinearColor(1.0f, 0.93f, 0.8f, 1.0f),   // 연한 노랑
};

void ABuildingBaseActor::ApplyMobileFacadeMaterialTuning()
{
	const FBox BuildingBounds = GetBuildingWorldBounds();
	UMeshComponent* const FacadeMeshes[] = { MainMeshComponent, Body_Module, Top_Module, Top_Empty_Module };
	for (UMeshComponent* FacadeMesh : FacadeMeshes)
	{
		MobileFacadeMaterialTuning::ApplyToSlotZero(FacadeMesh, BuildingBounds);
	}
}

void ABuildingBaseActor::BeginPlay()
{
	Super::BeginPlay();
	if (GroundContactSMC)
	{
		const bool bConfigured = BuildingGroundContact::Configure(this, MainMeshComponent, GroundContactSMC);
		GroundContactSMC->SetVisibility(bConfigured && Tags.Contains("Building")
			&& !bIsFloating && !bOccluderGhosted);
	}

	FOnTimelineFloat TimelineProgress;
	TimelineProgress.BindUFunction(this, FName("Wooble_TimelineUpdate"));
	FOnTimelineEvent TimelineFinished;
	TimelineFinished.BindUFunction(this, FName("Wooble_TimelineFinished"));

	Wooble_Timeline.AddInterpFloat(Wooble_Curve, TimelineProgress);
	Wooble_Timeline.SetTimelineFinishedFunc(TimelineFinished);

	// 창문 조명 색상 초기화
	InitializeWindowLightColor();

	// 낮엔 옥상 비콘(additive)을 숨겨 fill 절감 — day/night 전환 구독. 못 찾으면 항상 표시.
	if (ATimeCycleManager* Clock = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass())))
	{
		CachedTimeCycle = Clock;
		Clock->OnVisualTimeChanged.AddDynamic(this, &ABuildingBaseActor::HandleVisualTimeChanged);
		const int32 Cur = Clock->GetVisualTime().ToSeconds();
		const int32 Rise = Clock->GetSunRiseTime().ToSeconds();
		const int32 Set = Clock->GetSunSetTime().ToSeconds();
		bGlowVisible = (Cur >= Set) || (Cur < Rise);
		ApplyNightVisibility(bGlowVisible);
	}
}

void ABuildingBaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(NewFloorRiseTimerHandle);
	PendingFloorRecruitTicketCount = 0;
	NewFloorRiseInstanceIndex = INDEX_NONE;

	if (ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		Clock->OnVisualTimeChanged.RemoveDynamic(this, &ABuildingBaseActor::HandleVisualTimeChanged);
	}

	// 레벨 전환/철거 시 고스트 상태가 남지 않게 (핸들러가 못 따라오는 경로 대비)
	SetOccluderGhost(false);

	Super::EndPlay(EndPlayReason);
}

void ABuildingBaseActor::ApplyNightVisibility(bool bNight)
{
	bGlowVisible = bNight;

	// 고스트 중에는 본체가 없으므로 비콘도 띄우지 않는다. 상태만 기록해 두고 복원 시 재적용.
	if (bOccluderGhosted && bNight)
	{
		return;
	}

	if (BeaconISM)
	{
		BeaconISM->SetVisibility(bNight, true);
	}
}

void ABuildingBaseActor::HandleVisualTimeChanged()
{
	if (const ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		ApplyNightVisibility(Clock->IsVisualNight());
	}
}

void ABuildingBaseActor::InitializeWindowLightColor()
{
	// 10개 색상 중 랜덤 선택
	int32 RandomIndex = FMath::RandRange(0, WindowLightColors.Num() - 1);
	Window_Emissive_Color = WindowLightColors[RandomIndex];

	// 머티리얼에 적용
	ApplyWindowLightState();
}

void ABuildingBaseActor::ApplyWindowLightState()
{
	const UWorld* BuildingWorld = GetWorld();
	const bool bMobileES31 = BuildingWorld
		&& BuildingWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1;
	const FLinearColor WriteColor = MobileFacadeMaterialTuning::ResolveWindowEmissiveColor(
		Window_Emissive_Color,
		bMobileES31);
	const float Density = bWindowLightActive ? ActiveWindowLightDensity : IdleWindowLightDensity;

	UMeshComponent* const FacadeMeshes[] = { MainMeshComponent, Body_Module, Top_Module, Top_Empty_Module };
	for (UMeshComponent* FacadeMesh : FacadeMeshes)
	{
		MobileFacadeMaterialTuning::SetWindowEmissiveColorOnMaterials(FacadeMesh, WriteColor);
		MobileFacadeMaterialTuning::SetWindowLightDensityOnMaterials(FacadeMesh, Density);
	}
}

bool ABuildingBaseActor::bForceWindowLightsOn = false;

void ABuildingBaseActor::SetWindowLightActive(bool bActive)
{
	// [치트] 전역 강제 점등 중이면 꺼짐 요청 무시 — 운영 여부/버블 재평가와 무관하게 진행중 비율 유지
	if (bForceWindowLightsOn) { bActive = true; }

	// 외부에서 ApplyLight/ApplySkin 등이 같은 머티리얼 파라미터를 건드릴 수 있으므로
	// 캐시 가드를 두지 않고 호출 시 무조건 머티리얼을 정본 상태로 동기화
	bWindowLightActive = bActive;
	ApplyWindowLightState();
}

void ABuildingBaseActor::InitializeStaticMesh(const FName& name)
{
	Super::InitializeStaticMesh(name);

	BuildingID = name;

	UE_LOG(LogTemp, Log, TEXT("BuildingID: %s"), *BuildingID.ToString());

	// BuildingInfo
	if (BuildingDataTable)
	{
		// BuildingID  
		const FBuildingData* BuildingInfo = BuildingDataTable->FindRow<FBuildingData>(BuildingID, TEXT("OnConstruction"));
		if (BuildingInfo)
		{
			ConstructVisuals(BuildingInfo);
		}
	}
}

void ABuildingBaseActor::RegisterWithEntityManager()
{
	if (EntityManager != nullptr)
	{
		bool bRequireBuild = Tags.Contains("Build");
		EntityManager->AddBuilding(this, bRequireBuild);
	}
}

void ABuildingBaseActor::UnregisterWithEntityManager()
{
	if (EntityManager != nullptr)
	{
		EntityManager->RemoveBuilding(this);
	}
}

void ABuildingBaseActor::BuildSuccess()
{
	Tags.Remove("Build");
	Tags.Add("Building");
	if (GroundContactSMC && !bOccluderGhosted && !bIsFloating)
	{
		GroundContactSMC->SetVisibility(GroundContactSMC->GetStaticMesh() != nullptr);
	}

	if (EntityManager != nullptr)
	{
		EntityManager->ChangeBuildState(this, false);

		// 건물 완성 시 다음 건물 해금
		EntityManager->UnlockNextBuilding();
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URecruitmentManagerSubsystem* RecruitmentMgr =
			GameInstance->GetSubsystem<URecruitmentManagerSubsystem>())
		{
			// 튜토리얼 안내가 다음 걸음으로 건물·버튼을 짚는 순간이라 가운데 띠 대신 하단 작은 알림으로 알린다
			const UMissionManagerSubsystem* Missions = GameInstance->GetSubsystem<UMissionManagerSubsystem>();
			const bool bTutorialChain = Missions && Missions->HasActiveMission();
			const int32 Granted = RecruitmentMgr->ReconcilePermanentCapacityTickets(false, !bTutorialChain);
			if (bTutorialChain)
			{
				PendingFloorRecruitTicketCount += Granted;
				ShowPendingFloorRecruitTicketNotification();
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] BuildSuccess called"));

	// VFX는 PlacementHandler에서 ReCalcBoxExtent 이후에 재생됨
}

void ABuildingBaseActor::ConstructVisuals(const FBuildingData* BuildingData)
{
	if (!BuildingData)
		return;

	// footprint 칸수 캐시 — 증축 비용 배수. 홀드(10Hz) 경로가 매번 DT 를 뒤지지 않게 한다.
	CachedFootprintCells = FMath::Max(1, BuildingData->FootprintWidthCells * BuildingData->FootprintDepthCells);

	// DataTable에서 Material 로드
	Material_Main = BuildingData->MainMaterial.LoadSynchronous();
	Material_RoofElements = BuildingData->RoofElementsMaterial.LoadSynchronous();

	// 인스턴스 초기화
	Body_Module->ClearInstances();
	Top_Module->ClearInstances();
	Top_Empty_Module->ClearInstances();

	// DataTable에서 메시 로드 및 설정
	MainMeshComponent->SetStaticMesh(BuildingData->BaseMesh.LoadSynchronous());
	Body_Module->SetStaticMesh(BuildingData->BodyMesh.LoadSynchronous());
	Top_Module->SetStaticMesh(BuildingData->TopMesh.LoadSynchronous());
	Top_Empty_Module->SetStaticMesh(BuildingData->TopEmptyMesh.LoadSynchronous());

	// 표시 배수는 **메시 컴포넌트에만** 건다. 액터 스케일을 쓰면 안 된다 —
	// Wobble 은 "Wobble Center"/"Wobble Vector" 에 월드 좌표를 넣고, 층 상승(FloatingHeight)과 카메라 프레이밍은
	// 고정 world cm 를 쓴다. 전부 액터 스케일 1 을 전제한 코드라 루트를 스케일하면 그 연출들이 어긋난다.
	// (문서의 "SetActorScale3D 메시 리사이즈 = 과거 의도적 제거" 경고가 이 지점이다.)
	AppliedMeshScale = FMath::Max(0.05f, BuildingData->MeshScale);
	{
		const FVector MeshScaleVec(AppliedMeshScale);
		MainMeshComponent->SetRelativeScale3D(MeshScaleVec);   // NavBlocker 는 여기 붙어 있어 같이 따라온다
		Body_Module->SetRelativeScale3D(MeshScaleVec);         // 인스턴스 위치는 컴포넌트 공간이라 적층도 같이 줄어든다
		Top_Module->SetRelativeScale3D(MeshScaleVec);
		Top_Empty_Module->SetRelativeScale3D(MeshScaleVec);
		if (BeaconISM) { BeaconISM->SetRelativeScale3D(MeshScaleVec); }
	}
	if (GroundContactSMC)
	{
		BuildingGroundContact::Configure(this, MainMeshComponent, GroundContactSMC);
		GroundContactSMC->SetVisibility(false);
	}

	if (Material_Main)
	{
		// 기본 Material 적용 (스킨 적용 전 임시 Material)
		MainMeshComponent->SetMaterial(0, Material_Main);
		Body_Module->SetMaterial(0, Material_Main);
		Top_Module->SetMaterial(0, Material_Main);
	}

	if (Material_RoofElements)
	{
		// 지붕 Material을 슬롯 1에 적용
		Top_Module->SetMaterial(1, Material_RoofElements);
	}

	// UV 레이아웃 설정 (Material 커스텀 파라미터 index 0)
	MainMeshComponent->SetCustomPrimitiveDataFloat(0, UV_Layout_Selection);
	Body_Module->SetCustomPrimitiveDataFloat(0, UV_Layout_Selection);
	Top_Module->SetCustomPrimitiveDataFloat(0, UV_Layout_Selection);

	// 벽/창문 스위치 설정 (Material 커스텀 파라미터 index 1)
	MainMeshComponent->SetCustomPrimitiveDataFloat(1, Walls_Between_Windows_Switch);
	Body_Module->SetCustomPrimitiveDataFloat(1, Walls_Between_Windows_Switch);
	Top_Module->SetCustomPrimitiveDataFloat(1, Walls_Between_Windows_Switch);

	// 각 모듈의 높이 계산
	FVector Base_Min(ForceInit), Base_Max(ForceInit);
	MainMeshComponent->GetLocalBounds(Base_Min, Base_Max);
	const float Base_Height_CM = Base_Max.Z;

	FVector Body_Min(ForceInit), Body_Max(ForceInit);
	Body_Module->GetLocalBounds(Body_Min, Body_Max);
	const float Body_Height_CM = Body_Max.Z;

	FVector Top_Min(ForceInit), Top_Max(ForceInit);
	Top_Module->GetLocalBounds(Top_Min, Top_Max);
	const float Top_Height_CM = Top_Max.Z;

	FVector TopEmpty_Min(ForceInit), TopEmpty_Max(ForceInit);
	Top_Empty_Module->GetLocalBounds(TopEmpty_Min, TopEmpty_Max);
	const float TopEmpty_Height_CM = TopEmpty_Max.Z;

	float finalHeight = RebuildBodyModules();

	// Top Module 위치 설정
	FTransform TopTransform;
	TopTransform.SetLocation(FVector(0.f, 0.f, finalHeight));
	Top_Module->AddInstance(TopTransform);

	UpdateRooftopBeacons();
}

float ABuildingBaseActor::RebuildBodyModules()
{
	if (!Body_Module) return 0.0f;

	Body_Module->ClearInstances();

	// Base 높이
	FVector Base_Min(ForceInit), Base_Max(ForceInit);
	MainMeshComponent->GetLocalBounds(Base_Min, Base_Max);
	float TotalHeight = Base_Max.Z;

	// Body 높이
	FVector Body_Min(ForceInit), Body_Max(ForceInit);
	Body_Module->GetLocalBounds(Body_Min, Body_Max);
	const float Body_Height_CM = Body_Max.Z;

	FVector BodyModule_ScaleVector = FVector(1.0f, 1.0f, Floor_Height_BodyModuleScale);

	// Body_Module 인스턴스들 생성
	for (int32 i = 0; i < Body_Module_Copies + 1; i++)
	{
		FTransform BodyModuleTransform;
		BodyModuleTransform.SetLocation(FVector(0.f, 0.f, TotalHeight));
		BodyModuleTransform.SetScale3D(BodyModule_ScaleVector);
		Body_Module->AddInstance(BodyModuleTransform);

		TotalHeight += (Floor_Height_BodyModuleScale * Body_Height_CM);
	}

	UpdateBuildingInfo(TotalHeight); // 정보 업데이트

	return TotalHeight; // Top Module 위치를 위해 반환
}

float ABuildingBaseActor::CalculateTopModuleHeight() const
{
	// GetLocalBounds 는 메시가 없으면 출력을 쓰지 않는다 — 미초기화면 기기마다 쓰레기 값(-inf 실측).
	FVector Base_Min(ForceInit), Base_Max(ForceInit);
	MainMeshComponent->GetLocalBounds(Base_Min, Base_Max);
	float TotalHeight = Base_Max.Z;

	FVector Body_Min(ForceInit), Body_Max(ForceInit);
	Body_Module->GetLocalBounds(Body_Min, Body_Max);
	const float Body_Height_CM = Body_Max.Z;

	// Body Module들의 총 높이 계산 (재구성 없이)
	for (int32 i = 0; i < Body_Module_Copies + 1; i++)
	{
		TotalHeight += (Floor_Height_BodyModuleScale * Body_Height_CM);
	}

	return TotalHeight;
}

bool ABuildingBaseActor::TryGetOfficeFloorHeight(float& OutLocalHeightCm) const
{
	if (!IsValid(MainMeshComponent) || !IsValid(MainMeshComponent->GetStaticMesh())
		|| !IsValid(Body_Module) || !IsValid(Body_Module->GetStaticMesh())
		|| Body_Module_Copies < 0 || Body_Module_Copies == MAX_int32
		|| !FMath::IsFinite(Floor_Height_BodyModuleScale) || Floor_Height_BodyModuleScale <= 0.f
		|| !FMath::IsFinite(Standard_FloorHeight) || Standard_FloorHeight <= 0.f)
	{
		return false;
	}

	FVector BaseMin, BaseMax, BodyMin, BodyMax;
	MainMeshComponent->GetLocalBounds(BaseMin, BaseMax);
	Body_Module->GetLocalBounds(BodyMin, BodyMax);
	const float BodyHeightCm = static_cast<float>(BodyMax.Z);
	const float StandardFloorHeightCm = Standard_FloorHeight * 100.f;
	if (BaseMin.ContainsNaN() || BaseMax.ContainsNaN() || BodyMin.ContainsNaN() || BodyMax.ContainsNaN()
		|| BaseMax.Z < 0.0 || !FMath::IsFinite(BodyHeightCm) || BodyHeightCm <= 0.f
		|| !FMath::IsFinite(StandardFloorHeightCm))
	{
		return false;
	}

	// 기존 적층과 같은 Body Max.Z를 사용하며, 짧은 모듈에서는 한 층 높이를 모듈 높이로 제한한다.
	const float FloorHeightCm = FMath::Min(BodyHeightCm, StandardFloorHeightCm) * Floor_Height_BodyModuleScale;
	const float TopModuleHeightCm = CalculateTopModuleHeight();
	const float OfficeFloorHeightCm = TopModuleHeightCm - FloorHeightCm;
	if (!FMath::IsFinite(FloorHeightCm) || FloorHeightCm <= 0.f
		|| !FMath::IsFinite(TopModuleHeightCm) || !FMath::IsFinite(OfficeFloorHeightCm)
		|| OfficeFloorHeightCm <= 0.f || OfficeFloorHeightCm < BaseMax.Z || OfficeFloorHeightCm >= TopModuleHeightCm)
	{
		return false;
	}

	OutLocalHeightCm = OfficeFloorHeightCm;
	return true;
}

float ABuildingBaseActor::GetTotalBuildingHeight() const
{
	// Top Module 시작 위치 계산
	float TopModuleStartHeight = CalculateTopModuleHeight();

	// Top Module 높이 추가
	FVector Top_Min(ForceInit), Top_Max(ForceInit);
	Top_Module->GetLocalBounds(Top_Min, Top_Max);
	float TopModuleHeight = Top_Max.Z;

	return TopModuleStartHeight + TopModuleHeight;
}

float ABuildingBaseActor::GetTotalBuildingHeightWorld() const
{
	const FBox BuildingBounds = GetBuildingWorldBounds();
	return BuildingBounds.IsValid
		? FMath::Max(0.f, static_cast<float>(BuildingBounds.Max.Z - GetActorLocation().Z))
		: 0.f;
}

FBox ABuildingBaseActor::GetRoofBounds() const
{
	// 지붕은 상태에 따라 Top_Module 또는 Top_Empty_Module 에 들어감 — 둘 다 확인
	FBox Box(ForceInit);
	if (Top_Module && Top_Module->GetInstanceCount() > 0)
	{
		Box += Top_Module->Bounds.GetBox();
	}
	if (Top_Empty_Module && Top_Empty_Module->GetInstanceCount() > 0)
	{
		Box += Top_Empty_Module->Bounds.GetBox();
	}
	if (!Box.IsValid && MainMeshComponent)
	{
		Box = MainMeshComponent->Bounds.GetBox();
	}
	return Box;
}

void ABuildingBaseActor::UpdateRooftopBeacons()
{
	if (!BeaconISM)
		return;

	// 비콘 머티리얼 런타임 로드(에셋이 생성돼 있으면 적용 — 없으면 엔진 기본 메시 머티리얼 유지)
	if (UMaterialInterface* BeaconMat = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Environment/Beacon/MI_AviationBeacon.MI_AviationBeacon"))).LoadSynchronous())
	{
		BeaconISM->SetMaterial(0, BeaconMat);
	}

	BeaconISM->ClearInstances();

	// 옥상 윗면 로컬 Z = 건물 총높이(결정적: 로컬 바운드에서 재계산, 컴포넌트 Bounds 타이밍 무관)
	const float RoofTopLocalZ = GetTotalBuildingHeight();

	// 높이 게이팅: 기준 미만이면 0개
	if (RoofTopLocalZ < BeaconMinHeight)
		return;

	if (!Top_Module)
		return;

	// Top 모듈이 얹히는 로컬 Z(= 지붕 메시 원점). 소켓 오프셋(Z)의 기준점.
	const float TopOriginZ = CalculateTopModuleHeight();

	// 위치 + 최종 스케일을 먼저 수집한 뒤 일괄 배치.
	TArray<FVector> Positions;
	TArray<FVector> Scales; // 위치별 최종 스케일(베이스 × 소켓 Scale)

	// (B) 타입별 지정: TopMesh 비콘 소켓(있으면 우선).
	//     이름 규칙: "L_Beacon*" = 큰 비콘(BeaconScaleLarge), "Beacon*" = 보통(BeaconScaleNormal).
	//     소켓 RelativeLocation 은 지붕 메시 로컬(=Top 모듈 로컬) → Top 원점 Z 만 더함(성장 따라감).
	int32 SocketCount = 0;
	if (UStaticMesh* TopMesh = Top_Module->GetStaticMesh())
	{
		for (UStaticMeshSocket* Socket : TopMesh->Sockets)
		{
			if (!Socket)
				continue;

			const FString SockName = Socket->SocketName.ToString();
			const bool bLarge = SockName.StartsWith(TEXT("L_Beacon"));
			const bool bBeacon = bLarge || SockName.StartsWith(TEXT("Beacon"));
			if (!bBeacon)
				continue;

			const FVector L = Socket->RelativeLocation;
			Positions.Add(FVector(L.X, L.Y, TopOriginZ + L.Z));
			const float Base = bLarge ? BeaconScaleLarge : BeaconScaleNormal;
			Scales.Add(FVector(Base) * Socket->RelativeScale);
			++SocketCount;
		}
	}

	// (A) 폴백: 비콘 소켓이 없는 타입은 지붕 바운드에서 자동 중앙/4모서리.
	//     이름 신호가 없으므로 개수로 크기 결정(1~2개=Large, 4개=Normal).
	if (SocketCount == 0)
	{
		FVector TopMin(ForceInit), TopMax(ForceInit);
		Top_Module->GetLocalBounds(TopMin, TopMax);
		const float RoofCenterX = (TopMin.X + TopMax.X) * 0.5f;
		const float RoofCenterY = (TopMin.Y + TopMax.Y) * 0.5f;
		const float RoofSizeX = TopMax.X - TopMin.X;
		const float RoofSizeY = TopMax.Y - TopMin.Y;
		const float BeaconZ = RoofTopLocalZ + BeaconZLift;

		const bool bWide =
			RoofSizeX >= BeaconFourCornerMinExtent && RoofSizeY >= BeaconFourCornerMinExtent;

		if (bWide)
		{
			const float InsetX = FMath::Min(BeaconCornerInset, RoofSizeX * 0.45f);
			const float InsetY = FMath::Min(BeaconCornerInset, RoofSizeY * 0.45f);
			const float HX = RoofSizeX * 0.5f - InsetX;
			const float HY = RoofSizeY * 0.5f - InsetY;
			Positions.Add(FVector(RoofCenterX - HX, RoofCenterY - HY, BeaconZ));
			Positions.Add(FVector(RoofCenterX + HX, RoofCenterY - HY, BeaconZ));
			Positions.Add(FVector(RoofCenterX - HX, RoofCenterY + HY, BeaconZ));
			Positions.Add(FVector(RoofCenterX + HX, RoofCenterY + HY, BeaconZ));
		}
		else
		{
			Positions.Add(FVector(RoofCenterX, RoofCenterY, BeaconZ));
		}

		const float Base = (Positions.Num() <= 2) ? BeaconScaleLarge : BeaconScaleNormal;
		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			Scales.Add(FVector(Base));
		}
	}

	// 일괄 배치
	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		FTransform T;
		T.SetLocation(Positions[i]);
		T.SetScale3D(Scales[i]);
		BeaconISM->AddInstance(T);
	}

	// [임시 검증 로그 — 튜닝 후 제거]
	UE_LOG(LogTemp, Warning, TEXT("[Beacon] %s count=%d sockets=%d roofTopZ=%.0f"),
		*GetName(), BeaconISM->GetInstanceCount(), SocketCount, RoofTopLocalZ);
}

FVector ABuildingBaseActor::GetRoofAnchorPosition() const
{
	// 지붕 모듈의 월드 바운드 가로세로 중심 + 상단
	const FBox B = GetRoofBounds();
	return FVector(B.GetCenter().X, B.GetCenter().Y, B.Max.Z);
}

FVector ABuildingBaseActor::GetBubbleAnchorPosition() const
{
	// MainMeshComponent 로컬 바운드에서 수평 중심 계산
	FVector BoundsMin(ForceInit), BoundsMax(ForceInit);
	MainMeshComponent->GetLocalBounds(BoundsMin, BoundsMax);
	FVector LocalCenter = (BoundsMin + BoundsMax) * 0.5f;
	LocalCenter.Z = 0.0f;

	FVector WorldCenter = MainMeshComponent->GetComponentTransform().TransformPosition(LocalCenter);

	return FVector(WorldCenter.X, WorldCenter.Y,
		GetActorLocation().Z + GetTotalBuildingHeightWorld());
}

void ABuildingBaseActor::UpdateBuildingInfo(const float TotalHeight)
{
	// Body_Module 높이 정보
	FVector Body_Min(ForceInit), Body_Max(ForceInit);
	Body_Module->GetLocalBounds(Body_Min, Body_Max);
	const float Body_Height_CM = Body_Max.Z;

	// 층수 계산
	float Body_Height_M = Body_Height_CM / 100.f;
	float FloorsPerBodyModule = Body_Height_M / Standard_FloorHeight;
	NumberStories = FText::AsNumber(FMath::RoundToInt(FloorsPerBodyModule * (Body_Module_Copies + 1)));

	// Top Module 높이 가져오기
	FVector Top_Min(ForceInit), Top_Max(ForceInit);
	Top_Module->GetLocalBounds(Top_Min, Top_Max);
	const float Top_Height_CM = Top_Max.Z;

	// Building_Height
	Building_Height = FText::AsNumber(FMath::RoundToInt((TotalHeight + Top_Height_CM) / 100));
}

void ABuildingBaseActor::AddFloor(bool bShouldSave)
{
	// UpgradeEnhancement 외부 호출과 오염된 세이브에도 안전하도록 이 경계에서 다시 정규화한다.
	const int32 CurrentFloorLevel = FMath::Max(Body_Module_Copies, 0);
	Body_Module_Copies = CurrentFloorLevel;
	if (CurrentFloorLevel >= MAX_int32)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] Building floor is at int32 limit"));
		return;
	}

	// 층 추가 전 현재 높이 계산 (새 층이 추가될 위치)
	float oldHeight = CalculateTopModuleHeight();

	const int64 NewFloorLevel = static_cast<int64>(CurrentFloorLevel) + 1LL;
	Body_Module_Copies = static_cast<int32>(NewFloorLevel);
	float finalHeight = RebuildBodyModules(); // Body Module만 재구성

	// Top Module도 업데이트 (위치 변경)
	Top_Module->ClearInstances();
	FTransform TopTransform;
	TopTransform.SetLocation(FVector(0.f, 0.f, finalHeight));
	Top_Module->AddInstance(TopTransform);

	// Box Collision 높이도 업데이트
	ReCalcBoxExtent();
	ApplyMobileFacadeMaterialTuning();

	// 옥상 높이 변경 → 항공장애등 재배치
	UpdateRooftopBeacons();

	// 강화로 새 층 추가 — Drop thud 즉시 + 짧은 Rumble 깔기 (단발성이라 placement 보다 짧게 0.6s)
	if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
	{
		SoundMgr->PlaySound(FName("Building_FloorUp_Drop"));
		SoundMgr->PlaySoundWithDuration(FName("Building_FloorUp_Rumble"), 0.6f, 0.8f);
	}

	// thud 와 동기화된 카메라 셰이크 (배치 착지보다 약하게)
	if (!USettingsManagerSubsystem::IsReduceMotion(this))
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->ClientStartCameraShake(UBuildingLandCameraShake::StaticClass(), 0.35f);
		}
	}

	// 건물 업그레이드 시 먼지 VFX 재생 (추가되기 전 높이 = 새 층 바로 아래)
	PlayBuildCompleteVFX(oldHeight);

	// 새 층 밀어올림 연출 — 최상단 인스턴스만 아래에서 제자리로 (통짜 승강 아님)
	StartNewFloorRiseAnimation();

	if (bShouldSave)
	{
		SaveGameData();
	}
}

void ABuildingBaseActor::PlayPlacementLandSequence()
{
	USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr;
	if (!SoundMgr) return;

	// 떨어지는 시간(UpdateFloatingAnimation 기준 ≈ 0.25s) 에 맞춰 Whoosh cap. wav 0.72s 라 그대로면 시각 끝나고도 길게 들림
	SoundMgr->PlaySoundWithDuration(FName("Building_FloorUp_Whoosh"), 0.25f);

	// Body thud peak offset 0.048s — 0.25 - 0.048 = 0.202s 후 호출하면 peak 가 정확히 빌딩 착지 frame 과 일치
	FTimerHandle ThudHandle;
	TWeakObjectPtr<USoundManagerSubsystem> WeakSoundMgr(SoundMgr);
	TWeakObjectPtr<ABuildingBaseActor> WeakThis(this);
	GetWorldTimerManager().SetTimer(ThudHandle, FTimerDelegate::CreateLambda(
		[WeakSoundMgr, WeakThis]()
		{
			if (!WeakSoundMgr.IsValid()) return;
			WeakSoundMgr->PlaySound(FName("Building_FloorUp_Body"));
			// Rumble wav 0~0.8s 무음/약한 ramp-up 구간 skip — envelope 25% 도달 지점부터 재생해 즉시 들리게
			WeakSoundMgr->PlaySoundWithDuration(FName("Building_FloorUp_Rumble"), 1.5f, 0.8f);

			// thud 피크와 동기화된 카메라 셰이크
			if (WeakThis.IsValid() && !USettingsManagerSubsystem::IsReduceMotion(WeakThis.Get()))
			{
				if (APlayerController* PC = WeakThis->GetWorld()->GetFirstPlayerController())
				{
					PC->ClientStartCameraShake(UBuildingLandCameraShake::StaticClass(), 0.7f);
				}
			}
		}), 0.202f, false);
}

void ABuildingBaseActor::ReCalcBoxExtent() const
{
	// 부모 OnConstruction 은 메시 주입 전에 이 함수를 부른다 — 맞출 대상이 없으면 상자를 건드리지 않는다.
	if (MainMeshComponent == nullptr || BoxComponent == nullptr || !MainMeshComponent->GetStaticMesh())
	{
		return;
	}

	FVector boundsMin(ForceInit), boundsMax(ForceInit);
	MainMeshComponent->GetLocalBounds(boundsMin, boundsMax);

	// X, Y는 Base Module의 bounds 기준으로 사용
	boundsMax /= 100;
	boundsMax.X = FMath::RoundToFloat(boundsMax.X);
	boundsMax.Y = FMath::RoundToFloat(boundsMax.Y);

	// Z는 전체 건물 높이로 계산 (Body Module들 + Top Module 포함)
	float totalHeight = CalculateTopModuleHeight();

	// Top Module 높이도 추가
	FVector Top_Min(ForceInit), Top_Max(ForceInit);
	Top_Module->GetLocalBounds(Top_Min, Top_Max);
	const float Top_Height_CM = Top_Max.Z;
	totalHeight += Top_Height_CM;
	// /fp:fast 라 비교식으로는 NaN/Inf 를 못 거른다. 한 번 NaN 이 된 상자는 SetRelativeLocation 으로 복구되지 않는다.
	if (!FMath::IsFinite(totalHeight) || !FMath::IsFinite(AppliedMeshScale))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] ReCalcBoxExtent skipped - non-finite height %f (%s)"),
			totalHeight, *GetName());
		return;
	}

	// BoxExtent.Z = totalHeight / 2 (중심에서 위/아래로 반씩)
	float halfHeight = totalHeight * 0.5f;
	float heightInMeters = halfHeight / 100.0f;
	boundsMax.Z = FMath::RoundToFloat(heightInMeters);

	boundsMax *= 100; // 다시 cm 단위로

	// 각 축을 독립적으로 처리 (직사각형 건물 지원)
	boundsMax.X = std::max(boundsMax.X, 100.0);
	boundsMax.Y = std::max(boundsMax.Y, 100.0);
	boundsMax.Z = std::max(boundsMax.Z, 100.0);

	// 표시 배수 반영 — 위 치수는 전부 GetLocalBounds(메시 로컬) 기준이고 BoxComponent 는 스케일되지 않은
	// 루트에 붙어 있어서, 곱하지 않으면 클릭 볼륨이 줄어든 건물보다 커진다(빈 공간이 눌린다).
	boundsMax *= AppliedMeshScale;

	float boundGap = BoundGap * 100;
	boundsMax += FVector(boundGap, boundGap, boundGap);

	BoxComponent->SetBoxExtent(boundsMax);
	BoxComponent->SetWorldRotation(UKismetMathLibrary::MakeRotFromX(FVector(1, 0, 0)));

	// BoxComponent 중심을 건물 높이의 절반 지점으로 이동 (totalHeight 는 로컬 → 표시 배수 반영)
	FVector BoxCenter = FVector(0.f, 0.f, totalHeight * 0.5f * AppliedMeshScale);
	BoxComponent->SetRelativeLocation(BoxCenter);
	// 공유(아키타입) 바디셋업에서 갈라진 뒤 SetBoxExtent 는 물리 모양을 옛 기본 크기로 다시 만든다 — 트레이스가 새 크기를 보게 재생성.
	if (BoxComponent->IsPhysicsStateCreated())
	{
		BoxComponent->RecreatePhysicsState();
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildingBaseActor] ReCalcBoxExtent - Total Height: %.2f cm, Box Extent Z: %.2f, Box Center Z: %.2f"),
		totalHeight, boundsMax.Z, BoxCenter.Z);

	// 델리게이트 브로드캐스트 (PlacementHandler의 PostPlacementTargetRecalcBoxExtent 호출)
	if (ReCalcBoxExtentDelegate != nullptr)
	{
		ReCalcBoxExtentDelegate->Broadcast();
	}
}

void ABuildingBaseActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	//     OnConstruction   
	if (GetWorld()->IsGameWorld())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("OnConstruction called - BuildingID: %s"),
		*BuildingID.ToString());

	if (BuildingID.IsNone())
	{
		if (GroundContactSMC) { GroundContactSMC->SetVisibility(false); }
		if (MainMeshComponent) MainMeshComponent->SetStaticMesh(nullptr);
		if (Body_Module) Body_Module->ClearInstances();
		if (Top_Module) Top_Module->ClearInstances();
		if (Top_Empty_Module) Top_Empty_Module->ClearInstances();
		return;
	}

	// BuildingInfo
	if (BuildingDataTable)
	{
		// BuildingID  
		const FBuildingData* BuildingInfo = BuildingDataTable->FindRow<FBuildingData>(BuildingID, TEXT("OnConstruction"));
		if (BuildingInfo)
		{
			ConstructVisuals(BuildingInfo);
		}
	}
}

void ABuildingBaseActor::OnInteract_Implementation(APlayerController* PC)
{
	// 방문 모드에서는 인터랙션 차단
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (GI->IsVisitMode())
		{
			return;
		}
	}

	bIsInteracting = true;

	// 기본 방향으로 흔들기
	PlayWobble();

	UE_LOG(LogTemp, Log, TEXT("[ABuildingBaseActor] OnInteract_Implementation"));
}

void ABuildingBaseActor::SetCompanyType(ECompanyType NewType)
{
	CompanyType = NewType;
	SaveGameData();
	OnBubbleRefreshRequested.Broadcast(BuildingIndex);
}

void ABuildingBaseActor::OpenManagePanel(AMainMapPlayerController* MainPC)
{
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] GameInstance is null"));
		return;
	}

	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();

	if (!TableManager || !UIManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] TableManager or UIManager is null"));
		return;
	}

	TSubclassOf<UUserWidget> BuildingManagePanel = TableManager->GetWidgetClass(EWidgetType::BuildingManage);
	if (!BuildingManagePanel)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] BuildingManagePanel class not found in table"));
		return;
	}

	UUIBase* UIBase = UIManager->GetUIBase();
	if (!UIBase)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] UIBase is null"));
		return;
	}

	// 월드 조작 중 열린 공장 패널은 건물 관리 패널로 전환할 때 스택에서 정리한다.
	if (Cast<UFactoryPanelWidget>(UIBase->GetActiveBottomWidget()))
	{
		UIBase->PopBottomWidget();
	}

	// 패널 push 후 대상 건물 주입. 카메라 프레이밍은 SetTargetBuilding에서
	UCommonActivatableWidget* PushedWidget = UIBase->PushBottomClass(BuildingManagePanel.Get());
	UBuildingManagePanelWidget* ManagePanelWidget = Cast<UBuildingManagePanelWidget>(PushedWidget);

	if (ManagePanelWidget)
	{
		ManagePanelWidget->SetTargetBuilding(this);
		MainPC->GoToUIMode();
		UE_LOG(LogTemp, Log, TEXT("[BuildingBaseActor] BuildingManagePanel opened with camera focus and UI mode"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] Failed to cast to BuildingManagePanelWidget"));
	}
}

void ABuildingBaseActor::OpenBuildingUI(AMainMapPlayerController* MainPC)
{
	if (!MainPC) return;

	// 산업은 건설 시점(빌드 모달)에 베이크되므로 건물 클릭 = 항상 ManagePanel
	OpenManagePanel(MainPC);
}

void ABuildingBaseActor::OnEndInteract_Implementation(APlayerController* PC)
{
	if (!bIsInteracting)
	{
		return;
	}

	AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(PC);
	if (!MainPC || MainPC->GetCurrentInputMode() != EInputMode::Normal)
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildingBaseActor] Ignoring click - not in Normal mode"));
		return;
	}

	OpenBuildingUI(MainPC);

	bIsInteracting = false;
}

void ABuildingBaseActor::Wooble_TimelineUpdate(float scaleValue)
{
	// Wobble 스칼라는 PlayWobble에서 1회 ON, Wooble_TimelineFinished에서 1회 OFF (0/1 게이트).
	// 실제 흔들림은 머티리얼 WPO가 Wobble StartTime 기준 원샷으로 구동하므로 매 프레임 재푸시 불필요.
}

void ABuildingBaseActor::Wooble_TimelineFinished()
{
	// 모든 모듈의 Wobble 값을 0으로 리셋
	MainMeshComponent->SetScalarParameterValueOnMaterials("Wobble", 0.f);
	Body_Module->SetScalarParameterValueOnMaterials("Wobble", 0.f);
	Top_Module->SetScalarParameterValueOnMaterials("Wobble", 0.f);
	SetActorTickEnabled(false);
}

void ABuildingBaseActor::PlayWobble()
{
	// Timeline이 실행 중이면 완전히 정지하고 초기화
	if (Wooble_Timeline.IsPlaying())
	{
		Wooble_Timeline.Stop();
		Wooble_Timeline.SetNewTime(0.0f);  // Timeline 시간을 0으로 리셋
		// 모든 파라미터 초기화
		MainMeshComponent->SetScalarParameterValueOnMaterials("Wobble", 0.f);
		Body_Module->SetScalarParameterValueOnMaterials("Wobble", 0.f);
		Top_Module->SetScalarParameterValueOnMaterials("Wobble", 0.f);

		UE_LOG(LogTemp, Warning, TEXT("[PlayWobble] Stopped and reset previous wobble"));
	}


	FVector temVec = FVector(1, 0, 0);

	const float WobbleHeightWorld = CalculateTopModuleHeight() * AppliedMeshScale;

	MainMeshComponent->SetScalarParameterValueOnMaterials("Building Total Height", WobbleHeightWorld / 4.f);
	Body_Module->SetScalarParameterValueOnMaterials("Building Total Height", WobbleHeightWorld / 4.f);
	Top_Module->SetScalarParameterValueOnMaterials("Building Total Height", WobbleHeightWorld / 4.f);

	// 진폭을 높이 비례로 주입 — 머티리얼 기본값(20) 고정이면 절대 변위가 동일해 저층 건물이 비율상 과진동
	const float WobbleMoveLimit = FMath::Clamp(WobbleHeightWorld * 0.004f, 3.f, 16.f);
	MainMeshComponent->SetScalarParameterValueOnMaterials("Wobble Move Limit", WobbleMoveLimit);
	Body_Module->SetScalarParameterValueOnMaterials("Wobble Move Limit", WobbleMoveLimit);
	Top_Module->SetScalarParameterValueOnMaterials("Wobble Move Limit", WobbleMoveLimit);

	// MF_Wobble은 월드 정점 Z에서 Wobble Center.Z를 빼므로 모든 모듈이 같은 액터 바닥을 기준으로 흔들린다.
	const FVector BuildingBaseWorld = GetActorLocation();

	// Base Module (MainMeshComponent)
	MainMeshComponent->SetVectorParameterValueOnMaterials("Wobble Vector", temVec);
	MainMeshComponent->SetVectorParameterValueOnMaterials("Wobble Center", BuildingBaseWorld);

	// Body Modules
	Body_Module->SetVectorParameterValueOnMaterials("Wobble Vector", temVec);
	Body_Module->SetVectorParameterValueOnMaterials("Wobble Center", BuildingBaseWorld);

	// Top Module
	Top_Module->SetVectorParameterValueOnMaterials("Wobble Vector", temVec);
	Top_Module->SetVectorParameterValueOnMaterials("Wobble Center", BuildingBaseWorld);

	// 위상 동기화: MF_Wobble이 saturate((Time - StartTime) / 2) 원샷으로 샘플 (2초 = SetPlayRate(0.5f) 게이트 길이와 일치 유지)
	const float WobbleStartTime = GetWorld()->GetTimeSeconds();
	MainMeshComponent->SetScalarParameterValueOnMaterials("Wobble StartTime", WobbleStartTime);
	Body_Module->SetScalarParameterValueOnMaterials("Wobble StartTime", WobbleStartTime);
	Top_Module->SetScalarParameterValueOnMaterials("Wobble StartTime", WobbleStartTime);

	// Wobble 게이트 ON (Wooble_TimelineFinished에서 OFF). 흔들림은 WPO 머티리얼이 구동.
	MainMeshComponent->SetScalarParameterValueOnMaterials("Wobble", 1.f);
	Body_Module->SetScalarParameterValueOnMaterials("Wobble", 1.f);
	Top_Module->SetScalarParameterValueOnMaterials("Wobble", 1.f);

	Wooble_Timeline.SetPlayRate(0.5f);
	Wooble_Timeline.PlayFromStart();
	SetActorTickEnabled(true);
}

void ABuildingBaseActor::ApplySkin(int32 SkinID, bool bShouldSave)
{
	if (SkinID == 0)
	{
		return;
	}

	// GameInstance와 TableManager 가져오기
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApplySkin] GameInstance is null"));
		return;
	}

	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (!TableManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApplySkin] TableManager is null"));
		return;
	}

	// TableManager에서 SkinID로 스킨 데이터 가져오기
	bool bSuccess = false;
	FBuildingSkinData SkinData = TableManager->GetBuildingSkinData(SkinID, bSuccess);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApplySkin] Skin ID %d not found in DataTable"), SkinID);
		return;
	}

	// Material 적용
	UMaterialInterface* SkinMaterial = SkinData.SkinMaterial.LoadSynchronous();
	if (SkinMaterial)
	{
		// 각 컴포넌트에 Material 적용
		if (MainMeshComponent)
		{
			MainMeshComponent->SetMaterial(0, SkinMaterial);
		}

		if (Body_Module)
		{
			Body_Module->SetMaterial(0, SkinMaterial);
		}

		if (Top_Module)
		{
			Top_Module->SetMaterial(0, SkinMaterial);
		}

		if (Top_Empty_Module)
		{
			Top_Empty_Module->SetMaterial(0, SkinMaterial);
		}

		ApplyMobileFacadeMaterialTuning();

		// 적용된 스킨 ID 저장
		AppliedSkinID = SkinID;

		UE_LOG(LogTemp, Log, TEXT("[ApplySkin] Successfully applied skin ID %d"), SkinID);

		// ApplySkin이 머티리얼을 교체하면서 MID(emissive 포함)가 초기화되므로 조명 재적용
		if (AppliedLightID != 0)
		{
			ApplyLight(AppliedLightID, false);
		}

		// 스킨 변경 시 즉시 저장 (로드 중이 아닐 때만)
		// 연출(wobble/사운드)은 의도적으로 없음 — 머티리얼 wobble 은 엔진 시간 위상이라 임의 지점에서 시작해 부자연스러움(2026-06-04 제거 결정)
		if (bShouldSave)
		{
			SaveGameData();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApplySkin] Failed to load skin material"));
	}
}

void ABuildingBaseActor::ApplyLight(int32 LightID, bool bShouldSave)
{
	if (LightID == 0)
	{
		return;
	}

	UCGGameInstance* GameInstance = UCGGameInstance::GetInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApplyLight] GameInstance is null"));
		return;
	}

	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (!TableManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApplyLight] TableManager is null"));
		return;
	}

	bool bSuccess = false;
	FBuildingLightData LightData = TableManager->GetBuildingLightData(LightID, bSuccess);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApplyLight] Light ID %d not found in DataTable"), LightID);
		return;
	}

	// DT 컬러와 강도를 결합한 원본은 캐시에 그대로 보존하고 MID write 직전에만 모바일 배율을 적용한다.
	const FLinearColor SourceEmissive = LightData.EmissiveColor * LightData.EmissiveIntensity;
	const UWorld* BuildingWorld = GetWorld();
	const bool bMobileES31 = BuildingWorld
		&& BuildingWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1;
	const FLinearColor ResolvedEmissive = MobileFacadeMaterialTuning::ResolveWindowEmissiveColor(
		SourceEmissive,
		bMobileES31);
	static const FName EmissiveParamName(TEXT("Window_Emissive_Color"));

	auto ApplyToComponent = [&](UMeshComponent* Comp)
	{
		if (!Comp)
		{
			return;
		}
		UMaterialInterface* CurrentMat = Comp->GetMaterial(0);
		if (!CurrentMat)
		{
			return;
		}
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(CurrentMat);
		if (!MID)
		{
			MID = Comp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, CurrentMat);
		}
		if (MID)
		{
			MID->SetVectorParameterValue(EmissiveParamName, ResolvedEmissive);
		}
	};

	ApplyToComponent(MainMeshComponent);
	ApplyToComponent(Body_Module);
	ApplyToComponent(Top_Module);
	ApplyToComponent(Top_Empty_Module);

	AppliedLightID = LightID;
	// SetWindowLightActive 가 1초 주기로 이 멤버를 정본 삼아 재적용하므로, 적용색을 캐시하지 않으면 초기 랜덤색(흰색 계열)으로 되돌아감
	Window_Emissive_Color = SourceEmissive;

	UE_LOG(LogTemp, Log, TEXT("[ApplyLight] Successfully applied light ID %d (RGB %.2f,%.2f,%.2f x %.2f)"),
		LightID, LightData.EmissiveColor.R, LightData.EmissiveColor.G, LightData.EmissiveColor.B, LightData.EmissiveIntensity);

	if (bShouldSave)
	{
		SaveGameData();
	}
}

void ABuildingBaseActor::PlayBuildCompleteVFX(float SpawnHeight)
{
	UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] PlayBuildCompleteVFX called - SpawnHeight: %.2f"), SpawnHeight);

	if (!BuildCompleteVFX)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] BuildCompleteVFX is NULL!"));
		return;
	}

	if (!BoxComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] BoxComponent is NULL!"));
		return;
	}

	UStaticMeshComponent* VFXMesh = SpawnHeight > 0.0f ? Body_Module : MainMeshComponent;
	if (!VFXMesh || !VFXMesh->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] Build VFX requires a visual mesh"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] BuildCompleteVFX is valid, proceeding..."));

	// BoxComponent의 바운드 사용 (ReCalcBoxExtent에서 이미 계산됨)
	FVector BoxExtent = BoxComponent->GetScaledBoxExtent();

	// X, Y 평균 크기 (m 단위)
	float AvgSizeX = BoxExtent.X / 100.0f;  // cm → m
	float AvgSizeY = BoxExtent.Y / 100.0f;
	float AvgSize = (AvgSizeX + AvgSizeY) / 2.0f;

	// VFX Scale: 10x10m 건물 기준 Scale 5 (비율 0.5)
	float VFXScale = FMath::Max(AvgSize * 0.5f, 2.5f);

	// 증축은 Body 이음새, 완공은 Base 바닥의 실제 중심을 사용한다.
	// 클릭 박스의 여유 폭/반올림과 독립적으로 메시·액터 변환을 함께 반영한다.
	FVector MeshMin, MeshMax;
	VFXMesh->GetLocalBounds(MeshMin, MeshMax);
	FVector LocalVFXLocation = (MeshMin + MeshMax) * 0.5f;
	LocalVFXLocation.Z = SpawnHeight > 0.0f ? SpawnHeight : MeshMin.Z;
	const FTransform& MeshTransform = VFXMesh->GetComponentTransform();
	const FVector VFXLocation = MeshTransform.TransformPosition(LocalVFXLocation);

	// VFX 스폰
	UNiagaraComponent* SpawnedVFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		BuildCompleteVFX,
		VFXLocation,
		MeshTransform.Rotator(),
		FVector(VFXScale),
		true,  // Auto Destroy
		false, // 사용자 파라미터를 설정한 뒤 첫 버스트를 시작한다.
		ENCPoolMethod::None,
		true
	);

	if (SpawnedVFX)
	{
		// 원형 충격파 색상 (밝은 황토색)
		SpawnedVFX->SetColorParameter(FName("DustMin"), FLinearColor(0.9f, 0.7f, 0.4f, 1.0f));

		// 먼지 색상 (회갈색)
		SpawnedVFX->SetColorParameter(FName("DustColor"), FLinearColor(0.6f, 0.5f, 0.4f, 1.0f));

		// VFX 크기
		SpawnedVFX->SetFloatParameter(FName("Scale"), VFXScale);
		SpawnedVFX->Activate(true);

		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] Build VFX - SpawnHeight: %.2f, BoxExtent: %s, Scale: %.2f, Location: %s"),
			SpawnHeight, *BoxExtent.ToString(), VFXScale, *VFXLocation.ToString());
	}
}

void ABuildingBaseActor::SaveGameData()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) return;

	USaveLoadManager* SaveLoadManager = GameInstance->GetSubsystem<USaveLoadManager>();
	if (SaveLoadManager)
	{
		SaveLoadManager->SaveGameData();
		UE_LOG(LogTemp, Log, TEXT("[BuildingBaseActor] Game data saved (BuildingIndex: %d)"), BuildingIndex);
	}
}

// ========== 건물 강화 시스템 구현 ==========

bool ABuildingBaseActor::UpgradeEnhancement(EBuildingEnhancementType EnhancementType, int32 Count)
{
	if (Count <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] Invalid upgrade count: %d"), Count);
		return false;
	}

	if (!IsEnhancementPurchaseAllowed(EnhancementType))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildingBaseActor] 강화 구매 거부: BuildingIndex=%d Type=%d"),
			BuildingIndex,
			static_cast<int32>(EnhancementType));
		return false;
	}

	// 하드 가드 — UI가 어떤 값을 넘겨도 액터가 스스로 지킨다 (비용 계산 오버플로/레벨 폭주 방지)
	Count = FMath::Min(Count, 100);

	const int32 RawCurrentLevel = GetEnhancementLevel(EnhancementType);
	const int32 CurrentLevel = FMath::Max(RawCurrentLevel, 0);
	const int64 LevelHeadroom = static_cast<int64>(MAX_int32) - static_cast<int64>(CurrentLevel);
	if (LevelHeadroom <= 0)
	{
		return false;
	}
	Count = static_cast<int32>(FMath::Min(static_cast<int64>(Count), LevelHeadroom));

	// MaxLevel 가드 — DT 단일 진실(0=무제한). CSV MaxLevel>0 이면 캡 초과 홀드(10Hz)를 액터가 스스로 차단
	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		FBuildingEnhancementDefinition Def;
		if (TableMgr->GetEnhancementDefinition(EnhancementType, Def) && Def.MaxLevel > 0)
		{
			const int64 RemainingToMaxLevel = static_cast<int64>(Def.MaxLevel) - static_cast<int64>(CurrentLevel);
			if (RemainingToMaxLevel <= 0)
			{
				return false;
			}
			Count = static_cast<int32>(FMath::Min(static_cast<int64>(Count), RemainingToMaxLevel));
		}
	}
	const int64 NewLevel = static_cast<int64>(CurrentLevel) + static_cast<int64>(Count);

	// 업그레이드 비용 계산 — 기본 건설비는 패널 표시와 같은 출처(GetBaseBuildCostBricks)를 쓴다
	const int64 TotalCost = UBuildingEnhancementHelper::CalculateBulkUpgradeCost(
		EnhancementType, CurrentLevel, Count, GetBaseBuildCostBricks());

	// 보유 자금 확인 및 차감
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingBaseActor] ResourceItemManager not found"));
		return false;
	}

	// 비용 자원 타입은 DT 단일 진실(FBuildingEnhancementDefinition.CostResourceType) — 빌드업=Brick, 나머지=Money.
	const EResourceType CostType = UBuildingEnhancementHelper::GetCostResourceType(EnhancementType);
	if (!ResourceMgr->HasResource(CostType, TotalCost))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] Not enough resource (type=%d)! Need=%lld"),
			static_cast<int32>(CostType), TotalCost);
		return false;
	}

	// bShouldSave=false — 아래에서 지연 저장 1회로 통합 (기본값 true 면 클릭당 전체 세이브 2회 = 강화 렉 주범)
	ResourceMgr->SpendResource(CostType, TotalCost, /*bShouldSave=*/false);

	// 지출 플로팅 팝업 — 홀드(10Hz) 연타는 InGameLayer 측 배치 윈도우가 병합
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->SpawnSpendPopup(CostType, TotalCost);
		}
	}

	// 레벨 증가 — BuildingFloor는 Body_Module_Copies가 정본이므로 별도 경로, 나머지는 TMap에 누적
	if (EnhancementType == EBuildingEnhancementType::BuildingFloor)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			AddFloor(false);  // 저장은 아래 지연 저장 1회로 통합 — bulk 시 N+1 disk write 방지
		}
		UE_LOG(LogTemp, Log, TEXT("[BuildingBaseActor] Building floor upgraded to %d floors"), Body_Module_Copies);

		if (URecruitmentManagerSubsystem* RecruitmentMgr =
			GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>())
		{
			PendingFloorRecruitTicketCount += RecruitmentMgr->ReconcilePermanentCapacityTickets(false, false);
			if (!GetWorldTimerManager().IsTimerActive(NewFloorRiseTimerHandle))
			{
				ShowPendingFloorRecruitTicketNotification();
			}
		}
	}
	else
	{
		int32& StoredLevel = EnhancementLevels.FindOrAdd(EnhancementType, 0);
		StoredLevel = static_cast<int32>(NewLevel);
		UE_LOG(LogTemp, Log, TEXT("[BuildingBaseActor] Enhancement %d upgraded to Lv.%d"),
			static_cast<int32>(EnhancementType), StoredLevel);
	}

	// 지연 저장 — 강화는 클릭/홀드(10Hz) 고빈도 경로라 동기 전체 세이브를 매번 돌리면 프레임 히칭
	if (USaveLoadManager* SaveLoadMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveLoadMgr->RequestDeferredSave();
	}

	if (EnhancementType != EBuildingEnhancementType::BuildingFloor)
	{
		if (UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
		{
			Missions->NotifyBuildingStatUpgraded(Count);
		}
	}

	return true;
}

bool ABuildingBaseActor::IsEnhancementPurchaseAllowed(EBuildingEnhancementType EnhancementType) const
{
	UGameInstance* GameInstance = GetGameInstance();
	const UMissionManagerSubsystem* MissionMgr = GameInstance
		? GameInstance->GetSubsystem<UMissionManagerSubsystem>()
		: nullptr;
	const bool bCompanyEnhancementUnlocked = MissionMgr && MissionMgr->IsCompanyEnhancementUnlocked();

	bool bUnlockedByTier = false;
	if (GameInstance)
	{
		USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>();
		const UTableManagerSubsystem* TableMgr = GameInstance->GetSubsystem<UTableManagerSubsystem>();
		if (SaveMgr && TableMgr)
		{
			const int32 BuildingTier = SaveMgr->GetBuildingTier(BuildingIndex);
			bUnlockedByTier = TableMgr->GetUnlockedEnhancementsUpToTier(BuildingTier).Contains(EnhancementType);
		}
	}

	return FTutorialEnhancementGateRules::IsPurchaseAllowed(
		bCompanyEnhancementUnlocked,
		EnhancementType,
		bUnlockedByTier);
}

int32 ABuildingBaseActor::GetEnhancementLevel(EBuildingEnhancementType EnhancementType) const
{
	// BuildingFloor는 모듈 수가 정본 (TMap에는 없음)
	if (EnhancementType == EBuildingEnhancementType::BuildingFloor)
	{
		return Body_Module_Copies;
	}

	if (const int32* Found = EnhancementLevels.Find(EnhancementType))
	{
		return *Found;
	}
	return 0;
}

float ABuildingBaseActor::GetEnhancementMultiplier(EBuildingEnhancementType EnhancementType) const
{
	const int32 Level = GetEnhancementLevel(EnhancementType);
	return UBuildingEnhancementHelper::CalculateEffectMultiplier(EnhancementType, Level);
}

// GetFinalCostReductionMultiplier 제거됨 — CostReduction enum 이 세계지도 업그레이드 시스템으로 이동 (2026-04-23)

int32 ABuildingBaseActor::GetFootprintCells() const
{
	if (CachedFootprintCells > 0)
	{
		return CachedFootprintCells;
	}

	// ConstructVisuals 전에 비용을 물어오는 경로 방어 — 여기서 조용히 1 을 돌려주면
	// 3x3 이 1/9 가격에 증축되고 로그도 안 남는다. DT 를 직접 한 번 더 뒤진다.
	if (BuildingDataTable && !BuildingID.IsNone())
	{
		if (const FBuildingData* Row = BuildingDataTable->FindRow<FBuildingData>(BuildingID, TEXT("GetFootprintCells")))
		{
			return FMath::Max(1, Row->FootprintWidthCells * Row->FootprintDepthCells);
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[BuildingBaseActor] footprint 칸수 미해결 (ID=%s) — 증축 비용을 1칸 기준으로 계산한다"),
		*BuildingID.ToString());
	return 1;
}

int64 ABuildingBaseActor::GetBaseBuildCostBricks() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FBuildableCardTable CardInfo;
	if (TableMgr && !BuildingID.IsNone() && TableMgr->GetBuildableInfo(BuildingID, CardInfo))
	{
		for (const FConstructionCost& Cost : CardInfo.ConstructionCosts)
		{
			if (Cost.ResourceType == EResourceType::Brick && Cost.Cost > 0)
			{
				return Cost.Cost;
			}
		}
	}

	// 가격은 싸지는 쪽으로 조용히 실패하면 안 된다 — 로그를 남기고 최저 배수로 계산한다
	UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] 기본 건설비(벽돌) 미해결 (ID=%s) — 증축 비용을 배수 1로 계산한다"), *BuildingID.ToString());
	return 1;
}

int64 ABuildingBaseActor::GetUpgradeCost(EBuildingEnhancementType EnhancementType) const
{
	const int32 CurrentLevel = GetEnhancementLevel(EnhancementType);
	return UBuildingEnhancementHelper::CalculateUpgradeCost(EnhancementType, CurrentLevel, GetBaseBuildCostBricks());
}

int64 ABuildingBaseActor::GetBulkUpgradeCost(EBuildingEnhancementType EnhancementType, int32 Count) const
{
	const int32 CurrentLevel = GetEnhancementLevel(EnhancementType);
	return UBuildingEnhancementHelper::CalculateBulkUpgradeCost(EnhancementType, CurrentLevel, Count, GetBaseBuildCostBricks());
}

float ABuildingBaseActor::GetVaultCapacity() const
{
	// 지연 저장 전에도 강화 직후 값이 반영되도록 액터의 현재 레벨을 직접 전달한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			return OpMgr->CalculateWarehouseCapacityAtLevel(
				BuildingIndex, GetEnhancementLevel(EBuildingEnhancementType::VaultCapacity));
		}
	}
	return BaseVaultCapacity;
}

void ABuildingBaseActor::StartFloating(float Height, bool bImmediate)
{
	if (bIsFloating)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Building] StartFloating - Already floating"));
		return;
	}

	// 호출부는 고정 500cm 를 넘긴다 — 표시 배수를 곱해 줄어든 건물에서도 들어올림이 같은 비율로 보이게 한다.
	Height *= AppliedMeshScale;

	bIsFloating = true;
	bFloatingUp = true;
	if (GroundContactSMC) { GroundContactSMC->SetVisibility(false); }
	FloatingHeight = Height;
	OriginalZ = GetActorLocation().Z;

	UE_LOG(LogTemp, Log, TEXT("[Building] StartFloating - OriginalZ: %f, Height: %f, Immediate: %d"), OriginalZ, Height, bImmediate);

	if (bImmediate)
	{
		// 즉시 떠있는 상태로 설정
		FloatingAlpha = 1.0f;
		FVector CurrentLocation = GetActorLocation();
		SetActorLocation(FVector(CurrentLocation.X, CurrentLocation.Y, OriginalZ + FloatingHeight));
	}
	else
	{
		// 애니메이션으로 떠오르기
		FloatingAlpha = 0.0f;
		GetWorldTimerManager().SetTimer(
			FloatingTimerHandle,
			this,
			&ABuildingBaseActor::UpdateFloatingAnimation,
			0.016f,
			true
		);
	}
}

void ABuildingBaseActor::StopFloating()
{
	if (!bIsFloating)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Building] StopFloating - Starting descent"));

	bFloatingUp = false;
	FloatingAlpha = 1.0f;

	// 타이머가 없으면 시작 (Immediate 모드였던 경우)
	if (!GetWorldTimerManager().IsTimerActive(FloatingTimerHandle))
	{
		GetWorldTimerManager().SetTimer(
			FloatingTimerHandle,
			this,
			&ABuildingBaseActor::UpdateFloatingAnimation,
			0.016f,
			true
		);
	}
}

void ABuildingBaseActor::LandBuilding()
{
	if (!bIsFloating)
	{
		// 이미 바닥에 있으면 VFX만 재생
		PlayBuildCompleteVFX(0.0f);
		return;
	}

	// 떨어지는 케이스만 사운드 시퀀스 트리거 (whoosh 즉시 + thud peak 가 0.25s 착지 frame 과 일치)
	PlayPlacementLandSequence();

	// 착지 시 VFX 재생 플래그 설정
	bShouldPlayLandVFX = true;
	StopFloating();
}

void ABuildingBaseActor::UpdateFloatingAnimation()
{
	const float Speed = 4.0f;
	const float DeltaTime = 0.016f;

	if (bFloatingUp)
	{
		FloatingAlpha = FMath::Clamp(FloatingAlpha + DeltaTime * Speed, 0.0f, 1.0f);
	}
	else
	{
		FloatingAlpha = FMath::Clamp(FloatingAlpha - DeltaTime * Speed, 0.0f, 1.0f);
	}

	// EaseOut 효과 적용
	float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, FloatingAlpha, 2.0f);

	FVector CurrentLocation = GetActorLocation();
	float NewZ = OriginalZ + (FloatingHeight * EasedAlpha);
	SetActorLocation(FVector(CurrentLocation.X, CurrentLocation.Y, NewZ));

	// 애니메이션 완료 체크
	if (!bFloatingUp && FloatingAlpha <= 0.0f)
	{
		bIsFloating = false;
		GetWorldTimerManager().ClearTimer(FloatingTimerHandle);
		SetActorLocation(FVector(CurrentLocation.X, CurrentLocation.Y, OriginalZ));
		if (GroundContactSMC)
		{
			BuildingGroundContact::Configure(this, MainMeshComponent, GroundContactSMC);
			GroundContactSMC->SetVisibility(!bOccluderGhosted && !Tags.Contains("Build")
				&& GroundContactSMC->GetStaticMesh() != nullptr);
		}

		// 착지 VFX 재생
		if (bShouldPlayLandVFX)
		{
			bShouldPlayLandVFX = false;
			PlayBuildCompleteVFX(0.0f);
		}
	}
}

void ABuildingBaseActor::StartNewFloorRiseAnimation()
{
	GetWorldTimerManager().ClearTimer(NewFloorRiseTimerHandle);
	if (!Body_Module || USettingsManagerSubsystem::IsReduceMotion(this))
	{
		NewFloorRiseInstanceIndex = INDEX_NONE;
		return;
	}

	// 진행 중이던 이전 층 애니를 즉시 제자리로 스냅 (bulk 증축 연타 시 중간 상태 잔존 방지)
	if (NewFloorRiseInstanceIndex != INDEX_NONE)
	{
		FTransform Prev;
		if (Body_Module->GetInstanceTransform(NewFloorRiseInstanceIndex, Prev))
		{
			FVector L = Prev.GetLocation();
			L.Z = NewFloorRiseFinalZ;
			Prev.SetLocation(L);
			Body_Module->UpdateInstanceTransform(NewFloorRiseInstanceIndex, Prev, false, true, true);
			CGInstancedMesh::RewriteAllInstances(Body_Module);
		}
	}

	// 최상단 인스턴스 = 방금 추가된 새 층 (RebuildBodyModules 가 0..Copies 로 재구성, 마지막 = Copies)
	const int32 TopIndex = Body_Module->GetInstanceCount() - 1;
	FTransform TopTransform;
	if (TopIndex < 0 || !Body_Module->GetInstanceTransform(TopIndex, TopTransform))
	{
		NewFloorRiseInstanceIndex = INDEX_NONE;
		return;
	}

	NewFloorRiseInstanceIndex = TopIndex;
	NewFloorRiseFinalZ = TopTransform.GetLocation().Z;

	// 낙차 = 한 층 높이 (밀려 올라오는 티가 나되 과하지 않게)
	FVector Body_Min(ForceInit), Body_Max(ForceInit);
	Body_Module->GetLocalBounds(Body_Min, Body_Max);
	NewFloorRiseDrop = FMath::Max(Body_Max.Z * Floor_Height_BodyModuleScale, 50.0f);

	// 시작 위치 = 제자리보다 한 층 아래
	FVector StartLoc = TopTransform.GetLocation();
	StartLoc.Z = NewFloorRiseFinalZ - NewFloorRiseDrop;
	TopTransform.SetLocation(StartLoc);
	Body_Module->UpdateInstanceTransform(TopIndex, TopTransform, false, true, true);
	// 재추가 직후의 개별 갱신은 모바일(GPUScene 꺼짐)에서 엉뚱한 층을 옮긴다 — 같은 데이터로 다시 넣어 렌더 대응을 새로 만든다.
	CGInstancedMesh::RewriteAllInstances(Body_Module);

	NewFloorRiseElapsed = 0.0f;
	GetWorldTimerManager().SetTimer(NewFloorRiseTimerHandle, this,
		&ABuildingBaseActor::UpdateNewFloorRiseAnimation, 0.016f, true);
}

void ABuildingBaseActor::UpdateNewFloorRiseAnimation()
{
	const float RiseDuration = 0.25f;

	if (!Body_Module || NewFloorRiseInstanceIndex == INDEX_NONE)
	{
		GetWorldTimerManager().ClearTimer(NewFloorRiseTimerHandle);
		NewFloorRiseInstanceIndex = INDEX_NONE;
		ShowPendingFloorRecruitTicketNotification();
		return;
	}

	NewFloorRiseElapsed += 0.016f;
	const float T = FMath::Clamp(NewFloorRiseElapsed / RiseDuration, 0.0f, 1.0f);
	// EaseOutBack — 살짝 오버슛 후 안착 ("쿵" 얹히는 무게감)
	const float Eased = FWidgetAnimationUtils::EaseOutBack(T);

	FTransform Xf;
	if (Body_Module->GetInstanceTransform(NewFloorRiseInstanceIndex, Xf))
	{
		FVector L = Xf.GetLocation();
		L.Z = (NewFloorRiseFinalZ - NewFloorRiseDrop) + (NewFloorRiseDrop * Eased);
		Xf.SetLocation(L);
		Body_Module->UpdateInstanceTransform(NewFloorRiseInstanceIndex, Xf, false, true, true);
		CGInstancedMesh::RewriteAllInstances(Body_Module);
	}

	if (T >= 1.0f)
	{
		// 정확히 제자리로 스냅 후 종료 (오버슛 잔상 방지)
		if (Body_Module->GetInstanceTransform(NewFloorRiseInstanceIndex, Xf))
		{
			FVector L = Xf.GetLocation();
			L.Z = NewFloorRiseFinalZ;
			Xf.SetLocation(L);
			Body_Module->UpdateInstanceTransform(NewFloorRiseInstanceIndex, Xf, false, true, true);
			CGInstancedMesh::RewriteAllInstances(Body_Module);
		}
		NewFloorRiseInstanceIndex = INDEX_NONE;
		GetWorldTimerManager().ClearTimer(NewFloorRiseTimerHandle);
		ShowPendingFloorRecruitTicketNotification();
	}
}

void ABuildingBaseActor::ShowPendingFloorRecruitTicketNotification()
{
	const int32 GrantedCount = PendingFloorRecruitTicketCount;
	PendingFloorRecruitTicketCount = 0;
	if (GrantedCount <= 0)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIMgr = GameInstance ? GameInstance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!TableMgr || !UIMgr)
	{
		return;
	}

	FShopItemTable ItemRow;
	if (!TableMgr->GetShopItemByItemType(EItemType::RecruitTicketNormal, ItemRow) || ItemRow.DisplayName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingBaseActor] RecruitTicketNormal display name is missing from DT_ShopItem"));
		return;
	}

	UIMgr->ShowBottomNotification(FText::Format(
		NSLOCTEXT("RecruitCapacity", "FloorGrantNotification", "{0} +{1}"),
		ItemRow.DisplayName, FText::AsNumber(GrantedCount)));
}
