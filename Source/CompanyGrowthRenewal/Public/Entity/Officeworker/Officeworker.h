// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/EmployeeTypes.h"
#include "Data/CharacterAppearanceTypes.h"  // transitive 소비자 호환 (구 모듈러 외형 타입)
#include "Enum/BubbleType.h"
#include "Officeworker.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UEmployeeBehaviorComponent;
class AWorkstationActorBase;
class UWidgetComponent;
class UMaterialInterface;

/**
 * 직원 공유 게임플레이 베이스 (외형 무관).
 * Behavior/버블/글로우/초상화/배회 AI/워크스테이션/선택을 담당.
 * 외형은 자식이 구현: AModularOfficeworker(구 17파츠 모듈러) / AStickOfficeworker(스틱맨 코스메틱).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeworker : public ACharacter
{
	GENERATED_BODY()

public:
	// Behavior Component (상태/피로도/수익 관리)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Behavior")
	UEmployeeBehaviorComponent* BehaviorComponent;

	// 머리 위 상태 버블 (3D→Screen 자동 변환)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bubble")
	UWidgetComponent* BubbleWidgetComponent = nullptr;

	// 머리 위 상시 피로 게이지 바 (무드 버블과 별개 — 무드는 None 시 숨지만 이건 항상 표시)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fatigue")
	UWidgetComponent* FatigueBarComponent = nullptr;

	// 피로 바 화면 픽셀 크기(Screen-space DrawSize) — WBP가 아니라 이 값이 실제 크기 권위. BP 디테일에서 조절 → OnConstruction 재적용.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fatigue", meta = (ClampMin = "1.0"))
	FVector2D FatigueBarDrawSize = FVector2D(200.f, 32.f);

	// 피로 바 머리 위 높이(캡슐 상대 Z) — 서 있을 때. 바가 커질수록 머리와 안 겹치게 올림.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fatigue", meta = (ClampMin = "0.0"))
	float FatigueBarHeightZ = 128.f;

	// 착석 시 높이 — 캡슐은 그대로인데 머리만 내려가서(Head 본 실측: Idle +56 → Typing +13) 서 있을 때 값을 쓰면 붕 뜬다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fatigue", meta = (ClampMin = "0.0"))
	float FatigueBarSeatedHeightZ = 85.f;

	// 앉기/일어서기 전환 때 바가 순간이동하지 않게 하는 보간 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fatigue", meta = (ClampMin = "0.1"))
	float FatigueBarHeightInterpSpeed = 8.f;

	// 이모트 매핑 DataAsset (에디터에서 BP_DefaultOfficeworker 같은 자식 BP에 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bubble")
	TSoftObjectPtr<class UWorkerBubbleConfig> BubbleConfig;

	/** 직원 상태 버블 갱신 (None이면 숨김) */
	UFUNCTION(BlueprintCallable, Category = "Bubble")
	void SetWorkerBubbleType(EWorkerBubbleType NewType);

	UFUNCTION(BlueprintPure, Category = "Bubble")
	EWorkerBubbleType GetCurrentBubbleType() const { return CurrentBubbleType; }

	/** 메시 Overlay 글로우 (이벤트 버프 활성 시 ON) — 베이스는 no-op, 시각은 자식이 override */
	UFUNCTION(BlueprintCallable, Category = "Buff")
	virtual void SetGlowOverlay(bool bEnabled, FLinearColor Color = FLinearColor(1.f, 0.85f, 0.2f, 1.f));

private:
	EWorkerBubbleType CurrentBubbleType = EWorkerBubbleType::None;

	// 버블/Glow 평가 1초 게이트 누산기 — 워커별 독립이어야 함 (static 공유 시 N배 빨리 터지는 버그)
	float BubbleCheckAccum = 0.0f;

	// 착석 여부에 맞춰 피로 바 Z 를 목표값으로 보간 (정착 후에는 매 프레임 no-op)
	void UpdateFatigueBarHeight(float DeltaTime);

	float CurrentFatigueBarZ = 0.f;
	bool bFatigueBarZSettled = true;

	UPROPERTY()
	UMaterialInterface* GlowOverlayMaterial = nullptr;

public:

	// ABP 연동 Getter
	UFUNCTION(BlueprintPure, Category = "Animation")
	EEmployeeState GetEmployeeState() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	bool IsSeated() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	bool IsWalking() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetSpeed() const;

public:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	// Portrait 캡처 완료 델리게이트
	DECLARE_DELEGATE_OneParam(FOnPortraitCaptured, const FString& /*EmployeeID*/);
	FOnPortraitCaptured OnPortraitCaptured;

	// 메시 로드 완료 델리게이트
	DECLARE_DELEGATE(FOnMeshLoadCompleted);
	FOnMeshLoadCompleted OnMeshLoadCompleted;

	// Portrait 촬영 전용 모드 (레벨에 배치된 촬영용 캐릭터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	bool bIsPortraitMode = false;

	// ON = 촬영 직전 카메라를 바디 헤드에 바운드 기준으로 자동 정렬(스케일/오프셋 무관, 안전망).
	// OFF = BP/RT 에서 직접 잡은 카메라 위치·거리·FOV 를 그대로 존중. 직접 프레이밍을 튜닝하려면 OFF.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	bool bAutoFramePortraitCamera = false;

	// Portrait 캡처 시스템
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portrait")
	USceneCaptureComponent2D* FaceCaptureCamera;

	UFUNCTION()
	void SetupFaceCapture();

	UFUNCTION()
	void CaptureAndSaveFacePortrait();

	UFUNCTION()
	FString GetPortraitFilePath() const;

private:
	FTimerHandle MovementTimer;

	// 직원 ID (deterministic 신발 선택용)
	int32 EmployeeID = -1;

protected:
	// Sets default values for this pawn's properties
	AOfficeworker();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void LoadAssetsAsync() PURE_VIRTUAL(AOfficeworker::LoadAssetsAsync, return;)
	virtual void OnAssetsLoaded() PURE_VIRTUAL(AOfficeworker::OnAssetsLoaded, return;)
	// 자식 클래스에서 성별 반환 (순수 가상 함수)
	virtual EEmployeeGender GetCharacterGender() const PURE_VIRTUAL(AOfficeworker::GetCharacterGender, return EEmployeeGender::Male;)

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	// EmployeeID 설정 (deterministic 신발 선택용)
	UFUNCTION()
	void SetEmployeeID(int32 InEmployeeID) { EmployeeID = InEmployeeID; }

	UFUNCTION()
	int32 GetEmployeeID() const { return EmployeeID; }

	// 캐릭터 중심 위치 반환 (카메라 포커싱용)
	UFUNCTION(BlueprintCallable, Category = "Character")
	FVector GetCenterLocation() const;

	// ========== 랜덤 배회 AI ==========

	// 랜덤 배회 시작
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StartRandomRoaming(float InRoamRadius = 500.f, float InMinWaitTime = 2.f, float InMaxWaitTime = 5.f);

	// 랜덤 배회 중지
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StopRandomRoaming();

	// 현재 배회 중인지 확인
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool IsRoaming() const { return bIsRoaming; }

	// 선택 표시 — 베이스는 상태만 관리, 시각(오버레이)은 자식이 override (모듈러=파츠 오버레이)
	UFUNCTION()
	virtual void SetSelected(bool bSelected);

	UFUNCTION()
	bool IsSelected() const { return bIsSelected; }

	// ========== Workstation 할당 ==========

	// 할당된 Workstation 설정
	UFUNCTION(BlueprintCallable, Category = "AI|Workstation")
	void SetAssignedWorkstation(AWorkstationActorBase* Workstation, int32 SeatIndex);

	// Workstation으로 순간이동 후 앉기 (Stage 모드용)
	// @return 성공 여부 (Workstation이 할당되지 않으면 false)
	UFUNCTION(BlueprintCallable, Category = "AI|Workstation")
	bool TeleportToWorkstationAndSit();

	// Workstation 근처로 이동 (Operation 모드용)
	UFUNCTION(BlueprintCallable, Category = "AI|Workstation")
	void MoveToWorkstationArea();

	// Workstation에서 일어나기
	UFUNCTION(BlueprintCallable, Category = "AI|Workstation")
	void StandUpFromWorkstation();

	// 할당된 Workstation 반환
	UFUNCTION(BlueprintPure, Category = "AI|Workstation")
	AWorkstationActorBase* GetAssignedWorkstation() const { return AssignedWorkstation.Get(); }

	// 할당된 좌석 인덱스 반환
	UFUNCTION(BlueprintPure, Category = "AI|Workstation")
	int32 GetAssignedSeatIndex() const { return AssignedSeatIndex; }

protected:
	// 할당된 Workstation
	UPROPERTY()
	TWeakObjectPtr<AWorkstationActorBase> AssignedWorkstation;

	// 할당된 좌석 인덱스
	UPROPERTY()
	int32 AssignedSeatIndex = -1;

	// 선택 상태
	bool bIsSelected = false;

	// ========== 랜덤 배회 상태 ==========
	bool bIsRoaming = false;
	float RoamRadius = 500.f;
	float MinWaitTime = 2.f;
	float MaxWaitTime = 5.f;
	FVector RoamOrigin;  // 배회 시작 지점 (이 주변에서 배회)
	FTimerHandle RoamTimerHandle;

	// 다음 랜덤 위치로 이동
	void MoveToRandomLocation();

	// 이동 완료 후 호출
	void OnMoveCompleted();

	// Overlay Material (Constructor에서 로드)
	UPROPERTY()
	UMaterialInterface* SelectionOverlayMaterial;
};
