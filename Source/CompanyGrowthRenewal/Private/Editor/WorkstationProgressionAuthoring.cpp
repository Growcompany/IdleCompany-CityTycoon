#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Table/WidgetDataTable.h"
#include "UI/Panel/WorkstationPromotionWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace WorkstationProgressionAuthoring
{
	static void RegisterWidget()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[WorkstationAuthoring] Stop PIE before registering the promotion widget"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"));
		UClass* WidgetClass = LoadObject<UClass>(nullptr,
			TEXT("/Game/CompanyGrowth/UI/Panels/OfficeMap/UI_WorkstationPromotion.UI_WorkstationPromotion_C"));
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct()
			|| !WidgetClass || !WidgetClass->IsChildOf(UWorkstationPromotionWidget::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[WorkstationAuthoring] Missing or incompatible table / promotion WBP"));
			return;
		}
		TMap<FName, FWidgetDataTable> Before;
		for (const auto& Entry : Table->GetRowMap())
		{
			Before.Add(Entry.Key, *reinterpret_cast<const FWidgetDataTable*>(Entry.Value));
		}
		const FName RowName(TEXT("WorkstationPromotion"));
		FWidgetDataTable Row;
		Row.WidgetType = EWidgetType::WorkstationPromotion;
		Row.WidgetClass = WidgetClass;
		Table->AddRow(RowName, Row);
		for (const auto& Entry : Before)
		{
			if (Entry.Key == RowName) continue;
			const FWidgetDataTable* After = Table->FindRow<FWidgetDataTable>(Entry.Key, TEXT("WorkstationAuthoring"));
			if (!After || !FWidgetDataTable::StaticStruct()->CompareScriptStruct(&Entry.Value, After, 0))
			{
				UE_LOG(LogTemp, Error, TEXT("[WorkstationAuthoring] Unrelated row changed; table not saved"));
				return;
			}
		}
		UPackage* Package = Table->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, Table, *Filename, SaveArgs);
		const FString Report = FPaths::ProjectSavedDir() / TEXT("DeskProgression/widget_registration.txt");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report), true);
		FFileHelper::SaveStringToFile(bSaved ? TEXT("saved; unrelated rows preserved") : TEXT("save failed"), *Report);
		UE_LOG(LogTemp, Display, TEXT("[WorkstationAuthoring] Registration saved=%d, rows=%d"), bSaved, Table->GetRowMap().Num());
	}

	static FAutoConsoleCommand RegisterCommand(TEXT("CGR.Workstation.RegisterPromotion"),
		TEXT("Register the workstation promotion WBP without replacing unrelated widget rows."),
		FConsoleCommandDelegate::CreateStatic(&RegisterWidget));
}

#endif
