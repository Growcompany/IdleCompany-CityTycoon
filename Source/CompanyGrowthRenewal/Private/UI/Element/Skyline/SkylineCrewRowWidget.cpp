#include "UI/Element/Skyline/SkylineCrewRowWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Enum/CompanyType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/EntityManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"

void USkylineCrewRowWidget::SetCrew(const FEmployeeInstance& Employee, int32 SlotIndex, float Power)
{
	if (NameText) NameText->SetText(FText::FromString(Employee.EmployeeName));

	if (PowerText)
	{
		PowerText->SetText(UGlobalUtilFunctions::AbbreviateNumber(
			FMath::RoundToInt64(Power), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	}

	const FString Stars = UEmployeeTypeHelper::MakeStarString(Employee.EnhancementLevel);
	if (StarText)
	{
		// 별 표기는 UEmployeeTypeHelper::MakeStarString 이 단일 진실이다 — 여기서 "★N" 으로 따로 조판하면
		// 같은 강화도가 화면마다 다르게 보인다(0성 = 빈 문자열 = 배지째 숨김도 그 계약이다)
		StarText->SetText(FText::FromString(Stars));
		StarText->SetVisibility(Stars.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (DisciplineAccent)
	{
		// 6직능 스트립 위치색 단일 출처. 1-based 라 슬롯에 +1
		DisciplineAccent->SetVisibility(SlotIndex == INDEX_NONE ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (SlotIndex != INDEX_NONE) DisciplineAccent->SetColorAndOpacity(UGlobalUtilFunctions::GetStepColor(SlotIndex + 1));
	}

	if (DisciplineText)
	{
		// 직능 표시명은 산업마다 다르게 재해석된다(DECISION §7.2) — 산업은 이 직원이 배치된 건물이 안다.
		// 출전 팀은 정의상 그 건물 배치 인원이라 따로 넘겨받지 않아도 같은 답이 나온다.
		ECompanyType Industry = ECompanyType::None;
		if (UWorld* WorldPtr = GetWorld())
		{
			if (UEntityManager* EntityMgr = WorldPtr->GetSubsystem<UEntityManager>())
			{
				if (ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(Employee.AssignedBuildingIndex))
				{
					Industry = Building->GetCompanyType();
				}
			}
		}

		FText DisciplineName;
		if (Industry != ECompanyType::None && SlotIndex != INDEX_NONE)
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
				{
					DisciplineName = TableMgr->GetDisciplineDisplayName(Industry, SlotIndex);
				}
			}
		}

		const bool bHasDiscipline = !DisciplineName.IsEmpty();
		DisciplineText->SetText(DisciplineName);
		DisciplineText->SetVisibility(bHasDiscipline ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (PortraitImage)
	{
		UTexture2D* Portrait = UEmployeeRosterCardWidget::LoadPortraitTexture(Employee.EmployeeID, PortraitDisplaySize);
		if (Portrait) PortraitImage->SetBrushFromTexture(Portrait, false);
		PortraitImage->SetVisibility(Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
