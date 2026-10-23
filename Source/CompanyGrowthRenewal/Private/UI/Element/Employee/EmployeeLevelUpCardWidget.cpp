#include "UI/Element/Employee/EmployeeLevelUpCardWidget.h"

#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/ProductionDiscipline.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "CommonTextBlock.h"

void UEmployeeLevelUpCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
		if (const UTableManagerSubsystem* Tables = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			Tables->ApplyUIVFXTexture(TEXT("CTAShine"), LevelUpShine);
		}
	}
	if (LevelUpButton)
	{
		LevelUpButton->OnClicked().AddUObject(this, &ThisClass::HandleLevelUpClicked);
		if (!ReadyButtonStyle && LevelUpButton->GetStyle())
		{
			ReadyButtonStyle = LevelUpButton->GetStyle()->GetClass();
		}
	}
	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeExperienceChanged.AddUObject(this, &ThisClass::HandleEmployeeExperienceChanged);
		EmployeeManager->OnEmployeeLevelUp.AddUObject(this, &ThisClass::HandleEmployeeLevelUp);
		EmployeeManager->OnEmployeeDisciplineChanged.AddUObject(this, &ThisClass::HandleDisciplineChanged);
	}
	RefreshFromData();
}

void UEmployeeLevelUpCardWidget::NativeDestruct()
{
	if (LevelUpButton)
	{
		LevelUpButton->OnClicked().RemoveAll(this);
	}
	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeExperienceChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeLevelUp.RemoveAll(this);
		EmployeeManager->OnEmployeeDisciplineChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UEmployeeLevelUpCardWidget::Configure(int32 InEmployeeID)
{
	CurrentEmployeeID = InEmployeeID;
	RefreshFromData();
}

UWidget* UEmployeeLevelUpCardWidget::GetLevelUpButtonWidget() const
{
	// Blocked buttons stay clickable to explain why, so readiness decides the tutorial target.
	return LevelUpButton && bLevelUpReady ? LevelUpButton : nullptr;
}

UTexture2D* UEmployeeLevelUpCardWidget::GetDisciplineIconTexture(int32 DisciplineIndex) const
{
	const TSoftObjectPtr<UTexture2D> Icons[] =
	{
		DisciplineIcon0, DisciplineIcon1, DisciplineIcon2,
		DisciplineIcon3, DisciplineIcon4, DisciplineIcon5
	};
	return DisciplineIndex >= 0 && DisciplineIndex < UE_ARRAY_COUNT(Icons)
		? Icons[DisciplineIndex].LoadSynchronous() : nullptr;
}

void UEmployeeLevelUpCardWidget::RefreshFromData()
{
	const FEmployeeInstance* Employee = EmployeeManager ? EmployeeManager->FindEmployee(CurrentEmployeeID) : nullptr;
	const bool bAtMaxLevel = Employee && Employee->Level >= UEmployeeManager::MaxEmployeeLevel;
	const int32 Cost = Employee && !bAtMaxLevel ? EmployeeManager->GetLevelUpCost(CurrentEmployeeID) : 0;
	const bool bCanLevelUp = Employee && EmployeeManager->CanLevelUpEmployee(CurrentEmployeeID);
	const double ExperienceAmount = EmployeeManager && Employee ? EmployeeManager->GetExperience(CurrentEmployeeID) : 0.0;
	const int64 Experience = FMath::IsFinite(ExperienceAmount)
		? static_cast<int64>(FMath::Clamp(FMath::FloorToDouble(ExperienceAmount), 0.0, static_cast<double>(MAX_int64))) : 0;
	const bool bHasCost = Employee && !bAtMaxLevel && Cost > 0;
	const bool bExperienceCoversCost = bHasCost && Experience >= Cost;
	bLevelUpReady = bCanLevelUp;

	// "Lv." is a static WBP prefix so the number can be set much larger than the prefix.
	if (CurrentLevelText) CurrentLevelText->SetText(Employee ? FText::AsNumber(Employee->Level) : FText::GetEmpty());
	if (NextLevelText) NextLevelText->SetText(Employee && !bAtMaxLevel ? FText::AsNumber(static_cast<int64>(Employee->Level) + 1) : FText::GetEmpty());
	if (NextLevelGroup) NextLevelGroup->SetVisibility(Employee && !bAtMaxLevel ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	const int32 AvailableStatPoints = Employee ? EmployeeManager->GetAvailableStatPoints(CurrentEmployeeID) : 0;
	if (CurrentStatPointText) CurrentStatPointText->SetText(Employee ? UGlobalUtilFunctions::AbbreviateNumber(AvailableStatPoints) : FText::GetEmpty());
	if (NextStatPointText) NextStatPointText->SetText(Employee && !bAtMaxLevel
		? UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(AvailableStatPoints) + UEmployeeManager::StatPointsPerLevel) : FText::GetEmpty());
	if (SPChipGain) SPChipGain->SetText(FText::Format(NSLOCTEXT("EmployeeLevelUpCard", "StatPointGain", "+{0}"),
		FText::AsNumber(UEmployeeManager::StatPointsPerLevel)));

	if (ExperienceText)
	{
		ExperienceText->SetText(Employee ? UGlobalUtilFunctions::AbbreviateNumber(Experience) : FText::GetEmpty());
		if (bHasCost) ExperienceText->SetColorAndOpacity(bExperienceCoversCost ? ReadyBalanceColor : ShortBalanceColor);
	}
	if (LevelUpCostText) LevelUpCostText->SetText(bHasCost ? FText::AsNumber(Cost) : FText::GetEmpty());
	if (ExperienceBar)
	{
		ExperienceBar->SetPercent(bHasCost && FMath::IsFinite(ExperienceAmount)
			? static_cast<float>(FMath::Clamp(ExperienceAmount / Cost, 0.0, 1.0)) : (bAtMaxLevel ? 1.f : 0.f));
		ExperienceBar->SetFillColorAndOpacity(bExperienceCoversCost || bAtMaxLevel ? ReadyBarColor : ShortBarColor);
	}
	const int32 BestIndex = Employee ? GetPrimaryDisciplineSlot(*Employee) : INDEX_NONE;
	const bool bHasPrimaryDiscipline = Employee && BestIndex >= 0
		&& BestIndex < static_cast<int32>(EProductionDiscipline::Count)
		&& Employee->DisciplinePoints.IsValidIndex(BestIndex);

	if (LevelUpButton)
	{
		// Short experience is temporary: keep the button clickable so a tap explains it (UButtonWidget reject mode).
		const bool bWaitingForExperience = bHasPrimaryDiscipline && bHasCost && !bExperienceCoversCost;
		LevelUpButton->SetIsEnabled(bCanLevelUp || bWaitingForExperience);
		if (bWaitingForExperience)
		{
			LevelUpButton->SetRejectReason(NSLOCTEXT("EmployeeLevelUpCard", "NeedExperienceReason", "경험치가 부족합니다."));
		}
		else
		{
			LevelUpButton->ClearRejectReason();
		}
		const TSubclassOf<UCommonButtonStyle> TargetStyle = bCanLevelUp ? ReadyButtonStyle : BlockedButtonStyle;
		if (TargetStyle && (!LevelUpButton->GetStyle() || LevelUpButton->GetStyle()->GetClass() != TargetStyle))
		{
			LevelUpButton->SetStyle(TargetStyle);
		}
		LevelUpButton->SetButtonText(!Employee ? FText::GetEmpty()
			: bAtMaxLevel ? NSLOCTEXT("EmployeeLevelUpCard", "MaxLevel", "최대 레벨")
			: bWaitingForExperience ? NSLOCTEXT("EmployeeLevelUpCard", "NeedExperience", "경험치 부족")
			: NSLOCTEXT("EmployeeLevelUpCard", "LevelUp", "레벨업"));
		LevelUpButton->SetTextColor(bCanLevelUp ? ReadyLabelColor : BlockedLabelColor);
	}
	const int32 BestPower = bHasPrimaryDiscipline ? Employee->DisciplinePoints[BestIndex] : 0;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const ECompanyType Industry = EmployeeManager && Employee
		? EmployeeManager->GetCompanyTypeForBuilding(Employee->AssignedBuildingIndex) : ECompanyType::None;
	const FText DisciplineName = Tables && bHasPrimaryDiscipline
		? Tables->GetDisciplineDisplayName(Industry, BestIndex) : FText::GetEmpty();
	if (bHasPrimaryDiscipline && Tables && DisciplineName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeLevelUpCard] Missing discipline display name: industry=%d discipline=%d"),
			static_cast<int32>(Industry), BestIndex);
	}
	if (PrimaryDisciplineText) PrimaryDisciplineText->SetText(DisciplineName.IsEmpty()
		? FText::GetEmpty() : FText::Format(NSLOCTEXT("EmployeeLevelUpCard", "WorkPowerName", "{0} 업무력"), DisciplineName));
	if (CurrentWorkPowerText) CurrentWorkPowerText->SetText(bHasPrimaryDiscipline ? FText::AsNumber(BestPower) : FText::GetEmpty());
	if (NextWorkPowerText) NextWorkPowerText->SetText(bHasPrimaryDiscipline && !bAtMaxLevel
		? FText::AsNumber(static_cast<int64>(BestPower) + UEmployeeManager::WorkPowerPerLevel) : FText::GetEmpty());
	if (PrimaryDisciplineIcon)
	{
		PrimaryDisciplineIcon->SetBrushFromTexture(GetDisciplineIconTexture(
			GetDisciplineIconSlot(BestIndex, Employee ? Employee->DisciplinePoints.Num() : 0)));
		PrimaryDisciplineIcon->SetVisibility(bHasPrimaryDiscipline ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UEmployeeLevelUpCardWidget::HandleLevelUpClicked()
{
	if (EmployeeManager && EmployeeManager->LevelUpEmployee(CurrentEmployeeID))
	{
		RefreshFromData();
	}
}

void UEmployeeLevelUpCardWidget::HandleEmployeeExperienceChanged(int32 EmployeeID)
{
	if (EmployeeID == CurrentEmployeeID) RefreshFromData();
}

void UEmployeeLevelUpCardWidget::HandleEmployeeLevelUp(int32 EmployeeID, int32 NewLevel)
{
	if (EmployeeID == CurrentEmployeeID) RefreshFromData();
}

void UEmployeeLevelUpCardWidget::HandleDisciplineChanged(int32 EmployeeID)
{
	if (EmployeeID == CurrentEmployeeID) RefreshFromData();
}
