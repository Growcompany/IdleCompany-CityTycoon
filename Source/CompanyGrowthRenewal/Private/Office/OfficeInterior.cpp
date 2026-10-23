// Fill out your copyright notice in the Description page of Project Settings.

#include "Office/OfficeInterior.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Office/OfficeDistantBackdrop.h"
#include "Office/OfficeTelevision.h"
#include "Core/CGGameInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Global/GlobalAssetCache.h"
#include "Enum/PrimitiveShapeType.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Entity/Door/InteractiveDoorActor.h"
#include "Engine/StaticMesh.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Table/DecorationData.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

AOfficeInterior::AOfficeInterior()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	// 바닥 컴포넌트 (메시는 블루프린트에서 설정)
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(RootScene);
	// NavMesh가 바닥 위에 생성되도록 설정
	Floor->SetCanEverAffectNavigation(true);

	// 왼쪽 벽 컴포넌트 (메시는 블루프린트에서 설정)
	WallLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallLeft"));
	WallLeft->SetupAttachment(RootScene);

	// 오른쪽 벽 컴포넌트 (메시는 블루프린트에서 설정)
	WallRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallRight"));
	WallRight->SetupAttachment(RootScene);

	RecruitmentDoor = CreateDefaultSubobject<UChildActorComponent>(TEXT("RecruitmentDoor"));
	RecruitmentDoor->SetupAttachment(RootScene);
	RecruitmentDoor->SetRelativeLocation(FVector(DoorWallCornerInset, -5.f, 0.f));

	DoorWallLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorWallLeft"));
	DoorWallRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorWallRight"));
	DoorWallHeader = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorWallHeader"));
	DoorWallSill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorWallSill"));
	for (UStaticMeshComponent* WallPiece : { DoorWallLeft.Get(), DoorWallRight.Get(), DoorWallHeader.Get(), DoorWallSill.Get() })
	{
		WallPiece->SetupAttachment(RootScene);
		WallPiece->SetVisibility(false);
		WallPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// NavMesh 영역 정의용 박스 (런타임에는 충돌 없음)
	NavMeshBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("NavMeshBounds"));
	NavMeshBounds->SetupAttachment(RootScene);
	NavMeshBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavMeshBounds->SetBoxExtent(FVector(500.f, 500.f, 100.f));
	NavMeshBounds->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	NavMeshBounds->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	NavMeshBounds->bVisualizeComponent = true;
#endif
}

bool AOfficeInterior::TryApplyValidatedFootprint(
	const FIntPoint RequestedTileCount,
	const FIntPoint RequestedStarterTileCount,
	const bool bRequireExactRequestedTileCount)
{
	const FIntPoint MaximumTileCount = GetMaxTileCount();
	if (MaximumTileCount.X < FOfficeFootprintGeometry::MinTileCountX
		|| MaximumTileCount.Y < FOfficeFootprintGeometry::MinTileCountY)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeInterior] Invalid configured maximum tile count: %dx%d (minimum: %dx%d)"),
			MaximumTileCount.X,
			MaximumTileCount.Y,
			FOfficeFootprintGeometry::MinTileCountX,
			FOfficeFootprintGeometry::MinTileCountY);
		return false;
	}

	const FIntPoint NormalizedTileCount = FOfficeFootprintGeometry::NormalizeTileCount(
		RequestedTileCount,
		MaximumTileCount);
	const FIntPoint NormalizedStarterTileCount = FOfficeFootprintGeometry::NormalizeTileCount(
		RequestedStarterTileCount,
		MaximumTileCount);
	const FIntPoint ClampedStarterTileCount(
		FMath::Min(NormalizedStarterTileCount.X, NormalizedTileCount.X),
		FMath::Min(NormalizedStarterTileCount.Y, NormalizedTileCount.Y));
	if (bRequireExactRequestedTileCount && NormalizedTileCount != RequestedTileCount)
	{
		return false;
	}

	TileCountX = NormalizedTileCount.X;
	TileCountY = NormalizedTileCount.Y;
	StarterTileCountX = ClampedStarterTileCount.X;
	StarterTileCountY = ClampedStarterTileCount.Y;
	return true;
}

void AOfficeInterior::BeginPlay()
{
	ApplyPresentationScale();
	Super::BeginPlay();

	if (!TryApplyValidatedFootprint(
		GetTileCount(),
		FIntPoint(StarterTileCountX, StarterTileCountY),
		false))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] BeginPlay aborted because the footprint configuration is invalid."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] ========== BeginPlay START =========="));
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Initial TileCount: X=%d, Y=%d"), TileCountX, TileCountY);

	InitializeComponents();

	// 초기 스케일 적용 (TileCountX, TileCountY 기준)
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Calling UpdateScalesForExpansion with TileX=%d, TileY=%d"), TileCountX, TileCountY);
	UpdateScalesForExpansion();

	// FloorTileMesh가 없으면 테이블에서 기본 타일(첫 번째) 로드
	if (!FloorTileMesh)
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			// 기본 타일 RowName
			FName DefaultTileRowName = FName("FT_1");
			bool bSuccess = false;
			FDecorationData DecData = TableMgr->GetDecorationData(DefaultTileRowName, bSuccess);
			if (bSuccess && !DecData.DecorationMesh.IsNull())
			{
				FloorTileMesh = DecData.DecorationMesh.LoadSynchronous();
				CurrentFloorTileRowName = DefaultTileRowName;
				UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Loaded default floor tile: %s"), *DefaultTileRowName.ToString());
			}
		}
	}

	// 메시가 있고 타일이 없으면 초기 타일 생성
	if (FloorTileMesh && FloorTiles.Num() == 0)
	{
		GenerateFloorTiles();
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] FloorTiles count: %d, FloorTileMesh: %s"),
		FloorTiles.Num(), FloorTileMesh ? *FloorTileMesh->GetName() : TEXT("null"));

	UpdateNavMeshBounds();

	OnFootprintChanged.AddUObject(this, &AOfficeInterior::HandleCityBackdropFootprintChanged);
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] ========== BeginPlay END =========="));
	UpdateExpansionPreview();
	Television = GetWorld()->SpawnActor<AOfficeTelevision>();
	if (Television) { Television->Initialize(this); }
	OnFootprintChanged.Broadcast(GetTileCount());
}

void AOfficeInterior::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Television)) { Television->Destroy(); }
	Television = nullptr;
	OnFootprintChanged.RemoveAll(this);
	if (CityBackdrop) { CityBackdrop->Reset(); }
	CityBackdrop = nullptr;
	CityBackdropMaterial = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AOfficeInterior::HandleCityBackdropFootprintChanged(const FIntPoint TileCount)
{
	if (CityBackdrop && CityBackdrop->IsActive()) { SynchronizeCityBackdrop(); }
}

bool AOfficeInterior::SynchronizeCityBackdrop()
{
	const UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		if (CityBackdrop) { CityBackdrop->Reset(); }
		return false;
	}
	const FOfficeNeighborhoodSnapshot& Snapshot = GameInstance->GetOfficeNeighborhoodSnapshot();
	if (!Snapshot.bValid || Snapshot.ManagedBuildingIndex != GameInstance->GetCurrentManagedBuildingIndex()
		|| !IsValid(Snapshot.SourceBackdrop) || !Snapshot.SourceBackdrop->HasCapture())
	{
		if (CityBackdrop) { CityBackdrop->Reset(); }
		if (Snapshot.bValid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] City photo is unavailable for building %d."),
				GameInstance->GetCurrentManagedBuildingIndex());
		}
		return false;
	}
	if (!CityBackdropMaterial)
	{
		const TSoftObjectPtr<UMaterialInterface> MaterialAsset(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Office/Exterior/Materials/M_OfficeDistantBackdrop.M_OfficeDistantBackdrop")));
		CityBackdropMaterial = MaterialAsset.LoadSynchronous();
	}
	if (!CityBackdropMaterial)
	{
		if (CityBackdrop) { CityBackdrop->Reset(); }
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] City photo display material is missing."));
		return false;
	}
	if (!CityBackdrop) { CityBackdrop = NewObject<UOfficeDistantBackdrop>(this); }
	const FVector2D FloorCenter = GetCurrentFloorBoundsLocal().GetCenter();
	// 도시 거리의 원본 cm 배율은 유지하고 표시 위치만 실내 바닥 중심에 대응시킨다.
	const FTransform TargetFrame(GetActorQuat(), GetActorTransform().TransformPosition(
		FVector(FloorCenter, GetStructuralFloorZLocal())));
	const FTransform DestinationFrame = Snapshot.SourceBackdrop->GetCaptureFrame()
		.GetRelativeTransform(Snapshot.SourceAnchorWorldTransform) * TargetFrame;
	const UOfficeDistantBackdrop* Capture = Snapshot.SourceBackdrop;
	if (CityBackdrop->HasCapture()
		&& CityBackdrop->GetRenderTargets() == Capture->GetRenderTargets())
	{
		Capture = CityBackdrop;
	}
	const bool bApplied = CityBackdrop->ApplyCapture(this, CityBackdropMaterial, Capture, DestinationFrame, 0.35f);
	if (!bApplied)
	{
		CityBackdrop->Reset();
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] City photo display failed for building %d."), Snapshot.ManagedBuildingIndex);
	}
	return bApplied;
}

void AOfficeInterior::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	InitializeComponents();
	UpdateNavMeshBounds();
	UpdateRecruitmentEntrance();
}

void AOfficeInterior::ApplyPresentationScale()
{
	SetActorScale3D(FVector(PresentationScale));
	UpdateNavMeshBounds();
}

FTransform AOfficeInterior::OfficeLocalToWorldTransform(const FTransform& LocalTransform) const
{
	return LocalTransform * GetActorTransform();
}

FTransform AOfficeInterior::WorldToOfficeLocalTransform(const FTransform& WorldTransform) const
{
	return WorldTransform.GetRelativeTransform(GetActorTransform());
}

void AOfficeInterior::InitializeComponents()
{
	// 모바일은 처음 일치하는 directional 채널만 사용하므로 실내는 채널 1만 선택한다.
	for (UStaticMeshComponent* InteriorMesh : { Floor, WallLeft, WallRight })
	{
		if (InteriorMesh) InteriorMesh->SetLightingChannels(false, true, false);
	}
	// 벽에 Visibility 채널 콜리전 활성화 (레이캐스트용)
	if (WallLeft)
	{
		WallLeft->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		WallLeft->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	if (WallRight)
	{
		WallRight->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		WallRight->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}


void AOfficeInterior::UpdateNavMeshBounds()
{
	if (!NavMeshBounds)
	{
		return;
	}

	// BoxExtent는 로컬 크기다. 루트 배율과 회전은 상속으로 한 번만 적용한다.
	const FBox2D LocalFloor = GetCurrentFloorBoundsLocal();
	const FVector2D LocalCenter = LocalFloor.GetCenter();
	const FVector2D LocalExtent = LocalFloor.GetExtent();
	NavMeshBounds->SetRelativeScale3D(FVector::OneVector);
	NavMeshBounds->SetRelativeRotation(FRotator::ZeroRotator);
	NavMeshBounds->SetBoxExtent(FVector(LocalExtent.X, LocalExtent.Y, 150.f));
	NavMeshBounds->SetRelativeLocation(FVector(LocalCenter.X, LocalCenter.Y, GetStructuralFloorZLocal() + 150.f));

	// Dynamic NavMesh 업데이트 트리거 (Runtime Generation이 Dynamic일 때 작동)
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys && Floor)
	{
		// Floor 지오메트리 변경을 NavSystem에 알림
		NavSys->UpdateActorInNavOctree(*this);

		// 변경된 영역의 NavMesh 재생성 요청
		const FBox FloorBox = Floor->Bounds.GetBox();
		NavSys->AddDirtyArea(FloorBox, ENavigationDirtyFlag::All);

		UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] NavMesh dirty area added: %s"), *FloorBox.ToString());
	}
}

FBox AOfficeInterior::GetNavMeshBoundsBox() const
{
	if (NavMeshBounds)
	{
		return NavMeshBounds->Bounds.GetBox();
	}

	// 폴백: 타일 기반 기본 영역
	FVector2D CurrentSize = GetCurrentSize();
	FVector Center = GetActorLocation();
	FVector Extent(CurrentSize.X * 0.5f, CurrentSize.Y * 0.5f, 150.f);
	return FBox(Center - Extent, Center + Extent);
}

FVector AOfficeInterior::GetRandomFloorLocation() const
{
	const FBox2D LocalFloor = GetCurrentFloorBoundsLocal();
	return GetActorTransform().TransformPosition(FVector(
		FMath::RandRange(LocalFloor.Min.X, LocalFloor.Max.X),
		FMath::RandRange(LocalFloor.Min.Y, LocalFloor.Max.Y),
		GetStructuralFloorZLocal()));
}

// ========== 타일 생성/관리 ==========

void AOfficeInterior::GenerateFloorTiles()
{
	// 이 함수는 타일 교체 시에만 사용 (LoadFloorTileFromRowName에서 호출)
	// 확장 시에는 AddTilesForExpansion() 사용
	ClearFloorTiles();

	if (!FloorTileMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] FloorTileMesh is null - skipping tile generation"));
		return;
	}

	for (int32 X = 0; X < TileCountX; ++X)
	{
		for (int32 Y = 0; Y < TileCountY; ++Y)
		{
			UStaticMeshComponent* Tile = NewObject<UStaticMeshComponent>(this);
			Tile->SetStaticMesh(FloorTileMesh);
			Tile->SetLightingChannels(false, true, false);
			Tile->SetupAttachment(RootScene);

			Tile->SetRelativeLocation(FOfficeFootprintGeometry::MakeTileCenter(X, Y, TileSize));

			Tile->RegisterComponent();
			FloorTiles.Add(Tile);
		}
	}

	UpdateRecruitmentEntrance();
	UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Generated %d floor tiles (%dx%d)"), FloorTiles.Num(), TileCountX, TileCountY);
}

void AOfficeInterior::AddTilesForExpansion(EOfficeExpandDirection Direction)
{
	if (!FloorTileMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] FloorTileMesh is null - skipping tile addition"));
		return;
	}

	if (Direction == EOfficeExpandDirection::Left)
	{
		// 왼쪽 확장: 새로운 X열 추가 (TileCountX-1 위치에 TileCountY개)
		int32 NewX = TileCountX - 1;
		for (int32 Y = 0; Y < TileCountY; ++Y)
		{
			UStaticMeshComponent* Tile = NewObject<UStaticMeshComponent>(this);
			Tile->SetStaticMesh(FloorTileMesh);
			Tile->SetLightingChannels(false, true, false);
			Tile->SetupAttachment(RootScene);

			Tile->SetRelativeLocation(FOfficeFootprintGeometry::MakeTileCenter(NewX, Y, TileSize));

			Tile->RegisterComponent();
			FloorTiles.Add(Tile);
		}
		UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Added %d tiles for Left expansion (X=%d)"), TileCountY, NewX);
	}
	else
	{
		// 오른쪽 확장: 새로운 Y행 추가 (TileCountY-1 위치에 TileCountX개)
		int32 NewY = TileCountY - 1;
		for (int32 X = 0; X < TileCountX; ++X)
		{
			UStaticMeshComponent* Tile = NewObject<UStaticMeshComponent>(this);
			Tile->SetStaticMesh(FloorTileMesh);
			Tile->SetLightingChannels(false, true, false);
			Tile->SetupAttachment(RootScene);

			Tile->SetRelativeLocation(FOfficeFootprintGeometry::MakeTileCenter(X, NewY, TileSize));

			Tile->RegisterComponent();
			FloorTiles.Add(Tile);
		}
		UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Added %d tiles for Right expansion (Y=%d)"), TileCountX, NewY);
	}
}

void AOfficeInterior::ClearFloorTiles()
{
	for (UStaticMeshComponent* Tile : FloorTiles)
	{
		if (Tile)
		{
			Tile->DestroyComponent();
		}
	}
	FloorTiles.Empty();
}

void AOfficeInterior::SetFloorTileMesh(UStaticMesh* NewMesh)
{
	if (!NewMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] SetFloorTileMesh - NewMesh is null"));
		return;
	}

	FloorTileMesh = NewMesh;

	// 기존 타일들에 새 메시 적용
	for (UStaticMeshComponent* Tile : FloorTiles)
	{
		if (Tile)
		{
			Tile->SetStaticMesh(FloorTileMesh);
			Tile->SetLightingChannels(false, true, false);
		}
	}

	UpdateRecruitmentEntrance();
	UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Floor tile mesh changed to: %s"), *NewMesh->GetName());
}

void AOfficeInterior::LoadFloorTileFromRowName(FName RowName)
{
	if (RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] LoadFloorTileFromRowName - RowName is None"));
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeInterior] TableManagerSubsystem not found"));
		return;
	}

	bool bSuccess = false;
	FDecorationData DecData = TableMgr->GetDecorationData(RowName, bSuccess);

	if (bSuccess && !DecData.DecorationMesh.IsNull())
	{
		UStaticMesh* Mesh = DecData.DecorationMesh.LoadSynchronous();
		if (Mesh)
		{
			// 변경되었을 때만 저장
			bool bChanged = (CurrentFloorTileRowName != RowName);

			CurrentFloorTileRowName = RowName;
			SetFloorTileMesh(Mesh);

			if (bChanged)
			{
				if (USaveLoadManager* SaveLoadMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
				{
					SaveLoadMgr->SaveGameData();
				}
			}

			UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Floor tile loaded from RowName: %s (Changed: %d)"), *RowName.ToString(), bChanged);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Failed to load mesh for RowName: %s"), *RowName.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Failed to load floor tile from RowName: %s (bSuccess=%d)"), *RowName.ToString(), bSuccess);
	}
}

// ========== 확장 시스템 ==========

bool AOfficeInterior::ExpandLeft()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior::ExpandLeft] this=%p, WallLeft=%p, WallRight=%p"),
		this, WallLeft, WallRight);

	const FIntPoint PreviousTileCount = GetTileCount();
	const FIntPoint RequestedTileCount(PreviousTileCount.X + 1, PreviousTileCount.Y);
	if (!TryApplyValidatedFootprint(
		RequestedTileCount,
		FIntPoint(StarterTileCountX, StarterTileCountY),
		true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Cannot expand left - invalid footprint configuration."));
		return false;
	}

	// 컴포넌트가 없으면 다시 찾기
	if (!WallLeft || !WallRight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior::ExpandLeft] Walls NULL, re-initializing..."));
		InitializeComponents();
	}

	UpdateScalesForExpansion();
	AddTilesForExpansion(EOfficeExpandDirection::Left);
	UpdateNavMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Expanded left - TileCountX is now %d"), TileCountX);
	UpdateExpansionPreview();
	OnFootprintChanged.Broadcast(GetTileCount());
	return true;
}

bool AOfficeInterior::ExpandRight()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior::ExpandRight] this=%p, WallLeft=%p, WallRight=%p"),
		this, WallLeft, WallRight);

	const FIntPoint PreviousTileCount = GetTileCount();
	const FIntPoint RequestedTileCount(PreviousTileCount.X, PreviousTileCount.Y + 1);
	if (!TryApplyValidatedFootprint(
		RequestedTileCount,
		FIntPoint(StarterTileCountX, StarterTileCountY),
		true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Cannot expand right - invalid footprint configuration."));
		return false;
	}

	// 컴포넌트가 없으면 다시 찾기
	if (!WallLeft || !WallRight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior::ExpandRight] Walls NULL, re-initializing..."));
		InitializeComponents();
	}

	UpdateScalesForExpansion();
	AddTilesForExpansion(EOfficeExpandDirection::Right);
	UpdateNavMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] Expanded right - TileCountY is now %d"), TileCountY);
	UpdateExpansionPreview();
	OnFootprintChanged.Broadcast(GetTileCount());
	return true;
}

bool AOfficeInterior::CanExpand(EOfficeExpandDirection Direction) const
{
	if (Direction == EOfficeExpandDirection::Left)
	{
		return TileCountX < MaxTileCountX;
	}
	return TileCountY < MaxTileCountY;
}

int64 AOfficeInterior::GetExpandCost(EOfficeExpandDirection Direction) const
{
	// 기준점 = 시작 타일 수 (프리셋이 부여한 공짜 타일은 회차에 미포함)
	const int32 InitialCount = (Direction == EOfficeExpandDirection::Left) ? StarterTileCountX : StarterTileCountY;
	const int32 CurrentCount = (Direction == EOfficeExpandDirection::Left) ? TileCountX : TileCountY;
	const int32 ExpansionCount = FMath::Max(CurrentCount - InitialCount, 0);

	if (ExpansionDiamondCosts.IsEmpty())
	{
		return 0;
	}
	return ExpansionDiamondCosts[FMath::Min(ExpansionCount, ExpansionDiamondCosts.Num() - 1)];
}

FVector2D AOfficeInterior::GetCurrentSize() const
{
	return FVector2D(TileCountX * TileSize, TileCountY * TileSize);
}

FIntPoint AOfficeInterior::GetTileCount() const
{
	return FIntPoint(TileCountX, TileCountY);
}

FIntPoint AOfficeInterior::GetMaxTileCount() const
{
	return FIntPoint(MaxTileCountX, MaxTileCountY);
}

// ========== 다음 확장 칸 고스트 ==========

namespace
{
UMaterialInterface* LoadExpansionGuideMaterial()
{
	static TSoftObjectPtr<UMaterialInterface> EdgePath(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Resources/Materials/M_PlotEdgeFlow.M_PlotEdgeFlow")));
	return EdgePath.LoadSynchronous();
}

constexpr int32 ExpansionPreviewIndex(const EOfficeExpandDirection Direction)
{
	return Direction == EOfficeExpandDirection::Left ? 0 : 1;
}

struct FExpansionGuideEdge
{
	FVector CenterWorld = FVector::ZeroVector;
	FVector OutwardWorld = FVector::ZeroVector;
	FVector FlowWorld = FVector::ZeroVector;
	float TileWorldSize = 0.f;
};

double SupportRadius(const FTransform& Transform, const FVector& HalfExtent, const FVector& Direction)
{
	return FMath::Abs(FVector::DotProduct(Transform.TransformVector(FVector(HalfExtent.X, 0.0, 0.0)), Direction))
		+ FMath::Abs(FVector::DotProduct(Transform.TransformVector(FVector(0.0, HalfExtent.Y, 0.0)), Direction))
		+ FMath::Abs(FVector::DotProduct(Transform.TransformVector(FVector(0.0, 0.0, HalfExtent.Z)), Direction));
}

bool TryMakeExpansionGuideEdge(
	const FTransform& InteriorTransform,
	const UStaticMeshComponent* SourceFloor,
	const FIntPoint TileCount,
	const float TileSize,
	const float StructuralFloorZ,
	const EOfficeExpandDirection Direction,
	FExpansionGuideEdge& OutEdge)
{
	if (TileSize <= 0.f) { return false; }
	const FVector OutwardLocal = Direction == EOfficeExpandDirection::Left
		? FVector::XAxisVector : -FVector::YAxisVector;
	const FVector FlowLocal = Direction == EOfficeExpandDirection::Left
		? FVector::YAxisVector : FVector::XAxisVector;
	const FVector TileStepWorld = InteriorTransform.TransformVector(OutwardLocal * TileSize);
	const FVector OutwardWorld = TileStepWorld.GetSafeNormal();
	const FVector FlowWorld = InteriorTransform.TransformVector(FlowLocal).GetSafeNormal();
	if (OutwardWorld.IsNearlyZero() || FlowWorld.IsNearlyZero()) { return false; }

	FVector CenterWorld = FVector::ZeroVector;
	double EdgeRadius = 0.0;
	double SurfaceZ = 0.0;
	if (SourceFloor && SourceFloor->GetStaticMesh())
	{
		const FBoxSphereBounds MeshBounds = SourceFloor->GetStaticMesh()->GetBounds();
		const FTransform FloorTransform = SourceFloor->GetComponentTransform();
		CenterWorld = FloorTransform.TransformPosition(MeshBounds.Origin);
		EdgeRadius = SupportRadius(FloorTransform, MeshBounds.BoxExtent, OutwardWorld);
		SurfaceZ = CenterWorld.Z + SupportRadius(FloorTransform, MeshBounds.BoxExtent, FVector::UpVector);
	}
	else
	{
		const FBox2D FloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(TileCount, TileSize);
		if (!FloorBounds.bIsValid) { return false; }
		const FVector2D Center = FloorBounds.GetCenter();
		CenterWorld = InteriorTransform.TransformPosition(FVector(Center.X, Center.Y, StructuralFloorZ));
		const FVector HalfExtent(FloorBounds.GetSize().X * 0.5, FloorBounds.GetSize().Y * 0.5, 0.0);
		EdgeRadius = SupportRadius(InteriorTransform, HalfExtent, OutwardWorld);
		SurfaceZ = CenterWorld.Z;
	}

	OutEdge.CenterWorld = CenterWorld + OutwardWorld * EdgeRadius;
	OutEdge.CenterWorld.Z = SurfaceZ;
	OutEdge.OutwardWorld = OutwardWorld;
	OutEdge.FlowWorld = FlowWorld;
	OutEdge.TileWorldSize = static_cast<float>(TileStepWorld.Size());
	return true;
}
}

bool AOfficeInterior::HasExpansionCell(const EOfficeExpandDirection Direction) const
{
	return FOfficeFootprintGeometry::MakeExpansionCellBounds(
		GetTileCount(), GetMaxTileCount(), Direction, TileSize).bIsValid;
}

FVector AOfficeInterior::GetExpansionAnchorWorld(const EOfficeExpandDirection Direction) const
{
	if (!HasExpansionCell(Direction)) { return FVector::ZeroVector; }
	FExpansionGuideEdge Edge;
	if (!TryMakeExpansionGuideEdge(GetActorTransform(), Floor, GetTileCount(), TileSize,
		GetStructuralFloorZLocal(), Direction, Edge))
	{
		return FVector::ZeroVector;
	}
	return Edge.CenterWorld + Edge.OutwardWorld * (Edge.TileWorldSize * 0.1f);
}

FVector AOfficeInterior::GetExpansionDirectionWorld(const EOfficeExpandDirection Direction) const
{
	const FVector LocalDirection = Direction == EOfficeExpandDirection::Left
		? FVector::XAxisVector : -FVector::YAxisVector;
	return GetActorTransform().TransformVector(LocalDirection).GetSafeNormal();
}

void AOfficeInterior::SetExpansionPreviewVisible(const bool bVisible)
{
	bExpansionPreviewVisible = bVisible;
	UpdateExpansionPreview();
}

namespace
{
// 메시가 피벗에서 어느 쪽으로 뻗어 있는지 ― 성장 방향의 부호 포함 길이.
// 벽(SM_Walls_006)은 로컬 X[0,400] 이라 +400, 바닥(SM_Walls_011)은 X[-400,0] 이라 -400 이다.
float SignedLocalSpan(const UStaticMesh* Mesh, const EAxis::Type Axis)
{
	if (!Mesh) { return 0.f; }
	const FVector Origin = Mesh->GetBounds().Origin;
	switch (Axis)
	{
	case EAxis::X: return static_cast<float>(Origin.X) * 2.f;
	case EAxis::Y: return static_cast<float>(Origin.Y) * 2.f;
	default:       return static_cast<float>(Origin.Z) * 2.f;
	}
}

// 원본 컴포넌트를 "한 칸 더" 복제한 고스트. 회전·메시는 원본 그대로라 자세가 틀어질 수 없다.
void ApplySegmentGhost(
	UStaticMeshComponent* Ghost, const UStaticMeshComponent* Source,
	const EAxis::Type Axis, UMaterialInstanceDynamic* GhostMID, const float BandFraction)
{
	if (!Ghost || !Source) { return; }

	UStaticMesh* Mesh = Source->GetStaticMesh();
	Ghost->SetStaticMesh(Mesh);
	for (int32 Slot = 0; Slot < Ghost->GetNumMaterials(); ++Slot)
	{
		Ghost->SetMaterial(Slot, GhostMID);
	}
	Ghost->SetRelativeTransform(FOfficeFootprintGeometry::MakeExpansionSegmentTransform(
		Source->GetRelativeTransform(), Axis, SignedLocalSpan(Mesh, Axis), BandFraction));
	Ghost->SetVisibility(true);
}
}

void AOfficeInterior::UpdateExpansionPreview()
{
	static const EOfficeExpandDirection Dirs[2] = {
		EOfficeExpandDirection::Left, EOfficeExpandDirection::Right };

	for (const EOfficeExpandDirection Dir : Dirs)
	{
		const int32 Index = ExpansionPreviewIndex(Dir);
		const bool bLeft = (Dir == EOfficeExpandDirection::Left);

		const bool bShow = bExpansionPreviewVisible
			&& FOfficeFootprintGeometry::MakeExpansionCellBounds(
				GetTileCount(), GetMaxTileCount(), Dir, TileSize).bIsValid;

		if (!bShow)
		{
			if (ExpansionFloorPreview[Index]) { ExpansionFloorPreview[Index]->SetVisibility(false); }
			if (ExpansionWallPreview[Index]) { ExpansionWallPreview[Index]->SetVisibility(false); }
			continue;
		}

		UMaterialInterface* Guide = LoadExpansionGuideMaterial();
		if (!Guide || !Floor)
		{
			continue;
		}

		if (!ExpansionPreviewMID[Index]
			|| ExpansionPreviewMID[Index]->GetMaterial() != Guide->GetMaterial())
		{
			ExpansionPreviewMID[Index] = UMaterialInstanceDynamic::Create(Guide, this);
		}
		FExpansionGuideEdge Edge;
		if (!TryMakeExpansionGuideEdge(GetActorTransform(), Floor, GetTileCount(), TileSize,
			GetStructuralFloorZLocal(), Dir, Edge))
		{
			if (ExpansionFloorPreview[Index]) { ExpansionFloorPreview[Index]->SetVisibility(false); }
			continue;
		}
		const float EdgeOrigin = static_cast<float>(FVector::DotProduct(Edge.CenterWorld, Edge.OutwardWorld))
			+ Edge.TileWorldSize * 0.022f;
		ExpansionPreviewMID[Index]->SetVectorParameterValue(
			TEXT("Color"), FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("DED7C4"))));
		ExpansionPreviewMID[Index]->SetVectorParameterValue(TEXT("FlowAxis"),
			FLinearColor(static_cast<float>(Edge.FlowWorld.X), static_cast<float>(Edge.FlowWorld.Y),
				static_cast<float>(Edge.FlowWorld.Z), 0.f));
		ExpansionPreviewMID[Index]->SetVectorParameterValue(TEXT("CrossAxis"),
			FLinearColor(static_cast<float>(Edge.OutwardWorld.X), static_cast<float>(Edge.OutwardWorld.Y),
				static_cast<float>(Edge.OutwardWorld.Z), 0.f));
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("FlowSpeed"), 0.f);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("GuideStrength"), 0.f);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("GlowStrength"), 0.f);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("LineStrength"), ExpansionGuideLineStrength);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("EdgeSoftness"), Edge.TileWorldSize * 0.002f);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("DashPeriod"), Edge.TileWorldSize * 0.13f);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("DashLength"), Edge.TileWorldSize * 0.07f);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("FenceHeight"), Edge.TileWorldSize * 0.014f);
		ExpansionPreviewMID[Index]->SetScalarParameterValue(TEXT("EdgeOrigin"), EdgeOrigin);

		auto EnsureGhost = [this](TObjectPtr<UStaticMeshComponent>& Slot, const TCHAR* Tag)
		{
			if (Slot) { return; }
			Slot = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(Tag)));
			Slot->SetupAttachment(RootScene);
			Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Slot->SetGenerateOverlapEvents(false);
			Slot->SetCastShadow(false);
			Slot->SetCanEverAffectNavigation(false);
			Slot->RegisterComponent();
		};

		EnsureGhost(ExpansionFloorPreview[Index], TEXT("ExpansionFloorGhost"));

		// 원본 바닥 자세를 복제하고 경계 바깥 8%만 재질의 호스트로 남긴다.
		ApplySegmentGhost(ExpansionFloorPreview[Index], Floor,
			bLeft ? EAxis::X : EAxis::Z, ExpansionPreviewMID[Index], 0.08f);

		// 벽 토막 고스트는 쓰지 않는다 [확정 2026-10-07] ― 420 높이 판이라 정작 늘어나는
		// 바닥보다 덩치가 커서 "공간을 너무 많이 보여준다"는 판정을 받았다.
		if (ExpansionWallPreview[Index])
		{
			ExpansionWallPreview[Index]->SetVisibility(false);
		}
	}
}

FBox2D AOfficeInterior::GetCurrentFloorBoundsLocal() const
{
	return FOfficeFootprintGeometry::MakeFloorBounds(GetTileCount(), TileSize);
}

FBox2D AOfficeInterior::GetExteriorBoundsLocal() const
{
	FBox2D ExteriorBounds = GetCurrentFloorBoundsLocal();
	for (const UStaticMeshComponent* WallComponent : { WallLeft, WallRight })
	{
		if (!WallComponent) { continue; }
		const UStaticMesh* WallMesh = WallComponent->GetStaticMesh();
		if (!WallMesh) { continue; }
		const FTransform WallToInterior = WallComponent->GetComponentTransform().GetRelativeTransform(GetActorTransform());
		const FBox WallBounds = WallMesh->GetBoundingBox().TransformBy(WallToInterior);
		ExteriorBounds += FVector2D(WallBounds.Min.X, WallBounds.Min.Y);
		ExteriorBounds += FVector2D(WallBounds.Max.X, WallBounds.Max.Y);
	}
	return ExteriorBounds;
}

FBox2D AOfficeInterior::GetMaxFloorBoundsLocal() const
{
	return FOfficeFootprintGeometry::MakeFloorBounds(GetMaxTileCount(), TileSize);
}

float AOfficeInterior::GetStructuralFloorZLocal() const
{
	return FOfficeFootprintGeometry::StructuralFloorZ;
}

void AOfficeInterior::UpdateScalesForExpansion()
{
	// 초기 타일 개수
	const int32 InitialTileCountX = 1;
	const int32 InitialTileCountY = 2;

	// 확장 횟수 계산
	int32 LeftExpansions = TileCountX - InitialTileCountX;   // 0, 1, 2, ...
	int32 RightExpansions = TileCountY - InitialTileCountY;  // 0, 1, 2, ...

	// Floor 스케일: 초기 (1, 1, 2), 왼쪽 확장 시 X+1, 오른쪽 확장 시 Z+1
	if (Floor)
	{
		float ScaleX = 1.f + LeftExpansions;
		float ScaleY = 1.f;
		float ScaleZ = 2.f + RightExpansions;
		Floor->SetRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));
	}

	// WallLeft: 초기 X=1.0, 확장 시 +1씩 (1 -> 2 -> 3)
	if (WallLeft)
	{
		float ScaleX = 1.f + LeftExpansions;
		FVector NewScale(ScaleX, 1.f, 1.05f);
		WallLeft->SetRelativeScale3D(NewScale);

		// 적용 후 실제 스케일 확인
		FVector ActualScale = WallLeft->GetRelativeScale3D();
		UE_LOG(LogTemp, Warning, TEXT("[UpdateScalesForExpansion] WallLeft: Set=%s, Actual=%s, Component=%s"),
			*NewScale.ToString(), *ActualScale.ToString(), *WallLeft->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[UpdateScalesForExpansion] WallLeft is NULL!"));
	}

	// WallRight: 초기 X=2.05, 확장 시 +1씩 (2.05 -> 3.05 -> 4.05)
	if (WallRight)
	{
		float ScaleX = 2.05f + RightExpansions;
		FVector NewScale(ScaleX, 1.f, 1.05f);
		WallRight->SetRelativeScale3D(NewScale);

		// 적용 후 실제 스케일 확인
		FVector ActualScale = WallRight->GetRelativeScale3D();
		UE_LOG(LogTemp, Warning, TEXT("[UpdateScalesForExpansion] WallRight: Set=%s, Actual=%s, Component=%s"),
			*NewScale.ToString(), *ActualScale.ToString(), *WallRight->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[UpdateScalesForExpansion] WallRight is NULL!"));
	}

	FloorSize = GetCurrentSize();
	UpdateRecruitmentEntrance();
}

AInteractiveDoorActor* AOfficeInterior::GetRecruitmentDoor() const
{
	return RecruitmentDoor ? Cast<AInteractiveDoorActor>(RecruitmentDoor->GetChildActor()) : nullptr;
}

FBox AOfficeInterior::GetRecruitmentEntranceExclusionBoundsLocal() const
{
	if (!GetRecruitmentDoor() || !WallLeft || !WallLeft->GetStaticMesh()
		|| !DoorWallHeader || !DoorWallHeader->IsVisible())
	{
		return FBox(ForceInit);
	}

	const FVector DoorPosition = RecruitmentDoor->GetRelativeLocation();
	const FBox WallBounds = WallLeft->GetStaticMesh()->GetBoundingBox().TransformBy(WallLeft->GetRelativeTransform());
	const double Margin = FMath::Max(0.f, DoorPlacementMargin);
	const double HalfWidth = DoorOpeningSize.X * 0.5 + Margin;
	const double ApproachDepth = FMath::Max(0.f, DoorApproachDepth);
	// 실내는 벽의 -Y 쪽이다. 문 애니메이션 바운드 대신 고정 개구부를 기준으로 한다.
	return FBox(
		FVector(DoorPosition.X - HalfWidth, FMath::Min(WallBounds.Min.Y, GetCurrentFloorBoundsLocal().Max.Y) - ApproachDepth,
			DoorPosition.Z - Margin),
		FVector(DoorPosition.X + HalfWidth, WallBounds.Max.Y + Margin, DoorPosition.Z + DoorOpeningSize.Y + Margin));
}

bool AOfficeInterior::IsRecruitmentEntranceBlocked(const FBox& OfficeLocalBounds) const
{
	const FBox ReservedBounds = GetRecruitmentEntranceExclusionBoundsLocal();
	return ReservedBounds.IsValid && OfficeLocalBounds.IsValid && ReservedBounds.Intersect(OfficeLocalBounds);
}

bool AOfficeInterior::IsTelevisionPlacementBlocked(const FBox& OfficeLocalBounds) const
{
	const FBox ReservedBounds = AOfficeTelevision::GetExclusionBoundsLocal(this);
	return ReservedBounds.IsValid && OfficeLocalBounds.IsValid && ReservedBounds.Intersect(OfficeLocalBounds);
}

bool AOfficeInterior::IsRecruitmentEntranceFloorBlocked(const FBox2D& OfficeLocalFootprint) const
{
	const FBox ReservedBounds = GetRecruitmentEntranceExclusionBoundsLocal();
	return ReservedBounds.IsValid && OfficeLocalFootprint.bIsValid
		&& OfficeLocalFootprint.Min.X <= ReservedBounds.Max.X && OfficeLocalFootprint.Max.X >= ReservedBounds.Min.X
		&& OfficeLocalFootprint.Min.Y <= ReservedBounds.Max.Y && OfficeLocalFootprint.Max.Y >= ReservedBounds.Min.Y;
}

void AOfficeInterior::UpdateRecruitmentEntrance()
{
	if (!WallLeft) { return; }
	UStaticMesh* WallMesh = WallLeft->GetStaticMesh();
	const AInteractiveDoorActor* DoorActor = GetRecruitmentDoor();
	bool bValidEntrance = WallMesh && DoorActor;
	FBox SourceBounds(ForceInit);
	FBox WallBounds(ForceInit);
	FVector DoorPosition = FVector::ZeroVector;
	if (bValidEntrance)
	{
		SourceBounds = WallMesh->GetBoundingBox();
		WallBounds = SourceBounds.TransformBy(WallLeft->GetRelativeTransform());
		DoorPosition = RecruitmentDoor->GetRelativeLocation();
		DoorPosition.X = WallBounds.Min.X + DoorWallCornerInset;
		double FinishedFloorZ = GetStructuralFloorZLocal();
		if (FloorTileMesh)
		{
			FinishedFloorZ = FOfficeFootprintGeometry::FloorTileCenterZ + FloorTileMesh->GetBoundingBox().Max.Z;
		}
		else if (Floor && Floor->GetStaticMesh())
		{
			FinishedFloorZ = Floor->GetStaticMesh()->GetBoundingBox().TransformBy(Floor->GetRelativeTransform()).Max.Z;
		}
		DoorPosition.Z = FinishedFloorZ - DoorThresholdHeight;
		RecruitmentDoor->SetRelativeLocation(DoorPosition);
		const FVector MeshSize = SourceBounds.GetSize();
		const double HalfWidth = DoorOpeningSize.X * 0.5;
		bValidEntrance = WallLeft->GetRelativeRotation().IsNearlyZero()
			&& RecruitmentDoor->GetRelativeRotation().IsNearlyZero()
			&& RecruitmentDoor->GetRelativeScale3D().Equals(FVector::OneVector)
			&& MeshSize.GetMin() > UE_SMALL_NUMBER
			&& DoorOpeningSize.X > 0.f && DoorOpeningSize.Y > 0.f
			&& DoorPosition.Y >= WallBounds.Min.Y && DoorPosition.Y <= WallBounds.Max.Y
			&& DoorPosition.X - HalfWidth > WallBounds.Min.X
			&& DoorPosition.X + HalfWidth < WallBounds.Max.X
			&& DoorPosition.Z >= WallBounds.Min.Z
			&& DoorPosition.Z + DoorOpeningSize.Y < WallBounds.Max.Z;
	}
	WallLeft->SetVisibility(!bValidEntrance);
	WallLeft->SetHiddenInGame(bValidEntrance);
	WallLeft->SetCollisionEnabled(bValidEntrance ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	if (!bValidEntrance)
	{
		for (UStaticMeshComponent* WallPiece : { DoorWallLeft.Get(), DoorWallRight.Get(), DoorWallHeader.Get(), DoorWallSill.Get() })
		{
			WallPiece->SetVisibility(false);
			WallPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return;
	}

	// 원본 벽의 메시/재질을 재사용하고, 각 조각의 실제 경계를 출입구에 맞춘다.
	const auto FitWallPiece = [this, WallMesh, SourceBounds](UStaticMeshComponent* WallPiece, const FVector& Minimum, const FVector& Maximum)
	{
		const FVector PieceSize = Maximum - Minimum;
		const bool bHasVolume = PieceSize.GetMin() > UE_SMALL_NUMBER;
		WallPiece->SetVisibility(bHasVolume);
		WallPiece->SetHiddenInGame(!bHasVolume);
		WallPiece->SetCollisionEnabled(bHasVolume ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		if (!bHasVolume) { return; }
		WallPiece->SetStaticMesh(WallMesh);
		const FVector PieceScale = PieceSize / SourceBounds.GetSize();
		WallPiece->SetRelativeTransform(FTransform(FQuat::Identity, Minimum - SourceBounds.Min * PieceScale, PieceScale));
		for (int32 MaterialIndex = 0; MaterialIndex < WallLeft->GetNumMaterials(); ++MaterialIndex)
		{
			WallPiece->SetMaterial(MaterialIndex, WallLeft->GetMaterial(MaterialIndex));
		}
		WallPiece->SetLightingChannels(false, true, false);
		WallPiece->SetCollisionResponseToChannels(WallLeft->GetCollisionResponseToChannels());
	};
	const double OpeningLeft = DoorPosition.X - DoorOpeningSize.X * 0.5;
	const double OpeningRight = DoorPosition.X + DoorOpeningSize.X * 0.5;
	const double OpeningTop = DoorPosition.Z + DoorOpeningSize.Y;
	FitWallPiece(DoorWallLeft, WallBounds.Min, FVector(OpeningLeft, WallBounds.Max.Y, WallBounds.Max.Z));
	FitWallPiece(DoorWallRight, FVector(OpeningRight, WallBounds.Min.Y, WallBounds.Min.Z), WallBounds.Max);
	FitWallPiece(DoorWallHeader, FVector(OpeningLeft, WallBounds.Min.Y, OpeningTop), FVector(OpeningRight, WallBounds.Max.Y, WallBounds.Max.Z));
	FitWallPiece(DoorWallSill, FVector(OpeningLeft, WallBounds.Min.Y, WallBounds.Min.Z), FVector(OpeningRight, WallBounds.Max.Y, DoorPosition.Z));
}

// ========== 저장/로드 ==========

void AOfficeInterior::ApplyOfficeSaveData(const FOfficeSaveData& OfficeData)
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] ========== ApplyOfficeSaveData BEGIN =========="));
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Input: TileX=%d, TileY=%d"), OfficeData.TileCountX, OfficeData.TileCountY);
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Current before apply: TileX=%d, TileY=%d"), TileCountX, TileCountY);

	// 확장 상태 적용
	if (!TryApplyValidatedFootprint(
		FIntPoint(OfficeData.TileCountX, OfficeData.TileCountY),
		FIntPoint(OfficeData.StarterTileCountX, OfficeData.StarterTileCountY),
		false))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] Save data was not applied because the footprint configuration is invalid."));
		return;
	}

	// 컴포넌트 초기화 (세이브 로드 시점에 NULL일 수 있음)
	InitializeComponents();

	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] After assignment: TileX=%d, TileY=%d"), TileCountX, TileCountY);

	UpdateScalesForExpansion();

	// 기존 타일 모두 제거
	ClearFloorTiles();

	// 바닥 타일 메시 로드
	if (!OfficeData.CurrentFloorTileRowName.IsNone())
	{
		// 저장된 타일 RowName으로 메시 로드
		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bSuccess = false;
			FDecorationData DecData = TableMgr->GetDecorationData(OfficeData.CurrentFloorTileRowName, bSuccess);
			if (bSuccess && !DecData.DecorationMesh.IsNull())
			{
				FloorTileMesh = DecData.DecorationMesh.LoadSynchronous();
				CurrentFloorTileRowName = OfficeData.CurrentFloorTileRowName;
			}
		}
	}

	// 타일 메시가 있으면 전체 타일 생성
	if (FloorTileMesh)
	{
		GenerateFloorTiles();
	}

	UpdateNavMeshBounds();

	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] SaveData applied: TileCountX=%d, TileCountY=%d, FloorTile=%s, TileCount=%d"),
		TileCountX, TileCountY, *OfficeData.CurrentFloorTileRowName.ToString(), FloorTiles.Num());
	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] ========== ApplyOfficeSaveData END =========="));
	UpdateExpansionPreview();
	OnFootprintChanged.Broadcast(GetTileCount());
}

// ========== 벽 정보 (배치 시스템용) ==========

FVector AOfficeInterior::GetWallLocation(EWallSide WallSide) const
{
	UStaticMeshComponent* WallComp = (WallSide == EWallSide::Left) ? WallLeft : WallRight;
	if (WallComp)
	{
		return WallComp->GetComponentLocation();
	}
	return GetActorLocation();
}

FVector AOfficeInterior::GetWallNormal(EWallSide WallSide) const
{
	const FVector LocalNormal = WallSide == EWallSide::Left ? FVector(0, 1, 0) : FVector(-1, 0, 0);
	return GetActorTransform().TransformVectorNoScale(LocalNormal).GetSafeNormal();
}

FPlane AOfficeInterior::GetWallPlane(EWallSide WallSide) const
{
	FVector WallLoc = GetWallLocation(WallSide);
	FVector Normal = GetWallNormal(WallSide);

	// FPlane(법선벡터, 점)으로 평면 정의
	return FPlane(WallLoc, Normal);
}

void AOfficeInterior::GetWallPlacementBoundsLocal(EWallSide WallSide, FVector& OutMinBounds, FVector& OutMaxBounds) const
{
	const FBox2D FloorBounds = GetCurrentFloorBoundsLocal();
	const float FloorZ = GetStructuralFloorZLocal();
	if (WallSide == EWallSide::Left)
	{
		OutMinBounds = FVector(FloorBounds.Min.X + 50.f, -11.f, FloorZ + 50.f);
		OutMaxBounds = FVector(FloorBounds.Max.X - 50.f, -11.f, FloorZ + WallHeight - 30.f);
	}
	else
	{
		OutMinBounds = FVector(4.f, FloorBounds.Min.Y + 50.f, FloorZ + 50.f);
		OutMaxBounds = FVector(4.f, FloorBounds.Max.Y - 50.f, FloorZ + WallHeight - 30.f);
	}
}

void AOfficeInterior::GetWallPlacementBounds(EWallSide WallSide, FVector& OutMinBounds, FVector& OutMaxBounds) const
{
	FVector LocalMin, LocalMax;
	GetWallPlacementBoundsLocal(WallSide, LocalMin, LocalMax);
	const FBox WorldBounds = FBox(LocalMin, LocalMax).TransformBy(GetActorTransform());
	OutMinBounds = WorldBounds.Min;
	OutMaxBounds = WorldBounds.Max;
}

FVector AOfficeInterior::ConstrainWallDecorationLocation(EWallSide WallSide, const FVector& WorldLocation, bool bSnapToGrid) const
{
	FVector LocalPosition = GetActorTransform().InverseTransformPosition(WorldLocation);
	FVector LocalMin, LocalMax;
	GetWallPlacementBoundsLocal(WallSide, LocalMin, LocalMax);
	if (bSnapToGrid)
	{
		LocalPosition.X = FMath::GridSnap(LocalPosition.X, 5.0);
		LocalPosition.Y = FMath::GridSnap(LocalPosition.Y, 5.0);
		LocalPosition.Z = FMath::GridSnap(LocalPosition.Z, 5.0);
	}
	if (WallSide == EWallSide::Left) LocalPosition.Y = LocalMin.Y;
	else LocalPosition.X = LocalMin.X;
	LocalPosition.Z = FMath::Clamp(LocalPosition.Z, LocalMin.Z, LocalMax.Z);
	return GetActorTransform().TransformPosition(LocalPosition);
}
