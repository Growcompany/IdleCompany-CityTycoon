// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Office/DecorationTypes.h"
#include "Table/DecorationCardTable.h"
#include "Data/BuildingSaveData.h"
#include "Data/EmployeeTypes.h"
#include "OfficeManager.generated.h"

class ADecorationActor;
class AWorkstationActorBase;
class UWallSelectionWidget;
class AOfficeCameraPawn;
class AOfficeInterior;

/**
 * 오피스 시스템의 중앙 관리자 (WorldSubsystem)
 *
 * 역할:
 * - 꾸미기 모드 활성화/비활성화
 * - 장식품 배치 및 관리
 * - 업무공간 배치 및 관리
 * - 바닥 타일 관리
 * - 저장/로드 데이터 수집
 *
 * 접근 방법:
 * - GetWorld()->GetSubsystem<UOfficeManager>()
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ========== Subsystem Lifecycle ==========

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ========== 꾸미기 모드 제어 ==========

	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	void EnterDecorationMode();

	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	void ExitDecorationMode();

	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	bool IsInDecorationMode() const { return bIsInDecorationMode; }

	// ========== 장식품 선택 ==========

	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	void OnDecorationItemSelected(const FDecorationCardTable& CardData);

	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	void OnFloorTileSelected(FName TileRowName);

	// ========== 벽 선택 콜백 ==========

	UFUNCTION()
	void OnWallSelected(EWallSide WallSide);

	UFUNCTION()
	void OnWallSelectionCancelled();

	// ========== 장식품 등록/해제 ==========

	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	void RegisterPlacedDecoration(ADecorationActor* Decoration);

	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	void UnregisterPlacedDecoration(ADecorationActor* Decoration);

	// ========== 업무공간 등록/해제 ==========

	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	void RegisterPlacedWorkstation(AWorkstationActorBase* Workstation);

	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	void UnregisterPlacedWorkstation(AWorkstationActorBase* Workstation);

	// ========== 업무공간 검색 ==========

	/**
	 * 직원 ID로 해당 직원이 배정된 업무공간을 찾습니다.
	 * @param EmployeeID 찾고자 하는 직원의 ID
	 * @return 직원이 배정된 업무공간 (없으면 nullptr)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	AWorkstationActorBase* FindWorkstationByEmployeeID(int32 EmployeeID);

	/**
	 * 배치된 업무공간 목록 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	const TArray<AWorkstationActorBase*>& GetPlacedWorkstations() const { return PlacedWorkstations; }

	// ========== 착석/해제 SOT (수동 [배정] · 자동 착석 · 자리 비우기 공용) ==========

	/** 지정 책상의 첫 빈 슬롯에 착석 확정: 좌석 기록 + bIsAssigned + 카운터 + 워커 스폰 + 저장 + 미션 신호. bShouldSave=false면 호출자가 세이브 책임 */
	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	bool SeatEmployeeAtWorkstation(AWorkstationActorBase* Workstation, int32 EmployeeID, bool bShouldSave = true);

	/** 선호 책상 → 셋업 레벨 내림차순(동률=배치순) 스캔 자동 착석. 착석한 책상 반환 (만석이면 nullptr = 벤치 유지) */
	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	AWorkstationActorBase* AutoSeatEmployee(int32 EmployeeID, AWorkstationActorBase* PreferredWorkstation = nullptr,
		bool bShouldSave = true);

	/** 좌석 해제 + 벤치 복귀(bIsAssigned만 클리어) + 카운터 감소 + 워커 액터 제거 + 저장 */
	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	bool UnseatEmployee(AWorkstationActorBase* Workstation, int32 EmployeeID);

	/** 벤치 후보 정렬 정책 — 종합(★ 반영) 내림차순, 동률은 입력 순서 유지. 순수 함수라 테스트 대상. */
	static void SortBenchByOverallDesc(TArray<FEmployeeInstance>& InOutBench);

	/**
	 * 새로 놓인 책상의 빈 좌석 수만큼 벤치에서 자동 착석.
	 * 채용 시 자동 착석(AutoSeatEmployee)의 반대 방향 — 없으면 먼저 뽑아둔 직원이 영구히 생산 0으로 남는다.
	 * @return 착석시킨 EmployeeID 목록 (빈 배열 = 앉힐 사람이 없었음)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Workstation")
	TArray<int32> AutoSeatBenchedEmployees(AWorkstationActorBase* NewWorkstation);

	// 책상 배치 수 변경 알림 (뱃지 갱신용). 책상은 상한이 없다 — 플레이어가 관리하는 수는 인원 하나뿐이다.
	DECLARE_MULTICAST_DELEGATE(FOnWorkstationCountChanged);
	FOnWorkstationCountChanged OnWorkstationCountChanged;

	// ========== 저장/로드 ==========

	/**
	 * 현재 오피스 데이터를 저장 데이터에 채우기 (Decoration + Workstation)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|SaveLoad")
	void FillOfficeSaveData(FOfficeSaveData& OutOfficeData) const;

	/**
	 * 저장 데이터에서 오피스 복원 (Decoration + Workstation)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|SaveLoad")
	void ApplyOfficeSaveData(const FOfficeSaveData& OfficeData);

	// 이 오피스 월드가 ApplyOfficeSaveData로 복원되었는지 — 복원 전 SaveGameData가 빈 인테리어로 디스크를 덮어쓰는 것을 차단하는 게이트
	bool IsInteriorRestored() const { return bInteriorRestored; }

	// ========== 수익 저장 접근 (Building 데이터) ==========

	/**
	 * 현재 Building의 저장된 수익 가져오기
	 */
	UFUNCTION(BlueprintPure, Category = "Office|Revenue")
	float GetStoredRevenue() const;

	/**
	 * 현재 Building의 수익 저장 용량 가져오기
	 */
	UFUNCTION(BlueprintPure, Category = "Office|Revenue")
	float GetStoredRevenueCapacity() const;

	/**
	 * 수익 저장 (용량 제한 적용)
	 * @param Amount 저장할 금액
	 * @return 실제로 저장된 금액
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Revenue")
	float AddToStoredRevenue(float Amount);

	/**
	 * 저장된 수익 전액 수령 (0으로 초기화)
	 * @return 수령한 금액
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Revenue")
	float CollectStoredRevenue();

	/**
	 * 수익 저장 공간이 가득 찼는지 확인
	 */
	UFUNCTION(BlueprintPure, Category = "Office|Revenue")
	bool IsStoredRevenueFull() const;

	/**
	 * 새 건물 초기 기본 장식 스폰 (창문 WD_13)
	 * 저장 데이터가 없는 새 건물에서만 호출
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	void SpawnDefaultDecorations();

	/**
	 * 스타터 프리셋 2단계: PendingStarterPreset을 해석해 OfficeData에 데코/바닥타일을 채운다.
	 * ApplyOfficeSaveData 호출 "이전"에 불러야 함 (이 함수는 데이터만 부풀리고 스폰은 기존 복원기가 담당).
	 * @return 적용 여부
	 */
	bool ApplyStarterPresetToData(FOfficeSaveData& OfficeData, ECompanyType Industry);

	/**
	 * 배치된 장식물 목록 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Decoration")
	const TArray<ADecorationActor*>& GetPlacedDecorations() const { return PlacedDecorations; }

private:
	// ========== 내부 상태 ==========

	bool bIsInDecorationMode = false;
	FDecorationCardTable SelectedDecorationData;

	// ApplyOfficeSaveData 완료 시 true. 오피스 진입 직후 복원 전 구간에서 전체 저장이 빈 인테리어로 디스크를 클로버하는 것을 막는 게이트.
	bool bInteriorRestored = false;

	// BaseVaultCapacity는 ABuildingBaseActor::BaseVaultCapacity 참조

	// 배치된 장식품 목록
	UPROPERTY()
	TArray<ADecorationActor*> PlacedDecorations;

	// 배치된 업무공간 목록
	UPROPERTY()
	TArray<AWorkstationActorBase*> PlacedWorkstations;

	// 시작 자동 배치 책상 일괄 스폰 후 다음 틱에 NavMesh 장애물을 통합 재반영 (바닥 nav 비동기 생성 경합 회피)
	void RefreshWorkstationNavObstacles();

	// ========== UI 참조 ==========

	UPROPERTY()
	TSubclassOf<UWallSelectionWidget> WallSelectionWidgetClass;

	UPROPERTY()
	UWallSelectionWidget* WallSelectionWidget = nullptr;

	// ========== 게임 오브젝트 참조 ==========

	UPROPERTY()
	AOfficeCameraPawn* CameraPawn = nullptr;

	UPROPERTY()
	AOfficeInterior* OfficeInterior = nullptr;

	// ========== 내부 헬퍼 함수 ==========

	void CacheReferences();
	void StartWallDecorationFlow();
	void StartFloorDecorationFlow();
	void StartGridDecorationFlow();
	void CreateWallSelectionWidget();
	void ShowWallSelectionWidget();
	void HideWallSelectionWidget();

	// Building 데이터 접근 헬퍼
	FOfficeSaveData* GetCurrentOfficeSaveData() const;
};
