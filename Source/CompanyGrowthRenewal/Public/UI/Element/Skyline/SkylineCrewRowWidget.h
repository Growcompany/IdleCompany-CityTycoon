#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/EmployeeTypes.h"
#include "SkylineCrewRowWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * 스카이라인 출전 팀 레일의 직원 1행 (WBP: UIE_SkylineCrewRow).
 *
 * 로스터 카드(UEmployeeRosterCardWidget)를 상속하지 않는다 — 저 카드는 CommonButtonBase 라
 * 선택·클릭·ListView 엔트리 계약을 끌고 오는데, 이 레일은 "누가 올라가는지"만 보여 주는 표시용이다.
 * 초상 로드만 그 클래스의 static 공유 루틴을 쓴다(중복 디코딩 루틴을 또 만들지 않는다).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USkylineCrewRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** SlotIndex = 직능 슬롯(0~5, EProductionDiscipline 순서). 직능명·직능색이 둘 다 이 축을 따른다. */
	void SetCrew(const FEmployeeInstance& Employee, int32 SlotIndex, float Power);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PowerText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DisciplineText = nullptr;

	/** 직능 슬롯 색만 받는 이미지. 흰색으로 저작하고 런타임에 색을 곱한다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DisciplineAccent = nullptr;

	/** 강화 별 문자열. 조판은 UEmployeeTypeHelper::MakeStarString 이 단일 진실이고 0강이면 접힌다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StarText = nullptr;

	/** 초상 다운스케일 예산 — 레일 행 높이 108 기준 초상 84 (목업 NOTES §②). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	FVector2D PortraitDisplaySize = FVector2D(84.f, 105.f);
};
