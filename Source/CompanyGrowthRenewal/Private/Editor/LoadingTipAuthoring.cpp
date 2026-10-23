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
#include "Table/LoadingTipTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace LoadingTipAuthoring
{
	static bool SameText(const FText& Current, const FText& Expected)
	{
		if (Current.ToString() != Expected.ToString()) return false;
		if (Expected.IsEmpty()) return Current.IsEmpty();
		const auto CurrentNamespace = FTextInspector::GetNamespace(Current);
		const auto ExpectedNamespace = FTextInspector::GetNamespace(Expected);
		const auto CurrentKey = FTextInspector::GetKey(Current);
		const auto ExpectedKey = FTextInspector::GetKey(Expected);
		return CurrentNamespace.IsSet() == ExpectedNamespace.IsSet()
			&& (!CurrentNamespace.IsSet() || CurrentNamespace.GetValue() == ExpectedNamespace.GetValue())
			&& CurrentKey.IsSet() == ExpectedKey.IsSet()
			&& (!CurrentKey.IsSet() || CurrentKey.GetValue() == ExpectedKey.GetValue());
	}

	static bool RowsEqual(const FLoadingTipTable& Current, const FLoadingTipTable& Expected)
	{
		return FLoadingTipTable::StaticStruct()->CompareScriptStruct(&Current, &Expected, 0)
			&& SameText(Current.Body, Expected.Body);
	}

	static void SyncBody(const TArray<FString>& Args)
	{
		if (Args.Num() < 2 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply"))
			|| !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[LoadingTip] Usage: CGR.LoadingTip.SyncBody check|apply RowName [RowName...]; stop PIE/SIE first"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/UI/DT_LoadingTip.DT_LoadingTip"));
		if (!Table || Table->GetRowStruct() != FLoadingTipTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[LoadingTip] DT_LoadingTip missing, wrong struct, or dirty"));
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport/DT_LoadingTip_Import.csv"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[LoadingTip] DT_LoadingTip_Import.csv cannot be read"));
			return;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FLoadingTipTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[LoadingTip] CSV: %s"), *Problem);
			return;
		}

		TMap<FName, FLoadingTipTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value) { UE_LOG(LogTemp, Error, TEXT("[LoadingTip] Null row %s"), *Pair.Key.ToString()); return; }
			Before.Add(Pair.Key, *reinterpret_cast<const FLoadingTipTable*>(Pair.Value));
		}
		TMap<FName, FLoadingTipTable> Expected = Before;
		TSet<FName> Requested;
		TSet<FName> Changed;
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			const FName RowName(*Args[Index]);
			if (RowName.IsNone() || Requested.Contains(RowName))
			{
				UE_LOG(LogTemp, Error, TEXT("[LoadingTip] Empty or duplicate row: %s"), *Args[Index]);
				return;
			}
			Requested.Add(RowName);
			FLoadingTipTable* Target = Expected.Find(RowName);
			const FLoadingTipTable* Source = Incoming->FindRow<FLoadingTipTable>(RowName, TEXT("LoadingTipSync"), false);
			if (!Target || !Source || Target->TipKey != RowName || Source->TipKey != RowName
				|| Source->Body.IsEmpty() || Source->Body.ToString().Len() > FLoadingTipRules::MaxBodyChars)
			{
				UE_LOG(LogTemp, Error, TEXT("[LoadingTip] Missing or invalid asset/CSV row: %s"), *RowName.ToString());
				return;
			}
			Target->Body = FText::ChangeKey(TextNamespace, RowName.ToString() + TEXT("_Body"), Source->Body);
			const bool bRowMatches = RowsEqual(Before.FindChecked(RowName), *Target);
			if (!bRowMatches) Changed.Add(RowName);
			UE_LOG(LogTemp, Display, TEXT("[LoadingTip] %s row %s: asset='%s' CSV='%s'"),
				bRowMatches ? TEXT("MATCH") : TEXT("DIFF"), *RowName.ToString(),
				*Before.FindChecked(RowName).Body.ToString(), *Source->Body.ToString());
		}
		if (Args[0] == TEXT("check"))
		{
			if (Changed.IsEmpty())
			{
				UE_LOG(LogTemp, Display, TEXT("[LoadingTip] CHECK PASS (%d rows)"), Requested.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LoadingTip] CHECK FAIL (%d of %d rows differ)"), Changed.Num(), Requested.Num());
			}
			return;
		}
		if (Changed.IsEmpty())
		{
			UE_LOG(LogTemp, Display, TEXT("[LoadingTip] APPLY PASS: already matches; no save"));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("LoadingTip/Backups")
			/ FString::Printf(TEXT("%s_%lld"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::Now().GetTicks());
		if (!IFileManager::Get().FileExists(*Filename) || !IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / TEXT("DT_LoadingTip.uasset")), *Filename, false) != COPY_OK
			|| !FFileHelper::SaveStringToFile(Table->GetTableAsJSON(), *(BackupDir / TEXT("DT_LoadingTip.json"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[LoadingTip] Backup failed; table untouched"));
			return;
		}
		for (const FName RowName : Changed) Table->AddRow(RowName, Expected.FindChecked(RowName));
		bool bExact = Table->GetRowMap().Num() == Expected.Num();
		for (const TPair<FName, FLoadingTipTable>& Pair : Expected)
		{
			const FLoadingTipTable* Actual = Table->FindRow<FLoadingTipTable>(Pair.Key, TEXT("LoadingTipVerify"), false);
			bExact &= Actual && RowsEqual(*Actual, Pair.Value);
		}
		if (bExact) Table->HandleDataTableChanged();
		if (bExact) Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!bExact || !UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const FName RowName : Changed) Table->AddRow(RowName, Before.FindChecked(RowName));
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[LoadingTip] APPLY FAIL: memory restored; backup=%s"), *BackupDir);
			return;
		}
		bool bSavedRowsMatch = Table->GetRowMap().Num() == Expected.Num() && !Package->IsDirty();
		for (const TPair<FName, FLoadingTipTable>& Pair : Expected)
		{
			const FLoadingTipTable* Saved = Table->FindRow<FLoadingTipTable>(Pair.Key, TEXT("LoadingTipSaved"), false);
			bSavedRowsMatch &= Saved && RowsEqual(*Saved, Pair.Value);
		}
		if (!bSavedRowsMatch)
		{
			UE_LOG(LogTemp, Error, TEXT("[LoadingTip] Saved table readback mismatch; backup=%s"), *BackupDir);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[LoadingTip] APPLY PASS (%d of %d rows changed); only Body changed; backup=%s"),
			Changed.Num(), Requested.Num(), *BackupDir);
	}

	static FAutoConsoleCommand SyncBodyCommand(TEXT("CGR.LoadingTip.SyncBody"),
		TEXT("Compare or merge selected DT_LoadingTip.Body cells from CSV without replacing rooted DataTable: check|apply RowName [RowName...]."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SyncBody));
}

#endif
