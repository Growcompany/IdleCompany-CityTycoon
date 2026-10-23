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

UENUM(BlueprintType)
enum class EAppearPopStyle : uint8
{
	Standard,
	Compact
};

/**
 * 등장 모션을 지원하는 CommonActivatableWidget 베이스 클래스.
 * 우선순위: bInstantAppear(즉시) > "Show" 위젯 애니 > 중앙 내용판 팝 / 가장자리 슬라이드.
 * 투명도는 스택 전이(FADE_ONLY)가 담당하고, 팝은 배경과 분리된 내용판에만 적용한다.
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

	// 스택 밖에서 AddToViewport 한 뒤 호출한다. 활성화·입력 설정을 변경하지 않고 페이드도 함께 재생한다.
	UFUNCTION(BlueprintCallable, Category = "Appear")
	void PlayOverlayAppear();

protected:
	virtual void NativeDestruct() override;

	// 슬라이드를 적용할 대상 — WBP 에 이 이름의 위젯(보통 셸)이 있으면 그것만 움직인다.
	// 없으면 위젯 전체가 움직이는데, 자체 딤을 가진 패널은 **딤까지 같이 밀려** 화면 가장자리에
	// 안 덮인 띠가 생겼다가 채워진다(이동 거리가 클수록 확연). 딤은 스택 페이드만 타야 한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AppearSlideTarget = nullptr;

	// 중앙 모달에서 확대할 내용판 이름. None 또는 대상 없음이면 기존 슬라이드를 사용한다.
	// 전체 화면 딤을 포함하지 않는 위젯이어야 하며, 좌우 드로어에는 적용하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appear")
	FName AppearPopTargetName = TEXT("ModalFrame");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appear")
	EAppearPopStyle AppearPopStyle = EAppearPopStyle::Standard;

	// WBP에서 "Show"/"Hide" 이름으로 애니메이션 만들면 자동 바인딩
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Show;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Hide;

	// 자식 클래스에서 DeactivateWidget() 대신 호출
	void CloseWithAnimation();

	// 비활성 뷰포트 오버레이(activate 없이 AddToViewport)는 NativeOnActivated 를 안 타므로 직접 호출
	void StartAppearTween(bool bFadeIn = false);

	// 자체 등장 모션 없이 스택 페이드만 사용한다.
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
	void FinishAppearTween();

	UFUNCTION()
	void OnHideAnimFinished();

	bool bIsClosing = false;

	// 재생 중에만 원래 변환을 보관한다. 종료·중단·풀 재사용 모두 동일한 값으로 복원한다.
	bool bAppearing = false;
	bool bPopping = false;
	bool bFading = false;
	float AppearElapsed = 0.f;
	float AppearOriginalOpacity = 1.f;
	TWeakObjectPtr<UWidget> AppearTweenTarget;
	FWidgetTransform AppearOriginalTransform;
	FVector2D AppearOriginalPivot = FVector2D(0.5f, 0.5f);

	// Hide finished 유실 시 위젯이 '안 보이는데 active'로 남아 입력이 잠김 — 강제 마감 타이머
	FTimerHandle CloseFailsafeHandle;
};
