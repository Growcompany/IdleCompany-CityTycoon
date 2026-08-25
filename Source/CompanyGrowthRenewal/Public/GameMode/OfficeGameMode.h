// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/GachaTier.h"
#include "OfficeGameMode.generated.h"

class AOfficeInterior;
class UEmployeeManager;
class UOfficeStageProgressManager;
class AWorkstationActorBase;
struct FProjectData;

/**
 * OfficeMap 전용 GameMode
 *
 * 기능:
 * - GameInstance에서 진입 모드 확인 (PromotionTest, Training, FreeView, Normal)
 * - 모드에 따라 초기 UI 표시
 * - 작업 완료 후 오피스 메뉴 UI 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOfficeGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ========== Office Mode Management ==========

	// 현재 오피스 모드 가져오기
	UFUNCTION(BlueprintPure, Category = "Office")
	EOfficeMode GetCurrentOfficeMode() const;

	// 오피스 모드 변경 (씬 전환 없이)
	UFUNCTION(BlueprintCallable, Category = "Office")
	void SwitchOfficeMode(EOfficeMode NewMode);

	// 메인 맵으로 복귀
	UFUNCTION(BlueprintCallable, Category = "Office")
	void ReturnToMainMap();

	// OfficeInterior 가져오기 (중앙 관리)
	UFUNCTION(BlueprintPure, Category = "Office")
	AOfficeInterior* GetOfficeInterior() const { return CachedOfficeInterior; }

	// 직원 ID로 Actor 찾기
	UFUNCTION(BlueprintCallable, Category = "Office|Employee")
	AOfficeworker* FindEmployeeActorByID(int32 EmployeeID) const;

	// 단일 직원 워커를 지정 워크스테이션 좌석에 즉시 스폰+배치 (로드 시 일괄 스폰과 별개로, 책상 배정 시 즉시 등장용)
	void SpawnWorkerForAssignment(const FEmployeeInstance& Employee, AWorkstationActorBase* Workstation, int32 SeatIndex);

	// 워커 액터 즉시 제거 (자리 비우기/해고). SeatedWorkstation 지정 시 런타임 점유(StandUp)도 정리
	void DespawnWorkerByID(int32 EmployeeID, AWorkstationActorBase* SeatedWorkstation = nullptr);

	// ========== Stage Progress Management ==========

	// 스테이지 진행 매니저 가져오기
	UFUNCTION(BlueprintPure, Category = "Office|Stage")
	UOfficeStageProgressManager* GetStageProgressManager() const;

	// 프로젝트 시작 (스테이지 1부터)
	UFUNCTION(BlueprintCallable, Category = "Office|Stage")
	void StartProject(const FProjectData& ProjectData);

	// 현재 스테이지 종료
	UFUNCTION(BlueprintCallable, Category = "Office|Stage")
	void EndCurrentProject();

private:
	// ========== Employee Management ==========

	// 직원들을 3D로 스폰 (프레임 분산)
	void SpawnEmployeesInOffice();

	// 단일 직원 스폰 (내부용)
	void SpawnSingleEmployee(const FEmployeeInstance& Employee);

	// 스틱맨(신규 병렬) 사용 여부 게이트 — strangler-fig 전환 제어
	bool ShouldUseStickWorker(const FEmployeeInstance& Employee) const;

	// 저장된 ELootBoxRarity(6단계) → EGachaTier(3단계) 압축 (세이브 변경 없이 골드 신호 산출)
	static EGachaTier ResolveGachaTier(const FEmployeeInstance& Employee);

	// 다음 직원 스폰 (타이머 콜백)
	void SpawnNextEmployee();

	// NavMesh 위 랜덤 위치 찾기 (전체 NavMesh 영역에서)
	bool GetRandomSpawnLocationOnNavMesh(FVector& OutLocation);

	// 스폰된 직원 액터들
	UPROPERTY()
	TArray<AOfficeworker*> SpawnedEmployees;

	// 스폰 대기 직원 목록 (프레임 분산용)
	TArray<FEmployeeInstance> PendingEmployeesToSpawn;

	// 스폰 타이머 핸들
	FTimerHandle SpawnTimerHandle;

	// 현재 스폰 인덱스
	int32 CurrentSpawnIndex = 0;

	// 스틱맨 직원 전역 토글. true=신규 스틱맨 / false=구 모듈러. (실기 검증 단계라 ON; 구 직원으로 되돌리려면 false)
	UPROPERTY(EditDefaultsOnly, Category = "Worker")
	bool bUseStickWorkers = true;

	// 스틱맨 BP 클래스 1회 해석 캐시 — 스폰당 소프트 클래스 재로드/재해석 제거(GC 루트). 미존재 시 C++ 클래스 폴백 후 캐시.
	UPROPERTY(Transient)
	TSubclassOf<AOfficeworker> ResolvedStickWorkerClass = nullptr;

	// 직원 클릭 이벤트 처리
	UFUNCTION()
	void OnEmployeeClicked(AOfficeworker* ClickedEmployee);

private:
	// 현재 오피스 모드 (캐시)
	EOfficeMode CurrentMode = EOfficeMode::Normal;

	// OfficeInterior 중앙 관리
	UPROPERTY()
	AOfficeInterior* CachedOfficeInterior = nullptr;

	// OfficeInterior 찾기 (BeginPlay에서 호출)
	void FindAndCacheOfficeInterior();

	// ========== Portrait 촬영 (EmployeeManager 시스템 사용) ==========

	// 저장된 카드들의 Portrait 촬영 시작
	void StartPortraitCapture(int32 BuildingIndex);

	// 다음 Portrait 촬영
	void CaptureNextPortrait();

	// Portrait 촬영 완료 콜백
	void OnPortraitCaptured(const FString& EmployeeID);

	// 모든 촬영 완료 처리
	void OnAllPortraitsCaptured();

	// 촬영 대기 카드 목록 (포인터)
	TArray<FEmployeeInstance*> PendingPortraitCards;

	// 촬영 대상 BuildingIndex (콜백에서 사용)
	int32 PendingPortraitBuildingIndex = INDEX_NONE;

	// 현재 촬영 중인 EmployeeID
	int32 CurrentCapturingEmployeeID = -1;

	// 델리게이트 핸들
	FDelegateHandle PortraitCaptureHandle;

	// ========== 수익 수집 연출 ==========

	// 수익 수집 + 코인 연출 트리거
	void CheckAndCollectStoredRevenue();

	// 코인 연출 딜레이 타이머
	FTimerHandle RevenueCollectionTimerHandle;

	// ========== Stage Progress ==========

	// 스테이지 완료 콜백
	UFUNCTION()
	void OnStageCompleted();

	// 스테이지 상태 로드 (오피스 진입 시)
	void LoadStageProgressFromSave();

	// 로드 시 Active Operation이 있었는지 여부 (직원 스폰 시 모드 설정용)
	bool bHasActiveOperationOnLoad = false;

	// 스테이지 상태 저장 (오피스 퇴장 시)
	void SaveStageProgressToSave();

	// 델리게이트 핸들
	FDelegateHandle StageCompleteHandle;
};
