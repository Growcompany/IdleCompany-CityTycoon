#include "Components/CapsuleComponent.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficeworkerClickVolumeTests
{
// 사무실이 실제로 스폰하는 클래스 — BP 프리셋이 C++ 콜리전을 덮는 사고까지 잡으려고 BP CDO 를 본다
const AOfficeworker* LoadSpawnedWorkerDefaults(FAutomationTestBase& Test)
{
	const TSoftClassPtr<AOfficeworker> WorkerClass(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/BP_FunnyOfficeworker.BP_FunnyOfficeworker_C")));
	UClass* LoadedClass = WorkerClass.LoadSynchronous();
	Test.TestNotNull(TEXT("BP_FunnyOfficeworker class"), LoadedClass);
	return LoadedClass ? LoadedClass->GetDefaultObject<AOfficeworker>() : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeworkerClickVolumeChannelContractTest,
	"CGR.Office.Employee.ClickVolume.ChannelContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeworkerClickVolumeCoversBodyTest,
	"CGR.Office.Employee.ClickVolume.CoversBody",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeworkerClickVolumeChannelContractTest::RunTest(const FString& Parameters)
{
	const AOfficeworker* Worker = OfficeworkerClickVolumeTests::LoadSpawnedWorkerDefaults(*this);
	if (!Worker) { return false; }
	const UCapsuleComponent* Click = Worker->ClickVolume;
	if (!TestNotNull(TEXT("click volume"), Click)) { return false; }

	TestTrue(TEXT("query only"), Click->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
	TestTrue(TEXT("click channel blocks"), Click->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block);
	for (const ECollisionChannel Channel : {ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn, ECC_Visibility,
		ECC_Camera, ECC_PhysicsBody, ECC_GameTraceChannel2, ECC_GameTraceChannel3})
	{
		TestTrue(FString::Printf(TEXT("channel %d ignored"), static_cast<int32>(Channel)),
			Click->GetCollisionResponseToChannel(Channel) == ECR_Ignore);
	}
	TestFalse(TEXT("no overlap events"), Click->GetGenerateOverlapEvents());
	TestFalse(TEXT("does not affect navigation"), Click->CanEverAffectNavigation());
	return true;
}

bool FCGROfficeworkerClickVolumeCoversBodyTest::RunTest(const FString& Parameters)
{
	const AOfficeworker* Worker = OfficeworkerClickVolumeTests::LoadSpawnedWorkerDefaults(*this);
	if (!Worker) { return false; }
	const UCapsuleComponent* Click = Worker->ClickVolume;
	const UCapsuleComponent* Body = Worker->GetCapsuleComponent();
	if (!TestNotNull(TEXT("click volume"), Click) || !TestNotNull(TEXT("body capsule"), Body)) { return false; }

	TestTrue(TEXT("follows the body capsule"), Click->GetAttachParent() == Body);
	TestTrue(TEXT("centered on the body"), Click->GetRelativeLocation().IsNearlyZero());
	TestTrue(FString::Printf(TEXT("wider than body for touch (%.1f > %.1f)"),
		Click->GetUnscaledCapsuleRadius(), Body->GetUnscaledCapsuleRadius()),
		Click->GetUnscaledCapsuleRadius() > Body->GetUnscaledCapsuleRadius());
	TestTrue(FString::Printf(TEXT("covers full body height (%.1f >= %.1f)"),
		Click->GetUnscaledCapsuleHalfHeight(), Body->GetUnscaledCapsuleHalfHeight()),
		Click->GetUnscaledCapsuleHalfHeight() >= Body->GetUnscaledCapsuleHalfHeight());
	return true;
}

#endif
