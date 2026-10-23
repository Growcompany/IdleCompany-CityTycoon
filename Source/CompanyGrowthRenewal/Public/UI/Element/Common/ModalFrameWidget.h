#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ModalFrameWidget.generated.h"

class UBorder;
class UCloseButtonWidget;
class UHelpButtonWidget;
class UNamedSlot;
class UPanelHeaderWidget;
class UTexture2D;

// 중앙 모달의 셸·헤더·본문 칸. 활성화와 게임 입력은 소비 패널에 남긴다.
UCLASS(Abstract, meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UModalFrameWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable, Category = "Modal Frame")
	void SetTitle(const FText& InTitle);

	UFUNCTION(BlueprintCallable, Category = "Modal Frame")
	void SetHeaderIcon(UTexture2D* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Modal Frame")
	void SetHeaderIconResource(UObject* InResource);

	UFUNCTION(BlueprintCallable, Category = "Modal Frame")
	void SetHelpKey(FName InHelpKey);

	UFUNCTION(BlueprintCallable, Category = "Modal Frame")
	void SetAppearance(FName InThemeKey, FName InHeaderPatternKey, FName InSideGradientKey);

	UFUNCTION(BlueprintCallable, Category = "Modal Frame")
	void SetCloseButtonVisible(bool bVisible);

	// 조회용이다. 제목·아이콘·도움말·외형 변경은 Frame의 setter로 전달해야 재구성 때도 유지된다.
	UFUNCTION(BlueprintPure, Category = "Modal Frame")
	UPanelHeaderWidget* GetHeader() const;

	UFUNCTION(BlueprintPure, Category = "Modal Frame")
	UCloseButtonWidget* GetCloseButton() const;

	UFUNCTION(BlueprintPure, Category = "Modal Frame")
	UHelpButtonWidget* GetHelpButton() const;

protected:
	virtual void NativeConstruct() override;

	// 제목 밴드는 **레이아웃 높이의 분수**로 넘긴다. 머티리얼의 fwidth 는 스크린 px 라, DPI 스케일(UIScaleCurve)
	// 이나 디자이너 줌이 바뀌면 같은 px 가 위젯 기준 다른 높이가 되어 선이 헤더 경계에서 떨어진다.
	// 레이아웃 높이는 지오메트리가 생긴 뒤에만 알 수 있어 페인트에서 크기가 바뀔 때만 주입한다(틱은 꺼져 있다).
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// 분리선 두께. 레이아웃 px 로 저작하고 셸 높이로 나눠 분수로 넘긴다(배율 불변).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame|Appearance", meta = (ClampMin = "0"))
	float BandLineThicknessPx = 2.0f;

	// ZeroVector = 미주입. 테마 재적용 때 비워 셸 분수와 문양 경계를 다시 계산한다.
	mutable FVector2D CachedShellSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> HeaderIconResource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame")
	FName HelpKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame")
	bool bShowCloseButton = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame|Appearance")
	FName ThemeKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame|Appearance")
	FName HeaderPatternKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame|Appearance")
	FName SideGradientKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modal Frame|Appearance")
	bool bExtendHeaderPatternToTop = false;

	// 제 셸 그림을 프레임 아래 까는 창(출석판)만 켠다 — 셸 면만 안 그리고 패딩·헤더·본문 배치는 그대로다
	UPROPERTY(EditAnywhere, Category = "Modal Frame|Appearance")
	bool bHideShellSurface = false;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> ShellBorder;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> Body;

private:
	void ApplyPresentation();
	void ApplyAppearance();
};
