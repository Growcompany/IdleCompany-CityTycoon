// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Engine/GameInstance.h"
#include "Manager/EmployeeManager.h"
#include "Data/LootBoxInventoryData.h"
#include "Table/BuildingSkinData.h"
#include "Data/EntitySaveData.h"
#include "Data/FactorySaveData.h"
#include "Data/ProductionOrderData.h"
#include "Data/GameAudioSettings.h"
#include "Data/GameSettings.h"
#include "Data/RecruitmentData.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/ShopSaveData.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/CityAcqSaveData.h"
#include "Data/TrendState.h"
#include "Data/ShippedRecord.h"
#include "Data/BuildingSkinGachaSaveData.h"
#include "Data/WorldMapTypes.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Enum/ResourceType.h"
#include "Enum/RawMaterialType.h"
#include "Enum/ItemType.h"
#include "Enum/CompanyTitle.h"
#include "Entity/Country/CountryActor.h"
#include "GameSaveData.generated.h"

USTRUCT(BlueprintType)
struct FGameSaveData
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    TArray<FEmployeeInstance> EmployeeList;

    UPROPERTY(SaveGame)
    int32 NextEmployeeID = 1;

    UPROPERTY(SaveGame)
    TMap<int32, FEmployeeAppearanceData> EmployeeAppearances;

    // ===== LootBox & Inventory System =====

    // 룩박스 인벤토리 (카테고리별 보유 상자 현황)
    UPROPERTY(SaveGame)
    FLootBoxInventoryData LootBoxInventory;

    // 보유 중인 건물 스킨 목록
    UPROPERTY(SaveGame)
    TArray<FBuildingSkinInstance> OwnedBuildingSkins;

    // TODO: 향후 확장
    // UPROPERTY(SaveGame)
    // TArray<FBuildingItemInstance> OwnedBuildingItems;

    // ===== Building System =====

    // 스폰된 건물 데이터
    UPROPERTY(SaveGame, BlueprintReadOnly)
    TArray<FBuildingEntitySaveData> Buildings;

    // 오피스 데이터 (BuildingIndex를 Key로 사용)
    UPROPERTY(SaveGame)
    TMap<int32, FOfficeSaveData> OfficeDataMap;

    // 보유 재화 (Brick, Money, Diamond 등)
    UPROPERTY(SaveGame)
    TMap<EResourceType, int64> ResourceBank;

    // 건물 해금 레벨 (1~40, Building1부터 순차적으로 해금)
    UPROPERTY(SaveGame)
    int32 UnlockedConstructionLevel = 1;

    // Factory 데이터 (FactoryID를 키로 사용)
    UPROPERTY(SaveGame)
    TMap<FName, FFactorySaveData> FactoryData;

    // ===== Office Recruitment System =====

    // 오피스별 채용 데이터 (BuildingIndex를 Key로 사용)
    // 자리 수, 채용 카드, 타이머 등 관리
    UPROPERTY(SaveGame)
    TMap<int32, FOfficeRecruitmentData> OfficeRecruitmentMap;

    // 일반 채용권 지급에 반영된 계정 전체 영구 직원 정원의 최고치. 감소시키지 않는다.
    UPROPERTY(SaveGame)
    int32 CreditedPermanentEmployeeCapacity = 0;

    // ===== GDS Trend System =====

    // 산업별 트렌드 상태 (라이트 루프 — 소재 유행, 착수 3회마다 교체)
    UPROPERTY(SaveGame)
    TMap<ECompanyType, FTrendState> TrendStates;

    // 발견한 (산업|장르|소재) 조합 — 리뷰에서 등급 공개 시 기록, 도감(Phase 6)의 데이터
    UPROPERTY(SaveGame)
    TArray<FString> DiscoveredCombos;

    // 출시작 이력 (도감 [출시작] 탭)
    UPROPERTY(SaveGame)
    TArray<FShippedProjectRecord> ShippedProjects;

    // ===== Production Order System =====

    // 생산 주문서 목록 (제조업 양산 시스템)
    UPROPERTY(SaveGame)
    TArray<FProductionOrder> ProductionOrders;

    // 다음 주문 ID
    UPROPERTY(SaveGame)
    int32 NextProductionOrderID = 1;

    // ===== Building Index System =====

    // 다음 건물 인덱스 (새 건물 생성 시 사용)
    UPROPERTY(SaveGame)
    int32 NextBuildingIndex = 1;

    // ===== City Plot System =====

    // 소유한 도시 블록(부지) PlotId 목록 (DT_CityPlot RowName)
    UPROPERTY(SaveGame, BlueprintReadOnly)
    TArray<FName> OwnedPlotIds;

    // 도박 인수 상태 (key -> {state,R}) (2026-06-26)
    UPROPERTY(SaveGame)
    TMap<int32, FCityAcqSave> CityAcq;

    // ===== Mission System =====

    // 진행 중 미션 (DT_Mission RowName). NAME_None = 체인 끝/없음.
    // 신규 게임은 세이브 자체가 없으므로 MissionManager가 오프닝 시작 미션으로 초기화
    UPROPERTY(SaveGame)
    FName CurrentMissionID;

    // 현재 미션의 저장 가능한 카운트 진행도. M4에서는 책상 개수가 아니라 배치한 좌석 수다.
    UPROPERTY(SaveGame)
    int32 CurrentMissionProgress = 0;

    // 현재 미션의 조건 달성 후 수동 최종 클레임을 기다리는 상태.
    // CurrentMissionID와 함께 저장하며, 복원 시 체인 마지막 행에만 적용한다.
    UPROPERTY(SaveGame)
    bool bCurrentMissionReadyToClaim = false;

    // M7 피날레 수령까지 끝난 명시적 튜토리얼 완료 상태. GoalBoard 언락 여부를 대용하지 않는다.
    UPROPERTY(SaveGame)
    bool bTutorialCompleted = false;

    // ===== Goal Board (튜토리얼 체인 종료 후 미션판 — 스펙 2026-08-02 §5.2) =====
    UPROPERTY(SaveGame)
    bool bGoalBoardUnlocked = false;

    // 조건 충족 래치 (이벤트형 필수 — 도달형도 래치해 재평가 생략)
    UPROPERTY(SaveGame)
    TArray<FName> CompletedGoalIDs;

    UPROPERTY(SaveGame)
    TMap<FName, int64> GoalEventProgressByID;

    UPROPERTY(SaveGame)
    TArray<FName> ClaimedGoalIDs;

    // ===== Panel Intro (패널 최초 진입 코치마크 — 스펙 2026-08-12) =====

    // 코치마크를 이미 본 패널 (DT_PanelIntro.PanelKey). 미션 진행과 무관하게 패널별 1회
    UPROPERTY(SaveGame)
    TArray<FName> SeenPanelIntros;

    // 제스처 힌트 졸업 카운트 (키 = CatchHintRules::KeyDoze/KeyBolt, 값 = 캐치 성공 횟수)
    UPROPERTY(SaveGame)
    TMap<FName, int32> GestureHintCounts;

    // ===== Item Inventory System =====

    // 소비성 아이템 인벤토리 (채용권, 강화 재료 등)
    UPROPERTY(SaveGame)
    TMap<EItemType, int32> ItemInventory;

    // ===== Gacha Recruitment System =====

    // 가챠 뽑기 데이터 (천장, 마일리지)
    UPROPERTY(SaveGame)
    FGachaRecruitmentData GachaRecruitmentData;

    // ===== Shop System =====

    // 상점 구매 한도/리셋 상태
    UPROPERTY(SaveGame)
    FShopSaveData ShopSaveData;

    // ===== Building Trait System (BUILDING_TRAIT_SYSTEM v1.1) =====

    // 글로벌 특성 인벤토리 (보유 특성 + 도감 + dust + 가챠 천장/마일리지)
    UPROPERTY(SaveGame)
    FBuildingTraitInventory BuildingTraitInventory;

    // 스킨 가챠 천장/마일리지 (소유 스킨은 위 OwnedBuildingSkins 가 저장소)
    UPROPERTY(SaveGame)
    FBuildingSkinGachaInventory BuildingSkinGachaInventory;

    // ===== Audio Settings =====

    // 오디오 설정 (볼륨, 음소거 등)
    UPROPERTY(SaveGame)
    FGameAudioSettings AudioSettings;

    // 게임 설정 (그래픽 품질/FPS/연출 감소)
    UPROPERTY(SaveGame)
    FGameSettings GameSettings;

    // ===== 본사 레벨 시스템 =====
    UPROPERTY(SaveGame)
    int32 HQLevel = 1;

    // ===== 회사 등급 시스템 =====
    UPROPERTY(SaveGame)
    ECompanyTitle CompanyTitle = ECompanyTitle::Small;

    // ===== 누적 통계 (등급 조건 평가용) =====
    UPROPERTY(SaveGame)
    int32 TotalProjectsCompleted = 0;

    UPROPERTY(SaveGame)
    int32 AGradeProjectsCompleted = 0;

    UPROPERTY(SaveGame)
    int32 SGradeProjectsCompleted = 0;

    // ===== 랭킹 시스템 =====

    // 누적 총 매출 (랭킹 정렬 기준)
    UPROPERTY(SaveGame)
    int64 TotalRevenueEarned = 0;

    // 프로필 이미지 ID (DT_ProfileImage의 ImageID, 기본 0 = 빌딩1)
    UPROPERTY(SaveGame)
    int32 ProfileImageID = 0;

    // ===== 월드맵 생산 시스템 =====

    // 원자재 인벤토리 (WorldMapManager 소유)
    UPROPERTY(SaveGame)
    TMap<ERawMaterialType, int64> WorldMap_RawMaterialInventory;

    UPROPERTY(SaveGame)
    int64 WorldMap_EnergyCount = 0;

    UPROPERTY(SaveGame)
    int64 WorldMap_RefinedOilCount = 0;

    // 나라별 채광소 상태
    UPROPERTY(SaveGame)
    TArray<FMiningFacility> WorldMap_Mines;

    // 공장 라인 상태
    UPROPERTY(SaveGame)
    TArray<FFactoryLine> WorldMap_Lines;

    // 나라별 자동 배정 설정
    UPROPERTY(SaveGame)
    TMap<ECountryType, bool> WorldMap_AutoAssign;

    // 공장 라인 대기 중인 생산 주문서 큐
    UPROPERTY(SaveGame)
    TArray<FProductionOrder> WorldMap_PendingQueue;

    // 완성품 창고 (Key = FIntPoint(CompanyType, ProjectIndex))
    UPROPERTY(SaveGame)
    TMap<FIntPoint, int64> WorldMap_ProductInventory;

    // 완성품 등급 (uint8로 저장, EQualityGrade로 캐스트)
    UPROPERTY(SaveGame)
    TMap<FIntPoint, uint8> WorldMap_ProductGrades;

    // 마지막 저장 시각 (오프라인 catchup용)
    UPROPERTY(SaveGame)
    FDateTime WorldMap_LastSaveUtc;

    // 나라 × 산업 시장 수요 게이지 상태 (CountryMarketManager 소유)
    UPROPERTY(SaveGame)
    TArray<FCountryMarketState> WorldMap_CountryMarketStates;

    // ===== 채광 시스템 (UMineManager 신규) =====
    // 위 WorldMap_Mines (TArray<FMiningFacility>) 는 레거시 시스템. 이 필드가 새 시스템.
    // 오프라인 catchup 은 PlayFab OnOfflineGainsRequested 시퀀스가 권위 (12h 캡, 1.0x 효율 + Storage 자연 캡).
    UPROPERTY(SaveGame)
    TMap<ECountryType, FCountryMineData> WorldMap_MineData;

    // ===== 월드 공장 시스템 (UWorldFactoryManager) =====
    // 국가별 라인 진행/완성 상태 + 강화 레벨. LineElapsedSec 누적 모델 → 자체 LastTick 불필요.
    // 오프라인 catchup 은 PlayFab OnOfflineGainsRequested 시퀀스가 권위 (12h 캡, 0.2x 효율).
    UPROPERTY(SaveGame)
    TMap<ECountryType, FCountryFactoryData> WorldMap_FactoryData;

    // ===== 무역 주문서 시스템 =====

    UPROPERTY(SaveGame)
    TArray<FTradeOrder> TradeOrders_Active;

    UPROPERTY(SaveGame)
    int32 TradeOrders_NextOrderId = 1;

    UPROPERTY(SaveGame)
    int32 TradeOrders_ComboCount = 0;
};

UCLASS()
class COMPANYGROWTHRENEWAL_API USaveGame_GameData : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FGameSaveData GameData;

    UPROPERTY(VisibleAnywhere, Category = "Save")
    FString SaveSlotName = TEXT("GameSlot");

    UPROPERTY(VisibleAnywhere, Category = "Save")
    uint32 UserIndex = 0;
};

