#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "CourseStepWidget.generated.h"

class UCommonTextBlock;
class UWidgetSwitcher;

UENUM(BlueprintType)
enum class ECourseStepState : uint8
{
	Done = 0,     // 이미 깬 과정 — 골드 점등 + 체크
	Current,      // 지금 도전 중 — 솟은 카드, 기능색 림
	Next          // 아직 안 온 과정 — 요구치만
};

/**
 * 기술 레일의 한 칸 (WBP: UIE_CourseStep).
 *
 * 칸을 카드 복붙으로 만들면 카탈로그가 금지한 anti-pattern 이 된다
 * (UIE_OfficeProjectCardRequest DevPlate 6칸 선례) — 부품 하나를 레일이 N 번 배치한다.
 *
 * 상태별 생김새는 WBP 가 소유한다. C++ 은 어느 판을 보일지와 글자·잉크만 정한다
 * (정적 스타일은 WBP, C++ 세터 금지 — 플레이북). 잉크는 면이 갈리므로 런타임 주입이다.
 *
 * 가로 숫자 사다리에서 세로 기술 레일로 바뀌며 기술명 줄이 붙었다 — 판·배지·캡션 구성은 그대로다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCourseStepWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** RequirementLabel 이 비면 요구치 줄을 접는다(밴드 밖 과정). */
	void SetCourse(int32 Course, const FText& TopicName, const FText& RequirementLabel, ECourseStepState State);

protected:
	/** 0 = Done · 1 = Current · 2 = Next. 판 생김새만 갈린다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> PlateSwitcher = nullptr;

	/** 배지 3칸 — 완료 체크 / 번호 / 자물쇠. 판과 같은 인덱스를 쓴다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> BadgeSwitcher = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CourseNumberText = nullptr;

	/** 기술명 — 레일이 숫자가 아니라 기술 목록이 되면서 이 줄이 주인공이다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RequirementText = nullptr;

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor NameInkDone = FLinearColor(0.3233f, 0.3761f, 0.4566f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor NameInkCurrent = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor NameInkNext = FLinearColor(0.1527f, 0.1882f, 0.2419f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor RequirementInkDone = FLinearColor(0.1882f, 0.2272f, 0.2749f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor RequirementInkCurrent = FLinearColor(0.6243f, 0.7994f, 0.9550f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor RequirementInkNext = FLinearColor(0.1096f, 0.1329f, 0.1748f, 1.f);
};
