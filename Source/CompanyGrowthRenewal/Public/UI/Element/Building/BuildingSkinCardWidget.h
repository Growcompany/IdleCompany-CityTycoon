// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Table/BuildingSkinData.h"
#include "BuildingSkinCardWidget.generated.h"

class UBorder;
class UImage;
class USkinInfoWidget;

/**
 * Widget representing a single building skin card
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingSkinCardWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 카드에 스킨 데이터 설정
	UFUNCTION(BlueprintCallable, Category = "Skin")
	void SetSkinData(const FBuildingSkinData& InSkinData, bool bIsUnlocked);

	// 카드의 스킨 ID 가져오기
	UFUNCTION(BlueprintPure, Category = "Skin")
	int32 GetSkinID() const { return SkinData.SkinID; }

	// 스킨 잠금 해제 여부 가져오기
	UFUNCTION(BlueprintPure, Category = "Skin")
	bool IsUnlocked() const { return bUnlocked; }

	// 잠금 상태 설정 (메뉴 해금 등 외부에서 LockBorder 제어)
	UFUNCTION(BlueprintCallable, Category = "Skin")
	void SetLocked(bool bIsLocked);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* EntityImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	USkinInfoWidget* UIE_SkinInfo;

	// 에디터 미리보기용 기본값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	TSoftObjectPtr<UTexture2D> PreviewImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	FVector2D PreviewImageSize = FVector2D(64.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	FText PreviewSkinName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	FLinearColor PreviewBackgroundColor = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* LockBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* LockImage;

	// 현재 스킨 데이터
	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	FBuildingSkinData SkinData;

	// 스킨 잠금 해제 여부
	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	bool bUnlocked = false;

	// SetSkinData가 호출되었는지 여부
	bool bSkinDataSet = false;
};
