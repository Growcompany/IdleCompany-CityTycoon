// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/Ambient/AmbientSplineMover.h"
#include "AmbientVehicleMover.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

// AAmbientSplineMover 를 도로 차량용으로 특화한 무버.
// - 차량 프리셋: 상하 보빙 / 아이들 롤 0 (차는 안 출렁), 코너 약한 뱅킹.
// - 앞/뒤 발광 라이트카드를 MeshComp 자식으로 부착 → 부모가 MeshComp 를 움직이면 라이트도 함께 이동.
// - 라이트는 Additive emissive 머티리얼이 MPC Night_Intensity 로 밤에만 빛남(동적 광원 0, 눈속임).
UCLASS()
class COMPANYGROWTHRENEWAL_API AAmbientVehicleMover : public AAmbientSplineMover
{
	GENERATED_BODY()

public:
	AAmbientVehicleMover();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	// 앞 라이트카드(헤드라이트). MeshComp 자식.
	UPROPERTY(VisibleAnywhere, Category = "Vehicle|Light")
	UStaticMeshComponent* FrontLight;

	// 뒤 라이트카드(테일라이트). MeshComp 자식.
	UPROPERTY(VisibleAnywhere, Category = "Vehicle|Light")
	UStaticMeshComponent* RearLight;

	// MeshComp(차) 로컬 기준 앞 라이트 위치(cm). 차 forward축이 +Y 라 코앞은 +Y. 차종 길이에 맞춰 튜닝(~390~545cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Light")
	FVector FrontLightOffset = FVector(0.f, 190.f, 55.f);

	// MeshComp 로컬 기준 뒤 라이트 위치(cm). 뒤꽁무니는 -Y.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Light")
	FVector RearLightOffset = FVector(0.f, -190.f, 55.f);

	// 라이트카드 스케일(엔진 Plane 100cm 기준 → 0.6 ≈ 60cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Light", meta = (ClampMin = "0.01"))
	float LightCardScale = 0.6f;

	// 헤드라이트 머티리얼(Additive emissive × Night_Intensity). 미지정 시 생성자 기본값 사용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Light")
	UMaterialInterface* HeadLightMaterial;

	// 테일라이트 머티리얼.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Light")
	UMaterialInterface* TailLightMaterial;

private:
	// 라이트카드의 위치/스케일/머티리얼을 현재 UPROPERTY 값으로 동기화(회전은 컴포넌트 기본값 유지 → 에디터 튜닝 보존).
	void ApplyLightCardSetup();
};
