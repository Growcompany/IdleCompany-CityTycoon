#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "MoneyVFXTable.generated.h"

USTRUCT(BlueprintType)
struct FMoneyVFXTable : public FTableRowBase
{
    GENERATED_BODY()

    // 이 VFX가 적용되는 최소 금액 (이상)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
    int32 MinAmount = 0;

    // 이 VFX가 적용되는 최대 금액 (미만, 0이면 무제한)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
    int32 MaxAmount = 0;

    // 재생할 Niagara VFX
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    TSoftObjectPtr<UNiagaraSystem> MoneyVFX;

    // VFX 스케일
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    float VFXScale = 1.0f;

    // 재생할 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    TSoftObjectPtr<USoundBase> MoneySound;

    // 사운드 볼륨
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    float SoundVolume = 1.0f;

    FMoneyVFXTable()
        : MinAmount(0)
        , MaxAmount(0)
        , MoneyVFX(nullptr)
        , VFXScale(1.0f)
        , MoneySound(nullptr)
        , SoundVolume(1.0f)
    {}
};
