#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "BusinessTripCrewCardWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * 출장 선발 레일 카드 (UIE_BusinessTripCrewChip) — 직원창 로스터 카드의 컴팩트 변형.
 *
 * 초상·이름·Lv·별·등급 젬·선택 연출·클릭은 전부 부모(UEmployeeRosterCardWidget) 것을 쓴다.
 * 초상 크기는 부모의 PortraitDisplaySize 를 WBP Class Defaults 에서 작게 구워 맞춘다.
 * 여기엔 출장에만 있는 두 가지만 둔다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBusinessTripCrewCardWidget : public UEmployeeRosterCardWidget
{
	GENERATED_BODY()

public:
	/** 고른 카드가 요구하는 직능의 값. InScore 음수 = 카드 미선택이라 어느 직능인지 아직 정해지지 않음. */
	void SetContextScore(const FText& InDisciplineName, int32 InScore);

	/** 이 건물 배치 팀 표시 — 데려가면 그동안 이 건물 생산에서 빠진다는 유일한 신호. */
	void SetBuildingTeam(bool bInBuildingTeam);

protected:
	/** 부모의 OverallText(종합 전투력)와 역할이 달라 자리를 따로 둔다 — 여기는 «요구 직능» 값이다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* ContextScoreText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* RoleRing = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor BenchRingColor = FLinearColor(0.128f, 0.113f, 0.113f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor BuildingTeamRingColor = FLinearColor(0.565f, 0.212f, 0.003f, 1.f);
};
