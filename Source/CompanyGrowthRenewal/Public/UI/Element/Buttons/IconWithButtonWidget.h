// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "IconWithButtonWidget.generated.h"

class UImage;
class UTexture2D;
class UCommonTextBlock;
class UAlertMarkWidget;

/**
 * 아이콘을 포함하는 버튼 위젯
 * - ButtonWidget을 상속받아 아이콘 기능 추가
 * - 에디터에서 아이콘 텍스처, 크기, 패딩 설정 가능
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UIconWithButtonWidget : public UButtonWidget
{
	GENERATED_BODY()

protected:
	// 아이콘 이미지 위젯 (블루프린트에서 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* IconImage;

	// 수량 텍스트 (아이콘과 버튼 텍스트 사이, Optional)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CountText;

	// 수량 기본값 (에디터에서 설정 가능, 0이면 표시 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	int32 Count = 0;

	// 아이콘 텍스처 (에디터에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	UTexture2D* IconTexture;

	// 아이콘 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FVector2D IconSize = FVector2D(64.0f, 64.0f);

	// 아이콘 패딩 (상하좌우)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FMargin IconPadding = FMargin(0.0f);

	// 아이콘 색상 틴트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FLinearColor IconColor = FLinearColor::White;

	virtual void NativePreConstruct() override;

public:
	// 아이콘 텍스처 설정
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIcon(UTexture2D* NewIcon);

	// 아이콘 크기 설정
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIconSize(FVector2D NewSize);

	// 아이콘 패딩 설정
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIconPadding(FMargin NewPadding);

	// 아이콘 가시성 설정
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIconVisibility(ESlateVisibility InVisibility);

	// 수량 설정 — "x 10" 포맷으로 표시, 0이면 숨김
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetCount(int32 InCount);

	// 수량 텍스트 직접 설정 (커스텀 포맷용)
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetCountText(const FText& InText);

	// 수량 텍스트 가시성 설정
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetCountVisibility(ESlateVisibility InVisibility);

	// 아이콘 색상 틴트 설정
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIconColor(FLinearColor NewColor);

	// ========== 알림 뱃지 ==========

	// UIE_AlertMark 서브위젯 (BindWidgetOptional) — WBP 인스턴스명도 "AlertMark"로 통일
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> AlertMark = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Badge")
	void ShowBadge();

	UFUNCTION(BlueprintCallable, Category = "Badge")
	void HideBadge();

	UFUNCTION(BlueprintPure, Category = "Badge")
	bool IsBadgeVisible() const;

	virtual FGameplayTag GetClickSoundTag() const override;

private:
	// 아이콘 설정 적용 헬퍼 함수
	void ApplyIconSettings();
};
