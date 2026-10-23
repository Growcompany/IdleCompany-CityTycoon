#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ReviewCardWidget.generated.h"

class UCommonTextBlock;
class UBorder;

/**
 * 리뷰 카드 — 닉+코멘트(출시 결과창 실패 페이지의 테스터 의견).
 * 정적 스타일은 WBP(UIE_ReviewSnsCard) 소유, C++ 는 텍스트 주입과 점수 배지 숨김만.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UReviewCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetSnsData(const FText& InNick, const FText& InComment);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* ScoreBadge;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* ScoreText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* QuoteText;
};
