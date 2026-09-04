// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/Components/PlacementHandler.h"
#include "Player/PlayerCamera.h"
#include "Player/MainMapPlayerController.h"

#include "Components/BoxComponent.h"
#include "GameFramework/WorldSettings.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Util/CoordinateUtils.h"
#include "Entity/Building/BuildingBaseActor.h"

#include "Core/CGGameInstance.h"
#include "Table/InteractableInfo.h"

#include "Manager/EntityManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SpawnManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/WorkstationCapacityRules.h"
#include "Player/Components/PlotPlacementRules.h"

#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

#include "Input/InputPriorities.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Office/WorkstationActorBase.h"
#include "Player/OfficeCameraPawn.h"
#include "Office/DecorationActor.h"
#include "Office/OfficeManager.h"
#include "Office/OfficeInterior.h"
#include "Office/StarterPresetSeeder.h"
#include "Manager/TableManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Table/CityPlotData.h"
#include "Table/BuildingData.h" // FootprintCellSize (격자 셀 한 변 크기) 단일 소스
#include "Entity/Plot/CityPlotActor.h"
#include "Entity/Ambient/VacantPlotDressingManager.h"
#include "UI/Panel/InGameLayerWidget.h"


// 배치 회전은 90° 배수라 footprint 사각형은 축정렬을 유지한다 — 홀수 회전이면 W/D 스왑만으로 충분.
// 프리뷰 사각형(시각)·부지 clamp·겹침 AABB 가 전부 이 헬퍼를 거쳐 같은 규약을 공유한다.
static void ApplyYawToFootprintCells(float Yaw, int32& InOutWidthCells, int32& InOutDepthCells)
{
	const int32 QuarterTurns = FMath::RoundToInt(FRotator::NormalizeAxis(Yaw) / 90.f);
	if (FMath::Abs(QuarterTurns) % 2 == 1)
	{
		Swap(InOutWidthCells, InOutDepthCells);
	}
}


// Sets default values for this component's properties
UPlacementHandler::UPlacementHandler()
{
	PrimaryComponentTick.bCanEverTick = true;

	// PlacementSMC를 생성자에서 생성
	PlacementSMC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlacementMesh"));

	// 부지 footprint 프리뷰 사각형(PlotFootprintSMC)은 런타임에 첫 사용 시 1회 동적 생성한다(UpdatePlotFootprintPreview).

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> imc_BuildMode
	(TEXT("/Script/EnhancedInput.InputMappingContext'/Game/CompanyGrowth/Input/IMC_BuildMode.IMC_BuildMode'"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ia_BuildMove
	(TEXT("/Script/EnhancedInput.InputAction'/Game/CompanyGrowth/Input/IA_BuildMove.IA_BuildMove'"));

	if (imc_BuildMode.Succeeded())
	{
		IMC_BuildMode = imc_BuildMode.Object;
	}
	check(IMC_BuildMode);

	if (ia_BuildMove.Succeeded())
	{
		IA_BuildMove = ia_BuildMove.Object;
	}
	check(IA_BuildMove);
}


void UPlacementHandler::BeginPlay()
{
	Super::BeginPlay();

	// 배치 프리뷰 메시는 카메라를 따라다니면 안 되므로 월드(WorldSettings)에 붙인다.
	if (const UWorld* World = GetWorld())
	{
		if (AWorldSettings* WS = World->GetWorldSettings())
		{
			if (USceneComponent* WorldRoot = WS->GetRootComponent())
			{
				PlacementSMC->AttachToComponent(WorldRoot, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	GlobalAssetCache = UCGGameInstance::GetInstance()->GetGlobalAssetCache();

	PlacementSMC->SetStaticMesh(GlobalAssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Cube));
	PlacementSMC->SetMaterial(0, GlobalAssetCache->GetPlaceableMaterial());

	// 부지 footprint 프리뷰 사각형의 베이스 머티리얼(M_PlotHighlight, "Color" 벡터 파라미터)을 1회 로드.
	// footprint SMC 는 런타임에 첫 사용 시 1회 동적 생성하며(UpdatePlotFootprintPreview) 이 머티리얼의 DMI 를 입힌다.
	// 로드 실패(미생성)면 PlotHighlightBaseMaterial=null → footprint 프리뷰 생성 자체를 막아 graceful 숨김.
	TSoftObjectPtr<UMaterialInterface> HighlightMatPath(
		FSoftObjectPath(TEXT("/Game/CompanyGrowth/Resources/Materials/M_PlotHighlight.M_PlotHighlight")));
	PlotHighlightBaseMaterial = HighlightMatPath.LoadSynchronous();
}

void UPlacementHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndPlacementFeedback();
	HidePlotFootprintPreview();
	Super::EndPlay(EndPlayReason);
}

void UPlacementHandler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 건물을 잡고 드래그할 때 Edge Panning + 건물 위치 업데이트
	if (bIsDraggingPlacement)
	{
		// TrackMovePlacement 먼저 호출 (벽 전환 감지)
		// 그 후 UpdateEdgePanning (새 벽 기준으로 패닝)
		TrackMovePlacement();
		UpdateEdgePanning();
	}
}

void UPlacementHandler::Initialize()
{
	Owner = Cast<APlayerCamera>(GetOwner());

	// 월드 부착은 BeginPlay 로 이관 — Initialize 는 APlayerCamera 생성자에서 호출되는데,
	// 생성자 컨텍스트의 AttachToComponent 는 KeepRelative 로만 취급돼 ensure 로 에디터가 죽는다.

	// PlacementSMC의 속성 설정은 Initialize에서 진행
	PlacementSMC->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	PlacementSMC->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));
	PlacementSMC->SetGenerateOverlapEvents(false);
	PlacementSMC->SetCollisionProfileName(TEXT("NoCollision"));
	PlacementSMC->SetVisibility(false);

	// 부지 footprint 프리뷰 사각형은 첫 사용 시(UpdatePlotFootprintPreview) 동적으로 생성/부착한다.
}

void UPlacementHandler::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	UE_LOG(LogTemp, Warning, TEXT("SetupPlayerInputComponent called"));
	
	if (const ULocalPlayer* LocalPlayer = Owner->GetPlayerController()->GetLocalPlayer())
	{
		InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (InputSubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("InputSubsystem found"));
			if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
			{
				if (IA_BuildMove)
				{
					// 모바일: Started 사용
					EnhancedInputComponent->BindAction(IA_BuildMove, ETriggerEvent::Started, this,
						&UPlacementHandler::OnBuildMovePressed);
					// 드래그 이어지게 하는거
					EnhancedInputComponent->BindAction(IA_BuildMove, ETriggerEvent::Ongoing, this,
						&UPlacementHandler::OnBuildMovePressed);
					// PC: Triggered 사용
					EnhancedInputComponent->BindAction(IA_BuildMove, ETriggerEvent::Triggered, this,
						&UPlacementHandler::OnBuildMovePressed);
					// 드래그 끝
					EnhancedInputComponent->BindAction(IA_BuildMove, ETriggerEvent::Completed, this,
						&UPlacementHandler::OnBuildMoveReleased);
					EnhancedInputComponent->BindAction(IA_BuildMove, ETriggerEvent::Canceled, this,
						&UPlacementHandler::OnBuildMoveReleased);
					UE_LOG(LogTemp, Warning, TEXT("BuildMove action bound successfully"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("IA_BuildMove is NULL!"));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get InputSubsystem"));
		}
	}
}

bool UPlacementHandler::CheckEntityAgainstPlacementTarget(const FInteractableInfo& info)
{
	if (!PlacementTargetEntity)
	{
		return false;
	}

	if (PlacementTargetEntity->GetInteractableName() == info.Name)
	{
		return true;
	}

	PlacementTargetEntity->Destroy();
	PlacementTargetEntity = nullptr;
	EndPlacementFeedback();

	return false;
}

void UPlacementHandler::SetPlacementTargetEntity(const FInteractableInfo& interactableInfo, TSubclassOf<AActor> InOverlapCheckClass)
{
	OriginalBuildingPlotId = NAME_None;
	PlacementTargetInfo = interactableInfo;
	bHasPlacementTarget = true;
	CurrentPlacementMode = EPlacementMode::Building;

	// 겹침 체크 클래스 설정 (미지정 시 기본값: ABuildingBaseActor)
	if (InOverlapCheckClass)
	{
		OverlapCheckClass = InOverlapCheckClass;
	}
	else
	{
		OverlapCheckClass = ABuildingBaseActor::StaticClass();
	}

	UE_LOG(LogTemp, Log, TEXT("[UPlacementHandler]::SetPlacementTargetEntity: %s"), *interactableInfo.Name.ToString());

	USpawnManager* SpawnManager = GetWorld()->GetSubsystem<USpawnManager>();

	AInteractableBaseActor* interactableActor = SpawnManager->SpawnNewBuilding(PlacementTargetInfo);
	if (interactableActor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn interactable actor"));
		Owner->EndBuild();
		return;
	}

	PlacementTargetEntity = interactableActor;
	FVector IntersectionPos;
	CoordinateUtils::ProjectViewportCenterToGroundPlane(IntersectionPos);

	// 부지 탭 진입(InitialSeedPlotId 유효) 시 초기 프리뷰를 그 부지 중심에서 시작(선택적 nicety).
	// 전역 [건설] 버튼은 시드 없음 → 뷰포트 중심 밑 소유 부지로 동적 수렴.
	if (bPlotBuildSession && !InitialSeedPlotId.IsNone())
	{
		if (UTableManagerSubsystem* SeedTableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bSeedOk = false;
			const FCityPlotData SeedData = SeedTableMgr->GetCityPlotData(InitialSeedPlotId, bSeedOk);
			if (bSeedOk)
			{
				IntersectionPos.X = SeedData.Center.X;
				IntersectionPos.Y = SeedData.Center.Y;
			}
		}
	}

	// 신규 프리뷰는 화면 중앙에서 가까운 실제 유효 지점을 1회 탐색한다. 이후 드래그는 기존 자유 이동 규칙 그대로다.
	bPlotPlacementValid = true;
	{
		FVector PlotSnapped;
		bool bValidCell = false;
		FName ResolvedPlotId = NAME_None;
		if (bPlotBuildSession
			&& TryFindInitialPlotPlacement(IntersectionPos, PlotSnapped, ResolvedPlotId))
		{
			IntersectionPos.X = PlotSnapped.X;
			IntersectionPos.Y = PlotSnapped.Y;
			bPlotPlacementValid = true;
			PendingPlotId = ResolvedPlotId;
		}
		else if (ComputePlotPlacement(IntersectionPos, PlotSnapped, bValidCell, ResolvedPlotId))
		{
			IntersectionPos.X = PlotSnapped.X;
			IntersectionPos.Y = PlotSnapped.Y;
			bPlotPlacementValid = bValidCell;
			PendingPlotId = ResolvedPlotId;
		}
	}

	FTransform viewportCenterTransform = FTransform(FRotator(0, 0, 0),IntersectionPos, FVector(PlacementTargetInfo.Scale));
	PlacementTargetEntity->SetActorTransform(viewportCenterTransform);
	PlacementTargetEntity->Tags = TArray<FName>{ "PlacementMode" };
	PlacementTargetRecalcBoxExtentDelegateHandle = PlacementTargetEntity->AddReCalcBoxExtentDelegateHandle(this,
		&UPlacementHandler::PostPlacementTargetRecalcBoxExtent);

	// 건물이면 떠있는 상태로 시작
	if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(PlacementTargetEntity))
	{
		Building->StartFloating(500.0f, true);
	}

	BeginPlacementFeedback();
	UpdateVacantPlotDressingPreview();

	// 초기 프리뷰부터 부지 footprint 사각형을 표시(첫 위치/색). 부지 모드 아니면 내부에서 숨김.
	UpdatePlotFootprintPreview();
}

void UPlacementHandler::PostPlacementTargetRecalcBoxExtent()
{
	if (!IsValid(PlacementTargetEntity))
	{
		return;
	}

	// 데칼은 UpdateBuildAsset 에서 액터 회전을 그대로 먹으므로, 크기는 회전이 배제된 로컬 바운드로 잡아야 한다.
	// (월드 AABB 인 GetActorBounds 를 쓰면 회전이 스케일·회전 양쪽에 이중 적용돼 비정사각형 건물의 데칼이 틀어짐.)
	// 로컬 바운드는 액터 스케일이 나눠진 값이라 되곱해 월드 크기로 환산한다.
	const FVector boundExtent = PlacementTargetEntity->CalculateComponentsBoundingBoxInLocalSpace(true).GetExtent()
		* PlacementTargetEntity->GetActorScale3D().GetAbs();

	// 건물에 붙이지 않고 월드 좌표로 직접 설정 (Floating 효과와 분리)
	FVector ActorLocation = PlacementTargetEntity->GetActorLocation();
	PlacementSMC->SetWorldLocation(FVector(ActorLocation.X, ActorLocation.Y, 1.0f));
	// 크기: 건물보다 살짝 크게 (1.1배)
	PlacementSMC->SetWorldScale3D((boundExtent / 50.0f) * 1.05f);
	PlacementSMC->SetVisibility(true);

	UpdateBuildAsset();
}

void UPlacementHandler::ReleasePlacementTargetEntity()
{
	EndPlacementFeedback();

	PlacementSMC->SetVisibility(false);
	PlacementSMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

	// 부지 footprint 프리뷰도 함께 숨김(취소). World 부착 유지 — 다음 배치 때 재사용.
	HidePlotFootprintPreview();

	if (IsValid(PlacementTargetEntity))
	{
		PlacementTargetEntity->RemoveReCalcBoxExtentDelegateHandle(PlacementTargetRecalcBoxExtentDelegateHandle);

		PlacementTargetEntity->Destroy();
		PlacementTargetEntity = nullptr;
	}

	CurrentPlacementMode = EPlacementMode::None;
	bHasPlacementTarget = false;
	PendingCompanyType = ECompanyType::None;
	PendingPlotId = NAME_None;
	bPlotBuildSession = false;
	InitialSeedPlotId = NAME_None;
	bPlotPlacementValid = true;
	OriginalBuildingPlotId = NAME_None;
}

bool UPlacementHandler::PlacingEntity()
{
	if (IsValid(PlacementTargetEntity) == false || !CanDrop)
	{
		return false;
	}

	// 미리보기 건물을 그대로 최종 건물로 사용
	AInteractableBaseActor* interactableActor = PlacementTargetEntity;
	const FName ConfirmedPlotId = PendingPlotId;

	// 건물 착지 (내려오는 애니메이션 + VFX)
	if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(interactableActor))
	{
		if (Building->IsFloating())
		{
			// 바닥 위치로 설정 후 착지 애니메이션
			FVector CurrentLoc = Building->GetActorLocation();
			CurrentLoc.Z = 0.0f;
			Building->SetActorLocation(CurrentLoc);
			Building->StartFloating(500.0f, true);
			Building->LandBuilding();  // 내려오면서 VFX 재생
		}
	}

	// 태그 변경: PlacementMode 제거, Interactable 추가, NewlyPlaced 제거
	interactableActor->Tags.Remove("PlacementMode");
	interactableActor->Tags.Remove("NewlyPlaced");
	interactableActor->Tags.Add(FName("Interactable"));

	// EntityManager 등록 이벤트가 올바른 부지를 보도록 링크를 먼저 확정한다.
	ABuildingBaseActor* PlacedBuilding = Cast<ABuildingBaseActor>(interactableActor);
	if (PlacedBuilding)
	{
		PlacedBuilding->SetOwningPlotId(ConfirmedPlotId);
	}

	// EntityManager에 다시 등록 (PlacementMode에서 해제되었으므로)
	interactableActor->FinalizeEntityRegistration();

	// 산업 베이크 — finalize 직후 (SetCompanyType 이 세이브를 동반하므로 등록 후 호출 필수)
	if (PlacedBuilding)
	{
		// 스타터 프리셋 예약 — 영속 보증은 아래 CompleteBuildingPlacement의 무조건 SaveGameData (SetCompanyType의 저장은 부가)
		FStarterPresetSeeder::SeedPendingForBuilding(PlacedBuilding);

		if (PendingCompanyType != ECompanyType::None)
		{
			PlacedBuilding->SetCompanyType(PendingCompanyType);
		}
	}
	RefreshVacantPlotDressingPlots(NAME_None, ConfirmedPlotId);
	PendingCompanyType = ECompanyType::None;
	EndPlacementFeedback();
	PendingPlotId = NAME_None;
	bPlotBuildSession = false;
	InitialSeedPlotId = NAME_None;
	bPlotPlacementValid = true;

	// 배치 확정 → 부지 footprint 프리뷰 즉시 숨김(PendingPlotId 리셋 후라 무조건 숨김 분기로 처리됨).
	HidePlotFootprintPreview();

	UpdateBuildAsset();
	CompleteBuildingPlacement(interactableActor);

	// 미션 진행 신호 — 신규 건물 배치 확정 (오프닝 M1 '첫 회사 건설' 등). 이동 확정 경로(ConfirmBuildingMove)는 제외
	if (UMissionManagerSubsystem* MissionMgr = GetWorld()->GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyBuildingPlaced(Cast<ABuildingBaseActor>(interactableActor));
	}

	// 델리게이트 핸들 해제 (건물이 배치된 후에는 PlacementHandler와 연결 해제)
	interactableActor->RemoveReCalcBoxExtentDelegateHandle(PlacementTargetRecalcBoxExtentDelegateHandle);

	// PlacementTargetEntity를 nullptr로 설정 (Destroy하지 않음)
	PlacementTargetEntity = nullptr;

	return true;
}

void UPlacementHandler::InitPlacementIMC()
{
	UE_LOG(LogTemp, Warning, TEXT("InitPlacementIMC called"));
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("InitPlacementIMC called"));
	}

	if (InputSubsystem)
	{
		FModifyContextOptions options;
		options.bForceImmediately = true;  // 즉시 적용

		InputSubsystem->AddMappingContext(IMC_BuildMode, InputPriority::PLACEMENT, options);  // 우선순위 1
		HasMappingContext = true;
		UE_LOG(LogTemp, Warning, TEXT("BuildMode IMC added successfully"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("BuildMode IMC added successfully"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("InputSubsystem is NULL!"));
	}
}

void UPlacementHandler::ReleasePlacementIMC()
{
	if (InputSubsystem)
	{
		FModifyContextOptions options;
		options.bIgnoreAllPressedKeysUntilRelease = true;
		options.bForceImmediately = true;
		options.bNotifyUserSettings = false;

		InputSubsystem->RemoveMappingContext(IMC_BuildMode, options);
		HasMappingContext = false;
	}
}

AActor* UPlacementHandler::GetPlacementTargetActor() const
{
	switch (CurrentPlacementMode)
	{
	case EPlacementMode::Building:
		return PlacementTargetEntity;
	case EPlacementMode::Workstation:
		return PlacementWorkstation;
	case EPlacementMode::Decoration:
	case EPlacementMode::WallDecoration:
		return PlacementDecoration;
	default:
		return nullptr;
	}
}

FVector2D UPlacementHandler::GetPlacementTargetBottomScreenPosition() const
{
	FVector2D screenCenter = CoordinateUtils::GetViewportCenter();

	AActor* TargetActor = GetPlacementTargetActor();
	if (TargetActor == nullptr)
	{
		return screenCenter;
	}

	const AMainMapPlayerController* playerController = Owner->GetPlayerController();
	FVector2D PlacementTargetBottomScreenPosition;

	FVector boundOrigin;
	FVector boundExtent;

	// 빌딩은 메인 메시 사용, 나머지는 GetActorBounds 사용
	switch (CurrentPlacementMode)
	{
	case EPlacementMode::Building:
		if (PlacementTargetEntity)
		{
			UMeshComponent* mainMesh = PlacementTargetEntity->GetMainMeshComponent();
			if (mainMesh)
			{
				FBoxSphereBounds meshBounds = mainMesh->CalcBounds(mainMesh->GetComponentTransform());
				boundOrigin = meshBounds.Origin;
				boundExtent = meshBounds.BoxExtent;
			}
		}
		break;
	case EPlacementMode::Workstation:
	case EPlacementMode::Decoration:
	case EPlacementMode::WallDecoration:
	default:
		TargetActor->GetActorBounds(false, boundOrigin, boundExtent);
		break;
	}

	FVector worldPosition = TargetActor->GetActorLocation();

	FVector2D downScreen = screenCenter + FVector2D(0, 50);

	FVector centerLocation, downLocation;
	CoordinateUtils::ProjectScreenPosToGroundPlane(screenCenter, centerLocation);
	CoordinateUtils::ProjectScreenPosToGroundPlane(downScreen, downLocation);

	FVector downDirection = downLocation - centerLocation;
	downDirection.Normalize();

	worldPosition += downDirection * FMath::Min(boundExtent.X, boundExtent.Y) * 1.05f;

	playerController->ProjectWorldLocationToScreen(worldPosition,
		PlacementTargetBottomScreenPosition, true);

	return PlacementTargetBottomScreenPosition;
}

void UPlacementHandler::UpdateBuildAsset()
{
	AActor* TargetActor = GetPlacementTargetActor();
	if (!TargetActor)
	{
		return;
	}

	TArray<AActor*> overlappingActors;

	// 오피스맵 바닥 배치: 업무공간과 데코레이션 상호 충돌 체크
	if (CurrentPlacementMode == EPlacementMode::Workstation || CurrentPlacementMode == EPlacementMode::Decoration)
	{
		TArray<AActor*> workstationOverlaps;
		TArray<AActor*> decorationOverlaps;

		TargetActor->GetOverlappingActors(workstationOverlaps, AWorkstationActorBase::StaticClass());
		TargetActor->GetOverlappingActors(decorationOverlaps, ADecorationActor::StaticClass());

		overlappingActors.Append(workstationOverlaps);
		overlappingActors.Append(decorationOverlaps);
	}
	// 벽 장식: 데코레이션끼리만 충돌 체크
	else if (CurrentPlacementMode == EPlacementMode::WallDecoration)
	{
		TargetActor->GetOverlappingActors(overlappingActors, ADecorationActor::StaticClass());
	}
	// 부지 배치: 겹침 판정은 footprint 연속 AABB(ComputePlotPlacement → bPlotPlacementValid)가 권위.
	// 물리 오버랩은 footprint 보다 큰 메시가 밖으로 삐져나올 때 오탐하므로 건너뛴다(§11.4).
	// (호버 부지 밖이면 PendingPlotId=None 이라도 세션 자체가 부지 제약이므로 footprint 게이트가 빨강 처리)
	else if (CurrentPlacementMode == EPlacementMode::Building && bPlotBuildSession)
	{
		// overlappingActors 비움 — footprint 게이트만 사용
	}
	// 빌딩 등 기존 로직
	else if (OverlapCheckClass)
	{
		TargetActor->GetOverlappingActors(overlappingActors, OverlapCheckClass);
	}

	CanDrop = false;

	// 부지 배치: bPlotPlacementValid(footprint 사각형 + 블록 실형상 트레이스 + 되밀기)가 단일 권위.
	// IsPlacementGrounded 를 AND 로 걸면 안 된다 — 그쪽은 액터 메시 월드 AABB ×1.05 로 재서 footprint 보다
	// 넓다(1×1 기준 양쪽 약 150cm). 그래서 블록 경계에 딱 붙이면 프리뷰 사각형은 검은데 CanDrop 만 false 인
	// 불일치가 났다(빨강도 안 뜨는 무언의 거부). 물리 오버랩을 빼둔 것과 같은 논리·같은 이유(§11.4).
	// 되밀기가 있는 footprint 게이트 쪽이 경계 판정의 권위라, 트레이스 9~14회도 같이 절약된다.
	if (CurrentPlacementMode == EPlacementMode::Building && bPlotBuildSession)
	{
		CanDrop = bPlotPlacementValid;
	}
	else if (overlappingActors.Num() == 0 && IsPlacementGrounded())
	{
		CanDrop = true;
	}

	// 위치·회전·호버 부지 판정이 모두 수렴한 뒤 같은 footprint로 동적 부지 소품을 갱신한다.
	UpdateVacantPlotDressingPreview();

	FVector targetPosition = TargetActor->GetActorLocation();

	// 벽 장식 모드: 인디케이터를 물체 맨 아래에 붙이기
	if (CurrentPlacementMode == EPlacementMode::WallDecoration)
	{
		FVector boundOrigin, boundExtent;
		TargetActor->GetActorBounds(false, boundOrigin, boundExtent);

		// boundOrigin이 실제 바운드 중심이므로, 바운드 하단 = boundOrigin.Z - boundExtent.Z
		targetPosition.Z = boundOrigin.Z - boundExtent.Z;
	}

	FLinearColor targetPositionColor = FLinearColor(targetPosition);
	targetPositionColor.A = CanDrop ? 1.0f : 0.0f;

	UMaterialParameterCollection* cropoutMPC = GlobalAssetCache->GetCropoutMPC();

	if (cropoutMPC == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateBuildAsset - MPC is NULL!"));
		return;
	}

	UMaterialParameterCollectionInstance* inst = GetWorld()->GetParameterCollectionInstance(cropoutMPC);


	FLinearColor oldTargetPosition;
	if (inst->GetVectorParameterValue("Target Position", oldTargetPosition))
	{
		inst->SetVectorParameterValue("Target Position", targetPositionColor);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateBuildAsset - Target Position parameter not found!"));
	}

	// 건물 배치 모드: PlacementSMC 위치/회전을 건물 따라가되 Z는 바닥에 고정
	if (CurrentPlacementMode == EPlacementMode::Building && PlacementSMC)
	{
		FVector ActorLoc = TargetActor->GetActorLocation();
		PlacementSMC->SetWorldLocation(FVector(ActorLoc.X, ActorLoc.Y, 1.0f));
		PlacementSMC->SetWorldRotation(TargetActor->GetActorRotation());
	}

	// 부지 footprint 프리뷰 갱신(이동/위치/색의 단일 수렴점 — TrackMovePlacement 2종 모두 여기로 모임).
	// 부지 모드가 아니면 내부에서 graceful 숨김.
	UpdatePlotFootprintPreview();
}

bool UPlacementHandler::IsPlacementGrounded()
{
	// [Perf] 9 LineTrace 본체는 정적 지오메트리 대상이라 같은 위치/모드/벽면이면 결과 불변 → memoize.
	// 드래그 중 finger 정지·동일 셀 스냅·plot 재계산 프레임에서 트레이스를 전부 스킵(드래그 시작 시 캐시 무효).
	AActor* TargetActor = GetPlacementTargetActor();
	if (!TargetActor)
	{
		return false;
	}
	const FVector ActorLoc = TargetActor->GetActorLocation();
	const FVector GroundKey(FMath::RoundToFloat(ActorLoc.X), FMath::RoundToFloat(ActorLoc.Y), FMath::RoundToFloat(ActorLoc.Z));
	if (bGroundCacheValid && GroundKey.Equals(LastGroundKey, 0.5f)
		&& LastGroundMode == CurrentPlacementMode && LastGroundWallSide == CurrentWallSide)
	{
		return bLastGroundResult;
	}

	const bool bResult = ComputeIsPlacementGrounded();
	LastGroundKey = GroundKey;
	LastGroundMode = CurrentPlacementMode;
	LastGroundWallSide = CurrentWallSide;
	bLastGroundResult = bResult;
	bGroundCacheValid = true;
	return bResult;
}

bool UPlacementHandler::ComputeIsPlacementGrounded()
{
	AActor* TargetActor = GetPlacementTargetActor();
	if (!TargetActor)
	{
		return false;
	}

	FVector boundOrigin;
	FVector boundExtent;
	TargetActor->GetActorBounds(false, boundOrigin, boundExtent);

	FVector actorLocation = TargetActor->GetActorLocation();

	TArray<FHitResult> hitResults;
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.Add(TargetActor);

	// 벽 배치 모드: 벽 안쪽으로 레이캐스트
	if (CurrentPlacementMode == EPlacementMode::WallDecoration)
	{
		const float ExtentX = boundExtent.X * 1.05f;
		const float ExtentZ = boundExtent.Z * 1.05f;
		const float ExtentY = boundExtent.Y * 1.05f;

		if (CurrentWallSide == EWallSide::Left)
		{
			// Left 벽 (Y=-11): Y+ → Y- 방향으로 레이캐스트 (벽 안으로)
			// 코너는 X, Z 평면에서 계산
			const float ExtentMinusX = ExtentX - 1.0f;
			const float ExtentMinusZ = ExtentZ - 1.0f;

			FVector corner1 = actorLocation + FVector(ExtentMinusX, 0.f, ExtentMinusZ);
			FVector corner2 = actorLocation + FVector(ExtentMinusX, 0.f, -ExtentMinusZ);
			FVector corner3 = actorLocation + FVector(-ExtentMinusX, 0.f, ExtentMinusZ);
			FVector corner4 = actorLocation + FVector(-ExtentMinusX, 0.f, -ExtentMinusZ);

			// Y+ (벽 앞) → Y- (벽 안) 방향
			FVector rayStart = FVector(0.f, 100.f, 0.f);
			FVector rayEnd = FVector(0.f, -200.f, 0.f);

			bool corner1Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner1 + rayStart, corner1 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			bool corner2Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner2 + rayStart, corner2 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			bool corner3Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner3 + rayStart, corner3 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			bool corner4Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner4 + rayStart, corner4 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			return corner1Hit && corner2Hit && corner3Hit && corner4Hit;
		}
		else // Right 벽
		{
			// Right 벽 (X=2 고정): X+ → X- 방향으로 레이캐스트 (벽 안으로)
			// 물체는 X < 2 위치에 있고, 벽은 X=2에 있음
			// 코너는 Y, Z 평면에서 계산
			const float ExtentMinusY = ExtentY - 1.0f;
			const float ExtentMinusZ = ExtentZ - 1.0f;

			FVector corner1 = actorLocation + FVector(0.f, ExtentMinusY, ExtentMinusZ);
			FVector corner2 = actorLocation + FVector(0.f, ExtentMinusY, -ExtentMinusZ);
			FVector corner3 = actorLocation + FVector(0.f, -ExtentMinusY, ExtentMinusZ);
			FVector corner4 = actorLocation + FVector(0.f, -ExtentMinusY, -ExtentMinusZ);

			// X+ (벽 앞, 물체 위치) → X- (벽 안) 방향은 아님
			// 실제로는 물체가 X < 2 에 있으므로 X- → X+ 가 벽 방향
			// 아니, X=2 벽이면 물체는 X < 2 에 있고 벽으로 가려면 X+ 방향
			FVector rayStart = FVector(-100.f, 0.f, 0.f);
			FVector rayEnd = FVector(200.f, 0.f, 0.f);

			bool corner1Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner1 + rayStart, corner1 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			bool corner2Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner2 + rayStart, corner2 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			bool corner3Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner3 + rayStart, corner3 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			bool corner4Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner4 + rayStart, corner4 + rayEnd,
				static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
				EDrawDebugTrace::None, hitResults, true);

			return corner1Hit && corner2Hit && corner3Hit && corner4Hit;
		}
	}

	// 바닥 배치 모드: 아래 방향(-Z)으로 레이캐스트
	const float ExtentX = boundExtent.X * 1.05f;
	const float ExtentY = boundExtent.Y * 1.05f;
	const float ExtentMinusX = ExtentX - 1.0f;
	const float ExtentMinusY = ExtentY - 1.0f;

	// Floating 중인 건물은 바닥(Z=0) 기준으로 레이캐스트
	float buildingBottom = 0.0f;
	if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(TargetActor))
	{
		if (!Building->IsFloating())
		{
			buildingBottom = actorLocation.Z;
		}
	}
	else
	{
		buildingBottom = actorLocation.Z;
	}

	FVector corner1 = actorLocation + FVector(ExtentMinusX, ExtentMinusY, 0);
	FVector corner1_up = FVector(corner1.X, corner1.Y, buildingBottom + 100.0f);
	FVector corner1_down = FVector(corner1.X, corner1.Y, buildingBottom - 200.0f);

	FVector corner2 = actorLocation + FVector(ExtentMinusX, -ExtentMinusY, 0);
	FVector corner2_up = FVector(corner2.X, corner2.Y, buildingBottom + 100.0f);
	FVector corner2_down = FVector(corner2.X, corner2.Y, buildingBottom - 200.0f);

	FVector corner3 = actorLocation + FVector(-ExtentMinusX, ExtentMinusY, 0);
	FVector corner3_up = FVector(corner3.X, corner3.Y, buildingBottom + 100.0f);
	FVector corner3_down = FVector(corner3.X, corner3.Y, buildingBottom - 200.0f);

	FVector corner4 = actorLocation + FVector(-ExtentMinusX, -ExtentMinusY, 0);
	FVector corner4_up = FVector(corner4.X, corner4.Y, buildingBottom + 100.0f);
	FVector corner4_down = FVector(corner4.X, corner4.Y, buildingBottom - 200.0f);

	bool corner1Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner1_up, corner1_down,
		static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
		EDrawDebugTrace::None, hitResults, true);

	bool corner2Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner2_up, corner2_down,
		static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
		EDrawDebugTrace::None, hitResults, true);

	bool corner3Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner3_up, corner3_down,
		static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
		EDrawDebugTrace::None, hitResults, true);

	bool corner4Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner4_up, corner4_down,
		static_cast<ETraceTypeQuery>(static_cast<int>(ECC_WorldStatic)), false, actorsToIgnore,
		EDrawDebugTrace::None, hitResults, true);

	if (!corner1Hit || !corner2Hit || !corner3Hit || !corner4Hit)
	{
		return false;
	}

	// Building 모드: Street 표면 위에만 배치 가능 (StreetSurface 채널 체크)
	if (CurrentPlacementMode == EPlacementMode::Building)
	{
		ETraceTypeQuery StreetTraceType = UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel3);

		TArray<FHitResult> streetHitResults;

		// 4코너 + 중심점 총 5곳 체크
		bool street1Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner1_up, corner1_down,
			StreetTraceType, false, actorsToIgnore,
			EDrawDebugTrace::None, streetHitResults, true);

		bool street2Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner2_up, corner2_down,
			StreetTraceType, false, actorsToIgnore,
			EDrawDebugTrace::None, streetHitResults, true);

		bool street3Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner3_up, corner3_down,
			StreetTraceType, false, actorsToIgnore,
			EDrawDebugTrace::None, streetHitResults, true);

		bool street4Hit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), corner4_up, corner4_down,
			StreetTraceType, false, actorsToIgnore,
			EDrawDebugTrace::None, streetHitResults, true);

		FVector center_up = FVector(actorLocation.X, actorLocation.Y, buildingBottom + 100.0f);
		FVector center_down = FVector(actorLocation.X, actorLocation.Y, buildingBottom - 200.0f);

		bool streetCenterHit = UKismetSystemLibrary::LineTraceMulti(GetWorld(), center_up, center_down,
			StreetTraceType, false, actorsToIgnore,
			EDrawDebugTrace::None, streetHitResults, true);

		if (!street1Hit || !street2Hit || !street3Hit || !street4Hit || !streetCenterHit)
		{
			return false;
		}
	}

	return true;
}

void UPlacementHandler::OnBuildMovePressed(const FInputActionValue& Value)
{
	// bIsDraggingPlacement는 InteractableInputHandler에서 관리
	// 여기서는 아무것도 하지 않음 (드래그 입력은 TickComponent에서 처리)
}

void UPlacementHandler::OnBuildMoveReleased(const FInputActionValue& Value)
{
	// bIsDraggingPlacement는 InteractableInputHandler에서 관리
}

void UPlacementHandler::UpdateTrackMovePlacement()
{
	if (HasMappingContext)
	{
		TrackMovePlacement();
	}
}

void UPlacementHandler::RotatePlacementEntity()
{
	AActor* TargetActor = GetPlacementTargetActor();
	if (!TargetActor)
	{
		return;
	}

	// 벽 장식 모드: Pitch만 90도씩 회전 (Yaw는 벽 방향 고정)
	if (CurrentPlacementMode == EPlacementMode::WallDecoration)
	{
		WallDecorationRotationOffset = FMath::Fmod(WallDecorationRotationOffset + 90.f, 360.f);
		float BaseYaw = (CurrentWallSide == EWallSide::Left) ? 180.f : 270.f;
		TargetActor->SetActorRotation(FRotator(WallDecorationRotationOffset, BaseYaw, 0.f));
	}
	else
	{
		// 기존 로직: 단순히 90도 회전 추가
		TargetActor->AddActorWorldRotation(FRotator(0, 90, 0));
	}

	// 그라운드 캐시 키는 위치만 담아 회전을 못 잡는다 — 점유 축이 바뀌었으므로 강제 무효화.
	bGroundCacheValid = false;

	// 부지 세션: 뒤집힌 footprint 로 부지 안에 다시 clamp(가장자리에서 삐져나옴 방지)하면서 프리뷰/CanDrop 까지 갱신.
	// 그 외 모드: 위치 재계산 없이 데칼 회전 + 겹침/그라운드 판정만 즉시 갱신.
	if (CurrentPlacementMode == EPlacementMode::Building && bPlotBuildSession)
	{
		TrackMovePlacement(TargetActor->GetActorLocation());
	}
	else
	{
		UpdateBuildAsset();
	}
}

UStaticMeshComponent* UPlacementHandler::GetOrCreatePlotOccupiedSMC(int32 Index)
{
	// 머티리얼 미로드/캐시 부재면 마커를 만들지 않는다(graceful 숨김).
	if (!PlotHighlightBaseMaterial || !GlobalAssetCache)
	{
		return nullptr;
	}

	if (PlotOccupiedSMCs.IsValidIndex(Index) && PlotOccupiedSMCs[Index])
	{
		return PlotOccupiedSMCs[Index];
	}

	UWorld* W = GetWorld();
	if (!W || !W->GetWorldSettings())
	{
		return nullptr;
	}

	// 풀 슬롯 확보(인덱스까지 nullptr 패딩) — Index 는 호출자가 0..Max-1 로 보장.
	while (PlotOccupiedSMCs.Num() <= Index)
	{
		PlotOccupiedSMCs.Add(nullptr);
		PlotOccupiedMIDs.Add(nullptr);
	}

	UStaticMeshComponent* MarkerSMC = NewObject<UStaticMeshComponent>(this);
	if (!MarkerSMC)
	{
		return nullptr;
	}
	MarkerSMC->RegisterComponent();
	MarkerSMC->AttachToComponent(W->GetWorldSettings()->GetRootComponent(),
		FAttachmentTransformRules::KeepWorldTransform);
	MarkerSMC->SetStaticMesh(GlobalAssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Plane));
	MarkerSMC->SetGenerateOverlapEvents(false);
	MarkerSMC->SetCollisionProfileName(TEXT("NoCollision"));
	MarkerSMC->SetCanEverAffectNavigation(false);
	MarkerSMC->SetCastShadow(false);
	MarkerSMC->SetVisibility(false);

	UMaterialInstanceDynamic* MarkerMID = UMaterialInstanceDynamic::Create(PlotHighlightBaseMaterial, this);
	if (MarkerMID)
	{
		MarkerSMC->SetMaterial(0, MarkerMID);
	}

	PlotOccupiedSMCs[Index] = MarkerSMC;
	PlotOccupiedMIDs[Index] = MarkerMID;
	return MarkerSMC;
}

void UPlacementHandler::HidePlotFootprintPreview()
{
	if (PlotFootprintSMC)
	{
		PlotFootprintSMC->SetVisibility(false);
	}

	// 점유 마커 풀도 전부 숨김.
	for (UStaticMeshComponent* MarkerSMC : PlotOccupiedSMCs)
	{
		if (MarkerSMC)
		{
			MarkerSMC->SetVisibility(false);
		}
	}
}

bool UPlacementHandler::HasCapacityForCurrentPlacement(FName PlotId) const
{
	if (PlotId.IsNone())
	{
		return false;
	}

	UWorld* RuntimeWorld = GetWorld();
	UGameInstance* GameInstance = RuntimeWorld ? RuntimeWorld->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UEntityManager* EntityMgr = RuntimeWorld ? RuntimeWorld->GetSubsystem<UEntityManager>() : nullptr;
	if (!TableMgr || !EntityMgr)
	{
		return false;
	}

	bool bPlotDataOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bPlotDataOk);
	if (!bPlotDataOk)
	{
		return false;
	}

	const int32 CurrentBuildingCount = EntityMgr->GetBuildingsOnPlot(PlotId).Num();
	const ABuildingBaseActor* MovingBuilding = Cast<ABuildingBaseActor>(PlacementTargetEntity);
	const bool bExcludeMovingBuilding = MovingBuilding && MovingBuilding->GetOwningPlotId() == PlotId;
	return FPlotPlacementRules::HasCapacity(
		CurrentBuildingCount,
		PlotData.BuildingCapacity,
		bExcludeMovingBuilding);
}

void UPlacementHandler::BeginPlacementFeedback()
{
	EndPlacementFeedback();

	if (!bPlotBuildSession || CurrentPlacementMode != EPlacementMode::Building)
	{
		return;
	}

	UWorld* RuntimeWorld = GetWorld();
	UGameInstance* GameInstance = RuntimeWorld ? RuntimeWorld->GetGameInstance() : nullptr;
	if (ABuildingBaseActor* PreviewBuilding = Cast<ABuildingBaseActor>(PlacementTargetEntity))
	{
		if (UUIManagerSubsystem* UIManager = GameInstance ? GameInstance->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			if (UInGameLayerWidget* InGameLayer = UIManager->GetInGameLayer())
			{
				InGameLayer->SetPlacementBuildingMarker(PreviewBuilding, this);
			}
		}
	}
}

void UPlacementHandler::EndPlacementFeedback()
{
	ClearVacantPlotDressingPreview();

	UWorld* RuntimeWorld = GetWorld();
	UGameInstance* GameInstance = RuntimeWorld ? RuntimeWorld->GetGameInstance() : nullptr;
	if (UUIManagerSubsystem* UIManager = GameInstance ? GameInstance->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		if (UInGameLayerWidget* InGameLayer = UIManager->GetInGameLayer())
		{
			InGameLayer->SetPlacementBuildingMarker(nullptr, nullptr);
		}
	}
}

void UPlacementHandler::UpdateVacantPlotDressingPreview()
{
	UWorld* RuntimeWorld = GetWorld();
	USpawnManager* SpawnMgr = RuntimeWorld ? RuntimeWorld->GetSubsystem<USpawnManager>() : nullptr;
	AVacantPlotDressingManager* DressingManager = SpawnMgr
		? SpawnMgr->GetVacantPlotDressingManager()
		: nullptr;
	if (!DressingManager)
	{
		return;
	}

	AActor* PreviewActor = GetPlacementTargetActor();
	if (!bPlotBuildSession || CurrentPlacementMode != EPlacementMode::Building
		|| !PreviewActor || PendingPlotId.IsNone())
	{
		DressingManager->ClearBuildingPreview();
		return;
	}

	int32 FootprintWidth = FMath::Max(1, PendingFootprintWidthCells);
	int32 FootprintDepth = FMath::Max(1, PendingFootprintDepthCells);
	ApplyYawToFootprintCells(
		PreviewActor->GetActorRotation().Yaw, FootprintWidth, FootprintDepth);
	const FVector PreviewLocation = PreviewActor->GetActorLocation();
	DressingManager->UpdateBuildingPreview(
		PendingPlotId,
		FVector2D(PreviewLocation.X, PreviewLocation.Y),
		FVector2D(
			FootprintWidth * FootprintCellSize * 0.5f,
			FootprintDepth * FootprintCellSize * 0.5f));
}

void UPlacementHandler::ClearVacantPlotDressingPreview()
{
	UWorld* RuntimeWorld = GetWorld();
	USpawnManager* SpawnMgr = RuntimeWorld ? RuntimeWorld->GetSubsystem<USpawnManager>() : nullptr;
	if (AVacantPlotDressingManager* DressingManager = SpawnMgr
		? SpawnMgr->GetVacantPlotDressingManager()
		: nullptr)
	{
		DressingManager->ClearBuildingPreview();
	}
}

void UPlacementHandler::RefreshVacantPlotDressingPlots(
	FName FirstPlotId,
	FName SecondPlotId) const
{
	UWorld* RuntimeWorld = GetWorld();
	USpawnManager* SpawnMgr = RuntimeWorld ? RuntimeWorld->GetSubsystem<USpawnManager>() : nullptr;
	if (AVacantPlotDressingManager* DressingManager = SpawnMgr
		? SpawnMgr->GetVacantPlotDressingManager()
		: nullptr)
	{
		DressingManager->RefreshPlots(FirstPlotId, SecondPlotId);
	}
}

void UPlacementHandler::UpdatePlotFootprintPreview()
{
	// MainMap 부지 건설 세션에서만 footprint을 표시한다. 오피스와 기존 자유 배치는 기존 표시 경로를 유지한다.
	AActor* PreviewActor = GetPlacementTargetActor();
	if (!PlotHighlightBaseMaterial
		|| !GlobalAssetCache
		|| !bPlotBuildSession
		|| CurrentPlacementMode != EPlacementMode::Building
		|| !PreviewActor)
	{
		HidePlotFootprintPreview();
		return;
	}

	UWorld* W = GetWorld();
	if (!W || !W->GetWorldSettings())
	{
		HidePlotFootprintPreview();
		return;
	}

	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FCityPlotData PlotData;
	bool bHasValidPlotData = false;
	if (TableMgr && !PendingPlotId.IsNone())
	{
		PlotData = TableMgr->GetCityPlotData(PendingPlotId, bHasValidPlotData);
	}

	// DT는 유효한 부지의 표면 높이와 점유 정보만 제공한다. 부지/DT가 없어도 실제 크기를 줄이지 않고 빨강으로 표시한다.
	// footprint 한 변(월드) = 칸수 × 셀크기. 격자선 비율 곱하지 않음 — 실제 점유 크기 1:1.
	// 프리뷰 건물의 현재 회전을 반영하되 부지 크기로 줄이지 않는다. oversized도 실제 크기 그대로 빨강 표시한다.
	int32 FootW = FMath::Max(1, PendingFootprintWidthCells);
	int32 FootD = FMath::Max(1, PendingFootprintDepthCells);
	ApplyYawToFootprintCells(PreviewActor->GetActorRotation().Yaw, FootW, FootD);
	const float WidthWorld = FootW * FootprintCellSize;
	const float DepthWorld = FootD * FootprintCellSize;

	// Plane 기본 한 변(보통 100cm)을 실측해 스케일 환산(X=widthWorld/base, Y=depthWorld/base).
	float PlaneBaseSize = 100.f;
	if (UStaticMesh* PlaneMesh = GlobalAssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Plane))
	{
		const FVector PlaneBound = PlaneMesh->GetBounds().BoxExtent * 2.f; // 전체 한 변
		PlaneBaseSize = FMath::Max(KINDA_SMALL_NUMBER, FMath::Max(PlaneBound.X, PlaneBound.Y));
	}
	const float ScaleX = WidthWorld / PlaneBaseSize;
	const float ScaleY = DepthWorld / PlaneBaseSize;

	// footprint 평면 Z — 부지(블록) 표면 위로 살짝 띄워 가시화(블록에 묻혀 안 보이는 문제 회피).
	constexpr float FootprintLiftZ = 10.f;
	const float FootprintZ = bHasValidPlotData ? PlotData.Center.Z + FootprintLiftZ : FootprintLiftZ;

	// XY는 현재 프리뷰 위치를 그대로 쓴다. 유효한 부지에서는 ComputePlotPlacement가 경계 안으로 clamp한다.
	const FVector ActorLoc = PreviewActor->GetActorLocation();
	const FVector FootprintWorldLoc(ActorLoc.X, ActorLoc.Y, FootprintZ);

	// footprint SMC lazy 생성(첫 사용 시 1회). 머티리얼/월드 미비면 graceful 숨김.
	if (!PlotFootprintSMC)
	{
		UStaticMeshComponent* FootprintSMC = NewObject<UStaticMeshComponent>(this);
		if (!FootprintSMC)
		{
			return;
		}
		FootprintSMC->RegisterComponent();
		FootprintSMC->AttachToComponent(W->GetWorldSettings()->GetRootComponent(),
			FAttachmentTransformRules::KeepWorldTransform);
		FootprintSMC->SetStaticMesh(GlobalAssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Plane));
		FootprintSMC->SetGenerateOverlapEvents(false);
		FootprintSMC->SetCollisionProfileName(TEXT("NoCollision"));
		FootprintSMC->SetCanEverAffectNavigation(false);
		FootprintSMC->SetCastShadow(false);
		FootprintSMC->SetVisibility(false);

		PlotFootprintMID = UMaterialInstanceDynamic::Create(PlotHighlightBaseMaterial, this);
		if (PlotFootprintMID)
		{
			FootprintSMC->SetMaterial(0, PlotFootprintMID);
		}

		PlotFootprintSMC = FootprintSMC;
	}

	// 색: 유효(초록) / 무효(빨강). 기존 점유 공간은 아래 별도 마커가 검정을 유지한다.
	const bool bShowValidFootprint = bHasValidPlotData && CanDrop;
	const FLinearColor FootprintColor = bShowValidFootprint
		? FLinearColor(0.05f, 0.75f, 0.16f, 0.42f) // 현재 놓을 수 있는 footprint
		: FLinearColor(0.95f, 0.15f, 0.10f, 0.6f); // 불가(빨강)
	if (PlotFootprintMID)
	{
		PlotFootprintMID->SetVectorParameterValue(TEXT("Color"), FootprintColor);
	}

	PlotFootprintSMC->SetWorldLocation(FootprintWorldLoc);
	PlotFootprintSMC->SetWorldRotation(FRotator::ZeroRotator);
	PlotFootprintSMC->SetWorldScale3D(FVector(ScaleX, ScaleY, 1.f));
	PlotFootprintSMC->SetVisibility(true);

	// === 이미 차지된 공간 — 호버 부지의 기존 건물들 footprint 사각형(검은). 건물 1채당 1개(셀 격자 아님). ===
	// 내 프리뷰(PlacementTargetEntity)는 제외(재배치 중인 건물 자신도). 점유 마커는 살짝 낮은 Z로 깔아
	// 내 프리뷰가 위로 보이게 한다. 내 프리뷰가 점유 위로 가면 ComputePlotPlacement 가 무효→빨강이 된다.
	constexpr float OccupiedLiftZ = 8.f;
	const FLinearColor OccupiedColor(0.0f, 0.0f, 0.0f, 0.6f); // 이미 차지 = 검은(프리뷰보다 살짝 진하게)

	int32 MarkerIndex = 0;
	if (bHasValidPlotData && TableMgr)
	{
		const float OccupiedZ = PlotData.Center.Z + OccupiedLiftZ;
		if (UEntityManager* EntityMgr = W->GetSubsystem<UEntityManager>())
		{
			const TArray<ABuildingBaseActor*> PlotBuildings = EntityMgr->GetBuildingsOnPlot(PendingPlotId);
			for (ABuildingBaseActor* Existing : PlotBuildings)
			{
				if (MarkerIndex >= MaxPlotOccupiedMarkers)
				{
					break;
				}
				if (!IsValid(Existing) || Existing == PlacementTargetEntity)
				{
					continue;
				}

				int32 OtherW = 1;
				int32 OtherD = 1;
				bool bBOk = false;
				const FBuildingData OtherData = TableMgr->GetBuildingData(Existing->GetInteractableRowName(), bBOk);
				if (bBOk)
				{
					OtherW = FMath::Max(1, OtherData.FootprintWidthCells);
					OtherD = FMath::Max(1, OtherData.FootprintDepthCells);
				}
				// 기존 건물도 회전 상태로 저장/복원되므로(EntityManager 스폰 트랜스폼) 점유 축을 회전에 맞춘다.
				ApplyYawToFootprintCells(Existing->GetActorRotation().Yaw, OtherW, OtherD);

				UStaticMeshComponent* MarkerSMC = GetOrCreatePlotOccupiedSMC(MarkerIndex);
				if (!MarkerSMC)
				{
					break; // 생성 실패(머티리얼/월드 부재) — 더 진행 의미 없음.
				}

				// 마커 = 기존 건물 위치 중심, 그 건물 footprint 월드 크기(겹침 판정 AABB 와 동일 박스).
				const FVector OtherLoc = Existing->GetActorLocation();
				const float MarkerScaleX = (OtherW * FootprintCellSize) / PlaneBaseSize;
				const float MarkerScaleY = (OtherD * FootprintCellSize) / PlaneBaseSize;

				MarkerSMC->SetWorldLocation(FVector(OtherLoc.X, OtherLoc.Y, OccupiedZ));
				MarkerSMC->SetWorldRotation(FRotator::ZeroRotator);
				MarkerSMC->SetWorldScale3D(FVector(MarkerScaleX, MarkerScaleY, 1.f));
				if (PlotOccupiedMIDs.IsValidIndex(MarkerIndex) && PlotOccupiedMIDs[MarkerIndex])
				{
					PlotOccupiedMIDs[MarkerIndex]->SetVectorParameterValue(TEXT("Color"), OccupiedColor);
				}
				MarkerSMC->SetVisibility(true);
				++MarkerIndex;
			}
		}
	}

	// 안 쓴 점유 마커 숨김(이전 프레임/이전 부지 잔상 제거).
	for (int32 i = MarkerIndex; i < PlotOccupiedSMCs.Num(); ++i)
	{
		if (PlotOccupiedSMCs[i])
		{
			PlotOccupiedSMCs[i]->SetVisibility(false);
		}
	}
}

FContactSnapResult UPlacementHandler::ComputeContactSnap(
	const FVector2D& RawXY, float HalfX, float HalfY,
	TArrayView<const FNeighborFootprint> Neighbors,
	float SnapDistance, float Gap, bool bAlreadyOverlapping)
{
	// X/Y 는 독립 판정 — 구석에서 두 축이 동시에 붙어야 "구석에 딱"이 된다.
	FContactSnapResult Result;
	if (SnapDistance <= 0.f)
	{
		return Result; // 0 = 자석 완전 비활성(회귀 시 즉시 끄는 탈출구)
	}

	auto Consider = [&](int32 Axis, float TargetCenter)
	{
		const float Delta = TargetCenter - RawXY[Axis];
		if (FMath::Abs(Delta) <= SnapDistance
			&& (!Result.bHasDelta[Axis] || FMath::Abs(Delta) < FMath::Abs(Result.Delta[Axis])))
		{
			Result.Delta[Axis] = Delta;
			Result.bHasDelta[Axis] = true;
		}
	};

	// 반대축이 겹쳐야 접촉이 의미 있다(대각선 위치 대상에 X접촉은 무의미).
	// 게이트에는 Gap 을 넣지 않는다 — 두 사각형의 실제 span 이 겹치는지를 묻는 것이지, 띄운 목표를 묻는 게 아니다.
	// 겹침 판정은 스냅 전 좌표 기준 — X/Y 가 서로를 참조하지 않아 순서 의존이 없다.
	for (const FNeighborFootprint& N : Neighbors)
	{
		if (FMath::Abs(RawXY.Y - N.Center.Y) < (HalfY + N.HalfY))
		{
			Consider(0, N.Center.X - (N.HalfX + HalfX + Gap));
			Consider(0, N.Center.X + (N.HalfX + HalfX + Gap));
		}
		if (FMath::Abs(RawXY.X - N.Center.X) < (HalfX + N.HalfX))
		{
			Consider(1, N.Center.Y - (N.HalfY + HalfY + Gap));
			Consider(1, N.Center.Y + (N.HalfY + HalfY + Gap));
		}
	}

	// 이미 겹친 상태면 침투가 얕은 축 하나로만 밀어낸다(최소 이동축 = MTV 정석).
	// 겹침은 두 축 범위가 모두 겹쳤다는 뜻이라 게이트가 둘 다 열리는데, 그대로 둘 다 밀면
	// 대각선으로 튕겨 나가 밀어넣은 방향에 따라 결과가 제각각이 된다.
	// 바깥에서 접근하는 비겹침 상태에서만 축 독립을 유지한다(구석 채우기).
	if (Result.bHasDelta[0] && Result.bHasDelta[1] && bAlreadyOverlapping)
	{
		const int32 DropAxis = (FMath::Abs(Result.Delta[0]) <= FMath::Abs(Result.Delta[1])) ? 1 : 0;
		Result.bHasDelta[DropAxis] = false;
	}

	return Result;
}

FVector UPlacementHandler::ComputeOfficeFloorPosition(const FVector& InWorldPos) const
{
	// 5 유닛 격자
	FVector Stepped = InWorldPos / 5.f;
	Stepped.X = FMath::RoundToFloat(Stepped.X) * 5.f;
	Stepped.Y = FMath::RoundToFloat(Stepped.Y) * 5.f;
	Stepped.Z = 0.f;

	const FVector2D SnappedXY = ApplyOfficeContactSnap(FVector2D(Stepped.X, Stepped.Y));
	Stepped.X = SnappedXY.X;
	Stepped.Y = SnappedXY.Y;
	return Stepped;
}

FVector2D UPlacementHandler::ApplyOfficeContactSnap(const FVector2D& InXY) const
{
	const AActor* Preview = GetPlacementTargetActor();
	UWorld* W = GetWorld();
	if (!Preview || !W || OfficeSnapDistanceRatio <= 0.f)
	{
		return InXY;
	}

	// 프리뷰 점유 사각형 — 월드 축정렬 bounds 라 90도 회전이 자동 반영된다
	// (MainMap 처럼 격자 칸수를 Yaw 로 뒤집어줄 필요가 없다).
	FVector PreviewOrigin, PreviewExtent;
	Preview->GetActorBounds(false, PreviewOrigin, PreviewExtent);
	const float HalfX = static_cast<float>(PreviewExtent.X);
	const float HalfY = static_cast<float>(PreviewExtent.Y);
	if (HalfX <= 0.f || HalfY <= 0.f)
	{
		return InXY;
	}

	// 임계 거리는 배치물 짧은 축 폭에 비례 — 1인/2인/4인 책상 폭이 달라 고정 cm 는 그중 하나에만 맞는다.
	const float SnapDistance = FMath::Min(HalfX, HalfY) * 2.f * OfficeSnapDistanceRatio;

	TArray<FNeighborFootprint, TInlineAllocator<32>> Neighbors;
	auto AddActorFootprint = [&](const AActor* Other)
	{
		if (!IsValid(Other) || Other == Preview)
		{
			return;
		}
		FVector O, E;
		Other->GetActorBounds(false, O, E);
		if (E.X > 0.f && E.Y > 0.f)
		{
			Neighbors.Add({ FVector2D(O.X, O.Y), static_cast<float>(E.X), static_cast<float>(E.Y) });
		}
	};

	// 월드 순회 대신 OfficeManager 등록부를 쓴다 — 배치 확정이 여기에 등록하므로 항상 최신이다.
	if (const UOfficeManager* OfficeMgr = W->GetSubsystem<UOfficeManager>())
	{
		for (const AWorkstationActorBase* WS : OfficeMgr->GetPlacedWorkstations()) { AddActorFootprint(WS); }
		for (const ADecorationActor* Dec : OfficeMgr->GetPlacedDecorations()) { AddActorFootprint(Dec); }
	}

	// 벽도 같은 규약(축정렬 사각형)으로 후보에 넣어 이웃과 동일하게 처리한다.
	// Left 벽은 Y 축에 수직(방 안쪽 +Y), Right 벽은 X 축에 수직(방 안쪽 -X).
	// 길이 방향 반경을 크게 잡아 반대축 겹침 게이트를 항상 통과시키고(벽 전체가 붙일 면),
	// 두께 방향은 얇게 둬서 벽 "끝"에 붙는 엉뚱한 후보가 생기지 않게 한다.
	if (const AOfficeInterior* Interior = CachedOfficeInterior)
	{
		constexpr float WallSpanHalf = 100000.f;
		constexpr float WallThicknessHalf = 10.f;

		const FVector LeftLoc = Interior->GetWallLocation(EWallSide::Left);
		Neighbors.Add({ FVector2D(LeftLoc.X, LeftLoc.Y - WallThicknessHalf), WallSpanHalf, WallThicknessHalf });

		const FVector RightLoc = Interior->GetWallLocation(EWallSide::Right);
		Neighbors.Add({ FVector2D(RightLoc.X + WallThicknessHalf, RightLoc.Y), WallThicknessHalf, WallSpanHalf });
	}

	if (Neighbors.Num() == 0)
	{
		return InXY;
	}

	auto OverlapsAny = [&](const FVector2D& XY) -> bool
	{
		for (const FNeighborFootprint& N : Neighbors)
		{
			if (FMath::Abs(XY.X - N.Center.X) < (HalfX + N.HalfX)
				&& FMath::Abs(XY.Y - N.Center.Y) < (HalfY + N.HalfY))
			{
				return true;
			}
		}
		return false;
	};

	// 가구는 딱 붙이지 않고 PlacementPadding 만큼 띄운다 — 연속 배치가 프리뷰를 미는 간격과 같은 값이라야
	// 손으로 놓은 줄과 자동으로 밀린 줄이 어긋나지 않는다.
	const FContactSnapResult Snap = ComputeContactSnap(
		InXY, HalfX, HalfY, Neighbors, SnapDistance, PlacementPadding, OverlapsAny(InXY));

	auto Apply = [&](bool bUseX, bool bUseY) -> FVector2D
	{
		FVector2D XY = InXY;
		if (bUseX && Snap.bHasDelta[0]) { XY.X += Snap.Delta[0]; }
		if (bUseY && Snap.bHasDelta[1]) { XY.Y += Snap.Delta[1]; }
		return XY;
	};

	// 자석은 상황을 악화시키지 않는다 — 결과가 겹치면 축을 하나씩 빼보고, 그래도 겹치면 손가락 위치를 존중한다.
	FVector2D Chosen = Apply(true, true);
	if (OverlapsAny(Chosen) && Snap.bHasDelta[0] && Snap.bHasDelta[1])
	{
		Chosen = Apply(true, false);
		if (OverlapsAny(Chosen))
		{
			Chosen = Apply(false, true);
		}
	}
	if (OverlapsAny(Chosen))
	{
		Chosen = InXY;
	}
	return Chosen;
}

bool UPlacementHandler::TryFindInitialPlotPlacement(
	const FVector& DesiredWorldPos,
	FVector& OutPlacementPos,
	FName& OutPlotId) const
{
	OutPlacementPos = DesiredWorldPos;
	OutPlotId = NAME_None;

	UWorld* RuntimeWorld = GetWorld();
	USpawnManager* SpawnMgr = RuntimeWorld ? RuntimeWorld->GetSubsystem<USpawnManager>() : nullptr;
	if (!bPlotBuildSession || !SpawnMgr)
	{
		return false;
	}

	int32 FootW = FMath::Max(1, PendingFootprintWidthCells);
	int32 FootD = FMath::Max(1, PendingFootprintDepthCells);
	if (const AActor* PreviewActor = GetPlacementTargetActor())
	{
		ApplyYawToFootprintCells(PreviewActor->GetActorRotation().Yaw, FootW, FootD);
	}
	const float HalfX = FootW * FootprintCellSize * 0.5f;
	const float HalfY = FootD * FootprintCellSize * 0.5f;
	const FVector2D DesiredXY(DesiredWorldPos.X, DesiredWorldPos.Y);

	struct FInitialPlotSearchEntry
	{
		TWeakObjectPtr<ACityPlotActor> Plot;
		FName PlotId = NAME_None;
		FVector2D ClampedDesired = FVector2D::ZeroVector;
		double LowerBoundDistanceSq = 0.0;
	};
	TArray<FInitialPlotSearchEntry> SearchEntries;

	TArray<FName> OwnedPlotIds;
	SpawnMgr->GatherOwnedPlotIds(OwnedPlotIds);
	SearchEntries.Reserve(OwnedPlotIds.Num());
	for (const FName PlotId : OwnedPlotIds)
	{
		ACityPlotActor* PlotActor = SpawnMgr->GetSpawnedPlotById(PlotId);
		if (!IsValid(PlotActor) || !PlotActor->IsOwned() || !HasCapacityForCurrentPlacement(PlotId))
		{
			continue;
		}

		const FVector PlotCenter3D = PlotActor->GetCenter();
		const FVector2D PlotCenter(PlotCenter3D.X, PlotCenter3D.Y);
		const FVector2D PlotExtent = PlotActor->GetExtent();
		if (!FPlotPlacementRules::DoesCurrentOrientationFitWithinPlot(
			PlotExtent.X, PlotExtent.Y, HalfX, HalfY))
		{
			continue;
		}

		const FVector2D MinCenter = PlotCenter - FVector2D(PlotExtent.X - HalfX, PlotExtent.Y - HalfY);
		const FVector2D MaxCenter = PlotCenter + FVector2D(PlotExtent.X - HalfX, PlotExtent.Y - HalfY);
		const FVector2D ClampedDesired(
			FMath::Clamp(DesiredXY.X, MinCenter.X, MaxCenter.X),
			FMath::Clamp(DesiredXY.Y, MinCenter.Y, MaxCenter.Y));
		SearchEntries.Add({
			PlotActor,
			PlotId,
			ClampedDesired,
			FVector2D::DistSquared(DesiredXY, ClampedDesired)
		});
	}

	SearchEntries.Sort([](const FInitialPlotSearchEntry& Left, const FInitialPlotSearchEntry& Right)
	{
		if (Left.LowerBoundDistanceSq != Right.LowerBoundDistanceSq)
		{
			return Left.LowerBoundDistanceSq < Right.LowerBoundDistanceSq;
		}
		return Left.PlotId.LexicalLess(Right.PlotId);
	});

	// 공통 경로는 plot당 footprint-aware clamp 1점만 평가한다. 그 점이 무효인 가까운 부지만 5x5 fallback을 돈다.
	// 최악 비용도 clamp=소유 후보 수, fallback=4 plots x 25 candidates로 제한한다.
	constexpr int32 SamplesPerAxis = 5;
	constexpr int32 MaxFallbackPlots = 4;
	constexpr int32 MaxFallbackCandidatesPerPlot = SamplesPerAxis * SamplesPerAxis;
	int32 FallbackPlotsEvaluated = 0;

	bool bHasBestCandidate = false;
	FVector2D BestCandidatePosition = FVector2D::ZeroVector;
	FName BestCandidatePlotId = NAME_None;
	double BestCandidateDistanceSq = TNumericLimits<double>::Max();

	auto ConsiderCandidate = [&](const FVector2D& CandidatePosition, FName CandidatePlotId)
	{
		const double CandidateDistanceSq = FVector2D::DistSquared(DesiredXY, CandidatePosition);
		if (!bHasBestCandidate
			|| CandidateDistanceSq < BestCandidateDistanceSq
			|| (CandidateDistanceSq == BestCandidateDistanceSq
				&& CandidatePlotId.LexicalLess(BestCandidatePlotId)))
		{
			bHasBestCandidate = true;
			BestCandidatePosition = CandidatePosition;
			BestCandidatePlotId = CandidatePlotId;
			BestCandidateDistanceSq = CandidateDistanceSq;
		}
	};

	auto EvaluateCandidate = [&](const FInitialPlotSearchEntry& SearchEntry,
		const FVector2D& CandidateInput, FVector2D& OutCandidatePosition) -> bool
	{
		FVector SnappedPosition;
		bool bValidCandidate = false;
		FName ResolvedPlotId = NAME_None;
		const bool bHandled = ComputePlotPlacement(
			FVector(CandidateInput.X, CandidateInput.Y, DesiredWorldPos.Z),
			SnappedPosition,
			bValidCandidate,
			ResolvedPlotId);
		OutCandidatePosition = FVector2D(SnappedPosition.X, SnappedPosition.Y);
		return bHandled && bValidCandidate && ResolvedPlotId == SearchEntry.PlotId;
	};

	for (const FInitialPlotSearchEntry& SearchEntry : SearchEntries)
	{
		ACityPlotActor* PlotActor = SearchEntry.Plot.Get();
		if (!PlotActor)
		{
			continue;
		}

		// 정렬된 부지의 footprint-aware clamp 거리는 이 부지에서 가능한 최단거리다.
		// 현재 best보다 멀거나, 동거리인데 PlotId tie-break도 이길 수 없으면 이후 부지도 전부 가지치기한다.
		if (bHasBestCandidate
			&& (SearchEntry.LowerBoundDistanceSq > BestCandidateDistanceSq
				|| (SearchEntry.LowerBoundDistanceSq == BestCandidateDistanceSq
					&& !SearchEntry.PlotId.LexicalLess(BestCandidatePlotId))))
		{
			break;
		}

		FVector2D ClampedCandidatePosition;
		if (EvaluateCandidate(SearchEntry, SearchEntry.ClampedDesired, ClampedCandidatePosition))
		{
			ConsiderCandidate(ClampedCandidatePosition, SearchEntry.PlotId);
			if (ClampedCandidatePosition == SearchEntry.ClampedDesired)
			{
				// 현재 부지의 수학적 lower-bound가 실제 유효점이다. 뒤 부지는 더 가까워질 수 없다.
				break;
			}
		}

		if (FallbackPlotsEvaluated >= MaxFallbackPlots)
		{
			continue;
		}
		++FallbackPlotsEvaluated;

		const FVector PlotCenter3D = PlotActor->GetCenter();
		const FVector2D PlotCenter(PlotCenter3D.X, PlotCenter3D.Y);
		const FVector2D PlotExtent = PlotActor->GetExtent();
		const double MinX = PlotCenter.X - PlotExtent.X + HalfX;
		const double MaxX = PlotCenter.X + PlotExtent.X - HalfX;
		const double MinY = PlotCenter.Y - PlotExtent.Y + HalfY;
		const double MaxY = PlotCenter.Y + PlotExtent.Y - HalfY;

		TArray<double, TInlineAllocator<SamplesPerAxis>> SampleXs;
		TArray<double, TInlineAllocator<SamplesPerAxis>> SampleYs;
		for (int32 SampleIndex = 0; SampleIndex < SamplesPerAxis; ++SampleIndex)
		{
			const double Alpha = static_cast<double>(SampleIndex) / static_cast<double>(SamplesPerAxis - 1);
			SampleXs.Add(FMath::Lerp(MinX, MaxX, Alpha));
			SampleYs.Add(FMath::Lerp(MinY, MaxY, Alpha));
		}

		TArray<FVector2D> CandidatePositions;
		TArray<uint8> CandidateValidity;
		CandidatePositions.Reserve(MaxFallbackCandidatesPerPlot);
		CandidateValidity.Reserve(MaxFallbackCandidatesPerPlot);
		for (const double SampleX : SampleXs)
		{
			for (const double SampleY : SampleYs)
			{
				const FVector2D CandidateInput(SampleX, SampleY);
				if (CandidateInput.Equals(SearchEntry.ClampedDesired, 0.5))
				{
					continue; // clamp 1점은 위에서 이미 정확히 평가했다.
				}

				FVector2D CandidatePosition;
				const bool bValidCandidate = EvaluateCandidate(SearchEntry, CandidateInput, CandidatePosition);
				CandidatePositions.Add(CandidatePosition);
				CandidateValidity.Add(bValidCandidate ? 1 : 0);
			}
		}

		const int32 BestCandidateIndex = FPlotPlacementRules::FindNearestValidCandidateIndex(
			DesiredXY,
			MakeArrayView(CandidatePositions),
			MakeArrayView(CandidateValidity));
		if (BestCandidateIndex != INDEX_NONE)
		{
			ConsiderCandidate(CandidatePositions[BestCandidateIndex], SearchEntry.PlotId);
		}
	}

	if (!bHasBestCandidate)
	{
		return false;
	}

	OutPlacementPos.X = BestCandidatePosition.X;
	OutPlacementPos.Y = BestCandidatePosition.Y;
	OutPlotId = BestCandidatePlotId;
	return true;
}

bool UPlacementHandler::ComputePlotPlacement(const FVector& InWorldPos, FVector& OutSnappedPos, bool& bOutValidCell, FName& OutResolvedPlotId) const
{
	OutSnappedPos = InWorldPos;
	bOutValidCell = false;
	OutResolvedPlotId = NAME_None;

	if (!bPlotBuildSession)
	{
		return false; // 부지 건설 세션 아님 — 호출자가 기존 자유 배치로 폴백 (오피스 등)
	}

	UWorld* W = GetWorld();
	USpawnManager* SpawnMgr = W ? W->GetSubsystem<USpawnManager>() : nullptr;
	if (!SpawnMgr)
	{
		// 세션이지만 매니저 부재 — 자유 배치 폴백은 막고(부지 강제) 빨강 처리(clamp 없음, 무효).
		return true;
	}

	// === 동적 호버 부지 판정 — 프리뷰 입력 위치 밑의 소유 부지 ===
	ACityPlotActor* HoverPlot = SpawnMgr->GetOwnedPlotAt(InWorldPos);
	if (!HoverPlot)
	{
		// 소유 부지 위가 아님 → clamp 안 함(입력 유지), 무효(빨강). 세션이므로 true(자유 배치 폴백 차단).
		return true;
	}

	// 부지 경계: Center ± Extent(반경). 부지는 ZeroRotator 축정렬이라 월드 XY = 부지 축.
	const FVector PlotCenter = HoverPlot->GetCenter();
	const FVector2D Extent = HoverPlot->GetExtent();

	// 현재 회전 footprint은 줄이지 않는다. 부지보다 큰 축은 PlotCenter로 clamp하고 exact-fit 규칙이 무효 처리한다.
	int32 FootW = FMath::Max(1, PendingFootprintWidthCells);
	int32 FootD = FMath::Max(1, PendingFootprintDepthCells);
	if (const AActor* PreviewActor = GetPlacementTargetActor())
	{
		ApplyYawToFootprintCells(PreviewActor->GetActorRotation().Yaw, FootW, FootD);
	}

	// footprint 월드 반경.
	const float HalfX = FootW * FootprintCellSize * 0.5f;
	const float HalfY = FootD * FootprintCellSize * 0.5f;
	const bool bFitsCurrentOrientation = FPlotPlacementRules::DoesCurrentOrientationFitWithinPlot(
		Extent.X, Extent.Y, HalfX, HalfY);

	// === 위치 clamp — footprint 사각형이 부지 안에 통째로 들어오도록 center 만 clamp ===
	// center.X ∈ [Center.X - ExtentX + HalfX, Center.X + ExtentX - HalfX]. footprint 가 부지보다 크면(Min>Max)
	// Center.X 로 clamp(Min>Max 가드). Z 는 호출자가 사용하지 않음 — 입력 Z 그대로.
	const float MinCenterX = PlotCenter.X - Extent.X + HalfX;
	const float MaxCenterX = PlotCenter.X + Extent.X - HalfX;
	const float MinCenterY = PlotCenter.Y - Extent.Y + HalfY;
	const float MaxCenterY = PlotCenter.Y + Extent.Y - HalfY;

	auto ClampCenterToPlot = [&](FVector2D& InOutXY)
	{
		InOutXY.X = (MinCenterX > MaxCenterX) ? PlotCenter.X : FMath::Clamp(InOutXY.X, MinCenterX, MaxCenterX);
		InOutXY.Y = (MinCenterY > MaxCenterY) ? PlotCenter.Y : FMath::Clamp(InOutXY.Y, MinCenterY, MaxCenterY);
	};

	FVector2D RawXY(InWorldPos.X, InWorldPos.Y);
	ClampCenterToPlot(RawXY);
	OutResolvedPlotId = HoverPlot->GetPlotId();

	// === 이웃 건물 footprint 사각형 수집(회전 반영) ===
	// 접촉 자석 후보와 겹침 판정이 같은 목록을 공유한다 — 두 곳이 다른 사각형을 보면 "빨간데 붙거나
	// 검은데 안 붙는" 불일치가 난다. 프리뷰 건물 자신은 제외(재배치 중인 건물 자신도).
	TArray<FNeighborFootprint, TInlineAllocator<16>> Neighbors;

	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (UEntityManager* EntityMgr = W->GetSubsystem<UEntityManager>())
	{
		const TArray<ABuildingBaseActor*> PlotBuildings = EntityMgr->GetBuildingsOnPlot(OutResolvedPlotId);
		for (ABuildingBaseActor* Existing : PlotBuildings)
		{
			if (!IsValid(Existing) || Existing == PlacementTargetEntity)
			{
				continue;
			}

			int32 OtherW = 1;
			int32 OtherD = 1;
			if (TableMgr)
			{
				bool bBOk = false;
				const FBuildingData OtherData = TableMgr->GetBuildingData(Existing->GetInteractableRowName(), bBOk);
				if (bBOk)
				{
					OtherW = FMath::Max(1, OtherData.FootprintWidthCells);
					OtherD = FMath::Max(1, OtherData.FootprintDepthCells);
				}
			}
			// 마커 표시(UpdatePlotFootprintPreview)와 동일 규약 — 회전한 기존 건물의 실제 점유 축.
			ApplyYawToFootprintCells(Existing->GetActorRotation().Yaw, OtherW, OtherD);

			const FVector OtherLoc = Existing->GetActorLocation();
			Neighbors.Add({ FVector2D(OtherLoc.X, OtherLoc.Y),
				OtherW * FootprintCellSize * 0.5f, OtherD * FootprintCellSize * 0.5f });
		}
	}

	// 연속 AABB 겹침(footprint 사각형 ↔ 이웃 footprint 사각형).
	// 접촉면에 1cm 여유를 둔다 — 자석이 만든 접촉 좌표는 `Raw + (Target - Raw)` 라 부동소수 반올림으로
	// 목표에서 미세하게 안쪽에 떨어질 수 있고(월드 좌표가 클수록 오차가 커져 축마다 다르게 나타난다),
	// 그러면 자석이 방금 붙인 자리를 스스로 겹침으로 판정해 스냅을 취소해 버린다.
	constexpr float ContactEpsilon = 1.f;
	auto OverlapsNeighbor = [&](const FVector2D& XY) -> bool
	{
		for (const FNeighborFootprint& N : Neighbors)
		{
			if (FMath::Abs(XY.X - N.Center.X) < (HalfX + N.HalfX - ContactEpsilon)
				&& FMath::Abs(XY.Y - N.Center.Y) < (HalfY + N.HalfY - ContactEpsilon))
			{
				return true;
			}
		}
		return false;
	};

	// === 접촉 자석 — 이웃 건물 변만 ===
	// 부지 AABB 변은 스냅 대상이 아니다. 부지 사각형은 GridCols×셀크기로 "유도한" 근사 경계라
	// 실제 블록보다 크고, 그 차이가 곧 인도다 — 거기 붙이면 정의상 인도 위(빨강)가 된다.
	// 건설 가능 경계는 ResolveFootprintOntoGround(블록 메시 트레이스), 부지 사각형은 하드 clamp만
	// 건물은 Gap=0으로 밀착. 오피스 가구만 PlacementPadding만큼 이격
	const FContactSnapResult Snap = ComputeContactSnap(
		RawXY, HalfX, HalfY, Neighbors, PlacementSnapDistance, 0.f, OverlapsNeighbor(RawXY));
	const float* BestDelta = Snap.Delta;
	const bool* bHasDelta = Snap.bHasDelta;

	// 미리보기와 실제 확정이 이 람다 하나 공유 (갈리면 초록 표시인데 건설 불가인 자리 발생)
	// 후보 1개 평가: 스냅 → 부지 clamp → 지면 보정(인도 → 블록 안쪽) → 유효성. 경계 초과분은 블록 가장자리에 밀착
	struct FPlacementCandidate { FVector2D XY; bool bValid; };
	auto EvaluateCandidate = [&](bool bUseX, bool bUseY) -> FPlacementCandidate
	{
		FVector2D XY = RawXY;
		if (bUseX && bHasDelta[0]) { XY.X += BestDelta[0]; }
		if (bUseY && bHasDelta[1]) { XY.Y += BestDelta[1]; }
		ClampCenterToPlot(XY);
		if (!bFitsCurrentOrientation)
		{
			return { XY, false };
		}

		FVector2D GroundXY = XY;
		bool bGrounded = HoverPlot->ResolveFootprintOntoGround(
			FVector(XY.X, XY.Y, PlotCenter.Z), HalfX, HalfY, PlotGroundPushMax, GroundXY);

		// 지면 보정이 이웃과 겹침을 만들면 그 자리는 애초에 불가 — 보정을 되돌리고 무효 처리.
		if (bGrounded && GroundXY != XY && OverlapsNeighbor(GroundXY))
		{
			GroundXY = XY;
			bGrounded = false;
		}
		return { GroundXY, !OverlapsNeighbor(GroundXY) && bGrounded };
	};

	// 자석은 상황을 악화시키지 않음: 스냅 무효면 축을 하나씩 제외, 그래도 무효면 스냅 포기 (손가락 위치 존중)
	// 정상 경우는 평가 1회, 폴백은 실제 무효일 때만
	FPlacementCandidate Chosen = EvaluateCandidate(true, true);
	if (!Chosen.bValid && bHasDelta[0] && bHasDelta[1])
	{
		Chosen = EvaluateCandidate(true, false);
		if (!Chosen.bValid)
		{
			Chosen = EvaluateCandidate(false, true);
		}
	}
	if (!Chosen.bValid && (bHasDelta[0] || bHasDelta[1]))
	{
		Chosen = EvaluateCandidate(false, false);
	}

	OutSnappedPos.X = Chosen.XY.X;
	OutSnappedPos.Y = Chosen.XY.Y;

	// 최종 확정과 후보 피드백이 같은 capacity helper를 사용한다. 이동 중 원래 부지에서는 자기 자신만 제외한다.
	bOutValidCell = bFitsCurrentOrientation
		&& Chosen.bValid
		&& HasCapacityForCurrentPlacement(OutResolvedPlotId);
	return true;
}

void UPlacementHandler::TrackMovePlacement()
{
	AActor* TargetActor = GetPlacementTargetActor();
	if (!TargetActor) return;

	// WallDecoration 모드는 레이캐스트로 벽 Hit 위치 사용
	if (CurrentPlacementMode == EPlacementMode::WallDecoration)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (!PC) return;

		FVector2D ScreenPos;

		// 터치 입력 먼저 체크, 없으면 마우스 위치 사용
		float TouchX, TouchY;
		bool bIsTouchPressed;
		PC->GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bIsTouchPressed);

		if (bIsTouchPressed)
		{
			ScreenPos.X = TouchX;
			ScreenPos.Y = TouchY;
		}
		else
		{
			PC->GetMousePosition(ScreenPos.X, ScreenPos.Y);
		}

		FVector WorldOrigin, WorldDirection;
		PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldOrigin, WorldDirection);

		// 레이캐스트로 벽에 맞은 위치 찾기
		FHitResult HitResult;
		FVector TraceEnd = WorldOrigin + (WorldDirection * 10000.f);

		// 배치 중인 장식품은 무시
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(TargetActor);

		bool bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			WorldOrigin,
			TraceEnd,
			ECC_Visibility,
			QueryParams
		);

		UE_LOG(LogTemp, Warning, TEXT("[TrackMovePlacement] WallDecoration Raycast: bHit=%d, HitActor=%s, ScreenPos=(%f, %f)"),
			bHit,
			HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("None"),
			ScreenPos.X, ScreenPos.Y);

		if (bHit)
		{
			FVector HitLocation = HitResult.Location;
			UE_LOG(LogTemp, Warning, TEXT("[TrackMovePlacement] HitLocation: %s, CurrentWallSide: %s"),
				*HitLocation.ToString(), CurrentWallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"));

			// Hit된 컴포넌트 이름으로 벽 전환 감지
			UPrimitiveComponent* HitComponent = HitResult.GetComponent();
			if (HitComponent)
			{
				FString CompName = HitComponent->GetName();
				if (CompName.Contains(TEXT("WallLeft")) && CurrentWallSide != EWallSide::Left)
				{
					SwitchToWall(EWallSide::Left, HitLocation);
					return;
				}
				else if (CompName.Contains(TEXT("WallRight")) && CurrentWallSide != EWallSide::Right)
				{
					SwitchToWall(EWallSide::Right, HitLocation);
					return;
				}
			}

			// 벽 범위 가져오기
			FVector MinBounds = FVector::ZeroVector;
			FVector MaxBounds = FVector(0.f, 0.f, 300.f);

			AOfficeInterior* OfficeInterior = Cast<AOfficeInterior>(
				UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass()));
			if (OfficeInterior)
			{
				OfficeInterior->GetWallPlacementBounds(CurrentWallSide, MinBounds, MaxBounds);
			}

			// 그리드 스냅 (5 유닛) 및 고정 축 설정
			FVector SteppedPosition = HitLocation;

			if (CurrentWallSide == EWallSide::Left)
			{
				// Left 벽: X, Z만 이동, Y=-11 고정
				SteppedPosition.X = FMath::RoundToFloat(HitLocation.X / 5.f) * 5.f;
				SteppedPosition.Z = FMath::RoundToFloat(HitLocation.Z / 5.f) * 5.f;
				SteppedPosition.Y = -11.f;

				// Z만 클램핑 (X는 벽 길이에 따라 자유롭게 이동)
				SteppedPosition.Z = FMath::Clamp(SteppedPosition.Z, MinBounds.Z, MaxBounds.Z);
			}
			else
			{
				// Right 벽: Y, Z만 이동, X=4 고정
				SteppedPosition.Y = FMath::RoundToFloat(HitLocation.Y / 5.f) * 5.f;
				SteppedPosition.Z = FMath::RoundToFloat(HitLocation.Z / 5.f) * 5.f;
				SteppedPosition.X = 4.f;

				// Z만 클램핑 (Y는 벽 길이에 따라 자유롭게 이동)
				SteppedPosition.Z = FMath::Clamp(SteppedPosition.Z, MinBounds.Z, MaxBounds.Z);
			}

			FVector OldActorLocation = TargetActor->GetActorLocation();
			UE_LOG(LogTemp, Warning, TEXT("[TrackMovePlacement] OldLocation: %s, NewLocation: %s"),
				*OldActorLocation.ToString(), *SteppedPosition.ToString());

			if (!FVector::PointsAreSame(SteppedPosition, OldActorLocation))
			{
				TargetActor->SetActorLocation(SteppedPosition);
				UE_LOG(LogTemp, Warning, TEXT("[TrackMovePlacement] Actor MOVED to: %s"), *SteppedPosition.ToString());
				UpdateBuildAsset();
			}
		}
		return;
	}

	// 기존 바닥 배치 로직
	FVector2D ScreenPos;
	FVector IntersectionPos;
	if (CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, IntersectionPos))
	{
		FVector SteppedPosition;
		switch (CurrentPlacementMode)
		{
		case EPlacementMode::Workstation:
		case EPlacementMode::Decoration:
			SteppedPosition = ComputeOfficeFloorPosition(IntersectionPos);
			break;
		case EPlacementMode::Building:
		default:
			// 부지 세션이면 프리뷰 밑 소유 부지(동적 판정)에 자유 배치(부지밖 금지 + 겹침방지),
			// 아니면 기존 200 유닛 그리드. 부지밖이면 입력 위치 유지 + bPlotPlacementValid=false(빨강).
			{
				FVector PlotSnapped;
				bool bValidCell = false;
				FName ResolvedPlotId = NAME_None;
				if (ComputePlotPlacement(IntersectionPos, PlotSnapped, bValidCell, ResolvedPlotId))
				{
					// XY 만 부지 clamp 결과로 대체. Z 는 아래 Floating 처리에 맡겨 메시 높이/스케일 불변 보장.
					SteppedPosition = PlotSnapped;
					bPlotPlacementValid = bValidCell;
					PendingPlotId = ResolvedPlotId; // 현재 호버 부지로 갱신(부지밖이면 None)
				}
				else
				{
					// 빌딩은 200 유닛 그리드
					SteppedPosition = CoordinateUtils::SteppedPosition(IntersectionPos);
				}
			}
			// Floating 중인 건물은 Z값 유지
			if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(TargetActor))
			{
				if (Building->IsFloating())
				{
					SteppedPosition.Z = TargetActor->GetActorLocation().Z;
				}
			}
			break;
		}

		FVector OldActorLocation = TargetActor->GetActorLocation();
		// XY만 비교 (Floating 중이면 Z는 다를 수 있음)
		if (FMath::Abs(SteppedPosition.X - OldActorLocation.X) > KINDA_SMALL_NUMBER ||
			FMath::Abs(SteppedPosition.Y - OldActorLocation.Y) > KINDA_SMALL_NUMBER)
		{
			TargetActor->SetActorLocation(SteppedPosition);
			UpdateBuildAsset();
		}
		else if (CurrentPlacementMode == EPlacementMode::Building && bPlotBuildSession)
		{
			// 부지 세션: 위치 변화가 없어도 셀 유효성(겹침/부지밖/호버 부지 전환)은 갱신해야 CanDrop 이 최신
			UpdateBuildAsset();
		}
	}
}

void UPlacementHandler::TrackMovePlacement(const FVector& WorldPosition)
{
	AActor* TargetActor = GetPlacementTargetActor();
	if (!TargetActor) return;

	FVector SteppedPosition;
	switch (CurrentPlacementMode)
	{
	case EPlacementMode::Workstation:
	case EPlacementMode::Decoration:
		SteppedPosition = ComputeOfficeFloorPosition(WorldPosition);
		break;
	case EPlacementMode::Building:
	default:
		// 부지 세션이면 프리뷰 밑 소유 부지(동적 판정)에 자유 배치, 아니면 기존 200 유닛 그리드
		{
			FVector PlotSnapped;
			bool bValidCell = false;
			FName ResolvedPlotId = NAME_None;
			if (ComputePlotPlacement(WorldPosition, PlotSnapped, bValidCell, ResolvedPlotId))
			{
				SteppedPosition = PlotSnapped;
				bPlotPlacementValid = bValidCell;
				PendingPlotId = ResolvedPlotId; // 현재 호버 부지로 갱신(부지밖이면 None)
			}
			else
			{
				// 빌딩은 200 유닛 그리드
				SteppedPosition = CoordinateUtils::SteppedPosition(WorldPosition);
			}
		}
		// Floating 중인 건물은 Z값 유지
		if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(TargetActor))
		{
			if (Building->IsFloating())
			{
				SteppedPosition.Z = TargetActor->GetActorLocation().Z;
			}
		}
		break;
	}

	FVector OldActorLocation = TargetActor->GetActorLocation();
	// XY만 비교 (Floating 중이면 Z는 다를 수 있음)
	if (FMath::Abs(SteppedPosition.X - OldActorLocation.X) > KINDA_SMALL_NUMBER ||
		FMath::Abs(SteppedPosition.Y - OldActorLocation.Y) > KINDA_SMALL_NUMBER)
	{
		TargetActor->SetActorLocation(SteppedPosition);
		UpdateBuildAsset();
	}
	else if (CurrentPlacementMode == EPlacementMode::Building && bPlotBuildSession)
	{
		// 부지 세션: 위치 변화가 없어도 셀 유효성(겹침/부지밖/호버 부지 전환)은 갱신해야 CanDrop 이 최신
		UpdateBuildAsset();
	}
}

void UPlacementHandler::BeginPlacementZoom()
{
	// 건물 배치 시작 시 기존 "배치용 건물 포커스+줌" 재사용 (오피스 업무공간/장식 모드는 제외)
	if (CurrentPlacementMode != EPlacementMode::Building || !Owner) return;
	if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(GetPlacementTargetActor()))
	{
		Owner->FocusOnBuildingForPlacement(Building, 50.f, 500.f, PlacementZoomRatioScale);
	}
}

void UPlacementHandler::TrackMovePlacement(const FVector2D& ScreenPosition)
{
	// 벽 장식품 배치 전용 - 스크린 위치를 받아서 벽 Hit 위치로 변환
	if (CurrentPlacementMode != EPlacementMode::WallDecoration)
	{
		return;
	}

	AActor* TargetActor = GetPlacementTargetActor();
	if (!TargetActor) return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	FVector WorldOrigin, WorldDirection;
	PC->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldOrigin, WorldDirection);

	// 레이캐스트로 벽에 맞은 위치 찾기
	FHitResult HitResult;
	FVector TraceEnd = WorldOrigin + (WorldDirection * 10000.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(TargetActor);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		WorldOrigin,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	UE_LOG(LogTemp, Warning, TEXT("[TrackMovePlacement(Screen)] WallDecoration Raycast: bHit=%d, ScreenPos=(%f, %f)"),
		bHit, ScreenPosition.X, ScreenPosition.Y);

	if (bHit)
	{
		FVector HitLocation = HitResult.Location;

		// Hit된 컴포넌트 이름으로 벽 전환 감지
		UPrimitiveComponent* HitComponent = HitResult.GetComponent();
		if (HitComponent)
		{
			FString CompName = HitComponent->GetName();
			if (CompName.Contains(TEXT("WallLeft")) && CurrentWallSide != EWallSide::Left)
			{
				SwitchToWall(EWallSide::Left, HitLocation);
				return;
			}
			else if (CompName.Contains(TEXT("WallRight")) && CurrentWallSide != EWallSide::Right)
			{
				SwitchToWall(EWallSide::Right, HitLocation);
				return;
			}
		}

		// 벽 범위 가져오기
		FVector MinBounds = FVector::ZeroVector;
		FVector MaxBounds = FVector(0.f, 0.f, 300.f);

		AOfficeInterior* OfficeInterior = Cast<AOfficeInterior>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass()));
		if (OfficeInterior)
		{
			OfficeInterior->GetWallPlacementBounds(CurrentWallSide, MinBounds, MaxBounds);
		}

		// 그리드 스냅 및 고정 축 설정
		FVector SteppedPosition = HitLocation;

		if (CurrentWallSide == EWallSide::Left)
		{
			SteppedPosition.X = FMath::RoundToFloat(HitLocation.X / 5.f) * 5.f;
			SteppedPosition.Z = FMath::RoundToFloat(HitLocation.Z / 5.f) * 5.f;
			SteppedPosition.Y = -11.f;
			SteppedPosition.Z = FMath::Clamp(SteppedPosition.Z, MinBounds.Z, MaxBounds.Z);
		}
		else
		{
			SteppedPosition.Y = FMath::RoundToFloat(HitLocation.Y / 5.f) * 5.f;
			SteppedPosition.Z = FMath::RoundToFloat(HitLocation.Z / 5.f) * 5.f;
			SteppedPosition.X = 4.f;
			SteppedPosition.Z = FMath::Clamp(SteppedPosition.Z, MinBounds.Z, MaxBounds.Z);
		}

		FVector OldActorLocation = TargetActor->GetActorLocation();
		if (!FVector::PointsAreSame(SteppedPosition, OldActorLocation))
		{
			TargetActor->SetActorLocation(SteppedPosition);
			UpdateBuildAsset();
		}
	}
}

void UPlacementHandler::SetOriginalPosition(const FVector& Position)
{
	OriginalBuildingPosition = Position;
}

void UPlacementHandler::SetExistingBuildingAsTarget(AInteractableBaseActor* ExistingBuilding)
{
	OriginalBuildingPlotId = NAME_None;
	// 기존 PlacementTarget이 있고 다른 객체라면 정리
	if (PlacementTargetEntity && PlacementTargetEntity != ExistingBuilding)
	{
		PlacementTargetEntity->Destroy();
	}

	// 기존 건물을 PlacementTarget으로 설정
	PlacementTargetEntity = ExistingBuilding;
	bHasPlacementTarget = true;
	CurrentPlacementMode = EPlacementMode::Building;

	// 재배치도 신규 건설과 동일한 부지 세션으로 — 드래그(TickComponent→TrackMovePlacement)가
	// bPlotBuildSession 으로 ComputePlotPlacement 을 경유해 자유 배치/부지밖 금지/겹침방지(bPlotPlacementValid)를 자동 적용한다.
	// 시드 = 건물의 현재 부지(초기 프리뷰 시작 위치), footprint = DT 칸수. 실제 목적지는 매 프레임 호버 부지로 동적 판정.
	// (PlacementTargetEntity 가 위에서 이동 대상으로 설정되어 ComputePlotPlacement 의 자기 제외 가드가 동작.)
	if (ABuildingBaseActor* MovingBuilding = Cast<ABuildingBaseActor>(ExistingBuilding))
	{
		OriginalBuildingPlotId = MovingBuilding->GetOwningPlotId();
		int32 FootW = 1;
		int32 FootD = 1;
		if (UWorld* W = GetWorld())
		{
			if (UGameInstance* GI = W->GetGameInstance())
			{
				if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
				{
					bool bBuildingDataOk = false;
					const FBuildingData BData = TableMgr->GetBuildingData(MovingBuilding->GetInteractableRowName(), bBuildingDataOk);
					if (bBuildingDataOk)
					{
						FootW = FMath::Max(1, BData.FootprintWidthCells);
						FootD = FMath::Max(1, BData.FootprintDepthCells);
					}
				}
			}
		}
		SetPendingPlotId(MovingBuilding->GetOwningPlotId(), FootW, FootD);
	}

	// 태그 변경
	ExistingBuilding->Tags.Remove("Building");
	ExistingBuilding->Tags.Add("PlacementMode");
	// NavBlocker를 원래 위치에 고정시키기용
	ExistingBuilding->DetachNavBlockerForMove();

	PostPlacementTargetRecalcBoxExtent();
}

bool UPlacementHandler::ConfirmBuildingMove()
{
	if (IsValid(PlacementTargetEntity) == false || !CanDrop)
	{
		return false;
	}

	// 부지 세션 확정 게이트 — 부지 밖/겹침/바닥밖엔 못 놓음(신규 배치의 !CanDrop 가드와 동등).
	// bPlotBuildSession 이면 CanDrop 이 이미 bPlotPlacementValid 를 AND 하지만, 명시적으로 다시 막아 잘못된 위치 확정만 차단한다.
	// 거부 시 반환 false → 호출자(BuildPlacementPanelWidget)는 이동 모드 유지(원위치 복귀는 [취소]가 담당).
	if (bPlotBuildSession && !bPlotPlacementValid)
	{
		return false;
	}

	const FName SourcePlotId = OriginalBuildingPlotId;
	const FName DestinationPlotId = PendingPlotId;

	// 건물 착지 (내려오는 애니메이션 + VFX)
	if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(PlacementTargetEntity))
	{
		if (Building->IsFloating())
		{
			// 바닥 위치로 설정 후 착지 애니메이션
			FVector CurrentLoc = Building->GetActorLocation();
			CurrentLoc.Z = 0.0f;
			Building->SetActorLocation(CurrentLoc);
			Building->StartFloating(500.0f, true);
			Building->LandBuilding();  // 내려오면서 VFX 재생
		}
	}

	// 건물 ↔ 부지 링크 갱신 — 목적지 호버 부지(PendingPlotId)로 OwningPlotId 교체. CompleteBuildingPlacement 의
	// SaveGameData 전에 세팅해 같은 세이브에 직렬화. CanDrop=true(=bPlotPlacementValid) 보장 = 부지 위 유효 위치이므로
	// PendingPlotId 는 유효 부지(부지 간 이동 시 목적지 B). 옛 부지 A 의 점유는 OwningPlotId 변경만으로 자동 해제
	// (런타임 GetBuildingsOnPlot 이 OwningPlotId 로 산출 — 별도 점유맵 수정 불필요).
	if (bPlotBuildSession)
	{
		if (ABuildingBaseActor* MovingBuilding = Cast<ABuildingBaseActor>(PlacementTargetEntity))
		{
			MovingBuilding->SetOwningPlotId(DestinationPlotId);
		}
	}

	UpdateBuildAsset();
	PlacementTargetEntity->AttachNavBlockerToNewLocation();
	CompleteBuildingPlacement(PlacementTargetEntity);
	EndPlacementFeedback();
	RefreshVacantPlotDressingPlots(SourcePlotId, DestinationPlotId);

	// UI 정리
	PlacementSMC->SetVisibility(false);
	PlacementSMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	HidePlotFootprintPreview();

	ReleasePlacementIMC();

	// 부지 컨텍스트 리셋 — PlacingEntity 와 동일. 잔류 상태가 다음 진입(신규 건설/재배치)을 오염시키지 않도록.
	bPlotBuildSession = false;
	InitialSeedPlotId = NAME_None;
	PendingPlotId = NAME_None;
	PendingFootprintWidthCells = 1;
	PendingFootprintDepthCells = 1;
	bPlotPlacementValid = true;
	OriginalBuildingPlotId = NAME_None;

	bHasPlacementTarget = false;
	PlacementTargetEntity = nullptr;

	return true;
}

void UPlacementHandler::CancelBuildingMove()
{
	const FName SourcePlotId = OriginalBuildingPlotId;
	const FName CancelledDestinationPlotId = PendingPlotId;
	EndPlacementFeedback();

	if (PlacementTargetEntity)
	{
		// Floating 중인 건물을 내려오게 함 (취소이므로 VFX 없음)
		if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(PlacementTargetEntity))
		{
			Building->SetOwningPlotId(SourcePlotId);
			if (Building->IsFloating())
			{
				// 원래 위치(바닥)로 설정 후 내려오는 애니메이션
				FVector OriginalLoc = OriginalBuildingPosition;
				OriginalLoc.Z = 0.0f;
				Building->SetActorLocation(OriginalLoc);
				Building->StartFloating(500.0f, true);
				Building->StopFloating();  // VFX 없이 내려오기만
			}
			else
			{
				// Floating 아니면 바로 원래 위치로
				PlacementTargetEntity->SetActorLocation(OriginalBuildingPosition);
			}
		}
		else
		{
			// 건물 아니면 바로 원래 위치로
			PlacementTargetEntity->SetActorLocation(OriginalBuildingPosition);
		}

		// 태그 복원
		PlacementTargetEntity->Tags.Remove("PlacementMode");
		PlacementTargetEntity->Tags.Add("Building");

		// NavBlocker 원위치 재부착 — SetExistingBuildingAsTarget 의 DetachNavBlockerForMove 와 대칭.
		// (확정은 AttachNavBlockerToNewLocation 으로 복원하나 취소엔 없어 원위치 NavMesh 구멍이 남던 버그.)
		PlacementTargetEntity->AttachNavBlockerToNewLocation();

		PlacementSMC->SetVisibility(false);
		PlacementSMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
	RefreshVacantPlotDressingPlots(SourcePlotId, CancelledDestinationPlotId);

	HidePlotFootprintPreview();

	ReleasePlacementIMC();

	// 부지 세션 리셋 — 취소 시 OwningPlotId 는 그대로(원위치 복귀), 세션 상태만 정리해 다음 진입 오염 방지.
	bPlotBuildSession = false;
	InitialSeedPlotId = NAME_None;
	PendingPlotId = NAME_None;
	PendingFootprintWidthCells = 1;
	PendingFootprintDepthCells = 1;
	bPlotPlacementValid = true;
	OriginalBuildingPlotId = NAME_None;

	bHasPlacementTarget = false;
	PlacementTargetEntity = nullptr;
}

void UPlacementHandler::CompleteBuildingPlacement(AInteractableBaseActor* Building)
{
	if (Building)
	{
		// 태그 설정 (PlacementMode 제거, Build 추가)
		Building->Tags.Remove("PlacementMode");
		Building->Tags.Add("Build");

		// 건물 즉시 완성 처리
		if (ABuildingBaseActor* BuildingActor = Cast<ABuildingBaseActor>(Building))
		{
			// BuildSuccess 호출로 Building 태그 추가 + UnlockNextBuilding 처리
			BuildingActor->BuildSuccess();
			BuildingActor->SaveGameData();

			// 건설 완료 시 카메라가 새 건물을 화면 중앙에 살짝 줌인하며 포커싱
			if (APlayerCamera* PlayerCamera = Cast<APlayerCamera>(GetOwner()))
			{
				PlayerCamera->StopCameraTransition();
				PlayerCamera->FocusOnBuilding(BuildingActor, 50.f, 50.f, 0.5f);
				// 확정 직후 입력 모드가 Normal 로 돌아가 안전망이 어차피 끄므로, 0.2초 고스트 깜빡임만 남는다
				PlayerCamera->ClearFocusTarget();
			}
		}
	}
}

// ========== 업무공간 배치 ==========

void UPlacementHandler::SetWorkstationPlacementTarget(TSubclassOf<AWorkstationActorBase> WorkstationClass, FName WorkstationTypeID)
{
	if (!WorkstationClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlacementHandler] WorkstationClass is null!"));
		return;
	}

	CurrentPlacementMode = EPlacementMode::Workstation;
	bHasPlacementTarget = true;
	OverlapCheckClass = AWorkstationActorBase::StaticClass();
	PlacementWorkstationTypeID = WorkstationTypeID;

	// 벽 스냅 대상 — 배치 세션 동안 고정이라 여기서 1회만 찾는다(매 프레임 액터 검색 회피).
	CachedOfficeInterior = Cast<AOfficeInterior>(UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass()));

	// 업무공간 스폰
	FVector SpawnLocation;
	CoordinateUtils::ProjectViewportCenterToGroundPlane(SpawnLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PlacementWorkstation = GetWorld()->SpawnActor<AWorkstationActorBase>(WorkstationClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (PlacementWorkstation)
	{
		PlacementWorkstation->Tags.Add(FName("PlacementMode"));

		// 배치 모드 전용 콜리전 채널 설정 (ECC_GameTraceChannel2)
		if (PlacementWorkstation->DeskMesh)
		{
			PlacementWorkstation->DeskMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
		}

		// BoxComponent 크기 계산
		PlacementWorkstation->RecalcBoxExtent();

		// 배치 표시 메시 설정
		FVector BoundOrigin, BoundExtent;
		PlacementWorkstation->GetActorBounds(false, BoundOrigin, BoundExtent);

		PlacementSMC->AttachToComponent(PlacementWorkstation->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PlacementSMC->SetRelativeScale3D((BoundExtent / 50.0f) * 1.05f);
		PlacementSMC->SetRelativeLocation(FVector(0.f, 0.f, BoundExtent.Z));
		PlacementSMC->SetVisibility(true);

		// 스폰 직후 배치 가능 여부 업데이트
		UpdateBuildAsset();

		UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Workstation spawned for placement: %s"), *WorkstationClass->GetName());
	}
}

void UPlacementHandler::ReleaseWorkstationPlacementTarget()
{
	PlacementSMC->SetVisibility(false);
	PlacementSMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

	if (IsValid(PlacementWorkstation))
	{
		PlacementWorkstation->Destroy();
		PlacementWorkstation = nullptr;
	}

	CachedOfficeInterior = nullptr;
	CurrentPlacementMode = EPlacementMode::None;
	bHasPlacementTarget = false;
}

bool UPlacementHandler::PlacingWorkstation(bool* bOutCapacityReached)
{
	if (bOutCapacityReached)
	{
		*bOutCapacityReached = false;
	}

	if (!IsValid(PlacementWorkstation) || !CanDrop)
	{
		return false;
	}

	UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>();
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	UEmployeeManager* EmployeeMgr = GameInstance ? GameInstance->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!OfficeMgr || !GameInstance || !EmployeeMgr)
	{
		return false;
	}

	int32 CurrentSeats = 0;
	for (const AWorkstationActorBase* Workstation : OfficeMgr->GetPlacedWorkstations())
	{
		if (IsValid(Workstation))
		{
			CurrentSeats += Workstation->GetChairCount();
		}
	}

	const int32 CandidateSeats = PlacementWorkstation->GetChairCount();
	const int32 EmployeeCapacity = EmployeeMgr->GetBuildingEmployeeCapacity(
		GameInstance->GetCurrentManagedBuildingIndex());
	const FWorkstationCapacityDecision CapacityDecision = FWorkstationCapacityRules::Evaluate(
		CurrentSeats, CandidateSeats, EmployeeCapacity);
	if (!CapacityDecision.bCanPlace)
	{
		return false;
	}

	// 현재 위치에 새 업무공간 스폰 (PlacementWorkstation과 동일 클래스)
	FTransform PlacementTransform = PlacementWorkstation->GetActorTransform();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWorkstationActorBase* NewWorkstation = GetWorld()->SpawnActor<AWorkstationActorBase>(
		PlacementWorkstation->GetClass(), PlacementTransform, SpawnParams);

	if (NewWorkstation)
	{
		NewWorkstation->Tags.Remove(FName("PlacementMode"));
		NewWorkstation->Tags.Add(FName("Workstation"));

		// WorkstationTypeID 설정 (저장/로드용)
		NewWorkstation->WorkstationTypeID = PlacementWorkstationTypeID;

		NewWorkstation->InitializeWorkstation();

		// 먼저 런타임 목록에 등록하고, 미션 진행 갱신 뒤 아래에서 함께 저장한다.
		OfficeMgr->RegisterPlacedWorkstation(NewWorkstation);

		// 채용 시 자동 착석의 반대 방향 — 먼저 뽑아둔 직원이 새 자리에 앉는다
		OfficeMgr->AutoSeatBenchedEmployees(NewWorkstation);

		if (bOutCapacityReached)
		{
			*bOutCapacityReached = CapacityDecision.bReachesCapacity;
		}

		// M4 미션 가이드 — 배치한 책상의 실제 좌석 수만큼 진행 (튜토리얼 외엔 no-op)
		bool bWasPlaceDesksMission = false;
		bool bIsPlaceDesksMission = false;
		if (UMissionManagerSubsystem* MissionMgr = GameInstance->GetSubsystem<UMissionManagerSubsystem>())
		{
			bWasPlaceDesksMission = MissionMgr->GetActiveConditionType() == EMissionConditionType::PlaceDesks;
			MissionMgr->NotifyOfficePlacementCompleted(EOfficePlacementKind::Desk, CandidateSeats);
			bIsPlaceDesksMission = MissionMgr->GetActiveConditionType() == EMissionConditionType::PlaceDesks;
		}

		// 등록된 책상과 갱신된 좌석 진행도를 한 번의 스냅샷으로 저장한다.
		// M4 목표 도달은 CompleteActiveMission이 이미 다음 미션과 함께 저장했으므로 중복 저장하지 않는다.
		if (FWorkstationMissionProgressRules::ShouldSaveAfterPlacement(
			bWasPlaceDesksMission, bIsPlaceDesksMission))
		{
			if (USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>())
			{
				SaveMgr->SaveGameData();
			}
		}

		// 배치 완료 후 미리보기 액터를 옆으로 이동 (겹침 방지, 연속 배치 편의)
		if (IsValid(PlacementWorkstation))
		{
			FVector BoundOrigin, BoundExtent;
			PlacementWorkstation->GetActorBounds(false, BoundOrigin, BoundExtent);

			// 회전 방향에 따라 다음 미리보기 위치 계산
			FVector CurrentLocation = PlacementWorkstation->GetActorLocation();
			FVector MoveDirection = -PlacementWorkstation->GetActorRightVector();
			float MoveDistance = BoundExtent.Y * 2.f + PlacementPadding;
			CurrentLocation += MoveDirection * MoveDistance;
			PlacementWorkstation->SetActorLocation(CurrentLocation);

			// Overlap 정보 갱신 후 SMC 색상 업데이트
			if (PlacementWorkstation->BoxComponent)
			{
				PlacementWorkstation->BoxComponent->UpdateOverlaps();
			}
			UpdateBuildAsset();
		}

		UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Workstation placed at %s (TypeID: %s)"),
			*PlacementTransform.GetLocation().ToString(), *PlacementWorkstationTypeID.ToString());
	}

	return NewWorkstation != nullptr;
}

void UPlacementHandler::UpdateEdgePanning()
{
	if (!Owner) return;

	// 현재 터치/마우스 위치 가져오기
	FVector2D ScreenPos;
	FVector WorldPos;
	if (!CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, WorldPos))
	{
		return;
	}

	// 뷰포트 크기
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	else
	{
		return;
	}

	// 가장자리 영역 계산 (20%)
	float EdgeX = ViewportSize.X * EdgePanThreshold;
	float EdgeY = ViewportSize.Y * EdgePanThreshold;

	FVector2D PanDirection = FVector2D::ZeroVector;
	float PanStrength = 0.f;

	// 왼쪽 가장자리
	if (ScreenPos.X < EdgeX)
	{
		float Ratio = 1.f - (ScreenPos.X / EdgeX);
		PanDirection.X = -Ratio;
		PanStrength = FMath::Max(PanStrength, Ratio);
	}
	// 오른쪽 가장자리
	else if (ScreenPos.X > ViewportSize.X - EdgeX)
	{
		float Ratio = (ScreenPos.X - (ViewportSize.X - EdgeX)) / EdgeX;
		PanDirection.X = Ratio;
		PanStrength = FMath::Max(PanStrength, Ratio);
	}

	// 위쪽 가장자리
	if (ScreenPos.Y < EdgeY)
	{
		float Ratio = 1.f - (ScreenPos.Y / EdgeY);
		PanDirection.Y = -Ratio;
		PanStrength = FMath::Max(PanStrength, Ratio);
	}
	// 아래쪽 가장자리
	else if (ScreenPos.Y > ViewportSize.Y - EdgeY)
	{
		float Ratio = (ScreenPos.Y - (ViewportSize.Y - EdgeY)) / EdgeY;
		PanDirection.Y = Ratio;
		PanStrength = FMath::Max(PanStrength, Ratio);
	}

	// 패닝 적용
	if (PanStrength > 0.f)
	{
		FVector MoveDirection;

		if (CurrentPlacementMode == EPlacementMode::WallDecoration)
		{
			// 벽 배치 모드: 벽 방향 기준으로 이동 (카메라 보간 중에도 정확한 방향)
			// Left 벽 (Yaw=90): 화면 좌우 = 월드 X축, 화면 상하 = 월드 Z축
			// Right 벽 (Yaw=180): 화면 좌우 = 월드 Y축, 화면 상하 = 월드 Z축
			if (CurrentWallSide == EWallSide::Left)
			{
				// Left 벽: 화면 오른쪽 = X 감소, 화면 왼쪽 = X 증가
				MoveDirection = FVector(-PanDirection.X, 0.f, -PanDirection.Y);
			}
			else
			{
				// Right 벽: 화면 오른쪽 = Y 감소, 화면 왼쪽 = Y 증가
				MoveDirection = FVector(0.f, -PanDirection.X, -PanDirection.Y);
			}
		}
		else
		{
			// 바닥 배치 모드: 카메라 방향 기준으로 이동
			FRotator CameraRotation = Owner->GetActorRotation();
			FVector Forward = FRotationMatrix(FRotator(0.f, CameraRotation.Yaw, 0.f)).GetUnitAxis(EAxis::X);
			FVector Right = FRotationMatrix(FRotator(0.f, CameraRotation.Yaw, 0.f)).GetUnitAxis(EAxis::Y);

			MoveDirection = Forward * (-PanDirection.Y) + Right * PanDirection.X;
			MoveDirection.Z = 0.f;
		}
		MoveDirection.Normalize();

		float ActualSpeed = (CurrentPlacementMode == EPlacementMode::Building) ? EdgePanSpeedMain : EdgePanSpeedOffice;

		float DeltaTime = GetWorld()->GetDeltaSeconds();
		FVector MoveDelta = MoveDirection * ActualSpeed * PanStrength * DeltaTime;

		Owner->AddActorWorldOffset(MoveDelta);
	}
}

// ========== 장식품 배치 ==========

void UPlacementHandler::SetDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo)
{
	CurrentPlacementMode = EPlacementMode::Decoration;
	bHasPlacementTarget = true;
	PlacementDecorationInfo = DecorationInfo;
	OverlapCheckClass = ADecorationActor::StaticClass();

	// 벽 스냅 대상 — 배치 세션 동안 고정이라 여기서 1회만 찾는다(매 프레임 액터 검색 회피).
	CachedOfficeInterior = Cast<AOfficeInterior>(UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass()));

	// 스폰 위치 계산
	FVector SpawnLocation;
	CoordinateUtils::ProjectViewportCenterToGroundPlane(SpawnLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PlacementDecoration = GetWorld()->SpawnActor<ADecorationActor>(ADecorationActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (PlacementDecoration)
	{
		PlacementDecoration->Tags.Add(FName("PlacementMode"));
		PlacementDecoration->DecorationCardTableRowName = DecorationInfo.RowName;
		PlacementDecoration->Category = DecorationInfo.Category;
		PlacementDecoration->AllowedSurface = DecorationInfo.AllowedSurface;

		// 배치 모드 전용 콜리전 채널 설정 (ECC_GameTraceChannel2)
		if (PlacementDecoration->MeshComponent)
		{
			PlacementDecoration->MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
		}

		// TableManager에서 메시 로드
		UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bSuccess = false;
			FDecorationData DecData = TableMgr->GetDecorationData(DecorationInfo.RowName, bSuccess);

			if (bSuccess && !DecData.DecorationMesh.IsNull())
			{
				UStaticMesh* Mesh = DecData.DecorationMesh.LoadSynchronous();
				if (Mesh)
				{
					PlacementDecoration->SetDecorationMesh(Mesh);
					UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Decoration mesh loaded: %s"), *Mesh->GetName());
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[PlacementHandler] DecorationData not found or mesh is null for: %s"),
					*DecorationInfo.RowName.ToString());
			}
		}

		// 배치 표시 메시 설정
		FVector BoundOrigin, BoundExtent;
		PlacementDecoration->GetActorBounds(false, BoundOrigin, BoundExtent);

		PlacementSMC->AttachToComponent(PlacementDecoration->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PlacementSMC->SetRelativeScale3D((BoundExtent / 50.0f) * 1.05f);
		PlacementSMC->SetRelativeLocation(FVector(0.f, 0.f, BoundExtent.Z));
		PlacementSMC->SetVisibility(true);

		UpdateBuildAsset();

		UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Decoration spawned for placement: %s"), *DecorationInfo.RowName.ToString());
	}
}

void UPlacementHandler::ReleaseDecorationPlacementTarget()
{
	PlacementSMC->SetVisibility(false);
	PlacementSMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

	if (IsValid(PlacementDecoration))
	{
		PlacementDecoration->Destroy();
		PlacementDecoration = nullptr;
	}

	PlacementDecorationInfo = FDecorationCardTable();
	CachedOfficeInterior = nullptr;
	CurrentPlacementMode = EPlacementMode::None;
	bHasPlacementTarget = false;
}

bool UPlacementHandler::PlacingDecoration()
{
	if (!IsValid(PlacementDecoration) || !CanDrop)
	{
		return false;
	}

	// 현재 위치에 새 장식품 스폰 (미리보기 액터는 유지 → 연속 배치 가능)
	FTransform PlacementTransform = PlacementDecoration->GetActorTransform();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADecorationActor* NewDecoration = GetWorld()->SpawnActor<ADecorationActor>(
		ADecorationActor::StaticClass(), PlacementTransform, SpawnParams);

	if (NewDecoration)
	{
		NewDecoration->Tags.Add(FName("Decoration"));

		// 미리보기 액터에서 정보 복사 (이미 세팅되어 있음)
		NewDecoration->DecorationCardTableRowName = PlacementDecoration->DecorationCardTableRowName;
		NewDecoration->Category = PlacementDecoration->Category;
		NewDecoration->AllowedSurface = PlacementDecoration->AllowedSurface;

		// 메시 로드
		UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bSuccess = false;
			FDecorationData DecData = TableMgr->GetDecorationData(NewDecoration->DecorationCardTableRowName, bSuccess);
			if (bSuccess && !DecData.DecorationMesh.IsNull())
			{
				UStaticMesh* Mesh = DecData.DecorationMesh.LoadSynchronous();
				if (Mesh)
				{
					NewDecoration->SetDecorationMesh(Mesh);
				}
			}
		}

		// OfficeManager에 등록 (저장은 SpendResource에서 처리)
		if (UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>())
		{
			OfficeMgr->RegisterPlacedDecoration(NewDecoration);
		}

		// 장식은 1회 배치 — 확정 즉시 UI 가 배치 세션을 닫으므로 다음 프리뷰를 준비하지 않는다.
		// (파티션을 이어 까는 건 옆으로 밀기가 아니라 접촉 자석이 담당한다.)

		UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Decoration placed at %s"), *PlacementTransform.GetLocation().ToString());
	}

	return NewDecoration != nullptr;
}

// ========== 벽 장식품 배치 ==========

void UPlacementHandler::SetWallDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo, EWallSide WallSide)
{
	CurrentPlacementMode = EPlacementMode::WallDecoration;
	bHasPlacementTarget = true;
	PlacementDecorationInfo = DecorationInfo;
	CurrentWallSide = WallSide;
	OverlapCheckClass = ADecorationActor::StaticClass();

	// 새 배치 시작 시 회전 오프셋 초기화
	WallDecorationRotationOffset = 0.f;

	// OfficeInterior에서 벽 정보 가져오기
	AOfficeInterior* OfficeInterior = Cast<AOfficeInterior>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass()));

	// 벽에 따른 회전 설정: Left 벽 Yaw 180도, Right 벽 Yaw 270도
	float SpawnYaw = (WallSide == EWallSide::Left) ? 180.f : 270.f;

	// 벽 평면과 스폰 위치 설정
	// Left 벽: Y=-11 고정, X,Z 이동 가능
	// Right 벽: X=2 고정, Y,Z 이동 가능
	FVector SpawnLocation;
	if (WallSide == EWallSide::Left)
	{
		SpawnLocation = FVector(0.f, -11.f, 200.f);  // Y=-11, Z=200 (X는 벽 범위 중앙)
	}
	else
	{
		SpawnLocation = FVector(4.f, 0.f, 200.f);  // X=4, Z=200 (Y는 벽 범위 중앙)
	}

	if (OfficeInterior)
	{
		// OfficeInterior에서 실제 벽 정보 사용
		CurrentWallPlane = OfficeInterior->GetWallPlane(WallSide);

		// 벽 위치와 배치 가능 영역 가져오기
		FVector MinBounds, MaxBounds;
		OfficeInterior->GetWallPlacementBounds(WallSide, MinBounds, MaxBounds);

		// 스폰 X/Y 위치를 벽 범위 중앙으로 조정
		if (WallSide == EWallSide::Left)
		{
			SpawnLocation.X = (MinBounds.X + MaxBounds.X) * 0.5f;  // X 중앙
		}
		else
		{
			SpawnLocation.Y = (MinBounds.Y + MaxBounds.Y) * 0.5f;  // Y 중앙
		}

		UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Wall bounds - Min: %s, Max: %s"),
			*MinBounds.ToString(), *MaxBounds.ToString());
	}
	else
	{
		// OfficeInterior 없을 때 폴백 (기본값)
		UE_LOG(LogTemp, Warning, TEXT("[PlacementHandler] OfficeInterior not found, using fallback wall positions"));

		if (WallSide == EWallSide::Left)
		{
			CurrentWallPlane = FPlane(FVector(0, 1, 0), -11.f);  // Y=-11 평면
		}
		else
		{
			CurrentWallPlane = FPlane(FVector(1, 0, 0), 4.f);  // X=4 평면
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PlacementDecoration = GetWorld()->SpawnActor<ADecorationActor>(
		ADecorationActor::StaticClass(), SpawnLocation, FRotator(0.f, SpawnYaw, 0.f), SpawnParams);

	if (PlacementDecoration)
	{
		PlacementDecoration->Tags.Add(FName("PlacementMode"));
		PlacementDecoration->DecorationCardTableRowName = DecorationInfo.RowName;
		PlacementDecoration->Category = DecorationInfo.Category;
		PlacementDecoration->AllowedSurface = DecorationInfo.AllowedSurface;

		// 배치 모드 전용 콜리전 채널 설정 (ECC_GameTraceChannel2)
		if (PlacementDecoration->MeshComponent)
		{
			PlacementDecoration->MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
		}

		// 메시 로드
		UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bSuccess = false;
			FDecorationData DecData = TableMgr->GetDecorationData(DecorationInfo.RowName, bSuccess);
			if (bSuccess && !DecData.DecorationMesh.IsNull())
			{
				UStaticMesh* Mesh = DecData.DecorationMesh.LoadSynchronous();
				if (Mesh)
				{
					PlacementDecoration->SetDecorationMesh(Mesh);
				}
			}
		}

		// 메시의 로컬 바운드 가져오기 (회전 무관하게 일정한 값)
		FVector LocalBoundExtent = FVector::ZeroVector;
		if (UStaticMeshComponent* MeshComp = PlacementDecoration->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
			{
				FBoxSphereBounds LocalBounds = Mesh->GetBounds();
				LocalBoundExtent = LocalBounds.BoxExtent;
			}
		}

		// 초기 위치 조정: 카메라가 보는 방향 기준으로 오른쪽 끝에서 장식품 크기의 반절만큼 안쪽으로
		FVector CurrentLoc = PlacementDecoration->GetActorLocation();
		if (WallSide == EWallSide::Left)
		{
			CurrentLoc.X = LocalBoundExtent.X;
		}
		else
		{
			CurrentLoc.Y = LocalBoundExtent.X;  // 메시 로컬 X가 벽 평행 방향
		}
		PlacementDecoration->SetActorLocation(CurrentLoc);

		PlacementSMC->AttachToComponent(PlacementDecoration->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PlacementSMC->SetRelativeScale3D((LocalBoundExtent / 50.0f) * 1.05f);

		// SMC 위치: 로컬 Y+ 방향으로 메시 두께(Y)만큼 튀어나감
		PlacementSMC->SetRelativeLocation(FVector(0.f, LocalBoundExtent.Y, 0.f));
		PlacementSMC->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
		PlacementSMC->SetVisibility(true);

		UpdateBuildAsset();

		UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Wall decoration spawned: %s on %s wall"),
			*DecorationInfo.RowName.ToString(),
			WallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"));
	}
}

void UPlacementHandler::ReleaseWallDecorationPlacementTarget()
{
	PlacementSMC->SetVisibility(false);
	PlacementSMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

	if (IsValid(PlacementDecoration))
	{
		PlacementDecoration->Destroy();
		PlacementDecoration = nullptr;
	}

	PlacementDecorationInfo = FDecorationCardTable();
	CurrentPlacementMode = EPlacementMode::None;
	bHasPlacementTarget = false;

	// 배치 종료 시 회전 오프셋 초기화
	WallDecorationRotationOffset = 0.f;
}

bool UPlacementHandler::PlacingWallDecoration()
{
	if (!IsValid(PlacementDecoration) || !CanDrop)
	{
		return false;
	}

	FTransform PlacementTransform = PlacementDecoration->GetActorTransform();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADecorationActor* NewDecoration = GetWorld()->SpawnActor<ADecorationActor>(
		ADecorationActor::StaticClass(), PlacementTransform, SpawnParams);

	if (NewDecoration)
	{
		NewDecoration->Tags.Add(FName("Decoration"));
		NewDecoration->DecorationCardTableRowName = PlacementDecoration->DecorationCardTableRowName;
		NewDecoration->Category = PlacementDecoration->Category;
		NewDecoration->AllowedSurface = PlacementDecoration->AllowedSurface;

		// 메시 로드
		UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bSuccess = false;
			FDecorationData DecData = TableMgr->GetDecorationData(NewDecoration->DecorationCardTableRowName, bSuccess);
			if (bSuccess && !DecData.DecorationMesh.IsNull())
			{
				UStaticMesh* Mesh = DecData.DecorationMesh.LoadSynchronous();
				if (Mesh)
				{
					NewDecoration->SetDecorationMesh(Mesh);
				}
			}
		}

		// OfficeManager에 등록 (저장은 SpendResource에서 처리)
		if (UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>())
		{
			OfficeMgr->RegisterPlacedDecoration(NewDecoration);
		}

		// M8 미션 가이드 — 벽 그림 배치 완료 (튜토리얼 외엔 no-op)
		if (UWorld* W = GetWorld())
			if (UGameInstance* GI = W->GetGameInstance())
				if (UMissionManagerSubsystem* M = GI->GetSubsystem<UMissionManagerSubsystem>())
					M->NotifyOfficePlacementCompleted(EOfficePlacementKind::WallDecoration);

		// 장식은 1회 배치 — 확정 즉시 UI 가 배치 세션을 닫으므로 다음 프리뷰를 준비하지 않는다.

		UE_LOG(LogTemp, Log, TEXT("[PlacementHandler] Wall decoration placed at %s"),
			*PlacementTransform.GetLocation().ToString());
	}

	return NewDecoration != nullptr;
}

void UPlacementHandler::SwitchToWall(EWallSide NewWallSide, const FVector& HitLocation)
{
	if (CurrentPlacementMode != EPlacementMode::WallDecoration)
	{
		return;
	}

	if (CurrentWallSide == NewWallSide)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[PlacementHandler] Switching wall from %s to %s, HitLocation: %s"),
		CurrentWallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"),
		NewWallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"),
		*HitLocation.ToString());

	CurrentWallSide = NewWallSide;

	// OfficeInterior에서 새 벽 정보 가져오기
	AOfficeInterior* OfficeInterior = Cast<AOfficeInterior>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass()));

	FVector NewLocation;

	// HitLocation이 지정되었으면 그 위치 사용, 아니면 벽 중앙
	if (!HitLocation.IsZero())
	{
		NewLocation = HitLocation;
		// 고정 축 설정
		if (NewWallSide == EWallSide::Left)
		{
			NewLocation.Y = -11.f;
		}
		else
		{
			NewLocation.X = 4.f;
		}
	}
	else
	{
		// 기본값: 벽 중앙
		if (NewWallSide == EWallSide::Left)
		{
			NewLocation = FVector(0.f, -11.f, 200.f);
		}
		else
		{
			NewLocation = FVector(4.f, 0.f, 200.f);
		}

		if (OfficeInterior)
		{
			FVector MinBounds, MaxBounds;
			OfficeInterior->GetWallPlacementBounds(NewWallSide, MinBounds, MaxBounds);

			if (NewWallSide == EWallSide::Left)
			{
				NewLocation.X = (MinBounds.X + MaxBounds.X) * 0.5f;
			}
			else
			{
				NewLocation.Y = (MinBounds.Y + MaxBounds.Y) * 0.5f;
			}
		}
	}

	if (OfficeInterior)
	{
		CurrentWallPlane = OfficeInterior->GetWallPlane(NewWallSide);
	}

	// 회전 변경: Yaw는 벽 방향 고정, Pitch는 사용자 회전 오프셋 유지
	float BaseYaw = (NewWallSide == EWallSide::Left) ? 180.f : 270.f;

	if (PlacementDecoration)
	{
		PlacementDecoration->SetActorLocation(NewLocation);
		PlacementDecoration->SetActorRotation(FRotator(WallDecorationRotationOffset, BaseYaw, 0.f));

		// SMC 방향도 업데이트 (로컬 Y+ 방향이 벽 앞쪽)
		// 메시의 로컬 바운드 사용 (회전 무관)
		FVector LocalBoundExtent = FVector::ZeroVector;
		if (UStaticMeshComponent* MeshComp = PlacementDecoration->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
			{
				FBoxSphereBounds LocalBounds = Mesh->GetBounds();
				LocalBoundExtent = LocalBounds.BoxExtent;
			}
		}
		PlacementSMC->SetRelativeLocation(FVector(0.f, LocalBoundExtent.Y, 0.f));
	}

	// 카메라 회전 (OfficeCameraPawn을 통해) - 장식품 위치로 카메라 이동
	AOfficeCameraPawn* CameraPawn = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (CameraPawn)
	{
		CameraPawn->FocusOnWall(NewWallSide, NewLocation);
	}

	UpdateBuildAsset();
}
