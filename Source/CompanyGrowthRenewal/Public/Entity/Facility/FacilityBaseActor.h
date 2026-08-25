#pragma once

#include "CoreMinimal.h"
#include "Entity/InteractableBaseActor.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/FacilityType.h"
#include "FacilityBaseActor.generated.h"

UCLASS()
class COMPANYGROWTHRENEWAL_API AFacilityBaseActor : public AInteractableBaseActor
{
	GENERATED_BODY()

public:
	AFacilityBaseActor();

protected:
	virtual void BeginPlay() override;

	// 소속 나라
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
	ECountryType OwnerCountry = ECountryType::None;

	// 시설 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
	EFacilityType FacilityType = EFacilityType::None;

	// 해금 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
	bool bIsUnlocked = false;

	// 건설 비용 (Money)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
	int64 BuildCost = 0;

	// IInputHandler
	virtual void OnInteract_Implementation(APlayerController* InstigatingPC) override;
	virtual void OnEndInteract_Implementation(APlayerController* InstigatingPC) override;

	// 시설별 가상 함수 (BP 또는 C++ 서브클래스에서 오버라이드)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Facility")
	void OnFacilityInteract(APlayerController* PC);
	virtual void OnFacilityInteract_Implementation(APlayerController* PC) {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Facility")
	void OnFacilityEndInteract(APlayerController* PC);
	virtual void OnFacilityEndInteract_Implementation(APlayerController* PC) {}

	// 해금 시도
	UFUNCTION(BlueprintCallable, Category = "Facility")
	bool TryUnlock();

	// 잠금 비주얼 (머티리얼 어둡게)
	void SetLockedVisual(bool bLocked);

	// 모든 메시를 포함한 BoxComponent 크기 계산
	virtual void ReCalcBoxExtent() const override;

	// EntityManager 등록
	virtual void RegisterWithEntityManager() override;
	virtual void UnregisterWithEntityManager() override;

	// 잠금 시 머티리얼 인스턴스
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> LockedMaterialInstances;

public:
	UFUNCTION(BlueprintCallable, Category = "Facility")
	bool IsUnlocked() const { return bIsUnlocked; }

	UFUNCTION(BlueprintCallable, Category = "Facility")
	EFacilityType GetFacilityType() const { return FacilityType; }

	UFUNCTION(BlueprintCallable, Category = "Facility")
	ECountryType GetOwnerCountry() const { return OwnerCountry; }
};
