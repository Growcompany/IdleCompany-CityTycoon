#include "UI/Element/Trade/ProjectInfoBarWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"

UProjectInfoBarWidget::UProjectInfoBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProjectInfoBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UProjectInfoBarWidget::SetDevelopmentStatus(int32 ProjectNumber, const FText& ProjectName, EProjectMode Mode)
{
	if (!Text_StageInfo) return;

	// DT_ProjectModeDisplay 단일 진실. None / 미매핑은 ProjectNumber prefix 폴백
	FString Prefix = FString::Printf(TEXT("프로젝트%d"), ProjectNumber);
	FString Suffix = TEXT("개발 중");
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			TableMgr->GetProjectModeDisplay(Mode, Prefix, Suffix);
		}
	}

	FString Formatted = FString::Printf(TEXT("%s - %s %s"),
		*Prefix, *ProjectName.ToString(), *Suffix);
	Text_StageInfo->SetText(FText::FromString(Formatted));
}

void UProjectInfoBarWidget::SetIdleStatus()
{
	if (Text_StageInfo)
	{
		Text_StageInfo->SetText(FText::FromString(TEXT("프로젝트를 선택해주세요")));
	}
}

void UProjectInfoBarWidget::SetOperationStatus(int32 ProjectNumber, const FText& ProjectName, const FString& QualityGradeStr)
{
	if (Text_StageInfo)
	{
		FString Formatted = FString::Printf(TEXT("프로젝트%d - %s [%s] 운영 중"),
			ProjectNumber, *ProjectName.ToString(), *QualityGradeStr);
		Text_StageInfo->SetText(FText::FromString(Formatted));
	}
}
