// 출장 드랍 칩 행 하나를 DT_WidgetClass 에 네이티브로 등록한다 (에디터 전용 콘솔 명령).
//
// 왜 파이썬이 아닌가: 이 표의 JSON 왕복(export -> merge -> fill)은 **전체 교체**라
// 이름이 폐기된 raw enum 행이 0 으로 정규화돼 죽는다(2026-09-27 LootBoxLayerWidget 실사고,
// 백업에서 복구함). 그래서 대상 행만 `AddRow` 하고 나머지는 구조체 비교로 불변을 증명한다.
// 선례와 계약은 `FusionWidgetTableAuthoring.cpp` 를 따른다.
//
//   CGR.Trip.RegisterWidgets
//
// 보고서 = Saved/BusinessTripUI/widget_native_merge.json

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
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CGRTripWidgetTableAuthoring
{
	static const TCHAR* TableAssetPath = TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass");
	static const TCHAR* ChipRowName = TEXT("TripRewardChip");
	static const TCHAR* ChipClassPath =
		TEXT("/Game/CompanyGrowth/UI/Elements/LiveOps/UIE_TripRewardChip.UIE_TripRewardChip_C");

	static bool WriteReport(const TSharedRef<FJsonObject>& Report, const FString& Filename)
	{
		FString Json;
		return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	/** 이름이 폐기된 enum 도 원시 값으로 비교한다 — 문자열 변환을 거치지 않는다. */
	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterWidgets(const TArray<FString>&)
	{
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("BusinessTripUI"));
		const FString ReportFilename = Directory / TEXT("widget_native_merge.json");
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), false);
		Report->SetBoolField(TEXT("saved"), false);
		Report->SetBoolField(TEXT("other_rows_preserved"), false);
		Report->SetStringField(TEXT("error"), TEXT("operation_not_completed"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		ON_SCOPE_EXIT { WriteReport(Report, ReportFilename); };

		if (!GEditor)
		{
			Report->SetStringField(TEXT("error"), TEXT("editor_only"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr, TableAssetPath);
		if (!Table)
		{
			Report->SetStringField(TEXT("error"), TEXT("table_not_found"));
			return;
		}
		UClass* ChipClass = LoadObject<UClass>(nullptr, ChipClassPath);
		if (!ChipClass)
		{
			// WBP 를 먼저 만들어야 한다 — 클래스가 없는 채로 행을 넣으면 조용히 빈 드랍이 된다
			Report->SetStringField(TEXT("error"), TEXT("chip_widget_blueprint_missing"));
			return;
		}

		TMap<FName, FWidgetDataTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<FWidgetDataTable*>(Pair.Value));
		}
		Report->SetNumberField(TEXT("before_count"), Before.Num());

		const FName RowKey(ChipRowName);
		FWidgetDataTable Row = Before.Contains(RowKey) ? Before[RowKey] : FWidgetDataTable();
		Row.WidgetType = EWidgetType::TripRewardChip;
		Row.WidgetClass = ChipClass;
		Table->AddRow(RowKey, Row);

		// 대상 외 행이 한 줄도 안 움직였는지 — 이 표가 죽는 사고는 전부 여기서 났다
		bool bOthersPreserved = true;
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			if (Pair.Key == RowKey) continue;
			const FWidgetDataTable* Now = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("TripRegister"), false);
			if (!Now || !RowsEqual(*Now, Pair.Value)) { bOthersPreserved = false; break; }
		}
		Report->SetNumberField(TEXT("after_count"), Table->GetRowMap().Num());
		Report->SetBoolField(TEXT("other_rows_preserved"), bOthersPreserved);
		if (!bOthersPreserved)
		{
			Report->SetStringField(TEXT("error"), TEXT("other_rows_changed_not_saved"));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
		Report->SetBoolField(TEXT("saved"), bSaved);
		Report->SetBoolField(TEXT("success"), bSaved);
		Report->SetStringField(TEXT("error"), bSaved ? TEXT("") : TEXT("save_failed"));
		UE_LOG(LogTemp, Log, TEXT("[TripWidgetTable] %s rows=%d saved=%d"),
			ChipRowName, Table->GetRowMap().Num(), bSaved ? 1 : 0);
	}

	static FAutoConsoleCommand RegisterWidgetsCommand(
		TEXT("CGR.Trip.RegisterWidgets"),
		TEXT("Add the TripRewardChip row to DT_WidgetClass natively; other rows are proven unchanged before saving."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidgets));
}

#endif
