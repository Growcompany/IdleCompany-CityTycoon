#include "UI/Element/Common/DockPanelBorderWidget.h"

#include "Components/Border.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Common/PanelHeaderWidget.h"

namespace
{
	void FindVisiblePanelHeaders(UWidget* Candidate, UPanelHeaderWidget*& FoundHeader, int32& HeaderCount)
	{
		if (!Candidate || Candidate->GetVisibility() == ESlateVisibility::Collapsed || HeaderCount > 1)
		{
			return;
		}

		if (UPanelHeaderWidget* Header = Cast<UPanelHeaderWidget>(Candidate))
		{
			FoundHeader = Header;
			++HeaderCount;
			return;
		}

		// A child UserWidget owns its own WidgetTree; do not pick a card's nested header.
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Candidate))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				FindVisiblePanelHeaders(Panel->GetChildAt(Index), FoundHeader, HeaderCount);
			}
		}
	}
}

float UDockPanelBorderWidget::ResolveSeparatorHeightPx(const FGeometry& ShellGeometry, const UPanelHeaderWidget* Header) const
{
	if (!Header || !Content || !CommonBorder_48)
	{
		return 0.0f;
	}

	const UVerticalBox* RootColumn = Cast<UVerticalBox>(Content->GetContent());
	const UVerticalBoxSlot* HeaderBoxSlot = Cast<UVerticalBoxSlot>(Header->Slot);
	if (RootColumn && RootColumn->GetChildrenCount() > 0 && RootColumn->GetChildAt(0) == Header && HeaderBoxSlot)
	{
		const FMargin HeaderPadding = HeaderBoxSlot->GetPadding();
		return CommonBorder_48->GetPadding().Top + HeaderPadding.Top
			+ Header->GetHeaderHeightPx() + HeaderPadding.Bottom;
	}

	const FGeometry& HeaderGeometry = Header->GetCachedGeometry();
	if (ShellGeometry.GetLocalSize().Y <= UE_KINDA_SMALL_NUMBER ||
		HeaderGeometry.GetLocalSize().Y <= UE_KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector2D HeaderBottomAbsolute = HeaderGeometry.LocalToAbsolute(FVector2D(0.0f, HeaderGeometry.GetLocalSize().Y));
	const float SlotBottom = HeaderBoxSlot ? HeaderBoxSlot->GetPadding().Bottom : 0.0f;
	return ShellGeometry.AbsoluteToLocal(HeaderBottomAbsolute).Y + SlotBottom;
}

int32 UDockPanelBorderWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (CommonBorder_48 && Content)
	{
		const FVector2D ShellSize = AllottedGeometry.GetLocalSize();
		const float ShortSide = FMath::Min(ShellSize.X, ShellSize.Y);
		if (ShortSide > UE_KINDA_SMALL_NUMBER && ShellSize.Y > UE_KINDA_SMALL_NUMBER)
		{
			UPanelHeaderWidget* Header = nullptr;
			int32 HeaderCount = 0;
			FindVisiblePanelHeaders(Content->GetContent(), Header, HeaderCount);
			const float SeparatorHeightPx = HeaderCount == 1
				? FMath::Clamp(ResolveSeparatorHeightPx(CommonBorder_48->GetCachedGeometry(), Header), 0.0f, ShellSize.Y)
				: 0.0f;

			if (UMaterialInstanceDynamic* BorderMaterial = CommonBorder_48->GetDynamicMaterial())
			{
				if (CachedMaterial.Get() != BorderMaterial || !CachedSize.Equals(ShellSize) ||
					!FMath::IsNearlyEqual(CachedSeparatorHeightPx, SeparatorHeightPx) ||
					!FMath::IsNearlyEqual(CachedRimThicknessPx, RimThicknessPx) ||
					!FMath::IsNearlyEqual(CachedSeparatorThicknessPx, SeparatorThicknessPx))
				{
					BorderMaterial->SetScalarParameterValue(TEXT("BorderThickness"), 2.0f * RimThicknessPx / ShortSide);
					BorderMaterial->SetScalarParameterValue(TEXT("BandHeightFrac"), SeparatorHeightPx / ShellSize.Y);
					BorderMaterial->SetScalarParameterValue(TEXT("BandLineFrac"), SeparatorHeightPx > 0.0f
						? SeparatorThicknessPx / ShellSize.Y : 0.0f);
					BorderMaterial->SetScalarParameterValue(TEXT("BandOpacity"), 0.0f);
					BorderMaterial->SetScalarParameterValue(TEXT("BandHeightPx"), 0.0f);

					CachedMaterial = BorderMaterial;
					CachedSize = ShellSize;
					CachedSeparatorHeightPx = SeparatorHeightPx;
					CachedRimThicknessPx = RimThicknessPx;
					CachedSeparatorThicknessPx = SeparatorThicknessPx;
				}
			}
		}
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}
