// 공개 카드 행 하나를 DT_WidgetClass 에 네이티브로 더한다 (에디터 전용 콘솔 명령).
//
// 왜 파이썬·CSV 가 아닌가: 이 표는 여러 세션의 미커밋 행이 섞인 공유 바이너리라 JSON 왕복·CSV 재임포트(전체 교체)는
// 남의 행과 이름이 폐기된 raw enum 행을 지운다. 대상 행만 AddRow 하고 나머지는 구조체 비교로 불변을 증명한다
// (TripWidgetTableAuthoring 과 같은 계약 + 남의 미저장 편집이 있으면 거절, 저장 실패면 행을 되돌림).
//
//   CGR.GachaReveal.RegisterCardWidget
//
// 보고서 = Saved/GachaBoxReveal/card_widget_row.json

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
#include "Table/WidgetDataTable.h"
#include "UI/Element/Gacha/GachaRevealCardWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CGRGachaRevealCardWidgetTable
{
	static const TCHAR* TableAssetPath = TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass");
	static const TCHAR* CardRowName = TEXT("GachaRevealCard");
	static const TCHAR* CardClassPath =
		TEXT("/Game/CompanyGrowth/UI/Elements/Gacha/UIE_GachaRevealCard.UIE_GachaRevealCard_C");

	static bool WriteReport(const TSharedRef<FJsonObject>& Report, const FString& Filename)
	{
		FString Json;
		return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	// 이름이 폐기된 enum 도 원시 값으로 비교한다 ― 문자열 변환을 거치지 않는다
	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterCardWidget(const TArray<FString>&)
	{
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GachaBoxReveal"));
		const FString ReportFilename = Directory / TEXT("card_widget_row.json");
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), false);
		Report->SetBoolField(TEXT("saved"), false);
		Report->SetBoolField(TEXT("other_rows_preserved"), false);
		Report->SetStringField(TEXT("error"), TEXT("operation_not_completed"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		ON_SCOPE_EXIT { WriteReport(Report, ReportFilename); };

		if (!GEditor || GEditor->PlayWorld)
		{
			Report->SetStringField(TEXT("error"), TEXT("editor_only_pie_stopped"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr, TableAssetPath);
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct())
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
		UClass* CardClass = LoadObject<UClass>(nullptr, CardClassPath);
		if (!CardClass || !CardClass->IsChildOf(UGachaRevealCardWidget::StaticClass()))
		{
			// WBP 를 먼저 저작해야 한다 ― 클래스 없는 행은 공개 화면에서 조용히 빈 카드가 된다
			Report->SetStringField(TEXT("error"), CardClass ? TEXT("card_class_wrong_parent") : TEXT("card_widget_blueprint_missing"));
			return;
		}

		TMap<FName, FWidgetDataTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<const FWidgetDataTable*>(Pair.Value));
		}
		Report->SetNumberField(TEXT("before_count"), Before.Num());

		const FName RowKey(CardRowName);
		// 같은 WidgetType 을 다른 행이 이미 가지면 TableManager 캐시에서 한쪽이 조용히 이긴다
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			if (Pair.Key != RowKey && Pair.Value.WidgetType == EWidgetType::GachaRevealCard)
			{
				Report->SetStringField(TEXT("error"), TEXT("widget_type_already_used_by:") + Pair.Key.ToString());
				return;
			}
		}

		FWidgetDataTable Row;
		Row.WidgetType = EWidgetType::GachaRevealCard;
		Row.WidgetClass = CardClass;
		if (const FWidgetDataTable* Existing = Before.Find(RowKey))
		{
			if (RowsEqual(*Existing, Row))
			{
				Report->SetBoolField(TEXT("success"), true);
				Report->SetBoolField(TEXT("already_registered"), true);
				Report->SetBoolField(TEXT("other_rows_preserved"), true);
				Report->SetNumberField(TEXT("after_count"), Before.Num());
				Report->SetStringField(TEXT("error"), TEXT(""));
				return;
			}
			Report->SetStringField(TEXT("error"), TEXT("row_exists_with_other_values"));
			return;
		}
		Table->AddRow(RowKey, Row);

		bool bOthersPreserved = true;
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			const FWidgetDataTable* Now = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("GachaRevealCardRegister"), false);
			if (!Now || !RowsEqual(*Now, Pair.Value))
			{
				bOthersPreserved = false;
				break;
			}
		}
		const FWidgetDataTable* Added = Table->FindRow<FWidgetDataTable>(RowKey, TEXT("GachaRevealCardRegister"), false);
		const bool bAddedExact = Added && RowsEqual(*Added, Row);
		const int32 AfterCount = Table->GetRowMap().Num();
		Report->SetNumberField(TEXT("after_count"), AfterCount);
		Report->SetBoolField(TEXT("other_rows_preserved"), bOthersPreserved);
		Report->SetBoolField(TEXT("added_row_exact"), bAddedExact);
		if (!bOthersPreserved || !bAddedExact || AfterCount != Before.Num() + 1)
		{
			// 메모리에 남기면 다음 저장(남의 저장 포함)에 섞여 나간다
			Table->RemoveRow(RowKey);
			Report->SetStringField(TEXT("error"), TEXT("rows_not_as_expected_not_saved"));
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
			Table->RemoveRow(RowKey);
			Package->SetDirtyFlag(false);
			Report->SetNumberField(TEXT("after_count"), Table->GetRowMap().Num());
		}
		Report->SetBoolField(TEXT("saved"), bSaved);
		Report->SetBoolField(TEXT("success"), bSaved);
		Report->SetStringField(TEXT("error"), bSaved ? TEXT("") : TEXT("save_failed_row_removed"));
		UE_LOG(LogTemp, Log, TEXT("[GachaRevealCardWidgetTable] %s rows=%d saved=%d"),
			CardRowName, Table->GetRowMap().Num(), bSaved ? 1 : 0);
	}

	static FAutoConsoleCommand RegisterCardWidgetCommand(
		TEXT("CGR.GachaReveal.RegisterCardWidget"),
		TEXT("Add the GachaRevealCard row to DT_WidgetClass natively; other rows are proven unchanged before saving."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterCardWidget));
}

#endif
