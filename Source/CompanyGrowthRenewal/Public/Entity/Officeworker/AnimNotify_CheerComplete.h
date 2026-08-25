#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CheerComplete.generated.h"

// 직원 애니메이션 이벤트 타입
UENUM(BlueprintType)
enum class EEmployeeAnimEventType : uint8
{
	CheerSittingComplete	UMETA(DisplayName = "CheerSitting Complete"),
	CheerStandUpComplete	UMETA(DisplayName = "CheerStandUp Complete"),
	TypeToSitComplete		UMETA(DisplayName = "TypeToSit Complete"),
	SitToStandComplete		UMETA(DisplayName = "SitToStand Complete"),
	StandToSitComplete		UMETA(DisplayName = "StandToSit Complete"),
	GreetingComplete		UMETA(DisplayName = "Greeting Complete"),
};

/**
 * 직원 애니메이션 이벤트 AnimNotify
 * 다양한 애니메이션 완료 시점에 재사용 가능
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UAnimNotify_EmployeeEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	// 이벤트 타입 (에디터에서 선택)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	EEmployeeAnimEventType EventType = EEmployeeAnimEventType::CheerSittingComplete;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
