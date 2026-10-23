#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace NumericBalanceAuthoring
{
	struct FAllowedTable
	{
		const TCHAR* Name;
		const TCHAR* Directory;
	};

	static constexpr FAllowedTable AllowedTables[] = {
		{ TEXT("DT_WorkstationUpgrade"), TEXT("Office") },
		{ TEXT("DT_BuildingEnhancementDefinition"), TEXT("Building") },
		{ TEXT("DT_IndustryProfile"), TEXT("Company") },
		{ TEXT("DT_HQLevel"), TEXT("Company") },
		{ TEXT("DT_Project_Game"), TEXT("Company") },
		{ TEXT("DT_Project_IT"), TEXT("Company") },
		{ TEXT("DT_Project_Finance"), TEXT("Company") },
		{ TEXT("DT_WorldFactoryUpgradeDefinition"), TEXT("WorldMap") },
		{ TEXT("DT_MineUpgradeDefinition"), TEXT("WorldMap") }
	};

	using FRows = TMap<FName, TUniquePtr<FStructOnScope>>;

	static FRows Snapshot(const UDataTable& Table, const UScriptStruct& RowStruct)
	{
		FRows Rows;
		for (const auto& Pair : Table.GetRowMap())
		{
			TUniquePtr<FStructOnScope> Copy = MakeUnique<FStructOnScope>(&RowStruct);
			RowStruct.CopyScriptStruct(Copy->GetStructMemory(), Pair.Value);
			Rows.Add(Pair.Key, MoveTemp(Copy));
		}
		return Rows;
	}

	static bool Matches(const UDataTable& Table, const UScriptStruct& RowStruct, const FRows& Expected)
	{
		if (Table.GetRowMap().Num() != Expected.Num()) { return false; }
		for (const auto& Pair : Expected)
		{
			uint8* const* Actual = Table.GetRowMap().Find(Pair.Key);
			if (!Actual || !RowStruct.CompareScriptStruct(*Actual, Pair.Value->GetStructMemory(), 0))
			{
				UE_LOG(LogTemp, Error, TEXT("[NumericBalance] Row mismatch: %s"), *Pair.Key.ToString());
				return false;
			}
			for (TFieldIterator<FTextProperty> TextField(&RowStruct); TextField; ++TextField)
			{
				const FText* ActualText = TextField->ContainerPtrToValuePtr<FText>(*Actual);
				const FText* ExpectedText = TextField->ContainerPtrToValuePtr<FText>(Pair.Value->GetStructMemory());
				if (!ActualText->IdenticalTo(*ExpectedText))
				{
					UE_LOG(LogTemp, Error, TEXT("[NumericBalance] Text identity changed: %s.%s"),
						*Pair.Key.ToString(), *TextField->GetName());
					return false;
				}
			}
		}
		return true;
	}

	static bool RestoreMemory(UDataTable& Table, const UScriptStruct& RowStruct, const FRows& Before)
	{
		bool bSameRows = Table.GetRowMap().Num() == Before.Num();
		for (const auto& Pair : Before)
		{
			bSameRows &= Table.GetRowMap().Contains(Pair.Key);
		}
		if (bSameRows)
		{
			for (const auto& Pair : Before)
			{
				RowStruct.CopyScriptStruct(Table.GetRowMap().FindChecked(Pair.Key), Pair.Value->GetStructMemory());
			}
		}
		else
		{
			Table.EmptyTable();
			for (const auto& Pair : Before)
			{
				Table.AddRow(Pair.Key, *reinterpret_cast<const FTableRowBase*>(Pair.Value->GetStructMemory()));
			}
		}
		Table.HandleDataTableChanged();
		return Matches(Table, RowStruct, Before);
	}

	static bool RestoreDisk(const FString& BackupFile, const FString& AssetFile)
	{
		if (IFileManager::Get().Copy(*AssetFile, *BackupFile, true, true) != COPY_OK) { return false; }
		TArray<uint8> BackupBytes;
		TArray<uint8> AssetBytes;
		return FFileHelper::LoadFileToArray(BackupBytes, *BackupFile)
			&& FFileHelper::LoadFileToArray(AssetBytes, *AssetFile) && BackupBytes == AssetBytes;
	}

	static void Sync(const TArray<FString>& Args)
	{
		if (Args.Num() < 3 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply"))
			|| !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] Usage: CGR.Balance.SyncNumericColumns check|apply TableName Column [Column...]; stop PIE/SIE"));
			return;
		}
		const FAllowedTable* Allowed = nullptr;
		for (const FAllowedTable& Entry : AllowedTables)
		{
			if (Args[1] == Entry.Name) { Allowed = &Entry; break; }
		}
		if (!Allowed)
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] Table is not allowed: %s"), *Args[1]);
			return;
		}
		const FString ObjectPath = FString::Printf(TEXT("/Game/CompanyGrowth/Table/%s/%s.%s"),
			Allowed->Directory, Allowed->Name, Allowed->Name);
		UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath);
		if (!Table || !Table->GetRowStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] Missing, invalid, or dirty table: %s"), *ObjectPath);
			return;
		}
		const UScriptStruct* RowStruct = Table->GetRowStruct();
		TArray<const FProperty*> Columns;
		TSet<FName> Seen;
		for (int32 Index = 2; Index < Args.Num(); ++Index)
		{
			const FName ColumnName(*Args[Index]);
			const FProperty* Property = RowStruct->FindPropertyByName(ColumnName);
			const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
			const bool bWorkstationCurve = Args[1] == TEXT("DT_WorkstationUpgrade")
				&& ColumnName == TEXT("EffectCurve") && CastField<FEnumProperty>(Property);
			if (ColumnName.IsNone() || Seen.Contains(ColumnName) || (!bWorkstationCurve && (!Numeric || Numeric->IsEnum()))
				|| Property->ArrayDim != 1 || CastField<FBoolProperty>(Property))
			{
				UE_LOG(LogTemp, Error, TEXT("[NumericBalance] Missing, duplicate, or non-scalar numeric column: %s"), *Args[Index]);
				return;
			}
			Seen.Add(ColumnName);
			Columns.Add(Property);
		}
		const FString CsvFile = FPaths::ProjectDir() / TEXT("DataImport")
			/ (FString(Allowed->Name) + TEXT("_Import.csv"));
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *CsvFile))
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CSV missing: %s"), *CsvFile);
			return;
		}
		const FCsvParser CsvParser(Csv);
		if (CsvParser.GetRows().IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CSV has no header: %s"), *CsvFile);
			return;
		}
		for (int32 Index = 2; Index < Args.Num(); ++Index)
		{
			int32 HeaderMatches = 0;
			for (const TCHAR* Cell : CsvParser.GetRows()[0])
			{
				HeaderMatches += FString(Cell) == Args[Index] ? 1 : 0;
			}
			if (HeaderMatches != 1)
			{
				UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CSV needs exactly one %s column: %s"), *Args[Index], *CsvFile);
				return;
			}
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = Table->RowStruct;
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CSV: %s"), *Problem);
		}
		if (!Problems.IsEmpty() || Incoming->GetRowMap().Num() != Table->GetRowMap().Num())
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CSV parse or row count failed: %s"), *Args[1]);
			return;
		}
		for (const auto& Pair : Incoming->GetRowMap())
		{
			for (const FProperty* Column : Columns)
			{
				const FNumericProperty* Numeric = CastField<FNumericProperty>(Column);
				if (Numeric && Numeric->IsFloatingPoint()
					&& !FMath::IsFinite(Numeric->GetFloatingPointPropertyValue(
						Column->ContainerPtrToValuePtr<void>(Pair.Value))))
				{
					UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CSV has non-finite value: %s.%s"),
						*Pair.Key.ToString(), *Column->GetName());
					return;
				}
			}
		}
		FRows Before = Snapshot(*Table, *RowStruct);
		FRows Expected = Snapshot(*Table, *RowStruct);
		int32 ChangedFields = 0;
		int32 ChangedRows = 0;
		for (const auto& Pair : Expected)
		{
			uint8* const* Source = Incoming->GetRowMap().Find(Pair.Key);
			if (!Source)
			{
				UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CSV row missing: %s"), *Pair.Key.ToString());
				return;
			}
			bool bRowChanged = false;
			for (const FProperty* Column : Columns)
			{
				if (!Column->Identical_InContainer(Pair.Value->GetStructMemory(), *Source))
				{
					Column->CopyCompleteValue_InContainer(Pair.Value->GetStructMemory(), *Source);
					++ChangedFields;
					bRowChanged = true;
				}
			}
			ChangedRows += bRowChanged ? 1 : 0;
		}
		if (ChangedFields == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("[NumericBalance] CHECK PASS %s: %d rows, %d columns, 0 differences"),
				Allowed->Name, Expected.Num(), Columns.Num());
			return;
		}
		if (Args[0] == TEXT("check"))
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] CHECK FAIL %s: %d rows, %d columns, %d differing rows, %d differing fields"),
				Allowed->Name, Expected.Num(), Columns.Num(), ChangedRows, ChangedFields);
			return;
		}
		UPackage* Package = Table->GetOutermost();
		const FString AssetFile = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("NumericBalance/Backups")
			/ FString::Printf(TEXT("%s_%lld"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::Now().GetTicks());
		const FString BackupFile = BackupDir / (FString(Allowed->Name) + TEXT(".uasset"));
		if (!IFileManager::Get().FileExists(*AssetFile)
			|| !IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*BackupFile, *AssetFile, false, true) != COPY_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] Backup failed; table untouched: %s"), *AssetFile);
			return;
		}
		for (const auto& Pair : Expected)
		{
			uint8* const* Target = Table->GetRowMap().Find(Pair.Key);
			uint8* const* Source = Incoming->GetRowMap().Find(Pair.Key);
			for (const FProperty* Column : Columns)
			{
				Column->CopyCompleteValue_InContainer(*Target, *Source);
			}
		}
		Table->HandleDataTableChanged();
		bool bValid = Matches(*Table, *RowStruct, Expected);
		if (bValid)
		{
			Package->MarkPackageDirty();
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.Error = GWarn;
			bValid = UPackage::SavePackage(Package, Table, *AssetFile, SaveArgs)
				&& !Package->IsDirty() && Matches(*Table, *RowStruct, Expected);
		}
		if (!bValid)
		{
			const bool bMemoryRestored = RestoreMemory(*Table, *RowStruct, Before);
			const bool bDiskRestored = RestoreDisk(BackupFile, AssetFile);
			Package->SetDirtyFlag(!bMemoryRestored || !bDiskRestored);
			UE_LOG(LogTemp, Error, TEXT("[NumericBalance] APPLY FAIL %s; memory restore=%s disk restore=%s backup=%s"),
				Allowed->Name, bMemoryRestored ? TEXT("PASS") : TEXT("FAIL"),
				bDiskRestored ? TEXT("PASS") : TEXT("FAIL"), *BackupFile);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[NumericBalance] APPLY PASS %s: %d rows, %d columns, %d changed rows, %d changed fields; backup=%s"),
			Allowed->Name, Expected.Num(), Columns.Num(), ChangedRows, ChangedFields, *BackupFile);
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Balance.SyncNumericColumns"),
		TEXT("Sync selected scalar numeric CSV columns (and workstation EffectCurve) into an existing allowlisted DataTable: check|apply TableName Column [Column...]."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}

#endif
