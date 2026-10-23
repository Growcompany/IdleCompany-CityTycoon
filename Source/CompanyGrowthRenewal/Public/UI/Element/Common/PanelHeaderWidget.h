#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Layout/Margin.h"
#include "PanelHeaderWidget.generated.h"

class UCloseButtonWidget;
class UCommonTextBlock;
class UHelpButtonWidget;
class UImage;
class UOverlaySlot;
class UPanelWidget;
class UTexture2D;

// 제목과 헤더 장식을 공유한다. 닫기 처리와 입력 모드는 이 부품을 품는 패널이 소유한다.
UCLASS(Abstract, meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UPanelHeaderWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable, Category = "Panel Header")
	void SetTitle(const FText& InTitle);

	UFUNCTION(BlueprintCallable, Category = "Panel Header")
	void SetHeaderIcon(UTexture2D* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Panel Header")
	void SetHeaderIconResource(UObject* InResource);

	UFUNCTION(BlueprintCallable, Category = "Panel Header")
	void SetHelpKey(FName InHelpKey);

	UFUNCTION(BlueprintCallable, Category = "Panel Header")
	void SetAppearance(FName InThemeKey, FName InHeaderPatternKey, FName InSideGradientKey);

	// 배경 문양만 셸의 위쪽 여백으로 연장한다. 제목과 유틸리티의 레이아웃은 보존한다.
	void SetPatternTopExtension(float InExtensionPx);

	UFUNCTION(BlueprintCallable, Category = "Panel Header")
	void SetCloseButtonVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Panel Header")
	UCloseButtonWidget* GetCloseButton() const;

	UFUNCTION(BlueprintPure, Category = "Panel Header")
	UHelpButtonWidget* GetHelpButton() const;

	// 셸 제목 밴드가 이 값에서 유도된다. 주입 시점(SynchronizeProperties/디자인 타임)에는 아직 레이아웃이
	// 없어 지오메트리가 0 이고, 0 이면 밴드가 조용히 사라지는데 값 단언은 통과한다. 그래서 저작값을 권위로 둔다.
	// (검증은 지오메트리로 가능하다 — PanelLayout 캡처는 실인스턴스를 3패스 드로 후 GetCachedGeometry 를 읽는다.)
	UFUNCTION(BlueprintPure, Category = "Panel Header")
	float GetHeaderHeightPx() const { return HeaderHeightPx; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header")
	FText Title;

	// 기존 텍스처 아이콘과 그라데이션 재질 글리프를 같은 브러시 규격으로 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> HeaderIconResource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header")
	FName HelpKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header")
	bool bShowCloseButton = true;

	// 현재 공용 헤더의 실제 렌더 높이. `HeaderSize` SizeBox 의 MinDesiredHeight(112)는
	// bOverride 가 꺼져 있어 죽은 값이라 쓸 수 없다 — 실측 84 를 저작값으로 들고 있는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header", meta = (ClampMin = "0"))
	float HeaderHeightPx = 84.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header|Appearance")
	FName ThemeKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header|Appearance")
	FName HeaderPatternKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel Header|Appearance")
	FName SideGradientKey;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HeaderIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHelpButtonWidget> HelpButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> UIE_CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HeaderPatternLeft;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HeaderPatternRight;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeaderPatternBackdropLeft;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeaderPatternBackdropRight;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> DecorationSizeNeutral;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeaderWatermark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeaderDepthOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SideGradientLeft;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SideGradientRight;

private:
	void ApplyIdentity();
	void ApplyAppearance();
	void ApplyUtilityPadding();

	TWeakObjectPtr<UOverlaySlot> CachedHeaderRowSlot;
	FMargin AuthoredHeaderRowPadding;
	TWeakObjectPtr<UOverlaySlot> CachedDecorationSlot;
	FMargin AuthoredDecorationPadding;
	float PatternTopExtensionPx = 0.0f;

	bool bCapturedAppearance = false;
	bool bHasCellPatternTexture = false;
	FSlateColor AuthoredTitleColor;
	FVector2D AuthoredTitleShadowOffset = FVector2D::ZeroVector;
	FLinearColor AuthoredTitleShadowColor = FLinearColor::Transparent;
	FLinearColor AuthoredWatermarkTint = FLinearColor::White;
	FLinearColor AuthoredBackdropTints[2];
	TArray<TWeakObjectPtr<UImage>> PatternImages;
	TArray<FLinearColor> AuthoredPatternTints;
	FLinearColor AuthoredGradientTints[2];
};
