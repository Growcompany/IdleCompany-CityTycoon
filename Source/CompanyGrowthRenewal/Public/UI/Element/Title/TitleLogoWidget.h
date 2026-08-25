#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TitleLogoWidget.generated.h"

class UImage;

/**
 * 타이틀 화면 로고 위젯 (WBP: UIE_TitleLogo)
 * - 로고 = 배지 PNG 1장 (T_TitleLogo_Badge — 빌딩 클러스터 뒷판+벽돌+"회사키우기").
 *   배지가 자체 대비(스카이블렌드 플레이트)를 가져 별도 백플레이트 불필요.
 * - 정적 표시 전용 — 인트로/idle 애니메이션은 2026-07-06 사용자 결정으로 제거.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTitleLogoWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Badge = nullptr;
};
