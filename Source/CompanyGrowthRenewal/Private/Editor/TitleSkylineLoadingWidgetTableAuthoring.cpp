#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Table/WidgetDataTable.h"
#include "UI/Panel/TitleSkylineLoadingWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CGRTitleSkylineWidgetTableAuthoring
{
	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterWidget(const TArray<FString>& Arguments)
	{
		const bool bActivate = Arguments.Num() == 1 && Arguments[0].Equals(TEXT("activate"), ESearchCase::IgnoreCase);
		if ((!Arguments.IsEmpty() && !bActivate) || !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] Run CGR.TitleSkyline.RegisterWidget [activate] in the editor with PIE stopped."));
			return;
		}

		const FName RowName(TEXT("TitleSkylineLoading"));
		if (StaticEnum<EWidgetType>()->GetNameStringByValue(static_cast<int64>(EWidgetType::TitleSkylineLoading)) != RowName.ToString())
		{
			UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] TitleSkylineLoading enum is unavailable."));
			return;
		}

		UClass* WidgetClass = LoadObject<UClass>(nullptr,
			TEXT("/Game/CompanyGrowth/UI/Panels/System/UI_TitleSkylineLoadingWidget.UI_TitleSkylineLoadingWidget_C"));
		if (!WidgetClass || !WidgetClass->IsChildOf(UTitleSkylineLoadingWidget::StaticClass())
			|| WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] UI_TitleSkylineLoadingWidget class is missing or incompatible."));
			return;
		}

		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"));
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] DT_WidgetClass is unavailable, incompatible, or dirty."));
			return;
		}

		TMap<FName, FWidgetDataTable> BeforeRows;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value)
			{
				UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] DT_WidgetClass contains a null row."));
				return;
			}
			const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			if (Pair.Key != RowName && Existing.WidgetType == EWidgetType::TitleSkylineLoading)
			{
				UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] Another row already uses TitleSkylineLoading: %s"), *Pair.Key.ToString());
				return;
			}
			BeforeRows.Add(Pair.Key, Existing);
		}

		FWidgetDataTable Desired;
		Desired.WidgetType = EWidgetType::TitleSkylineLoading;
		Desired.WidgetClass = WidgetClass;
		const bool bAddPreview = !BeforeRows.Contains(RowName);
		if (const FWidgetDataTable* Existing = BeforeRows.Find(RowName))
		{
			if (!RowsEqual(*Existing, Desired))
			{
				UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] TitleSkylineLoading row has a different registration."));
				return;
			}
		}
		FName LoadingRowName = NAME_None;
		for (const TPair<FName, FWidgetDataTable>& Pair : BeforeRows)
		{
			if (Pair.Value.WidgetType == EWidgetType::Loading)
			{
				if (!LoadingRowName.IsNone())
				{
					UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] Multiple rows use the Loading WidgetType."));
					return;
				}
				LoadingRowName = Pair.Key;
			}
		}
		const FWidgetDataTable* LoadingBefore = BeforeRows.Find(LoadingRowName);
		if (bActivate && !LoadingBefore)
		{
			UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] No row uses the Loading WidgetType."));
			return;
		}
		const bool bChangeLoading = bActivate && LoadingBefore->WidgetClass != WidgetClass;
		if (!bAddPreview && !bChangeLoading)
		{
			UE_LOG(LogTemp, Display, TEXT("[TitleSkyline] Registration already matches (active=%d)."), bActivate ? 1 : 0);
			return;
		}

		Table->Modify(false);
		if (bAddPreview)
		{
			Table->AddRow(RowName, Desired);
		}
		if (bChangeLoading)
		{
			FWidgetDataTable Activated = *LoadingBefore;
			Activated.WidgetClass = WidgetClass;
			Table->AddRow(LoadingRowName, Activated);
		}
		const auto Rollback = [&]()
		{
			if (bAddPreview)
			{
				Table->RemoveRow(RowName);
			}
			if (bChangeLoading)
			{
				Table->AddRow(LoadingRowName, *LoadingBefore);
			}
			Table->GetOutermost()->SetDirtyFlag(false);
		};
		bool bPreserved = Table->GetRowMap().Num() == BeforeRows.Num() + (bAddPreview ? 1 : 0);
		for (const TPair<FName, FWidgetDataTable>& Pair : BeforeRows)
		{
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("TitleSkyline unchanged rows"), false);
			if (Pair.Key == LoadingRowName && bChangeLoading)
			{
				FWidgetDataTable Expected = Pair.Value;
				Expected.WidgetClass = WidgetClass;
				bPreserved &= Current && RowsEqual(Expected, *Current);
			}
			else
			{
				bPreserved &= Current && RowsEqual(Pair.Value, *Current);
			}
		}
		const FWidgetDataTable* Added = Table->FindRow<FWidgetDataTable>(RowName, TEXT("TitleSkyline target row"), false);
		bPreserved &= Added && RowsEqual(Desired, *Added);
		if (!bPreserved)
		{
			Rollback();
			UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] Native row verification failed; table not saved."));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		Table->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
		{
			Rollback();
			UE_LOG(LogTemp, Error, TEXT("[TitleSkyline] DT_WidgetClass save failed."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[TitleSkyline] Saved registration (active=%d); %d prior rows verified."), bActivate ? 1 : 0, BeforeRows.Num());
	}

	static FAutoConsoleCommand RegisterCommand(TEXT("CGR.TitleSkyline.RegisterWidget"),
		TEXT("Register TitleSkylineLoading; optional 'activate' also points Loading to the skyline class."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidget));
}

#endif
