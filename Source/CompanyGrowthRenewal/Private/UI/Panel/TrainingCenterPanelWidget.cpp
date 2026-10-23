#include "UI/Panel/TrainingCenterPanelWidget.h"
#include "UI/UIBase.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Enum/WidgetType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SideContentSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Table/SideDungeonLootTable.h"
#include "Table/TrainingCourseTable.h"
#include "Table/TrainingTopicTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/LiveOps/CourseStepWidget.h"
#include "UI/Element/LiveOps/TrainingRewardChipWidget.h"

namespace
{
	// 판정 문구가 크림 플레이트 위로 올라가 밝은 의미색이 무너진다 — 라이트 면 전용 진한 잉크를 쓴다.
	const FLinearColor CourseInkPass(0.0137f, 0.0929f, 0.0382f, 1.0f);
	const FLinearColor CourseInkMiss(0.1946f, 0.0232f, 0.0130f, 1.0f);
	const FLinearColor CourseInkMaxed(0.1471f, 0.0685f, 0.0018f, 1.0f);

	// 판정 바 색은 사이드 컨텐츠 네 모듈 공통값이다 (카탈로그 §4-A · 미달 권위색 #B04A3E).
	const FLinearColor BarFillPass(0.263f, 0.788f, 0.369f, 1.0f);
	const FLinearColor BarFillMiss(0.434f, 0.068f, 0.048f, 1.0f);

	FText FormatTrainingTopic(const UTableManagerSubsystem* TableMgr, ECompanyType Company, int32 Course)
	{
		int32 Level = 0;
		const FTrainingTopicRow* Topic = TableMgr ? TableMgr->FindTrainingTopic(Company, Course, Level) : nullptr;
		return Topic && !Topic->DisplayName.IsEmpty() ? FText::Format(NSLOCTEXT("Training", "TopicLevel", "{0} · Lv.{1}"),
			Topic->DisplayName, FText::AsNumber(Level)) : FText::GetEmpty();
	}

	// 지급이 끝난 보상을 표시용으로만 합쳐 동일 품목이 토스트 한 칸을 쓰게 한다.
	TArray<FMissionReward> BuildTrainingToastRewards(const TArray<FMissionReward>& Rewards)
	{
		TArray<FMissionReward> Merged;
		for (const CGRewardChip::FDisplayEntry& Entry : CGRewardChip::BuildDisplayEntries(Rewards))
		{
			FMissionReward* Existing = Merged.FindByPredicate([&Entry](const FMissionReward& Candidate)
			{
				return Candidate.ItemType == Entry.ItemType && Candidate.ResourceType == Entry.ResourceType;
			});
			if (!Existing)
			{
				Existing = &Merged.AddDefaulted_GetRef();
				Existing->ItemType = Entry.ItemType;
				Existing->ResourceType = Entry.ResourceType;
			}
			if (Entry.ItemType != EItemType::None) Existing->ItemAmount += static_cast<int32>(Entry.Quantity);
			else Existing->Amount += Entry.Quantity;
		}
		return Merged;
	}
}

UTrainingCenterPanelWidget::UTrainingCenterPanelWidget()
{
	bIsBackHandler = true;
}

void UTrainingCenterPanelWidget::Configure(int32 InBuildingIndex, ECompanyType InCompanyType)
{
	BuildingIndex = InBuildingIndex;
	CompanyType = InCompanyType;
	RefreshAll();
}

void UTrainingCenterPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ChallengeButton) ChallengeButton->OnClicked().AddUObject(this, &UTrainingCenterPanelWidget::HandleChallenge);
	if (SweepButton) SweepButton->OnClicked().AddUObject(this, &UTrainingCenterPanelWidget::HandleSweep);
	if (SweepButtonPromoted) SweepButtonPromoted->OnClicked().AddUObject(this, &UTrainingCenterPanelWidget::HandleSweep);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USideContentSubsystem* Side = GI->GetSubsystem<USideContentSubsystem>())
		{
			Side->OnSideContentChanged.AddUObject(this, &UTrainingCenterPanelWidget::RefreshAll);
		}
	}
}

void UTrainingCenterPanelWidget::NativeDestruct()
{
	if (ChallengeButton) ChallengeButton->OnClicked().RemoveAll(this);
	if (SweepButton) SweepButton->OnClicked().RemoveAll(this);
	if (SweepButtonPromoted) SweepButtonPromoted->OnClicked().RemoveAll(this);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USideContentSubsystem* Side = GI->GetSubsystem<USideContentSubsystem>())
		{
			Side->OnSideContentChanged.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

namespace
{
	// 비활성(회색 채움)은 왜 안 되는지 못 알린다 — 버튼은 살려 두고 사유를 걸어 클릭만 삼킨다.
	void GateActionButton(UButtonWidget* Button, bool bUsable, const FText& Reason)
	{
		if (!Button) return;
		Button->SetIsEnabled(true);
		if (bUsable) Button->ClearRejectReason();
		else Button->SetRejectReason(Reason);
	}
}

bool UTrainingCenterPanelWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	ClosePanel();
	return true;
}

UWidget* UTrainingCenterPanelWidget::NativeGetDesiredFocusTarget() const
{
	for (UButtonWidget* ActionButton : { ChallengeButton.Get(), SweepButtonPromoted.Get(), SweepButton.Get() })
	{
		if (ActionButton && ActionButton->IsVisible() && !ActionButton->HasRejectReason()) return ActionButton;
	}
	return GetPanelCloseButton() ? GetPanelCloseButton() : Super::NativeGetDesiredFocusTarget();
}

void UTrainingCenterPanelWidget::RefreshRewardPreview(int32 Course)
{
	if (!RewardBox) return;
	RewardBox->ClearChildren();

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;
	const TSubclassOf<UUserWidget> ChipClass = TableMgr->GetWidgetClass(EWidgetType::TrainingRewardChip);
	if (!ChipClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("TrainingRewardChip is missing from DT_WidgetClass"));
		return;
	}
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	const int32 HQLevel = SaveMgr ? SaveMgr->GetHQLevel() : 1;
	const int32 CompanyLevel = SaveMgr ? SaveMgr->GetBuildingTier(BuildingIndex) : 0;

	// 추첨과 같은 후보 게이트를 사용한다. 확률 밴드는 Weight를 재계산하지 않고 행의 Odds를 읽는다.
	TArray<FSideDungeonLootRow> PreviewRows;
	for (const FSideDungeonLootRow& Row : TableMgr->GetSideDungeonLootRows(TEXT("Training")))
	{
		if (Course < Row.MinStage || Course > Row.MaxStage) continue;
		if (Row.Weight <= 0.0f || HQLevel < Row.RequiredHQLevel || CompanyLevel < Row.RequiredCompanyLevel) continue;
		if (Row.ItemType != EItemType::None && FMath::Max(Row.ItemAmountMin, Row.ItemAmountMax) > 0)
		{
			PreviewRows.Add(Row);
		}
		if (Row.ResourceType != EResourceType::None && FMath::Max(Row.ResourceAmountMin, Row.ResourceAmountMax) > 0)
		{
			FSideDungeonLootRow ResourceRow = Row;
			ResourceRow.ItemType = EItemType::None;
			PreviewRows.Add(ResourceRow);
		}
	}

	for (int32 Index = 0; Index < PreviewRows.Num(); ++Index)
	{
		UTrainingRewardChipWidget* Chip = CreateWidget<UTrainingRewardChipWidget>(GetOwningPlayer(), ChipClass);
		if (!Chip) continue;
		RewardBox->AddChildToUniformGrid(Chip, Index / 2, Index % 2);
		Chip->ConfigurePreview(PreviewRows[Index]);
	}
}

void UTrainingCenterPanelWidget::RefreshCourseRail(int32 Course, int32 Best, int32 MaxCourse)
{
	if (!CourseRailBox) return;
	CourseRailBox->ClearChildren();

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	USideContentSubsystem* Side = GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
	if (!TableMgr || !Side) return;

	const TSubclassOf<UUserWidget> RowClass = TableMgr->GetWidgetClass(EWidgetType::CourseStep);
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("CourseStep is missing from DT_WidgetClass"));
		return;
	}

	int32 Level = 0;
	const FTrainingTopicRow* Topic = TableMgr->FindTrainingTopic(CompanyType, Course, Level);
	if (!Topic) return;

	// Sequence 가 곧 Lv 안의 순번이라 레벨의 첫 과정은 뺄셈 하나로 나온다.
	const int32 LevelStart = Course - (Topic->Sequence - 1);
	int32 DoneInLevel = 0;
	int32 CountInLevel = 0;

	for (int32 Step = LevelStart; Step <= MaxCourse; ++Step)
	{
		int32 StepLevel = 0;
		const FTrainingTopicRow* StepTopic = TableMgr->FindTrainingTopic(CompanyType, Step, StepLevel);
		if (!StepTopic || StepLevel != Level) break;   // 다음 Lv 로 넘어갔다

		UCourseStepWidget* Row = CreateWidget<UCourseStepWidget>(GetOwningPlayer(), RowClass);
		if (!Row) continue;
		UVerticalBoxSlot* RowSlot = CourseRailBox->AddChildToVerticalBox(Row);
		// 첫 행만 위 여백 0 — 나머지는 행끼리 붙지 않게 띄운다
		if (RowSlot) RowSlot->SetPadding(FMargin(0.f, Step == LevelStart ? 0.f : 12.f, 0.f, 0.f));

		const bool bStepDone = Step <= Best;
		const ECourseStepState State = bStepDone ? ECourseStepState::Done
			: (Step == Course && Best < MaxCourse) ? ECourseStepState::Current : ECourseStepState::Next;
		const FText RequirementLabel = FText::Format(NSLOCTEXT("Training", "RailRequirement", "요구 {0}"),
			UGlobalUtilFunctions::AbbreviateNumber(FMath::CeilToInt64(Side->GetCourseRequirement(Step)),
				ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil));
		Row->SetCourse(StepTopic->Sequence, StepTopic->DisplayName, RequirementLabel, State);

		++CountInLevel;
		if (bStepDone) ++DoneInLevel;
	}

	if (RailLevelText)
	{
		RailLevelText->SetText(FText::Format(NSLOCTEXT("Training", "RailLevel", "Lv.{0}"), FText::AsNumber(Level)));
	}
	if (RailProgressText)
	{
		RailProgressText->SetText(FText::Format(NSLOCTEXT("Training", "RailProgress", "{0} / {1} 완료"),
			FText::AsNumber(DoneInLevel), FText::AsNumber(CountInLevel)));
	}
}

void UTrainingCenterPanelWidget::RefreshAll()
{
	if (BuildingIndex == INDEX_NONE) return;

	UGameInstance* GI = GetGameInstance();
	USideContentSubsystem* Side = GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!Side || !EmployeeMgr) return;

	// ── 컨텍스트 밴드 ──
	if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
	{
		bool bHasInfo = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(CompanyType, bHasInfo);
		if (bHasInfo)
		{
			if (IndustryWell) IndustryWell->SetColorAndOpacity(Info.AccentColor);
			if (IndustryGlyph && !Info.GlyphIcon.IsNull())
			{
				if (UTexture2D* Glyph = Info.GlyphIcon.LoadSynchronous()) IndustryGlyph->SetBrushFromTexture(Glyph);
			}
		}
	}

	const int32 TeamCount = EmployeeMgr->GetEmployeesInBuilding(BuildingIndex).Num();
	const float TeamPower = Side->GetTrainingTeamPower(BuildingIndex);
	const int32 Entries = Side->GetTrainingEntries(BuildingIndex);
	const int32 Course = Side->GetCurrentCourse(BuildingIndex);
	const int32 Best = Side->GetCourseBest(BuildingIndex);
	const float Required = Side->GetCourseRequirement(Course);
	const ETrainingBlockReason Reason = Side->GetTrainingBlockReason(BuildingIndex);
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	const int32 MaxCourse = TableMgr ? TableMgr->GetTrainingMaxCourse() : 0;
	const FTrainingCourseRow* Band = TableMgr ? TableMgr->FindTrainingCourseBand(Course) : nullptr;
	int32 TopicLevel = 0;
	const FTrainingTopicRow* Topic = TableMgr ? TableMgr->FindTrainingTopic(CompanyType, Course, TopicLevel) : nullptr;
	const bool bMaxed = MaxCourse > 0 && Best >= MaxCourse;
	const bool bCanGo = Side->CanAdvance(BuildingIndex) && !bMaxed;
	const bool bBlocked = (Reason == ETrainingBlockReason::NoCrew)
		|| (Reason == ETrainingBlockReason::NoEntry)
		|| (Reason == ETrainingBlockReason::CompanyLocked);

	if (TeamCountText) TeamCountText->SetText(FText::AsNumber(TeamCount));
	const FText TeamPowerDisplay = UGlobalUtilFunctions::AbbreviateNumber(FMath::FloorToInt64(TeamPower));
	const FText RequirementDisplay = UGlobalUtilFunctions::AbbreviateNumber(FMath::CeilToInt64(Required),
		ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil);
	if (EntryChipText)
	{
		// 칩 자체가 `입장권` 라벨을 이미 달고 있다 — 문구에 다시 넣으면 두 번 찍힌다.
		EntryChipText->SetText(FText::Format(NSLOCTEXT("Training", "Entries", "{0}/5 보유"),
			FText::AsNumber(Entries)));
	}
	if (RefillNote)
	{
		RefillNote->SetVisibility(Entries == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// ── 현재 연구 목표 ──
	if (CurrentResearchText)
	{
		CurrentResearchText->SetText(Topic ? Topic->DisplayName : FText::GetEmpty());
		CurrentResearchText->SetVisibility(Topic && !Topic->DisplayName.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CurrentResearchLabelText)
	{
		// Lv 은 레일 머리(RailLevelText)가 이미 말한다 — 여기서 다시 붙이면 같은 화면에 두 번 나온다.
		CurrentResearchLabelText->SetText(Topic && !Topic->DisplayName.IsEmpty()
			? (bMaxed ? NSLOCTEXT("Training", "CurrentCompleted", "최종 연구")
				: NSLOCTEXT("Training", "CurrentResearch", "현재 연구"))
			: FText::GetEmpty());
		CurrentResearchLabelText->SetVisibility(Topic && !Topic->DisplayName.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (RequirementValueText) RequirementValueText->SetText(RequirementDisplay);
	if (TeamPowerValueText)
	{
		TeamPowerValueText->SetText(TeamPowerDisplay);
		TeamPowerValueText->SetColorAndOpacity(bMaxed ? CourseInkMaxed : (bCanGo ? CourseInkPass : CourseInkMiss));
	}
	if (PowerBar)
	{
		PowerBar->SetPercent(Required > 0.0f ? FMath::Clamp(TeamPower / Required, 0.0f, 1.0f) : 1.0f);
		PowerBar->SetFillColorAndOpacity(bMaxed || bCanGo ? BarFillPass : BarFillMiss);
	}
	if (ResearchStatusText)
	{
		const FText Shortfall = UGlobalUtilFunctions::AbbreviateNumber(FMath::Max<int64>(1, FMath::CeilToInt64(Required - TeamPower)),
			ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil);
		ResearchStatusText->SetText(
			Reason == ETrainingBlockReason::CompanyLocked ? NSLOCTEXT("Training", "StatusCompanyLocked", "회사 성장 필요")
			: Reason == ETrainingBlockReason::NoCrew ? NSLOCTEXT("Training", "StatusNoCrew", "직원 배치 필요")
			: Reason == ETrainingBlockReason::NoEntry ? NSLOCTEXT("Training", "StatusNoEntry", "입장권 없음")
			: bMaxed ? NSLOCTEXT("Training", "StatusMaxed", "모든 연구 완료")
			: bCanGo ? NSLOCTEXT("Training", "StatusReady", "시작 가능")
			: FText::Format(NSLOCTEXT("Training", "NoteShortfall", "팀 파워 {0} 부족"), Shortfall));
		ResearchStatusText->SetColorAndOpacity(
			bBlocked ? CourseInkMiss : bMaxed ? CourseInkMaxed : bCanGo ? CourseInkPass : CourseInkMiss);
	}
	if (NextGrowthText)
	{
		const FTrainingCourseRow* NextBand = Band && Band->MaxCourse < MaxCourse
			? TableMgr->FindTrainingCourseBand(Band->MaxCourse + 1) : nullptr;
		NextGrowthText->SetText(NextBand
			? FText::Format(NSLOCTEXT("Training", "NextGrowthDistance", "연구 {0}개 후 · 보상 {1}회 추첨"),
				FText::AsNumber(NextBand->MinCourse - Course), FText::AsNumber(NextBand->Rolls))
			: FText::GetEmpty());
		NextGrowthText->SetVisibility(NextBand ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	RefreshCourseRail(Course, Best, MaxCourse);

	// ── 보상 플레이트 ──
	const bool bShowSweep = !bCanGo;
	const int32 RewardCourse = bShowSweep && Best > 0 ? Best : Course;
	const FTrainingCourseRow* RewardBand = TableMgr ? TableMgr->FindTrainingCourseBand(RewardCourse) : nullptr;
	if (RewardTitleText)
	{
		RewardTitleText->SetText(bShowSweep && Best > 0
			? NSLOCTEXT("Training", "RewardTitleCompleted", "완료 기술 보상")
			: NSLOCTEXT("Training", "RewardTitleAvailable", "획득 가능 보상"));
	}
	if (RewardHintText)
	{
		const FText RollHint = RewardBand
			? FText::Format(NSLOCTEXT("Training", "RollHint", "{0}회 추첨 · 수량은 1회 기준"), FText::AsNumber(RewardBand->Rolls))
			: FText::GetEmpty();
		RewardHintText->SetText(RollHint);
	}
	RefreshRewardPreview(RewardCourse);

	// ── 푸터: 충족이면 [시작] 하나, 미달·최종에서는 [성과 수령]도 표시 ──
	// 최종 연구에서는 성과 수령이 유일한 행동이므로 주 CTA로 표시한다.
	const bool bPromoteSweep = bShowSweep && bMaxed;
	const bool bSweepUsable = !bBlocked && Best > 0;
	const FText GateReason =
		(Reason == ETrainingBlockReason::NoCrew) ? NSLOCTEXT("Training", "GateNoCrew", "직원을 먼저 배치해 주세요.")
		: (Reason == ETrainingBlockReason::NoEntry) ? NSLOCTEXT("Training", "GateNoEntry", "입장권이 없습니다.")
		: NSLOCTEXT("Training", "GateCompany", "회사 레벨이 모자랍니다.");
	if (SweepButton)
	{
		SweepButton->SetVisibility(bShowSweep && !bPromoteSweep
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		GateActionButton(SweepButton, bSweepUsable, GateReason);
	}
	if (SweepButtonPromoted)
	{
		SweepButtonPromoted->SetVisibility(bPromoteSweep
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		GateActionButton(SweepButtonPromoted, bSweepUsable, GateReason);
	}
	if (ChallengeButton)
	{
		ChallengeButton->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		GateActionButton(ChallengeButton, !bBlocked, GateReason);
	}
	if (ChallengeOutline)
	{
		ChallengeOutline->SetVisibility(bBlocked && !bMaxed
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 비용은 버튼 안이 아니라 버튼 옆에 선다 — 아이콘 + 수량만.
	if (CostRow)
	{
		CostRow->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (CostText)
	{
		CostText->SetText(FText::AsNumber(1));
		CostText->SetColorAndOpacity(Entries > 0 ? CostInk : CostInkShort);
	}

	if (FooterNoteText)
	{
		FText ActionNote =
			(Reason == ETrainingBlockReason::NoCrew) ? NSLOCTEXT("Training", "FootNoCrew", "직원을 배치하면 시작할 수 있습니다.")
			: (Reason == ETrainingBlockReason::NoEntry) ? NSLOCTEXT("Training", "FootNoEntry", "입장권이 없습니다. 매일 00시에 1장이 채워집니다.")
			: (Reason == ETrainingBlockReason::CompanyLocked) ? NSLOCTEXT("Training", "FootCompanyLocked", "회사 레벨을 올리면 이용할 수 있습니다.")
			: bMaxed ? NSLOCTEXT("Training", "FootMaxed", "완료 기술·레벨 기준으로 성과를 수령합니다.")
			: bCanGo ? NSLOCTEXT("Training", "FootStartCost", "시작 시 입장권 1장 사용")
			: NSLOCTEXT("Training", "FootShort", "파워 미달 시 입장권을 사용하지 않습니다.");
		if (bShowSweep && Best > 0 && !bBlocked)
		{
			const FText CompletedTopic = FormatTrainingTopic(TableMgr, CompanyType, Best);
			if (!CompletedTopic.IsEmpty())
			{
				const FText SweepNote = FText::Format(NSLOCTEXT("Training", "SweepBasis", "성과 수령 · {0} 기준"), CompletedTopic);
				ActionNote = bMaxed ? SweepNote : FText::Format(NSLOCTEXT("Training", "ActionAndSweep", "{0}\n{1}"), ActionNote, SweepNote);
			}
		}
		FooterNoteText->SetText(ActionNote);
	}

	if (EmptyState)
	{
		EmptyState->SetVisibility(TeamCount == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTrainingCenterPanelWidget::HandleChallenge()
{
	if (bActionInFlight || IsCloseRequested()) return;
	UGameInstance* GI = GetGameInstance();
	USideContentSubsystem* Side = GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
	if (!Side) return;

	TGuardValue<bool> ActionGuard(bActionInFlight, true);
	const int32 Course = Side->GetCurrentCourse(BuildingIndex);
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	const FText TopicLabel = FormatTrainingTopic(TableMgr, CompanyType, Course);
	TArray<FMissionReward> Rewards;
	const bool bSucceeded = Side->Challenge(BuildingIndex, Rewards);
	RefreshAll();
	if (bSucceeded)
	{
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowRewardToast(BuildTrainingToastRewards(Rewards), TopicLabel.IsEmpty()
				? FText::GetEmpty() : FText::Format(NSLOCTEXT("Training", "CompleteTopicToast", "{0} 완료"), TopicLabel), true);
		}
	}
}

void UTrainingCenterPanelWidget::HandleSweep()
{
	if (bActionInFlight || IsCloseRequested()) return;
	UGameInstance* GI = GetGameInstance();
	USideContentSubsystem* Side = GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
	if (!Side) return;

	TGuardValue<bool> ActionGuard(bActionInFlight, true);
	const int32 Course = Side->GetCourseBest(BuildingIndex);
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	const FText TopicLabel = FormatTrainingTopic(TableMgr, CompanyType, Course);
	TArray<FMissionReward> Rewards;
	const bool bSucceeded = Side->SweepTraining(BuildingIndex, Rewards);
	RefreshAll();
	if (bSucceeded)
	{
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowRewardToast(BuildTrainingToastRewards(Rewards), TopicLabel.IsEmpty()
				? FText::GetEmpty() : FText::Format(NSLOCTEXT("Training", "SweepTopicToast", "{0} 성과"), TopicLabel), true);
		}
	}
}
