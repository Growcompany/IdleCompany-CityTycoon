// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "FatigueBarWidget.generated.h"

class UProgressBar;

/**
 * 직원 머리 위 상시 피로 게이지 바 (월드추적 WidgetComponent 내용물).
 *
 * 무드 버블(UIE_WorkerBubble)과 별개의 전용 위젯 — 무드는 None 시 컴포넌트째 숨겨지지만
 * 이 바는 항상 표시되어야 하므로 분리. 워커는 FatigueBarComponent(전용 UWidgetComponent)로 들고 있다.
 *
 * 구동: 워커가 1초 throttle 로 SetFatigue01(BehaviorComponent->GetFatigue01()) 호출.
 *  - Percent = Fill01 (피로 낮음=거의 빈 바 / 높음=가득 — "차오르는" 모델)
 *  - FillColorAndOpacity = 초록→노랑→빨강 보간 (피곤할수록 위험해 보이게)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UFatigueBarWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 0~1 정규화 피로값으로 바 채움 + 색 갱신 (값 변화 없으면 no-op)
	UFUNCTION(BlueprintCallable, Category = "Fatigue")
	void SetFatigue01(float InFill01);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* FatigueBar;

private:
	// 마지막 적용값 (불필요한 SetPercent/SetFillColor 호출 회피)
	float LastFill01 = -1.f;

	// Fill01(0~1) → HP 바 색. 0=초록, 0.5=노랑, 1=빨강.
	static FLinearColor FillColorForFatigue(float Fill01);
};
