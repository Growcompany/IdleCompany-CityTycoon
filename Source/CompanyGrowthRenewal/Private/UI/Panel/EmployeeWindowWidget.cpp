// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/EmployeeWindowWidget.h"
#include "UI/Panel/EnhanceStarforceModalWidget.h"
#include "UI/Panel/PotentialOddsWidget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "TimerManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "UI/UIBase.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/EntityManager.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Data/EntityCardData.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EmployeePotentialData.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/ResourceType.h"
#include "Enum/ItemType.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "UI/Element/Employee/DisciplineCardWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "CommonButtonBase.h"
#include "UI/Gacha/GachaCaptureStage.h"
#include "Manager/ItemInventoryManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/ListView.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ComboBoxString.h"
#include "Enum/ProductionDiscipline.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "CommonTextBlock.h"

namespace
{
	// P2 강화 스펙의 레벨 캡 — 지금은 "Lv.N / 30" 표시 전용, 실제 XP 캡은 P2 에서
	constexpr int32 MaxEmployeeLevelDisplay = 30;

	// 잠긴/빈 잠재 줄 뉴트럴 (linear)
	const FLinearColor PotBarNeutral(0.314f, 0.361f, 0.424f, 1.f);
	const FLinearColor InkMute(0.258f, 0.300f, 0.356f, 1.f);

	// 명함 슬롯 순서 고정 (CubeIndex = 이 배열의 인덱스) — 종이/골드/블랙
	const EItemType CubeTypes[3] = { EItemType::BusinessCardPaper, EItemType::BusinessCardGold, EItemType::BusinessCardBlack };
}

void UEmployeeWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGlobalUtilFunctions::InitProgressHead(ExpBarHead);

	// 스탯 셀 탭 → 효과 툴팁. 페이로드 바인딩이라 핸들러 하나로 6칸 처리(UFUNCTION 6개 불필요)
	for (int32 i = 0; i < 6; ++i)
	{
		if (UCommonButtonBase* Btn = Cast<UCommonButtonBase>(
			GetWidgetFromName(*FString::Printf(TEXT("HeroStatBtn%d"), i))))
		{
			Btn->OnClicked().AddUObject(this, &UEmployeeWindowWidget::HandleStatInfoTapped, i);
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	}

	if (RosterListView)
	{
		RosterListView->SetSelectionMode(ESelectionMode::Single);
		RosterListView->OnEntryWidgetGenerated().AddUObject(this, &UEmployeeWindowWidget::OnRosterEntryGenerated);
	}

	if (RerollButton)
	{
		RerollButton->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnRerollClicked);
	}

	if (RosterSortCycleBtn)
	{
		RosterSortCycleBtn->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnRosterSortCycleClicked);
	}
	UpdateRosterSortLabel();

	if (RosterFilterCombo)
	{
		RosterFilterCombo->OnSelectionChanged.AddDynamic(this, &UEmployeeWindowWidget::OnRosterFilterChanged);
	}

	if (CubeSlot0) { CubeSlot0->OnItemCardClicked.BindUObject(this, &UEmployeeWindowWidget::OnCubeSlotClicked, 0); }
	if (CubeSlot1) { CubeSlot1->OnItemCardClicked.BindUObject(this, &UEmployeeWindowWidget::OnCubeSlotClicked, 1); }
	if (CubeSlot2) { CubeSlot2->OnItemCardClicked.BindUObject(this, &UEmployeeWindowWidget::OnCubeSlotClicked, 2); }

	if (EnhanceButton)
	{
		EnhanceButton->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnEnhanceClicked);
	}
	if (FireButton)
	{
		FireButton->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnFireClicked);
	}
	if (OddsButton)
	{
		OddsButton->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnOddsClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UEmployeeWindowWidget::OnCloseDelegate);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.AddUObject(this, &UEmployeeWindowWidget::HandleEmployeeStatsChanged);
		EmployeeManager->OnEmployeeDisciplineChanged.AddUObject(this, &UEmployeeWindowWidget::HandleEmployeeDisciplineChanged);
		EmployeeManager->OnEmployeeLevelUp.AddUObject(this, &UEmployeeWindowWidget::HandleEmployeeLevelUp);
		EmployeeManager->OnEmployeeRosterChanged.AddUObject(this, &UEmployeeWindowWidget::HandleRosterChanged);
		EmployeeManager->OnExperienceGained.AddDynamic(this, &UEmployeeWindowWidget::HandleExperienceGained);
	}

	if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResourceMgr->OnResourceChanged.AddUObject(this, &UEmployeeWindowWidget::HandleResourceChanged);
	}
}

void UEmployeeWindowWidget::NativeDestruct()
{
	if (RosterListView)
	{
		RosterListView->OnEntryWidgetGenerated().RemoveAll(this);
	}

	if (RosterSortCycleBtn)
	{
		RosterSortCycleBtn->OnClicked().RemoveAll(this);
	}

	if (RosterFilterCombo)
	{
		RosterFilterCombo->OnSelectionChanged.RemoveDynamic(this, &UEmployeeWindowWidget::OnRosterFilterChanged);
	}

	if (RerollButton)
	{
		RerollButton->OnClicked().RemoveAll(this);
	}

	// 스탯 셀 탭 타깃 — NativeConstruct 의 AddUObject 와 쌍. 빠뜨리면 창 재오픈마다 누적돼 1탭에 N번 발화한다
	for (int32 i = 0; i < 6; ++i)
	{
		if (UCommonButtonBase* Btn = Cast<UCommonButtonBase>(
			GetWidgetFromName(*FString::Printf(TEXT("HeroStatBtn%d"), i))))
		{
			Btn->OnClicked().RemoveAll(this);
		}
	}

	if (CubeSlot0) { CubeSlot0->OnItemCardClicked.Unbind(); }
	if (CubeSlot1) { CubeSlot1->OnItemCardClicked.Unbind(); }
	if (CubeSlot2) { CubeSlot2->OnItemCardClicked.Unbind(); }

	if (EnhanceButton)
	{
		EnhanceButton->OnClicked().RemoveAll(this);
	}
	if (FireButton)
	{
		FireButton->OnClicked().RemoveAll(this);
	}
	if (OddsButton)
	{
		OddsButton->OnClicked().RemoveAll(this);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UEmployeeWindowWidget::OnCloseDelegate);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeDisciplineChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeLevelUp.RemoveAll(this);
		EmployeeManager->OnEmployeeRosterChanged.RemoveAll(this);
		EmployeeManager->OnExperienceGained.RemoveDynamic(this, &UEmployeeWindowWidget::HandleExperienceGained);
	}

	// teardown 중엔 GameInstance 가 먼저 사라질 수 있음
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.RemoveAll(this);
		}
	}

	// Deactivated 를 안 거치고 파괴되는 경로 안전망 (idempotent)
	ReleaseHeroLiveView();

	Super::NativeDestruct();
}

// DT_PanelIntro.PanelKey. 유니티 빌드 셰도잉을 피하려 이름을 길게 둔다
static const FName PanelIntroKey_EmployeeWindow(TEXT("EmployeeWindow"));

void UEmployeeWindowWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 위에 쌓인 패널이 닫혀 재활성될 때도 최신 상태 반영 (채용 P3 대비)
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	BuildingIndex = GameInstance ? GameInstance->GetCurrentManagedBuildingIndex() : INDEX_NONE;

	// 부서 표시명은 CompanyType(=BuildingIndex) 산업별 — 옵션 채움은 필터 적용(RefreshRoster) 전에
	SetupRosterFilterCombo();
	RefreshRoster();
	RefreshWallet();
	StartIntroStagger();

	// M9 튜토리얼 — 직원창 열림 등록+전이 (튜토리얼 외 no-op)
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyEmployeeWindowOpened(this);
	}

	// 최초 진입 코치마크 — 등장 스태거(≈0.46s)가 끝난 뒤 딤을 덮는다
	if (UPanelIntroSubsystem* IntroMgr = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>())
	{
		if (IntroMgr->ShouldPlay(PanelIntroKey_EmployeeWindow) && GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				PanelIntroTimer, this, &UEmployeeWindowWidget::TryPlayPanelIntro, 0.5f, false);
		}
	}
}

void UEmployeeWindowWidget::TryPlayPanelIntro()
{
	UGameInstance* GI = GetGameInstance();
	UPanelIntroSubsystem* IntroMgr = GI ? GI->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	// 타이머 대기 중 창이 닫혔을 수 있다
	if (!IntroMgr || !TableMgr || !IntroMgr->ShouldPlay(PanelIntroKey_EmployeeWindow) || !IsActivated())
	{
		return;
	}

	TSubclassOf<UUserWidget> OverlayClass = TableMgr->GetWidgetClass(EWidgetType::PanelIntroOverlay);
	if (!OverlayClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PanelIntro] EWidgetType::PanelIntroOverlay 미등록 — 최초 진입 안내 생략"));
		return;
	}
	UPanelIntroOverlayWidget* Overlay = CreateWidget<UPanelIntroOverlayWidget>(GetOwningPlayer(), OverlayClass);
	if (!Overlay)
	{
		return;
	}

	Overlay->AddToViewport(9100); // 미션 가이드(9000) 위
	Overlay->OnIntroFinished.AddUObject(this, &UEmployeeWindowWidget::HandlePanelIntroFinished);
	if (!Overlay->StartIntro(PanelIntroKey_EmployeeWindow, this))
	{
		Overlay->RemoveFromParent();
		return;
	}
	PanelIntroOverlay = Overlay;

	// 딤 2겹을 플래그로 협상하지 않고 가시성으로 배제한다 (설계 §3.3)
	TArray<UUserWidget*> FoundOverlays;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundOverlays, UMissionGuideOverlayWidget::StaticClass(), false);
	for (UUserWidget* Found : FoundOverlays)
	{
		if (Found && Found->GetVisibility() != ESlateVisibility::Collapsed)
		{
			HiddenMissionOverlay = Found;
			Found->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
	}
}

void UEmployeeWindowWidget::HandlePanelIntroFinished()
{
	if (UWidget* Mission = HiddenMissionOverlay.Get())
	{
		// 미션 오버레이 루트는 HitTestInvisible (입력 통과) — Visible 로 되돌리면 화면을 먹는다
		Mission->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	HiddenMissionOverlay.Reset();
	PanelIntroOverlay.Reset();
}

void UEmployeeWindowWidget::StartIntroStagger()
{
	// 컬럼 4개 이름 조회 — WBP 개편으로 이름이 사라지면 해당 컬럼만 자연 제외
	static const FName PanelNames[] = { FName("RailBox"), FName("HeroColBox"), FName("DisciplineCard"), FName("PotentialPanel") };
	IntroPanels.Reset();
	for (const FName& Name : PanelNames)
	{
		if (UWidget* Panel = GetWidgetFromName(Name))
		{
			Panel->SetRenderOpacity(0.f);
			Panel->SetRenderTranslation(FVector2D(0.f, IntroSlide));
			IntroPanels.Add(Panel);
		}
	}
	IntroElapsed = IntroPanels.Num() > 0 ? 0.f : -1.f;
}

void UEmployeeWindowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateFilterChipMaterialSize(MyGeometry);

	// 등장 스태거
	if (IntroElapsed >= 0.f)
	{
		IntroElapsed += InDeltaTime;

		bool bAllDone = true;
		for (int32 i = 0; i < IntroPanels.Num(); ++i)
		{
			UWidget* Panel = IntroPanels[i].Get();
			if (!Panel)
			{
				continue;
			}
			const float T = FMath::Clamp((IntroElapsed - i * IntroStagger) / FMath::Max(IntroDuration, 0.01f), 0.f, 1.f);
			const float Ease = 1.f - FMath::Cube(1.f - T);
			Panel->SetRenderOpacity(Ease);
			Panel->SetRenderTranslation(FVector2D(0.f, (1.f - Ease) * IntroSlide));
			bAllDone &= (T >= 1.f);
		}

		if (bAllDone)
		{
			IntroElapsed = -1.f;
		}
	}

	// 등급 글로우 브리딩 — 색/기본알파는 UpdateHeroLiveView 가, 호흡은 RenderOpacity 가 소유(간섭 없음)
	if (HeroRarityGlow && HeroRarityGlow->GetVisibility() != ESlateVisibility::Hidden)
	{
		GlowBreatheTime += InDeltaTime;
		const float Breathe = 1.f - GlowBreatheAmp * 0.5f * (1.f - FMath::Cos(2.f * PI * GlowBreatheHz * GlowBreatheTime));
		HeroRarityGlow->SetRenderOpacity(Breathe);
	}

	// 라이브 뷰 워밍업 페이드인 (캡처 첫 프레임 노출 정착 가림)
	if (LiveViewWarmupElapsed >= 0.f && HeroPortraitImage)
	{
		LiveViewWarmupElapsed += InDeltaTime;
		const float T = FMath::Clamp((LiveViewWarmupElapsed - LiveViewWarmupHold) / FMath::Max(LiveViewWarmupFade, 0.01f), 0.f, 1.f);
		HeroPortraitImage->SetRenderOpacity(T);
		if (T >= 1.f)
		{
			LiveViewWarmupElapsed = -1.f;
		}
	}

	// 잠재 리롤 연출 — 줄 3개 순차 재추첨(펀치+페이드인) + 등급 배지 펀치(등급업 시 강펀치) + 큐브 눌림
	if (RerollFxElapsed >= 0.f)
	{
		RerollFxElapsed += InDeltaTime;
		UBorder* FxLines[3] = { PotLine0, PotLine1, PotLine2 };
		const float LineAmp = bRerollUpgraded ? 0.09f : 0.05f;
		bool bFxDone = true;
		for (int32 i = 0; i < 3; ++i)
		{
			if (!FxLines[i])
			{
				continue;
			}
			const float T = FMath::Clamp((RerollFxElapsed - i * 0.07f) / 0.32f, 0.f, 1.f);
			const float LineScale = 1.f + LineAmp * FMath::Sin(PI * T);
			FxLines[i]->SetRenderScale(FVector2D(LineScale, LineScale));
			FxLines[i]->SetRenderOpacity(RerollLineTargetOpacity[i] * (0.35f + 0.65f * T));
			bFxDone &= (T >= 1.f);
		}
		if (TierBadge)
		{
			const float T = FMath::Clamp(RerollFxElapsed / 0.3f, 0.f, 1.f);
			const float BadgeScale = 1.f + (bRerollUpgraded ? 0.35f : 0.18f) * FMath::Sin(PI * T);
			TierBadge->SetRenderScale(FVector2D(BadgeScale, BadgeScale));
		}
		UItemCardSlotWidget* CubeFx[3] = { CubeSlot0, CubeSlot1, CubeSlot2 };
		if (RerollCubeIndex != INDEX_NONE && CubeFx[RerollCubeIndex])
		{
			const float T = FMath::Clamp(RerollFxElapsed / 0.25f, 0.f, 1.f);
			const float CubeScale = 1.f - 0.12f * FMath::Sin(PI * T);
			CubeFx[RerollCubeIndex]->SetRenderScale(FVector2D(CubeScale, CubeScale));
		}
		if (bFxDone)
		{
			RerollFxElapsed = -1.f;
			// 리롤 도중 RefreshPotential 이 딤(잠금 0.55)을 세팅했을 수 있어 오파시티는 건드리지 않고 종료
		}
	}
}

void UEmployeeWindowWidget::NativeOnDeactivated()
{
	// 인트로도 뷰포트 오버레이라 스택이 안 거둬간다. RemoveFromParent → NativeDestruct 가 복구 신호를 쏜다
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PanelIntroTimer);
	}
	if (UPanelIntroOverlayWidget* Intro = PanelIntroOverlay.Get())
	{
		Intro->RemoveFromParent();
	}
	// 오버레이가 이미 죽어 신호를 못 쏘는 경로 대비 — 미션 오버레이를 숨긴 채로 두지 않는다
	HandlePanelIntroFinished();

	// 직원창이 닫히면(ESC 포함) 위에 뜬 강화 모달도 함께 정리 — 뷰포트 오버레이라 스택이 안 거둬줌
	if (UEnhanceStarforceModalWidget* Modal = OpenEnhanceModal.Get())
	{
		Modal->RemoveFromParent();
	}
	OpenEnhanceModal.Reset();

	if (UPotentialOddsWidget* Odds = OpenOddsPanel.Get())
	{
		Odds->RemoveFromParent();
	}
	OpenOddsPanel.Reset();

	// 닫힘 시 스태거 중단 + 트랜스폼 원복 (재오픈은 StartIntroStagger 가 다시 세팅)
	IntroElapsed = -1.f;
	for (const TWeakObjectPtr<UWidget>& Weak : IntroPanels)
	{
		if (UWidget* Panel = Weak.Get())
		{
			Panel->SetRenderOpacity(1.f);
			Panel->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	// 캡처는 활성 동안만 — 닫힐 때 무대 반납(가챠가 인계 중이면 no-op)
	ReleaseHeroLiveView();

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

FEmployeeInstance* UEmployeeWindowWidget::GetSelectedEmployee() const
{
	return (EmployeeManager && SelectedEmployeeID >= 0) ? EmployeeManager->FindEmployee(SelectedEmployeeID) : nullptr;
}

UWidget* UEmployeeWindowWidget::GetSkillCardWidget() const
{
	if (!DisciplineCard)
	{
		return nullptr;
	}
	// SP 0 = [+] 전부 비활성. 카드 자체는 enabled 라 안전망을 통과해버리므로 여기서 걸러 null 을 반환한다.
	UWidget* FirstInvest = DisciplineCard->GetFirstInvestButtonWidget();
	if (!FirstInvest || !FirstInvest->GetIsEnabled())
	{
		return nullptr;
	}
	return DisciplineCard;
}

UWidget* UEmployeeWindowWidget::GetFirstSkillInvestButtonWidget() const
{
	return DisciplineCard ? DisciplineCard->GetFirstInvestButtonWidget() : nullptr;
}

UWidget* UEmployeeWindowWidget::GetEnhanceButtonWidget() const
{
	return EnhanceButton;
}

void UEmployeeWindowWidget::RefreshRoster()
{
	if (!RosterListView || !EmployeeManager)
	{
		return;
	}

	// 착석 + 벤치 전부 (같은 건물 소속)
	TArray<FEmployeeInstance> Roster = EmployeeManager->GetEmployeesInBuilding(BuildingIndex);
	Roster.Append(EmployeeManager->GetUnassignedEmployeesInBuilding(BuildingIndex));

	// 필터 → 정렬 순서 고정 (부서 필터 먼저, 그 다음 정렬 모드)
	if (RosterFilterDiscipline != EProductionDiscipline::Count)
	{
		Roster.RemoveAll([this](const FEmployeeInstance& E)
		{
			return DepartmentToDiscipline(E.Department) != RosterFilterDiscipline;
		});
	}

	Roster.Sort([this](const FEmployeeInstance& A, const FEmployeeInstance& B)
	{
		switch (RosterSortMode)
		{
		case ERosterSortMode::Rarity: return A.SpawnRarity > B.SpawnRarity;
		case ERosterSortMode::Name:   return A.EmployeeName < B.EmployeeName;
		default:                      return A.Level > B.Level; // 레벨순 기본
		}
	});

	if (RosterHeaderText)
	{
		RosterHeaderText->SetText(FText::FromString(FString::Printf(TEXT("보유 직원 %d"), Roster.Num())));
	}

	// 선택 유지 — 사라졌으면(해고 등) 종합 1위로
	const bool bSelectedAlive = Roster.ContainsByPredicate(
		[this](const FEmployeeInstance& E) { return E.EmployeeID == SelectedEmployeeID; });
	if (!bSelectedAlive)
	{
		SelectedEmployeeID = Roster.Num() > 0 ? Roster[0].EmployeeID : -1;
	}

	RosterListView->ClearListItems();
	UObject* SelectedItem = nullptr;
	for (const FEmployeeInstance& EmployeeData : Roster)
	{
		UEntityCardData* CardData = NewObject<UEntityCardData>(this);
		CardData->EmployeeInfo = EmployeeData;
		RosterListView->AddItem(CardData);
		if (EmployeeData.EmployeeID == SelectedEmployeeID)
		{
			SelectedItem = CardData;
		}
	}
	if (SelectedItem)
	{
		RosterListView->SetSelectedItem(SelectedItem);
	}

	RefreshSelected();
}

void UEmployeeWindowWidget::SetupRosterFilterCombo()
{
	if (!RosterFilterCombo)
	{
		return;
	}

	// 배정 빌딩 CompanyType — 부서 표시명이 산업별이라 필요
	ECompanyType CompanyType = ECompanyType::None;
	if (UEntityManager* EntityMgr = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr)
	{
		if (ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(BuildingIndex))
		{
			CompanyType = Building->GetCompanyType();
		}
	}
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// 재오픈 시 다른 회사일 수 있어 매번 재구성 — GetDepartmentDisplayName 은 매핑 없어도 DepartmentToString 폴백이라 항상 비지 않음
	RosterFilterCombo->ClearOptions();
	RosterFilterCombo->AddOption(TEXT("전체"));
	for (uint8 i = 0; i < static_cast<uint8>(EProductionDiscipline::Count); ++i)
	{
		const EEmployeeDepartment Dept = DisciplineToDepartment(static_cast<EProductionDiscipline>(i));
		const FText DisplayName = TableMgr ? TableMgr->GetDepartmentDisplayName(CompanyType, Dept) : FText::FromString(DepartmentToString(Dept));
		RosterFilterCombo->AddOption(DisplayName.ToString());
	}

	const int32 SelectIndex = (RosterFilterDiscipline == EProductionDiscipline::Count)
		? 0 : static_cast<int32>(RosterFilterDiscipline) + 1;
	RosterFilterCombo->SetSelectedIndex(SelectIndex);
}

void UEmployeeWindowWidget::OnRosterSortCycleClicked()
{
	switch (RosterSortMode)
	{
	case ERosterSortMode::Level:  RosterSortMode = ERosterSortMode::Rarity; break;
	case ERosterSortMode::Rarity: RosterSortMode = ERosterSortMode::Name;   break;
	default:                      RosterSortMode = ERosterSortMode::Level; break;
	}

	UpdateRosterSortLabel();
	RefreshRoster();
}

void UEmployeeWindowWidget::OnRosterFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 Index = RosterFilterCombo ? RosterFilterCombo->GetSelectedIndex() : 0;
	RosterFilterDiscipline = (Index <= 0) ? EProductionDiscipline::Count : static_cast<EProductionDiscipline>(Index - 1);
	RefreshRoster();
}

void UEmployeeWindowWidget::UpdateRosterSortLabel()
{
	if (!RosterSortCycleBtn)
	{
		return;
	}

	FString Label;
	switch (RosterSortMode)
	{
	case ERosterSortMode::Rarity: Label = TEXT("등급순"); break;
	case ERosterSortMode::Name:   Label = TEXT("이름순"); break;
	default:                      Label = TEXT("레벨순"); break;
	}
	RosterSortCycleBtn->SetButtonText(FText::FromString(Label + TEXT(" ▼")));
}

void UEmployeeWindowWidget::RefreshSelected()
{
	RefreshHero();
	RefreshHeroStats();
	RefreshPotential();
	if (DisciplineCard) { DisciplineCard->Configure(SelectedEmployeeID); }
}

void UEmployeeWindowWidget::RefreshHero()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();

	UpdateHeroLiveView(Employee);

	if (HeroNameText)
	{
		HeroNameText->SetText(Employee ? FText::FromString(Employee->EmployeeName) : FText::FromString(TEXT("직원 없음")));
	}

	if (StarBadge)
	{
		// 히어로 강화 = 골드 별 아이콘 N개 (카드는 "+N", 히어로는 개수 시각화 — 2026-07-25 사용자 확정)
		// 0강 = 빈 별(SfEmpty) 1개 자리표시 — 별 유무로 정체성 블록 높이가 출렁이는 것 방지 (2026-07-26)
		const int32 Enh = Employee ? Employee->EnhancementLevel : 0;
		StarBadge->SetVisibility(Employee ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (UHorizontalBox* StarRow = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("StarRow"))))
		{
			// 전 자식 스윕 후 재생성 — WBP 잔존물(구 StarsText 등)도 함께 청소
			TArray<UWidget*> Stale;
			for (int32 i = 0; i < StarRow->GetChildrenCount(); ++i)
			{
				Stale.Add(StarRow->GetChildAt(i));
			}
			for (UWidget* W : Stale) { StarRow->RemoveChild(W); }

			static const TSoftObjectPtr<UTexture2D> FilledTexPtr(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Rating/T_UIIcon_Star_SfFilled.T_UIIcon_Star_SfFilled")));
			static const TSoftObjectPtr<UTexture2D> EmptyTexPtr(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Rating/T_UIIcon_Star_SfEmpty.T_UIIcon_Star_SfEmpty")));
			UTexture2D* StarTex = (Enh > 0 ? FilledTexPtr : EmptyTexPtr).LoadSynchronous();
			const int32 StarCount = FMath::Max(Enh, 1);

			// 한 줄 유지 — 만강까지 카드 안에 들어가도록 개수에 맞춰 축소(줄바꿈 대신).
			// StarRowMaxWidth = 히어로 카드 내부 폭(컬럼 586.7 - 패딩 28*2)
			const float StarRowMaxWidth = 530.f;
			const float StarGap = 4.f;
			const float FitPx = (StarRowMaxWidth - StarGap * (StarCount - 1)) / StarCount;
			const float StarPx = FMath::Clamp(FMath::FloorToFloat(FitPx), 20.f, 40.f);

			for (int32 s = 0; s < StarCount; ++s)
			{
				UImage* Star = NewObject<UImage>(this);
				FSlateBrush StarBrush;
				StarBrush.SetResourceObject(StarTex);
				StarBrush.ImageSize = FVector2D(StarPx, StarPx);
				Star->SetBrush(StarBrush);
				if (UHorizontalBoxSlot* StarSlot = StarRow->AddChildToHorizontalBox(Star))
				{
					StarSlot->SetVerticalAlignment(VAlign_Center);
					StarSlot->SetPadding(FMargin(s == 0 ? 0.f : StarGap, 0.f, 0.f, 0.f));
				}
			}
		}
	}

	if (HeroSubText)
	{
		if (Employee)
		{
			HeroSubText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Employee->SpawnRarity)));
			// 라이트 플레이트 위 가독 — 등급색 원색(일반=실버)은 플레이트와 동화, ×0.55 다크 파생(레이더 스트로크 문법)
			FLinearColor SubColor = FLootBoxRarityUtility::GetRarityColor(Employee->SpawnRarity) * 0.55f;
			// ×0.55 는 채도색(희귀~신화)엔 듣지만 일반(무채색 회색)은 #A1A1A1 = 카드 대비 2.36:1 로 실패한다.
			// 캡션 잉크 하한 #5A6B7D 의 상대휘도까지 눌러 전 등급이 5:1 이상을 확보(등급 코딩은 색상比 유지).
			const float CaptionFloorLum = 0.1417f;
			const float SubLum = 0.2126f * SubColor.R + 0.7152f * SubColor.G + 0.0722f * SubColor.B;
			if (SubLum > CaptionFloorLum)
			{
				SubColor *= CaptionFloorLum / SubLum;
			}
			SubColor.A = 1.f;
			HeroSubText->SetColorAndOpacity(FSlateColor(SubColor));
		}
		else
		{
			HeroSubText->SetText(FText::GetEmpty());
		}
	}

	// FC 재질 — 스테이지 등급 틴트: 면 16% / 프레임 42% 다크 / 젬 원색 (목업 승인 2026-07-25, sRGB 혼합→linear)
	{
		const FLinearColor RaritySrgb = FLootBoxRarityUtility::GetRarityColor(
			Employee ? Employee->SpawnRarity : ELootBoxRarity::Common);
		auto S2L = [](float C) { return FMath::Pow((C + 0.055f) / 1.055f, 2.4f); };
		auto MixL = [&](const FLinearColor& BaseSrgb, float Ratio, float Alpha)
		{
			const FLinearColor M = BaseSrgb * (1.f - Ratio) + RaritySrgb * Ratio;
			return FLinearColor(S2L(M.R), S2L(M.G), S2L(M.B), Alpha);
		};
		if (UImage* Face = Cast<UImage>(GetWidgetFromName(TEXT("PortraitPlateBG"))))
		{
			FSlateBrush FaceBrush = Face->GetBrush();
			FaceBrush.TintColor = FSlateColor(MixL(FLinearColor(0.90f, 0.92f, 0.94f), 0.16f, 1.f));
			Face->SetBrush(FaceBrush);
		}
		if (UImage* StageFrame = Cast<UImage>(GetWidgetFromName(TEXT("StageFrame"))))
		{
			FSlateBrush FrameBrush = StageFrame->GetBrush();
			FrameBrush.OutlineSettings.Color = FSlateColor(MixL(FLinearColor(0.50f, 0.55f, 0.62f), 0.42f, 0.5f));
			StageFrame->SetBrush(FrameBrush);
		}
		if (UImage* Gem = Cast<UImage>(GetWidgetFromName(TEXT("HeroRarityGem"))))
		{
			Gem->SetColorAndOpacity(FLinearColor(S2L(RaritySrgb.R), S2L(RaritySrgb.G), S2L(RaritySrgb.B), 1.f));
		}
	}

	if (HeroPortraitRim)
	{
		HeroPortraitRim->SetColorAndOpacity(Employee
			? FLootBoxRarityUtility::GetRarityColor(Employee->SpawnRarity)
			: FLinearColor::Transparent);
	}

	// 직능 칩 = DisciplinePoints 최고값의 enum DisplayName + 글리프 (로스터 카드와 동일 argmax — FIFA 포지션 문법, 2026-07-25)
	if (DeptChipText)
	{
		FText DiscName;
		int32 BestIdx = -1;
		if (Employee)
		{
			int32 BestVal = 0;
			for (int32 i = 0; i < Employee->DisciplinePoints.Num(); ++i)
			{
				if (Employee->DisciplinePoints[i] > BestVal) { BestVal = Employee->DisciplinePoints[i]; BestIdx = i; }
			}
			if (BestIdx >= 0)
			{
				// 표시명 SOT = DT_DisciplineDisplay (슬롯 의미는 enum 고정, 표시만 산업별 재해석)
				UGameInstance* GI = GetGameInstance();
				UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
				UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
				const ECompanyType Industry = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;
				if (TableMgr)
				{
					DiscName = TableMgr->GetDisciplineDisplayName(Industry, BestIdx);
				}
			}
		}
		DeptChipText->SetText(DiscName);

		if (UImage* ChipIcon = Cast<UImage>(GetWidgetFromName(TEXT("DeptChipIcon"))))
		{
			static const TCHAR* DiscIconPaths[6] = {
				TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Plan.T_UIIcon_Disc_Plan"),
				TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Dev.T_UIIcon_Disc_Dev"),
				TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Art.T_UIIcon_Disc_Art"),
				TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Sound.T_UIIcon_Disc_Sound"),
				TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Server.T_UIIcon_Disc_Server"),
				TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_QA.T_UIIcon_Disc_QA"),
			};
			UTexture2D* DiscTex = (BestIdx >= 0 && BestIdx < 6)
				? TSoftObjectPtr<UTexture2D>(FSoftObjectPath(DiscIconPaths[BestIdx])).LoadSynchronous()
				: nullptr;
			if (DiscTex) { ChipIcon->SetBrushFromTexture(DiscTex); }
			ChipIcon->SetVisibility(DiscTex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	if (OverallText)
	{
		// OVR = 최고 직능 포인트 (로스터 카드 레이팅과 동일 값 — FIFA 문법, 2026-07-25)
		int32 BestDisc = 0;
		if (Employee)
		{
			for (const int32 P : Employee->DisciplinePoints) { BestDisc = FMath::Max(BestDisc, P); }
		}
		// 라벨 "OVR"은 WBP OvrEyebrow 가 담당 — 여긴 숫자만 (v9x 2단 구성)
		OverallText->SetText(Employee
			? FText::FromString(FString::Printf(TEXT("%d"), BestDisc))
			: FText::GetEmpty());
	}

	if (EnhanceButton)
	{
		EnhanceButton->SetIsEnabled(Employee != nullptr);
	}
	if (FireButton)
	{
		FireButton->SetIsEnabled(Employee != nullptr);
	}

	RefreshExpOnly();

}

void UEmployeeWindowWidget::UpdateHeroLiveView(FEmployeeInstance* Employee)
{
	// 미니 스테이지 장식(글로우/접지 그림자)은 캐릭터와 운명 공동체
	const ESlateVisibility StageVis = Employee ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
	if (HeroRarityGlow)
	{
		// FC 재질 v9x: 무대광=전 등급 (스테이지가 등급색 재질면이라 일반 실버도 흰 끼 없음 — 일반만 저알파)
		const bool bShowGlow = Employee != nullptr;
		HeroRarityGlow->SetVisibility(bShowGlow ? StageVis : ESlateVisibility::Hidden);
		if (bShowGlow)
		{
			FLinearColor GlowColor = FLootBoxRarityUtility::GetRarityColor(Employee->SpawnRarity);
			GlowColor.A = (Employee->SpawnRarity == ELootBoxRarity::Common) ? 0.18f : 0.28f;
			HeroRarityGlow->SetColorAndOpacity(GlowColor);
		}
	}
	if (HeroGroundShadow)
	{
		HeroGroundShadow->SetVisibility(StageVis);
	}

	if (!HeroPortraitImage)
	{
		return;
	}

	if (!Employee)
	{
		ReleaseHeroLiveView();
		HeroPortraitImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// 라이브 뷰 비활성(노브) 시 정적 초상화 폴백 — 흰색 렌더 재발 시 이 노브로 즉시 우회 (현재 기본 = 라이브뷰 on)
	if (!bLiveHeroViewEnabled)
	{
		ReleaseHeroLiveView();
		LiveViewWarmupElapsed = -1.f;
		HeroPortraitImage->SetRenderOpacity(1.f);
		UTexture2D* Portrait = UEmployeeRosterCardWidget::LoadPortraitTexture(Employee->EmployeeID, HeroPortraitSize);
		if (Portrait)
		{
			HeroPortraitImage->SetBrushFromTexture(Portrait);
			HeroPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			HeroPortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	// 비활성 중(닫히는 중/전환) 무대 재획득 금지 — 재활성 시 NativeOnActivated 경로가 다시 채움
	if (!IsActivated())
	{
		return;
	}

	// 같은 직원 + 같은 의상 축(강화) + 내가 소유 중 = 재의상 불필요 (스탯/잠재 갱신 등)
	if (IsValid(LiveStage) && LiveStage->IsHeldBy(this)
		&& Employee->EmployeeID == LiveViewEmployeeID
		&& Employee->EnhancementLevel == LiveViewEnhancementLevel)
	{
		return;
	}

	UTextureRenderTarget2D* Rt = nullptr;
	if (UClass* StageCls = LiveStageClass.LoadSynchronous())
	{
		LiveStage = AGachaCaptureStage::AcquireShared(this, GetWorld(), StageCls);
		if (LiveStage)
		{
			// Idle(숨쉬기) — 제자리 걷기는 정면 카메라에서 러닝머신처럼 어색 (2026-07-20 PIE 피드백)
			Rt = LiveStage->BeginReveal(*Employee, EStageAnimMode::Idle);
		}
	}

	if (!Rt)
	{
		// 무대 불가(클래스 로드/스폰 실패) — 증명사진 PNG 폴백
		LiveViewEmployeeID = -1;
		LiveViewEnhancementLevel = -1;
		LiveViewWarmupElapsed = -1.f;
		HeroPortraitImage->SetRenderOpacity(1.f);
		UTexture2D* Portrait = UEmployeeRosterCardWidget::LoadPortraitTexture(Employee->EmployeeID, HeroPortraitSize);
		if (Portrait)
		{
			HeroPortraitImage->SetBrushFromTexture(Portrait);
			HeroPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			HeroPortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	LiveViewEmployeeID = Employee->EmployeeID;
	LiveViewEnhancementLevel = Employee->EnhancementLevel;

	if (!LiveViewMID)
	{
		// RT 알파 반전(캐릭터=투명/배경=불투명) 보정 — 가챠 리빌과 동일 컷아웃 기법(OneMinus a)
		TSoftObjectPtr<UMaterialInterface> MatPath(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT.M_GachaRevealRT")));
		if (UMaterialInterface* Mat = MatPath.LoadSynchronous())
		{
			LiveViewMID = UMaterialInstanceDynamic::Create(Mat, this);
		}
	}

	if (LiveViewMID)
	{
		LiveViewMID->SetTextureParameterValue(FName("RT"), Rt);
		HeroPortraitImage->SetBrushFromMaterial(LiveViewMID);
	}
	else
	{
		FSlateBrush Brush = HeroPortraitImage->GetBrush();
		Brush.SetResourceObject(Rt); // 폴백(머티리얼 미생성): 알파 반전 상태 직표시
		HeroPortraitImage->SetBrush(Brush);
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] LiveView MID 생성 실패 — M_GachaRevealRT 로드 확인 (RT 원본 직표시)"));
	}

	// 캡처 첫 프레임(노출/TAA 정착) 가림 — 홀드 후 페이드인 (구 "흰색 렌더" 과도기 대응)
	HeroPortraitImage->SetRenderOpacity(0.f);
	LiveViewWarmupElapsed = 0.f;
	HeroPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UEmployeeWindowWidget::ReleaseHeroLiveView()
{
	if (IsValid(LiveStage))
	{
		LiveStage->ReleaseShared(this); // 가챠가 인계했으면 no-op — 그쪽이 반납
	}
	LiveStage = nullptr;
	LiveViewEmployeeID = -1;
	LiveViewEnhancementLevel = -1;
}

void UEmployeeWindowWidget::RefreshExpOnly()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();

	if (LevelText)
	{
		LevelText->SetText(Employee
			? FText::FromString(FString::Printf(TEXT("Lv.%d / %d"), Employee->Level, MaxEmployeeLevelDisplay))
			: FText::FromString(TEXT("-")));
	}

	float Percent = 0.f;
	if (Employee && EmployeeManager)
	{
		const int32 MaxExp = EmployeeManager->GetMaxExperienceForLevel(Employee->Level);
		Percent = MaxExp > 0 ? FMath::Clamp(Employee->Experience / MaxExp, 0.f, 1.f) : 0.f;
	}

	if (ExpBar)
	{
		ExpBar->SetPercent(Percent);
		// 20px 얇은 바 + Comet 유효 발광부(캔버스의 ~60%) 감안 — 기본 스필 15론 바를 못 덮어 보임 (2026-07-22 PIE)
		UGlobalUtilFunctions::UpdateProgressHead(ExpBar, ExpBarHead, Percent, true, 24.f);
	}

	if (ExpText)
	{
		ExpText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Percent * 100.f)));
	}
}

void UEmployeeWindowWidget::RefreshHeroStats()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	const int32 EnhB = Employee ? UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel) : 0;

	UCommonTextBlock* Names[6]   = { HeroStatName0, HeroStatName1, HeroStatName2, HeroStatName3, HeroStatName4, HeroStatName5 };
	UCommonTextBlock* Values[6]  = { HeroStatValue0, HeroStatValue1, HeroStatValue2, HeroStatValue3, HeroStatValue4, HeroStatValue5 };
	UCommonTextBlock* Bonuses[6] = { HeroStatBonus0, HeroStatBonus1, HeroStatBonus2, HeroStatBonus3, HeroStatBonus4, HeroStatBonus5 };
	UCommonTextBlock* Descs[6]   = { HeroStatDesc0, HeroStatDesc1, HeroStatDesc2, HeroStatDesc3, HeroStatDesc4, HeroStatDesc5 };

	for (int32 i = 0; i < 6; ++i)
	{
		if (Names[i])
		{
			Names[i]->SetText(UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(i)));
		}

		// 토글은 칩(부모 Border)째 — 텍스트만 숨기면 빈 알약이 남는다 (BadgePlateOrSelf)
		UWidget* BonusToggle = (Bonuses[i] && Bonuses[i]->GetParent() && Bonuses[i]->GetParent()->IsA<UBorder>())
			? static_cast<UWidget*>(Bonuses[i]->GetParent()) : Bonuses[i];

		if (!Employee)
		{
			if (Values[i])   { Values[i]->SetText(FText::GetEmpty()); }
			if (Descs[i])    { Descs[i]->SetText(FText::GetEmpty()); }
			if (BonusToggle) { BonusToggle->SetVisibility(ESlateVisibility::Collapsed); }
			continue;
		}

		const int32 BaseValue = UEmployeeStatsHelper::GetStatValueByIndex(Employee->Stats, static_cast<uint8>(i));
		const int32 EffectiveValue = BaseValue + EnhB;

		// 값 = 체감 단위 단독(2026-08-13). 원시 포인트는 "52" 처럼 아무 느낌이 없어 카드에서 감췄다 —
		// 포인트가 필요한 표면(종합·강화 모달)은 GetStatValueByIndex 를 직접 쓴다.
		if (Values[i])
		{
			Values[i]->SetText(UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(i), EffectiveValue));
		}

		// 그린칩도 같은 단위의 델타. 부호는 실제 방향 그대로 — 업무속도만 음수가 이득이다.
		if (Bonuses[i])
		{
			const FText DeltaText = UEmployeeStatsHelper::GetStatFeltDeltaText(
				static_cast<uint8>(i), BaseValue, EffectiveValue);
			Bonuses[i]->SetText(DeltaText);
			BonusToggle->SetVisibility(
				DeltaText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}

		if (Descs[i])
		{
			// 효과 문구는 계수(EmployeeStatTuning/DA_FatigueConfig)에서 계산 — 하드코딩 0
			Descs[i]->SetText(UEmployeeStatsHelper::GetStatEffectText(static_cast<uint8>(i), EffectiveValue));
		}
	}
}

void UEmployeeWindowWidget::UpdateFilterChipMaterialSize(const FGeometry& /*MyGeometry*/)
{
	// 콤보 자신의 지오메트리를 써야 한다 — 패널 전체 크기를 넣으면 코너 반경이 엉뚱해진다
	// TODO(perf): 이 이름 조회가 크기 early-out 앞이라 창이 열려 있는 내내 매 프레임 위젯트리를 순회한다.
	//   같은 파일 HandleStatInfoTapped 의 이름 규약과 달리 여기는 틱 경로 — 캐시 멤버로 1회만 해석할 것
	//   (헤더 변경이라 풀빌드 창에서 처리).
	UWidget* Combo = GetWidgetFromName(TEXT("RosterFilterCombo"));
	if (!Combo)
	{
		return;
	}

	const FVector2D LocalSize = Combo->GetCachedGeometry().GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastFilterChipSize, 0.5f))
	{
		return;
	}
	LastFilterChipSize = LocalSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (const TCHAR* Name : { TEXT("FilterChipBG"), TEXT("FilterChipLine") })
	{
		if (UImage* Layer = Cast<UImage>(GetWidgetFromName(Name)))
		{
			if (UMaterialInstanceDynamic* MID = Layer->GetDynamicMaterial())
			{
				MID->SetScalarParameterValue(WpxParam, LocalSize.X);
				MID->SetScalarParameterValue(HpxParam, LocalSize.Y);
			}
		}
	}
}

void UEmployeeWindowWidget::HandleStatInfoTapped(int32 StatIndex)
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	if (!Employee || StatIndex < 0 || StatIndex >= 6)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return;
	}

	const int32 EnhB = UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
	const int32 EffectiveValue =
		UEmployeeStatsHelper::GetStatValueByIndex(Employee->Stats, static_cast<uint8>(StatIndex)) + EnhB;

	// 앵커 = 탭한 셀의 화면 중심. 커서 좌표는 모바일 터치에서 갱신되지 않아 (0,0) 에 뜬다.
	FVector2D AnchorAbs = FVector2D::ZeroVector;
	if (UWidget* Btn = GetWidgetFromName(*FString::Printf(TEXT("HeroStatBtn%d"), StatIndex)))
	{
		const FGeometry& Geo = Btn->GetCachedGeometry();
		AnchorAbs = Geo.LocalToAbsolute(Geo.GetLocalSize() * 0.5f);
	}

	if (!ActiveStatTooltip)
	{
		const TSubclassOf<UUserWidget> TooltipClass = TableMgr->GetWidgetClass(EWidgetType::ItemTooltip);
		if (!TooltipClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] EWidgetType::ItemTooltip 미등록"));
			return;
		}
		ActiveStatTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass);
	}

	if (ActiveStatTooltip)
	{
		ActiveStatTooltip->ShowAt(
			AnchorAbs,
			UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(StatIndex)),
			nullptr,
			UEmployeeStatsHelper::GetStatEffectText(static_cast<uint8>(StatIndex), EffectiveValue),
			/*BasePrice*/ 0, /*Duration*/ 2.5f, ETooltipAnchor::BelowAnchor);
	}
}

void UEmployeeWindowWidget::RefreshPotential()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();

	UBorder* Lines[3] = { PotLine0, PotLine1, PotLine2 };
	UImage* Bars[3] = { PotLineBar0, PotLineBar1, PotLineBar2 };
	UImage* Grads[3] = { PotLineGrad0, PotLineGrad1, PotLineGrad2 };
	UImage* Glows[3] = { PotLineGlow0, PotLineGlow1, PotLineGlow2 };
	UCommonTextBlock* Names[3] = { PotLineName0, PotLineName1, PotLineName2 };
	UCommonTextBlock* Values[3] = { PotLineValue0, PotLineValue1, PotLineValue2 };

	// v7 젤 플레이트 — 플레이트 아웃라인/그라데이션/림 글로우를 등급색으로 통일 주입
	const FLinearColor LockedOutline(0.584f, 0.638f, 0.716f, 1.f);
	auto ApplyPlateStyle = [&](int32 Index, const FLinearColor& Grade, bool bActive)
	{
		if (Grads[Index])
		{
			Grads[Index]->SetColorAndOpacity(FLinearColor(Grade.R, Grade.G, Grade.B, bActive ? 0.10f : 0.f));
		}
		if (Glows[Index])
		{
			Glows[Index]->SetColorAndOpacity(FLinearColor(Grade.R, Grade.G, Grade.B, bActive ? 0.12f : 0.f));
		}
		if (Lines[Index])
		{
			FSlateBrush Brush = Lines[Index]->Background;
			Brush.OutlineSettings.Color = bActive
				? FSlateColor(FLinearColor(Grade.R, Grade.G, Grade.B, 0.45f))
				: FSlateColor(LockedOutline);
			Lines[Index]->SetBrush(Brush);
		}
	};

	const int32 Slots = Employee
		? UEmployeePotentialHelper::GetPotentialSlotsForRank(
			UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel))
		: 0;
	const TArray<FPotentialOptionLine>* Options = Employee ? &Employee->PotentialAbility.Options : nullptr;

	for (int32 i = 0; i < 3; ++i)
	{
		if (!Lines[i] || !Names[i] || !Values[i])
		{
			continue;
		}

		if (i >= Slots)
		{
			// 잠긴 칸 — 해금 조건 = 슬롯 매핑(★0~2:1줄/★3~5:2줄/★6+:3줄)의 경계 별 수
			Lines[i]->SetRenderOpacity(0.55f);
			Names[i]->SetText(FText::FromString(TEXT("잠김")));
			Values[i]->SetText(FText::FromString(i == 1 ? TEXT("3성에 해금") : TEXT("6성에 해금")));
			Values[i]->SetColorAndOpacity(FSlateColor(InkMute));
			if (Bars[i])
			{
				Bars[i]->SetColorAndOpacity(PotBarNeutral);
			}
			ApplyPlateStyle(i, LockedOutline, false);
		}
		else if (Options && i < Options->Num())
		{
			const FPotentialOptionLine& Line = (*Options)[i];
			const FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(Line.Rarity);
			// 라이트 젤 위 가독용 다크 파생 (linear ×0.55 ≈ 지각 25% 어둡게)
			FLinearColor RarityDark = RarityColor * 0.55f;
			RarityDark.A = 1.f;
			Lines[i]->SetRenderOpacity(1.f);
			Names[i]->SetText(UEmployeePotentialHelper::GetPotentialOptionName(Line.OptionType));
			Values[i]->SetText(FText::FromString(FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(Line.Value))));
			Values[i]->SetColorAndOpacity(FSlateColor(RarityDark));
			if (Bars[i])
			{
				Bars[i]->SetColorAndOpacity(RarityColor);
			}
			ApplyPlateStyle(i, RarityColor, true);
		}
		else
		{
			// 해금됐지만 아직 안 굴린 줄 (강화로 슬롯이 늘어난 직후)
			Lines[i]->SetRenderOpacity(1.f);
			Names[i]->SetText(FText::FromString(TEXT("빈 줄")));
			Values[i]->SetText(FText::FromString(TEXT("리롤로 개방")));
			Values[i]->SetColorAndOpacity(FSlateColor(InkMute));
			if (Bars[i])
			{
				Bars[i]->SetColorAndOpacity(PotBarNeutral);
			}
			ApplyPlateStyle(i, LockedOutline, false);
		}
	}

	const ELootBoxRarity CurrentRarity = Employee ? Employee->PotentialAbility.CurrentRarity : ELootBoxRarity::Common;
	if (TierBadge)
	{
		// 다크 등급 틴트 칩 — 등급색은 텍스트가 담당(밝은 등급서도 흰바탕 대비 보장)
		FLinearColor Fill = FLootBoxRarityUtility::GetRarityColor(CurrentRarity) * 0.13f;
		Fill.A = 1.f;
		TierBadge->SetBrushColor(Fill);
	}
	if (TierText)
	{
		TierText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(CurrentRarity)));
		FLinearColor TierCol = FLootBoxRarityUtility::GetRarityColor(CurrentRarity);
		TierCol.A = 1.f;
		TierText->SetColorAndOpacity(FSlateColor(TierCol));   // 등급색 텍스트 = 다크칩 위 가독 + 등급 코딩
	}

	RefreshCubeSlots();
}

void UEmployeeWindowWidget::RefreshCubeSlots()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();

	UItemCardSlotWidget* Slots[3] = { CubeSlot0, CubeSlot1, CubeSlot2 };
	UCommonTextBlock* CubeNames[3] = { CubeName0, CubeName1, CubeName2 };
	UCommonTextBlock* CubeCaps[3] = { CubeCap0, CubeCap1, CubeCap2 };

	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();

	int32 SelectedCubeCount = 0;

	for (int32 i = 0; i < 3; ++i)
	{
		const EItemType CubeType = CubeTypes[i];
		const int32 Count = ItemMgr ? ItemMgr->GetItemCount(CubeType) : 0;

		if (CubeNames[i])
		{
			// 표시명 SOT = ItemType.h (⚠ UMETA 는 에디터 전용 — 패키징 빌드에서 "BusinessCardPaper" 로 떨어진다)
			CubeNames[i]->SetText(GetItemTypeDisplayName(CubeType));
		}
		if (CubeCaps[i])
		{
			const ELootBoxRarity Ceiling = UEmployeePotentialHelper::GetCubeCeiling(CubeType);
			CubeCaps[i]->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Ceiling) + TEXT("까지")));
		}
		if (CubeType == SelectedCube)
		{
			SelectedCubeCount = Count;
		}

		if (Slots[i])
		{
			bool bFound = false;
			UTexture2D* Icon = TableMgr ? TableMgr->GetItemIcon(CubeType, bFound) : nullptr;
			if (!bFound)
			{
				// T3 전 임시 폴백 — DT_ShopItem 신규 큐브 2종 행 등록되면 위 GetItemIcon 이 실제 아이콘을 우선 반환
				Icon = FallbackCubeIcon.LoadSynchronous();
			}

			Slots[i]->SetItem(Icon, Count);
			Slots[i]->SetInsufficient(Count <= 0);
			Slots[i]->SetSelected(CubeType == SelectedCube);
			Slots[i]->SetRenderOpacity(Count > 0 ? 1.f : 0.4f);
		}
	}

	if (RerollButton)
	{
		const bool bCanRoll = (Employee != nullptr && SelectedCubeCount > 0);
		RerollButton->SetIsEnabled(bCanRoll);

		// 비활성 라벨은 스타일상 흰색이라 크림 플레이트 위에서 사라진다 — 잉크색으로 눌러 읽히게 한다
		// (라이트 플레이트 위 컬러 버튼의 disabled 공통 함정. 활성일 때는 스타일 기본 흰색으로 복귀)
		RerollButton->SetTextColor(bCanRoll
			? FSlateColor(FLinearColor::White)
			: FSlateColor(FLinearColor(0.353f, 0.420f, 0.490f, 1.f)));
		RerollButton->SetTextOutlineEnabled(bCanRoll, 2.f, FLinearColor(0.106f, 0.310f, 0.475f, 1.f));
	}

	if (OddsButton)
	{
		// 확률표는 재고와 무관하게 언제나 볼 수 있어야 한다 — 살 이유를 판단하는 화면이므로
		OddsButton->SetIsEnabled(Employee != nullptr);
	}
}

void UEmployeeWindowWidget::RefreshWallet()
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		return;
	}

	if (WalletMoneyChip)
	{
		WalletMoneyChip->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Money));
	}
}

void UEmployeeWindowWidget::SelectEmployee(int32 EmployeeID)
{
	if (EmployeeID < 0 || EmployeeID == SelectedEmployeeID)
	{
		return;
	}

	SelectedEmployeeID = EmployeeID;

	// ListView 아이템 선택 동기화 → 카드 하이라이트는 엔트리가 스스로 갱신
	if (RosterListView)
	{
		for (UObject* Item : RosterListView->GetListItems())
		{
			UEntityCardData* CardData = Cast<UEntityCardData>(Item);
			if (CardData && CardData->EmployeeInfo.EmployeeID == EmployeeID)
			{
				RosterListView->SetSelectedItem(Item);
				break;
			}
		}
	}

	RefreshSelected();
}

void UEmployeeWindowWidget::OnRosterCardClicked(int32 EmployeeID)
{
	SelectEmployee(EmployeeID);
}

void UEmployeeWindowWidget::OnRosterEntryGenerated(UUserWidget& EntryWidget)
{
	if (UEmployeeRosterCardWidget* Card = Cast<UEmployeeRosterCardWidget>(&EntryWidget))
	{
		Card->OnRosterCardClicked.RemoveDynamic(this, &UEmployeeWindowWidget::OnRosterCardClicked);
		Card->OnRosterCardClicked.AddDynamic(this, &UEmployeeWindowWidget::OnRosterCardClicked);
	}
}

void UEmployeeWindowWidget::OnRerollClicked()
{
	if (SelectedEmployeeID < 0 || !EmployeeManager)
	{
		return;
	}

	FEmployeeInstance* PreEmployee = GetSelectedEmployee();
	const ELootBoxRarity PreRarity = PreEmployee ? PreEmployee->PotentialAbility.CurrentRarity : ELootBoxRarity::Common;

	if (!EmployeeManager->RerollEmployeePotential(SelectedEmployeeID, SelectedCube))
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("EmployeeWindow", "RerollNoCard", "명함이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	// 리롤 연출 — 갱신(델리게이트)은 이미 끝난 뒤: 줄 3개 순차 재추첨 + 등급 배지 펀치 + 큐브 눌림 + 사운드
	UBorder* FxLines[3] = { PotLine0, PotLine1, PotLine2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (FxLines[i])
		{
			FxLines[i]->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			RerollLineTargetOpacity[i] = FxLines[i]->GetRenderOpacity();
		}
	}
	if (TierBadge)
	{
		TierBadge->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	UItemCardSlotWidget* CubeFx[3] = { CubeSlot0, CubeSlot1, CubeSlot2 };
	RerollCubeIndex = INDEX_NONE;
	for (int32 i = 0; i < 3; ++i)
	{
		if (CubeTypes[i] == SelectedCube && CubeFx[i])
		{
			RerollCubeIndex = i;
			CubeFx[i]->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			break;
		}
	}
	RerollFxElapsed = 0.f;

	if (USoundManagerSubsystem* SoundMgr = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
	{
		FEmployeeInstance* PostEmployee = GetSelectedEmployee();
		const ELootBoxRarity NewRarity = PostEmployee ? PostEmployee->PotentialAbility.CurrentRarity : PreRarity;
		bRerollUpgraded = NewRarity > PreRarity;
		if (NewRarity > PreRarity)
		{
			// 등급업 순간만 등급별 리빌 사운드 — 평시 리롤은 굴리기 소리 하나
			switch (NewRarity)
			{
			case ELootBoxRarity::Epic:      SoundMgr->PlayUISound(CGUISoundTags::GachaResultEpic); break;
			case ELootBoxRarity::Legendary:
			case ELootBoxRarity::Mythic:    SoundMgr->PlayUISound(CGUISoundTags::GachaResultLegendary); break;
			default:                        SoundMgr->PlayUISound(CGUISoundTags::GachaResultRare); break;
			}
		}
		else
		{
			SoundMgr->PlayUISound(CGUISoundTags::PotentialReroll);
		}
	}
}

void UEmployeeWindowWidget::OnCubeSlotClicked(FName ItemID, int32 CubeIndex)
{
	if (CubeIndex < 0 || CubeIndex >= 3)
	{
		return;
	}

	const EItemType CubeType = CubeTypes[CubeIndex];
	if (CubeType == SelectedCube)
	{
		return;
	}

	// 품절 큐브는 선택 불가 (딤 처리로 이미 시각 안내됨)
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	if (!ItemMgr || ItemMgr->GetItemCount(CubeType) <= 0)
	{
		return;
	}

	SelectedCube = CubeType;
	RefreshCubeSlots();
}

void UEmployeeWindowWidget::OnEnhanceClicked()
{
	if (SelectedEmployeeID < 0)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::EnhanceStarforceModal);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] EnhanceStarforceModal 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	// PromptStack 은 스위처식(top 만 표시)이라 push 하면 직원창이 사라짐 —
	// 모달은 뷰포트 오버레이로 직원창 위에 띄운다 (자체 딤이 아래 입력 차단, 닫힘 시 스스로 제거)
	// ⚠ ActivateWidget 금지: 뷰포트 root 활성화가 CommonUI 기본 입력컨피그(Menu)를 적용,
	//   닫힌 뒤에도 잔류해 게임 입력(카메라/월드탭)이 영구 차단됨 — 가챠 리빌 등 선례와 동일하게 비활성 오버레이로
	if (UEnhanceStarforceModalWidget* Modal = CreateWidget<UEnhanceStarforceModalWidget>(GetOwningPlayer(), Cls.Get()))
	{
		Modal->AddToViewport(120);
		Modal->Configure(SelectedEmployeeID);
		OpenEnhanceModal = Modal;
	}
}

void UEmployeeWindowWidget::OnFireClicked()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	if (!Employee)
	{
		return;
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr)
	{
		return;
	}

	// 대상을 여는 시점에 고정 — 모달이 떠 있는 동안 로스터가 갱신되면 선택이 옮겨간다
	const int32 TargetID = Employee->EmployeeID;

	// ShowConfirmDialog 는 AddToViewport(10000) 이라 직원창이 살아 있다 (PromptStack push 는 스위처식이라 창이 꺼짐)
	UIMgr->ShowConfirmDialog(
		NSLOCTEXT("EmployeeWindow", "FireTitle", "직원 해고"),
		FText::FromString(FString::Printf(TEXT("%s 직원을 해고합니다. 되돌릴 수 없습니다."), *Employee->EmployeeName)),
		FSimpleDelegate::CreateUObject(this, &UEmployeeWindowWidget::HandleFireConfirmed, TargetID));
}

void UEmployeeWindowWidget::HandleFireConfirmed(int32 EmployeeID)
{
	if (!EmployeeManager)
	{
		return;
	}

	// 로스터 재구성/재선택은 FireEmployee 의 OnEmployeeRosterChanged 브로드캐스트가 담당
	EmployeeManager->FireEmployee(EmployeeID);
}

void UEmployeeWindowWidget::OnOddsClicked()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	if (!Employee)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::PotentialOddsPanel);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] PotentialOddsPanel 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	// 연타 시 딤이 겹친 채 앞의 패널을 위크포인터가 놓친다 — 열려 있으면 걷고 새로 연다(대상 직원이 바뀌었을 수 있음)
	if (UPotentialOddsWidget* Prev = OpenOddsPanel.Get())
	{
		Prev->RemoveFromParent();
		OpenOddsPanel.Reset();
	}

	// 강화 모달과 동일 — 뷰포트 오버레이. ⚠ ActivateWidget 금지(입력컨피그 잔류로 게임 입력 영구 차단)
	// CreateWidget<T> 는 타입 불일치 시 assert 즉사 — Cast 로 완만히 실패시킨다 (2026-08-06 LC 크래시)
	if (UPotentialOddsWidget* Panel = Cast<UPotentialOddsWidget>(CreateWidget(GetOwningPlayer(), Cls.Get())))
	{
		Panel->AddToViewport(120);
		Panel->Setup(FText::FromString(Employee->EmployeeName), Employee->PotentialAbility.MaxAchievedRarity);
		OpenOddsPanel = Panel;
	}
}

void UEmployeeWindowWidget::OnCloseDelegate()
{
	CloseWithAnimation();
}

void UEmployeeWindowWidget::HandleEmployeeStatsChanged(int32 EmployeeID)
{
	if (EmployeeID != SelectedEmployeeID)
	{
		return;
	}

	// 투자/자동배분/리롤 — 종합·스탯·잠재 부분 갱신 (로스터 재정렬은 다음 오픈)
	RefreshSelected();
}

void UEmployeeWindowWidget::HandleEmployeeDisciplineChanged(int32 EmployeeID)
{
	if (EmployeeID != SelectedEmployeeID)
	{
		return;
	}

	// 직능 투자/리셋 — argmax 부서가 바뀔 수 있어 부서칩·종합 갱신. 카드 6행은 카드 자체 구독으로 갱신됨.
	RefreshHero();
}

void UEmployeeWindowWidget::HandleEmployeeLevelUp(int32 EmployeeID, int32 NewLevel)
{
	// 로스터 카드의 Lv 라벨 갱신 (표시 중인 엔트리만)
	if (RosterListView && EmployeeManager)
	{
		if (FEmployeeInstance* Employee = EmployeeManager->FindEmployee(EmployeeID))
		{
			for (UUserWidget* Entry : RosterListView->GetDisplayedEntryWidgets())
			{
				UEmployeeRosterCardWidget* Card = Cast<UEmployeeRosterCardWidget>(Entry);
				if (Card && Card->GetEmployeeID() == EmployeeID)
				{
					Card->SetEmployeeInfo(*Employee);
					break;
				}
			}
		}
	}

	if (EmployeeID == SelectedEmployeeID)
	{
		RefreshHero();
		RefreshHeroStats();
	}
}

void UEmployeeWindowWidget::HandleRosterChanged()
{
	RefreshRoster();
}

void UEmployeeWindowWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type != EResourceType::Money)
	{
		return;
	}

	RefreshWallet();
}

void UEmployeeWindowWidget::HandleExperienceGained(int32 EmployeeID, float Amount)
{
	if (EmployeeID != SelectedEmployeeID)
	{
		return;
	}

	RefreshExpOnly();
}
