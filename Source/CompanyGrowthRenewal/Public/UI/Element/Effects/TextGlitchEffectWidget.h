// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TextGlitchEffectWidget.generated.h"

class UTextBlock;
class URetainerBox;

/**
 * 글리치 이펙트가 적용된 텍스트 위젯
 * RetainerBox에 글리치 머티리얼을 적용하여 텍스트에 효과 부여
 * 블루프린트에서 디자인하고 C++에서 텍스트 설정
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTextGlitchEffectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 글리치 텍스트 설정
	UFUNCTION(BlueprintCallable, Category = "Text Glitch Effect")
	void SetText(const FText& InText);

	// FString 오버로드
	UFUNCTION(BlueprintCallable, Category = "Text Glitch Effect")
	void SetTextFromString(const FString& InText);

	// 현재 텍스트 가져오기
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Text Glitch Effect")
	FText GetText() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	// 에디터에서 설정 가능한 기본 텍스트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Glitch Effect")
	FText DefaultText;

	// 블루프린트의 TextBlock_30과 바인딩 (Optional로 유연하게 처리)
	// 블루프린트에서 위젯 이름을 GlitchText로 변경하면 자동 바인딩됨
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* GlitchText;

	// RetainerBox 바인딩 (글리치 머티리얼이 적용된 컨테이너)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	URetainerBox* GlitchRetainerBox;

private:
	// 기본값을 위젯에 적용
	void ApplyDefaultText();
};
