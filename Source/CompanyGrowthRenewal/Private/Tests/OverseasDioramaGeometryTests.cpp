#include "Misc/AutomationTest.h"
#include "Entity/OverseasDiorama/DioramaGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FColor kDioTestColor(10, 20, 30);

	// 볼록 도형 하나만 담긴 G 에서: UE 앞면 규칙 위반 수 + 법선이 안쪽을 향하는 면 수(정점 평균 = 항상 내부점).
	int32 DioTestCountBadTriangles(const FDioramaGeometry& G)
	{
		FVector Center = FVector::ZeroVector;
		int32 VertexCount = 0;
		for (const TPair<uint32, FDioramaMeshBucket>& Pair : G.GetBuckets())
		{
			for (const FVector& Vertex : Pair.Value.Vertices)
			{
				Center += Vertex;
				++VertexCount;
			}
		}
		Center /= static_cast<double>(FMath::Max(VertexCount, 1));
		int32 Bad = 0;
		for (const TPair<uint32, FDioramaMeshBucket>& Pair : G.GetBuckets())
		{
			const FDioramaMeshBucket& Bucket = Pair.Value;
			for (int32 Tri = 0; Tri + 2 < Bucket.Triangles.Num(); Tri += 3)
			{
				const FVector& A = Bucket.Vertices[Bucket.Triangles[Tri]];
				const FVector& B = Bucket.Vertices[Bucket.Triangles[Tri + 1]];
				const FVector& C = Bucket.Vertices[Bucket.Triangles[Tri + 2]];
				const FVector& N = Bucket.Normals[Bucket.Triangles[Tri]];
				const bool bFront = (((C - A) ^ (B - A)) | N) > 0.0;
				const bool bOutward = ((((A + B + C) / 3.0) - Center) | N) > 0.0;
				if (!bFront || !bOutward)
				{
					++Bad;
				}
			}
		}
		return Bad;
	}

	int32 DioTestCountFacing(const FDioramaGeometry& G, const FVector& Dir)
	{
		int32 Count = 0;
		for (const TPair<uint32, FDioramaMeshBucket>& Pair : G.GetBuckets())
		{
			const FDioramaMeshBucket& Bucket = Pair.Value;
			for (int32 Tri = 0; Tri + 2 < Bucket.Triangles.Num(); Tri += 3)
			{
				if ((Bucket.Normals[Bucket.Triangles[Tri]] | Dir) > 0.999)
				{
					++Count;
				}
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasGeometryToUETest,
	"CGR.Overseas.Geometry.ToUE",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasGeometryToUETest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("(1,2,3) -> (-300,100,200)"), FDioramaGeometry::ToUE(1, 2, 3), FVector(-300, 100, 200));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasGeometryBoxBTest,
	"CGR.Overseas.Geometry.BoxB",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasGeometryBoxBTest::RunTest(const FString& Parameters)
{
	FDioramaGeometry G;
	G.BoxB(2, 1, 4, kDioTestColor, 0, 0, 0);
	TestEqual(TEXT("triangles"), G.NumTriangles(), 12);
	TestEqual(TEXT("buckets"), G.GetBuckets().Num(), 1);
	TestTrue(TEXT("bucket key = packed ARGB"), G.GetBuckets().Contains(kDioTestColor.ToPackedARGB()));
	if (const FDioramaMeshBucket* Bucket = G.GetBuckets().Find(kDioTestColor.ToPackedARGB()))
	{
		TestEqual(TEXT("flat shading = 24 vertices"), Bucket->Vertices.Num(), 24);
		TestEqual(TEXT("one normal per vertex"), Bucket->Normals.Num(), 24);
	}
	const FBox Bounds = G.GetBoundsUE();
	TestTrue(TEXT("bounds"), Bounds.Equals(FBox(FVector(-200, -100, 0), FVector(200, 100, 100)), 0.01));
	TestFalse(TEXT("not empty"), G.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasGeometryWindingTest,
	"CGR.Overseas.Geometry.Winding",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasGeometryWindingTest::RunTest(const FString& Parameters)
{
	{
		FDioramaGeometry G;
		G.Box(2, 1, 3, kDioTestColor, 0, 0, 0);
		TestEqual(TEXT("axis box"), DioTestCountBadTriangles(G), 0);
	}
	{
		FDioramaGeometry G;
		G.PushFrame(3, 1, -2, 0.3, 0.7, -0.4);
		G.Box(2, 1, 3, kDioTestColor, 0.5, 0.2, -0.1, 0.2, -0.5, 0.9);
		G.PopFrame();
		TestEqual(TEXT("rotated box in rotated frame"), DioTestCountBadTriangles(G), 0);
	}
	{
		FDioramaGeometry G;
		G.Cyl(1, 1, 2, kDioTestColor, 0, 0, 0);
		TestEqual(TEXT("cylinder triangles"), G.NumTriangles(), 48);
		TestEqual(TEXT("cylinder"), DioTestCountBadTriangles(G), 0);
	}
	{
		FDioramaGeometry G;
		G.PushFrame(-4, 2, 5, 0, 1.1, 0);
		G.Cyl(0.6, 1.0, 2, kDioTestColor, 1, 0, 0, 12, 0.4, 0, -0.8);
		G.PopFrame();
		TestEqual(TEXT("rotated frustum"), DioTestCountBadTriangles(G), 0);
	}
	{
		FDioramaGeometry G;
		G.CylB(0, 1, 2, kDioTestColor, 0, 0, 0, 8);
		TestEqual(TEXT("cone"), DioTestCountBadTriangles(G), 0);
	}
	{
		FDioramaGeometry G;
		G.Cyl(1, 0, 1.5, kDioTestColor, 0, 0, 0, 6, 0.5, 0.25, 0);
		TestEqual(TEXT("inverted cone triangles"), G.NumTriangles(), 12);
		TestEqual(TEXT("inverted cone"), DioTestCountBadTriangles(G), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasGeometryFrameYawTest,
	"CGR.Overseas.Geometry.FrameYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasGeometryFrameYawTest::RunTest(const FString& Parameters)
{
	{
		FDioramaGeometry G;
		G.PushFrame(5, 0, 0, 0, UE_DOUBLE_HALF_PI, 0);
		G.BoxB(1, 1, 1, kDioTestColor, 0, 0, 0);
		G.PopFrame();
		TestEqual(TEXT("yawed frame bottom box center"), G.GetBoundsUE().GetCenter(), FDioramaGeometry::ToUE(5, 0.5, 0), 0.01f);
	}
	{
		FDioramaGeometry G;
		G.PushFrame(0, 0, 0, 0, UE_DOUBLE_HALF_PI, 0);
		G.Box(2, 1, 1, kDioTestColor, 1, 0, 0);
		G.PopFrame();
		const FBox Bounds = G.GetBoundsUE();
		TestEqual(TEXT("three.js +x turns to -z"), Bounds.GetCenter(), FDioramaGeometry::ToUE(0, 0, -1), 0.01f);
		TestEqual(TEXT("width now along mockup z"), Bounds.GetExtent(), FVector(100, 50, 50), 0.01f);
	}
	{
		FDioramaGeometry G;
		G.PushFrame(5, 0, 0, 0, UE_DOUBLE_HALF_PI, 0);
		G.PushFrame(0, 0, 0, 0, UE_DOUBLE_HALF_PI, 0);
		G.Box(1, 1, 1, kDioTestColor, 1, 0, 0);
		G.PopFrame();
		G.PopFrame();
		TestEqual(TEXT("nested frames compose"), G.GetBoundsUE().GetCenter(), FDioramaGeometry::ToUE(4, 0, 0), 0.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasGeometryConeTest,
	"CGR.Overseas.Geometry.Cone",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasGeometryConeTest::RunTest(const FString& Parameters)
{
	FDioramaGeometry G;
	G.CylB(0, 1, 2, kDioTestColor, 0, 0, 0, 12);
	TestEqual(TEXT("12 side + 12 bottom cap"), G.NumTriangles(), 24);
	TestEqual(TEXT("no top cap"), DioTestCountFacing(G, FVector::UpVector), 0);
	TestEqual(TEXT("bottom cap"), DioTestCountFacing(G, FVector::DownVector), 12);
	const FBox Bounds = G.GetBoundsUE();
	TestEqual(TEXT("bottom at y"), Bounds.Min.Z, 0.0, 0.01);
	TestEqual(TEXT("apex at y + h"), Bounds.Max.Z, 200.0, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasGeometryColorsMergeTest,
	"CGR.Overseas.Geometry.ColorsMerge",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasGeometryColorsMergeTest::RunTest(const FString& Parameters)
{
	FDioramaGeometry G;
	G.Box(1, 1, 1, kDioTestColor, 0, 0, 0);
	G.Box(1, 2, 1, kDioTestColor, 3, 0, 0);
	G.BoxB(2, 1, 1, kDioTestColor, 0, 0, 4, 0.5);
	TestEqual(TEXT("same color = one bucket"), G.GetBuckets().Num(), 1);
	TestEqual(TEXT("3 boxes = 36 triangles"), G.NumTriangles(), 36);

	G.Box(1, 1, 1, FColor(200, 100, 50), 0, 0, 0);
	TestEqual(TEXT("new color = new bucket"), G.GetBuckets().Num(), 2);
	TestEqual(TEXT("4 boxes = 48 triangles"), G.NumTriangles(), 48);

	G.PushFrame(1, 0, 0);
	G.Reset();
	TestTrue(TEXT("reset empties"), G.IsEmpty());
	TestEqual(TEXT("reset clears buckets"), G.GetBuckets().Num(), 0);
	G.Box(1, 1, 1, kDioTestColor, 0, 0, 0);
	TestEqual(TEXT("reset clears frame stack"), G.GetBoundsUE().GetCenter(), FVector::ZeroVector, 0.01f);
	return true;
}

#endif
