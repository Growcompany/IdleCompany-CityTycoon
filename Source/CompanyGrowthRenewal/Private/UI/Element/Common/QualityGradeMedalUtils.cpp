#include "UI/Element/Common/QualityGradeMedalUtils.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Table/QualityGradeDisplayTable.h"

void CGQualityGradeMedal::Apply(UGameInstance* GI, ECompanyType CompanyType, EQualityGrade Grade,
	UBorder* LetterPlate, UCommonTextBlock* Text_QualityGrade)
{
	// 재사용된 위젯에서 이전 등급 재질을 먼저 제거한다. 조회 실패 시 빈 표시가 남는다.
	if (LetterPlate)
	{
		LetterPlate->SetVisibility(ESlateVisibility::Collapsed);
		LetterPlate->SetBrushFromMaterial(nullptr);
	}
	if (Text_QualityGrade)
	{
		Text_QualityGrade->SetVisibility(ESlateVisibility::Collapsed);
		Text_QualityGrade->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo ResetFont = Text_QualityGrade->GetFont();
		ResetFont.FontMaterial = nullptr;
		ResetFont.OutlineSettings.OutlineColor = FLinearColor::Transparent;
		ResetFont.OutlineSettings.OutlineSize = 0;
		Text_QualityGrade->SetFont(ResetFont);
	}

	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FQualityGradeDisplayRow DisplayData;
	if (!TableMgr || !TableMgr->GetQualityGradeDisplayData(CompanyType, Grade, DisplayData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[QualityGradeMedal] 품질 등급 표시 데이터 조회 실패: Company=%d, Grade=%d"),
			static_cast<int32>(CompanyType), static_cast<int32>(Grade));
		return;
	}

	UMaterialInterface* MedalMaterial = DisplayData.LaunchMedalMaterial.IsNull()
		? nullptr : DisplayData.LaunchMedalMaterial.LoadSynchronous();
	if (!MedalMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QualityGradeMedal] 품질 등급 메달 재질 로드 실패: Company=%d, Grade=%d, Path=%s"),
			static_cast<int32>(CompanyType), static_cast<int32>(Grade), *DisplayData.LaunchMedalMaterial.ToSoftObjectPath().ToString());
		return;
	}

	UMaterialInterface* LetterMaterial = DisplayData.LaunchLetterMaterial.IsNull()
		? nullptr : DisplayData.LaunchLetterMaterial.LoadSynchronous();
	if (!LetterMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QualityGradeMedal] 품질 등급 글자 재질 로드 실패: Company=%d, Grade=%d, Path=%s"),
			static_cast<int32>(CompanyType), static_cast<int32>(Grade), *DisplayData.LaunchLetterMaterial.ToSoftObjectPath().ToString());
		return;
	}

	if (LetterPlate)
	{
		LetterPlate->SetBrushFromMaterial(MedalMaterial);
		LetterPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Text_QualityGrade)
	{
		FSlateFontInfo GradeFont = Text_QualityGrade->GetFont();
		GradeFont.FontMaterial = LetterMaterial;
		Text_QualityGrade->SetFont(GradeFont);
		Text_QualityGrade->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
