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
#include "Table/EmployeeEquipmentTable.h"
#include "Table/GoalTable.h"
#include "Table/MissionTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace EmployeeExperienceTextAuthoring
{
	static FText TargetText(const FText& Current, const FText& Source, const FString& TextNamespace,
		FName RowName, const FString& Field)
	{
		if (Source.IsEmpty()) { return FText::GetEmpty(); }
		FString Key = RowName.ToString() + TEXT("_") + Field;
		const auto ExistingNamespace = FTextInspector::GetNamespace(Current);
		const auto ExistingKey = FTextInspector::GetKey(Current);
		if (ExistingNamespace.IsSet() && ExistingNamespace.GetValue() == TextNamespace && ExistingKey.IsSet())
		{
			Key = ExistingKey.GetValue();
		}
		return FText::ChangeKey(TextNamespace, Key, Source);
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

	static bool MergeText(FText& Destination, const FText& Source, const FString& TextNamespace,
		FName RowName, const FString& Field)
	{
		const FText Expected = TargetText(Destination, Source, TextNamespace, RowName, Field);
		const bool bMatches = SameText(Destination, Expected);
		Destination = Expected;
		return bMatches;
	}

	static bool MergeLines(TArray<FText>& Destination, const TArray<FText>& Source,
		const FString& TextNamespace, FName RowName, const TCHAR* Field)
	{
		bool bMatches = Destination.Num() == Source.Num();
		TArray<FText> Result;
		Result.Reserve(Source.Num());
		for (int32 Index = 0; Index < Source.Num(); ++Index)
		{
			FText Value = Destination.IsValidIndex(Index) ? Destination[Index] : FText::GetEmpty();
			bMatches &= MergeText(Value, Source[Index], TextNamespace, RowName,
				FString::Printf(TEXT("%s_Index%d"), Field, Index));
			Result.Add(MoveTemp(Value));
		}
		Destination = MoveTemp(Result);
		return bMatches;
	}

	template <typename RowType, typename MergeFunction>
	static bool SyncTable(const TCHAR* AssetName, const TCHAR* AssetPath, int32 ExpectedRowCount,
		FName SelectedRow, bool bApply, bool bPreflight, const FString& BackupDir, MergeFunction Merge)
	{
		UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
		if (!Table || Table->GetRowStruct() != RowType::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Missing, wrong, or dirty table: %s"), AssetPath);
			return false;
		}
		FString Csv;
		const FString CsvPath = FPaths::ProjectDir() / TEXT("DataImport") / (FString(AssetName) + TEXT("_Import.csv"));
		if (!FFileHelper::LoadFileToString(Csv, *CsvPath))
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Missing CSV: %s"), *CsvPath);
			return false;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = RowType::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems)
			{
				UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] %s CSV: %s"), AssetName, *Problem);
			}
			return false;
		}
		if (ExpectedRowCount > 0 && (Table->GetRowMap().Num() != ExpectedRowCount || Incoming->GetRowMap().Num() != ExpectedRowCount))
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Row count changed in %s (asset=%d CSV=%d expected=%d)"),
				AssetName, Table->GetRowMap().Num(), Incoming->GetRowMap().Num(), ExpectedRowCount);
			return false;
		}
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		TMap<FName, RowType> Before;
		TMap<FName, RowType> Expected;
		bool bMatches = true;
		for (const auto& Pair : Table->GetRowMap())
		{
			if (SelectedRow != NAME_None && Pair.Key != SelectedRow) { continue; }
			const RowType& Current = *reinterpret_cast<const RowType*>(Pair.Value);
			const RowType* Source = Incoming->FindRow<RowType>(Pair.Key, TEXT("EmployeeExperienceText"), false);
			if (!Source)
			{
				UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] CSV lacks %s.%s"), AssetName, *Pair.Key.ToString());
				return false;
			}
			RowType Next = Current;
			bool bRowMatches = true;
			if (!Merge(Pair.Key, Current, *Source, Next, TextNamespace, bRowMatches))
			{
				UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Identity mismatch %s.%s"), AssetName, *Pair.Key.ToString());
				return false;
			}
			Before.Add(Pair.Key, Current);
			Expected.Add(Pair.Key, MoveTemp(Next));
			bMatches &= bRowMatches;
		}
		if (Expected.Num() != (SelectedRow == NAME_None ? ExpectedRowCount : 1))
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Missing target row in %s"), AssetName);
			return false;
		}
		if (bPreflight) { return true; }
		if (!bApply || bMatches)
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[EmployeeExperienceText] %s PASS %s (%d rows)"),
					bApply ? TEXT("APPLY") : TEXT("CHECK"), AssetName, Expected.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] CHECK FAIL %s (%d rows)"), AssetName, Expected.Num());
			}
			return bMatches;
		}
		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*BackupDir, true)
			|| IFileManager::Get().Copy(*(BackupDir / (FString(AssetName) + TEXT(".uasset"))), *Filename) != COPY_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Backup failed: %s"), AssetName);
			return false;
		}
		for (const auto& Pair : Expected) { Table->AddRow(Pair.Key, Pair.Value); }
		Table->HandleDataTableChanged();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			for (const auto& Pair : Before) { Table->AddRow(Pair.Key, Pair.Value); }
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Save failed: %s; backup %s"), AssetName, *BackupDir);
			return false;
		}
		for (const auto& Pair : Expected)
		{
			const RowType* Saved = Table->FindRow<RowType>(Pair.Key, TEXT("EmployeeExperienceTextReadback"), false);
			if (!Saved)
			{
				UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Readback missing %s.%s"), AssetName, *Pair.Key.ToString());
				return false;
			}
			RowType Probe = *Saved;
			bool bRowMatches = true;
			if (!Merge(Pair.Key, *Saved, Pair.Value, Probe, TextNamespace, bRowMatches) || !bRowMatches)
			{
				UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Readback mismatch %s.%s; backup %s"),
					AssetName, *Pair.Key.ToString(), *BackupDir);
				return false;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[EmployeeExperienceText] APPLY PASS %s; backup %s"), AssetName, *BackupDir);
		return true;
	}

	static bool MergeEffects(FName RowName, const FEmployeeEquipmentEffectRow& Current,
		const FEmployeeEquipmentEffectRow& Source, FEmployeeEquipmentEffectRow& Next,
		const FString& TextNamespace, bool& bMatches)
	{
		if (Source.EffectId != Current.EffectId || Source.EffectId != RowName) { return false; }
		bMatches &= MergeText(Next.Description, Source.Description, TextNamespace, RowName, TEXT("Description"));
		bMatches &= MergeText(Next.ManufacturingDescription, Source.ManufacturingDescription,
			TextNamespace, RowName, TEXT("ManufacturingDescription"));
		return true;
	}

	static bool MergeGoal(FName RowName, const FGoalTable& Current, const FGoalTable& Source,
		FGoalTable& Next, const FString& TextNamespace, bool& bMatches)
	{
		if (Current.ConditionType != Source.ConditionType) { return false; }
		bMatches &= MergeText(Next.Desc, Source.Desc, TextNamespace, RowName, TEXT("Desc"));
		bMatches &= MergeLines(Next.StepLines, Source.StepLines, TextNamespace, RowName, TEXT("StepLines"));
		return true;
	}

	static bool MergeMission(FName RowName, const FMissionTable& Current, const FMissionTable& Source,
		FMissionTable& Next, const FString& TextNamespace, bool& bMatches)
	{
		if (Current.ConditionType != Source.ConditionType) { return false; }
		bMatches &= MergeLines(Next.MentorLines, Source.MentorLines, TextNamespace, RowName, TEXT("MentorLines"));
		bMatches &= MergeLines(Next.GuideLines, Source.GuideLines, TextNamespace, RowName, TEXT("GuideLines"));
		bMatches &= MergeLines(Next.ExplainLines, Source.ExplainLines, TextNamespace, RowName, TEXT("ExplainLines"));
		return true;
	}

	static void Sync(const TArray<FString>& Args)
	{
		if (Args.Num() != 1 || (Args[0] != TEXT("check") && Args[0] != TEXT("apply"))
			|| !GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] Usage: CGR.Employee.SyncExperienceText check|apply; stop PIE first"));
			return;
		}
		const bool bApply = Args[0] == TEXT("apply");
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("EmployeeExperienceText")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		const auto Effects = [bApply, &BackupDir](bool bPreflight)
		{
			return SyncTable<FEmployeeEquipmentEffectRow>(TEXT("DT_EmployeeEquipmentEffect"),
				TEXT("/Game/CompanyGrowth/Table/Employee/DT_EmployeeEquipmentEffect.DT_EmployeeEquipmentEffect"),
				59, NAME_None, bApply, bPreflight, BackupDir, MergeEffects);
		};
		const auto Goal = [bApply, &BackupDir](bool bPreflight)
		{
			return SyncTable<FGoalTable>(TEXT("DT_Goal"),
				TEXT("/Game/CompanyGrowth/Table/Mission/DT_Goal.DT_Goal"),
				0, TEXT("G12_LevelUpEmployee"), bApply, bPreflight, BackupDir, MergeGoal);
		};
		const auto Mission = [bApply, &BackupDir](bool bPreflight)
		{
			return SyncTable<FMissionTable>(TEXT("DT_Mission"),
				TEXT("/Game/CompanyGrowth/Table/Mission/DT_Mission.DT_Mission"),
				33, TEXT("M16_LevelUp"), bApply, bPreflight, BackupDir, MergeMission);
		};
		if (bApply && (!Effects(true) || !Goal(true) || !Mission(true)))
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] APPLY aborted before changing tables"));
			return;
		}
		const bool bEffectsOk = Effects(false);
		const bool bGoalOk = Goal(false);
		const bool bMissionOk = Mission(false);
		if (bEffectsOk && bGoalOk && bMissionOk)
		{
			UE_LOG(LogTemp, Display, TEXT("[EmployeeExperienceText] %s PASS"), bApply ? TEXT("APPLY") : TEXT("CHECK"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[EmployeeExperienceText] %s FAIL"), bApply ? TEXT("APPLY") : TEXT("CHECK"));
		}
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Employee.SyncExperienceText"),
		TEXT("Merge employee XP explanation text from CSV into existing DataTables: check|apply."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));
}

#endif
