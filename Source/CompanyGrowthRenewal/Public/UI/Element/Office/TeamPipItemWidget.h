// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TeamPipItemWidget.generated.h"

class UCommonTextBlock;
class UImage;

// 내 팀 직능 1항목 = 이름 + 핍 5개. 켜진 핍 색은 SetPips 호출자가 넘기는 통과/미달 색, 꺼진 핍은 PipOffColor(잉크 14%) — WBP 브러시는 흰색 고정.
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UTeamPipItemWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	// OnColor = 슬롯 통과/미달 색(호출자 주입). 켜진 핍만 이 색, 꺼진 핍은 PipOffColor
	void SetPips(const FText& Name, int32 FilledPips, const FLinearColor& OnColor);
protected:
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* Text_Name;
	UPROPERTY(meta = (BindWidget)) UImage* Pip1;
	UPROPERTY(meta = (BindWidget)) UImage* Pip2;
	UPROPERTY(meta = (BindWidget)) UImage* Pip3;
	UPROPERTY(meta = (BindWidget)) UImage* Pip4;
	UPROPERTY(meta = (BindWidget)) UImage* Pip5;
	UPROPERTY(EditAnywhere, Category = "Style|Color") FLinearColor PipOffColor = FLinearColor(0.030f, 0.012f, 0.003f, 0.14f);
};
