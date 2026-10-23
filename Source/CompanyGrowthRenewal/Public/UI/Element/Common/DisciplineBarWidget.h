#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DisciplineBarWidget.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UImage;
class UTexture2D;

/**
 * 분야(직능) 세로 미니 바 1칸 — 값 / 세로 바 / 라벨.
 * 정적 스타일은 WBP(UIE_DisciplineBar) 소유, C++은 데이터와 상태색만 주입한다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UDisciplineBarWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 활성 분야 — 이름·달성률(%)·바 채움·미달 상태색 일괄 주입
	void SetInfo(const FText& InName, float InAcquired, float InTarget, const FLinearColor& InFillColor);

	// 비활성 분야(요구 없음) — 빈 트랙 + 뮤트 잉크
	void SetEmpty(const FText& InName);

	// 결과 화면 리빌 — SetInfo 값을 0..1 진행만큼 채우고 센다.
	// 세는 동안 ValueInk, 100%를 넘는 순간 HitInk, 100% 전에 멈추면 MissInk.
	void SetRevealProgress(float Alpha);

	// 채움을 흰빛 쪽으로 당긴다(0~1) — 100% 돌파 순간의 번쩍임
	void SetFillFlash(float Amount);

	// SetInfo 의 획득/목표 비율(오버필 포함). 비활성 칸은 0
	float GetAchievedRatio() const { return AchievedRatio; }
	bool IsActiveDiscipline() const { return bActiveDiscipline; }

	// 라이트 웰용 채움색 — GetStepColor 원본은 다크 배경용 파스텔이라 흰 바탕에서 물빠져 보인다(카탈로그 라이트면 ×0.55)
	static FLinearColor GetLightWellSlotFill(int32 SlotIdx);

protected:
	virtual void NativePreConstruct() override;
	virtual void SynchronizeProperties() override;
	void ApplyIconProperties();

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Value;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* Bar;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Name;

	// 나머지 셋과 달리 Optional — 필수로 걸면 아이콘을 넣기 전 UIE_DisciplineBar 가 컴파일 에러로 죽는다
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* DiscIcon;

	// 분야 글리프는 패널 WBP 인스턴스가 소유한다(정적 스타일=WBP). 비우면 아이콘이 접힌다
	UPROPERTY(EditAnywhere, Category = "Discipline Bar|Icon")
	TSoftObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Discipline Bar|Icon")
	FVector2D IconSize = FVector2D(22.0f, 22.0f);

	UPROPERTY(EditAnywhere, Category = "Discipline Bar|Icon")
	FMargin IconPadding = FMargin(0.0f, 0.0f, 0.0f, 3.0f);

	// 잉크 노브 — WBP CDO가 SOT (전부 linear). 바 채움색은 호출자가 주입한다(직능 슬롯 위치색 = GetStepColor SOT)
	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	FLinearColor ValueInk = FLinearColor(0.030f, 0.012f, 0.003f, 0.55f);   // 카드 rest 잉크 #301D0A @55%

	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	FLinearColor MissInk = FLinearColor(0.571f, 0.209f, 0.002f, 1.0f);     // 앰버 #C77E08

	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	FLinearColor NameInk = FLinearColor(0.030f, 0.012f, 0.003f, 0.62f);    // 카드 잉크 #301D0A @62%

	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	float EmptyInkAlpha = 0.3f;

	// 리빌에서 100%를 넘긴 달성률 글자색 — 다크면 증가 잉크 #6CC386
	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	FLinearColor HitInk = FLinearColor(0.150f, 0.546f, 0.238f, 1.0f);

private:
	void LockRevealValueWidth();
	void UnlockRevealValueWidth();

	float AchievedRatio = 0.0f;
	FLinearColor RevealFillColor = FLinearColor::White;
	bool bActiveDiscipline = false;
	float RevealBaseMinWidth = 0.0f;
	bool bRevealWidthLocked = false;
};
