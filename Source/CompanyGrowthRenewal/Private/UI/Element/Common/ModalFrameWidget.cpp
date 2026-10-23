#include "UI/Element/Common/ModalFrameWidget.h"

#include "Components/Border.h"
#include "Components/NamedSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Common/PanelHeaderWidget.h"

void UModalFrameWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyPresentation();
}

void UModalFrameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetHelpKey(HelpKey);
}

void UModalFrameWidget::SetTitle(const FText& InTitle)
{
	Title = InTitle;
	if (PanelHeader) { PanelHeader->SetTitle(Title); }
}

void UModalFrameWidget::SetHeaderIcon(UTexture2D* InTexture)
{
	SetHeaderIconResource(InTexture);
}

void UModalFrameWidget::SetHeaderIconResource(UObject* InResource)
{
	HeaderIconResource = InResource;
	if (PanelHeader) { PanelHeader->SetHeaderIconResource(HeaderIconResource); }
}

void UModalFrameWidget::SetHelpKey(FName InHelpKey)
{
	HelpKey = InHelpKey;
	if (PanelHeader) { PanelHeader->SetHelpKey(HelpKey); }
}

void UModalFrameWidget::SetAppearance(FName InThemeKey, FName InHeaderPatternKey, FName InSideGradientKey)
{
	ThemeKey = InThemeKey;
	HeaderPatternKey = InHeaderPatternKey;
	SideGradientKey = InSideGradientKey;
	ApplyAppearance();
}

void UModalFrameWidget::SetCloseButtonVisible(bool bVisible)
{
	bShowCloseButton = bVisible;
	if (PanelHeader) { PanelHeader->SetCloseButtonVisible(bShowCloseButton); }
}

UPanelHeaderWidget* UModalFrameWidget::GetHeader() const
{
	return PanelHeader;
}

UCloseButtonWidget* UModalFrameWidget::GetCloseButton() const
{
	return PanelHeader ? PanelHeader->GetCloseButton() : nullptr;
}

UHelpButtonWidget* UModalFrameWidget::GetHelpButton() const
{
	return PanelHeader ? PanelHeader->GetHelpButton() : nullptr;
}

void UModalFrameWidget::ApplyPresentation()
{
	SetTitle(Title);
	SetHeaderIconResource(HeaderIconResource);
	SetHelpKey(HelpKey);
	SetCloseButtonVisible(bShowCloseButton);
	ApplyAppearance();
}

void UModalFrameWidget::ApplyAppearance()
{
	if (PanelHeader)
	{
		PanelHeader->SetPatternTopExtension(0.0f);
		PanelHeader->SetAppearance(ThemeKey, HeaderPatternKey, SideGradientKey);
	}
	if (!ShellBorder) { return; }
	if (bHideShellSurface)
	{
		// 셸 머티리얼을 안 그리니 MID 도 만들지 않는다 — 테마 주입과 밴드 분수(NativePaint)도 같이 건너뛴다
		FSlateBrush HiddenSurface = ShellBorder->Background;
		HiddenSurface.DrawAs = ESlateBrushDrawType::NoDrawType;
		ShellBorder->SetBrush(HiddenSurface);
		return;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UTableManagerSubsystem* Tables = IsDesignTime() ? GetDefault<UTableManagerSubsystem>()
		: (GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr);
	FPanelThemeRow Theme;
	const bool bHasTheme = !ThemeKey.IsNone() && Tables && Tables->GetPanelThemeRow(ThemeKey, Theme);
	// 테마가 없을 때도 MID 가 필요하다 — 제목 밴드는 테마가 아니라 프레임 계약이 정하기 때문이다.
	// (구 코드는 테마 없으면 브러시 리소스를 MID 로 캐스팅했는데, 그건 MaterialInstanceConstant 라
	//  항상 null 이었다 = 테마 없는 패널에는 주입 경로가 아예 없었다.)
	UMaterialInstanceDynamic* ShellMaterial = ShellBorder->GetDynamicMaterial();
	if (!ShellMaterial) { return; }
	if (bHasTheme)
	{
		ShellMaterial->SetVectorParameterValue(TEXT("FillTop"), Theme.ShellFillTop);
		ShellMaterial->SetVectorParameterValue(TEXT("FillBottom"), Theme.ShellFillBottom);
		ShellMaterial->SetVectorParameterValue(TEXT("BorderColor"), Theme.ShellBorderColor);
		ShellMaterial->SetVectorParameterValue(TEXT("LipColor"), Theme.ShellLipColor);
	}
	else
	{
		ShellMaterial->ClearParameterValues();
	}
	ShellMaterial->SetScalarParameterValue(TEXT("BandOpacity"), 0.0f);
	// 밴드 분수는 레이아웃 높이가 필요해 NativePaint 가 넣는다. ClearParameterValues() 가 지웠으므로
	// 캐시를 비워 다음 페인트에서 재주입시킨다.
	CachedShellSize = FVector2D::ZeroVector;
}

int32 UModalFrameWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// 셸의 레이아웃 높이는 한 프레임 전 값이다. 첫 페인트에는 0 이라 밴드가 한 프레임 비는데,
	// 0 을 주입하면 밴드가 영구히 사라지므로 그쪽이 낫다.
	if (ShellBorder && !bHideShellSurface)
	{
		const FVector2D ShellSize = ShellBorder->GetCachedGeometry().GetLocalSize();
		const float ShellHeight = ShellSize.Y;
		if (ShellSize.GetMin() > UE_KINDA_SMALL_NUMBER && !ShellSize.Equals(CachedShellSize))
		{
			if (UMaterialInstanceDynamic* ShellMaterial = ShellBorder->GetDynamicMaterial())
			{
				CachedShellSize = ShellSize;
				if (PanelHeader && bExtendHeaderPatternToTop)
				{
					// 셸 SDF의 짧은 축은 [-1,1]이다. 그림자 여백과 림 안쪽까지만 장식을 연장한다.
					const float ShellInset = ShellMaterial->K2_GetScalarParameterValue(TEXT("ShadowMargin"))
						+ ShellMaterial->K2_GetScalarParameterValue(TEXT("OuterThickness"))
						+ ShellMaterial->K2_GetScalarParameterValue(TEXT("BorderThickness"))
						+ ShellMaterial->K2_GetScalarParameterValue(TEXT("InnerThickness"));
					const float PatternInsetPx = FMath::Max(0.0f, ShellInset) * ShellSize.GetMin() * 0.5f + 2.0f;
					PanelHeader->SetPatternTopExtension(FMath::Max(0.0f, ShellBorder->GetPadding().Top - PatternInsetPx));
				}
				// 계약은 그대로 「셸 상단 패딩 + 헤더 높이」다. 단위만 스크린 px -> 셸 높이의 분수로 바꿨다.
				const float BandPx = ShellBorder->GetPadding().Top
					+ (PanelHeader ? PanelHeader->GetHeaderHeightPx() : 0.0f);
				ShellMaterial->SetScalarParameterValue(TEXT("BandHeightFrac"), BandPx / ShellHeight);
				ShellMaterial->SetScalarParameterValue(TEXT("BandLineFrac"), BandLineThicknessPx / ShellHeight);
			}
		}
	}
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}
