// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Entity/Factory/BrickLaborerPath.h"
#include "BrickLaborer.generated.h"

class ABrickFactory;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
	* 벽돌공장 인부. 문 사이를 통과하며 구간당 한 번 굴뚝에서 벽돌이 나온다.
	*
	* 경로가 고정이므로 네비·캡슐·CharacterMovement 없이 위치와 방향을 직접 계산한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ABrickLaborer : public AActor
{
	GENERATED_BODY()

public:
	ABrickLaborer();

	virtual void Tick(float DeltaTime) override;

	// 공장이 스폰 직후 1회 호출. LaneIndex로 바깥 경로와 보행 위상을 분산한다.
	void InitLaborer(ABrickFactory* InFactory, int32 InLaneIndex, int32 InLaneCount);

	// 문에서 문까지 한 구간에 걸리는 시간(초) = 수확 1회 주기. 공장이 강화 곡선에서 계산해 넘긴다.
	void SetCycleDuration(float InSeconds);

	// 외형 교체로 바뀐 문 소켓에 현재 구간을 다시 맞춘다.
	void RefreshPath();

	// 평소/홀드 배속. 공장이 바꿔 준다.
	void SetSpeedMultiplier(float InMultiplier) { SpeedMultiplier = FMath::Max(0.01f, InMultiplier); }

	// 문에 닿을 때마다 공장이 뱉을 벽돌 수. 공장이 (총생산량 / 명수) 로 계산해 넘긴다.
	void SetHarvestAmount(int32 InAmount) { HarvestAmount = FMath::Max(1, InAmount); }

	// 명수가 줄어 해고될 때, 다음 문에 닿는 순간 빠진다.
	void RequestRetireAfterCycle() { bRetireRequested = true; }

	// 밤 모드 — 공장이 일출/일몰에 밀어 준다. 30명이 각자 시계를 구독하지 않게 공장이 한 번만 받아 전달한다.
	void SetNightMode(bool bInNight);

	// 보행 클립과 재생 속도. 공장이 평소/홀드에 따라 걷기↔뛰기로 갈아끼운다.
	void SetGait(UAnimSequence* Clip, float PlayRate, float GroundSpeed, UAnimSequence* CarryClip = nullptr);

protected:
	virtual void BeginPlay() override;

	// 바디 + 안전모/상의/바지/신발. 메시는 BP 에서 꽂는다.
	// 파츠를 바디에 LeaderPose 로 물리지 않는 이유: 바디 자신이 공장 포즈 마스터의 추종자라
	// 2단 체인이 되는데, UE 는 추종자를 리더로 삼는 걸 지원하지 않는다. 전부 마스터에 직접 건다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laborer")
	USkeletalMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laborer")
	USkeletalMeshComponent* HeadwearMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laborer")
	USkeletalMeshComponent* OuterwearMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laborer")
	USkeletalMeshComponent* PantsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laborer")
	USkeletalMeshComponent* ShoeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laborer|Carry")
	UStaticMeshComponent* CarriedBrickMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Laborer|Carry")
	TSoftObjectPtr<UStaticMesh> CarriedBrickAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Laborer|Carry")
	FName CarryAttachmentBone = TEXT("RightHand");

	UPROPERTY(EditDefaultsOnly, Category = "Laborer|Carry")
	FTransform CarryAttachmentTransform = FTransform(
		FQuat(0.588580, -0.040274, -0.805696, 0.052925),
		FVector(-0.166590, -0.147846, -0.159024), FVector(0.01f));

	// 위 5개를 한 번에 돌리기 위한 캐시. 생성자에서 채운다.
	UPROPERTY()
	TArray<USkeletalMeshComponent*> PartMeshes;

	// 문 소켓의 안쪽 방향으로 들어가는 깊이(cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Cycle", meta = (ClampMin = "0.0"))
	float DoorEnterDepth = 140.f;

	// 문 앞에서는 이 거리까지 정면으로 걷고 바깥 곡선에 합류한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Cycle", meta = (ClampMin = "50.0"))
	float DoorApproachDistance = 220.f;

	// 생산 주기 안에 포함하는 내부 회전 시간. 홀드 배속도 함께 적용된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Cycle", meta = (ClampMin = "0.0"))
	float InteriorTurnSeconds = 0.35f;

	// 내부 회전에 이즈인아웃을 적용한다. 보행은 거리 기준으로 진행한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Path")
	bool bEaseAtDoors = true;

	// 잡부 전체 크기. BP 가 아니라 여기 두는 이유: 네이티브 부모가 바뀌면 BP CDO 값이 재컴파일로 리셋된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float LaborerScale = 1.2f;

	// Funny 메시는 앞축이 UE 기본과 90도 틀어져 있다 — 보정 없이 진행방향으로 돌리면 옆을 보고 걷는다.
	// AOfficeworker 가 거는 FRotator(0, -90, 0) 과 같은 값이다(Officeworker.cpp:63).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look")
	float MeshYawOffset = -90.f;

	// 액터 원점(문 지점) 기준 발 높이 보정(cm). 문 컴포넌트를 바닥이 아닌 높이에 꽂았으면 여기서 내린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look")
	float MeshZOffset = 0.f;

	// 외형 색. 파츠마다 MID 를 만들어 M_FunnyToon 의 TintColor 로 넣는다.
	// MID 없이 머티리얼 원본을 그대로 쓰면 틴트가 안 먹어 전부 허옇게 나온다(2026-10-02 실측).
	//
	// ⚠ 전부 **linear** 값이다. sRGB 분수를 그대로 넣으면 물이 빠져 회색처럼 보인다(2026-10-02 실측).
	//   괄호 안이 원래 sRGB 색. FunnyOfficeworker 의 SkinPalette 와 같은 규약.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look")
	FLinearColor SkinTint = FLinearColor(1.00f, 0.69f, 0.51f);        // sRGB #FFD9BE

	// 안전모는 텍스처에 노란색이 구워져 있다. M_FunnyToon 이 Unlit 이라 틴트가 1 이면
	// 텍스처 색이 그대로 최종 픽셀이 돼 풀브라이트로 나가고, 블룸에 걸려 빛나 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look")
	FLinearColor HeadwearTint = FLinearColor(0.62f, 0.62f, 0.62f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look")
	FLinearColor OuterwearTint = FLinearColor(0.32f, 0.38f, 0.44f);   // sRGB #9AA5B1 작업 셔츠

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look")
	FLinearColor PantsTint = FLinearColor(0.044f, 0.068f, 0.125f);    // sRGB #3B4A63 데님

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look")
	FLinearColor ShoeTint = FLinearColor(0.023f, 0.019f, 0.016f);     // sRGB #2B2622 작업화

	// 밤에 틴트에 곱하는 계수. M_FunnyToon 은 **Unlit** 이라 주변이 어두워져도 재료 색 그대로 나간다 —
	// 보정 없이 두면 어두운 배경에서 야광봉처럼 보인다. 0 으로 죽이지 않는 이유는 실루엣은 남아야 하기 때문.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float NightTintScale = 0.45f;

	// M_FunnyToon 의 Brightness 기본값 1.25 는 사무실 PPV 기준이라, 노출을 0.25 로 고정하고 블룸 임계값이
	// 1.5 인 도시 맵에서는 인부가 떠 보인다. 에셋을 고치면 사무실 직원까지 따라가므로 MID 로만 낮춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laborer|Look", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float LookBrightness = 0.70f;

private:
	UPROPERTY()
	TWeakObjectPtr<ABrickFactory> OwnerFactory;

	// 현재 구간의 출발 문과 도착 문. 다음 구간은 방금 들어간 문에서 시작한다.
	int32 FromDoorIndex = 0;
	int32 TargetDoorIndex = 0;
	int32 CompletedTrips = 0;

	// 현재 구간 안에서의 진행도 0~1. 한 구간 = 문에서 문까지 = 수확 1회.
	float LegAlpha = 0.f;

	float CycleDuration = 2.f;
	float SpeedMultiplier = 1.f;
	int32 HarvestAmount = 1;
	int32 LaneIndex = 0;
	int32 LaneCount = 1;
	bool bRetireRequested = false;
	bool bNightMode = false;
	bool bHarvestedThisLeg = false;
	bool bCarryingThisLeg = false;

	FBrickLaborerPath Path;
	float PathClearance = 130.f;
	float GaitPlayRate = 1.f;
	float GaitGroundSpeed = 200.f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> NormalGaitClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CarryGaitClip;

	void AdvanceLeg();
	void ApplyLegEndpoints();

	void UpdatePathPose();
	void ApplyGaitForCurrentLeg(bool bPreservePhase);
	float GetEntranceAlpha() const;
	void HarvestAtEntrance();

	// 파츠 4개를 바디에 물린다. 같은 액터 안이라 프로젝트의 검증된 패턴(FunnyOfficeworker)과 동일하다.
	void AttachPartsToBody();

	// 파츠마다 MID 를 만들어 색을 넣는다.
	void ApplyLook();

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> PartMIDs;

	// 파츠 전체를 캐릭터 2톤 툰으로 통일할 부모 머티리얼. 에셋 기본값은 Headwear 만 M_FunnyToon 이고
	// Body/Outerwear/Pants/Shoe 는 팩 M_Material(Default Lit) 이라, 강제하지 않으면 한 액터가 두 셰이딩
	// 모델로 갈려 밤에 몸만 20~70배 어두워진다. M_Material 에는 TexGain/TexBlend/Brightness 가 없어
	// ApplyLook 의 파라미터 계약도 조용히 버려진다.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ToonMaterial;
};
