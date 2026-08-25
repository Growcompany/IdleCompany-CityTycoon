// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "UI/Interface/ButtonSoundInterface.h"
#include "Enum/ResourceType.h"
#include "ButtonWidget.generated.h"

class UCommonTextBlock;

// Selected 상태 변경 신호 — UCommonButtonBase 의 protected 델리게이트를 외부 C++ 에서도 쓰도록 재노출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnButtonIsSelectedChanged, bool, bIsSelected);

/**
 * 기본 텍스트 버튼 위젯
 * - IButtonSoundInterface 상속으로 자동 사운드 재생
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UButtonWidget : public UCommonButtonBase, public IButtonSoundInterface
{
	GENERATED_BODY()

public:
	// Selected/Deselected 시 발화 — Native 가상함수에서 포워딩
	UPROPERTY(BlueprintAssignable, Category = "Button")
	FOnButtonIsSelectedChanged OnIsSelectedChanged;

protected:
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	UCommonTextBlock* ButtonText;

	UPROPERTY(EditAnywhere, Category = "Button")
	FText Text;

	// 텍스트 크기 (블루프린트 인스턴스에서 설정 가능, 0이면 기본 크기 유지)
	UPROPERTY(EditAnywhere, Category = "Button", meta = (ClampMin = "0.0"))
	float TextSize = 0.0f;

	// 텍스트 패딩 (상하좌우)
	UPROPERTY(EditAnywhere, Category = "Button")
	FMargin TextPadding = FMargin(0.0f, 0.0f, 0.0f, 0.0f);

	// 텍스트 색상 (White면 기본 스타일 색상 사용)
	UPROPERTY(EditAnywhere, Category = "Button")
	FSlateColor TextColor = FSlateColor(FLinearColor::White);

	// 텍스트 아웃라인 활성화 여부
	UPROPERTY(EditAnywhere, Category = "Button|Outline")
	bool bEnableTextOutline = false;

	// 아웃라인 크기
	UPROPERTY(EditAnywhere, Category = "Button|Outline", meta = (EditCondition = "bEnableTextOutline", ClampMin = "0.0"))
	float OutlineSize = 2.0f;

	// 아웃라인 색상
	UPROPERTY(EditAnywhere, Category = "Button|Outline", meta = (EditCondition = "bEnableTextOutline"))
	FLinearColor OutlineColor = FLinearColor::Black;

	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetButtonText(const FText& NewText);

	// 텍스트 크기 설정
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetTextSize(float NewSize);

	// 텍스트 패딩 설정
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetTextPadding(FMargin NewPadding);

	// 텍스트 색상 설정
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetTextColor(FSlateColor NewColor);

	// 텍스트 아웃라인 활성화/비활성화
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetTextOutlineEnabled(bool bEnabled, float Size = 2.0f, FLinearColor Color = FLinearColor::Black);

	// 아웃라인 설정만 업데이트 (이미 활성화된 상태에서)
	UFUNCTION(BlueprintCallable, Category = "Button")
	void UpdateTextOutline(float Size, FLinearColor Color);

	// 글자 사이 간격 (Letter Spacing) 설정. 음수면 더 밀착, 양수면 더 벌어짐.
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetLetterSpacing(int32 NewSpacing);

	// claim 가능 어포던스 — 브리딩 스케일 펄스 루프 on/off (꺼지면 스케일 복원)
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SetPulseEnabled(bool bEnabled);

	/**
	 * 비활성(SetIsEnabled(false)) 대신 쓰는 거부 모드.
	 * 버튼은 활성 상태를 유지하되 클릭을 삼키고 사유를 알린다 — 왜 안 되는지 모른 채 무반응인 상황을 없앤다.
	 * 재화 부족처럼 "지금은 안 되지만 곧 가능한" 조건에만 쓸 것. MAX/미해금 같은 구조적 불가는 비활성이 맞다.
	 * 선택형(탭) 버튼에는 쓰지 말 것 — 선택 토글은 클릭 삼킴보다 먼저 일어난다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Button|Reject")
	void SetRejectReason(const FText& Reason);

	// 자원 부족 거부 — 부족분은 실시간으로 변하므로 문구는 클릭 시점에 만들어진다
	UFUNCTION(BlueprintCallable, Category = "Button|Reject")
	void SetRejectInsufficient(EResourceType Type, int64 Need);

	UFUNCTION(BlueprintCallable, Category = "Button|Reject")
	void ClearRejectReason();

	UFUNCTION(BlueprintPure, Category = "Button|Reject")
	bool HasRejectReason() const;

private:
	// 거부 사유 — 둘 중 하나만 활성. bRejectInsufficient 면 클릭 시점에 부족분을 계산한다.
	FText RejectReason;
	bool bRejectInsufficient = false;
	EResourceType RejectResourceType = EResourceType::Money;
	int64 RejectResourceNeed = 0;

	void PlayRejectFeedback();

	// 거부 셰이크 — 펀치/펄스보다 우선(같은 RenderTransform 을 쓴다). <0 = 비활성
	float RejectShakeElapsed = -1.f;
	static constexpr float RejectShakeDuration = 0.28f;
	static constexpr float RejectShakeAmplitude = 6.f;
	void UpdateRejectShake(float DeltaTime);

	void ApplyTextOutline();

	// 클릭 펀치 (눌림 0.95 → 1.0 복원). <0 = 비활성
	float PressPunchElapsed = -1.f;
	static constexpr float PressPunchDuration = 0.12f;
	static constexpr float PressPunchMinScale = 0.95f;
	void UpdatePressPunch(float DeltaTime);

	// 펄스 — 펀치 진행 중엔 펀치가 우선
	bool bPulseEnabled = false;
	float PulseElapsed = 0.f;
	static constexpr float PulsePeriod = 1.2f;
	static constexpr float PulseScaleAmp = 0.04f;
	void UpdatePulse(float DeltaTime);
};
