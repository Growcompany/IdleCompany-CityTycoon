#include "UI/Panel/BuildingManagePanelWidget.h"

#include "Data/BuildingEnhancementData.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Internationalization/Text.h"

/**
 * 도움말 "?" 의 강화 곡선 표 ― 설계 R2 §6.2 / §9.4.
 *
 * 곡선은 UBuildingEnhancementHelper 가 소유한다. 여기서는 읽어서 줄로 세울 뿐이고,
 * 수치를 DT_Help 에 복사하지 않는다 ― 복사하면 밸런스를 고칠 때마다 도움말만 틀려진다.
 *
 * 축 두 개:
 *   페이지 = 이 건물에 보이는 강화 항목 (빌드업 / 브랜드파워 / ...)
 *   탭     = 비용 / 효과
 */
namespace
{
	// 묶음 강화 단위. UI 의 "n회 강화" 버튼과 같은 눈금이어야 도움말이 화면과 같은 말을 한다
	constexpr int32 GBulkSteps[] = { 1, 5, 10, 25 };

	// 이 건물 타입에 실제로 노출되는 강화만 페이지에 올린다. 안 보이는 항목을 도움말이
	// 먼저 알려주면 "있는데 왜 안 보이지" 가 된다
	void CollectVisibleTypes(const ABuildingBaseActor* Building, TArray<EBuildingEnhancementType>& Out)
	{
		Out.Reset();
		if (!Building) { return; }
		const UEnum* TypeEnum = StaticEnum<EBuildingEnhancementType>();
		if (!TypeEnum) { return; }
		// 마지막 항목은 _MAX 계열이므로 제외한다
		const int32 Count = TypeEnum->NumEnums() - 1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const EBuildingEnhancementType Type =
				static_cast<EBuildingEnhancementType>(TypeEnum->GetValueByIndex(Index));
			if (Building->GetEnhancementLevel(Type) >= 0) { Out.Add(Type); }
		}
	}

	FText TypeLabel(EBuildingEnhancementType Type)
	{
		const UEnum* TypeEnum = StaticEnum<EBuildingEnhancementType>();
		return TypeEnum ? TypeEnum->GetDisplayNameTextByValue(static_cast<int64>(Type)) : FText::GetEmpty();
	}

	FText Money(int64 Amount)
	{
		FNumberFormattingOptions Options;
		Options.SetUseGrouping(true);
		return FText::AsNumber(Amount, &Options);
	}

	FText Multiplier(float Value)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(2);
		Options.SetMaximumFractionalDigits(2);
		return FText::Format(FText::FromString(TEXT("x{0}")), FText::AsNumber(Value, &Options));
	}
}

bool UBuildingManagePanelWidget::GetHelpTableInfo(FName HelpKey, FHelpTableInfo& Out) const
{
	TArray<EBuildingEnhancementType> Types;
	CollectVisibleTypes(TargetBuilding, Types);
	if (Types.Num() == 0)
	{
		// 건물이 없으면 표를 만들 수 없다 ― "?" 는 DT_Help 의 글 한 장으로 내려간다
		return false;
	}

	Out.Title = NSLOCTEXT("Help", "BuildingManageTable", "강화 정보");
	Out.AxisLabels = {
		NSLOCTEXT("Help", "BuildingManageCost", "비용"),
		NSLOCTEXT("Help", "BuildingManageEffect", "효과"),
	};
	Out.PageLabels.Reset();
	for (EBuildingEnhancementType Type : Types)
	{
		const int32 Level = TargetBuilding->GetEnhancementLevel(Type);
		Out.PageLabels.Add(FText::Format(
			FText::FromString(TEXT("{0}  Lv.{1}")), TypeLabel(Type), FText::AsNumber(Level)));
	}
	Out.Footnote = NSLOCTEXT("Help", "BuildingManageSource",
		"수치는 현재 건물 기준으로 계산됩니다");
	return true;
}

void UBuildingManagePanelWidget::GetHelpTableRows(FName HelpKey, int32 PageIndex, int32 AxisIndex,
	TArray<FHelpTableRow>& Out) const
{
	Out.Reset();
	TArray<EBuildingEnhancementType> Types;
	CollectVisibleTypes(TargetBuilding, Types);
	if (!Types.IsValidIndex(PageIndex)) { return; }

	const EBuildingEnhancementType Type = Types[PageIndex];
	const int32 Level = TargetBuilding->GetEnhancementLevel(Type);

	if (AxisIndex == 0)
	{
		// 비용 ― 지금 누르면 얼마인가, 묶어 누르면 얼마인가
		for (int32 Step : GBulkSteps)
		{
			FHelpTableRow Row;
			Row.Label = FText::Format(FText::FromString(TEXT("+{0}")), FText::AsNumber(Step));
			Row.Value = Money(TargetBuilding->GetBulkUpgradeCost(Type, Step));
			Row.Note = FText::Format(
				FText::FromString(TEXT("Lv.{0} → Lv.{1}")),
				FText::AsNumber(Level), FText::AsNumber(Level + Step));
			Out.Add(Row);
		}
		return;
	}

	// 효과 ― 같은 눈금에서 배율이 어디까지 가는가. 비용 탭과 행이 1:1 로 맞아야 비교가 된다
	FHelpTableRow Now;
	Now.Label = NSLOCTEXT("Help", "BuildingManageNow", "현재");
	Now.Value = Multiplier(UBuildingEnhancementHelper::CalculateEffectMultiplier(Type, Level));
	Now.Note = FText::Format(FText::FromString(TEXT("Lv.{0}")), FText::AsNumber(Level));
	Out.Add(Now);

	for (int32 Step : GBulkSteps)
	{
		FHelpTableRow Row;
		Row.Label = FText::Format(FText::FromString(TEXT("+{0}")), FText::AsNumber(Step));
		Row.Value = Multiplier(UBuildingEnhancementHelper::CalculateEffectMultiplier(Type, Level + Step));
		Row.Note = FText::Format(FText::FromString(TEXT("Lv.{0}")), FText::AsNumber(Level + Step));
		Out.Add(Row);
	}
}
