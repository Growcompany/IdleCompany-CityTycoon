// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "Enum/LootBoxRarity.h"
#include "Table/LootBoxData.h"
#include "LootBoxActor.generated.h"

// 에디터 프리뷰용 상태
UENUM(BlueprintType)
enum class ELootBoxPreviewState : uint8
{
	Closed UMETA(DisplayName = "Closed"),
	Opened UMETA(DisplayName = "Opened")
};

class UStaticMeshComponent;
class UNiagaraComponent;
class UAudioComponent;
class UTableManagerSubsystem;

// 애니메이션 완료 시 호출되는 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnLootBoxAnimationFinished);

// 루트 박스 액터
// 기능:
// - 플레이어 상호작용 시 열림 애니메이션 재생
// - 등급(Rarity)과 타입(Square/Sphere)별로 다른 비주얼 표현
// - 흔들림 → 메시 교체 → 떠오름 → 아이템 스폰 순서로 애니메이션
UCLASS()
class COMPANYGROWTHRENEWAL_API ALootBoxActor : public AActor
{
	GENERATED_BODY()

public:
	ALootBoxActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// ===== 컴포넌트 =====

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootScene;

	// 닫힌 상자 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ClosedMeshComponent;

	// 열린 상자 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* OpenedMeshComponent;

	// 닫힌 상태 VFX (대기 중 파티클)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* ClosedVFXComponent;

	// 열리는 순간 VFX
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* OpenVFXComponent;

	// 열린 후 지속 VFX
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* OpenedVFXComponent;

	// 기둥 효과 VFX (선택적)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* PillarVFXComponent;

	// 사운드 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* AudioComponent;

	// 보상 프리뷰용 Sphere (열릴 때 띄워질 구체)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RewardSphereComponent;

	// Sphere를 밝게 비추는 Spot Light
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USpotLightComponent* RewardLightComponent;

	// ===== 설정 =====

	// 루트 박스 등급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Box")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 루트 박스 타입 (Square/Sphere)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Box")
	ELootBoxType ChestType = ELootBoxType::Square;

	// DataTable Row Name (예: "Square_Epic", "Sphere_Legendary")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Box")
	FName LootBoxRowName = NAME_None;

	// 에디터 프리뷰 상태 (닫힘/열림)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Box|Preview")
	ELootBoxPreviewState PreviewState = ELootBoxPreviewState::Closed;

	// 에디터에서 LootBoxRowName 변경 시 자동으로 프리뷰 업데이트
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// LootBox 비주얼 변경 (선택 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Loot Box")
	void SetLootBoxVisual(FName LootBoxID);

	// ===== 상호작용 =====

	// 상자 열기 (외부에서 호출) - RewardSkinID를 전달받음
	UFUNCTION(BlueprintCallable, Category = "Loot Box")
	void OpenLootBox(int32 RewardSkinID);

	// 애니메이션 완료 델리게이트
	FOnLootBoxAnimationFinished OnAnimationFinished;

	// 이미 열렸는지 여부
	UPROPERTY(BlueprintReadOnly, Category = "Loot Box")
	bool bIsOpened = false;

	// 애니메이션 완료 후 입력 대기 중
	UPROPERTY(BlueprintReadOnly, Category = "Loot Box")
	bool bWaitingForInput = false;

	// 보상 확인 완료 (사용자가 클릭함)
	UFUNCTION(BlueprintCallable, Category = "Loot Box")
	void CompleteReward();

	// ===== Reward Animation Curves =====

	// Reward 위치 애니메이션 커브 (Z축 상승)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UCurveFloat* PositionCurve;

	// Reward 크기 애니메이션 커브 (Scale)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UCurveFloat* ScaleCurve;

	// Reward 투명도 애니메이션 커브 (Alpha/Opacity)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UCurveFloat* AlphaCurve;

protected:
	// 현재 설정 캐시
	FLootBoxTable CurrentConfig;

	// TableManager 참조
	UTableManagerSubsystem* TableManager = nullptr;

	// 현재 보상 SkinID (Material 적용용)
	int32 CurrentRewardSkinID = 0;

	// 애니메이션 상태
	bool bIsAnimating = false;
	float AnimationTime = 0.0f;

	// DataTable에서 설정 로드 및 메시/VFX 적용
	void ApplyVisualConfig();

	// 에디터 전용: CurrentConfig 직접 적용
	void ApplyVisualConfigDirect();

	// 애니메이션 업데이트 (Tick에서 호출)
	void UpdateAnimation(float DeltaTime);

	// 흔들림 애니메이션
	void UpdateShakeAnimation(float Progress);

	// 떠오름 애니메이션
	void UpdateRiseAnimation(float Progress);

	// Reward Sphere 애니메이션 업데이트
	void UpdateRewardAnimation(float DeltaTime);

	// 애니메이션 완료 시 호출
	UFUNCTION(BlueprintNativeEvent, Category = "Loot Box")
	void OnOpenAnimationFinished();
	virtual void OnOpenAnimationFinished_Implementation();
};
