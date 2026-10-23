// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enum/CountryType.h"
#include "LegacyCountryActor.generated.h"

/**
 * 세계지도 나라 위치 마커 액터
 *
 * 레벨에 배치하여 각 나라의 위치와 이름 표시를 담당.
 * 시설(Facility)은 별도의 FacilityBaseActor로 분리됨.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ALegacyCountryActor : public AActor
{
	GENERATED_BODY()

public:
	ALegacyCountryActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country")
	ECountryType CountryType = ECountryType::None;

	UPROPERTY()
	UTexture2D* CachedFlagIcon = nullptr;

	// 월드에서 특정 ECountryType의 액터 검색 (UI에서 카메라 포커스 시 사용)
	UFUNCTION(BlueprintCallable, Category = "Country", meta = (WorldContext = "WorldContextObject"))
	static ALegacyCountryActor* FindCountryActor(const UObject* WorldContextObject, ECountryType InCountry);

	// 카메라 포커스 요청 이벤트 — BP에서 추가 연출(페이드/사운드 등)을 구현
	UFUNCTION(BlueprintImplementableEvent, Category = "Country|Camera")
	void OnFocusRequested(bool bAnimate);

	// 외부에서 호출하는 진입점 (내부적으로 카메라 FocusOnLocation + BP 이벤트)
	UFUNCTION(BlueprintCallable, Category = "Country|Camera")
	void RequestFocus(bool bAnimate = true);

	// 포커스 시 카메라와 나라 사이 거리 (SpringArmLength 기준)
	// 0이면 현재 줌 유지, 양수면 해당 거리로 줌 인
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Camera")
	float FocusDistance = 40000.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* DefaultSceneRoot;
};
