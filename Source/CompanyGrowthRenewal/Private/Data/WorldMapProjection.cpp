#include "Data/WorldMapProjection.h"

FVector2D FWorldMapProjection::ToAnchor(double Lon, double Lat)
{
	return FVector2D((Lon - Lon0) / (Lon1 - Lon0), (Lat0 - Lat) / (Lat0 - Lat1));
}

double FWorldMapProjection::AnchorDistance(const FVector2D& A, const FVector2D& B)
{
	const double DX = A.X - B.X;
	const double DY = (A.Y - B.Y) / AspectRatio;
	return FMath::Sqrt(DX * DX + DY * DY);
}

bool FWorldMapProjection::IsAnchorSet(const FVector2D& Anchor)
{
	return !Anchor.IsNearlyZero(1e-4);
}
