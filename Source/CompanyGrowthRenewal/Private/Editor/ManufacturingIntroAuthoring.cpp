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
#include "Table/PanelIntroTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace ManufacturingIntroAuthoring
{
	static void Sync(const TArray<FString>& Args)
	{
		if (Args.Num() != 1 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply"))
			|| !GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] Usage: CGR.Tutorial.SyncManufacturingIntro check|apply; stop PIE first"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/UI/DT_PanelIntro.DT_PanelIntro"));
		if (!Table || Table->GetRowStruct() != FPanelIntroTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] Missing, wrong or dirty DT_PanelIntro"));
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport/DT_PanelIntro_Import.csv")))) { return; }
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FPanelIntroTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		for (const FString& Problem : Problems) { UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] CSV: %s"), *Problem); }
		if (!Problems.IsEmpty()) { return; }
		const TMap<FName, int32> Keys = {
			{TEXT("ManufacturingUnlock"), 1}, {TEXT("ManufacturingOffice"), 3},
			{TEXT("ManufacturingResearch"), 2}, {TEXT("ManufacturingProduction"), 3}
		};
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		TMap<FName, FPanelIntroTable> Expected;
		bool bMatches = true;
		for (const auto& Key : Keys)
		{
			for (int32 Index = 0; Index < Key.Value; ++Index)
			{
				const FName RowName(*FString::Printf(TEXT("%s_%d"), *Key.Key.ToString(), Index));
				const FPanelIntroTable* Source = Incoming->FindRow<FPanelIntroTable>(RowName, TEXT("ManufacturingIntro"), false);
				if (!Source || Source->PanelKey != Key.Key || Source->StepIndex != Index
					|| Source->AnchorName.IsNone() || Source->Body.IsEmpty() || Source->Eyebrow.IsEmpty())
				{
					UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] Invalid row: %s"), *RowName.ToString());
					return;
				}
				FPanelIntroTable Next = *Source;
				Next.Body = FText::ChangeKey(TextNamespace, RowName.ToString() + TEXT("_Body"), Source->Body);
				Next.Eyebrow = FText::ChangeKey(TextNamespace, RowName.ToString() + TEXT("_Eyebrow"), Source->Eyebrow);
				const FPanelIntroTable* Current = Table->FindRow<FPanelIntroTable>(RowName, TEXT("ManufacturingIntro"), false);
				bMatches &= Current && FPanelIntroTable::StaticStruct()->CompareScriptStruct(Current, &Next, 0);
				Expected.Add(RowName, MoveTemp(Next));
			}
		}
		if (Args[0] == TEXT("check") || bMatches)
		{
			if (bMatches) { UE_LOG(LogTemp, Display, TEXT("[ManufacturingIntro] CHECK PASS (%d rows)"), Expected.Num()); }
			else { UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] CHECK FAIL; apply CSV in an owned editor")); }
			return;
		}
		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("ManufacturingIntro/Backups") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		if (!IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / TEXT("DT_PanelIntro.uasset")), *Filename) != COPY_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] Backup failed"));
			return;
		}
		TMap<FName, FPanelIntroTable> Before;
		for (const auto& Pair : Table->GetRowMap()) { Before.Add(Pair.Key, *reinterpret_cast<const FPanelIntroTable*>(Pair.Value)); }
		for (const auto& Pair : Expected) { Table->AddRow(Pair.Key, Pair.Value); }
		Table->HandleDataTableChanged();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const auto& Pair : Expected) { Table->RemoveRow(Pair.Key); }
			for (const auto& Pair : Before) { Table->AddRow(Pair.Key, Pair.Value); }
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] Save failed; restored memory, backup: %s"), *BackupDir);
			return;
		}
		for (const auto& Pair : Before)
		{
			if (Expected.Contains(Pair.Key)) { continue; }
			const FPanelIntroTable* After = Table->FindRow<FPanelIntroTable>(Pair.Key, TEXT("ManufacturingIntro"), false);
			if (!After || !FPanelIntroTable::StaticStruct()->CompareScriptStruct(After, &Pair.Value, 0))
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingIntro] Unrelated row changed: %s"), *Pair.Key.ToString());
				return;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[ManufacturingIntro] APPLY PASS (%d rows); backup: %s"), Expected.Num(), *BackupDir);
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Tutorial.SyncManufacturingIntro"),
		TEXT("Merge manufacturing coachmarks from CSV without recreating rooted DataTables: check|apply."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}

#endif
