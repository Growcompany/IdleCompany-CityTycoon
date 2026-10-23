#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Enum/OfficeExpansionType.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Office/OfficeInterior.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficeExpansionPreviewTests
{
class FScopedTestWorld
{
public:
	FScopedTestWorld()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (TestWorld && GEngine)
		{
			FWorldContext& TestWorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			TestWorldContext.SetCurrentWorld(TestWorld);
		}
	}

	~FScopedTestWorld()
	{
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
	}

	UWorld* Get() const { return TestWorld; }

private:
	UWorld* TestWorld = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExpansionAnchorTest,
	"CompanyGrowth.Office.ExpansionPreview.Anchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGROfficeExpansionAnchorTest::RunTest(const FString&)
{
	OfficeExpansionPreviewTests::FScopedTestWorld WorldScope;
	UWorld* TestWorld = WorldScope.Get();
	if (!TestWorld) { AddError(TEXT("테스트 월드를 만들지 못했다")); return false; }

	AOfficeInterior* Interior = TestWorld->SpawnActor<AOfficeInterior>();
	if (!Interior) { AddError(TEXT("OfficeInterior 를 스폰하지 못했다")); return false; }
	Interior->SetActorTransform(FTransform(FRotator(0.f, 37.f, 0.f),
		FVector(120.f, -340.f, 75.f), FVector(1.5f, 1.5f, 1.5f)));

	const float Tile = Interior->GetTileSize();
	const auto ExpectedAnchor = [Interior, Tile](const EOfficeExpandDirection Direction)
	{
		const FBox2D Bounds = FOfficeFootprintGeometry::MakeFloorBounds(Interior->GetTileCount(), Tile);
		const FVector2D Center = Bounds.GetCenter();
		const FVector LocalEdge = Direction == EOfficeExpandDirection::Left
			? FVector(Bounds.Max.X + Tile * 0.1f, Center.Y, Interior->GetStructuralFloorZLocal())
			: FVector(Center.X, Bounds.Min.Y - Tile * 0.1f, Interior->GetStructuralFloorZLocal());
		return Interior->GetActorTransform().TransformPosition(LocalEdge);
	};

	TestTrue(TEXT("시작 크기에 가로 칸이 있다"),
		Interior->HasExpansionCell(EOfficeExpandDirection::Left));
	TestTrue(TEXT("시작 크기에 세로 칸이 있다"),
		Interior->HasExpansionCell(EOfficeExpandDirection::Right));

	const FVector Anchor = Interior->GetExpansionAnchorWorld(EOfficeExpandDirection::Left);
	TestTrue(TEXT("회전·스케일된 가로 바닥 경계에서 0.1칸 바깥에 앵커가 있다"),
		Anchor.Equals(ExpectedAnchor(EOfficeExpandDirection::Left), 0.01));
	const FVector RightAnchor = Interior->GetExpansionAnchorWorld(EOfficeExpandDirection::Right);
	TestTrue(TEXT("회전·스케일된 세로 바닥 경계에서 0.1칸 바깥에 앵커가 있다"),
		RightAnchor.Equals(ExpectedAnchor(EOfficeExpandDirection::Right), 0.01));
	TestTrue(TEXT("가로 성장 방향은 actor +X 이다"),
		Interior->GetExpansionDirectionWorld(EOfficeExpandDirection::Left).Equals(
			Interior->GetActorTransform().TransformVector(FVector::XAxisVector).GetSafeNormal(), 0.0001));
	TestTrue(TEXT("세로 성장 방향은 actor -Y 이다"),
		Interior->GetExpansionDirectionWorld(EOfficeExpandDirection::Right).Equals(
			Interior->GetActorTransform().TransformVector(-FVector::YAxisVector).GetSafeNormal(), 0.0001));

	// 확장 직후 앵커가 **새** 타일 수를 따라야 한다 ― 옛 값이 남으면 프리뷰가 한 박자 늦는다.
	TestTrue(TEXT("가로 확장이 성공한다"), Interior->ExpandLeft());
	const FVector After = Interior->GetExpansionAnchorWorld(EOfficeExpandDirection::Left);
	TestFalse(TEXT("확장 뒤 앵커가 옮겨간다"), After.Equals(Anchor, 1.0));
	TestTrue(TEXT("가로 확장 뒤 앵커가 새 경계에 있다"),
		After.Equals(ExpectedAnchor(EOfficeExpandDirection::Left), 0.01));
	TestTrue(TEXT("가로 앵커가 월드 한 칸만큼 이동한다"),
		(After - Anchor).Equals(Interior->GetActorTransform().TransformVector(FVector(Tile, 0.f, 0.f)), 0.01));

	const FVector BeforeRightExpansion = Interior->GetExpansionAnchorWorld(EOfficeExpandDirection::Right);
	TestTrue(TEXT("세로 확장이 성공한다"), Interior->ExpandRight());
	const FVector AfterRightExpansion = Interior->GetExpansionAnchorWorld(EOfficeExpandDirection::Right);
	TestTrue(TEXT("세로 확장 뒤 앵커가 새 경계에 있다"),
		AfterRightExpansion.Equals(ExpectedAnchor(EOfficeExpandDirection::Right), 0.01));
	TestTrue(TEXT("세로 앵커가 월드 한 칸만큼 이동한다"),
		(AfterRightExpansion - BeforeRightExpansion).Equals(
			Interior->GetActorTransform().TransformVector(FVector(0.f, -Tile, 0.f)), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExpansionAnchorAtMaximumTest,
	"CompanyGrowth.Office.ExpansionPreview.AtMaximum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGROfficeExpansionAnchorAtMaximumTest::RunTest(const FString&)
{
	OfficeExpansionPreviewTests::FScopedTestWorld WorldScope;
	UWorld* TestWorld = WorldScope.Get();
	if (!TestWorld) { AddError(TEXT("테스트 월드를 만들지 못했다")); return false; }

	AOfficeInterior* Interior = TestWorld->SpawnActor<AOfficeInterior>();
	if (!Interior) { AddError(TEXT("OfficeInterior 를 스폰하지 못했다")); return false; }

	// 가로를 끝까지 민다 (1 -> 5).
	const FIntPoint Max = Interior->GetMaxTileCount();
	while (Interior->GetTileCount().X < Max.X)
	{
		if (!Interior->ExpandLeft()) { break; }
	}

	TestEqual(TEXT("가로가 최대다"), Interior->GetTileCount().X, Max.X);
	TestFalse(TEXT("최대 축에는 칸이 없다"),
		Interior->HasExpansionCell(EOfficeExpandDirection::Left));
	TestEqual(TEXT("칸이 없으면 앵커도 없다"),
		Interior->GetExpansionAnchorWorld(EOfficeExpandDirection::Left), FVector::ZeroVector);
	TestTrue(TEXT("세로는 그대로 남는다"),
		Interior->HasExpansionCell(EOfficeExpandDirection::Right));

	return true;
}

#endif
