// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeCollectionPanelWidget.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonHierarchicalScrollBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "Core/CGGameInstance.h"
#include "UI/Element/Office/OfficeProjectCardWidget.h"
#include "Enum/WidgetType.h"
#include "Components/Spacer.h"
#include "Table/ProjectDataTable.h"
#include "Enum/CompanyType.h"
#include "Manager/SaveLoadManager.h"

void UOfficeCollectionPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGameInstance* GI = GetWorld()->GetGameInstance();
	TableManager = GI->GetSubsystem<UTableManagerSubsystem>();

	UE_LOG(LogTemp, Log, TEXT("[OfficeCollectionPanel] NativeConstruct"));
}

void UOfficeCollectionPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	StageFilterButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	StageFilterButtonGroup->SetSelectionRequired(true);

	if (Stage1to10Btn)   { StageFilterButtonGroup->AddWidget(Stage1to10Btn);   Stage1to10Btn->SetIsSelectable(true); }
	if (Stage11to20Btn)  { StageFilterButtonGroup->AddWidget(Stage11to20Btn);  Stage11to20Btn->SetIsSelectable(true); }
	if (Stage21to30Btn)  { StageFilterButtonGroup->AddWidget(Stage21to30Btn);  Stage21to30Btn->SetIsSelectable(true); }
	if (Stage31to40Btn)  { StageFilterButtonGroup->AddWidget(Stage31to40Btn);  Stage31to40Btn->SetIsSelectable(true); }
	if (Stage41to50Btn)  { StageFilterButtonGroup->AddWidget(Stage41to50Btn);  Stage41to50Btn->SetIsSelectable(true); }
	if (Stage51to60Btn)  { StageFilterButtonGroup->AddWidget(Stage51to60Btn);  Stage51to60Btn->SetIsSelectable(true); }
	if (Stage61to70Btn)  { StageFilterButtonGroup->AddWidget(Stage61to70Btn);  Stage61to70Btn->SetIsSelectable(true); }
	if (Stage71to80Btn)  { StageFilterButtonGroup->AddWidget(Stage71to80Btn);  Stage71to80Btn->SetIsSelectable(true); }
	if (Stage81to90Btn)  { StageFilterButtonGroup->AddWidget(Stage81to90Btn);  Stage81to90Btn->SetIsSelectable(true); }
	if (Stage91to100Btn) { StageFilterButtonGroup->AddWidget(Stage91to100Btn); Stage91to100Btn->SetIsSelectable(true); }

	StageFilterButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(
		this, &UOfficeCollectionPanelWidget::OnStageFilterSelectionChanged);

	if (StageFilterButtonGroup && Stage1to10Btn)
	{
		StageFilterButtonGroup->SelectButtonAtIndex(0);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UOfficeCollectionPanelWidget::OnBackgroundClicked);
	}

	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &UOfficeCollectionPanelWidget::OnBackButtonClicked);
	}

	RefreshProjectCards();

	UE_LOG(LogTemp, Log, TEXT("[OfficeCollectionPanel] NativeOnActivated"));
}

void UOfficeCollectionPanelWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UOfficeCollectionPanelWidget::OnBackgroundClicked);
	}

	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}

	if (StageFilterButtonGroup)
	{
		StageFilterButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	if (ProjectItemBox)
	{
		ProjectItemBox->ClearChildren();
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeCollectionPanel] NativeOnDeactivated"));
}

void UOfficeCollectionPanelWidget::NativeDestruct()
{
	Super::NativeDestruct();

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UOfficeCollectionPanelWidget::OnBackgroundClicked);
	}

	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}

	if (StageFilterButtonGroup)
	{
		StageFilterButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
}

void UOfficeCollectionPanelWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}

void UOfficeCollectionPanelWidget::OnBackButtonClicked()
{
	DeactivateWidget();
}

void UOfficeCollectionPanelWidget::OnStageFilterSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	CurrentStageFilter = ButtonIndex;
	RefreshProjectCards();
}

void UOfficeCollectionPanelWidget::RefreshProjectCards()
{
	if (!ProjectItemBox) return;
	ProjectItemBox->ClearChildren();

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	ECompanyType CurrentCompanyType = GI->GetCurrentBuildingCompanyType();

	if (!TableManager) return;

	TArray<FProjectData> AllProjects = TableManager->GetProjectsByCompanyType(CurrentCompanyType);

	int32 MinProjectIndex = (CurrentStageFilter * 10) + 1;
	int32 MaxProjectIndex = MinProjectIndex + 9;

	TSubclassOf<UUserWidget> ProjectCardClass = TableManager->GetWidgetClass(EWidgetType::OfficeProjectCard);
	if (!ProjectCardClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeCollectionPanel] OfficeProjectCard widget class not found"));
		return;
	}

	// 자체개발 성공 이력만 도감에 표시 (제조업은 기존처럼 전체 표시)
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	const TArray<int32>* SelfDevCleared = StageMgr ? &StageMgr->GetTierProgress().ClearedProjects : nullptr;
	const bool bFilterBySelfDev = IsProjectType(CurrentCompanyType) && SelfDevCleared != nullptr;

	for (const FProjectData& ProjectData : AllProjects)
	{
		if (ProjectData.ProjectIndex < MinProjectIndex || ProjectData.ProjectIndex > MaxProjectIndex)
		{
			continue;
		}
		if (bFilterBySelfDev && !SelfDevCleared->Contains(ProjectData.ProjectIndex))
		{
			continue;
		}

		UOfficeProjectCardWidget* Card = CreateWidget<UOfficeProjectCardWidget>(this, ProjectCardClass);
		if (!Card) continue;

		if (ProjectItemBox->GetChildrenCount() > 0)
		{
			USpacer* Spacer = NewObject<USpacer>(this);
			Spacer->SetSize(FVector2D(8.0f, 1.0f));
			ProjectItemBox->AddChildToHorizontalBox(Spacer);
		}

		ProjectItemBox->AddChildToHorizontalBox(Card);

		Card->SetProjectData(ProjectData.ProjectIndex);
		Card->UpdateStepProgress(
			0, ProjectData.RequiredScore_Step1,
			0, ProjectData.RequiredScore_Step2,
			0, ProjectData.RequiredScore_Step3,
			0, ProjectData.RequiredScore_Step4);

		Card->OnStartButtonClicked.AddDynamic(this, &UOfficeCollectionPanelWidget::OnProjectCardStartClicked);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeCollectionPanel] Filter %d, Range %d-%d"),
		CurrentStageFilter, MinProjectIndex, MaxProjectIndex);
}

void UOfficeCollectionPanelWidget::OnProjectCardStartClicked(int32 ProjectIndex)
{
	// 도감은 열람 전용 — 별도 동작 없이 닫기만
	UE_LOG(LogTemp, Log, TEXT("[OfficeCollectionPanel] Card clicked: %d (read-only)"), ProjectIndex);
	DeactivateWidget();
}
