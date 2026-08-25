// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Office/WorkstationActorBase.h"
#include "SingleWorkstationActor.generated.h"

class UDataTable;
class AOfficeworker;

/**
 * 싱글 업무공간 액터 - 책상+의자+컴퓨터 통합 가구 (싱글/더블 의자 지원)
 *
 * 구조:
 * - 책상: BP에서 고정 설정 (타입별로 다른 BP)
 * - 의자: 스킨으로 외형 변경 (고정 2개 컴포넌트, ChairCount로 1/2개 구분)
 * - 노트북: 스킨으로 외형 변경 (항상 존재)
 * - 모니터: 세팅 레벨에 따라 Flat/Curved 활성화, 스킨으로 색상 변경
 * - Vertical 모니터: Lv6에서 활성화
 * - 키보드: Lv2부터 활성화, 스킨으로 외형 변경
 * - 마우스: Lv2부터 활성화, 스킨으로 외형 변경
 * - 본체: Lv3부터 활성화 (고정 메시)
 *
 * 업그레이드 단계:
 * - Lv1: 노트북
 * - Lv2: 노트북 + 키보드 + 마우스
 * - Lv3: 노트북 + Flat모니터 + 본체
 * - Lv4: 노트북 + Flat모니터 + 본체 + 키보드 + 마우스
 * - Lv5: 노트북 + Curved모니터 + 본체 + 키보드 + 마우스
 * - Lv6: 노트북 + Curved + Vertical + 본체 + 키보드 + 마우스
 */
UCLASS(Blueprintable)
class COMPANYGROWTHRENEWAL_API ASingleWorkstationActor : public AWorkstationActorBase
{
	GENERATED_BODY()

public:
	ASingleWorkstationActor();

protected:
	virtual void BeginPlay() override;

public:
	// ========== 컴포넌트 - 의자 (고정 2개) ==========
	// 의자 0 (싱글/더블 공용 - 첫 번째 의자)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Chair")
	USceneComponent* ChairSlot_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Chair")
	UStaticMeshComponent* ChairMesh_0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Chair")
	USceneComponent* SeatPoint_0;

	// 의자 1 (더블 전용 - 두 번째 의자)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Chair")
	USceneComponent* ChairSlot_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Chair")
	UStaticMeshComponent* ChairMesh_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Chair")
	USceneComponent* SeatPoint_1;

	// 업무공간 타입 (싱글/더블 - BP에서 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workstation|Chair")
	EWorkstationType WorkstationType = EWorkstationType::Single;

	// 의자별 상태 (고정 2개)
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Chair")
	FChairSlotState ChairState_0;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Chair")
	FChairSlotState ChairState_1;

	// 각 의자에 앉아있는 직원들 (고정 2개)
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Seat")
	AOfficeworker* Occupant_0 = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Seat")
	AOfficeworker* Occupant_1 = nullptr;

	// ========== 컴포넌트 - 노트북 (2개 위치) ==========

	// 노트북 중앙 (Lv1 - 노트북만 사용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Laptop")
	USceneComponent* LaptopSlot_Center;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Laptop")
	UStaticMeshComponent* LaptopMesh_Center;

	// 노트북 사이드 (Lv3A - Curved 모니터 옆)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Laptop")
	USceneComponent* LaptopSlot_Side;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Laptop")
	UStaticMeshComponent* LaptopMesh_Side;

	// ========== 컴포넌트 - 모니터 (4개 위치) ==========

	// 싱글 모니터 (중앙) - Lv2, Lv3A + Curved+Vertical 조합의 Curved
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	USceneComponent* MonitorSlot_Single;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	UStaticMeshComponent* MonitorMesh_Single;

	// Curved+Curved 조합 (Lv3B) 왼쪽
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	USceneComponent* MonitorSlot_DualLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	UStaticMeshComponent* MonitorMesh_DualLeft;

	// Curved+Curved 조합 (Lv3B) 오른쪽
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	USceneComponent* MonitorSlot_DualRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	UStaticMeshComponent* MonitorMesh_DualRight;

	// Vertical 모니터 (Curved+Vertical 조합 Lv3C용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	USceneComponent* MonitorSlot_Vertical;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Monitor")
	UStaticMeshComponent* MonitorMesh_Vertical;

	// ========== 컴포넌트 - 키보드 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Keyboard")
	USceneComponent* KeyboardSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Keyboard")
	UStaticMeshComponent* KeyboardMesh;

	// ========== 컴포넌트 - 마우스 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Mouse")
	USceneComponent* MouseSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Mouse")
	UStaticMeshComponent* MouseMesh;

	// ========== 컴포넌트 - 본체 (고정 메시) ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Computer")
	USceneComponent* ComputerSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Computer")
	UStaticMeshComponent* ComputerMesh;

	// ========== 컴포넌트 - 액세서리 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Accessory")
	USceneComponent* AccessorySlot1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Accessory")
	UStaticMeshComponent* AccessoryMesh1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Accessory")
	USceneComponent* AccessorySlot2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Accessory")
	UStaticMeshComponent* AccessoryMesh2;

public:
	// ==================== 부모 가상 함수 오버라이드 ====================

	// 초기화
	virtual void InitializeWorkstation() override;
	virtual void RestoreFromSaveData(const FWorkstationSaveData& SaveData) override;
	virtual FWorkstationSaveData GetSaveData() const override;

	// 타입/의자
	virtual EWorkstationType GetWorkstationType() const override { return WorkstationType; }
	virtual int32 GetChairCount() const override { return (WorkstationType == EWorkstationType::Double) ? 2 : 1; }

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

	// 슬롯 스킨 (노트북/모니터는 여러 위치에 적용하기 위해 오버라이드)
	virtual bool SetSlotSkin(EWorkstationSlot Slot, FName SkinID) override;
	virtual bool IsSlotActive(EWorkstationSlot Slot) const override;
	virtual void SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh) override;
	virtual void SetSlotVisibility(EWorkstationSlot Slot, bool bVisible) override;

	// 소켓 기반 배치
	virtual void AttachEquipmentToSockets() override;

	// ==================== 싱글 전용 함수 ====================

	/** 업무공간 타입 설정 (스폰 후 테이블에서 읽어서 설정) */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Chair")
	void SetWorkstationType(EWorkstationType NewType);

protected:
	// 슬롯에 해당하는 MeshComponent 반환
	virtual UStaticMeshComponent* GetMeshComponentForSlot(EWorkstationSlot Slot) const override;

	// 세팅 레벨에 따라 슬롯 활성화/비활성화 적용
	virtual void ApplySetupLevel() override;

	// ========== 소켓 이름 상수 ==========
	static const FName SocketName_Laptop_Center;  // Lv1 중앙 위치
	static const FName SocketName_Laptop_Side;    // Lv3A 사이드 위치
	static const FName SocketName_Monitor_Single;     // 싱글 모니터 & Curved+Vertical의 Curved (중앙)
	static const FName SocketName_Monitor_DualLeft;   // Curved+Curved 왼쪽
	static const FName SocketName_Monitor_DualRight;  // Curved+Curved 오른쪽
	static const FName SocketName_Monitor_Vertical;   // Vertical 전용
	static const FName SocketName_Keyboard;
	static const FName SocketName_Mouse;
	static const FName SocketName_Computer;
	// 의자/착석 소켓 이름 (고정 2개)
	static const FName SocketName_Chair_0;
	static const FName SocketName_Chair_1;
	static const FName SocketName_Seat_0;
	static const FName SocketName_Seat_1;
};
