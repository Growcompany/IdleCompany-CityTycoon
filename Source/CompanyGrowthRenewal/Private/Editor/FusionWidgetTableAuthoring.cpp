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
#include "UI/Element/Fusion/FusionBuildingRowWidget.h"
#include "UI/Element/Fusion/FusionCardWidget.h"
#include "UI/Element/Fusion/FusionSlotWidget.h"
#include "UI/Panel/FusionProjectPanelWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"

namespace CGRFusionWidgetTableAuthoring
{
	static bool WriteReport(const TSharedRef<FJsonObject>& Report, const FString& Filename)
	{
		FString Json;
		return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		// 이름이 폐기된 enum도 원시 값으로 비교한다. JSON/문자열 변환을 거치지 않는다.
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterWidgets(const TArray<FString>& Arguments)
	{
		const FString ReportDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("FusionUI"));
		const FString ReportFilename = ReportDirectory / TEXT("widget_native_merge.json");
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), false);
		Report->SetBoolField(TEXT("saved"), false);
		Report->SetBoolField(TEXT("other_rows_preserved"), false);
		Report->SetBoolField(TEXT("target_rows_match"), false);
		Report->SetNumberField(TEXT("before_count"), -1);
		Report->SetNumberField(TEXT("after_count"), -1);
		Report->SetNumberField(TEXT("expected_count"), -1);
		Report->SetStringField(TEXT("error"), TEXT("operation_not_completed"));
		if (!IFileManager::Get().MakeDirectory(*ReportDirectory, true) || !WriteReport(Report, ReportFilename))
		{
			UE_LOG(LogTemp, Error, TEXT("[FusionWidgetTable] Cannot prepare report: %s"), *ReportFilename);
			return;
		}
		ON_SCOPE_EXIT
		{
			if (!WriteReport(Report, ReportFilename))
			{
				UE_LOG(LogTemp, Error, TEXT("[FusionWidgetTable] Report write failed: %s"), *ReportFilename);
			}
		};
		auto Fail = [&Report](const FString& Message)
		{
			Report->SetStringField(TEXT("error"), Message);
			UE_LOG(LogTemp, Error, TEXT("[FusionWidgetTable] %s"), *Message);
		};
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			Fail(TEXT("Requires the editor game thread with PIE/SIE stopped."));
			return;
		}
		if (!Arguments.IsEmpty())
		{
			Fail(TEXT("Usage: CGR.Fusion.RegisterWidgets"));
			return;
		}

		struct FRegistration
		{
			const TCHAR* RowName;
			EWidgetType Type;
			const TCHAR* ClassPath;
			UClass* BaseClass;
		};
		const FRegistration Registrations[] = {
			{TEXT("FusionProjectPanel"), EWidgetType::FusionProjectPanel,
				TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_FusionProjectPanel.UI_FusionProjectPanel_C"), UFusionProjectPanelWidget::StaticClass()},
			{TEXT("FusionCard"), EWidgetType::FusionCard,
				TEXT("/Game/CompanyGrowth/UI/Elements/Fusion/UIE_FusionCard.UIE_FusionCard_C"), UFusionCardWidget::StaticClass()},
			{TEXT("FusionSlot"), EWidgetType::FusionSlot,
				TEXT("/Game/CompanyGrowth/UI/Elements/Fusion/UIE_FusionSlot.UIE_FusionSlot_C"), UFusionSlotWidget::StaticClass()},
			{TEXT("FusionBuildingRow"), EWidgetType::FusionBuildingRow,
				TEXT("/Game/CompanyGrowth/UI/Elements/Fusion/UIE_FusionBuildingRow.UIE_FusionBuildingRow_C"), UFusionBuildingRowWidget::StaticClass()}
		};
		TMap<FName, FWidgetDataTable> DesiredRows;
		for (const FRegistration& Registration : Registrations)
		{
			if (StaticEnum<EWidgetType>()->GetNameStringByValue(static_cast<int64>(Registration.Type)) != Registration.RowName)
			{
				Fail(FString::Printf(TEXT("Widget enum unavailable: %s. No rows changed or saved."), Registration.RowName));
				return;
			}
			UClass* WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(Registration.ClassPath)).LoadSynchronous();
			if (!WidgetClass || !WidgetClass->IsChildOf(Registration.BaseClass) || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
			{
				Fail(FString::Printf(TEXT("Widget class unavailable or incompatible: %s. No rows changed or saved."), Registration.ClassPath));
				return;
			}
			FWidgetDataTable NewRow;
			NewRow.WidgetType = Registration.Type;
			NewRow.WidgetClass = WidgetClass;
			DesiredRows.Add(FName(Registration.RowName), NewRow);
		}

		TStrongObjectPtr<UDataTable> Table(TSoftObjectPtr<UDataTable>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous());
		if (!Table.IsValid() || Table->GetRowStruct() != FWidgetDataTable::StaticStruct())
		{
			Fail(TEXT("DT_WidgetClass failed to load or has a different row struct."));
			return;
		}
		TMap<FName, FWidgetDataTable> BeforeRows;
		TMap<FName, TArray<uint8>> OtherRowBytes;
		const int32 RowSize = FWidgetDataTable::StaticStruct()->GetStructureSize();
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value)
			{
				Fail(TEXT("DT_WidgetClass contains a null row."));
				return;
			}
			const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			BeforeRows.Add(Pair.Key, Existing);
			for (const FRegistration& Registration : Registrations)
			{
				if (Existing.WidgetType == Registration.Type && Pair.Key != FName(Registration.RowName))
				{
					Fail(FString::Printf(TEXT("Duplicate Fusion WidgetType in unrelated row %s; no rows changed or saved."), *Pair.Key.ToString()));
					return;
				}
			}
			if (const FWidgetDataTable* Desired = DesiredRows.Find(Pair.Key))
			{
				if (Existing.WidgetType != Desired->WidgetType)
				{
					Fail(FString::Printf(TEXT("Target row %s belongs to another WidgetType; no rows changed or saved."), *Pair.Key.ToString()));
					return;
				}
			}
			else
			{
				// 복사 생성자의 패딩 처리에 의존하지 않고 원래 행의 바이트를 보관한다.
				OtherRowBytes.FindOrAdd(Pair.Key).Append(Pair.Value, RowSize);
			}
		}
		int32 ExpectedCount = BeforeRows.Num();
		for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
		{
			if (!BeforeRows.Contains(Pair.Key)) ++ExpectedCount;
		}
		Report->SetNumberField(TEXT("before_count"), BeforeRows.Num());
		Report->SetNumberField(TEXT("after_count"), BeforeRows.Num());
		Report->SetNumberField(TEXT("expected_count"), ExpectedCount);

		auto RowsMatch = [&Table](const TMap<FName, FWidgetDataTable>& ExpectedRows)
		{
			for (const TPair<FName, FWidgetDataTable>& Pair : ExpectedRows)
			{
				const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("Fusion native rows"), false);
				if (!Current || !RowsEqual(Pair.Value, *Current)) return false;
			}
			return true;
		};
		auto OtherRowsPreserved = [&Table, &OtherRowBytes, &BeforeRows]()
		{
			for (const TPair<FName, TArray<uint8>>& Pair : OtherRowBytes)
			{
				const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("Fusion unchanged rows"), false);
				if (!Current || !RowsEqual(BeforeRows.FindChecked(Pair.Key), *Current)
					|| FMemory::Memcmp(Pair.Value.GetData(), Current, Pair.Value.Num()) != 0) return false;
			}
			return true;
		};

		UPackage* TablePackage = Table->GetOutermost();
		const bool bWasDirty = TablePackage->IsDirty();
		bool bSaved = false;
		Table->Modify(false);
		ON_SCOPE_EXIT
		{
			if (!bSaved)
			{
				// 실패한 호출의 네 대상 행만 복구한다. 무관한 행에는 쓰지 않는다.
				for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
				{
					if (const FWidgetDataTable* Previous = BeforeRows.Find(Pair.Key)) Table->AddRow(Pair.Key, *Previous);
					else Table->RemoveRow(Pair.Key);
				}
				const bool bOtherRowsRestored = OtherRowsPreserved();
				const bool bRestored = Table->GetRowMap().Num() == BeforeRows.Num() && RowsMatch(BeforeRows) && bOtherRowsRestored;
				Report->SetBoolField(TEXT("rollback_restored"), bRestored);
				Report->SetBoolField(TEXT("other_rows_preserved"), bOtherRowsRestored);
				Report->SetBoolField(TEXT("target_rows_match"), RowsMatch(DesiredRows));
				Report->SetNumberField(TEXT("after_count"), Table->GetRowMap().Num());
				TablePackage->SetDirtyFlag(bRestored ? bWasDirty : true);
			}
		};
		for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
		{
			Table->AddRow(Pair.Key, Pair.Value);
		}
		const bool bOtherRowsPreserved = OtherRowsPreserved();
		const bool bTargetsMatch = RowsMatch(DesiredRows);
		const int32 AfterCount = Table->GetRowMap().Num();
		Report->SetNumberField(TEXT("after_count"), AfterCount);
		Report->SetBoolField(TEXT("other_rows_preserved"), bOtherRowsPreserved);
		Report->SetBoolField(TEXT("target_rows_match"), bTargetsMatch);
		if (AfterCount != ExpectedCount || !bOtherRowsPreserved || !bTargetsMatch)
		{
			Fail(TEXT("Exact row/count/byte verification failed; no package saved."));
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
		Report->SetStringField(TEXT("error"), TEXT(""));
		Report->SetBoolField(TEXT("success"), true);
		UE_LOG(LogTemp, Display, TEXT("[FusionWidgetTable] SUCCESS before=%d after=%d; all unrelated row bytes preserved."),
			BeforeRows.Num(), AfterCount);
	}

	static FAutoConsoleCommand RegisterWidgetsCommand(TEXT("CGR.Fusion.RegisterWidgets"),
		TEXT("Register only the four Fusion widget rows in DT_WidgetClass, preserving every unrelated native row."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidgets));
}

#endif
