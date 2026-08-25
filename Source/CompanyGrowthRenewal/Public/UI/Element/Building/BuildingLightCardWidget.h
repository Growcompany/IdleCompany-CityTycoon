// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Table/BuildingLightData.h"
#include "BuildingLightCardWidget.generated.h"

class UBorder;
class UImage;
class USkinInfoWidget;

/**
 * Widget representing a single building light card
 * - EntityImage에 EmissiveColor를 단색 tint로 적용 (텍스처 아님)
 * - SkinInfoWidget을 재활용해 이름+희귀도 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingLightCardWidget : public UCommonButtonBase
{
    GENERATED_BODY()

public:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintCallable, Category = "Light")
    void SetLightData(const FBuildingLightData& InLightData, bool bIsUnlocked);

    UFUNCTION(BlueprintPure, Category = "Light")
    int32 GetLightID() const { return LightData.LightID; }

    UFUNCTION(BlueprintPure, Category = "Light")
    bool IsUnlocked() const { return bUnlocked; }

    UFUNCTION(BlueprintCallable, Category = "Light")
    void SetLocked(bool bIsLocked);

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UImage* EntityImage;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    USkinInfoWidget* UIE_SkinInfo;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UBorder* LockBorder;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* LockImage;

    // 에디터 미리보기용 기본값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    FLinearColor PreviewColor = FLinearColor(1.0f, 0.902f, 0.408f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    FText PreviewLightName;

    UPROPERTY(BlueprintReadOnly, Category = "Light")
    FBuildingLightData LightData;

    UPROPERTY(BlueprintReadOnly, Category = "Light")
    bool bUnlocked = false;

    bool bLightDataSet = false;
};
