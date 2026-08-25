#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ProfileImagePanelWidget.generated.h"

class UButton;
class UImage;
class UCommonTextBlock;
class UCloseButtonWidget;
class UWrapBox;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UTableManagerSubsystem;
class UProfileAvatarTileWidget;

/**
 * 프로필 이미지 선택 패널
 * - 좌: 현재 프로필 미리보기 (메인 HUD 액자와 같은 규격)
 * - 우: DT_ProfileImage 아바타 격자, 타일 클릭 → ProfileImageID 변경
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProfileImagePanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> UIE_CloseButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> ImageCardContainer = nullptr;

	// ===== 좌측 미리보기 =====
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> PreviewImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PreviewNameText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PreviewSubText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PreviewLevelText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CountText = nullptr;

private:
	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> TileGroup = nullptr;

	UPROPERTY()
	TObjectPtr<UTableManagerSubsystem> TableMgr = nullptr;

	UFUNCTION()
	void OnTileSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnBackgroundClicked();

	void BuildTileGrid();
	void RefreshPreview(int32 ImageID);
	void ApplyProfileImage(int32 ImageID);
};
