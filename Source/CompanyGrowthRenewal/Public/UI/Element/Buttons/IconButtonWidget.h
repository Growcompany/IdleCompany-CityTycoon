// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "UI/Interface/ButtonSoundInterface.h"
#include "IconButtonWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UTexture2D;

/**
 * 아이콘과 텍스트를 표시하는 버튼 위젯
 * - 버튼, 룩박스, 아이템 등에 사용
 * - IButtonSoundInterface 상속으로 자동 사운드 재생
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UIconButtonWidget : public UCommonButtonBase, public IButtonSoundInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	UTexture2D* IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FVector2D IconSize = FVector2D(96.0f, 96.0f);

	// 버튼 텍스트 (블루프린트에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	FText Text;

	// 아이콘 이미지
	UPROPERTY(EditAnywhere, meta = (BindWidgetOptional))
	UImage* IconImage;

	// 버튼 텍스트 위젯 (내부 바인딩용)
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* ButtonText;

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;

public:
	// 아이콘 동적 설정
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIcon(UTexture2D* NewIcon);

	// 아이콘 비동기 로드 (TSoftObjectPtr용)
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIconFromSoft(TSoftObjectPtr<UTexture2D> SoftIcon);

	// 버튼 텍스트 설정 (범용)
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetButtonText(const FText& NewText);

	// 보유 개수 설정 (내부적으로 SetButtonText 사용)
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetCount(int32 Count);

	// 보유 개수 가져오기
	UFUNCTION(BlueprintCallable, Category = "Icon")
	int32 GetCount() const { return CurrentCount; }

	// 선택 상태 설정 (UCommonButtonBase 내장 기능 사용)
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetSelected(bool bInSelected);

private:
	void ApplyIconSettings();

	// 현재 표시 중인 개수
	int32 CurrentCount = 0;
};
