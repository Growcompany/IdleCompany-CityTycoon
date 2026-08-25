// Fill out your copyright notice in the Description page of Project Settings.

#include "Office/SingleWorkstationActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Table/WorkstationTable.h"

// 소켓 이름 상수 정의
const FName ASingleWorkstationActor::SocketName_Laptop_Center = TEXT("Socket_Laptop_Center");
const FName ASingleWorkstationActor::SocketName_Laptop_Side = TEXT("Socket_Laptop_Side");
const FName ASingleWorkstationActor::SocketName_Monitor_Single = TEXT("Socket_Monitor_Single");
const FName ASingleWorkstationActor::SocketName_Monitor_DualLeft = TEXT("Socket_Monitor_DualLeft");
const FName ASingleWorkstationActor::SocketName_Monitor_DualRight = TEXT("Socket_Monitor_DualRight");
const FName ASingleWorkstationActor::SocketName_Monitor_Vertical = TEXT("Socket_Monitor_Vertical");
const FName ASingleWorkstationActor::SocketName_Keyboard = TEXT("Socket_Keyboard");
const FName ASingleWorkstationActor::SocketName_Mouse = TEXT("Socket_Mouse");
const FName ASingleWorkstationActor::SocketName_Computer = TEXT("Socket_Computer");
const FName ASingleWorkstationActor::SocketName_Chair_0 = TEXT("Socket_Chair_0");
const FName ASingleWorkstationActor::SocketName_Chair_1 = TEXT("Socket_Chair_1");
const FName ASingleWorkstationActor::SocketName_Seat_0 = TEXT("Socket_Seat_0");
const FName ASingleWorkstationActor::SocketName_Seat_1 = TEXT("Socket_Seat_1");

ASingleWorkstationActor::ASingleWorkstationActor()
{
	// 의자 0 (싱글/더블 공용 - 첫 번째 의자)
	ChairSlot_0 = CreateDefaultSubobject<USceneComponent>(TEXT("ChairSlot_0"));
	ChairSlot_0->SetupAttachment(RootScene);
	ChairMesh_0 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh_0"));
	ChairMesh_0->SetupAttachment(ChairSlot_0);
	SeatPoint_0 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint_0"));
	SeatPoint_0->SetupAttachment(RootScene);

	// 의자 1 (더블 전용 - 두 번째 의자)
	ChairSlot_1 = CreateDefaultSubobject<USceneComponent>(TEXT("ChairSlot_1"));
	ChairSlot_1->SetupAttachment(RootScene);
	ChairMesh_1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh_1"));
	ChairMesh_1->SetupAttachment(ChairSlot_1);
	SeatPoint_1 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint_1"));
	SeatPoint_1->SetupAttachment(RootScene);

	// 노트북 중앙 슬롯 및 메시 (Lv1 - 노트북만 사용)
	LaptopSlot_Center = CreateDefaultSubobject<USceneComponent>(TEXT("LaptopSlot_Center"));
	LaptopSlot_Center->SetupAttachment(RootScene);
	LaptopMesh_Center = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaptopMesh_Center"));
	LaptopMesh_Center->SetupAttachment(LaptopSlot_Center);

	// 노트북 사이드 슬롯 및 메시 (Lv3A - Curved 모니터 옆)
	LaptopSlot_Side = CreateDefaultSubobject<USceneComponent>(TEXT("LaptopSlot_Side"));
	LaptopSlot_Side->SetupAttachment(RootScene);
	LaptopMesh_Side = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaptopMesh_Side"));
	LaptopMesh_Side->SetupAttachment(LaptopSlot_Side);

	// 싱글 모니터 슬롯 및 메시 (중앙 위치 - Lv2, Lv3A, Lv3C의 Curved용)
	MonitorSlot_Single = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_Single"));
	MonitorSlot_Single->SetupAttachment(RootScene);
	MonitorMesh_Single = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_Single"));
	MonitorMesh_Single->SetupAttachment(MonitorSlot_Single);

	// Curved+Curved (Lv3B) 왼쪽 슬롯 및 메시
	MonitorSlot_DualLeft = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_DualLeft"));
	MonitorSlot_DualLeft->SetupAttachment(RootScene);
	MonitorMesh_DualLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_DualLeft"));
	MonitorMesh_DualLeft->SetupAttachment(MonitorSlot_DualLeft);

	// Curved+Curved (Lv3B) 오른쪽 슬롯 및 메시
	MonitorSlot_DualRight = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_DualRight"));
	MonitorSlot_DualRight->SetupAttachment(RootScene);
	MonitorMesh_DualRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_DualRight"));
	MonitorMesh_DualRight->SetupAttachment(MonitorSlot_DualRight);

	// Vertical 모니터 슬롯 및 메시 (Lv3C의 Curved+Vertical용)
	MonitorSlot_Vertical = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_Vertical"));
	MonitorSlot_Vertical->SetupAttachment(RootScene);
	MonitorMesh_Vertical = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_Vertical"));
	MonitorMesh_Vertical->SetupAttachment(MonitorSlot_Vertical);

	// 키보드 슬롯 및 메시
	KeyboardSlot = CreateDefaultSubobject<USceneComponent>(TEXT("KeyboardSlot"));
	KeyboardSlot->SetupAttachment(RootScene);
	KeyboardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyboardMesh"));
	KeyboardMesh->SetupAttachment(KeyboardSlot);

	// 마우스 슬롯 및 메시
	MouseSlot = CreateDefaultSubobject<USceneComponent>(TEXT("MouseSlot"));
	MouseSlot->SetupAttachment(RootScene);
	MouseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MouseMesh"));
	MouseMesh->SetupAttachment(MouseSlot);

	// 본체 슬롯 및 메시
	ComputerSlot = CreateDefaultSubobject<USceneComponent>(TEXT("ComputerSlot"));
	ComputerSlot->SetupAttachment(RootScene);
	ComputerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ComputerMesh"));
	ComputerMesh->SetupAttachment(ComputerSlot);

	// 액세서리 슬롯 1
	AccessorySlot1 = CreateDefaultSubobject<USceneComponent>(TEXT("AccessorySlot1"));
	AccessorySlot1->SetupAttachment(RootScene);
	AccessoryMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccessoryMesh1"));
	AccessoryMesh1->SetupAttachment(AccessorySlot1);

	// 액세서리 슬롯 2
	AccessorySlot2 = CreateDefaultSubobject<USceneComponent>(TEXT("AccessorySlot2"));
	AccessorySlot2->SetupAttachment(RootScene);
	AccessoryMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccessoryMesh2"));
	AccessoryMesh2->SetupAttachment(AccessorySlot2);
}

void ASingleWorkstationActor::BeginPlay()
{
	Super::BeginPlay();
	// Super::BeginPlay()에서 InitializeWorkstation() 호출됨
}

// ========== 초기화 ==========

void ASingleWorkstationActor::InitializeWorkstation()
{
	// 부모 초기화 먼저 호출
	Super::InitializeWorkstation();

	// WorkstationType에 따라 의자 1 활성화/비활성화
	if (ChairMesh_1)
	{
		ChairMesh_1->SetVisibility(WorkstationType == EWorkstationType::Double);
	}

	// 초기 레벨 적용
	ApplySetupLevel();

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Initialized: %s (ID: %s, Type: %s, Level: %d)"),
		*WorkstationTypeID.ToString(), *InstanceID,
		(WorkstationType == EWorkstationType::Double) ? TEXT("Double") : TEXT("Single"),
		static_cast<int32>(CurrentSetupLevel));
}

void ASingleWorkstationActor::RestoreFromSaveData(const FWorkstationSaveData& SaveData)
{
	// 부모 복원 먼저 호출
	Super::RestoreFromSaveData(SaveData);

	// 의자 상태 복원 (TArray에서 고정 변수로)
	if (SaveData.ChairStates.Num() > 0)
	{
		ChairState_0 = SaveData.ChairStates[0];
	}
	if (SaveData.ChairStates.Num() > 1)
	{
		ChairState_1 = SaveData.ChairStates[1];
	}

	// 레벨 적용은 Super 가 가상 디스패치로 이미 수행 — 여기서 또 부르면 복원된 슬롯 스킨을 메시째 덮어쓴다

	// 의자 스킨 복원
	if (!ChairState_0.CurrentSkinID.IsNone())
	{
		SetChairSkin(0, ChairState_0.CurrentSkinID);
	}
	if (GetChairCount() >= 2 && !ChairState_1.CurrentSkinID.IsNone())
	{
		SetChairSkin(1, ChairState_1.CurrentSkinID);
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Restored: %s (Level: %d)"),
		*WorkstationTypeID.ToString(), static_cast<int32>(CurrentSetupLevel));
}

FWorkstationSaveData ASingleWorkstationActor::GetSaveData() const
{
	// 부모 데이터 먼저 가져오기
	FWorkstationSaveData SaveData = Super::GetSaveData();

	// 의자 상태 저장 (GetChairCount() 기반으로 유효한 의자만 저장)
	int32 Count = GetChairCount();
	SaveData.ChairStates.SetNum(Count);
	SaveData.ChairStates[0] = ChairState_0;
	if (Count > 1)
	{
		SaveData.ChairStates[1] = ChairState_1;
	}

	return SaveData;
}

void ASingleWorkstationActor::ApplySetupLevel()
{
	if (ChairMesh_1)
	{
		ChairMesh_1->SetVisibility(WorkstationType == EWorkstationType::Double);
	}

	UStaticMeshComponent* ManagedMeshes[] =
	{
		LaptopMesh_Center, LaptopMesh_Side,
		MonitorMesh_Single, MonitorMesh_DualLeft, MonitorMesh_DualRight, MonitorMesh_Vertical,
		KeyboardMesh, MouseMesh, ComputerMesh
	};
	for (UStaticMeshComponent* MeshComponent : ManagedMeshes)
	{
		if (MeshComponent)
		{
			MeshComponent->SetVisibility(false);
			MeshComponent->SetStaticMesh(nullptr);
		}
	}

	const FComputerSetupLevelData* LevelData = FindSetupLevelData(CurrentSetupLevel);
	FString ValidationError;
	if (!LevelData || !ValidateAndLoadSetupMeshes(*LevelData, ValidationError))
	{
		for (TPair<EWorkstationSlot, FSlotSkinState>& Pair : SlotStates)
		{
			Pair.Value.bIsActive = false;
		}

		CurrentMonitor1Type = EMonitorType::None;
		CurrentMonitor2Type = EMonitorType::None;
		UE_LOG(LogTemp, Error, TEXT("[SingleWorkstationActor] Cannot apply Level%d: %s"),
			static_cast<int32>(CurrentSetupLevel) + 1,
			LevelData ? *ValidationError : TEXT("row missing"));
		return;
	}

	UStaticMesh* Laptop = LevelData->LaptopPlacement != ELaptopPlacement::None
		? LevelData->LaptopMesh.Get()
		: nullptr;
	UStaticMeshComponent* ActiveLaptopComponent = LevelData->LaptopPlacement == ELaptopPlacement::Center
		? LaptopMesh_Center
		: LevelData->LaptopPlacement == ELaptopPlacement::Side ? LaptopMesh_Side : nullptr;
	if (ActiveLaptopComponent)
	{
		ActiveLaptopComponent->SetStaticMesh(Laptop);
		ActiveLaptopComponent->SetVisibility(true);
	}

	if (FSlotSkinState* State = SlotStates.Find(EWorkstationSlot::Laptop))
	{
		State->bIsActive = ActiveLaptopComponent != nullptr;
	}

	auto ApplyOptionalMesh = [](UStaticMeshComponent* MeshComponent, UStaticMesh* Mesh, bool bActive)
	{
		if (MeshComponent && bActive)
		{
			MeshComponent->SetStaticMesh(Mesh);
			MeshComponent->SetVisibility(true);
		}
	};

	const bool bPrimaryMonitorActive = LevelData->PrimaryMonitorType != EPrimaryMonitorType::None;
	const bool bTwinLayout = LevelData->SecondMonitorType == ESecondMonitorType::Curved;
	if (bTwinLayout)
	{
		// 구 Lv3B 그대로 — 주모니터가 중앙(Single)이 아니라 좌우 두 자리로 간다
		ApplyOptionalMesh(MonitorMesh_DualLeft, LevelData->PrimaryMonitorMesh.Get(), true);
		ApplyOptionalMesh(MonitorMesh_DualRight, LevelData->DualMonitorMesh.Get(), true);
	}
	else
	{
		ApplyOptionalMesh(MonitorMesh_Single, LevelData->PrimaryMonitorMesh.Get(), bPrimaryMonitorActive);
		ApplyOptionalMesh(MonitorMesh_Vertical, LevelData->VerticalMonitorMesh.Get(),
			LevelData->SecondMonitorType == ESecondMonitorType::Vertical);
	}
	ApplyOptionalMesh(KeyboardMesh, LevelData->KeyboardMesh.Get(), LevelData->bKeyboardActive);
	ApplyOptionalMesh(MouseMesh, LevelData->MouseMesh.Get(), LevelData->bMouseActive);
	ApplyOptionalMesh(ComputerMesh, LevelData->ComputerMesh.Get(), LevelData->bComputerActive);

	if (FSlotSkinState* State = SlotStates.Find(EWorkstationSlot::Monitor)) State->bIsActive = bPrimaryMonitorActive;
	if (FSlotSkinState* State = SlotStates.Find(EWorkstationSlot::Keyboard)) State->bIsActive = LevelData->bKeyboardActive;
	if (FSlotSkinState* State = SlotStates.Find(EWorkstationSlot::Mouse)) State->bIsActive = LevelData->bMouseActive;
	if (FSlotSkinState* State = SlotStates.Find(EWorkstationSlot::Computer)) State->bIsActive = LevelData->bComputerActive;

	CurrentMonitor1Type = LevelData->PrimaryMonitorType == EPrimaryMonitorType::Flat
		? EMonitorType::Flat
		: LevelData->PrimaryMonitorType == EPrimaryMonitorType::Curved ? EMonitorType::Curved : EMonitorType::None;
	CurrentMonitor2Type = LevelData->SecondMonitorType == ESecondMonitorType::Vertical
		? EMonitorType::Vertical
		: LevelData->SecondMonitorType == ESecondMonitorType::Curved ? EMonitorType::Curved : EMonitorType::None;

	AttachEquipmentToSockets();
	RecalcBoxExtent();

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Applied setup Level%d"),
		static_cast<int32>(CurrentSetupLevel) + 1);
}

// ========== 스킨 설정 ==========

bool ASingleWorkstationActor::SetSlotSkin(EWorkstationSlot Slot, FName SkinID)
{
	// 의자는 별도 함수 사용
	if (Slot == EWorkstationSlot::Chair)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Use SetChairSkin for chairs"));
		return false;
	}

	// 슬롯 활성화 확인
	if (!IsSlotActive(Slot))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Slot %d is not active"), static_cast<int32>(Slot));
		return false;
	}

	// 스킨 데이터 찾기
	FWorkstationItemSkinData* SkinData = FindSkinData(SkinID, Slot);
	if (!SkinData)
	{
		UE_LOG(LogTemp, Error, TEXT("[SingleWorkstationActor] Skin not found: %s"), *SkinID.ToString());
		return false;
	}

	// 모니터의 경우 타입 확인
	if (Slot == EWorkstationSlot::Monitor)
	{
		if (SkinData->MonitorType != CurrentMonitor1Type)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Monitor type mismatch. Required: %d, Skin: %d"),
				static_cast<int32>(CurrentMonitor1Type), static_cast<int32>(SkinData->MonitorType));
			return false;
		}
	}

	// 메시 적용
	if (UStaticMesh* Mesh = SkinData->Mesh.LoadSynchronous())
	{
		if (Slot == EWorkstationSlot::Monitor)
		{
			// 모니터의 경우 4개 메시 모두에 적용 (보여지는 것만 런타임에 결정됨)
			if (MonitorMesh_Single) MonitorMesh_Single->SetStaticMesh(Mesh);
			if (MonitorMesh_DualLeft) MonitorMesh_DualLeft->SetStaticMesh(Mesh);
			if (MonitorMesh_DualRight) MonitorMesh_DualRight->SetStaticMesh(Mesh);
			if (MonitorMesh_Vertical) MonitorMesh_Vertical->SetStaticMesh(Mesh);
		}
		else if (Slot == EWorkstationSlot::Laptop)
		{
			// 노트북의 경우 2개 메시 모두에 적용 (보여지는 것만 런타임에 결정됨)
			if (LaptopMesh_Center) LaptopMesh_Center->SetStaticMesh(Mesh);
			if (LaptopMesh_Side) LaptopMesh_Side->SetStaticMesh(Mesh);
		}
		else
		{
			SetSlotMesh(Slot, Mesh);
		}
	}

	// 상태 업데이트
	if (FSlotSkinState* State = SlotStates.Find(Slot))
	{
		State->CurrentSkinID = SkinID;
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Slot %d skin set to: %s"),
		static_cast<int32>(Slot), *SkinID.ToString());

	return true;
}

bool ASingleWorkstationActor::IsSlotActive(EWorkstationSlot Slot) const
{
	if (Slot == EWorkstationSlot::Chair)
	{
		return GetChairCount() > 0;
	}

	if (const FSlotSkinState* State = SlotStates.Find(Slot))
	{
		return State->bIsActive;
	}
	return false;
}

void ASingleWorkstationActor::SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh)
{
	if (UStaticMeshComponent* MeshComp = GetMeshComponentForSlot(Slot))
	{
		MeshComp->SetStaticMesh(NewMesh);
	}
}

void ASingleWorkstationActor::SetSlotVisibility(EWorkstationSlot Slot, bool bVisible)
{
	// 모니터와 노트북은 ApplySetupLevel에서 레벨에 따라 별도 처리함
	// 여기서는 다른 슬롯만 처리
	if (Slot == EWorkstationSlot::Monitor)
	{
		// 모니터 Visibility는 ApplySetupLevel에서 싱글/듀얼에 따라 처리
		// 여기서는 아무것도 하지 않음 (호출 무시)
		return;
	}

	if (Slot == EWorkstationSlot::Laptop)
	{
		// 노트북 Visibility는 ApplySetupLevel에서 중앙/사이드에 따라 처리
		// 여기서는 아무것도 하지 않음 (호출 무시)
		return;
	}

	if (UStaticMeshComponent* MeshComp = GetMeshComponentForSlot(Slot))
	{
		MeshComp->SetVisibility(bVisible);
	}
}

UStaticMeshComponent* ASingleWorkstationActor::GetMeshComponentForSlot(EWorkstationSlot Slot) const
{
	switch (Slot)
	{
	case EWorkstationSlot::Laptop:
		return LaptopMesh_Center && LaptopMesh_Center->IsVisible() ? LaptopMesh_Center : LaptopMesh_Side;
	case EWorkstationSlot::Monitor:
		return MonitorMesh_Single;
	case EWorkstationSlot::Keyboard:
		return KeyboardMesh;
	case EWorkstationSlot::Mouse:
		return MouseMesh;
	case EWorkstationSlot::Computer:
		return ComputerMesh;
	case EWorkstationSlot::Accessory1:
		return AccessoryMesh1;
	case EWorkstationSlot::Accessory2:
		return AccessoryMesh2;
	default:
		return nullptr;
	}
}

// ========== 의자 슬롯 관리 ==========

void ASingleWorkstationActor::SetChairMesh(int32 ChairIndex, UStaticMesh* NewMesh)
{
	// 인덱스 유효성 검사 (0 또는 1만 허용)
	if (ChairIndex < 0 || ChairIndex > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Invalid chair index: %d (must be 0 or 1)"), ChairIndex);
		return;
	}

	// ChairCount 검사
	if (ChairIndex >= GetChairCount())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Chair index %d exceeds ChairCount %d"), ChairIndex, GetChairCount());
		return;
	}

	UStaticMeshComponent* TargetMesh = (ChairIndex == 0) ? ChairMesh_0 : ChairMesh_1;
	if (TargetMesh)
	{
		TargetMesh->SetStaticMesh(NewMesh);
	}
}

bool ASingleWorkstationActor::SetChairSkin(int32 ChairIndex, FName SkinID)
{
	// 인덱스 유효성 검사 (0 또는 1만 허용)
	if (ChairIndex < 0 || ChairIndex > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Invalid chair index: %d (must be 0 or 1)"), ChairIndex);
		return false;
	}

	// ChairCount 검사
	if (ChairIndex >= GetChairCount())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Chair index %d exceeds ChairCount %d"), ChairIndex, GetChairCount());
		return false;
	}

	FWorkstationItemSkinData* SkinData = FindSkinData(SkinID, EWorkstationSlot::Chair);
	if (!SkinData)
	{
		UE_LOG(LogTemp, Error, TEXT("[SingleWorkstationActor] Chair skin not found: %s"), *SkinID.ToString());
		return false;
	}

	if (UStaticMesh* Mesh = SkinData->Mesh.LoadSynchronous())
	{
		SetChairMesh(ChairIndex, Mesh);
	}

	// 상태 업데이트
	if (ChairIndex == 0)
	{
		ChairState_0.CurrentSkinID = SkinID;
	}
	else
	{
		ChairState_1.CurrentSkinID = SkinID;
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Chair %d skin set to: %s"), ChairIndex, *SkinID.ToString());

	return true;
}

void ASingleWorkstationActor::SetWorkstationType(EWorkstationType NewType)
{
	WorkstationType = NewType;

	// 의자 1 활성화/비활성화 적용
	if (ChairMesh_1)
	{
		ChairMesh_1->SetVisibility(WorkstationType == EWorkstationType::Double);
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] WorkstationType set to: %s"),
		(WorkstationType == EWorkstationType::Double) ? TEXT("Double") : TEXT("Single"));
}

bool ASingleWorkstationActor::SetAllChairsSkin(FName SkinID)
{
	bool bSuccess = true;
	for (int32 i = 0; i < GetChairCount(); ++i)
	{
		if (!SetChairSkin(i, SkinID))
		{
			bSuccess = false;
		}
	}
	return bSuccess;
}

FName ASingleWorkstationActor::GetChairSkinID(int32 ChairIndex) const
{
	if (ChairIndex == 0)
	{
		return ChairState_0.CurrentSkinID;
	}
	else if (ChairIndex == 1)
	{
		return ChairState_1.CurrentSkinID;
	}
	return NAME_None;
}

// ========== 착석 시스템 ==========

bool ASingleWorkstationActor::HasEmptySeat() const
{
	// 첫 번째 의자 체크
	if (!Occupant_0)
	{
		return true;
	}
	// ChairCount가 2인 경우 두 번째 의자 체크
	if (GetChairCount() >= 2 && !Occupant_1)
	{
		return true;
	}
	return false;
}

bool ASingleWorkstationActor::CanSitAt(int32 SeatIndex) const
{
	// 인덱스 유효성 검사
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return false;
	}
	// ChairCount 검사
	if (SeatIndex >= GetChairCount())
	{
		return false;
	}
	// 이미 착석한 직원 확인
	if (SeatIndex == 0)
	{
		return Occupant_0 == nullptr;
	}
	else
	{
		return Occupant_1 == nullptr;
	}
}

int32 ASingleWorkstationActor::SitDown(AOfficeworker* Worker)
{
	if (!Worker)
	{
		UE_LOG(LogTemp, Error, TEXT("[SingleWorkstationActor] SitDown: Worker is NULL"));
		return -1;
	}

	// 빈 자리 찾기
	for (int32 i = 0; i < GetChairCount(); ++i)
	{
		if (CanSitAt(i))
		{
			if (SitDownAt(i, Worker))
			{
				return i;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] No empty seat available"));
	return -1;
}

bool ASingleWorkstationActor::SitDownAt(int32 SeatIndex, AOfficeworker* Worker)
{
	if (!CanSitAt(SeatIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] Cannot sit at seat %d"), SeatIndex);
		return false;
	}

	if (!Worker)
	{
		UE_LOG(LogTemp, Error, TEXT("[SingleWorkstationActor] SitDownAt: Worker is NULL"));
		return false;
	}

	// 착석 처리
	if (SeatIndex == 0)
	{
		Occupant_0 = Worker;
	}
	else
	{
		Occupant_1 = Worker;
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Worker %s sat at seat %d"), *Worker->GetName(), SeatIndex);

	return true;
}

void ASingleWorkstationActor::StandUp(AOfficeworker* Worker)
{
	if (!Worker)
	{
		return;
	}

	int32 SeatIndex = FindWorkerSeatIndex(Worker);
	if (SeatIndex >= 0)
	{
		if (SeatIndex == 0)
		{
			Occupant_0 = nullptr;
		}
		else
		{
			Occupant_1 = nullptr;
		}
		UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Worker %s stood up from seat %d"), *Worker->GetName(), SeatIndex);
	}
}

FVector ASingleWorkstationActor::GetSeatLocation(int32 SeatIndex) const
{
	if (SeatIndex == 0 && SeatPoint_0)
	{
		return SeatPoint_0->GetComponentLocation();
	}
	else if (SeatIndex == 1 && SeatPoint_1)
	{
		return SeatPoint_1->GetComponentLocation();
	}
	return GetActorLocation();
}

FRotator ASingleWorkstationActor::GetSeatRotation(int32 SeatIndex) const
{
	if (SeatIndex == 0 && SeatPoint_0)
	{
		return SeatPoint_0->GetComponentRotation();
	}
	else if (SeatIndex == 1 && SeatPoint_1)
	{
		return SeatPoint_1->GetComponentRotation();
	}
	return GetActorRotation();
}

int32 ASingleWorkstationActor::FindWorkerSeatIndex(AOfficeworker* Worker) const
{
	if (!Worker)
	{
		return -1;
	}

	if (Occupant_0 == Worker)
	{
		return 0;
	}
	if (GetChairCount() >= 2 && Occupant_1 == Worker)
	{
		return 1;
	}
	return -1;
}

int32 ASingleWorkstationActor::GetOccupantCount() const
{
	int32 Count = 0;
	if (Occupant_0)
	{
		++Count;
	}
	if (GetChairCount() >= 2 && Occupant_1)
	{
		++Count;
	}
	return Count;
}

AOfficeworker* ASingleWorkstationActor::GetOccupantAt(int32 SeatIndex) const
{
	if (SeatIndex == 0)
	{
		return Occupant_0;
	}
	else if (SeatIndex == 1 && GetChairCount() >= 2)
	{
		return Occupant_1;
	}
	return nullptr;
}

// ========== Socket 기반 배치 ==========

void ASingleWorkstationActor::AttachEquipmentToSockets()
{
	if (!DeskMesh || !DeskMesh->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] DeskMesh not set, cannot attach to sockets"));
		return;
	}

	// 의자 0 소켓 부착
	if (ChairSlot_0)
	{
		AttachToSocket(ChairSlot_0, SocketName_Chair_0);
	}
	// SeatPoint_0는 BP에서 직접 위치 설정 (소켓 Snap 안 함)

	// 의자 1 소켓 부착
	if (ChairSlot_1)
	{
		AttachToSocket(ChairSlot_1, SocketName_Chair_1);
	}
	// SeatPoint_1는 BP에서 직접 위치 설정 (소켓 Snap 안 함)

	// 노트북 중앙 (Lv1)
	if (LaptopSlot_Center)
	{
		AttachToSocket(LaptopSlot_Center, SocketName_Laptop_Center);
	}

	// 노트북 사이드 (Lv3A)
	if (LaptopSlot_Side)
	{
		AttachToSocket(LaptopSlot_Side, SocketName_Laptop_Side);
	}

	// 모니터: 4개 위치 모두 각각의 소켓에 부착
	// 싱글 모니터 (중앙) - Lv2, Lv3A, Lv3C의 Curved
	if (MonitorSlot_Single)
	{
		AttachToSocket(MonitorSlot_Single, SocketName_Monitor_Single);
	}

	// Curved+Curved (Lv3B) 왼쪽
	if (MonitorSlot_DualLeft)
	{
		AttachToSocket(MonitorSlot_DualLeft, SocketName_Monitor_DualLeft);
	}

	// Curved+Curved (Lv3B) 오른쪽
	if (MonitorSlot_DualRight)
	{
		AttachToSocket(MonitorSlot_DualRight, SocketName_Monitor_DualRight);
	}

	// Vertical 모니터 (Lv3C)
	if (MonitorSlot_Vertical)
	{
		AttachToSocket(MonitorSlot_Vertical, SocketName_Monitor_Vertical);
	}

	// 키보드
	if (KeyboardSlot)
	{
		AttachToSocket(KeyboardSlot, SocketName_Keyboard);
	}

	// 마우스
	if (MouseSlot)
	{
		AttachToSocket(MouseSlot, SocketName_Mouse);
	}

	// 본체
	if (ComputerSlot)
	{
		AttachToSocket(ComputerSlot, SocketName_Computer);
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Equipment attached to sockets"));
}

// ========== 직원 배정 시스템 ==========

bool ASingleWorkstationActor::AssignEmployeeToSeat(int32 SeatIndex, int32 EmployeeID)
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] AssignEmployeeToSeat: Invalid seat index %d"), SeatIndex);
		return false;
	}

	if (SeatIndex >= GetChairCount())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SingleWorkstationActor] AssignEmployeeToSeat: Seat %d exceeds ChairCount %d"), SeatIndex, GetChairCount());
		return false;
	}

	if (SeatIndex == 0)
	{
		ChairState_0.OccupantEmployeeID = EmployeeID;
	}
	else
	{
		ChairState_1.OccupantEmployeeID = EmployeeID;
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Assigned EmployeeID %d to seat %d"), EmployeeID, SeatIndex);
	return true;
}

void ASingleWorkstationActor::UnassignSeat(int32 SeatIndex)
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return;
	}

	if (SeatIndex == 0)
	{
		ChairState_0.OccupantEmployeeID = -1;
	}
	else
	{
		ChairState_1.OccupantEmployeeID = -1;
	}

	UE_LOG(LogTemp, Log, TEXT("[SingleWorkstationActor] Unassigned seat %d"), SeatIndex);
}

int32 ASingleWorkstationActor::GetAssignedEmployeeID(int32 SeatIndex) const
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return -1;
	}

	if (SeatIndex >= GetChairCount())
	{
		return -1;
	}

	if (SeatIndex == 0)
	{
		return ChairState_0.OccupantEmployeeID;
	}
	else
	{
		return ChairState_1.OccupantEmployeeID;
	}
}

int32 ASingleWorkstationActor::FindEmptyAssignmentSlot() const
{
	for (int32 i = 0; i < GetChairCount(); ++i)
	{
		if (GetAssignedEmployeeID(i) == -1)
		{
			return i;
		}
	}
	return -1;
}
