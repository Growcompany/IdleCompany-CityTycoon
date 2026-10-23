#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Table/CountryInfoTable.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CountryRoleAuthoring
{
	static void Sync(const TArray<FString>& Args)
	{
		if (Args.Num() < 2 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply")) || !GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Usage: CGR.Overseas.SyncCountryRoles check|apply <row names>; editor, PIE stopped"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CompanyGrowth/Table/WorldMap/DT_CountryInfo.DT_CountryInfo"));
		if (!Table || Table->GetRowStruct() != FCountryInfoTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Country table missing, wrong struct, or has unsaved edits"));
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport/DT_CountryInfo_Import.csv"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Country CSV could not be read; table untouched"));
			return;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FCountryInfoTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems)
			{
				UE_LOG(LogTemp, Error, TEXT("[CountryRoles] CSV: %s"), *Problem);
			}
			return;
		}
		TMap<FName, FCountryInfoTable> Before;
		for (const auto& Pair : Table->GetRowMap()) Before.Add(Pair.Key, *reinterpret_cast<const FCountryInfoTable*>(Pair.Value));
		TMap<FName, FCountryInfoTable> Expected = Before;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			const FName RowName(*Args[Index]);
			FCountryInfoTable* Target = Expected.Find(RowName);
			const FCountryInfoTable* SourceRow = Incoming->FindRow<FCountryInfoTable>(RowName, TEXT("CountryRoles"), false);
			if (!Target || !SourceRow || Target->CountryType != SourceRow->CountryType)
			{
				UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Missing or mismatched row %s"), *Args[Index]);
				return;
			}
			Target->bSupportsFactory = SourceRow->bSupportsFactory;
			Target->bSupportsMine = SourceRow->bSupportsMine;
			Target->bIsTradeHub = SourceRow->bIsTradeHub;
			Target->SupportedIndustries = SourceRow->SupportedIndustries;
			Target->MinableResources = SourceRow->MinableResources;
			Target->RepresentativeIndustry = SourceRow->RepresentativeIndustry;
		}
		bool bAlreadyMatches = true;
		for (const auto& Pair : Expected)
		{
			const FCountryInfoTable& Row = Pair.Value;
			if (CountryDrawer::FacilitiesOf(Row).Num() != 1
				|| (!Row.bSupportsMine && !Row.MinableResources.IsEmpty())
				|| (!Row.bSupportsFactory && (!Row.SupportedIndustries.IsEmpty() || Row.RepresentativeIndustry != ECompanyType::None)))
			{
				UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Invalid single role %s"), *Pair.Key.ToString());
				return;
			}
			bAlreadyMatches &= FCountryInfoTable::StaticStruct()->CompareScriptStruct(&Before[Pair.Key], &Row, 0);
		}
		if (Args[0] == TEXT("check"))
		{
			if (bAlreadyMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[CountryRoles] CHECK PASS: %d single-role countries; selected CSV role fields match"), Expected.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[CountryRoles] CHECK FAIL: saved country roles differ from selected CSV role fields"));
			}
			return;
		}
		if (bAlreadyMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[CountryRoles] APPLY PASS: already matches; no save"));
			return;
		}
		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("OverseasDiorama/SingleRole") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		if (!IFileManager::Get().MakeDirectory(*Directory, true)
			|| IFileManager::Get().Copy(*(Directory / TEXT("DT_CountryInfo.uasset")), *Filename) != COPY_OK
			|| !FFileHelper::SaveStringToFile(Table->GetTableAsJSON(), *(Directory / TEXT("before.json"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Backup failed; table untouched"));
			return;
		}
		for (const auto& Pair : Expected)
		{
			if (!FCountryInfoTable::StaticStruct()->CompareScriptStruct(&Before[Pair.Key], &Pair.Value, 0)) Table->AddRow(Pair.Key, Pair.Value);
		}
		bool bExact = Table->GetRowMap().Num() == Expected.Num();
		for (const auto& Pair : Expected)
		{
			const FCountryInfoTable* Actual = Table->FindRow<FCountryInfoTable>(Pair.Key, TEXT("CountryRoles"), false);
			bExact &= Actual && FCountryInfoTable::StaticStruct()->CompareScriptStruct(Actual, &Pair.Value, 0);
		}
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		Package->MarkPackageDirty();
		if (!bExact || !UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const auto& Pair : Before) Table->AddRow(Pair.Key, Pair.Value);
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Verification/save failed; memory restored; backup %s"), *Directory);
			return;
		}
		const bool bReportSaved = FFileHelper::SaveStringToFile(Table->GetTableAsJSON(), *(Directory / TEXT("after.json")));
		if (!bReportSaved)
		{
			UE_LOG(LogTemp, Error, TEXT("[CountryRoles] Table saved but after.json write failed"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("[CountryRoles] APPLY PASS: %d countries, only requested role fields merged; other fields preserved; %s"), Expected.Num(), *Directory);
		}
	}

	static FAutoConsoleCommand Command(TEXT("CGR.Overseas.SyncCountryRoles"),
		TEXT("Merge selected countries' role fields from DT_CountryInfo_Import.csv: check|apply <row names>."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}

#endif
