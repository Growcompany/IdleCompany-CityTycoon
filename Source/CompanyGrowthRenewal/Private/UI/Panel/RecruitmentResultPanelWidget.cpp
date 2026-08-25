#include "UI/Panel/RecruitmentResultPanelWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Core/CGGameInstance.h"
#include "Manager/EmployeeManager.h"

void URecruitmentResultPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (OutButton)
	{
		OutButton->OnClicked().AddUObject(this, &URecruitmentResultPanelWidget::OnConfirmClicked);
	}
}

void URecruitmentResultPanelWidget::NativeDestruct()
{
	if (OutButton)
	{
		OutButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void URecruitmentResultPanelWidget::OnConfirmClicked()
{
	OnConfirmExitRequested.Broadcast();
}

void URecruitmentResultPanelWidget::InitResourceDisplay(int32 BuildingIndex)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI) return;

	UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmpMgr)
	{
		// 조용히 나가면 인원 칩이 빈 채로 남아 원인이 화면에 안 드러난다
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentResult] EmployeeManager 없음 — Building %d 인원 칩을 채우지 못했습니다"), BuildingIndex);
		return;
	}

	if (UIE_Resource_Employee)
	{
		// 분자는 고용 게이트와 같은 술어(로스터=벤치 포함) — 방금 뽑은 직원이 즉시 세어져야 문구와 안 갈라진다
		UIE_Resource_Employee->SetValueWithMax(
			EmpMgr->GetEmployeeCountInBuilding(BuildingIndex),
			EmpMgr->GetBuildingEmployeeCapacity(BuildingIndex, /*bLogIfZero*/ false));
	}
}
