#include "Misc/AutomationTest.h"
#include "Enum/OfficeExpansionType.h"
#include "Office/OfficeFootprintGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExpansionCellBoundsTest,
	"CompanyGrowth.Office.ExpansionCell.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGROfficeExpansionCellBoundsTest::RunTest(const FString&)
{
	const FIntPoint Max(5, 6);
	const float Tile = 400.f;

	// 시작 1x2 바닥은 X[4,404] Y[-809,-9] 다 (MakeFloorBounds 실측).
	const FBox2D Left = FOfficeFootprintGeometry::MakeExpansionCellBounds(
		FIntPoint(1, 2), Max, EOfficeExpandDirection::Left, Tile);
	// UE5 의 FVector2D/FVector 성분은 double 이다 ― float 리터럴로 비교하면 TestEqual 오버로드가 모호해진다.
	TestTrue(TEXT("가로 칸 유효"), Left.bIsValid);
	TestEqual(TEXT("가로 칸 MinX"), Left.Min.X, 404.0);
	TestEqual(TEXT("가로 칸 MaxX"), Left.Max.X, 804.0);
	TestEqual(TEXT("가로 칸 MinY"), Left.Min.Y, -809.0);
	TestEqual(TEXT("가로 칸 MaxY"), Left.Max.Y, -9.0);

	const FBox2D Right = FOfficeFootprintGeometry::MakeExpansionCellBounds(
		FIntPoint(1, 2), Max, EOfficeExpandDirection::Right, Tile);
	TestTrue(TEXT("세로 칸 유효"), Right.bIsValid);
	TestEqual(TEXT("세로 칸 MinX"), Right.Min.X, 4.0);
	TestEqual(TEXT("세로 칸 MaxX"), Right.Max.X, 404.0);
	TestEqual(TEXT("세로 칸 MinY"), Right.Min.Y, -1209.0);
	TestEqual(TEXT("세로 칸 MaxY"), Right.Max.Y, -809.0);

	// 성장은 +X 와 -Y ― 새 칸 중심이 기존 바닥 바깥이어야 한다.
	const FVector LeftCenter = FOfficeFootprintGeometry::MakeExpansionCellCenter(
		FIntPoint(1, 2), Max, EOfficeExpandDirection::Left, Tile);
	TestEqual(TEXT("가로 칸 중심 X"), LeftCenter.X, 604.0);
	TestEqual(TEXT("가로 칸 중심 Y"), LeftCenter.Y, -409.0);
	TestEqual(TEXT("가로 칸 중심 Z"), LeftCenter.Z,
		static_cast<double>(FOfficeFootprintGeometry::StructuralFloorZ));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExpansionCellAtMaximumTest,
	"CompanyGrowth.Office.ExpansionCell.AtMaximum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGROfficeExpansionCellAtMaximumTest::RunTest(const FString&)
{
	const FIntPoint Max(5, 6);
	const float Tile = 400.f;

	// 최대에 닿은 축은 존재하지 않는 칸을 돌려주면 안 된다.
	TestFalse(TEXT("가로 최대"), FOfficeFootprintGeometry::MakeExpansionCellBounds(
		FIntPoint(5, 3), Max, EOfficeExpandDirection::Left, Tile).bIsValid);
	TestFalse(TEXT("세로 최대"), FOfficeFootprintGeometry::MakeExpansionCellBounds(
		FIntPoint(3, 6), Max, EOfficeExpandDirection::Right, Tile).bIsValid);
	TestTrue(TEXT("가로 최대여도 세로는 유효"), FOfficeFootprintGeometry::MakeExpansionCellBounds(
		FIntPoint(5, 3), Max, EOfficeExpandDirection::Right, Tile).bIsValid);
	TestEqual(TEXT("무효면 중심은 0"), FOfficeFootprintGeometry::MakeExpansionCellCenter(
		FIntPoint(5, 3), Max, EOfficeExpandDirection::Left, Tile), FVector::ZeroVector);

	return true;
}

namespace OfficeExpansionCellTests
{
// 아래 값은 BP_OfficeInterior 의 실측이다 (2026-10-07, 에디터 CDO 덤프).
// 벽 SM_Walls_006 로컬 박스 / 바닥 SM_Walls_011 로컬 박스.
const FBox WallMeshBox(FVector(0.0, -10.0, 0.0), FVector(400.0, 10.0, 400.0));
const FBox FloorMeshBox(FVector(-400.0, -8.0, 0.0), FVector(0.0, 12.0, 400.0));

// WallLeft: 바닥의 최대 Y 변에서 X 를 따라 뻗는다. 스케일 X = TileCountX.
const FTransform WallLeftXf(FRotator(0.f, 0.f, 0.f), FVector(3.7, 0.3, -27.3), FVector(1.0, 1.0, 1.05));
// WallRight: 최소 X 변에서 Y 를 따라. 스케일 X = TileCountY + 0.05 (저작 보정값).
const FTransform WallRightXf(FRotator(0.f, -90.f, 0.f), FVector(-6.3, 10.3, -27.3), FVector(2.05, 1.0, 1.05));
// Floor: 성장은 로컬 X(가로) 와 로컬 Z(세로). 스케일 (1+가로확장, 1, 2+세로확장).
const FTransform FloorXf(FRotator(0.f, 180.f, 90.f), FVector(3.8, -9.7, -14.3), FVector(1.0, 1.0, 2.0));

FBox BoxUnder(const FBox& Local, const FTransform& Xf)
{
	FBox Out(ForceInit);
	for (int32 SX = 0; SX < 2; ++SX)
	{
		for (int32 SY = 0; SY < 2; ++SY)
		{
			for (int32 SZ = 0; SZ < 2; ++SZ)
			{
				Out += Xf.TransformPosition(FVector(
					SX ? Local.Max.X : Local.Min.X,
					SY ? Local.Max.Y : Local.Min.Y,
					SZ ? Local.Max.Z : Local.Min.Z));
			}
		}
	}
	return Out;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExpansionPreviewTransformTest,
	"CompanyGrowth.Office.ExpansionCell.PreviewTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGROfficeExpansionPreviewTransformTest::RunTest(const FString&)
{
	using namespace OfficeExpansionCellTests;

	// ── 가로 확장. 새 타일은 X[404,804] 이고 WallLeft 가 그만큼 길어진다.
	const FBox LeftWall = BoxUnder(WallMeshBox,
		FOfficeFootprintGeometry::MakeExpansionSegmentTransform(WallLeftXf, EAxis::X, 400.f));
	TestEqual(TEXT("가로 벽 토막 MinX"), LeftWall.Min.X, 403.7, 0.05);
	TestEqual(TEXT("가로 벽 토막 MaxX"), LeftWall.Max.X, 803.7, 0.05);
	TestTrue(TEXT("가로 벽 토막은 Y 로 얇다"), (LeftWall.Max.Y - LeftWall.Min.Y) < 21.0);
	TestEqual(TEXT("가로 벽 토막은 기존 벽과 같은 Y"), LeftWall.Min.Y, -9.7, 0.05);
	TestEqual(TEXT("가로 벽 토막 높이"), LeftWall.Max.Z - LeftWall.Min.Z, 420.0, 0.05);

	const FBox LeftFloor = BoxUnder(FloorMeshBox,
		FOfficeFootprintGeometry::MakeExpansionSegmentTransform(FloorXf, EAxis::X, -400.f));
	TestEqual(TEXT("가로 바닥 토막 MinX"), LeftFloor.Min.X, 403.8, 0.05);
	TestEqual(TEXT("가로 바닥 토막 MaxX"), LeftFloor.Max.X, 803.8, 0.05);
	TestEqual(TEXT("가로 바닥 토막은 기존 바닥과 같은 Y 범위"), LeftFloor.Min.Y, -809.7, 0.05);
	TestEqual(TEXT("가로 바닥 토막 MaxY"), LeftFloor.Max.Y, -9.7, 0.05);

	// ── 세로 확장. 새 타일은 Y[-1209,-809] 이고 WallRight 가 그만큼 길어진다.
	const FBox RightWall = BoxUnder(WallMeshBox,
		FOfficeFootprintGeometry::MakeExpansionSegmentTransform(WallRightXf, EAxis::X, 400.f));
	TestEqual(TEXT("세로 벽 토막 MinY"), RightWall.Min.Y, -1209.7, 0.05);
	TestEqual(TEXT("세로 벽 토막 MaxY"), RightWall.Max.Y, -809.7, 0.05);
	TestTrue(TEXT("세로 벽 토막은 X 로 얇다"), (RightWall.Max.X - RightWall.Min.X) < 21.0);
	TestEqual(TEXT("세로 벽 토막은 기존 벽과 같은 X"), RightWall.Min.X, -16.3, 0.05);

	const FBox RightFloor = BoxUnder(FloorMeshBox,
		FOfficeFootprintGeometry::MakeExpansionSegmentTransform(FloorXf, EAxis::Z, 400.f));
	TestEqual(TEXT("세로 바닥 토막 MinY"), RightFloor.Min.Y, -1209.7, 0.05);
	TestEqual(TEXT("세로 바닥 토막 MaxY"), RightFloor.Max.Y, -809.7, 0.05);
	TestEqual(TEXT("세로 바닥 토막은 기존 바닥과 같은 X 범위"), RightFloor.Min.X, 3.8, 0.05);
	TestEqual(TEXT("세로 바닥 토막 MaxX"), RightFloor.Max.X, 403.8, 0.05);

	// ── 띠로 줄여도 **시작점은 기존 경계에 붙어 있어야** 한다. 길이만 줄어든다.
	const FBox LeftBand = BoxUnder(FloorMeshBox,
		FOfficeFootprintGeometry::MakeExpansionSegmentTransform(FloorXf, EAxis::X, -400.f, 0.25f));
	TestEqual(TEXT("띠도 기존 경계에서 시작한다"), LeftBand.Min.X, 403.8, 0.05);
	TestEqual(TEXT("띠 길이는 한 칸의 25%"), LeftBand.Max.X, 503.8, 0.05);
	TestEqual(TEXT("띠의 다른 축은 그대로"), LeftBand.Min.Y, -809.7, 0.05);

	const FBox RightBand = BoxUnder(FloorMeshBox,
		FOfficeFootprintGeometry::MakeExpansionSegmentTransform(FloorXf, EAxis::Z, 400.f, 0.25f));
	TestEqual(TEXT("세로 띠도 경계에서 시작"), RightBand.Max.Y, -809.7, 0.05);
	TestEqual(TEXT("세로 띠 길이는 한 칸의 25%"), RightBand.Min.Y, -909.7, 0.05);

	return true;
}

#endif
