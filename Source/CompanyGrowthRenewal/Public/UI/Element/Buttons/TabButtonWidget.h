// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TabButtonWidget.generated.h"

class UButtonWidget;
class UCommonButtonBase;

/**
 * 탭 전용 합성 래퍼 위젯
 * Overlay(ClipToBounds) + 음수 Padding 마스킹과 디자인 기본값을 한 곳에 캡슐화.
 * 외부에서는 GetButton() 으로 내부 버튼을 꺼내 ButtonGroup 에 등록한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTabButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Tab")
	UCommonButtonBase* GetButton() const;

	UFUNCTION(BlueprintCallable, Category = "Tab")
	void SetTabText(const FText& NewText);

protected:
	// WBP 안에서 정확히 "TabButton" 이름으로 배치된 ButtonWidget 인스턴스
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButtonWidget* TabButton;

	UPROPERTY(EditAnywhere, Category = "Tab")
	FText Text;

	// #2F2F2F (선택 = 다크 글씨, 밝은 선택 탭 #E4E7EC 위)
	UPROPERTY(EditAnywhere, Category = "Tab")
	FSlateColor SelectedTextColor = FSlateColor(FLinearColor(0.028426f, 0.028426f, 0.028426f, 1.0f));

	// #8B92A0 (미선택 = 뮤트 쿨그레이)
	UPROPERTY(EditAnywhere, Category = "Tab")
	FSlateColor NormalTextColor = FSlateColor(FLinearColor(0.258183f, 0.287441f, 0.351533f, 1.0f));

	// 자식 버튼 최소 크기 — 인스턴스마다 패널 사이즈에 맞춰 오버라이드 가능
	UPROPERTY(EditAnywhere, Category = "Tab|Size", meta = (ClampMin = "0"))
	int32 MinWidth = 200;

	UPROPERTY(EditAnywhere, Category = "Tab|Size", meta = (ClampMin = "0"))
	int32 MinHeight = 150;

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleSelectedChanged(bool bIsSelected);

private:
	void ApplySelectedColor(bool bIsSelected);
};
