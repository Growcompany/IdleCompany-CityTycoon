#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Misc/PackageName.h"
#include "Table/GachaBoxVisualTable.h"
#include "Tests/GachaRevealTestTables.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CGRGachaBoxVisualTests
{
	// 경로만 적힌 행이 아니라 실제로 임포트된 텍스처를 가리키는지 본다 ― CSV 오타가 여기서 잡힌다
	static void CollectMissing(const TSoftObjectPtr<UTexture2D>& Texture, TArray<FString>& OutMissing)
	{
		if (Texture.IsNull() || !FPackageName::DoesPackageExist(Texture.ToSoftObjectPath().GetLongPackageName()))
		{
			OutMissing.Add(Texture.IsNull() ? FString(TEXT("(비어 있음)")) : Texture.ToString());
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaBoxVisualTableTest,
	"CGR.Gacha.Box.VisualTable",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaBoxVisualTableTest::RunTest(const FString& Parameters)
{
	UTableManagerSubsystem* Tables = NewObject<UTableManagerSubsystem>(NewObject<UGameInstance>());
	FGachaBoxVisualTableTestAccessor::Initialize(*Tables, nullptr);
	if (!TestNotNull(TEXT("DT_GachaBoxVisual 을 생성자에서 불러옴"), FGachaBoxVisualTableTestAccessor::LoadedTable(*Tables)))
	{
		return false;
	}

	const EGachaBoxType Baked[] = {
		EGachaBoxType::EquipmentNormal, EGachaBoxType::EquipmentAdvanced, EGachaBoxType::EquipmentPremium,
		EGachaBoxType::TraitNormal, EGachaBoxType::TraitAdvanced,
		EGachaBoxType::SkinNormal, EGachaBoxType::SkinAdvanced };
	for (const EGachaBoxType BoxType : Baked)
	{
		const FString Label = UEnum::GetValueAsString(BoxType);
		const FGachaBoxVisualRow* Row = Tables->GetGachaBoxVisual(BoxType);
		if (!TestNotNull(*(Label + TEXT(" 행")), Row))
		{
			continue;
		}
		TestTrue(*(Label + TEXT(" BoxType 일치")), Row->BoxType == BoxType);
		TestEqual(*(Label + TEXT(" FlapFront 11장")), Row->FlapFront.Num(), 11);
		TestEqual(*(Label + TEXT(" FlapBack 11장")), Row->FlapBack.Num(), 11);
		TestTrue(*(Label + TEXT(" SeamL.X < SeamR.X")), Row->SeamL.X < Row->SeamR.X);
		TestTrue(*(Label + TEXT(" LayerSize 양수")), Row->LayerSize.X > 0.0 && Row->LayerSize.Y > 0.0);

		TArray<FString> Missing;
		CGRGachaBoxVisualTests::CollectMissing(Row->Body, Missing);
		CGRGachaBoxVisualTests::CollectMissing(Row->Glow, Missing);
		CGRGachaBoxVisualTests::CollectMissing(Row->Icon, Missing);
		CGRGachaBoxVisualTests::CollectMissing(Row->Closed, Missing);
		for (const TSoftObjectPtr<UTexture2D>& Frame : Row->FlapFront)
		{
			CGRGachaBoxVisualTests::CollectMissing(Frame, Missing);
		}
		for (const TSoftObjectPtr<UTexture2D>& Frame : Row->FlapBack)
		{
			CGRGachaBoxVisualTests::CollectMissing(Frame, Missing);
		}
		TestTrue(*FString::Printf(TEXT("%s 텍스처 경로가 전부 실제 에셋 (없음: %s)"), *Label, *FString::Join(Missing, TEXT(", "))),
			Missing.Num() == 0);
	}

	struct FTicketCase { EItemType Ticket; EGachaBoxType Expected; };
	const FTicketCase Tickets[] = {
		{ EItemType::EmployeeEquipmentTicketNormal,   EGachaBoxType::EquipmentNormal },
		{ EItemType::EmployeeEquipmentTicketAdvanced, EGachaBoxType::EquipmentAdvanced },
		{ EItemType::EmployeeEquipmentTicketPremium,  EGachaBoxType::EquipmentPremium },
		{ EItemType::BuildingTraitTicketNormal,       EGachaBoxType::TraitNormal },
		{ EItemType::BuildingTraitTicketAdvanced,     EGachaBoxType::TraitAdvanced },
		{ EItemType::BuildingTraitTicketMythic,       EGachaBoxType::TraitAdvanced },
		{ EItemType::SkinTicketNormal,                EGachaBoxType::SkinNormal },
		{ EItemType::SkinTicketAdvanced,              EGachaBoxType::SkinAdvanced } };
	for (const FTicketCase& Case : Tickets)
	{
		TestTrue(*(UEnum::GetValueAsString(Case.Ticket) + TEXT(" → ") + UEnum::GetValueAsString(Case.Expected)),
			GachaBoxTypeFromTicket(Case.Ticket) == Case.Expected);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaBoxMissingRowLoudTest,
	"CGR.Gacha.Box.MissingRowLoud",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaBoxMissingRowLoudTest::RunTest(const FString& Parameters)
{
	UDataTable* EmptyTable = NewObject<UDataTable>();
	EmptyTable->RowStruct = FGachaBoxVisualRow::StaticStruct();
	UTableManagerSubsystem* Tables = NewObject<UTableManagerSubsystem>(NewObject<UGameInstance>());
	FGachaBoxVisualTableTestAccessor::Initialize(*Tables, EmptyTable);

	AddExpectedError(TEXT("GachaBoxVisual"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("행 없는 상자 종류는 nullptr"), Tables->GetGachaBoxVisual(EGachaBoxType::EquipmentNormal));

	AddExpectedError(TEXT("GachaBoxTicket"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("상자가 아닌 아이템은 일반 장비 상자로"),
		GachaBoxTypeFromTicket(EItemType::RecruitTicketNormal) == EGachaBoxType::EquipmentNormal);
	return true;
}

#endif
