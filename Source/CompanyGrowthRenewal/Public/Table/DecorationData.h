#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DecorationData.generated.h"

/**
 * FDecorationData
 *
 * 장식품의 비주얼 데이터만 관리하는 구조체
 * - Mesh: 장식품의 Static Mesh
 * - Material: 장식품의 머티리얼 (옵션)
 *
 * 게임플레이 데이터(가격, 해금 레벨 등)는 DecorationCardTable에서 관리됨
 */
USTRUCT(BlueprintType)
struct FDecorationData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 장식품 메시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> DecorationMesh;

	// 장식품 머티리얼 (설정 시 메시의 기본 머티리얼 오버라이드)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TSoftObjectPtr<UMaterialInterface> DecorationMaterial;

	FDecorationData()
		: DecorationMesh(nullptr)
		, DecorationMaterial(nullptr)
	{}
};
