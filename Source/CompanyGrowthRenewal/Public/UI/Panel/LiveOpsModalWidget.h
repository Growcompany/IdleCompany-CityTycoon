// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "LiveOpsModalWidget.generated.h"

class UButton;
class UModalFrameWidget;
class UCloseButtonWidget;
class UGameClockSubsystem;

/**
 * 라이브옵스 창(출석/미션/우편함/소식)의 공용 골격.
 * 딤 클릭 · X 닫기 · 입력 모드 쌍 · 서버 시간 경고 밴드까지만 책임지고, 내용은 파생이 RefreshAll 로 채운다.
 */
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API ULiveOpsModalWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	ULiveOpsModalWidget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	// ESC·안드로이드 뒤로가기 = X 와 같은 닫기 ― 접속 때 스스로 뜨는 창이라 뒤로가기로 못 닫으면 갇힌 것처럼 느낀다
	virtual bool NativeOnHandleBackAction() override;

	// 파생: 매니저 상태로 표시 전체를 다시 채운다 (활성화 · 서버시간 유효성 변화 시 호출)
	virtual void RefreshAll() {}

	void ClosePanel();
	UCloseButtonWidget* GetPanelCloseButton() const;
	bool IsServerTimeValidNow() const;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UModalFrameWidget> ModalFrame;

	// 서버 시간 미확인 안내 밴드 — 유효하면 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* ServerTimeWarn = nullptr;

private:
	UFUNCTION()
	void OnBackgroundClicked();

	UFUNCTION()
	void OnCloseClicked();

	UGameClockSubsystem* GetGameClock() const;

	void ApplyServerTimeWarn(bool bValid);

	// 델리게이트 진입점 — 밴드만 갱신하고 끝내면 안 되고 표시값 전체가 권위 시각에 의존한다
	void HandleServerTimeValidity(bool bValid);
};
