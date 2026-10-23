#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "Table/HelpTable.h"
#include "Table/WidgetDataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"

namespace CGRHelpOddsWidgetTableAuthoring
{
	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterHelpAxisTab(const TArray<FString>& Arguments)
	{
		if (!Arguments.IsEmpty() || !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpOdds] Use CGR.HelpOdds.RegisterWidget in the editor with PIE stopped."));
			return;
		}

		const FName RowName(TEXT("HelpAxisTab"));
		if (StaticEnum<EWidgetType>()->GetNameStringByValue(static_cast<int64>(EWidgetType::HelpAxisTab)) != RowName.ToString())
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpOdds] HelpAxisTab enum is unavailable."));
			return;
		}
		UClass* WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Elements/Button/UIE_HelpAxisTab.UIE_HelpAxisTab_C"))).LoadSynchronous();
		if (!WidgetClass || !WidgetClass->IsChildOf(UTabButtonWidget::StaticClass()) || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpOdds] HelpAxisTab widget class is missing or incompatible."));
			return;
		}

		TStrongObjectPtr<UDataTable> Table(TSoftObjectPtr<UDataTable>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous());
		if (!Table.IsValid() || Table->GetRowStruct() != FWidgetDataTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpOdds] DT_WidgetClass is unavailable, incompatible, or already dirty."));
			return;
		}

		TMap<FName, FWidgetDataTable> BeforeRows;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value)
			{
				UE_LOG(LogTemp, Error, TEXT("[HelpOdds] DT_WidgetClass contains a null row."));
				return;
			}
			const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			if (Pair.Key != RowName && Existing.WidgetType == EWidgetType::HelpAxisTab)
			{
				UE_LOG(LogTemp, Error, TEXT("[HelpOdds] Another row already uses HelpAxisTab: %s"), *Pair.Key.ToString());
				return;
			}
			BeforeRows.Add(Pair.Key, Existing);
		}

		FWidgetDataTable Desired;
		Desired.WidgetType = EWidgetType::HelpAxisTab;
		Desired.WidgetClass = WidgetClass;
		if (const FWidgetDataTable* Existing = BeforeRows.Find(RowName))
		{
			if (!RowsEqual(*Existing, Desired))
			{
				UE_LOG(LogTemp, Error, TEXT("[HelpOdds] HelpAxisTab row has a different registration."));
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("[HelpOdds] HelpAxisTab is already registered."));
			return;
		}

		Table->Modify(false);
		Table->AddRow(RowName, Desired);
		bool bPreserved = Table->GetRowMap().Num() == BeforeRows.Num() + 1;
		for (const TPair<FName, FWidgetDataTable>& Pair : BeforeRows)
		{
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("HelpOdds unchanged rows"), false);
			bPreserved &= Current && RowsEqual(Pair.Value, *Current);
		}
		const FWidgetDataTable* Added = Table->FindRow<FWidgetDataTable>(RowName, TEXT("HelpOdds target row"), false);
		bPreserved &= Added && RowsEqual(Desired, *Added);
		if (!bPreserved)
		{
			Table->RemoveRow(RowName);
			Table->GetOutermost()->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[HelpOdds] Native row verification failed; no package saved."));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		Table->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Table.Get(), *Filename, SaveArgs))
		{
			Table->RemoveRow(RowName);
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[HelpOdds] DT_WidgetClass save failed."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[HelpOdds] Registered HelpAxisTab; %d unrelated rows preserved."), BeforeRows.Num());
	}

	static FAutoConsoleCommand RegisterCommand(TEXT("CGR.HelpOdds.RegisterWidget"),
		TEXT("Register only HelpAxisTab in DT_WidgetClass, preserving unrelated native rows."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterHelpAxisTab));

	static bool SameText(const FText& Current, const FText& Expected)
	{
		if (Current.ToString() != Expected.ToString()) return false;
		if (Expected.IsEmpty()) return Current.IsEmpty();
		const auto CurrentNamespace = FTextInspector::GetNamespace(Current);
		const auto ExpectedNamespace = FTextInspector::GetNamespace(Expected);
		const auto CurrentKey = FTextInspector::GetKey(Current);
		const auto ExpectedKey = FTextInspector::GetKey(Expected);
		return CurrentNamespace.IsSet() && ExpectedNamespace.IsSet()
			&& CurrentNamespace.GetValue() == ExpectedNamespace.GetValue()
			&& CurrentKey.IsSet() && ExpectedKey.IsSet() && CurrentKey.GetValue() == ExpectedKey.GetValue();
	}

	static bool HelpRowsEqual(const FHelpTable& Current, const FHelpTable& Expected)
	{
		return FHelpTable::StaticStruct()->CompareScriptStruct(&Current, &Expected, 0)
			&& SameText(Current.Eyebrow, Expected.Eyebrow)
			&& SameText(Current.Body, Expected.Body);
	}

	static void SyncHelpBodies(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < 2 || (Arguments[0] != TEXT("check") && Arguments[0] != TEXT("apply"))
			|| !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpBody] Usage: CGR.UI.SyncHelpBodies check|apply RowName [RowName...]; stop PIE/SIE first"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CompanyGrowth/Table/UI/DT_Help.DT_Help"));
		if (!Table || Table->GetRowStruct() != FHelpTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpBody] DT_Help missing, wrong struct, or dirty"));
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport/DT_Help_Import.csv"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpBody] DT_Help_Import.csv cannot be read"));
			return;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FHelpTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[HelpBody] CSV: %s"), *Problem);
			return;
		}

		TMap<FName, FHelpTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value) { UE_LOG(LogTemp, Error, TEXT("[HelpBody] Null row %s"), *Pair.Key.ToString()); return; }
			Before.Add(Pair.Key, *reinterpret_cast<const FHelpTable*>(Pair.Value));
		}
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		TMap<FName, FHelpTable> Expected = Before;
		TSet<FName> Requested;
		bool bMatches = true;
		for (int32 Index = 1; Index < Arguments.Num(); ++Index)
		{
			const FName RowName(*Arguments[Index]);
			if (RowName.IsNone() || Requested.Contains(RowName))
			{
				UE_LOG(LogTemp, Error, TEXT("[HelpBody] Empty or duplicate row: %s"), *Arguments[Index]);
				return;
			}
			Requested.Add(RowName);
			FHelpTable* Target = Expected.Find(RowName);
			const FHelpTable* Source = Incoming->FindRow<FHelpTable>(RowName, TEXT("HelpBody"), false);
			if (!Target || !Source || Target->HelpKey != RowName || Source->HelpKey != RowName || Source->Body.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("[HelpBody] Missing or invalid asset/CSV row: %s"), *RowName.ToString());
				return;
			}
			Target->Body = FText::ChangeKey(TextNamespace, RowName.ToString() + TEXT("_Body"), Source->Body);
			const bool bRowMatches = HelpRowsEqual(Before.FindChecked(RowName), *Target);
			bMatches &= bRowMatches;
			UE_LOG(LogTemp, Display, TEXT("[HelpBody] %s row %s: asset='%s' CSV='%s'"),
				bRowMatches ? TEXT("MATCH") : TEXT("DIFF"), *RowName.ToString(),
				*Before.FindChecked(RowName).Body.ToString(), *Source->Body.ToString());
		}
		if (Arguments[0] == TEXT("check"))
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[HelpBody] CHECK PASS (%d rows)"), Requested.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[HelpBody] CHECK FAIL (%d rows)"), Requested.Num());
			}
			return;
		}
		if (bMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[HelpBody] APPLY PASS: already matches; no save"));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("HelpBody/Backups")
			/ FString::Printf(TEXT("%s_%lld"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::Now().GetTicks());
		if (!IFileManager::Get().FileExists(*Filename) || !IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / TEXT("DT_Help.uasset")), *Filename, false) != COPY_OK
			|| !FFileHelper::SaveStringToFile(Table->GetTableAsJSON(), *(BackupDir / TEXT("DT_Help.json"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpBody] Backup failed; table untouched"));
			return;
		}
		for (const FName RowName : Requested)
		{
			if (!HelpRowsEqual(Before.FindChecked(RowName), Expected.FindChecked(RowName)))
			{
				Table->AddRow(RowName, Expected.FindChecked(RowName));
			}
		}
		bool bExact = Table->GetRowMap().Num() == Before.Num();
		for (const TPair<FName, FHelpTable>& Pair : Expected)
		{
			const FHelpTable* Actual = Table->FindRow<FHelpTable>(Pair.Key, TEXT("HelpBodyVerify"), false);
			bExact &= Actual && HelpRowsEqual(*Actual, Pair.Value);
		}
		if (bExact) Table->HandleDataTableChanged();
		if (bExact) Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!bExact || !UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const FName RowName : Requested) Table->AddRow(RowName, Before.FindChecked(RowName));
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[HelpBody] APPLY FAIL: memory restored; backup=%s"), *BackupDir);
			return;
		}
		bool bSavedRowsMatch = Table->GetRowMap().Num() == Expected.Num() && !Package->IsDirty();
		for (const TPair<FName, FHelpTable>& Pair : Expected)
		{
			const FHelpTable* Saved = Table->FindRow<FHelpTable>(Pair.Key, TEXT("HelpBodySaved"), false);
			bSavedRowsMatch &= Saved && HelpRowsEqual(*Saved, Pair.Value);
		}
		if (!bSavedRowsMatch)
		{
			UE_LOG(LogTemp, Error, TEXT("[HelpBody] Saved table readback mismatch; backup=%s"), *BackupDir);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[HelpBody] APPLY PASS (%d rows); only Body changed; backup=%s"),
			Requested.Num(), *BackupDir);
	}

	static FAutoConsoleCommand SyncHelpBodiesCommand(TEXT("CGR.UI.SyncHelpBodies"),
		TEXT("Merge selected DT_Help.Body cells from DataImport/DT_Help_Import.csv without replacing rooted DataTable: check|apply RowName [RowName...]."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SyncHelpBodies));
}

#endif
