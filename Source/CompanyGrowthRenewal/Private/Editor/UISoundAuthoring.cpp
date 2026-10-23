#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "ScopedTransaction.h"
#include "Sound/SoundBase.h"
#include "Table/SoundTable.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

namespace CGRUISoundAuthoring
{
	static void SetUISound(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() != 2 || !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[UISound] Usage: CGR.Audio.SetUISound <existing row name> <SoundBase object path>; run in the editor with PIE stopped."));
			return;
		}

		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/Audio/DT_UISounds.DT_UISounds"));
		if (!Table || Table->GetRowStruct() != FUISoundTable::StaticStruct())
		{
			UE_LOG(LogTemp, Error, TEXT("[UISound] DT_UISounds is missing or has the wrong row struct."));
			return;
		}
		if (Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[UISound] DT_UISounds has unsaved changes; save or discard them first."));
			return;
		}

		const FName RowName(*Arguments[0]);
		FUISoundTable* Row = Table->FindRow<FUISoundTable>(RowName, TEXT("CGR.Audio.SetUISound"), false);
		if (!Row)
		{
			UE_LOG(LogTemp, Error, TEXT("[UISound] Existing row not found: %s"), *Arguments[0]);
			return;
		}

		const FSoftObjectPath RequestedPath(Arguments[1]);
		USoundBase* Sound = RequestedPath.IsValid() ? Cast<USoundBase>(RequestedPath.TryLoad()) : nullptr;
		if (!Sound)
		{
			UE_LOG(LogTemp, Error, TEXT("[UISound] SoundBase object could not be loaded: %s"), *Arguments[1]);
			return;
		}

		const FSoftObjectPath SoundPath(Sound->GetPathName());
		if (Row->Sound.ToSoftObjectPath() == SoundPath)
		{
			UE_LOG(LogTemp, Display, TEXT("[UISound] %s already uses %s; no change."), *RowName.ToString(), *SoundPath.ToString());
			return;
		}

		const FScopedTransaction Transaction(NSLOCTEXT("CGRUISoundAuthoring", "SetUISound", "Set UI sound DataTable row"));
		Table->Modify();
		Row->Sound = TSoftObjectPtr<USoundBase>(Sound);
		Table->HandleDataTableChanged(RowName);
		Table->MarkPackageDirty();
		UE_LOG(LogTemp, Display, TEXT("[UISound] %s now uses %s; save DT_UISounds after verification."), *RowName.ToString(), *SoundPath.ToString());
	}

	static FAutoConsoleCommand SetUISoundCommand(
		TEXT("CGR.Audio.SetUISound"),
		TEXT("Set the SoundBase reference on one existing DT_UISounds row without saving the table."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetUISound));
}

#endif
