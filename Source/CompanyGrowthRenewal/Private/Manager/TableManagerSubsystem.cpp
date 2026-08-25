// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/TableManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Table/WidgetDataTable.h"
#include "Table/ConstructionCost.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"

#include "Table/BuildingData.h"
#include "Table/InteractableInfo.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Enum/ProjectStepType.h"
#include "Core/CGGameInstance.h"

UTableManagerSubsystem::UTableManagerSubsystem()
{
    static ConstructorHelpers::FObjectFinder<UDataTable> DT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass'")
    );
    if (DT.Succeeded())
    {
        WidgetDataTable = DT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> UIVFXT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/UI/DT_UIVFXTexture.DT_UIVFXTexture'")
    );
    if (UIVFXT.Succeeded())
    {
        UIVFXTexDataTable = UIVFXT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> LLBT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_LaunchLootBand.DT_LaunchLootBand'")
    );
    if (LLBT.Succeeded())
    {
        LaunchLootBandDataTable = LLBT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> EVTICON(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_EventChoiceIcon.DT_EventChoiceIcon'")
    );
    if (EVTICON.Succeeded())
    {
        EventChoiceIconDataTable = EVTICON.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> SHOPIT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Shop/DT_ShopItem.DT_ShopItem'")
    );
    if (SHOPIT.Succeeded())
    {
        ShopItemDataTable = SHOPIT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> LNCHLOOT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_LaunchLoot.DT_LaunchLoot'")
    );
    if (LNCHLOOT.Succeeded())
    {
        LaunchLootDataTable = LNCHLOOT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> TPOT(
        TEXT("DataTable'/Game/CompanyGrowth/_Dev/Table/DT_TestProductionOrders.DT_TestProductionOrders'")
    );
    if (TPOT.Succeeded())
    {
        TestProductionOrdersTable = TPOT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> TRST(
        TEXT("DataTable'/Game/CompanyGrowth/_Dev/Table/DT_TestResourceScenarios.DT_TestResourceScenarios'")
    );
    if (TRST.Succeeded())
    {
        TestResourceScenariosTable = TRST.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> DPPT(
        TEXT("DataTable'/Game/CompanyGrowth/_Dev/Table/DT_DevProgressPreset.DT_DevProgressPreset'")
    );
    if (DPPT.Succeeded())
    {
        DevProgressPresetTable = DPPT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> DPBT(
        TEXT("DataTable'/Game/CompanyGrowth/_Dev/Table/DT_DevPresetBuilding.DT_DevPresetBuilding'")
    );
    if (DPBT.Succeeded())
    {
        DevPresetBuildingTable = DPBT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> BT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/BuildableTable.BuildableTable'")
    );
    if (BT.Succeeded())
    {
        BuildableDataTable = BT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> ET(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_EmployeeCards.DT_EmployeeCards'")
    );
    if (ET.Succeeded())
    {
        EmployeeCardDataTable = ET.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> GIDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/InteractableDataTable.InteractableDataTable'")
    );
    if (GIDT.Succeeded())
    {
        InteractableDataTable = GIDT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> BDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/BuildingDataTable.BuildingDataTable'")
    );
    if (BDT.Succeeded())
    {
        BuildingDataTable = BDT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> LNT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_LastNames.DT_LastNames'")
    );
    if (LNT.Succeeded())
    {
        LastNameDataTable = LNT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> FNT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_FirstNames.DT_FirstNames'")
    );
    if (FNT.Succeeded())
    {
        FirstNameDataTable = FNT.Object;
    }

    // Character Appearance DataTables
    static ConstructorHelpers::FObjectFinder<UDataTable> HPT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_HairPart.DT_HairPart'")
    );
    if (HPT.Succeeded())
    {
        HairPartDataTable = HPT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> RCT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_RankClothing.DT_RankClothing'")
    );
    if (RCT.Succeeded())
    {
        RankClothingDataTable = RCT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> HPGT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_HairPartGirl.DT_HairPartGirl'")
    );
    if (HPGT.Succeeded())
    {
        HairPartGirlDataTable = HPGT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> RCGT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_RankClothingGirl.DT_RankClothingGirl'")
    );
    if (RCGT.Succeeded())
    {
        RankClothingGirlDataTable = RCGT.Object;
    }

    // Worker Cosmetic (스틱맨 직원 외형: 색 + 안경)
    static ConstructorHelpers::FObjectFinder<UDataTable> WrkCosmeticT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_WorkerCosmetic.DT_WorkerCosmetic'")
    );
    if (WrkCosmeticT.Succeeded())
    {
        WorkerCosmeticDataTable = WrkCosmeticT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> ECT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_EyeColor.DT_EyeColor'")
    );
    if (ECT.Succeeded())
    {
        EyeColorDataTable = ECT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> SCT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_SkinColor.DT_SkinColor'")
    );
    if (SCT.Succeeded())
    {
        SkinColorDataTable = SCT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> HCT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_HairColor.DT_HairColor'")
    );
    if (HCT.Succeeded())
    {
        HairColorDataTable = HCT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> CBMT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_CharacterBaseMesh.DT_CharacterBaseMesh'")
    );
    if (CBMT.Succeeded())
    {
        CharacterBaseMeshDataTable = CBMT.Object;
    }

    // LootBox DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> LBT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/DT_LootBox.DT_LootBox'")
    );
    if (LBT.Succeeded())
    {
        LootBoxDataTable = LBT.Object;
    }

    // Resource DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> RT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/DT_Resource.DT_Resource'")
    );
    if (RT.Succeeded())
    {
        ResourceDataTable = RT.Object;
    }

    // BuildingSkin DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> BST(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/DT_BuildingSkin.DT_BuildingSkin'")
    );
    if (BST.Succeeded())
    {
        BuildingSkinDataTable = BST.Object;
    }

    // BuildingLight DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> BLDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/DT_BuildingLight.DT_BuildingLight'")
    );
    if (BLDT.Succeeded())
    {
        BuildingLightDataTable = BLDT.Object;
    }

    // Decoration DataTables
    static ConstructorHelpers::FObjectFinder<UDataTable> DCT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_DecorationCard.DT_DecorationCard'")
    );
    if (DCT.Succeeded())
    {
        DecorationCardDataTable = DCT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> DecoDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_DecorationData.DT_DecorationData'")
    );
    if (DecoDT.Succeeded())
    {
        DecorationDataDataTable = DecoDT.Object;
    }

    // Workstation DataTables
    static ConstructorHelpers::FObjectFinder<UDataTable> WCT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_WorkstationCard.DT_WorkstationCard'")
    );
    if (WCT.Succeeded())
    {
        WorkstationCardDataTable = WCT.Object;
    }

    // Workstation Item Skin DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> WIST(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_WorkstationItemSkin.DT_WorkstationItemSkin'")
    );
    if (WIST.Succeeded())
    {
        WorkstationItemSkinDataTable = WIST.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> WSLT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_WorkstationSetupLevel.DT_WorkstationSetupLevel'")
    );
    if (WSLT.Succeeded())
    {
        WorkstationSetupLevelDataTable = WSLT.Object;
    }

    // UIIcon DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> UIIT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/UI/DT_UIIcons.DT_UIIcons'")
    );
    if (UIIT.Succeeded())
    {
        UIIconDataTable = UIIT.Object;
    }

    // MoneyVFX DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> MVFXT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/VFX/DT_MoneyVFX.DT_MoneyVFX'")
    );
    if (MVFXT.Succeeded())
    {
        MoneyVFXDataTable = MVFXT.Object;
    }

    // GameProject DataTable (산업별 6종)
    static ConstructorHelpers::FObjectFinder<UDataTable> GPT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_Project_Game.DT_Project_Game'")
    );
    if (GPT.Succeeded())
    {
        GameProjectDataTable = GPT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> PHA(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_Project_Electronics.DT_Project_Electronics'")
    );
    if (PHA.Succeeded()) { ProjectDataTable_Electronics = PHA.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> PAU(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_Project_Automobile.DT_Project_Automobile'")
    );
    if (PAU.Succeeded()) { ProjectDataTable_Automobile = PAU.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> PSE(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_Project_Semiconductor.DT_Project_Semiconductor'")
    );
    if (PSE.Succeeded()) { ProjectDataTable_Semiconductor = PSE.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> PFI(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_Project_Finance.DT_Project_Finance'")
    );
    if (PFI.Succeeded()) { ProjectDataTable_Finance = PFI.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> PIT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_Project_IT.DT_Project_IT'")
    );
    if (PIT.Succeeded()) { ProjectDataTable_IT = PIT.Object; }

    // GDS Core Loop 데이터층 (착수 발견형). 에셋 미생성 시 null 유지 — Init 에서 null 가드.
    static ConstructorHelpers::FObjectFinder<UDataTable> PGEN(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_ProjectGenre.DT_ProjectGenre'")
    );
    if (PGEN.Succeeded()) { ProjectGenreDataTable = PGEN.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> TRT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/UI/DT_TierRankTitle.DT_TierRankTitle'")
    );
    if (TRT.Succeeded()) { TierRankTitleDataTable = TRT.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> IPRF(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_IndustryProfile.DT_IndustryProfile'")
    );
    if (IPRF.Succeeded()) { IndustryProfileDataTable = IPRF.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> CRIT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_CriticDisplay.DT_CriticDisplay'")
    );
    if (CRIT.Succeeded()) { CriticDisplayDataTable = CRIT.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> RVCM(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_ReviewComment.DT_ReviewComment'")
    );
    if (RVCM.Succeeded()) { ReviewCommentDataTable = RVCM.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> BGMB(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_BoostGamble.DT_BoostGamble'")
    );
    if (BGMB.Succeeded()) { BoostGambleDataTable = BGMB.Object; }

    // Trade Order Balance DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> TOB(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Balance/DT_TradeOrderBalance.DT_TradeOrderBalance'")
    );
    if (TOB.Succeeded()) { TradeOrderBalanceDataTable = TOB.Object; }

    // Market Balance DataTable (시장 성장 곡선)
    static ConstructorHelpers::FObjectFinder<UDataTable> MBT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Balance/DT_MarketBalance.DT_MarketBalance'")
    );
    if (MBT.Succeeded()) { MarketBalanceDataTable = MBT.Object; }

    // VFX DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> VFXT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/VFX/DT_VFX.DT_VFX'")
    );
    if (VFXT.Succeeded())
    {
        VFXDataTable = VFXT.Object;
    }

    // TierUnlock DataTable — 티어별 강화 해금 (구 DT_BuildingLevel 의 UnlockedEnhancements 승계)
    static ConstructorHelpers::FObjectFinder<UDataTable> TUT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/DT_TierUnlock.DT_TierUnlock'")
    );
    if (TUT.Succeeded())
    {
        TierUnlockDataTable = TUT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> CIT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/WorldMap/DT_CountryInfo.DT_CountryInfo'")
    );
    if (CIT.Succeeded())
    {
        CountryInfoDataTable = CIT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> CDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/WorldMap/DT_CountryDemand.DT_CountryDemand'")
    );
    if (CDT.Succeeded())
    {
        CountryDemandDataTable = CDT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> CMRT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/WorldMap/DT_CountryMarketRole.DT_CountryMarketRole'")
    );
    if (CMRT.Succeeded())
    {
        CountryMarketRoleDataTable = CMRT.Object;
    }

    // Company Info (산업 메타: Icon, DisplayName, AccentColor)
    static ConstructorHelpers::FObjectFinder<UDataTable> CIDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_CompanyInfo.DT_CompanyInfo'")
    );
    if (CIDT.Succeeded())
    {
        CompanyInfoDataTable = CIDT.Object;
    }

    // Mission (미션 체인 정의 — RowName = MissionID)
    static ConstructorHelpers::FObjectFinder<UDataTable> MSDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Mission/DT_Mission.DT_Mission'")
    );
    if (MSDT.Succeeded())
    {
        MissionDataTable = MSDT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> GT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Mission/DT_Goal.DT_Goal'"));
    if (GT.Succeeded()) { GoalDataTable = GT.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> PNIT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/UI/DT_PanelIntro.DT_PanelIntro'"));
    if (PNIT.Succeeded()) { PanelIntroDataTable = PNIT.Object; }

    // Recipe DataTables (제조업 타입별)
    static ConstructorHelpers::FObjectFinder<UDataTable> RHT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/Recipe/DT_Recipe_Electronics.DT_Recipe_Electronics'")
    );
    if (RHT.Succeeded()) { RecipeDataTable_Electronics = RHT.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> RAT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/Recipe/DT_Recipe_Automobile.DT_Recipe_Automobile'")
    );
    if (RAT.Succeeded()) { RecipeDataTable_Automobile = RAT.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> RST(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/Recipe/DT_Recipe_Semiconductor.DT_Recipe_Semiconductor'")
    );
    if (RST.Succeeded()) { RecipeDataTable_Semiconductor = RST.Object; }

    // HQ Level DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> HQLT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_HQLevel.DT_HQLevel'")
    );
    if (HQLT.Succeeded())
    {
        HQLevelDataTable = HQLT.Object;
    }

    // City Plot DataTable (도시 블록 부지)
    static ConstructorHelpers::FObjectFinder<UDataTable> CPLT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/City/DT_CityPlot.DT_CityPlot'")
    );
    if (CPLT.Succeeded())
    {
        CityPlotDataTable = CPLT.Object;
    }

    // City Dressing DataTable (도시 거리·빈 부지 패치 슬롯)
    static ConstructorHelpers::FObjectFinder<UDataTable> CDRT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/City/DT_CityDressing.DT_CityDressing'")
    );
    if (CDRT.Succeeded())
    {
        CityDressingDataTable = CDRT.Object;
    }

    // City Company DataTable (도시 인수 — 스카이라인 가상회사)
    static ConstructorHelpers::FObjectFinder<UDataTable> CCMP(
        TEXT("DataTable'/Game/CompanyGrowth/Table/CityCompany/DT_CityCompany.DT_CityCompany'")
    );
    if (CCMP.Succeeded())
    {
        CityCompanyDataTable = CCMP.Object;
    }

    // Menu Unlock DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> MULT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Menu/DT_MenuUnlock.DT_MenuUnlock'")
    );
    if (MULT.Succeeded())
    {
        MenuUnlockDataTable = MULT.Object;
    }

    // Profile Image DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> PIDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/UI/DT_ProfileImage.DT_ProfileImage'")
    );
    if (PIDT.Succeeded())
    {
        ProfileImageDataTable = PIDT.Object;
    }

    // Department Display Name DataTable (산업 × 부서 → 표시명)
    static ConstructorHelpers::FObjectFinder<UDataTable> DDPT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_DepartmentDisplay.DT_DepartmentDisplay'")
    );
    if (DDPT.Succeeded())
    {
        DepartmentDisplayDataTable = DDPT.Object;
    }

    // Quality Grade Display DataTable (산업 × 품질등급 → 한국어 라벨)
    static ConstructorHelpers::FObjectFinder<UDataTable> QGDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_QualityGradeDisplay.DT_QualityGradeDisplay'")
    );
    if (QGDT.Succeeded())
    {
        QualityGradeDisplayDataTable = QGDT.Object;
    }

    // Employee Potential Option Display DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> EPDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_EmployeePotentialDisplay.DT_EmployeePotentialDisplay'")
    );
    if (EPDT.Succeeded())
    {
        PotentialOptionDisplayDataTable = EPDT.Object;
    }

    // Employee Additional Option Display DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> EADT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Employee/DT_EmployeeAdditionalDisplay.DT_EmployeeAdditionalDisplay'")
    );
    if (EADT.Succeeded())
    {
        AdditionalOptionDisplayDataTable = EADT.Object;
    }

    // Trade Order Tier Display DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> TOTT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/WorldMap/DT_TradeOrderTierDisplay.DT_TradeOrderTierDisplay'")
    );
    if (TOTT.Succeeded())
    {
        TradeOrderTierDisplayDataTable = TOTT.Object;
    }

    // Project Mode Display DataTable
    static ConstructorHelpers::FObjectFinder<UDataTable> PMDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_ProjectModeDisplay.DT_ProjectModeDisplay'")
    );
    if (PMDT.Succeeded())
    {
        ProjectModeDisplayDataTable = PMDT.Object;
    }

    // Step Display Name DataTable (산업 × variant × step → 표시명)
    static ConstructorHelpers::FObjectFinder<UDataTable> SDNT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_StepDisplayName.DT_StepDisplayName'")
    );
    if (SDNT.Succeeded())
    {
        StepDisplayNameDataTable = SDNT.Object;
    }

    // Discipline Display DataTable (산업 → 6직능 슬롯 표시명)
    static ConstructorHelpers::FObjectFinder<UDataTable> DDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Company/DT_DisciplineDisplay.DT_DisciplineDisplay'")
    );
    if (DDT.Succeeded())
    {
        DisciplineDisplayDataTable = DDT.Object;
    }

    // Building Enhancement Definition DataTable (슬롯 메타데이터 — 아이콘/이름/설명/카테고리/SortOrder)
    static ConstructorHelpers::FObjectFinder<UDataTable> BEDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/DT_BuildingEnhancementDefinition.DT_BuildingEnhancementDefinition'")
    );
    if (BEDT.Succeeded())
    {
        BuildingEnhancementDefinitionDataTable = BEDT.Object;
    }

    // World Factory Upgrade Definition DataTable (세계지도 국가 공장 강화 5종 — 메타데이터/밸런스)
    static ConstructorHelpers::FObjectFinder<UDataTable> WFUDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/WorldMap/DT_WorldFactoryUpgradeDefinition.DT_WorldFactoryUpgradeDefinition'")
    );
    if (WFUDT.Succeeded())
    {
        WorldFactoryUpgradeDefinitionDataTable = WFUDT.Object;
    }

    // Mine Upgrade Definition DataTable (세계지도 채광 강화 2종 — 메타데이터/밸런스)
    static ConstructorHelpers::FObjectFinder<UDataTable> MUDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/WorldMap/DT_MineUpgradeDefinition.DT_MineUpgradeDefinition'")
    );
    if (MUDT.Succeeded())
    {
        MineUpgradeDefinitionDataTable = MUDT.Object;
    }

    // Factory Upgrade Definition DataTable (BrickFactory 강화 5종 — 메타데이터/밸런스)
    static ConstructorHelpers::FObjectFinder<UDataTable> FUDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Factory/DT_FactoryUpgradeDefinition.DT_FactoryUpgradeDefinition'")
    );
    if (FUDT.Succeeded())
    {
        FactoryUpgradeDefinitionDataTable = FUDT.Object;
    }

    // Building Trait DataTable (BUILDING_TRAIT_SYSTEM v1.1 — 105행)
    static ConstructorHelpers::FObjectFinder<UDataTable> BTDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/DT_BuildingTrait.DT_BuildingTrait'")
    );
    if (BTDT.Succeeded())
    {
        BuildingTraitDataTable = BTDT.Object;
    }

    // Building Trait Set Bonus DataTable (38행 = 19분야 x 2/3세트)
    static ConstructorHelpers::FObjectFinder<UDataTable> BTSBDT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Building/DT_BuildingTraitSetBonus.DT_BuildingTraitSetBonus'")
    );
    if (BTSBDT.Succeeded())
    {
        BuildingTraitSetBonusDataTable = BTSBDT.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> OSPT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_OfficeStarterPreset.DT_OfficeStarterPreset'")
    );
    if (OSPT.Succeeded()) { OfficeStarterPresetDataTable = OSPT.Object; }

    static ConstructorHelpers::FObjectFinder<UDataTable> IMPT(
        TEXT("DataTable'/Game/CompanyGrowth/Table/Office/DT_IndustryMoodPalette.DT_IndustryMoodPalette'")
    );
    if (IMPT.Succeeded()) { IndustryMoodPaletteDataTable = IMPT.Object; }

}

void UTableManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    InitializeWidgetTable();
    InitializeBuildableTable();
    InitializeEmployeeCardTable();
    InitializeInteractableTable();
    InitializeBuildingTable();
    InitializeHairPartTable();
    InitializeRankClothingTable();
    InitializeHairPartGirlTable();
    InitializeRankClothingGirlTable();
    InitializeWorkerCosmeticTable();
    InitializeEyeColorTable();
    InitializeSkinColorTable();
    InitializeHairColorTable();
    InitializeCharacterBaseMeshTable();
    InitializeLootBoxTable();
    InitializeResourceTable();
    InitializeBuildingSkinTable();
    InitializeBuildingLightTable();
    InitializeDecorationCardTable();
    InitializeDecorationDataTable();
    InitializeWorkstationCardTable();
    InitializeWorkstationItemSkinTable();
    InitializeWorkstationSetupLevelTable();
    InitializeUIIconTable();
    InitializeMoneyVFXTable();
    InitializeGameProjectTable();
    InitializeProjectGenreTable();
    InitializeTierRankTitleTable();
    InitializeIndustryProfileTable();
    InitializeCriticDisplayTable();
    InitializeReviewCommentTable();
    InitializeBoostGambleTable();
    InitializeTradeOrderBalanceTable();
    InitializeMarketBalanceTable();
    InitializeVFXTable();
    InitializeTierUnlockTable();
    InitializeCountryInfoTable();
    InitializeMissionTable();
    InitializeGoalTable();
    InitializePanelIntroTable();
    InitializeCountryDemandTable();
    InitializeCountryMarketRoleTable();
    InitializeCompanyInfoTable();
    InitializeDepartmentDisplayTable();
    InitializeQualityGradeDisplayTable();
    InitializeEmployeePotentialDisplayTables();
    InitializeTradeOrderTierDisplayTable();
    InitializeProjectModeDisplayTable();
    InitializeStepDisplayNameTable();
    InitializeDisciplineDisplayTable();
    InitializeUIVFXTexTable();
    InitializeLaunchLootBandTable();
    InitializeShopItemTable();
    InitializeLaunchLootTable();
    InitializeBuildingEnhancementDefinitionTable();
    InitializeWorldFactoryUpgradeDefinitionTable();
    InitializeMineUpgradeDefinitionTable();
    InitializeFactoryUpgradeDefinitionTable();
    InitializeBuildingTraitTable();
    InitializeBuildingTraitSetBonusTable();
    InitializeOfficeStarterPresetTable();
    InitializeEventChoiceIconTable();
    InitializeIndustryMoodPaletteTable();
    InitializeRecipeTable_Electronics();
    InitializeRecipeTable_Automobile();
    InitializeRecipeTable_Semiconductor();
    InitializeHQLevelTable();
    InitializeMenuUnlockTable();
    InitializeProfileImageTable();
    InitializeCityPlotTable();
    InitializeCityDressingTable();
    InitializeCityCompanyTable();

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] Widget Table Initialized (%d entries)"), WidgetTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildTable Table Initialized (%d entries)"), BuildTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] EmployeeCardTable Table Initialized (%d entries)"), EmployeeCardTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] InteractableTable Table Initialized (%d entries)"), InteractableTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingTable Table Initialized (%d entries)"), BuildingTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] HairCombinationTable Initialized (%d entries)"), HairPartTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] RankClothingTable Initialized (%d entries)"), RankClothingTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] EyeColorTable Initialized (%d entries)"), EyeColorTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] SkinColorTable Initialized (%d entries)"), SkinColorTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] HairColorTable Initialized (%d entries)"), HairColorTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] ResourceTable Initialized (%d entries)"), ResourceTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingSkinTable Initialized (%d entries)"), BuildingSkinTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingLightTable Initialized (%d entries)"), BuildingLightTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] DecorationCardTable Initialized (%d entries)"), DecorationCardTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] DecorationDataTable Initialized (%d entries)"), DecorationDataTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] WorkstationCardTable Initialized (%d entries)"), WorkstationCardTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] WorkstationItemSkinTable Initialized (%d entries)"), WorkstationItemSkinTable.Num());
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] HQLevelTable Initialized (%d entries)"), HQLevelTable.Num());
}

void UTableManagerSubsystem::Deinitialize()
{
    Super::Deinitialize();
}


void UTableManagerSubsystem::InitializeWidgetTable()
{
    if (!WidgetDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeWidgetTable: WidgetDataTable is not set! Please assign it in the editor."));
        return;
    }

    TArray<FWidgetDataTable*> Rows; 
    WidgetDataTable->GetAllRows<FWidgetDataTable>(TEXT("InitializeWidgetTable"), Rows);

    for (FWidgetDataTable* Row : Rows)
    {
        if (Row)
        {
            WidgetTable.Add(Row->WidgetType, Row->WidgetClass); // Row->WidgetType 사용
        }
    }
    UE_LOG(LogTemp, Log, TEXT("InitializeWidgetTable: Loaded %d widget entries."), WidgetTable.Num());
}

void UTableManagerSubsystem::InitializeBuildableTable()
{
    if (!BuildableDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeBuildableTable: BuildableDataTable is not set! Please assign it in the editor."));
        return;
    }

    // DataTable의 RowMap을 순회하면서 Row Name을 키로 사용
    const TMap<FName, uint8*>& RowMap = BuildableDataTable->GetRowMap();

    for (const auto& Pair : RowMap)
    {
        FName RowName = Pair.Key;
        FBuildableCardTable* Row = reinterpret_cast<FBuildableCardTable*>(Pair.Value);

        if (Row)
        {
            Row->RowName = RowName;  // RowName을 구조체에 저장
            BuildTable.Add(RowName, *Row);  // RowName을 키로 사용
        }
    }
    UE_LOG(LogTemp, Log, TEXT("InitializeBuildableTable: Loaded %d buildable entries."), BuildTable.Num());
}

void UTableManagerSubsystem::InitializeEmployeeCardTable()
{
    if (!EmployeeCardDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeEmployeeCardTable: EmployeeCardDataTable is not set! Please assign it in the editor."));
        return;
    }

    // DataTable의 RowMap을 순회하면서 Row Name을 키로 사용
    const TMap<FName, uint8*>& RowMap = EmployeeCardDataTable->GetRowMap();

    for (const auto& Pair : RowMap)
    {
        FName RowName = Pair.Key;
        FEmployeeCardTable* Row = reinterpret_cast<FEmployeeCardTable*>(Pair.Value);

        if (Row)
        {
            Row->RowName = RowName;  // RowName을 구조체에 저장
            EmployeeCardTable.Add(RowName, *Row);  // RowName을 키로 사용
        }
    }
    UE_LOG(LogTemp, Log, TEXT("InitializeEmployeeCardTable: Loaded %d employee entries."), EmployeeCardTable.Num());
}

void UTableManagerSubsystem::InitializeInteractableTable()
{
    if (!InteractableDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeInteractableTable: InteractableDataTable is not set! Please assign it in the editor."));
        return;
    }

    // DataTable의 RowMap을 순회하면서 Row Name을 키로 사용
    const TMap<FName, uint8*>& RowMap = InteractableDataTable->GetRowMap();

    for (const auto& Pair : RowMap)
    {
        FName RowName = Pair.Key;
        FInteractableInfo* Row = reinterpret_cast<FInteractableInfo*>(Pair.Value);

        if (Row)
        {
            Row->RowName = RowName;  // RowName을 구조체에 저장
            InteractableTable.Add(RowName, *Row);  // RowName을 키로 사용
        }
    }

    UE_LOG(LogTemp, Log, TEXT("InitializeInteractableMap: Loaded %d  interactable entries."), InteractableTable.Num());
}

void UTableManagerSubsystem::InitializeBuildingTable()
{
    if (!BuildingDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeBuildingTable: BuildingDataTable is not set! Please assign it in the editor."));
        return;
    }

    // DataTable의 RowMap을 순회하면서 Row Name을 키로 사용
    const TMap<FName, uint8*>& RowMap = BuildingDataTable->GetRowMap();

    for (const auto& Pair : RowMap)
    {
        FName RowName = Pair.Key;
        FBuildingData* Row = reinterpret_cast<FBuildingData*>(Pair.Value);

        if (Row)
        {
            BuildingTable.Add(RowName, *Row);  // Row Name을 키로 사용
        }
    }

    UE_LOG(LogTemp, Log, TEXT("InitializeBuildingTable: Loaded %d building entries."), BuildingTable.Num());
}

void UTableManagerSubsystem::InitializeHairPartTable()
{
    if (!HairPartDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHairPartTable: HairPartDataTable is not set!"));
        return;
    }

    TArray<FHairPartTable*> Rows;
    HairPartDataTable->GetAllRows<FHairPartTable>(TEXT("InitializeHairPartTable"), Rows);

    // 1단계: 먼저 HairPartTable에 Row 추가
    for (FHairPartTable* Row : Rows)
    {
        if (Row)
        {
            HairPartTable.Add(FName(*Row->PartName), *Row);
        }
    }

    // 2단계: 비동기 로딩을 위한 경로 수집
    TArray<FSoftObjectPath> AssetsToLoad;
    for (FHairPartTable* Row : Rows)
    {
        if (Row && Row->HairMesh.ToSoftObjectPath().IsValid())
        {
            AssetsToLoad.Add(Row->HairMesh.ToSoftObjectPath());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Starting async load for %d hair meshes (Male)..."), AssetsToLoad.Num());

    // StreamableManager로 비동기 로딩 시작
    HairMeshLoadHandle = StreamableManager.RequestAsyncLoad(
        AssetsToLoad,
        FStreamableDelegate::CreateUObject(this, &UTableManagerSubsystem::OnHairMeshesLoaded),
        FStreamableManager::AsyncLoadHighPriority
    );

    UE_LOG(LogTemp, Log, TEXT("InitializeHairPartTable (Male): %d parts registered, meshes loading async..."),
        HairPartTable.Num());
}

void UTableManagerSubsystem::InitializeRankClothingTable()
{
    if (!RankClothingDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeRankClothingTable: RankClothingDataTable is not set!"));
        return;
    }

    TArray<FRankClothingTable*> Rows;
    RankClothingDataTable->GetAllRows<FRankClothingTable>(TEXT("InitializeRankClothingTable"), Rows);

    // 중복 제거를 위해 unique mesh path만 수집
    TSet<FSoftObjectPath> UniqueTopMeshes;
    TSet<FSoftObjectPath> UniqueAccessoryMeshes;
    TSet<FSoftObjectPath> UniqueShoeMeshes;

    // 1단계: 모든 Rank의 mesh path 수집 (중복 제거)
    for (FRankClothingTable* Row : Rows)
    {
        if (Row)
        {
            if (Row->TopMeshes.ToSoftObjectPath().IsValid())
            {
                UniqueTopMeshes.Add(Row->TopMeshes.ToSoftObjectPath());
            }
            if (Row->AccessoryMeshes.ToSoftObjectPath().IsValid())
            {
                UniqueAccessoryMeshes.Add(Row->AccessoryMeshes.ToSoftObjectPath());
            }
            for (const TSoftObjectPtr<USkeletalMesh>& ShoeMesh : Row->ShoeMeshes)
            {
                if (ShoeMesh.ToSoftObjectPath().IsValid())
                {
                    UniqueShoeMeshes.Add(ShoeMesh.ToSoftObjectPath());
                }
            }
        }
    }

    // 2단계: 비동기 Preload 시작
    TArray<FSoftObjectPath> AssetsToLoad;
    AssetsToLoad.Append(UniqueTopMeshes.Array());
    AssetsToLoad.Append(UniqueAccessoryMeshes.Array());
    AssetsToLoad.Append(UniqueShoeMeshes.Array());

    UE_LOG(LogTemp, Log, TEXT("Starting async load for %d clothing meshes (Male)..."), AssetsToLoad.Num());

    // StreamableManager로 비동기 로딩 시작
    ClothingMeshLoadHandle = StreamableManager.RequestAsyncLoad(
        AssetsToLoad,
        FStreamableDelegate::CreateUObject(this, &UTableManagerSubsystem::OnClothingMeshesLoaded),
        FStreamableManager::AsyncLoadHighPriority
    );

    // 3단계: RankClothingTable에 Row 추가 (메시는 비동기 로딩 중)
    for (FRankClothingTable* Row : Rows)
    {
        if (Row)
        {
            RankClothingTable.Add(FName(*FString::Printf(TEXT("Rank_%d"), (int32)Row->Rank)), *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("InitializeRankClothingTable (Male): %d ranks registered, meshes loading async..."),
        Rows.Num());
}

void UTableManagerSubsystem::InitializeHairPartGirlTable()
{
    if (!HairPartGirlDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHairPartGirlTable: HairPartGirlDataTable is not set!"));
        return;
    }

    TArray<FHairPartTable*> Rows;
    HairPartGirlDataTable->GetAllRows<FHairPartTable>(TEXT("InitializeHairPartGirlTable"), Rows);

    // 1단계: 먼저 HairPartGirlTable에 Row 추가
    for (FHairPartTable* Row : Rows)
    {
        if (Row)
        {
            HairPartGirlTable.Add(FName(*Row->PartName), *Row);
        }
    }

    // 2단계: 비동기 로딩을 위한 경로 수집
    TArray<FSoftObjectPath> AssetsToLoad;
    for (FHairPartTable* Row : Rows)
    {
        if (Row && Row->HairMesh.ToSoftObjectPath().IsValid())
        {
            AssetsToLoad.Add(Row->HairMesh.ToSoftObjectPath());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Starting async load for %d hair meshes (Female)..."), AssetsToLoad.Num());

    // StreamableManager로 비동기 로딩 시작
    HairGirlMeshLoadHandle = StreamableManager.RequestAsyncLoad(
        AssetsToLoad,
        FStreamableDelegate::CreateUObject(this, &UTableManagerSubsystem::OnHairGirlMeshesLoaded),
        FStreamableManager::AsyncLoadHighPriority
    );

    UE_LOG(LogTemp, Log, TEXT("InitializeHairPartGirlTable (Female): %d parts registered, meshes loading async..."),
        HairPartGirlTable.Num());
}

void UTableManagerSubsystem::InitializeRankClothingGirlTable()
{
    if (!RankClothingGirlDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeRankClothingGirlTable: RankClothingDataTable is not set!"));
        return;
    }

    TArray<FRankClothingTable*> Rows;
    RankClothingGirlDataTable->GetAllRows<FRankClothingTable>(TEXT("InitializeRankClothingGirlTable"), Rows);

    // 중복 제거를 위해 unique mesh path만 수집
    TSet<FSoftObjectPath> UniqueTopMeshes;
    TSet<FSoftObjectPath> UniqueAccessoryMeshes;
    TSet<FSoftObjectPath> UniqueShoeMeshes;

    // 1단계: 모든 Rank의 mesh path 수집 (중복 제거)
    for (FRankClothingTable* Row : Rows)
    {
        if (Row)
        {
            if (Row->TopMeshes.ToSoftObjectPath().IsValid())
            {
                UniqueTopMeshes.Add(Row->TopMeshes.ToSoftObjectPath());
            }
            if (Row->AccessoryMeshes.ToSoftObjectPath().IsValid())
            {
                UniqueAccessoryMeshes.Add(Row->AccessoryMeshes.ToSoftObjectPath());
            }
            for (const TSoftObjectPtr<USkeletalMesh>& ShoeMesh : Row->ShoeMeshes)
            {
                if (ShoeMesh.ToSoftObjectPath().IsValid())
                {
                    UniqueShoeMeshes.Add(ShoeMesh.ToSoftObjectPath());
                }
            }
        }
    }

    // 2단계: 비동기 Preload 시작
    TArray<FSoftObjectPath> AssetsToLoad;
    AssetsToLoad.Append(UniqueTopMeshes.Array());
    AssetsToLoad.Append(UniqueAccessoryMeshes.Array());
    AssetsToLoad.Append(UniqueShoeMeshes.Array());

    UE_LOG(LogTemp, Log, TEXT("Starting async load for %d clothing meshes (Female)..."), AssetsToLoad.Num());

    // StreamableManager로 비동기 로딩 시작
    ClothingGirlMeshLoadHandle = StreamableManager.RequestAsyncLoad(
        AssetsToLoad,
        FStreamableDelegate::CreateUObject(this, &UTableManagerSubsystem::OnClothingGirlMeshesLoaded),
        FStreamableManager::AsyncLoadHighPriority
    );

    // 3단계: RankClothingGirlTable에 Row 추가 (메시는 비동기 로딩 중)
    for (FRankClothingTable* Row : Rows)
    {
        if (Row)
        {
            RankClothingGirlTable.Add(FName(*FString::Printf(TEXT("Rank_%d"), (int32)Row->Rank)), *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("InitializeRankClothingGirlTable (Female): %d ranks registered, meshes loading async..."),
        Rows.Num());
}

void UTableManagerSubsystem::InitializeEyeColorTable()
{
    if (!EyeColorDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeEyeColorTable: EyeColorDataTable is not set!"));
        return;
    }

    TArray<FEyeColorTable*> Rows;
    EyeColorDataTable->GetAllRows<FEyeColorTable>(TEXT("InitializeEyeColorTable"), Rows);

    for (FEyeColorTable* Row : Rows)
    {
        if (Row)
        {
            EyeColorTable.Add(FName(*Row->ColorSetName), *Row);
        }
    }
}

void UTableManagerSubsystem::InitializeSkinColorTable()
{
    if (!SkinColorDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeSkinColorTable: SkinColorDataTable is not set!"));
        return;
    }

    TArray<FSkinColorTable*> Rows;
    SkinColorDataTable->GetAllRows<FSkinColorTable>(TEXT("InitializeSkinColorTable"), Rows);

    for (FSkinColorTable* Row : Rows)
    {
        if (Row)
        {
            SkinColorTable.Add(FName(*Row->ColorSetName), *Row);
        }
    }
}

void UTableManagerSubsystem::InitializeHairColorTable()
{
    if (!HairColorDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHairColorTable: HairColorDataTable is not set!"));
        return;
    }

    TArray<FHairColorTable*> Rows;
    HairColorDataTable->GetAllRows<FHairColorTable>(TEXT("InitializeHairColorTable"), Rows);

    for (FHairColorTable* Row : Rows)
    {
        if (Row)
        {
            HairColorTable.Add(FName(*Row->ColorSetName), *Row);
        }
    }
}

void UTableManagerSubsystem::InitializeCharacterBaseMeshTable()
{
    if (!CharacterBaseMeshDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeCharacterBaseMeshTable: CharacterBaseMeshDataTable is not set!"));
        return;
    }

    TArray<FCharacterBaseMeshTable*> Rows;
    CharacterBaseMeshDataTable->GetAllRows<FCharacterBaseMeshTable>(TEXT("InitializeCharacterBaseMeshTable"), Rows);

    for (FCharacterBaseMeshTable* Row : Rows)
    {
        if (Row)
        {
            CharacterBaseMeshTable.Add(Row->Gender, *Row);
        }
    }
}

void UTableManagerSubsystem::InitializeLootBoxTable()
{
    if (!LootBoxDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("LootBoxDataTable is nullptr!"));
        return;
    }

    LootBoxTable.Empty();

    TArray<FName> RowNames = LootBoxDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FLootBoxTable* Row = LootBoxDataTable->FindRow<FLootBoxTable>(RowName, TEXT("InitializeLootBoxTable"));
        if (Row)
        {
            LootBoxTable.Add(RowName, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] LootBox Table Initialized (%d entries)"), LootBoxTable.Num());
}

TSubclassOf<UUserWidget> UTableManagerSubsystem::GetWidgetClass(EWidgetType Type) const
{
    if (const TSubclassOf<UUserWidget>* Found = WidgetTable.Find(Type))
    {
        return *Found;
    }
    return nullptr;
}

TArray<FBuildableCardTable> UTableManagerSubsystem::GetBuildableInfos() const
{
    TArray<FBuildableCardTable> BuildableInfos;
    BuildableInfos.Reserve(BuildTable.Num());

    for (const auto& Pair : BuildTable)
    {
        BuildableInfos.Add(Pair.Value);
    }

    return BuildableInfos;
}

bool UTableManagerSubsystem::GetBuildableInfo(FName RowName, FBuildableCardTable& OutInfo) const
{
    if (const FBuildableCardTable* Found = BuildTable.Find(RowName))
    {
        OutInfo = *Found;
        return true;
    }
    return false;
}

TArray<FEmployeeCardTable> UTableManagerSubsystem::GetEmployeeCardInfos() const
{
    TArray<FEmployeeCardTable> EmployeeCardInfos;
    EmployeeCardInfos.Reserve(EmployeeCardTable.Num());

    for (const auto& Pair : EmployeeCardTable)
    {
        EmployeeCardInfos.Add(Pair.Value);
    }

    return EmployeeCardInfos;
}

FInteractableInfo UTableManagerSubsystem::GetInteractableInfo(FName RowName, bool& bOutSuccess) const
{
    bOutSuccess = false;
    if (InteractableTable.Contains(RowName))
    {
        bOutSuccess = true;
        return InteractableTable[RowName];
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManager] Row '%s' not found in InteractableTable!"), *RowName.ToString());
    return FInteractableInfo();
}

FBuildingData UTableManagerSubsystem::GetBuildingData(FName RowName, bool& bOutSuccess) const
{
    bOutSuccess = false;
    if (BuildingTable.Contains(RowName)) // TMap에서 조회
    {
        bOutSuccess = true;
        return BuildingTable[RowName];
    }

    UE_LOG(LogTemp, Warning, TEXT("GetBuildingData: Row '%s' not found in BuildingTable!"), *RowName.ToString());
    return FBuildingData();
}

FHairPartTable* UTableManagerSubsystem::GetHairPartByName(const FString& PartName, EEmployeeGender Gender)
{
    UE_LOG(LogTemp, Warning, TEXT("GetHairPartByName: Searching for '%s', Gender=%d"), *PartName, (int32)Gender);

    // 성별에 따라 다른 테이블 선택
    TMap<FName, FHairPartTable>* TargetTable = nullptr;

    if (Gender == EEmployeeGender::Female)
    {
        TargetTable = &HairPartGirlTable;
        UE_LOG(LogTemp, Warning, TEXT("Using HairPartGirlTable, Size=%d"), HairPartGirlTable.Num());
    }
    else
    {
        TargetTable = &HairPartTable;
        UE_LOG(LogTemp, Warning, TEXT("Using HairPartTable (Male), Size=%d"), HairPartTable.Num());
    }

    if (!TargetTable) return nullptr;

    // 해당 테이블에서 파트명 찾기
    for (auto& Pair : *TargetTable)
    {
        FHairPartTable& HairPart = Pair.Value;
        UE_LOG(LogTemp, Warning, TEXT("  Checking Row: Key='%s', PartName='%s', HairMesh.IsValid=%d"),
            *Pair.Key.ToString(), *HairPart.PartName, HairPart.HairMesh.IsValid());

        if (HairPart.PartName == PartName)
        {
            UE_LOG(LogTemp, Warning, TEXT("  FOUND: PartName matches. HairMesh='%s', IsValid=%d"),
                *HairPart.HairMesh.ToString(), HairPart.HairMesh.IsValid());
            return &HairPart;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("  NOT FOUND: '%s' not in table"), *PartName);
    return nullptr;
}

FRankClothingTable* UTableManagerSubsystem::GetClothingForRank(EEmployeeRank Rank, EEmployeeGender Gender)
{
    // 성별에 따라 다른 테이블 선택
    TMap<FName, FRankClothingTable>* TargetTable = nullptr;

    if (Gender == EEmployeeGender::Female)
    {
        TargetTable = &RankClothingGirlTable; // 새로운 여성용 테이블
    }
    else
    {
        TargetTable = &RankClothingTable; // 기존 남성용 테이블
    }

    if (!TargetTable) return nullptr;

    // 해당 테이블에서 랭크 찾기
    for (auto& Pair : *TargetTable)
    {
        FRankClothingTable& ClothingData = Pair.Value;
        if (ClothingData.Rank == Rank)
        {
            return &ClothingData;
        }
    }
    return nullptr;
}

void UTableManagerSubsystem::InitializeWorkerCosmeticTable()
{
    if (!WorkerCosmeticDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] WorkerCosmeticDataTable is NULL"));
        return;
    }

    WorkerCosmeticTableMap.Empty();

    const TArray<FName> RowNames = WorkerCosmeticDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        if (const FWorkerCosmeticTable* Row = WorkerCosmeticDataTable->FindRow<FWorkerCosmeticTable>(RowName, TEXT("InitializeWorkerCosmeticTable")))
        {
            WorkerCosmeticTableMap.Add(RowName, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] WorkerCosmeticTable Initialized (%d entries)"), WorkerCosmeticTableMap.Num());
}

FWorkerCosmeticTable UTableManagerSubsystem::GetWorkerCosmeticData(FName RowName, bool& bOutSuccess) const
{
    if (const FWorkerCosmeticTable* Found = WorkerCosmeticTableMap.Find(RowName))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GetWorkerCosmeticData '%s' not found"), *RowName.ToString());
    bOutSuccess = false;
    return FWorkerCosmeticTable();
}

FEyeColorTable* UTableManagerSubsystem::GetRandomEyeColor()
{
    if (EyeColorTable.Num() == 0) return nullptr;

    TArray<FName> Keys;
    EyeColorTable.GetKeys(Keys);

    FName RandomKey = Keys[FMath::RandRange(0, Keys.Num() - 1)];
    return &EyeColorTable[RandomKey];
}

FSkinColorTable* UTableManagerSubsystem::GetRandomSkinColor()
{
    if (SkinColorTable.Num() == 0) return nullptr;

    TArray<FName> Keys;
    SkinColorTable.GetKeys(Keys);

    FName RandomKey = Keys[FMath::RandRange(0, Keys.Num() - 1)];
    return &SkinColorTable[RandomKey];
}

FHairColorTable* UTableManagerSubsystem::GetRandomHairColor()
{
    if (HairColorTable.Num() == 0) return nullptr;

    TArray<FName> Keys;
    HairColorTable.GetKeys(Keys);

    FName RandomKey = Keys[FMath::RandRange(0, Keys.Num() - 1)];
    return &HairColorTable[RandomKey];
}

FCharacterBaseMeshTable* UTableManagerSubsystem::GetCharacterBaseMesh(EEmployeeGender Gender)
{
    if (CharacterBaseMeshTable.Contains(Gender))
    {
        return &CharacterBaseMeshTable[Gender];
    }

    UE_LOG(LogTemp, Warning, TEXT("GetCharacterBaseMesh: Gender '%d' not found in CharacterBaseMeshTable!"), (int32)Gender);
    return nullptr;
}

FLootBoxTable UTableManagerSubsystem::GetLootBoxData(FName RowName, bool& bOutSuccess) const
{
    if (const FLootBoxTable* Found = LootBoxTable.Find(RowName))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("GetLootBoxData: RowName '%s' not found!"), *RowName.ToString());
    bOutSuccess = false;
    return FLootBoxTable();
}

TArray<TPair<FName, FLootBoxTable>> UTableManagerSubsystem::GetLootBoxesByCategoryWithNames(ELootBoxCategory Category) const
{
    TArray<TPair<FName, FLootBoxTable>> Result;

    for (const auto& Pair : LootBoxTable)
    {
        if (Pair.Value.Category == Category)
        {
            Result.Add(TPair<FName, FLootBoxTable>(Pair.Key, Pair.Value));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] GetLootBoxesByCategoryWithNames(%d): Found %d lootboxes"),
        (int32)Category, Result.Num());

    return Result;
}

void UTableManagerSubsystem::InitializeResourceTable()
{
    if (!ResourceDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeResourceTable: ResourceDataTable is nullptr!"));
        return;
    }

    ResourceTable.Empty();

    TArray<FName> RowNames = ResourceDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FResourceInfo* Row = ResourceDataTable->FindRow<FResourceInfo>(RowName, TEXT("InitializeResourceTable"));
        if (Row)
        {
            ResourceTable.Add(Row->ResourceType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] Resource Table Initialized (%d entries)"), ResourceTable.Num());
}

FResourceInfo UTableManagerSubsystem::GetResourceInfo(EResourceType ResourceType, bool& bOutSuccess) const
{
    if (const FResourceInfo* Found = ResourceTable.Find(ResourceType))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("GetResourceInfo: ResourceType '%d' not found!"), (int32)ResourceType);
    bOutSuccess = false;
    return FResourceInfo();
}

void UTableManagerSubsystem::InitializeBuildingSkinTable()
{
    if (!BuildingSkinDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeBuildingSkinTable: BuildingSkinDataTable is nullptr!"));
        return;
    }

    BuildingSkinTable.Empty();

    TArray<FName> RowNames = BuildingSkinDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FBuildingSkinData* Row = BuildingSkinDataTable->FindRow<FBuildingSkinData>(RowName, TEXT("InitializeBuildingSkinTable"));
        if (Row)
        {
            BuildingSkinTable.Add(Row->SkinID, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingSkin Table Initialized (%d entries)"), BuildingSkinTable.Num());
}

FBuildingSkinData UTableManagerSubsystem::GetBuildingSkinData(int32 SkinID, bool& bOutSuccess) const
{
    if (const FBuildingSkinData* Found = BuildingSkinTable.Find(SkinID))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("GetBuildingSkinData: SkinID '%d' not found!"), SkinID);
    bOutSuccess = false;
    return FBuildingSkinData();
}

TArray<FBuildingSkinData> UTableManagerSubsystem::GetBuildingSkinsByRarity(ELootBoxRarity Rarity) const
{
    TArray<FBuildingSkinData> Result;

    for (const auto& Pair : BuildingSkinTable)
    {
        if (Pair.Value.Rarity == Rarity)
        {
            Result.Add(Pair.Value);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] GetBuildingSkinsByRarity(%d): Found %d skins"),
        (int32)Rarity, Result.Num());

    return Result;
}

void UTableManagerSubsystem::InitializeBuildingLightTable()
{
    if (!BuildingLightDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeBuildingLightTable: BuildingLightDataTable is nullptr!"));
        return;
    }

    BuildingLightTable.Empty();

    TArray<FName> RowNames = BuildingLightDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FBuildingLightData* Row = BuildingLightDataTable->FindRow<FBuildingLightData>(RowName, TEXT("InitializeBuildingLightTable"));
        if (Row)
        {
            BuildingLightTable.Add(Row->LightID, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingLight Table Initialized (%d entries)"), BuildingLightTable.Num());
}

FBuildingLightData UTableManagerSubsystem::GetBuildingLightData(int32 LightID, bool& bOutSuccess) const
{
    if (const FBuildingLightData* Found = BuildingLightTable.Find(LightID))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("GetBuildingLightData: LightID '%d' not found!"), LightID);
    bOutSuccess = false;
    return FBuildingLightData();
}

TArray<FBuildingLightData> UTableManagerSubsystem::GetBuildingLightsByRarity(ELootBoxRarity Rarity) const
{
    TArray<FBuildingLightData> Result;
    for (const auto& Pair : BuildingLightTable)
    {
        if (Pair.Value.Rarity == Rarity)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FBuildingLightData> UTableManagerSubsystem::GetAllBuildingLights() const
{
    TArray<FBuildingLightData> Result;
    Result.Reserve(BuildingLightTable.Num());
    for (const auto& Pair : BuildingLightTable)
    {
        Result.Add(Pair.Value);
    }
    // 카드 진열은 LightID 오름차순으로 (희귀도 낮은 → 높은 순서 보장)
    Result.Sort([](const FBuildingLightData& A, const FBuildingLightData& B) { return A.LightID < B.LightID; });
    return Result;
}

bool UTableManagerSubsystem::IsFullyInitialized() const
{
    // 필수 DataTable들이 로드되었는지 확인
    bool bDataTablesValid = InteractableDataTable != nullptr &&
                            BuildingDataTable != nullptr &&
                            WidgetDataTable != nullptr &&
                            BuildableDataTable != nullptr &&
                            EmployeeCardDataTable != nullptr;

    // 필수 TMap들이 초기화되었는지 확인
    bool bTablesPopulated = InteractableTable.Num() > 0 &&
                            BuildingTable.Num() > 0 &&
                            WidgetTable.Num() > 0 &&
                            BuildTable.Num() > 0 &&
                            EmployeeCardTable.Num() > 0;

    // 캐릭터 관련 테이블들 (optional이지만 있으면 초기화되어야 함)
    bool bCharacterTablesValid = true;
    if (HairPartDataTable != nullptr)
    {
        bCharacterTablesValid = bCharacterTablesValid && HairPartTable.Num() > 0;
    }
    if (RankClothingDataTable != nullptr)
    {
        bCharacterTablesValid = bCharacterTablesValid && RankClothingTable.Num() > 0;
    }

    bool bIsInitialized = bDataTablesValid && bTablesPopulated && bCharacterTablesValid;

    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Not fully initialized - DataTables: %s, Core tables: %s, Character tables: %s"),
            bDataTablesValid ? TEXT("Valid") : TEXT("Invalid"),
            bTablesPopulated ? TEXT("Populated") : TEXT("Empty"),
            bCharacterTablesValid ? TEXT("Valid") : TEXT("Invalid"));
    }

    return bIsInitialized;
}

// ========== 비동기 로딩 콜백 함수들 ==========

void UTableManagerSubsystem::OnClothingMeshesLoaded()
{
    if (!ClothingMeshLoadHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("OnClothingMeshesLoaded: Invalid load handle!"));
        return;
    }

    // 로드된 에셋 가져오기
    TArray<UObject*> LoadedAssets;
    ClothingMeshLoadHandle->GetLoadedAssets(LoadedAssets);

    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (UObject* Asset : LoadedAssets)
    {
        if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
        {
            // UPROPERTY 배열에 저장하여 GC 방지
            PreloadedClothingMeshes.Add(Mesh);
            SuccessCount++;
            UE_LOG(LogTemp, Log, TEXT("Preloaded clothing mesh (Male): %s"), *Mesh->GetName());
        }
        else
        {
            FailCount++;
            UE_LOG(LogTemp, Warning, TEXT("Failed to cast loaded asset to SkeletalMesh"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Async preload complete (Male): %d meshes loaded, %d failed"),
        SuccessCount, FailCount);

    // 로딩 핸들 해제
    ClothingMeshLoadHandle.Reset();
}

void UTableManagerSubsystem::OnClothingGirlMeshesLoaded()
{
    if (!ClothingGirlMeshLoadHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("OnClothingGirlMeshesLoaded: Invalid load handle!"));
        return;
    }

    // 로드된 에셋 가져오기
    TArray<UObject*> LoadedAssets;
    ClothingGirlMeshLoadHandle->GetLoadedAssets(LoadedAssets);

    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (UObject* Asset : LoadedAssets)
    {
        if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
        {
            // UPROPERTY 배열에 저장하여 GC 방지
            PreloadedClothingMeshes.Add(Mesh);
            SuccessCount++;
            UE_LOG(LogTemp, Log, TEXT("Preloaded clothing mesh (Female): %s"), *Mesh->GetName());
        }
        else
        {
            FailCount++;
            UE_LOG(LogTemp, Warning, TEXT("Failed to cast loaded asset to SkeletalMesh"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Async preload complete (Female): %d meshes loaded, %d failed"),
        SuccessCount, FailCount);

    // 로딩 핸들 해제
    ClothingGirlMeshLoadHandle.Reset();
}

void UTableManagerSubsystem::OnHairMeshesLoaded()
{
    if (!HairMeshLoadHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("OnHairMeshesLoaded: Invalid load handle!"));
        return;
    }

    // 로드된 에셋 가져오기
    TArray<UObject*> LoadedAssets;
    HairMeshLoadHandle->GetLoadedAssets(LoadedAssets);

    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (UObject* Asset : LoadedAssets)
    {
        if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
        {
            // UPROPERTY 배열에 저장하여 GC 방지
            PreloadedHairMeshes.Add(Mesh);
            SuccessCount++;
            UE_LOG(LogTemp, Log, TEXT("Preloaded hair mesh (Male): %s"), *Mesh->GetName());
        }
        else
        {
            FailCount++;
            UE_LOG(LogTemp, Warning, TEXT("Failed to cast loaded asset to SkeletalMesh"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Async preload complete (Male Hair): %d meshes loaded, %d failed"),
        SuccessCount, FailCount);

    // 로딩 핸들 해제
    HairMeshLoadHandle.Reset();
}

void UTableManagerSubsystem::OnHairGirlMeshesLoaded()
{
    if (!HairGirlMeshLoadHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("OnHairGirlMeshesLoaded: Invalid load handle!"));
        return;
    }

    // 로드된 에셋 가져오기
    TArray<UObject*> LoadedAssets;
    HairGirlMeshLoadHandle->GetLoadedAssets(LoadedAssets);

    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (UObject* Asset : LoadedAssets)
    {
        if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
        {
            // UPROPERTY 배열에 저장하여 GC 방지
            PreloadedHairMeshes.Add(Mesh);
            SuccessCount++;
            UE_LOG(LogTemp, Log, TEXT("Preloaded hair mesh (Female): %s"), *Mesh->GetName());
        }
        else
        {
            FailCount++;
            UE_LOG(LogTemp, Warning, TEXT("Failed to cast loaded asset to SkeletalMesh"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Async preload complete (Female Hair): %d meshes loaded, %d failed"),
        SuccessCount, FailCount);

    // 로딩 핸들 해제
    HairGirlMeshLoadHandle.Reset();
}

// ========== Decoration Card Table ==========

void UTableManagerSubsystem::InitializeDecorationCardTable()
{
    if (!DecorationCardDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[TableManagerSubsystem] DecorationCardDataTable is NULL"));
        return;
    }

    DecorationCardTable.Empty();

    TArray<FName> RowNames = DecorationCardDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FDecorationCardTable* Row = DecorationCardDataTable->FindRow<FDecorationCardTable>(RowName, TEXT(""));
        if (Row)
        {
            Row->RowName = RowName;  // RowName 저장
            DecorationCardTable.Add(RowName, *Row);
        }
    }
}

TArray<FDecorationCardTable> UTableManagerSubsystem::GetDecorationCardInfos() const
{
    TArray<FDecorationCardTable> Result;
    for (const auto& Pair : DecorationCardTable)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FDecorationCardTable> UTableManagerSubsystem::GetDecorationCardsByCategory(EDecorationCategory Category) const
{
    TArray<FDecorationCardTable> Result;
    for (const auto& Pair : DecorationCardTable)
    {
        if (Pair.Value.Category == Category)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FDecorationCardTable> UTableManagerSubsystem::GetDecorationCardsBySurface(EDecorationSurface Surface) const
{
    TArray<FDecorationCardTable> Result;
    for (const auto& Pair : DecorationCardTable)
    {
        if (Pair.Value.AllowedSurface == Surface)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

FDecorationCardTable UTableManagerSubsystem::GetDecorationCardInfo(FName RowName, bool& bOutSuccess) const
{
    const FDecorationCardTable* Found = DecorationCardTable.Find(RowName);
    if (Found)
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] DecorationCardInfo not found: %s"), *RowName.ToString());
    bOutSuccess = false;
    return FDecorationCardTable();
}

// ========== Decoration Data Table ==========

void UTableManagerSubsystem::InitializeDecorationDataTable()
{
    if (!DecorationDataDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[TableManagerSubsystem] DecorationDataDataTable is NULL"));
        return;
    }

    DecorationDataTable.Empty();

    TArray<FName> RowNames = DecorationDataDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FDecorationData* Row = DecorationDataDataTable->FindRow<FDecorationData>(RowName, TEXT(""));
        if (Row)
        {
            DecorationDataTable.Add(RowName, *Row);
        }
    }
}

FDecorationData UTableManagerSubsystem::GetDecorationData(FName RowName, bool& bOutSuccess) const
{
    const FDecorationData* Found = DecorationDataTable.Find(RowName);
    if (Found)
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] DecorationData not found: %s"), *RowName.ToString());
    bOutSuccess = false;
    return FDecorationData();
}

// ========== Workstation Card Table ==========

void UTableManagerSubsystem::InitializeWorkstationCardTable()
{
    if (!WorkstationCardDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] WorkstationCardDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    WorkstationCardTable.Empty();

    TArray<FName> RowNames = WorkstationCardDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FWorkstationCardTable* Row = WorkstationCardDataTable->FindRow<FWorkstationCardTable>(RowName, TEXT(""));
        if (Row)
        {
            Row->RowName = RowName;
            WorkstationCardTable.Add(RowName, *Row);
        }
    }
}

TArray<FWorkstationCardTable> UTableManagerSubsystem::GetWorkstationCardInfos() const
{
    TArray<FWorkstationCardTable> Result;
    for (const auto& Pair : WorkstationCardTable)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

FWorkstationCardTable UTableManagerSubsystem::GetWorkstationCardInfo(FName RowName, bool& bOutSuccess) const
{
    const FWorkstationCardTable* Found = WorkstationCardTable.Find(RowName);
    if (Found)
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] WorkstationCardInfo not found: %s"), *RowName.ToString());
    bOutSuccess = false;
    return FWorkstationCardTable();
}

TArray<FWorkstationCardTable> UTableManagerSubsystem::GetWorkstationCardsByType(EWorkstationType InWorkstationType) const
{
    TArray<FWorkstationCardTable> Result;
    for (const auto& Pair : WorkstationCardTable)
    {
        if (Pair.Value.WorkstationType == InWorkstationType)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

// ========== Workstation Item Skin Table ==========

void UTableManagerSubsystem::InitializeWorkstationItemSkinTable()
{
    if (!WorkstationItemSkinDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] WorkstationItemSkinDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    WorkstationItemSkinTable.Empty();

    TArray<FName> RowNames = WorkstationItemSkinDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FWorkstationItemSkinData* Row = WorkstationItemSkinDataTable->FindRow<FWorkstationItemSkinData>(RowName, TEXT(""));
        if (Row)
        {
            Row->SkinID = RowName;
            WorkstationItemSkinTable.Add(RowName, *Row);
        }
    }
}

TArray<FWorkstationItemSkinData> UTableManagerSubsystem::GetWorkstationItemSkinInfos() const
{
    TArray<FWorkstationItemSkinData> Result;
    for (const auto& Pair : WorkstationItemSkinTable)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

FWorkstationItemSkinData UTableManagerSubsystem::GetWorkstationItemSkinInfo(FName RowName, bool& bOutSuccess) const
{
    const FWorkstationItemSkinData* Found = WorkstationItemSkinTable.Find(RowName);
    if (Found)
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] WorkstationItemSkinInfo not found: %s"), *RowName.ToString());
    bOutSuccess = false;
    return FWorkstationItemSkinData();
}

TArray<FWorkstationItemSkinData> UTableManagerSubsystem::GetWorkstationItemSkinsBySlot(EWorkstationSlot Slot) const
{
    TArray<FWorkstationItemSkinData> Result;
    for (const auto& Pair : WorkstationItemSkinTable)
    {
        if (Pair.Value.TargetSlot == Slot)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

void UTableManagerSubsystem::InitializeWorkstationSetupLevelTable()
{
    WorkstationSetupLevelTable.Empty();
    if (!WorkstationSetupLevelDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] DT_WorkstationSetupLevel is missing"));
        return;
    }

    for (const FName& RowName : WorkstationSetupLevelDataTable->GetRowNames())
    {
        const FComputerSetupLevelData* Row =
            WorkstationSetupLevelDataTable->FindRow<FComputerSetupLevelData>(RowName, TEXT("Workstation setup cache"));
        if (!Row)
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Invalid workstation setup row: %s"), *RowName.ToString());
            continue;
        }

        FString ValidationError;
        if (!Row->IsValidConfiguration(&ValidationError))
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Rejected workstation setup row %s: %s"),
                *RowName.ToString(), *ValidationError);
            continue;
        }

        // 기대 이름은 리플렉션에서 직접 얻는다 — "Level%d" 로 재구성하면 Level6Twin 처럼 순번 밖 이름이 탈락한다
        const UEnum* LevelEnum = StaticEnum<EComputerSetupLevel>();
        const FString ExpectedRowName = LevelEnum
            ? LevelEnum->GetNameStringByValue(static_cast<int64>(Row->Level))
            : FString();
        if (ExpectedRowName.IsEmpty() || RowName != FName(*ExpectedRowName)
            || WorkstationSetupLevelTable.Contains(Row->Level))
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Duplicate or mismatched workstation setup row: %s (expected %s)"),
                *RowName.ToString(), *ExpectedRowName);
            continue;
        }

        WorkstationSetupLevelTable.Add(Row->Level, *Row);
    }

    // Max 는 마지막 선언이라 그 값이 곧 실제 레벨 행 수(외형 변형 포함)다
    const int32 ExpectedLevelCount = static_cast<int32>(EComputerSetupLevel::Max);
    if (WorkstationSetupLevelTable.Num() != ExpectedLevelCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Workstation setup table has %d/%d valid rows"),
            WorkstationSetupLevelTable.Num(), ExpectedLevelCount);
    }
}

const FComputerSetupLevelData* UTableManagerSubsystem::FindWorkstationSetupLevelData(
    EComputerSetupLevel Level) const
{
    const FComputerSetupLevelData* Found = WorkstationSetupLevelTable.Find(Level);
    if (!Found)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Workstation setup Level%d not found"),
            static_cast<int32>(Level) + 1);
    }
    return Found;
}

// UI Icon Data
void UTableManagerSubsystem::InitializeUIIconTable()
{
    if (!UIIconDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] UIIconDataTable is not set!"));
        return;
    }

    const TMap<FName, uint8*>& RowMap = UIIconDataTable->GetRowMap();
    for (const auto& Pair : RowMap)
    {
        FName RowName = Pair.Key;
        FUIIconData* Row = reinterpret_cast<FUIIconData*>(Pair.Value);

        if (Row)
        {
            UIIconTable.Add(RowName, *Row);
            UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] UIIcon Loaded: %s (Category: %d)"),
                *RowName.ToString(), (int32)Row->Category);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] UIIconTable Initialized (%d entries)"), UIIconTable.Num());
}

FUIIconData UTableManagerSubsystem::GetUIIconData(FName RowName, bool& bOutSuccess) const
{
    const FUIIconData* Found = UIIconTable.Find(RowName);
    if (Found)
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] UIIconData not found: %s"), *RowName.ToString());
    bOutSuccess = false;
    return FUIIconData();
}

TArray<FUIIconData> UTableManagerSubsystem::GetUIIconsByCategory(EUIIconCategory Category) const
{
    TArray<FUIIconData> Result;

    for (const auto& Pair : UIIconTable)
    {
        if (Pair.Value.Category == Category)
        {
            Result.Add(Pair.Value);
        }
    }

    return Result;
}

// ========== Money VFX Table ==========

void UTableManagerSubsystem::InitializeMoneyVFXTable()
{
    if (!MoneyVFXDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] MoneyVFXDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    MoneyVFXTable.Empty();

    TArray<FName> RowNames = MoneyVFXDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FMoneyVFXTable* Row = MoneyVFXDataTable->FindRow<FMoneyVFXTable>(RowName, TEXT("InitializeMoneyVFXTable"));
        if (Row)
        {
            MoneyVFXTable.Add(*Row);
        }
    }

    // MinAmount 기준으로 오름차순 정렬
    MoneyVFXTable.Sort([](const FMoneyVFXTable& A, const FMoneyVFXTable& B)
    {
        return A.MinAmount < B.MinAmount;
    });

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] MoneyVFXTable Initialized (%d entries)"), MoneyVFXTable.Num());
}

FMoneyVFXTable UTableManagerSubsystem::GetMoneyVFXByAmount(int32 Amount, bool& bOutSuccess) const
{
    bOutSuccess = false;

    // 정렬된 배열을 역순으로 순회 (가장 높은 MinAmount부터)
    for (int32 i = MoneyVFXTable.Num() - 1; i >= 0; --i)
    {
        const FMoneyVFXTable& VFXData = MoneyVFXTable[i];

        // Amount >= MinAmount 체크
        if (Amount >= VFXData.MinAmount)
        {
            // MaxAmount == 0 이면 무제한, 그렇지 않으면 Amount < MaxAmount 체크
            if (VFXData.MaxAmount == 0 || Amount < VFXData.MaxAmount)
            {
                bOutSuccess = true;
                return VFXData;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] No MoneyVFX found for amount: %d"), Amount);
    return FMoneyVFXTable();
}

// ========== Game Project Table ==========

void UTableManagerSubsystem::InitializeGameProjectTable()
{
    GameProjectTable.Empty();
    AllProjectTable.Empty();

    // 테이블 포인터 자체를 산업의 진실 소스로 사용. Row->CompanyType은 보조 필드로만 취급.
    // CSV re-import 누락 등으로 Row->CompanyType이 stale해도 테이블 기준으로 올바른 복합키 구성.
    struct FTableCompanyPair
    {
        UDataTable* Table;
        ECompanyType Company;
    };
    const FTableCompanyPair AllTables[] = {
        { GameProjectDataTable,             ECompanyType::Game           },
        { ProjectDataTable_Electronics,  ECompanyType::Electronics },
        { ProjectDataTable_Automobile,      ECompanyType::Automobile     },
        { ProjectDataTable_Semiconductor,   ECompanyType::Semiconductor  },
        { ProjectDataTable_Finance,         ECompanyType::Finance        },
        { ProjectDataTable_IT,              ECompanyType::IT             },
    };

    int32 MismatchCount = 0;
    for (const FTableCompanyPair& Pair : AllTables)
    {
        UDataTable* Table = Pair.Table;
        const ECompanyType TableCompany = Pair.Company;
        if (!Table) continue;

        for (const FName& RowName : Table->GetRowNames())
        {
            const FProjectData* Row = Table->FindRow<FProjectData>(RowName, TEXT("InitializeGameProjectTable"));
            if (!Row) continue;

            FProjectData Entry = *Row;
            if (Entry.CompanyType != TableCompany)
            {
                // CSV/uasset 불일치 탐지 — 테이블 기준으로 보정하여 저장
                ++MismatchCount;
                Entry.CompanyType = TableCompany;
            }

            // 복합키 (TableCompany, ProjectIndex) — 항상 테이블 기준으로 저장
            const FIntPoint Key(static_cast<int32>(TableCompany), Entry.ProjectIndex);
            AllProjectTable.Add(Key, Entry);

            // 레거시: Game 산업만 ProjectIndex 단일키로도 유지
            if (TableCompany == ECompanyType::Game)
            {
                GameProjectTable.Add(Entry.ProjectIndex, Entry);
            }
        }
    }

    if (MismatchCount > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Row CompanyType 불일치 %d건 발견 — 테이블 기준으로 보정. CSV re-import 권장"),
            MismatchCount);
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] GameProjectTable=%d entries (Game), AllProjectTable=%d entries (All)"),
        GameProjectTable.Num(), AllProjectTable.Num());
}

FProjectData UTableManagerSubsystem::GetGameProjectData(int32 ProjectIndex, bool& bOutSuccess) const
{
    const FProjectData* Found = GameProjectTable.Find(ProjectIndex);
    if (Found)
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GameProjectData not found for ProjectIndex: %d"), ProjectIndex);
    bOutSuccess = false;
    return FProjectData();
}

FProjectData UTableManagerSubsystem::GetGameProjectDataByStage(int32 StageNumber, bool& bOutSuccess) const
{
    // Stage 1~4 → Project 1, Stage 5~8 → Project 2, ...
    int32 ProjectIndex = ((StageNumber - 1) / 4) + 1;
    return GetGameProjectData(ProjectIndex, bOutSuccess);
}

FProjectData UTableManagerSubsystem::GetProjectData(ECompanyType CompanyType, int32 ProjectIndex, bool& bOutSuccess) const
{
    const FIntPoint Key(static_cast<int32>(CompanyType), ProjectIndex);
    if (const FProjectData* Found = AllProjectTable.Find(Key))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ProjectData not found for Company=%d, ProjectIndex=%d"),
        static_cast<int32>(CompanyType), ProjectIndex);
    bOutSuccess = false;
    return FProjectData();
}

FProjectData UTableManagerSubsystem::ResolveProjectData(int32 ProjectIndex, bool& bOutSuccess) const
{
    ECompanyType CompanyType = ECompanyType::None;
    if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
    {
        CompanyType = GI->GetCurrentBuildingCompanyType();
    }
    if (CompanyType == ECompanyType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ResolveProjectData: 현재 빌딩 CompanyType 미확정 (ProjectIndex=%d)"), ProjectIndex);
        bOutSuccess = false;
        return FProjectData();
    }
    return GetProjectData(CompanyType, ProjectIndex, bOutSuccess);
}

TArray<FProjectData> UTableManagerSubsystem::GetProjectsByCompanyType(ECompanyType CompanyType) const
{
    TArray<FProjectData> Result;

    // 6개 산업 전부 담긴 AllProjectTable 에서 산업 필터링
    for (const auto& Pair : AllProjectTable)
    {
        if (Pair.Value.CompanyType == CompanyType)
        {
            Result.Add(Pair.Value);
        }
    }

    Result.Sort([](const FProjectData& A, const FProjectData& B) {
        return A.ProjectIndex < B.ProjectIndex;
    });

    return Result;
}

// ========== Trade Order Balance Table ==========

void UTableManagerSubsystem::InitializeTradeOrderBalanceTable()
{
    // 단일 row "Default" 만 사용. row 없으면 struct 기본값(헤더에 정의) 그대로 유지.
    if (!TradeOrderBalanceDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] TradeOrderBalanceDataTable is NULL - using struct defaults"));
        return;
    }

    if (const FTradeOrderBalanceData* Row = TradeOrderBalanceDataTable->FindRow<FTradeOrderBalanceData>(
        FName(TEXT("Default")), TEXT("InitializeTradeOrderBalanceTable")))
    {
        TradeOrderBalanceCache = *Row;
        UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] TradeOrderBalance loaded (TierWeightBase=%.2f, MaxActive=%d/%d/%d)"),
            TradeOrderBalanceCache.TierWeightBase,
            TradeOrderBalanceCache.MaxActive_Normal,
            TradeOrderBalanceCache.MaxActive_Urgent,
            TradeOrderBalanceCache.MaxActive_VIP);
    }
}

// ========== Market Balance Table ==========

void UTableManagerSubsystem::InitializeMarketBalanceTable()
{
    // 단일 row "Default" 가 단일 진실 소스. 누락 = 설정 오류이므로 조용히 넘기지 않고 loud failure.
    if (!ensureMsgf(MarketBalanceDataTable != nullptr,
        TEXT("[TableManagerSubsystem] DT_MarketBalance 누락 - /Game/CompanyGrowth/Table/Balance/DT_MarketBalance 임포트 필요")))
    {
        return;
    }

    const FMarketBalanceData* Row = MarketBalanceDataTable->FindRow<FMarketBalanceData>(
        FName(TEXT("Default")), TEXT("InitializeMarketBalanceTable"));
    if (!ensureMsgf(Row != nullptr,
        TEXT("[TableManagerSubsystem] DT_MarketBalance 'Default' row 누락")))
    {
        return;
    }

    MarketBalanceCache = *Row;
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] MarketBalance loaded (BaseMul=%.3f, Coef=%.2f, PivotMC=%lld, MaxMul=%.2f)"),
        MarketBalanceCache.BaseMul,
        MarketBalanceCache.Coef,
        MarketBalanceCache.PivotMC,
        MarketBalanceCache.MaxMul);
}

// ========== VFX Table ==========

void UTableManagerSubsystem::InitializeVFXTable()
{
    if (!VFXDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] VFXDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    VFXTable.Empty();

    TArray<FName> RowNames = VFXDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FVFXTableRow* Row = VFXDataTable->FindRow<FVFXTableRow>(RowName, TEXT("InitializeVFXTable"));
        if (Row && Row->VFXType != EVFXType::None)
        {
            VFXTable.Add(Row->VFXType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] VFXTable Initialized (%d entries)"), VFXTable.Num());
}

FVFXTableRow UTableManagerSubsystem::GetVFXData(EVFXType VFXType, bool& bOutSuccess) const
{
    const FVFXTableRow* Found = VFXTable.Find(VFXType);
    if (Found)
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] VFX not found for type: %d"), static_cast<int32>(VFXType));
    bOutSuccess = false;
    return FVFXTableRow();
}

UNiagaraSystem* UTableManagerSubsystem::GetVFXAsset(EVFXType VFXType)
{
    const FVFXTableRow* Found = VFXTable.Find(VFXType);
    if (Found && !Found->VFXAsset.IsNull())
    {
        return Found->VFXAsset.LoadSynchronous();
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] VFX asset not found for type: %d"), static_cast<int32>(VFXType));
    return nullptr;
}


void UTableManagerSubsystem::InitializeTierUnlockTable()
{
    if (!TierUnlockDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] TierUnlockDataTable is nullptr! 강화 해금이 전부 잠긴다."));
        return;
    }

    TierUnlockTable.Empty();

    TArray<FName> RowNames = TierUnlockDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FTierUnlockData* Row = TierUnlockDataTable->FindRow<FTierUnlockData>(RowName, TEXT("InitializeTierUnlockTable"));
        if (!Row)
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManager] TierUnlock Row '%s' → FindRow FAILED (wrong RowStruct?)"),
                *RowName.ToString());
            continue;
        }

        const int32 Tier = FCString::Atoi(*RowName.ToString());
        if (Tier <= 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManager] TierUnlock Row '%s' → Invalid tier (parsed as %d), SKIPPED"),
                *RowName.ToString(), Tier);
            continue;
        }

        TierUnlockTable.Add(Tier, *Row);
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] TierUnlock Table Initialized (%d entries)"), TierUnlockTable.Num());
}

FTierUnlockData UTableManagerSubsystem::GetTierUnlockData(int32 Tier, bool& bOutSuccess) const
{
    if (const FTierUnlockData* Found = TierUnlockTable.Find(Tier))
    {
        bOutSuccess = true;
        return *Found;
    }
    bOutSuccess = false;
    return FTierUnlockData();
}

TArray<EBuildingEnhancementType> UTableManagerSubsystem::GetUnlockedEnhancementsUpToTier(int32 Tier) const
{
    TArray<EBuildingEnhancementType> Result;
    for (int32 T = 1; T <= Tier; ++T)
    {
        if (const FTierUnlockData* Found = TierUnlockTable.Find(T))
        {
            for (EBuildingEnhancementType Type : Found->UnlockedEnhancements)
            {
                Result.AddUnique(Type);
            }
        }
    }
    return Result;
}


// ========== Country Info ==========

void UTableManagerSubsystem::InitializeCountryInfoTable()
{
    if (!CountryInfoDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CountryInfoDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    CountryInfoTable.Empty();

    TArray<FName> RowNames = CountryInfoDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FCountryInfoTable* Row = CountryInfoDataTable->FindRow<FCountryInfoTable>(RowName, TEXT("InitializeCountryInfoTable"));
        if (Row && Row->CountryType != ECountryType::None)
        {
            CountryInfoTable.Add(Row->CountryType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CountryInfoTable Initialized (%d entries)"), CountryInfoTable.Num());
}

FCountryInfoTable UTableManagerSubsystem::GetCountryInfo(ECountryType CountryType, bool& bOutSuccess) const
{
    if (const FCountryInfoTable* Found = CountryInfoTable.Find(CountryType))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CountryInfo not found for type: %d"), static_cast<int32>(CountryType));
    bOutSuccess = false;
    return FCountryInfoTable();
}

TArray<FCountryInfoTable> UTableManagerSubsystem::GetAllCountryInfos() const
{
    TArray<FCountryInfoTable> Out;
    Out.Reserve(CountryInfoTable.Num());
    for (const TPair<ECountryType, FCountryInfoTable>& Pair : CountryInfoTable)
    {
        Out.Add(Pair.Value);
    }
    return Out;
}

// ========== Mission ==========

void UTableManagerSubsystem::InitializeMissionTable()
{
    if (!MissionDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] MissionDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    MissionTable.Empty();

    TArray<FName> RowNames = MissionDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FMissionTable* Row = MissionDataTable->FindRow<FMissionTable>(RowName, TEXT("InitializeMissionTable"));
        if (Row)
        {
            MissionTable.Add(RowName, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] MissionTable Initialized (%d entries)"), MissionTable.Num());
}

FMissionTable UTableManagerSubsystem::GetMissionData(FName MissionID, bool& bOutSuccess) const
{
    if (const FMissionTable* Found = MissionTable.Find(MissionID))
    {
        bOutSuccess = true;
        return *Found;
    }

    bOutSuccess = false;
    return FMissionTable();
}

void UTableManagerSubsystem::InitializeGoalTable()
{
    GoalDataMap.Empty();
    GoalRowOrder.Empty();
    if (!GoalDataTable)
    {
        // 에셋은 Task 10 에디터 스크립트가 생성 — 그 전까진 빈 맵 (보드가 조용히 빈 상태)
        UE_LOG(LogTemp, Warning, TEXT("[TableManager] DT_Goal 미로드 — 미션판 데이터 없음"));
        return;
    }
    for (const FName RowName : GoalDataTable->GetRowNames())
    {
        if (const FGoalTable* Row = GoalDataTable->FindRow<FGoalTable>(
            RowName, TEXT("InitializeGoalTable"), /*bWarnIfRowMissing=*/false))
        {
            GoalDataMap.Add(RowName, *Row);
            GoalRowOrder.Add(RowName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManager] DT_Goal 행 로드 실패: %s"), *RowName.ToString());
        }
    }
}

void UTableManagerSubsystem::InitializePanelIntroTable()
{
    PanelIntroMap.Empty();
    if (!PanelIntroDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManager] DT_PanelIntro 미로드 — 패널 최초 진입 안내 없음"));
        return;
    }
    for (const auto& Pair : PanelIntroDataTable->GetRowMap())
    {
        const FPanelIntroTable* Row = reinterpret_cast<const FPanelIntroTable*>(Pair.Value);
        if (!Row || Row->PanelKey.IsNone())
        {
            continue;
        }
        PanelIntroMap.FindOrAdd(Row->PanelKey).Add(*Row);
    }
    // RowMap 순회 순서는 보장이 없다 — 재생 순서는 StepIndex 가 단독 권위
    for (auto& Pair : PanelIntroMap)
    {
        Pair.Value.Sort([](const FPanelIntroTable& A, const FPanelIntroTable& B)
        {
            return A.StepIndex < B.StepIndex;
        });
    }
}

bool UTableManagerSubsystem::GetGoalData(const FName& RowName, FGoalTable& OutData) const
{
    if (const FGoalTable* Found = GoalDataMap.Find(RowName))
    {
        OutData = *Found;
        return true;
    }
    return false;
}

// ========== Company Info ==========

void UTableManagerSubsystem::InitializeCompanyInfoTable()
{
    if (!CompanyInfoDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CompanyInfoDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    CompanyInfoTable.Empty();

    TArray<FName> RowNames = CompanyInfoDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FCompanyInfoTable* Row = CompanyInfoDataTable->FindRow<FCompanyInfoTable>(RowName, TEXT("InitializeCompanyInfoTable"));
        if (Row && Row->CompanyType != ECompanyType::None)
        {
            CompanyInfoTable.Add(Row->CompanyType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CompanyInfoTable Initialized (%d entries)"), CompanyInfoTable.Num());
}

FCompanyInfoTable UTableManagerSubsystem::GetCompanyInfo(ECompanyType CompanyType, bool& bOutSuccess) const
{
    if (const FCompanyInfoTable* Found = CompanyInfoTable.Find(CompanyType))
    {
        bOutSuccess = true;
        return *Found;
    }
    bOutSuccess = false;
    return FCompanyInfoTable();
}

// ========== Country Demand ==========

void UTableManagerSubsystem::InitializeCountryDemandTable()
{
    if (!CountryDemandDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CountryDemandDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    CountryDemandTable.Empty();

    TArray<FName> RowNames = CountryDemandDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FCountryDemandTable* Row = CountryDemandDataTable->FindRow<FCountryDemandTable>(RowName, TEXT("InitializeCountryDemandTable"));
        if (Row && Row->Country != ECountryType::None && Row->Industry != ECompanyType::None)
        {
            const FIntPoint Key(static_cast<int32>(Row->Country), static_cast<int32>(Row->Industry));
            CountryDemandTable.Add(Key, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CountryDemandTable Initialized (%d entries)"), CountryDemandTable.Num());
}

FCountryDemandTable UTableManagerSubsystem::GetCountryDemand(ECountryType CountryType, ECompanyType Industry, bool& bOutSuccess) const
{
    const FIntPoint Key(static_cast<int32>(CountryType), static_cast<int32>(Industry));
    if (const FCountryDemandTable* Found = CountryDemandTable.Find(Key))
    {
        bOutSuccess = true;
        return *Found;
    }

    bOutSuccess = false;
    return FCountryDemandTable();
}

TArray<FCountryDemandTable> UTableManagerSubsystem::GetAllCountryDemands() const
{
    TArray<FCountryDemandTable> Out;
    Out.Reserve(CountryDemandTable.Num());
    for (const TPair<FIntPoint, FCountryDemandTable>& Pair : CountryDemandTable)
    {
        Out.Add(Pair.Value);
    }
    return Out;
}

// ========== Country MarketRole ==========

void UTableManagerSubsystem::InitializeCountryMarketRoleTable()
{
    if (!CountryMarketRoleDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CountryMarketRoleDataTable is NULL - DataTable may not exist yet"));
        return;
    }

    CountryMarketRoleTable.Empty();

    TArray<FName> RowNames = CountryMarketRoleDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FCountryMarketRoleTable* Row = CountryMarketRoleDataTable->FindRow<FCountryMarketRoleTable>(RowName, TEXT("InitializeCountryMarketRoleTable"));
        if (Row && Row->Country != ECountryType::None && Row->Industry != ECompanyType::None)
        {
            const FIntPoint Key(static_cast<int32>(Row->Country), static_cast<int32>(Row->Industry));
            CountryMarketRoleTable.Add(Key, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CountryMarketRoleTable Initialized (%d entries)"), CountryMarketRoleTable.Num());
}

FText UTableManagerSubsystem::GetMarketRole(ECountryType CountryType, ECompanyType Industry, bool& bOutSuccess) const
{
    const FIntPoint Key(static_cast<int32>(CountryType), static_cast<int32>(Industry));
    if (const FCountryMarketRoleTable* Found = CountryMarketRoleTable.Find(Key))
    {
        bOutSuccess = true;
        return Found->MarketRole;
    }

    bOutSuccess = false;
    return FText::GetEmpty();
}

// ========== HQ Level Table ==========

void UTableManagerSubsystem::InitializeHQLevelTable()
{
    if (!HQLevelDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] HQLevelDataTable is nullptr! DataTable may not exist yet."));
        return;
    }

    HQLevelTable.Empty();

    TArray<FName> RowNames = HQLevelDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FHQLevelData* Row = HQLevelDataTable->FindRow<FHQLevelData>(RowName, TEXT("InitializeHQLevelTable"));
        if (Row)
        {
            int32 Level = FCString::Atoi(*RowName.ToString());

            if (Level > 0)
            {
                HQLevelTable.Add(Level, *Row);
                UE_LOG(LogTemp, Log, TEXT("[TableManager] HQLevel Row '%s' -> Lv.%d: MoneyCost=%lld"),
                    *RowName.ToString(), Level, Row->MoneyCost);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[TableManager] HQLevel Row '%s' -> Invalid level (parsed as %d), SKIPPED"),
                    *RowName.ToString(), Level);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManager] HQLevel Row '%s' -> FindRow FAILED (wrong RowStruct?)"),
                *RowName.ToString());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] HQLevel Table Initialized (%d entries)"), HQLevelTable.Num());
}

FHQLevelData UTableManagerSubsystem::GetHQLevelData(int32 Level, bool& bOutSuccess) const
{
    if (const FHQLevelData* Found = HQLevelTable.Find(Level))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] HQLevelData not found for Level: %d"), Level);
    bOutSuccess = false;
    return FHQLevelData();
}

// ========== City Plot Table ==========

void UTableManagerSubsystem::InitializeCityPlotTable()
{
    if (!CityPlotDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CityPlotDataTable is nullptr! DataTable may not exist yet."));
        return;
    }

    CityPlotMap.Empty();

    TArray<FName> RowNames = CityPlotDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FCityPlotData* Row = CityPlotDataTable->FindRow<FCityPlotData>(RowName, TEXT("InitializeCityPlotTable"));
        if (Row)
        {
            CityPlotMap.Add(RowName, *Row);
            UE_LOG(LogTemp, Log, TEXT("[TableManager] CityPlot Row '%s' -> Price=%lld, Capacity=%d, OwnedAtStart=%d"),
                *RowName.ToString(), Row->MoneyPrice, Row->BuildingCapacity, Row->bOwnedAtStart ? 1 : 0);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManager] CityPlot Row '%s' -> FindRow FAILED (wrong RowStruct?)"),
                *RowName.ToString());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CityPlot Table Initialized (%d entries)"), CityPlotMap.Num());
}

FCityPlotData UTableManagerSubsystem::GetCityPlotData(FName PlotId, bool& bOk) const
{
    if (const FCityPlotData* Found = CityPlotMap.Find(PlotId))
    {
        bOk = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CityPlotData not found for PlotId: %s"), *PlotId.ToString());
    bOk = false;
    return FCityPlotData();
}

void UTableManagerSubsystem::GetAllCityPlotRows(TArray<FName>& OutIds) const
{
    CityPlotMap.GetKeys(OutIds);
}

// ========== City Dressing Table ==========

void UTableManagerSubsystem::InitializeCityDressingTable()
{
    CityDressingMap.Empty();
    if (!CityDressingDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CityDressingDataTable is nullptr"));
        return;
    }

    for (const FName& RowName : CityDressingDataTable->GetRowNames())
    {
        if (const FCityDressingData* Row = CityDressingDataTable->FindRow<FCityDressingData>(
            RowName, TEXT("InitializeCityDressingTable")))
        {
            CityDressingMap.Add(RowName, *Row);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] Invalid CityDressing row: %s"),
                *RowName.ToString());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CityDressing Table Initialized (%d entries)"),
        CityDressingMap.Num());
}

void UTableManagerSubsystem::GetAllCityDressingRows(TArray<FCityDressingData>& OutRows) const
{
    CityDressingMap.GenerateValueArray(OutRows);
    OutRows.Sort([](const FCityDressingData& A, const FCityDressingData& B)
    {
        const int32 PresetCompare = A.PresetId.ToString().Compare(
            B.PresetId.ToString(), ESearchCase::CaseSensitive);
        if (PresetCompare != 0)
        {
            return PresetCompare < 0;
        }
        return A.SlotId.ToString().Compare(B.SlotId.ToString(), ESearchCase::CaseSensitive) < 0;
    });
}

// ========== City Company Table (도시 인수 — 스카이라인 가상회사) ==========

void UTableManagerSubsystem::InitializeCityCompanyTable()
{
    CityCompanyMap.Empty();
    if (!CityCompanyDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManager] CityCompanyDataTable is nullptr! (DT_CityCompany 미생성 — Task2 임포트 필요)"));
        return;
    }

    const TArray<FName> RowNames = CityCompanyDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FCityCompanyData* Row = CityCompanyDataTable->FindRow<FCityCompanyData>(RowName, TEXT("InitializeCityCompanyTable"));
        if (Row && Row->BuildingKey > 0)
        {
            CityCompanyMap.Add(Row->BuildingKey, *Row);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManager] CityCompany Row '%s' -> 무효(BuildingKey<=0 또는 RowStruct 불일치)"), *RowName.ToString());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CityCompany Table Initialized (%d entries)"), CityCompanyMap.Num());
}

bool UTableManagerSubsystem::GetCityCompanyData(int32 BuildingKey, FCityCompanyData& OutData) const
{
    if (const FCityCompanyData* Found = CityCompanyMap.Find(BuildingKey))
    {
        OutData = *Found;
        return true;
    }
    return false;
}

// ========== Menu Unlock Table ==========

void UTableManagerSubsystem::InitializeMenuUnlockTable()
{
    if (!MenuUnlockDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] MenuUnlockDataTable is nullptr! DataTable may not exist yet."));
        return;
    }

    MenuUnlockTable.Empty();

    TArray<FName> RowNames = MenuUnlockDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FMenuUnlockData* Row = MenuUnlockDataTable->FindRow<FMenuUnlockData>(RowName, TEXT("InitializeMenuUnlockTable"));
        if (Row)
        {
            MenuUnlockTable.Add(RowName, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] MenuUnlock Table Initialized (%d entries)"), MenuUnlockTable.Num());
}

FMenuUnlockData UTableManagerSubsystem::GetMenuUnlockData(FName ButtonName, bool& bOutSuccess) const
{
    if (const FMenuUnlockData* Found = MenuUnlockTable.Find(ButtonName))
    {
        bOutSuccess = true;
        return *Found;
    }

    bOutSuccess = false;
    return FMenuUnlockData();
}

// ========== Profile Image Table ==========

void UTableManagerSubsystem::InitializeProfileImageTable()
{
    if (!ProfileImageDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ProfileImageDataTable is nullptr! DataTable may not exist yet."));
        return;
    }

    ProfileImageTable.Empty();

    TArray<FName> RowNames = ProfileImageDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FProfileImageData* Row = ProfileImageDataTable->FindRow<FProfileImageData>(RowName, TEXT("InitializeProfileImageTable"));
        if (Row)
        {
            ProfileImageTable.Add(Row->ImageID, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] ProfileImage Table Initialized (%d entries)"), ProfileImageTable.Num());
}

FProfileImageData UTableManagerSubsystem::GetProfileImageData(int32 ImageID, bool& bOutSuccess) const
{
    if (const FProfileImageData* Found = ProfileImageTable.Find(ImageID))
    {
        bOutSuccess = true;
        return *Found;
    }

    bOutSuccess = false;
    return FProfileImageData();
}

TArray<FProfileImageData> UTableManagerSubsystem::GetAllProfileImages() const
{
    TArray<FProfileImageData> Result;
    ProfileImageTable.GenerateValueArray(Result);
    // ImageID 순으로 정렬
    Result.Sort([](const FProfileImageData& A, const FProfileImageData& B) { return A.ImageID < B.ImageID; });
    return Result;
}

// ========== Product Recipe Tables ==========

void UTableManagerSubsystem::InitializeRecipeTable_Electronics()
{
    if (!RecipeDataTable_Electronics)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] RecipeDataTable_Electronics is NULL - DataTable may not exist yet"));
        return;
    }

    RecipeTable_Electronics.Empty();
    TArray<FName> RowNames = RecipeDataTable_Electronics->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FProductRecipeTable* Row = RecipeDataTable_Electronics->FindRow<FProductRecipeTable>(RowName, TEXT("InitializeRecipeTable_Electronics"));
        if (Row)
        {
            RecipeTable_Electronics.Add(Row->ProjectIndex, *Row);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] RecipeTable_Electronics Initialized (%d entries)"), RecipeTable_Electronics.Num());
}

void UTableManagerSubsystem::InitializeRecipeTable_Automobile()
{
    if (!RecipeDataTable_Automobile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] RecipeDataTable_Automobile is NULL - DataTable may not exist yet"));
        return;
    }

    RecipeTable_Automobile.Empty();
    TArray<FName> RowNames = RecipeDataTable_Automobile->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FProductRecipeTable* Row = RecipeDataTable_Automobile->FindRow<FProductRecipeTable>(RowName, TEXT("InitializeRecipeTable_Automobile"));
        if (Row)
        {
            RecipeTable_Automobile.Add(Row->ProjectIndex, *Row);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] RecipeTable_Automobile Initialized (%d entries)"), RecipeTable_Automobile.Num());
}

void UTableManagerSubsystem::InitializeRecipeTable_Semiconductor()
{
    if (!RecipeDataTable_Semiconductor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] RecipeDataTable_Semiconductor is NULL - DataTable may not exist yet"));
        return;
    }

    RecipeTable_Semiconductor.Empty();
    TArray<FName> RowNames = RecipeDataTable_Semiconductor->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FProductRecipeTable* Row = RecipeDataTable_Semiconductor->FindRow<FProductRecipeTable>(RowName, TEXT("InitializeRecipeTable_Semiconductor"));
        if (Row)
        {
            RecipeTable_Semiconductor.Add(Row->ProjectIndex, *Row);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] RecipeTable_Semiconductor Initialized (%d entries)"), RecipeTable_Semiconductor.Num());
}

FProductRecipeTable UTableManagerSubsystem::GetProductRecipe(ECompanyType CompanyType, int32 ProjectIndex, bool& bOutSuccess) const
{
    const TMap<int32, FProductRecipeTable>* TargetTable = nullptr;
    switch (CompanyType)
    {
    case ECompanyType::Electronics: TargetTable = &RecipeTable_Electronics; break;
    case ECompanyType::Automobile:     TargetTable = &RecipeTable_Automobile;     break;
    case ECompanyType::Semiconductor:  TargetTable = &RecipeTable_Semiconductor;  break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GetProductRecipe: Invalid CompanyType %d"), static_cast<int32>(CompanyType));
        bOutSuccess = false;
        return FProductRecipeTable();
    }

    if (const FProductRecipeTable* Found = TargetTable->Find(ProjectIndex))
    {
        bOutSuccess = true;
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GetProductRecipe: Recipe not found for %s Index %d"),
        *CompanyTypeToString(CompanyType), ProjectIndex);
    bOutSuccess = false;
    return FProductRecipeTable();
}

void UTableManagerSubsystem::InitializeDepartmentDisplayTable()
{
    if (!DepartmentDisplayDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] DepartmentDisplayDataTable is NULL - falling back to game default DepartmentToString"));
        return;
    }

    DepartmentDisplayMap.Empty();

    TArray<FName> RowNames = DepartmentDisplayDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FDepartmentDisplayRow* Row = DepartmentDisplayDataTable->FindRow<FDepartmentDisplayRow>(RowName, TEXT("InitializeDepartmentDisplayTable"));
        if (Row)
        {
            FIntPoint Key = FDepartmentDisplayRow::MakeKey(Row->CompanyType, Row->Department);
            DepartmentDisplayMap.Add(Key, Row->DisplayName);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] DepartmentDisplay Table Initialized (%d entries)"), DepartmentDisplayMap.Num());
}

FText UTableManagerSubsystem::GetDepartmentDisplayName(ECompanyType CompanyType, EEmployeeDepartment Department) const
{
    // 폴백 1: 회사 컨텍스트 없거나 매핑 데이터 없으면 게임 기본값
    if (CompanyType != ECompanyType::None && DepartmentDisplayMap.Num() > 0)
    {
        const FIntPoint Key = FDepartmentDisplayRow::MakeKey(CompanyType, Department);
        if (const FText* Found = DepartmentDisplayMap.Find(Key))
        {
            return *Found;
        }
    }

    // 폴백 2: 기본 한글명 (게임 기준 = 개발팀, 기획팀, 영업팀, ...)
    return FText::FromString(DepartmentToString(Department));
}

// ========== Quality Grade Display Table ==========

void UTableManagerSubsystem::InitializeQualityGradeDisplayTable()
{
    QualityGradeDisplayMap.Empty();

    if (!QualityGradeDisplayDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] QualityGradeDisplayDataTable is NULL — DT_QualityGradeDisplay 자산 누락 또는 경로 불일치. 산업별 품질 등급 라벨 빈 FText 반환."));
        return;
    }

    for (const FName& RowName : QualityGradeDisplayDataTable->GetRowNames())
    {
        const FQualityGradeDisplayRow* Row = QualityGradeDisplayDataTable->FindRow<FQualityGradeDisplayRow>(RowName, TEXT("InitializeQualityGradeDisplayTable"));
        if (!Row) continue;

        const FIntPoint Key = FQualityGradeDisplayRow::MakeKey(Row->CompanyType, Row->Grade);
        QualityGradeDisplayMap.Add(Key, Row->DisplayName);
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] QualityGradeDisplay Table Initialized (%d entries)"), QualityGradeDisplayMap.Num());
}

FText UTableManagerSubsystem::GetQualityGradeLabel(ECompanyType CompanyType, EQualityGrade Grade) const
{
    if (CompanyType == ECompanyType::None) return FText::GetEmpty();

    const FIntPoint Key = FQualityGradeDisplayRow::MakeKey(CompanyType, Grade);
    if (const FText* Found = QualityGradeDisplayMap.Find(Key))
    {
        return *Found;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] QualityGradeLabel 미매핑: Company=%d, Grade=%d — DT_QualityGradeDisplay row 누락"),
        static_cast<int32>(CompanyType), static_cast<int32>(Grade));
    return FText::GetEmpty();
}

// ========== Employee Potential / Additional Option Display ==========

void UTableManagerSubsystem::InitializeEmployeePotentialDisplayTables()
{
    PotentialOptionDisplayMap.Empty();
    AdditionalOptionDisplayMap.Empty();

    if (PotentialOptionDisplayDataTable)
    {
        for (const FName& RowName : PotentialOptionDisplayDataTable->GetRowNames())
        {
            const FPotentialOptionDisplayRow* Row = PotentialOptionDisplayDataTable->FindRow<FPotentialOptionDisplayRow>(RowName, TEXT("InitializeEmployeePotentialDisplayTables"));
            if (!Row) continue;
            PotentialOptionDisplayMap.Add(Row->OptionType, Row->DisplayName);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] PotentialOptionDisplayDataTable is NULL — DT_EmployeePotentialDisplay 자산 누락"));
    }

    if (AdditionalOptionDisplayDataTable)
    {
        for (const FName& RowName : AdditionalOptionDisplayDataTable->GetRowNames())
        {
            const FAdditionalOptionDisplayRow* Row = AdditionalOptionDisplayDataTable->FindRow<FAdditionalOptionDisplayRow>(RowName, TEXT("InitializeEmployeePotentialDisplayTables"));
            if (!Row) continue;
            AdditionalOptionDisplayMap.Add(Row->OptionType, Row->DisplayName);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] AdditionalOptionDisplayDataTable is NULL — DT_EmployeeAdditionalDisplay 자산 누락"));
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] EmployeePotentialDisplay Tables Initialized (Potential=%d, Additional=%d entries)"),
        PotentialOptionDisplayMap.Num(), AdditionalOptionDisplayMap.Num());
}

FText UTableManagerSubsystem::GetPotentialOptionDisplayName(EPotentialOptionType OptionType) const
{
    if (const FText* Found = PotentialOptionDisplayMap.Find(OptionType))
    {
        return *Found;
    }
    return FText::GetEmpty();
}

FText UTableManagerSubsystem::GetAdditionalOptionDisplayName(EAdditionalOptionType OptionType) const
{
    if (const FText* Found = AdditionalOptionDisplayMap.Find(OptionType))
    {
        return *Found;
    }
    return FText::GetEmpty();
}

// ========== Trade Order Tier Display ==========

void UTableManagerSubsystem::InitializeTradeOrderTierDisplayTable()
{
    TradeOrderTierDisplayMap.Empty();
    if (!TradeOrderTierDisplayDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] TradeOrderTierDisplayDataTable is NULL — DT_TradeOrderTierDisplay 자산 누락"));
        return;
    }

    for (const FName& RowName : TradeOrderTierDisplayDataTable->GetRowNames())
    {
        const FTradeOrderTierDisplayRow* Row = TradeOrderTierDisplayDataTable->FindRow<FTradeOrderTierDisplayRow>(RowName, TEXT("InitializeTradeOrderTierDisplayTable"));
        if (!Row) continue;
        TradeOrderTierDisplayMap.Add(Row->Tier, *Row);
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] TradeOrderTierDisplay Table Initialized (%d entries)"), TradeOrderTierDisplayMap.Num());
}

bool UTableManagerSubsystem::GetTradeOrderTierDisplay(ETradeOrderTier Tier, FText& OutLabel, FLinearColor& OutColor) const
{
    if (const FTradeOrderTierDisplayRow* Found = TradeOrderTierDisplayMap.Find(Tier))
    {
        OutLabel = Found->Label;
        OutColor = Found->Color;
        return true;
    }
    return false;
}

// ========== Project Mode Display ==========

void UTableManagerSubsystem::InitializeProjectModeDisplayTable()
{
    ProjectModeDisplayMap.Empty();
    if (!ProjectModeDisplayDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ProjectModeDisplayDataTable is NULL — DT_ProjectModeDisplay 자산 누락"));
        return;
    }

    for (const FName& RowName : ProjectModeDisplayDataTable->GetRowNames())
    {
        const FProjectModeDisplayRow* Row = ProjectModeDisplayDataTable->FindRow<FProjectModeDisplayRow>(RowName, TEXT("InitializeProjectModeDisplayTable"));
        if (!Row) continue;
        ProjectModeDisplayMap.Add(Row->Mode, *Row);
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] ProjectModeDisplay Table Initialized (%d entries)"), ProjectModeDisplayMap.Num());
}

bool UTableManagerSubsystem::GetProjectModeDisplay(EProjectMode Mode, FString& OutPrefix, FString& OutSuffix) const
{
    if (const FProjectModeDisplayRow* Found = ProjectModeDisplayMap.Find(Mode))
    {
        OutPrefix = Found->Prefix;
        OutSuffix = Found->Suffix;
        return true;
    }
    return false;
}


// ========== Step Display Name Table ==========

namespace
{
    // ECompanyType → CSV / RowName 표준 문자열 (enum 이름과 동일)
    static FString CompanyTypeToRowToken(ECompanyType InType)
    {
        switch (InType)
        {
        case ECompanyType::Game:           return TEXT("Game");
        case ECompanyType::Electronics: return TEXT("Electronics");
        case ECompanyType::Finance:        return TEXT("Finance");
        case ECompanyType::IT:             return TEXT("IT");
        case ECompanyType::Semiconductor:  return TEXT("Semiconductor");
        case ECompanyType::Automobile:     return TEXT("Automobile");
        default:                           return FString();
        }
    }
}

void UTableManagerSubsystem::InitializeStepDisplayNameTable()
{
    if (!StepDisplayNameDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] StepDisplayNameDataTable is NULL - Step 이름 lookup은 빈 FText 반환"));
        return;
    }

    StepDisplayNameMap.Empty();

    TArray<FName> RowNames = StepDisplayNameDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FStepDisplayNameRow* Row = StepDisplayNameDataTable->FindRow<FStepDisplayNameRow>(RowName, TEXT("InitializeStepDisplayNameTable"));
        if (Row)
        {
            StepDisplayNameMap.Add(RowName, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] StepDisplayName Table Initialized (%d entries)"), StepDisplayNameMap.Num());
}

void UTableManagerSubsystem::InitializeDisciplineDisplayTable()
{
    if (!DisciplineDisplayDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] DisciplineDisplayDataTable is NULL - 직능 표시명은 빈 FText 반환"));
        return;
    }
    DisciplineDisplayMap.Empty();
    for (const FName& RowName : DisciplineDisplayDataTable->GetRowNames())
    {
        if (const FDisciplineDisplayRow* Row = DisciplineDisplayDataTable->FindRow<FDisciplineDisplayRow>(RowName, TEXT("InitializeDisciplineDisplayTable")))
        {
            const ECompanyType Company = StringToCompanyType(Row->CompanyType);
            if (Company == ECompanyType::None)
            {
                UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] DisciplineDisplay row '%s' CompanyType '%s' 미인식 - 스킵"), *RowName.ToString(), *Row->CompanyType);
                continue;
            }
            DisciplineDisplayMap.Add(Company, *Row);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] DisciplineDisplay Table Initialized (%d entries)"), DisciplineDisplayMap.Num());
}

FText UTableManagerSubsystem::GetDisciplineDisplayName(ECompanyType CompanyType, int32 SlotIndex) const
{
    if (const FDisciplineDisplayRow* Row = DisciplineDisplayMap.Find(CompanyType))
    {
        switch (SlotIndex)
        {
        case 0: return Row->Plan;
        case 1: return Row->Dev;
        case 2: return Row->Graphics;
        case 3: return Row->Sound;
        case 4: return Row->Server;
        case 5: return Row->QA;
        default: break;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] DisciplineDisplay 매핑 없음: Company=%d Slot=%d"), static_cast<int32>(CompanyType), SlotIndex);
    return FText::GetEmpty();
}

void UTableManagerSubsystem::InitializeUIVFXTexTable()
{
    if (!UIVFXTexDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] UIVFXTexDataTable is NULL - VFX 텍스처 주입은 조용히 생략됨"));
        return;
    }

    UIVFXTexMap.Empty();

    TArray<FName> RowNames = UIVFXTexDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        if (FUIVFXTexRow* Row = UIVFXTexDataTable->FindRow<FUIVFXTexRow>(RowName, TEXT("InitializeUIVFXTexTable")))
        {
            UIVFXTexMap.Add(RowName, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] UIVFXTex Table Initialized (%d entries)"), UIVFXTexMap.Num());
}

bool UTableManagerSubsystem::GetUIVFXTexRow(FName Key, FUIVFXTexRow& OutRow) const
{
    if (const FUIVFXTexRow* Found = UIVFXTexMap.Find(Key))
    {
        OutRow = *Found;
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] UIVFXTex row not found: %s"), *Key.ToString());
    return false;
}

void UTableManagerSubsystem::InitializeShopItemTable()
{
    if (!ShopItemDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ShopItemDataTable is NULL - 상점 그리드는 빈 상태로 표시됨"));
        return;
    }

    ShopItemMap.Empty();

    TArray<FName> RowNames = ShopItemDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        if (FShopItemTable* Row = ShopItemDataTable->FindRow<FShopItemTable>(RowName, TEXT("InitializeShopItemTable")))
        {
            ShopItemMap.Add(RowName, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] ShopItem Table Initialized (%d entries)"), ShopItemMap.Num());
}

void UTableManagerSubsystem::InitializeLaunchLootTable()
{
    if (!LaunchLootDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] LaunchLootDataTable is NULL - 출시 전리품 드랍은 빈 상태"));
        return;
    }

    LaunchLootMap.Empty();

    int32 EntryCount = 0;
    TArray<FName> RowNames = LaunchLootDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        if (FLaunchLootTable* Row = LaunchLootDataTable->FindRow<FLaunchLootTable>(RowName, TEXT("InitializeLaunchLootTable")))
        {
            // ReviewScoreToTableKey 가 Low/Mid/High 만 만들어내므로 그 밖의 키는 아무도 조회 못 하는 유령 그룹 = CSV 오타
            // (넣기는 한다 — 치트 LootRoll 로 임의 키를 조회할 수 있어야 오타 그룹을 눈으로 확인할 수 있다)
            if (Row->TableKey != FName(TEXT("Low")) && Row->TableKey != FName(TEXT("Mid")) && Row->TableKey != FName(TEXT("High")))
            {
                UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] LaunchLoot row '%s' TableKey '%s' 는 Low/Mid/High 밖 - 출시 롤이 절대 조회하지 않는다(CSV 오타 의심)"),
                    *RowName.ToString(), *Row->TableKey.ToString());
            }
            // 재화·아이템 배타 계약 — 둘 다 채우면 RollAndGrant 의 저장 분기가 재화만 채워, 병합 술어가 매 롤 새 카드를 만든다
            if (Row->ResourceType != EResourceType::None && Row->ItemType != EItemType::None)
            {
                UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] LaunchLoot row '%s' 가 ResourceType 과 ItemType 을 동시 지정 - 아이템이 무시된다(둘 중 하나만 채울 것)"),
                    *RowName.ToString());
            }
            if (Row->AmountMin < 1 || Row->AmountMax < Row->AmountMin)
            {
                UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] LaunchLoot row '%s' 지급량 범위 이상 (Min=%lld Max=%lld) - 0/음수는 UI 카드까지 새어나간다"),
                    *RowName.ToString(), Row->AmountMin, Row->AmountMax);
            }

            LaunchLootMap.FindOrAdd(Row->TableKey).Add(*Row);
            ++EntryCount;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] LaunchLoot Table Initialized (%d tables / %d entries)"), LaunchLootMap.Num(), EntryCount);
}

const TArray<FLaunchLootTable>* UTableManagerSubsystem::GetLaunchLootEntries(FName TableKey) const
{
    return LaunchLootMap.Find(TableKey);
}

bool UTableManagerSubsystem::GetShopItemRow(FName RowName, FShopItemTable& OutRow) const
{
    if (const FShopItemTable* Found = ShopItemMap.Find(RowName))
    {
        OutRow = *Found;
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ShopItem row not found: %s"), *RowName.ToString());
    return false;
}

UTexture2D* UTableManagerSubsystem::GetItemIcon(EItemType ItemType, bool& bOutFound) const
{
    bOutFound = false;
    if (ItemType == EItemType::None)
    {
        return nullptr;
    }
    // 한 EItemType 의 아이콘은 어느 상점 행이든 동일 가정 — 첫 매치 반환
    for (const TPair<FName, FShopItemTable>& Pair : ShopItemMap)
    {
        if (Pair.Value.Item == ItemType && !Pair.Value.Icon.IsNull())
        {
            bOutFound = true;
            return Pair.Value.Icon.LoadSynchronous();
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GetItemIcon: DT_ShopItem 에 아이콘 없음 (ItemType=%d)"), (int32)ItemType);
    return nullptr;
}

bool UTableManagerSubsystem::GetShopItemByItemType(EItemType ItemType, FShopItemTable& OutRow) const
{
    if (ItemType == EItemType::None)
    {
        return false;
    }
    // 한 EItemType 의 표시정보는 어느 상점 행이든 동일 가정 — 첫 매치 반환 (GetItemIcon 과 동일 정책)
    for (const TPair<FName, FShopItemTable>& Pair : ShopItemMap)
    {
        if (Pair.Value.Item == ItemType)
        {
            OutRow = Pair.Value;
            return true;
        }
    }
    // Loud failure: DT 행 누락(미임포트 등)을 즉시 드러냄 (GetItemIcon 과 동일 정책)
    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GetShopItemByItemType: DT_ShopItem 에 행 없음 (ItemType=%d)"), (int32)ItemType);
    return false;
}

TArray<FName> UTableManagerSubsystem::GetShopItemRowsForTab(EShopTab Tab) const
{
    TArray<TPair<FName, int32>> Found;
    for (const TPair<FName, FShopItemTable>& Pair : ShopItemMap)
    {
        if (Pair.Value.Tab == Tab)
        {
            Found.Emplace(Pair.Key, Pair.Value.SortOrder);
        }
    }
    Found.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B) { return A.Value < B.Value; });

    TArray<FName> Names;
    Names.Reserve(Found.Num());
    for (const TPair<FName, int32>& P : Found)
    {
        Names.Add(P.Key);
    }
    return Names;
}

bool UTableManagerSubsystem::ApplyUIVFXTexture(FName Key, UImage* Image) const
{
    if (!Image)
    {
        return false;
    }

    FUIVFXTexRow Row;
    if (!GetUIVFXTexRow(Key, Row) || Row.Texture.IsNull())
    {
        return false;
    }

    UTexture2D* Tex = Row.Texture.LoadSynchronous();
    if (!Tex)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] UIVFXTex texture load failed: %s"), *Key.ToString());
        return false;
    }

    FSlateBrush Brush;
    Brush.SetResourceObject(Tex);
    Brush.ImageSize = Row.ImageSize;
    Brush.TintColor = FSlateColor(Row.Tint);
    Image->SetBrush(Brush);
    return true;
}

void UTableManagerSubsystem::InitializeEventChoiceIconTable()
{
    EventChoiceIconMap.Empty();
    if (!EventChoiceIconDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] EventChoiceIconDataTable is NULL — DT_EventChoiceIcon 자산 누락 (이벤트 카드 아이콘 미표시)"));
        return;
    }

    for (const FName& RowName : EventChoiceIconDataTable->GetRowNames())
    {
        const FEventChoiceIconRow* Row = EventChoiceIconDataTable->FindRow<FEventChoiceIconRow>(RowName, TEXT("InitializeEventChoiceIconTable"));
        if (Row && Row->Archetype != EChoiceArchetype::None)
        {
            EventChoiceIconMap.Add(Row->Archetype, Row->Icon);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] EventChoiceIcon Table Initialized (%d entries)"), EventChoiceIconMap.Num());
}

UTexture2D* UTableManagerSubsystem::GetEventChoiceIcon(EChoiceArchetype Archetype) const
{
    if (Archetype == EChoiceArchetype::None)
    {
        return nullptr;
    }

    const TSoftObjectPtr<UTexture2D>* Found = EventChoiceIconMap.Find(Archetype);
    if (!Found || Found->IsNull())
    {
        return nullptr;
    }
    return Found->LoadSynchronous();
}

FText UTableManagerSubsystem::GetStepDisplayName(ECompanyType CompanyType, FName VariantKey, int32 StepNumber) const
{
    const FString CompanyToken = CompanyTypeToRowToken(CompanyType);

    if (StepDisplayNameMap.Num() > 0 && !CompanyToken.IsEmpty())
    {
        // 1) {CompanyType}_{Variant} 우선 lookup
        // composite row의 Step 이름이 빈값이면 → 단독 row 폴백 (variant가 해당 Step 미지정)
        if (!VariantKey.IsNone())
        {
            const FName CompositeKey(*FString::Printf(TEXT("%s_%s"), *CompanyToken, *VariantKey.ToString()));
            if (const FStepDisplayNameRow* Found = StepDisplayNameMap.Find(CompositeKey))
            {
                FText Name = Found->GetStepName(StepNumber);
                if (!Name.IsEmpty())
                {
                    return Name;
                }
            }
        }

        // 2) {CompanyType} 단독 lookup — row를 찾으면 결과 확정 (빈값도 존중, 코드 폴백 금지)
        // DT에서 의도적으로 비워둔 Step은 "이 산업은 이 Step이 없음"을 의미 → UI Collapsed 처리됨
        const FName CompanyOnlyKey(*CompanyToken);
        if (const FStepDisplayNameRow* Found = StepDisplayNameMap.Find(CompanyOnlyKey))
        {
            return Found->GetStepName(StepNumber);
        }

        UE_LOG(LogTemp, Warning, TEXT("[StepDisplay] 매핑 없음: Company=%s, Variant=%s, Step=%d, MapSize=%d - DT_StepDisplayName 확인 필요"),
            *CompanyToken, *VariantKey.ToString(), StepNumber, StepDisplayNameMap.Num());
    }
    else if (StepDisplayNameMap.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StepDisplay] StepDisplayNameMap is EMPTY - DT_StepDisplayName 재 import 필요"));
    }

    // DT가 단일 진실 소스. 매핑 없으면 빈 FText 반환 → UI가 Collapsed 처리 (loud failure 의도)
    return FText::GetEmpty();
}

// ===== Building Enhancement Definition =====

void UTableManagerSubsystem::InitializeBuildingEnhancementDefinitionTable()
{
    if (!BuildingEnhancementDefinitionDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] BuildingEnhancementDefinitionDataTable is NULL - 강화 슬롯 패널 비어 보일 수 있음"));
        return;
    }

    EnhancementDefinitionTable.Empty();

    TArray<FName> RowNames = BuildingEnhancementDefinitionDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FBuildingEnhancementDefinition* Row = BuildingEnhancementDefinitionDataTable->FindRow<FBuildingEnhancementDefinition>(
            RowName, TEXT("InitializeBuildingEnhancementDefinitionTable"));
        if (Row)
        {
            // Key는 EnhancementType으로 통일 (RowName은 식별자, 키는 enum) — 중복 enum 있으면 뒤 행이 덮어씀
            EnhancementDefinitionTable.Add(Row->EnhancementType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingEnhancementDefinition Table Initialized (%d entries)"),
        EnhancementDefinitionTable.Num());
}

bool UTableManagerSubsystem::GetEnhancementDefinition(EBuildingEnhancementType Type, FBuildingEnhancementDefinition& OutDef) const
{
    if (const FBuildingEnhancementDefinition* Found = EnhancementDefinitionTable.Find(Type))
    {
        OutDef = *Found;
        return true;
    }
    return false;
}


TArray<FBuildingEnhancementDefinition> UTableManagerSubsystem::GetEnhancementsForCompanyType(ECompanyType CompanyType) const
{
    TArray<FBuildingEnhancementDefinition> Result;

    for (const TPair<EBuildingEnhancementType, FBuildingEnhancementDefinition>& Pair : EnhancementDefinitionTable)
    {
        const FBuildingEnhancementDefinition& Def = Pair.Value;
        if (UBuildingEnhancementHelper::IsEnhancementVisibleForCompanyType(Def.Category, CompanyType))
        {
            Result.Add(Def);
        }
    }

    // SortOrder 오름차순 정렬 — DT_BuildingEnhancementDefinition 의 SortOrder 값이 단일 진실 원천.
    // 해금 레벨 순서와 맞추려면 CSV 에서 SortOrder 를 (UnlockLevel * 10 + 카테고리 순서) 형태로 지정.
    Result.Sort([](const FBuildingEnhancementDefinition& A, const FBuildingEnhancementDefinition& B)
    {
        if (A.SortOrder != B.SortOrder) return A.SortOrder < B.SortOrder;
        return static_cast<uint8>(A.EnhancementType) < static_cast<uint8>(B.EnhancementType);
    });

    return Result;
}

// ===== World Factory Upgrade Definition =====

void UTableManagerSubsystem::InitializeWorldFactoryUpgradeDefinitionTable()
{
    if (!WorldFactoryUpgradeDefinitionDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] WorldFactoryUpgradeDefinitionDataTable is NULL - 세계지도 공장 강화 슬롯 비어 보일 수 있음"));
        return;
    }

    WorldFactoryUpgradeDefinitionTable.Empty();

    TArray<FName> RowNames = WorldFactoryUpgradeDefinitionDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FWorldFactoryUpgradeDefinition* Row = WorldFactoryUpgradeDefinitionDataTable->FindRow<FWorldFactoryUpgradeDefinition>(
            RowName, TEXT("InitializeWorldFactoryUpgradeDefinitionTable"));
        if (Row)
        {
            WorldFactoryUpgradeDefinitionTable.Add(Row->UpgradeType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] WorldFactoryUpgradeDefinition Table Initialized (%d entries)"),
        WorldFactoryUpgradeDefinitionTable.Num());
}

bool UTableManagerSubsystem::GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType Type, FWorldFactoryUpgradeDefinition& OutDef) const
{
    if (const FWorldFactoryUpgradeDefinition* Found = WorldFactoryUpgradeDefinitionTable.Find(Type))
    {
        OutDef = *Found;
        return true;
    }
    return false;
}

TArray<FWorldFactoryUpgradeDefinition> UTableManagerSubsystem::GetAllWorldFactoryUpgradeDefinitions() const
{
    TArray<FWorldFactoryUpgradeDefinition> Result;
    Result.Reserve(WorldFactoryUpgradeDefinitionTable.Num());

    for (const TPair<EWorldFactoryUpgradeType, FWorldFactoryUpgradeDefinition>& Pair : WorldFactoryUpgradeDefinitionTable)
    {
        Result.Add(Pair.Value);
    }

    Result.Sort([](const FWorldFactoryUpgradeDefinition& A, const FWorldFactoryUpgradeDefinition& B)
    {
        if (A.SortOrder != B.SortOrder) return A.SortOrder < B.SortOrder;
        return static_cast<uint8>(A.UpgradeType) < static_cast<uint8>(B.UpgradeType);
    });

    return Result;
}

// ===== Mine Upgrade Definition =====

void UTableManagerSubsystem::InitializeMineUpgradeDefinitionTable()
{
    if (!MineUpgradeDefinitionDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] MineUpgradeDefinitionDataTable is NULL - 세계지도 채광 강화 슬롯 비어 보일 수 있음"));
        return;
    }

    MineUpgradeDefinitionTable.Empty();

    TArray<FName> RowNames = MineUpgradeDefinitionDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FMineUpgradeDefinition* Row = MineUpgradeDefinitionDataTable->FindRow<FMineUpgradeDefinition>(
            RowName, TEXT("InitializeMineUpgradeDefinitionTable"));
        if (Row)
        {
            MineUpgradeDefinitionTable.Add(Row->UpgradeType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] MineUpgradeDefinition Table Initialized (%d entries)"),
        MineUpgradeDefinitionTable.Num());
}

bool UTableManagerSubsystem::GetMineUpgradeDefinition(EMineUpgradeType Type, FMineUpgradeDefinition& OutDef) const
{
    if (const FMineUpgradeDefinition* Found = MineUpgradeDefinitionTable.Find(Type))
    {
        OutDef = *Found;
        return true;
    }
    return false;
}

TArray<FMineUpgradeDefinition> UTableManagerSubsystem::GetAllMineUpgradeDefinitions() const
{
    TArray<FMineUpgradeDefinition> Result;
    Result.Reserve(MineUpgradeDefinitionTable.Num());

    for (const TPair<EMineUpgradeType, FMineUpgradeDefinition>& Pair : MineUpgradeDefinitionTable)
    {
        Result.Add(Pair.Value);
    }

    Result.Sort([](const FMineUpgradeDefinition& A, const FMineUpgradeDefinition& B)
    {
        if (A.SortOrder != B.SortOrder) return A.SortOrder < B.SortOrder;
        return static_cast<uint8>(A.UpgradeType) < static_cast<uint8>(B.UpgradeType);
    });

    return Result;
}

// ===== Factory Upgrade Definition =====

void UTableManagerSubsystem::InitializeFactoryUpgradeDefinitionTable()
{
    if (!FactoryUpgradeDefinitionDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] FactoryUpgradeDefinitionDataTable is NULL - Factory 강화 슬롯 비어 보일 수 있음"));
        return;
    }

    FactoryUpgradeDefinitionTable.Empty();

    TArray<FName> RowNames = FactoryUpgradeDefinitionDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FFactoryUpgradeDefinition* Row = FactoryUpgradeDefinitionDataTable->FindRow<FFactoryUpgradeDefinition>(
            RowName, TEXT("InitializeFactoryUpgradeDefinitionTable"));
        if (Row)
        {
            FactoryUpgradeDefinitionTable.Add(Row->UpgradeType, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] FactoryUpgradeDefinition Table Initialized (%d entries)"),
        FactoryUpgradeDefinitionTable.Num());
}

bool UTableManagerSubsystem::GetFactoryUpgradeDefinition(EFactoryUpgradeType Type, FFactoryUpgradeDefinition& OutDef) const
{
    if (const FFactoryUpgradeDefinition* Found = FactoryUpgradeDefinitionTable.Find(Type))
    {
        OutDef = *Found;
        return true;
    }
    return false;
}

TArray<FFactoryUpgradeDefinition> UTableManagerSubsystem::GetAllFactoryUpgradeDefinitions() const
{
    TArray<FFactoryUpgradeDefinition> Result;
    Result.Reserve(FactoryUpgradeDefinitionTable.Num());

    for (const TPair<EFactoryUpgradeType, FFactoryUpgradeDefinition>& Pair : FactoryUpgradeDefinitionTable)
    {
        Result.Add(Pair.Value);
    }

    Result.Sort([](const FFactoryUpgradeDefinition& A, const FFactoryUpgradeDefinition& B)
    {
        if (A.SortOrder != B.SortOrder) return A.SortOrder < B.SortOrder;
        return static_cast<uint8>(A.UpgradeType) < static_cast<uint8>(B.UpgradeType);
    });

    return Result;
}

// ========== Building Trait (BUILDING_TRAIT_SYSTEM v1.1) ==========

void UTableManagerSubsystem::InitializeBuildingTraitTable()
{
    if (!BuildingTraitDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] BuildingTraitDataTable is NULL"));
        return;
    }

    BuildingTraitTable.Empty();

    TArray<FName> RowNames = BuildingTraitDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FBuildingTraitTableRow* Row = BuildingTraitDataTable->FindRow<FBuildingTraitTableRow>(
            RowName, TEXT("InitializeBuildingTraitTable"));
        if (Row)
        {
            // CSV 의 TraitID 컬럼이 비어있을 수 있으므로 RowName 으로 강제 동기화
            FBuildingTraitTableRow Copy = *Row;
            Copy.TraitID = RowName;
            BuildingTraitTable.Add(RowName, Copy);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingTrait Table Initialized (%d entries)"),
        BuildingTraitTable.Num());
}

void UTableManagerSubsystem::InitializeBuildingTraitSetBonusTable()
{
    if (!BuildingTraitSetBonusDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] BuildingTraitSetBonusDataTable is NULL"));
        return;
    }

    BuildingTraitSetBonusTableMap.Empty();

    TArray<FName> RowNames = BuildingTraitSetBonusDataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FBuildingTraitSetBonus* Row = BuildingTraitSetBonusDataTable->FindRow<FBuildingTraitSetBonus>(
            RowName, TEXT("InitializeBuildingTraitSetBonusTable"));
        if (Row)
        {
            FIntPoint Key(static_cast<int32>(Row->Category), Row->RequiredCount);
            BuildingTraitSetBonusTableMap.Add(Key, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BuildingTraitSetBonus Table Initialized (%d entries)"),
        BuildingTraitSetBonusTableMap.Num());
}

bool UTableManagerSubsystem::GetBuildingTraitData(FName TraitID, FBuildingTraitTableRow& OutRow) const
{
    if (const FBuildingTraitTableRow* Found = BuildingTraitTable.Find(TraitID))
    {
        OutRow = *Found;
        return true;
    }
    return false;
}

TArray<FBuildingTraitTableRow> UTableManagerSubsystem::GetAllBuildingTraits() const
{
    TArray<FBuildingTraitTableRow> Result;
    Result.Reserve(BuildingTraitTable.Num());
    for (const TPair<FName, FBuildingTraitTableRow>& Pair : BuildingTraitTable)
    {
        Result.Add(Pair.Value);
    }
    Result.Sort([](const FBuildingTraitTableRow& A, const FBuildingTraitTableRow& B)
    {
        return A.SortOrder < B.SortOrder;
    });
    return Result;
}

TArray<FBuildingTraitTableRow> UTableManagerSubsystem::GetBuildingTraitsByRarity(ELootBoxRarity Rarity) const
{
    TArray<FBuildingTraitTableRow> Result;
    for (const TPair<FName, FBuildingTraitTableRow>& Pair : BuildingTraitTable)
    {
        if (Pair.Value.Rarity == Rarity)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FBuildingTraitTableRow> UTableManagerSubsystem::GetBuildingTraitsByCategory(EBuildingTraitCategory Category) const
{
    TArray<FBuildingTraitTableRow> Result;
    for (const TPair<FName, FBuildingTraitTableRow>& Pair : BuildingTraitTable)
    {
        if (Pair.Value.Category == Category)
        {
            Result.Add(Pair.Value);
        }
    }
    Result.Sort([](const FBuildingTraitTableRow& A, const FBuildingTraitTableRow& B)
    {
        if (A.Rarity != B.Rarity) return static_cast<uint8>(A.Rarity) < static_cast<uint8>(B.Rarity);
        return A.SortOrder < B.SortOrder;
    });
    return Result;
}

bool UTableManagerSubsystem::GetBuildingTraitSetBonus(EBuildingTraitCategory Category, int32 RequiredCount, FBuildingTraitSetBonus& OutRow) const
{
    FIntPoint Key(static_cast<int32>(Category), RequiredCount);
    if (const FBuildingTraitSetBonus* Found = BuildingTraitSetBonusTableMap.Find(Key))
    {
        OutRow = *Found;
        return true;
    }
    return false;
}

TArray<FBuildingTraitSetBonus> UTableManagerSubsystem::GetBuildingTraitSetBonusesByCategory(EBuildingTraitCategory Category) const
{
    TArray<FBuildingTraitSetBonus> Result;
    for (const TPair<FIntPoint, FBuildingTraitSetBonus>& Pair : BuildingTraitSetBonusTableMap)
    {
        if (Pair.Value.Category == Category)
        {
            Result.Add(Pair.Value);
        }
    }
    Result.Sort([](const FBuildingTraitSetBonus& A, const FBuildingTraitSetBonus& B)
    {
        return A.RequiredCount < B.RequiredCount;
    });
    return Result;
}

// ========== Office Starter Preset Table ==========

void UTableManagerSubsystem::InitializeOfficeStarterPresetTable()
{
    if (!OfficeStarterPresetDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[TableManagerSubsystem] OfficeStarterPresetDataTable is NULL"));
        return;
    }
    OfficeStarterPresetTable.Empty();
    for (const FName& RowName : OfficeStarterPresetDataTable->GetRowNames())
    {
        if (FOfficeStarterPresetRow* Row = OfficeStarterPresetDataTable->FindRow<FOfficeStarterPresetRow>(RowName, TEXT("")))
        {
            FOfficeStarterPresetRow Cached = *Row;
            Cached.RowName = RowName;   // 런타임 자기 식별
            OfficeStarterPresetTable.Add(RowName, Cached);
        }
    }
}

FOfficeStarterPresetRow UTableManagerSubsystem::GetStarterPresetRow(FName RowName, bool& bOutSuccess) const
{
    if (const FOfficeStarterPresetRow* Found = OfficeStarterPresetTable.Find(RowName))
    {
        bOutSuccess = true;
        return *Found;
    }
    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] StarterPresetRow not found: %s"), *RowName.ToString());
    bOutSuccess = false;
    return FOfficeStarterPresetRow();
}

FOfficeStarterPresetRow UTableManagerSubsystem::GetStarterPresetForBuilding(int32 FootprintCells, int32 BuildingIndex, bool& bOutSuccess) const
{
    auto Collect = [this](int32 Cells, TArray<const FOfficeStarterPresetRow*>& Out)
    {
        for (const TPair<FName, FOfficeStarterPresetRow>& Pair : OfficeStarterPresetTable)
        {
            if (Pair.Value.FootprintCells == Cells)
            {
                Out.Add(&Pair.Value);
            }
        }
    };

    TArray<const FOfficeStarterPresetRow*> Variants;
    int32 UsedCells = FMath::Max(1, FootprintCells);
    Collect(UsedCells, Variants);
    while (Variants.Num() == 0 && UsedCells > 1)   // 정확 버킷 없으면 더 작은 버킷으로 폴백
    {
        --UsedCells;
        Collect(UsedCells, Variants);
    }
    if (Variants.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] No StarterPreset for cells %d"), FootprintCells);
        bOutSuccess = false;
        return FOfficeStarterPresetRow();
    }

    Variants.Sort([](const FOfficeStarterPresetRow& A, const FOfficeStarterPresetRow& B) { return A.Variant < B.Variant; });
    const int32 Pick = FMath::Abs(BuildingIndex) % Variants.Num();
    bOutSuccess = true;
    return *Variants[Pick];
}

// ========== Industry Mood Palette Table ==========

void UTableManagerSubsystem::InitializeIndustryMoodPaletteTable()
{
    if (!IndustryMoodPaletteDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[TableManagerSubsystem] IndustryMoodPaletteDataTable is NULL"));
        return;
    }
    IndustryMoodPaletteTable.Empty();
    for (const FName& RowName : IndustryMoodPaletteDataTable->GetRowNames())
    {
        if (FIndustryMoodPaletteRow* Row = IndustryMoodPaletteDataTable->FindRow<FIndustryMoodPaletteRow>(RowName, TEXT("")))
        {
            IndustryMoodPaletteTable.Add(RowName, *Row);
        }
    }
}

FIndustryMoodPaletteRow UTableManagerSubsystem::GetIndustryMoodPalette(ECompanyType Industry, bool& bOutSuccess) const
{
    for (const auto& Pair : IndustryMoodPaletteTable)
    {
        if (Pair.Value.Industry == Industry)
        {
            bOutSuccess = true;
            return Pair.Value;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] No IndustryMoodPalette for %s"), *CompanyTypeToString(Industry));
    bOutSuccess = false;
    return FIndustryMoodPaletteRow();
}

// ========== GDS Core Loop 데이터층 (착수 발견형) ==========
// 산업 키 토큰은 위 익명 namespace 의 CompanyTypeToRowToken("Game"/"IT"/... ) 재사용.
// CSV Industry(FString) → StringToCompanyType → ECompanyType 로 정규화하여 캐싱.

void UTableManagerSubsystem::InitializeProjectGenreTable()
{
    if (!ProjectGenreDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ProjectGenreDataTable is NULL - DT_ProjectGenre 미생성 가능. GetUnlockedGenres 는 빈 목록 반환"));
        return;
    }

    ProjectGenreRows.Empty();

    for (const FName& RowName : ProjectGenreDataTable->GetRowNames())
    {
        const FProjectGenreRow* Row = ProjectGenreDataTable->FindRow<FProjectGenreRow>(RowName, TEXT("InitializeProjectGenreTable"));
        if (!Row) continue;
        ProjectGenreRows.Add(*Row);
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] ProjectGenreRows Initialized (%d entries)"), ProjectGenreRows.Num());
}

void UTableManagerSubsystem::InitializeTierRankTitleTable()
{
    if (!TierRankTitleDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] TierRankTitleDataTable is NULL - DT_TierRankTitle 미생성 가능. GetTierRankTitle 는 빈 FText 반환"));
        return;
    }

    TierRankTitleRows.Empty();

    for (const FName& RowName : TierRankTitleDataTable->GetRowNames())
    {
        const FTierRankTitleRow* Row = TierRankTitleDataTable->FindRow<FTierRankTitleRow>(RowName, TEXT("InitializeTierRankTitleTable"));
        if (!Row) continue;
        TierRankTitleRows.Add(*Row);
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] TierRankTitleRows Initialized (%d entries)"), TierRankTitleRows.Num());
}

FText UTableManagerSubsystem::GetTierRankTitle(ECompanyType Industry, int32 Tier) const
{
    for (const FTierRankTitleRow& Row : TierRankTitleRows)
    {
        if (Row.Tier == Tier && StringToCompanyType(Row.Industry) == Industry)
        {
            return Row.Title;
        }
    }
    return FText::GetEmpty();
}

void UTableManagerSubsystem::InitializeIndustryProfileTable()
{
    if (!IndustryProfileDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] IndustryProfileDataTable is NULL - DT_IndustryProfile 미생성 가능. GetIndustryProfile 는 실패 반환"));
        return;
    }

    IndustryProfileMap.Empty();

    for (const FName& RowName : IndustryProfileDataTable->GetRowNames())
    {
        const FIndustryProfileRow* Row = IndustryProfileDataTable->FindRow<FIndustryProfileRow>(RowName, TEXT("InitializeIndustryProfileTable"));
        if (!Row) continue;

        const ECompanyType IndustryType = StringToCompanyType(Row->Industry);
        if (IndustryType == ECompanyType::None)
        {
            UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] IndustryProfile row '%s' 의 Industry '%s' 미인식 - 스킵"), *RowName.ToString(), *Row->Industry);
            continue;
        }
        IndustryProfileMap.Add(IndustryType, *Row);
    }

    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] IndustryProfileMap Initialized (%d entries)"), IndustryProfileMap.Num());
}

TArray<FName> UTableManagerSubsystem::GetUnlockedGenres(ECompanyType Industry, int32 Tier) const
{
    TArray<FName> Result;
    for (const FProjectGenreRow& Row : ProjectGenreRows)
    {
        if (Row.UnlockTier <= Tier && StringToCompanyType(Row.Industry) == Industry)
        {
            Result.AddUnique(Row.Genre);
        }
    }
    return Result;
}

bool UTableManagerSubsystem::GetGenreInfo(ECompanyType Industry, FName Genre, FProjectGenreRow& OutRow) const
{
    for (const FProjectGenreRow& Row : ProjectGenreRows)
    {
        if (Row.Genre == Genre && StringToCompanyType(Row.Industry) == Industry)
        {
            OutRow = Row;
            return true;
        }
    }
    return false;
}

FIndustryProfileRow UTableManagerSubsystem::GetIndustryProfile(ECompanyType Industry, bool& bOutSuccess) const
{
    if (const FIndustryProfileRow* Found = IndustryProfileMap.Find(Industry))
    {
        bOutSuccess = true;
        return *Found;
    }
    bOutSuccess = false;
    return FIndustryProfileRow();
}

FText UTableManagerSubsystem::PickProjectName(ECompanyType Industry, FName Genre, FName Material) const
{
    const int32 IndustryInt = static_cast<int32>(Industry);
    for (const auto& Pair : AllProjectTable)
    {
        if (Pair.Key.X != IndustryInt) continue;

        const FProjectData& Data = Pair.Value;
        if (Data.Genre == Genre && Data.Material == Material && !Data.ProjectName.IsEmpty())
        {
            return Data.ProjectName;
        }
    }
    return FText::GetEmpty();
}

FProjectData UTableManagerSubsystem::GetProjectByGenreMaterial(ECompanyType Industry, FName Genre, FName Material, bool& bOutSuccess) const
{
    const int32 IndustryInt = static_cast<int32>(Industry);
    for (const auto& Pair : AllProjectTable)
    {
        if (Pair.Key.X != IndustryInt) continue;
        const FProjectData& Data = Pair.Value;
        if (Data.Genre == Genre && Data.Material == Material)
        {
            bOutSuccess = true;
            return Data;
        }
    }
    bOutSuccess = false;
    return FProjectData();
}

TArray<FProjectGenreRow> UTableManagerSubsystem::GetGenresForIndustry(ECompanyType Industry) const
{
    TArray<FProjectGenreRow> Result;
    for (const FProjectGenreRow& Row : ProjectGenreRows)
    {
        if (StringToCompanyType(Row.Industry) == Industry)
        {
            Result.Add(Row);
        }
    }
    Result.Sort([](const FProjectGenreRow& A, const FProjectGenreRow& B)
    {
        if (A.UnlockTier != B.UnlockTier) { return A.UnlockTier < B.UnlockTier; }
        return A.Complexity < B.Complexity;
    });
    return Result;
}

TArray<FName> UTableManagerSubsystem::GetMaterialsForIndustry(ECompanyType Industry) const
{
    // 소재 전용 테이블이 없다 — 프로젝트 행이 소재의 단일 출처. 소재 미태깅 산업은 빈 목록(= 트렌드 없음).
    const int32 IndustryInt = static_cast<int32>(Industry);
    TArray<FName> Result;
    for (const auto& Pair : AllProjectTable)
    {
        if (Pair.Key.X != IndustryInt) { continue; }
        if (Pair.Value.Material.IsNone()) { continue; }
        Result.AddUnique(Pair.Value.Material);
    }
    return Result;
}

void UTableManagerSubsystem::InitializeCriticDisplayTable()
{
    if (!CriticDisplayDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] CriticDisplayDataTable is NULL - DT_CriticDisplay 미생성 가능. GetCriticNames 는 빈 배열 반환"));
        return;
    }

    CriticDisplayMap.Empty();
    for (const FName& RowName : CriticDisplayDataTable->GetRowNames())
    {
        const FCriticDisplayRow* Row = CriticDisplayDataTable->FindRow<FCriticDisplayRow>(RowName, TEXT("InitializeCriticDisplayTable"));
        if (!Row) continue;
        const ECompanyType IndustryType = StringToCompanyType(Row->Industry);
        if (IndustryType == ECompanyType::None) continue;
        CriticDisplayMap.Add(IndustryType, *Row);
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] CriticDisplayMap Initialized (%d entries)"), CriticDisplayMap.Num());
}

TArray<FText> UTableManagerSubsystem::GetCriticNames(ECompanyType Industry) const
{
    TArray<FText> Result;
    if (const FCriticDisplayRow* Found = CriticDisplayMap.Find(Industry))
    {
        Result.Add(Found->Critic1);
        Result.Add(Found->Critic2);
        Result.Add(Found->Critic3);
        Result.Add(Found->Critic4);
    }
    return Result;
}

TArray<TArray<EProductionDiscipline>> UTableManagerSubsystem::GetCriticFocus(ECompanyType Industry) const
{
    TArray<TArray<EProductionDiscipline>> Result;
    const FCriticDisplayRow* Found = CriticDisplayMap.Find(Industry);
    if (!Found) { return Result; }

    const FString* FocusStrs[4] = { &Found->Critic1Focus, &Found->Critic2Focus, &Found->Critic3Focus, &Found->Critic4Focus };
    const UEnum* Enum = StaticEnum<EProductionDiscipline>();
    for (const FString* FocusStr : FocusStrs)
    {
        TArray<EProductionDiscipline> Focus;
        TArray<FString> Parts;
        FocusStr->ParseIntoArray(Parts, TEXT(","));
        for (FString& Part : Parts)
        {
            Part.TrimStartAndEndInline();
            const int64 Value = Enum->GetValueByNameString(Part);
            if (Value != INDEX_NONE && Value >= 0 && Value < static_cast<int64>(EProductionDiscipline::Count))
            {
                Focus.Add(static_cast<EProductionDiscipline>(Value));
            }
            else if (!Part.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GetCriticFocus: 직능 식별자 오타 '%s' — 무시"), *Part);
            }
        }
        Result.Add(MoveTemp(Focus));
    }
    return Result;
}

void UTableManagerSubsystem::InitializeReviewCommentTable()
{
    if (!ReviewCommentDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] ReviewCommentDataTable is NULL - DT_ReviewComment 미생성 가능. GetReviewComments 는 빈 결과 반환"));
        return;
    }

    ReviewCommentRows.Empty();
    for (const FName& RowName : ReviewCommentDataTable->GetRowNames())
    {
        const FReviewCommentRow* Row = ReviewCommentDataTable->FindRow<FReviewCommentRow>(RowName, TEXT("InitializeReviewCommentTable"));
        if (!Row) continue;
        ReviewCommentRows.Add(*Row);
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] ReviewCommentRows Initialized (%d entries)"), ReviewCommentRows.Num());
}

FString UTableManagerSubsystem::ApplyReviewTokens(const FString& In, const FReviewTokenValues& Tokens)
{
    FString Out = In;
    Out.ReplaceInline(TEXT("{프로젝트}"), *Tokens.ProjectName);
    Out.ReplaceInline(TEXT("{장르}"), *Tokens.GenreText);
    Out.ReplaceInline(TEXT("{최고분야}"), *Tokens.BestStepName);
    Out.ReplaceInline(TEXT("{최저분야}"), *Tokens.WorstStepName);
    Out.ReplaceInline(TEXT("{산업}"), *Tokens.IndustryText);
    return Out;
}

void UTableManagerSubsystem::GetReviewComments(FName CommentSlot, ECompanyType Industry, FName Band,
    const TSet<FName>& ActiveContexts, int32 Count, const FReviewTokenValues& Tokens,
    TArray<FText>& OutNicks, TArray<FText>& OutComments) const
{
    OutNicks.Empty();
    OutComments.Empty();

    // 필터: Slot+Band 일치, Context 는 None 이거나 활성 집합에 포함
    const FString IndustryToken = CompanyTypeToRowToken(Industry);
    TArray<const FReviewCommentRow*> IndustryPool;
    TArray<const FReviewCommentRow*> CommonPool;
    for (const FReviewCommentRow& Row : ReviewCommentRows)
    {
        if (Row.Slot != CommentSlot || Row.Band != Band) { continue; }
        const bool bContextOk = Row.Context.IsNone() || Row.Context == FName(TEXT("None"))
            || ActiveContexts.Contains(Row.Context);
        if (!bContextOk) { continue; }
        if (Row.Industry == IndustryToken) { IndustryPool.Add(&Row); }
        else if (Row.Industry.IsEmpty()) { CommonPool.Add(&Row); }
    }

    auto ShufflePool = [](TArray<const FReviewCommentRow*>& Pool)
    {
        for (int32 Index = Pool.Num() - 1; Index > 0; --Index)
        {
            Pool.Swap(Index, FMath::RandRange(0, Index));
        }
    };
    ShufflePool(IndustryPool);
    ShufflePool(CommonPool);

    // 산업 전용 1행 보장(있으면) — 전용 행을 전부 앞세우면 매번 같은 행이 반복되므로 1행만 확정하고
    // 나머지는 공용+잔여 산업 행을 섞어 뽑는다. 마지막에 전체를 다시 섞어 산업 행 위치도 랜덤화.
    TArray<const FReviewCommentRow*> Picked;
    if (IndustryPool.Num() > 0 && Count > 0)
    {
        Picked.Add(IndustryPool[0]);
        IndustryPool.RemoveAt(0);
    }
    TArray<const FReviewCommentRow*> Rest = MoveTemp(CommonPool);
    Rest.Append(IndustryPool);
    ShufflePool(Rest);
    for (const FReviewCommentRow* Row : Rest)
    {
        if (Picked.Num() >= Count) { break; }
        Picked.Add(Row);
    }
    ShufflePool(Picked);

    if (Picked.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] GetReviewComments: 풀 비어있음 (Slot=%s Band=%s Industry=%s)"),
            *CommentSlot.ToString(), *Band.ToString(), *IndustryToken);
        return;
    }

    for (const FReviewCommentRow* Row : Picked)
    {
        OutNicks.Add(Row->Nick);
        OutComments.Add(FText::FromString(ApplyReviewTokens(Row->Comment.ToString(), Tokens)));
    }
}

void UTableManagerSubsystem::InitializeBoostGambleTable()
{
    if (!BoostGambleDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] BoostGambleDataTable is NULL - DT_BoostGamble 미생성 가능. GetRandomBoostGamble 는 false 반환(트리거 스킵)"));
        return;
    }

    BoostGambleRows.Empty();
    for (const FName& RowName : BoostGambleDataTable->GetRowNames())
    {
        const FBoostGambleRow* Row = BoostGambleDataTable->FindRow<FBoostGambleRow>(RowName, TEXT("InitializeBoostGambleTable"));
        if (!Row) continue;
        BoostGambleRows.Add(*Row);
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] BoostGambleRows Initialized (%d entries)"), BoostGambleRows.Num());
}

bool UTableManagerSubsystem::GetRandomBoostGamble(ECompanyType Industry, FBoostGambleRow& OutRow) const
{
    const FString IndustryToken = CompanyTypeToRowToken(Industry);
    TArray<const FBoostGambleRow*> Pool;
    for (const FBoostGambleRow& Row : BoostGambleRows)
    {
        if (Row.Industry == IndustryToken)
        {
            Pool.Add(&Row);
        }
    }
    if (Pool.Num() == 0)
    {
        return false;
    }
    OutRow = *Pool[FMath::RandRange(0, Pool.Num() - 1)];
    return true;
}

void UTableManagerSubsystem::InitializeLaunchLootBandTable()
{
    LaunchLootBandMap.Empty();
    if (!LaunchLootBandDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] LaunchLootBandDataTable is NULL - 판정 라벨은 빈 값으로 표시됨"));
        return;
    }
    for (const FName& RowName : LaunchLootBandDataTable->GetRowNames())
    {
        if (FLaunchLootBandRow* Row = LaunchLootBandDataTable->FindRow<FLaunchLootBandRow>(RowName, TEXT("InitializeLaunchLootBandTable")))
        {
            LaunchLootBandMap.Add(RowName, *Row);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[TableManagerSubsystem] LaunchLootBand Table Initialized (%d entries)"), LaunchLootBandMap.Num());
}

bool UTableManagerSubsystem::GetLaunchLootBand(FName Key, FLaunchLootBandRow& OutRow) const
{
    if (const FLaunchLootBandRow* Found = LaunchLootBandMap.Find(Key))
    {
        OutRow = *Found;
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("[TableManagerSubsystem] LaunchLootBand row missing: %s"), *Key.ToString());
    return false;
}
