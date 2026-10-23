#include "UI/Element/Common/PanelHeaderWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/HelpButtonWidget.h"

void UPanelHeaderWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyIdentity();
	ApplyAppearance();
}

void UPanelHeaderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetHelpKey(HelpKey);
}

void UPanelHeaderWidget::SetTitle(const FText& InTitle)
{
	Title = InTitle;
	if (TitleText) { TitleText->SetText(Title); }
}

void UPanelHeaderWidget::SetHeaderIcon(UTexture2D* InTexture)
{
	SetHeaderIconResource(InTexture);
}

void UPanelHeaderWidget::SetHeaderIconResource(UObject* InResource)
{
	HeaderIconResource = InResource;
	if (HeaderIconResource && !HeaderIconResource->IsA<UTexture2D>() && !HeaderIconResource->IsA<UMaterialInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PanelHeader] %s: unsupported icon resource %s"), *GetPathName(), *HeaderIconResource->GetPathName());
		HeaderIconResource = nullptr;
	}
	if (HeaderIcon)
	{
		// 리소스만 교체하여 브러시 크기·Margin·Tiling·틴트를 공용 WBP 값으로 유지한다.
		HeaderIcon->SetBrushResourceObject(HeaderIconResource);
		HeaderIcon->SetVisibility(HeaderIconResource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (HeaderWatermark)
	{
		HeaderWatermark->SetBrushResourceObject(bHasCellPatternTexture ? HeaderIconResource.Get() : nullptr);
		HeaderWatermark->SetVisibility(bHasCellPatternTexture && HeaderIconResource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UPanelHeaderWidget::SetHelpKey(FName InHelpKey)
{
	HelpKey = InHelpKey;
	if (!HelpButton) { ApplyUtilityPadding(); return; }
	if (IsDesignTime())
	{
		// 디자이너에는 플레이어와 도움말 데이터 공급자가 없다. 실제 내용 판정은 Construct에서 수행한다.
		HelpButton->HelpKey = HelpKey;
		HelpButton->SetVisibility(HelpKey.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	else
	{
		HelpButton->SetHelpKey(HelpKey);
	}
	ApplyUtilityPadding();
}

void UPanelHeaderWidget::SetAppearance(FName InThemeKey, FName InHeaderPatternKey, FName InSideGradientKey)
{
	ThemeKey = InThemeKey;
	HeaderPatternKey = InHeaderPatternKey;
	SideGradientKey = InSideGradientKey;
	ApplyAppearance();
}

void UPanelHeaderWidget::SetCloseButtonVisible(bool bVisible)
{
	bShowCloseButton = bVisible;
	if (UIE_CloseButton)
	{
		UIE_CloseButton->SetVisibility(bShowCloseButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	ApplyUtilityPadding();
}

void UPanelHeaderWidget::SetPatternTopExtension(float InExtensionPx)
{
	const float ExtensionPx = FMath::Max(0.0f, InExtensionPx);
	if (FMath::IsNearlyEqual(PatternTopExtensionPx, ExtensionPx)) { return; }
	PatternTopExtensionPx = ExtensionPx;
	ApplyAppearance();
}

UCloseButtonWidget* UPanelHeaderWidget::GetCloseButton() const
{
	return UIE_CloseButton;
}

UHelpButtonWidget* UPanelHeaderWidget::GetHelpButton() const
{
	return HelpButton;
}

void UPanelHeaderWidget::ApplyIdentity()
{
	SetTitle(Title);
	SetHeaderIconResource(HeaderIconResource);
	SetHelpKey(HelpKey);
	SetCloseButtonVisible(bShowCloseButton);
}

void UPanelHeaderWidget::ApplyUtilityPadding()
{
	UPanelWidget* TitleRow = TitleText ? TitleText->GetParent() : nullptr;
	UOverlaySlot* TitleRowSlot = TitleRow ? Cast<UOverlaySlot>(TitleRow->Slot) : nullptr;
	if (!TitleRowSlot) { return; }
	if (CachedHeaderRowSlot.Get() != TitleRowSlot)
	{
		CachedHeaderRowSlot = TitleRowSlot;
		AuthoredHeaderRowPadding = TitleRowSlot->GetPadding();
	}

	// 버튼이 하나라도 자리를 차지하면 공용 WBP의 대칭 여백으로 제목 중앙을 유지한다.
	const bool bHasUtility = (HelpButton && HelpButton->GetVisibility() != ESlateVisibility::Collapsed)
		|| (UIE_CloseButton && UIE_CloseButton->GetVisibility() != ESlateVisibility::Collapsed);
	FMargin TitlePadding = AuthoredHeaderRowPadding;
	if (!bHasUtility)
	{
		TitlePadding.Left = 0.0f;
		TitlePadding.Right = 0.0f;
	}
	TitleRowSlot->SetPadding(TitlePadding);
}

void UPanelHeaderWidget::ApplyAppearance()
{
	if (!TitleText) { return; }
	UImage* Gradients[] = { SideGradientLeft, SideGradientRight };
	UImage* Backdrops[] = { HeaderPatternBackdropLeft, HeaderPatternBackdropRight };
	if (!bCapturedAppearance)
	{
		bCapturedAppearance = true;
		AuthoredTitleColor = TitleText->GetColorAndOpacity();
		AuthoredTitleShadowOffset = TitleText->GetShadowOffset();
		AuthoredTitleShadowColor = TitleText->GetShadowColorAndOpacity();
		AuthoredWatermarkTint = HeaderWatermark ? HeaderWatermark->GetColorAndOpacity() : FLinearColor::White;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Backdrops); ++Index)
		{
			AuthoredBackdropTints[Index] = Backdrops[Index] ? Backdrops[Index]->GetColorAndOpacity() : FLinearColor::White;
		}
		for (UPanelWidget* PatternRoot : { HeaderPatternLeft.Get(), HeaderPatternRight.Get() })
		{
			if (!PatternRoot) { continue; }
			// 공용 WBP의 캔버스가 셀의 위치·크기·알파를 소유한다. 리소스 선택은 배열 형태를 바꾸지 않는다.
			for (UWidget* PatternChild : PatternRoot->GetAllChildren())
			{
				if (UImage* PatternImage = Cast<UImage>(PatternChild))
				{
					PatternImages.Add(PatternImage);
					AuthoredPatternTints.Add(PatternImage->GetColorAndOpacity());
				}
			}
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Gradients); ++Index)
		{
			AuthoredGradientTints[Index] = Gradients[Index] ? Gradients[Index]->GetColorAndOpacity() : FLinearColor::White;
		}
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UTableManagerSubsystem* Tables = IsDesignTime() ? GetDefault<UTableManagerSubsystem>()
		: (GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr);
	FPanelThemeRow Theme;
	const bool bHasTheme = !ThemeKey.IsNone() && Tables && Tables->GetPanelThemeRow(ThemeKey, Theme);
	TitleText->SetColorAndOpacity(bHasTheme ? FSlateColor(Theme.TitleColor) : AuthoredTitleColor);
	const bool bUseTitleShadow = bHasTheme && !Theme.TitleShadowOffset.IsNearlyZero() && Theme.TitleShadowColor.A > 0.0f;
	TitleText->SetShadowOffset(bUseTitleShadow ? Theme.TitleShadowOffset : AuthoredTitleShadowOffset);
	TitleText->SetShadowColorAndOpacity(bUseTitleShadow ? Theme.TitleShadowColor : AuthoredTitleShadowColor);
	if (HeaderDepthOverlay)
	{
		UMaterialInterface* DepthMaterial = nullptr;
		if (bHasTheme && !Theme.HeaderDepthMaterial.IsNull())
		{
			DepthMaterial = Theme.HeaderDepthMaterial.LoadSynchronous();
			if (!DepthMaterial)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PanelHeader] %s: depth material load failed for theme %s"), *GetPathName(), *ThemeKey.ToString());
			}
		}
		HeaderDepthOverlay->SetBrushResourceObject(DepthMaterial);
		HeaderDepthOverlay->SetVisibility(DepthMaterial ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const FName DecorationKeys[] = { HeaderPatternKey, SideGradientKey };
	for (int32 PairIndex = 0; PairIndex < UE_ARRAY_COUNT(DecorationKeys); ++PairIndex)
	{
		const FName DecorationKey = DecorationKeys[PairIndex];
		FUIVFXTexRow TextureRow;
		UTexture2D* Texture = nullptr;
		if (!DecorationKey.IsNone() && Tables && Tables->GetUIVFXTexRow(DecorationKey, TextureRow) && !TextureRow.Texture.IsNull())
		{
			Texture = TextureRow.Texture.LoadSynchronous();
			if (!Texture)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PanelHeader] %s: texture load failed for %s"), *GetPathName(), *DecorationKey.ToString());
			}
		}
		const FLinearColor ThemeTint = bHasTheme ? (PairIndex == 0 ? Theme.PatternTint : Theme.GradientTint) : FLinearColor::White;
		auto ApplyDecoration = [Texture, &TextureRow, &ThemeTint](UImage* Decoration, const FLinearColor& AuthoredTint)
		{
			if (Decoration)
			{
				Decoration->SetBrushResourceObject(Texture);
				Decoration->SetColorAndOpacity(AuthoredTint * ThemeTint * TextureRow.Tint);
				Decoration->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			}
		};
		if (PairIndex == 0)
		{
			const bool bUseBackdrop = Texture && TextureRow.bHeaderBackgroundPattern;
			if (UOverlaySlot* DecorationSlot = DecorationSizeNeutral ? Cast<UOverlaySlot>(DecorationSizeNeutral->Slot) : nullptr)
			{
				if (CachedDecorationSlot.Get() != DecorationSlot)
				{
					CachedDecorationSlot = DecorationSlot;
					AuthoredDecorationPadding = DecorationSlot->GetPadding();
				}
				FMargin DecorationPadding = AuthoredDecorationPadding;
				DecorationPadding.Top -= bUseBackdrop ? PatternTopExtensionPx : 0.0f;
				DecorationSlot->SetPadding(DecorationPadding);
			}
			bHasCellPatternTexture = Texture && !bUseBackdrop;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Backdrops); ++Index)
			{
				UImage* Backdrop = Backdrops[Index];
				if (!Backdrop) { continue; }
				if (bUseBackdrop)
				{
					// WBP가 가진 타일·페이드 재질은 유지하고 행의 텍스처와 반복 크기만 공급한다.
					if (UMaterialInstanceDynamic* PatternMaterial = Backdrop->GetDynamicMaterial())
					{
						PatternMaterial->SetTextureParameterValue(TEXT("PatternTex"), Texture);
						PatternMaterial->SetScalarParameterValue(TEXT("TopFade"), PatternTopExtensionPx > 0.0f ? 0.0f : 1.0f);
						PatternMaterial->SetVectorParameterValue(TEXT("PatternSize"), FLinearColor(
							FMath::Max(1.0f, static_cast<float>(TextureRow.ImageSize.X)),
							FMath::Max(1.0f, static_cast<float>(TextureRow.ImageSize.Y)), 0.0f, 0.0f));
						Backdrop->SetColorAndOpacity(AuthoredBackdropTints[Index] * ThemeTint * TextureRow.Tint);
						Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
					}
					else
					{
						Backdrop->SetVisibility(ESlateVisibility::Collapsed);
					}
				}
				else
				{
					Backdrop->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
			for (UPanelWidget* PatternRoot : { HeaderPatternLeft.Get(), HeaderPatternRight.Get() })
			{
				if (PatternRoot) { PatternRoot->SetVisibility(bHasCellPatternTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
			}
			for (int32 Index = 0; Index < PatternImages.Num(); ++Index)
			{
				if (bUseBackdrop)
				{
					if (UImage* PatternImage = PatternImages[Index].Get())
					{
						PatternImage->SetBrushResourceObject(nullptr);
						PatternImage->SetVisibility(ESlateVisibility::Collapsed);
					}
				}
				else
				{
					ApplyDecoration(PatternImages[Index].Get(), AuthoredPatternTints[Index]);
				}
			}
		}
		else
		{
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Gradients); ++Index)
			{
				ApplyDecoration(Gradients[Index], AuthoredGradientTints[Index]);
			}
		}
	}
	if (HeaderWatermark)
	{
		HeaderWatermark->SetColorAndOpacity(AuthoredWatermarkTint * (bHasTheme ? Theme.PatternTint : FLinearColor::White));
		HeaderWatermark->SetBrushResourceObject(bHasCellPatternTexture ? HeaderIconResource.Get() : nullptr);
		HeaderWatermark->SetVisibility(bHasCellPatternTexture && HeaderIconResource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
