#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/AmbientAircraftBonusRules.h"
#include "Data/GameSaveData.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Components/InteractableDragPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftBonusCooldownTest, "CGR.Aircraft.ClickBonus.CooldownAndIndependentSources",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAircraftBonusCooldownTest::RunTest(const FString& Parameters)
{
    TMap<FName, int64> Receipts;
    const FName FirstAircraft(TEXT("Aircraft_01"));
    const FName SecondAircraft(TEXT("Aircraft_02"));
    constexpr int64 FirstClaimUnix = 1791280000;

    TestEqual(TEXT("first aircraft pays a small growth-scaled bonus"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, FirstAircraft, FirstClaimUnix, 200.0, 1000, 1000), int64(500));
    TestEqual(TEXT("receipt is committed before a reentrant claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, FirstAircraft, FirstClaimUnix, 200.0, 1500, 1500), int64(0));
    TestEqual(TEXT("another aircraft has its own cooldown"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, SecondAircraft, FirstClaimUnix, 200.0, 1500, 1500), int64(500));
    TestEqual(TEXT("59 seconds is still cooling down"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, FirstAircraft, FirstClaimUnix + 59, 200.0, 2000, 2000), int64(0));
    TestEqual(TEXT("exactly 60 seconds allows the next claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, FirstAircraft, FirstClaimUnix + 60, 200.0, 2000, 2000), int64(500));
    TestEqual(TEXT("clock rollback does not grant or overwrite a receipt"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, FirstAircraft, FirstClaimUnix + 10, 200.0, 2500, 2500), int64(0));
    TestEqual(TEXT("last successful claim survives rejected requests"), Receipts.FindChecked(FirstAircraft), FirstClaimUnix + 60);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftBonusInvalidClaimTest, "CGR.Aircraft.ClickBonus.InvalidClaimsDoNotConsumeCooldown",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAircraftBonusInvalidClaimTest::RunTest(const FString& Parameters)
{
    TMap<FName, int64> Receipts;
    const FName AircraftKey(TEXT("Aircraft_01"));
    constexpr int64 ClaimUnix = 1791280000;

    TestEqual(TEXT("invalid clock cannot claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, AircraftKey, 0, 200.0, 0, 0), int64(0));
    TestEqual(TEXT("missing actor identity cannot claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, NAME_None, ClaimUnix, 200.0, 0, 0), int64(0));
    TestEqual(TEXT("missing growth rate cannot claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, AircraftKey, ClaimUnix, 0.0, 0, 0), int64(0));
    TestEqual(TEXT("rounding to zero cannot consume a minute"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, AircraftKey, ClaimUnix, 0.1, 0, 0), int64(0));
    TestEqual(TEXT("money overflow cannot claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, AircraftKey, ClaimUnix, 200.0, MAX_int64 - 499, 0), int64(0));
    TestEqual(TEXT("revenue overflow cannot claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, AircraftKey, ClaimUnix, 200.0, 0, MAX_int64 - 499), int64(0));
    TestEqual(TEXT("unrepresentable reward cannot claim"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, AircraftKey, ClaimUnix, static_cast<double>(MAX_int64), 0, 0), int64(0));
    TestEqual(TEXT("rejected claims leave no receipt"), Receipts.Num(), 0);
    TestEqual(TEXT("valid retry immediately succeeds"),
        FAmbientAircraftBonusRules::TryClaim(Receipts, AircraftKey, ClaimUnix, 200.0, 0, 0), int64(500));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftBonusPersistenceTest, "CGR.Aircraft.ClickBonus.SaveReloadKeepsCooldown",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAircraftBonusPersistenceTest::RunTest(const FString& Parameters)
{
    USaveGame_GameData* SaveObject = NewObject<USaveGame_GameData>();
    const FName AircraftKey(TEXT("AmbientAircraftFlyby_UAID_ABCDEF0123456789"));
    constexpr int64 ClaimUnix = 1791280000;
    FAmbientAircraftBonusRules::TryClaim(SaveObject->GameData.AmbientAircraftBonusLastClaimUnix,
        AircraftKey, ClaimUnix, 200.0, 0, 0);

    TArray<uint8> SaveBytes;
    if (!TestTrue(TEXT("serialize to memory without touching a player save"), UGameplayStatics::SaveGameToMemory(SaveObject, SaveBytes)))
    {
        return false;
    }
    USaveGame_GameData* ReloadedSave = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
    if (!TestNotNull(TEXT("save reload succeeds"), ReloadedSave))
    {
        return false;
    }
    TestEqual(TEXT("actor identity and receipt survive serialization"),
        ReloadedSave->GameData.AmbientAircraftBonusLastClaimUnix.FindRef(AircraftKey), ClaimUnix);
    TestEqual(TEXT("reload cannot bypass the cooldown"),
        FAmbientAircraftBonusRules::TryClaim(ReloadedSave->GameData.AmbientAircraftBonusLastClaimUnix,
            AircraftKey, ClaimUnix + 59, 200.0, 500, 500), int64(0));
    TestEqual(TEXT("reloaded receipt expires at the same boundary"),
        FAmbientAircraftBonusRules::TryClaim(ReloadedSave->GameData.AmbientAircraftBonusLastClaimUnix,
            AircraftKey, ClaimUnix + 60, 200.0, 500, 500), int64(500));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftBonusDragTest, "CGR.Aircraft.ClickBonus.FastReleaseDoesNotRewardCameraDrag",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAircraftBonusDragTest::RunTest(const FString& Parameters)
{
    const FVector2D PressPosition(300.0, 200.0);
    TestTrue(TEXT("fast swipe cancels even before the hold trigger sets dragging"),
        CGRInteractableDragPolicy::ShouldCancelAircraftTap(false, true, PressPosition, FVector2D(340.0, 200.0)));
    TestFalse(TEXT("tap jitter still allows collection"),
        CGRInteractableDragPolicy::ShouldCancelAircraftTap(false, true, PressPosition, FVector2D(305.0, 205.0)));
    TestTrue(TEXT("a drag returning to the press location stays cancelled"),
        CGRInteractableDragPolicy::ShouldCancelAircraftTap(true, true, PressPosition, PressPosition));
    TestTrue(TEXT("missing release coordinates cannot grant money"),
        CGRInteractableDragPolicy::ShouldCancelAircraftTap(false, false, PressPosition, PressPosition));
    return true;
}

#endif
