#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/ProjectMode.h"
#include "ProjectInfoBarWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * 프로젝트 정보 바 위젯
 * 현재 진행 중인 프로젝트 정보를 상태별로 표시
 * - 대기: "프로젝트를 선택해주세요"
 * - 개발 중: "수주 - 점프왕 개발 중" / "자체개발 - 점프왕 개발 중"
 * - 운영 중: "프로젝트1 - 점프왕 [A] 운영 중"
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProjectInfoBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UProjectInfoBarWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 개발 중 상태 표시 (모드별 접두어)
	 */
	UFUNCTION(BlueprintCallable, Category = "Project Info")
	void SetDevelopmentStatus(int32 ProjectNumber, const FText& ProjectName, EProjectMode Mode = EProjectMode::None);

	/**
	 * 대기 상태 표시 — 아무 프로젝트도 선택되지 않았을 때
	 */
	UFUNCTION(BlueprintCallable, Category = "Project Info")
	void SetIdleStatus();

	/**
	 * 운영 상태 표시 (프로젝트 번호 + 품질 등급 포함)
	 */
	UFUNCTION(BlueprintCallable, Category = "Project Info")
	void SetOperationStatus(int32 ProjectNumber, const FText& ProjectName, const FString& QualityGradeStr);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_StageInfo;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ProjectIcon;
};
