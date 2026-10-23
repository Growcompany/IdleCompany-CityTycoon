// 장비 뽑기 전용 창 행을 DT_WidgetClass 에 넣고 구 채용 패널 행을 뺀다 (에디터 전용 콘솔 명령).
//
// 왜 파이썬이 아닌가: 이 표의 JSON 왕복은 전체 교체라 이름이 폐기된 raw enum 행이 0 으로 정규화돼 죽는다.
// 대상 두 행만 AddRow/RemoveRow 하고 나머지는 구조체 비교로 불변을 증명한다 (TripWidgetTableAuthoring 과 같은 계약).
//
//   CGR.EquipmentGacha.RegisterWidgets
//
// 보고서 = Saved/EquipmentGachaPanel/widget_native_merge.json

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

namespace CGREquipmentGachaWidgetTableAuthoring
{
	static const TCHAR* TableAssetPath = TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass");
	static const TCHAR* PanelRowName = TEXT("EquipmentGachaPanelWidget");
	static const TCHAR* PanelClassPath =
		TEXT("/Game/CompanyGrowth/UI/Panels/OfficeMap/UI_EquipmentGachaPanel.UI_EquipmentGachaPanel_C");
	static const TCHAR* RetiredRowName = TEXT("OfficeRecruitmentPanelWidget");
	static const TCHAR* RetiredClassMarker = TEXT("UI_OfficeRecuritmentLayer");

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
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("EquipmentGachaPanel"));
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
		UClass* PanelClass = LoadObject<UClass>(nullptr, PanelClassPath);
		if (!PanelClass)
		{
			// WBP 를 먼저 저장해야 한다 — 클래스 없이 행을 넣으면 [장비] 가 조용히 아무것도 안 연다
			Report->SetStringField(TEXT("error"), TEXT("panel_widget_blueprint_missing"));
			return;
		}

		TMap<FName, FWidgetDataTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<FWidgetDataTable*>(Pair.Value));
		}
		Report->SetNumberField(TEXT("before_count"), Before.Num());

		const FName RetiredKey(RetiredRowName);
		if (const FWidgetDataTable* Retired = Before.Find(RetiredKey))
		{
			// 이름만 같은 남의 행을 지우지 않게 실제 가리키는 WBP 까지 확인한다
			const FString RetiredPath = Retired->WidgetClass ? Retired->WidgetClass->GetPathName() : FString();
			Report->SetStringField(TEXT("retired_row_class"), RetiredPath);
			if (!RetiredPath.Contains(RetiredClassMarker))
			{
				Report->SetStringField(TEXT("error"), TEXT("retired_row_points_elsewhere"));
				return;
			}
			Table->RemoveRow(RetiredKey);
		}
		Report->SetBoolField(TEXT("retired_row_removed"), !Table->GetRowMap().Contains(RetiredKey));

		const FName PanelKey(PanelRowName);
		FWidgetDataTable Row = Before.Contains(PanelKey) ? Before[PanelKey] : FWidgetDataTable();
		Row.WidgetType = EWidgetType::EquipmentGachaPanel;
		Row.WidgetClass = PanelClass;
		Table->AddRow(PanelKey, Row);

		// 대상 외 행이 한 줄도 안 움직였는지 — 이 표가 죽는 사고는 전부 여기서 났다
		bool bOthersPreserved = true;
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			if (Pair.Key == PanelKey || Pair.Key == RetiredKey) continue;
			const FWidgetDataTable* Now = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("EquipmentGachaRegister"), false);
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
		UE_LOG(LogTemp, Log, TEXT("[EquipmentGachaWidgetTable] rows=%d saved=%d"), Table->GetRowMap().Num(), bSaved ? 1 : 0);
	}

	static FAutoConsoleCommand RegisterWidgetsCommand(
		TEXT("CGR.EquipmentGacha.RegisterWidgets"),
		TEXT("Swap the retired OfficeRecruitmentPanel row for the EquipmentGachaPanel row in DT_WidgetClass; other rows are proven unchanged before saving."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidgets));
}

#endif
