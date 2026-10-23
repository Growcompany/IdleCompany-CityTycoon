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
#include "UI/Element/Common/ModalFrameWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"

namespace CGRModalStyleAuthoring
{
	static bool WriteReport(const TSharedRef<FJsonObject>& Report, const FString& Filename)
	{
		FString Json;
		return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		// 이름이 폐기된 enum 값도 문자열 변환 없이 그대로 보존하고 비교한다.
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterWidgets(const TArray<FString>& Arguments)
	{
		const FString ReportDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ModalStyle"));
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
			UE_LOG(LogTemp, Error, TEXT("[ModalStyle] Cannot prepare report: %s"), *ReportFilename);
			return;
		}
		ON_SCOPE_EXIT
		{
			if (!WriteReport(Report, ReportFilename))
			{
				UE_LOG(LogTemp, Error, TEXT("[ModalStyle] Report write failed: %s"), *ReportFilename);
			}
		};
		auto Fail = [&Report](const FString& Message)
		{
			Report->SetStringField(TEXT("error"), Message);
			UE_LOG(LogTemp, Error, TEXT("[ModalStyle] %s"), *Message);
		};
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			Fail(TEXT("Requires the editor game thread with PIE/SIE stopped."));
			return;
		}
		if (!Arguments.IsEmpty())
		{
			Fail(TEXT("Usage: CGR.ModalStyle.RegisterWidgets"));
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
			{TEXT("PanelHeader"), EWidgetType::PanelHeader,
				TEXT("/Game/CompanyGrowth/UI/Elements/Common/UIE_PanelHeader.UIE_PanelHeader_C"), UPanelHeaderWidget::StaticClass()},
			{TEXT("ModalFrame"), EWidgetType::ModalFrame,
				TEXT("/Game/CompanyGrowth/UI/Elements/Common/UIE_ModalFrame.UIE_ModalFrame_C"), UModalFrameWidget::StaticClass()}
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
					Fail(FString::Printf(TEXT("Duplicate common WidgetType in unrelated row %s; no rows changed or saved."), *Pair.Key.ToString()));
					return;
				}
			}
			if (const FWidgetDataTable* Desired = DesiredRows.Find(Pair.Key); Desired && !RowsEqual(Existing, *Desired))
			{
				Fail(FString::Printf(TEXT("Target row name %s is occupied by a different registration; no rows changed or saved."), *Pair.Key.ToString()));
				return;
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
				// 실패한 호출의 두 대상 행만 복구한다. 전체 테이블 재임포트는 하지 않는다.
				for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
				{
					if (const FWidgetDataTable* Previous = BeforeRows.Find(Pair.Key)) Table->AddRow(Pair.Key, *Previous);
					else Table->RemoveRow(Pair.Key);
				}
				bool bRestored = Table->GetRowMap().Num() == BeforeRows.Num();
				for (const TPair<FName, FWidgetDataTable>& Pair : BeforeRows)
				{
					const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("ModalStyle rollback"), false);
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
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("ModalStyle unchanged rows"), false);
			bOtherRowsPreserved &= Current && RowsEqual(Pair.Value, *Current);
		}
		bool bTargetsMatch = true;
		for (const TPair<FName, FWidgetDataTable>& Pair : DesiredRows)
		{
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("ModalStyle registered rows"), false);
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
		Report->SetStringField(TEXT("error"), TEXT(""));
		Report->SetBoolField(TEXT("success"), true);
		UE_LOG(LogTemp, Display, TEXT("[ModalStyle] Widget registration SUCCESS before=%d after=%d; unrelated rows preserved."),
			BeforeRows.Num(), AfterCount);
	}

	static FAutoConsoleCommand RegisterWidgetsCommand(TEXT("CGR.ModalStyle.RegisterWidgets"),
		TEXT("Register only PanelHeader and ModalFrame in DT_WidgetClass and preserve every other native row."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidgets));
}

#endif
