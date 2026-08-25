// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Engine/TimerHandle.h"
#include "AnimatedActivatableWidget.generated.h"

// 등장 슬라이드가 시작되는 방향. 화면 가장자리에 붙는 시트(드로어)는 그 가장자리에서 나와야 자연스럽다.
UENUM(BlueprintType)
enum class EAppearSlideFrom : uint8
{
	Bottom UMETA(DisplayName = "아래에서"),
	Left   UMETA(DisplayName = "왼쪽에서"),
	Right  UMETA(DisplayName = "오른쪽에서")
};

/**
 * 등장 모션을 지원하는 CommonActivatableWidget 베이스 클래스.
 * 우선순위: bInstantAppear(즉시) > "Show" 위젯 애니(저작된 경우) > 코드 등장 트윈(아래→위 슬라이드).
 * 투명도 페이드는 스택 전이(FADE_ONLY)가 담당하고, 여기서는 위치 슬라이드만 제어한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UAnimatedActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	bool IsCloseRequested() const { return bIsClosing; }

protected:
	// 슬라이드를 적용할 대상 — WBP 에 이 이름의 위젯(보통 셸)이 있으면 그것만 움직인다.
	// 없으면 위젯 전체가 움직이는데, 자체 딤을 가진 패널은 **딤까지 같이 밀려** 화면 가장자리에
	// 안 덮인 띠가 생겼다가 채워진다(이동 거리가 클수록 확연). 딤은 스택 페이드만 타야 한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AppearSlideTarget = nullptr;

	// WBP에서 "Show"/"Hide" 이름으로 애니메이션 만들면 자동 바인딩
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Show;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Hide;

	// 자식 클래스에서 DeactivateWidget() 대신 호출
	void CloseWithAnimation();

	// 비활성 뷰포트 오버레이(activate 없이 AddToViewport)는 NativeOnActivated 를 안 타므로 직접 호출
	void StartAppearTween();

	// 좌우가 큰 관리창은 슬라이드가 무겁고 어색 — 등장 슬라이드 없이 스택 페이드만(거의 즉시).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appear")
	bool bInstantAppear = false;

	// 등장 슬라이드 튜닝 — 이동 거리(px)와 시간(s). CubicOut 감속.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appear")
	float AppearRiseDistance = 48.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appear")
	float AppearDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appear")
	EAppearSlideFrom AppearFrom = EAppearSlideFrom::Bottom;

private:
	// 방향 → 시작 오프셋 (트윈이 이 값에서 0 으로 감속)
	FVector2D GetAppearOffset() const;

	// 실제로 움직일 위젯 (AppearSlideTarget 이 없으면 자기 자신)
	UWidget* GetSlideWidget() const;

	UFUNCTION()
	void OnHideAnimFinished();

	bool bIsClosing = false;

	// 등장 슬라이드 트윈 상태 (opacity 는 스택 FADE_ONLY 담당, 여기선 위치만)
	bool bAppearing = false;
	float AppearElapsed = 0.f;

	// Hide finished 유실 시 위젯이 '안 보이는데 active'로 남아 입력이 잠김 — 강제 마감 타이머
	FTimerHandle CloseFailsafeHandle;
};
