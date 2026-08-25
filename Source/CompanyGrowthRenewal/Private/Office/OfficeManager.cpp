// Fill out your copyright notice in the Description page of Project Settings.

#include "Office/OfficeManager.h"
#include "Table/OfficeStarterPresetTable.h"
#include "Office/DecorationActor.h"
#include "Office/WorkstationActorBase.h"
#include "TimerManager.h"
#include "UI/Element/Building/WallSelectionWidget.h"
#include "Office/OfficeInterior.h"
#include "Player/OfficeCameraPawn.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Enum/NotificationType.h"
#include "Core/CGGameInstance.h"
#include "Data/EmployeeStatsData.h"
#include "Table/DecorationData.h"
#include "Table/WorkstationCardTable.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Kismet/GameplayStatics.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "GameMode/OfficeGameMode.h"
#include "Data/EmployeeTypes.h"

// ========== Subsystem Lifecycle ==========

void UOfficeManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] WorldSubsystem Initialized"));
}

void UOfficeManager::Deinitialize()
{
	if (WallSelectionWidget)
	{
		WallSelectionWidget->RemoveFromParent();
		WallSelectionWidget = nullptr;
	}

	Super::Deinitialize();

	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] WorldSubsystem Deinitialized"));
}

bool UOfficeManager::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UOfficeManager::CacheReferences()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!CameraPawn)
	{
		CameraPawn = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(World, 0));
		if (!CameraPawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] CameraPawn not found (may not be in OfficeMap)"));
		}
	}

	if (!OfficeInterior)
	{
		OfficeInterior = Cast<AOfficeInterior>(UGameplayStatics::GetActorOfClass(World, AOfficeInterior::StaticClass()));
	}
}

// ========== 꾸미기 모드 제어 ==========

void UOfficeManager::EnterDecorationMode()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] EnterDecorationMode called"));

	bIsInDecorationMode = true;
	CacheReferences();
}

void UOfficeManager::ExitDecorationMode()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] ExitDecorationMode called"));

	bIsInDecorationMode = false;
	SelectedDecorationData = FDecorationCardTable();

	HideWallSelectionWidget();

	if (CameraPawn && CameraPawn->IsInWallEditMode())
	{
		CameraPawn->ExitWallEditMode();
	}
}

// ========== 장식품 선택 ==========

void UOfficeManager::OnDecorationItemSelected(const FDecorationCardTable& CardData)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] OnDecorationItemSelected: %s"), *CardData.Name.ToString());

	CacheReferences();

	SelectedDecorationData = CardData;

	EDecorationSurface Surface = CardData.AllowedSurface;
	EDecorationCategory Category = CardData.Category;

	UE_LOG(LogTemp, Log, TEXT("  Surface: %d, Category: %d"), (int32)Surface, (int32)Category);

	switch (Surface)
	{
	case EDecorationSurface::Wall:
		StartWallDecorationFlow();
		break;

	case EDecorationSurface::Grid:
		StartGridDecorationFlow();
		break;

	case EDecorationSurface::None:
		UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] Surface::None - should be handled separately"));
		break;

	default:
		UE_LOG(LogTemp, Error, TEXT("[OfficeManager] Unknown surface type: %d"), (int32)Surface);
		break;
	}
}

void UOfficeManager::OnFloorTileSelected(FName TileRowName)
{
	if (TileRowName.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeManager] OnFloorTileSelected: TileRowName is None"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] OnFloorTileSelected: %s"), *TileRowName.ToString());

	CacheReferences();

	if (OfficeInterior)
	{
		OfficeInterior->LoadFloorTileFromRowName(TileRowName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] OfficeInterior not found"));
	}
}

// ========== 벽 선택 콜백 ==========

void UOfficeManager::OnWallSelected(EWallSide WallSide)
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] OnWallSelected: %s"),
		WallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"));

	HideWallSelectionWidget();

	if (CameraPawn)
	{
		CameraPawn->FocusOnWall(WallSide);

		if (!SelectedDecorationData.RowName.IsNone())
		{
			CameraPawn->BeginWallDecorationPlacement(SelectedDecorationData, WallSide);
		}
	}
}

void UOfficeManager::OnWallSelectionCancelled()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] OnWallSelectionCancelled"));

	HideWallSelectionWidget();

	if (CameraPawn && CameraPawn->IsInWallEditMode())
	{
		CameraPawn->ExitWallEditMode();
	}

	SelectedDecorationData = FDecorationCardTable();
}

// ========== 내부 헬퍼 함수 ==========

void UOfficeManager::StartWallDecorationFlow()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] StartWallDecorationFlow"));

	if (CameraPawn)
	{
		CameraPawn->EnterWallEditMode();
		CameraPawn->FocusOnWall(EWallSide::Left);

		if (!SelectedDecorationData.RowName.IsNone())
		{
			CameraPawn->BeginWallDecorationPlacement(SelectedDecorationData, EWallSide::Left);
		}
	}
}

void UOfficeManager::StartFloorDecorationFlow()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] StartFloorDecorationFlow"));

	if (CameraPawn && !SelectedDecorationData.RowName.IsNone())
	{
		CameraPawn->BeginDecorationPlacement(SelectedDecorationData);
	}
}

void UOfficeManager::StartGridDecorationFlow()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] StartGridDecorationFlow"));

	if (CameraPawn && !SelectedDecorationData.RowName.IsNone())
	{
		CameraPawn->BeginDecorationPlacement(SelectedDecorationData);
	}
}

void UOfficeManager::CreateWallSelectionWidget()
{
	if (!WallSelectionWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeManager] WallSelectionWidgetClass is not set"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	WallSelectionWidget = CreateWidget<UWallSelectionWidget>(World, WallSelectionWidgetClass);
	if (!WallSelectionWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeManager] Failed to create WallSelectionWidget"));
		return;
	}

	WallSelectionWidget->OnWallSelected.AddDynamic(this, &UOfficeManager::OnWallSelected);
	WallSelectionWidget->OnSelectionCancelled.AddDynamic(this, &UOfficeManager::OnWallSelectionCancelled);

	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] WallSelectionWidget created successfully"));
}

void UOfficeManager::ShowWallSelectionWidget()
{
	if (!WallSelectionWidget)
	{
		CreateWallSelectionWidget();
	}

	if (!WallSelectionWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeManager] WallSelectionWidget is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] ShowWallSelectionWidget"));

	WallSelectionWidget->AddToViewport();
}

void UOfficeManager::HideWallSelectionWidget()
{
	if (!WallSelectionWidget)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] HideWallSelectionWidget"));

	WallSelectionWidget->RemoveFromParent();
}

// ========== 장식품 등록/해제 ==========

void UOfficeManager::RegisterPlacedDecoration(ADecorationActor* Decoration)
{
	if (Decoration && !PlacedDecorations.Contains(Decoration))
	{
		PlacedDecorations.Add(Decoration);

		UE_LOG(LogTemp, Log, TEXT("[OfficeManager] Registered decoration: %s (Total: %d)"),
			*Decoration->GetName(), PlacedDecorations.Num());
	}
}

void UOfficeManager::UnregisterPlacedDecoration(ADecorationActor* Decoration)
{
	if (Decoration)
	{
		PlacedDecorations.Remove(Decoration);

		UE_LOG(LogTemp, Log, TEXT("[OfficeManager] Unregistered decoration: %s (Total: %d)"),
			*Decoration->GetName(), PlacedDecorations.Num());
	}
}

// ========== 업무공간 등록/해제 ==========

void UOfficeManager::RegisterPlacedWorkstation(AWorkstationActorBase* Workstation)
{
	if (Workstation && !PlacedWorkstations.Contains(Workstation))
	{
		PlacedWorkstations.Add(Workstation);

		UE_LOG(LogTemp, Log, TEXT("[OfficeManager] Registered workstation: %s (Total: %d)"),
			*Workstation->GetName(), PlacedWorkstations.Num());

		OnWorkstationCountChanged.Broadcast();
	}
}

void UOfficeManager::UnregisterPlacedWorkstation(AWorkstationActorBase* Workstation)
{
	if (Workstation)
	{
		PlacedWorkstations.Remove(Workstation);

		UE_LOG(LogTemp, Log, TEXT("[OfficeManager] Unregistered workstation: %s (Total: %d)"),
			*Workstation->GetName(), PlacedWorkstations.Num());

		OnWorkstationCountChanged.Broadcast();
	}
}

// ========== 업무공간 검색 ==========

AWorkstationActorBase* UOfficeManager::FindWorkstationByEmployeeID(int32 EmployeeID)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] FindWorkstationByEmployeeID(%d) - PlacedWorkstations: %d"),
		EmployeeID, PlacedWorkstations.Num());

	for (AWorkstationActorBase* Workstation : PlacedWorkstations)
	{
		if (!Workstation)
		{
			continue;
		}

		// 배정된 직원 EmployeeID로 검색 (저장된 배정 정보 사용)
		for (int32 i = 0; i < Workstation->GetChairCount(); ++i)
		{
			int32 AssignedID = Workstation->GetAssignedEmployeeID(i);

			UE_LOG(LogTemp, Log, TEXT("[OfficeManager]   Seat %d: AssignedEmployeeID=%d"),
				i, AssignedID);

			if (AssignedID == EmployeeID)
			{
				return Workstation;
			}
		}
	}

	return nullptr;
}

// ========== 저장/로드 ==========

void UOfficeManager::FillOfficeSaveData(FOfficeSaveData& OutOfficeData) const
{
	// Decoration 데이터 수집
	OutOfficeData.PlacedDecorations.Empty();
	for (ADecorationActor* Decoration : PlacedDecorations)
	{
		if (!Decoration || Decoration->DecorationCardTableRowName.IsNone())
		{
			continue;
		}

		FDecorationSaveData DecSave;
		DecSave.RowName = Decoration->DecorationCardTableRowName;
		DecSave.Transform = Decoration->GetActorTransform();
		DecSave.Category = Decoration->GetCategory();

		OutOfficeData.PlacedDecorations.Add(DecSave);
	}

	// Workstation 데이터 수집
	OutOfficeData.OfficeWorkstations.Empty();
	for (AWorkstationActorBase* Workstation : PlacedWorkstations)
	{
		if (!Workstation)
		{
			continue;
		}

		FWorkstationSaveData WsSave = Workstation->GetSaveData();
		OutOfficeData.OfficeWorkstations.Add(WsSave);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] FillOfficeSaveData - Decorations: %d, Workstations: %d"),
		OutOfficeData.PlacedDecorations.Num(), OutOfficeData.OfficeWorkstations.Num());
}

void UOfficeManager::ApplyOfficeSaveData(const FOfficeSaveData& OfficeData)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeManager] TableManagerSubsystem not found"));
		return;
	}

	// ========== Decoration 복원 ==========

	// 기존 장식품 제거
	for (ADecorationActor* Decoration : PlacedDecorations)
	{
		if (Decoration)
		{
			Decoration->Destroy();
		}
	}
	PlacedDecorations.Empty();

	// 저장 데이터에서 장식품 복원
	for (const FDecorationSaveData& DecSave : OfficeData.PlacedDecorations)
	{
		if (DecSave.RowName.IsNone())
		{
			continue;
		}

		bool bSuccess = false;
		FDecorationData DecData = TableMgr->GetDecorationData(DecSave.RowName, bSuccess);
		if (!bSuccess || DecData.DecorationMesh.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] Failed to load decoration data: %s"), *DecSave.RowName.ToString());
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ADecorationActor* NewDecoration = World->SpawnActor<ADecorationActor>(
			ADecorationActor::StaticClass(),
			DecSave.Transform,
			SpawnParams);

		if (NewDecoration)
		{
			NewDecoration->DecorationCardTableRowName = DecSave.RowName;
			NewDecoration->Category = DecSave.Category;

			UStaticMesh* Mesh = DecData.DecorationMesh.LoadSynchronous();
			if (Mesh)
			{
				NewDecoration->SetDecorationMesh(Mesh);
			}

			// 배열에 직접 추가 (Register 호출하지 않음)
			PlacedDecorations.Add(NewDecoration);
		}
	}

	// ========== Workstation 복원 ==========

	// 기존 업무공간 제거
	for (AWorkstationActorBase* Workstation : PlacedWorkstations)
	{
		if (Workstation)
		{
			Workstation->Destroy();
		}
	}
	PlacedWorkstations.Empty();

	// 저장 데이터에서 업무공간 복원
	for (const FWorkstationSaveData& WsSave : OfficeData.OfficeWorkstations)
	{
		if (WsSave.WorkstationTypeID.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] WorkstationTypeID is None, skipping"));
			continue;
		}

		// WorkstationTypeID로 테이블에서 BlueprintClass 가져오기
		bool bSuccess = false;
		FWorkstationCardTable WsCardData = TableMgr->GetWorkstationCardInfo(WsSave.WorkstationTypeID, bSuccess);
		if (!bSuccess || WsCardData.BlueprintClass.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] Failed to load workstation card data: %s"), *WsSave.WorkstationTypeID.ToString());
			continue;
		}

		UClass* WsClass = WsCardData.BlueprintClass.LoadSynchronous();
		if (!WsClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] Failed to load workstation blueprint class: %s"), *WsSave.WorkstationTypeID.ToString());
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AWorkstationActorBase* NewWorkstation = World->SpawnActor<AWorkstationActorBase>(
			WsClass,
			WsSave.Transform,
			SpawnParams);

		if (NewWorkstation)
		{
			NewWorkstation->RestoreFromSaveData(WsSave);

			// 배열에 직접 추가 (Register 호출하지 않음)
			PlacedWorkstations.Add(NewWorkstation);

			UE_LOG(LogTemp, Log, TEXT("[OfficeManager] Restored workstation: %s at %s"),
				*WsSave.WorkstationTypeID.ToString(), *WsSave.Transform.GetLocation().ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] ApplyOfficeSaveData - Restored %d decorations, %d workstations"),
		PlacedDecorations.Num(), PlacedWorkstations.Num());

	OnWorkstationCountChanged.Broadcast();

	// 자동 배치(시작 프리셋/세이브 복원) 책상은 바닥 NavMesh 비동기 생성과 경합 → 전부 스폰된 다음 틱에 장애물 일괄 재반영
	if (World)
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UOfficeManager::RefreshWorkstationNavObstacles));
	}

	// 복원 완료 — 이 시점 이후의 SaveGameData만 오피스 인테리어를 디스크에 기록하도록 허용
	bInteriorRestored = true;
}

void UOfficeManager::RefreshWorkstationNavObstacles()
{
	// RecalcBoxExtent 로 재사이징(스폰 후 적용된 의자/스킨 메시까지 풋프린트에 포함) + NavMesh 재반영
	for (AWorkstationActorBase* Workstation : PlacedWorkstations)
	{
		if (Workstation)
		{
			Workstation->RecalcBoxExtent();
		}
	}
}

// ========== 스타터 프리셋 2단계 ==========

bool UOfficeManager::ApplyStarterPresetToData(FOfficeSaveData& OfficeData, ECompanyType Industry)
{
	if (OfficeData.PendingStarterPreset.IsNone())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return false;
	}

	bool bFound = false;
	const FOfficeStarterPresetRow Preset = TableMgr->GetStarterPresetRow(OfficeData.PendingStarterPreset, bFound);
	if (!bFound)
	{
		// 소비 후 재시도가 없으므로 책상 0 → 자리 0 → 그 건물 채용이 영구 차단된다. 조용히 넘기면 원인 추적 불가.
		UE_LOG(LogTemp, Error, TEXT("[OfficeManager] StarterPreset: 프리셋 행 '%s' 없음 — 책상 시드 없이 소비 처리. 이 건물은 책상을 직접 놓기 전까지 채용이 막힙니다 (DT_OfficeStarterPreset 정합 점검)"),
			*OfficeData.PendingStarterPreset.ToString());
		OfficeData.PendingStarterPreset = NAME_None;
		return false;
	}

	bool bPaletteFound = false;
	const FIndustryMoodPaletteRow Palette = TableMgr->GetIndustryMoodPalette(Industry, bPaletteFound);

	// 오피스 원점 = OfficeInterior 액터 트랜스폼 (타일/벽이 전부 이 액터의 상대 좌표)
	AOfficeInterior* Interior = Cast<AOfficeInterior>(
		UGameplayStatics::GetActorOfClass(World, AOfficeInterior::StaticClass()));
	FTransform Origin = Interior ? Interior->GetActorTransform() : FTransform::Identity;
	Origin.SetScale3D(FVector::OneVector); // 루트 스케일이 1이 아니어도 RelLocation/데코 크기가 스케일되지 않도록

	UCGGameInstance* CGI = Cast<UCGGameInstance>(GI);
	const int32 BuildingIndex = CGI ? CGI->GetCurrentManagedBuildingIndex() : 0;

	int32 Added = 0;
	for (int32 i = 0; i < Preset.Decorations.Num(); ++i)
	{
		const FStarterDecoEntry& Entry = Preset.Decorations[i];

		// 슬롯 치환 — 결정적 풀 선택 (BuildingIndex + 엔트리 인덱스)
		FName RowName = Entry.DecorationRowName;
		if (bPaletteFound && Entry.Slot != EStarterDecoSlot::None)
		{
			const TArray<FName>* Pool = nullptr;
			switch (Entry.Slot)
			{
			case EStarterDecoSlot::Picture: Pool = &Palette.PicturePool; break;
			case EStarterDecoSlot::Plant:   Pool = &Palette.PlantPool;   break;
			case EStarterDecoSlot::Accent:  Pool = &Palette.AccentPool;  break;
			default: break;
			}
			if (Pool && Pool->Num() > 0)
			{
				RowName = (*Pool)[FMath::Abs(BuildingIndex + i) % Pool->Num()];
			}
		}
		if (RowName.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] StarterPreset entry %d: RowName 없음 - 스킵"), i);
			continue;
		}

		bool bCardOk = false;
		const FDecorationCardTable Card = TableMgr->GetDecorationCardInfo(RowName, bCardOk);
		if (!bCardOk)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] StarterPreset entry %d: 미등록 데코 %s - 스킵"), i, *RowName.ToString());
			continue;
		}

		const FTransform Rel(FRotator(0.f, Entry.YawDeg, 0.f), Entry.RelLocation);
		OfficeData.PlacedDecorations.Add(FDecorationSaveData(RowName, Rel * Origin, Card.Category));
		++Added;
	}

	// 업무공간 시드 — 실제 기능하는 워크스테이션 (빈 의자 1개)
	// 산업 팔레트에 WorkstationPool이 있으면 책상 모양을 산업 시그니처로 치환
	// 방어: 풀/프리셋이 가리키는 행이 DT_WorkstationCard에서 삭제돼도 오피스가 텅 비지 않도록 3단 폴백 (Loud 경고)
	FName FallbackSingleRow = NAME_None; // DT 실재 첫 싱글 행 (지연 해석)
	int32 WsAdded = 0;
	for (int32 wi = 0; wi < Preset.Workstations.Num(); ++wi)
	{
		const FStarterWorkstationEntry& WsEntry = Preset.Workstations[wi];

		// 1순위: 산업 시그니처 풀 → 없으면 프리셋 기본행
		FName WsRow = WsEntry.WorkstationTypeID;
		if (bPaletteFound && Palette.WorkstationPool.Num() > 0)
		{
			WsRow = Palette.WorkstationPool[FMath::Abs(BuildingIndex + wi) % Palette.WorkstationPool.Num()];
		}

		bool bWsOk = false;
		if (!WsRow.IsNone())
		{
			TableMgr->GetWorkstationCardInfo(WsRow, bWsOk);
		}

		// 2순위: 풀이 죽은 행을 가리켰으면 프리셋 기본행으로 폴백
		if (!bWsOk && !WsEntry.WorkstationTypeID.IsNone() && WsEntry.WorkstationTypeID != WsRow)
		{
			TableMgr->GetWorkstationCardInfo(WsEntry.WorkstationTypeID, bWsOk);
			if (bWsOk)
			{
				UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] StarterPreset: 업무공간 '%s' 미등록 → 프리셋 기본행 '%s' 폴백 (DT 정합 점검)"),
					*WsRow.ToString(), *WsEntry.WorkstationTypeID.ToString());
				WsRow = WsEntry.WorkstationTypeID;
			}
		}

		// 3순위: 그래도 없으면 DT 실재 임의 싱글행 (모양은 달라도 책상은 반드시 시드)
		if (!bWsOk)
		{
			if (FallbackSingleRow.IsNone())
			{
				const TArray<FWorkstationCardTable> Singles = TableMgr->GetWorkstationCardsByType(EWorkstationType::Single);
				if (Singles.Num() > 0)
				{
					FallbackSingleRow = Singles[0].RowName;
				}
			}
			if (FallbackSingleRow.IsNone())
			{
				UE_LOG(LogTemp, Error, TEXT("[OfficeManager] StarterPreset: DT_WorkstationCard에 유효한 싱글 행이 없음 - 업무공간 시드 중단"));
				break;
			}
			UE_LOG(LogTemp, Error, TEXT("[OfficeManager] StarterPreset: 업무공간 '%s' 미등록 → 임의 싱글행 '%s' 폴백. 프리셋 DT와 DT_WorkstationCard 정합을 점검하세요."),
				*WsRow.ToString(), *FallbackSingleRow.ToString());
			WsRow = FallbackSingleRow;
			bWsOk = true;
		}

		FWorkstationSaveData WsSave;
		WsSave.WorkstationType = EWorkstationType::Single;
		WsSave.WorkstationTypeID = WsRow;
		WsSave.Transform = FTransform(FRotator(0.f, WsEntry.YawDeg, 0.f), WsEntry.RelLocation) * Origin;
		WsSave.ChairStates.SetNum(1); // 빈 의자 — WorkstationInfoWidget에서 배정
		OfficeData.OfficeWorkstations.Add(WsSave);
		++WsAdded;
	}

	// 바닥 타일 무드 시드 (이미 타일이 있으면 존중)
	if (bPaletteFound && OfficeData.CurrentFloorTileRowName.IsNone() && !Palette.FloorTileRow.IsNone())
	{
		OfficeData.CurrentFloorTileRowName = Palette.FloorTileRow;
		OfficeData.OwnedFloorTileRowNames.AddUnique(Palette.FloorTileRow);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] StarterPreset %s 적용: 데코 %d개 + 업무공간 %d개 (산업 %s)"),
		*OfficeData.PendingStarterPreset.ToString(), Added, WsAdded, *CompanyTypeToString(Industry));

	OfficeData.PendingStarterPreset = NAME_None;
	return true;
}

// ========== 기본 장식 스폰 ==========

void UOfficeManager::SpawnDefaultDecorations()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	// WD_13 창문 데이터 로드
	FName WindowRowName = FName(TEXT("WD_13"));
	bool bSuccess = false;
	FDecorationData DecData = TableMgr->GetDecorationData(WindowRowName, bSuccess);
	if (!bSuccess || DecData.DecorationMesh.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeManager] SpawnDefaultDecorations: Failed to load WD_13"));
		return;
	}

	// 카드 데이터에서 카테고리/표면 정보 가져오기
	bool bCardSuccess = false;
	FDecorationCardTable CardData = TableMgr->GetDecorationCardInfo(WindowRowName, bCardSuccess);

	// 오른쪽 벽 중앙 고정 좌표
	FVector Location(5.f, -400.f, 200.f);
	FRotator Rotation(0.f, 90.f, 0.f);
	FTransform SpawnTransform(Rotation, Location);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADecorationActor* NewDecoration = World->SpawnActor<ADecorationActor>(
		ADecorationActor::StaticClass(),
		SpawnTransform,
		SpawnParams);

	if (NewDecoration)
	{
		NewDecoration->DecorationCardTableRowName = WindowRowName;
		NewDecoration->Category = bCardSuccess ? CardData.Category : EDecorationCategory::Window;
		NewDecoration->AllowedSurface = bCardSuccess ? CardData.AllowedSurface : EDecorationSurface::Wall;

		UStaticMesh* Mesh = DecData.DecorationMesh.LoadSynchronous();
		if (Mesh)
		{
			NewDecoration->SetDecorationMesh(Mesh);
		}

		RegisterPlacedDecoration(NewDecoration);

		UE_LOG(LogTemp, Log, TEXT("[OfficeManager] SpawnDefaultDecorations: Placed default window WD_13"));
	}
}

// ========== Building 데이터 접근 헬퍼 ==========

FOfficeSaveData* UOfficeManager::GetCurrentOfficeSaveData() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(World->GetGameInstance());
	if (!GI)
	{
		return nullptr;
	}

	int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIndex < 0)
	{
		return nullptr;
	}

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr)
	{
		return nullptr;
	}

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData)
	{
		return nullptr;
	}

	// OfficeDataMap에서 현재 BuildingIndex로 조회
	return SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
}

// ========== 수익 저장 접근 ==========

float UOfficeManager::GetStoredRevenue() const
{
	if (const FOfficeSaveData* OfficeData = GetCurrentOfficeSaveData())
	{
		return OfficeData->StoredRevenue;
	}
	return 0.0f;
}

float UOfficeManager::GetStoredRevenueCapacity() const
{
	// 시간기반 단일 정본에 위임 — 기존엔 배율조차 없이 200 상수만 반환하던 숨은 버그(오피스 재실 적립이 금고강화 무시) 해소.
	if (UWorld* World = GetWorld())
	{
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(World->GetGameInstance()))
		{
			const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
			if (BuildingIndex >= 0)
			{
				if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
				{
					return OpMgr->CalculateWarehouseCapacity(BuildingIndex);
				}
			}
		}
	}
	return ABuildingBaseActor::BaseVaultCapacity;
}

float UOfficeManager::AddToStoredRevenue(float Amount)
{
	if (Amount <= 0.0f)
	{
		return 0.0f;
	}

	FOfficeSaveData* OfficeData = GetCurrentOfficeSaveData();
	if (!OfficeData)
	{
		return 0.0f;
	}

	float Capacity = GetStoredRevenueCapacity();
	float AvailableSpace = Capacity - OfficeData->StoredRevenue;
	float ActualAmount = FMath::Min(Amount, AvailableSpace);

	if (ActualAmount > 0.0f)
	{
		OfficeData->StoredRevenue += ActualAmount;
	}

	return ActualAmount;
}

float UOfficeManager::CollectStoredRevenue()
{
	FOfficeSaveData* OfficeData = GetCurrentOfficeSaveData();
	if (!OfficeData)
	{
		return 0.0f;
	}

	float CollectedAmount = OfficeData->StoredRevenue;
	OfficeData->StoredRevenue = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[OfficeManager] Collected stored revenue: %.0f"), CollectedAmount);

	return CollectedAmount;
}

bool UOfficeManager::IsStoredRevenueFull() const
{
	if (const FOfficeSaveData* OfficeData = GetCurrentOfficeSaveData())
	{
		return OfficeData->StoredRevenue >= GetStoredRevenueCapacity();
	}
	return false;
}

// ========== 착석/해제 헬퍼 ==========

bool UOfficeManager::SeatEmployeeAtWorkstation(AWorkstationActorBase* Workstation, int32 EmployeeID, bool bShouldSave)
{
	if (!Workstation || EmployeeID < 0)
	{
		return false;
	}

	const int32 EmptySlot = Workstation->FindEmptyAssignmentSlot();
	if (EmptySlot < 0)
	{
		return false;
	}
	if (!Workstation->AssignEmployeeToSeat(EmptySlot, EmployeeID))
	{
		return false;
	}

	UGameInstance* GI = GetWorld()->GetGameInstance();
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	FEmployeeInstance* Emp = EmpMgr ? EmpMgr->GetEmployeeData(EmployeeID) : nullptr;
	if (Emp)
	{
		Emp->bIsAssigned = true;
	}

	// 데이터만 갱신하면 리로드 전까지 워커가 안 보이므로 즉시 스폰+착석
	if (Emp)
	{
		if (AOfficeGameMode* OfficeGM = Cast<AOfficeGameMode>(GetWorld()->GetAuthGameMode()))
		{
			OfficeGM->SpawnWorkerForAssignment(*Emp, Workstation, EmptySlot);
		}
	}

	// ChairState 는 메모리 전용 — 명시 저장 (리로드 desync 방지)
	if (bShouldSave)
	{
		if (USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr)
		{
			SaveMgr->SaveGameData();
		}
	}

	if (UMissionManagerSubsystem* MissionMgr = GI ? GI->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->NotifyEmployeeSeated();
	}

	if (EmpMgr)
	{
		EmpMgr->OnEmployeeRosterChanged.Broadcast();
	}

	return true;
}

AWorkstationActorBase* UOfficeManager::AutoSeatEmployee(int32 EmployeeID, AWorkstationActorBase* PreferredWorkstation,
	bool bShouldSave)
{
	if (PreferredWorkstation && SeatEmployeeAtWorkstation(PreferredWorkstation, EmployeeID, bShouldSave))
	{
		return PreferredWorkstation;
	}

	// 셋업 레벨 높은 책상부터 (동률=배치순 유지) — 책상 보너스가 게임플레이화돼도 올바른 기본값
	// GC로 무효화된 엔트리 방어 — 포인터 배열 StableSort 는 역참조하므로 정렬 전에 걸러야 함
	TArray<AWorkstationActorBase*> Candidates;
	Candidates.Reserve(PlacedWorkstations.Num());
	for (AWorkstationActorBase* Placed : PlacedWorkstations)
	{
		if (IsValid(Placed))
		{
			Candidates.Add(Placed);
		}
	}
	Candidates.StableSort([](const AWorkstationActorBase& A, const AWorkstationActorBase& B)
	{
		return static_cast<int32>(A.GetCurrentSetupLevel()) > static_cast<int32>(B.GetCurrentSetupLevel());
	});

	for (AWorkstationActorBase* Candidate : Candidates)
	{
		if (Candidate && Candidate != PreferredWorkstation
			&& SeatEmployeeAtWorkstation(Candidate, EmployeeID, bShouldSave))
		{
			return Candidate;
		}
	}
	return nullptr;
}

bool UOfficeManager::UnseatEmployee(AWorkstationActorBase* Workstation, int32 EmployeeID)
{
	if (!Workstation || EmployeeID < 0)
	{
		return false;
	}

	// 좌석0 하드코딩 금지 — 실제 앉은 좌석을 스캔 해제 (더블 책상 대응, FireOccupant 와 동일 규약)
	bool bReleased = false;
	const int32 ChairCount = Workstation->GetChairCount();
	for (int32 SeatIdx = 0; SeatIdx < ChairCount; ++SeatIdx)
	{
		if (Workstation->GetAssignedEmployeeID(SeatIdx) == EmployeeID)
		{
			Workstation->UnassignSeat(SeatIdx);
			bReleased = true;
			break;
		}
	}
	if (!bReleased)
	{
		return false;
	}

	UGameInstance* GI = GetWorld()->GetGameInstance();

	// 벤치 복귀 — UnassignEmployee 는 소속 건물까지 리셋하므로 사용 금지 (건물 벤치 이탈 방지)
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (EmpMgr)
	{
		if (FEmployeeInstance* Emp = EmpMgr->GetEmployeeData(EmployeeID))
		{
			Emp->bIsAssigned = false;
		}
	}

	if (AOfficeGameMode* OfficeGM = Cast<AOfficeGameMode>(GetWorld()->GetAuthGameMode()))
	{
		OfficeGM->DespawnWorkerByID(EmployeeID, Workstation);
	}

	if (USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr)
	{
		SaveMgr->SaveGameData();
	}

	if (EmpMgr)
	{
		EmpMgr->OnEmployeeRosterChanged.Broadcast();
	}

	return true;
}

void UOfficeManager::SortBenchByOverallDesc(TArray<FEmployeeInstance>& InOutBench)
{
	InOutBench.StableSort([](const FEmployeeInstance& A, const FEmployeeInstance& B)
	{
		return UEmployeeStatsHelper::CalculateEffectiveOverall(A.Stats, A.EnhancementLevel)
			 > UEmployeeStatsHelper::CalculateEffectiveOverall(B.Stats, B.EnhancementLevel);
	});
}

TArray<int32> UOfficeManager::AutoSeatBenchedEmployees(AWorkstationActorBase* NewWorkstation)
{
	TArray<int32> Seated;
	if (!NewWorkstation || NewWorkstation->FindEmptyAssignmentSlot() < 0)
	{
		return Seated;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UCGGameInstance* CGI = Cast<UCGGameInstance>(GI);
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!CGI || !EmpMgr)
	{
		return Seated;
	}

	TArray<FEmployeeInstance> Bench = EmpMgr->GetUnassignedEmployeesInBuilding(CGI->GetCurrentManagedBuildingIndex());
	if (Bench.Num() == 0)
	{
		return Seated;
	}

	// 좋은 자원끼리 붙인다 — 채용 시 자동 착석이 셋업 레벨 높은 책상부터 고르는 것과 같은 정책
	SortBenchByOverallDesc(Bench);

	for (const FEmployeeInstance& Candidate : Bench)
	{
		if (NewWorkstation->FindEmptyAssignmentSlot() < 0)
		{
			break;
		}
		if (SeatEmployeeAtWorkstation(NewWorkstation, Candidate.EmployeeID, /*bShouldSave=*/false))
		{
			Seated.Add(Candidate.EmployeeID);
		}
	}

	if (Seated.Num() > 0)
	{
		// 좌석마다 저장하면 배치 한 번에 블로킹 세이브가 좌석 수만큼 돈다 — 루프가 끝난 뒤 한 번만
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}

		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			// 이름이 비면 " 사원이…" 가 되므로, 이름 문장 대신 인원수 문장으로 떨어진다
			const FEmployeeInstance* Emp = (Seated.Num() == 1) ? EmpMgr->GetEmployeeData(Seated[0]) : nullptr;
			const FText Message = (Emp && !Emp->EmployeeName.IsEmpty())
				? FText::Format(
					NSLOCTEXT("Office", "AutoSeatOne", "{0} 사원이 새 자리에 앉았습니다"),
					FText::FromString(Emp->EmployeeName))
				: FText::Format(
					NSLOCTEXT("Office", "AutoSeatMany", "대기 중이던 {0}명이 새 자리에 앉았습니다"),
					FText::AsNumber(Seated.Num()));
			UIMgr->ShowNotification(Message, 3.0f, ENotificationType::Success);
		}
	}

	return Seated;
}
