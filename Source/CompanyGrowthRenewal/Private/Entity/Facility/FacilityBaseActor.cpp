#include "Entity/Facility/FacilityBaseActor.h"
#include "Manager/ResourceItemManager.h"
#include "Enum/ResourceType.h"
#include "Components/BoxComponent.h"

AFacilityBaseActor::AFacilityBaseActor()
{
	Tags.Add(FName("Interactable"));
	Tags.Add(FName("Facility"));
}

void AFacilityBaseActor::BeginPlay()
{
	Super::BeginPlay();

	// 모든 메시 컴포넌트의 머티리얼을 Dynamic Material Instance로 교체 (잠금 비주얼용)
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);

	for (UMeshComponent* MeshComp : MeshComponents)
	{
		for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
		{
			UMaterialInterface* Mat = MeshComp->GetMaterial(i);
			if (Mat)
			{
				UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(Mat, this);
				MeshComp->SetMaterial(i, DynMat);
				LockedMaterialInstances.Add(DynMat);
			}
		}
	}

	// 초기 비주얼 적용
	SetLockedVisual(!bIsUnlocked);
}

void AFacilityBaseActor::OnInteract_Implementation(APlayerController* InstigatingPC)
{
	if (!bIsUnlocked)
	{
		// 잠금 상태: 해금 시도
		TryUnlock();
		return;
	}

	// 해금 상태: 시설별 동작
	OnFacilityInteract(InstigatingPC);
}

void AFacilityBaseActor::OnEndInteract_Implementation(APlayerController* InstigatingPC)
{
	if (!bIsUnlocked) return;

	OnFacilityEndInteract(InstigatingPC);
}

bool AFacilityBaseActor::TryUnlock()
{
	if (bIsUnlocked) return true;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return false;

	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!RMgr) return false;

	if (!RMgr->HasResource(EResourceType::Money, BuildCost)) return false;

	// Money 차감 + 해금
	RMgr->SpendResource(EResourceType::Money, BuildCost);
	bIsUnlocked = true;
	SetLockedVisual(false);

	UE_LOG(LogTemp, Log, TEXT("[FacilityBaseActor] Unlocked %s facility in %s (Cost: %lld)"),
		*UEnum::GetValueAsString(FacilityType),
		*UEnum::GetValueAsString(OwnerCountry),
		BuildCost);

	return true;
}

void AFacilityBaseActor::SetLockedVisual(bool bLocked)
{
	// 잠금: 어둡게(0.3), 해금: 원래 밝기(1.0)
	float Brightness = bLocked ? 0.3f : 1.0f;

	for (UMaterialInstanceDynamic* DynMat : LockedMaterialInstances)
	{
		if (DynMat)
		{
			DynMat->SetScalarParameterValue(FName("Brightness"), Brightness);
		}
	}
}

void AFacilityBaseActor::ReCalcBoxExtent() const
{
	if (!BoxComponent) return;

	// 모든 메시 컴포넌트의 바운드를 합산
	FBox CombinedBounds(ForceInit);
	TArray<UMeshComponent*> MeshComps;
	const_cast<AFacilityBaseActor*>(this)->GetComponents<UMeshComponent>(MeshComps);

	for (const UMeshComponent* MeshComp : MeshComps)
	{
		if (MeshComp->IsRegistered())
		{
			CombinedBounds += MeshComp->Bounds.GetBox();
		}
	}

	if (!CombinedBounds.IsValid) return;

	FVector Extent = CombinedBounds.GetExtent();
	Extent.X = FMath::Max(Extent.X, 100.0f);
	Extent.Y = FMath::Max(Extent.Y, 100.0f);
	Extent.Z = FMath::Max(Extent.Z, 100.0f);

	BoxComponent->SetBoxExtent(Extent);
	BoxComponent->SetWorldLocation(CombinedBounds.GetCenter());
}

void AFacilityBaseActor::RegisterWithEntityManager()
{
	// TODO: EntityManager에 시설 등록
}

void AFacilityBaseActor::UnregisterWithEntityManager()
{
	// TODO: EntityManager에서 시설 해제
}
