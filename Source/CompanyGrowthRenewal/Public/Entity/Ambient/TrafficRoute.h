// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficRoute.generated.h"

class USplineComponent;

// 도로 위에 드롭해 스플라인을 작도하는 경량 경로 액터. ATrafficManager 가 레벨의 모든 ATrafficRoute 를
// 모아 그 위에 인스턴싱 차량을 주행시킨다. 자체 틱/메시 없음(순수 경로 데이터).
UCLASS()
class COMPANYGROWTHRENEWAL_API ATrafficRoute : public AActor
{
	GENERATED_BODY()

public:
	ATrafficRoute();

	USplineComponent* GetSpline() const { return Path; }

	// 이 경로 차량이 스플라인을 역방향으로 주행(반대 차선용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic")
	bool bReverse = false;

	// 이 경로에 배정할 차량 상대 가중치(클수록 더 많이 배정). 0 이하면 길이 비례.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float CarWeight = 0.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Traffic")
	USceneComponent* Root;

	// 에디터에서 도로 따라 작도
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	USplineComponent* Path;
};
