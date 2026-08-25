#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DisciplineBarWidget.generated.h"

class UCommonTextBlock;
class UProgressBar;

/**
 * 분야(직능) 세로 미니 바 1칸 — 값 / 세로 바 / 라벨.
 * 정적 스타일은 WBP(UIE_DisciplineBar) 소유, C++은 데이터와 상태색만 주입한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UDisciplineBarWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 활성 분야 — 이름·달성률(%)·바 채움·미달 상태색 일괄 주입
	void SetInfo(const FText& InName, float InAcquired, float InTarget, const FLinearColor& InFillColor);

	// 비활성 분야(요구 없음) — 빈 트랙 + 뮤트 잉크
	void SetEmpty(const FText& InName);

	// 라이트 웰용 채움색 — GetStepColor 원본은 다크 배경용 파스텔이라 흰 바탕에서 물빠져 보인다(카탈로그 라이트면 ×0.55)
	static FLinearColor GetLightWellSlotFill(int32 SlotIdx);

protected:
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Value;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* Bar;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Name;

	// 잉크 노브 — WBP CDO가 SOT (전부 linear). 바 채움색은 호출자가 주입한다(직능 슬롯 위치색 = GetStepColor SOT)
	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	FLinearColor ValueInk = FLinearColor(0.030f, 0.012f, 0.003f, 0.55f);   // 카드 rest 잉크 #301D0A @55%

	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	FLinearColor MissInk = FLinearColor(0.571f, 0.209f, 0.002f, 1.0f);     // 앰버 #C77E08

	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	FLinearColor NameInk = FLinearColor(0.030f, 0.012f, 0.003f, 0.62f);    // 카드 잉크 #301D0A @62%

	UPROPERTY(EditAnywhere, Category = "Discipline Bar")
	float EmptyInkAlpha = 0.3f;
};
