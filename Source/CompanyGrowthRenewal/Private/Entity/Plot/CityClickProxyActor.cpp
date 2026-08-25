#include "Entity/Plot/CityClickProxyActor.h"
#include "Manager/CityAcquisitionManager.h"
#include "Components/BoxComponent.h"

ACityClickProxyActor::ACityClickProxyActor()
{
    PrimaryActorTick.bCanEverTick = false;

    BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
    SetRootComponent(BoxComponent);

    // 입력 핸들러(GetOverlappingActor/OnInteractionCanceled)가 "Interactable" 태그로 게이트하므로 필수
    Tags.Add(FName("Interactable"));

    BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);

    // GameTraceChannel1("BuildingClick") 에만 Block — 나머지는 무시
    FCollisionResponseContainer Resp;
    Resp.SetAllChannels(ECR_Ignore);
    Resp.SetResponse(ECC_GameTraceChannel1, ECR_Block);
    BoxComponent->SetCollisionResponseToChannels(Resp);
}

void ACityClickProxyActor::InitProxy(int32 InKey, FVector BoxExtent)
{
    CompanyKey = InKey;
    BoxComponent->SetBoxExtent(BoxExtent);
}

void ACityClickProxyActor::OnEndInteract_Implementation(APlayerController* /*PC*/)
{
    if (UCityAcquisitionManager* M = GetWorld()->GetSubsystem<UCityAcquisitionManager>())
    {
        M->OnCompanyClicked(CompanyKey);
    }
}
