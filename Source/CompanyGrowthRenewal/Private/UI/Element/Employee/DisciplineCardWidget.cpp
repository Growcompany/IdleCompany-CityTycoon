#include "UI/Element/Employee/DisciplineCardWidget.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Enum/ResourceType.h"
#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/ProductionDiscipline.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Employee/DisciplineCellButtonWidget.h"
#include "UI/Element/Employee/DisciplineRadarWidget.h"
#include "UI/UISoundTags.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UDisciplineCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	}

	// 인덱스 = EProductionDiscipline 0~5 순서로 모음 (채움/바인딩 루프용)
	DiscCells = { DiscCell0, DiscCell1, DiscCell2, DiscCell3, DiscCell4, DiscCell5 };

	// 인스턴스 수명 쌍(NativeConstruct↔NativeDestruct). payload 로 직능 인덱스 보존.
	// OnClicked 는 바인딩하지 않는다 — Pressed 가 이미 1회 투자하므로 이중 투자가 된다 (Factory 선례).
	for (int32 i = 0; i < DiscCells.Num(); ++i)
	{
		if (DiscCells[i])
		{
			DiscCells[i]->OnPressed().AddUObject(this, &UDisciplineCardWidget::HandleCellPressed, i);
			DiscCells[i]->OnReleased().AddUObject(this, &UDisciplineCardWidget::HandleCellReleased);
		}
	}

	if (ResetButton)
	{
		ResetButton->OnClicked().AddUObject(this, &UDisciplineCardWidget::HandleResetClicked);
	}
}

void UDisciplineCardWidget::NativeDestruct()
{
	StopInvestHold();

	for (UDisciplineCellButtonWidget* Cell : DiscCells)
	{
		if (Cell)
		{
			Cell->OnPressed().RemoveAll(this);
			Cell->OnReleased().RemoveAll(this);
		}
	}

	if (ResetButton)
	{
		ResetButton->OnClicked().RemoveAll(this);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeDisciplineChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

UWidget* UDisciplineCardWidget::GetFirstInvestButtonWidget() const
{
	// NativeConstruct 전엔 DiscCells 가 비어있으므로 IsValidIndex 가드 필수
	return DiscCells.IsValidIndex(0) ? Cast<UWidget>(DiscCells[0]) : nullptr;
}

void UDisciplineCardWidget::Configure(int32 InEmployeeID)
{
	// 홀드 중 직원이 바뀌면 타이머가 남아 새 직원 SP 를 계속 먹는다 (되돌리려면 다이아 50)
	if (InEmployeeID != CurrentEmployeeID)
	{
		StopInvestHold();
	}

	CurrentEmployeeID = InEmployeeID;

	// 재사용 위젯 — 매 오픈 재바인딩(중복구독 가드), 해제는 NativeDestruct
	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeDisciplineChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeDisciplineChanged.AddUObject(this, &UDisciplineCardWidget::HandleDisciplineChanged);
	}

	RefreshFromData();
}

void UDisciplineCardWidget::RefreshFromData()
{
	if (!EmployeeManager)
	{
		return;
	}

	FEmployeeInstance* Emp = EmployeeManager->GetEmployeeData(CurrentEmployeeID);
	if (!Emp)
	{
		return;
	}

	const int32 SP = Emp->AvailableSkillPoints;
	if (SkillPointText)
	{
		SkillPointText->SetText(FText::AsNumber(SP));
	}
	const bool bCanInvest = SP > 0;
	if (UWidget* SPGlow = GetWidgetFromName(TEXT("SPGlow")))
	{
		SPGlow->SetVisibility(bCanInvest ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);   // 강화 가능 = SP 칩 글로우
	}

	int32 TotalInvested = 0;
	const int32 N = static_cast<int32>(EProductionDiscipline::Count);
	TArray<int32> Totals;
	Totals.Reserve(N);
	TArray<FText> Names;
	Names.Reserve(N);

	// 직능 표시명 = 산업별 DT — 현재 진입 건물의 산업 기준
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
	const ECompanyType Industry = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;

	for (int32 i = 0; i < N; ++i)
	{
		const int32 Total    = Emp->DisciplinePoints.IsValidIndex(i)          ? Emp->DisciplinePoints[i]          : 0;
		const int32 Invested = Emp->InvestedDisciplinePoints.IsValidIndex(i)  ? Emp->InvestedDisciplinePoints[i]  : 0;
		TotalInvested += Invested;
		Totals.Add(Total);

		const FText Name = TableMgr ? TableMgr->GetDisciplineDisplayName(Industry, i) : FText::GetEmpty();
		Names.Add(Name);

		if (DiscCells.IsValidIndex(i) && DiscCells[i])
		{
			DiscCells[i]->SetDiscipline(Name, Total, Invested);
			DiscCells[i]->SetInvestEnabled(bCanInvest);
		}
	}

	// 레이더 정규화 = 절대 상한 (근거는 기존 주석/스펙 유지 — 값 변경 없음)
	const int32 MythicPrimary = FMath::RoundToInt(
		GetDisciplineTotalForRarity(ELootBoxRarity::Mythic) * 0.65f);   // MakeDisciplineProfile PrimaryShare
	const int32 MaxTotal = FMath::Max(
		GaugeDisplayMin,
		MythicPrimary + 29 * UEmployeeManager::SkillPointsPerLevel);    // 레벨캡 30 → 투자 가능 29
	if (RadarWidget)
	{
		RadarWidget->SetAxisLabels(Names);
		RadarWidget->SetRadarData(Totals, MaxTotal, FLootBoxRarityUtility::GetRarityColor(Emp->SpawnRarity));
	}

	if (ResetButton)
	{
		// 비용 표시 + 다이아 자동 afford 체크(부족 시 입력 삼킴+토스트는 버튼 내장)
		ResetButton->SetCost(UEmployeeManager::DisciplineResetDiamondCost, EResourceType::Diamond, /*bCheckAfford=*/true);
		// 되돌릴 투자분이 없으면 잠금 (비용 게이트와 분리된 축)
		ResetButton->SetLockedState(TotalInvested == 0);
	}
}

bool UDisciplineCardWidget::TryInvest(int32 DisciplineIndex)
{
	if (!EmployeeManager || DisciplineIndex < 0 || DisciplineIndex >= static_cast<int32>(EProductionDiscipline::Count))
	{
		return false;
	}
	// 성공 시 OnEmployeeDisciplineChanged 로 자동 갱신 — 별도 refresh 불필요
	if (!EmployeeManager->InvestDisciplinePoint(CurrentEmployeeID, static_cast<EProductionDiscipline>(DisciplineIndex)))
	{
		return false;
	}
	if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
	{
		SoundMgr->PlayUISound(CGUISoundTags::UpgradeSuccess);
	}
	return true;
}

void UDisciplineCardWidget::HandleCellPressed(int32 DisciplineIndex)
{
	if (!TryInvest(DisciplineIndex))
	{
		return;
	}
	CurrentHoldDiscipline = DisciplineIndex;
	if (UWorld* CardWorld = GetWorld())
	{
		CardWorld->GetTimerManager().SetTimer(
			InvestHoldTimerHandle,
			this,
			&UDisciplineCardWidget::OnInvestHoldTick,
			HoldRepeatInterval,
			true,             // bLoop
			HoldStartDelay);  // 첫 반복까지 대기 (싱글 탭과 구분)
	}
}

void UDisciplineCardWidget::HandleCellReleased()
{
	StopInvestHold();
}

void UDisciplineCardWidget::OnInvestHoldTick()
{
	// SP 소진 → SetInvestEnabled(false)가 셀을 비활성화하지만, 타이머는 여기서 직접 끊는다
	if (!TryInvest(CurrentHoldDiscipline))
	{
		StopInvestHold();
	}
}

void UDisciplineCardWidget::StopInvestHold()
{
	if (UWorld* CardWorld = GetWorld())
	{
		CardWorld->GetTimerManager().ClearTimer(InvestHoldTimerHandle);
	}
}

void UDisciplineCardWidget::HandleResetClicked()
{
	if (!EmployeeManager)
	{
		return;
	}
	// 버튼이 다이아 부족을 이미 삼키므로 여기 도달 = 원칙상 지불 가능.
	// 실패 = 창 열린 사이 다이아를 다른 데서 쓴 stale afford 레이스 — 폴백 토스트만 유지.
	if (!EmployeeManager->ResetDisciplinePoints(CurrentEmployeeID))
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			UIMgr->NotifyInsufficientResource(EResourceType::Diamond, UEmployeeManager::DisciplineResetDiamondCost);
		}
	}
}

void UDisciplineCardWidget::HandleDisciplineChanged(int32 ChangedEmployeeID)
{
	if (ChangedEmployeeID != CurrentEmployeeID)
	{
		return;
	}
	RefreshFromData();
}
