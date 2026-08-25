#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Office/ReviewCardWidget.h"
#include "Manager/LaunchReactionSubsystem.h"
#include "ReviewReactionToastWidget.generated.h"

class UImage;

/**
 * 운영 중 이벤트 레일 반응 토스트 (UIE_ReviewReactionToast) — 비평가/SNS 겸용, 자동 만료.
 * 등장 = 우측 슬라이드, 유지 HoldSeconds, 퇴장 = 페이드 후 OnRailRemoveRequested(레일이 RemoveEventCard).
 * 전 트리 HitTestInvisible — 사무실 입력 통과(레일 계약).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UReviewReactionToastWidget : public UReviewCardWidget
{
	GENERATED_BODY()

public:
	void SetupReaction(const FLaunchReaction& Reaction);
	FSimpleDelegate OnRailRemoveRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* KindText;

	UPROPERTY(meta = (BindWidget))
	UImage* AccentBar;

	// 튜닝 노브 (WBP Class Defaults)
	UPROPERTY(EditAnywhere, Category = "Toast")
	float SlideInSeconds = 0.32f;

	UPROPERTY(EditAnywhere, Category = "Toast")
	float HoldSeconds = 7.f;

	UPROPERTY(EditAnywhere, Category = "Toast")
	float FadeOutSeconds = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Toast")
	float SlideDistance = 60.f;

	// 다크 칩용 밴드색 (부모 High/Mid/LowColor 는 라이트 플레이트 톤이라 별도)
	UPROPERTY(EditAnywhere, Category = "Toast|Color")
	FLinearColor DarkHigh = FLinearColor(0.110f, 0.522f, 0.091f);   // #5FBF58

	UPROPERTY(EditAnywhere, Category = "Toast|Color")
	FLinearColor DarkMid = FLinearColor(0.254f, 0.313f, 0.412f);    // #8A97AB

	UPROPERTY(EditAnywhere, Category = "Toast|Color")
	FLinearColor DarkLow = FLinearColor(0.745f, 0.141f, 0.107f);    // #E06A5C

private:
	float Clock = -1.f;   // <0 = 미시작
	bool bRemoveRequested = false;
};
