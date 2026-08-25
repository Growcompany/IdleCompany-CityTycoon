// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/OfficeProjectCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/IconCardWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "Components/ProgressBar.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ProjectDataTable.h"
#include "Components/WidgetSwitcher.h"
#include "Core/CGGameInstance.h"
#include "Enum/CompanyType.h"
#include "Enum/ProjectStepType.h"

void UOfficeProjectCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UI_ProjectImageCard의 Glow 효과 비활성화
	if (UI_ProjectImageCard)
	{
		UI_ProjectImageCard->SetEnableGlow(false);
	}

	if (StartButton)
	{
		StartButton->OnClicked().AddUObject(this, &UOfficeProjectCardWidget::OnStartBtnClicked);
	}

	if (StopOperationButton)
	{
		StopOperationButton->OnClicked().AddUObject(this, &UOfficeProjectCardWidget::OnStopOperationBtnClicked);
	}

}

void UOfficeProjectCardWidget::NativeDestruct()
{
	if (StartButton)
	{
		StartButton->OnClicked().RemoveAll(this);
	}

	if (StopOperationButton)
	{
		StopOperationButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UOfficeProjectCardWidget::SetProjectData(int32 ProjectIndex)
{
	CurrentProjectIndex = ProjectIndex;

	// TableManagerSubsystem에서 프로젝트 데이터 가져오기
	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeProjectCardWidget] GameInstance is null"));
		return;
	}

	UTableManagerSubsystem* TableManager = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeProjectCardWidget] TableManagerSubsystem is null"));
		return;
	}

	bool bSuccess = false;
	FProjectData ProjectData = TableManager->ResolveProjectData(ProjectIndex, bSuccess);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeProjectCardWidget] Failed to get project data for index %d"), ProjectIndex);
		return;
	}

	// 프로젝트 이미지 및 이름 설정 (UI_ProjectImageCard의 IconImage와 IconDisplayText)
	if (UI_ProjectImageCard)
	{
		UTexture2D* IconTexture = nullptr;
		if (!ProjectData.Icon.IsNull())
		{
			IconTexture = ProjectData.Icon.LoadSynchronous();
		}
		UI_ProjectImageCard->SetDisplayInfo(IconTexture, ProjectData.ProjectName);
	}
}

void UOfficeProjectCardWidget::UpdateStepProgress(int32 Step1Current, int32 Step1Max, int32 Step2Current, int32 Step2Max,
                                                  int32 Step3Current, int32 Step3Max, int32 Step4Current, int32 Step4Max)
{
	// 현재 빌딩 산업 + 해당 프로젝트의 VariantKey로 DT_StepDisplayName 조회 (실패 시 코드 기본값 자동 폴백)
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
	const ECompanyType CompanyType = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;

	FName VariantKey = NAME_None;
	if (TableMgr && CurrentProjectIndex > 0)
	{
		bool bOk = false;
		const FProjectData ProjData = TableMgr->ResolveProjectData(CurrentProjectIndex, bOk);
		if (bOk) VariantKey = ProjData.VariantKey;
	}

	auto StepName = [TableMgr, CompanyType, VariantKey](int32 StepNumber) -> FString
	{
		// DT 단일 진실. TableMgr 없거나 매핑 없으면 빈 문자열 (UI Collapsed)
		return TableMgr ? TableMgr->GetStepDisplayName(CompanyType, VariantKey, StepNumber).ToString() : FString();
	};

	// DT에서 StepName이 빈 문자열이면 해당 Step 슬롯 자체를 Collapsed 처리 (3/4 Step 가변)
	auto ApplyStepRow = [&StepName](UStatRowWidget* Row, int32 StepNumber, int32 Current, int32 Max)
	{
		if (!Row) return;
		const FString Name = StepName(StepNumber);
		if (Name.IsEmpty())
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		Row->SetVisibility(ESlateVisibility::Visible);
		Row->SetStatName(Name);
		Row->SetCurrentProgressAnimated(static_cast<float>(Current), static_cast<float>(Max));
	};

	ApplyStepRow(UIE_StatRow_1, 1, Step1Current, Step1Max);
	ApplyStepRow(UIE_StatRow_2, 2, Step2Current, Step2Max);
	ApplyStepRow(UIE_StatRow_3, 3, Step3Current, Step3Max);
	ApplyStepRow(UIE_StatRow_4, 4, Step4Current, Step4Max);
}

void UOfficeProjectCardWidget::UpdateStepProgressInstant(int32 Step1Current, int32 Step1Max, int32 Step2Current, int32 Step2Max,
                                                          int32 Step3Current, int32 Step3Max, int32 Step4Current, int32 Step4Max)
{
	if (UIE_StatRow_1) UIE_StatRow_1->SetCurrentProgress(static_cast<float>(Step1Current), static_cast<float>(Step1Max));
	if (UIE_StatRow_2) UIE_StatRow_2->SetCurrentProgress(static_cast<float>(Step2Current), static_cast<float>(Step2Max));
	if (UIE_StatRow_3) UIE_StatRow_3->SetCurrentProgress(static_cast<float>(Step3Current), static_cast<float>(Step3Max));
	if (UIE_StatRow_4) UIE_StatRow_4->SetCurrentProgress(static_cast<float>(Step4Current), static_cast<float>(Step4Max));
}

void UOfficeProjectCardWidget::OnStartBtnClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeProjectCardWidget] StartButton clicked - ProjectIndex: %d"), CurrentProjectIndex);
	OnStartButtonClicked.Broadcast(CurrentProjectIndex);
}

void UOfficeProjectCardWidget::OnStopOperationBtnClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeProjectCardWidget] StopOperationButton clicked"));
	OnStopOperationClicked.Broadcast();
}

void UOfficeProjectCardWidget::SwitchToOperationMode(float RevenuePerSec, int64 TotalRevenue, const FString& QualityGradeStr)
{
	if (CardSwitcher)
	{
		CardSwitcher->SetActiveWidgetIndex(1);
	}
	UpdateOperationInfo(RevenuePerSec, TotalRevenue, QualityGradeStr);
}

void UOfficeProjectCardWidget::SwitchToStageMode()
{
	if (CardSwitcher)
	{
		CardSwitcher->SetActiveWidgetIndex(0);
	}
}

void UOfficeProjectCardWidget::SetTitle(const FText& TitleText)
{
	if (TitleButton)
	{
		TitleButton->SetButtonText(TitleText);
	}
}

void UOfficeProjectCardWidget::UpdateOperationInfo(float RevenuePerSec, int64 TotalRevenue, const FString& QualityGradeStr)
{
	if (UIE_QualityGrade)
	{
		UIE_QualityGrade->SetStatInfo(TEXT("등급"), QualityGradeStr);
	}

	if (UIE_RevenuePerSec)
	{
		FNumberFormattingOptions NumberOpts;
		NumberOpts.MinimumFractionalDigits = 1;
		NumberOpts.MaximumFractionalDigits = 1;
		FString ValueStr = FText::AsNumber(RevenuePerSec, &NumberOpts).ToString() + TEXT("원");
		UIE_RevenuePerSec->SetStatInfo(TEXT("기대 수익/초"), ValueStr);
	}

	if (UIE_TotalRevenue)
	{
		FString ValueStr = FText::AsNumber(TotalRevenue).ToString() + TEXT("원");
		UIE_TotalRevenue->SetStatInfo(TEXT("총수익"), ValueStr);
	}
}

UWidget* UOfficeProjectCardWidget::GetStepWidget(int32 StepNumber) const
{
	UStatRowWidget* StatRow = nullptr;
	switch (StepNumber)
	{
	case 1: StatRow = UIE_StatRow_1; break;
	case 2: StatRow = UIE_StatRow_2; break;
	case 3: StatRow = UIE_StatRow_3; break;
	default: return nullptr;
	}

	// ProgressBar가 있으면 그것을 반환 (더 정확한 타겟)
	if (StatRow)
	{
		UProgressBar* Bar = StatRow->GetProgressBarWidget();
		if (Bar) return Bar;
	}
	return StatRow;
}
