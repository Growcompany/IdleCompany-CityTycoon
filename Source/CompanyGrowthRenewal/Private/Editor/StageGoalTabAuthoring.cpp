#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Table/WidgetDataTable.h"
#include "UI/Element/Effects/StageGoalTabWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"

// 분야 목표 달성 서랍 탭(UIE_StageGoalTab)을 DT_WidgetClass 에 한 행만 넣는다.
// 공유 표라 다른 행의 구조값·원시 바이트가 그대로인지 확인한 뒤에만 저장한다.
namespace CGRStageGoalTabAuthoring
{
	static const TCHAR* RowName = TEXT("StageGoalTab");
	static const TCHAR* ClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Effects/UIE_StageGoalTab.UIE_StageGoalTab_C");

	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static bool SaveTable(UDataTable* Table)
	{
		UPackage* Package = Table->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Table, *Filename, SaveArgs);
	}

	static void Register(const TArray<FString>& Arguments)
	{
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor || !Arguments.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] Usage: CGR.StageGoalTab.Register (editor game thread, PIE/SIE stopped)"));
			return;
		}
		if (StaticEnum<EWidgetType>()->GetNameStringByValue(static_cast<int64>(EWidgetType::StageGoalTab)) != RowName)
		{
			UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] EWidgetType::StageGoalTab missing"));
			return;
		}
		UClass* WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(ClassPath)).LoadSynchronous();
		if (!WidgetClass || !WidgetClass->IsChildOf(UStageGoalTabWidget::StaticClass()) || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] Missing/incompatible class: %s"), ClassPath);
			return;
		}
		FWidgetDataTable Desired;
		Desired.WidgetType = EWidgetType::StageGoalTab;
		Desired.WidgetClass = WidgetClass;

		UDataTable* Table = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous();
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct())
		{
			UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] DT_WidgetClass missing or wrong row struct."));
			return;
		}
		if (Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] Save unsaved DT_WidgetClass changes before registering."));
			return;
		}
		const FName TargetRow(RowName);
		TMap<FName, FWidgetDataTable> BeforeRows;
		TMap<FName, TArray<uint8>> OtherRowBytes;
		const int32 RowSize = FWidgetDataTable::StaticStruct()->GetStructureSize();
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value) return;
			const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			BeforeRows.Add(Pair.Key, Existing);
			if (Pair.Key == TargetRow)
			{
				if (Existing.WidgetType != Desired.WidgetType)
				{
					UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] Target row occupied by another type."));
					return;
				}
				continue;
			}
			if (Existing.WidgetType == Desired.WidgetType)
			{
				UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] StageGoalTab already used in row %s."), *Pair.Key.ToString());
				return;
			}
			OtherRowBytes.FindOrAdd(Pair.Key).Append(Pair.Value, RowSize);
		}
		const int32 ExpectedCount = BeforeRows.Num() + (BeforeRows.Contains(TargetRow) ? 0 : 1);

		UPackage* Package = Table->GetOutermost();
		const bool bWasDirty = Package->IsDirty();
		Table->Modify(false);
		Table->AddRow(TargetRow, Desired);
		bool bValid = Table->GetRowMap().Num() == ExpectedCount;
		const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(TargetRow, TEXT("StageGoalTab target"), false);
		bValid &= Current && RowsEqual(*Current, Desired);
		for (const TPair<FName, TArray<uint8>>& Pair : OtherRowBytes)
		{
			const FWidgetDataTable* Other = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("StageGoalTab preserved"), false);
			bValid &= Other && RowsEqual(*Other, BeforeRows.FindChecked(Pair.Key))
				&& FMemory::Memcmp(Pair.Value.GetData(), Other, Pair.Value.Num()) == 0;
		}
		if (!bValid || !SaveTable(Table))
		{
			if (const FWidgetDataTable* Previous = BeforeRows.Find(TargetRow)) Table->AddRow(TargetRow, *Previous);
			else Table->RemoveRow(TargetRow);
			Package->SetDirtyFlag(bWasDirty);
			UE_LOG(LogTemp, Error, TEXT("[StageGoalTabAuthoring] Row merge verification/save failed; restored the target row in memory."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[StageGoalTabAuthoring] Registered StageGoalTab; before=%d after=%d; other row bytes unchanged."),
			BeforeRows.Num(), Table->GetRowMap().Num());
	}

	static FAutoConsoleCommand RegisterCommand(TEXT("CGR.StageGoalTab.Register"),
		TEXT("Upsert the StageGoalTab DT_WidgetClass row while preserving every other row."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Register));
}

#endif
