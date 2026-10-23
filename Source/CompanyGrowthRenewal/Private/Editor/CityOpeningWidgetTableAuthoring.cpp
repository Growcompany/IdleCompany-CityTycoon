#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Table/WidgetDataTable.h"
#include "UI/HUD/CityOpeningOverlayWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CGRCityOpeningWidgetTableAuthoring
{
	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterOverlay(const TArray<FString>& Arguments)
	{
		if (!Arguments.IsEmpty() || !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[CityOpening] Run CGR.CityOpening.RegisterWidget in the editor with PIE stopped."));
			return;
		}

		const FName RowName(TEXT("CityOpeningOverlay"));
		if (StaticEnum<EWidgetType>()->GetNameStringByValue(static_cast<int64>(EWidgetType::CityOpeningOverlay)) != RowName.ToString())
		{
			UE_LOG(LogTemp, Error, TEXT("[CityOpening] CityOpeningOverlay enum is unavailable."));
			return;
		}

		UClass* WidgetClass = LoadObject<UClass>(nullptr,
			TEXT("/Game/CompanyGrowth/UI/Elements/HUD/UI_CityOpeningOverlay.UI_CityOpeningOverlay_C"));
		if (!WidgetClass || !WidgetClass->IsChildOf(UCityOpeningOverlayWidget::StaticClass())
			|| WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogTemp, Error, TEXT("[CityOpening] UI_CityOpeningOverlay class is missing or incompatible."));
			return;
		}

		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"));
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[CityOpening] DT_WidgetClass is unavailable, incompatible, or dirty."));
			return;
		}

		TMap<FName, FWidgetDataTable> BeforeRows;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value)
			{
				UE_LOG(LogTemp, Error, TEXT("[CityOpening] DT_WidgetClass contains a null row."));
				return;
			}
			const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			if (Pair.Key != RowName && Existing.WidgetType == EWidgetType::CityOpeningOverlay)
			{
				UE_LOG(LogTemp, Error, TEXT("[CityOpening] Another row already uses CityOpeningOverlay: %s"), *Pair.Key.ToString());
				return;
			}
			BeforeRows.Add(Pair.Key, Existing);
		}

		FWidgetDataTable Desired;
		Desired.WidgetType = EWidgetType::CityOpeningOverlay;
		Desired.WidgetClass = WidgetClass;
		if (const FWidgetDataTable* Existing = BeforeRows.Find(RowName))
		{
			if (!RowsEqual(*Existing, Desired))
			{
				UE_LOG(LogTemp, Error, TEXT("[CityOpening] CityOpeningOverlay row has a different registration."));
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("[CityOpening] CityOpeningOverlay is already registered."));
			return;
		}

		Table->Modify(false);
		Table->AddRow(RowName, Desired);
		bool bPreserved = Table->GetRowMap().Num() == BeforeRows.Num() + 1;
		for (const TPair<FName, FWidgetDataTable>& Pair : BeforeRows)
		{
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("CityOpening unchanged rows"), false);
			bPreserved &= Current && RowsEqual(Pair.Value, *Current);
		}
		const FWidgetDataTable* Added = Table->FindRow<FWidgetDataTable>(RowName, TEXT("CityOpening target row"), false);
		bPreserved &= Added && RowsEqual(Desired, *Added);
		if (!bPreserved)
		{
			Table->RemoveRow(RowName);
			Table->GetOutermost()->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[CityOpening] Native row verification failed; table not saved."));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		Table->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			Table->RemoveRow(RowName);
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[CityOpening] DT_WidgetClass save failed."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[CityOpening] Registered CityOpeningOverlay; %d other rows preserved."), BeforeRows.Num());
	}

	static FAutoConsoleCommand RegisterCommand(TEXT("CGR.CityOpening.RegisterWidget"),
		TEXT("Register only CityOpeningOverlay in DT_WidgetClass, preserving unrelated native rows."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterOverlay));
}

#endif
