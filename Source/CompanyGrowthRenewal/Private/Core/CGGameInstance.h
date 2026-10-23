// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Global/GlobalAssetCache.h"
#include "Level/CGLevelScriptBase.h"
#include "Enum/CompanyType.h"
#include "Enum/GachaTier.h"
#include "Enum/MusicType.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/FactorySaveData.h"
#include "Data/RankingData.h"
#include "Office/OfficeNeighborhoodSnapshot.h"
#include "TimeCycle/TimeCycleContinuation.h"

#include "CGGameInstance.generated.h"

class ACGGameModeBase;
class ACGLevelScriptBase;
class UGlobalAssetCache;
class ABuildingBaseActor;
class UGameViewportClient;
class UTexture2D;
class IInputProcessor;
class SLevelTransitionOverlay;
class FLevelTransitionVisualState;
class UFont;
enum class ELoadingTipContext : uint8;

/**
 * 오피스 씬 진입 모드
 */
UENUM(BlueprintType)
enum class EOfficeMode : uint8
{
	Normal          UMETA(DisplayName = "일반 관리"),
	PromotionTest   UMETA(DisplayName = "승급 테스트"),
	Training        UMETA(DisplayName = "직원 강화"),
	FreeView        UMETA(DisplayName = "자유 관람"),
	Skyline         UMETA(DisplayName = "스카이라인 개발")
};

/**
 * 현재 맵 타입
 */
UENUM(BlueprintType)
enum class ECurrentMapType : uint8
{
	None            UMETA(DisplayName = "없음"),
	MainMap         UMETA(DisplayName = "메인 맵"),
	OfficeMap       UMETA(DisplayName = "오피스 맵"),
	RecruitmentMap  UMETA(DisplayName = "채용 맵"),
	WorldMap        UMETA(DisplayName = "세계지도 맵")
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UCGGameInstance : public UGameInstance
{
	GENERATED_BODY()

	UCGGameInstance();

	UPROPERTY()
	TObjectPtr<UGlobalAssetCache> GlobalAssetCache;

	FStreamableManager streamableManager;

public:
	UFUNCTION(BlueprintCallable, Category = "Level")
	void TransitionToLevel(const FString& LevelName, int32 BackgroundIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Level")
	bool IsLevelTransitionActive() const { return LevelTransitionState.IsValid(); }

#if !UE_BUILD_SHIPPING
	/**
	 * 레벨 이동 없이 전환 오버레이(Holding 외형 + 팁 밴드)를 Seconds 동안 화면에 붙잡아 둔다.
	 * 실제 전환 상태와 멤버를 공유하지 않는다 ― IsLevelTransitionActive 가 켜지면 매니저들이 갱신을 멈춘다.
	 */
	void PreviewLevelTransition(float Seconds, const FString& TipKey);
#endif

private:
	UPROPERTY()
	ACGGameModeBase* CurrentInGameMode;

	UPROPERTY()
	ACGLevelScriptBase* CurrentLevelScript;

	UPROPERTY()
	APlayerController* CurrentPlayerController;

	// 오피스 씬 관련 데이터 (레벨 전환 시 유지)
	// 현재 관리 중인 건물 인덱스
	UPROPERTY()
	int32 CurrentManagedBuildingIndex = INDEX_NONE;

	// 오피스에서 도시로 나올 때 도착한 카메라가 프레이밍할 건물. 레벨 전환이 카메라를 파괴하므로
	// 맵을 건너는 유일한 통로인 GI 가 인덱스를 들고 가고, 도착한 GameMode 가 1회 소비한다.
	UPROPERTY(Transient)
	int32 PendingMainMapFocusBuildingIndex = INDEX_NONE;

	// 도시 월드를 보존하지 않는 입장 시점의 시각 데이터. 채용 맵 왕복 동안 유지한다.
	UPROPERTY(Transient)
	FOfficeNeighborhoodSnapshot OfficeNeighborhoodSnapshot;

	// 도시 사진 촬영은 실기기에서 954ms 블로킹이다. 같은 건물·같은 도시 구성이면 다시 찍지 않는다.
	// 도시 복귀가 OfficeNeighborhoodSnapshot 을 비우므로(SetCurrentManagedBuilding(nullptr)) 캐시는 따로 산다.
	UPROPERTY(Transient)
	FOfficeNeighborhoodSnapshot CachedNeighborhoodSnapshot;
	int32 CachedNeighborhoodBuildingIndex = INDEX_NONE;
	int32 CachedNeighborhoodCityStamp = INDEX_NONE;
	// 증축·이동·사무실 확장은 건물 수를 바꾸지 않지만 높이·기준점·촬영 시점을 바꾼다.
	FTransform CachedNeighborhoodBuildingTransform = FTransform::Identity;
	FIntPoint CachedNeighborhoodTileCount = FIntPoint::NoneValue;

	UPROPERTY(Transient)
	FTimeCycleContinuation TimeCycleContinuation;

	// 다음 건물에 할당할 인덱스 (새 건물 생성 시 자동 증가)
	UPROPERTY()
	int32 NextBuildingIndex = 1;

	EOfficeMode CurrentOfficeMode = EOfficeMode::Normal;

	// 현재 관리 중인 건물의 회사 타입
	ECompanyType CurrentBuildingCompanyType = ECompanyType::None;

	// 현재 맵 타입
	ECurrentMapType CurrentMapType = ECurrentMapType::None;

	virtual void Init() override;
	virtual void OnStart() override;
	virtual void Shutdown() override;

	FTSTicker::FDelegateHandle TickDelegateHandle;
	bool BeginLevelTransition(const FString& LevelName, int32 BackgroundIndex, bool bDestinationAlreadyLoaded = false);
	bool TickLevelTransition(float DeltaTime);
	bool IsLevelTransitionDestinationReady();
	void FinishLevelTransition();
	void PrepareLevelLoadingScreen();
	void PerformLevelTransition(const FString& LevelName, int32 BackgroundIndex);
	bool TrySaveGameBeforeLevelTransition();

	// 캐시가 이 건물·이 도시 구성과 맞으면 재사용하고, 아니면 지금 찍는다.
	void BuildOfficeNeighborhoodSnapshot(ABuildingBaseActor* Building);

	// 페이드가 도는 동안 목적지 맵을 비동기로 읽어둔다. LoadMap 이 안에서 전체 GC 퍼지를 먼저 돌리므로
	// 이 UPROPERTY 참조가 없으면 미리 읽은 패키지가 그대로 버려진다.
	void BeginLevelPreload(const FString& LevelName);
	void ReleaseLevelPreload();
	static FString ResolveLevelPackageName(const FString& LevelName);

	UPROPERTY(Transient)
	TObjectPtr<UPackage> PreloadedLevelPackage;
	// 패키지만 잡으면 GC 가 그 안의 월드를 치워 LoadMap 이 "Failed to load package" 로 떨어진다 — 월드를 직접 잡아야 산다
	UPROPERTY(Transient)
	TObjectPtr<UWorld> PreloadedLevelWorld;
	FString PreloadedLevelPackageName;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LevelTransitionPattern;

	// 로딩 스레드가 그리는 동안 GC 가 돌므로 GI 가 폰트 참조를 쥐고 있어야 한다 (패턴 텍스처와 같은 이유)
	UPROPERTY(Transient)
	TObjectPtr<UFont> TransitionTipBodyFont;
	UPROPERTY(Transient)
	TObjectPtr<UFont> TransitionTipLabelFont;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TransitionTipIcon;

	// 전환 1회당 1개. 뷰포트 위젯과 MoviePlayer 위젯이 같은 문구를 써야 경계에서 팁이 바뀌지 않는다
	FText CurrentTransitionTip;
	FName LastTransitionTipKey;
	void PickTransitionTip(ELoadingTipContext Context);
	static ELoadingTipContext ContextForLevelName(const FString& LevelName);

#if !UE_BUILD_SHIPPING
	bool TickPreviewTransition(float DeltaTime);
	void FinishPreviewTransition();
	TSharedPtr<FLevelTransitionVisualState, ESPMode::ThreadSafe> PreviewTransitionState;
	TSharedPtr<SLevelTransitionOverlay> PreviewTransitionWidget;
	TWeakObjectPtr<UGameViewportClient> PreviewTransitionViewport;
	FTSTicker::FDelegateHandle PreviewTickHandle;
	double PreviewTransitionEndTime = 0.0;
#endif

	TSharedPtr<FLevelTransitionVisualState, ESPMode::ThreadSafe> LevelTransitionState;
	TSharedPtr<SLevelTransitionOverlay> LevelTransitionViewportWidget;
	TSharedPtr<IInputProcessor> LevelTransitionInputBlocker;
	TWeakObjectPtr<UGameViewportClient> LevelTransitionViewport;
	FDelegateHandle PrepareLoadingScreenHandle;
	FString PendingTransitionLevel;
	int32 PendingTransitionBackground = 0;
	ECurrentMapType TransitionSourceMapType = ECurrentMapType::None;
	double TransitionPhaseStartedAt = 0.0;
	double TransitionStartedAt = 0.0;
	uint64 TransitionCoveredFrame = 0;
	uint64 TransitionReadyFrame = 0;
	// 패키지·월드 스트리밍이 끝난 시각. 렌더 에셋 스트리밍은 이 시점부터 상한을 두고만 기다린다.
	double TransitionCoreResourcesReadyAt = 0.0;
	// 목적지 준비 시각. 정착 프레임 대기에 시간 상한을 같이 거는 기준.
	double TransitionReadyStartedAt = 0.0;
	bool bTransitionTravelRequested = false;
	bool bTransitionDestinationLoaded = false;
	bool bTransitionResourcesPending = false;
	bool bPendingVisitMode = false;
	bool bHasPendingVisitMode = false;
	bool bClearVisitDataAfterTravel = false;
	bool bTransitionSourceVisitMode = false;
	TWeakObjectPtr<UWorld> TransitionSourceWorld;

	// 모든 레벨 로드 완료 시점에 BGM 자동 트리거 (첫 부팅 + TransitionToLevel 둘 다 cover)
	FDelegateHandle PostLoadMapHandle;
	void OnPostLoadMapWithWorld(UWorld* LoadedWorld);
	EMusicType ResolveMusicTypeForCurrentMap() const;

	// PlayFab 로그인 성공 → Firebase 토큰 교환 트리거 (설계서 §3.1)
	FDelegateHandle PlayFabLoginHandle;
	void HandlePlayFabLoginComplete(bool bSuccess);

	// ===== 앱 라이프사이클 훅 — 백그라운드 진입 시 즉시 저장, 복귀 시 오프라인 catchup 재실행 =====
	FDelegateHandle EnterBackgroundHandle;
	FDelegateHandle EnterForegroundHandle;
	void HandleAppEnterBackground();
	void HandleAppEnterForeground();
	double BackgroundEnterRealtime = -1.0;  // 백그라운드 진입 시각(FPlatformTime::Seconds). 복귀 체류시간 판정용

	// 도시 액터가 없는 맵에서만 유지하는 생산 시계. 레벨 로드와 백그라운드 중에는 멈춘다.
	TMap<FName, FFactoryAwayProductionClock> AwayFactoryProduction;
	FTSTicker::FDelegateHandle AwayFactoryTickHandle;
	bool bAwayFactoryTravelPaused = false;
	bool bAwayFactorySettlementInProgress = false;
	bool TickAwayFactoryProduction(float DeltaTime);
	void BeginAwayFactoryProductionFromCity();
	void PauseAwayFactoryProduction();
	void ResumeAwayFactoryProduction();
	void StopAwayFactoryProduction();

public:
	static UCGGameInstance* Instance;

	const FTimeCycleContinuation& GetTimeCycleContinuation() const { return TimeCycleContinuation; }
	void SetTimeCycleContinuation(const FTimeCycleContinuation& InState) { TimeCycleContinuation = InState; }

	static UCGGameInstance* GetInstance()
	{
		return Instance;
	}

	UGlobalAssetCache* GetGlobalAssetCache() const
	{
		return GlobalAssetCache;
	}


	ACGGameModeBase* GetCurrentInGameMode() const
	{
		return CurrentInGameMode;
	}

	ACGLevelScriptBase* GetCurrentLevelScript() const
	{
		return CurrentLevelScript;
	}

	void SetCurrentPlayerController(APlayerController* playerController);
	APlayerController* GetCurrentPlayerController();

	void SetCurrentLevelScript(ACGLevelScriptBase* cgLevelScriptBase);

public: 
	UFUNCTION(BlueprintCallable, Category = "Save/Load")
	void SaveGameBeforeLevelTransition();

	// 저장 스냅샷의 리소스 은행 수집 직전에 정산하고, 공장별 소수 잔량을 같은 스냅샷에 넣는다.
	bool AccrueAwayFactoryProduction();
	void CopyAwayFactoryProductionFractions(TMap<FName, FFactorySaveData>& InOutFactories) const;

	UFUNCTION(BlueprintCallable, Category = "Save/Load")
	void LoadGameAfterLevelStart();

public: // Office System
	// 오피스 씬 관련 데이터 관리
	UFUNCTION(BlueprintCallable, Category = "Office")
	void SetCurrentManagedBuilding(ABuildingBaseActor* Building);

	UFUNCTION(BlueprintCallable, Category = "Office")
	void SetOfficeMode(EOfficeMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Office")
	EOfficeMode GetOfficeMode() const;

	UFUNCTION(BlueprintCallable, Category = "Office")
	void SetCurrentBuildingCompanyType(ECompanyType Type) { CurrentBuildingCompanyType = Type; }

	UFUNCTION(BlueprintPure, Category = "Office")
	ECompanyType GetCurrentBuildingCompanyType() const { return CurrentBuildingCompanyType; }

	// 현재 관리 중인 건물 인덱스 (int32)
	UFUNCTION(BlueprintCallable, Category = "Office")
	int32 GetCurrentManagedBuildingIndex() const { return CurrentManagedBuildingIndex; }

	UFUNCTION(BlueprintCallable, Category = "Office")
	void SetCurrentManagedBuildingIndex(int32 InIndex);

	// 사무실 이름판·TV·채용 안내 공용 회사 이름 ― 「게임회사 2」처럼 업종 이름에 같은 업종 안 지은 순서를 붙인다(첫 회사도 1)
	FText GetCompanyDisplayName(int32 BuildingIndex) const;

	UFUNCTION(BlueprintPure, Category = "Office")
	const FOfficeNeighborhoodSnapshot& GetOfficeNeighborhoodSnapshot() const { return OfficeNeighborhoodSnapshot; }

	// 건물 패널이 열려 있는 동안 목적지 맵을 비동기로 읽어둔다. 로딩 스레드에서 돌아 프레임을 멈추지 않으므로
	// 캡처와 달리 미리 해도 공짜다. 입장 없이 패널을 닫으면 CancelLevelPreloadIfIdle 로 되돌린다.
	void PrewarmLevelPreload(const FString& LevelName) { BeginLevelPreload(LevelName); }
	void CancelLevelPreloadIfIdle();

	// 오피스를 떠나기 직전 기록. TransitionToLevel 이 자동으로 채우므로, 관리 건물을 먼저 비우는
	// 경로(ReturnToMainMap)만 명시 호출이 필요하다.
	void SetPendingMainMapFocusBuilding(int32 InIndex) { PendingMainMapFocusBuildingIndex = InIndex; }

	// 읽는 즉시 비운다 — 안 비우면 다음 도시 진입(랭킹 방문 복귀 등)이 묵은 인덱스로 또 포커스한다
	int32 ConsumePendingMainMapFocusBuilding();

	// 다음 빌딩 인덱스 할당
	UFUNCTION(BlueprintCallable, Category = "Building")
	int32 AllocateNextBuildingIndex() { return NextBuildingIndex++; }

	// NextBuildingIndex Getter/Setter (저장/로드용)
	int32 GetNextBuildingIndex() const { return NextBuildingIndex; }
	void SetNextBuildingIndex(int32 InIndex) { NextBuildingIndex = InIndex; }

public: // Map Type
	UFUNCTION(BlueprintCallable, Category = "MapType")
	void SetCurrentMapType(ECurrentMapType MapType) { CurrentMapType = MapType; }

	UFUNCTION(BlueprintPure, Category = "MapType")
	ECurrentMapType GetCurrentMapType() const { return CurrentMapType; }

	UFUNCTION(BlueprintPure, Category = "MapType")
	bool IsInOfficeMap() const { return CurrentMapType == ECurrentMapType::OfficeMap; }

public: // Recruitment System
	// 채용맵으로 전환 (가챠 결과를 저장 후 전환)
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void TransitionToRecruitmentMap(const FGachaResultData& Result, int32 BackgroundIndex = 0);

	// 채용 완료 후 오피스맵으로 복귀
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void ReturnFromRecruitmentMap();

	// 대기 중인 가챠 결과
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	const FGachaResultData& GetPendingGachaResult() const { return PendingGachaResult; }

	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	bool HasPendingGachaResult() const { return bHasPendingGachaResult; }

private:
	FGachaResultData PendingGachaResult;
	bool bHasPendingGachaResult = false;

	// 마지막 채용 직원 ID (OfficeMap 자동 선택용, 런타임 전용)
	int32 LastRecruitedEmployeeID = INDEX_NONE;

public:
	void SetLastRecruitedEmployeeID(int32 ID) { LastRecruitedEmployeeID = ID; }
	int32 GetLastRecruitedEmployeeID() const { return LastRecruitedEmployeeID; }
	void ClearLastRecruitedEmployeeID() { LastRecruitedEmployeeID = INDEX_NONE; }

	UFUNCTION(Exec)
	void TestClothingRank(int32 RankValue);

	UFUNCTION(Exec)
	void ListEmployees();

public: // Visit Mode (랭킹 도시 방문)
	UFUNCTION(BlueprintCallable, Category = "Visit")
	void SetVisitMode(bool bInVisitMode);

	UFUNCTION(BlueprintPure, Category = "Visit")
	bool IsVisitMode() const;

	void SetVisitData(const FCitySnapshot& InSnapshot, const FString& InPlayFabId, const FString& InDisplayName)
	{
		VisitCitySnapshot = InSnapshot;
		VisitTargetPlayFabId = InPlayFabId;
		VisitTargetDisplayName = InDisplayName;
	}

	void ClearVisitData()
	{
		if (IsLevelTransitionActive())
		{
			bClearVisitDataAfterTravel = true;
			return;
		}
		VisitCitySnapshot = FCitySnapshot();
		VisitTargetPlayFabId.Empty();
		VisitTargetDisplayName.Empty();
	}

	UFUNCTION(BlueprintPure, Category = "Visit")
	const FCitySnapshot& GetVisitCitySnapshot() const { return VisitCitySnapshot; }

	UFUNCTION(BlueprintPure, Category = "Visit")
	const FString& GetVisitTargetDisplayName() const { return VisitTargetDisplayName; }

	UFUNCTION(BlueprintPure, Category = "Visit")
	const FString& GetVisitTargetPlayFabId() const { return VisitTargetPlayFabId; }

private:
	bool bIsVisitMode = false;
	FCitySnapshot VisitCitySnapshot;
	FString VisitTargetPlayFabId;
	FString VisitTargetDisplayName;
};
