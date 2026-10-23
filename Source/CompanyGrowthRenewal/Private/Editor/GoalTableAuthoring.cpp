#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Table/GoalTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace GoalTableAuthoring
{
	static FText NormalizeText(const FText& Source, const FString& TextNamespace,
		FName RowName, const FString& Field)
	{
		return Source.IsEmpty() ? FText::GetEmpty()
			: FText::ChangeKey(TextNamespace, RowName.ToString() + TEXT("_") + Field, Source);
	}

	static FGoalTable NormalizeRow(const FGoalTable& Source, const FString& TextNamespace, FName RowName)
	{
		FGoalTable Result = Source;
		Result.Title = NormalizeText(Source.Title, TextNamespace, RowName, TEXT("Title"));
		Result.Desc = NormalizeText(Source.Desc, TextNamespace, RowName, TEXT("Desc"));
		for (int32 Index = 0; Index < Result.StepLines.Num(); ++Index)
		{
			Result.StepLines[Index] = NormalizeText(Source.StepLines[Index], TextNamespace, RowName,
				FString::Printf(TEXT("StepLines_Index%d"), Index));
		}
		return Result;
	}

	static bool SameText(const FText& Current, const FText& Expected)
	{
		if (Current.ToString() != Expected.ToString()) { return false; }
		if (Expected.IsEmpty()) { return Current.IsEmpty(); }
		const auto CurrentNamespace = FTextInspector::GetNamespace(Current);
		const auto CurrentKey = FTextInspector::GetKey(Current);
		const auto ExpectedNamespace = FTextInspector::GetNamespace(Expected);
		const auto ExpectedKey = FTextInspector::GetKey(Expected);
		return CurrentNamespace.IsSet() && ExpectedNamespace.IsSet()
			&& CurrentNamespace.GetValue() == ExpectedNamespace.GetValue()
			&& CurrentKey.IsSet() && ExpectedKey.IsSet() && CurrentKey.GetValue() == ExpectedKey.GetValue();
	}

	static bool RowsMatch(const FGoalTable& Current, const FGoalTable& Expected)
	{
		if (!FGoalTable::StaticStruct()->CompareScriptStruct(&Current, &Expected, 0)
			|| !SameText(Current.Title, Expected.Title) || !SameText(Current.Desc, Expected.Desc)
			|| Current.StepLines.Num() != Expected.StepLines.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Current.StepLines.Num(); ++Index)
		{
			if (!SameText(Current.StepLines[Index], Expected.StepLines[Index])) { return false; }
		}
		return true;
	}

	static void RestoreRows(UDataTable& Table, const TMap<FName, FGoalTable>& Before,
		const TArray<FName>& Requested)
	{
		for (const FName RowName : Requested)
		{
			if (const FGoalTable* Previous = Before.Find(RowName)) { Table.AddRow(RowName, *Previous); }
			else { Table.RemoveRow(RowName); }
		}
		Table.HandleDataTableChanged();
		Table.GetOutermost()->SetDirtyFlag(false);
	}

	static void Sync(const TArray<FString>& Args)
	{
		if (Args.Num() < 2 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply"))
			|| !GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[GoalTable] Usage: CGR.Goal.SyncRows check|apply RowID [RowID...]; stop PIE first"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/Mission/DT_Goal.DT_Goal"));
		if (!Table || Table->GetRowStruct() != FGoalTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[GoalTable] Missing, wrong or dirty DT_Goal"));
			return;
		}
		FString Csv;
		const FString CsvPath = FPaths::ProjectDir() / TEXT("DataImport/DT_Goal_Import.csv");
		if (!FFileHelper::LoadFileToString(Csv, *CsvPath))
		{
			UE_LOG(LogTemp, Error, TEXT("[GoalTable] Missing CSV: %s"), *CsvPath);
			return;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FGoalTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		for (const FString& Problem : Problems) { UE_LOG(LogTemp, Error, TEXT("[GoalTable] CSV: %s"), *Problem); }
		if (!Problems.IsEmpty()) { return; }

		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		TArray<FName> Requested;
		TMap<FName, FGoalTable> Expected;
		TSet<FName> Seen;
		bool bMatches = true;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			const FName RowName(*Args[Index]);
			if (RowName.IsNone() || Seen.Contains(RowName))
			{
				UE_LOG(LogTemp, Error, TEXT("[GoalTable] Empty or duplicate RowID: %s"), *Args[Index]);
				return;
			}
			Seen.Add(RowName);
			const FGoalTable* Source = Incoming->FindRow<FGoalTable>(RowName, TEXT("GoalTableAuthoring"), false);
			if (!Source)
			{
				UE_LOG(LogTemp, Error, TEXT("[GoalTable] Missing CSV row: %s"), *RowName.ToString());
				return;
			}
			Requested.Add(RowName);
			FGoalTable Next = NormalizeRow(*Source, TextNamespace, RowName);
			const FGoalTable* Current = Table->FindRow<FGoalTable>(RowName, TEXT("GoalTableAuthoring"), false);
			bMatches &= Current && RowsMatch(*Current, Next);
			Expected.Add(RowName, MoveTemp(Next));
		}
		if (Args[0] == TEXT("check") || bMatches)
		{
			if (bMatches) { UE_LOG(LogTemp, Display, TEXT("[GoalTable] CHECK PASS (%d rows)"), Requested.Num()); }
			else { UE_LOG(LogTemp, Error, TEXT("[GoalTable] CHECK FAIL (%d rows); apply CSV in an owned editor"), Requested.Num()); }
			return;
		}

		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("GoalTable/Backups")
			/ FString::Printf(TEXT("%s_%lld"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::Now().GetTicks());
		if (!IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / TEXT("DT_Goal.uasset")), *Filename) != COPY_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("[GoalTable] Backup failed"));
			return;
		}
		TMap<FName, FGoalTable> Before;
		for (const auto& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<const FGoalTable*>(Pair.Value));
		}
		for (const FName RowName : Requested) { Table->AddRow(RowName, Expected.FindChecked(RowName)); }
		Table->HandleDataTableChanged();
		for (const auto& Pair : Before)
		{
			if (Seen.Contains(Pair.Key)) { continue; }
			const FGoalTable* Unrelated = Table->FindRow<FGoalTable>(Pair.Key, TEXT("GoalTableUnrelated"), false);
			if (!Unrelated || !FGoalTable::StaticStruct()->CompareScriptStruct(Unrelated, &Pair.Value, 0))
			{
				RestoreRows(*Table, Before, Requested);
				UE_LOG(LogTemp, Error, TEXT("[GoalTable] Unrelated row changed: %s"), *Pair.Key.ToString());
				return;
			}
		}
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			RestoreRows(*Table, Before, Requested);
			UE_LOG(LogTemp, Error, TEXT("[GoalTable] Save failed; restored memory, backup: %s"), *BackupDir);
			return;
		}
		for (const FName RowName : Requested)
		{
			const FGoalTable* Saved = Table->FindRow<FGoalTable>(RowName, TEXT("GoalTableReadback"), false);
			if (!Saved || !RowsMatch(*Saved, Expected.FindChecked(RowName)))
			{
				UE_LOG(LogTemp, Error, TEXT("[GoalTable] Readback mismatch: %s; backup: %s"), *RowName.ToString(), *BackupDir);
				return;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[GoalTable] APPLY PASS (%d rows); backup: %s"), Requested.Num(), *BackupDir);
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Goal.SyncRows"),
		TEXT("Merge selected DT_Goal CSV rows without recreating the rooted DataTable: check|apply RowID [RowID...]."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}

#endif
