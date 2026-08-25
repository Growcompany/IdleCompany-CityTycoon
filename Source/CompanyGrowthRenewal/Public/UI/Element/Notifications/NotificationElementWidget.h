// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NotificationElementWidget.generated.h"

class UHorizontalBox;
class UImage;
class UCommonTextBlock;
class UTexture2D;

// 알림 애니메이션 상태
UENUM(BlueprintType)
enum class ENotificationAnimState : uint8
{
	Idle,
	ShowingIn,
	Displaying,
	HidingOut
};

// 알림 종료 델리게이트
DECLARE_DELEGATE_OneParam(FOnNotificationFinished, UNotificationElementWidget*);

/**
 * 알림 UI Element 위젯
 * - 화면 상단에 메시지를 표시하고 자동으로 사라지는 알림 위젯
 * - 슬라이드 인/아웃 애니메이션 지원
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UNotificationElementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNotificationElementWidget(const FObjectInitializer& ObjectInitializer);

	// 알림 초기화 및 표시 시작
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void ShowMessage(const FText& Message, float Duration = 3.0f, FLinearColor TextColor = FLinearColor(1.0f, 0.867f, 0.478f, 1.0f));

	UFUNCTION(BlueprintCallable, Category = "Notification")
	void ShowIconMessage(const FText& Message, UTexture2D* Icon, float Duration = 3.0f, FLinearColor TextColor = FLinearColor(1.0f, 0.867f, 0.478f, 1.0f));

	// 강제 숨기기
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void ForceHide();

	// 같은 문구 재발행 흡수 — 새 엘리먼트를 만들지 않고 표시 시간만 되돌린다
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void RefreshDisplay(float Duration, FLinearColor TextColor);

	// 알림 종료 델리게이트
	FOnNotificationFinished OnNotificationFinished;

	// 슬라이드 축 선택 — 우측 레일 토스트는 우측에서 슬라이드-인(true), 전역 배너는 상단 드롭(false, 기본).
	// 레일이 CreateWidget 직후 ShowMessage 전에 호출한다.
	void SetSlideFromRight(bool bFromRight) { bSlideFromRight = bFromRight; }

protected:
	// BindWidget 멤버들 (필수)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* HBox_Background;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Image_LeftGradient;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Image_CenterBg;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Image_RightGradient;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_Message;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* Image_ResourceIcon;

	// 설정 프로퍼티
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Timing")
	float DisplayDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Timing")
	float FadeInDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Timing")
	float FadeOutDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Animation")
	float SlideDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Layout")
	float GradientWidth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Layout")
	float MinCenterPadding = 40.0f;

	// Native 오버라이드
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// 현재 애니메이션 상태
	ENotificationAnimState CurrentAnimState = ENotificationAnimState::Idle;

	// 슬라이드 축 (true=우측 X축, false=상단 Y축). 직렬화 불필요(런타임에 레일이 설정).
	bool bSlideFromRight = false;

	// 애니메이션 시간 추적
	float AnimationElapsedTime = 0.0f;

	// 표시 시간 추적
	float DisplayElapsedTime = 0.0f;

	// 실제 사용할 표시 시간 (Initialize에서 설정)
	float ActualDisplayDuration = 3.0f;

	// 초기 Y 오프셋 (RenderTransform용)
	float InitialYOffset = 0.0f;

	// 애니메이션 상태별 업데이트
	void UpdateShowingIn(float DeltaTime);
	void UpdateDisplaying(float DeltaTime);
	void UpdateHidingOut(float DeltaTime);

	// 애니메이션 상태 전환
	void TransitionToState(ENotificationAnimState NewState);

	// 렌더 트랜스폼 및 알파 설정
	void SetRenderOffset(float Offset);
	void SetWidgetAlpha(float Alpha);

	// Easing 함수
	float EaseOutQuad(float t) const;
	float EaseInQuad(float t) const;
};
