#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/InputTypeManager.h"
#include "MainMapPlayerController.generated.h"

UENUM()
enum class EInputMode
{
    Normal,        
    BuildPlace,
    BuildingClick,
    Factory,    
    UI            
};

class APlayerCamera;
struct FInteractableInfo;

UCLASS()
class COMPANYGROWTHRENEWAL_API AMainMapPlayerController : public APlayerController
{
    GENERATED_BODY()

private:
    UPROPERTY()
    EInputMode CurrentInputMode = EInputMode::Normal;

public:
    AMainMapPlayerController();

    // Input Mode 관리
    UFUNCTION(BlueprintCallable)
    void SetGameInputMode(EInputMode NewMode);
    
    // 모드별 시작/종료
    UFUNCTION(BlueprintCallable)
    void GoToNormalMode();
    void GoToUIMode();
    void GoToBuildPlaceMode();
    void GoToFactoryMode();

    EInputMode GetCurrentInputMode() const { return CurrentInputMode; }
    EInputType GetCurrentInputType() const { return UInputTypeManager::GetPlatformInputType(); }

public:
    // 터치 이벤트 핸들러
    UFUNCTION()
    void OnTouchPressed(ETouchIndex::Type FingerIndex, FVector Location);

    UFUNCTION()
    void OnTouchReleased(ETouchIndex::Type FingerIndex, FVector Location);

    UFUNCTION()
    void OnTouchMoved(ETouchIndex::Type FingerIndex, FVector Location);

private:
    // 터치 상태 추적
    float TouchStartTime = 0.0f;
    FVector2D TouchStartLocation = FVector2D::ZeroVector;
    bool bIsTouchActive = false;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Destroyed() override;
    virtual void OnPossess(APawn* InPawn) override;

private:
    UPROPERTY()
    TObjectPtr<APlayerCamera> CameraPawn = nullptr;
};
