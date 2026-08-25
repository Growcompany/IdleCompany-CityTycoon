#include "UI/Element/Cards/IconCardWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Materials/MaterialInstanceDynamic.h"

UIconCardWidget::UIconCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UIconCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	UpdateVisuals();
}

void UIconCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UButtonWidget* Button = GetButtonWidget();
	if (Button)
	{
		Button->OnClicked().AddUObject(this, &UIconCardWidget::HandleButtonClicked);
	}

	InitializeGlowMaterial();
	UpdateVisuals();
}

void UIconCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bEnableGlow && bIsSelected && GlowMaterial)
	{
		UpdatePulse(InDeltaTime);
	}
}

void UIconCardWidget::SetDisplayInfo(UTexture2D* Icon, const FText& InDisplayText)
{
	CachedIcon = Icon;
	CachedDisplayText = InDisplayText;
	UpdateVisuals();
}

void UIconCardWidget::SetIcon(UTexture2D* Icon)
{
	CachedIcon = Icon;
	UImage* Image = GetImageWidget();
	if (Image && Icon)
	{
		Image->SetBrushFromTexture(Icon);
	}
}

void UIconCardWidget::SetDisplayText(const FText& InDisplayText)
{
	CachedDisplayText = InDisplayText;
	UTextBlock* Text = GetTextWidget();
	if (Text)
	{
		Text->SetText(InDisplayText);
	}
}

void UIconCardWidget::SetTextVisible(bool bVisible)
{
	if (UTextBlock* Text = GetTextWidget())
	{
		Text->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UIconCardWidget::SetSelected(bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;

	UBorder* Border = GetSelectionBorderWidget();
	if (Border)
	{
		// Glow 비활성화 시 항상 숨김
		bool bShowBorder = bEnableGlow && bIsSelected;
		Border->SetVisibility(bShowBorder ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!bIsSelected && GlowMaterial)
	{
		PulseTime = 0.0f;
		GlowMaterial->SetScalarParameterValue(TEXT("Glow Size"), PulseMinSize);
	}
}

void UIconCardWidget::HandleButtonClicked()
{
	OnClicked.Broadcast();
}

void UIconCardWidget::InitializeGlowMaterial()
{
	UBorder* Border = GetSelectionBorderWidget();
	if (!Border || bMaterialInitialized)
	{
		return;
	}

	UObject* Resource = Border->Background.GetResourceObject();

	if (UMaterialInterface* BaseMaterial = Cast<UMaterialInterface>(Resource))
	{
		GlowMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);

		if (GlowMaterial)
		{
			FSlateBrush NewBrush = Border->Background;
			NewBrush.SetResourceObject(GlowMaterial);
			Border->SetBrush(NewBrush);

			GlowMaterial->SetScalarParameterValue(TEXT("Glow Size"), PulseMinSize);
			GlowMaterial->SetVectorParameterValue(TEXT("Glow Color"), GlowColor);

			bMaterialInitialized = true;
		}
	}
}

void UIconCardWidget::UpdatePulse(float DeltaTime)
{
	PulseTime += DeltaTime;

	float SinValue = (FMath::Sin(PulseTime * PulseSpeed) + 1.0f) * 0.5f;

	float CurrentGlowSize = FMath::Lerp(PulseMinSize, PulseMaxSize, SinValue);
	GlowMaterial->SetScalarParameterValue(TEXT("Glow Size"), CurrentGlowSize);
}

void UIconCardWidget::UpdateVisuals()
{
	UImage* Image = GetImageWidget();
	if (Image && CachedIcon)
	{
		Image->SetBrushFromTexture(CachedIcon);
	}

	UTextBlock* Text = GetTextWidget();
	if (Text && !CachedDisplayText.IsEmpty())
	{
		Text->SetText(CachedDisplayText);
	}

	UBorder* Border = GetSelectionBorderWidget();
	if (Border)
	{
		bool bShowBorder = bEnableGlow && bIsSelected;
		Border->SetVisibility(bShowBorder ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

#if WITH_EDITOR
void UIconCardWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UIconCardWidget, GlowColor))
	{
		if (GlowMaterial)
		{
			GlowMaterial->SetVectorParameterValue(TEXT("Glow Color"), GlowColor);
		}
	}
}
#endif
