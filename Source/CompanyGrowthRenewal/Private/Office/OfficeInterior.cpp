// Fill out your copyright notice in the Description page of Project Settings.

#include "Office/OfficeInterior.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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

	UE_LOG(LogTemp, Warning, TEXT("[OfficeInterior] ========== BeginPlay END =========="));
	OnFootprintChanged.Broadcast(GetTileCount());
}

void AOfficeInterior::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	InitializeComponents();
	UpdateNavMeshBounds();
}

void AOfficeInterior::InitializeComponents()
{
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

	// 바닥 컴포넌트가 있으면 그 크기 기준으로 설정
	if (Floor)
	{
		FBoxSphereBounds FloorBounds = Floor->Bounds;
		FVector Extent = FloorBounds.BoxExtent;

		// 바닥 영역에 맞게 NavMesh Bounds 설정 (높이는 캐릭터가 이동 가능한 범위)
		NavMeshBounds->SetBoxExtent(FVector(Extent.X, Extent.Y, 150.f));
		NavMeshBounds->SetWorldLocation(FloorBounds.Origin + FVector(0.f, 0.f, 150.f));

		UE_LOG(LogTemp, Log, TEXT("[OfficeInterior] NavMesh Bounds updated: Extent=(%f, %f, %f)"),
			Extent.X, Extent.Y, 150.f);
	}
	else
	{
		// 바닥 없으면 타일 기반 크기 사용
		FVector2D CurrentSize = GetCurrentSize();
		NavMeshBounds->SetBoxExtent(FVector(CurrentSize.X * 0.5f, CurrentSize.Y * 0.5f, 150.f));
		NavMeshBounds->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	}

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
	FBox Bounds = GetNavMeshBoundsBox();

	// 박스 내 랜덤 위치 (Z는 바닥 높이로 고정)
	float X = FMath::RandRange(Bounds.Min.X, Bounds.Max.X);
	float Y = FMath::RandRange(Bounds.Min.Y, Bounds.Max.Y);
	float Z = GetActorLocation().Z;

	return FVector(X, Y, Z);
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
			Tile->SetupAttachment(RootScene);

			Tile->SetRelativeLocation(FOfficeFootprintGeometry::MakeTileCenter(X, Y, TileSize));

			Tile->RegisterComponent();
			FloorTiles.Add(Tile);
		}
	}

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
		}
	}

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

FBox2D AOfficeInterior::GetCurrentFloorBoundsLocal() const
{
	return FOfficeFootprintGeometry::MakeFloorBounds(GetTileCount(), TileSize);
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
	// Left 벽: Y=-11에 위치, 방 안쪽(+Y)을 향함
	// Right 벽: X=2에 위치, 방 안쪽(-X)을 향함
	if (WallSide == EWallSide::Left)
	{
		return FVector(0.f, 1.f, 0.f);  // Y+ 방향 (방 안쪽)
	}
	return FVector(-1.f, 0.f, 0.f);  // X- 방향 (방 안쪽)
}

FPlane AOfficeInterior::GetWallPlane(EWallSide WallSide) const
{
	FVector WallLoc = GetWallLocation(WallSide);
	FVector Normal = GetWallNormal(WallSide);

	// FPlane(법선벡터, 점)으로 평면 정의
	return FPlane(WallLoc, Normal);
}

void AOfficeInterior::GetWallPlacementBounds(EWallSide WallSide, FVector& OutMinBounds, FVector& OutMaxBounds) const
{
	UStaticMeshComponent* WallComp = (WallSide == EWallSide::Left) ? WallLeft : WallRight;

	if (!WallComp)
	{
		// 폴백: 기본값 사용
		OutMinBounds = FVector::ZeroVector;
		OutMaxBounds = FVector(0.f, 0.f, WallHeight);
		return;
	}

	// 벽 컴포넌트의 바운드에서 배치 가능 영역 계산
	FBoxSphereBounds WallBounds = WallComp->Bounds;
	FVector WallLoc = WallComp->GetComponentLocation();
	FVector WallExtent = WallBounds.BoxExtent;

	UE_LOG(LogTemp, Warning, TEXT("[GetWallPlacementBounds] WallSide=%s, WallLoc=%s, WallExtent=%s"),
		WallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"),
		*WallLoc.ToString(), *WallExtent.ToString());

	// 마진 (벽 가장자리에서 여유 공간)
	const float EdgeMargin = 50.f;
	const float BottomMargin = 50.f;    // 바닥에서 최소 높이
	const float TopMargin = 30.f;       // 천장에서 여유

	if (WallSide == EWallSide::Left)
	{
		// Left 벽: X축과 Z축 이동 가능, Y=-11 고정
		OutMinBounds = FVector(
			WallLoc.X - WallExtent.X + EdgeMargin,        // X 최소
			WallLoc.Y,                                    // Y는 벽 위치 고정
			BottomMargin                                  // Z 최소 (바닥에서 50 위)
		);
		OutMaxBounds = FVector(
			WallLoc.X + WallExtent.X - EdgeMargin,        // X 최대
			WallLoc.Y,
			WallHeight - TopMargin                        // Z 최대
		);
	}
	else
	{
		// Right 벽: X축과 Z축 범위 제한
		OutMinBounds = FVector(
			WallLoc.X - WallExtent.X + EdgeMargin,        // X 최소
			WallLoc.Y,                                    // Y는 벽 위치 고정
			BottomMargin
		);
		OutMaxBounds = FVector(
			WallLoc.X + WallExtent.X - EdgeMargin,        // X 최대
			WallLoc.Y,
			WallHeight - TopMargin
		);
	}
}
