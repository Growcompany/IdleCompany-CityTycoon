#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Table/FactoryAppearanceTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CGRFactoryCameraAuthoring
{
	static void AuthorSizeFocus(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() != 1 || !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Usage: CGR.Factory.AuthorSizeFocus <initial-distance-cm>; run in the editor with PIE stopped."));
			return;
		}

		double InitialDistance = 0.0;
		if (!LexTryParseString(InitialDistance, *Arguments[0]) || !FMath::IsFinite(InitialDistance) || InitialDistance <= 0.0)
		{
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Initial distance must be a finite positive number: %s"), *Arguments[0]);
			return;
		}

		UDataTable* Table = LoadObject<UDataTable>(nullptr,
			TEXT("/Game/CompanyGrowth/Table/Factory/DT_FactoryAppearance.DT_FactoryAppearance"));
		if (!Table || Table->GetRowStruct() != FFactoryAppearanceRow::StaticStruct())
		{
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] DT_FactoryAppearance is missing or has the wrong row struct."));
			return;
		}
		UPackage* Package = Table->GetOutermost();
		if (Package->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] DT_FactoryAppearance has unsaved changes; save or discard them first."));
			return;
		}

		const FString CSVPath = FPaths::ProjectDir() / TEXT("DataImport/DT_FactoryAppearance_Import.csv");
		FString OriginalCSV;
		if (!IFileManager::Get().FileExists(*CSVPath) || !FFileHelper::LoadFileToString(OriginalCSV, *CSVPath))
		{
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Cannot read existing CSV: %s"), *CSVPath);
			return;
		}

		const TArray<FName> RowNames = Table->GetRowNames();
		if (RowNames.Num() != 8)
		{
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Expected eight appearance rows; found %d."), RowNames.Num());
			return;
		}

		FFactoryAppearanceRow* Rows[8] = {};
		double Diagonals[8] = {};
		for (const FName RowName : RowNames)
		{
			FFactoryAppearanceRow* Row = Table->FindRow<FFactoryAppearanceRow>(RowName, TEXT("CGR.Factory.AuthorSizeFocus"), false);
			if (!Row || Row->Level < 1 || Row->Level > 8 || RowName != FName(*FString::Printf(TEXT("Level_%02d"), Row->Level))
				|| Rows[Row->Level - 1] || Row->LaborerCapacity <= 0 || Row->Mesh.IsNull()
				|| !FMath::IsFinite(Row->CameraFocusDistanceCm) || Row->CameraFocusDistanceCm <= 0.f)
			{
				UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Invalid appearance row: %s"), *RowName.ToString());
				return;
			}
			UStaticMesh* Mesh = Row->Mesh.LoadSynchronous();
			if (!Mesh)
			{
				UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Mesh failed to load for %s: %s"), *RowName.ToString(), *Row->Mesh.ToString());
				return;
			}
			const FBox Bounds = Mesh->GetBoundingBox();
			const double Diagonal = Bounds.GetSize().Size();
			if (!Bounds.IsValid || !FMath::IsFinite(Diagonal) || Diagonal <= 0.0)
			{
				UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Invalid mesh bounds for %s: %s"), *RowName.ToString(), *Mesh->GetPathName());
				return;
			}
			Rows[Row->Level - 1] = Row;
			Diagonals[Row->Level - 1] = Diagonal;
		}

		if (!Rows[0] || !FMath::IsFinite(Diagonals[0]) || Diagonals[0] <= 0.0)
		{
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Level_01 reference mesh is missing."));
			return;
		}

		float NewDistances[8] = {};
		float OldDistances[8] = {};
		for (int32 Index = 0; Index < 8; ++Index)
		{
			if (!Rows[Index])
			{
				UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Missing factory appearance level %d."), Index + 1);
				return;
			}
			const double Rounded = FMath::RoundToDouble(InitialDistance * Diagonals[Index] / Diagonals[0] / 100.0) * 100.0;
			if (!FMath::IsFinite(Rounded) || Rounded <= 0.0 || Rounded > static_cast<double>(TNumericLimits<float>::Max()))
			{
				UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Invalid computed distance for level %d."), Index + 1);
				return;
			}
			NewDistances[Index] = static_cast<float>(Rounded);
			OldDistances[Index] = Rows[Index]->CameraFocusDistanceCm;
		}

		Table->Modify();
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Rows[Index]->CameraFocusDistanceCm = NewDistances[Index];
		}
		Table->HandleDataTableChanged();
		Table->MarkPackageDirty();

		const FString UpdatedCSV = Table->GetTableAsCSV();
		if (UpdatedCSV.IsEmpty() || !FFileHelper::SaveStringToFile(UpdatedCSV, *CSVPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			if (!FFileHelper::SaveStringToFile(OriginalCSV, *CSVPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] CSV rollback also failed: %s"), *CSVPath);
			}
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Rows[Index]->CameraFocusDistanceCm = OldDistances[Index];
			}
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] CSV save failed: %s"), *CSVPath);
			return;
		}

		const FString PackagePath = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Table, *PackagePath, SaveArgs))
		{
			if (!FFileHelper::SaveStringToFile(OriginalCSV, *CSVPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] Package save failed and CSV rollback also failed: %s"), *CSVPath);
			}
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Rows[Index]->CameraFocusDistanceCm = OldDistances[Index];
			}
			Table->HandleDataTableChanged();
			Table->MarkPackageDirty();
			UE_LOG(LogTemp, Error, TEXT("[FactoryFocus] DataTable package save failed: %s"), *PackagePath);
			return;
		}

		for (int32 Index = 0; Index < 8; ++Index)
		{
			UE_LOG(LogTemp, Display, TEXT("[FactoryFocus] Level_%02d: %.0f cm (mesh diagonal %.1f cm)"),
				Index + 1, NewDistances[Index], Diagonals[Index]);
		}
		UE_LOG(LogTemp, Display, TEXT("[FactoryFocus] Saved DT_FactoryAppearance and %s"), *CSVPath);
	}

	static FAutoConsoleCommand AuthorSizeFocusCommand(
		TEXT("CGR.Factory.AuthorSizeFocus"),
		TEXT("Set all eight factory camera focus distances from current mesh diagonals; save the DataTable and import CSV."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&AuthorSizeFocus));
}

#endif
