#include "UI/Element/Buttons/OfficeFeatureButtonWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void UOfficeFeatureButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyFeatureIcon();
}

void UOfficeFeatureButtonWidget::SetFeatureLocked(bool bInLocked, const FText& LevelLabel, const FText& ToastReason)
{
	SetLocked(bInLocked, LevelLabel);
	if (bInLocked)
	{
		SetRejectReason(ToastReason);
	}

	ApplyFeatureIcon();
}

void UOfficeFeatureButtonWidget::ApplyFeatureIcon()
{
	if (!IsValid(IconImage) || !IsValid(FeatureIconMaterial))
	{
		return;
	}

	if (!IsValid(FeatureIconMID) || IconImage->GetBrush().GetResourceObject() != FeatureIconMID)
	{
		SetIconResource(FeatureIconMaterial);
		FeatureIconMID = IconImage->GetDynamicMaterial();
	}

	if (IsValid(FeatureIconMID))
	{
		FeatureIconMID->SetScalarParameterValue(TEXT("Locked"), IsLocked() ? 1.0f : 0.0f);
	}
	if (IsValid(BackingImage) && IsValid(FeatureBackingMaterial))
	{
		if (!IsValid(FeatureBackingMID) || BackingImage->GetBrush().GetResourceObject() != FeatureBackingMID)
		{
			BackingImage->SetBrushFromMaterial(FeatureBackingMaterial);
			FeatureBackingMID = BackingImage->GetDynamicMaterial();
		}
		if (IsValid(FeatureBackingMID))
		{
			FeatureBackingMID->SetScalarParameterValue(TEXT("Locked"), IsLocked() ? 1.0f : 0.0f);
		}
	}
}
