#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Table/ProfileImageData.h"
#include "ProfileAvatarTileWidget.generated.h"

class UImage;

/**
 * 프로필 이미지 격자의 타일 1칸
 * 선택 표시는 CommonButtonGroupBase 가 몰아주는 Selected 상태에만 반응한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProfileAvatarTileWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Profile")
	void SetProfileImage(const FProfileImageData& InData);

	UFUNCTION(BlueprintPure, Category = "Profile")
	int32 GetImageID() const { return ImageID; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> ThumbImage = nullptr;

	// 4코너 선택 브래킷 (T_UI_SelectBracket_Corner)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> SelectBracket = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> UseBadge = nullptr;

private:
	void ApplySelectionVisual(bool bChosen);

	int32 ImageID = 0;
};
