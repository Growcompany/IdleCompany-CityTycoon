#pragma once

#include "CoreMinimal.h"

struct FDioramaMeshBucket
{
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<int32> Triangles;
};

// 입력은 시안 공간(미터, Y 위, three.js 오른손), 버킷 정점은 UE 공간(cm).
class COMPANYGROWTHRENEWAL_API FDioramaGeometry
{
public:
	static FVector ToUE(double X, double Y, double Z);

	void PushFrame(double X, double Y, double Z, double RX = 0, double RY = 0, double RZ = 0);
	void PopFrame();

	void Box(double W, double H, double D, FColor Color, double X, double Y, double Z, double RX = 0, double RY = 0, double RZ = 0);
	void BoxB(double W, double H, double D, FColor Color, double X, double Y, double Z, double RY = 0);
	void Cyl(double RTop, double RBottom, double H, FColor Color, double X, double Y, double Z, int32 Segments = 12, double RX = 0, double RY = 0, double RZ = 0);
	void CylB(double RTop, double RBottom, double H, FColor Color, double X, double Y, double Z, int32 Segments = 12, double RX = 0, double RY = 0, double RZ = 0);

	const TMap<uint32, FDioramaMeshBucket>& GetBuckets() const { return Buckets; }
	int32 NumTriangles() const;
	FBox GetBoundsUE() const;
	bool IsEmpty() const { return NumTriangles() == 0; }
	void Reset();

private:
	FMatrix CurrentFrame() const;

	TMap<uint32, FDioramaMeshBucket> Buckets;
	// UE 행벡터 규약(v * M)으로 담는다: 시안 그룹 계층 = Local * Parent.
	TArray<FMatrix> Frames;
};
