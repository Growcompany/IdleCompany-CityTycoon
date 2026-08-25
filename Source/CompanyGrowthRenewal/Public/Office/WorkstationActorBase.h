// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Office/WorkstationTypes.h"
#include "Table/WorkstationTable.h"
#include "Interfaces/BubbleAnchorProvider.h"
#include "WorkstationActorBase.generated.h"

class UDataTable;
class AOfficeworker;
class UBoxComponent;
class UGlobalAssetCache;

/**
 * 업무공간 액터 베이스 - 싱글/더블 공통 기능
 */
UCLASS(Abstract, Blueprintable)
class COMPANYGROWTHRENEWAL_API AWorkstationActorBase : public AActor, public IBubbleAnchorProvider
{
	GENERATED_BODY()

public:
	AWorkstationActorBase();

	// IBubbleAnchorProvider — 빈 좌석 버블이 추적할 책상 상단 월드 위치
	virtual FVector GetBubbleAnchorPosition() const override;

protected:
	virtual void BeginPlay() override;

public:
	// ==================== 공통 컴포넌트 ====================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation")
	USceneComponent* RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation|Desk")
	UStaticMeshComponent* DeskMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation")
	UBoxComponent* BoxComponent;

	// 직원 AI 가 책상을 가로지르지 않도록 NavMesh 에 구멍을 내는 동적 장애물 (물리 차단 아님)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workstation")
	UBoxComponent* NavBlocker;

	// ==================== 공통 데이터 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workstation|Data")
	FName WorkstationTypeID;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Data")
	FString InstanceID;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Level")
	EComputerSetupLevel CurrentSetupLevel = EComputerSetupLevel::Level1;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Level")
	EMonitorType CurrentMonitor1Type = EMonitorType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Level")
	EMonitorType CurrentMonitor2Type = EMonitorType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|State")
	TMap<EWorkstationSlot, FSlotSkinState> SlotStates;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Stats")
	float WorkSpeedBonusRate = 0.0f;

	// ==================== DataTable 참조 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workstation|DataTable")
	UDataTable* WorkstationItemSkinTable;

public:
	// ==================== 공통 함수 ====================

	UFUNCTION(BlueprintCallable, Category = "Workstation")
	virtual void InitializeWorkstation();

	/** 하이라이트 켜기/끄기 */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Highlight")
	void SetHighlight(bool bEnabled);

	void RecalcBoxExtent();

	// NavBlocker 를 nav 옥트리에 재등록 + 해당 영역 재생성 요청 (시작 배치 책상이 바닥 NavMesh 비동기 생성과 경합해 안 파이는 것 방지)
	void RefreshNavObstacle();

	UFUNCTION(BlueprintCallable, Category = "Workstation")
	virtual void RestoreFromSaveData(const FWorkstationSaveData& SaveData);

	UFUNCTION(BlueprintCallable, Category = "Workstation")
	virtual FWorkstationSaveData GetSaveData() const;

	// ==================== 컴퓨터 세팅 레벨 ====================

	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	bool TryUpgradeSetupLevel();

	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	EComputerSetupLevel GetCurrentSetupLevel() const { return CurrentSetupLevel; }

	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	bool GetCurrentSetupLevelData(FComputerSetupLevelData& OutData) const;

	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	bool GetNextSetupLevelData(FComputerSetupLevelData& OutData) const;

	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	bool IsMaxLevel() const;

	/** 최대 레벨의 반대 외형 행. 최대 레벨이 아니거나 행이 없으면 false. */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	bool GetMaxVariantData(FComputerSetupLevelData& OutData) const;

	/** 최대 레벨에서 반대 외형 행이 존재할 때만 true */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	bool CanSwapMaxVariant() const;

	/** 반대 외형 행 조회 → 메시 검증 → Diamond 정확 인출 → 성공 시에만 전환. 레벨·속도는 그대로다. */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Level")
	bool TrySwapMaxVariant();

	// ==================== 스킨 설정 ====================

	UFUNCTION(BlueprintCallable, Category = "Workstation|Skin")
	virtual bool SetSlotSkin(EWorkstationSlot Slot, FName SkinID);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Skin")
	FName GetSlotSkinID(EWorkstationSlot Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Workstation|Skin")
	virtual bool IsSlotActive(EWorkstationSlot Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Workstation|Mesh")
	virtual void SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Mesh")
	virtual void SetSlotVisibility(EWorkstationSlot Slot, bool bVisible);

	// ==================== 스탯 ====================

	UFUNCTION(BlueprintCallable, Category = "Workstation|Stats")
	float GetWorkSpeedBonusRate() const { return WorkSpeedBonusRate; }

	// ==================== 순수 가상 함수 (자식에서 구현 필수) ====================

	UFUNCTION(BlueprintCallable, Category = "Workstation|Type")
	virtual EWorkstationType GetWorkstationType() const PURE_VIRTUAL(AWorkstationActorBase::GetWorkstationType, return EWorkstationType::Single;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Chair")
	virtual int32 GetChairCount() const PURE_VIRTUAL(AWorkstationActorBase::GetChairCount, return 0;);

	// 착석 시스템
	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual bool HasEmptySeat() const PURE_VIRTUAL(AWorkstationActorBase::HasEmptySeat, return false;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual bool CanSitAt(int32 SeatIndex) const PURE_VIRTUAL(AWorkstationActorBase::CanSitAt, return false;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual int32 SitDown(AOfficeworker* Worker) PURE_VIRTUAL(AWorkstationActorBase::SitDown, return -1;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual bool SitDownAt(int32 SeatIndex, AOfficeworker* Worker) PURE_VIRTUAL(AWorkstationActorBase::SitDownAt, return false;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual void StandUp(AOfficeworker* Worker) PURE_VIRTUAL(AWorkstationActorBase::StandUp, );

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual FVector GetSeatLocation(int32 SeatIndex) const PURE_VIRTUAL(AWorkstationActorBase::GetSeatLocation, return FVector::ZeroVector;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual FRotator GetSeatRotation(int32 SeatIndex) const PURE_VIRTUAL(AWorkstationActorBase::GetSeatRotation, return FRotator::ZeroRotator;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual int32 FindWorkerSeatIndex(AOfficeworker* Worker) const PURE_VIRTUAL(AWorkstationActorBase::FindWorkerSeatIndex, return -1;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual int32 GetOccupantCount() const PURE_VIRTUAL(AWorkstationActorBase::GetOccupantCount, return 0;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Seat")
	virtual AOfficeworker* GetOccupantAt(int32 SeatIndex) const PURE_VIRTUAL(AWorkstationActorBase::GetOccupantAt, return nullptr;);

	// ==================== 직원 배정 시스템 ====================

	/** 특정 좌석에 직원 배정 (EmployeeID 저장) */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Assignment")
	virtual bool AssignEmployeeToSeat(int32 SeatIndex, int32 EmployeeID) PURE_VIRTUAL(AWorkstationActorBase::AssignEmployeeToSeat, return false;);

	/** 특정 좌석의 배정 해제 */
	UFUNCTION(BlueprintCallable, Category = "Workstation|Assignment")
	virtual void UnassignSeat(int32 SeatIndex) PURE_VIRTUAL(AWorkstationActorBase::UnassignSeat, );

	/** 특정 좌석의 배정된 직원 ID 반환 (-1이면 미배정) */
	UFUNCTION(BlueprintPure, Category = "Workstation|Assignment")
	virtual int32 GetAssignedEmployeeID(int32 SeatIndex) const PURE_VIRTUAL(AWorkstationActorBase::GetAssignedEmployeeID, return -1;);

	/** 빈 배정 슬롯 인덱스 반환 (-1이면 모두 배정됨) */
	UFUNCTION(BlueprintPure, Category = "Workstation|Assignment")
	virtual int32 FindEmptyAssignmentSlot() const PURE_VIRTUAL(AWorkstationActorBase::FindEmptyAssignmentSlot, return -1;);

	// 의자 스킨
	UFUNCTION(BlueprintCallable, Category = "Workstation|Chair")
	virtual bool SetChairSkin(int32 ChairIndex, FName SkinID) PURE_VIRTUAL(AWorkstationActorBase::SetChairSkin, return false;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Chair")
	virtual bool SetAllChairsSkin(FName SkinID) PURE_VIRTUAL(AWorkstationActorBase::SetAllChairsSkin, return false;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Chair")
	virtual FName GetChairSkinID(int32 ChairIndex) const PURE_VIRTUAL(AWorkstationActorBase::GetChairSkinID, return NAME_None;);

	UFUNCTION(BlueprintCallable, Category = "Workstation|Chair")
	virtual void SetChairMesh(int32 ChairIndex, UStaticMesh* NewMesh) PURE_VIRTUAL(AWorkstationActorBase::SetChairMesh, );

	// 소켓 기반 배치
	UFUNCTION(BlueprintCallable, Category = "Workstation|Socket")
	virtual void AttachEquipmentToSockets() PURE_VIRTUAL(AWorkstationActorBase::AttachEquipmentToSockets, );

protected:
	// 하이라이트 표시용 SMC
	UPROPERTY()
	UStaticMeshComponent* HighlightSMC;

	// GlobalAssetCache 캐시
	UPROPERTY()
	UGlobalAssetCache* GlobalAssetCache;

	// 공통 Protected 함수
	// 좌석별 메시 컴포넌트를 아는 자식만 구현 가능 — 베이스 사본을 두면 스키마 변경이 3중 중복된다
	virtual void ApplySetupLevel() PURE_VIRTUAL(AWorkstationActorBase::ApplySetupLevel, );
	void RecalculateStats();
	void SaveWorkstationData();
	FWorkstationItemSkinData* FindSkinData(FName SkinID, EWorkstationSlot Slot) const;
	const FComputerSetupLevelData* FindSetupLevelData(EComputerSetupLevel Level) const;
	bool ValidateAndLoadSetupMeshes(const FComputerSetupLevelData& LevelData, FString& OutError) const;
	bool AttachToSocket(USceneComponent* Component, FName SocketName);

	// 자식에서 오버라이드하여 슬롯별 메시 컴포넌트 반환
	virtual UStaticMeshComponent* GetMeshComponentForSlot(EWorkstationSlot Slot) const;
};
