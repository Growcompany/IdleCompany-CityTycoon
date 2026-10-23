// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/SoundCategory.h"
#include "SettingsPanelWidget.generated.h"

class UButton;
class UBorder;
class UButtonWidget;
class UCloseButtonWidget;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UEditableTextBox;
class UIconWithButtonWidget;
class UImage;
class UModalFrameWidget;
class USettingSegmentRowWidget;
class USettingSliderRowWidget;
class USettingToggleRowWidget;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

/**
 * 설정창 — 사운드/그래픽/계정 3탭 중앙 모달 (좌측 세로 탭 레일).
 * 값 변경은 즉시 적용, 디스크 저장은 닫힐 때 dirty 기준 1회.
 * SOT: docs/05_UI/mockups/settings_redesign/CODEX_HANDOFF.md
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USettingsPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// ===== 공통 크롬 =====
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UModalFrameWidget* ModalFrame;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* VersionText;

	// ===== 탭 레일 =====
	UPROPERTY(meta = (BindWidget))
	UIconWithButtonWidget* SoundTab;

	UPROPERTY(meta = (BindWidget))
	UIconWithButtonWidget* GraphicsTab;

	UPROPERTY(meta = (BindWidget))
	UIconWithButtonWidget* AccountTab;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* SoundTabPlate;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* GraphicsTabPlate;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* AccountTabPlate;

	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* ContentSwitcher;

	// ===== 사운드 탭 (ESoundCategory 1:1) =====
	UPROPERTY(meta = (BindWidget))
	USettingSliderRowWidget* MasterRow;

	UPROPERTY(meta = (BindWidget))
	USettingSliderRowWidget* MusicRow;

	UPROPERTY(meta = (BindWidget))
	USettingSliderRowWidget* SFXRow;

	UPROPERTY(meta = (BindWidget))
	USettingSliderRowWidget* UIRow;

	// ===== 그래픽 탭 =====
	UPROPERTY(meta = (BindWidget))
	USettingSegmentRowWidget* QualityRow;

	UPROPERTY(meta = (BindWidget))
	USettingSegmentRowWidget* FpsRow;

	UPROPERTY(meta = (BindWidget))
	USettingToggleRowWidget* ReduceMotionRow;

	UPROPERTY(meta = (BindWidget))
	USettingToggleRowWidget* HandOpenBoxRow;

	// ===== 계정 탭 =====
	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatusChipText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* NickNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* AccountNoteText;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* StatusDot;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* NickInput;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* NickChangeBtn;

	// 게스트일 때만 표시되는 행 컨테이너 (Border 등 아무 위젯)
	UPROPERTY(meta = (BindWidget))
	UWidget* GoogleLinkRow;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* AccountManageCard;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* GoogleLinkBtn;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* LogoutBtn;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* ResetBtn;

private:
	// 탭
	void InitTabs();
	void UpdateTabPlates(int32 SelectedIndex);
	UFUNCTION()
	void HandleTabChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	// 사운드
	void RefreshSoundTab();
	void HandleVolumeRowChanged(float NewValue01, ESoundCategory Category);
	void HandleMuteRowToggled(bool bNewMuted, ESoundCategory Category);
	void HandleSliderReleased();
	void ApplyMasterDim();

	// 그래픽
	void RefreshGraphicsTab();
	void HandleQualityChanged(int32 Index);
	void HandleFpsChanged(int32 Index);
	void HandleReduceMotionToggled(bool bOn);
	void HandleHandOpenBoxToggled(bool bOn);

	// 계정
	void RefreshAccountTab();
	void HandleNickChangeClicked();
	void HandleGoogleLinkClicked();
	void HandleLogoutClicked();
	void HandleResetClicked();
	void HandleResetConfirmed();
	void HandleQuitConfirmed();

	void ClosePanel();
	UFUNCTION()
	void OnBackgroundClicked();
	UFUNCTION()
	void OnCloseClicked();

	class USoundManagerSubsystem* GetSoundMgr() const;
	class USettingsManagerSubsystem* GetSettingsMgr() const;
	class UPlayFabManagerSubsystem* GetPlayFabMgr() const;
	class UUIManagerSubsystem* GetUIMgr() const;

	UPROPERTY()
	UCommonButtonGroupBase* TabGroup = nullptr;

	bool bAudioDirty = false;
	bool bSettingsDirty = false;
	// Configure 중 델리게이트 역류 방지
	bool bRefreshing = false;
};
