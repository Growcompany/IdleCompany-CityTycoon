// DT_UISounds 에 CSV 파일의 새 행만 더한다 (에디터 전용 콘솔 명령).
//
// 왜 재임포트가 아닌가: 이 표는 여러 세션이 행을 직접 넣어 DataImport CSV 가 표의 전부가 아니다.
// CSV 재임포트·JSON 왕복은 전체 교체라 CSV 에 없는 남의 행이 사라진다. 여기서는 AddRow 만 하고
// 기존 행은 구조체 비교로 불변을 증명한 뒤에만 저장한다 (EquipmentGachaWidgetTableAuthoring 과 같은 계약).
//
//   CGR.Audio.AppendUISoundRows <CSV 경로 ― 프로젝트 폴더 기준, 공백 없이>
//
// CSV 열은 DT_UISounds_Import.csv 와 같다. 이름·값이 같은 기존 행은 건너뛰고(재실행 안전),
// 이름이나 태그가 기존 행과 부딪히면 아무것도 바꾸지 않는다. 보고서 = Saved/UISoundRowAppend/report.json

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
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
#include "Sound/SoundBase.h"
#include "Table/SoundTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CGRUISoundRowAppend
{
	static const TCHAR* TableAssetPath = TEXT("/Game/CompanyGrowth/Table/Audio/DT_UISounds.DT_UISounds");

	static bool WriteReport(const TSharedRef<FJsonObject>& Report, const FString& Filename)
	{
		FString Json;
		return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static bool RowsEqual(const FUISoundTable& A, const FUISoundTable& B)
	{
		return FUISoundTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static TArray<TSharedPtr<FJsonValue>> ToJsonStrings(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FString& Value : Values)
		{
			Out.Add(MakeShared<FJsonValueString>(Value));
		}
		return Out;
	}

	static void AppendRows(const TArray<FString>& Arguments)
	{
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("UISoundRowAppend"));
		const FString ReportFilename = Directory / TEXT("report.json");
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), false);
		Report->SetBoolField(TEXT("saved"), false);
		Report->SetBoolField(TEXT("other_rows_preserved"), false);
		Report->SetStringField(TEXT("error"), TEXT("operation_not_completed"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		ON_SCOPE_EXIT { WriteReport(Report, ReportFilename); };

		if (Arguments.Num() != 1 || !GEditor || GEditor->PlayWorld)
		{
			Report->SetStringField(TEXT("error"), TEXT("usage: CGR.Audio.AppendUISoundRows <project-relative csv>, editor only, PIE stopped"));
			return;
		}
		const FString CsvFilename = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Arguments[0]);
		Report->SetStringField(TEXT("csv"), CsvFilename);
		FString CsvText;
		if (!FFileHelper::LoadFileToString(CsvText, *CsvFilename))
		{
			Report->SetStringField(TEXT("error"), TEXT("csv_unreadable"));
			return;
		}

		UDataTable* Table = LoadObject<UDataTable>(nullptr, TableAssetPath);
		if (!Table || Table->GetRowStruct() != FUISoundTable::StaticStruct())
		{
			Report->SetStringField(TEXT("error"), TEXT("table_missing_or_wrong_row_struct"));
			return;
		}
		// 이 에디터에서 남이 고치다 만 행까지 같이 저장되지 않게 한다
		if (Table->GetOutermost()->IsDirty())
		{
			Report->SetStringField(TEXT("error"), TEXT("table_has_unsaved_edits"));
			return;
		}

		// 엔진 CSV 임포터로 읽는다 ― 태그·소프트 경로 해석이 재임포트와 같다
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FUISoundTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(CsvText);
		Report->SetArrayField(TEXT("csv_problems"), ToJsonStrings(Problems));
		if (Problems.Num() > 0 || Incoming->GetRowMap().Num() == 0)
		{
			Report->SetStringField(TEXT("error"), Problems.Num() > 0 ? TEXT("csv_problems") : TEXT("csv_empty"));
			return;
		}

		TMap<FName, FUISoundTable> Before;
		TMap<FGameplayTag, FName> TagOwners;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const FUISoundTable& Row = *reinterpret_cast<const FUISoundTable*>(Pair.Value);
			Before.Add(Pair.Key, Row);
			if (Row.SoundTag.IsValid())
			{
				TagOwners.Add(Row.SoundTag, Pair.Key);
			}
		}
		Report->SetNumberField(TEXT("before_count"), Before.Num());

		TArray<TPair<FName, FUISoundTable>> ToAdd;
		TArray<FString> Added;
		TArray<FString> Skipped;
		for (const TPair<FName, uint8*>& Pair : Incoming->GetRowMap())
		{
			const FUISoundTable& Row = *reinterpret_cast<const FUISoundTable*>(Pair.Value);
			const FString RowLabel = Pair.Key.ToString();
			if (!Row.SoundTag.IsValid())
			{
				Report->SetStringField(TEXT("error"), TEXT("invalid_tag:") + RowLabel);
				return;
			}
			if (Row.Sound.IsNull() || !Cast<USoundBase>(Row.Sound.ToSoftObjectPath().TryLoad()))
			{
				Report->SetStringField(TEXT("error"), TEXT("sound_not_loadable:") + RowLabel);
				return;
			}
			if (const FUISoundTable* Existing = Before.Find(Pair.Key))
			{
				if (RowsEqual(*Existing, Row))
				{
					Skipped.Add(RowLabel);
					continue;
				}
				Report->SetStringField(TEXT("error"), TEXT("row_exists_with_other_values:") + RowLabel);
				return;
			}
			// 같은 태그 두 행이면 SoundManager 캐시에서 한쪽이 조용히 이긴다
			if (const FName* Owner = TagOwners.Find(Row.SoundTag))
			{
				Report->SetStringField(TEXT("error"), FString::Printf(TEXT("tag_already_used:%s by %s"), *Row.SoundTag.ToString(), *Owner->ToString()));
				return;
			}
			TagOwners.Add(Row.SoundTag, Pair.Key);
			ToAdd.Emplace(Pair.Key, Row);
			Added.Add(RowLabel);
		}
		Report->SetArrayField(TEXT("added"), ToJsonStrings(Added));
		Report->SetArrayField(TEXT("skipped_identical"), ToJsonStrings(Skipped));

		for (const TPair<FName, FUISoundTable>& Entry : ToAdd)
		{
			Table->AddRow(Entry.Key, Entry.Value);
		}

		bool bOthersPreserved = true;
		for (const TPair<FName, FUISoundTable>& Pair : Before)
		{
			const FUISoundTable* Now = Table->FindRow<FUISoundTable>(Pair.Key, TEXT("AppendUISoundRows"), false);
			if (!Now || !RowsEqual(*Now, Pair.Value))
			{
				bOthersPreserved = false;
				break;
			}
		}
		bool bAddedExact = true;
		for (const TPair<FName, FUISoundTable>& Entry : ToAdd)
		{
			const FUISoundTable* Now = Table->FindRow<FUISoundTable>(Entry.Key, TEXT("AppendUISoundRows"), false);
			if (!Now || !RowsEqual(*Now, Entry.Value))
			{
				bAddedExact = false;
				break;
			}
		}
		const int32 AfterCount = Table->GetRowMap().Num();
		Report->SetNumberField(TEXT("after_count"), AfterCount);
		Report->SetBoolField(TEXT("other_rows_preserved"), bOthersPreserved);
		Report->SetBoolField(TEXT("added_rows_exact"), bAddedExact);
		if (!bOthersPreserved || !bAddedExact || AfterCount != Before.Num() + ToAdd.Num())
		{
			// 메모리에 남기면 다음 저장(남의 저장 포함)에 섞여 나간다
			for (const TPair<FName, FUISoundTable>& Entry : ToAdd)
			{
				Table->RemoveRow(Entry.Key);
			}
			Report->SetStringField(TEXT("error"), TEXT("rows_not_as_expected_not_saved"));
			return;
		}
		if (ToAdd.Num() == 0)
		{
			Report->SetBoolField(TEXT("success"), true);
			Report->SetStringField(TEXT("error"), TEXT(""));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		// 기본값 GError 는 파일 이동 실패(Error Code 32)에서 에디터를 죽인다 ― 경고로 받고 false 를 보고한다
		SaveArgs.Error = GWarn;
		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
		if (!bSaved)
		{
			// 반쯤 들어간 행이 dirty 로 남으면 이 에디터의 Save All 이 검증 안 된 표를 쓴다
			for (const TPair<FName, FUISoundTable>& Entry : ToAdd)
			{
				Table->RemoveRow(Entry.Key);
			}
			Package->SetDirtyFlag(false);
			Report->SetNumberField(TEXT("after_count"), Table->GetRowMap().Num());
			Report->SetBoolField(TEXT("restored_in_memory"), Table->GetRowMap().Num() == Before.Num());
		}
		Report->SetBoolField(TEXT("saved"), bSaved);
		Report->SetBoolField(TEXT("success"), bSaved);
		Report->SetStringField(TEXT("error"), bSaved ? TEXT("") : TEXT("save_failed_rows_removed"));
		UE_LOG(LogTemp, Log, TEXT("[UISoundRowAppend] added=%d skipped=%d rows=%d saved=%d"),
			ToAdd.Num(), Skipped.Num(), AfterCount, bSaved ? 1 : 0);
	}

	static FAutoConsoleCommand AppendRowsCommand(
		TEXT("CGR.Audio.AppendUISoundRows"),
		TEXT("Add new rows from a project-relative CSV (DT_UISounds_Import.csv columns) to DT_UISounds; existing rows are proven unchanged before saving."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&AppendRows));
}

#endif
