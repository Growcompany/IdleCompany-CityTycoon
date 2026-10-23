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
#include "Data/ManufacturingReservationRules.h"
#include "Data/ManufacturingResearchRules.h"
#include "Table/EmployeeEquipmentTable.h"
#include "Table/DepartmentDisplayTable.h"
#include "Table/DisciplineDisplayTable.h"
#include "Table/ProductRecipeTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace ManufacturingRecipeAuthoring
{
	static void Sync(const TArray<FString>& Args)
	{
		if ((Args.Num() != 2 && Args.Num() != 3) || (Args[0] != TEXT("check") && Args[0] != TEXT("apply"))
			|| (Args[1] != TEXT("Electronics") && Args[1] != TEXT("Automobile") && Args[1] != TEXT("Semiconductor"))
			|| (Args.Num() == 3 && Args[2] != TEXT("prices"))
			|| !GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingRecipes] Usage: CGR.Manufacturing.SyncRecipes check|apply Electronics|Automobile|Semiconductor [prices]; PIE stopped"));
			return;
		}
		const bool bPricesOnly = Args.Num() == 3;
		const FString AssetName = TEXT("DT_Recipe_") + Args[1];
		const FString AssetPath = TEXT("/Game/CompanyGrowth/Table/Company/Recipe/") + AssetName + TEXT(".") + AssetName;
		UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
		if (!Table || Table->GetRowStruct() != FProductRecipeTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingRecipes] Missing/wrong/dirty table: %s"), *AssetPath);
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport") / (AssetName + TEXT("_Import.csv"))))) return;
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FProductRecipeTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[ManufacturingRecipes] CSV: %s"), *Problem);
			return;
		}
		if (Incoming->GetRowMap().Num() != 100 || Table->GetRowMap().Num() != 100)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingRecipes] Expected 100 rows in both tables; untouched"));
			return;
		}
		TMap<FName, FProductRecipeTable> Before;
		TMap<FName, FProductRecipeTable> Expected;
		bool bMatches = true;
		for (const auto& Pair : Table->GetRowMap())
		{
			const FProductRecipeTable& Current = *reinterpret_cast<const FProductRecipeTable*>(Pair.Value);
			const FProductRecipeTable* Source = Incoming->FindRow<FProductRecipeTable>(Pair.Key, TEXT("ManufacturingRecipes"), false);
			if (!Source || Source->CompanyType != Current.CompanyType || Source->ProjectIndex != Current.ProjectIndex
				|| Source->ProjectIndex < 1 || Source->ProjectIndex > 100
				|| Pair.Key != FName(*FString::Printf(TEXT("Recipe%03d"), Source->ProjectIndex))
				|| (bPricesOnly ? Source->BaseSalePrice <= 0
					: (!ManufacturingReservationRules::HasValidCosts(Source->DevelopmentPointCosts)
						|| !ManufacturingResearchRules::HasValidDefinition(Source->RequiredTeamCapability, Source->ResearchWorkSeconds))))
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingRecipes] Invalid/mismatched row %s; untouched"), *Pair.Key.ToString());
				return;
			}
			Before.Add(Pair.Key, Current);
			FProductRecipeTable Next = Current;
			if (bPricesOnly)
			{
				Next.BaseSalePrice = Source->BaseSalePrice;
			}
			else
			{
				Next.DevelopmentPointCosts = Source->DevelopmentPointCosts;
				Next.RequiredTeamCapability = Source->RequiredTeamCapability;
				Next.ResearchWorkSeconds = Source->ResearchWorkSeconds;
			}
			Expected.Add(Pair.Key, Next);
			bMatches &= bPricesOnly ? Current.BaseSalePrice == Next.BaseSalePrice
				: (Current.DevelopmentPointCosts == Next.DevelopmentPointCosts
					&& Current.RequiredTeamCapability == Next.RequiredTeamCapability
					&& Current.ResearchWorkSeconds == Next.ResearchWorkSeconds);
		}
		if (Args[0] == TEXT("check"))
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[ManufacturingRecipes] CHECK PASS %s: %d %s match"), *Args[1], Expected.Num(), bPricesOnly ? TEXT("sale prices") : TEXT("costs and research requirements"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingRecipes] CHECK FAIL %s: %s differ"), *Args[1], bPricesOnly ? TEXT("sale prices") : TEXT("costs or research requirements"));
			}
			return;
		}
		if (bMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[ManufacturingRecipes] APPLY PASS %s: unchanged"), *Args[1]);
			return;
		}
		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("ManufacturingDevelopment/Recipes") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		if (!IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / (AssetName + TEXT(".uasset"))), *Filename) != COPY_OK) return;
		for (const auto& Pair : Expected) Table->AddRow(Pair.Key, Pair.Value);
		Table->HandleDataTableChanged();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const auto& Pair : Before) Table->AddRow(Pair.Key, Pair.Value);
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingRecipes] Save failed; restored memory; backup %s"), *BackupDir);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[ManufacturingRecipes] APPLY PASS %s: %d %s; other fields preserved"), *Args[1], Expected.Num(), bPricesOnly ? TEXT("sale prices") : TEXT("costs and research requirements"));
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Manufacturing.SyncRecipes"),
		TEXT("Merge development costs and research requirements, or prices-only with [prices], from recipe CSV without recreating rooted DataTables."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}

namespace ManufacturingFieldAuthoring
{
	static bool IsManufacturingRow(FName RowName)
	{
		return RowName == TEXT("Electronics") || RowName == TEXT("Automobile") || RowName == TEXT("Semiconductor");
	}

	static FText TargetText(const FText& Source, const FString& TextNamespace, FName RowName, const TCHAR* Field)
	{
		return Source.IsEmpty() ? FText::GetEmpty()
			: FText::ChangeKey(TextNamespace, RowName.ToString() + TEXT("_") + Field, Source);
	}

	static bool TextMatches(const FText& Current, const FText& Expected,
		const FString& TextNamespace, FName RowName, const TCHAR* Field)
	{
		if (Current.ToString() != Expected.ToString()) return false;
		if (Expected.IsEmpty()) return Current.IsEmpty();
		const auto CurrentNamespace = FTextInspector::GetNamespace(Current);
		const auto CurrentKey = FTextInspector::GetKey(Current);
		return CurrentNamespace.IsSet() && CurrentNamespace.GetValue() == TextNamespace
			&& CurrentKey.IsSet() && CurrentKey.GetValue() == RowName.ToString() + TEXT("_") + Field;
	}

	template <typename RowType, typename MergeFunction>
	static bool SyncTable(const TCHAR* AssetName, const TCHAR* AssetPath, int32 ExpectedRows,
		int32 ExpectedSelectedRows, bool bApply, bool bPreflight, MergeFunction Merge)
	{
		UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
		if (!Table || Table->GetRowStruct() != RowType::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Missing/wrong/dirty table: %s"), AssetPath);
			return false;
		}
		FString Csv;
		const FString CsvPath = FPaths::ProjectDir() / TEXT("DataImport") / (FString(AssetName) + TEXT("_Import.csv"));
		if (!FFileHelper::LoadFileToString(Csv, *CsvPath))
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] CSV could not be read: %s"), *CsvPath);
			return false;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = RowType::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] CSV: %s"), *Problem);
			return false;
		}
		if (Table->GetRowMap().Num() != ExpectedRows || Incoming->GetRowMap().Num() != ExpectedRows)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Expected %d rows in %s; untouched"), ExpectedRows, AssetName);
			return false;
		}
		TMap<FName, RowType> Before;
		TMap<FName, RowType> Expected;
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		bool bMatches = true;
		for (const auto& Pair : Table->GetRowMap())
		{
			const RowType& Current = *reinterpret_cast<const RowType*>(Pair.Value);
			const RowType* Source = Incoming->FindRow<RowType>(Pair.Key, TEXT("ManufacturingFields"), false);
			if (!Source)
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Missing source row %s in %s"), *Pair.Key.ToString(), AssetName);
				return false;
			}
			RowType Next = Current;
			bool bSelected = false;
			bool bRowMatches = true;
			if (!Merge(Pair.Key, Current, *Source, Next, TextNamespace, bSelected, bRowMatches))
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Identity mismatch: %s in %s"), *Pair.Key.ToString(), AssetName);
				return false;
			}
			if (bSelected)
			{
				Before.Add(Pair.Key, Current);
				Expected.Add(Pair.Key, Next);
				bMatches &= bRowMatches;
			}
		}
		if (Expected.Num() != ExpectedSelectedRows)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Expected %d selected rows in %s; got %d"),
				ExpectedSelectedRows, AssetName, Expected.Num());
			return false;
		}
		if (!bApply)
		{
			if (bPreflight) return true;
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[ManufacturingFields] CHECK PASS %s: %d selected rows"), AssetName, Expected.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] CHECK FAIL %s: display text or translation key differs"), AssetName);
			}
			return bMatches;
		}
		if (bMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[ManufacturingFields] APPLY PASS %s: unchanged"), AssetName);
			return true;
		}
		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("ManufacturingDevelopment/Fields")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		if (!IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / (FString(AssetName) + TEXT(".uasset"))), *Filename) != COPY_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Backup failed: %s"), AssetName);
			return false;
		}
		for (const auto& Pair : Expected) Table->AddRow(Pair.Key, Pair.Value);
		Table->HandleDataTableChanged();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const auto& Pair : Before) Table->AddRow(Pair.Key, Pair.Value);
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Save failed; restored memory; backup %s"), *BackupDir);
			return false;
		}
		for (const auto& Pair : Expected)
		{
			const RowType* Saved = Table->FindRow<RowType>(Pair.Key, TEXT("ManufacturingFieldsReadback"), false);
			if (!Saved) return false;
			RowType Probe = *Saved;
			bool bSelected = false;
			bool bRowMatches = true;
			if (!Merge(Pair.Key, *Saved, Pair.Value, Probe, TextNamespace, bSelected, bRowMatches) || !bSelected || !bRowMatches)
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Readback mismatch %s; backup %s"), *Pair.Key.ToString(), *BackupDir);
				return false;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[ManufacturingFields] APPLY PASS %s: %d selected rows; backup %s"),
			AssetName, Expected.Num(), *BackupDir);
		return true;
	}

	static void Sync(const TArray<FString>& Args)
	{
		if (Args.Num() != 1 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply")) || !GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] Usage: CGR.Manufacturing.SyncFields check|apply; PIE stopped"));
			return;
		}
		const bool bApply = Args[0] == TEXT("apply");
		const auto MergeDiscipline = [](FName RowName, const FDisciplineDisplayRow& Current,
			const FDisciplineDisplayRow& Source, FDisciplineDisplayRow& Next, const FString& TextNamespace,
			bool& bSelected, bool& bMatches) -> bool
		{
			if (Source.CompanyType != Current.CompanyType) return false;
			bSelected = IsManufacturingRow(RowName);
			if (!bSelected) return true;
			if (Source.CompanyType != RowName.ToString()) return false;
			struct FTextField { const TCHAR* Name; FText FDisciplineDisplayRow::* Value; };
			const FTextField Fields[] = {
				{ TEXT("Plan"), &FDisciplineDisplayRow::Plan }, { TEXT("Dev"), &FDisciplineDisplayRow::Dev },
				{ TEXT("Graphics"), &FDisciplineDisplayRow::Graphics }, { TEXT("Sound"), &FDisciplineDisplayRow::Sound },
				{ TEXT("Server"), &FDisciplineDisplayRow::Server }, { TEXT("QA"), &FDisciplineDisplayRow::QA }
			};
			for (const FTextField& Field : Fields)
			{
				Next.*(Field.Value) = TargetText(Source.*(Field.Value), TextNamespace, RowName, Field.Name);
				bMatches &= TextMatches(Current.*(Field.Value), Next.*(Field.Value), TextNamespace, RowName, Field.Name);
			}
			return true;
		};
		const auto MergeDepartment = [](FName RowName, const FDepartmentDisplayRow& Current,
			const FDepartmentDisplayRow& Source, FDepartmentDisplayRow& Next, const FString& TextNamespace,
			bool& bSelected, bool& bMatches) -> bool
		{
			if (Source.CompanyType != Current.CompanyType || Source.Department != Current.Department) return false;
			bSelected = IsManufacturingType(Current.CompanyType)
				&& (Current.Department == EEmployeeDepartment::Design
					|| Current.Department == EEmployeeDepartment::Development
					|| Current.Department == EEmployeeDepartment::QA);
			if (!bSelected) return true;
			const FString ExpectedName = StaticEnum<ECompanyType>()->GetNameStringByValue(static_cast<int64>(Current.CompanyType))
				+ TEXT("_") + StaticEnum<EEmployeeDepartment>()->GetNameStringByValue(static_cast<int64>(Current.Department));
			if (RowName.ToString() != ExpectedName) return false;
			Next.DisplayName = TargetText(Source.DisplayName, TextNamespace, RowName, TEXT("DisplayName"));
			bMatches = TextMatches(Current.DisplayName, Next.DisplayName, TextNamespace, RowName, TEXT("DisplayName"));
			return true;
		};
		constexpr TCHAR DisciplinePath[] = TEXT("/Game/CompanyGrowth/Table/Company/DT_DisciplineDisplay.DT_DisciplineDisplay");
		constexpr TCHAR DepartmentPath[] = TEXT("/Game/CompanyGrowth/Table/Employee/DT_DepartmentDisplay.DT_DepartmentDisplay");
		if (bApply && (!SyncTable<FDisciplineDisplayRow>(TEXT("DT_DisciplineDisplay"), DisciplinePath,
			7, 3, false, true, MergeDiscipline)
			|| !SyncTable<FDepartmentDisplayRow>(TEXT("DT_DepartmentDisplay"), DepartmentPath,
				54, 9, false, true, MergeDepartment)))
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] APPLY aborted before saving either table"));
			return;
		}
		const bool bFieldsOk = SyncTable<FDisciplineDisplayRow>(TEXT("DT_DisciplineDisplay"),
			DisciplinePath, 7, 3, bApply, false, MergeDiscipline);
		const bool bDepartmentsOk = SyncTable<FDepartmentDisplayRow>(TEXT("DT_DepartmentDisplay"),
			DepartmentPath, 54, 9, bApply, false, MergeDepartment);
		if (bFieldsOk && bDepartmentsOk)
		{
			UE_LOG(LogTemp, Display, TEXT("[ManufacturingFields] %s PASS"), bApply ? TEXT("APPLY") : TEXT("CHECK"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingFields] %s FAIL"), bApply ? TEXT("APPLY") : TEXT("CHECK"));
		}
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Manufacturing.SyncFields"),
		TEXT("Merge manufacturing discipline and department display cells from CSV: check|apply."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}

namespace ManufacturingEffectAuthoring
{
	static void Sync(const TArray<FString>& Args)
	{
		if (Args.Num() != 1 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply"))
			|| !GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] Usage: CGR.Manufacturing.SyncEffectDescriptions check|apply; PIE stopped"));
			return;
		}
		constexpr TCHAR AssetName[] = TEXT("DT_EmployeeEquipmentEffect");
		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/Employee/DT_EmployeeEquipmentEffect.DT_EmployeeEquipmentEffect"));
		if (!Table || Table->GetRowStruct() != FEmployeeEquipmentEffectRow::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] Missing, wrong, or dirty effect table"));
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport") / (FString(AssetName) + TEXT("_Import.csv")))))
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] CSV could not be read"));
			return;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FEmployeeEquipmentEffectRow::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] CSV: %s"), *Problem);
			return;
		}
		if (Incoming->GetRowMap().Num() != 59 || Table->GetRowMap().Num() != 59)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] Expected 59 existing rows; untouched"));
			return;
		}
		TMap<FName, FEmployeeEquipmentEffectRow> Before;
		TMap<FName, FEmployeeEquipmentEffectRow> Expected;
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		bool bMatches = true;
		for (const auto& Pair : Table->GetRowMap())
		{
			const FEmployeeEquipmentEffectRow& Current = *reinterpret_cast<const FEmployeeEquipmentEffectRow*>(Pair.Value);
			const FEmployeeEquipmentEffectRow* Source = Incoming->FindRow<FEmployeeEquipmentEffectRow>(Pair.Key, TEXT("ManufacturingEffects"), false);
			if (!Source || Source->EffectId != Current.EffectId || Source->ManufacturingDescription.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] Invalid or missing row %s; untouched"), *Pair.Key.ToString());
				return;
			}
			Before.Add(Pair.Key, Current);
			FEmployeeEquipmentEffectRow Next = Current;
			const FString TextKey = Pair.Key.ToString() + TEXT("_ManufacturingDescription");
			Next.ManufacturingDescription = FText::ChangeKey(TextNamespace, TextKey, Source->ManufacturingDescription);
			Expected.Add(Pair.Key, Next);
			bMatches &= Current.ManufacturingDescription.ToString() == Next.ManufacturingDescription.ToString();
			const auto CurrentNamespace = FTextInspector::GetNamespace(Current.ManufacturingDescription);
			const auto CurrentKey = FTextInspector::GetKey(Current.ManufacturingDescription);
			bMatches &= CurrentNamespace.IsSet() && CurrentNamespace.GetValue() == TextNamespace
				&& CurrentKey.IsSet() && CurrentKey.GetValue() == TextKey;
		}
		if (Args[0] == TEXT("check"))
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[ManufacturingEffects] CHECK PASS: %d manufacturing descriptions"), Expected.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] CHECK FAIL: descriptions differ"));
			}
			return;
		}
		if (bMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[ManufacturingEffects] APPLY PASS: unchanged"));
			return;
		}
		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("ManufacturingDevelopment/Effects") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		if (!IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / (FString(AssetName) + TEXT(".uasset"))), *Filename) != COPY_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] Backup failed; untouched"));
			return;
		}
		for (const auto& Pair : Expected) Table->AddRow(Pair.Key, Pair.Value);
		Table->HandleDataTableChanged();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const auto& Pair : Before) Table->AddRow(Pair.Key, Pair.Value);
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] Save failed; restored memory; backup %s"), *BackupDir);
			return;
		}
		for (const auto& Pair : Expected)
		{
			const FEmployeeEquipmentEffectRow* Saved = Table->FindRow<FEmployeeEquipmentEffectRow>(Pair.Key, TEXT("ManufacturingEffectsReadback"), false);
			if (!Saved || Saved->ManufacturingDescription.ToString() != Pair.Value.ManufacturingDescription.ToString())
			{
				UE_LOG(LogTemp, Error, TEXT("[ManufacturingEffects] Readback mismatch: %s; backup %s"), *Pair.Key.ToString(), *BackupDir);
				return;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[ManufacturingEffects] APPLY PASS: 59 descriptions, other effect fields preserved; backup %s"), *BackupDir);
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Manufacturing.SyncEffectDescriptions"),
		TEXT("Merge only manufacturing effect descriptions from CSV: check|apply."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}
#endif
