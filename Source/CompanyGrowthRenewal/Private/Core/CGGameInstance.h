// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Global/GlobalAssetCache.h"
#include "Level/CGLevelScriptBase.h"
#include "Enum/LootBoxCategory.h"
#include "Enum/CompanyType.h"
#include "Enum/GachaTier.h"
#include "Enum/MusicType.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/RankingData.h"

#include "CGGameInstance.generated.h"

class ACGGameModeBase;
class ACGLevelScriptBase;
class UGlobalAssetCache;
class ABuildingBaseActor;

/**
 * 오피스 씬 진입 모드
 */
UENUM(BlueprintType)
enum class EOfficeMode : uint8
{
	Normal          UMETA(DisplayName = "일반 관리"),
	PromotionTest   UMETA(DisplayName = "승급 테스트"),
	Training        UMETA(DisplayName = "직원 강화"),
	FreeView        UMETA(DisplayName = "자유 관람")
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
	LootBoxMap      UMETA(DisplayName = "뽑기 맵"),
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
	void TransitionToLevel(const FString& LevelName, int32 BackgroundIndex = 0);

private:
	UPROPERTY()
	ACGGameModeBase* CurrentInGameMode;

	UPROPERTY()
	ACGLevelScriptBase* CurrentLevelScript;

	UPROPERTY()
	APlayerController* CurrentPlayerController;

	// LootBox 시스템 - 현재 선택된 카테고리
	ELootBoxCategory CurrentLootBoxCategory = ELootBoxCategory::BuildingSkin;

	// 오피스 씬 관련 데이터 (레벨 전환 시 유지)
	// 현재 관리 중인 건물 인덱스
	UPROPERTY()
	int32 CurrentManagedBuildingIndex = INDEX_NONE;

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

	// Tick
	FTSTicker::FDelegateHandle TickDelegateHandle;
	/*bool Tick(float DeltaTime);*/

	// 모든 레벨 로드 완료 시점에 BGM 자동 트리거 (첫 부팅 + TransitionToLevel 둘 다 cover)
	FDelegateHandle PostLoadMapHandle;
	void OnPostLoadMapWithWorld(UWorld* LoadedWorld);
	EMusicType ResolveMusicTypeForCurrentMap() const;

	// ===== 앱 라이프사이클 훅 — 백그라운드 진입 시 즉시 저장, 복귀 시 오프라인 catchup 재실행 =====
	FDelegateHandle EnterBackgroundHandle;
	FDelegateHandle EnterForegroundHandle;
	void HandleAppEnterBackground();
	void HandleAppEnterForeground();
	double BackgroundEnterRealtime = -1.0;  // 백그라운드 진입 시각(FPlatformTime::Seconds). 복귀 체류시간 판정용

public:
	static UCGGameInstance* Instance;

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

	UFUNCTION(BlueprintCallable, Category = "Save/Load")
	void LoadGameAfterLevelStart();

public: // LootBox System
	// LootBox 카테고리 관리
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void SetCurrentLootBoxCategory(ELootBoxCategory Category);

	UFUNCTION(BlueprintCallable, Category = "LootBox")
	ELootBoxCategory GetCurrentLootBoxCategory() const;

	// LootBoxMap으로 전환 (카테고리 지정)
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void TransitionToLootBoxMap(ELootBoxCategory Category, int32 BackgroundIndex = 0);

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
	void SetCurrentManagedBuildingIndex(int32 InIndex) { CurrentManagedBuildingIndex = InIndex; }

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
	void SetVisitMode(bool bInVisitMode) { bIsVisitMode = bInVisitMode; }

	UFUNCTION(BlueprintPure, Category = "Visit")
	bool IsVisitMode() const { return bIsVisitMode; }

	void SetVisitData(const FCitySnapshot& InSnapshot, const FString& InPlayFabId, const FString& InDisplayName)
	{
		VisitCitySnapshot = InSnapshot;
		VisitTargetPlayFabId = InPlayFabId;
		VisitTargetDisplayName = InDisplayName;
	}

	void ClearVisitData()
	{
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

