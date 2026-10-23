#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/GuideTooltipPlacement.h"
#include "GuideTooltipWidget.generated.h"

class UCommonTextBlock;
class UCommonRichTextBlock;
class UImage;
class USizeBox;

/**
 * 튜토리얼 코치마크 말풍선 1장 ― 아이브로우(대상 이름) + 본문(안내문) + 꼬리.
 * 순수 표시 위젯: 자기 위치와 생명주기를 모른다(MissionGuideOverlayWidget 이 소유).
 * 꼬리 4방향은 한 장을 회전 배치한다 ― 4방향 텍스처를 따로 굽지 않는다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UGuideTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 같은 문구의 반복 주입은 레이아웃과 등장 상태를 유지한다. */
	bool SetContent(const FText& Eyebrow, const FText& Body);
	/** 소유 오버레이가 배치를 마친 프레임에 호출한다. */
	void AdvanceReveal(float DeltaSeconds);
	void HideGuide();

	/** Offset = 툴팁 안에서 꼬리가 놓일 축 방향 시작점(px). ComputeGuideTailOffset 결과. */
	void SetTail(EGuideTooltipDir Dir, float Offset);

protected:
	// 데이터 주입 대상이라 required ― Optional 이면 WBP 배선 누락이 silent null 이 된다
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* EyebrowText = nullptr;

	UPROPERTY(meta = (BindWidget))
	UCommonRichTextBlock* BodyText = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* TailImage = nullptr;

	UPROPERTY(meta = (BindWidget))
	USizeBox* TailBox = nullptr;

private:
	FText CurrentEyebrow;
	FText CurrentBody;
	bool bRevealPending = true;
	float RevealElapsed = 0.f;
	EGuideTooltipDir LastDir = EGuideTooltipDir::Up;
	float LastOffset = -1.0f;
};
