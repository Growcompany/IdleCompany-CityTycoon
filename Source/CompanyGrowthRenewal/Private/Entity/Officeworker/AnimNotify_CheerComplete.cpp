#include "Entity/Officeworker/AnimNotify_CheerComplete.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_EmployeeEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Owner를 찾는 여러 방법 시도
	AActor* Owner = MeshComp->GetOwner();

	// GetOwner() 실패 시 AttachmentRoot 시도
	if (!Owner)
	{
		Owner = MeshComp->GetAttachmentRootActor();
	}

	// 그래도 실패 시 Outer 체인에서 Actor 찾기
	if (!Owner)
	{
		for (UObject* Outer = MeshComp->GetOuter(); Outer; Outer = Outer->GetOuter())
		{
			if (AActor* OuterActor = Cast<AActor>(Outer))
			{
				Owner = OuterActor;
				break;
			}
		}
	}

	if (!Owner)
	{
		return;
	}

	// 게임 월드가 아닌 경우 무시 (에디터 프리뷰 등)
	UWorld* World = Owner->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	UEmployeeBehaviorComponent* BehaviorComp = Owner->FindComponentByClass<UEmployeeBehaviorComponent>();
	if (!BehaviorComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EmployeeEvent] BehaviorComp is null on %s"), *Owner->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[AnimNotify_EmployeeEvent] Processing EventType: %d on %s"), (int32)EventType, *Owner->GetName());

	// 이벤트 타입에 따라 처리
	switch (EventType)
	{
	case EEmployeeAnimEventType::CheerSittingComplete:
	case EEmployeeAnimEventType::CheerStandUpComplete:
		BehaviorComp->OnCheerAnimationComplete();
		break;

	case EEmployeeAnimEventType::TypeToSitComplete:
		BehaviorComp->OnTypeToSitComplete();
		break;

	case EEmployeeAnimEventType::SitToStandComplete:
		BehaviorComp->OnSitToStandComplete();
		break;

	case EEmployeeAnimEventType::StandToSitComplete:
		BehaviorComp->OnStandToSitComplete();
		break;

	case EEmployeeAnimEventType::GreetingComplete:
		BehaviorComp->OnGreetingComplete();
		break;
	}
}

FString UAnimNotify_EmployeeEvent::GetNotifyName_Implementation() const
{
	switch (EventType)
	{
	case EEmployeeAnimEventType::CheerSittingComplete:
		return TEXT("CheerSitting Complete");
	case EEmployeeAnimEventType::CheerStandUpComplete:
		return TEXT("CheerStandUp Complete");
	case EEmployeeAnimEventType::TypeToSitComplete:
		return TEXT("TypeToSit Complete");
	case EEmployeeAnimEventType::SitToStandComplete:
		return TEXT("SitToStand Complete");
	case EEmployeeAnimEventType::StandToSitComplete:
		return TEXT("StandToSit Complete");
	case EEmployeeAnimEventType::GreetingComplete:
		return TEXT("Greeting Complete");
	default:
		return TEXT("Employee Event");
	}
}
