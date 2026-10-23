#include "Entity/OverseasDiorama/DioramaGeometry.h"

namespace
{
	FVector DioRotateX(const FVector& V, double Angle)
	{
		const double C = FMath::Cos(Angle);
		const double S = FMath::Sin(Angle);
		return FVector(V.X, V.Y * C - V.Z * S, V.Y * S + V.Z * C);
	}

	FVector DioRotateY(const FVector& V, double Angle)
	{
		const double C = FMath::Cos(Angle);
		const double S = FMath::Sin(Angle);
		return FVector(V.X * C + V.Z * S, V.Y, V.Z * C - V.X * S);
	}

	FVector DioRotateZ(const FVector& V, double Angle)
	{
		const double C = FMath::Cos(Angle);
		const double S = FMath::Sin(Angle);
		return FVector(V.X * C - V.Y * S, V.X * S + V.Y * C, V.Z);
	}

	// three.js Euler 'XYZ' = Rx * Ry * Rz (Rz 먼저 적용).
	FMatrix DioLocalMatrix(double X, double Y, double Z, double RX, double RY, double RZ)
	{
		auto Rotate = [RX, RY, RZ](const FVector& Axis)
		{
			return DioRotateX(DioRotateY(DioRotateZ(Axis, RZ), RY), RX);
		};
		return FMatrix(Rotate(FVector::XAxisVector), Rotate(FVector::YAxisVector), Rotate(FVector::ZAxisVector), FVector(X, Y, Z));
	}

	FVector DioNormalToUE(const FMatrix& Xf, const FVector& LocalNormal)
	{
		const FVector N = FVector(Xf.TransformVector(LocalNormal));
		return FVector(-N.Z, N.X, N.Y).GetSafeNormal();
	}

	int32 DioAddVertex(FDioramaMeshBucket& Bucket, const FMatrix& Xf, const FVector& Local, const FVector& NormalUE)
	{
		const FVector P = FVector(Xf.TransformPosition(Local));
		Bucket.Vertices.Add(FDioramaGeometry::ToUE(P.X, P.Y, P.Z));
		Bucket.Normals.Add(NormalUE);
		return Bucket.Vertices.Num() - 1;
	}

	// 시안 공간에서 밖에서 봐 반시계(three.js 앞면)인 순서를 그대로 쓴다: 축 변환 행렬식 −1이 UE 앞면 규칙으로 뒤집어 준다.
	void DioAddTriangle(FDioramaMeshBucket& Bucket, int32 A, int32 B, int32 C)
	{
		Bucket.Triangles.Add(A);
		Bucket.Triangles.Add(B);
		Bucket.Triangles.Add(C);
	}

	FVector DioRim(double Radius, double Theta, double PY)
	{
		return FVector(Radius * FMath::Sin(Theta), PY, Radius * FMath::Cos(Theta));
	}

	void DioAddCap(FDioramaMeshBucket& Bucket, const FMatrix& Xf, double Radius, double PY, int32 SegmentCount, bool bTop)
	{
		const FVector NormalUE = DioNormalToUE(Xf, FVector(0.0, bTop ? 1.0 : -1.0, 0.0));
		const int32 Center = DioAddVertex(Bucket, Xf, FVector(0.0, PY, 0.0), NormalUE);
		const int32 First = Bucket.Vertices.Num();
		const double Step = UE_DOUBLE_TWO_PI / SegmentCount;
		for (int32 Seg = 0; Seg < SegmentCount; ++Seg)
		{
			DioAddVertex(Bucket, Xf, DioRim(Radius, Step * Seg, PY), NormalUE);
		}
		for (int32 Seg = 0; Seg < SegmentCount; ++Seg)
		{
			const int32 RimA = First + Seg;
			const int32 RimB = First + (Seg + 1) % SegmentCount;
			if (bTop)
			{
				DioAddTriangle(Bucket, Center, RimA, RimB);
			}
			else
			{
				DioAddTriangle(Bucket, Center, RimB, RimA);
			}
		}
	}
}

FVector FDioramaGeometry::ToUE(double X, double Y, double Z)
{
	return FVector(-Z, X, Y) * 100.0;
}

FMatrix FDioramaGeometry::CurrentFrame() const
{
	return Frames.IsEmpty() ? FMatrix::Identity : Frames.Last();
}

void FDioramaGeometry::PushFrame(double X, double Y, double Z, double RX, double RY, double RZ)
{
	Frames.Add(DioLocalMatrix(X, Y, Z, RX, RY, RZ) * CurrentFrame());
}

void FDioramaGeometry::PopFrame()
{
	if (ensureMsgf(!Frames.IsEmpty(), TEXT("FDioramaGeometry::PopFrame without matching PushFrame")))
	{
		Frames.Pop();
	}
}

void FDioramaGeometry::Box(double W, double H, double D, FColor Color, double X, double Y, double Z, double RX, double RY, double RZ)
{
	const FMatrix Xf = DioLocalMatrix(X, Y, Z, RX, RY, RZ) * CurrentFrame();
	FDioramaMeshBucket& Bucket = Buckets.FindOrAdd(Color.ToPackedARGB());
	const FVector Half(W * 0.5, H * 0.5, D * 0.5);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (const double Sign : { 1.0, -1.0 })
		{
			FVector N = FVector::ZeroVector;
			N[Axis] = Sign;
			// U×V = N 이 되게 골라 (−U−V → +U−V → +U+V → −U+V)가 밖에서 봐 반시계가 되게 한다.
			const int32 AxisU = (Axis + (Sign > 0.0 ? 1 : 2)) % 3;
			const int32 AxisV = (Axis + (Sign > 0.0 ? 2 : 1)) % 3;
			FVector U = FVector::ZeroVector;
			FVector V = FVector::ZeroVector;
			U[AxisU] = Half[AxisU];
			V[AxisV] = Half[AxisV];
			const FVector FaceCenter = N * Half;

			const FVector NormalUE = DioNormalToUE(Xf, N);
			const int32 V0 = DioAddVertex(Bucket, Xf, FaceCenter - U - V, NormalUE);
			const int32 V1 = DioAddVertex(Bucket, Xf, FaceCenter + U - V, NormalUE);
			const int32 V2 = DioAddVertex(Bucket, Xf, FaceCenter + U + V, NormalUE);
			const int32 V3 = DioAddVertex(Bucket, Xf, FaceCenter - U + V, NormalUE);
			DioAddTriangle(Bucket, V0, V1, V2);
			DioAddTriangle(Bucket, V0, V2, V3);
		}
	}
}

void FDioramaGeometry::BoxB(double W, double H, double D, FColor Color, double X, double Y, double Z, double RY)
{
	Box(W, H, D, Color, X, Y + H * 0.5, Z, 0.0, RY, 0.0);
}

void FDioramaGeometry::Cyl(double RTop, double RBottom, double H, FColor Color, double X, double Y, double Z, int32 Segments, double RX, double RY, double RZ)
{
	const bool bTopRim = RTop > 0.0;
	const bool bBottomRim = RBottom > 0.0;
	if (!bTopRim && !bBottomRim)
	{
		return;
	}

	const int32 SegmentCount = FMath::Max(Segments, 3);
	const FMatrix Xf = DioLocalMatrix(X, Y, Z, RX, RY, RZ) * CurrentFrame();
	FDioramaMeshBucket& Bucket = Buckets.FindOrAdd(Color.ToPackedARGB());
	const double HalfH = H * 0.5;
	const double Step = UE_DOUBLE_TWO_PI / SegmentCount;
	const double HalfStepCos = FMath::Cos(Step * 0.5);

	// 정점 각도·순서는 three.js CylinderGeometry와 같다(x = r·sinθ, z = r·cosθ, a/b/c/d).
	for (int32 Seg = 0; Seg < SegmentCount; ++Seg)
	{
		const double Theta0 = Step * Seg;
		const double Theta1 = Step * (Seg + 1);
		const double ThetaMid = (Theta0 + Theta1) * 0.5;
		const FVector SideNormal = FVector(H * FMath::Sin(ThetaMid), (RBottom - RTop) * HalfStepCos, H * FMath::Cos(ThetaMid)).GetSafeNormal();
		const FVector NormalUE = DioNormalToUE(Xf, SideNormal);

		if (!bTopRim)
		{
			const int32 B = DioAddVertex(Bucket, Xf, DioRim(RBottom, Theta0, -HalfH), NormalUE);
			const int32 C = DioAddVertex(Bucket, Xf, DioRim(RBottom, Theta1, -HalfH), NormalUE);
			const int32 Apex = DioAddVertex(Bucket, Xf, FVector(0.0, HalfH, 0.0), NormalUE);
			DioAddTriangle(Bucket, B, C, Apex);
		}
		else if (!bBottomRim)
		{
			const int32 A = DioAddVertex(Bucket, Xf, DioRim(RTop, Theta0, HalfH), NormalUE);
			const int32 Apex = DioAddVertex(Bucket, Xf, FVector(0.0, -HalfH, 0.0), NormalUE);
			const int32 D = DioAddVertex(Bucket, Xf, DioRim(RTop, Theta1, HalfH), NormalUE);
			DioAddTriangle(Bucket, A, Apex, D);
		}
		else
		{
			const int32 A = DioAddVertex(Bucket, Xf, DioRim(RTop, Theta0, HalfH), NormalUE);
			const int32 B = DioAddVertex(Bucket, Xf, DioRim(RBottom, Theta0, -HalfH), NormalUE);
			const int32 C = DioAddVertex(Bucket, Xf, DioRim(RBottom, Theta1, -HalfH), NormalUE);
			const int32 D = DioAddVertex(Bucket, Xf, DioRim(RTop, Theta1, HalfH), NormalUE);
			DioAddTriangle(Bucket, A, B, D);
			DioAddTriangle(Bucket, B, C, D);
		}
	}

	if (bTopRim)
	{
		DioAddCap(Bucket, Xf, RTop, HalfH, SegmentCount, true);
	}
	if (bBottomRim)
	{
		DioAddCap(Bucket, Xf, RBottom, -HalfH, SegmentCount, false);
	}
}

void FDioramaGeometry::CylB(double RTop, double RBottom, double H, FColor Color, double X, double Y, double Z, int32 Segments, double RX, double RY, double RZ)
{
	Cyl(RTop, RBottom, H, Color, X, Y + H * 0.5, Z, Segments, RX, RY, RZ);
}

int32 FDioramaGeometry::NumTriangles() const
{
	int32 Count = 0;
	for (const TPair<uint32, FDioramaMeshBucket>& Pair : Buckets)
	{
		Count += Pair.Value.Triangles.Num() / 3;
	}
	return Count;
}

FBox FDioramaGeometry::GetBoundsUE() const
{
	FBox Bounds(ForceInit);
	for (const TPair<uint32, FDioramaMeshBucket>& Pair : Buckets)
	{
		for (const FVector& Vertex : Pair.Value.Vertices)
		{
			Bounds += Vertex;
		}
	}
	return Bounds;
}

void FDioramaGeometry::Reset()
{
	Buckets.Reset();
	Frames.Reset();
}
