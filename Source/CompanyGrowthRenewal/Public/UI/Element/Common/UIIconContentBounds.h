#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "UIIconContentBounds.generated.h"

class UImage;

// 아이콘 텍스처의 보이는 그림 상자(알파 > 24/255, 0~1 정규화). Tools/LiveOps/icon_bounds 가 재서 텍스처 AssetUserData 로 붙인다.
UCLASS(BlueprintType)
class COMPANYGROWTHRENEWAL_API UUIIconContentBounds : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "IconContent")
	FVector2f Min = FVector2f(0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "IconContent")
	FVector2f Max = FVector2f(1.f, 1.f);
};

// 투명 여백이 제각각인 아이콘을 같은 겉크기로 — 그림 상자만 UV 로 잘라 Box 안에 비율대로 꽉 채운다
struct COMPANYGROWTHRENEWAL_API FUIIconContentFit
{
	// ContentAspectSize 의 비율을 지키며 Box 안에 들어가는 가장 큰 크기. 비율이 퇴화면 Box 그대로.
	static FVector2D FitSize(FVector2D Box, FVector2D ContentAspectSize);

	// Resource 가 null 이 아니면 브러시 리소스도 그것으로 바꾼다. 여백 정보가 있는 UTexture2D 면 UV 를 그림 상자로 자르고
	// 크기 = FitSize 를 브러시 ImageSize 에 넣고 true. 아니면 UV 를 비우고 크기 = FitSize(Box, 텍스처 크기 ― 머티리얼은 정사각), false.
	// 부모 슬롯이 Fill 정렬이면 ImageSize 가 무시돼 늘어나므로 가운데 정렬 슬롯 안에서 쓸 것.
	static bool Apply(UImage* Image, UObject* Resource, FVector2D Box);
};
