// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "Table/BuildableCardTable.h"
#include "Table/EmployeeCardTable.h"
#include "Enum/InteractableType.h"
#include "Enum/WidgetType.h"
#include "Enum/ResourceType.h"
#include "Table/InteractableInfo.h"
#include "Table/BuildingData.h"
#include "Table/CharacterAppearanceTable.h"
#include "Table/WorkerCosmeticTable.h"
#include "Table/LootBoxData.h"
#include "Table/ResourceInfo.h"
#include "Table/BuildingSkinData.h"
#include "Table/BuildingLightData.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Table/DecorationCardTable.h"
#include "Table/DecorationData.h"
#include "Table/WorkstationCardTable.h"
#include "Table/WorkstationTable.h"
#include "Table/TierRankTitleTable.h"
#include "Table/UIIconData.h"
#include "Table/MoneyVFXTable.h"
#include "Table/ProjectDataTable.h"
#include "Table/ProjectGenreTable.h"
#include "Table/IndustryProfileTable.h"
#include "Table/CriticDisplayTable.h"
#include "Table/ReviewCommentTable.h"
#include "Enum/ProductionDiscipline.h"
#include "Table/BoostGambleTable.h"
#include "Table/VFXTable.h"
#include "Table/TierUnlockData.h"
#include "Table/CountryInfoTable.h"
#include "Table/CountryDemandTable.h"
#include "Table/CountryMarketRoleTable.h"
#include "Table/CompanyInfoTable.h"
#include "Table/HQLevelData.h"
#include "Table/MissionTable.h"
#include "Table/GoalTable.h"
#include "Table/PanelIntroTable.h"
#include "Table/MenuUnlockData.h"
#include "Table/ProfileImageData.h"
#include "Table/ProductRecipeTable.h"
#include "Table/DepartmentDisplayTable.h"
#include "Table/QualityGradeDisplayTable.h"
#include "Table/EmployeePotentialDisplayTable.h"
#include "Table/TradeOrderTierDisplayTable.h"
#include "Table/ProjectModeDisplayTable.h"
#include "Table/StepDisplayNameTable.h"
#include "Table/DisciplineDisplayTable.h"
#include "Table/UIVFXTexTable.h"
#include "Table/LaunchLootBandTable.h"
#include "Table/ShopItemTable.h"
#include "Table/LaunchLootTable.h"
#include "Table/TradeOrderBalanceTable.h"
#include "Table/MarketBalanceTable.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Data/MineUpgradeData.h"
#include "Data/FactoryUpgradeData.h"
#include "Table/BuildingTraitTable.h"
#include "Table/BuildingTraitSetBonusTable.h"
#include "Enum/BuildingTraitCategory.h"
#include "Table/OfficeStarterPresetTable.h"
#include "Table/EventChoiceIconTable.h"
#include "Table/CityPlotData.h"
#include "Table/CityDressingData.h"
#include "Table/CityCompanyData.h"
#include "TableManagerSubsystem.generated.h"

UCLASS()
class COMPANYGROWTHRENEWAL_API UTableManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UTableManagerSubsystem();

    /** Subsystem 초기화 */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** WidgetClass 반환 */
    UFUNCTION(BlueprintCallable, Category = "UI")
    TSubclassOf<UUserWidget> GetWidgetClass(EWidgetType Type) const;

    // Buildable
    UFUNCTION(BlueprintCallable, Category = "Build")
    TArray<FBuildableCardTable> GetBuildableInfos() const;

    UFUNCTION(BlueprintCallable, Category = "Build")
    bool GetBuildableInfo(FName RowName, FBuildableCardTable& OutInfo) const;

    // EmployeeCard
    UFUNCTION(BlueprintCallable, Category = "Build")
    TArray<FEmployeeCardTable> GetEmployeeCardInfos() const;

    // Interactable

    // 주어진 RowName에 해당하는 FInteractableInfo (메인 테이블) 데이터를 가져옵니다.
    UFUNCTION(BlueprintCallable, Category = "Interactable")
    FInteractableInfo GetInteractableInfo(FName RowName, bool& bOutSuccess) const;

    // 주어진 RowName에 해당하는 FBuildingData를 가져옵니다.
    UFUNCTION(BlueprintCallable, Category = "Table Manager")
    FBuildingData GetBuildingData(FName RowName, bool& bOutSuccess) const;

    // 테스트용 ProductionOrder 일괄 spawn 시드
    UFUNCTION(BlueprintCallable, Category = "Test")
    UDataTable* GetTestProductionOrdersTable() const { return TestProductionOrdersTable; }

    // 테스트용 자원 시나리오 시드 (Default/Rich/Poor/Empty)
    UFUNCTION(BlueprintCallable, Category = "Test")
    UDataTable* GetTestResourceScenariosTable() const { return TestResourceScenariosTable; }

    // 개발용 진행 상태 프리셋 (Early/Mid/Late)
    UFUNCTION(BlueprintCallable, Category = "Test")
    UDataTable* GetDevProgressPresetTable() const { return DevProgressPresetTable; }

    // 프리셋별 빌딩 스펙 행 (PresetID 로 묶임)
    UFUNCTION(BlueprintCallable, Category = "Test")
    UDataTable* GetDevPresetBuildingTable() const { return DevPresetBuildingTable; }

    // EmployeeManager에서 접근할 수 있도록 Getter 함수들
    UFUNCTION(BlueprintCallable, Category = "Employee Names")
    UDataTable* GetLastNameDataTable() const { return LastNameDataTable; }

    UFUNCTION(BlueprintCallable, Category = "Employee Names")
    UDataTable* GetFirstNameDataTable() const { return FirstNameDataTable; }

    // Character Appearance Data
    FHairPartTable* GetHairPartByName(const FString& PartName, EEmployeeGender Gender);
    FRankClothingTable* GetClothingForRank(EEmployeeRank Rank, EEmployeeGender Gender);
    FEyeColorTable* GetRandomEyeColor();
    FSkinColorTable* GetRandomSkinColor();
    FHairColorTable* GetRandomHairColor();
    FCharacterBaseMeshTable* GetCharacterBaseMesh(EEmployeeGender Gender);

    // Worker Cosmetic (AStickOfficeworker 외형: 색 + 안경). RowName = "D{dept}_R{rank}_T{tier}".
    UFUNCTION(BlueprintCallable, Category = "Worker Cosmetic")
    FWorkerCosmeticTable GetWorkerCosmeticData(FName RowName, bool& bOutSuccess) const;

    // LootBox Data
    UFUNCTION(BlueprintCallable, Category = "LootBox")
    FLootBoxTable GetLootBoxData(FName RowName, bool& bOutSuccess) const;

    // 카테고리별 룩박스 목록 조회 (RowName 포함)
    // Note: TPair는 UFUNCTION에서 지원되지 않으므로 C++ 전용
    TArray<TPair<FName, FLootBoxTable>> GetLootBoxesByCategoryWithNames(ELootBoxCategory Category) const;

    // Resource Data
    UFUNCTION(BlueprintCallable, Category = "Resource")
    FResourceInfo GetResourceInfo(EResourceType ResourceType, bool& bOutSuccess) const;

    // BuildingSkin Data
    UFUNCTION(BlueprintCallable, Category = "BuildingSkin")
    FBuildingSkinData GetBuildingSkinData(int32 SkinID, bool& bOutSuccess) const;

    // 희귀도별 BuildingSkin 목록 조회
    UFUNCTION(BlueprintCallable, Category = "BuildingSkin")
    TArray<FBuildingSkinData> GetBuildingSkinsByRarity(ELootBoxRarity Rarity) const;

    // BuildingLight Data
    UFUNCTION(BlueprintCallable, Category = "BuildingLight")
    FBuildingLightData GetBuildingLightData(int32 LightID, bool& bOutSuccess) const;

    UFUNCTION(BlueprintCallable, Category = "BuildingLight")
    TArray<FBuildingLightData> GetBuildingLightsByRarity(ELootBoxRarity Rarity) const;

    // 모든 BuildingLight 행 (희귀도 무관, 카드 일괄 표시용)
    UFUNCTION(BlueprintCallable, Category = "BuildingLight")
    TArray<FBuildingLightData> GetAllBuildingLights() const;

    // 모든 테이블이 완전히 초기화되었는지 확인
    UFUNCTION(BlueprintCallable, Category = "Table Manager")
    bool IsFullyInitialized() const;

    // Decoration Data
    UFUNCTION(BlueprintCallable, Category = "Decoration")
    TArray<FDecorationCardTable> GetDecorationCardInfos() const;

    UFUNCTION(BlueprintCallable, Category = "Decoration")
    TArray<FDecorationCardTable> GetDecorationCardsByCategory(EDecorationCategory Category) const;

    UFUNCTION(BlueprintCallable, Category = "Decoration")
    TArray<FDecorationCardTable> GetDecorationCardsBySurface(EDecorationSurface Surface) const;

    UFUNCTION(BlueprintCallable, Category = "Decoration")
    FDecorationCardTable GetDecorationCardInfo(FName RowName, bool& bOutSuccess) const;

    UFUNCTION(BlueprintCallable, Category = "Decoration")
    FDecorationData GetDecorationData(FName RowName, bool& bOutSuccess) const;

    // OfficeStarterPreset
    UFUNCTION(BlueprintCallable, Category = "Office")
    FOfficeStarterPresetRow GetStarterPresetRow(FName RowName, bool& bOutSuccess) const;

    // FootprintCells 버킷 + BuildingIndex로 변형 결정 (BuildingIndex % 해당 버킷 변형 수). 행 없으면 bOutSuccess=false
    UFUNCTION(BlueprintCallable, Category = "Office")
    FOfficeStarterPresetRow GetStarterPresetForBuilding(int32 FootprintCells, int32 BuildingIndex, bool& bOutSuccess) const;

    UFUNCTION(BlueprintCallable, Category = "Office")
    FIndustryMoodPaletteRow GetIndustryMoodPalette(ECompanyType Industry, bool& bOutSuccess) const;

    // Workstation Data
    UFUNCTION(BlueprintCallable, Category = "Workstation")
    TArray<FWorkstationCardTable> GetWorkstationCardInfos() const;

    UFUNCTION(BlueprintCallable, Category = "Workstation")
    FWorkstationCardTable GetWorkstationCardInfo(FName RowName, bool& bOutSuccess) const;

    UFUNCTION(BlueprintCallable, Category = "Workstation")
    TArray<FWorkstationCardTable> GetWorkstationCardsByType(EWorkstationType InWorkstationType) const;

    // Workstation Item Skin Data
    UFUNCTION(BlueprintCallable, Category = "Workstation")
    TArray<FWorkstationItemSkinData> GetWorkstationItemSkinInfos() const;

    UFUNCTION(BlueprintCallable, Category = "Workstation")
    FWorkstationItemSkinData GetWorkstationItemSkinInfo(FName RowName, bool& bOutSuccess) const;

    UFUNCTION(BlueprintCallable, Category = "Workstation")
    TArray<FWorkstationItemSkinData> GetWorkstationItemSkinsBySlot(EWorkstationSlot Slot) const;

    /** 책상 선형 업그레이드 행. 누락 시 nullptr이며 코드 폴백을 만들지 않는다. */
    const FComputerSetupLevelData* FindWorkstationSetupLevelData(EComputerSetupLevel Level) const;

    // UI Icon Data
    UFUNCTION(BlueprintCallable, Category = "UI Icon")
    FUIIconData GetUIIconData(FName RowName, bool& bOutSuccess) const;

    UFUNCTION(BlueprintCallable, Category = "UI Icon")
    TArray<FUIIconData> GetUIIconsByCategory(EUIIconCategory Category) const;

    // Money VFX Data
    UFUNCTION(BlueprintCallable, Category = "VFX")
    FMoneyVFXTable GetMoneyVFXByAmount(int32 Amount, bool& bOutSuccess) const;

    // General VFX Data
    UFUNCTION(BlueprintCallable, Category = "VFX")
    FVFXTableRow GetVFXData(EVFXType VFXType, bool& bOutSuccess) const;

    // VFX 에셋 직접 반환 (로드 포함)
    UFUNCTION(BlueprintCallable, Category = "VFX")
    UNiagaraSystem* GetVFXAsset(EVFXType VFXType);

    // Tier Unlock Data
    UFUNCTION(BlueprintCallable, Category = "Tier Unlock")
    FTierUnlockData GetTierUnlockData(int32 Tier, bool& bOutSuccess) const;

    // 1..Tier 까지 누적 해금된 강화 타입 전체
    UFUNCTION(BlueprintCallable, Category = "Tier Unlock")
    TArray<EBuildingEnhancementType> GetUnlockedEnhancementsUpToTier(int32 Tier) const;

    // HQ Level Data
    UFUNCTION(BlueprintCallable, Category = "HQ Level")
    FHQLevelData GetHQLevelData(int32 Level, bool& bOutSuccess) const;

    UFUNCTION(BlueprintPure, Category = "HQ Level")
    int32 GetMaxHQLevel() const { return HQLevelTable.Num(); }

    // Menu Unlock Data
    UFUNCTION(BlueprintCallable, Category = "Menu")
    FMenuUnlockData GetMenuUnlockData(FName ButtonName, bool& bOutSuccess) const;

    // Profile Image Data
    UFUNCTION(BlueprintCallable, Category = "Profile")
    FProfileImageData GetProfileImageData(int32 ImageID, bool& bOutSuccess) const;

    UFUNCTION(BlueprintCallable, Category = "Profile")
    TArray<FProfileImageData> GetAllProfileImages() const;

    // Country Info Data
    UFUNCTION(BlueprintCallable, Category = "Country")
    FCountryInfoTable GetCountryInfo(ECountryType CountryType, bool& bOutSuccess) const;

    // 모든 등록된 국가 정보를 배열로 반환 (라우터 패널 등 순회용)
    UFUNCTION(BlueprintCallable, Category = "Country")
    TArray<FCountryInfoTable> GetAllCountryInfos() const;

    // Mission Data (DT_Mission — 미션 체인, RowName = MissionID)
    UFUNCTION(BlueprintCallable, Category = "Mission")
    FMissionTable GetMissionData(FName MissionID, bool& bOutSuccess) const;

    // 미션판 미션 조회 (DT_Goal). 없으면 false
    bool GetGoalData(const FName& RowName, FGoalTable& OutData) const;
    const TMap<FName, FGoalTable>& GetAllGoalData() const { return GoalDataMap; }
    const TArray<FName>& GetGoalRowOrder() const { return GoalRowOrder; }

    // 패널 최초 진입 코치마크 스텝 (DT_PanelIntro, StepIndex 오름차순). 미정의 패널이면 nullptr
    const TArray<FPanelIntroTable>* GetPanelIntroSteps(const FName& PanelKey) const { return PanelIntroMap.Find(PanelKey); }

    // (Country, Industry) 조합으로 수요 정의 조회
    UFUNCTION(BlueprintCallable, Category = "Country|Demand")
    FCountryDemandTable GetCountryDemand(ECountryType CountryType, ECompanyType Industry, bool& bOutSuccess) const;

    // 모든 등록된 (Country, Industry) 수요 정의 반환 (CountryMarketManager 초기화용)
    UFUNCTION(BlueprintCallable, Category = "Country|Demand")
    TArray<FCountryDemandTable> GetAllCountryDemands() const;

    // 판매 모달용 (Country, Industry) 한 줄 라벨 조회 (예: "거대 소비처", "프리미엄 본거지")
    UFUNCTION(BlueprintCallable, Category = "Country|MarketRole")
    FText GetMarketRole(ECountryType CountryType, ECompanyType Industry, bool& bOutSuccess) const;

    // Company Info Data — 산업 메타 (DisplayName, Icon, AccentColor)
    UFUNCTION(BlueprintCallable, Category = "Company")
    FCompanyInfoTable GetCompanyInfo(ECompanyType CompanyType, bool& bOutSuccess) const;

    // Product Recipe Data (제조업 레시피)
    UFUNCTION(BlueprintCallable, Category = "Recipe")
    FProductRecipeTable GetProductRecipe(ECompanyType CompanyType, int32 ProjectIndex, bool& bOutSuccess) const;

    // Game Project Data (legacy: Game 산업 전용)
    UFUNCTION(BlueprintCallable, Category = "Project")
    FProjectData GetGameProjectData(int32 ProjectIndex, bool& bOutSuccess) const;

    // 스테이지 번호로 프로젝트 데이터 가져오기 (내부적으로 ProjectIndex 계산)
    UFUNCTION(BlueprintCallable, Category = "Project")
    FProjectData GetGameProjectDataByStage(int32 StageNumber, bool& bOutSuccess) const;

    // 산업 + 프로젝트 인덱스로 프로젝트 데이터 조회 (6개 산업 전부 지원)
    // TradeOrder/WorldMap 등 산업 구분이 필요한 호출부에서 사용.
    UFUNCTION(BlueprintCallable, Category = "Project")
    FProjectData GetProjectData(ECompanyType CompanyType, int32 ProjectIndex, bool& bOutSuccess) const;

    // 현재 진입한 빌딩 컨텍스트(CGGameInstance::GetCurrentBuildingCompanyType)로 CompanyType을 자동 resolve
    // 해 프로젝트 데이터를 조회. OfficeMap 내 UI 위젯에서 사용. 현재 CompanyType이 None이면 실패.
    UFUNCTION(BlueprintCallable, Category = "Project")
    FProjectData ResolveProjectData(int32 ProjectIndex, bool& bOutSuccess) const;

    // 회사 타입별 프로젝트 목록 가져오기
    UFUNCTION(BlueprintCallable, Category = "Project")
    TArray<FProjectData> GetProjectsByCompanyType(ECompanyType CompanyType) const;

    // ===== GDS Core Loop 데이터층 (착수 발견형) =====

    // 해당 산업에서 UnlockTier <= Tier 인 장르 목록 (착수 화면 발견형 노출용).
    UFUNCTION(BlueprintCallable, Category = "Project")
    TArray<FName> GetUnlockedGenres(ECompanyType Industry, int32 Tier) const;

    // (산업, 장르) 장르 정의(복잡도/해금 티어) 조회. 없으면 false.
    UFUNCTION(BlueprintCallable, Category = "Project")
    bool GetGenreInfo(ECompanyType Industry, FName Genre, FProjectGenreRow& OutRow) const;

    // 산업별 수익 곡선/리뷰 프로파일. 없으면 bOutSuccess=false + 기본값.
    UFUNCTION(BlueprintCallable, Category = "Project")
    FIndustryProfileRow GetIndustryProfile(ECompanyType Industry, bool& bOutSuccess) const;

    // (산업, 장르, 소재) 일치하는 기존 프로젝트 행들의 ProjectName 중 하나. 없으면 빈 FText.
    UFUNCTION(BlueprintCallable, Category = "Project")
    FText PickProjectName(ECompanyType Industry, FName Genre, FName Material) const;

    // (산업, 장르, 소재) 일치 프로젝트 행 전체 반환 — 피치의 요구점수(RequiredScore_Step) 소스. 첫 매칭, 없으면 bOutSuccess=false.
    FProjectData GetProjectByGenreMaterial(ECompanyType Industry, FName Genre, FName Material, bool& bOutSuccess) const;

    // 해당 산업의 모든 장르 정의(잠금 포함) — 착수 1단계 그리드(잠긴 장르도 흐리게 노출)용. UnlockTier↑·Complexity↑ 정렬.
    UFUNCTION(BlueprintCallable, Category = "Project")
    TArray<FProjectGenreRow> GetGenresForIndustry(ECompanyType Industry) const;

    // 해당 산업의 소재 목록(중복 제거) — 트렌드 로테이션 풀. 프로젝트 행에서 파생(소재 전용 테이블 없음).
    UFUNCTION(BlueprintCallable, Category = "Project")
    TArray<FName> GetMaterialsForIndustry(ECompanyType Industry) const;

    // 산업별 티어 랭크 칭호 (포트폴리오/도감 헤더). 매핑 없으면 빈 FText (데이터주도 — 코드 폴백 금지).
    UFUNCTION(BlueprintCallable, Category = "Project")
    FText GetTierRankTitle(ECompanyType Industry, int32 Tier) const;

    // 산업별 비평가 4명 표시명 (리뷰 /40). 매핑 없으면 빈 배열 (DT 단일 진실 — 코드 폴백 금지).
    UFUNCTION(BlueprintCallable, Category = "Project")
    TArray<FText> GetCriticNames(ECompanyType Industry) const;

    // 비평가별 관심 직능 (CriticNFocus 파싱). [i]=비평가 i — 빈 배열=전체 평가. 산업 매핑 없으면 통째 빈 결과.
    TArray<TArray<EProductionDiscipline>> GetCriticFocus(ECompanyType Industry) const;

    // 통합 리뷰 코멘트 — Slot/Band/Context 필터 → 산업 1행 보장 + 셔플 → 토큰 치환.
    // 풀이 비면 빈 결과 + Warning (코드 폴백 금지 — UI는 빈 카드 Collapsed).
    void GetReviewComments(FName CommentSlot, ECompanyType Industry, FName Band,
        const TSet<FName>& ActiveContexts, int32 Count, const FReviewTokenValues& Tokens,
        TArray<FText>& OutNicks, TArray<FText>& OutComments) const;

    // 산업에 맞는 부스트 도박 시나리오 1개 랜덤 반환. 매칭 행 없으면 false (트리거 스킵).
    UFUNCTION(BlueprintCallable, Category = "Project")
    bool GetRandomBoostGamble(ECompanyType Industry, FBoostGambleRow& OutRow) const;

    // 무역 주문서 밸런스 (단일 row "Default" 캐시본)
    UFUNCTION(BlueprintPure, Category = "Balance")
    const FTradeOrderBalanceData& GetTradeOrderBalance() const { return TradeOrderBalanceCache; }

    // Market Balance — 시장 성장 곡선 (단일 row 캐시)
    const FMarketBalanceData& GetMarketBalance() const { return MarketBalanceCache; }

    // Department Display Name (산업별 부서 표시명)
    // 매핑 없으면 게임 기본값으로 폴백 (DepartmentToString)
    UFUNCTION(BlueprintPure, Category = "Department")
    FText GetDepartmentDisplayName(ECompanyType CompanyType, EEmployeeDepartment Department) const;

    // Quality Grade Label (산업별 품질 등급 한국어 라벨)
    // 예: (Game, S) → "명작", (Electronics, S) → "프리미엄"
    // 매핑 없으면 빈 FText 반환 (DT 단일 진실 — 호출부에서 IsEmpty() 체크 후 알파벳 폴백 가능)
    UFUNCTION(BlueprintPure, Category = "QualityGrade")
    FText GetQualityGradeLabel(ECompanyType CompanyType, EQualityGrade Grade) const;

    // Employee Potential / Additional Option 한국어 라벨
    UFUNCTION(BlueprintPure, Category = "Employee")
    FText GetPotentialOptionDisplayName(EPotentialOptionType OptionType) const;

    UFUNCTION(BlueprintPure, Category = "Employee")
    FText GetAdditionalOptionDisplayName(EAdditionalOptionType OptionType) const;

    // Trade Order Tier Label / Color (긴급/VIP/일반)
    UFUNCTION(BlueprintPure, Category = "TradeOrder")
    bool GetTradeOrderTierDisplay(ETradeOrderTier Tier, FText& OutLabel, FLinearColor& OutColor) const;

    // Project Mode Prefix / Suffix (수주/자체개발)
    UFUNCTION(BlueprintPure, Category = "Project")
    bool GetProjectModeDisplay(EProjectMode Mode, FString& OutPrefix, FString& OutSuffix) const;

    // Step Display Name (산업 × variant × step → 표시명)
    // 1) {CompanyType}_{Variant} 우선 lookup
    // 2) {CompanyType} 단독 lookup (variant 폴백)
    // 3) 매핑 없으면 빈 FText 반환 (DT 단일 진실)
    UFUNCTION(BlueprintCallable, Category = "StepDisplay")
    FText GetStepDisplayName(ECompanyType CompanyType, FName VariantKey, int32 StepNumber) const;

    /** 산업별 직능 표시명. SlotIndex = EProductionDiscipline(0~5). 매핑 없으면 빈 FText + Warning. */
    FText GetDisciplineDisplayName(ECompanyType CompanyType, int32 SlotIndex) const;

    // UI VFX 텍스처 레지스트리 (DT_UIVFXTexture) — 키로 연출 텍스처/크기/틴트 조회
    UFUNCTION(BlueprintCallable, Category = "UIVFX")
    bool GetUIVFXTexRow(FName Key, FUIVFXTexRow& OutRow) const;

    // 출시 평점 판정 밴드 표시(라벨/색). Key = Low/Mid/High. 없으면 false + Warning (UI 는 기본 흰색/빈 라벨)
    bool GetLaunchLootBand(FName Key, struct FLaunchLootBandRow& OutRow) const;

    // 빈 Image 위젯에 DT 행(텍스처+ImageSize+틴트)을 브러시로 주입. 행 없거나 로드 실패 시 false (위젯은 빈 채 유지)
    UFUNCTION(BlueprintCallable, Category = "UIVFX")
    bool ApplyUIVFXTexture(FName Key, class UImage* Image) const;

    // 상점 상품 행 조회 (DT_ShopItem)
    UFUNCTION(BlueprintCallable, Category = "Shop")
    bool GetShopItemRow(FName RowName, FShopItemTable& OutRow) const;

    // EItemType → 아이콘 (DT_ShopItem 역참조). 못 찾으면 nullptr + bOutFound=false.
    UFUNCTION(BlueprintCallable, Category = "Shop")
    UTexture2D* GetItemIcon(EItemType ItemType, bool& bOutFound) const;

    // EItemType → 상점 행(이름/아이콘/단가 등, DT_ShopItem 역참조). 첫 매치 반환. 없으면 false.
    UFUNCTION(BlueprintCallable, Category = "Shop")
    bool GetShopItemByItemType(EItemType ItemType, FShopItemTable& OutRow) const;

    // 탭별 상품 RowName 목록 (SortOrder 오름차순)
    UFUNCTION(BlueprintCallable, Category = "Shop")
    TArray<FName> GetShopItemRowsForTab(EShopTab Tab) const;

    // TableKey(Low/Mid/High)의 드랍 후보 목록. 없으면 nullptr
    const TArray<FLaunchLootTable>* GetLaunchLootEntries(FName TableKey) const;

    // Building Enhancement Definition — 회사 타입별 강화 슬롯 메타데이터 (아이콘, 이름, 설명, 카테고리, 정렬 순서 등)
    UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
    bool GetEnhancementDefinition(EBuildingEnhancementType Type, FBuildingEnhancementDefinition& OutDef) const;

    // 회사 타입에 노출되어야 하는 강화 슬롯 목록 (공통 + 해당 산업 전용)
    // 정렬: FBuildingEnhancementDefinition.SortOrder 오름차순 (DT 단일 진실 원천)
    UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
    TArray<FBuildingEnhancementDefinition> GetEnhancementsForCompanyType(ECompanyType CompanyType) const;

    // World Factory Upgrade Definition — 세계지도 국가 공장 강화 슬롯 메타데이터/밸런스
    UFUNCTION(BlueprintCallable, Category = "World Factory Upgrade")
    bool GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType Type, FWorldFactoryUpgradeDefinition& OutDef) const;

    // 정렬된 전체 강화 슬롯 목록 (SortOrder 오름차순)
    UFUNCTION(BlueprintCallable, Category = "World Factory Upgrade")
    TArray<FWorldFactoryUpgradeDefinition> GetAllWorldFactoryUpgradeDefinitions() const;

    // Mine Upgrade Definition — 세계지도 채광 강화 슬롯 메타데이터/밸런스
    UFUNCTION(BlueprintCallable, Category = "Mine Upgrade")
    bool GetMineUpgradeDefinition(EMineUpgradeType Type, FMineUpgradeDefinition& OutDef) const;

    // 정렬된 전체 채광 강화 슬롯 목록 (SortOrder 오름차순)
    UFUNCTION(BlueprintCallable, Category = "Mine Upgrade")
    TArray<FMineUpgradeDefinition> GetAllMineUpgradeDefinitions() const;

    // Factory Upgrade Definition — BrickFactory 강화 슬롯 메타데이터/밸런스
    UFUNCTION(BlueprintCallable, Category = "Factory Upgrade")
    bool GetFactoryUpgradeDefinition(EFactoryUpgradeType Type, FFactoryUpgradeDefinition& OutDef) const;

    // 정렬된 전체 Factory 강화 슬롯 목록 (SortOrder 오름차순)
    UFUNCTION(BlueprintCallable, Category = "Factory Upgrade")
    TArray<FFactoryUpgradeDefinition> GetAllFactoryUpgradeDefinitions() const;

    // ========== Building Trait (BUILDING_TRAIT_SYSTEM v1.1) ==========

    // 특성 단건 조회 (TraitID = FName)
    UFUNCTION(BlueprintCallable, Category = "Building Trait")
    bool GetBuildingTraitData(FName TraitID, FBuildingTraitTableRow& OutRow) const;

    // 전체 특성 목록 (도감/가챠 풀 구성용)
    UFUNCTION(BlueprintCallable, Category = "Building Trait")
    TArray<FBuildingTraitTableRow> GetAllBuildingTraits() const;

    // 등급별 풀 (가챠 결과 등급별 랜덤 픽 시 사용)
    UFUNCTION(BlueprintCallable, Category = "Building Trait")
    TArray<FBuildingTraitTableRow> GetBuildingTraitsByRarity(ELootBoxRarity Rarity) const;

    // 분야별 풀 (도감 카테고리 탭, 합성 결과 풀 등)
    UFUNCTION(BlueprintCallable, Category = "Building Trait")
    TArray<FBuildingTraitTableRow> GetBuildingTraitsByCategory(EBuildingTraitCategory Category) const;

    // 세트 보너스 조회 (분야 + 요구 개수 2/3)
    UFUNCTION(BlueprintCallable, Category = "Building Trait")
    bool GetBuildingTraitSetBonus(EBuildingTraitCategory Category, int32 RequiredCount, FBuildingTraitSetBonus& OutRow) const;

    // 분야별 전체 세트 보너스 (보통 2개 — 2세트/3세트)
    UFUNCTION(BlueprintCallable, Category = "Building Trait")
    TArray<FBuildingTraitSetBonus> GetBuildingTraitSetBonusesByCategory(EBuildingTraitCategory Category) const;

    // ========== Event Choice Icon ==========

    // 이벤트 선택지 행동 아키타입 → 아이콘 텍스처 (없거나 로드 실패 시 nullptr)
    UFUNCTION(BlueprintCallable, Category = "Event Icon")
    UTexture2D* GetEventChoiceIcon(EChoiceArchetype Archetype) const;

    // ========== City Plot (도시 블록 부지) ==========

    // PlotId(=RowName)로 부지 데이터 조회. 없으면 bOk=false + 기본값 반환.
    UFUNCTION(BlueprintCallable, Category = "City Plot")
    FCityPlotData GetCityPlotData(FName PlotId, bool& bOk) const;

    // 등록된 모든 부지 PlotId 목록 (레벨 배치/시작 부지 초기화 순회용)
    UFUNCTION(BlueprintCallable, Category = "City Plot")
    void GetAllCityPlotRows(TArray<FName>& OutIds) const;

    // ========== City Dressing (도시 거리·빈 부지 소품) ==========

    // PresetId → SlotId 순으로 정렬된 전체 패치 슬롯. 누락 행/메시는 호출자가 fail-closed로 건너뛴다.
    UFUNCTION(BlueprintCallable, Category = "City Dressing")
    void GetAllCityDressingRows(TArray<FCityDressingData>& OutRows) const;

    // ========== City Company (도시 인수 — 스카이라인 가상회사) ==========

    // BuildingKey(BP_MB### 파싱값)로 회사 데이터 조회. 없으면 bOk=false + 기본값.
    UFUNCTION(BlueprintCallable, Category = "City Company")
    bool GetCityCompanyData(int32 BuildingKey, FCityCompanyData& OutData) const;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend struct FGoalTableRowOrderTestAccessor;
#endif

    // UI Data
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    UDataTable* WidgetDataTable = nullptr;

    UPROPERTY()
    UDataTable* TestProductionOrdersTable = nullptr;

    UPROPERTY()
    UDataTable* TestResourceScenariosTable = nullptr;

    UPROPERTY()
    UDataTable* DevProgressPresetTable = nullptr;

    UPROPERTY()
    UDataTable* DevPresetBuildingTable = nullptr;

    UPROPERTY()
    TMap<EWidgetType, TSubclassOf<UUserWidget>> WidgetTable;

    void InitializeWidgetTable();

    // BuildData EntityCard에서 사용하는 빌딩의 건설비용정보
    UPROPERTY(EditDefaultsOnly, Category = "BuildData")
    UDataTable* BuildableDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FBuildableCardTable> BuildTable;

    void InitializeBuildableTable();

    // EmployeeCard에서 사용하는 정보
    UPROPERTY(EditDefaultsOnly, Category = "EmployeeData")
    UDataTable* EmployeeCardDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FEmployeeCardTable> EmployeeCardTable;

    void InitializeEmployeeCardTable();

    // Interactable Data Tables
    UPROPERTY(EditAnywhere, Category = "Data Tables")
    UDataTable* InteractableDataTable; // FInteractableInfo를 사용하는 메인 테이블
    
    // 건물 자체의 정보 ex) Mesh등
    UPROPERTY(EditAnywhere, Category = "Data Tables")
    UDataTable* BuildingDataTable; // FBuildingData를 사용하는 테이블

    UPROPERTY()
    TMap<FName, FInteractableInfo> InteractableTable;
    
    UPROPERTY()
    TMap<FName, FBuildingData> BuildingTable;
    
    void InitializeInteractableTable();
    void InitializeBuildingTable();

    // Employee Name Data Tables
    UPROPERTY(EditAnywhere, Category = "Employee Names")
    UDataTable* LastNameDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Employee Names")
    UDataTable* FirstNameDataTable = nullptr;

    // Character Appearance Data Tables
    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* HairPartDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* RankClothingDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* HairPartGirlDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* RankClothingGirlDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* EyeColorDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* SkinColorDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* HairColorDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Character Appearance")
    UDataTable* CharacterBaseMeshDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FHairPartTable> HairPartTable;

    UPROPERTY()
    TMap<FName, FRankClothingTable> RankClothingTable;

    UPROPERTY()
    TMap<FName, FHairPartTable> HairPartGirlTable;

    UPROPERTY()
    TMap<FName, FRankClothingTable> RankClothingGirlTable;

    // Worker Cosmetic (AStickOfficeworker 외형). 로드는 생성자 ConstructorHelpers 단일 경로 — 에디터 할당 금지(프로젝트 DT 규칙).
    UPROPERTY()
    UDataTable* WorkerCosmeticDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FWorkerCosmeticTable> WorkerCosmeticTableMap;

    UPROPERTY()
    TMap<FName, FEyeColorTable> EyeColorTable;

    UPROPERTY()
    TMap<FName, FSkinColorTable> SkinColorTable;

    UPROPERTY()
    TMap<FName, FHairColorTable> HairColorTable;

    UPROPERTY()
    TMap<EEmployeeGender, FCharacterBaseMeshTable> CharacterBaseMeshTable;

    // LootBox Data Table
    UPROPERTY(EditAnywhere, Category = "LootBox")
    UDataTable* LootBoxDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FLootBoxTable> LootBoxTable;

    // Resource Data Table
    UPROPERTY(EditAnywhere, Category = "Resource")
    UDataTable* ResourceDataTable = nullptr;

    UPROPERTY()
    TMap<EResourceType, FResourceInfo> ResourceTable;

    // BuildingSkin Data Table
    UPROPERTY(EditAnywhere, Category = "BuildingSkin")
    UDataTable* BuildingSkinDataTable = nullptr;

    UPROPERTY()
    TMap<int32, FBuildingSkinData> BuildingSkinTable;

    // BuildingLight Data Table
    UPROPERTY(EditAnywhere, Category = "BuildingLight")
    UDataTable* BuildingLightDataTable = nullptr;

    UPROPERTY()
    TMap<int32, FBuildingLightData> BuildingLightTable;

    // Decoration Data Tables
    UPROPERTY(EditAnywhere, Category = "Decoration")
    UDataTable* DecorationCardDataTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Decoration")
    UDataTable* DecorationDataDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FDecorationCardTable> DecorationCardTable;

    UPROPERTY()
    TMap<FName, FDecorationData> DecorationDataTable;

    // Workstation Data Tables
    UPROPERTY(EditAnywhere, Category = "Workstation")
    UDataTable* WorkstationCardDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FWorkstationCardTable> WorkstationCardTable;

    // Workstation Item Skin Data Table
    UPROPERTY(EditAnywhere, Category = "Workstation")
    UDataTable* WorkstationItemSkinDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FWorkstationItemSkinData> WorkstationItemSkinTable;

    UPROPERTY()
    UDataTable* WorkstationSetupLevelDataTable = nullptr;

    UPROPERTY()
    TMap<EComputerSetupLevel, FComputerSetupLevelData> WorkstationSetupLevelTable;

    // UI Icon Data Table
    UPROPERTY(EditAnywhere, Category = "UI Icon")
    UDataTable* UIIconDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FUIIconData> UIIconTable;

    // Money VFX Data Table
    UPROPERTY(EditAnywhere, Category = "VFX")
    UDataTable* MoneyVFXDataTable = nullptr;

    UPROPERTY()
    TArray<FMoneyVFXTable> MoneyVFXTable;

    // Game Project Data Tables (산업별 6종 분리)
    // NOTE: 과거엔 Game 산업만 로드했으나, 모든 산업을 로드해야 TradeOrder/WorldMap 에서
    //       (CompanyType, ProjectIndex) 조합으로 정확한 프로젝트 이름 조회 가능.
    UPROPERTY(EditAnywhere, Category = "Project")
    UDataTable* GameProjectDataTable = nullptr;                 // Game 산업 (legacy 유지)

    UPROPERTY()
    UDataTable* ProjectDataTable_Electronics = nullptr;
    UPROPERTY()
    UDataTable* ProjectDataTable_Automobile = nullptr;
    UPROPERTY()
    UDataTable* ProjectDataTable_Semiconductor = nullptr;
    UPROPERTY()
    UDataTable* ProjectDataTable_Finance = nullptr;
    UPROPERTY()
    UDataTable* ProjectDataTable_IT = nullptr;

    // Legacy: Game 산업만 ProjectIndex 키로 캐싱 (Office 기존 호출부 호환)
    UPROPERTY()
    TMap<int32, FProjectData> GameProjectTable;

    // Unified: (CompanyType, ProjectIndex) → FProjectData, 6개 산업 모두 포함
    // Key = FIntPoint(static_cast<int32>(CompanyType), ProjectIndex)
    UPROPERTY()
    TMap<FIntPoint, FProjectData> AllProjectTable;

    // ===== GDS Core Loop 데이터층 (착수 발견형) =====

    // Project Genre (산업별 장르 정의: 복잡도/해금 티어)
    UPROPERTY()
    UDataTable* ProjectGenreDataTable = nullptr;

    UPROPERTY()
    TArray<FProjectGenreRow> ProjectGenreRows;

    void InitializeProjectGenreTable();

    // Tier Rank Title (산업별 티어 랭크 칭호)
    UPROPERTY()
    UDataTable* TierRankTitleDataTable = nullptr;

    UPROPERTY()
    TArray<FTierRankTitleRow> TierRankTitleRows;

    void InitializeTierRankTitleTable();

    // Industry Profile (산업별 수익 곡선/리뷰 계수). Key = ECompanyType.
    UPROPERTY()
    UDataTable* IndustryProfileDataTable = nullptr;

    UPROPERTY()
    TMap<ECompanyType, FIndustryProfileRow> IndustryProfileMap;

    void InitializeIndustryProfileTable();

    // Critic Display (산업별 비평가 4명 표시명 — 리뷰 /40)
    UPROPERTY()
    UDataTable* CriticDisplayDataTable = nullptr;

    UPROPERTY()
    TMap<ECompanyType, FCriticDisplayRow> CriticDisplayMap;

    void InitializeCriticDisplayTable();

    // Review Comment (통합 리뷰/실패 코멘트 풀 — specs/2026-08-02)
    UPROPERTY()
    UDataTable* ReviewCommentDataTable = nullptr;

    UPROPERTY()
    TArray<FReviewCommentRow> ReviewCommentRows;

    void InitializeReviewCommentTable();

    static FString ApplyReviewTokens(const FString& In, const FReviewTokenValues& Tokens);

    // Boost Gamble (부스트 도박 시나리오 풀 — 산업 인격)
    UPROPERTY()
    UDataTable* BoostGambleDataTable = nullptr;

    UPROPERTY()
    TArray<FBoostGambleRow> BoostGambleRows;

    void InitializeBoostGambleTable();

    // Trade Order 밸런스 — 단일 row 직접 캐시
    UPROPERTY()
    UDataTable* TradeOrderBalanceDataTable = nullptr;

    UPROPERTY()
    FTradeOrderBalanceData TradeOrderBalanceCache;

    // Market Balance — 단일 row 직접 캐시
    UPROPERTY()
    UDataTable* MarketBalanceDataTable = nullptr;

    UPROPERTY()
    FMarketBalanceData MarketBalanceCache;

    // Tier Unlock Data Table
    UPROPERTY(EditAnywhere, Category = "Tier Unlock")
    UDataTable* TierUnlockDataTable = nullptr;

    UPROPERTY()
    TMap<int32, FTierUnlockData> TierUnlockTable;

    // Country Info Data Table
    UPROPERTY(EditAnywhere, Category = "Country")
    UDataTable* CountryInfoDataTable = nullptr;

    UPROPERTY()
    TMap<ECountryType, FCountryInfoTable> CountryInfoTable;

    // Mission Data Table (미션 체인 정의)
    UPROPERTY()
    UDataTable* MissionDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FMissionTable> MissionTable;

    UPROPERTY()
    TObjectPtr<UDataTable> GoalDataTable = nullptr;
    TMap<FName, FGoalTable> GoalDataMap;
    TArray<FName> GoalRowOrder;
    void InitializeGoalTable();

    UPROPERTY()
    TObjectPtr<UDataTable> PanelIntroDataTable = nullptr;
    // PanelKey → 스텝 배열(StepIndex 오름차순). 행 하나가 스텝 하나라 여기서 묶어 정렬해 둔다
    TMap<FName, TArray<FPanelIntroTable>> PanelIntroMap;
    void InitializePanelIntroTable();

    // Country Demand Data Table (Row Name = "{Country}_{Industry}")
    UPROPERTY()
    UDataTable* CountryDemandDataTable = nullptr;

    // Cached: FIntPoint(Country int, Industry int) → Demand row
    UPROPERTY()
    TMap<FIntPoint, FCountryDemandTable> CountryDemandTable;

    // Country MarketRole Data Table (Row Name = "{Country}_{Industry}") — 판매 모달 라벨 전용
    UPROPERTY()
    UDataTable* CountryMarketRoleDataTable = nullptr;

    UPROPERTY()
    TMap<FIntPoint, FCountryMarketRoleTable> CountryMarketRoleTable;

    // Company Info Data Table (산업 메타: 아이콘, 표시명, 강조색)
    UPROPERTY()
    UDataTable* CompanyInfoDataTable = nullptr;

    UPROPERTY()
    TMap<ECompanyType, FCompanyInfoTable> CompanyInfoTable;

    void InitializeCompanyInfoTable();

    // Department Display Name Data Table (산업 × 부서 → 표시명)
    UPROPERTY(EditAnywhere, Category = "Department")
    UDataTable* DepartmentDisplayDataTable = nullptr;

    // 캐시: Key = FIntPoint(CompanyType, Department)
    UPROPERTY()
    TMap<FIntPoint, FText> DepartmentDisplayMap;

    // Quality Grade Display Data Table (산업 × 품질등급 → 한국어 라벨)
    UPROPERTY(EditAnywhere, Category = "QualityGrade")
    UDataTable* QualityGradeDisplayDataTable = nullptr;

    // 캐시: Key = FIntPoint(CompanyType, Grade)
    UPROPERTY()
    TMap<FIntPoint, FText> QualityGradeDisplayMap;

    // Employee Potential Option Display Data Table
    UPROPERTY(EditAnywhere, Category = "Employee")
    UDataTable* PotentialOptionDisplayDataTable = nullptr;

    UPROPERTY()
    TMap<EPotentialOptionType, FText> PotentialOptionDisplayMap;

    // Employee Additional Option Display Data Table
    UPROPERTY(EditAnywhere, Category = "Employee")
    UDataTable* AdditionalOptionDisplayDataTable = nullptr;

    UPROPERTY()
    TMap<EAdditionalOptionType, FText> AdditionalOptionDisplayMap;

    // Trade Order Tier Display Data Table
    UPROPERTY(EditAnywhere, Category = "TradeOrder")
    UDataTable* TradeOrderTierDisplayDataTable = nullptr;

    UPROPERTY()
    TMap<ETradeOrderTier, FTradeOrderTierDisplayRow> TradeOrderTierDisplayMap;

    // Project Mode Display Data Table
    UPROPERTY(EditAnywhere, Category = "Project")
    UDataTable* ProjectModeDisplayDataTable = nullptr;

    UPROPERTY()
    TMap<EProjectMode, FProjectModeDisplayRow> ProjectModeDisplayMap;

    // Step Display Name Data Table (산업 × variant × step → 표시명)
    UPROPERTY(EditAnywhere, Category = "StepDisplay")
    UDataTable* StepDisplayNameDataTable = nullptr;

    // 캐시: Key = RowName ("Game", "Finance_예적금" 등 — 변형 행은 {산업}_{VariantKey})
    UPROPERTY()
    TMap<FName, FStepDisplayNameRow> StepDisplayNameMap;

    // Discipline Display Data Table (산업 → 6직능 슬롯 표시명)
    UPROPERTY()
    UDataTable* DisciplineDisplayDataTable = nullptr;

    // 캐시: Key = ECompanyType (Game/Finance/IT 3종 — 제조는 의도적 미시드, miss = 빈 FText)
    UPROPERTY()
    TMap<ECompanyType, FDisciplineDisplayRow> DisciplineDisplayMap;

    // UI VFX 텍스처 레지스트리 DataTable (DT 단일 진실: 연출 텍스처/크기/틴트)
    UPROPERTY()
    UDataTable* UIVFXTexDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FUIVFXTexRow> UIVFXTexMap;

    // 출시 평점 판정 밴드 표시 DataTable (DT 단일 진실: 라벨/색)
    UPROPERTY()
    UDataTable* LaunchLootBandDataTable = nullptr;

    TMap<FName, FLaunchLootBandRow> LaunchLootBandMap;

    // 상점 상품 DataTable (DT 단일 진실: 탭/품목/가격/한도)
    UPROPERTY()
    UDataTable* ShopItemDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FShopItemTable> ShopItemMap;

    void InitializeShopItemTable();

    // 출시 전리품 드랍 DataTable (TableKey별 엔트리 캐시)
    UPROPERTY()
    UDataTable* LaunchLootDataTable = nullptr;

    // UPROPERTY 불가 — UHT 는 중첩 컨테이너(TMap 값이 TArray)를 지원하지 않는다. FLaunchLootTable 은 UObject 참조가 없어 GC 문제도 없음
    TMap<FName, TArray<FLaunchLootTable>> LaunchLootMap;

    void InitializeLaunchLootTable();

    // Building Enhancement Definition DataTable (DT 단일 진실: 슬롯 메타데이터)
    UPROPERTY()
    UDataTable* BuildingEnhancementDefinitionDataTable = nullptr;

    UPROPERTY()
    TMap<EBuildingEnhancementType, FBuildingEnhancementDefinition> EnhancementDefinitionTable;

    void InitializeBuildingEnhancementDefinitionTable();

    // World Factory Upgrade Definition DataTable (DT 단일 진실: 세계지도 공장 강화 슬롯)
    UPROPERTY()
    UDataTable* WorldFactoryUpgradeDefinitionDataTable = nullptr;

    UPROPERTY()
    TMap<EWorldFactoryUpgradeType, FWorldFactoryUpgradeDefinition> WorldFactoryUpgradeDefinitionTable;

    void InitializeWorldFactoryUpgradeDefinitionTable();

    // Mine Upgrade Definition DataTable (DT 단일 진실: 세계지도 채광 강화 슬롯)
    UPROPERTY()
    UDataTable* MineUpgradeDefinitionDataTable = nullptr;

    UPROPERTY()
    TMap<EMineUpgradeType, FMineUpgradeDefinition> MineUpgradeDefinitionTable;

    void InitializeMineUpgradeDefinitionTable();

    // Factory Upgrade Definition DataTable (DT 단일 진실: BrickFactory 강화 슬롯)
    UPROPERTY()
    UDataTable* FactoryUpgradeDefinitionDataTable = nullptr;

    UPROPERTY()
    TMap<EFactoryUpgradeType, FFactoryUpgradeDefinition> FactoryUpgradeDefinitionTable;

    void InitializeFactoryUpgradeDefinitionTable();

    // Building Trait DataTables
    UPROPERTY()
    UDataTable* BuildingTraitDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FBuildingTraitTableRow> BuildingTraitTable;

    UPROPERTY()
    UDataTable* BuildingTraitSetBonusDataTable = nullptr;

    // 세트 보너스는 (Category, RequiredCount) 키 — FIntPoint 로 캐시
    UPROPERTY()
    TMap<FIntPoint, FBuildingTraitSetBonus> BuildingTraitSetBonusTableMap;

    void InitializeBuildingTraitTable();
    void InitializeBuildingTraitSetBonusTable();

    // Office Starter Preset DataTables
    UPROPERTY()
    UDataTable* OfficeStarterPresetDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FOfficeStarterPresetRow> OfficeStarterPresetTable;

    void InitializeOfficeStarterPresetTable();

    // Event Choice Icon DataTable (행동 아키타입 → 아이콘)
    UPROPERTY()
    UDataTable* EventChoiceIconDataTable = nullptr;

    UPROPERTY()
    TMap<EChoiceArchetype, TSoftObjectPtr<UTexture2D>> EventChoiceIconMap;

    void InitializeEventChoiceIconTable();

    // City Plot DataTable (도시 블록 부지: 가격/수용량/식별)
    UPROPERTY()
    UDataTable* CityPlotDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FCityPlotData> CityPlotMap;

    void InitializeCityPlotTable();

    // City Dressing DataTable (빈 부지 패치 슬롯). 직렬화된 Mesh 소프트 참조는 쿠커가 따라간다.
    UPROPERTY()
    UDataTable* CityDressingDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FCityDressingData> CityDressingMap;

    void InitializeCityDressingTable();

    // City Company DataTable (도시 인수 — 스카이라인 가상회사). Key = BuildingKey(int32).
    UPROPERTY()
    UDataTable* CityCompanyDataTable = nullptr;

    UPROPERTY()
    TMap<int32, FCityCompanyData> CityCompanyMap;

    void InitializeCityCompanyTable();

    UPROPERTY()
    UDataTable* IndustryMoodPaletteDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FIndustryMoodPaletteRow> IndustryMoodPaletteTable;

    void InitializeIndustryMoodPaletteTable();

    // VFX Data Table
    UPROPERTY(EditAnywhere, Category = "VFX")
    UDataTable* VFXDataTable = nullptr;

    UPROPERTY()
    TMap<EVFXType, FVFXTableRow> VFXTable;

    // HQ Level Data Table
    UPROPERTY()
    UDataTable* HQLevelDataTable = nullptr;

    UPROPERTY()
    TMap<int32, FHQLevelData> HQLevelTable;

    // Menu Unlock Data Table
    UPROPERTY()
    UDataTable* MenuUnlockDataTable = nullptr;

    UPROPERTY()
    TMap<FName, FMenuUnlockData> MenuUnlockTable;

    // Profile Image Data Table
    UPROPERTY()
    UDataTable* ProfileImageDataTable = nullptr;

    UPROPERTY()
    TMap<int32, FProfileImageData> ProfileImageTable;

    // Product Recipe Data Tables (제조업 타입별 분리)
    UPROPERTY()
    UDataTable* RecipeDataTable_Electronics = nullptr;

    UPROPERTY()
    UDataTable* RecipeDataTable_Automobile = nullptr;

    UPROPERTY()
    UDataTable* RecipeDataTable_Semiconductor = nullptr;

    // 캐시: Key = ProjectIndex (1~100)
    UPROPERTY()
    TMap<int32, FProductRecipeTable> RecipeTable_Electronics;

    UPROPERTY()
    TMap<int32, FProductRecipeTable> RecipeTable_Automobile;

    UPROPERTY()
    TMap<int32, FProductRecipeTable> RecipeTable_Semiconductor;

    void InitializeRecipeTable_Electronics();
    void InitializeRecipeTable_Automobile();
    void InitializeRecipeTable_Semiconductor();

    // 초기화 함수들
    void InitializeHairPartTable();
    void InitializeRankClothingTable();
    void InitializeHairPartGirlTable();
    void InitializeRankClothingGirlTable();
    void InitializeWorkerCosmeticTable();
    void InitializeEyeColorTable();
    void InitializeSkinColorTable();
    void InitializeHairColorTable();
    void InitializeCharacterBaseMeshTable();
    void InitializeLootBoxTable();
    void InitializeResourceTable();
    void InitializeBuildingSkinTable();
    void InitializeBuildingLightTable();
    void InitializeDecorationCardTable();
    void InitializeDecorationDataTable();
    void InitializeWorkstationCardTable();
    void InitializeWorkstationItemSkinTable();
    void InitializeWorkstationSetupLevelTable();
    void InitializeUIIconTable();
    void InitializeMoneyVFXTable();
    void InitializeGameProjectTable();
    void InitializeTradeOrderBalanceTable();
    void InitializeMarketBalanceTable();
    void InitializeVFXTable();
    void InitializeTierUnlockTable();
    void InitializeCountryInfoTable();
    void InitializeMissionTable();
    void InitializeCountryDemandTable();
    void InitializeCountryMarketRoleTable();
    void InitializeDepartmentDisplayTable();
    void InitializeQualityGradeDisplayTable();
    void InitializeEmployeePotentialDisplayTables();
    void InitializeTradeOrderTierDisplayTable();
    void InitializeProjectModeDisplayTable();
    void InitializeStepDisplayNameTable();
    void InitializeDisciplineDisplayTable();
    void InitializeUIVFXTexTable();
    void InitializeLaunchLootBandTable();
    void InitializeHQLevelTable();
    void InitializeMenuUnlockTable();
    void InitializeProfileImageTable();

    // 비동기 로딩 관리
    FStreamableManager StreamableManager;

    // Preload된 메시 참조 유지 (GC 방지)
    UPROPERTY()
    TArray<USkeletalMesh*> PreloadedHairMeshes;

    UPROPERTY()
    TArray<USkeletalMesh*> PreloadedClothingMeshes;

    // 비동기 로딩 핸들
    TSharedPtr<FStreamableHandle> HairMeshLoadHandle;
    TSharedPtr<FStreamableHandle> ClothingMeshLoadHandle;
    TSharedPtr<FStreamableHandle> HairGirlMeshLoadHandle;
    TSharedPtr<FStreamableHandle> ClothingGirlMeshLoadHandle;

    // 비동기 로딩 완료 콜백
    void OnHairMeshesLoaded();
    void OnClothingMeshesLoaded();
    void OnHairGirlMeshesLoaded();
    void OnClothingGirlMeshesLoaded();
};
