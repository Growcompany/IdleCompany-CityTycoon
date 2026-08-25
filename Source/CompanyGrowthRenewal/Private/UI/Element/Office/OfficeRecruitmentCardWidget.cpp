// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/OfficeRecruitmentCardWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "CommonButtonBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Core/CGGameInstance.h"
#include "Enum/WidgetType.h"
#include "Table/UIIconData.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "Components/Border.h"

void UOfficeRecruitmentCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (DetailBtn)
		DetailBtn->OnClicked().AddUObject(this, &UOfficeRecruitmentCardWidget::OnDetailBtnClicked);
	if (SummaryBtn)
		SummaryBtn->OnClicked().AddUObject(this, &UOfficeRecruitmentCardWidget::OnSummaryBtnClicked);

	// 현재 선택 상태에 따라 테두리 설정
	if (SelectionBorder)
	{
		if (GetSelected())
			SelectionBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		else
			SelectionBorder->SetVisibility(ESlateVisibility::Collapsed);
	}

	SwitchView(0);
}

void UOfficeRecruitmentCardWidget::NativeDestruct()
{
	if (DetailBtn)
		DetailBtn->OnClicked().RemoveAll(this);
	if (SummaryBtn)
		SummaryBtn->OnClicked().RemoveAll(this);

	ClearStatRows();
	Super::NativeDestruct();
}

void UOfficeRecruitmentCardWidget::OnButtonClicked()
{
	Super::OnButtonClicked();
}

void UOfficeRecruitmentCardWidget::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UOfficeRecruitmentCardWidget::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOfficeRecruitmentCardWidget::OnDetailBtnClicked()
{
	SwitchView(1);
}

void UOfficeRecruitmentCardWidget::OnSummaryBtnClicked()
{
	SwitchView(0);
}

void UOfficeRecruitmentCardWidget::SwitchView(int32 ViewIndex)
{
	if (StatViewSwitcher)
		StatViewSwitcher->SetActiveWidgetIndex(ViewIndex);
}

void UOfficeRecruitmentCardWidget::CreateStatRows()
{
	ClearStatRows();

	// 기존 자식 위젯 모두 제거 (블루프린트에서 배치된 것 포함)
	if (MainStatInfoBox)
	{
		MainStatInfoBox->ClearChildren();
	}
	if (DetailStatInfoBox)
	{
		DetailStatInfoBox->ClearChildren();
	}

	// TableMgr에서 StatRow 위젯 클래스 가져오기
	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentCardWidget] CreateStatRows - TableMgr is null"));
		return;
	}

	TSubclassOf<UUserWidget> StatRowClass = TableMgr->GetWidgetClass(EWidgetType::StatRow);
	if (!StatRowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentCardWidget] CreateStatRows - StatRowClass is null"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitmentCardWidget] CreateStatRows - StatRowClass: %s, MainStatInfoBox: %s, DetailStatInfoBox: %s"),
		*StatRowClass->GetName(),
		MainStatInfoBox ? TEXT("Valid") : TEXT("NULL"),
		DetailStatInfoBox ? TEXT("Valid") : TEXT("NULL"));

	for (int32 i = 0; i < 2; i++)
	{
		UStatRowWidget* Row = CreateWidget<UStatRowWidget>(this, StatRowClass);
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentCardWidget] MainStatRow %d - Row: %s"), i, Row ? TEXT("Created") : TEXT("NULL"));
		if (Row && MainStatInfoBox)
		{
			MainStatInfoBox->AddChildToVerticalBox(Row);
			MainStatRows.Add(Row);
		}
	}

	for (int32 i = 0; i < 6; i++)
	{
		UStatRowWidget* Row = CreateWidget<UStatRowWidget>(this, StatRowClass);
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentCardWidget] DetailStatRow %d - Row: %s"), i, Row ? TEXT("Created") : TEXT("NULL"));
		if (Row && DetailStatInfoBox)
		{
			DetailStatInfoBox->AddChildToVerticalBox(Row);
			DetailStatRows.Add(Row);
		}
	}
}

void UOfficeRecruitmentCardWidget::ClearStatRows()
{
	for (UStatRowWidget* Row : MainStatRows)
	{
		if (Row)
			Row->RemoveFromParent();
	}
	MainStatRows.Empty();

	for (UStatRowWidget* Row : DetailStatRows)
	{
		if (Row)
			Row->RemoveFromParent();
	}
	DetailStatRows.Empty();
}

void UOfficeRecruitmentCardWidget::SetEmployeeData(const FEmployeeInstance& InEmployee)
{
	EmployeeData = InEmployee;
	CurrentDepartment = InEmployee.Department;

	UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentCardWidget] SetEmployeeData - ID: %d, Name: %s, Dept: %d, Gender: %s"),
		InEmployee.EmployeeID, *InEmployee.EmployeeName, static_cast<int32>(CurrentDepartment),
		InEmployee.Gender == EEmployeeGender::Male ? TEXT("Male") : TEXT("Female"));

	// 이름 설정
	if (EmployeeNameText)
		EmployeeNameText->SetText(FText::FromString(InEmployee.EmployeeName));

	// 부서명 설정 — 산업별 표시명 DT 경유 (매핑 없으면 게터가 게임 기본값으로 폴백)
	if (DepartmentNameText)
	{
		UCGGameInstance* CGGI = Cast<UCGGameInstance>(GetGameInstance());
		UTableManagerSubsystem* DeptTableMgr = CGGI ? CGGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		DepartmentNameText->SetText(DeptTableMgr
			? DeptTableMgr->GetDepartmentDisplayName(CGGI->GetCurrentBuildingCompanyType(), CurrentDepartment)
			: FText::FromString(DepartmentToString(CurrentDepartment)));
	}

	// 직급 설정
	if (RankText)
	{
		UEmployeeManager* EmpMgr = GetWorld()->GetGameInstance()->GetSubsystem<UEmployeeManager>();
		if (EmpMgr)
		{
			EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(InEmployee.EnhancementLevel);
			RankText->SetText(FText::FromString(EmpMgr->GetRankDisplayName(Rank)));
		}
	}

	// Portrait 로드 및 설정
	if (EntityImage)
	{
		LoadPortraitImage(InEmployee.EmployeeID);
	}

	// 부서 이미지 로드
	LoadDepartmentImage(CurrentDepartment);

	CreateStatRows();
	UpdateStatDisplay();

	UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentCardWidget] SetEmployeeData complete - MainStatRows: %d, DetailStatRows: %d"),
		MainStatRows.Num(), DetailStatRows.Num());
}

void UOfficeRecruitmentCardWidget::UpdateStatDisplay()
{
	const FEmployeeStats& Stats = EmployeeData.Stats;

	UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitmentCardWidget] UpdateStatDisplay - Speed: %d, Crit: %d, Composure: %d, Exp: %d, Stam: %d, Focus: %d"),
		Stats.WorkSpeed, Stats.CritChance, Stats.Composure, Stats.ExpGain, Stats.Stamina, Stats.Focus);

	TArray<int32> StatValues = {
		Stats.WorkSpeed, Stats.CritChance, Stats.Composure, Stats.ExpGain, Stats.Stamina, Stats.Focus
	};

	// 부서-스탯 매핑 폐기(Task 2) — 실제 최고값 스탯 2개를 하이라이트로 대체
	int32 PrimaryIdx = 0, SecondaryIdx = 1;
	{
		TArray<int32> SortedIdx;
		for (int32 i = 0; i < StatValues.Num(); ++i) { SortedIdx.Add(i); }
		SortedIdx.Sort([&StatValues](int32 A, int32 B) { return StatValues[A] > StatValues[B]; });
		if (SortedIdx.Num() >= 2) { PrimaryIdx = SortedIdx[0]; SecondaryIdx = SortedIdx[1]; }
	}

	if (MainStatRows.Num() >= 2)
	{
		if (MainStatRows[0])
			MainStatRows[0]->SetStatInfo(FString::Printf(TEXT("★ %s"), *UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(PrimaryIdx)).ToString()), FString::FromInt(StatValues[PrimaryIdx]));
		if (MainStatRows[1])
			MainStatRows[1]->SetStatInfo(FString::Printf(TEXT("☆ %s"), *UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(SecondaryIdx)).ToString()), FString::FromInt(StatValues[SecondaryIdx]));
	}

	for (int32 i = 0; i < DetailStatRows.Num() && i < StatValues.Num(); i++)
	{
		if (!DetailStatRows[i])
			continue;

		FString Prefix = TEXT("");
		if (i == PrimaryIdx)
			Prefix = TEXT("★ ");
		else if (i == SecondaryIdx)
			Prefix = TEXT("☆ ");

		DetailStatRows[i]->SetStatInfo(Prefix + UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(i)).ToString(), FString::FromInt(StatValues[i]));
	}
}

void UOfficeRecruitmentCardWidget::LoadPortraitImage(int32 EmployeeID)
{
	FString BasePath = FPaths::ProjectSavedDir();
	FString FilePath = BasePath / TEXT("Portraits") / (FString::FromInt(EmployeeID) + TEXT(".png"));

	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentCardWidget] Portrait not found: %s"), *FilePath);
		return;
	}

	TArray<uint8> RawFileData;
	if (!FFileHelper::LoadFileToArray(RawFileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeRecruitmentCardWidget] Failed to load portrait: %s"), *FilePath);
		return;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeRecruitmentCardWidget] Failed to decompress PNG: %s"), *FilePath);
		return;
	}

	TArray<uint8> UncompressedBGRA;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeRecruitmentCardWidget] Failed to get raw image data: %s"), *FilePath);
		return;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(
		ImageWrapper->GetWidth(),
		ImageWrapper->GetHeight(),
		PF_B8G8R8A8
	);

	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeRecruitmentCardWidget] Failed to create texture"));
		return;
	}

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	EntityImage->SetBrushFromTexture(Texture);
	UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitmentCardWidget] Portrait loaded: %s"), *FilePath);
}

void UOfficeRecruitmentCardWidget::LoadDepartmentImage(EEmployeeDepartment Department)
{
	if (!DepartmentImage)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	if (Department == EEmployeeDepartment::None)
	{
		DepartmentImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FString DepartmentName;
	switch (Department)
	{
	case EEmployeeDepartment::Development: DepartmentName = TEXT("Development"); break;
	case EEmployeeDepartment::Design:      DepartmentName = TEXT("Design"); break;
	case EEmployeeDepartment::Sales:       DepartmentName = TEXT("Sales"); break;
	case EEmployeeDepartment::HR:          DepartmentName = TEXT("HR"); break;
	case EEmployeeDepartment::Management:  DepartmentName = TEXT("Management"); break;
	default:
		DepartmentImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FName RowName = FName(*FString::Printf(TEXT("Department_%s"), *DepartmentName));
	bool bSuccess = false;
	FUIIconData IconData = TableMgr->GetUIIconData(RowName, bSuccess);

	if (bSuccess)
	{
		UTexture2D* Texture = IconData.Icon.LoadSynchronous();
		if (Texture)
		{
			DepartmentImage->SetBrushFromTexture(Texture);
			DepartmentImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			DepartmentImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		DepartmentImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}
