// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YieldRangeBarWidget.generated.h"

// 리빌 완료 통지 — 소비자(인수 모달)가 토스트/닫기를 수행. 코드베이스 관례(ConfirmCancelWidget) 형태.
DECLARE_MULTICAST_DELEGATE(FOnYieldRevealFinished);

/**
 * 회수 구간 바 차트 (UIE_YieldRangeBar) — [Min,Max] 균등분포 도박을 한 줄로 읽히게.
 *
 * 트랙/채움/본전 눈금/기댓값 마커/축 라벨 전부 NativePaint(MakeCustomVerts+MakeLines+MakeText) — 텍스처/자식 트리 0.
 * WBP 는 빈 트리(자가 SizeBox 루트, DisciplineRadar 패턴) — 데이터·등급색은 CityCompanyInfo 가 주입.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UYieldRangeBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 회수 구간(%) + 기댓값(%) + 등급색 — 미호출 상태에서는 빈 트랙만 그린다
	void SetYieldRange(int32 InMinPct, int32 InMaxPct, int32 InEvPct, const FLinearColor& InGradeColor);

	// 굴린 회수율(%)로 결과 리빌 재생. 완료 시 OnRevealFinished 브로드캐스트.
	void PlayResultReveal(int32 RolledPct);

	FOnYieldRevealFinished OnRevealFinished;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ===== 노브 (QHD 캔버스 px) =====

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float BarWidth = 948.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float BarHeight = 96.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float TrackHeight = 26.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float CornerRadius = 8.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float FillAlpha = 0.85f;

	// 본전 눈금이 트랙 위아래로 삐져나오는 길이
	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float TickOverhang = 8.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float TickThickness = 3.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float MarkerWidth = 22.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	float MarkerHeight = 14.f;

	// 축 최소 도메인 — 저배당 회사도 본전 100% 눈금이 바 안쪽에 오게
	UPROPERTY(EditAnywhere, Category = "YieldBar")
	int32 AxisMinSpan = 120;

	// 결과 니들 — 트랙 하단에서 위를 찌른다. EV 마커는 상단이라 축이 갈려 동시 판독된다.
	UPROPERTY(EditAnywhere, Category = "YieldBar|Reveal")
	float NeedleWidth = 26.f;

	UPROPERTY(EditAnywhere, Category = "YieldBar|Reveal")
	float NeedleHeight = 18.f;

	// 본전 이상 착지색 — 라이트 플레이트 잉크용 진한 앰버(sRGB #B8770E → linear). 카탈로그 §4-A 골드는 다크 트랙 fill 틴트값이라 흰 플레이트 위에선 대비 1.2:1 로 안 보여 폐기
	UPROPERTY(EditAnywhere, Category = "YieldBar|Reveal")
	FLinearColor NeedleGoldColor = FLinearColor(0.479320f, 0.184475f, 0.004391f, 1.f);

	// 페이즈 길이(초). PIE 실측으로 확정할 것 — 아래는 시작점이다.
	UPROPERTY(EditAnywhere, Category = "YieldBar|Reveal", meta = (ClampMin = "0.0"))
	float SweepDuration = 0.6f;

	UPROPERTY(EditAnywhere, Category = "YieldBar|Reveal", meta = (ClampMin = "0.0"))
	float LandDuration = 0.35f;

	UPROPERTY(EditAnywhere, Category = "YieldBar|Reveal", meta = (ClampMin = "0.0"))
	float ImpactDuration = 0.5f;

	// 스윕 왕복 횟수
	UPROPERTY(EditAnywhere, Category = "YieldBar|Reveal", meta = (ClampMin = "1"))
	int32 SweepCycles = 3;

	UPROPERTY(EditAnywhere, Category = "YieldBar")
	FLinearColor TrackColor = FLinearColor(0.f, 0.f, 0.f, 0.07f);

	// 본전 눈금 잉크 #1A2330
	UPROPERTY(EditAnywhere, Category = "YieldBar")
	FLinearColor TickColor = FLinearColor(0.0103f, 0.0168f, 0.0296f, 1.f);

	// 축 라벨 잉크 — 라이트 플레이트 캡션 하한 #5A6B7D
	UPROPERTY(EditAnywhere, Category = "YieldBar")
	FLinearColor LabelColor = FLinearColor(0.1022f, 0.1471f, 0.2051f, 1.f);

	// 미지정 시 RebuildWidget 에서 NEXON Regular 21 로 채움 (WBP 인스턴스에서 교체 가능)
	UPROPERTY(EditAnywhere, Category = "YieldBar")
	FSlateFontInfo LabelFont;

private:
	int32 MinPct = 0;
	int32 MaxPct = 0;
	int32 EvPct = 0;
	bool bHasData = false;
	FLinearColor GradeColor = FLinearColor(0.239f, 0.608f, 0.878f, 1.f);

	// 리빌 시작 후 계속 true — 니들 표시 여부
	bool bRevealActive = false;
	// 착지 목표(굴린 회수율 %)
	int32 RevealPct = 0;

	// 리빌 경과(초). 애니메이션이 끝나면 bRevealAnimating=false 로 내려간다.
	bool bRevealAnimating = false;
	float RevealElapsed = 0.f;
	// 스윕 시작점 — 구간 좌단에서 출발
	float NeedleDisplayPct = 0.f;

	// 착지 결과음이 이미 재생됐는가 — 탭 스킵으로 RevealElapsed 가 점프해도 정확히 1회만 재생되게 보장
	bool bLandSoundPlayed = false;

	// 착지가 끝났는가 — 라벨/펀치의 기준.
	// ⚠ bRevealAnimating 은 임팩트 페이즈 동안에도 true 다(TotalEnd 에서야 내려감).
	//    그래서 "!bRevealAnimating" 으로 라벨을 걸면 모달이 닫히는 순간까지 라벨이 안 뜬다.
	bool IsRevealLanded() const
	{
		return bRevealActive && (!bRevealAnimating || RevealElapsed >= SweepDuration + LandDuration);
	}
};
