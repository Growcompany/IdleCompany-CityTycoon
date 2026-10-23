#include "Entity/Factory/BrickFactory.h"

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "NiagaraComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryChimneySmokeTest,
	"CGR.Factory.ChimneySmoke.AllSocketsPulse",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryChimneySmokeTest::RunTest(const FString& Parameters)
{
	UClass* FactoryClass = TSoftClassPtr<ABrickFactory>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Blueprint/Factory/BP_BrickFactory.BP_BrickFactory_C"))).LoadSynchronous();
	if (!TestNotNull(TEXT("factory Blueprint is available"), FactoryClass)) return false;
	UNiagaraSystem* SmokeSystem = FactoryClass->GetDefaultObject<ABrickFactory>()->BrickSmokeVFX;
	if (!TestNotNull(TEXT("factory Blueprint supplies its smoke system"), SmokeSystem)) return false;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world exists"), TestWorld)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	ABrickFactory* Factory = TestWorld->SpawnActor<ABrickFactory>();
	if (!TestNotNull(TEXT("native factory exists"), Factory))
	{
		if (GEngine) GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
		return false;
	}
	Factory->BrickSmokeVFX = SmokeSystem;

	TArray<USceneComponent*> Chimneys;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		USceneComponent* Chimney = NewObject<USceneComponent>(Factory);
		Factory->AddInstanceComponent(Chimney);
		Chimney->SetupAttachment(Factory->GetRootComponent());
		Chimney->RegisterComponent();
		Chimney->SetRelativeLocation(FVector(Index * 120.f, (Index % 2) * 90.f, 400.f));
		Chimneys.Add(Chimney);
	}
	Factory->SpawnPoints = Chimneys;
	Factory->OnLaborerEntered(1);

	TArray<UNiagaraComponent*> SmokeComponents;
	Factory->GetComponents<UNiagaraComponent>(SmokeComponents);
	TestEqual(TEXT("one Niagara component exists per chimney"), SmokeComponents.Num(), Chimneys.Num());
	for (USceneComponent* Chimney : Chimneys)
	{
		int32 AttachedCount = 0;
		for (UNiagaraComponent* Smoke : SmokeComponents)
		{
			if (Smoke && Smoke->GetAttachParent() == Chimney &&
				Smoke->GetRelativeLocation().IsNearlyZero() && Smoke->GetAsset() == SmokeSystem)
			{
				++AttachedCount;
			}
		}
		TestEqual(TEXT("each chimney owns a smoke emitter at its origin"), AttachedCount, 1);
	}

	Factory->SetActorTransform(FTransform(FRotator(0.f, 75.f, 0.f), FVector(700.f, -300.f, 100.f)));
	for (UNiagaraComponent* Smoke : SmokeComponents)
	{
		USceneComponent* Chimney = Smoke ? Smoke->GetAttachParent() : nullptr;
		TestTrue(TEXT("smoke follows the moved and rotated chimney"), Smoke && Chimney &&
			Smoke->GetComponentLocation().Equals(Chimney->GetComponentLocation(), 0.1f));
	}

	// Advance the fixture clock past the idle pulse gate without starting factory gameplay/economy.
	TestWorld->TimeSeconds += 3.0f;
	Factory->OnLaborerEntered(1);
	TArray<UNiagaraComponent*> ReusedComponents;
	Factory->GetComponents<UNiagaraComponent>(ReusedComponents);
	TestEqual(TEXT("another pulse reuses the six emitters"), ReusedComponents.Num(), Chimneys.Num());
	for (UNiagaraComponent* Smoke : SmokeComponents)
	{
		TestTrue(TEXT("the original emitter is retained"), ReusedComponents.Contains(Smoke));
	}

	Factory->SpawnPoints.SetNum(1);
	TestWorld->TimeSeconds += 3.0f;
	Factory->OnLaborerEntered(1);
	TArray<UNiagaraComponent*> SingleChimneyComponents;
	Factory->GetComponents<UNiagaraComponent>(SingleChimneyComponents);
	TestEqual(TEXT("fewer active chimneys discard stale emitters"), SingleChimneyComponents.Num(), 1);
	if (SingleChimneyComponents.Num() == 1)
	{
		TestEqual(TEXT("remaining emitter follows the active chimney"),
			SingleChimneyComponents[0]->GetAttachParent(), Chimneys[0]);
	}

	if (GEngine) GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);
	return true;
}

#endif
