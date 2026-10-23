#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Table/WidgetDataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"

namespace CGRSkylineWidgetTableAuthoring
{
	static bool WriteReport(const TSharedRef<FJsonObject>& Report, const FString& Filename)
	{
		FString Json;
		return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		// enum 문자열로 변환하지 않는다. 이름이 폐기된 원시 enum 값도 비교 대상이다.
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterWidgets(const TArray<FString>& Arguments)
	{
		const FString ReportDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SkylineUI"));
		const FString ReportFilename = ReportDirectory / TEXT("widget_native_merge.json");
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), false);
		Report->SetBoolField(TEXT("saved"), false);
		Report->SetBoolField(TEXT("other_rows_preserved"), false);
		Report->SetNumberField(TEXT("before_count"), -1);
		Report->SetNumberField(TEXT("after_count"), -1);
		Report->SetNumberField(TEXT("expected_count"), -1);
		Report->SetNumberField(TEXT("backup_raw_enum"), -1);
		Report->SetStringField(TEXT("repaired_row"), TEXT(""));
		Report->SetStringField(TEXT("error"), TEXT("operation_not_completed"));
		if (!IFileManager::Get().MakeDirectory(*ReportDirectory, true) || !WriteReport(Report, ReportFilename))
		{
			UE_LOG(LogTemp, Error, TEXT("[SkylineWidgetTable] Cannot prepare report: %s"), *ReportFilename);
			return;
		}
		ON_SCOPE_EXIT
		{
			if (!WriteReport(Report, ReportFilename))
			{
				UE_LOG(LogTemp, Error, TEXT("[SkylineWidgetTable] Report write failed: %s"), *ReportFilename);
			}
		};
		auto Fail = [&Report](const FString& Message)
		{
			Report->SetStringField(TEXT("error"), Message);
			UE_LOG(LogTemp, Error, TEXT("[SkylineWidgetTable] %s"), *Message);
		};
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			Fail(TEXT("Requires the editor game thread with PIE/SIE stopped."));
			return;
		}
		// 콘솔의 공백 토큰 분할을 되돌린 뒤 경로를 검증한다.
		const FString BackupArgument = FString::Join(Arguments, TEXT(" ")).TrimStartAndEnd().TrimQuotes();
		const bool bRestoreBackup = !BackupArgument.IsEmpty();

		struct FRegistration
		{
			const TCHAR* RowName;
			EWidgetType Type;
			const TCHAR* ClassPath;
		};
		const FRegistration Registrations[] = {
			{TEXT("SkylineScreen"), EWidgetType::SkylineScreen, TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_SkylineScreen.UI_SkylineScreen_C")},
			{TEXT("SkylineFloorEntry"), EWidgetType::SkylineFloorEntry, TEXT("/Game/CompanyGrowth/UI/Elements/Skyline/UIE_SkylineFloorEntry.UIE_SkylineFloorEntry_C")},
			{TEXT("SkylineCrewRow"), EWidgetType::SkylineCrewRow, TEXT("/Game/CompanyGrowth/UI/Elements/Skyline/UIE_SkylineCrewRow.UIE_SkylineCrewRow_C")},
			{TEXT("SkylineCompanyRow"), EWidgetType::SkylineCompanyRow, TEXT("/Game/CompanyGrowth/UI/Elements/Skyline/UIE_SkylineCompanyRow.UIE_SkylineCompanyRow_C")}
		};
		TMap<FName, FWidgetDataTable> DesiredRows;
		for (const FRegistration& Registration : Registrations)
		{
			UClass* WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(Registration.ClassPath)).LoadSynchronous();
			if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()) || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
			{
				Fail(FString::Printf(TEXT("Widget class unavailable: %s. No rows changed or saved."), Registration.ClassPath));
				return;
			}
			FWidgetDataTable NewRow;
			NewRow.WidgetType = Registration.Type;
			NewRow.WidgetClass = WidgetClass;
			DesiredRows.Add(FName(Registration.RowName), NewRow);
		}

		if (bRestoreBackup)
		{
			FString BackupFilename = FPaths::ConvertRelativePathToFull(BackupArgument);
			FPaths::NormalizeFilename(BackupFilename);
			if (FPaths::GetCleanFilename(BackupFilename) != TEXT("DT_WidgetClass.uasset") || !FPaths::FileExists(BackupFilename))
			{
				Fail(TEXT("Backup must be an existing DT_WidgetClass.uasset file."));
				return;
			}
			const FString MountRoot(TEXT("/SkylineRestore/"));
			const FString BackupDirectory = FPaths::GetPath(BackupFilename) + TEXT("/");
			if (FPackageName::MountPointExists(MountRoot) || FindPackage(nullptr, TEXT("/SkylineRestore/DT_WidgetClass")))
			{
				Fail(TEXT("SkylineRestore mount or package already exists. Use a fresh editor process for backup restoration."));
				return;
			}
			FPackageName::RegisterMountPoint(MountRoot, BackupDirectory);
			ON_SCOPE_EXIT { FPackageName::UnRegisterMountPoint(MountRoot, BackupDirectory); };
			TStrongObjectPtr<UDataTable> Backup(TSoftObjectPtr<UDataTable>(FSoftObjectPath(
				TEXT("/SkylineRestore/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous());
			if (!Backup.IsValid() || Backup->GetRowStruct() != FWidgetDataTable::StaticStruct())
			{
				Fail(TEXT("Backup table failed to load or has a different row struct."));
				return;
			}
			const FName RepairName(TEXT("LootBoxLayerWidget"));
			const FWidgetDataTable* OriginalRow = Backup->FindRow<FWidgetDataTable>(RepairName, TEXT("Skyline native restore"), false);
			if (!OriginalRow || OriginalRow->WidgetClass != nullptr)
			{
				Fail(TEXT("Backup is missing LootBoxLayerWidget with WidgetClass=None. No rows changed or saved."));
				return;
			}
			const int32 RawEnum = static_cast<uint8>(OriginalRow->WidgetType);
			DesiredRows.Add(RepairName, *OriginalRow);
			Report->SetStringField(TEXT("backup_file"), BackupFilename);
			Report->SetNumberField(TEXT("backup_raw_enum"), RawEnum);
			UE_LOG(LogTemp, Display, TEXT("[SkylineWidgetTable] Backup LootBoxLayerWidget raw enum=%d, WidgetClass=None."), RawEnum);
		}

		TStrongObjectPtr<UDataTable> Table(TSoftObjectPtr<UDataTable>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous());
		if (!Table.IsValid() || Table->GetRowStruct() != FWidgetDataTable::StaticStruct())
		{
			Fail(TEXT("Main DT_WidgetClass failed to load or has a different row struct."));
			return;
		}
		TMap<FName, FWidgetDataTable> BeforeRows;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value)
			{
				Fail(TEXT("Main table contains a null row."));
				return;
			}
			const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			BeforeRows.Add(Pair.Key, Existing);
			for (const FRegistration& Registration : Registrations)
			{
				if (Existing.WidgetType == Registration.Type && Pair.Key != FName(Registration.RowName))
				{
					Fail(FString::Printf(TEXT("Duplicate Skyline WidgetType in unrelated row %s; no rows changed or saved."), *Pair.Key.ToString()));
					return;
				}
			}
		}
		int32 ExpectedCount = BeforeRows.Num();
		for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
		{
			if (!BeforeRows.Contains(Pair.Key)) ++ExpectedCount;
		}
		Report->SetNumberField(TEXT("before_count"), BeforeRows.Num());
		Report->SetNumberField(TEXT("expected_count"), ExpectedCount);

		UPackage* TablePackage = Table->GetOutermost();
		const bool bWasDirty = TablePackage->IsDirty();
		bool bSaved = false;
		Table->Modify(false);
		ON_SCOPE_EXIT
		{
			if (!bSaved)
			{
				// 실패한 호출의 대상 행만 되돌린다. 다른 행을 전체 재임포트하지 않는다.
				for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
				{
					if (const FWidgetDataTable* Previous = BeforeRows.Find(Pair.Key)) Table->AddRow(Pair.Key, *Previous);
					else Table->RemoveRow(Pair.Key);
				}
				bool bRestored = Table->GetRowMap().Num() == BeforeRows.Num();
				for (const TPair<FName, FWidgetDataTable>& Pair : BeforeRows)
				{
					const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("Skyline rollback"), false);
					bRestored &= Current && RowsEqual(Pair.Value, *Current);
				}
				Report->SetBoolField(TEXT("rollback_restored"), bRestored);
				TablePackage->SetDirtyFlag(bRestored ? bWasDirty : true);
			}
		};
		for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
		{
			Table->AddRow(Pair.Key, Pair.Value);
		}
		bool bOtherRowsPreserved = true;
		for (const TPair<FName, FWidgetDataTable>& Pair : BeforeRows)
		{
			if (DesiredRows.Contains(Pair.Key)) continue;
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("Skyline unchanged rows"), false);
			bOtherRowsPreserved &= Current && RowsEqual(Pair.Value, *Current);
		}
		bool bTargetsMatch = true;
		for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
		{
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("Skyline registered rows"), false);
			bTargetsMatch &= Current && RowsEqual(Pair.Value, *Current);
		}
		const int32 AfterCount = Table->GetRowMap().Num();
		Report->SetNumberField(TEXT("after_count"), AfterCount);
		Report->SetBoolField(TEXT("other_rows_preserved"), bOtherRowsPreserved);
		Report->SetBoolField(TEXT("target_rows_match"), bTargetsMatch);
		if (AfterCount != ExpectedCount || !bOtherRowsPreserved || !bTargetsMatch)
		{
			Fail(TEXT("Exact row/count verification failed; no package saved."));
			return;
		}

		Table->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(TablePackage->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		bSaved = UPackage::SavePackage(TablePackage, Table.Get(), *Filename, SaveArgs);
		Report->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Fail(TEXT("Native DT_WidgetClass package save failed."));
			return;
		}
		if (bRestoreBackup) Report->SetStringField(TEXT("repaired_row"), TEXT("LootBoxLayerWidget"));
		Report->SetStringField(TEXT("error"), TEXT(""));
		Report->SetBoolField(TEXT("success"), true);
		UE_LOG(LogTemp, Display, TEXT("[SkylineWidgetTable] SUCCESS before=%d after=%d; unrelated rows preserved; backup repair=%d."),
			BeforeRows.Num(), AfterCount, bRestoreBackup);
	}

	static FAutoConsoleCommand RegisterWidgetsCommand(TEXT("CGR.Skyline.RegisterWidgets"),
		TEXT("Register only the four Skyline widget rows; optional binary backup restores LootBoxLayerWidget without JSON conversion."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidgets));
}

#endif
