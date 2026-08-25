// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorkstationInfoWidget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Office/WorkstationActorBase.h"
#include "Office/WorkstationTypes.h"
#include "Office/OfficeManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/WorkstationCardTable.h"
#include "Table/WorkstationTable.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EntityCardData.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/ProductionDiscipline.h"
#include "UI/Element/Employee/DisciplineRadarWidget.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/UIBase.h"
#include "UI/Panel/OfficeRecruitmentPanelWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/MainMapPlayerController.h"
#include "GameMode/OfficeGameMode.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Enum/WidgetType.h"
#include "Core/CGGameInstance.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

void UWorkstationInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 미션 패널 등록 유지 — 구 도크 대상 가이드 타겟(채용/좌석/성장, 체인에서 이관됨)은 nullptr 스텁이라 링만 생략됨
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyWorkstationPanelOpened(this);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	}

	// 책상 강화 버튼 (UUpgradeSlot 내부 UCostActionButtonWidget — DECLARE_EVENT 이라 AddUObject)
	if (DeskUpgradeSlot)
	{
		if (UCostActionButtonWidget* UpgradeBtn = DeskUpgradeSlot->GetUpgradeButton())
		{
			DefaultUpgradeButtonText = UpgradeBtn->ButtonText;
			UpgradeBtn->OnClicked().AddUObject(this, &UWorkstationInfoWidget::OnDeskUpgradeClicked);
		}
	}

	if (EmployeeWindowButton)
	{
		EmployeeWindowButton->OnClicked().AddUObject(this, &UWorkstationInfoWidget::OnEmployeeWindowClicked);
	}

	if (BenchListView)
	{
		BenchListView->SetSelectionMode(ESelectionMode::None);   // 탭=즉시 착석, 선택 상태 없음
		BenchListView->OnEntryWidgetGenerated().AddUObject(this, &UWorkstationInfoWidget::OnBenchEntryGenerated);
	}

	if (UnseatButton)
	{
		UnseatButton->OnClicked().AddUObject(this, &UWorkstationInfoWidget::OnUnseatClicked);
	}

	if (RecruitButton)
	{
		RecruitButton->OnClicked().AddUObject(this, &UWorkstationInfoWidget::OnRecruitClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UWorkstationInfoWidget::OnCloseDelegate);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UWorkstationInfoWidget::OnBackgroundClicked);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.AddUObject(this, &UWorkstationInfoWidget::HandleEmployeeStatsChanged);
		EmployeeManager->OnEmployeeRosterChanged.AddUObject(this, &UWorkstationInfoWidget::HandleRosterChanged);
	}
}

void UWorkstationInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateEquipPlateMaterialSize();
}

void UWorkstationInfoWidget::NativeDestruct()
{
	if (DeskUpgradeSlot)
	{
		if (UCostActionButtonWidget* UpgradeBtn = DeskUpgradeSlot->GetUpgradeButton())
		{
			UpgradeBtn->OnClicked().RemoveAll(this);
		}
	}

	if (EmployeeWindowButton)
	{
		EmployeeWindowButton->OnClicked().RemoveAll(this);
	}

	if (BenchListView)
	{
		BenchListView->OnEntryWidgetGenerated().RemoveAll(this);
	}

	if (UnseatButton)
	{
		UnseatButton->OnClicked().RemoveAll(this);
	}

	if (RecruitButton)
	{
		RecruitButton->OnClicked().RemoveAll(this);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UWorkstationInfoWidget::OnCloseDelegate);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UWorkstationInfoWidget::OnBackgroundClicked);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeRosterChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UWorkstationInfoWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 위에 쌓인 패널이 닫혀 재활성될 때 점유/책상 상태 갱신.
	// 첫 오픈 땐 CurrentWorkstation 이 아직 null(SetWorkstationData 가 뒤) 이라 UpdateUI 는 early-return.
	UpdateUI();
}

void UWorkstationInfoWidget::NativeOnDeactivated()
{
	// 패널 닫힐 때 점유 직원 오버레이도 해제
	SetOccupantHighlight(false);

	if (CurrentWorkstation)
	{
		CurrentWorkstation->SetHighlight(false);
	}

	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

	Super::NativeOnDeactivated();
}

void UWorkstationInfoWidget::SetWorkstationData(AWorkstationActorBase* Workstation)
{
	CurrentWorkstation = Workstation;

	if (CurrentWorkstation)
	{
		CurrentWorkstation->SetHighlight(true);
		FocusCameraOnDesk();
	}

	UpdateUI();
}

void UWorkstationInfoWidget::SetEmployeeData(int32 EmployeeID)
{
	CurrentEmployeeID = EmployeeID;
	UpdateUI();
}

void UWorkstationInfoWidget::FocusCameraOnDesk()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	if (AOfficeCameraPawn* CameraPawn = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
	{
		CameraPawn->FocusOnLocation(CurrentWorkstation->GetActorLocation(), 600.f);
	}
}

void UWorkstationInfoWidget::SetOccupantHighlight(bool bHighlighted)
{
	if (CurrentEmployeeID < 0)
	{
		return;
	}

	if (AOfficeGameMode* OfficeGM = Cast<AOfficeGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (AOfficeworker* Worker = OfficeGM->FindEmployeeActorByID(CurrentEmployeeID))
		{
			Worker->SetSelected(bHighlighted);
		}
	}
}

void UWorkstationInfoWidget::SyncOccupancyFromWorkstation()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	const int32 ChairCount = CurrentWorkstation->GetChairCount();

	// 칩 직원이 이 책상을 떠났으면 해제 (직원창 [자리 비우기]/해고 반영)
	if (CurrentEmployeeID >= 0)
	{
		bool bStillSeated = false;
		for (int32 SeatIdx = 0; SeatIdx < ChairCount; ++SeatIdx)
		{
			if (CurrentWorkstation->GetAssignedEmployeeID(SeatIdx) == CurrentEmployeeID)
			{
				bStillSeated = true;
				break;
			}
		}
		if (!bStillSeated)
		{
			SetOccupantHighlight(false);
			CurrentEmployeeID = -1;
		}
	}

	// 빈 칩 상태에서 자동 착석이 채웠으면 첫 점유자 채택
	if (CurrentEmployeeID < 0)
	{
		for (int32 SeatIdx = 0; SeatIdx < ChairCount; ++SeatIdx)
		{
			const int32 OccupantID = CurrentWorkstation->GetAssignedEmployeeID(SeatIdx);
			if (OccupantID >= 0)
			{
				CurrentEmployeeID = OccupantID;
				break;
			}
		}
	}
}

void UWorkstationInfoWidget::UpdateUI()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	SyncOccupancyFromWorkstation();
	UpdateDeskSection();
	UpdateEquipBlock();
	UpdateWorkerCard();
	UpdateBenchSection();
}

void UWorkstationInfoWidget::UpdateDeskSection()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	// DT_WorkstationCard.UIIcon(WorkstationTypeID 행) → 강화 슬롯 아이콘, DisplayName → 헤더 서브
	FString DeskName;
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		bool bCardOK = false;
		const FWorkstationCardTable Card = TableMgr->GetWorkstationCardInfo(CurrentWorkstation->WorkstationTypeID, bCardOK);
		if (bCardOK)
		{
			DeskName = Card.DisplayName.ToString();
			if (!Card.UIIcon.IsNull())
			{
				if (UTexture2D* DeskTex = Card.UIIcon.LoadSynchronous())
				{
					if (DeskUpgradeSlot)
					{
						DeskUpgradeSlot->SetIconTexture(DeskTex);
					}
				}
			}
		}
	}

	const int32 SetupLevelAsInt = GetComputerSetupLevelNumber(CurrentWorkstation->GetCurrentSetupLevel());

	// 레벨은 DeskUpgradeSlot 의 LevelText 가 표시 — 여기선 책상 이름만(중복 방지)
	if (HeaderSubText)
	{
		HeaderSubText->SetText(FText::FromString(DeskName));
		HeaderSubText->SetVisibility(DeskName.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (DeskUpgradeSlot)
	{
		FComputerSetupLevelData CurrentLevelData;
		if (!CurrentWorkstation->GetCurrentSetupLevelData(CurrentLevelData))
		{
			UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] 현재 책상 세팅 행을 찾을 수 없습니다."));
			DeskUpgradeSlot->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		const bool bMax = CurrentWorkstation->IsMaxLevel();
		FComputerSetupLevelData NextLevelData;
		if (!bMax && !CurrentWorkstation->GetNextSetupLevelData(NextLevelData))
		{
			UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] 다음 책상 세팅 행을 찾을 수 없습니다."));
			DeskUpgradeSlot->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		DeskUpgradeSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		// 부제는 비운다 — 장비 목록은 아래 장비 블록이 구조 컬럼에서 생성한다
		DeskUpgradeSlot->SetRuntimePresentation(
			CurrentLevelData.DisplayName,
			FText::GetEmpty(),
			EResourceType::Diamond);

		// 최대 레벨은 잠기는 대신 동일 성능 외형 「변경」이 된다
		FComputerSetupLevelData VariantData;
		const bool bSwapMode = bMax && CurrentWorkstation->GetMaxVariantData(VariantData);

		const float CurrentSpeedPercent = CurrentLevelData.WorkSpeedBonusRate * 100.f;
		const float NextSpeedPercent = bMax
			? CurrentSpeedPercent
			: NextLevelData.WorkSpeedBonusRate * 100.f;

		// 상한은 항상 넘긴다 — 배지가 "Lv.N / 6" 로 남은 단계를 보여주고, MAX 판정(Level >= MaxLevel)은 그대로다
		const int32 MaxSetupLevelAsInt = GetComputerSetupLevelNumber(EComputerSetupLevel::Level6);

		// UpdateInfo 의 MaxLevel 인자는 배지 표기와 MAX 잠금을 겸한다. 변경 모드는 잠기면 안 되므로
		// 인자로는 0 을 주고(=잠금 해제), 배지는 override 로 따로 채운다.
		DeskUpgradeSlot->SetLevelBadgeOverride(bSwapMode
			? FText::Format(FText::FromString(TEXT("Lv.{0} / {1}")), SetupLevelAsInt, MaxSetupLevelAsInt)
			: FText::GetEmpty());

		DeskUpgradeSlot->UpdateInfo(
			SetupLevelAsInt,
			CurrentSpeedPercent,
			NextSpeedPercent,
			bSwapMode ? VariantData.VariantSwapCost : (bMax ? 0 : NextLevelData.UpgradeCost),
			/*bIsLocked*/ false,
			bSwapMode ? 0 : MaxSetupLevelAsInt);

		if (UCostActionButtonWidget* UpgradeBtn = DeskUpgradeSlot->GetUpgradeButton())
		{
			UpgradeBtn->SetButtonText(bSwapMode
				? NSLOCTEXT("WorkstationInfo", "SwapAppearance", "변경")
				: DefaultUpgradeButtonText);
		}
	}
}

void UWorkstationInfoWidget::UpdateEquipBlock()
{
	if (!CurrentWorkstation) { return; }

	FComputerSetupLevelData CurrentData;
	if (!CurrentWorkstation->GetCurrentSetupLevelData(CurrentData)) { return; }

	if (EquipSummaryText)
	{
		// "지금 책상 위" 라벨은 트리 정적 위젯이 소유한다 — 여기서 접두로 붙이면 라벨이 두 번 뜨고,
		// 인라인으로 이으면 도크 폭에서 마지막 한 글자가 홀로 넘어간다 (WORKSTATION_INFO_PANEL §4.1)
		EquipSummaryText->SetText(BuildWorkstationEquipSummary(CurrentData));
	}

	// 최대 레벨에서는 "다음 단계"가 없는 대신 "바꿀 외형"이 있다 — 「변경」이 무엇으로 바뀌는지 같은 자리에서 보여준다
	FComputerSetupLevelData NextData;
	const bool bHasNext = CurrentWorkstation->IsMaxLevel()
		? CurrentWorkstation->GetMaxVariantData(NextData)
		: CurrentWorkstation->GetNextSetupLevelData(NextData);
	const FWorkstationEquipDelta Delta = BuildWorkstationEquipDelta(CurrentData, bHasNext ? &NextData : nullptr);

	const bool bShowItem = (Delta.Kind != EWorkstationEquipDeltaKind::None) && !Delta.ItemName.IsEmpty();
	if (EquipNextRow)
	{
		EquipNextRow->SetVisibility(bShowItem ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (EquipNextLead)
	{
		// 품목명만 있으면 "이게 지금 있는 건가 생길 건가"가 안 읽힌다 — 언제(레벨)와 성격(추가/교체)을 먼저 말한다
		const FText ChangeWord = (Delta.Kind == EWorkstationEquipDeltaKind::Replaced)
			? NSLOCTEXT("WorkstationInfo", "EquipChangeReplace", "교체")
			: NSLOCTEXT("WorkstationInfo", "EquipChangeAdd", "추가");
		const int32 NextLevelNumber =
			GetComputerSetupLevelNumber(CurrentWorkstation->GetCurrentSetupLevel()) + 1;

		EquipNextLead->SetText(CurrentWorkstation->IsMaxLevel()
			? FText::Format(NSLOCTEXT("WorkstationInfo", "EquipLeadVariant", "외형 변경 시 {0}"), ChangeWord)
			: FText::Format(NSLOCTEXT("WorkstationInfo", "EquipLeadNext", "Lv.{0} 강화 시 {1}"),
				FText::AsNumber(NextLevelNumber), ChangeWord));
	}
	if (EquipNextName)   { EquipNextName->SetText(Delta.ItemName); }
	if (EquipNextDetail) { EquipNextDetail->SetText(Delta.Detail); }

	if (EquipNextIcon)
	{
		UTexture2D* IconTexture = nullptr;
		if (const TSoftObjectPtr<UTexture2D>* Found = EquipIcons.Find(Delta.Item))
		{
			if (!Found->IsNull()) { IconTexture = Found->LoadSynchronous(); }
		}
		if (IconTexture)
		{
			EquipNextIcon->SetBrushFromTexture(IconTexture);
			EquipNextIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			// 아이콘 미지정은 조용히 넘기지 않는다 — 빈 흰 사각형이 남는 것보다 접는 편이 낫다
			EquipNextIcon->SetVisibility(ESlateVisibility::Collapsed);
			if (bShowItem)
			{
				UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] EquipIcons 에 품목 %d 아이콘이 없습니다"),
					static_cast<int32>(Delta.Item));
			}
		}
	}
}

void UWorkstationInfoWidget::UpdateEquipPlateMaterialSize()
{
	if (!EquipBlockBG && !EquipBlockLine)
	{
		return;
	}

	// 도크 전체가 아니라 플레이트 자신의 크기여야 한다 — 다음 단계 행이 접히면 높이만 바뀐다
	const UWidget* SizeSource = EquipBlockBG
		? static_cast<const UWidget*>(EquipBlockBG)
		: static_cast<const UWidget*>(EquipBlockLine);
	const FVector2D PlateSize = SizeSource->GetCachedGeometry().GetLocalSize();
	if (PlateSize.X < 1.f || PlateSize.Y < 1.f || PlateSize.Equals(LastEquipPlateSize, 0.5f))
	{
		return;
	}
	LastEquipPlateSize = PlateSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (UImage* PlateLayer : { EquipBlockBG, EquipBlockLine })
	{
		if (!PlateLayer)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = PlateLayer->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(WpxParam, PlateSize.X);
			MID->SetScalarParameterValue(HpxParam, PlateSize.Y);
		}
	}
}

void UWorkstationInfoWidget::UpdateWorkerCard()
{
	if (!WorkerCard)
	{
		return;
	}

	FEmployeeInstance* Employee = (EmployeeManager && CurrentEmployeeID >= 0)
		? EmployeeManager->FindEmployee(CurrentEmployeeID)
		: nullptr;

	if (!Employee)
	{
		WorkerCard->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// SelfHitTestInvisible — 카드 판은 히트 통과, 내부 [직원창] 버튼만 클릭
	WorkerCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// ── 신원(초상·이름·Lv·★·종합·등급 젬) = 로스터 카드 한 부품이 통째로 담당 ──
	// 대기 목록·직원창과 같은 위젯이라 세 화면의 표시 규칙이 구조적으로 갈라질 수 없다.
	if (OccupantCard)
	{
		OccupantCard->SetEmployeeInfo(*Employee);
	}

	// ── 직능 레이더 + 특화/주력/보조 (직원창 DisciplineCard 와 동일 규칙: enum DisplayName, 상한 max(15,최고)) ──
	const int32 DiscCount = static_cast<int32>(EProductionDiscipline::Count);
	TArray<int32> Disc = Employee->DisciplinePoints;
	if (Disc.Num() != DiscCount) { Disc.SetNumZeroed(DiscCount); }

	int32 MaxVal = 0;
	for (int32 v : Disc) { MaxVal = FMath::Max(MaxVal, v); }
	const int32 DisplayMax = FMath::Max(15, MaxVal);

	// 표시명 SOT = DT_DisciplineDisplay (슬롯 의미는 enum 고정, 표시만 산업별 재해석)
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* DiscTableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
	const ECompanyType Industry = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;
	auto DiscName = [DiscTableMgr, Industry](int32 Idx) -> FText
	{
		return DiscTableMgr ? DiscTableMgr->GetDisciplineDisplayName(Industry, Idx) : FText::GetEmpty();
	};

	if (OccupantRadar)
	{
		TArray<FText> Labels;
		for (int32 i = 0; i < DiscCount; ++i) { Labels.Add(DiscName(i)); }
		OccupantRadar->SetAxisLabels(Labels);
		OccupantRadar->SetRadarData(Disc, DisplayMax, FLootBoxRarityUtility::GetRarityColor(Employee->SpawnRarity));
	}

	// 상위 2 직능 (동점 = 낮은 인덱스 우선)
	int32 Best0 = -1, Best1 = -1;
	for (int32 i = 0; i < DiscCount; ++i)
	{
		if (Best0 < 0 || Disc[i] > Disc[Best0]) { Best1 = Best0; Best0 = i; }
		else if (Best1 < 0 || Disc[i] > Disc[Best1]) { Best1 = i; }
	}
	if (OccupantSpecText)
	{
		OccupantSpecText->SetText(FText::FromString(MaxVal > 0
			? FString::Printf(TEXT("%s 특화"), *DiscName(Best0).ToString())
			: TEXT("특화 없음")));
	}
	if (OccupantLeadValue0) { OccupantLeadValue0->SetText(MaxVal > 0 ? DiscName(Best0) : FText::GetEmpty()); }
	if (OccupantLeadValue1) { OccupantLeadValue1->SetText(MaxVal > 0 ? DiscName(Best1) : FText::GetEmpty()); }

	// ── 연혁 ──
	if (OccupantNoText)
	{
		OccupantNoText->SetText(FText::FromString(FString::Printf(TEXT("#%04d"), Employee->EmployeeID)));
	}
	if (OccupantHiredText)
	{
		OccupantHiredText->SetText(FText::FromString(Employee->HiredDate.ToString(TEXT("%Y.%m.%d"))));
	}
	if (OccupantTenureText)
	{
		const FTimespan Span = FDateTime::Now() - Employee->HiredDate;
		const int32 TotalDays = FMath::Max(0, static_cast<int32>(Span.GetTotalDays()));
		const int32 Months = TotalDays / 30, Days = TotalDays % 30;
		OccupantTenureText->SetText(FText::FromString(
			Months > 0 ? FString::Printf(TEXT("%d개월 %d일"), Months, Days) : FString::Printf(TEXT("%d일"), Days)));
	}

	// 초상 로드는 OccupantCard 안에서 끝난다 — SetEmployeeInfo 가 자기 초상까지 채운다

	SetOccupantHighlight(true);
}

void UWorkstationInfoWidget::UpdateBenchSection()
{
	if (!CurrentWorkstation || !BenchSection) { return; }

	const bool bHasEmptySeat = CurrentWorkstation->FindEmptyAssignmentSlot() >= 0;
	BenchSection->SetVisibility(bHasEmptySeat ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (NoteText)
	{
		NoteText->SetText(bHasEmptySeat
			? NSLOCTEXT("Office", "DockNoteEmpty", "탭하면 이 책상에 앉습니다. 육성은 직원창에서 합니다.")
			: NSLOCTEXT("Office", "DockNoteOccupied", "육성은 직원창에서 합니다."));
	}

	if (!bHasEmptySeat) { return; }

	UGameInstance* GI = GetGameInstance();
	UCGGameInstance* CGI = Cast<UCGGameInstance>(GI);
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!CGI || !EmpMgr || !BenchListView) { return; }

	TArray<FEmployeeInstance> Bench = EmpMgr->GetUnassignedEmployeesInBuilding(CGI->GetCurrentManagedBuildingIndex());
	UOfficeManager::SortBenchByOverallDesc(Bench);

	BenchListView->ClearListItems();
	for (const FEmployeeInstance& Employee : Bench)
	{
		UEntityCardData* CardData = NewObject<UEntityCardData>(this);
		CardData->EmployeeInfo = Employee;
		BenchListView->AddItem(CardData);
	}

	if (BenchHeadText)
	{
		BenchHeadText->SetText(FText::Format(
			NSLOCTEXT("Office", "BenchHead", "대기 중인 직원 · {0}명"), FText::AsNumber(Bench.Num())));
	}

	const bool bEmpty = (Bench.Num() == 0);
	BenchListView->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (BenchEmptyBox)
	{
		BenchEmptyBox->SetVisibility(bEmpty ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UWorkstationInfoWidget::OnBenchEntryGenerated(UUserWidget& EntryWidget)
{
	if (UEmployeeRosterCardWidget* Card = Cast<UEmployeeRosterCardWidget>(&EntryWidget))
	{
		// 재활용 엔트리라 매 생성마다 갈아끼운다 (누적 방지)
		Card->OnRosterCardClicked.RemoveDynamic(this, &UWorkstationInfoWidget::OnBenchCardClicked);
		Card->OnRosterCardClicked.AddDynamic(this, &UWorkstationInfoWidget::OnBenchCardClicked);
	}
}

void UWorkstationInfoWidget::OnBenchCardClicked(int32 EmployeeID)
{
	if (!CurrentWorkstation) { return; }
	if (UOfficeManager* OfficeMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr)
	{
		if (OfficeMgr->SeatEmployeeAtWorkstation(CurrentWorkstation, EmployeeID))
		{
			UpdateUI();
		}
	}
}

void UWorkstationInfoWidget::OnUnseatClicked()
{
	if (!CurrentWorkstation || CurrentEmployeeID < 0) { return; }
	if (UOfficeManager* OfficeMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr)
	{
		SetOccupantHighlight(false);
		if (OfficeMgr->UnseatEmployee(CurrentWorkstation, CurrentEmployeeID))
		{
			CurrentEmployeeID = -1;
			UpdateUI();
		}
	}
}

void UWorkstationInfoWidget::OnRecruitClicked()
{
	UCGGameInstance* CGI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!CGI || !TableMgr || !UIManager || !UIManager->GetUIBase()) { return; }

	TSubclassOf<UUserWidget> RecruitmentPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeRecruitmentPanel);
	if (!RecruitmentPanel) { return; }

	UCommonActivatableWidget* Pushed = UIManager->GetUIBase()->PushPromptClass(RecruitmentPanel.Get());
	if (UOfficeRecruitmentPanelWidget* RecruitmentWidget = Cast<UOfficeRecruitmentPanelWidget>(Pushed))
	{
		RecruitmentWidget->SetBuildingIndex(CGI->GetCurrentManagedBuildingIndex());
	}
}

void UWorkstationInfoWidget::OnDeskUpgradeClicked()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	// 최대 레벨에서는 승급이 아니라 동일 성능 외형 변경이다
	const bool bSucceeded = CurrentWorkstation->IsMaxLevel()
		? CurrentWorkstation->TrySwapMaxVariant()
		: CurrentWorkstation->TryUpgradeSetupLevel();

	if (bSucceeded)
	{
		if (DeskUpgradeSlot)
		{
			DeskUpgradeSlot->PlayUpgradeEffect();
		}
		UpdateUI();
	}
}

void UWorkstationInfoWidget::OnEmployeeWindowClicked()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase())
	{
		return;
	}

	TSubclassOf<UUserWidget> EmployeeWindow = TableMgr->GetWidgetClass(EWidgetType::EmployeeWindow);
	if (!EmployeeWindow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] EmployeeWindow 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	UIManager->GetUIBase()->PushPromptClass(EmployeeWindow.Get());

	// 입력모드 쌍 — 닫힐 때 EmployeeWindow::NativeOnDeactivated 가 GoToNormalMode 복원 (도크는 아래 유지)
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
		{
			PC->GoToUIMode();
		}
	}
}

void UWorkstationInfoWidget::HandleEmployeeStatsChanged(int32 EmployeeID)
{
	if (EmployeeID != CurrentEmployeeID)
	{
		return;
	}

	// 강화(★)/초상 변화 → 칩 부분 갱신
	UpdateWorkerCard();
}

void UWorkstationInfoWidget::HandleRosterChanged()
{
	// 자동 착석/자리 비우기/해고가 이 책상 점유를 바꿨으면 칩 동기화 (직원창 아래 가려진 채로도 동작)
	const int32 PrevID = CurrentEmployeeID;
	SyncOccupancyFromWorkstation();
	if (CurrentEmployeeID != PrevID)
	{
		UpdateWorkerCard();
	}
}

void UWorkstationInfoWidget::OnCloseDelegate()
{
	CloseWithAnimation();
}

void UWorkstationInfoWidget::OnBackgroundClicked()
{
	CloseWithAnimation();
}
