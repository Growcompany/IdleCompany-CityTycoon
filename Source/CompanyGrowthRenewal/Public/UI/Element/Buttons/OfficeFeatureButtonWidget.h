#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/MenuTileWidget.h"
#include "OfficeFeatureButtonWidget.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Office HUD feature entry with a WBP-authored icon material and level lock. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeFeatureButtonWidget : public UMenuTileWidget
{
	GENERATED_BODY()

public:
	void SetFeatureLocked(bool bInLocked, const FText& LevelLabel, const FText& ToastReason);

protected:
	virtual void NativePreConstruct() override;

	// IconWithButtonWidget reapplies IconTexture during PreConstruct; keep the authored material explicit.
	UPROPERTY(EditAnywhere, Category = "Office Feature|Icon")
	TObjectPtr<UMaterialInterface> FeatureIconMaterial = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> BackingImage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Office Feature|Icon")
	TObjectPtr<UMaterialInterface> FeatureBackingMaterial = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FeatureIconMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FeatureBackingMID = nullptr;

	void ApplyFeatureIcon();
};
