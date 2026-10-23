#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DockPanelBorderWidget.generated.h"

class UBorder;
class UMaterialInstanceDynamic;
class UNamedSlot;
class UPanelHeaderWidget;

// Shared right-dock shell. The consuming panel owns its header and content layout.
UCLASS(Abstract, meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UDockPanelBorderWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dock Panel|Appearance", meta = (ClampMin = "0.01"))
	float RimThicknessPx = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dock Panel|Appearance", meta = (ClampMin = "0.01"))
	float SeparatorThicknessPx = 2.0f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> CommonBorder_48;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> Content;

private:
	float ResolveSeparatorHeightPx(const FGeometry& ShellGeometry, const UPanelHeaderWidget* Header) const;

	mutable TWeakObjectPtr<UMaterialInstanceDynamic> CachedMaterial;
	mutable FVector2D CachedSize = FVector2D::ZeroVector;
	mutable float CachedSeparatorHeightPx = -1.0f;
	mutable float CachedRimThicknessPx = -1.0f;
	mutable float CachedSeparatorThicknessPx = -1.0f;
};
