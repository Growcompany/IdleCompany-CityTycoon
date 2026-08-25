// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientSplineMover.generated.h"

class USplineComponent;
class UStaticMeshComponent;

// 강 위 배 등 배경 요소를 스플라인 따라 천천히 루프 이동 + 룩어헤드 조향(각속도 제한) + 미세 보빙시키는
// 레벨 드레싱 액터. 메시/머티리얼/스케일은 에디터에서 MeshComp 에 지정(정적 액터 승격 시 계승).
UCLASS()
class COMPANYGROWTHRENEWAL_API AAmbientSplineMover : public AActor
{
	GENERATED_BODY()

public:
	AAmbientSplineMover();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Ambient")
	USceneComponent* Root;

	// 에디터에서 강 따라 작도
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient")
	USplineComponent* Path;

	// 배 메시 (에디터에서 SM_container_ship_001 + override M_Color + scale 2 지정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient")
	UStaticMeshComponent* MeshComp;

	// 이동 속도 (cm/s) — 은은함 위해 느리게
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient", meta = (ClampMin = "0.0"))
	float Speed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	bool bLoop = true;

	// 스플라인 시작 거리 오프셋 (여러 척 분산용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient", meta = (ClampMin = "0.0"))
	float StartDistance = 0.f;

	// 메시 forward축을 진행방향에 정렬 (배가 옆으로 가면 Yaw 90 보정). 스무딩 후 적용되는 렌더 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	FRotator MeshYawOffset = FRotator::ZeroRotator;

	// 상하 보빙 진폭 (cm) — 수면에 뜬 느낌
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Bob", meta = (ClampMin = "0.0"))
	float BobAmplitude = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Bob", meta = (ClampMin = "0.01"))
	float BobPeriod = 4.f;

	// 좌우 아이들 롤 진폭 (deg) — 뱅킹과 별개의 잔잔한 흔들림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Bob", meta = (ClampMin = "0.0"))
	float RollAmplitude = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Bob", meta = (ClampMin = "0.01"))
	float RollPeriod = 5.f;

	// 룩어헤드 거리 (cm) — 앞쪽 이 지점을 바라보며 선회. 클수록 코너를 미리 보고 완만해짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Steering", meta = (ClampMin = "0.0"))
	float LookAheadDistance = 3500.f;

	// 최대 선회 각속도 (deg/s) — 낮을수록 무거운 배 (scale-2 컨테이너선 45~60 권장)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Steering", meta = (ClampMin = "1.0"))
	float MaxYawRate = 45.f;

	// 선회 시 안쪽으로 기우는 뱅킹 롤 각 (deg)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Steering", meta = (ClampMin = "0.0"))
	float BankAmplitude = 4.f;

private:
	float DistanceAlongSpline = 0.f;
	float SplineLength = 0.f;
	float ElapsedTime = 0.f;
	float CurrentYaw = 0.f;   // 스무딩된 헤딩 (프레임 간 유지)
	float PreviousYaw = 0.f;  // 뱅킹 롤 계산용 직전 헤딩

	void UpdateTransform(float DeltaTime);
};
