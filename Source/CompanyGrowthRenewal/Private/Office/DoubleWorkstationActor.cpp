// Fill out your copyright notice in the Description page of Project Settings.

#include "Office/DoubleWorkstationActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Table/WorkstationTable.h"

// 소켓 이름 상수 정의 - 좌석 0
const FName ADoubleWorkstationActor::SocketName_Chair_0 = TEXT("Socket_Chair_0");
const FName ADoubleWorkstationActor::SocketName_Seat_0 = TEXT("Socket_Seat_0");
const FName ADoubleWorkstationActor::SocketName_Laptop_0_Center = TEXT("Socket_Laptop_0_Center");
const FName ADoubleWorkstationActor::SocketName_Laptop_0_Side = TEXT("Socket_Laptop_0_Side");
const FName ADoubleWorkstationActor::SocketName_Monitor_0_Single = TEXT("Socket_Monitor_0_Single");
const FName ADoubleWorkstationActor::SocketName_Monitor_0_DualLeft = TEXT("Socket_Monitor_0_DualLeft");
const FName ADoubleWorkstationActor::SocketName_Monitor_0_DualRight = TEXT("Socket_Monitor_0_DualRight");
const FName ADoubleWorkstationActor::SocketName_Monitor_0_Vertical = TEXT("Socket_Monitor_0_Vertical");
const FName ADoubleWorkstationActor::SocketName_Keyboard_0 = TEXT("Socket_Keyboard_0");
const FName ADoubleWorkstationActor::SocketName_Mouse_0 = TEXT("Socket_Mouse_0");
const FName ADoubleWorkstationActor::SocketName_Computer_0 = TEXT("Socket_Computer_0");

// 소켓 이름 상수 정의 - 좌석 1
const FName ADoubleWorkstationActor::SocketName_Chair_1 = TEXT("Socket_Chair_1");
const FName ADoubleWorkstationActor::SocketName_Seat_1 = TEXT("Socket_Seat_1");
const FName ADoubleWorkstationActor::SocketName_Laptop_1_Center = TEXT("Socket_Laptop_1_Center");
const FName ADoubleWorkstationActor::SocketName_Laptop_1_Side = TEXT("Socket_Laptop_1_Side");
const FName ADoubleWorkstationActor::SocketName_Monitor_1_Single = TEXT("Socket_Monitor_1_Single");
const FName ADoubleWorkstationActor::SocketName_Monitor_1_DualLeft = TEXT("Socket_Monitor_1_DualLeft");
const FName ADoubleWorkstationActor::SocketName_Monitor_1_DualRight = TEXT("Socket_Monitor_1_DualRight");
const FName ADoubleWorkstationActor::SocketName_Monitor_1_Vertical = TEXT("Socket_Monitor_1_Vertical");
const FName ADoubleWorkstationActor::SocketName_Keyboard_1 = TEXT("Socket_Keyboard_1");
const FName ADoubleWorkstationActor::SocketName_Mouse_1 = TEXT("Socket_Mouse_1");
const FName ADoubleWorkstationActor::SocketName_Computer_1 = TEXT("Socket_Computer_1");

ADoubleWorkstationActor::ADoubleWorkstationActor()
{
	// ========== 좌석 0 컴포넌트 생성 ==========

	// 의자 0
	ChairSlot_0 = CreateDefaultSubobject<USceneComponent>(TEXT("ChairSlot_0"));
	ChairSlot_0->SetupAttachment(RootScene);
	ChairMesh_0 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh_0"));
	ChairMesh_0->SetupAttachment(ChairSlot_0);
	SeatPoint_0 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint_0"));
	SeatPoint_0->SetupAttachment(RootScene);

	// 노트북 0 중앙
	LaptopSlot_0_Center = CreateDefaultSubobject<USceneComponent>(TEXT("LaptopSlot_0_Center"));
	LaptopSlot_0_Center->SetupAttachment(RootScene);
	LaptopMesh_0_Center = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaptopMesh_0_Center"));
	LaptopMesh_0_Center->SetupAttachment(LaptopSlot_0_Center);

	// 노트북 0 사이드
	LaptopSlot_0_Side = CreateDefaultSubobject<USceneComponent>(TEXT("LaptopSlot_0_Side"));
	LaptopSlot_0_Side->SetupAttachment(RootScene);
	LaptopMesh_0_Side = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaptopMesh_0_Side"));
	LaptopMesh_0_Side->SetupAttachment(LaptopSlot_0_Side);

	// 모니터 0 싱글
	MonitorSlot_0_Single = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_0_Single"));
	MonitorSlot_0_Single->SetupAttachment(RootScene);
	MonitorMesh_0_Single = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_0_Single"));
	MonitorMesh_0_Single->SetupAttachment(MonitorSlot_0_Single);

	// 모니터 0 듀얼 왼쪽
	MonitorSlot_0_DualLeft = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_0_DualLeft"));
	MonitorSlot_0_DualLeft->SetupAttachment(RootScene);
	MonitorMesh_0_DualLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_0_DualLeft"));
	MonitorMesh_0_DualLeft->SetupAttachment(MonitorSlot_0_DualLeft);

	// 모니터 0 듀얼 오른쪽
	MonitorSlot_0_DualRight = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_0_DualRight"));
	MonitorSlot_0_DualRight->SetupAttachment(RootScene);
	MonitorMesh_0_DualRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_0_DualRight"));
	MonitorMesh_0_DualRight->SetupAttachment(MonitorSlot_0_DualRight);

	// 모니터 0 버티컬
	MonitorSlot_0_Vertical = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_0_Vertical"));
	MonitorSlot_0_Vertical->SetupAttachment(RootScene);
	MonitorMesh_0_Vertical = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_0_Vertical"));
	MonitorMesh_0_Vertical->SetupAttachment(MonitorSlot_0_Vertical);

	// 키보드 0
	KeyboardSlot_0 = CreateDefaultSubobject<USceneComponent>(TEXT("KeyboardSlot_0"));
	KeyboardSlot_0->SetupAttachment(RootScene);
	KeyboardMesh_0 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyboardMesh_0"));
	KeyboardMesh_0->SetupAttachment(KeyboardSlot_0);

	// 마우스 0
	MouseSlot_0 = CreateDefaultSubobject<USceneComponent>(TEXT("MouseSlot_0"));
	MouseSlot_0->SetupAttachment(RootScene);
	MouseMesh_0 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MouseMesh_0"));
	MouseMesh_0->SetupAttachment(MouseSlot_0);

	// 본체 0
	ComputerSlot_0 = CreateDefaultSubobject<USceneComponent>(TEXT("ComputerSlot_0"));
	ComputerSlot_0->SetupAttachment(RootScene);
	ComputerMesh_0 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ComputerMesh_0"));
	ComputerMesh_0->SetupAttachment(ComputerSlot_0);

	// ========== 좌석 1 컴포넌트 생성 ==========

	// 의자 1
	ChairSlot_1 = CreateDefaultSubobject<USceneComponent>(TEXT("ChairSlot_1"));
	ChairSlot_1->SetupAttachment(RootScene);
	ChairMesh_1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh_1"));
	ChairMesh_1->SetupAttachment(ChairSlot_1);
	SeatPoint_1 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint_1"));
	SeatPoint_1->SetupAttachment(RootScene);

	// 노트북 1 중앙
	LaptopSlot_1_Center = CreateDefaultSubobject<USceneComponent>(TEXT("LaptopSlot_1_Center"));
	LaptopSlot_1_Center->SetupAttachment(RootScene);
	LaptopMesh_1_Center = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaptopMesh_1_Center"));
	LaptopMesh_1_Center->SetupAttachment(LaptopSlot_1_Center);

	// 노트북 1 사이드
	LaptopSlot_1_Side = CreateDefaultSubobject<USceneComponent>(TEXT("LaptopSlot_1_Side"));
	LaptopSlot_1_Side->SetupAttachment(RootScene);
	LaptopMesh_1_Side = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaptopMesh_1_Side"));
	LaptopMesh_1_Side->SetupAttachment(LaptopSlot_1_Side);

	// 모니터 1 싱글
	MonitorSlot_1_Single = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_1_Single"));
	MonitorSlot_1_Single->SetupAttachment(RootScene);
	MonitorMesh_1_Single = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_1_Single"));
	MonitorMesh_1_Single->SetupAttachment(MonitorSlot_1_Single);

	// 모니터 1 듀얼 왼쪽
	MonitorSlot_1_DualLeft = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_1_DualLeft"));
	MonitorSlot_1_DualLeft->SetupAttachment(RootScene);
	MonitorMesh_1_DualLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_1_DualLeft"));
	MonitorMesh_1_DualLeft->SetupAttachment(MonitorSlot_1_DualLeft);

	// 모니터 1 듀얼 오른쪽
	MonitorSlot_1_DualRight = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_1_DualRight"));
	MonitorSlot_1_DualRight->SetupAttachment(RootScene);
	MonitorMesh_1_DualRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_1_DualRight"));
	MonitorMesh_1_DualRight->SetupAttachment(MonitorSlot_1_DualRight);

	// 모니터 1 버티컬
	MonitorSlot_1_Vertical = CreateDefaultSubobject<USceneComponent>(TEXT("MonitorSlot_1_Vertical"));
	MonitorSlot_1_Vertical->SetupAttachment(RootScene);
	MonitorMesh_1_Vertical = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh_1_Vertical"));
	MonitorMesh_1_Vertical->SetupAttachment(MonitorSlot_1_Vertical);

	// 키보드 1
	KeyboardSlot_1 = CreateDefaultSubobject<USceneComponent>(TEXT("KeyboardSlot_1"));
	KeyboardSlot_1->SetupAttachment(RootScene);
	KeyboardMesh_1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyboardMesh_1"));
	KeyboardMesh_1->SetupAttachment(KeyboardSlot_1);

	// 마우스 1
	MouseSlot_1 = CreateDefaultSubobject<USceneComponent>(TEXT("MouseSlot_1"));
	MouseSlot_1->SetupAttachment(RootScene);
	MouseMesh_1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MouseMesh_1"));
	MouseMesh_1->SetupAttachment(MouseSlot_1);

	// 본체 1
	ComputerSlot_1 = CreateDefaultSubobject<USceneComponent>(TEXT("ComputerSlot_1"));
	ComputerSlot_1->SetupAttachment(RootScene);
	ComputerMesh_1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ComputerMesh_1"));
	ComputerMesh_1->SetupAttachment(ComputerSlot_1);
}

void ADoubleWorkstationActor::BeginPlay()
{
	Super::BeginPlay();
}

// ========== 초기화 ==========

void ADoubleWorkstationActor::InitializeWorkstation()
{
	Super::InitializeWorkstation();

	// 좌석별 SlotStates 초기화
	SlotStates_0.Empty();
	SlotStates_1.Empty();

	// 각 슬롯 타입에 대해 초기 상태 설정
	for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(EWorkstationSlot::Max); ++SlotIdx)
	{
		EWorkstationSlot Slot = static_cast<EWorkstationSlot>(SlotIdx);
		if (Slot == EWorkstationSlot::Chair || Slot == EWorkstationSlot::Max)
		{
			continue;
		}

		FSlotSkinState DefaultState;
		DefaultState.bIsActive = false;
		DefaultState.CurrentSkinID = NAME_None;

		SlotStates_0.Add(Slot, DefaultState);
		SlotStates_1.Add(Slot, DefaultState);
	}

	// 초기 레벨 적용
	ApplySetupLevel();

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Initialized: %s (ID: %s, Level: %d)"),
		*WorkstationTypeID.ToString(), *InstanceID, static_cast<int32>(CurrentSetupLevel));
}

void ADoubleWorkstationActor::RestoreFromSaveData(const FWorkstationSaveData& SaveData)
{
	Super::RestoreFromSaveData(SaveData);

	// 의자 상태 복원
	if (SaveData.ChairStates.Num() > 0)
	{
		ChairState_0 = SaveData.ChairStates[0];
	}
	if (SaveData.ChairStates.Num() > 1)
	{
		ChairState_1 = SaveData.ChairStates[1];
	}

	// 레벨 적용은 Super 가 가상 디스패치로 이미 수행 — 재호출은 메시 초기화/nav 재등록만 중복시킨다

	// 의자 스킨 복원
	if (!ChairState_0.CurrentSkinID.IsNone())
	{
		SetChairSkin(0, ChairState_0.CurrentSkinID);
	}
	if (!ChairState_1.CurrentSkinID.IsNone())
	{
		SetChairSkin(1, ChairState_1.CurrentSkinID);
	}

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Restored: %s (Level: %d)"),
		*WorkstationTypeID.ToString(), static_cast<int32>(CurrentSetupLevel));
}

FWorkstationSaveData ADoubleWorkstationActor::GetSaveData() const
{
	FWorkstationSaveData SaveData = Super::GetSaveData();

	// 의자 상태 저장
	SaveData.ChairStates.SetNum(2);
	SaveData.ChairStates[0] = ChairState_0;
	SaveData.ChairStates[1] = ChairState_1;

	// 배정된 직원 EmployeeID 저장 (ChairState에 이미 저장되어 있음)
	SaveData.ChairStates[0].OccupantEmployeeID = ChairState_0.OccupantEmployeeID;
	SaveData.ChairStates[1].OccupantEmployeeID = ChairState_1.OccupantEmployeeID;

	return SaveData;
}

// ========== 세팅 레벨 적용 ==========

void ADoubleWorkstationActor::ApplySetupLevel()
{
	ApplySetupLevelForSeat(0);
	ApplySetupLevelForSeat(1);

	if (const FComputerSetupLevelData* LevelData = FindSetupLevelData(CurrentSetupLevel))
	{
		CurrentMonitor1Type = LevelData->PrimaryMonitorType == EPrimaryMonitorType::Flat
			? EMonitorType::Flat
			: LevelData->PrimaryMonitorType == EPrimaryMonitorType::Curved ? EMonitorType::Curved : EMonitorType::None;
		CurrentMonitor2Type = LevelData->SecondMonitorType == ESecondMonitorType::Vertical
			? EMonitorType::Vertical
			: LevelData->SecondMonitorType == ESecondMonitorType::Curved ? EMonitorType::Curved : EMonitorType::None;
	}
	else
	{
		CurrentMonitor1Type = EMonitorType::None;
		CurrentMonitor2Type = EMonitorType::None;
	}

	AttachEquipmentToSockets();
	RecalcBoxExtent();

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Applied Level%d to both seats"),
		static_cast<int32>(CurrentSetupLevel) + 1);
}

void ADoubleWorkstationActor::ApplySetupLevelForSeat(int32 SeatIndex)
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return;
	}

	const FComputerSetupLevelData* LevelData = FindSetupLevelData(CurrentSetupLevel);
	TMap<EWorkstationSlot, FSlotSkinState>& SeatSlotStates = GetSlotStatesForSeat(SeatIndex);

	// 좌석별 메시 컴포넌트 참조
	UStaticMeshComponent* LaptopMeshCenter = (SeatIndex == 0) ? LaptopMesh_0_Center : LaptopMesh_1_Center;
	UStaticMeshComponent* LaptopMeshSide = (SeatIndex == 0) ? LaptopMesh_0_Side : LaptopMesh_1_Side;
	UStaticMeshComponent* MonitorMeshSingle = (SeatIndex == 0) ? MonitorMesh_0_Single : MonitorMesh_1_Single;
	UStaticMeshComponent* MonitorMeshDualLeft = (SeatIndex == 0) ? MonitorMesh_0_DualLeft : MonitorMesh_1_DualLeft;
	UStaticMeshComponent* MonitorMeshDualRight = (SeatIndex == 0) ? MonitorMesh_0_DualRight : MonitorMesh_1_DualRight;
	UStaticMeshComponent* MonitorMeshVertical = (SeatIndex == 0) ? MonitorMesh_0_Vertical : MonitorMesh_1_Vertical;
	UStaticMeshComponent* KeyboardMeshComp = (SeatIndex == 0) ? KeyboardMesh_0 : KeyboardMesh_1;
	UStaticMeshComponent* MouseMeshComp = (SeatIndex == 0) ? MouseMesh_0 : MouseMesh_1;
	UStaticMeshComponent* ComputerMeshComp = (SeatIndex == 0) ? ComputerMesh_0 : ComputerMesh_1;

	UStaticMeshComponent* ManagedMeshes[] =
	{
		LaptopMeshCenter, LaptopMeshSide,
		MonitorMeshSingle, MonitorMeshDualLeft, MonitorMeshDualRight, MonitorMeshVertical,
		KeyboardMeshComp, MouseMeshComp, ComputerMeshComp
	};
	for (UStaticMeshComponent* MeshComponent : ManagedMeshes)
	{
		if (MeshComponent)
		{
			MeshComponent->SetVisibility(false);
			MeshComponent->SetStaticMesh(nullptr);
		}
	}

	FString ValidationError;
	if (!LevelData || !ValidateAndLoadSetupMeshes(*LevelData, ValidationError))
	{
		for (TPair<EWorkstationSlot, FSlotSkinState>& Pair : SeatSlotStates)
		{
			Pair.Value.bIsActive = false;
		}
		UE_LOG(LogTemp, Error, TEXT("[DoubleWorkstationActor] Seat %d cannot apply Level%d: %s"),
			SeatIndex, static_cast<int32>(CurrentSetupLevel) + 1,
			LevelData ? *ValidationError : TEXT("row missing"));
		return;
	}

	UStaticMeshComponent* ActiveLaptopComponent = LevelData->LaptopPlacement == ELaptopPlacement::Center
		? LaptopMeshCenter
		: LevelData->LaptopPlacement == ELaptopPlacement::Side ? LaptopMeshSide : nullptr;
	if (ActiveLaptopComponent)
	{
		ActiveLaptopComponent->SetStaticMesh(LevelData->LaptopMesh.Get());
		ActiveLaptopComponent->SetVisibility(true);
	}
	if (FSlotSkinState* State = SeatSlotStates.Find(EWorkstationSlot::Laptop))
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
	if (LevelData->SecondMonitorType == ESecondMonitorType::Curved)
	{
		// 구 Lv3B 그대로 — 주모니터가 중앙(Single)이 아니라 좌우 두 자리로 간다
		ApplyOptionalMesh(MonitorMeshDualLeft, LevelData->PrimaryMonitorMesh.Get(), true);
		ApplyOptionalMesh(MonitorMeshDualRight, LevelData->DualMonitorMesh.Get(), true);
	}
	else
	{
		ApplyOptionalMesh(MonitorMeshSingle, LevelData->PrimaryMonitorMesh.Get(), bPrimaryMonitorActive);
		ApplyOptionalMesh(MonitorMeshVertical, LevelData->VerticalMonitorMesh.Get(),
			LevelData->SecondMonitorType == ESecondMonitorType::Vertical);
	}
	ApplyOptionalMesh(KeyboardMeshComp, LevelData->KeyboardMesh.Get(), LevelData->bKeyboardActive);
	ApplyOptionalMesh(MouseMeshComp, LevelData->MouseMesh.Get(), LevelData->bMouseActive);
	ApplyOptionalMesh(ComputerMeshComp, LevelData->ComputerMesh.Get(), LevelData->bComputerActive);

	if (FSlotSkinState* State = SeatSlotStates.Find(EWorkstationSlot::Monitor)) State->bIsActive = bPrimaryMonitorActive;
	if (FSlotSkinState* State = SeatSlotStates.Find(EWorkstationSlot::Keyboard)) State->bIsActive = LevelData->bKeyboardActive;
	if (FSlotSkinState* State = SeatSlotStates.Find(EWorkstationSlot::Mouse)) State->bIsActive = LevelData->bMouseActive;
	if (FSlotSkinState* State = SeatSlotStates.Find(EWorkstationSlot::Computer)) State->bIsActive = LevelData->bComputerActive;
}

// ========== 슬롯 상태 헬퍼 ==========

TMap<EWorkstationSlot, FSlotSkinState>& ADoubleWorkstationActor::GetSlotStatesForSeat(int32 SeatIndex)
{
	return (SeatIndex == 0) ? SlotStates_0 : SlotStates_1;
}

const TMap<EWorkstationSlot, FSlotSkinState>& ADoubleWorkstationActor::GetSlotStatesForSeat(int32 SeatIndex) const
{
	return (SeatIndex == 0) ? SlotStates_0 : SlotStates_1;
}

// ========== 스킨 설정 ==========

bool ADoubleWorkstationActor::SetSlotSkin(EWorkstationSlot Slot, FName SkinID)
{
	// 의자는 별도 함수 사용
	if (Slot == EWorkstationSlot::Chair)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Use SetChairSkin for chairs"));
		return false;
	}

	// 양쪽 좌석 모두에 적용
	bool bSuccess = true;
	bSuccess &= SetSlotSkinForSeat(0, Slot, SkinID);
	bSuccess &= SetSlotSkinForSeat(1, Slot, SkinID);

	return bSuccess;
}

bool ADoubleWorkstationActor::SetSlotSkinForSeat(int32 SeatIndex, EWorkstationSlot Slot, FName SkinID)
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Invalid seat index: %d"), SeatIndex);
		return false;
	}

	if (Slot == EWorkstationSlot::Chair)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Use SetChairSkin for chairs"));
		return false;
	}

	if (!IsSlotActiveForSeat(SeatIndex, Slot))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Slot %d is not active for seat %d"),
			static_cast<int32>(Slot), SeatIndex);
		return false;
	}

	FWorkstationItemSkinData* SkinData = FindSkinData(SkinID, Slot);
	if (!SkinData)
	{
		UE_LOG(LogTemp, Error, TEXT("[DoubleWorkstationActor] Skin not found: %s"), *SkinID.ToString());
		return false;
	}

	// 모니터의 경우 타입 확인
	if (Slot == EWorkstationSlot::Monitor)
	{
		if (SkinData->MonitorType != CurrentMonitor1Type)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Monitor type mismatch. Required: %d, Skin: %d"),
				static_cast<int32>(CurrentMonitor1Type), static_cast<int32>(SkinData->MonitorType));
			return false;
		}
	}

	// 메시 적용
	if (UStaticMesh* Mesh = SkinData->Mesh.LoadSynchronous())
	{
		if (Slot == EWorkstationSlot::Monitor)
		{
			// 모니터의 경우 4개 메시 모두에 적용
			if (SeatIndex == 0)
			{
				if (MonitorMesh_0_Single) MonitorMesh_0_Single->SetStaticMesh(Mesh);
				if (MonitorMesh_0_DualLeft) MonitorMesh_0_DualLeft->SetStaticMesh(Mesh);
				if (MonitorMesh_0_DualRight) MonitorMesh_0_DualRight->SetStaticMesh(Mesh);
				if (MonitorMesh_0_Vertical) MonitorMesh_0_Vertical->SetStaticMesh(Mesh);
			}
			else
			{
				if (MonitorMesh_1_Single) MonitorMesh_1_Single->SetStaticMesh(Mesh);
				if (MonitorMesh_1_DualLeft) MonitorMesh_1_DualLeft->SetStaticMesh(Mesh);
				if (MonitorMesh_1_DualRight) MonitorMesh_1_DualRight->SetStaticMesh(Mesh);
				if (MonitorMesh_1_Vertical) MonitorMesh_1_Vertical->SetStaticMesh(Mesh);
			}
		}
		else if (Slot == EWorkstationSlot::Laptop)
		{
			// 노트북의 경우 2개 메시 모두에 적용
			if (SeatIndex == 0)
			{
				if (LaptopMesh_0_Center) LaptopMesh_0_Center->SetStaticMesh(Mesh);
				if (LaptopMesh_0_Side) LaptopMesh_0_Side->SetStaticMesh(Mesh);
			}
			else
			{
				if (LaptopMesh_1_Center) LaptopMesh_1_Center->SetStaticMesh(Mesh);
				if (LaptopMesh_1_Side) LaptopMesh_1_Side->SetStaticMesh(Mesh);
			}
		}
		else
		{
			SetSlotMeshForSeat(SeatIndex, Slot, Mesh);
		}
	}

	// 상태 업데이트
	TMap<EWorkstationSlot, FSlotSkinState>& SeatSlotStates = GetSlotStatesForSeat(SeatIndex);
	if (FSlotSkinState* State = SeatSlotStates.Find(Slot))
	{
		State->CurrentSkinID = SkinID;
	}

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Seat %d Slot %d skin set to: %s"),
		SeatIndex, static_cast<int32>(Slot), *SkinID.ToString());

	return true;
}

bool ADoubleWorkstationActor::IsSlotActive(EWorkstationSlot Slot) const
{
	// 어느 좌석이든 활성화되어 있으면 true
	return IsSlotActiveForSeat(0, Slot) || IsSlotActiveForSeat(1, Slot);
}

bool ADoubleWorkstationActor::IsSlotActiveForSeat(int32 SeatIndex, EWorkstationSlot Slot) const
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return false;
	}

	if (Slot == EWorkstationSlot::Chair)
	{
		return true;
	}

	const TMap<EWorkstationSlot, FSlotSkinState>& SeatSlotStates = GetSlotStatesForSeat(SeatIndex);
	if (const FSlotSkinState* State = SeatSlotStates.Find(Slot))
	{
		return State->bIsActive;
	}
	return false;
}

FName ADoubleWorkstationActor::GetSlotSkinIDForSeat(int32 SeatIndex, EWorkstationSlot Slot) const
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return NAME_None;
	}

	const TMap<EWorkstationSlot, FSlotSkinState>& SeatSlotStates = GetSlotStatesForSeat(SeatIndex);
	if (const FSlotSkinState* State = SeatSlotStates.Find(Slot))
	{
		return State->CurrentSkinID;
	}
	return NAME_None;
}

void ADoubleWorkstationActor::SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh)
{
	// 양쪽 좌석 모두에 적용
	SetSlotMeshForSeat(0, Slot, NewMesh);
	SetSlotMeshForSeat(1, Slot, NewMesh);
}

void ADoubleWorkstationActor::SetSlotMeshForSeat(int32 SeatIndex, EWorkstationSlot Slot, UStaticMesh* NewMesh)
{
	if (UStaticMeshComponent* MeshComp = GetMeshComponentForSlotAndSeat(SeatIndex, Slot))
	{
		MeshComp->SetStaticMesh(NewMesh);
	}
}

void ADoubleWorkstationActor::SetSlotVisibility(EWorkstationSlot Slot, bool bVisible)
{
	// 양쪽 좌석 모두에 적용
	SetSlotVisibilityForSeat(0, Slot, bVisible);
	SetSlotVisibilityForSeat(1, Slot, bVisible);
}

void ADoubleWorkstationActor::SetSlotVisibilityForSeat(int32 SeatIndex, EWorkstationSlot Slot, bool bVisible)
{
	// 모니터와 노트북은 ApplySetupLevel에서 레벨에 따라 별도 처리함
	if (Slot == EWorkstationSlot::Monitor || Slot == EWorkstationSlot::Laptop)
	{
		return;
	}

	if (UStaticMeshComponent* MeshComp = GetMeshComponentForSlotAndSeat(SeatIndex, Slot))
	{
		MeshComp->SetVisibility(bVisible);
	}
}

UStaticMeshComponent* ADoubleWorkstationActor::GetMeshComponentForSlot(EWorkstationSlot Slot) const
{
	// 기본적으로 좌석 0의 컴포넌트 반환
	return GetMeshComponentForSlotAndSeat(0, Slot);
}

UStaticMeshComponent* ADoubleWorkstationActor::GetMeshComponentForSlotAndSeat(int32 SeatIndex, EWorkstationSlot Slot) const
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return nullptr;
	}

	if (SeatIndex == 0)
	{
		switch (Slot)
		{
		case EWorkstationSlot::Laptop:
			return LaptopMesh_0_Center && LaptopMesh_0_Center->IsVisible()
				? LaptopMesh_0_Center : LaptopMesh_0_Side;
		case EWorkstationSlot::Monitor:
			return MonitorMesh_0_Single;
		case EWorkstationSlot::Keyboard:
			return KeyboardMesh_0;
		case EWorkstationSlot::Mouse:
			return MouseMesh_0;
		case EWorkstationSlot::Computer:
			return ComputerMesh_0;
		default:
			return nullptr;
		}
	}
	else // SeatIndex == 1
	{
		switch (Slot)
		{
		case EWorkstationSlot::Laptop:
			return LaptopMesh_1_Center && LaptopMesh_1_Center->IsVisible()
				? LaptopMesh_1_Center : LaptopMesh_1_Side;
		case EWorkstationSlot::Monitor:
			return MonitorMesh_1_Single;
		case EWorkstationSlot::Keyboard:
			return KeyboardMesh_1;
		case EWorkstationSlot::Mouse:
			return MouseMesh_1;
		case EWorkstationSlot::Computer:
			return ComputerMesh_1;
		default:
			return nullptr;
		}
	}
}

// ========== 의자 슬롯 관리 ==========

void ADoubleWorkstationActor::SetChairMesh(int32 ChairIndex, UStaticMesh* NewMesh)
{
	if (ChairIndex < 0 || ChairIndex > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Invalid chair index: %d"), ChairIndex);
		return;
	}

	UStaticMeshComponent* TargetMesh = (ChairIndex == 0) ? ChairMesh_0 : ChairMesh_1;
	if (TargetMesh)
	{
		TargetMesh->SetStaticMesh(NewMesh);
	}
}

bool ADoubleWorkstationActor::SetChairSkin(int32 ChairIndex, FName SkinID)
{
	if (ChairIndex < 0 || ChairIndex > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Invalid chair index: %d"), ChairIndex);
		return false;
	}

	FWorkstationItemSkinData* SkinData = FindSkinData(SkinID, EWorkstationSlot::Chair);
	if (!SkinData)
	{
		UE_LOG(LogTemp, Error, TEXT("[DoubleWorkstationActor] Chair skin not found: %s"), *SkinID.ToString());
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

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Chair %d skin set to: %s"), ChairIndex, *SkinID.ToString());

	return true;
}

bool ADoubleWorkstationActor::SetAllChairsSkin(FName SkinID)
{
	bool bSuccess = true;
	bSuccess &= SetChairSkin(0, SkinID);
	bSuccess &= SetChairSkin(1, SkinID);
	return bSuccess;
}

FName ADoubleWorkstationActor::GetChairSkinID(int32 ChairIndex) const
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

bool ADoubleWorkstationActor::HasEmptySeat() const
{
	return !Occupant_0 || !Occupant_1;
}

bool ADoubleWorkstationActor::CanSitAt(int32 SeatIndex) const
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		return false;
	}

	if (SeatIndex == 0)
	{
		return Occupant_0 == nullptr;
	}
	else
	{
		return Occupant_1 == nullptr;
	}
}

int32 ADoubleWorkstationActor::SitDown(AOfficeworker* Worker)
{
	if (!Worker)
	{
		UE_LOG(LogTemp, Error, TEXT("[DoubleWorkstationActor] SitDown: Worker is NULL"));
		return -1;
	}

	// 빈 자리 찾기
	for (int32 i = 0; i < 2; ++i)
	{
		if (CanSitAt(i))
		{
			if (SitDownAt(i, Worker))
			{
				return i;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] No empty seat available"));
	return -1;
}

bool ADoubleWorkstationActor::SitDownAt(int32 SeatIndex, AOfficeworker* Worker)
{
	if (!CanSitAt(SeatIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] Cannot sit at seat %d"), SeatIndex);
		return false;
	}

	if (!Worker)
	{
		UE_LOG(LogTemp, Error, TEXT("[DoubleWorkstationActor] SitDownAt: Worker is NULL"));
		return false;
	}

	if (SeatIndex == 0)
	{
		Occupant_0 = Worker;
	}
	else
	{
		Occupant_1 = Worker;
	}

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Worker %s sat at seat %d"), *Worker->GetName(), SeatIndex);

	return true;
}

void ADoubleWorkstationActor::StandUp(AOfficeworker* Worker)
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
		UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Worker %s stood up from seat %d"), *Worker->GetName(), SeatIndex);
	}
}

FVector ADoubleWorkstationActor::GetSeatLocation(int32 SeatIndex) const
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

FRotator ADoubleWorkstationActor::GetSeatRotation(int32 SeatIndex) const
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

int32 ADoubleWorkstationActor::FindWorkerSeatIndex(AOfficeworker* Worker) const
{
	if (!Worker)
	{
		return -1;
	}

	if (Occupant_0 == Worker)
	{
		return 0;
	}
	if (Occupant_1 == Worker)
	{
		return 1;
	}
	return -1;
}

int32 ADoubleWorkstationActor::GetOccupantCount() const
{
	int32 Count = 0;
	if (Occupant_0)
	{
		++Count;
	}
	if (Occupant_1)
	{
		++Count;
	}
	return Count;
}

AOfficeworker* ADoubleWorkstationActor::GetOccupantAt(int32 SeatIndex) const
{
	if (SeatIndex == 0)
	{
		return Occupant_0;
	}
	else if (SeatIndex == 1)
	{
		return Occupant_1;
	}
	return nullptr;
}

// ========== Socket 기반 배치 ==========

void ADoubleWorkstationActor::AttachEquipmentToSockets()
{
	if (!DeskMesh || !DeskMesh->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] DeskMesh not set, cannot attach to sockets"));
		return;
	}

	// ========== 좌석 0 소켓 부착 ==========

	if (ChairSlot_0) AttachToSocket(ChairSlot_0, SocketName_Chair_0);
	if (SeatPoint_0) AttachToSocket(SeatPoint_0, SocketName_Seat_0);
	if (LaptopSlot_0_Center) AttachToSocket(LaptopSlot_0_Center, SocketName_Laptop_0_Center);
	if (LaptopSlot_0_Side) AttachToSocket(LaptopSlot_0_Side, SocketName_Laptop_0_Side);
	if (MonitorSlot_0_Single) AttachToSocket(MonitorSlot_0_Single, SocketName_Monitor_0_Single);
	if (MonitorSlot_0_DualLeft) AttachToSocket(MonitorSlot_0_DualLeft, SocketName_Monitor_0_DualLeft);
	if (MonitorSlot_0_DualRight) AttachToSocket(MonitorSlot_0_DualRight, SocketName_Monitor_0_DualRight);
	if (MonitorSlot_0_Vertical) AttachToSocket(MonitorSlot_0_Vertical, SocketName_Monitor_0_Vertical);
	if (KeyboardSlot_0) AttachToSocket(KeyboardSlot_0, SocketName_Keyboard_0);
	if (MouseSlot_0) AttachToSocket(MouseSlot_0, SocketName_Mouse_0);
	if (ComputerSlot_0) AttachToSocket(ComputerSlot_0, SocketName_Computer_0);

	// ========== 좌석 1 소켓 부착 ==========

	if (ChairSlot_1) AttachToSocket(ChairSlot_1, SocketName_Chair_1);
	if (SeatPoint_1) AttachToSocket(SeatPoint_1, SocketName_Seat_1);
	if (LaptopSlot_1_Center) AttachToSocket(LaptopSlot_1_Center, SocketName_Laptop_1_Center);
	if (LaptopSlot_1_Side) AttachToSocket(LaptopSlot_1_Side, SocketName_Laptop_1_Side);
	if (MonitorSlot_1_Single) AttachToSocket(MonitorSlot_1_Single, SocketName_Monitor_1_Single);
	if (MonitorSlot_1_DualLeft) AttachToSocket(MonitorSlot_1_DualLeft, SocketName_Monitor_1_DualLeft);
	if (MonitorSlot_1_DualRight) AttachToSocket(MonitorSlot_1_DualRight, SocketName_Monitor_1_DualRight);
	if (MonitorSlot_1_Vertical) AttachToSocket(MonitorSlot_1_Vertical, SocketName_Monitor_1_Vertical);
	if (KeyboardSlot_1) AttachToSocket(KeyboardSlot_1, SocketName_Keyboard_1);
	if (MouseSlot_1) AttachToSocket(MouseSlot_1, SocketName_Mouse_1);
	if (ComputerSlot_1) AttachToSocket(ComputerSlot_1, SocketName_Computer_1);

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Equipment attached to sockets for both seats"));
}

// ========== 직원 배정 시스템 ==========

bool ADoubleWorkstationActor::AssignEmployeeToSeat(int32 SeatIndex, int32 EmployeeID)
{
	if (SeatIndex < 0 || SeatIndex > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleWorkstationActor] AssignEmployeeToSeat: Invalid seat index %d"), SeatIndex);
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

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Assigned EmployeeID %d to seat %d"), EmployeeID, SeatIndex);
	return true;
}

void ADoubleWorkstationActor::UnassignSeat(int32 SeatIndex)
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

	UE_LOG(LogTemp, Log, TEXT("[DoubleWorkstationActor] Unassigned seat %d"), SeatIndex);
}

int32 ADoubleWorkstationActor::GetAssignedEmployeeID(int32 SeatIndex) const
{
	if (SeatIndex < 0 || SeatIndex > 1)
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

int32 ADoubleWorkstationActor::FindEmptyAssignmentSlot() const
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
