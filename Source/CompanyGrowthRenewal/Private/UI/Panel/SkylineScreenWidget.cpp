#include "UI/Panel/SkylineScreenWidget.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Data/SideContentRules.h"
#include "Engine/World.h"
#include "Enum/CompanyType.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SideContentSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/BuildingLandCameraShake.h"
#include "Player/MainMapPlayerController.h"
#include "Table/CompanyInfoTable.h"
#include "TimerManager.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/Skyline/SkylineCompanyRowWidget.h"
#include "UI/Element/Skyline/SkylineCrewRowWidget.h"
#include "UI/Element/Skyline/SkylineFloorEntryWidget.h"
#include "UI/Panel/BuildOpenWidget.h"
#include "UI/UIBase.h"
#include "Core/CGGameInstance.h"
#include "GameMode/OfficeGameMode.h"
#include "Manager/EntityManager.h"
#include "Entity/Building/BuildingBaseActor.h"

namespace
{
	// linear 필수 — sRGB 분수를 그대로 넣으면 물이 빠진다. 기술 연구소 판정 밴드와 같은 두 색이다.
	const FLinearColor VerdictPass(0.263f, 0.788f, 0.369f, 1.0f);
	const FLinearColor VerdictMiss(0.434f, 0.068f, 0.048f, 1.0f);

	// 최고 층 기준 위아래로 이만큼만 만든다 — 100층을 다 지으면 목록이 스크롤 쓰레기가 된다.
	constexpr int32 FloorWindow = 6;
	constexpr int32 MaxCrewRows = 6;

	// 동적 자식이라 WBP 슬롯이 없다 — 간격은 코드가 가질 수밖에 없다(선례: BusinessTripPanelWidget).
	constexpr float FloorCardGap = 10.0f;
	constexpr float CrewRowGap = 10.0f;
	constexpr float SpineGap = 6.0f;
	constexpr float PickRowGap = 8.0f;

	void ApplyStackPadding(UPanelSlot* PanelSlotPtr, float BottomGap)
	{
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(PanelSlotPtr))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomGap));
		}
		else if (UScrollBoxSlot* SSlot = Cast<UScrollBoxSlot>(PanelSlotPtr))
		{
			SSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomGap));
		}
	}

	/** 개인 파워도 팀 파워와 같은 계산이어야 한다 — 레일 합과 밴드 숫자가 어긋나면 그 자체가 결함이다. */
	float ComputeSoloPower(const FEmployeeInstance& Employee)
	{
		TArray<FEmployeeInstance> Solo;
		Solo.Add(Employee);

		TArray<int32> Ids;
		Ids.Add(Employee.EmployeeID);

		TArray<float> Power;
		FSideContentRules::ComputeTeamPower(Solo, Ids, Power);

		float Sum = 0.0f;
		for (float P : Power) Sum += P;
		return Sum;
	}

	FText MakeBlockText(ESkylineBlockReason Reason)
	{
		switch (Reason)
		{
		case ESkylineBlockReason::NoBuilding: return NSLOCTEXT("Skyline", "BlockNoBuilding", "보유한 회사가 없습니다");
		case ESkylineBlockReason::NoCrew:     return NSLOCTEXT("Skyline", "BlockNoCrew", "이 회사에 배치된 직원이 없습니다");
		case ESkylineBlockReason::NoEntry:    return NSLOCTEXT("Skyline", "BlockNoEntry", "오늘 입장권을 모두 썼습니다");
		case ESkylineBlockReason::HQLocked:   return NSLOCTEXT("Skyline", "BlockHQLocked", "본사 Lv.2 에서 열립니다");
		case ESkylineBlockReason::MaxFloor:   return NSLOCTEXT("Skyline", "BlockMaxFloor", "최고 층에 도달했습니다");
		case ESkylineBlockReason::PowerShort: return NSLOCTEXT("Skyline", "BlockPowerShort", "이 회사로는 더 오를 수 없습니다");
		default: return FText::GetEmpty();
		}
	}
}

USkylineScreenWidget::USkylineScreenWidget()
{
	bIsBackHandler = true;
}

USideContentSubsystem* USkylineScreenWidget::GetSide() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
}

void USkylineScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ActionButton) ActionButton->OnClicked().AddUObject(this, &USkylineScreenWidget::HandleAction);
	if (BackButton) BackButton->OnClicked().AddUObject(this, &USkylineScreenWidget::HandleBack);
	if (CompanyChipButton) CompanyChipButton->OnClicked().AddUObject(this, &USkylineScreenWidget::HandleCompanyChip);
	if (CompanyPickCloseButton) CompanyPickCloseButton->OnClicked().AddUObject(this, &USkylineScreenWidget::HandleCompanyPickClose);
	if (EndRunButton) EndRunButton->OnClicked().AddUObject(this, &USkylineScreenWidget::HandleEndRun);
	if (ResultConfirmButton) ResultConfirmButton->OnClicked().AddUObject(this, &USkylineScreenWidget::HandleResultConfirm);
}

void USkylineScreenWidget::NativeDestruct()
{
	ClearBeatTimer();
	if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->ResetSkylineClimbPresentation();
	UnbindOfficeEmployees();
	if (USideContentSubsystem* Side = GetSide()) Side->OnSideContentChanged.RemoveAll(this);
	SetMainMapHUDSuppressed(false);

	if (ActionButton) ActionButton->OnClicked().RemoveAll(this);
	if (BackButton) BackButton->OnClicked().RemoveAll(this);
	if (CompanyChipButton) CompanyChipButton->OnClicked().RemoveAll(this);
	if (CompanyPickCloseButton) CompanyPickCloseButton->OnClicked().RemoveAll(this);
	if (EndRunButton) EndRunButton->OnClicked().RemoveAll(this);
	if (ResultConfirmButton) ResultConfirmButton->OnClicked().RemoveAll(this);

	for (const TObjectPtr<USkylineCompanyRowWidget>& Row : CompanyRows)
	{
		if (Row) Row->OnCompanyRowClicked.RemoveAll(this);
	}
	CompanyRows.Reset();

	Super::NativeDestruct();
}

void USkylineScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	const UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	bOfficePresentation = GameInstance && GameInstance->IsInOfficeMap()
		&& GameInstance->GetOfficeMode() == EOfficeMode::Skyline;
	bTravelRequested = false;
	bOfficePlaybackStarted = false;
	SetMainMapHUDSuppressed(!bOfficePresentation);

	if (USideContentSubsystem* Side = GetSide())
	{
		Side->OnSideContentChanged.RemoveAll(this);
		Side->OnSideContentChanged.AddUObject(this, &USkylineScreenWidget::RefreshAll);
		SelectedBuildingIndex = Side->GetSkylineBuildingIndex();
	}

	// HUD 를 덮는 전용 화면이라 월드 입력을 막는다 — 해제는 NativeOnDeactivated 가 짝을 맞춘다
	if (UWorld* WorldPtr = GetWorld())
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(WorldPtr->GetFirstPlayerController()))
		{
			if (PC->GetCurrentInputMode() != EInputMode::UI) PC->GoToUIMode();
		}
	}

	Beat = ESkylineBeat::Idle;
	bShowingResult = false;
	PlayIndex = 0;
	SpineShown = 0;

	if (SceneSwitcher) SceneSwitcher->SetActiveWidgetIndex(bOfficePresentation ? 1 : 0);
	if (NightBG) NightBG->SetVisibility(bOfficePresentation ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (FloorSpineBox) FloorSpineBox->ClearChildren();
	if (ResultRoot) ResultRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (CompanyPickRoot) CompanyPickRoot->SetVisibility(ESlateVisibility::Collapsed);

	RefreshAll();
	if (bOfficePresentation)
	{
		if (OfficeFloorText) OfficeFloorText->SetText(NSLOCTEXT("Skyline", "OfficeLoading", "오피스 준비 중"));
		if (OfficeTenantText) OfficeTenantText->SetText(FText::GetEmpty());
		SkylineOffice = GetWorld()->GetAuthGameMode<AOfficeGameMode>();
		if (AOfficeGameMode* Office = SkylineOffice.Get())
		{
			Office->OnOfficeEmployeesReady.AddUObject(this, &USkylineScreenWidget::HandleOfficeEmployeesReady);
			if (Office->AreOfficeEmployeesReady()) HandleOfficeEmployeesReady();
		}
	}
}

void USkylineScreenWidget::NativeOnDeactivated()
{
	ClearBeatTimer();
	// UnbindOfficeEmployees 가 SkylineOffice 를 비우므로 복귀가 먼저다
	if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->ResetSkylineClimbPresentation();
	UnbindOfficeEmployees();
	Beat = ESkylineBeat::Idle;
	SetMainMapHUDSuppressed(false);

	if (USideContentSubsystem* Side = GetSide())
	{
		Side->OnSideContentChanged.RemoveAll(this);
	}

	// 메뉴 시트가 UI 모드를 켠 채로 이 화면을 넘겨준다 — 여기서 끊지 않으면 월드 터치가 죽는다
	if (UWorld* WorldPtr = GetWorld())
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(WorldPtr->GetFirstPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

	Super::NativeOnDeactivated();
}

bool USkylineScreenWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	HandleBack();
	return true;
}

void USkylineScreenWidget::UnbindOfficeEmployees()
{
	if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->OnOfficeEmployeesReady.RemoveAll(this);
	SkylineOffice.Reset();
}

void USkylineScreenWidget::HandleOfficeEmployeesReady()
{
	if (!bOfficePresentation || bTravelRequested || bOfficePlaybackStarted) return;
	bOfficePlaybackStarted = true;
	USideContentSubsystem* Side = GetSide();
	if (!Side) return;
	bool bStarted = false;
	{
		TGuardValue<bool> DeferRecord(bDeferringRecordRefresh, true);
		bStarted = Side->StartSkylineOfficeRun(RunResult);
	}
	if (bStarted)
	{
		StartPlayback();
	}
	else
	{
		ShowResultCard();
		if (ResultGainText) ResultGainText->SetText(NSLOCTEXT("Skyline", "OfficeNotReady", "개발을 시작하지 못했습니다"));
		if (ResultStopText) ResultStopText->SetText(NSLOCTEXT("Skyline", "OfficeNotReadyDetail", "입장권은 사용하지 않았습니다. 회사의 직원과 책상 배치를 확인해 주세요."));
		UE_LOG(LogTemp, Warning, TEXT("[Skyline] Office preparation failed for building %d; entry not consumed"), SelectedBuildingIndex);
	}
}

void USkylineScreenWidget::SetMainMapHUDSuppressed(bool bSuppress)
{
	if (!bSuppress)
	{
		if (UCommonActivatableWidget* HUD = SuppressedMainMapHUD.Get())
		{
			HUD->SetVisibility(MainMapHUDVisibility);
		}
		SuppressedMainMapHUD.Reset();
		return;
	}
	if (SuppressedMainMapHUD.IsValid()) return;

	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	UBuildOpenWidget* HUD = UIBase ? Cast<UBuildOpenWidget>(UIBase->GetActiveBottomWidget()) : nullptr;
	if (HUD)
	{
		SuppressedMainMapHUD = HUD;
		MainMapHUDVisibility = HUD->GetVisibility();
		HUD->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ─────────────────────────────── 갱신 ───────────────────────────────

void USkylineScreenWidget::RefreshAll()
{
	USideContentSubsystem* Side = GetSide();
	if (!Side) return;

	// 건물이 팔리면 저장 인덱스가 죽는다 — 서브시스템이 대신 골라 주므로 갱신마다 다시 읽는다
	SelectedBuildingIndex = bOfficePresentation ? Side->GetSkylineOfficeBuildingIndex() : Side->GetSkylineBuildingIndex();

	if (EntryText)
	{
		EntryText->SetText(FText::Format(NSLOCTEXT("Skyline", "Entries", "{0}/{1}"),
			FText::AsNumber(Side->GetSkylineEntries()), FText::AsNumber(USideContentSubsystem::GetSkylineEntryCap())));
	}
	// 판정은 즉시 저장되지만 최종 기록은 재생이 끝난 뒤 공개한다. 입장권은 소모 즉시 반영한다.
	if (!bDeferringRecordRefresh && !IsPlaybackActive())
	{
		if (BestFloorText)
		{
			BestFloorText->SetText(FText::Format(NSLOCTEXT("Skyline", "BestFloor", "{0}층"), FText::AsNumber(Side->GetBestFloor())));
		}
		if (BonusText)
		{
			const int32 Percent = FMath::RoundToInt((Side->GetSkylineRevenueMultiplier() - 1.0f) * 100.0f);
			BonusText->SetText(FText::Format(NSLOCTEXT("Skyline", "Bonus", "운영 수익 +{0}%"), FText::AsNumber(Percent)));
		}
		if (NextGateText)
		{
			const int32 NextGate = Side->GetNextBossFloor();
			NextGateText->SetText(NextGate > 0
				? FText::Format(NSLOCTEXT("Skyline", "NextGate", "다음 관문 {0}층"), FText::AsNumber(NextGate))
				: FText::GetEmpty());
			NextGateText->SetVisibility(NextGate > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	if (!bOfficePresentation && !IsPlaybackActive() && !bShowingResult)
	{
		RefreshCompanyChip();
		RebuildFloorList();
		RebuildCrewRail();
	}
	else if (bOfficePresentation) RefreshCompanyChip();
	RefreshVerdictBand();

	// 재생 중·결과 카드 위에서는 CTA 가 그 흐름의 것이다 — 로비 라벨로 되돌리면 안 된다
	if (!bOfficePresentation && !IsPlaybackActive() && !bShowingResult)
	{
		RefreshActionButton();
	}
	else if (bOfficePresentation && !IsPlaybackActive() && ActionButton)
	{
		ActionButton->SetVisibility(ESlateVisibility::Hidden);
	}
	RefreshNavigationControls();
}

void USkylineScreenWidget::RefreshCompanyChip()
{
	if (SelectedBuildingIndex == INDEX_NONE) return;

	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!SaveMgr || !TableMgr) return;

	bool bHasInfo = false;
	const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(SaveMgr->GetBuildingCompanyType(SelectedBuildingIndex), bHasInfo);
	if (!bHasInfo) return;

	if (CompanyNameText) CompanyNameText->SetText(Info.DisplayName);
	if (CompanyGlyph && !Info.GlyphIcon.IsNull())
	{
		if (UTexture2D* Glyph = Info.GlyphIcon.LoadSynchronous()) CompanyGlyph->SetBrushFromTexture(Glyph);
	}
}

void USkylineScreenWidget::RefreshVerdictBand()
{
	USideContentSubsystem* Side = GetSide();
	if (!Side) return;

	const bool bPlayingFloor = IsPlaybackActive() && RunResult.ClearedFloors.IsValidIndex(PlayIndex);
	const int32 Floor = bPlayingFloor ? RunResult.ClearedFloors[PlayIndex] : Side->GetBestFloor() + 1;
	const float Required = bShowingResult ? RunResult.StopRequirement : Side->GetFloorRequirement(Floor);
	const float Power = (bPlayingFloor || bShowingResult) ? RunResult.TeamPower : Side->GetSkylineTeamPower(SelectedBuildingIndex);
	const ESkylineBlockReason Reason = bPlayingFloor ? ESkylineBlockReason::None
		: bShowingResult ? (RunResult.StopFloor > 0 ? ESkylineBlockReason::PowerShort : ESkylineBlockReason::MaxFloor)
		: Side->GetSkylineBlockReason(SelectedBuildingIndex);

	if (RequirementText) RequirementText->SetText(FText::AsNumber(FMath::RoundToInt(Required)));
	if (TeamPowerText) TeamPowerText->SetText(FText::AsNumber(FMath::RoundToInt(Power)));

	// 바 길이는 「얼마나」만 말하고 「모자란다」는 못 말한다 — 색이 그 자리를 진다(합불 2상태뿐이라 데이터 주입).
	const bool bPass = (Required > 0.0f && Power >= Required) || (Reason == ESkylineBlockReason::MaxFloor);
	if (VerdictBar)
	{
		VerdictBar->SetPercent(Required > 0.0f ? FMath::Clamp(Power / Required, 0.0f, 1.0f)
			: Reason == ESkylineBlockReason::MaxFloor ? 1.0f : 0.0f);
		VerdictBar->SetFillColorAndOpacity(bPass ? VerdictPass : VerdictMiss);
	}

	if (BlockReasonText)
	{
		const FText Note = MakeBlockText(Reason);
		BlockReasonText->SetText(Note);
		BlockReasonText->SetVisibility(Note.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void USkylineScreenWidget::RefreshActionButton()
{
	USideContentSubsystem* Side = GetSide();
	if (!Side) return;

	SetActionLabel(FText::Format(NSLOCTEXT("Skyline", "ActionClimb", "개발하기 {0}/{1}"),
		FText::AsNumber(Side->GetSkylineEntries()), FText::AsNumber(USideContentSubsystem::GetSkylineEntryCap())));

	if (ActionButton)
	{
		ActionButton->SetVisibility(ESlateVisibility::Visible);
		ActionButton->SetIsEnabled(Side->GetSkylineBlockReason(SelectedBuildingIndex) == ESkylineBlockReason::None);
	}
}

void USkylineScreenWidget::RefreshNavigationControls()
{
	const bool bPlaying = IsPlaybackActive();
	const bool bInOffice = SceneSwitcher && SceneSwitcher->GetActiveWidgetIndex() != 0;
	if (EndRunButton) EndRunButton->SetVisibility(bPlaying ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (CompanyChipButton) CompanyChipButton->SetIsEnabled(!bOfficePresentation && !bTravelRequested && !bPlaying && !bShowingResult);
	if (UButtonWidget* TextButton = Cast<UButtonWidget>(BackButton))
	{
		TextButton->SetButtonText(bInOffice || bShowingResult
			? NSLOCTEXT("Skyline", "BackToFloors", "층 목록")
			: NSLOCTEXT("Skyline", "Exit", "나가기"));
	}
}

void USkylineScreenWidget::RebuildFloorList()
{
	if (!FloorListBox) return;
	FloorListBox->ClearChildren();

	UGameInstance* GI = GetGameInstance();
	USideContentSubsystem* Side = GetSide();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Side || !TableMgr) return;

	const TSubclassOf<UUserWidget> EntryClass = TableMgr->GetWidgetClass(EWidgetType::SkylineFloorEntry);
	if (!EntryClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkylineScreen] SkylineFloorEntry 위젯 클래스 미등록 (DT_WidgetClass 확인)"));
		return;
	}

	const int32 Best = Side->GetBestFloor();
	const int32 MaxFloor = Side->GetSkylineMaxFloor();
	const int32 NextGate = Side->GetNextBossFloor();
	const int32 Bottom = FMath::Max(1, Best - FloorWindow);
	const int32 Top = (MaxFloor > 0) ? FMath::Min(Best + FloorWindow, MaxFloor) : Best + FloorWindow;

	// 탑이니까 위가 높은 층이다 — 내림차순으로 쌓는다
	for (int32 Floor = Top; Floor >= Bottom; --Floor)
	{
		USkylineFloorEntryWidget* Card = CreateWidget<USkylineFloorEntryWidget>(this, EntryClass);
		if (!Card) continue;

		const ESkylineFloorState State = (Floor <= Best) ? ESkylineFloorState::Cleared
			: (Floor == Best + 1) ? ESkylineFloorState::Current
			: ESkylineFloorState::Locked;

		Card->SetFloor(Floor, Side->GetFloorRequirement(Floor), State,
			Side->IsBossFloor(Floor), Floor == NextGate, Side->GetFloorTenantName(Floor));

		ApplyStackPadding(FloorListBox->AddChild(Card), FloorCardGap);
	}
}

void USkylineScreenWidget::RebuildCrewRail()
{
	if (CrewRailBox) CrewRailBox->ClearChildren();
	if (CrewCountText) CrewCountText->SetText(FText::GetEmpty());

	UGameInstance* GI = GetGameInstance();
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!EmployeeMgr || !TableMgr || SelectedBuildingIndex == INDEX_NONE) return;

	const TSubclassOf<UUserWidget> RowClass = TableMgr->GetWidgetClass(EWidgetType::SkylineCrewRow);
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkylineScreen] SkylineCrewRow 위젯 클래스 미등록 (DT_WidgetClass 확인)"));
		return;
	}

	// 레일이 6칸이라 잘라야 한다 — 아무나 6명이 아니라 「센 순서로 6명」이어야 판정과 읽힘이 맞는다
	const TArray<FEmployeeInstance> Crew = EmployeeMgr->GetEmployeesInBuilding(SelectedBuildingIndex);
	if (CrewCountText)
	{
		CrewCountText->SetText(Crew.Num() > MaxCrewRows
			? FText::Format(NSLOCTEXT("Skyline", "CrewCountTop", "배치 {0}명 · 상위 {1}명 표시"),
				FText::AsNumber(Crew.Num()), FText::AsNumber(MaxCrewRows))
			: FText::Format(NSLOCTEXT("Skyline", "CrewCount", "배치 {0}명"), FText::AsNumber(Crew.Num())));
	}
	TArray<TPair<float, int32>> Ranked;
	Ranked.Reserve(Crew.Num());
	for (int32 i = 0; i < Crew.Num(); ++i)
	{
		Ranked.Emplace(ComputeSoloPower(Crew[i]), i);
	}
	Ranked.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key > B.Key; });

	const int32 Shown = FMath::Min(Ranked.Num(), MaxCrewRows);
	for (int32 i = 0; i < MaxCrewRows; ++i)
	{
		if (i < Shown && CrewRailBox)
		{
			if (USkylineCrewRowWidget* Row = CreateWidget<USkylineCrewRowWidget>(this, RowClass))
			{
				const FEmployeeInstance& Employee = Crew[Ranked[i].Value];
				Row->SetCrew(Employee, GetPrimaryDisciplineSlot(Employee), Ranked[i].Key);
				ApplyStackPadding(CrewRailBox->AddChild(Row), CrewRowGap);
			}
		}

	}
}

void USkylineScreenWidget::RebuildCompanyPick()
{
	if (!CompanyPickBox) return;

	for (const TObjectPtr<USkylineCompanyRowWidget>& OldRow : CompanyRows)
	{
		if (OldRow) OldRow->OnCompanyRowClicked.RemoveAll(this);
	}
	CompanyRows.Reset();
	CompanyPickBox->ClearChildren();

	UGameInstance* GI = GetGameInstance();
	USideContentSubsystem* Side = GetSide();
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Side || !EmployeeMgr || !SaveMgr || !TableMgr) return;

	const TSubclassOf<UUserWidget> RowClass = TableMgr->GetWidgetClass(EWidgetType::SkylineCompanyRow);
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkylineScreen] SkylineCompanyRow 위젯 클래스 미등록 (DT_WidgetClass 확인)"));
		return;
	}

	for (int32 BuildingIdx : Side->GetSkylineCandidateBuildings())
	{
		USkylineCompanyRowWidget* Row = CreateWidget<USkylineCompanyRowWidget>(this, RowClass);
		if (!Row) continue;

		Row->SetCompany(BuildingIdx, SaveMgr->GetBuildingCompanyType(BuildingIdx),
			Side->GetSkylineTeamPower(BuildingIdx),
			EmployeeMgr->GetEmployeesInBuilding(BuildingIdx).Num(),
			BuildingIdx == SelectedBuildingIndex);

		// 매 오픈 새로 만드는 행이라도 가드를 둔다 — 재사용 위젯에서 중복 구독이 조용히 쌓이는 자리다
		Row->OnCompanyRowClicked.RemoveAll(this);
		Row->OnCompanyRowClicked.AddUObject(this, &USkylineScreenWidget::HandleCompanyRowClicked);

		if (UVerticalBoxSlot* BoxSlot = CompanyPickBox->AddChildToVerticalBox(Row))
		{
			BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, PickRowGap));
		}
		CompanyRows.Add(Row);
	}
}

// ─────────────────────────────── 입력 ───────────────────────────────

void USkylineScreenWidget::HandleAction()
{
	if (bTravelRequested) return;
	if (bOfficePresentation || IsPlaybackActive() || bShowingResult) return;

	USideContentSubsystem* Side = GetSide();
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UEntityManager* Entities = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr;
	ABuildingBaseActor* Building = Entities ? Entities->GetBuildingByIndex(SelectedBuildingIndex) : nullptr;
	if (!Side || !GI || !IsValid(Building) || !Side->PrepareSkylineOfficeRun(SelectedBuildingIndex))
	{
		RefreshAll();
		return;
	}
	bTravelRequested = true;
	if (ActionButton) ActionButton->SetIsEnabled(false);
	GI->SetCurrentManagedBuilding(Building);
	GI->SetOfficeMode(EOfficeMode::Skyline);
	GI->TransitionToLevel(TEXT("OfficeMap"));
}

void USkylineScreenWidget::HandleBack()
{
	if (bTravelRequested) return;
	if (CompanyPickRoot && CompanyPickRoot->GetVisibility() != ESlateVisibility::Collapsed)
	{
		HandleCompanyPickClose();
		return;
	}

	if (bOfficePresentation || IsPlaybackActive() || bShowingResult)
	{
		ReturnToLobby();
		return;
	}

	DeactivateWidget();
}

void USkylineScreenWidget::HandleCompanyChip()
{
	if (!CompanyPickRoot || bOfficePresentation || bTravelRequested || IsPlaybackActive() || bShowingResult) return;

	RebuildCompanyPick();
	CompanyPickRoot->SetVisibility(ESlateVisibility::Visible);
}

void USkylineScreenWidget::HandleCompanyPickClose()
{
	if (CompanyPickRoot) CompanyPickRoot->SetVisibility(ESlateVisibility::Collapsed);
}

void USkylineScreenWidget::HandleCompanyRowClicked(int32 BuildingIndex)
{
	USideContentSubsystem* Side = GetSide();
	if (!Side) return;

	Side->SetSkylineBuildingIndex(BuildingIndex);
	HandleCompanyPickClose();

	// 같은 회사를 다시 고르면 서브시스템이 브로드캐스트를 생략한다 — 화면은 그래도 닫혀야 한다
	RefreshAll();
}

void USkylineScreenWidget::HandleEndRun()
{
	if (!IsPlaybackActive()) return;
	FinishPlayback();
}

void USkylineScreenWidget::HandleResultConfirm()
{
	ReturnToLobby();
}

// ─────────────────────────────── 재생 ───────────────────────────────

void USkylineScreenWidget::StartPlayback()
{
	StopPlayback();

	bShowingResult = false;
	PlayIndex = 0;
	SpineShown = 0;
	if (ResultRoot) ResultRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (FloorSpineBox) FloorSpineBox->ClearChildren();
	if (SceneSwitcher) SceneSwitcher->SetActiveWidgetIndex(1);

	if (RunResult.ClearedFloors.Num() == 0)
	{
		FinishPlayback();
		return;
	}

	Beat = ESkylineBeat::Work;
	RunPlayback();
}

void USkylineScreenWidget::RunPlayback()
{
	// 노브가 0 이면 타이머가 안 걸린다 — 재귀 대신 루프로 결과까지 흘려보낸다
	for (;;)
	{
		ApplyBeat();

		const int32 Millis = GetCurrentBeatMs();

		if (Millis > 0)
		{
			if (UWorld* WorldPtr = GetWorld())
			{
				WorldPtr->GetTimerManager().SetTimer(BeatTimer, this,
					&USkylineScreenWidget::HandleBeatTimer, Millis / 1000.0f, false);
				return;
			}
		}

		if (!StepBeat())
		{
			FinishPlayback();
			return;
		}
	}
}

void USkylineScreenWidget::HandleBeatTimer()
{
	if (!StepBeat())
	{
		FinishPlayback();
		return;
	}
	RunPlayback();
}

bool USkylineScreenWidget::StepBeat()
{
	switch (Beat)
	{
	case ESkylineBeat::Work:
		Beat = ESkylineBeat::Clear;
		return true;

	case ESkylineBeat::Clear:
		// 마지막 층이면 결과로 간다. 아니면 대기 없이 바로 다음 층 개발로 이어붙인다
		if (!RunResult.ClearedFloors.IsValidIndex(PlayIndex + 1)) return false;
		++PlayIndex;
		Beat = ESkylineBeat::Work;
		return true;

	default:
		return false;
	}
}

int32 USkylineScreenWidget::GetCurrentBeatMs() const
{
	if (Beat == ESkylineBeat::Idle) return 0;

	FSkylineBeatTuning Tuning;
	Tuning.WorkMs = WorkMs;
	Tuning.SlamMs = SlamMs;
	Tuning.AccelPerFloor = AccelPerFloor;
	Tuning.MinFloorBeatMs = MinFloorBeatMs;
	Tuning.BossBeatMul = BossBeatMul;

	const USideContentSubsystem* Side = GetSide();
	const int32 Floor = RunResult.ClearedFloors.IsValidIndex(PlayIndex) ? RunResult.ClearedFloors[PlayIndex] : 0;
	const bool bBoss = Side && Floor > 0 && Side->IsBossFloor(Floor);

	const FSkylineBeatDurations Beats = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, PlayIndex, bBoss);
	return Beat == ESkylineBeat::Work ? Beats.WorkMs : Beats.SlamMs;
}

void USkylineScreenWidget::ApplyBeat()
{
	USideContentSubsystem* Side = GetSide();
	const int32 Floor = RunResult.ClearedFloors.IsValidIndex(PlayIndex) ? RunResult.ClearedFloors[PlayIndex] : 0;
	RefreshVerdictBand();
	RefreshNavigationControls();

	switch (Beat)
	{
	case ESkylineBeat::Work:
		if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->SetSkylineEmployeesWorking(true);
		if (ActionButton) ActionButton->SetVisibility(ESlateVisibility::Hidden);
		if (OfficeFloorText)
		{
			OfficeFloorText->SetText(FText::Format(
				NSLOCTEXT("Skyline", "OfficeWork", "{0}층 개발 중"), FText::AsNumber(Floor)));
		}
		if (OfficeTenantText && Side) OfficeTenantText->SetText(Side->GetFloorTenantName(Floor));
		break;

	case ESkylineBeat::Clear:
		EnsureSpineThrough(PlayIndex + 1, /*bAnimate*/ true);
		if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->SetSkylineEmployeesWorking(false);
		if (OfficeFloorText)
		{
			OfficeFloorText->SetText(FText::Format(
				NSLOCTEXT("Skyline", "OfficeClear", "{0}층 돌파"), FText::AsNumber(Floor)));
		}
		// 셰이크는 UI 가 아니라 카메라가 진다(VFX 소스북 §2.5) — 보스 관문만 더 세게
		if (!FMotionGate::IsReduced())
		{
			const bool bBossFloor = Side && Floor > 0 && Side->IsBossFloor(Floor);
			if (APlayerController* PC = GetOwningPlayer())
			{
				PC->ClientStartCameraShake(ULightImpactCameraShake::StaticClass(), bBossFloor ? 1.4f : 1.0f);
			}
		}
		break;

	default:
		break;
	}
}

void USkylineScreenWidget::EnsureSpineThrough(int32 Count, bool bAnimate)
{
	const int32 Target = FMath::Min(Count, RunResult.ClearedFloors.Num());
	if (!FloorSpineBox)
	{
		SpineShown = FMath::Max(SpineShown, Target);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	USideContentSubsystem* Side = GetSide();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Side || !TableMgr) return;

	const TSubclassOf<UUserWidget> EntryClass = TableMgr->GetWidgetClass(EWidgetType::SkylineFloorEntry);
	if (!EntryClass) return;

	if (Target <= SpineShown) return;

	// 최신 여섯 층만 내림차순으로 갱신한다. 장거리 등반도 같은 카드 인스턴스를 재사용한다.
	const int32 VisibleCount = FMath::Min(Target, FloorWindow);
	for (int32 VisibleIndex = 0; VisibleIndex < VisibleCount; ++VisibleIndex)
	{
		const int32 Floor = RunResult.ClearedFloors[Target - 1 - VisibleIndex];

		USkylineFloorEntryWidget* Card = Cast<USkylineFloorEntryWidget>(FloorSpineBox->GetChildAt(VisibleIndex));
		if (!Card)
		{
			Card = CreateWidget<USkylineFloorEntryWidget>(this, EntryClass);
			if (Card) ApplyStackPadding(FloorSpineBox->AddChild(Card), SpineGap);
		}
		if (!Card) continue;

		Card->SetCompactMode(true);
		Card->SetFloor(Floor, Side->GetFloorRequirement(Floor), ESkylineFloorState::Cleared,
			Side->IsBossFloor(Floor), false, Side->GetFloorTenantName(Floor));

		// 맨 위 한 장만 떨어뜨린다 — 아래 카드들은 재사용 인스턴스라 매번 튀면 목록이 요동친다
		if (bAnimate && VisibleIndex == 0) Card->PlaySlamIn(GetCurrentBeatMs() / 1000.0f);
	}
	SpineShown = Target;

	// 사무실 아래로 실제 층이 쌓인다 — UI 스파인과 같은 층 수를 3D 외장·카메라에 넘긴다
	if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->SetSkylineClimbFloors(Target);
}

void USkylineScreenWidget::ClearBeatTimer()
{
	if (UWorld* WorldPtr = GetWorld())
	{
		WorldPtr->GetTimerManager().ClearTimer(BeatTimer);
	}
}

void USkylineScreenWidget::StopPlayback()
{
	ClearBeatTimer();
	Beat = ESkylineBeat::Idle;
	if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->ResetSkylineClimbPresentation();
}

void USkylineScreenWidget::FinishPlayback()
{
	ClearBeatTimer();
	Beat = ESkylineBeat::Idle;
	if (AOfficeGameMode* Office = SkylineOffice.Get()) Office->SetSkylineEmployeesWorking(false);

	// 건너뛰기도 최종 도달 층 주변까지 갱신한다.
	EnsureSpineThrough(RunResult.ClearedFloors.Num());
	ShowResultCard();
}

void USkylineScreenWidget::ShowResultCard()
{
	bShowingResult = true;
	RefreshAll();

	if (ActionButton) ActionButton->SetVisibility(ESlateVisibility::Hidden);
	if (ResultRoot) ResultRoot->SetVisibility(ESlateVisibility::Visible);

	const int32 Gain = RunResult.ReachedFloor - RunResult.FromFloor;
	if (ResultGainText)
	{
		ResultGainText->SetText(Gain > 0
			? FText::Format(NSLOCTEXT("Skyline", "ResultGain", "{0}층 상승"), FText::AsNumber(Gain))
			: NSLOCTEXT("Skyline", "ResultNoGain", "한 층도 오르지 못했습니다"));
	}
	if (ResultStopText)
	{
		ResultStopText->SetText(RunResult.StopFloor > 0
			? FText::Format(NSLOCTEXT("Skyline", "ResultStop", "{0}층에서 막혔습니다. 요구치 {1}"),
				FText::AsNumber(RunResult.StopFloor), FText::AsNumber(FMath::RoundToInt(RunResult.StopRequirement)))
			: NSLOCTEXT("Skyline", "ResultTop", "최고 층에 도달했습니다"));
	}

	if (ResultRewardsBox && WidgetTree)
	{
		TArray<FMissionReward> Totals;
		for (const CGRewardChip::FDisplayEntry& Entry : CGRewardChip::BuildDisplayEntries(RunResult.Rewards))
		{
			FMissionReward* Total = Totals.FindByPredicate([&Entry](const FMissionReward& Reward)
			{
				return Reward.ResourceType == Entry.ResourceType && Reward.ItemType == Entry.ItemType;
			});
			if (!Total)
			{
				Total = &Totals.AddDefaulted_GetRef();
				Total->ResourceType = Entry.ResourceType;
				Total->ItemType = Entry.ItemType;
			}
			if (Entry.ItemType != EItemType::None) Total->ItemAmount += static_cast<int32>(Entry.Quantity);
			else Total->Amount += Entry.Quantity;
		}
		TArray<FMissionReward> IconRewards;
		for (const FMissionReward& Total : Totals)
		{
			if (ResultStopText && Total.ItemType != EItemType::None
				&& !CGRewardChip::ResolveIcon(GetGameInstance(), Total))
			{
				// 아이콘이 없는 보상은 이름과 수량으로 표시해 정체 없는 숫자 칩을 남기지 않는다.
				ResultStopText->SetText(FText::Format(NSLOCTEXT("Skyline", "ResultNamedReward", "{0}\n{1} x{2}"),
					ResultStopText->GetText(), GetItemTypeDisplayName(Total.ItemType), FText::AsNumber(Total.ItemAmount)));
			}
			else
			{
				IconRewards.Add(Total);
			}
		}
		// 빈 결과에서도 WBP 샘플을 남겨 다음 등반의 아이콘 크기와 폰트를 보존한다.
		const bool bHasRewards = !IconRewards.IsEmpty()
			&& CGRewardChip::RenderIcons(*WidgetTree, GetGameInstance(), *ResultRewardsBox, IconRewards);
		ResultRewardsBox->SetVisibility(bHasRewards ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USkylineScreenWidget::ReturnToLobby()
{
	if (bTravelRequested) return;
	StopPlayback();
	if (bOfficePresentation)
	{
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
		{
			bTravelRequested = true;
			if (USideContentSubsystem* Side = GetSide()) Side->FinishSkylineOfficeRun();
			// TransitionToLevel의 저장까지 Skyline 모드를 유지해 일반 프로젝트를 보존한다.
			GI->TransitionToLevel(TEXT("MainMap_City"));
		}
		return;
	}

	bShowingResult = false;
	PlayIndex = 0;
	SpineShown = 0;
	if (ResultRoot) ResultRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (FloorSpineBox) FloorSpineBox->ClearChildren();
	if (SceneSwitcher) SceneSwitcher->SetActiveWidgetIndex(0);

	RefreshAll();
}

void USkylineScreenWidget::SetActionLabel(const FText& Label)
{
	// UCommonButtonBase 에는 라벨 세터가 없다 — 프로젝트 표준 텍스트 버튼일 때만 글자가 바뀐다
	if (UButtonWidget* TextButton = Cast<UButtonWidget>(ActionButton))
	{
		TextButton->SetButtonText(Label);
	}
}
