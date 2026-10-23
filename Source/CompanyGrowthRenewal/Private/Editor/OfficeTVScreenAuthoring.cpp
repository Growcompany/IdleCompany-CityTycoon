#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Table/WidgetDataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace OfficeTVScreenAuthoring
{
	static void RegisterWidget(const TArray<FString>&)
	{
		if (!GEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeTV] Editor is required"));
			return;
		}

		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"));
		UClass* ScreenClass = LoadObject<UClass>(nullptr,
			TEXT("/Game/CompanyGrowth/UI/Elements/Office/UIE_OfficeTVScreen.UIE_OfficeTVScreen_C"));
		if (!Table || !ScreenClass || !ScreenClass->IsChildOf(UUserWidget::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeTV] DT_WidgetClass or UIE_OfficeTVScreen is missing"));
			return;
		}

		const FName RowKey(TEXT("OfficeTVScreen"));
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const FWidgetDataTable* Existing = reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			if (Pair.Key != RowKey && Existing->WidgetType == EWidgetType::OfficeTVScreen)
			{
				UE_LOG(LogTemp, Error, TEXT("[OfficeTV] WidgetType already mapped by another row: %s"), *Pair.Key.ToString());
				return;
			}
		}

		TMap<FName, FWidgetDataTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<const FWidgetDataTable*>(Pair.Value));
		}
		FWidgetDataTable Desired = Before.Contains(RowKey) ? Before[RowKey] : FWidgetDataTable();
		Desired.WidgetType = EWidgetType::OfficeTVScreen;
		Desired.WidgetClass = ScreenClass;
		if (const FWidgetDataTable* Existing = Table->FindRow<FWidgetDataTable>(RowKey, TEXT("OfficeTV"), false))
		{
			if (FWidgetDataTable::StaticStruct()->CompareScriptStruct(Existing, &Desired, 0))
			{
				UE_LOG(LogTemp, Log, TEXT("[OfficeTV] Widget row is already current"));
				return;
			}
		}
		Table->AddRow(RowKey, Desired);
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			if (Pair.Key == RowKey)
			{
				continue;
			}
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("OfficeTV"), false);
			if (!Current || !FWidgetDataTable::StaticStruct()->CompareScriptStruct(Current, &Pair.Value, 0))
			{
				UE_LOG(LogTemp, Error, TEXT("[OfficeTV] Unrelated row changed; table was not saved: %s"), *Pair.Key.ToString());
				return;
			}
		}
		if (Table->GetRowMap().Num() != Before.Num() + (Before.Contains(RowKey) ? 0 : 1))
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeTV] Unexpected row count; table was not saved"));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs);
		if (bSaved)
		{
			UE_LOG(LogTemp, Log, TEXT("[OfficeTV] Registered OfficeTVScreen; rows=%d"), Table->GetRowMap().Num());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeTV] Failed to save DT_WidgetClass"));
		}
	}

	static FAutoConsoleCommand RegisterWidgetCommand(
		TEXT("CGR.OfficeTV.RegisterWidget"),
		TEXT("Register OfficeTVScreen in DT_WidgetClass while preserving all other rows."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidget));
}

#endif
