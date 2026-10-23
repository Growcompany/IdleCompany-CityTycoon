#include "Misc/AutomationTest.h"
#include "Utils/EmployeeEquipmentAppearanceResolver.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGREquipmentAppearancePrecedenceTest,
    "CGR.Employee.Equipment.AppearancePrecedence",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREquipmentAppearancePrecedenceTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentAppearanceRow Hat;
    Hat.AppearanceId = TEXT("OwnedHat");
    Hat.EquipmentSlot = EEmployeeEquipmentSlot::Head;
    Hat.bHideHair = true;
    Hat.VisualGrade = EEmployeeEquipmentGrade::Legendary;
    FEquipmentPartOverride HatPart;
    HatPart.PartSlot = EFunnyPartSlot::Headwear;
    HatPart.PartRowId = TEXT("HatA");
    Hat.PartOverrides.Add(HatPart);
    FEmployeeEquipmentAppearanceRow CosmeticHat = Hat;
    CosmeticHat.AppearanceId = TEXT("CosmeticHat");
    CosmeticHat.VisualGrade = EEmployeeEquipmentGrade::Common;
    CosmeticHat.PartOverrides[0].PartRowId = TEXT("HatB");

    FEquipmentAppearanceInput Input;
    Input.Equipped = &Hat;
    FResolvedEquipmentAppearance Resolved;
    FString Error;
    TestTrue(TEXT("장비 외형 해석 성공"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestEqual(TEXT("장비 모자 적용"), Resolved.Parts[0].PartRowId, FName(TEXT("HatA")));
    TestTrue(TEXT("모자 헤어 숨김"), Resolved.HiddenParts.Contains(EFunnyPartSlot::Hair));
    TestEqual(TEXT("장착 외형 등급"), Resolved.VisualGrades.FindChecked(EFunnyPartSlot::Headwear), EEmployeeEquipmentGrade::Legendary);

    Input.Mode = EEquipmentAppearanceMode::Override;
    Input.Override = &CosmeticHat;
    Input.bOverrideUnlocked = true;
    TestTrue(TEXT("보유 외형 덮어쓰기"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestEqual(TEXT("장비 대신 선택 외형"), Resolved.Parts[0].PartRowId, FName(TEXT("HatB")));
    TestEqual(TEXT("일반 외형은 전설 장비 효과를 상속하지 않음"), Resolved.VisualGrades.FindChecked(EFunnyPartSlot::Headwear), EEmployeeEquipmentGrade::Common);
    CosmeticHat.VisualGrade = EEmployeeEquipmentGrade::Ascendant;
    TestTrue(TEXT("초월 외형 해석"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestEqual(TEXT("초월 표시 외형이 효과 권위"), Resolved.VisualGrades.FindChecked(EFunnyPartSlot::Headwear), EEmployeeEquipmentGrade::Ascendant);

    Input.Mode = EEquipmentAppearanceMode::Hidden;
    TestTrue(TEXT("모자 숨김"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestEqual(TEXT("장비 외형 없음"), Resolved.Parts.Num(), 0);
    TestTrue(TEXT("숨기면 모든 등급 효과 제거"), Resolved.VisualGrades.IsEmpty());
    TestTrue(TEXT("모자 컴포넌트 숨김"), Resolved.HiddenParts.Contains(EFunnyPartSlot::Headwear));
    TestFalse(TEXT("기본 헤어 복원"), Resolved.HiddenParts.Contains(EFunnyPartSlot::Hair));

    Input.EquipmentSlot = EEmployeeEquipmentSlot::Eyewear;
    TestTrue(TEXT("안경 숨김"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestTrue(TEXT("자동 직급 안경까지 숨김"), Resolved.HiddenParts.Contains(EFunnyPartSlot::Glasses));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGREquipmentAppearanceRejectTest,
    "CGR.Employee.Equipment.AppearanceRejectsInvalidComposition",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREquipmentAppearanceRejectTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentAppearanceRow Outfit;
    Outfit.EquipmentSlot = EEmployeeEquipmentSlot::Outfit;
    FEquipmentPartOverride Part;
    Part.PartSlot = EFunnyPartSlot::Body;
    Part.PartRowId = TEXT("BodyOverwrite");
    Outfit.PartOverrides.Add(Part);
    FEquipmentAppearanceInput Input;
    Input.EquipmentSlot = EEmployeeEquipmentSlot::Outfit;
    Input.Equipped = &Outfit;
    FResolvedEquipmentAppearance Resolved;
    FString Error;
    TestFalse(TEXT("의상으로 직원 피부 변경 거부"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestEqual(TEXT("불완전 결과 미발행"), Resolved.Parts.Num(), 0);
    Outfit.PartOverrides[0].PartSlot = EFunnyPartSlot::Outerwear;
    Outfit.VisualGrade = EEmployeeEquipmentGrade::Count;
    TestFalse(TEXT("잘못된 외형 등급 거부"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestTrue(TEXT("잘못된 등급의 효과 미발행"), Resolved.VisualGrades.IsEmpty());
    Outfit.VisualGrade = EEmployeeEquipmentGrade::Common;

    Input.Mode = EEquipmentAppearanceMode::Hidden;
    TestFalse(TEXT("의상 숨김 거부"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    Input.Mode = EEquipmentAppearanceMode::Override;
    Input.Override = &Outfit;
    TestFalse(TEXT("미해금 외형 거부"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));

    Input.Mode = EEquipmentAppearanceMode::UseEquipment;
    Input.EquipmentSlot = EEmployeeEquipmentSlot::Head;
    TestFalse(TEXT("다른 부위 외형 거부"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    Input.Equipped = nullptr;
    TestTrue(TEXT("빈 슬롯 기본 외형"), FEmployeeEquipmentAppearanceResolver::Resolve({Input}, Resolved, Error));
    TestEqual(TEXT("빈 슬롯 변경 없음"), Resolved.Parts.Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGREquipmentAppearanceKeyTest,
    "CGR.Employee.Equipment.AppearanceKeyIgnoresUnrelatedTransactions",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREquipmentAppearanceKeyTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentState State;
    FEmployeeEquipmentDefinitionRow Definition;
    Definition.DefinitionId = TEXT("Hat");
    Definition.AppearanceId = TEXT("HatAppearance");
    TMap<FName, FEmployeeEquipmentDefinitionRow> Definitions;
    Definitions.Add(Definition.DefinitionId, Definition);
    TMap<FName, FEmployeeEquipmentAppearanceRow> Appearances;
    FEmployeeEquipmentAppearanceRow& Appearance = Appearances.Add(Definition.AppearanceId);
    Appearance.AppearanceId = Definition.AppearanceId;
    Appearance.VisualGrade = EEmployeeEquipmentGrade::Common;
    FEmployeeEquipmentInstance Instance;
    Instance.InstanceId = FGuid::NewGuid();
    Instance.DefinitionId = Definition.DefinitionId;
    State.Instances.Add(Instance.InstanceId, Instance);
    const FString Empty = FEmployeeEquipmentAppearanceResolver::BuildKey(State, Definitions, Appearances, 7);
    State.Loadouts.FindOrAdd(7).Slots.Add(EEmployeeEquipmentSlot::Head, Instance.InstanceId);
    const FString Equipped = FEmployeeEquipmentAppearanceResolver::BuildKey(State, Definitions, Appearances, 7);
    TestNotEqual(TEXT("장착하면 촬영 키 변경"), Empty, Equipped);
    State.Revision += 5;
    State.bStarterClaimed = true;
    State.Instances[Instance.InstanceId].EnhanceLevel = 5;
    TestEqual(TEXT("강화/최초 보상 상태 변화는 재촬영 없음"), FEmployeeEquipmentAppearanceResolver::BuildKey(State, Definitions, Appearances, 7), Equipped);
    Appearance.VisualGrade = EEmployeeEquipmentGrade::Legendary;
    TestNotEqual(TEXT("보이는 외형 등급 변경은 PNG 무효화"), FEmployeeEquipmentAppearanceResolver::BuildKey(State, Definitions, Appearances, 7), Equipped);
    FEquipmentAppearanceSelection& Selection = State.AppearanceSelections.FindOrAdd(7).Slots.FindOrAdd(EEmployeeEquipmentSlot::Head);
    Selection.Mode = EEquipmentAppearanceMode::Hidden;
    const FString Hidden = FEmployeeEquipmentAppearanceResolver::BuildKey(State, Definitions, Appearances, 7);
    Appearance.VisualGrade = EEmployeeEquipmentGrade::Ascendant;
    TestEqual(TEXT("숨긴 외형 등급 변경은 재촬영 없음"), FEmployeeEquipmentAppearanceResolver::BuildKey(State, Definitions, Appearances, 7), Hidden);
    State.Loadouts.FindOrAdd(7).Slots.Remove(EEmployeeEquipmentSlot::Head);
    TestEqual(TEXT("숨긴 부위 장비를 해제해도 모습 동일"), FEmployeeEquipmentAppearanceResolver::BuildKey(State, Definitions, Appearances, 7), Hidden);
    TestNotEqual(TEXT("숨김은 기본 헤어와 구분"), Hidden, Empty);
    return true;
}
#endif
