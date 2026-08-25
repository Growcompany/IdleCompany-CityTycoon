// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BrickCollectWidget.generated.h"

class UCanvasPanelSlot;

/**
 * 벽돌 수집 연출 — 2페이즈 (분출: 콘 방향 물리 포물선 → 흡수: 베지어 EaseIn 가속 도착)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBrickCollectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// BurstDelayExtra = 배치 내 스태거(분출 페이즈 길이에 가산 → 전원 동시에 튀고 흡수만 순차)
	// InLandSoundVolume = 착지음 볼륨 — 배치 간 감쇠 상태는 InGameLayerWidget 이 들고 여기로 내려줌 (인스턴스 휘발이라 자체 보관 불가)
	UFUNCTION(BlueprintCallable, Category = "Collect")
	void InitCollect(const FVector2D& InStartPos, const FVector2D& InTargetPos, UCanvasPanelSlot* InSlot, int32 InCollectAmount, float InBurstDelayExtra, bool bInIsLastOfBatch, int32 InBatchTotal, float InLandSoundVolume = 1.0f);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaSeconds) override;

private:
	enum class ECollectPhase : uint8
	{
		Burst,
		Homing
	};

	void BeginHoming();
	void FinishCollect();

	// ===== 분출 페이즈 노브 (무거운 벽돌 톤 — 낮고 짧은 포물선, 임의로 키우지 말 것) =====

	UPROPERTY(EditAnywhere, Category = "Collect|Burst")
	float BurstConeHalfAngleDeg = 25.f;

	UPROPERTY(EditAnywhere, Category = "Collect|Burst")
	float BurstSpeedMin = 260.f;

	UPROPERTY(EditAnywhere, Category = "Collect|Burst")
	float BurstSpeedMax = 420.f;

	UPROPERTY(EditAnywhere, Category = "Collect|Burst")
	float Gravity = 2200.f;

	UPROPERTY(EditAnywhere, Category = "Collect|Burst")
	float BurstDuration = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Collect|Burst")
	float SpawnPopDuration = 0.12f;

	// 등장 시작 스케일 — 낮을수록 "작게 태어나서 커지는" 인상이 강해짐
	UPROPERTY(EditAnywhere, Category = "Collect|Burst")
	float SpawnStartScale = 0.8f;

	// ===== 흡수 페이즈 노브 =====

	UPROPERTY(EditAnywhere, Category = "Collect|Homing")
	float HomingDuration = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Collect|Homing")
	float HomingEaseExp = 3.0f;

	// 컨트롤 포인트를 분출 관성 방향으로 미는 거리 — 페이즈 전환이 꺾이지 않게
	UPROPERTY(EditAnywhere, Category = "Collect|Homing")
	float HomingCurveLead = 40.f;

	UPROPERTY(EditAnywhere, Category = "Collect|Homing")
	float ArriveScale = 0.85f;

	// ===== 회전 노브 (스핀 거의 없음 — 각도 지터로 정적 다양성만) =====

	UPROPERTY(EditAnywhere, Category = "Collect|Spin")
	float InitialAngleJitterDeg = 10.f;

	UPROPERTY(EditAnywhere, Category = "Collect|Spin")
	float SpinSpeedMaxDeg = 12.f;

	// ===== 런타임 상태 =====

	UPROPERTY()
	UCanvasPanelSlot* CanvasSlot = nullptr;

	ECollectPhase Phase = ECollectPhase::Burst;
	float ElapsedTime = 0.f;

	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	FVector2D TargetPos = FVector2D::ZeroVector;

	// 흡수 베지어 P0/P1 (P2 = TargetPos)
	FVector2D HomingStart = FVector2D::ZeroVector;
	FVector2D HomingControl = FVector2D::ZeroVector;

	float CurrentAngle = 0.f;
	float SpinSpeed = 0.f;

	float BurstDelayExtra = 0.f;
	int32 CollectAmount = 1;
	bool bIsLastOfBatch = true;
	int32 BatchTotal = 1;
	float LandSoundVolume = 1.f;
};
