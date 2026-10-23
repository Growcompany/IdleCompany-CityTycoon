#pragma once

#include "CoreMinimal.h"

class UImage;

// 아이콘 브러시를 텍스처/머티리얼 구분 없이 적용 — 상점 아이템 아이콘이 MI(티어색 파라미터)일 수 있어 SetBrushFromTexture 대체
namespace CGIconBrush
{
	// ImageSize 가 0 이면 기존 브러시 크기 유지(SetBrushFromTexture(bMatchSize=false) 와 동일). Resource null 이면 아무것도 안 함.
	void ApplyResource(UImage* Image, UObject* Resource, FVector2D ImageSize = FVector2D::ZeroVector);
}
