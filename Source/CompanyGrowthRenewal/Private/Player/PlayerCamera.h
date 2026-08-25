#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Enum/CompanyType.h"
#include "PlayerCamera.generated.h"

// Components
class USphereComponent;
class USpringArmComponent;
class UCameraComponent;
class UFloatingPawnMovement;

// Handlers
class UMovementInputHandler;
class UBuilderHandler;
class UInteractableInputHandler;
class UPlacementHandler;
class UFocusOcclusionHandler;

// Others
class AMainMapPlayerController;
class UInputMappingContext;
class UInputAction;
class AInteractableBaseActor;

struct FBuildableCardTable;

DECLARE_LOG_CATEGORY_EXTERN(InputKey, Log, All);

/**
 * 포커스 글라이드가 "눈으로" 도착했는가.
 *
 * ⚠ 시간 기준이 주(主)다 ― 지수 보간(speed 3.0)에서 완료율은 초기 거리와 무관하게 시간만의 함수라
 * (0.6s = 83%), 절대 거리 임계는 같은 연출이 거리에 따라 1.4~1.8초로 들쭉날쭉해진다.
 * 거리 임계는 짧은 이동이 0.6초를 기다리지 않게 하는 보조 탈출로다.
 */
inline bool ShouldTreatFocusAsSettled(
    bool bTransitioning, float GlideElapsedSeconds, float SettleSeconds,
    float LocationDist, float LocationTolerance,
    float ZoomDiff, float ZoomTolerance)
{
    if (!bTransitioning)
    {
        return true;
    }
    if (GlideElapsedSeconds >= SettleSeconds)
    {
        return true;
    }
    return LocationDist <= LocationTolerance && ZoomDiff <= ZoomTolerance;
}

UCLASS()
class COMPANYGROWTHRENEWAL_API APlayerCamera : public APawn
{
    GENERATED_BODY()

public:
    APlayerCamera();

    UFUNCTION()
    void OnChangedInputType();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    UFUNCTION()
    void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

public:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> CameraComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UFloatingPawnMovement> MovementComponent;

	// Handler
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UMovementInputHandler> MovementInputHandler;

    UPROPERTY(EditAnywhere)
    TObjectPtr<UInteractableInputHandler> InteractableInputHandler = nullptr;

    UPROPERTY(EditAnywhere)
    TObjectPtr<UPlacementHandler> PlacementHandler = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UFocusOcclusionHandler> FocusOcclusionHandler;

    // SoftMove
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USphereComponent> Collision;

    AMainMapPlayerController* GetPlayerController();

public:
    virtual void Tick(float DeltaTime) override;

    //build
    // TargetPlotId: 시작 부지 시드. 현재 호출부는 전부 None 이고 제약은 bPlotBuildSession 동적 판정이 쥔다(이전 경로만 SetPendingPlotId 직접 사용).
    void BeginBuild(const FBuildableCardTable& buildableInfo, ECompanyType CompanyType = ECompanyType::None, FName TargetPlotId = NAME_None);
    void EndBuild();

    //building click
    void StartBuildingRelocation(AInteractableBaseActor* ExistingBuilding);
    void EndBuildingRelocation();

    // 건물 스킨 패널을 위한 카메라 프레이밍
    // ScreenHeightRatioScale: 건물 화면 점유 비율 배수 (1보다 작으면 카메라가 덜 줌인됨)
    UFUNCTION(BlueprintCallable, Category = "Camera")
    // 반환값 = 최종 카메라 팔길이(arm). 영향권 줌과 비교하는 데 사용.
    float FocusOnBuilding(class ABuildingBaseActor* Building, float TopPadding = 50.f, float BottomPadding = 50.f, float ScreenCenterRatioX = 0.5f, float ScreenHeightRatioScale = 1.0f);

    // 건물 클릭했을 때 위젯으로 들어가졌을 때 (카메라 포커싱 + 입력 복원)
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void FocusOnBuildingForPlacement(class ABuildingBaseActor* Building, float TopPadding = 50.f, float BottomPadding = 500.f, float ScreenHeightRatioScale = 1.0f);

    // 카메라 전환 중단
    void StopCameraTransition();

    // 포커스 종료 — 가림 고스트 해제. 패널 NativeOnDeactivated 에서 GoToNormalMode 와 쌍으로 호출할 것.
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ClearFocusTarget();

    // 카메라를 움직이지 않고 가림 판정만 재등록 — 자식 모달에서 돌아온 패널이 NativeOnActivated 에서 고스트를 되살릴 때
    // (FocusOn* 를 다시 부르면 카메라가 또 글라이드한다). ClearFocusTarget 의 짝.
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void RestoreFocusTarget(AActor* Target);

    // 특정 위치로 카메라 이동
    UFUNCTION(BlueprintCallable, Category = "Camera")
    virtual void FocusOnLocation(const FVector& TargetLocation, float DesiredDistance = 0.f);

    // 범용 Actor 포커스 (바운딩 박스 기반 화면 비율 계산)
    // bTrackOcclusion=false 면 가림 고스트를 켜지 않는다(로딩 뒤 초기 포커스·배치 모드용).
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void FocusOnActor(AActor* Actor, float ScreenCenterRatioX = 0.5f, bool bTrackOcclusion = true);

    // 키스톤 영향권 등 넓은 영역을 한 화면에 — MaxZoomDistance를 임시 확장해 기존 줌 트랜지션으로 멀리 빠짐.
    void FocusOnAreaRadius(const FVector& Center, float WorldRadiusCm);
    void RestoreZoomRange();

    // 건물 층수 비례 포커싱 기본 + 영향권 반경이 더 줌아웃을 요구하면 반경이 다 보이게 더 넓게(둘 중 큰 줌).
    void FocusOnBuildingOrAura(class ABuildingBaseActor* Building, float TopPadding, float BottomPadding, float ScreenCenterRatioX, float AuraRadiusCm);

    // 포커스 글라이드 진행 여부. 도착 델리게이트가 없어 폴링으로 읽는다 ―
    // 플레이어 입력이 StopCameraTransition() 으로 글라이드를 취소할 수 있어 "완료" 통지가 안 오는 경로가 있다
    bool IsTransitioningCamera() const { return bIsTransitioningCamera; }

    // 눈으로 도착했는가. 완료 플래그의 임계(0.1uu / 0.001)는 지수 보간과 만나 시각적 도착보다
    // 2~3초 늦게 떨어져, 그 사이를 기다리면 튜토리얼 설명이 타임아웃으로 날아간다
    bool IsFocusVisuallySettled(float LocationTolerance = 50.f, float ZoomTolerance = 0.01f) const;

protected:
    // 글라이드 시작 단일 지점 — 새 진입점이 생겨도 경과 리셋을 빠뜨리지 않게 여기로 모은다.
    // 리셋을 한 곳이라도 빠뜨리면 이전 글라이드의 경과가 남아 다음 포커스가 즉시 "도착" 판정된다
    void BeginCameraTransition();

    // 카메라 전환 상태
    bool bIsTransitioningCamera = false;
    FVector TargetCameraLocation;
    float TargetZoomValue; // SpringArmLength 대신 ZoomValue 보간
    float CameraTransitionSpeed = 3.0f;

    // 현재 글라이드 누적 경과(초). 벽시계 ― 보간 상한(1/30)이 아니라 실제 DeltaTime 을 더한다
    float FocusGlideElapsed = 0.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Focus")
    float FocusVisualSettleSeconds = 0.6f;

    // 넓은 영역 포커스 동안 임시 확장된 MaxZoomDistance 원복용 (-1 = 미저장)
    float SavedMaxZoomDistance = -1.f;
};