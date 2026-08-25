// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Data/CharacterAppearanceTypes.h"
#include "ModularOfficeworker.generated.h"

class USkeletalMesh;
class UAnimInstance;

/**
 * 구 모듈러 직원 베이스 (Male/Female 전용 중간 클래스).
 * 17개 파츠 SkeletalMeshComponent + 헤어/모프/의상 외형 파이프라인을 보유.
 * 공유 게임플레이(Behavior/버블/초상화/배회/워크스테이션)는 부모 AOfficeworker 소관.
 * 스틱 워커(AStickOfficeworker)는 이 클래스를 거치지 않고 AOfficeworker 직속.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AModularOfficeworker : public AOfficeworker
{
	GENERATED_BODY()

public:
	AModularOfficeworker();

	// Face = GetMesh() 별칭 (모듈러 파츠들의 LeaderPose 기준)
	UPROPERTY()
	USkeletalMeshComponent* Face;

	UPROPERTY()
	USkeletalMeshComponent* Trousers;

	UPROPERTY()
	USkeletalMeshComponent* Top;

	UPROPERTY()
	USkeletalMeshComponent* Accessory1;

	UPROPERTY()
	USkeletalMeshComponent* Accessory2;

	UPROPERTY()
	USkeletalMeshComponent* Mouth;

	UPROPERTY()
	USkeletalMeshComponent* Hands;

	UPROPERTY()
	USkeletalMeshComponent* Cape;

	UPROPERTY()
	USkeletalMeshComponent* Tail;

	UPROPERTY()
	USkeletalMeshComponent* Belt;

	UPROPERTY()
	USkeletalMeshComponent* Shoes;

	UPROPERTY()
	USkeletalMeshComponent* Ears;

	UPROPERTY()
	USkeletalMeshComponent* Eyebrows;

	UPROPERTY()
	USkeletalMeshComponent* HairBase;

	UPROPERTY()
	USkeletalMeshComponent* HairBack;

	UPROPERTY()
	USkeletalMeshComponent* HairFringe;

	UPROPERTY()
	USkeletalMeshComponent* HairSides;

	UPROPERTY()
	FName HairAttachSocketName = FName("head");

protected:
	// 기본 메시들 (모든 모듈러 캐릭터 공통)
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> BaseFaceMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultMouthMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultEarsMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultHandsMesh;

	// 기본 헤어/의상 (초기 로딩용)
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultHairBaseMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultHairBackMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultHairSidesMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultHairFringeMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultEyebrowsMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultTopMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultTrousersMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultShoesMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultAccessoryMesh;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> DefaultBeltMesh;

	// Animation Blueprint 클래스 (설정 시 ABP 사용, 미설정 시 SingleNode 애니메이션)
	UPROPERTY()
	TSoftClassPtr<UAnimInstance> AnimBlueprintClass;

public:
	// 런타임에 적용되는 메시를 초기화 (헤어, 의상)
	void ClearDynamicMeshes();

	// 모듈러 외형 적용 + 메시 로드 완료 폴링 → OnMeshLoadCompleted 발화 (초상화 캡쳐 경로)
	UFUNCTION()
	void ApplyAppearanceAndNotify(const FCharacterAppearance& Appearance, EEmployeeRank Rank, EEmployeeGender Gender, int32 HairCombinationType,
		int32 RandomSeed, int32 EmployeeID = -1);

	UFUNCTION()
	void SetCharacterAppearance(const FCharacterAppearance& Appearance, EEmployeeRank Rank, EEmployeeGender Gender, int32 EmployeeID = -1);

	UFUNCTION()
	virtual void SetHairCombination(int32 CombinationType, int32 RandomSeed) PURE_VIRTUAL(AModularOfficeworker::SetHairCombination, return;)

	UFUNCTION()
	void SetEyebrowsPart(const FString& PartName, EEmployeeGender Gender);

	UFUNCTION()
	void SetHairColors(const FHairColorSet& HairColors);

	UFUNCTION()
	void SetEyebrowsColors(const FLinearColor& Color1, const FLinearColor& ColorLines);

	UFUNCTION()
	void SetEyeColors(const FEyeColorSet& Colors);

	UFUNCTION()
	void SetSkinColors(const FSkinColorSet& Colors);

	UFUNCTION()
	void ApplyMorphTargets(const FMorphTargetSet& MorphSet);

	UFUNCTION()
	void SetClothingByRank(EEmployeeRank Rank, EEmployeeGender Gender, int32 EmployeeID = -1);

	// 선택/버프 글로우 — 모듈러 파츠(Top/Trousers/Hair*)에 오버레이 적용 (부모는 상태만 관리)
	virtual void SetSelected(bool bSelected) override;
	virtual void SetGlowOverlay(bool bEnabled, FLinearColor Color = FLinearColor(1.f, 0.85f, 0.2f, 1.f)) override;

protected:
	virtual void PostInitializeComponents() override;

	// 기본 메시 로드 (TableManager → Default*Mesh)
	void LoadDefaultMeshesFromTable();

	// 헬퍼 함수
	void SetMaterialParameter(USkeletalMeshComponent* Component, const FString& ParameterName, const FLinearColor&
		Color);

	void SetHairPart(EHairPartType PartType, const FString& PartName, EEmployeeGender Gender);
	void ClearHairPart(EHairPartType PartType);
	void SetRandomHairPart(EHairPartType PartType, const TArray<FString>& PartNames, FRandomStream&
		RandomStream, EEmployeeGender Gender);
	USkeletalMeshComponent* GetHairComponent(EHairPartType PartType);
};
