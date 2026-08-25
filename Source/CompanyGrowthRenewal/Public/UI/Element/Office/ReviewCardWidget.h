#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ReviewCardWidget.generated.h"

class UCommonTextBlock;
class UBorder;

/**
 * 리뷰 피드 카드 — 비평가(이름+점수배지+한줄평)와 SNS/테스터(닉+코멘트) 겸용 베이스.
 * 정적 스타일은 WBP 변형(UIE_ReviewSnsCard / UIE_ReviewReactionToast) 소유, C++ 는 텍스트/밴드색 주입만.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UReviewCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetCriticData(const FText& InName, int32 InScore, const FText& InQuote);
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

	// 점수 배지 밴드색 (라이트 플레이트용 진한 톤, linear) — WBP Class Defaults 가 튜닝 표면
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor HighColor = FLinearColor(0.048f, 0.237f, 0.036f);   // sRGB #3E7E35

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor MidColor = FLinearColor(0.136f, 0.174f, 0.254f);    // sRGB #67748A

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor LowColor = FLinearColor(0.434f, 0.068f, 0.048f);    // sRGB #B04A3E
};
