#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputTypeManager.generated.h"

// �Է� Ÿ�� ������
UENUM(BlueprintType)
enum class EInputType : uint8
{
    Unknown UMETA(DisplayName = "Unknown"),
    KeyMouse UMETA(DisplayName = "KeyMouse"),
    GamePad UMETA(DisplayName = "GamePad"),
    Touch UMETA(DisplayName = "Touch")
};

/**
 * InputTypeManager Ŭ������ ���� �Է� Ÿ���� �����մϴ�.
 */
UCLASS(Blueprintable)
class COMPANYGROWTHRENEWAL_API UInputTypeManager : public UObject
{
    GENERATED_BODY()

    public:
    // �⺻ ������
    UInputTypeManager();

    UFUNCTION(BlueprintCallable, Category = "Input")
    EInputType GetValue() const { return CurrentInputType; }

    UFUNCTION(BlueprintCallable, Category = "Input")
    void SetValue(EInputType NewInputType) { CurrentInputType = NewInputType; }

    // 플랫폼 기반 입력 타입 자동 감지 (Static 메서드)
    UFUNCTION(BlueprintPure, Category = "Input")
    static EInputType GetPlatformInputType();

private:
    // ���� �Է� Ÿ��
    UPROPERTY(VisibleAnywhere, Category = "Input")
    EInputType CurrentInputType;
};
