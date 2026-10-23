#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Table/BuildableCardTable.h"
#include "Table/MissionTable.h"
#include "Table/ResourceInfo.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace StartupEconomyAuthoring
{
	static const TCHAR* ResourcePath = TEXT("/Game/CompanyGrowth/Table/DT_Resource.DT_Resource");
	static const TCHAR* MissionPath = TEXT("/Game/CompanyGrowth/Table/Mission/DT_Mission.DT_Mission");
	static const TCHAR* BuildablePath = TEXT("/Game/CompanyGrowth/Table/Building/BuildableTable.BuildableTable");
	static const FName MissionNames[] = {
		TEXT("M1_CollectBricks"), TEXT("M2_UpgradeFactory"), TEXT("M3_BuildFirstCompany"), TEXT("M33_MenuTour")
	};
	static const FName LinkNames[] = {
		TEXT("M21_PlacePainting"), TEXT("M26_LaunchSecond"), TEXT("M22_LaunchGame2")
	};

	static FText TargetText(const FText& Source, const FString& TextNamespace, FName Row, const FString& Field)
	{
		return Source.IsEmpty() ? FText::GetEmpty()
			: FText::ChangeKey(TextNamespace, Row.ToString() + TEXT("_") + Field, Source);
	}

	static bool SameText(const FText& Actual, const FText& Expected)
	{
		if (Actual.ToString() != Expected.ToString()) return false;
		if (Expected.IsEmpty()) return Actual.IsEmpty();
		const auto ActualNamespace = FTextInspector::GetNamespace(Actual);
		const auto ExpectedNamespace = FTextInspector::GetNamespace(Expected);
		const auto ActualKey = FTextInspector::GetKey(Actual);
		const auto ExpectedKey = FTextInspector::GetKey(Expected);
		return ActualNamespace.IsSet() && ExpectedNamespace.IsSet()
			&& ActualNamespace.GetValue() == ExpectedNamespace.GetValue()
			&& ActualKey.IsSet() && ExpectedKey.IsSet() && ActualKey.GetValue() == ExpectedKey.GetValue();
	}

	static bool MergeLines(TArray<FText>& Target, const TArray<FText>& Source,
		const FString& TextNamespace, FName Row, const TCHAR* Field)
	{
		bool bMatches = Target.Num() == Source.Num();
		TArray<FText> Next;
		Next.Reserve(Source.Num());
		for (int32 Index = 0; Index < Source.Num(); ++Index)
		{
			const FText Value = TargetText(Source[Index], TextNamespace, Row,
				FString::Printf(TEXT("%s_Index%d"), Field, Index));
			bMatches &= Target.IsValidIndex(Index) && SameText(Target[Index], Value);
			Next.Add(Value);
		}
		Target = MoveTemp(Next);
		return bMatches;
	}

	static bool LoadCsv(UDataTable* Destination, const TCHAR* CsvName, UDataTable*& Incoming)
	{
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport") / CsvName)))
		{
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Cannot read %s"), CsvName);
			return false;
		}
		Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = Destination->RowStruct;
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] CSV %s: %s"), CsvName, *Problem);
		return Problems.IsEmpty();
	}

	static bool Backup(UDataTable* Table, const FString& Directory)
	{
		const FString Source = FPackageName::LongPackageNameToFilename(
			Table->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		const FString Destination = Directory / (Table->GetName() + TEXT(".uasset"));
		return IFileManager::Get().FileExists(*Source)
			&& IFileManager::Get().MakeDirectory(*Directory, true)
			&& IFileManager::Get().Copy(*Destination, *Source, false, true) == COPY_OK;
	}

	static bool Save(UDataTable* Table)
	{
		UPackage* Package = Table->GetOutermost();
		Table->HandleDataTableChanged();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.Error = GWarn;
		return UPackage::SavePackage(Package, Table, *Filename, Args) && !Package->IsDirty();
	}

	static bool RestoreBackup(UDataTable* Table, const FString& Directory)
	{
		const FString Source = Directory / (Table->GetName() + TEXT(".uasset"));
		const FString Destination = FPackageName::LongPackageNameToFilename(
			Table->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*Destination, *Source, true, true) != COPY_OK) return false;
		TArray<uint8> Expected;
		TArray<uint8> Actual;
		return FFileHelper::LoadFileToArray(Expected, *Source)
			&& FFileHelper::LoadFileToArray(Actual, *Destination) && Actual == Expected;
	}

	static void Sync(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() != 1 || (Arguments[0] != TEXT("check") && Arguments[0] != TEXT("apply"))
			|| !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Usage: CGR.Tutorial.SyncStartupEconomy check|apply (stop PIE/SIE)"));
			return;
		}
		const bool bApply = Arguments[0] == TEXT("apply");
		UDataTable* Resource = LoadObject<UDataTable>(nullptr, ResourcePath);
		UDataTable* Mission = LoadObject<UDataTable>(nullptr, MissionPath);
		UDataTable* Buildable = LoadObject<UDataTable>(nullptr, BuildablePath);
		if (!Resource || !Mission || !Buildable || Resource->GetRowStruct() != FResourceInfo::StaticStruct()
			|| Mission->GetRowStruct() != FMissionTable::StaticStruct()
			|| Buildable->GetRowStruct() != FBuildableCardTable::StaticStruct()
			|| Resource->GetOutermost()->IsDirty() || Mission->GetOutermost()->IsDirty()
			|| Buildable->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Missing, wrong, or dirty destination table"));
			return;
		}
		UDataTable* CsvResource = nullptr;
		UDataTable* CsvMission = nullptr;
		UDataTable* CsvBuildable = nullptr;
		if (!LoadCsv(Resource, TEXT("DT_Resource_Import.csv"), CsvResource)
			|| !LoadCsv(Mission, TEXT("DT_Mission_Import.csv"), CsvMission)
			|| !LoadCsv(Buildable, TEXT("BuildableTable_Import.csv"), CsvBuildable)) return;
		TSet<FName> CsvChain;
		FName Cursor(TEXT("M1_CollectBricks"));
		while (!Cursor.IsNone() && !CsvChain.Contains(Cursor))
		{
			const FMissionTable* Row = CsvMission->FindRow<FMissionTable>(Cursor, TEXT("StartupEconomyChain"), false);
			if (!Row) break;
			CsvChain.Add(Cursor);
			Cursor = Row->NextMissionID;
		}
		if (!Cursor.IsNone() || CsvChain.Num() != 33 || CsvMission->GetRowMap().Num() != 33)
		{
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] CSV mission chain must visit all 33 rows once"));
			return;
		}

		const FName MoneyName(TEXT("Money"));
		const FResourceInfo* CurrentMoney = Resource->FindRow<FResourceInfo>(MoneyName, TEXT("StartupEconomy"), false);
		const FResourceInfo* CsvMoney = CsvResource->FindRow<FResourceInfo>(MoneyName, TEXT("StartupEconomy"), false);
		if (!CurrentMoney || !CsvMoney || CurrentMoney->ResourceType != EResourceType::Money
			|| CsvMoney->ResourceType != EResourceType::Money
			|| Resource->GetRowMap().Num() != CsvResource->GetRowMap().Num()
			|| Mission->GetRowMap().Num() != CsvMission->GetRowMap().Num()
			|| Buildable->GetRowMap().Num() != CsvBuildable->GetRowMap().Num())
		{
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Resource identity or table row count mismatch"));
			return;
		}
		const FResourceInfo BeforeMoney = *CurrentMoney;
		FResourceInfo NextMoney = BeforeMoney;
		NextMoney.InitialAmount = CsvMoney->InitialAmount;
		bool bMatches = BeforeMoney.InitialAmount == NextMoney.InitialAmount;
		TMap<FName, FMissionTable> BeforeMissions;
		TMap<FName, FMissionTable> NextMissions;
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Mission->GetName(), TextNamespaceUtil::GetPackageNamespace(Mission), true);
		for (const FName Row : MissionNames)
		{
			const FMissionTable* Current = Mission->FindRow<FMissionTable>(Row, TEXT("StartupEconomy"), false);
			const FMissionTable* Source = CsvMission->FindRow<FMissionTable>(Row, TEXT("StartupEconomy"), false);
			if (!Current || !Source || Current->ConditionType != Source->ConditionType)
			{
				UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Missing or changed mission identity: %s"), *Row.ToString());
				return;
			}
			BeforeMissions.Add(Row, *Current);
			FMissionTable Next = *Current;
			if (Row == TEXT("M33_MenuTour"))
			{
				const FText Title = TargetText(Source->Title, TextNamespace, Row, TEXT("Title"));
				bMatches &= SameText(Next.Title, Title);
				Next.Title = Title;
			}
			if (Row == TEXT("M1_CollectBricks"))
			{
				bMatches &= Next.ConditionAmount == Source->ConditionAmount;
				Next.ConditionAmount = Source->ConditionAmount;
			}
			bMatches &= MergeLines(Next.MentorLines, Source->MentorLines, TextNamespace, Row, TEXT("MentorLines"));
			bMatches &= MergeLines(Next.GuideLines, Source->GuideLines, TextNamespace, Row, TEXT("GuideLines"));
			bMatches &= MergeLines(Next.ExplainLines, Source->ExplainLines, TextNamespace, Row, TEXT("ExplainLines"));
			if (Row == TEXT("M2_UpgradeFactory"))
			{
				const FText Shortfall = TargetText(Source->ShortfallLine, TextNamespace, Row, TEXT("ShortfallLine"));
				bMatches &= SameText(Next.ShortfallLine, Shortfall);
				Next.ShortfallLine = Shortfall;
			}
			NextMissions.Add(Row, MoveTemp(Next));
		}
		const FName StarterName(TEXT("B1"));
		const FBuildableCardTable* CurrentStarter = Buildable->FindRow<FBuildableCardTable>(StarterName, TEXT("StartupEconomy"), false);
		const FBuildableCardTable* CsvStarter = CsvBuildable->FindRow<FBuildableCardTable>(StarterName, TEXT("StartupEconomy"), false);
		if (!CurrentStarter || !CsvStarter || CurrentStarter->Name != CsvStarter->Name
			|| CurrentStarter->InteractableType != CsvStarter->InteractableType
			|| CurrentStarter->ConstructionCosts.Num() != 1 || CsvStarter->ConstructionCosts.Num() != 1
			|| CurrentStarter->ConstructionCosts[0].ResourceType != EResourceType::Brick
			|| CsvStarter->ConstructionCosts[0].ResourceType != EResourceType::Brick
			|| CsvStarter->ConstructionCosts[0].Cost <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Missing or changed B1 Brick construction cost"));
			return;
		}
		const FBuildableCardTable BeforeStarter = *CurrentStarter;
		FBuildableCardTable NextStarter = BeforeStarter;
		bMatches &= BeforeStarter.ConstructionCosts[0].Cost == CsvStarter->ConstructionCosts[0].Cost;
		NextStarter.ConstructionCosts = CsvStarter->ConstructionCosts;
		for (const FName Row : LinkNames)
		{
			const FMissionTable* Current = Mission->FindRow<FMissionTable>(Row, TEXT("StartupEconomy"), false);
			const FMissionTable* Source = CsvMission->FindRow<FMissionTable>(Row, TEXT("StartupEconomy"), false);
			if (!Current || !Source || Current->ConditionType != Source->ConditionType
				|| Source->NextMissionID.IsNone()
				|| !Mission->FindRow<FMissionTable>(Source->NextMissionID, TEXT("StartupEconomy"), false)
				|| !CsvMission->FindRow<FMissionTable>(Source->NextMissionID, TEXT("StartupEconomy"), false))
			{
				UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Missing or changed mission link: %s"), *Row.ToString());
				return;
			}
			BeforeMissions.Add(Row, *Current);
			FMissionTable Next = *Current;
			bMatches &= Next.NextMissionID == Source->NextMissionID;
			Next.NextMissionID = Source->NextMissionID;
			NextMissions.Add(Row, MoveTemp(Next));
		}
		if (!bApply || bMatches)
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[StartupEconomy] %s PASS: Money=%lld, missions=%d"),
					bApply ? TEXT("APPLY") : TEXT("CHECK"), NextMoney.InitialAmount, NextMissions.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] CHECK FAIL: Money=%lld, missions=%d"),
					NextMoney.InitialAmount, NextMissions.Num());
			}
			return;
		}
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("StartupEconomy/Backup")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		if (!Backup(Resource, BackupDir) || !Backup(Mission, BackupDir) || !Backup(Buildable, BackupDir))
		{
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] Backup failed; tables untouched"));
			return;
		}
		Resource->AddRow(MoneyName, NextMoney);
		for (const auto& Pair : NextMissions) Mission->AddRow(Pair.Key, Pair.Value);
		Buildable->AddRow(StarterName, NextStarter);
		bool bExact = Resource->GetRowMap().Num() == CsvResource->GetRowMap().Num()
			&& Mission->GetRowMap().Num() == CsvMission->GetRowMap().Num()
			&& Buildable->GetRowMap().Num() == CsvBuildable->GetRowMap().Num();
		const FResourceInfo* NewMoney = Resource->FindRow<FResourceInfo>(MoneyName, TEXT("StartupEconomyReadback"), false);
		bExact &= NewMoney && FResourceInfo::StaticStruct()->CompareScriptStruct(NewMoney, &NextMoney, 0);
		for (const auto& Pair : NextMissions)
		{
			const FMissionTable* NewRow = Mission->FindRow<FMissionTable>(Pair.Key, TEXT("StartupEconomyReadback"), false);
			bExact &= NewRow && FMissionTable::StaticStruct()->CompareScriptStruct(NewRow, &Pair.Value, 0);
		}
		const FBuildableCardTable* NewStarter = Buildable->FindRow<FBuildableCardTable>(StarterName, TEXT("StartupEconomyReadback"), false);
		bExact &= NewStarter && FBuildableCardTable::StaticStruct()->CompareScriptStruct(NewStarter, &NextStarter, 0);
		if (!bExact || !Save(Resource) || !Save(Mission) || !Save(Buildable))
		{
			Resource->AddRow(MoneyName, BeforeMoney);
			for (const auto& Pair : BeforeMissions) Mission->AddRow(Pair.Key, Pair.Value);
			Buildable->AddRow(StarterName, BeforeStarter);
			Resource->HandleDataTableChanged();
			Mission->HandleDataTableChanged();
			Buildable->HandleDataTableChanged();
			const bool bResourceRestored = RestoreBackup(Resource, BackupDir);
			const bool bMissionRestored = RestoreBackup(Mission, BackupDir);
			const bool bBuildableRestored = RestoreBackup(Buildable, BackupDir);
			Resource->GetOutermost()->SetDirtyFlag(!bResourceRestored);
			Mission->GetOutermost()->SetDirtyFlag(!bMissionRestored);
			Buildable->GetOutermost()->SetDirtyFlag(!bBuildableRestored);
			UE_LOG(LogTemp, Error, TEXT("[StartupEconomy] APPLY FAIL; backup restore Resource=%s Mission=%s Buildable=%s; backup=%s"),
				bResourceRestored ? TEXT("PASS") : TEXT("FAIL"), bMissionRestored ? TEXT("PASS") : TEXT("FAIL"),
				bBuildableRestored ? TEXT("PASS") : TEXT("FAIL"), *BackupDir);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[StartupEconomy] APPLY PASS: selected fields only; backup=%s"), *BackupDir);
	}

	static FAutoConsoleCommand SyncCommand(TEXT("CGR.Tutorial.SyncStartupEconomy"),
		TEXT("Merge starting Money, M1-M3 text/goal, M33 menu text, M21/M26/M22 links, and B1 Brick cost from CSV: check|apply."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Sync));

	static void SyncMineRates(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() != 1 || (Arguments[0] != TEXT("check") && Arguments[0] != TEXT("apply"))
			|| !IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[MineRates] Usage: CGR.Mine.SyncBaseRates check|apply (stop PIE/SIE)"));
			return;
		}
		UDataTable* Resource = LoadObject<UDataTable>(nullptr, ResourcePath);
		if (!Resource || Resource->GetRowStruct() != FResourceInfo::StaticStruct()
			|| Resource->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[MineRates] Missing, wrong, or dirty DT_Resource"));
			return;
		}
		UDataTable* CsvResource = nullptr;
		if (!LoadCsv(Resource, TEXT("DT_Resource_Import.csv"), CsvResource)
			|| Resource->GetRowMap().Num() != CsvResource->GetRowMap().Num()) return;

		TMap<FName, FResourceInfo> Before;
		TMap<FName, FResourceInfo> Expected;
		bool bMatches = true;
		for (const auto& Pair : Resource->GetRowMap())
		{
			const FResourceInfo* Current = reinterpret_cast<const FResourceInfo*>(Pair.Value);
			const FResourceInfo* Source = CsvResource->FindRow<FResourceInfo>(Pair.Key, TEXT("MineRates"), false);
			if (!Source || Current->ResourceType != Source->ResourceType
				|| !FMath::IsFinite(Source->MiningRatePerMinute) || Source->MiningRatePerMinute < 0.0f)
			{
				UE_LOG(LogTemp, Error, TEXT("[MineRates] Missing or invalid resource row: %s"), *Pair.Key.ToString());
				return;
			}
			Before.Add(Pair.Key, *Current);
			FResourceInfo Next = *Current;
			Next.MiningRatePerMinute = Source->MiningRatePerMinute;
			bMatches &= Current->MiningRatePerMinute == Next.MiningRatePerMinute;
			Expected.Add(Pair.Key, MoveTemp(Next));
		}
		if (Arguments[0] == TEXT("check") || bMatches)
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[MineRates] PASS: %d resource rows"), Expected.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[MineRates] CHECK FAIL: %d resource rows"), Expected.Num());
			}
			return;
		}
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("MineRates/Backup")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		if (!Backup(Resource, BackupDir))
		{
			UE_LOG(LogTemp, Error, TEXT("[MineRates] Backup failed; table untouched"));
			return;
		}
		for (const auto& Pair : Expected) Resource->AddRow(Pair.Key, Pair.Value);
		bool bExact = Resource->GetRowMap().Num() == Before.Num();
		for (const auto& Pair : Expected)
		{
			const FResourceInfo* Actual = Resource->FindRow<FResourceInfo>(Pair.Key, TEXT("MineRatesReadback"), false);
			bExact &= Actual && FResourceInfo::StaticStruct()->CompareScriptStruct(Actual, &Pair.Value, 0);
		}
		if (!bExact || !Save(Resource))
		{
			for (const auto& Pair : Before) Resource->AddRow(Pair.Key, Pair.Value);
			Resource->HandleDataTableChanged();
			const bool bRestored = RestoreBackup(Resource, BackupDir);
			Resource->GetOutermost()->SetDirtyFlag(!bRestored);
			UE_LOG(LogTemp, Error, TEXT("[MineRates] APPLY FAIL; backup restore=%s; backup=%s"),
				bRestored ? TEXT("PASS") : TEXT("FAIL"), *BackupDir);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[MineRates] APPLY PASS: rate field only; backup=%s"), *BackupDir);
	}

	static FAutoConsoleCommand SyncMineRatesCommand(TEXT("CGR.Mine.SyncBaseRates"),
		TEXT("Merge DT_Resource.MiningRatePerMinute from CSV: check|apply."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SyncMineRates));
}

#endif
