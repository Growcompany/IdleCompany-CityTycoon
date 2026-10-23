// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"
#include "Data/WorldMapTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/CountryType.h"
#include "TradeRouteWidget.generated.h"

class FSlateWindowElementList;

/** 한 산업에 대한 국가 하나의 생산력. 값으로 받는 이유 = 매니저 없이 ResolveRoute 를 검증하기 위해서다. */
USTRUCT()
struct FCountryProduction
{
	GENERATED_BODY()

	UPROPERTY()
	ECountryType Country = ECountryType::None;

	UPROPERTY()
	int32 ActiveLines = 0;
};

/** 한 산업에 대한 국가 하나의 수요 비율 (0.0~1.0). */
USTRUCT()
struct FCountryDemand
{
	GENERATED_BODY()

	UPROPERTY()
	ECountryType Country = ECountryType::None;

	UPROPERTY()
	float Ratio = 0.0f;

	/**
	 * `DT_CountryDemand.PriceMul` ― 같은 수요 비율에서 실제로 더 비싸게 팔리는 쪽을 가리는 축.
	 * 게임 초반에는 모든 셀이 만수요(1.0)라 Ratio 만으로는 전부 동률이고, 그때 enum 순서로 고르면
	 * 화면이 "여기가 수요 1등" 이라고 거짓말한다(수익은 똑같다). 그 자리를 이 값이 메운다.
	 */
	UPROPERTY()
	float PriceMul = 1.0f;
};

/** 판 위에 그릴 선 하나. bValid 가 false 면 끝점을 못 골랐다는 뜻이라 그리지 않는다. */
USTRUCT()
struct FTradeRoute
{
	GENERATED_BODY()

	UPROPERTY()
	ECountryType From = ECountryType::None;

	UPROPERTY()
	ECountryType To = ECountryType::None;

	UPROPERTY()
	ECompanyType Industry = ECompanyType::None;

	UPROPERTY()
	bool bValid = false;
};

/** 생산 항로 입력 한 나라. 패널이 DT·잠금 판정으로 채운다. Anchor = 판 정규화 좌표(0~1). */
USTRUCT()
struct FProductionRouteInput
{
	GENERATED_BODY()

	UPROPERTY()
	ECountryType Country = ECountryType::None;

	UPROPERTY()
	FVector2D Anchor = FVector2D::ZeroVector;

	UPROPERTY()
	bool bFactory = false;

	UPROPERTY()
	bool bHub = false;

	UPROPERTY()
	bool bUnlocked = false;
};

/** 공장 나라 → 가장 가까운 무역항. 앵커를 같이 실어 국가 앵커 맵 없이 그린다. */
USTRUCT()
struct FProductionRoute
{
	GENERATED_BODY()

	UPROPERTY()
	ECountryType From = ECountryType::None;

	UPROPERTY()
	ECountryType Hub = ECountryType::None;

	UPROPERTY()
	FVector2D FromAnchor = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D HubAnchor = FVector2D::ZeroVector;
};

/**
 * 무역 항로 레이어 ― 판 위에 국가 쌍을 잇는 이차 베지어를 직접 그린다 (텍스처·자식 위젯 0).
 *
 * ⚠ 이 위젯의 로컬 사각형이 곧 앵커 공간이다. 판(MapBaseImage)·메달리온 캔버스와 **정확히 같은 사각형**에
 *    풀스트레치로 놓여야 선이 메달리온에 꽂힌다. 크기가 다르면 비례해서 조용히 어긋난다.
 *
 * 데이터 출처(Ruling F, 2026-09-24): FTradeOrder 에는 국가 필드가 없다. 그래서 주문에서 경로를 읽지 않고
 * (그 산업을 지원하는 국가 중 **가동 라인이 가장 많은** 국가) → (그 산업 수요가 가장 높은 국가) 로 파생한다.
 *
 * ⚠ 출발지 기준은 "그 산업의 생산량" 이 아니다. `UWorldFactoryManager::GetActiveLineCount(Country)` 에는
 *    산업 인자가 없고 `FWorldFactoryLineState` 에도 산업 필드가 없어서, 실제로 비교되는 값은
 *    **그 나라의 전체 가동 라인 수**다. 자동차 5라인 국가가 반도체 1라인 국가를 반도체 주문에서도 이긴다.
 *    산업별로 가르려면 매니저에 산업축이 먼저 생겨야 한다 ― 여기서 고칠 수 있는 문제가 아니다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTradeRouteWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 산업 하나의 항로를 정한다.
	 * 매니저를 직접 부르지 않고 후보 배열을 받는 이유 = 월드·서브시스템 없는 자동화 테스트에서 검증하기 위해서다.
	 *
	 * 도착 후보에서 **출발지를 먼저 뺀 뒤** 1등을 고른다. 빼지 않으면 게임 초반이 통째로 막힌다 ―
	 * 수요는 만수요(1.0)에서 시작해 팔아야 내려가므로 전 국가가 동률이고, 그때 1등이 출발지 자신이 되어
	 * "출발=도착" 으로 주문이 버려진다. 한국만 해금된 구간에서는 그 결과가 **항로 0개**다.
	 *
	 * 순위: 수요 비율 → 같으면 `PriceMul`(실제로 더 비싸게 팔리는 쪽) → 그래도 같으면 enum 값.
	 * 마지막 enum 비교는 의미 축이 아니라 **프레임마다 선이 바뀌지 않게 하는 최후의 결정론 장치**다.
	 */
	static FTradeRoute ResolveRoute(ECompanyType Industry, const TArray<FCountryProduction>& Producers,
		const TArray<FCountryDemand>& Demands);

	/** 수락한 주문(bAccepted)만 항로가 된다 = 화면에서 파란 것이 곧 지금 할 일이다. */
	static TArray<FTradeRoute> BuildRoutes(const TArray<FTradeOrder>& Orders,
		TFunctionRef<TArray<FCountryProduction>(ECompanyType)> GetProducers,
		TFunctionRef<TArray<FCountryDemand>(ECompanyType)> GetDemands);

	/** 아직 수락하지 않은 주문의 경로 = "수락하면 열리는 길". 옅은 점선으로만 그린다. */
	static TArray<FTradeRoute> BuildIdleRoutes(const TArray<FTradeOrder>& Orders,
		TFunctionRef<TArray<FCountryProduction>(ECompanyType)> GetProducers,
		TFunctionRef<TArray<FCountryDemand>(ECompanyType)> GetDemands);

	/** 같은 두 국가를 잇는 선이 이미 있는지 (방향 무관). 파란 선 위에 점선을 겹쳐 그리지 않으려고 쓴다. */
	static bool ContainsPair(const TArray<FTradeRoute>& Routes, ECountryType A, ECountryType B);

	/**
	 * 곡선 분할 수 = `clamp(길이 / 24px, 2, 32)`.
	 *
	 * 24px 는 **목표 간격이지 보장이 아니다** ― 길이가 48px 미만이면 하한 2 가 이겨서 간격이 24 아래로 내려간다.
	 * 실제로 지키는 것은 두 끝값뿐이다: 하한 2 는 `BuildCurvePoints` 의 0 나눗셈과 직선화를 막고,
	 * 상한 32 는 한 항로의 드로우 예산을 묶는다.
	 * (미터 한계는 여기서 지켜 주는 것이 아니다 ― 이차 베지어의 최악 꺾임각이 18.2° 로 76.5° 한계에서 멀고,
	 *  굵기 13px 에 필요한 최소 세그먼트가 2.4px 라 본선 길이 구간에서는 애초에 안전하다.)
	 */
	static int32 ResolveSegmentCount(double LengthPx);

	/** 이차 베지어 폴리라인. 휘는 방향이 진행 방향에 묶여 있어 A→B 와 B→A 가 한 줄로 겹치지 않는다. */
	static TArray<FVector2D> BuildCurvePoints(const FVector2D& Start, const FVector2D& End, double BowRatio);

	/** 국가 → 정규화 앵커(0~1). 판 크기는 이 위젯이 자기 지오메트리에서 실측한다. */
	void SetCountryAnchors(const TMap<ECountryType, FVector2D>& InAnchors);

	void SetRoutes(const TArray<FTradeRoute>& InRoutes);
	void SetIdleRoutes(const TArray<FTradeRoute>& InRoutes);

	/** 공장 지원 + 잠금 해제 + 무역항 아님. 무역항을 겸하는 나라(미국)는 제자리에서 팔아 항로가 없다. */
	static bool IsProductionSource(const FProductionRouteInput& Entry);

	static bool IsProductionHub(const FProductionRouteInput& Entry);

	/**
	 * 출발지마다 가장 가까운 잠금 해제 무역항 하나(동률이면 enum 이 작은 쪽). 거리는 판 픽셀(앵커 × BoardSize)로 잰다 ―
	 * 판이 가로로 약 2.46배 길어서 정규화 좌표 거리로 재면 화면에서 더 먼 항구가 뽑히는 경우가 있다.
	 */
	static TArray<FProductionRoute> ResolveProductionRoutes(const TArray<FProductionRouteInput>& Countries,
		const FVector2D& BoardSize);

	/** 이차 베지어, 제어점 = 중점에서 화면 위로 (현 길이 × LiftRatio). 방향과 무관하게 늘 위로 솟는다. */
	static TArray<FVector2D> BuildLiftedCurvePoints(const FVector2D& Start, const FVector2D& End, double LiftRatio);

	/** 생산 항로 모드. 주문 항로(SetRoutes/SetIdleRoutes)와 독립이고, 그 아래 층에 그린다. */
	void SetProductionRoutes(const TArray<FProductionRouteInput>& Countries);

	/**
	 * "주문은 있는데 선이 0개" 를 1회만 알린다. 선이 0개면 앵커 검증도 그릴 것이 없어 침묵하므로,
	 * 가장 헷갈리는 상태가 로그에 아무 흔적도 안 남는다. 원인을 셋 중에서 고를 수 있게
	 * 호출부가 후보 수(배치국·생산·도착)를 문자열로 실어 보낸다.
	 */
	void ReportNoRoutes(int32 OrderCount, const FString& CandidateDetail);

	/** 렌더 검증(Task 9)이 그려질 선을 읽는 입구. */
	const TArray<FTradeRoute>& GetRoutes() const { return Routes; }
	const TArray<FTradeRoute>& GetIdleRoutes() const { return IdleRoutes; }
	const TArray<FProductionRoute>& GetProductionRoutes() const { return ProductionRoutes; }
	const TArray<FVector2D>& GetProductionHubAnchors() const { return ProductionHubAnchors; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	// 블루는 카탈로그가 기능색으로 묶어 둔 색이다 ― 활성 항로 말고 다른 데 쓰면 "파란 것 = 할 일" 신호가 희석된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Style")
	FLinearColor ActiveRouteColor = FLinearColor::FromSRGBColor(FColor(0x3D, 0x9B, 0xE0));

	// 비활성은 정보가 아니라 "체계가 있다"는 신호라 눈에 띄면 안 된다 (크롬 잉크 #E6E9F0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Style")
	FLinearColor IdleRouteColor = FLinearColor::FromSRGBColor(FColor(0xE6, 0xE9, 0xF0));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleRouteAlpha = 0.24f;

	// 대시 아래에 깔리는 연속 스트로크. 대시만 있으면 경로가 끊겨 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActiveGlowAlpha = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Style", meta = (ClampMin = "0.5"))
	float ActiveThickness = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Style", meta = (ClampMin = "1.0"))
	float GlowThicknessScale = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Style", meta = (ClampMin = "0.5"))
	float IdleThickness = 2.5f;

	// 현 길이 대비 휨. 0 이면 직선이라 여러 항로가 한 국가에서 겹칠 때 서로를 덮는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Shape", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RouteBowRatio = 0.16f;

	// 메달리온 반경만큼 양 끝을 비워 선이 원 밑에서 튀어나오지 않게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Shape", meta = (ClampMin = "0.0"))
	float EndpointInsetPx = 46.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Dash", meta = (ClampMin = "1.0"))
	float ActiveDashLength = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Dash", meta = (ClampMin = "1.0"))
	float ActiveDashGap = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Dash", meta = (ClampMin = "1.0"))
	float IdleDashLength = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Dash", meta = (ClampMin = "1.0"))
	float IdleDashGap = 14.0f;

	// 대시가 한 주기 흐르는 시간. 이동 방향이 출발→도착이라 선 자체가 방향을 말한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Dash", meta = (ClampMin = "0.1"))
	float DashLoopSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production")
	FLinearColor ProductionRouteColor = FLinearColor::FromSRGBColor(FColor(0xDC, 0xE6, 0xF2));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProductionRouteAlpha = 0.34f;

	// 점 지름. 굵은 폴리라인 1px 조각은 진행 방향에 수직인 막대로 보여 둥근 원판으로 찍는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.5"))
	float ProductionDotThickness = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.0"))
	float ProductionDotLength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "1.0"))
	float ProductionDotGap = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProductionRouteLift = 0.22f;

	// 시안은 한 주기에 대시 오프셋 −32 = 점 간격(1+15)의 2배라 이음매 없이 돈다. 간격을 바꾸면 그 배수로 맞출 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.0"))
	float ProductionFlowPerLoopPx = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.1"))
	float ProductionLoopSeconds = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production")
	FLinearColor ProductionHubColor = FLinearColor::FromSRGBColor(FColor(0x73, 0xE6, 0xD6));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProductionHubAlpha = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TradeRoute|Production", meta = (ClampMin = "0.0"))
	float ProductionHubRadius = 10.0f;

private:
	/** 앵커가 없는 국가를 걸러 내고 경고를 남긴다 ― 매 프레임 그리기 안에서는 짖을 수 없다. */
	void ValidateAnchors(const TArray<FTradeRoute>& InRoutes, const TCHAR* Label) const;

	bool ResolveEndpoints(const FTradeRoute& Route, const FVector2D& BoardSize, FVector2D& OutStart,
		FVector2D& OutEnd) const;

	/** 생산 항로 점·무역항 점을 BaseLayer 부터 그리고 다음 빈 층을 돌려준다(그릴 것이 없으면 BaseLayer). */
	int32 PaintProductionRoutes(FSlateWindowElementList& OutDrawElements, int32 BaseLayer,
		const FGeometry& AllottedGeometry, const FVector2D& BoardSize) const;

	TArray<FTradeRoute> Routes;
	TArray<FTradeRoute> IdleRoutes;
	TMap<ECountryType, FVector2D> CountryAnchors;

	TArray<FProductionRoute> ProductionRoutes;
	TArray<FVector2D> ProductionHubAnchors;

	// 대시 위상 누산기 (NativeTick 갱신, NativePaint 는 읽기만).
	float DashElapsed = 0.0f;
	float ProductionFlowElapsed = 0.0f;

	// 판 사각형을 못 받은 채로 조용히 안 보이는 것을 막는다. 한 번만 짖는다.
	bool bWarnedDegenerateSize = false;

	// "주문은 있는데 선이 0개" 경고도 한 번만. 주문·생산 이벤트마다 다시 짖으면 로그가 못 쓰게 된다.
	bool bWarnedNoRoutes = false;
};
