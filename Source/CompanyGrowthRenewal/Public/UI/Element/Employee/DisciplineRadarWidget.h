// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DisciplineRadarWidget.generated.h"

/**
 * 6직능 육각 레이더 차트 (UIE_DisciplineRadar) — 직원 특화 실루엣을 한눈에.
 *
 * 링/스포크/외곽선 = MakeLines, 폴리곤 채움 = MakeCustomVerts, 축 라벨 = MakeText — 텍스처/자식 트리 0.
 * WBP 는 빈 트리(자가 SizeBox 루트, SelectionChevron 패턴) — 데이터·등급색은 DisciplineCard 가 주입.
 * 목업 SOT = docs/05_UI/EmployeeWindow_FnArea_MOCKUP.html v7 (레이더 336, fill 등급색 20%)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UDisciplineRadarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 직능 원시값 6개 + 정규화 상한 + 등급색(GetRarityColor) — 스트로크는 등급색 다크 파생을 내부 계산
	void SetRadarData(const TArray<int32>& InValues, int32 InDisplayMax, const FLinearColor& InGradeColor);

	// 축 라벨 6개 (EProductionDiscipline DisplayName 순서 = 12시부터 시계방향)
	void SetAxisLabels(const TArray<FText>& InLabels);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ===== 노브 (QHD 캔버스 px) =====

	UPROPERTY(EditAnywhere, Category = "Radar")
	float RadarWidth = 336.f;

	UPROPERTY(EditAnywhere, Category = "Radar")
	float RadarHeight = 284.f;

	// 반경 = min(W,H)/2 - LabelInset (라벨 공간 확보)
	UPROPERTY(EditAnywhere, Category = "Radar")
	float LabelInset = 46.f;

	UPROPERTY(EditAnywhere, Category = "Radar")
	float LabelGap = 14.f;

	// 라이트 패널 위 그리드 라인 (#D9DDE3)
	UPROPERTY(EditAnywhere, Category = "Radar")
	FLinearColor RingColor = FLinearColor(0.687f, 0.717f, 0.766f, 1.f);

	// 축 라벨 잉크 — 라이트 플레이트 캡션 하한 #5A6B7D
	UPROPERTY(EditAnywhere, Category = "Radar")
	FLinearColor LabelColor = FLinearColor(0.102f, 0.147f, 0.205f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Radar")
	float RingThickness = 2.f;

	UPROPERTY(EditAnywhere, Category = "Radar")
	float StrokeThickness = 4.f;

	UPROPERTY(EditAnywhere, Category = "Radar")
	float FillAlpha = 0.20f;

	// 스트로크 = 등급색 × 이 값 (linear — 레전드리 골드가 라이트 패널에 묻히지 않게 다크 파생)
	UPROPERTY(EditAnywhere, Category = "Radar")
	float StrokeDarken = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Radar")
	float DotRadius = 5.f;

	// 값 0 도 실루엣이 보이게 하는 최소 반경 비율
	UPROPERTY(EditAnywhere, Category = "Radar")
	float MinValueFrac = 0.05f;

	// 미지정 시 RebuildWidget 에서 NEXON Bold 21 로 채움 (WBP 인스턴스에서 교체 가능)
	UPROPERTY(EditAnywhere, Category = "Radar")
	FSlateFontInfo LabelFont;

	// 값 변화 트윈 속도 (지수 접근) — 첫 오픈 = 중심에서 드로인, 투자 시 = 이전값→새값. 0 이하 = 즉시 스냅
	UPROPERTY(EditAnywhere, Category = "Radar")
	float ValueTweenSpeed = 9.f;

private:
	// 정규화된 값 0~1 (6개 고정) — 목표값. 표시는 ShownValues 가 트윈으로 따라감
	TArray<float> NormValues;
	TArray<float> ShownValues;
	TArray<FText> AxisLabels;
	FLinearColor GradeColor = FLinearColor(0.239f, 0.608f, 0.878f, 1.f);
};
