#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Table/ShopItemTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCGRGachaBoxItemTableTest,
    "CGR.Gacha.Box.ShopItems",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaBoxItemTableTest::RunTest(const FString& Parameters)
{
    const UDataTable* ShopTable = LoadObject<UDataTable>(nullptr,
        TEXT("/Game/CompanyGrowth/Table/Shop/DT_ShopItem.DT_ShopItem"));
    if (!TestNotNull(TEXT("실제 DT_ShopItem 로드"), ShopTable))
    {
        return false;
    }

    struct FBoxCase
    {
        EItemType Item;
        const TCHAR* Name;
        const TCHAR* AssetSuffix;
        int32 ExpectedSkuCount;
    };
    const FBoxCase Boxes[] = {
        { EItemType::EmployeeEquipmentTicketNormal, TEXT("일반 장비 상자"), TEXT("Equipment_Normal"), 3 },
        { EItemType::EmployeeEquipmentTicketAdvanced, TEXT("고급 장비 상자"), TEXT("Equipment_Advanced"), 2 },
        { EItemType::EmployeeEquipmentTicketPremium, TEXT("최고급 장비 상자"), TEXT("Equipment_Premium"), 2 },
        { EItemType::BuildingTraitTicketNormal, TEXT("일반 특성 상자"), TEXT("Trait_Normal"), 2 },
        { EItemType::BuildingTraitTicketAdvanced, TEXT("고급 특성 상자"), TEXT("Trait_Advanced"), 1 },
        { EItemType::SkinTicketNormal, TEXT("일반 스킨 상자"), TEXT("Skin_Normal"), 2 },
        { EItemType::SkinTicketAdvanced, TEXT("고급 스킨 상자"), TEXT("Skin_Advanced"), 2 },
    };

    int32 TotalBoxSkus = 0;
    for (const FBoxCase& Box : Boxes)
    {
        const FString Label = UEnum::GetValueAsString(Box.Item);
        const FString ExpectedPackage = FString::Printf(
            TEXT("/Game/CompanyGrowth/UI/Textures/GachaBox/Box/%s/T_ItemIcon_Box_%s"),
            Box.AssetSuffix, Box.AssetSuffix);
        TestEqual(*(Label + TEXT(" 표시명")), GetItemTypeDisplayName(Box.Item).ToString(), FString(Box.Name));
        TestTrue(*(Label + TEXT(" 아이콘 에셋 존재")), FPackageName::DoesPackageExist(ExpectedPackage));

        int32 Seen = 0;
        for (const auto& Entry : ShopTable->GetRowMap())
        {
            const FShopItemTable* Row = reinterpret_cast<const FShopItemTable*>(Entry.Value);
            if (Row->Item != Box.Item)
            {
                continue;
            }
            ++Seen;
            const FString ExpectedName = Entry.Key == FName(TEXT("Weekly_EquipmentNormal_14"))
                ? FString(TEXT("무료 ")) + Box.Name : FString(Box.Name);
            const FString RowLabel = Label + TEXT("/") + Entry.Key.ToString();
            TestEqual(*(RowLabel + TEXT(" 이름")), Row->DisplayName.ToString(), ExpectedName);
            TestTrue(*(RowLabel + TEXT(" IconMaterial 비움")), Row->IconMaterial.IsNull());
            TestEqual(*(RowLabel + TEXT(" Box Icon")),
                Row->Icon.ToSoftObjectPath().GetLongPackageName(), ExpectedPackage);
            TestTrue(*(RowLabel + TEXT(" 텍스처 로드")), Cast<UTexture2D>(Row->ResolveIconResource()) != nullptr);
        }
        TestEqual(*(Label + TEXT(" 상품 수")), Seen, Box.ExpectedSkuCount);
        TotalBoxSkus += Seen;
    }
    TestEqual(TEXT("상자 상품 14행"), TotalBoxSkus, 14);

    struct FRecruitCase
    {
        const TCHAR* RowName;
        EItemType Item;
        const TCHAR* DisplayName;
        const TCHAR* IconName;
    };
    const FRecruitCase Recruitment[] = {
        { TEXT("Daily_RecruitNormal"), EItemType::RecruitTicketNormal, TEXT("일반 채용권"), TEXT("RecruitTicketNormal") },
        { TEXT("Weekly_RecruitAdv"), EItemType::RecruitTicketAdvanced, TEXT("고급 채용권"), TEXT("RecruitTicketAdvanced") },
        { TEXT("Dia_RecruitAdv"), EItemType::RecruitTicketAdvanced, TEXT("고급 채용권"), TEXT("RecruitTicketAdvanced") },
        { TEXT("Dia_RecruitPrem"), EItemType::RecruitTicketPremium, TEXT("프리미엄 채용권"), TEXT("RecruitTicketPremium") },
        { TEXT("Mile_RecruitNormal"), EItemType::RecruitTicketNormal, TEXT("일반 채용권"), TEXT("RecruitTicketNormal") },
    };
    for (const FRecruitCase& Ticket : Recruitment)
    {
        const FName RowName(Ticket.RowName);
        const FShopItemTable* Row = ShopTable->FindRow<FShopItemTable>(RowName, TEXT("GachaBoxItemTest"));
        if (!TestNotNull(*(RowName.ToString() + TEXT(" 채용권 행")), Row))
        {
            continue;
        }
        TestTrue(*(RowName.ToString() + TEXT(" 아이템 타입")), Row->Item == Ticket.Item);
        TestEqual(*(RowName.ToString() + TEXT(" 이름")), Row->DisplayName.ToString(), FString(Ticket.DisplayName));
        TestEqual(*(RowName.ToString() + TEXT(" 아이콘 경로")),
            Row->Icon.ToSoftObjectPath().GetLongPackageName(),
            FString::Printf(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/%s"), Ticket.IconName));
        TestTrue(*(RowName.ToString() + TEXT(" IconMaterial 비움")), Row->IconMaterial.IsNull());
    }
    int32 RecruitSkuCount = 0;
    for (const auto& Entry : ShopTable->GetRowMap())
    {
        const FShopItemTable* Row = reinterpret_cast<const FShopItemTable*>(Entry.Value);
        if (Row->Item == EItemType::RecruitTicketNormal
            || Row->Item == EItemType::RecruitTicketAdvanced
            || Row->Item == EItemType::RecruitTicketPremium)
        {
            ++RecruitSkuCount;
        }
    }
    TestEqual(TEXT("채용권 상품 5행 유지"), RecruitSkuCount, 5);
    TestEqual(TEXT("일반 채용권 표시명 유지"), GetItemTypeDisplayName(EItemType::RecruitTicketNormal).ToString(), FString(TEXT("일반 채용권")));
    TestEqual(TEXT("고급 채용권 표시명 유지"), GetItemTypeDisplayName(EItemType::RecruitTicketAdvanced).ToString(), FString(TEXT("고급 채용권")));
    TestEqual(TEXT("프리미엄 채용권 표시명 유지"), GetItemTypeDisplayName(EItemType::RecruitTicketPremium).ToString(), FString(TEXT("프리미엄 채용권")));

    return true;
}

#endif
