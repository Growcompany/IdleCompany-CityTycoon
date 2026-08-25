// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NiagaraTextEffectWidget.generated.h"

class UNiagaraSystemWidget;
class UCommonTextBlock;

// 이펙트 완료 시 브로드캐스트되는 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEffectFinished);

/**
 * 텍스트 효과 스타일 (6가지)
 */
UENUM(BlueprintType)
enum class ETextEffectStyle : uint8
{
	Style1 = 0,
	Style2,
	Style3,
	Style4,
	Style5,
	Style6
};

/**
 * Niagara 파티클 효과와 함께 텍스트를 애니메이션하는 위젯
 * 색상별로 Widget Blueprint 생성, 스타일은 코드로 선택
 * 화면 중앙에 표시되며, 효과 종료 후 자동으로 제거됨
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UNiagaraTextEffectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이펙트 완료 시 브로드캐스트 (RemoveFromParent 직전)
	UPROPERTY(BlueprintAssignable, Category = "Effect")
	FOnEffectFinished OnEffectFinished;

	// 스타일 설정 (5가지 중 선택)
	UFUNCTION(BlueprintCallable, Category = "Effect")
	void SetStyle(ETextEffectStyle Style);

	// 효과 재생 및 종료 후 자동 제거
	UFUNCTION(BlueprintCallable, Category = "Effect")
	void PlayEffectAndRemove();

	// 텍스트 설정
	UFUNCTION(BlueprintCallable, Category = "Effect")
	void SetEffectText(const FText& InText);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 스타일별 NiagaraWidget 바인딩 (색상은 Blueprint에서 설정)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UNiagaraSystemWidget* NiagaraEffect_Style1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UNiagaraSystemWidget* NiagaraEffect_Style2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UNiagaraSystemWidget* NiagaraEffect_Style3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UNiagaraSystemWidget* NiagaraEffect_Style4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UNiagaraSystemWidget* NiagaraEffect_Style5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UNiagaraSystemWidget* NiagaraEffect_Style6;

	// 텍스트 위젯
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* EffectText;

	// 애니메이션 설정
	UPROPERTY(EditAnywhere, Category = "Animation")
	float EffectDuration = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float TextDelayTime = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float PopAnimDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float FadeOutStartTime = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float FadeOutEndTime = 1.8f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float StartScale = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float OvershootScale = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float EndScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float StartOffsetY = 30.0f;

	// 효과 재생 시 함께 트리거할 SoundID — DT_GameSFX RowName. 색깔별 BP에서 override 가능
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName SoundID = TEXT("TextEffect_Positive");

private:
	// 현재 활성화된 Niagara 위젯
	UPROPERTY()
	UNiagaraSystemWidget* ActiveNiagaraWidget = nullptr;

	// 애니메이션 상태
	bool bIsAnimating = false;
	float ElapsedTime = 0.0f;

	// 모든 Niagara 위젯 배열 반환
	TArray<UNiagaraSystemWidget*> GetAllNiagaraWidgets() const;

	// 이징 함수
	float EaseOutCubic(float Alpha) const;
	float EaseOutBack(float Alpha) const;

	// 애니메이션 업데이트
	void UpdatePopAnimation(float Alpha);
};
