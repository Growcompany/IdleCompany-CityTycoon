// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TeamPipItemWidget.generated.h"

class UCommonTextBlock;
class UImage;

// 내 팀 직능 1항목 = 이름 + 핍 5개. 핍 색은 Style|Color 노브(기본 켜짐 #3D7FC0 / 꺼짐 잉크 14%, WBP Class Defaults 로 덮어쓰기 가능)를 C++ 가 SetColorAndOpacity 로 주입 — WBP 브러시는 흰색 고정.
UCLASS()
class COMPANYGROWTHRENEWAL_API UTeamPipItemWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	void SetPips(const FText& Name, int32 FilledPips);
protected:
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* Text_Name;
	UPROPERTY(meta = (BindWidget)) UImage* Pip1;
	UPROPERTY(meta = (BindWidget)) UImage* Pip2;
	UPROPERTY(meta = (BindWidget)) UImage* Pip3;
	UPROPERTY(meta = (BindWidget)) UImage* Pip4;
	UPROPERTY(meta = (BindWidget)) UImage* Pip5;
	UPROPERTY(EditAnywhere, Category = "Style|Color") FLinearColor PipOnColor = FLinearColor(0.047f, 0.212f, 0.522f, 1.f);   // #3D7FC0 linear
	UPROPERTY(EditAnywhere, Category = "Style|Color") FLinearColor PipOffColor = FLinearColor(0.030f, 0.012f, 0.003f, 0.14f);
};
