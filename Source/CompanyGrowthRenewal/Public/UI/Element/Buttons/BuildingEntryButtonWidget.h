#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "BuildingEntryButtonWidget.generated.h"

class UCommonButtonStyle;
class UImage;
class USizeBox;

/** 건물 관리 패널의 입장 버튼. 입력 영역과 가운데 표시 면을 별도 위젯으로 둔다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingEntryButtonWidget : public UIconWithButtonWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Face = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USizeBox> FaceSize = nullptr;

	// 입력층의 투명 스타일과 별도로 표시 면의 상태별 브러시를 공급한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Entry")
	TSubclassOf<UCommonButtonStyle> FaceStyleClass;

	// 0이면 WBP에 저작한 높이를 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Entry", meta = (ClampMin = "0.0"))
	float VisualHeight = 0.0f;

	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnPressed() override;
	virtual void NativeOnReleased() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;

private:
	enum class EFaceState : uint8
	{
		Unset,
		Normal,
		NormalHovered,
		NormalPressed,
		Selected,
		SelectedHovered,
		SelectedPressed,
		Disabled
	};

	void UpdateFaceBrush(bool bForce);

	EFaceState AppliedFaceState = EFaceState::Unset;
	TSubclassOf<UCommonButtonStyle> AppliedFaceStyleClass;
};
