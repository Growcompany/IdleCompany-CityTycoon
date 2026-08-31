// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/WidgetType.h"
#include "MenuPanelWidget.generated.h"

class UButton;
class UBuildingSkinCardWidget;
class UCloseButtonWidget;
class UTableManagerSubsystem;
class USaveLoadManager;

/**
 * 메뉴 패널 위젯 - 순위, 가챠, 설정 메뉴를 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMenuPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION()
	void OnCancelButtonClicked();

	UFUNCTION()
	void OnRankButtonClicked();

	UFUNCTION()
	void OnGachaButtonClicked();

	UFUNCTION()
	void OnSettingsButtonClicked();

	UFUNCTION()
	void OnBackgroundClicked();

	// 패널 닫기
	void ClosePanel();

	// 배경 클릭 시 닫기용 투명 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn;

	// 닫기 버튼 (UIE_CloseButton)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// 랭킹 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBuildingSkinCardWidget* RankBtn;

	// 가챠(특성 뽑기) 버튼 — BuildingTraitGachaPanel 진입점 (WBP 추가 전까지 Optional)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBuildingSkinCardWidget* GachaBtn;

	// 설정 버튼 — 하단 유틸 필의 투명 히트 영역 (BackgroundBtn과 같은 패턴)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* SettingsBtn;

private:
	// 메뉴 버튼 해금 상태 갱신
	void UpdateAllButtonLockStates();
	void UpdateButtonLockState(UBuildingSkinCardWidget* Btn, FName BtnName);

	// 닫기 애니메이션 후 열 위젯 타입 (None이면 없음)
	EWidgetType PendingOpenWidget = EWidgetType::None;
};
