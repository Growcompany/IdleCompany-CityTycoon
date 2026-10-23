#pragma once

#include "CoreMinimal.h"

// 증명사진 PNG 의 알파 에지에 외곽선을 굽는다. 라이브 무대 RT 는 M_GachaRevealRT 가 같은 수식을 셰이더로 수행.
struct FPortraitOutlineParams
{
	float ThicknessPx = 1.5f;           // 이미지 픽셀 단위 (호출자가 해상도 비례 환산)
	float Opacity = 0.8f;
	FColor Color = FColor(97, 90, 90);  // sRGB 바이트 = linear (0.12, 0.10, 0.10)
};

namespace PortraitOutline
{
	// Pixels: straight alpha, A = 캐릭터 커버리지(0=배경). 제자리 수정.
	void Apply(TArray<FColor>& Pixels, int32 Width, int32 Height, const FPortraitOutlineParams& Params);
}
