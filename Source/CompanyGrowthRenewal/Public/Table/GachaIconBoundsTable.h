#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GachaIconBoundsTable.generated.h"

// The CSV records the nontransparent alpha rectangle in original texture UVs.
USTRUCT(BlueprintType)
struct FGachaIconBoundsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gacha|Icon")
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gacha|Icon")
	FVector2D MinUV = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gacha|Icon")
	FVector2D MaxUV = FVector2D::ZeroVector;

	bool HasValidBounds() const
	{
		return !Texture.IsNull() && FMath::IsFinite(MinUV.X) && FMath::IsFinite(MinUV.Y) &&
			FMath::IsFinite(MaxUV.X) && FMath::IsFinite(MaxUV.Y) &&
			MinUV.X >= 0.0 && MinUV.Y >= 0.0 && MaxUV.X <= 1.0 && MaxUV.Y <= 1.0 &&
			MaxUV.X > MinUV.X && MaxUV.Y > MinUV.Y;
	}

	FBox2d UVRegion() const { return FBox2d(MinUV, MaxUV); }
};
