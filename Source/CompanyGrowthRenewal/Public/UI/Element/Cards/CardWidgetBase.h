// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "CardWidgetBase.generated.h"

class UImage;
class UTexture2D;
class UEntityCardFrameWidget;

/**
 * 카드 위젯 베이스 클래스 (공통 기능만 제공)
 * - BuildEntityCardWidget, EmployeeCardWidget 등의 부모
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCardWidgetBase : public UCommonButtonBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual void OnButtonClicked();
	void OnButtonHovered();
	void OnButtonUnhovered();

	// 카드 프레임 (EntityImage, LockBorder 등 포함)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UEntityCardFrameWidget* CardFrame;

	// 레거시 지원: 프레임 없이 직접 바인딩된 경우
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* EntityImage;

public:
	// 아이콘 로드 완료 콜백
	UFUNCTION()
	void OnIconLoaded(FSoftObjectPath LoadedPath);

	// 이미지 위젯 가져오기 (Frame 또는 직접 바인딩)
	UImage* GetEntityImage() const;
};

