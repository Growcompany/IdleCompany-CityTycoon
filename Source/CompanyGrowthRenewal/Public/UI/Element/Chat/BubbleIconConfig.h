// 버블 아이콘 설정 DataAsset
// 에디터에서 DA_BubbleIconConfig 에셋을 생성하여 타입별 셸 머티리얼/아이콘 지정

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Enum/BubbleType.h"
#include "BubbleIconConfig.generated.h"

class UMaterialInterface;

UCLASS()
class COMPANYGROWTHRENEWAL_API UBubbleIconConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// 타입별 셸 머티리얼 (MI_UI_BubbleShell_*). 상태색은 MI 가 소유한다
	UPROPERTY(EditDefaultsOnly, Category = "Bubble")
	TMap<EBubbleType, TSoftObjectPtr<UMaterialInterface>> ShellMap;

	// ShellMap 에 없는 타입이 남았을 때의 텍스처 폴백
	UPROPERTY(EditDefaultsOnly, Category = "Bubble")
	TSoftObjectPtr<UTexture2D> BubbleBG;

	// IconMap에 없는 타입에 사용할 기본 아이콘
	UPROPERTY(EditDefaultsOnly, Category = "Bubble")
	TSoftObjectPtr<UTexture2D> DefaultIcon;

	// 타입별 아이콘 텍스처 매핑 (없으면 DefaultIcon 사용)
	UPROPERTY(EditDefaultsOnly, Category = "Bubble")
	TMap<EBubbleType, TSoftObjectPtr<UTexture2D>> IconMap;
};
