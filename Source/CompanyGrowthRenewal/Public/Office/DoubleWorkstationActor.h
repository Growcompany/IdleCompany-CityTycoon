// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Office/WorkstationActorBase.h"
#include "DoubleWorkstationActor.generated.h"

class UDataTable;
class AOfficeworker;

/**
 * 더블 업무공간 액터 - 2인용 책상+의자+컴퓨터 통합 가구
 *
 * 각 좌석(0, 1)마다 독립적인 컴퓨터 장비 세트 보유
 *
 * 구조:
 * Double 책상 (양쪽에 좌석)
 * ├── Seat 0 (왼쪽)
 * │   ├── 의자
 * │   ├── 노트북 (2위치: Center, Side)
 * │   ├── 모니터 (4위치: Single, DualLeft, DualRight, Vertical)
 * │   ├── 키보드
 * │   ├── 마우스
 * │   └── 본체
 * └── Seat 1 (오른쪽)
 *     ├── 의자
 *     ├── 노트북 (2위치: Center, Side)
 *     ├── 모니터 (4위치: Single, DualLeft, DualRight, Vertical)
 *     ├── 키보드
 *     ├── 마우스
 *     └── 본체
 */
UCLASS(Blueprintable)
class COMPANYGROWTHRENEWAL_API ADoubleWorkstationActor : public AWorkstationActorBase
{
	GENERATED_BODY()

public:
	ADoubleWorkstationActor();

protected:
	virtual void BeginPlay() override;

public:
	// ========== 좌석 0 컴포넌트 - 의자 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Chair")
	USceneComponent* ChairSlot_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Chair")
	UStaticMeshComponent* ChairMesh_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Chair")
	USceneComponent* SeatPoint_0;

	// ========== 좌석 0 컴포넌트 - 노트북 (2위치) ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Laptop")
	USceneComponent* LaptopSlot_0_Center;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Laptop")
	UStaticMeshComponent* LaptopMesh_0_Center;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Laptop")
	USceneComponent* LaptopSlot_0_Side;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Laptop")
	UStaticMeshComponent* LaptopMesh_0_Side;

	// ========== 좌석 0 컴포넌트 - 모니터 (4위치) ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	USceneComponent* MonitorSlot_0_Single;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	UStaticMeshComponent* MonitorMesh_0_Single;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	USceneComponent* MonitorSlot_0_DualLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	UStaticMeshComponent* MonitorMesh_0_DualLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	USceneComponent* MonitorSlot_0_DualRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	UStaticMeshComponent* MonitorMesh_0_DualRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	USceneComponent* MonitorSlot_0_Vertical;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Monitor")
	UStaticMeshComponent* MonitorMesh_0_Vertical;

	// ========== 좌석 0 컴포넌트 - 키보드/마우스/본체 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Keyboard")
	USceneComponent* KeyboardSlot_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Keyboard")
	UStaticMeshComponent* KeyboardMesh_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Mouse")
	USceneComponent* MouseSlot_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Mouse")
	UStaticMeshComponent* MouseMesh_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Computer")
	USceneComponent* ComputerSlot_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat0|Computer")
	UStaticMeshComponent* ComputerMesh_0;

	// ========== 좌석 1 컴포넌트 - 의자 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Chair")
	USceneComponent* ChairSlot_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Chair")
	UStaticMeshComponent* ChairMesh_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Chair")
	USceneComponent* SeatPoint_1;

	// ========== 좌석 1 컴포넌트 - 노트북 (2위치) ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Laptop")
	USceneComponent* LaptopSlot_1_Center;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Laptop")
	UStaticMeshComponent* LaptopMesh_1_Center;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Laptop")
	USceneComponent* LaptopSlot_1_Side;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Laptop")
	UStaticMeshComponent* LaptopMesh_1_Side;

	// ========== 좌석 1 컴포넌트 - 모니터 (4위치) ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	USceneComponent* MonitorSlot_1_Single;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	UStaticMeshComponent* MonitorMesh_1_Single;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	USceneComponent* MonitorSlot_1_DualLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	UStaticMeshComponent* MonitorMesh_1_DualLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	USceneComponent* MonitorSlot_1_DualRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	UStaticMeshComponent* MonitorMesh_1_DualRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	USceneComponent* MonitorSlot_1_Vertical;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Monitor")
	UStaticMeshComponent* MonitorMesh_1_Vertical;

	// ========== 좌석 1 컴포넌트 - 키보드/마우스/본체 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Keyboard")
	USceneComponent* KeyboardSlot_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Keyboard")
	UStaticMeshComponent* KeyboardMesh_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Mouse")
	USceneComponent* MouseSlot_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Mouse")
	UStaticMeshComponent* MouseMesh_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Computer")
	USceneComponent* ComputerSlot_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Seat1|Computer")
	UStaticMeshComponent* ComputerMesh_1;

	// ========== 좌석별 상태 ==========

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Chair")
	FChairSlotState ChairState_0;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Chair")
	FChairSlotState ChairState_1;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Seat")
	AOfficeworker* Occupant_0 = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Seat")
	AOfficeworker* Occupant_1 = nullptr;

	// 좌석별 슬롯 상태 (부모의 SlotStates 대신 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|State")
	TMap<EWorkstationSlot, FSlotSkinState> SlotStates_0;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|State")
	TMap<EWorkstationSlot, FSlotSkinState> SlotStates_1;

public:
	// ==================== 부모 가상 함수 오버라이드 ====================

	virtual void InitializeWorkstation() override;
	virtual void RestoreFromSaveData(const FWorkstationSaveData& SaveData) override;
	virtual FWorkstationSaveData GetSaveData() const override;

	virtual EWorkstationType GetWorkstationType() const override { return EWorkstationType::Double; }
	virtual int32 GetChairCount() const override { return 2; }

	// 착석 시스템
	virtual bool HasEmptySeat() const override;
	virtual bool CanSitAt(int32 SeatIndex) const override;
	virtual int32 SitDown(AOfficeworker* Worker) override;
	virtual bool SitDownAt(int32 SeatIndex, AOfficeworker* Worker) override;
	virtual void StandUp(AOfficeworker* Worker) override;
	virtual FVector GetSeatLocation(int32 SeatIndex) const override;
	virtual FRotator GetSeatRotation(int32 SeatIndex) const override;
	virtual int32 FindWorkerSeatIndex(AOfficeworker* Worker) const override;
	virtual int32 GetOccupantCount() const override;
	virtual AOfficeworker* GetOccupantAt(int32 SeatIndex) const override;

	// 직원 배정 시스템
	virtual bool AssignEmployeeToSeat(int32 SeatIndex, int32 EmployeeID) override;
	virtual void UnassignSeat(int32 SeatIndex) override;
	virtual int32 GetAssignedEmployeeID(int32 SeatIndex) const override;
	virtual int32 FindEmptyAssignmentSlot() const override;

	// 의자 스킨
	virtual bool SetChairSkin(int32 ChairIndex, FName SkinID) override;
	virtual bool SetAllChairsSkin(FName SkinID) override;
	virtual FName GetChairSkinID(int32 ChairIndex) const override;
	virtual void SetChairMesh(int32 ChairIndex, UStaticMesh* NewMesh) override;

	// 슬롯 스킨 - 모든 좌석에 적용
	virtual bool SetSlotSkin(EWorkstationSlot Slot, FName SkinID) override;
	virtual bool IsSlotActive(EWorkstationSlot Slot) const override;
	virtual void SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh) override;
	virtual void SetSlotVisibility(EWorkstationSlot Slot, bool bVisible) override;

	virtual void AttachEquipmentToSockets() override;

	// ==================== Double 전용 함수 ====================

	/** 특정 좌석에만 슬롯 스킨 적용 */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Skin")
	bool SetSlotSkinForSeat(int32 SeatIndex, EWorkstationSlot Slot, FName SkinID);

	/** 특정 좌석의 슬롯 활성화 여부 */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Skin")
	bool IsSlotActiveForSeat(int32 SeatIndex, EWorkstationSlot Slot) const;

	/** 특정 좌석의 슬롯 스킨 ID */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Skin")
	FName GetSlotSkinIDForSeat(int32 SeatIndex, EWorkstationSlot Slot) const;

	/** 특정 좌석의 슬롯 메시 설정 */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Mesh")
	void SetSlotMeshForSeat(int32 SeatIndex, EWorkstationSlot Slot, UStaticMesh* NewMesh);

	/** 특정 좌석의 슬롯 가시성 설정 */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Mesh")
	void SetSlotVisibilityForSeat(int32 SeatIndex, EWorkstationSlot Slot, bool bVisible);

protected:
	virtual UStaticMeshComponent* GetMeshComponentForSlot(EWorkstationSlot Slot) const override;
	virtual void ApplySetupLevel() override;

	// Double 전용 헬퍼 함수
	UStaticMeshComponent* GetMeshComponentForSlotAndSeat(int32 SeatIndex, EWorkstationSlot Slot) const;
	void ApplySetupLevelForSeat(int32 SeatIndex);
	TMap<EWorkstationSlot, FSlotSkinState>& GetSlotStatesForSeat(int32 SeatIndex);
	const TMap<EWorkstationSlot, FSlotSkinState>& GetSlotStatesForSeat(int32 SeatIndex) const;

	// ========== 소켓 이름 상수 - 좌석 0 ==========
	static const FName SocketName_Chair_0;
	static const FName SocketName_Seat_0;
	static const FName SocketName_Laptop_0_Center;
	static const FName SocketName_Laptop_0_Side;
	static const FName SocketName_Monitor_0_Single;
	static const FName SocketName_Monitor_0_DualLeft;
	static const FName SocketName_Monitor_0_DualRight;
	static const FName SocketName_Monitor_0_Vertical;
	static const FName SocketName_Keyboard_0;
	static const FName SocketName_Mouse_0;
	static const FName SocketName_Computer_0;

	// ========== 소켓 이름 상수 - 좌석 1 ==========
	static const FName SocketName_Chair_1;
	static const FName SocketName_Seat_1;
	static const FName SocketName_Laptop_1_Center;
	static const FName SocketName_Laptop_1_Side;
	static const FName SocketName_Monitor_1_Single;
	static const FName SocketName_Monitor_1_DualLeft;
	static const FName SocketName_Monitor_1_DualRight;
	static const FName SocketName_Monitor_1_Vertical;
	static const FName SocketName_Keyboard_1;
	static const FName SocketName_Mouse_1;
	static const FName SocketName_Computer_1;
};
