#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Table/WidgetDataTable.h"
#include "UI/Panel/PlotPurchasePanelWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"

namespace CGRPlotPurchaseWidgetTableAuthoring
{
	static bool RowsEqual(const FWidgetDataTable& Left, const FWidgetDataTable& Right)
	{
		return FWidgetDataTable::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	static void RegisterWidget(const TArray<FString>& Arguments)
	{
		if (!Arguments.IsEmpty() || !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] Usage: CGR.PlotPurchase.RegisterWidget (editor, PIE stopped)."));
			return;
		}

		constexpr TCHAR RowName[] = TEXT("PlotPurchasePanel");
		const FName RowKey(RowName);
		if (StaticEnum<EWidgetType>()->GetNameStringByValue(static_cast<int64>(EWidgetType::PlotPurchasePanel)) != RowName)
		{
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] New WidgetType enum is unavailable; rebuild the editor."));
			return;
		}

		UClass* PanelClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_PlotPurchasePanel.UI_PlotPurchasePanel_C"))).LoadSynchronous();
		if (!PanelClass || !PanelClass->IsChildOf(UPlotPurchasePanelWidget::StaticClass()) || PanelClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] WBP class is unavailable or has the wrong parent."));
			return;
		}

		TStrongObjectPtr<UDataTable> Table(TSoftObjectPtr<UDataTable>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous());
		if (!Table.IsValid() || Table->GetRowStruct() != FWidgetDataTable::StaticStruct())
		{
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] DT_WidgetClass is unavailable or has the wrong row struct."));
			return;
		}

		TMap<FName, FWidgetDataTable> BeforeRows;
		for (const TPair<FName, uint8*>& Entry : Table->GetRowMap())
		{
			if (!Entry.Value)
			{
				UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] Null row in DT_WidgetClass; no changes made."));
				return;
			}
			const FWidgetDataTable& Current = *reinterpret_cast<const FWidgetDataTable*>(Entry.Value);
			if (Current.WidgetType == EWidgetType::PlotPurchasePanel && Entry.Key != RowKey)
			{
				UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] WidgetType is already used by row %s; no changes made."), *Entry.Key.ToString());
				return;
			}
			BeforeRows.Add(Entry.Key, Current);
		}

		FWidgetDataTable Desired;
		Desired.WidgetType = EWidgetType::PlotPurchasePanel;
		Desired.WidgetClass = PanelClass;
		if (const FWidgetDataTable* Existing = BeforeRows.Find(RowKey))
		{
			if (RowsEqual(*Existing, Desired))
			{
				UE_LOG(LogTemp, Display, TEXT("[PlotPurchase] DT_WidgetClass row already registered; %d rows unchanged."), BeforeRows.Num());
				return;
			}
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] Row name is occupied by a different registration; no changes made."));
			return;
		}

		UPackage* TablePackage = Table->GetOutermost();
		if (TablePackage->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] DT_WidgetClass has unsaved changes; save or resolve them first."));
			return;
		}

		Table->Modify(false);
		Table->AddRow(RowKey, Desired);
		bool bPreserved = Table->GetRowMap().Num() == BeforeRows.Num() + 1;
		for (const TPair<FName, FWidgetDataTable>& Entry : BeforeRows)
		{
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Entry.Key, TEXT("PlotPurchase verify"), false);
			bPreserved &= Current && RowsEqual(*Current, Entry.Value);
		}
		const FWidgetDataTable* Added = Table->FindRow<FWidgetDataTable>(RowKey, TEXT("PlotPurchase verify"), false);
		bPreserved &= Added && RowsEqual(*Added, Desired);
		if (!bPreserved)
		{
			Table->RemoveRow(RowKey);
			TablePackage->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] Row comparison failed; reverted the new row without saving."));
			return;
		}

		Table->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(TablePackage->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(TablePackage, Table.Get(), *Filename, SaveArgs))
		{
			Table->RemoveRow(RowKey);
			TablePackage->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[PlotPurchase] DT_WidgetClass save failed; reverted the new row in memory."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[PlotPurchase] Registered row PlotPurchasePanel: %d -> %d; all prior rows preserved."),
			BeforeRows.Num(), Table->GetRowMap().Num());
	}

	static FAutoConsoleCommand RegisterWidgetCommand(
		TEXT("CGR.PlotPurchase.RegisterWidget"),
		TEXT("Register only PlotPurchasePanel in DT_WidgetClass without rewriting existing rows."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidget));
}

#endif
