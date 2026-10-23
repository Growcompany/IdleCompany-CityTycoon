#include "Office/OfficeTVBroadcastSource.h"

#include "Core/CGGameInstance.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/BuildingSaveData.h"
#include "Data/EmployeeEquipmentRuntimeData.h"
#include "Data/EmployeeTypes.h"
#include "Data/GameSaveData.h"
#include "Data/OfficeRosterSummary.h"
#include "Data/OperationData.h"
#include "Data/ProductionOrderData.h"
#include "Data/ProjectBoardData.h"
#include "Data/ProjectEventData.h"
#include "Data/ShippedRecord.h"
#include "Data/TierLayout.h"
#include "Engine/World.h"
#include "Enum/LiveOpsTypes.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/QualityGrade.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/EmployeeManager.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/LiveOpsSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Math/RandomStream.h"
#include "Table/LiveOpsTables.h"
#include "Table/ProjectDataTable.h"
#include "Table/ProjectGenreTable.h"
#include "Table/TierUnlockData.h"
#include "UI/Element/Cards/EventChoiceCardWidget.h"

#define LOCTEXT_NAMESPACE "OfficeTVBroadcastSource"

// Named rather than anonymous: unity builds merge this file with others that use the same helper names.
namespace OfficeTVSource
{
	constexpr int32 CrewLimit = 5;
	constexpr int32 HallOfFameLimit = 5;
	constexpr int32 TopRevenueLimit = 3;
	constexpr int32 RevenueBars = 24;

	struct FTVContext
	{
		UWorld* World = nullptr;
		UCGGameInstance* Game = nullptr;
		UTableManagerSubsystem* Tables = nullptr;
		USaveGame_GameData* Save = nullptr;
		UOfficeStageProgressManager* Stage = nullptr;
		UEmployeeManager* Employees = nullptr;
		int32 Building = INDEX_NONE;
		ECompanyType Industry = ECompanyType::None;

		bool Resolve(UWorld* InWorld)
		{
			World = InWorld;
			Game = InWorld ? Cast<UCGGameInstance>(InWorld->GetGameInstance()) : nullptr;
			if (!Game || !Game->IsInOfficeMap() || Game->IsVisitMode())
			{
				return false;
			}
			Building = Game->GetCurrentManagedBuildingIndex();
			Industry = Game->GetCurrentBuildingCompanyType();
			Tables = Game->GetSubsystem<UTableManagerSubsystem>();
			USaveLoadManager* Saves = Game->GetSubsystem<USaveLoadManager>();
			Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
			Stage = InWorld->GetSubsystem<UOfficeStageProgressManager>();
			Employees = Game->GetSubsystem<UEmployeeManager>();
			return Building >= 0 && Industry != ECompanyType::None && Tables && Save;
		}

		const FProjectTierProgress* TierProgress() const
		{
			if (Stage)
			{
				return &Stage->GetTierProgress();
			}
			const FOfficeSaveData* Office = Save->GameData.OfficeDataMap.Find(Building);
			return Office ? &Office->TierProgress : nullptr;
		}
	};

	bool FindProject(const UTableManagerSubsystem* Tables, ECompanyType Industry, int32 ProjectId, FProjectData& OutProject)
	{
		if (!Tables || Industry == ECompanyType::None || ProjectId <= 0)
		{
			return false;
		}
		bool bFound = false;
		OutProject = Tables->GetProjectData(Industry, ProjectId, bFound);
		return bFound && !OutProject.ProjectName.IsEmpty();
	}

	FText ProjectTitle(const FString& PlayerName, const FProjectData& Project)
	{
		return PlayerName.IsEmpty() ? Project.ProjectName : FText::FromString(PlayerName);
	}

	const FEmployeeInstance* FindEmployee(UEmployeeManager* Employees, int32 EmployeeId)
	{
		return Employees && EmployeeId != INDEX_NONE ? Employees->FindEmployee(EmployeeId) : nullptr;
	}

	FText FullName(UEmployeeManager* Employees, int32 EmployeeId)
	{
		return FText::FromString(Employees->GetEmployeeFullName(EmployeeId));
	}

	FText LevelBadge(int32 Level)
	{
		return FText::Format(LOCTEXT("LevelBadge", "Lv.{0}"), FText::AsNumber(Level));
	}

	FText JobTitle(int32 Level)
	{
		return FText::FromString(UEmployeeTypeHelper::GetJobTitleDisplayName(UEmployeeTypeHelper::GetJobTitleFromLevel(Level)));
	}

	/** Money exactly as the office top bar prints it, so the TV never disagrees with it. */
	FText StripMoney(double Amount)
	{
		return UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(FMath::Max(0.0, Amount)), ENumberAbbrevStyle::Auto,
			ENumberRoundMode::Floor);
	}

	FText ClockText(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60));
	}

	FText DurationText(float Seconds)
	{
		const int32 Whole = FMath::Max(1, FMath::RoundToInt(Seconds));
		if (Whole < 60)
		{
			return FText::Format(LOCTEXT("DurationSeconds", "{0}초"), FText::AsNumber(Whole));
		}
		return Whole % 60 == 0
			? FText::Format(LOCTEXT("DurationMinutes", "{0}분"), FText::AsNumber(Whole / 60))
			: FText::Format(LOCTEXT("DurationMinutesSeconds", "{0}분 {1}초"), FText::AsNumber(Whole / 60), FText::AsNumber(Whole % 60));
	}

	FText MultiplierText(float Multiplier)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 1;
		return FText::Format(LOCTEXT("Multiplier", "x{0}"), FText::AsNumber(Multiplier, &Options));
	}

	int32 GradeRank(EQualityGrade Grade)
	{
		return static_cast<int32>(Grade);
	}

	void PortraitFrame(UEmployeeManager* Employees, const FEmployeeInstance& Employee, FOfficeTVBroadcastFrame& Frame)
	{
		Frame.PortraitEmployeeId = Employee.EmployeeID;
		Frame.PortraitBadge = LevelBadge(Employee.Level);
		Frame.Accent = FLootBoxRarityUtility::GetRarityColor(Employee.SpawnRarity);
		Frame.Headline = FullName(Employees, Employee.EmployeeID);
	}

	FOfficeTVBroadcastEntry PortraitEntry(UEmployeeManager* Employees, const FEmployeeInstance& Employee)
	{
		FOfficeTVBroadcastEntry Entry;
		Entry.PortraitEmployeeId = Employee.EmployeeID;
		Entry.Title = FullName(Employees, Employee.EmployeeID);
		Entry.Caption = LevelBadge(Employee.Level);
		Entry.Accent = FLootBoxRarityUtility::GetRarityColor(Employee.SpawnRarity);
		return Entry;
	}

	bool InBuilding(const FEmployeeInstance& Employee, int32 Building)
	{
		return Employee.bIsAssigned && Employee.AssignedBuildingIndex == Building;
	}

	/** Participants of the active development run, ordered by employee id. */
	void CollectParticipants(const FTVContext& Context, TArray<TPair<int32, int32>>& Out)
	{
		Out.Reset();
		const FEquipmentAttemptRuntime* Attempt = Context.Stage ? Context.Stage->FindEquipmentAttempt() : nullptr;
		const FStageProgressData* Progress = Context.Stage ? &Context.Stage->GetProgressData() : nullptr;
		if (!Attempt || !Progress || Attempt->AttemptId != Progress->EquipmentAttemptId || Attempt->BuildingId != Context.Building
			|| !Context.Employees)
		{
			return;
		}
		for (const FEmployeeInstance& Employee : Context.Employees->GetAllEmployeesRef())
		{
			if (!InBuilding(Employee, Context.Building))
			{
				continue;
			}
			const FEquipmentEmployeeRuntime* Recorded = Attempt->Employees.Find(Employee.EmployeeID);
			if (Recorded && Recorded->EmployeeId == Employee.EmployeeID && (Recorded->NativeCount > 0 || Recorded->bStartParticipant))
			{
				Out.Emplace(Employee.EmployeeID, Recorded->NativeCount);
			}
		}
		Out.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Key < B.Key; });
	}

	const FShippedProjectRecord* FindShipped(const FTVContext& Context, int32 ProjectIndex)
	{
		return Context.Save->GameData.ShippedProjects.FindByPredicate([&Context, ProjectIndex](const FShippedProjectRecord& Record)
		{
			return Record.BuildingIndex == Context.Building && Record.Industry == Context.Industry && Record.ProjectIndex == ProjectIndex;
		});
	}

	void CollectShipped(const FTVContext& Context, TArray<const FShippedProjectRecord*>& Out)
	{
		Out.Reset();
		for (const FShippedProjectRecord& Record : Context.Save->GameData.ShippedProjects)
		{
			if (Record.BuildingIndex == Context.Building && Record.Industry == Context.Industry && Record.ProjectIndex > 0)
			{
				Out.Add(&Record);
			}
		}
	}

	FText EnhancementNames(const FTVContext& Context, int32 Tier)
	{
		bool bFound = false;
		const FTierUnlockData Unlock = Context.Tables->GetTierUnlockData(Tier, bFound);
		TArray<FString> Names;
		if (bFound)
		{
			for (const EBuildingEnhancementType Type : Unlock.UnlockedEnhancements)
			{
				FBuildingEnhancementDefinition Definition;
				if (Context.Tables->GetEnhancementDefinition(Type, Definition) && !Definition.DisplayName.IsEmpty())
				{
					Names.Add(Definition.DisplayName.ToString());
				}
			}
		}
		return FText::FromString(FString::Join(Names, TEXT(" · ")));
	}

	// ---------------- Development ----------------

	void GatherDevelopment(const FTVContext& Context, const FOfficeTVNewsBuffers& News,
		OfficeTVBroadcastSource::FSupportFilter IsSupported, FOfficeTVSchedule& Out)
	{
		const FStageProgressData& Progress = Context.Stage->GetProgressData();
		FProjectData Project;
		if (Progress.CompanyType != Context.Industry || !FindProject(Context.Tables, Context.Industry, Progress.ProjectID, Project))
		{
			return;
		}
		Out.Pool = EOfficeTVPool::Developing;
		Out.Opener = EOfficeTVProgram::Development;
		const FText Title = ProjectTitle(Progress.ProjectName, Project);
		const float Total = Context.Stage->GetTotalDuration();
		const float Left = Context.Stage->GetRemainingTime();
		const bool bTimed = Total > 0.f && FMath::IsFinite(Total) && FMath::IsFinite(Left);
		const float Ratio = bTimed ? FMath::Clamp(1.f - Left / Total, 0.f, 1.f) : -1.f;

		if (bTimed && Context.Stage->GetLifecycle() == EProjectLifecycle::Developing && IsSupported(EOfficeTVProgram::LaunchCountdown))
		{
			const float Window = OfficeTVBroadcastSource::CountdownWindow(Total);
			if (Left <= Window)
			{
				FOfficeTVBroadcastFrame Frame;
				Frame.Program = EOfficeTVProgram::LaunchCountdown;
				Frame.Artwork = Project.Icon;
				Frame.Progress = Ratio;
				Frame.Value = ClockText(Left);
				Frame.Count = FMath::Max(0, FMath::CeilToInt(Left));
				Frame.Total = FMath::CeilToInt(Window);
				Frame.Headline = Title;
				Out.Locked = Frame;
			}
		}

		if (IsSupported(EOfficeTVProgram::Development))
		{
			FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::Development;
			Frame.Artwork = Project.Icon;
			Frame.Headline = Title;
			Frame.Progress = FMath::Max(Ratio, 0.f);
			Frame.Value = FText::AsPercent(Frame.Progress);
		}

		if (IsSupported(EOfficeTVProgram::DevRace))
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::DevRace;
			int32 Reached = 0;
			for (const FStepRoundData& Step : Progress.Steps)
			{
				if (Step.DisciplineSlot == INDEX_NONE || Step.TargetScore <= 0.f)
				{
					continue;
				}
				FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
				Entry.Title = Context.Tables->GetDisciplineDisplayName(Context.Industry, Step.DisciplineSlot);
				Entry.Slot = Step.DisciplineSlot;
				Entry.Ratio = Step.AcquiredScore / Step.TargetScore;
				Entry.bHighlighted = Entry.Ratio >= 1.f;
				Reached += Entry.bHighlighted ? 1 : 0;
			}
			if (!Frame.Entries.IsEmpty())
			{
				Frame.Headline = FText::Format(LOCTEXT("RaceHeadline", "{0} 목표 도달"), Title);
				Frame.HeadlineValue = FText::Format(LOCTEXT("RaceCount", "{0}/{1}"), FText::AsNumber(Reached), FText::AsNumber(Frame.Entries.Num()));
				Out.Rotation.Add(MoveTemp(Frame));
			}
		}

		if (!IsManufacturingType(Context.Industry) && IsSupported(EOfficeTVProgram::DevForecast))
		{
			FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::DevForecast;
			const EQualityGrade Grade = Context.Stage->GetCurrentQualityGrade();
			Frame.Grade = OfficeTVBroadcastSource::GradeLetter(Grade);
			Frame.GradeLabel = Context.Tables->GetQualityGradeLabel(Context.Industry, Grade);
			Frame.Progress = FMath::Clamp((Context.Stage->GetCurrentQualityScore() - 0.5f) / 1.5f, 0.f, 1.f);
			for (const EQualityGrade Zone : {EQualityGrade::C, EQualityGrade::B, EQualityGrade::A, EQualityGrade::S})
			{
				FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
				Entry.Grade = OfficeTVBroadcastSource::GradeLetter(Zone);
				Entry.Title = Context.Tables->GetQualityGradeLabel(Context.Industry, Zone);
				Entry.bHighlighted = Zone == Grade;
			}
			Frame.Headline = FText::Format(LOCTEXT("ForecastHeadline", "{0} 예상 등급"), Title);
		}

		TArray<TPair<int32, int32>> Participants;
		CollectParticipants(Context, Participants);

		if (Context.Stage->GetLifecycle() == EProjectLifecycle::Developing && IsSupported(EOfficeTVProgram::EmployeeHighlight))
		{
			// The most active participant; ties go to the earlier hire.
			const TPair<int32, int32>* Best = nullptr;
			for (const TPair<int32, int32>& Participant : Participants)
			{
				if (!Best || Participant.Value > Best->Value)
				{
					Best = &Participant;
				}
			}
			const FEmployeeInstance* Employee = Best ? FindEmployee(Context.Employees, Best->Key) : nullptr;
			if (Employee)
			{
				const int32 Count = Best->Value;
				FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
				Frame.Program = EOfficeTVProgram::EmployeeHighlight;
				PortraitFrame(Context.Employees, *Employee, Frame);
				Frame.Value = FText::AsNumber(Count);
				Frame.Count = Count;
				Frame.Subtitle = Count == 0 ? LOCTEXT("EmployeeStarting", "신작 개발 시작!")
					: Count == 1 ? LOCTEXT("EmployeeFirstWork", "첫 작업 완료!")
					: LOCTEXT("EmployeeWorking", "개발에 집중 중");
			}
		}

		if (Participants.Num() >= 2 && IsSupported(EOfficeTVProgram::Crew))
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Crew;
			for (const TPair<int32, int32>& Participant : Participants)
			{
				if (const FEmployeeInstance* Employee = FindEmployee(Context.Employees, Participant.Key))
				{
					FOfficeTVBroadcastEntry Entry = PortraitEntry(Context.Employees, *Employee);
					Entry.Value = FText::AsNumber(Participant.Value);
					Frame.Entries.Add(MoveTemp(Entry));
				}
				if (Frame.Entries.Num() >= CrewLimit)
				{
					break;
				}
			}
			Frame.Headline = FText::Format(LOCTEXT("CrewHeadline", "{0} 제작진"), Title);
			Frame.HeadlineValue = FText::Format(LOCTEXT("CrewCount", "{0}명"), FText::AsNumber(Participants.Num()));
			if (Frame.Entries.Num() >= 2)
			{
				Out.Rotation.Add(MoveTemp(Frame));
			}
		}

		if (!News.RecentWork.IsEmpty() && IsSupported(EOfficeTVProgram::WorkFeed))
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::WorkFeed;
			for (const FOfficeTVNewsBuffers::FWork& Work : News.RecentWork)
			{
				if (const FEmployeeInstance* Employee = FindEmployee(Context.Employees, Work.EmployeeId))
				{
					FOfficeTVBroadcastEntry Entry = PortraitEntry(Context.Employees, *Employee);
					Entry.Value = FText::Format(LOCTEXT("WorkScore", "+{0}"), FText::AsNumber(FMath::Max(1, FMath::RoundToInt(Work.Score))));
					Entry.Slot = Work.Slot;
					Entry.bHighlighted = Frame.Entries.IsEmpty();
					Frame.Entries.Add(MoveTemp(Entry));
				}
			}
			Frame.Headline = LOCTEXT("WorkFeedHeadline", "방금 들어온 작업");
			if (!Frame.Entries.IsEmpty())
			{
				Out.Rotation.Add(MoveTemp(Frame));
			}
		}

		if (IsSupported(EOfficeTVProgram::Mentoring))
		{
			const FEquipmentAttemptRuntime* Attempt = Context.Stage->FindEquipmentAttempt();
			if (Attempt && Attempt->AttemptId == Progress.EquipmentAttemptId)
			{
				for (int32 Index = Attempt->Supports.Num() - 1; Index >= 0; --Index)
				{
					const FEquipmentSupportRuntime& Support = Attempt->Supports[Index];
					const FEmployeeInstance* Provider = FindEmployee(Context.Employees, Support.ProviderEmployeeId);
					const FEmployeeInstance* Recipient = FindEmployee(Context.Employees, Support.RecipientEmployeeId);
					if (!Provider || !Recipient || Provider == Recipient)
					{
						continue;
					}
					FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
					Frame.Program = EOfficeTVProgram::Mentoring;
					Frame.Entries.Add(PortraitEntry(Context.Employees, *Provider));
					Frame.Entries.Add(PortraitEntry(Context.Employees, *Recipient));
					Frame.Headline = FText::Format(LOCTEXT("MentorHeadline", "{0} → {1}"), Frame.Entries[0].Title, Frame.Entries[1].Title);
					break;
				}
			}
		}

		if (Context.Employees && IsSupported(EOfficeTVProgram::StaffGap))
		{
			TArray<FOfficeRosterLine> Lines;
			int32 Members = 0;
			OfficeRosterSummary::BuildForBuilding(Context.Employees->GetAllEmployeesRef(), Context.Building, Lines, Members);
			for (const FStepRoundData& Step : Progress.Steps)
			{
				if (!Lines.IsValidIndex(Step.DisciplineSlot) || Lines[Step.DisciplineSlot].Headcount > 0)
				{
					continue;
				}
				const FText Discipline = Context.Tables->GetDisciplineDisplayName(Context.Industry, Step.DisciplineSlot);
				FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
				Frame.Program = EOfficeTVProgram::StaffGap;
				Frame.IconSlot = Step.DisciplineSlot;
				Frame.Title = Discipline;
				Frame.Value = FText::AsNumber(0);
				Frame.Headline = FText::Format(LOCTEXT("GapHeadline", "이번 작품에 {0}{1} 필요합니다"), Discipline,
					FText::FromString(OfficeTVBroadcastSource::SubjectParticle(Discipline.ToString())));
				break;
			}
		}
	}

	// ---------------- Idle and records ----------------

	void AddAdvertisement(const FTVContext& Context, int32 ProjectIndex, const FString& PlayerName, FOfficeTVSchedule& Out)
	{
		FProjectData Project;
		if (!FindProject(Context.Tables, Context.Industry, ProjectIndex, Project))
		{
			return;
		}
		FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
		Frame.Program = EOfficeTVProgram::Advertisement;
		Frame.Title = ProjectTitle(PlayerName, Project);
		Frame.Subtitle = Project.SubName;
		Frame.Artwork = Project.Icon;
		Frame.Label = FText::FromName(Project.Genre);
		if (const FShippedProjectRecord* Shipped = FindShipped(Context, ProjectIndex))
		{
			Frame.Grade = OfficeTVBroadcastSource::GradeLetter(Shipped->QualityGrade);
			Frame.GradeLabel = Context.Tables->GetQualityGradeLabel(Context.Industry, Shipped->QualityGrade);
		}
	}

	void AddLastShippedAdvertisement(const FTVContext& Context, const FProjectTierProgress& Tier, FOfficeTVSchedule& Out)
	{
		if (IsManufacturingType(Context.Industry))
		{
			return; // 제조업 클리어는 설계 완료이며 실제 제품 판매를 뜻하지 않는다.
		}
		for (int32 Index = Tier.ClearedProjects.Num() - 1; Index >= 0; --Index)
		{
			FProjectData Project;
			if (FindProject(Context.Tables, Context.Industry, Tier.ClearedProjects[Index], Project))
			{
				AddAdvertisement(Context, Tier.ClearedProjects[Index], FString(), Out);
				return;
			}
		}
	}

	void AddNextTitle(const FTVContext& Context, const FProjectTierProgress& Tier, FOfficeTVSchedule& Out)
	{
		if (Tier.CurrentTier >= FTierLayout::MaxTier())
		{
			return;
		}
		const int32 Remaining = Tier.GetRemainingForNextTier();
		if (Remaining <= 0)
		{
			return;
		}
		FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
		Frame.Program = EOfficeTVProgram::CompanyNews;
		const FText RemainingText = FText::Format(LOCTEXT("ProjectsRemaining", "{0}개"), FText::AsNumber(Remaining));
		FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
		Entry.Title = Context.Tables->GetTierRankTitle(Context.Industry, Tier.CurrentTier + 1);
		Entry.Caption = Context.Tables->GetTierRankTitle(Context.Industry, Tier.CurrentTier);
		Entry.Value = EnhancementNames(Context, Tier.CurrentTier + 1);
		Frame.Headline = LOCTEXT("NextTitleHeadline", "승급까지 출시");
		Frame.HeadlineValue = RemainingText;
	}

	void AddTierCodex(const FTVContext& Context, const FProjectTierProgress& Tier, FOfficeTVSchedule& Out)
	{
		TArray<FProjectTierEntry> Entries;
		UOfficeStageProgressManager::BuildTierProjectEntries(Context.Industry, Tier.CurrentTier, Tier.CurrentTier,
			Tier.ClearedProjects, Entries);
		if (Entries.IsEmpty())
		{
			return;
		}
		FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
		Frame.Program = EOfficeTVProgram::TierCodex;
		for (const FProjectTierEntry& Source : Entries)
		{
			FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
			Entry.Title = Source.ProjectName;
			Entry.Artwork = Source.Cover;
			Entry.Slot = Source.ProjectIndex;
			Entry.bConcealed = !Tier.ClearedProjects.Contains(Source.ProjectIndex);
			if (const FShippedProjectRecord* Shipped = Entry.bConcealed ? nullptr : FindShipped(Context, Source.ProjectIndex))
			{
				Entry.Grade = OfficeTVBroadcastSource::GradeLetter(Shipped->QualityGrade);
			}
		}
		Frame.Count = Tier.GetTierClearedCount(Tier.CurrentTier);
		Frame.Total = FTierLayout::ClearToUnlock(Tier.CurrentTier);
		Frame.Value = FText::Format(LOCTEXT("CodexCount", "{0}/{1}"), FText::AsNumber(Frame.Count), FText::AsNumber(Frame.Total));
		if (Tier.CurrentTier < FTierLayout::MaxTier() && Tier.GetRemainingForNextTier() > 0)
		{
			Frame.Headline = FText::Format(LOCTEXT("CodexHeadline", "T{0} 승급까지"), FText::AsNumber(Tier.CurrentTier + 1));
			Frame.HeadlineValue = FText::Format(LOCTEXT("CodexRemaining", "{0}개"), FText::AsNumber(Tier.GetRemainingForNextTier()));
		}
		else
		{
			Frame.Headline = FText::Format(LOCTEXT("CodexTier", "T{0} 작품"), FText::AsNumber(Tier.CurrentTier));
		}
	}

	void AddUpcoming(const FTVContext& Context, const FProjectTierProgress& Tier, FOfficeTVSchedule& Out)
	{
		TArray<FProjectTierEntry> Entries;
		UOfficeStageProgressManager::BuildTierProjectEntries(Context.Industry, Tier.CurrentTier, Tier.CurrentTier,
			Tier.ClearedProjects, Entries);
		for (const FProjectTierEntry& Source : Entries)
		{
			FProjectData Project;
			if (Tier.ClearedProjects.Contains(Source.ProjectIndex) || !FindProject(Context.Tables, Context.Industry, Source.ProjectIndex, Project))
			{
				continue;
			}
			FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::Upcoming;
			Frame.Title = Project.ProjectName;
			Frame.Artwork = Project.Icon;
			Frame.Label = FText::FromName(Project.Genre);
			FProjectGenreRow Genre;
			if (Context.Tables->GetGenreInfo(Context.Industry, Project.Genre, Genre))
			{
				Frame.Accent = Genre.Color;
			}
			Frame.Headline = Project.SubName;
			return;
		}
	}

	void AddHallOfFame(const FTVContext& Context, FOfficeTVSchedule& Out)
	{
		TArray<const FShippedProjectRecord*> Shipped;
		CollectShipped(Context, Shipped);
		if (Shipped.IsEmpty())
		{
			return;
		}
		Shipped.Sort([](const FShippedProjectRecord& A, const FShippedProjectRecord& B)
		{
			return GradeRank(A.QualityGrade) != GradeRank(B.QualityGrade) ? GradeRank(A.QualityGrade) > GradeRank(B.QualityGrade)
				: A.CumulativeRevenue > B.CumulativeRevenue;
		});
		FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
		Frame.Program = EOfficeTVProgram::HallOfFame;
		TMap<EQualityGrade, int32> Counts;
		for (const FShippedProjectRecord* Record : Shipped)
		{
			++Counts.FindOrAdd(Record->QualityGrade);
			FProjectData Project;
			if (Frame.Entries.Num() < HallOfFameLimit && FindProject(Context.Tables, Context.Industry, Record->ProjectIndex, Project))
			{
				FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
				Entry.Title = ProjectTitle(Record->ProjectName, Project);
				Entry.Artwork = Project.Icon;
				Entry.Grade = OfficeTVBroadcastSource::GradeLetter(Record->QualityGrade);
			}
		}
		TArray<FString> Parts;
		for (const EQualityGrade Grade : {EQualityGrade::S, EQualityGrade::A, EQualityGrade::B, EQualityGrade::C, EQualityGrade::D, EQualityGrade::F})
		{
			if (const int32* Count = Counts.Find(Grade))
			{
				Parts.Add(FString::Printf(TEXT("%s %d"), *Context.Tables->GetQualityGradeLabel(Context.Industry, Grade).ToString(), *Count));
			}
		}
		Frame.Subtitle = FText::FromString(FString::Join(Parts, TEXT(" · ")));
		Frame.Headline = LOCTEXT("HallHeadline", "명예의 전당");
		Frame.HeadlineValue = FText::Format(LOCTEXT("HallCount", "{0}편"), FText::AsNumber(Shipped.Num()));
	}

	void AddTopRevenue(const FTVContext& Context, FOfficeTVSchedule& Out)
	{
		TArray<const FShippedProjectRecord*> Shipped;
		CollectShipped(Context, Shipped);
		if (Shipped.Num() < 2)
		{
			return;
		}
		Shipped.Sort([](const FShippedProjectRecord& A, const FShippedProjectRecord& B)
		{
			return A.CumulativeRevenue != B.CumulativeRevenue ? A.CumulativeRevenue > B.CumulativeRevenue : A.ProjectIndex < B.ProjectIndex;
		});
		FOfficeTVBroadcastFrame Frame;
		Frame.Program = EOfficeTVProgram::TopRevenue;
		for (const FShippedProjectRecord* Record : Shipped)
		{
			FProjectData Project;
			if (!FindProject(Context.Tables, Context.Industry, Record->ProjectIndex, Project))
			{
				continue;
			}
			FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
			Entry.Title = ProjectTitle(Record->ProjectName, Project);
			Entry.Artwork = Project.Icon;
			Entry.Value = UGlobalUtilFunctions::AbbreviateNumberFloat(static_cast<double>(Record->CumulativeRevenue));
			Entry.Slot = Frame.Entries.Num();
			if (Frame.Entries.Num() >= TopRevenueLimit)
			{
				break;
			}
		}
		if (Frame.Entries.Num() >= 2)
		{
			Frame.Headline = LOCTEXT("TopHeadline", "역대 매출");
			Frame.HeadlineValue = FText::Format(LOCTEXT("TopCount", "TOP {0}"), FText::AsNumber(Frame.Entries.Num()));
			Out.Rotation.Add(MoveTemp(Frame));
		}
	}

	void AddAnniversary(const FTVContext& Context, FOfficeTVSchedule& Out)
	{
		if (!Context.Employees)
		{
			return;
		}
		const FDateTime Now = FDateTime::Now();
		for (const FEmployeeInstance& Employee : Context.Employees->GetAllEmployeesRef())
		{
			int32 Days = 0;
			if (!InBuilding(Employee, Context.Building) || !OfficeTVBroadcastSource::IsAnniversary(Employee.HiredDate, Now, Days))
			{
				continue;
			}
			FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::Anniversary;
			PortraitFrame(Context.Employees, Employee, Frame);
			Frame.Value = FText::AsNumber(Days);
			Frame.Headline = FText::Format(LOCTEXT("AnniversaryHeadline", "{0} 근속 {1}일"), Frame.Headline, FText::AsNumber(Days));
			return;
		}
	}

	// ---------------- Operating and production ----------------

	void GatherOperations(const FTVContext& Context, const FOfficeTVNewsBuffers& News,
		OfficeTVBroadcastSource::FSupportFilter IsSupported, const FProjectTierProgress* Tier, FOfficeTVSchedule& Out)
	{
		UProjectOperationManager* Operations = Context.Game->GetSubsystem<UProjectOperationManager>();
		const FOperationData* Operation = Operations ? Operations->GetOperationByBuildingID(Context.Building) : nullptr;
		FProjectData Project;
		if (!Operation || !FindProject(Context.Tables, Context.Industry, Operation->ProjectID, Project))
		{
			return;
		}
		Out.Pool = EOfficeTVPool::Operating;
		const FText Title = ProjectTitle(Operation->ProjectName, Project);

		if (ULiveOpsSubsystem* LiveOps = Context.Game->GetSubsystem<ULiveOpsSubsystem>())
		{
			const float Multiplier = LiveOps->GetRevenueMultiplierNow();
			if (Multiplier > 1.001f && IsSupported(EOfficeTVProgram::HotTime))
			{
				int32 EndHour = INDEX_NONE;
				const UGameClockSubsystem* Clock = Context.Game->GetSubsystem<UGameClockSubsystem>();
				if (Clock && Clock->IsServerTimeValid())
				{
					const int32 Hour = Clock->GetHourKst();
					TArray<FLiveOpsEventDef> Events;
					LiveOps->GetActiveEvents(Events);
					for (const FLiveOpsEventDef& Event : Events)
					{
						for (const FLiveOpsHotTimeWindow& Window : Event.HotTime)
						{
							const bool bInside = Window.StartHourKst <= Window.EndHourKst
								? Hour >= Window.StartHourKst && Hour < Window.EndHourKst
								: Hour >= Window.StartHourKst || Hour < Window.EndHourKst;
							if (Event.Type == ELiveOpsEventType::HotTimeEvent && bInside)
							{
								EndHour = Window.EndHourKst;
							}
						}
					}
				}
				FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
				Frame.Program = EOfficeTVProgram::HotTime;
				Frame.Value = MultiplierText(Multiplier);
				Frame.Headline = EndHour != INDEX_NONE
					? FText::Format(LOCTEXT("HotTimeUntil", "{0}시까지 수익이 {1}배입니다"), FText::AsNumber(EndHour), FText::AsNumber(Multiplier))
					: FText::Format(LOCTEXT("HotTimeNow", "지금 수익이 {0}배입니다"), FText::AsNumber(Multiplier));
				Out.Opener = EOfficeTVProgram::HotTime;
			}
		}

		if (IsSupported(EOfficeTVProgram::RevenueTicker))
		{
			const float Total = FMath::Max(Operation->TotalOperationTime, 1.f);
			const float HalfLifeFraction = Operation->HalfLifeFrac > 0.f ? Operation->HalfLifeFrac : 0.5f;
			const float DecayHalf = FMath::Max(1.f, Total * HalfLifeFraction);
			// Same decay model as the office sales curve: the peak is the launch-time rate.
			const float DecayNow = FMath::Max(FMath::Pow(0.5f, Operation->ElapsedTime / DecayHalf), 0.02f);
			// The office top bar's rate: decayed, without the per-second wobble of ActualRevenuePerSecond.
			const float Rate = FMath::Max(0.f, Operations->GetBuildingDisplayNetPerSec(Context.Building));
			FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::RevenueTicker;
			Frame.Value = FText::Format(LOCTEXT("RevenuePerSecond", "+{0}"), StripMoney(Rate));
			Frame.SecondaryValue = StripMoney(Rate / DecayNow);
			for (int32 Index = 0; Index < RevenueBars; ++Index)
			{
				const float Time = Operation->ElapsedTime * Index / static_cast<float>(RevenueBars - 1);
				FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
				Entry.Ratio = FMath::Pow(0.5f, Time / DecayHalf);
				Entry.bHighlighted = Index == RevenueBars - 1;
			}
			Frame.Headline = FText::Format(LOCTEXT("OperatingHeadline", "{0} 운영 중"), Title);
		}

		if (IsSupported(EOfficeTVProgram::Operations))
		{
			FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::Operations;
			// Employee income raises the total between ticks; the top bar only redraws on the tick.
			Frame.Value = StripMoney(News.TickBuilding == Context.Building ? News.TickTotalRevenue : Operation->TotalRevenueEarned);
			Frame.SecondaryValue = ClockText(Operation->RemainingTime);
			Frame.Progress = Operation->TotalOperationTime > 0.f
				? FMath::Clamp(Operation->RemainingTime / Operation->TotalOperationTime, 0.f, 1.f) : 0.f;
			Frame.Headline = Title;
		}

		if (IsSupported(EOfficeTVProgram::Advertisement))
		{
			AddAdvertisement(Context, Operation->ProjectID, Operation->ProjectName, Out);
		}
		if (Tier && IsSupported(EOfficeTVProgram::CompanyNews))
		{
			AddNextTitle(Context, *Tier, Out);
		}
	}

	bool GatherProduction(const FTVContext& Context, OfficeTVBroadcastSource::FSupportFilter IsSupported, FOfficeTVSchedule& Out)
	{
		UProductionOrderManager* Orders = Context.Game->GetSubsystem<UProductionOrderManager>();
		if (!Orders || !IsManufacturingType(Context.Industry))
		{
			return false;
		}
		const TArray<FProductionOrder> BuildingOrders = Orders->GetOrdersByBuilding(Context.Building);
		const FProductionOrder* Active = nullptr;
		for (const FProductionOrder& Order : BuildingOrders)
		{
			if (!Order.IsConsumed() && (!Active || Order.OrderID > Active->OrderID))
			{
				Active = &Order;
			}
		}
		if (!Active)
		{
			return false;
		}
		Out.Pool = EOfficeTVPool::Producing;
		if (IsSupported(EOfficeTVProgram::ProductionLine))
		{
			FOfficeTVBroadcastFrame& Frame = Out.Rotation.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::ProductionLine;
			Frame.Title = FText::FromString(Active->ProductName);
			Frame.Count = FMath::Max(0, Active->Quantity - Active->RemainingQuantity);
			Frame.Total = Active->Quantity;
			Frame.Value = FText::AsNumber(Frame.Count);
			Frame.SecondaryValue = FText::Format(LOCTEXT("ProductionTotal", "/{0}"), FText::AsNumber(Frame.Total));
			Frame.Grade = OfficeTVBroadcastSource::GradeLetter(Active->Grade);
			Frame.GradeLabel = Context.Tables->GetQualityGradeLabel(Active->CompanyType, Active->Grade);
			Frame.Headline = Frame.Title;
			FProjectData Project;
			if (FindProject(Context.Tables, Active->CompanyType, Active->ProjectIndex, Project))
			{
				Frame.Artwork = Project.Icon;
			}
		}
		return true;
	}
}

namespace OfficeTVBroadcastSource
{
	using OfficeTVSource::FTVContext;
	using OfficeTVSource::FindProject;
	using OfficeTVSource::ProjectTitle;
	using OfficeTVSource::GatherDevelopment;
	using OfficeTVSource::GatherOperations;
	using OfficeTVSource::GatherProduction;
	using OfficeTVSource::AddNextTitle;
	using OfficeTVSource::AddTierCodex;
	using OfficeTVSource::AddLastShippedAdvertisement;
	using OfficeTVSource::AddUpcoming;
	using OfficeTVSource::AddHallOfFame;
	using OfficeTVSource::AddTopRevenue;
	using OfficeTVSource::AddAnniversary;
	using OfficeTVSource::CollectParticipants;
	using OfficeTVSource::FindEmployee;
	using OfficeTVSource::PortraitEntry;
	using OfficeTVSource::DurationText;
	using OfficeTVSource::EnhancementNames;
	using OfficeTVSource::PortraitFrame;
	using OfficeTVSource::LevelBadge;
	using OfficeTVSource::JobTitle;
	using OfficeTVSource::MultiplierText;

	void Gather(UWorld* World, const FOfficeTVNewsBuffers& News, FSupportFilter IsSupported, FOfficeTVSchedule& Out)
	{
		Out = FOfficeTVSchedule();
		FTVContext Context;
		if (!Context.Resolve(World))
		{
			return;
		}
		const FProjectTierProgress* Tier = Context.TierProgress();
		const EProjectLifecycle Lifecycle = Context.Stage ? Context.Stage->GetLifecycle() : EProjectLifecycle::Idle;
		if (Context.Stage && (Lifecycle == EProjectLifecycle::Developing || Lifecycle == EProjectLifecycle::LaunchPending))
		{
			GatherDevelopment(Context, News, IsSupported, Out);
			return;
		}
		if (Lifecycle == EProjectLifecycle::Operating && !IsManufacturingType(Context.Industry))
		{
			GatherOperations(Context, News, IsSupported, Tier, Out);
			return;
		}
		if (Lifecycle != EProjectLifecycle::Idle)
		{
			return;
		}
		if (GatherProduction(Context, IsSupported, Out))
		{
			if (Tier && IsSupported(EOfficeTVProgram::CompanyNews)) { AddNextTitle(Context, *Tier, Out); }
			if (Tier && IsSupported(EOfficeTVProgram::TierCodex)) { AddTierCodex(Context, *Tier, Out); }
			return;
		}
		Out.Pool = EOfficeTVPool::Idle;
		if (Tier && IsSupported(EOfficeTVProgram::Advertisement)) { AddLastShippedAdvertisement(Context, *Tier, Out); }
		if (Tier && IsSupported(EOfficeTVProgram::CompanyNews)) { AddNextTitle(Context, *Tier, Out); }
		if (Tier && IsSupported(EOfficeTVProgram::TierCodex)) { AddTierCodex(Context, *Tier, Out); }
		if (Tier && IsSupported(EOfficeTVProgram::Upcoming)) { AddUpcoming(Context, *Tier, Out); }
		if (IsSupported(EOfficeTVProgram::HallOfFame)) { AddHallOfFame(Context, Out); }
		if (IsSupported(EOfficeTVProgram::TopRevenue)) { AddTopRevenue(Context, Out); }
		if (IsSupported(EOfficeTVProgram::Anniversary)) { AddAnniversary(Context, Out); }
	}

	bool CaptureLaunchRecord(UWorld* World, FOfficeTVLaunchRecord& Out)
	{
		Out = FOfficeTVLaunchRecord();
		FTVContext Context;
		if (!Context.Resolve(World) || !Context.Stage || Context.Stage->GetLifecycle() != EProjectLifecycle::LaunchPending)
		{
			return false;
		}
		const FStageProgressData& Progress = Context.Stage->GetProgressData();
		if (Progress.CompanyType != Context.Industry)
		{
			return false;
		}
		Out.ReviewScore = Progress.ReviewScore;
		Out.CriticScores = Progress.CriticScores;
		Out.bFirstDiscovery = Progress.bFirstDiscovery;
		Out.Genre = Progress.Genre;
		Out.Material = Progress.Material;
		Out.DevelopmentSeconds = Context.Stage->GetTotalDuration();
		CollectParticipants(Context, Out.Participants);
		Out.bValid = true;
		return true;
	}

	void BuildLaunchSequence(UWorld* World, const FOfficeTVLaunchRecord& Record, TArray<FOfficeTVBroadcastFrame>& Out)
	{
		Out.Reset();
		FTVContext Context;
		if (!Context.Resolve(World) || IsManufacturingType(Context.Industry))
		{
			return;
		}
		UProjectOperationManager* Operations = Context.Game->GetSubsystem<UProjectOperationManager>();
		const FOperationData* Operation = Operations ? Operations->GetOperationByBuildingID(Context.Building) : nullptr;
		FProjectData Project;
		if (!Operation || !FindProject(Context.Tables, Context.Industry, Operation->ProjectID, Project))
		{
			return;
		}
		const FText Title = ProjectTitle(Operation->ProjectName, Project);

		FOfficeTVBroadcastFrame& Celebration = Out.AddDefaulted_GetRef();
		Celebration.Program = EOfficeTVProgram::LaunchCelebration;
		Celebration.GradeLabel = Context.Tables->GetQualityGradeLabel(Context.Industry, Operation->QualityGrade);
		Celebration.Artwork = Project.Icon;
		Celebration.Grade = GradeLetter(Operation->QualityGrade);
		Celebration.Headline = FText::Format(LOCTEXT("LaunchHeadline", "{0} 출시했어요!"), Title);

		if (!Record.bValid)
		{
			return;
		}
		const TArray<FText> Critics = Context.Tables->GetCriticNames(Context.Industry);
		if (!Record.CriticScores.IsEmpty() && Critics.Num() >= Record.CriticScores.Num())
		{
			FOfficeTVBroadcastFrame& Frame = Out.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::CriticScores;
			const int32 Best = FMath::Max(Record.CriticScores);
			for (int32 Index = 0; Index < Record.CriticScores.Num(); ++Index)
			{
				FOfficeTVBroadcastEntry& Entry = Frame.Entries.AddDefaulted_GetRef();
				Entry.Title = Critics[Index];
				Entry.Value = FText::AsNumber(Record.CriticScores[Index]);
				Entry.bHighlighted = Record.CriticScores[Index] == Best;
			}
			Frame.Headline = FText::Format(LOCTEXT("CriticHeadline", "{0} 평점"), Title);
			Frame.HeadlineValue = FText::Format(LOCTEXT("CriticTotal", "{0}/{1}"), FText::AsNumber(Record.ReviewScore),
				FText::AsNumber(Record.CriticScores.Num() * 10));
		}

		if (Record.bFirstDiscovery && !Record.Genre.IsNone() && !Record.Material.IsNone())
		{
			FOfficeTVBroadcastFrame& Frame = Out.AddDefaulted_GetRef();
			Frame.Program = EOfficeTVProgram::FirstDiscovery;
			FOfficeTVBroadcastEntry& GenreEntry = Frame.Entries.AddDefaulted_GetRef();
			GenreEntry.Title = FText::FromName(Record.Genre);
			FProjectGenreRow Genre;
			if (Context.Tables->GetGenreInfo(Context.Industry, Record.Genre, Genre))
			{
				GenreEntry.Accent = Genre.Color;
			}
			FOfficeTVBroadcastEntry& MaterialEntry = Frame.Entries.AddDefaulted_GetRef();
			MaterialEntry.Title = FText::FromName(Record.Material);
			Frame.Headline = FText::Format(LOCTEXT("DiscoveryHeadline", "{0} 첫 발견"), Title);
		}

		if (!Record.Participants.IsEmpty() && Context.Employees)
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Credits;
			Frame.Title = Title;
			for (const TPair<int32, int32>& Participant : Record.Participants)
			{
				if (const FEmployeeInstance* Employee = FindEmployee(Context.Employees, Participant.Key))
				{
					FOfficeTVBroadcastEntry Entry = PortraitEntry(Context.Employees, *Employee);
					Entry.Value = FText::AsNumber(Participant.Value);
					Frame.Entries.Add(MoveTemp(Entry));
				}
			}
			if (Record.DevelopmentSeconds > 0.f)
			{
				Frame.SecondaryValue = DurationText(Record.DevelopmentSeconds);
			}
			if (!Frame.Entries.IsEmpty())
			{
				Out.Add(MoveTemp(Frame));
			}
		}
	}

	bool BuildTierUp(UWorld* World, int32 NewTier, FOfficeTVBroadcastFrame& Out)
	{
		FTVContext Context;
		if (!Context.Resolve(World) || NewTier <= 1)
		{
			return false;
		}
		Out = FOfficeTVBroadcastFrame();
		Out.Program = EOfficeTVProgram::TierUp;
		Out.Grade = FName(*FString::Printf(TEXT("T%d"), NewTier));
		Out.Title = Context.Tables->GetTierRankTitle(Context.Industry, NewTier);
		Out.Value = EnhancementNames(Context, NewTier);
		Out.Headline = FText::Format(LOCTEXT("TierUpHeadline", "T{0} 승급을 축하합니다"), FText::AsNumber(NewTier));
		return !Out.Title.IsEmpty();
	}

	bool BuildCritical(UWorld* World, int32 EmployeeId, int32 Slot, float Score, FOfficeTVBroadcastFrame& Out)
	{
		FTVContext Context;
		const FEmployeeInstance* Employee = Context.Resolve(World) ? FindEmployee(Context.Employees, EmployeeId) : nullptr;
		if (!Employee)
		{
			return false;
		}
		Out = FOfficeTVBroadcastFrame();
		Out.Program = EOfficeTVProgram::CriticalFlash;
		PortraitFrame(Context.Employees, *Employee, Out);
		Out.IconSlot = Slot;
		Out.Value = FText::Format(LOCTEXT("CriticalScore", "+{0}"), FText::AsNumber(FMath::Max(1, FMath::RoundToInt(Score))));
		return true;
	}

	bool BuildReaction(UWorld* World, const FOfficeTVBroadcastEntry& Reaction, FOfficeTVBroadcastFrame& Out)
	{
		FTVContext Context;
		if (!Context.Resolve(World))
		{
			return false;
		}
		UProjectOperationManager* Operations = Context.Game->GetSubsystem<UProjectOperationManager>();
		const FOperationData* Operation = Operations ? Operations->GetOperationByBuildingID(Context.Building) : nullptr;
		FProjectData Project;
		if (!Operation || !FindProject(Context.Tables, Context.Industry, Operation->ProjectID, Project))
		{
			return false;
		}
		Out = FOfficeTVBroadcastFrame();
		Out.Program = EOfficeTVProgram::Reactions;
		Out.Entries.Add(Reaction);
		Out.Headline = FText::Format(LOCTEXT("ReactionHeadline", "{0} 반응"), ProjectTitle(Operation->ProjectName, Project));
		return true;
	}

	bool BuildFever(UWorld* World, FOfficeTVBroadcastFrame& Out)
	{
		FTVContext Context;
		if (!Context.Resolve(World) || !Context.Stage)
		{
			return false;
		}
		const FStageProgressData& Progress = Context.Stage->GetProgressData();
		FProjectData Project;
		Out = FOfficeTVBroadcastFrame();
		Out.Program = EOfficeTVProgram::Fever;
		if (FindProject(Context.Tables, Context.Industry, Progress.ProjectID, Project))
		{
			Out.Artwork = Project.Icon;
		}
		const float Multiplier = Context.Stage->GetFeverScoreMultiplier();
		Out.Value = MultiplierText(Multiplier);
		Out.Headline = Multiplier > 1.001f
			? FText::Format(LOCTEXT("FeverHeadline", "작업 점수가 {0}배로 올라갑니다"), FText::AsNumber(Multiplier))
			: LOCTEXT("FeverPlain", "작업 점수가 올라갑니다");
		return true;
	}

	bool BuildIssueResult(UWorld* World, const FProjectEventData& Event, int32 ChoiceIndex, FOfficeTVBroadcastFrame& Out)
	{
		FTVContext Context;
		if (!Context.Resolve(World) || !Event.Choices.IsValidIndex(ChoiceIndex) || Event.EventTitle.IsEmpty())
		{
			return false;
		}
		const FProjectEventChoice& Choice = Event.Choices[ChoiceIndex];
		Out = FOfficeTVBroadcastFrame();
		Out.Program = EOfficeTVProgram::IssueResult;
		Out.Title = Event.EventTitle;
		Out.Subtitle = Choice.ChoiceText;
		const FString Effect = UEventChoiceCardWidget::BuildEffectSummaryText(Choice,
			UEventChoiceCardWidget::CountWorkersInWorld(World), UEventChoiceCardWidget::GetActiveProjectMode(World));
		Out.Value = Effect.IsEmpty() ? LOCTEXT("NoEffect", "변화 없음") : FText::FromString(Effect);
		FProjectData Project;
		const FStageProgressData* Progress = Context.Stage ? &Context.Stage->GetProgressData() : nullptr;
		Out.Headline = Progress && FindProject(Context.Tables, Context.Industry, Progress->ProjectID, Project)
			? FText::Format(LOCTEXT("IssueHeadline", "{0} 돌발 상황"), ProjectTitle(Progress->ProjectName, Project))
			: LOCTEXT("IssuePlain", "돌발 상황");
		return true;
	}

	bool BuildPromotion(UWorld* World, int32 EmployeeId, int32 OldLevel, int32 NewLevel, FOfficeTVBroadcastFrame& Out)
	{
		FTVContext Context;
		const FEmployeeInstance* Employee = Context.Resolve(World) ? FindEmployee(Context.Employees, EmployeeId) : nullptr;
		if (!Employee || UEmployeeTypeHelper::GetJobTitleFromLevel(OldLevel) == UEmployeeTypeHelper::GetJobTitleFromLevel(NewLevel))
		{
			return false;
		}
		Out = FOfficeTVBroadcastFrame();
		Out.Program = EOfficeTVProgram::Promotion;
		PortraitFrame(Context.Employees, *Employee, Out);
		Out.Subtitle = JobTitle(OldLevel);
		Out.Title = JobTitle(NewLevel);
		Out.Value = LevelBadge(NewLevel);
		return true;
	}

	bool BuildNewHire(UWorld* World, int32 EmployeeId, FOfficeTVBroadcastFrame& Out)
	{
		FTVContext Context;
		const FEmployeeInstance* Employee = Context.Resolve(World) ? FindEmployee(Context.Employees, EmployeeId) : nullptr;
		if (!Employee || (Employee->bIsAssigned && Employee->AssignedBuildingIndex != Context.Building))
		{
			return false;
		}
		Out = FOfficeTVBroadcastFrame();
		Out.Program = EOfficeTVProgram::NewHire;
		PortraitFrame(Context.Employees, *Employee, Out);
		Out.Title = FText::FromString(Employee->EmployeeName);
		Out.Subtitle = FText::Format(LOCTEXT("NewHireLine", "{0} · {1}"), JobTitle(Employee->Level),
			FText::FromString(FLootBoxRarityUtility::GetKoreanName(Employee->SpawnRarity)));
		Out.Headline = LOCTEXT("NewHireHeadline", "새 식구가 들어왔습니다");
		return true;
	}

	float CountdownWindow(float TotalSeconds)
	{
		return FMath::Clamp(TotalSeconds * 0.3f, 3.f, 10.f);
	}

	bool IsAnniversary(const FDateTime& Hired, const FDateTime& Now, int32& OutDays)
	{
		OutDays = (Now.GetDate() - Hired.GetDate()).GetDays();
		return Hired.GetTicks() > 0 && (OutDays == 30 || OutDays == 100 || OutDays == 365);
	}

	FName GradeLetter(EQualityGrade Grade)
	{
		return FName(*QualityGradeToAlphabetString(Grade));
	}

	FString SubjectParticle(const FString& Word)
	{
		const TCHAR Last = Word.IsEmpty() ? TEXT('\0') : Word[Word.Len() - 1];
		const bool bHangul = Last >= 0xAC00 && Last <= 0xD7A3;
		return bHangul && (Last - 0xAC00) % 28 != 0 ? TEXT("이") : TEXT("가");
	}
}

#undef LOCTEXT_NAMESPACE
