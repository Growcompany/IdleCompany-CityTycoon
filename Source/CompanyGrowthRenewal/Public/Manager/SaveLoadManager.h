// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/EmployeeManager.h"
#include "Manager/EntityManager.h"
#include "Manager/ResourceItemManager.h"
#include "Data/GameSaveData.h"
#include "Enum/CompanyTitle.h"
#include "Enum/CompanyType.h"
#include "SaveLoadManager.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnGameDataLoaded);

// 본사 레벨업 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHQLevelUp, int32, NewLevel);

// 회사 등급 변경 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanyTitleChanged, ECompanyTitle, NewTitle);

// 오프라인 보상 완료 (TotalGained, OfflineSeconds) — A6 모달이 바인딩해서 표시
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOfflineGainsApplied, float, float);

// 오프라인 정산 빌딩별 내역 (6-2 정산 모달 — 금고 손실 표시 → 강화 유도).
// 금고 재설계로 LossByVault = 금고 시간 부족분 × 수익률이 되어 강화 1레벨 가치를 원단위 직번역 가능.
USTRUCT()
struct FOfflineGainEntry
{
	GENERATED_BODY()

	UPROPERTY() int32 BuildingIndex = -1;
	UPROPERTY() float RawGain = 0.0f;        // 감쇠 반영 잠재 수익 (금고 무제한 가정)
	UPROPERTY() float ActualGain = 0.0f;     // 실제 금고 적립분
	UPROPERTY() float LossByVault = 0.0f;    // RawGain − ActualGain (금고 초과로 놓친 수익, >0이면 강화 유도)
	UPROPERTY() int32 VaultLevel = 0;
	// 손실 원인 구분: 금고 초과(false) vs 운영 수명 만료(true). 수명 손실을 금고로 오귀인 금지 (Fable §4-1)
	UPROPERTY() bool bLifespanBound = false;
};

// 오프라인 정산 1회분의 계산 결과 (실지급 경로와 dry-run 계측이 공유).
USTRUCT()
struct FOfflineGainsResult
{
	GENERATED_BODY()

	UPROPERTY() float TotalGained = 0.0f;           // 금고에 실제 적립된 합
	UPROPERTY() float TotalLostToVaultCap = 0.0f;   // 금고 초과로 잘려나간 합
	UPROPERTY() int32 EligibleBuildings = 0;        // 오프라인 수익이 발생한 빌딩 수
	UPROPERTY() float ElapsedAdvance = 0.0f;        // 오프라인 동안 앞당긴 운영 경과(초). 대상 빌딩 0이면 0.
	UPROPERTY() TArray<FOfflineGainEntry> Entries;
};

// 6-2 정산 모달용 상세 델리게이트 (기존 OnOfflineGainsApplied 는 그대로 — CityAcquisition/A6 호환).
// (TotalGained, OfflineSeconds, 빌딩별 내역, 12h 캡 도달 여부, 벽돌 지급량)
// 벽돌은 건물이 아니라 자원이라 Entries(FOfflineGainEntry=건물별 금고)에 끼울 수 없다 → 별도 파라미터.
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnOfflineGainsDetailed, float, float, const TArray<FOfflineGainEntry>&, bool, int64);

UCLASS()
class COMPANYGROWTHRENEWAL_API USaveLoadManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 로드 완료 이벤트
    FOnGameDataLoaded OnGameDataLoaded;

    // 오프라인 보상 적용 완료
    FOnOfflineGainsApplied OnOfflineGainsApplied;

    // 6-2 정산 모달 — 빌딩별 내역 + 12h 캡 도달 여부 (표시 타이밍은 PendingReport flush)
    FOnOfflineGainsDetailed OnOfflineGainsDetailed;

    // 오프라인 정산 결과 보관 (로그인 콜백엔 UIBase 없음 → MainMap UI 준비 후 모달 1회 flush).
    // OnOfflineGainsDetailed 는 인라인 발화 없이 오직 Consume 에서 1회 발화 → 이중 표시 방지.
    bool HasPendingOfflineReport() const { return bHasPendingOfflineReport; }
    void ConsumePendingOfflineReport();  // MainMap UI 준비 완료 시 호출 → OnOfflineGainsDetailed 발화 후 소비

    // 공장이 OnOfflineGainsApplied 수신 중에 호출 — 자기가 지급한 벽돌을 정산 보고에 싣는다.
    // 생산율 계산이 액터 문 간격에 묶여 있어(세이브에 없음) 여기서 계산할 수 없다. 결과만 받는다.
    void AddPendingOfflineBrickGain(int64 Bricks);

    // 이 시간 미만의 부재는 재접속 오차로 보고 정산하지 않는다. 돈·벽돌이 같은 값을 봐야
    // "벽돌만 있고 돈은 없는 정산 모달" 이 생기지 않는다.
    static constexpr float MinOfflineSettleSeconds = 60.0f;

    // 오프라인 정산 상한(초). 권위는 백엔드 서브시스템이지만, UI 가 그걸 직접 참조하면
    // 백엔드 교체 때 UI 까지 따라 바뀐다 — 정산 도메인을 쥔 이쪽에서 중계한다.
    static float GetOfflineCapSeconds();

    // [디버그/치트] 테스트용 정산 보고 주입 — 실제 표시 경로(TryShowOfflineReport→Consume→모달)를 그대로 태운다.
    // 이 함수는 금고 잔액을 건드리지 않는다 — 금고 충전은 호출처(ShowOfflineReport 치트)가 직접 한다. 이 치트에서만 사용.
    void DebugSetPendingOfflineReport(float InTotal, float InSeconds, const TArray<FOfflineGainEntry>& InEntries, bool bInCapReached, int64 InBricks = 0);

    UFUNCTION(BlueprintCallable)
    bool SaveGameData();

    // [치트 DevMode] 이 세션의 디스크 쓰기를 봉인한다. 인메모리 커밋은 그대로 성공하고 파일만 안 바뀐다 —
    // 치트로 부풀린 상태가 세이브에 남지 않게 하는 게 목적이라 해제 수단을 두지 않는다(앱 재시작이 해제).
    void LockSavesForDevMode();
    bool IsSaveLockedForDevMode() const { return bDevModeSaveLocked; }

    FString GetActiveSaveSlotName() const;

    // 개발 로컬 모드에서는 마지막 저장 UTC를 기준으로 기존 오프라인 수익 계산을 실행한다.
    void SettleLocalOfflineGains();

    // 후보 경제 상태는 디스크 저장 성공 전 캐시/매니저에 적용하지 않는다.
    bool CanCommitEquipmentTransaction() const;
    bool BuildCurrentSnapshot(FGameSaveData& OutSnapshot);
    // 이미 수집한 상태를 생산 재정산 없이 저장한다. 호출자가 live 상태 적용/실패 복원과 알림을 소유한다.
    bool CommitPreparedSnapshot(const FGameSaveData& Snapshot);
    bool CommitEquipmentSnapshot(const FGameSaveData& Candidate);
    bool CommitEquipmentSnapshot(const FGameSaveData& Candidate, TFunctionRef<void()> ApplyEmployeeState, TFunctionRef<void()> PublishEmployeeChanges);

    // 고빈도 변경(강화 클릭/홀드 등)용 지연 저장 — 스로틀: 첫 요청 후 3초 뒤 1회 저장, 대기 중 재요청은 무시.
    // SaveGameData 는 동기 전체 직렬화+디스크 쓰기라 클릭마다 부르면 프레임 히칭 (레벨 전환/종료는 기존 즉시 저장이 커버)
    UFUNCTION(BlueprintCallable)
    void RequestDeferredSave();

    UFUNCTION(BlueprintCallable)
    bool LoadGameData();

    // 현재 세이브 데이터 가져오기 (캐시 활용)
    UFUNCTION(BlueprintCallable)
    USaveGame_GameData* GetCurrentSaveData();

    // 캐시 무효화 (저장/로드 후 호출)
    void InvalidateCache();

    // 데이터 초기화 후 캐시 재저장으로 슬롯이 부활하는 것을 차단 — 게임 재시작까지 유효
    void SuppressSaving() { bSaveSuppressed = true; }

    // 세이브 슬롯 + 직원 초상화를 한 몸으로 제거. 초상화 파일명이 EmployeeID 라 슬롯만 지우면
    // ID 1 부터 다시 받는 신입이 지운 직원 얼굴을 물려받는다(OfficeGameMode 가 파일 존재 시 촬영을 스킵).
    // 게임 도중 호출 시에는 SuppressSaving() 을 먼저 부를 것 — 안 그러면 캐시가 슬롯을 되살린다.
    void WipeAllSaveData();

    // ========== 빌딩 티어 조회 ==========
    // 티어는 OfficeDataMap[idx].TierProgress 에, 빌딩은 Buildings[] 배열에 산다.
    // 소비처(HQ 조건·회사 등급·랭킹)가 각자 교차 조회하면 신축 빌딩 처리 같은 규칙이 갈리므로 여기서 단일 소유한다.

    // 이 빌딩의 현재 프로젝트 티어. 오피스 데이터가 아직 없는 신축 빌딩은 1.
    UFUNCTION(BlueprintCallable, Category = "Save|Tier")
    int32 GetBuildingTier(int32 BuildingIndex);

    // 보유 빌딩 중 최고 티어. 빌딩이 없으면 0.
    UFUNCTION(BlueprintCallable, Category = "Save|Tier")
    int32 GetMaxBuildingTier();

    // MinTier 이상인 빌딩 수.
    UFUNCTION(BlueprintCallable, Category = "Save|Tier")
    int32 CountBuildingsAtTier(int32 MinTier);

    // 이 빌딩의 산업. 건물별 패널 구성에 사용한다 (FindBuildingSaveData 는 private).
    UFUNCTION(BlueprintCallable, Category = "Save")
    ECompanyType GetBuildingCompanyType(int32 BuildingIndex);

    // ========== 본사 레벨 시스템 ==========

    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    int32 GetHQLevel();

    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    bool CanLevelUpHQ();

    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    bool TryLevelUpHQ();

    UPROPERTY(BlueprintAssignable, Category = "HQ Level")
    FOnHQLevelUp OnHQLevelUp;

    // 다음 레벨 행이 있는가 = 아직 만렙이 아닌가. HQ 패널의 레벨업 버튼/문구가 만렙을 구분하는 축.
    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    bool HasNextHQLevel();

    // ===== 산업 해금 (파생 상태 — 세이브 없음. HQLevel >= DT_CompanyInfo.RequiredHQLevel) =====

    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    bool IsIndustryUnlocked(ECompanyType CompanyType);

    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    int32 GetIndustryRequiredHQLevel(ECompanyType CompanyType);

    // 이번 빌드에 포함된 업종인가 (DT_CompanyInfo.bAvailableAtLaunch). 해금(IsIndustryUnlocked)과 다른 축 —
    // 잠김은 갈망 티저로 보여주지만 미가용은 UI 에서 접는다.
    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    bool IsIndustryAvailable(ECompanyType CompanyType);

    // ========== 회사 등급 시스템 ==========

    UFUNCTION(BlueprintCallable, Category = "Company Title")
    ECompanyTitle GetCompanyTitle();

    UFUNCTION(BlueprintCallable, Category = "Company Title")
    bool CanPromoteTitle();

    UFUNCTION(BlueprintCallable, Category = "Company Title")
    bool TryPromoteTitle();

    // 프로젝트 완료 시 카운터 증가 (프로젝트 시스템에서 호출)
    void RecordProjectCompletion(bool bIsAGrade, bool bIsSGrade);

    // ========== 오프라인 보상 시스템 ==========

    // PlayFab OnOfflineGainsRequested 리스너 — 빌딩별 StoredRevenue 에 오프라인 수익 누적
    // OfflineSeconds: 서버 시간 기반으로 이미 12h 캡 적용된 값 (UPlayFabManagerSubsystem::OfflineCapSeconds)
    void CalculateOfflineGains(float OfflineSeconds);

    // 오프라인 정산의 계산 본체 (CalculateOfflineGains 가 이걸 호출해 지급까지 마무리한다).
    // bApplyToSave=false 면 세이브(금고/운영 경과)를 전혀 건드리지 않는 dry-run — 밸런스 계측 치트 전용.
    // 60초 미만 스킵/브로드캐스트/저장/모달은 호출자 책임 — 여기선 순수 계산만.
    // @return 세이브 데이터가 없으면 false (OutResult 는 기본값)
    bool ComputeOfflineGains(float OfflineSeconds, bool bApplyToSave, FOfflineGainsResult& OutResult);

    // [dev 프리셋] 시드 중 PlayFab 리더보드 오염 차단.
    // 업로드만 막고 while(TryPromoteTitle()) 승격 루프는 유지한다 — 프리셋이 그 루프로 등급을 올린다.
    void SetLeaderboardUploadSuppressed(bool bSuppressed) { bLeaderboardUploadSuppressed = bSuppressed; }
    bool IsLeaderboardUploadSuppressed() const { return bLeaderboardUploadSuppressed; }

    UPROPERTY(BlueprintAssignable, Category = "Company Title")
    FOnCompanyTitleChanged OnCompanyTitleChanged;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugPrintSaveData()
    {
        if (UGameplayStatics::DoesSaveGameExist(GetActiveSaveSlotName(), 0))
        {
            USaveGame_GameData* LoadedData = Cast<USaveGame_GameData>(
                UGameplayStatics::LoadGameFromSlot(GetActiveSaveSlotName(), 0));

            if (LoadedData)
            {
                UE_LOG(LogTemp, Warning, TEXT("=== Save Data Debug ==="));
                UE_LOG(LogTemp, Warning, TEXT("Next Employee ID: %d"), LoadedData->GameData.NextEmployeeID);

                // 건물별 직원 수 출력
                int32 TotalEmployees = 0;
                for (const auto& Pair : LoadedData->GameData.OfficeDataMap)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Building %d: Employees=%d"), Pair.Key, Pair.Value.EmployeeList.Num());
                    for (const FEmployeeInstance& Employee : Pair.Value.EmployeeList)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  Employee ID: %d, Name: %s, Department: %d, Level: %d"),
                            Employee.EmployeeID, *Employee.EmployeeName, static_cast<int32>(Employee.Department), Employee.Level);
                    }
                    TotalEmployees += Pair.Value.EmployeeList.Num();
                }
                UE_LOG(LogTemp, Warning, TEXT("Total Employees (all buildings): %d"), TotalEmployees);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No save file found!"));
        }
    }
private:
    bool bLocalOfflineGainsSettledAfterLoad = false;
    FDateTime PendingLocalOfflineLastSaveUtc;

    // 디스크 쓰기는 공유 커밋 범위 안에서만 호출한다. 외부 공개는 모든 live 상태 적용 후 수행한다.
    bool PersistSnapshot(const FGameSaveData& Candidate);
    void PublishPersistedSnapshot();
    bool bSnapshotCommitInProgress = false;

    // 캐시된 세이브 데이터 (매번 로드하지 않기 위함)
    UPROPERTY()
    USaveGame_GameData* CachedSaveData = nullptr;

    // 최초 LoadGameData 완료 전에는 저장 금지 — 서브시스템 Initialize 단계에서 SaveGameData가
    // 호출되면 아직 로드 안 된 매니저들이 기본/빈 값(미션 None, 빌딩 0 등)을 내놓아 디스크 세이브를 덮어쓴다.
    bool bInitialLoadComplete = false;

    // 데이터 초기화 중 세이브 억제 플래그 (DeleteGameInSlot 직후 캐시 부활 차단)
    bool bSaveSuppressed = false;

    // DevMode 치트 세션 잠금 — PersistSnapshot 에서만 본다 (디스크 쓰기 유일 통로)
    bool bDevModeSaveLocked = false;

    // 지연 저장 타이머 (RequestDeferredSave) — SaveGameData 성공 시 해제해 중복 저장 방지
    FTimerHandle DeferredSaveTimerHandle;
    static constexpr float DeferredSaveDelaySeconds = 3.0f;

    // 6-2 오프라인 정산 보고 보관 (로그인 시점엔 UIBase 없음 → MainMap 준비 후 flush)
    bool bHasPendingOfflineReport = false;
    float PendingOfflineTotal = 0.0f;
    float PendingOfflineSeconds = 0.0f;
    bool bPendingOfflineCapReached = false;
    TArray<FOfflineGainEntry> PendingOfflineEntries;

    // 공장이 broadcast 중에 올린 벽돌 지급량. CalculateOfflineGains 진입부에서 리셋되고
    // 보류 필드 대입(Total/Seconds/Entries)에는 섞이지 않으므로 broadcast 이후에도 살아남는다.
    int64 PendingOfflineBricks = 0;

    // 타이머 콜백 (SaveGameData 는 bool 반환이라 직접 바인딩 불가)
    void OnDeferredSaveTimer();

    // BuildingIndex로 FBuildingSaveData를 찾아 반환 (없으면 nullptr)
    FBuildingSaveData* FindBuildingSaveData(int32 BuildingIndex);

    // 레벨업 연쇄 판정 (내부 호출용)

    // 리소스 변경 이벤트 핸들러 — 시총 변경 시 자동 승격 체크 + PlayFab 리더보드 업로드
    void OnResourceChangedHandler(EResourceType Type, int64 NewValue);

    // Money 주간매출 Diff 계산용 — Initialize 시 현재 값으로 셋팅
    int64 LastMoneyValue = 0;

    // dev 프리셋 시드 구간에서만 true — 수십억 시드가 실제 랭킹을 오염시키는 것을 막는다
    bool bLeaderboardUploadSuppressed = false;
};
