#include "UI/Element/Buttons/BuildingEntryButtonWidget.h"

#include "CommonButtonBase.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"

void UBuildingEntryButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (FaceSize && VisualHeight > 0.0f)
	{
		FaceSize->SetHeightOverride(VisualHeight);
	}

	UpdateFaceBrush(true);
}

void UBuildingEntryButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateFaceBrush(false);
}

void UBuildingEntryButtonWidget::NativeOnPressed()
{
	Super::NativeOnPressed();
	UpdateFaceBrush(false);
}

void UBuildingEntryButtonWidget::NativeOnReleased()
{
	Super::NativeOnReleased();
	UpdateFaceBrush(false);
}

void UBuildingEntryButtonWidget::NativeOnHovered()
{
	Super::NativeOnHovered();
	UpdateFaceBrush(false);
}

void UBuildingEntryButtonWidget::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	UpdateFaceBrush(false);
}

void UBuildingEntryButtonWidget::UpdateFaceBrush(bool bForce)
{
	if (!Face || !FaceStyleClass)
	{
		return;
	}

	EFaceState CurrentFaceState;
	if (!GetIsEnabled())
	{
		CurrentFaceState = EFaceState::Disabled;
	}
	else if (GetSelected())
	{
		CurrentFaceState = IsPressed() ? EFaceState::SelectedPressed
			: IsHovered() ? EFaceState::SelectedHovered : EFaceState::Selected;
	}
	else
	{
		CurrentFaceState = IsPressed() ? EFaceState::NormalPressed
			: IsHovered() ? EFaceState::NormalHovered : EFaceState::Normal;
	}

	if (!bForce && CurrentFaceState == AppliedFaceState && FaceStyleClass == AppliedFaceStyleClass)
	{
		return;
	}

	const UCommonButtonStyle* FaceStyle = FaceStyleClass.GetDefaultObject();
	if (!FaceStyle)
	{
		return;
	}

	FSlateBrush FaceBrush;
	switch (CurrentFaceState)
	{
	case EFaceState::Normal:
		FaceStyle->GetNormalBaseBrush(FaceBrush);
		break;
	case EFaceState::NormalHovered:
		FaceStyle->GetNormalHoveredBrush(FaceBrush);
		break;
	case EFaceState::NormalPressed:
		FaceStyle->GetNormalPressedBrush(FaceBrush);
		break;
	case EFaceState::Selected:
		FaceStyle->GetSelectedBaseBrush(FaceBrush);
		break;
	case EFaceState::SelectedHovered:
		FaceStyle->GetSelectedHoveredBrush(FaceBrush);
		break;
	case EFaceState::SelectedPressed:
		FaceStyle->GetSelectedPressedBrush(FaceBrush);
		break;
	case EFaceState::Disabled:
		FaceStyle->GetDisabledBrush(FaceBrush);
		break;
	default:
		return;
	}

	Face->SetBrush(FaceBrush);
	AppliedFaceState = CurrentFaceState;
	AppliedFaceStyleClass = FaceStyleClass;
}
