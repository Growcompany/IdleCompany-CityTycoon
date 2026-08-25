// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeRecruitmentPanelWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/Image.h"
#include "Manager/MissionManagerSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Panel/EmployeeGachaPresentationWidget.h"
#include "UI/Panel/EmployeeGachaProbabilityWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Utils/GachaBatchMath.h"
#include "Office/WorkstationActorBase.h"
#include "Enum/NotificationType.h"
#include "Enum/LootBoxRarity.h"
#include "Core/CGGameInstance.h"
#include "Manager/EmployeeManager.h"
#include "UI/UIBase.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"

void UOfficeRecruitmentPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 서브시스템 캐시
	if (UGameInstance* GI = GetGameInstance())
	{
		RecruitmentManager = GI->GetSubsystem<URecruitmentManagerSubsystem>();
		ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
		ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
		UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	}

	UGlobalUtilFunctions::InitProgressHead(PityHead);
	UGlobalUtilFunctions::InitProgressHead(MileageHead);

	// BindWidget된 리소스 위젯들을 TMap에 등록 (OfficeLayerWidget 패턴)
	if (UIE_Resource_Money_1)
	{
		ResourceWidgets.Add(EResourceType::Money, UIE_Resource_Money_1);
		if (ResourceMgr)
		{
			UIE_Resource_Money_1->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Money));
		}
	}

	if (UIE_Resource_Diamond_1)
	{
		ResourceWidgets.Add(EResourceType::Diamond, UIE_Resource_Diamond_1);
		if (ResourceMgr)
		{
			UIE_Resource_Diamond_1->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Diamond));
		}
	}

	if (UIE_Resource_Employee)
	{
		ResourceWidgets.Add(EResourceType::Employee, UIE_Resource_Employee);
		UpdateEmployeeChip();
	}

	// UIManagerSubsystem을 통한 리소스 변경 구독
	if (UIMgr)
	{
		UIResourceChangedHandle = UIMgr->OnUIResourceChanged.AddUObject(
			this, &UOfficeRecruitmentPanelWidget::HandleResourceChanged);
	}

	// 닫기 버튼 바인딩
	if (UIE_CloseButton && !UIE_CloseButton->OnCloseClicked.IsBound())
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UOfficeRecruitmentPanelWidget::OnCloseButtonClicked);
	}

	// 배너 그룹 셋업 (탭 배타 선택은 NativeConstruct에서만)
	BannerGroup = NewObject<UCommonButtonGroupBase>(this);
	BannerGroup->SetSelectionRequired(true);
	BannerGroup->AddWidget(BannerNormal);
	BannerGroup->AddWidget(BannerAdvanced);
	BannerGroup->AddWidget(BannerPremium);
	// OnButtonBaseClicked = 매 클릭 발동(배너가 selectable 아니어도). OnSelectedButtonBaseChanged 는
	// 선택이 실제로 바뀔 때만 발동 → 탭 버튼이 selectable 아니면 침묵해서 고급/프리미엄 전환 안 됨.
	BannerGroup->OnButtonBaseClicked.AddDynamic(this, &UOfficeRecruitmentPanelWidget::OnBannerSelected);
	BannerGroup->SelectButtonAtIndex(0);
	SelectedTier = EGachaTier::Normal;

	// CTA + 마일리지 교환 + 링크 버튼 바인딩
	if (PullButton)
	{
		PullButton->OnClicked().RemoveAll(this);
		PullButton->OnClicked().AddUObject(this, &UOfficeRecruitmentPanelWidget::OnPullClicked);
	}
	if (PullButtonMulti)
	{
		PullButtonMulti->OnClicked().RemoveAll(this);
		PullButtonMulti->OnClicked().AddUObject(this, &UOfficeRecruitmentPanelWidget::OnPullMultiClicked);
	}
	if (MileageExchangeButton)
	{
		MileageExchangeButton->OnClicked().RemoveAll(this);
		MileageExchangeButton->OnClicked().AddUObject(this, &UOfficeRecruitmentPanelWidget::OnMileageExchangeClicked);
	}
	if (ProbabilityInfoButton)
	{
		ProbabilityInfoButton->OnClicked().RemoveAll(this);
		ProbabilityInfoButton->OnClicked().AddUObject(this, &UOfficeRecruitmentPanelWidget::OnProbabilityInfoClicked);
	}
	if (MileageShopButton)
	{
		MileageShopButton->OnClicked().RemoveAll(this);
		MileageShopButton->OnClicked().AddUObject(this, &UOfficeRecruitmentPanelWidget::OnMileageShopClicked);
	}

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentPanel] NativeConstruct - RecruitmentMgr: %s, ItemMgr: %s"),
		RecruitmentManager ? TEXT("Valid") : TEXT("Null"),
		ItemMgr ? TEXT("Valid") : TEXT("Null"));
}

void UOfficeRecruitmentPanelWidget::NativeDestruct()
{
	// 닫기 버튼 해제
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}

	// 배너 그룹 해제
	if (BannerGroup)
	{
		BannerGroup->OnButtonBaseClicked.RemoveAll(this);
	}

	// CTA + 링크 버튼 해제
	if (PullButton) PullButton->OnClicked().RemoveAll(this);
	if (PullButtonMulti) PullButtonMulti->OnClicked().RemoveAll(this);
	if (MileageExchangeButton) MileageExchangeButton->OnClicked().RemoveAll(this);
	if (ProbabilityInfoButton) ProbabilityInfoButton->OnClicked().RemoveAll(this);
	if (MileageShopButton) MileageShopButton->OnClicked().RemoveAll(this);

	// UIManagerSubsystem 구독 해제
	if (UIMgr)
	{
		UIMgr->OnUIResourceChanged.Remove(UIResourceChangedHandle);
	}

	Super::NativeDestruct();
}

UWidget* UOfficeRecruitmentPanelWidget::GetPullButtonWidget() const
{
	return PullButton;
}

UWidget* UOfficeRecruitmentPanelWidget::GetPullButtonMultiWidget() const
{
	// PullButtonMulti 는 BindWidgetOptional — 미배선 패널에서 링이 사라지지 않도록 단발로 폴백
	return PullButtonMulti ? static_cast<UWidget*>(PullButtonMulti) : GetPullButtonWidget();
}

UWidget* UOfficeRecruitmentPanelWidget::GetCloseButtonWidget() const
{
	return UIE_CloseButton;
}

void UOfficeRecruitmentPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 매니저 델리게이트 구독
	if (RecruitmentManager)
	{
		OnMileageChangedHandle = RecruitmentManager->OnMileageChanged.AddUObject(
			this, &UOfficeRecruitmentPanelWidget::HandleMileageChanged);

		// 오버레이 재뽑기로 피티/마일리지/티켓이 바뀌면 게이지/CTA 갱신
		OnGachaPullCompletedHandle = RecruitmentManager->OnGachaPullCompleted.AddUObject(
			this, &UOfficeRecruitmentPanelWidget::HandleGachaPullCompleted);
	}
	if (ItemMgr)
	{
		OnItemChangedHandle = ItemMgr->OnItemChanged.AddUObject(
			this, &UOfficeRecruitmentPanelWidget::HandleItemChanged);
	}

	// 분자를 움직이는 유일한 축이 로스터다. 가챠는 이 패널을 덮는 오버레이라 고용 후 재활성이 없어,
	// 여기서 구독하지 않으면 칩이 고용 게이트와 갈라진 채로 남는다.
	if (UEmployeeManager* RosterEmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		RosterEmpMgr->OnEmployeeRosterChanged.AddUObject(this, &UOfficeRecruitmentPanelWidget::UpdateEmployeeChip);
		// 멀티 라벨의 ×N 은 남은 정원으로 클램프된다 — 고용 확정으로 정원이 줄면 라벨도 같이 내려가야 한다.
		RosterEmpMgr->OnEmployeeRosterChanged.AddUObject(this, &UOfficeRecruitmentPanelWidget::UpdateCTALabel);
	}

	RefreshAllDisplay();

	// M4 미션 가이드 — 채용 패널 열림 → [채용] 통과, [뽑기] 유도로 전환 + 패널 등록 (M3 ManagePanel 패턴 미러)
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyRecruitPanelOpened(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentPanel] NativeOnActivated - BuildingIndex: %d"), CurrentBuildingIndex);
}

void UOfficeRecruitmentPanelWidget::NativeOnDeactivated()
{
	// 매니저 델리게이트 해제
	if (RecruitmentManager)
	{
		RecruitmentManager->OnMileageChanged.Remove(OnMileageChangedHandle);
		RecruitmentManager->OnGachaPullCompleted.Remove(OnGachaPullCompletedHandle);
	}
	if (ItemMgr)
	{
		ItemMgr->OnItemChanged.Remove(OnItemChangedHandle);
	}

	if (UEmployeeManager* RosterEmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		RosterEmpMgr->OnEmployeeRosterChanged.RemoveAll(this);
	}

	// M4 가이드 — 채용 패널 닫힘 알림 (좌석 패널 복귀 = M4 완료 조건)
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyRecruitPanelClosed();
	}

	Super::NativeOnDeactivated();

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentPanel] NativeOnDeactivated"));
}

// ========== 리소스 변경 핸들러 ==========

void UOfficeRecruitmentPanelWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (UResourceWidget** FoundWidget = ResourceWidgets.Find(Type))
	{
		if (*FoundWidget)
		{
			(*FoundWidget)->SetValue(NewValue);
		}
	}

	// Diamond 변경 시 CTA 라벨 갱신 (다이아 폴백 표기)
	if (Type == EResourceType::Diamond)
	{
		UpdateCTALabel();
	}
}

// 칩 분자는 고용 게이트와 같은 술어(로스터=벤치 포함)여야 한다. 착석 수를 쓰면 신규 고용이
// 전원 벤치라 안 올라가서, 같은 패널에서 칩은 0/6 인데 버튼은 "인원이 가득 찼습니다" 가 된다.
int32 UOfficeRecruitmentPanelWidget::GetEmployeeChipNumerator() const
{
	UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	return EmpMgr ? EmpMgr->GetEmployeeCountInBuilding(CurrentBuildingIndex) : 0;
}

int32 UOfficeRecruitmentPanelWidget::GetEmployeeChipDenominator() const
{
	UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	// 표시 경로 — NativeConstruct 는 SetBuildingIndex 이전에 돌아 인덱스가 INDEX_NONE 일 수 있다. 로그를 끈다.
	return EmpMgr ? EmpMgr->GetBuildingEmployeeCapacity(CurrentBuildingIndex, /*bLogIfZero*/ false) : 0;
}

// 인원 칩 갱신의 유일 경로. 진입점 3곳(생성·로스터변경·전체갱신) 중 하나라도 빠지면
// 그 경로에서만 고용 게이트와 다른 수가 보인다.
void UOfficeRecruitmentPanelWidget::UpdateEmployeeChip()
{
	if (UResourceWidget** FoundWidget = ResourceWidgets.Find(EResourceType::Employee))
	{
		if (*FoundWidget)
		{
			(*FoundWidget)->SetValueWithMax(GetEmployeeChipNumerator(), GetEmployeeChipDenominator());
		}
	}
}

void UOfficeRecruitmentPanelWidget::SetPreferredWorkstation(AWorkstationActorBase* Workstation)
{
	PreferredWorkstationWeak = Workstation;
}

// ========== 건물 인덱스 설정 ==========

void UOfficeRecruitmentPanelWidget::SetBuildingIndex(int32 BuildingIndex)
{
	CurrentBuildingIndex = BuildingIndex;

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentPanel] SetBuildingIndex: %d"), BuildingIndex);

	if (CurrentBuildingIndex != INDEX_NONE)
	{
		RefreshAllDisplay();
	}
}

// ========== UI 갱신 함수 ==========

void UOfficeRecruitmentPanelWidget::RefreshAllDisplay()
{
	UpdateHeroAndProbability();
	UpdateGauges();
	UpdateCTALabel();

	UpdateEmployeeChip();
}

void UOfficeRecruitmentPanelWidget::UpdateHeroAndProbability()
{
	if (!RecruitmentManager) { return; }
	const int32 HR = RecruitmentManager->GetHRPower(CurrentBuildingIndex);

	if (HeroTitleText)
	{
		const FText Title = (SelectedTier == EGachaTier::Normal) ? FText::FromString(TEXT("일반 채용"))
			: (SelectedTier == EGachaTier::Advanced) ? FText::FromString(TEXT("고급 채용"))
			: FText::FromString(TEXT("프리미엄 채용"));
		HeroTitleText->SetText(Title);
	}

	if (HeroDescText)
	{
		const FText Desc = (SelectedTier == EGachaTier::Normal) ? FText::FromString(TEXT("HR 부서 직원을 강화할수록 좋은 등급이 나옵니다"))
			: (SelectedTier == EGachaTier::Advanced) ? FText::FromString(TEXT("레전드까지 나옵니다. 60회 안에 레전드가 확정됩니다"))
			: FText::FromString(TEXT("뽑을 때마다 마일리지가 쌓입니다. 200P로 레전드를 확정 교환합니다"));
		HeroDescText->SetText(Desc);
	}

	if (ProbabilityPillRow)
	{
		ProbabilityPillRow->ClearChildren();
		const TArray<FGachaRarityChance> Rows = RecruitmentManager->GetProbabilityTableForUI(SelectedTier, HR);
		for (const FGachaRarityChance& Row : Rows)
		{
			UTextBlock* Pill = NewObject<UTextBlock>(this);
			if (PillFont.FontObject) { Pill->SetFont(PillFont); }
			Pill->SetText(FText::FromString(FString::Printf(TEXT("%s %.0f%%"),
				*FLootBoxRarityUtility::GetKoreanName(Row.Rarity), Row.Percent)));
			Pill->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityColor(Row.Rarity)));
			ProbabilityPillRow->AddChildToHorizontalBox(Pill);
		}
	}
}

void UOfficeRecruitmentPanelWidget::UpdateGauges()
{
	if (!RecruitmentManager) { return; }

	if (SelectedTier == EGachaTier::Normal)
	{
		const int32 HR = RecruitmentManager->GetHRPower(CurrentBuildingIndex);

		// 경계 10/20/30/50 = GetProbabilityTable 의 Normal 분기 값 — 어긋나면 안내가 실제 개방 지점과 달라진다
		int32 SegmentBase = 0;
		int32 SegmentSize = 10;
		FString NextHint = TEXT("10에서 레어 개방");
		if (HR >= 50) { SegmentBase = 50; SegmentSize = 0; NextHint = TEXT("최고 구간"); }
		else if (HR >= 30) { SegmentBase = 30; SegmentSize = 20; NextHint = TEXT("50에서 확률 상승"); }
		else if (HR >= 20) { SegmentBase = 20; SegmentSize = 10; NextHint = TEXT("30에서 에픽 개방"); }
		else if (HR >= 10) { SegmentBase = 10; SegmentSize = 10; NextHint = TEXT("20에서 확률 상승"); }

		const float HRPct = (SegmentSize > 0)
			? FMath::Clamp((float)(HR - SegmentBase) / (float)SegmentSize, 0.f, 1.f)
			: 1.f;

		if (PityLabel) { PityLabel->SetText(FText::FromString(FString::Printf(TEXT("HR 파워 %d · %s"), HR, *NextHint))); }
		if (PityBar)
		{
			PityBar->SetPercent(HRPct);
			UGlobalUtilFunctions::UpdateProgressHead(PityBar, PityHead, HRPct);
		}
	}
	else
	{
		const int32 Pity = RecruitmentManager->GetPityCount(SelectedTier);
		const float PityPct = FMath::Clamp((float)Pity / (float)FGachaPityData::HardPity, 0.f, 1.f);
		if (PityLabel) { PityLabel->SetText(FText::FromString(FString::Printf(TEXT("천장 %d/%d"), Pity, FGachaPityData::HardPity))); }
		if (PityBar)
		{
			PityBar->SetPercent(PityPct);
			UGlobalUtilFunctions::UpdateProgressHead(PityBar, PityHead, PityPct);
		}
	}

	const int32 Mileage = RecruitmentManager->GetMileagePoints();
	const float MileagePct = FMath::Clamp((float)Mileage / (float)FGachaMileageData::ExchangeCost, 0.f, 1.f);
	if (MileageLabel) { MileageLabel->SetText(FText::FromString(FString::Printf(TEXT("마일리지 %d/%d"), Mileage, FGachaMileageData::ExchangeCost))); }
	if (MileageBar)
	{
		MileageBar->SetPercent(MileagePct);
		UGlobalUtilFunctions::UpdateProgressHead(MileageBar, MileageHead, MileagePct);
	}
	// 버튼은 항상 enabled(검정 유지) — 부족 게이팅은 OnMileageExchangeClicked 클릭 시점에서.
}

void UOfficeRecruitmentPanelWidget::UpdateCTALabel()
{
	if (!RecruitmentManager || !ItemMgr || !PullButton) { return; }

	// 티어별 전용 채용권 — 종류/티어는 좌측 아이콘(색=티어)이 보여주므로 텍스트엔 이름 안 박음.
	const EItemType Ticket = URecruitmentManagerSubsystem::GetTicketTypeForTier(SelectedTier);

	const int32 TicketCount = ItemMgr->GetItemCount(Ticket);
	FString Label;
	bool bEnabled = true;

	if (TicketCount > 0)
	{
		Label = TEXT("×1 채용");   // 비용(×1)을 앞에, 액션을 뒤에. 보유 개수는 상단 티켓 칩이 표시.
	}
	else if (SelectedTier == EGachaTier::Normal)
	{
		Label = TEXT("채용권 부족");
		bEnabled = false;   // 일반은 다이아 폴백 없음
	}
	else
	{
		const int32 Diamond = RecruitmentManager->GetDiamondCost(SelectedTier);
		Label = FString::Printf(TEXT("채용  ·  다이아 ×%d"), Diamond);
	}

	PullButton->SetButtonText(FText::FromString(Label));
	PullButton->SetIsEnabled(bEnabled);
	PullButton->SetCount(0);   // 별도 카운트 칸 숨김 — 비용은 ButtonText 한 줄로 통일(작은 글씨 겹침 제거)

	// 좌측 아이콘 = 해당 티어 전용 채용권(색=티어). DT_ShopItem→GetItemIcon, 없으면 Collapsed.
	if (TableMgr)
	{
		bool bIconFound = false;
		UTexture2D* TicketIcon = TableMgr->GetItemIcon(Ticket, bIconFound);
		PullButton->SetIcon(bIconFound ? TicketIcon : nullptr);
	}

	// 멀티 CTA — 갱신 트리거(열기·배너전환·아이템변경·뽑기완료·로스터변경)를 단발 라벨과 공유한다.
	if (PullButtonMulti)
	{
		EItemType MultiTicket = EItemType::RecruitTicketNormal;
		const int32 MultiCount = ComputeMultiPullCount(MultiTicket);
		FString MultiLabel;
		const bool bMultiEnabled = (MultiCount >= 2);

		if (MultiCount >= 2)
		{
			MultiLabel = FString::Printf(TEXT("×%d 한번에 채용"), MultiCount);
		}
		else if (ItemMgr->GetItemCount(MultiTicket) < 2)
		{
			MultiLabel = TEXT("채용권 부족");   // 티켓 원인 우선 — 정원이 같이 부족해도 먼저 살 것을 안내
		}
		else
		{
			MultiLabel = TEXT("정원 부족");
		}

		PullButtonMulti->SetButtonText(FText::FromString(MultiLabel));
		PullButtonMulti->SetIsEnabled(bMultiEnabled);
		PullButtonMulti->SetCount(0);

		if (TableMgr)
		{
			bool bMultiIconFound = false;
			UTexture2D* MultiIcon = TableMgr->GetItemIcon(MultiTicket, bMultiIconFound);
			PullButtonMulti->SetIcon(bMultiIconFound ? MultiIcon : nullptr);
		}
	}

	UpdateTicketCounts();
}

void UOfficeRecruitmentPanelWidget::UpdateTicketCounts()
{
	if (!ItemMgr) { return; }

	// 배너 BindWidget 타입은 UCommonButtonBase 계약 유지 — 실제 위젯만 캐스팅해서 수량을 넣는다
	auto SetBannerCount = [this](UCommonButtonBase* Banner, EItemType Ticket)
	{
		if (UGachaBannerButton* B = Cast<UGachaBannerButton>(Banner))
		{
			B->SetOwnedCount(ItemMgr->GetItemCount(Ticket));
		}
	};

	SetBannerCount(BannerNormal, EItemType::RecruitTicketNormal);
	SetBannerCount(BannerAdvanced, EItemType::RecruitTicketAdvanced);
	SetBannerCount(BannerPremium, EItemType::RecruitTicketPremium);
}

// ========== 델리게이트 핸들러 ==========

void UOfficeRecruitmentPanelWidget::HandleMileageChanged(int32 NewPoints)
{
	UpdateGauges();
}

void UOfficeRecruitmentPanelWidget::HandleItemChanged(EItemType ItemType, int32 NewCount, int32 Delta)
{
	// 채용권 타입 변경 시 CTA 라벨 갱신
	if (ItemType == EItemType::RecruitTicketNormal
		|| ItemType == EItemType::RecruitTicketAdvanced
		|| ItemType == EItemType::RecruitTicketPremium)
	{
		UpdateCTALabel();
	}
}

void UOfficeRecruitmentPanelWidget::HandleGachaPullCompleted(const FGachaResultData& Result)
{
	// 오버레이 재뽑기로 게이지가 변할 수 있어 패널을 다시 동기화
	UpdateGauges();
	UpdateCTALabel();
}

// ========== 버튼 이벤트 ==========

void UOfficeRecruitmentPanelWidget::OnCloseButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentPanel] Close button clicked"));

	// PushPromptClass로 열리므로 DeactivateWidget으로 닫기
	DeactivateWidget();
}

void UOfficeRecruitmentPanelWidget::OnBannerSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("[Recruit] OnBannerSelected idx=%d (0일반/1고급/2프리미엄) — 고급·프리미엄 클릭 시 이게 안 뜨면 WBP 배너 겹침/hit-test 문제"), ButtonIndex);
	switch (ButtonIndex)
	{
	case 0: SelectedTier = EGachaTier::Normal; break;
	case 1: SelectedTier = EGachaTier::Advanced; break;
	case 2: SelectedTier = EGachaTier::Premium; break;
	default: SelectedTier = EGachaTier::Normal; break;
	}
	// 클릭 핸들러(OnButtonBaseClicked) 경유라 선택 하이라이트는 수동 동기화. 우리 핸들러는
	// 선택 변경 델리게이트에 안 묶여 있어 재진입 없음.
	if (BannerGroup) { BannerGroup->SelectButtonAtIndex(ButtonIndex); }
	UpdateHeroAndProbability();
	UpdateGauges();
	UpdateCTALabel();
}

bool UOfficeRecruitmentPanelWidget::NotifyIfBuildingFull()
{
	// 게이트는 fail-closed — 매니저를 못 얻으면 정원을 모른다는 뜻이므로 통과시키지 않는다
	UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	if (EmpMgr && EmpMgr->CanHireIntoBuilding(CurrentBuildingIndex)) { return false; }

	if (!EmpMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[RecruitPanel] EmployeeManager 없음 — 정원 확인 불가로 채용을 차단합니다"));
	}

	if (UIMgr)
	{
		UIMgr->ShowNotification(UEmployeeManager::GetCapacityFullMessage(), 3.0f, ENotificationType::Failed);
	}
	else
	{
		// 안내를 못 띄우면 플레이어에겐 버튼이 죽은 것처럼 보인다 — 최소한 로그로 남긴다
		UE_LOG(LogTemp, Warning, TEXT("[RecruitPanel] 정원 초과로 채용 차단 (UIManager 없음 — 토스트 미표시), Building=%d"), CurrentBuildingIndex);
	}
	return true;
}

void UOfficeRecruitmentPanelWidget::OnPullClicked()
{
	if (!RecruitmentManager) { return; }
	if (NotifyIfBuildingFull()) { return; }
	if (!RecruitmentManager->CanExecuteGachaPull(SelectedTier))
	{
		if (UIMgr) { UIMgr->ShowNotification(NSLOCTEXT("Recruitment", "NoCost", "재화가 부족합니다"), 3.0f, ENotificationType::Failed); }
		return;
	}
	FGachaResultData Result;
	if (RecruitmentManager->ExecuteGachaPull(SelectedTier, CurrentBuildingIndex, Result))
	{
		if (!TableMgr) { return; }
		TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::EmployeeGachaPresentation);
		if (!Cls) { return; }
		// 결과 연출도 패널 '위에' 떠야 함(확률 정보와 동일) — 같은 스택 push면 패널이 deactivate→가려짐. 뷰포트 오버레이로.
		APlayerController* PC = GetOwningPlayer();
		if (!PC) { return; }
		if (UEmployeeGachaPresentationWidget* Pres = CreateWidget<UEmployeeGachaPresentationWidget>(PC, Cls.Get()))
		{
			Pres->AddToViewport(1000);
			Pres->SetPreferredWorkstation(PreferredWorkstationWeak.Get());
			Pres->SetupPull(Result, SelectedTier, CurrentBuildingIndex);
		}
	}
}

int32 UOfficeRecruitmentPanelWidget::ComputeMultiPullCount(EItemType& OutTicket) const
{
	OutTicket = URecruitmentManagerSubsystem::GetTicketTypeForTier(SelectedTier);
	if (!ItemMgr || !RecruitmentManager) { return 0; }

	return GachaBatchMath::ComputeBatchPullCount(
		ItemMgr->GetItemCount(OutTicket), RecruitmentManager->GetFreeCapacity(CurrentBuildingIndex));
}

void UOfficeRecruitmentPanelWidget::OnPullMultiClicked()
{
	if (!RecruitmentManager) { return; }
	if (NotifyIfBuildingFull()) { return; }

	EItemType Ticket = EItemType::RecruitTicketNormal;
	const int32 PullCount = ComputeMultiPullCount(Ticket);
	if (PullCount < 2)
	{
		UpdateCTALabel();   // 라벨 표시와 클릭 사이에 티켓/정원이 변한 경우 — 조용히 라벨만 재동기
		return;
	}

	TArray<FGachaResultData> Results;
	if (RecruitmentManager->ExecuteGachaPullBatch(SelectedTier, PullCount, CurrentBuildingIndex, Results) && TableMgr)
	{
		TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::EmployeeGachaPresentation);
		APlayerController* PC = GetOwningPlayer();
		if (!Cls || !PC) { return; }
		// 단발과 동일 — 결과 연출은 패널 위 뷰포트 오버레이로
		if (UEmployeeGachaPresentationWidget* Pres = CreateWidget<UEmployeeGachaPresentationWidget>(PC, Cls.Get()))
		{
			Pres->AddToViewport(1000);
			Pres->SetPreferredWorkstation(PreferredWorkstationWeak.Get());
			Pres->SetupPullBatch(Results, SelectedTier, CurrentBuildingIndex);
		}
	}
}

void UOfficeRecruitmentPanelWidget::OnMileageExchangeClicked()
{
	if (!RecruitmentManager) { return; }
	if (NotifyIfBuildingFull()) { return; }

	// 버튼을 항상 활성(검정)으로 두는 대신 클릭 시점에 게이팅 — 부족하면 알림만.
	if (RecruitmentManager->GetMileagePoints() < FGachaMileageData::ExchangeCost)
	{
		if (UIMgr) { UIMgr->ShowNotification(NSLOCTEXT("Recruitment", "NoMileage", "마일리지가 부족합니다"), 3.0f, ENotificationType::Failed); }
		return;
	}

	FGachaResultData Result;
	if (RecruitmentManager->ExchangeMileage(CurrentBuildingIndex, Result))
	{
		if (!TableMgr) { return; }
		TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::EmployeeGachaPresentation);
		if (!Cls) { return; }
		// 마일리지 교환 결과도 패널 위 뷰포트 오버레이로(확률 정보와 동일).
		APlayerController* PC = GetOwningPlayer();
		if (!PC) { return; }
		if (UEmployeeGachaPresentationWidget* Pres = CreateWidget<UEmployeeGachaPresentationWidget>(PC, Cls.Get()))
		{
			Pres->AddToViewport(1000);
			Pres->SetPreferredWorkstation(PreferredWorkstationWeak.Get());
			Pres->SetupPull(Result, EGachaTier::Premium, CurrentBuildingIndex);
		}
	}
}

void UOfficeRecruitmentPanelWidget::OnProbabilityInfoClicked()
{
	if (!TableMgr) { return; }
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::EmployeeGachaProbabilityPanel);
	if (!Cls) { return; }

	// PushPromptClass(PromptStack)면 같은 스택의 채용 패널이 deactivate→가려짐.
	// 확률 정보는 패널 '위에' 떠야 하므로 뷰포트 오버레이로(UIBase 전체 위). 닫기=RemoveFromParent.
	APlayerController* PC = GetOwningPlayer();
	if (!PC) { return; }
	if (UEmployeeGachaProbabilityWidget* Prob = CreateWidget<UEmployeeGachaProbabilityWidget>(PC, Cls.Get()))
	{
		Prob->AddToViewport(1000);
		Prob->SetupTable(RecruitmentManager ? RecruitmentManager->GetHRPower(CurrentBuildingIndex) : 0);
	}
}

void UOfficeRecruitmentPanelWidget::OnMileageShopClicked()
{
	// 상점 패널 푸시 + 마일리지 탭 사전 선택은 상점 세션 API 착지 후 연결
	// TODO 상점 SetInitialTab
	if (!TableMgr || !UIMgr) { return; }
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::ShopPanel);
	if (Cls) { UIMgr->GetUIBase()->PushPromptClass(Cls.Get()); }
}
