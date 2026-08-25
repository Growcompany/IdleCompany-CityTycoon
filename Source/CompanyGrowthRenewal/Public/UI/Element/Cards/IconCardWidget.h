#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IconCardWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UButtonWidget;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIconCardClicked);

/**
 * 범용 아이콘 카드 위젯 베이스 클래스
 * - 아이콘 이미지와 선택적 텍스트 표시
 * - 선택 시 Glow Pulse 효과
 * - 특화 아이콘 카드의 베이스
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UIconCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UIconCardWidget(const FObjectInitializer& ObjectInitializer);

	// 카드 클릭 이벤트 (파라미터 없음)
	UPROPERTY(BlueprintAssignable, Category = "Card")
	FOnIconCardClicked OnClicked;

	// 표시 정보 설정 (아이콘 + 텍스트)
	UFUNCTION(BlueprintCallable, Category = "Card")
	virtual void SetDisplayInfo(UTexture2D* Icon, const FText& DisplayText);

	// 아이콘만 설정
	UFUNCTION(BlueprintCallable, Category = "Card")
	virtual void SetIcon(UTexture2D* Icon);

	// 텍스트만 설정
	UFUNCTION(BlueprintCallable, Category = "Card")
	virtual void SetDisplayText(const FText& DisplayText);

	// 카드 위 텍스트 표시 토글 — 패널이 이름을 자체 표시할 때 커버 위 중복 제거용
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetTextVisible(bool bVisible);

	// 선택 상태 설정
	UFUNCTION(BlueprintCallable, Category = "Card")
	virtual void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Card")
	bool IsSelected() const { return bIsSelected; }

	// Glow 활성화 설정
	UFUNCTION(BlueprintCallable, Category = "Glow")
	void SetEnableGlow(bool bEnable) { bEnableGlow = bEnable; }

	UFUNCTION(BlueprintPure, Category = "Glow")
	bool IsGlowEnabled() const { return bEnableGlow; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// === Widget 바인딩 (IconCardWidget 블루프린트에서 사용) ===
	// 서브클래스에서는 다른 이름의 위젯을 바인딩하고 getter를 오버라이드하여 사용
	// 이 변수들은 IconCardWidget 블루프린트 직접 사용 시에만 바인딩됨

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButtonWidget* IconButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* IconDisplayText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UBorder* IconSelectionBorder;

	// === Glow 설정 ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glow")
	bool bEnableGlow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glow")
	float PulseSpeed = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glow")
	float PulseMinSize = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glow")
	float PulseMaxSize = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glow")
	FLinearColor GlowColor = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f);

	// 서브클래스에서 접근 가능한 내부 함수들
	void InitializeGlowMaterial();
	void UpdatePulse(float DeltaTime);
	virtual void UpdateVisuals();

	// 버튼 클릭 핸들러 - 서브클래스에서 오버라이드 가능
	UFUNCTION()
	virtual void HandleButtonClicked();

	// 위젯 getter - 서브클래스에서 다른 이름의 위젯 사용 시 오버라이드
	virtual UButtonWidget* GetButtonWidget() const { return IconButton; }
	virtual UImage* GetImageWidget() const { return IconImage; }
	virtual UTextBlock* GetTextWidget() const { return IconDisplayText; }
	virtual UBorder* GetSelectionBorderWidget() const { return IconSelectionBorder; }

	// 캐싱된 데이터
	UPROPERTY()
	UTexture2D* CachedIcon;

	UPROPERTY()
	FText CachedDisplayText;

	UPROPERTY()
	UMaterialInstanceDynamic* GlowMaterial;

	bool bIsSelected = false;
	bool bMaterialInitialized = false;
	float PulseTime = 0.0f;

#if WITH_EDITOR
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
