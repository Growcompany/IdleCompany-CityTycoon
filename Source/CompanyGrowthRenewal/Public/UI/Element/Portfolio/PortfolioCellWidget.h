#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "PortfolioCellWidget.generated.h"

class UImage;
class UBorder;
class UCommonTextBlock;
class UTexture2D;

/**
 * 포트폴리오 셀 1개 (UIE_PortfolioCell) — 발견/미발견/잠금 한 WBP + 상태 토글.
 * 생김새(크림 플레이트+음각 well+등급 배지+접지그림자+광택)는 WBP 트리, 상태/데이터는 CodexPanel C++가 구동.
 * ★ 열람 전용 — 착수는 기획 보드가 단일 창구다(2026-07-26 셀 클릭 착수 제거).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPortfolioCellWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 발견 셀: 크림 플레이트 + 산업색 다크 well + 등급 배지 + 이름/누적매출 + 접지그림자.
	// IconTex 있으면 라운드 머티리얼(M_UI_RoundedThumb) MID 로 실제 게임 이미지 주입, 없으면 디자인타임 기본 유지.
	void ConfigureDiscovered(const FText& Name, const FString& Rarity, const FText& Sales, const FLinearColor& IndustryColor, UTexture2D* IconTex);

	// 미발견 셀: 다크 플레이트 + 음각 well + '?' + 뮤트 등급힌트/소재힌트
	void ConfigureUndiscovered(const FString& RarityHint, const FText& MaterialHint);

	// 잠금 셀(현재/미래 티어): 미발견보다 더 뮤트 + 잠금 힌트. 클릭은 CodexPanel이 델리게이트 미바인딩으로 비활성.
	void ConfigureLocked(const FString& RarityHint);

protected:
	// 카드 플레이트 (SetBrushColor: 크림 vs 다크)
	UPROPERTY(meta = (BindWidget))
	UBorder* Plate;

	// 음각 썸네일 well (SetBrushTintColor: 산업다크 vs 다크)
	UPROPERTY(meta = (BindWidget))
	UImage* WellBG;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* NameText;

	// 발견=누적매출 / 미발견=소재힌트 공용
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* SalesText;

	// 등급 배지 (SetBrushColor: 등급색 vs 뮤트)
	UPROPERTY(meta = (BindWidget))
	UBorder* RarityBadge;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* BadgeText;

	// 미발견 '?' 오버레이
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* QText;

	// 접지 그림자 (발견만 표시)
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* GroundShadow;

	// well 상단 광택
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* WellGloss;

	// 제품 썸네일 (데이터 배선 전엔 숨김)
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* ThumbnailImage;
};
