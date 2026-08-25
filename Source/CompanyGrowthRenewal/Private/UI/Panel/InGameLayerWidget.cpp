// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/InGameLayerWidget.h"

#include "Core/CGGameInstance.h"

// 해상도 및 회전 감지
#include "UnrealClient.h"         
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Misc/CoreDelegates.h"

// UI 관련
#include "CommonTextBlock.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Enum/ResourceType.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonButtonBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/UserWidget.h"

// Subsystem 헤더
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "Table/ProfileImageData.h"
#include <UI/Element/Common/BrickCollectWidget.h>
#include "UI/Element/Effects/FloatingNumberWidget.h"
#include "UI/Element/Effects/SelectionChevronWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include <Kismet/GameplayStatics.h>
#include <Blueprint/WidgetLayoutLibrary.h>
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"

#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/EntityManager.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "UI/Element/Chat/BubbleContainerWidget.h"
#include "UI/Element/Common/CoinFlyoutContainerWidget.h"
#include "Table/ResourceInfo.h"
#include "Player/MainMapPlayerController.h"
#include "Manager/SpawnManager.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Table/CityPlotData.h"
#include "UI/Element/Plot/PlotPriceBadgeWidget.h"
#include "UI/Element/Building/VaultGaugeWidget.h"
#include "UI/Element/Common/RevenueRateChipWidget.h"
#include "Enum/GaugeHealth.h"
#include "Enum/CompanyType.h"
// 줌 접근 — BubbleContainerWidget.cpp 와 동일한 쌍 (PlayerCamera.h 는 Private/Player/ 지만 같은 모듈)
#include "Player/PlayerCamera.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/Components/PlacementHandler.h"


void UInGameLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateScreenRings(InDeltaTime);
	UpdateSelectionMarker(InDeltaTime);

	// 가격 배지 위치는 매 틱 재투영(카메라 추적) — 부지는 고정이지만 화면 위 위치는 카메라 이동마다 바뀌므로 필수.
	// 멤버십/가격/자금색은 아래 주기 타이머 + 소유 변경 이벤트에서만 갱신(매 틱 DT 조회/문자열 생성 회피).
	UpdatePlotBadgePositions();

	// 금고 게이지도 같은 이유로 위치/폭만 매 틱 (값·색은 아래 주기 블록)
	UpdateVaultGaugeLOD();
	UpdateVaultGaugePositions();
	RefreshVaultGaugeValues();

	// 콜드 진입 초기 갱신 — 캔버스가 실제 크기를 가진 첫 틱에 1회. 소비 조건이 "틱 몇 번째" 가 아니라
	// "지오메트리가 쓸 만한가" 인 이유: 페인트 완료 시점은 프레임 수로 보장되지 않고, 실패하면 후보가
	// 조용히 0이 되어 증상이 F3 와 똑같아진다. 크기가 계속 0이면 플래그를 든 채 다음 틱에 재시도한다
	// (게이지 없음으로만 degrade — 스핀도 로그도 없다).
	if (bVaultGaugeInitialRefreshPending && InGameCanvas)
	{
		const FVector2D CanvasSize = InGameCanvas->GetCachedGeometry().GetLocalSize();
		if (CanvasSize.X > 0.0f && CanvasSize.Y > 0.0f)
		{
			bVaultGaugeInitialRefreshPending = false;
			RefreshVaultGauges();
		}
	}

	// 예약 대상이 카메라 이동으로 화면에 들어오는 첫 프레임을 놓치지 않도록 준비될 때까지만 재평가한다.
	if (!bVaultGaugeInitialRefreshPending
		&& TutorialReservedGaugeBuildingIndex != INDEX_NONE
		&& GetVaultGaugeWidgetForBuilding(TutorialReservedGaugeBuildingIndex) == nullptr)
	{
		bVaultGaugeReevalPending = true;
	}

	// 주기적 버블 전체 재평가 (CompanyType/EmployeeCount 전용 델리게이트 미구현 대응)
	BubbleReevalTimer += InDeltaTime;
	if (BubbleReevalTimer >= BubbleReevalInterval)
	{
		BubbleReevalTimer = 0.0f;
		bBubbleReevalPending = true;
		bVaultGaugeReevalPending = true;

		// 인접-미소유 배지 상시 갱신(소유 변경 반영 + 자금 변동으로 빨강 토글). 토글 게이트 없음.
		RefreshPlotPriceBadges();
	}

	// HUD 유량 갱신 — 1초. ⚠ 위 5초 블록에 얹지 말 것: 직전 표본 대비 5% 변화 펄스의 판정 해상도가
	// 5초로 낮아지면 실제 수익 변화를 한참 뒤에 알리게 된다.
	// 매니저 FTSTicker 가 1.0초라 1초가 유효 해상도의 상한이고, 5초 리듬은 버블·부지배지와 공유하므로 못 줄인다.
	RevenueChipTimer += InDeltaTime;
	if (RevenueChipTimer >= RevenueChipInterval)
	{
		RevenueChipTimer = 0.0f;
		RefreshRevenueRateChip();
	}

	// 단일 건물 이벤트/주기 폴링을 프레임당 1회 전역 재평가로 합류 — 동시 표시 상한(3개) 정합 유지
	if (bBubbleReevalPending)
	{
		bBubbleReevalPending = false;
		EvaluateAllBuildingBubbles();
	}

	// 버블 후보를 먼저 실제 ActiveBubbles 멤버십으로 확정한 뒤 게이지 후보를 조정한다.
	if (bVaultGaugeReevalPending)
	{
		RefreshVaultGauges();
	}

	FlushPendingAmountPopups(InDeltaTime);
}

// ========== 스크린 링 FX ==========

namespace
{
	// 튜닝 상수 — 모바일: 손가락이 터치점을 가리므로 손가락 폭(~60px)보다 크게
	constexpr float TapRingDuration = 0.5f;
	constexpr float TapRingStartRadius = 18.f;
	constexpr float TapRingEndRadius = 78.f;
	constexpr float GaugeRadius = 220.f;
	constexpr float GaugePopDuration = 0.25f;

	// 글로우 텍스처의 링이 라인 반경과 겹치도록 박스 확대 배율 (ring_a 기준, 시각루프로 튜닝)
	constexpr float RingGlowScale = 1.35f;

	// 선택/배치 셰브론 (지붕 중심 위 이중 V 위젯 — 슬롯 배치)
	constexpr float MarkerLiftY = 30.f;        // 지붕 앵커에서 위로 띄우는 양(px)
	constexpr float MarkerBobAmp = 10.f;       // 바운스 진폭(px)
	constexpr float MarkerBobHz = 1.1f;
	constexpr float InvalidMarkerBobAmp = 2.f; // 색 외에도 정지에 가까운 움직임으로 배치 불가를 구분
	constexpr float InvalidMarkerPulseHz = 0.85f;
	constexpr float MarkerPopDuration = 0.25f; // 등장 팝

	const FLinearColor SelectionMainColor(1.f, 0.92f, 0.6f, 1.f);
	const FLinearColor SelectionEchoColor(1.f, 0.92f, 0.6f, 0.45f);
	const FLinearColor PlacementValidMainColor(0.08f, 0.78f, 0.18f, 1.f);
	const FLinearColor PlacementValidEchoColor(0.08f, 0.78f, 0.18f, 0.45f);
	const FLinearColor PlacementInvalidMainColor(0.95f, 0.16f, 0.10f, 1.f);
	const FLinearColor PlacementInvalidEchoColor(0.95f, 0.16f, 0.10f, 0.45f);
}

void UInGameLayerWidget::SpawnTapRing()
{
	FScreenRingFX Ring;
	Ring.AbsPos = UGlobalUtilFunctions::GetPointerAbsolutePosition();
	ActiveRings.Add(Ring);
}

void UInGameLayerWidget::BeginHoldPulse(float CycleSeconds)
{
	bHoldPulseActive = true;
	HoldPulseElapsed = 0.f;
	HoldPulseCycle = FMath::Max(CycleSeconds, 0.05f);
	GaugePopElapsed = -1.f;
	HoldPulseAbsPos = UGlobalUtilFunctions::GetPointerAbsolutePosition();
}

void UInGameLayerWidget::EndHoldPulse()
{
	bHoldPulseActive = false;
}

void UInGameLayerWidget::SetSelectedBuildingMarker(ABuildingBaseActor* Building)
{
	const bool bChanged = MarkerBuilding.Get() != Building;
	MarkerBuilding = Building;
	if (bChanged)
	{
		bVaultGaugeReevalPending = true;
		if (!PlacementMarkerBuilding.IsValid() || !PlacementMarkerHandler.IsValid())
		{
			ActiveMarkerBuilding.Reset();
			MarkerVisualState = EMarkerVisualState::None;
			MarkerElapsed = 0.f;
		}
	}

	if (!Building && !PlacementMarkerBuilding.IsValid() && MarkerChevron)
	{
		MarkerChevron->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInGameLayerWidget::SetPlacementBuildingMarker(ABuildingBaseActor* Building, UPlacementHandler* PlacementHandler)
{
	const bool bEnablePlacementMarker = Building != nullptr && PlacementHandler != nullptr;
	ABuildingBaseActor* NewBuilding = bEnablePlacementMarker ? Building : nullptr;
	UPlacementHandler* NewHandler = bEnablePlacementMarker ? PlacementHandler : nullptr;
	const bool bChanged = PlacementMarkerBuilding.Get() != NewBuilding
		|| PlacementMarkerHandler.Get() != NewHandler;

	PlacementMarkerBuilding = NewBuilding;
	PlacementMarkerHandler = NewHandler;

	if (bChanged)
	{
		ActiveMarkerBuilding.Reset();
		MarkerVisualState = EMarkerVisualState::None;
		MarkerElapsed = 0.f;
	}

	if (!bEnablePlacementMarker && !MarkerBuilding.IsValid() && MarkerChevron)
	{
		MarkerChevron->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInGameLayerWidget::UpdateSelectionMarker(float DeltaTime)
{
	const bool bPlacementMarkerActive = PlacementMarkerBuilding.IsValid() && PlacementMarkerHandler.IsValid();
	ABuildingBaseActor* EffectiveBuilding = bPlacementMarkerActive
		? PlacementMarkerBuilding.Get()
		: MarkerBuilding.Get();
	const EMarkerVisualState DesiredVisualState = !EffectiveBuilding
		? EMarkerVisualState::None
		: bPlacementMarkerActive
			? (PlacementMarkerHandler->CanDropHere()
				? EMarkerVisualState::PlacementValid
				: EMarkerVisualState::PlacementInvalid)
			: EMarkerVisualState::Selected;

	if (!EffectiveBuilding)
	{
		ActiveMarkerBuilding.Reset();
		MarkerVisualState = EMarkerVisualState::None;
		if (MarkerChevron)
		{
			MarkerChevron->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// 최초 1회 생성 — 자가 페인트 셰브론 위젯 (슬롯 하단중앙 정렬 = V 꼭짓점이 앵커)
	if (!MarkerChevron && InGameCanvas)
	{
		MarkerChevron = CreateWidget<USelectionChevronWidget>(this, USelectionChevronWidget::StaticClass());
		MarkerSlot = InGameCanvas->AddChildToCanvas(MarkerChevron);
		MarkerChevron->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (MarkerSlot)
		{
			MarkerSlot->SetAutoSize(true);
			MarkerSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			MarkerSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		}
	}
	if (!MarkerChevron || !MarkerSlot)
	{
		return;
	}

	if (ActiveMarkerBuilding.Get() != EffectiveBuilding || MarkerVisualState != DesiredVisualState)
	{
		ActiveMarkerBuilding = EffectiveBuilding;
		MarkerVisualState = DesiredVisualState;
		MarkerElapsed = 0.f;

		switch (MarkerVisualState)
		{
		case EMarkerVisualState::PlacementValid:
			MarkerChevron->SetChevronColors(PlacementValidMainColor, PlacementValidEchoColor);
			break;
		case EMarkerVisualState::PlacementInvalid:
			MarkerChevron->SetChevronColors(PlacementInvalidMainColor, PlacementInvalidEchoColor);
			break;
		case EMarkerVisualState::Selected:
		default:
			MarkerChevron->SetChevronColors(SelectionMainColor, SelectionEchoColor);
			break;
		}
	}

	// 지붕 중심 투영 → 캔버스 로컬 (SpawnBrickCollectAt 과 동일한 검증된 변환)
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	FVector2D ViewportPos;
	if (!PC || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC, EffectiveBuilding->GetRoofAnchorPosition(), ViewportPos, true))
	{
		MarkerChevron->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FGeometry CanvasGeo = InGameCanvas->GetCachedGeometry();
	const FVector2D CanvasLocal = CanvasGeo.AbsoluteToLocal(
		UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(ViewportPos));

	MarkerSlot->SetPosition(CanvasLocal - FVector2D(0.f, MarkerLiftY));

	// 등장 팝 + 바운스 (위젯 자체 렌더트랜스폼 — 슬롯 위치와 독립)
	MarkerElapsed += DeltaTime;
	const float PopT = FMath::Clamp(MarkerElapsed / MarkerPopDuration, 0.f, 1.f);
	const float PopEase = 1.f - (1.f - PopT) * (1.f - PopT) * (1.f - PopT);
	const float Scale = FMath::Lerp(1.6f, 1.f, PopEase);
	const bool bInvalidPlacement = MarkerVisualState == EMarkerVisualState::PlacementInvalid;
	const float BobAmplitude = bInvalidPlacement ? InvalidMarkerBobAmp : MarkerBobAmp;
	const float Bob = FMath::Sin(MarkerElapsed * 2.f * PI * MarkerBobHz) * BobAmplitude;
	const float InvalidPulse = 0.72f + 0.18f
		* (0.5f + 0.5f * FMath::Sin(MarkerElapsed * 2.f * PI * InvalidMarkerPulseHz));

	FWidgetTransform Xform;
	Xform.Translation = FVector2D(0.f, Bob);
	Xform.Scale = FVector2D(Scale, Scale);
	MarkerChevron->SetRenderTransform(Xform);
	MarkerChevron->SetRenderOpacity(PopT * (bInvalidPlacement ? InvalidPulse : 1.f));
	MarkerChevron->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UInGameLayerWidget::UpdateScreenRings(float DeltaTime)
{
	for (int32 i = ActiveRings.Num() - 1; i >= 0; --i)
	{
		ActiveRings[i].Elapsed += DeltaTime;
		if (ActiveRings[i].Elapsed >= TapRingDuration)
		{
			ActiveRings.RemoveAtSwap(i);
		}
	}

	if (bHoldPulseActive)
	{
		// 진행도 wrap = 한 바퀴 완료 = 생산 틱 (타이머와 같은 시점에 시작한 선형 진행이라 동기)
		const float PrevProgress = FMath::Fmod(HoldPulseElapsed, HoldPulseCycle) / HoldPulseCycle;
		HoldPulseElapsed += DeltaTime;
		const float NewProgress = FMath::Fmod(HoldPulseElapsed, HoldPulseCycle) / HoldPulseCycle;
		if (NewProgress < PrevProgress)
		{
			GaugePopElapsed = 0.f;
		}

		// 홀드 중 손가락/커서 위치 추적
		HoldPulseAbsPos = UGlobalUtilFunctions::GetPointerAbsolutePosition();
	}

	if (GaugePopElapsed >= 0.f)
	{
		GaugePopElapsed += DeltaTime;
		if (GaugePopElapsed >= GaugePopDuration)
		{
			GaugePopElapsed = -1.f;
		}
	}

}

namespace
{
	// 호(arc)를 라인 세그먼트로 드로잉 (텍스처 불필요). 스크린 공간은 Y-down 이라 각도 증가 = 시계방향
	void DrawScreenArc(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& LocalCenter, float Radius, float StartAngleRad, float SweepRad,
		const FLinearColor& Color, float Thickness)
	{
		const int32 NumSegments = FMath::Max(2, FMath::CeilToInt(32.f * SweepRad / (2.f * PI)));
		TArray<FVector2D> Points;
		Points.Reserve(NumSegments + 1);
		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float Angle = StartAngleRad + (SweepRad * i) / NumSegments;
			Points.Add(LocalCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color, true, Thickness);
	}

	void DrawScreenRing(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& LocalCenter, float Radius, const FLinearColor& Color, float Thickness)
	{
		DrawScreenArc(OutDrawElements, Layer, Geo, LocalCenter, Radius, 0.f, 2.f * PI, Color, Thickness);
	}

	float RingEaseOutCubic(float A)
	{
		const float T = 1.f - A;
		return 1.f - T * T * T;
	}

	// 소프트 글로우 텍스처를 링 중심 박스로 드로잉 (라인 링 아래 레이어 — 브러시 미지정 시 no-op)
	void DrawScreenGlow(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& LocalCenter, float Radius, const FSlateBrush* Brush, const FLinearColor& Color)
	{
		if (!Brush || !Brush->GetResourceObject())
		{
			return;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			Geo.ToPaintGeometry(FVector2f(Radius * 2.f, Radius * 2.f),
				FSlateLayoutTransform(FVector2f(LocalCenter - FVector2D(Radius, Radius)))),
			Brush, ESlateDrawEffect::None, Color);
	}
}

int32 UInGameLayerWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (ActiveRings.Num() == 0 && !bHoldPulseActive && GaugePopElapsed < 0.f)
	{
		return MaxLayer;
	}
	const int32 GlowLayer = MaxLayer + 1;   // 텍스처 글로우 (라인 아래)
	const int32 RingLayer = MaxLayer + 2;   // 라인 링

	// AbsPos(커서) 는 데스크톱 절대공간인데 OnPaint 의 AllottedGeometry 는 윈도우 공간
	// (SWidget::Paint 가 WindowToDesktopTransform 을 더해 DesktopGeometry 를 따로 만듦) —
	// 변환은 데스크톱 공간 지오메트리로, 드로잉은 페인트 지오메트리로 (로컬 공간은 둘이 동일)
	const FGeometry& DesktopGeo = GetTickSpaceGeometry();

	// 탭: 크림톤 리플
	for (const FScreenRingFX& Ring : ActiveRings)
	{
		const FVector2D LocalCenter = DesktopGeo.AbsoluteToLocal(Ring.AbsPos);
		const float T = FMath::Clamp(Ring.Elapsed / TapRingDuration, 0.f, 1.f);
		const float E = RingEaseOutCubic(T);
		DrawScreenGlow(OutDrawElements, GlowLayer, AllottedGeometry, LocalCenter,
			FMath::Lerp(TapRingStartRadius, TapRingEndRadius, E) * RingGlowScale,
			&RingGlowBrush, FLinearColor(1.f, 0.95f, 0.8f, (1.f - T) * 0.5f));
		DrawScreenRing(OutDrawElements, RingLayer, AllottedGeometry, LocalCenter,
			FMath::Lerp(TapRingStartRadius, TapRingEndRadius, E),
			FLinearColor(1.f, 0.95f, 0.8f, (1.f - T) * 0.9f), FMath::Lerp(5.f, 2.f, T));
	}

	if (bHoldPulseActive)
	{
		// 홀드: 생산 주기 라디얼 게이지 — 옅은 바탕 링 + 12시부터 시계방향으로 차오르는 호
		const FVector2D LocalCenter = DesktopGeo.AbsoluteToLocal(HoldPulseAbsPos);
		DrawScreenGlow(OutDrawElements, GlowLayer, AllottedGeometry, LocalCenter,
			GaugeRadius * RingGlowScale, &RingGlowBrush, FLinearColor(1.f, 0.9f, 0.7f, 0.12f));
		DrawScreenRing(OutDrawElements, RingLayer, AllottedGeometry, LocalCenter,
			GaugeRadius, FLinearColor(1.f, 0.9f, 0.7f, 0.22f), 5.f);

		const float Progress = FMath::Fmod(HoldPulseElapsed, HoldPulseCycle) / HoldPulseCycle;
		if (Progress > KINDA_SMALL_NUMBER)
		{
			DrawScreenArc(OutDrawElements, RingLayer, AllottedGeometry, LocalCenter,
				GaugeRadius, -PI / 2.f, Progress * 2.f * PI, FLinearColor(1.f, 0.9f, 0.7f, 0.95f), 10.f);
		}
	}

	if (GaugePopElapsed >= 0.f)
	{
		// 한 바퀴 완료(생산 틱) 팝 — 게이지에서 밝게 퍼지는 링
		const FVector2D LocalCenter = DesktopGeo.AbsoluteToLocal(HoldPulseAbsPos);
		const float T = FMath::Clamp(GaugePopElapsed / GaugePopDuration, 0.f, 1.f);
		DrawScreenGlow(OutDrawElements, GlowLayer, AllottedGeometry, LocalCenter,
			FMath::Lerp(GaugeRadius, GaugeRadius + 110.f, RingEaseOutCubic(T)) * RingGlowScale,
			&RingGlowBrush, FLinearColor(1.f, 0.97f, 0.85f, (1.f - T) * 0.5f));
		DrawScreenRing(OutDrawElements, RingLayer, AllottedGeometry, LocalCenter,
			FMath::Lerp(GaugeRadius, GaugeRadius + 110.f, RingEaseOutCubic(T)),
			FLinearColor(1.f, 0.97f, 0.85f, (1.f - T) * 0.9f), 7.f);
	}

	return RingLayer;
}


void UInGameLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGameInstance* GI = GetWorld()->GetGameInstance();
	TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();

	// 링 글로우 텍스처 주입 — DT_UIVFXTexture 단일 진실 (행 없으면 빈 브러시 = 라인 전용)
	if (TableMgr)
	{
		FUIVFXTexRow VFXRow;
		if (TableMgr->GetUIVFXTexRow(TEXT("RingGlow"), VFXRow) && !VFXRow.Texture.IsNull())
		{
			if (UTexture2D* GlowTex = VFXRow.Texture.LoadSynchronous())
			{
				RingGlowBrush.SetResourceObject(GlowTex);
				RingGlowBrush.ImageSize = VFXRow.ImageSize;
			}
		}
	}

	// BindWidget된 리소스 위젯들을 TMap에 등록
	auto RegisterResource = [&](UResourceWidget* Widget, EResourceType Type)
	{
		if (!Widget) return;
		Widget->bShouldCalculateIconPos = true;
		// WBP CDO 도 True 지만 "HUD 칩 = 롤업 대상" 의도를 코드에 명시 (동작 변화 없음)
		Widget->bLiveCounterRollup = true;
		ResourceWidgets.Add(Type, Widget);
		if (RMgr)
		{
			Widget->SetValue(RMgr->GetResourceAmount(Type));
		}
	};

	RegisterResource(UIE_Resource_Money, EResourceType::Money);
	RegisterResource(UIE_Resource_Brick, EResourceType::Brick);
	RegisterResource(UIE_Resource_Diamond, EResourceType::Diamond);

	// 건물 카운터 위젯은 X/Y 분수 표시 - 일반 리소스 갱신 경로와 분리
	if (UIE_Resource_Building)
	{
		UIE_Resource_Building->bShouldCalculateIconPos = true;
		ResourceWidgets.Add(EResourceType::Building, UIE_Resource_Building);
	}

	// EntityManager의 건물 수 변경 구독 (AddBuilding/RemoveBuilding에서 Broadcast)
	if (UWorld* World = GetWorld())
	{
		if (UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>())
		{
			BuildingCountChangedHandle = EntityMgr->OnBuildingCountChanged.AddUObject(
				this, &UInGameLayerWidget::RefreshBuildingCount);
		}
	}

	// 초기 카운트 표시 + 모든 빌딩의 버블 재평가 델리게이트 wire-up (HQ 레벨/건물 배열 준비 후 1회)
	RefreshBuildingCount();
	RewireBuildingBubbleSubscriptions();

	// UIManagerSubsystem을 통한 리소스 변경 구독
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UIResourceChangedHandle = UIMgr->OnUIResourceChanged.AddUObject(
			this, &UInGameLayerWidget::HandleResourceChanged);
	}

	// 화면 회전 감지 (모바일용)
	OrientationDelegateHandle =
		FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.AddLambda([this](auto)
			{
				UpdateIconScreenPos();
			});

	// 뷰포트 크기 변경 감지 (창 모드 ↔ 전체화면 전환, 창 리사이즈)
	ViewportDelegateHandle = FViewport::ViewportResizedEvent.AddLambda([this](FViewport*, uint32)
		{
			UpdateIconScreenPos();
		});

	// ========== 건물 버블 컨테이너 생성 ==========
	if (!BubbleContainer)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			BubbleContainer = CreateWidget<UBubbleContainerWidget>(PC);
			if (BubbleContainer && InGameCanvas)
			{
				UCanvasPanelSlot* BubbleSlot = InGameCanvas->AddChildToCanvas(BubbleContainer);
				if (BubbleSlot)
				{
					BubbleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
					BubbleSlot->SetOffsets(FMargin(0.0f));
					BubbleSlot->SetZOrder(-1);
				}
			}
		}
	}

	// 버블 클릭 액션 바인딩
	if (BubbleContainer)
	{
		BubbleContainer->OnBubbleAction.BindUObject(this, &UInGameLayerWidget::HandleBubbleAction);
		BubbleContainer->OnActiveBubbleMembershipChanged.AddUObject(
			this, &UInGameLayerWidget::HandleActiveBubbleMembershipChanged);
	}

	// 버블 델리게이트 바인딩
	if (auto* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
	{
		OpMgr->OnOperationCompleted.AddDynamic(this, &ThisClass::OnOperationCompletedForBubble);
		OpMgr->OnOperationStarted.AddDynamic(this, &ThisClass::OnOperationStartedForBubble);
		OpMgr->OnRevenueCollected.AddDynamic(this, &ThisClass::OnRevenueCollectedForBubble);

		// 창고 구독은 이 하나뿐 — 게이트 없는 형제를 같이 붙이면 게이트가 무의미해진다
		// (해제는 아래 NativeDestruct 블록과 1:1 쌍)
		OpMgr->OnWarehouseUpdated.AddDynamic(this, &ThisClass::HandleWarehouseUpdated);
	}

	// 초기 버블 상태 평가
	EvaluateAllBuildingBubbles();

	// 인접-미소유 부지 가격 배지 초기 평가 (시작 스폰/세이브 복원 직후 — 상시 표시).
	RefreshPlotPriceBadges();

	if (RevenueRateChip)
	{
		RevenueRateChip->SetRate(0.0);
		RefreshRevenueRateChip();
	}

	// 금고 게이지 초기 평가 예약 — BubbleReevalTimer 가 0 에서 시작하므로 이게 없으면 입장 후 첫 5초간 게이지가 없다.
	// ⚠ 여기서 직접 RefreshVaultGauges() 를 부르면 무효다: 아직 페인트 전이라 InGameCanvas 캐시 지오메트리가
	// 기본 크기고, 투영이 전부 실패해 후보 0으로 끝난다. 캔버스가 실제 크기를 가진 뒤 NativeTick 이 실행한다.
	bVaultGaugeInitialRefreshPending = true;

	// 프로필 닉네임 표시 (PlayFab 로그인 완료 시)
	if (PlayerNameText)
	{
		if (UPlayFabManagerSubsystem* PlayFabMgr = GI->GetSubsystem<UPlayFabManagerSubsystem>())
		{
			if (PlayFabMgr->IsLoggedIn())
			{
				const FString& Name = PlayFabMgr->GetUserInfo().DisplayName;
				PlayerNameText->SetText(FText::FromString(Name.IsEmpty() ? TEXT("Player") : Name));
			}
			else
			{
				// 로그인 완료 시 닉네임 갱신
				PlayFabMgr->OnLoginComplete.AddWeakLambda(this, [this, PlayFabMgr](bool bSuccess)
				{
					if (bSuccess && PlayerNameText)
					{
						const FString& Name = PlayFabMgr->GetUserInfo().DisplayName;
						PlayerNameText->SetText(FText::FromString(Name.IsEmpty() ? TEXT("Player") : Name));
					}
				});
			}
		}
	}

	// 본사 레벨 표시
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		if (PlayerLevelText)
		{
			PlayerLevelText->SetText(FText::FromString(
				FString::Printf(TEXT("%d"), SaveMgr->GetHQLevel())));
		}

		SaveMgr->OnHQLevelUp.AddDynamic(this, &UInGameLayerWidget::HandleHQLevelUp);
	}

	// 프로필 이미지 초기 로드 (SaveData → 텍스처)
	if (ProfileImage && TableMgr)
	{
		if (USaveLoadManager* SaveMgr2 = GI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr2->GetCurrentSaveData())
			{
				UpdateProfileImage(SaveData->GameData.ProfileImageID);
			}
		}
	}

	// 프로필 이미지 버튼
	if (ProfileImageBtn)
	{
		ProfileImageBtn->OnClicked.AddDynamic(this, &UInGameLayerWidget::OnProfileImageBtnClicked);
	}
}

void UInGameLayerWidget::NativeDestruct()
{
	// UIManagerSubsystem 구독 해제
	if (UUIManagerSubsystem* UIMgr = GetWorld()
		->GetGameInstance()
		->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->OnUIResourceChanged.Remove(UIResourceChangedHandle);
	}

	// EntityManager 건물 수 구독 해제 + 모든 빌딩의 버블 재평가 델리게이트 해제
	if (UWorld* World = GetWorld())
	{
		if (UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>())
		{
			if (BuildingCountChangedHandle.IsValid())
			{
				EntityMgr->OnBuildingCountChanged.Remove(BuildingCountChangedHandle);
			}

			for (ABuildingBaseActor* Building : EntityMgr->GetBuildings())
			{
				if (!Building) continue;
				Building->OnBubbleRefreshRequested.RemoveDynamic(this, &UInGameLayerWidget::HandleBuildingBubbleRefreshRequested);
			}
		}
	}
	BuildingCountChangedHandle.Reset();

	if (OrientationDelegateHandle.IsValid())
	{
		FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.Remove(
			OrientationDelegateHandle);
	}

	if (ViewportDelegateHandle.IsValid())
	{
		FViewport::ViewportResizedEvent.Remove(ViewportDelegateHandle);
	}

	// 버블 델리게이트 해제
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (auto* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->OnOperationCompleted.RemoveDynamic(this, &ThisClass::OnOperationCompletedForBubble);
			OpMgr->OnOperationStarted.RemoveDynamic(this, &ThisClass::OnOperationStartedForBubble);
			OpMgr->OnRevenueCollected.RemoveDynamic(this, &ThisClass::OnRevenueCollectedForBubble);

			// 위 NativeConstruct AddDynamic 과 1:1 쌍
			OpMgr->OnWarehouseUpdated.RemoveDynamic(this, &ThisClass::HandleWarehouseUpdated);
		}

		// 본사 레벨업 구독 해제
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->OnHQLevelUp.RemoveDynamic(this, &UInGameLayerWidget::HandleHQLevelUp);
		}
	}

	if (ProfileImageBtn)
	{
		ProfileImageBtn->OnClicked.RemoveDynamic(this, &UInGameLayerWidget::OnProfileImageBtnClicked);
	}

	// 가격 배지 풀 제거(전용 위젯 — 캔버스에서 떼고 풀 비움).
	for (UPlotPriceBadgeWidget* Badge : PlotPriceBadgePool)
	{
		if (Badge)
		{
			Badge->RemoveFromParent();
		}
	}
	PlotPriceBadgePool.Reset();

	// 버블 액션 해제 + 컨테이너 제거
	if (BubbleContainer)
	{
		BubbleContainer->OnBubbleAction.Unbind();
		BubbleContainer->OnActiveBubbleMembershipChanged.RemoveAll(this);
		BubbleContainer->RemoveFromParent();
		BubbleContainer = nullptr;
	}

	// 코인 플라이아웃 정리
	if (CurrentCoinFlyout)
	{
		CurrentCoinFlyout->OnCoinArrived.Unbind();
		CurrentCoinFlyout->OnAllCoinsComplete.Unbind();
		CurrentCoinFlyout->RemoveFromParent();
		CurrentCoinFlyout = nullptr;
	}

	// 타이머 정리 추가
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::NativeDestruct();
}

void UInGameLayerWidget::UpdateIconScreenPos()
{
	// Slate 레이아웃이 새 뷰포트 크기로 갱신될 때까지 지연 후 재계산
	// (즉시 ForceRecalculate하면 GetCachedGeometry()가 구형 값을 반환)
	UWorld* World = GetWorld();
	if (!World) return;

	World->GetTimerManager().SetTimer(
		IconRecalcTimerHandle,
		[this]()
		{
			for (auto& Pair : ResourceWidgets)
			{
				if (Pair.Value)
				{
					Pair.Value->ForceRecalculateIconPos();
				}
			}
		},
		0.15f,
		false
	);
}

void UInGameLayerWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	// 코인 플라이아웃 중에는 Money 업데이트 억제 (점진적 표시 중)
	if (bSuppressMoneyUpdate && Type == EResourceType::Money) return;

	UE_LOG(LogTemp, Warning, TEXT("[InGameLayerWidget] HandleResourceChanged: Type=%d, NewValue=%lld"),
		(int32)Type, NewValue);

	if (UResourceWidget** FoundWidget = ResourceWidgets.Find(Type))
	{
		if (*FoundWidget)
		{
			(*FoundWidget)->SetValue(NewValue);
			UE_LOG(LogTemp, Log, TEXT("[InGameLayerWidget] Updated ResourceWidget[%d] to %lld"), (int32)Type, NewValue);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[InGameLayerWidget] Failed to find widget for ResourceType=%d"), (int32)Type);
	}
}


UResourceWidget* UInGameLayerWidget::GetResourceWidget(EResourceType Type)
{
	// TMap에서 Type으로 직접 검색
	if (UResourceWidget** FoundWidget = ResourceWidgets.Find(Type))
	{
		return *FoundWidget;
	}

	return nullptr;
}

void UInGameLayerWidget::SpawnGainPopup(EResourceType Type, int64 DeltaAmount)
{
	SpawnAmountPopup(Type, DeltaAmount, /*bSpend=*/false);
}

void UInGameLayerWidget::SpawnSpendPopup(EResourceType Type, int64 DeltaAmount)
{
	SpawnAmountPopup(Type, DeltaAmount, /*bSpend=*/true);
}

void UInGameLayerWidget::SpawnAmountPopup(EResourceType Type, int64 Amount, bool bSpend)
{
	if (Amount <= 0) return;

	// 스팸 게이트 — 같은 (Type, bSpend) 는 0.2s 배치 윈도우로 합산 후 1회 스폰 (BrickCollect 의 BatchTotal 패턴)
	const uint32 BatchKey = (static_cast<uint32>(Type) << 1) | (bSpend ? 1u : 0u);
	FPendingAmountPopup& Pending = PendingAmountPopups.FindOrAdd(BatchKey);
	if (Pending.Accum <= 0)
	{
		Pending.Type = Type;
		Pending.bSpend = bSpend;
		Pending.Remaining = AmountPopupBatchWindow;
	}
	Pending.Accum += Amount;
}

void UInGameLayerWidget::FlushPendingAmountPopups(float DeltaTime)
{
	if (PendingAmountPopups.Num() == 0) return;

	for (auto It = PendingAmountPopups.CreateIterator(); It; ++It)
	{
		It->Value.Remaining -= DeltaTime;
		if (It->Value.Remaining <= 0.f)
		{
			SpawnAmountPopupNow(It->Value.Type, It->Value.Accum, It->Value.bSpend);
			It.RemoveCurrent();
		}
	}
}

void UInGameLayerWidget::SpawnAmountPopupNow(EResourceType Type, int64 Amount, bool bSpend)
{
	if (Amount <= 0) return;

	UResourceWidget* Counter = GetResourceWidget(Type);
	if (!Counter || !EffectCanvas) return;

	// 아이콘 중앙 스크린 좌표 — 코인 플라이아웃 타겟과 동일. 아이콘 위에서 떠오름.
	if (!Counter->CalculateIconScreenPos()) return;
	const FVector2D IconAbs = Counter->GetCachedIconScreenPos();

	// 지출=레드 / 획득=DT_Resource.UIColor (카운터 텍스트와 같은 타입색 SOT — 하드코딩 웜톤 금지)
	FLinearColor Color = FLinearColor(1.0f, 0.78f, 0.45f, 1.0f);
	if (bSpend)
	{
		Color = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
	}
	else if (TableMgr)
	{
		bool bColorOk = false;
		const FResourceInfo Info = TableMgr->GetResourceInfo(Type, bColorOk);
		if (bColorOk)
		{
			Color = Info.UIColor;
		}
	}

	const FText Num = UGlobalUtilFunctions::FormatExactNumber(Amount);
	const FText Text = bSpend
		? FText::Format(NSLOCTEXT("FloatingNumber", "SpendFmt", "-{0}"), Num)
		: FText::Format(NSLOCTEXT("FloatingNumber", "GainFmt", "+{0}"), Num);

	UFloatingNumberWidget::Spawn(this, EffectCanvas, IconAbs, Text, Color, GainPopupStackIndex++);
}

FVector2D UInGameLayerWidget::GetBrickIconScreenPosition() const
{
	// TMap에서 Brick 타입의 위젯을 찾음
	if (const UResourceWidget* const* BrickWidget = ResourceWidgets.Find(EResourceType::Brick))
	{
		if (*BrickWidget)
		{
			return (*BrickWidget)->GetCachedIconScreenPos();
		}
	}

	return FVector2D::ZeroVector;
}

namespace
{
	// 벽돌 착지음 배치 간 감쇠 스펙 (확정: x0.7, 하한 0.35, 0.8s 리셋) — 만렙 생산간격 0.2s = 초당 5발 대응
	constexpr float BrickLandSoundDecayMult = 0.7f;
	constexpr float BrickLandSoundMinVolume = 0.35f;
	constexpr double BrickLandSoundResetSeconds = 0.8;
}

void UInGameLayerWidget::SpawnBrickCollectAt(FVector2D Startpos, EResourceType TargetResourceType, int CollectAmount)
{
	if (!EffectCanvas) return;

	TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::BrickCollection);
	check(WidgetClass);

	FGeometry EffectCanvasGeo = EffectCanvas->GetCachedGeometry();

	// 뷰포트 시작점과 HUD 절대 타깃을 풀스크린 이펙트 레이어의 로컬 좌표로 통일한다.
	FVector2D CanvasLocalStart = EffectCanvasGeo.AbsoluteToLocal(
		UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(Startpos)
	);

	FVector2D TargetAbsPos = FVector2D::ZeroVector;
	if (const UResourceWidget* const* TargetWidget = ResourceWidgets.Find(TargetResourceType))
	{
		if (*TargetWidget)
		{
			TargetAbsPos = (*TargetWidget)->GetCachedIconScreenPos();
		}
	}
	FVector2D CanvasLocalTarget = EffectCanvasGeo.AbsoluteToLocal(TargetAbsPos);

	// 생산량을 아이콘 여러 개로 분배 (나머지는 마지막 아이콘에 가산)
	const int32 IconCount = FMath::Clamp(CollectAmount, 1, BrickBurstMaxIcons);
	const int32 PerIcon = CollectAmount / IconCount;
	const int32 Remainder = CollectAmount - PerIcon * IconCount;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::BrickPop);
		}
	}

	// 착지음 감쇠 볼륨 산출 — 배치(=착지 1발) 간격이 리셋 윈도우보다 짧으면 연쇄 감쇠.
	// 비행시간이 상수라 스폰 간격 = 착지 간격 (스폰 시점 판정으로 충분).
	const double NowSeconds = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
	if (NowSeconds - LastBrickLandSoundTime >= BrickLandSoundResetSeconds)
	{
		BrickLandSoundVolume = 1.0f;
	}
	else
	{
		BrickLandSoundVolume = FMath::Max(BrickLandSoundVolume * BrickLandSoundDecayMult, BrickLandSoundMinVolume);
	}
	LastBrickLandSoundTime = NowSeconds;

	for (int32 i = 0; i < IconCount; ++i)
	{
		UBrickCollectWidget* BrickW = CreateWidget<UBrickCollectWidget>(this, WidgetClass);
		if (!BrickW) continue;

		UCanvasPanelSlot* SlotIcon = EffectCanvas->AddChildToCanvas(BrickW);
		if (!SlotIcon) continue;

		SlotIcon->SetAutoSize(true);
		SlotIcon->SetAnchors(FAnchors(0, 0, 0, 0));
		SlotIcon->SetAlignment(FVector2D(0.5f, 0.5f));
		SlotIcon->SetPosition(CanvasLocalStart);

		const bool bLast = (i == IconCount - 1);
		BrickW->InitCollect(CanvasLocalStart, CanvasLocalTarget, SlotIcon, bLast ? PerIcon + Remainder : PerIcon,
			i * BrickBurstStagger, bLast, CollectAmount, BrickLandSoundVolume);
	}
}

// ============================================================
// 건물 버블 시스템
// ============================================================

void UInGameLayerWidget::RewireBuildingBubbleSubscriptions()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	const TArray<ABuildingBaseActor*>& Buildings = EntityMgr->GetBuildings();
	for (ABuildingBaseActor* Building : Buildings)
	{
		if (!Building) continue;
		// 중복 바인딩 방지 — 동일 (Object, Function) 조합은 RemoveDynamic 후 AddDynamic
		Building->OnBubbleRefreshRequested.RemoveDynamic(this, &UInGameLayerWidget::HandleBuildingBubbleRefreshRequested);
		Building->OnBubbleRefreshRequested.AddDynamic(this, &UInGameLayerWidget::HandleBuildingBubbleRefreshRequested);
	}
}

void UInGameLayerWidget::HandleBuildingBubbleRefreshRequested(int32 BuildingIndex)
{
	// 단일 갱신은 전역 상한(3개)을 깨므로 다음 틱 전역 재평가로 합류
	bBubbleReevalPending = true;
	RequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::CompanyOrBubbleRefresh);

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			UpdateBuildingWindowLight(BuildingIndex, OpMgr->HasActiveOperation(BuildingIndex));
		}
	}
}

void UInGameLayerWidget::EvaluateAllBuildingBubbles()
{
	if (!BubbleContainer) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	UCGGameInstance* GI = Cast<UCGGameInstance>(World->GetGameInstance());
	UProjectOperationManager* OpMgr = GI ? GI->GetSubsystem<UProjectOperationManager>() : nullptr;

	// 1) 건물별 타입 산출 (+ 창문 조명 동기화)
	struct FBubbleSlotCandidate
	{
		int32 BuildingIndex = INDEX_NONE;
		EBubbleType Type = EBubbleType::None;
		bool bOnScreen = false;
	};

	const TArray<ABuildingBaseActor*>& Buildings = EntityMgr->GetBuildings();
	TArray<FBubbleSlotCandidate> Evaluated;
	Evaluated.Reserve(Buildings.Num());
	for (ABuildingBaseActor* Building : Buildings)
	{
		if (!Building) continue;

		FBubbleSlotCandidate Cand;
		Cand.BuildingIndex = Building->GetBuildingIndex();
		Cand.Type = EvaluateBubbleTypeForBuilding(Cand.BuildingIndex);

		// 화면 판정은 캡 경합에 실제로 들어가는 것만 — None 은 어차피 최후순위라 200건물분 투영이 순수 낭비다.
		if (Cand.Type != EBubbleType::None)
		{
			Cand.bOnScreen = BubbleContainer->IsBuildingAnchorOnScreen(Cand.BuildingIndex);
		}
		Evaluated.Add(Cand);

		if (OpMgr)
		{
			Building->SetWindowLightActive(ShouldWindowLightBeOn(Building, OpMgr));
		}
	}

	// 2) 전역 동시 표시 상한 — 화면 내 우선, 그 다음 우선순위(타입값 오름차순) 상위 MaxConcurrentBubbles 만 유지.
	// ⚠ 캡은 컨테이너의 화면 판정보다 먼저 걸린다 — 화면 밖 건물이 BuildingIndex 순으로 슬롯을 먹으면
	// 화면 안 만금고가 아무 표시도 못 받는다(캡을 올려도 이것만으로는 안 고쳐지는 부분).
	// 화면 안 그룹 내부의 tie-break 는 그대로 타입값 → BuildingIndex — 프레임 간 순서 요동 = 깜빡임 방지.
	Evaluated.Sort([](const FBubbleSlotCandidate& A, const FBubbleSlotCandidate& B)
	{
		if (A.bOnScreen != B.bOnScreen) return A.bOnScreen;
		const uint8 RankA = (A.Type == EBubbleType::None) ? MAX_uint8 : static_cast<uint8>(A.Type);
		const uint8 RankB = (B.Type == EBubbleType::None) ? MAX_uint8 : static_cast<uint8>(B.Type);
		if (RankA != RankB) return RankA < RankB;
		return A.BuildingIndex < B.BuildingIndex;
	});
	// 전역 동시 표시 상한 — 우선순위 상위 MaxConcurrentBubbles 개만 (결산 대기 등 행동 버블 대상)
	int32 KeptCount = 0;
	for (FBubbleSlotCandidate& Entry : Evaluated)
	{
		if (Entry.Type == EBubbleType::None)
		{
			continue;
		}
		if (++KeptCount > MaxConcurrentBubbles)
		{
			Entry.Type = EBubbleType::None;
		}
	}

	// 3) 최종 타입 적용 — 초과분/조건 해제는 None (컨테이너가 제거 애니메이션 처리)
	for (const FBubbleSlotCandidate& Entry : Evaluated)
	{
		BubbleContainer->UpdateBubbleForBuilding(Entry.BuildingIndex, Entry.Type);
	}
}

// ============================================================
// 부지 가격 배지 — 인접-미소유 부지 위 상시(잠금+가격). WBP UIE_PlotPriceBadge(UPlotPriceBadgeWidget) 풀.
// ============================================================

bool UInGameLayerWidget::ProjectPlotAnchorToCanvas(const ACityPlotActor* Plot, FVector2D& OutCanvasLocal) const
{
	OutCanvasLocal = FVector2D::ZeroVector;
	if (!Plot || !InGameCanvas) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return false;

	// BubbleContainer 의 검증된 변환: 3D 앵커 → Viewport 로컬(논리 픽셀, DPI 보정) → Absolute → CanvasGeo.AbsoluteToLocal.
	const FVector AnchorWorld = Plot->GetBubbleAnchorPosition();

	FVector2D ViewportPos;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, AnchorWorld, ViewportPos, true))
	{
		return false;
	}

	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(World);
	const FVector2D AbsolutePos = ViewportGeo.LocalToAbsolute(ViewportPos);

	const FGeometry CanvasGeo = InGameCanvas->GetCachedGeometry();
	OutCanvasLocal = CanvasGeo.AbsoluteToLocal(AbsolutePos);

	const FVector2D CanvasSize = CanvasGeo.GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return false;
	}
	constexpr float OffscreenMargin = 200.0f;
	if (OutCanvasLocal.X < -OffscreenMargin || OutCanvasLocal.X > CanvasSize.X + OffscreenMargin ||
		OutCanvasLocal.Y < -OffscreenMargin || OutCanvasLocal.Y > CanvasSize.Y + OffscreenMargin)
	{
		return false;
	}

	return true;
}

UPlotPriceBadgeWidget* UInGameLayerWidget::GetOrCreatePlotBadge(int32 PoolIndex)
{
	if (PlotPriceBadgePool.IsValidIndex(PoolIndex) && PlotPriceBadgePool[PoolIndex])
	{
		return PlotPriceBadgePool[PoolIndex];
	}

	if (!InGameCanvas || !TableMgr)
	{
		return nullptr;
	}

	// 표준 패턴: EWidgetType + DT_WidgetClass 로 WBP(UIE_PlotPriceBadge) 클래스 로드.
	// 미등록(DT 행/WBP 미생성)이면 graceful 스킵 — 배지 없이 진행 + 1회 경고.
	TSubclassOf<UUserWidget> BadgeClass = TableMgr->GetWidgetClass(EWidgetType::PlotPriceBadge);
	if (!BadgeClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InGameLayer] PlotPriceBadge 위젯 클래스 미등록(DT_WidgetClass 행 + UIE_PlotPriceBadge 확인) — 배지 생략."));
		return nullptr;
	}

	UPlotPriceBadgeWidget* Badge = CreateWidget<UPlotPriceBadgeWidget>(this, BadgeClass);
	if (!Badge)
	{
		return nullptr;
	}

	UCanvasPanelSlot* BadgeSlot = InGameCanvas->AddChildToCanvas(Badge);
	if (BadgeSlot)
	{
		BadgeSlot->SetAutoSize(true);
		BadgeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		// 월드 추적 오버레이는 HUD 크롬(자원 카운터 등) 뒤로 — BubbleContainer(-1)와 동일. 미설정 시 런타임 후순위 Add 라 ZOrder 0 크롬 위를 덮음.
		BadgeSlot->SetZOrder(-1);
	}

	// 인덱스 슬롯 보장(중간 빈칸이 생기지 않게 순차 Add 만 함 — PoolIndex 는 항상 Num() 와 일치하게 호출).
	PlotPriceBadgePool.Add(Badge);
	return Badge;
}

void UInGameLayerWidget::RefreshPlotPriceBadges()
{
	if (!InGameCanvas) return;

	UWorld* World = GetWorld();
	if (!World) return;

	USpawnManager* SpawnMgr = World->GetSubsystem<USpawnManager>();
	if (!SpawnMgr) return;

	UTableManagerSubsystem* TblMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TblMgr) return;

	UResourceItemManager* RMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr;
	const int64 OwnedMoney = RMgr ? RMgr->GetResourceAmount(EResourceType::Money) : 0;

	// 인접-미소유 부지만 배지 표시(원본 인접 마커와 동일 정의 — 소유 변경 시 한 겹씩 바깥으로 번짐).
	TArray<ACityPlotActor*> AdjacentPlots;
	SpawnMgr->GetAdjacentUnownedPlots(AdjacentPlots);

	int32 UsedCount = 0;
	for (ACityPlotActor* Plot : AdjacentPlots)
	{
		if (!IsValid(Plot)) continue;

		bool bOk = false;
		const FCityPlotData PlotData = TblMgr->GetCityPlotData(Plot->GetPlotId(), bOk);
		if (!bOk) continue;

		FVector2D CanvasLocal;
		const bool bOnScreen = ProjectPlotAnchorToCanvas(Plot, CanvasLocal);

		UPlotPriceBadgeWidget* Badge = GetOrCreatePlotBadge(UsedCount);
		if (!Badge) continue;

		// 가격 텍스트(축약) + 자금 부족 빨강.
		const FText PriceText = UGlobalUtilFunctions::AbbreviateNumber(
			PlotData.MoneyPrice, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil);
		const bool bCanAfford = (OwnedMoney >= PlotData.MoneyPrice);

		Badge->SetPlot(Plot);
		Badge->SetPrice(PriceText, bCanAfford);

		const ESlateVisibility DesiredVis = bOnScreen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (Badge->GetVisibility() != DesiredVis)
		{
			Badge->SetVisibility(DesiredVis);
		}

		if (bOnScreen)
		{
			if (UCanvasPanelSlot* BadgeSlot = Cast<UCanvasPanelSlot>(Badge->Slot))
			{
				BadgeSlot->SetPosition(CanvasLocal);
			}
		}

		++UsedCount;
	}

	// 매 틱 위치 추적(UpdatePlotBadgePositions)이 순회할 활성 배지 수 확정.
	ActivePlotBadgeCount = UsedCount;
	bForcePlotBadgeReproject = true; // 배지 셋 갱신됨 → 다음 틱 카메라 게이트 무시하고 1회 재투영

	// 남는 풀 배지는 숨김(파괴 대신 재사용 — 부지 수 변동이 잦지 않음).
	for (int32 i = UsedCount; i < PlotPriceBadgePool.Num(); ++i)
	{
		if (PlotPriceBadgePool[i] && PlotPriceBadgePool[i]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			PlotPriceBadgePool[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// 매 틱 — 활성 배지(인접-미소유)만 부지 앵커를 재투영해 화면 위치를 갱신(카메라 추적). 가격/자금색은 손대지 않음.
void UInGameLayerWidget::UpdatePlotBadgePositions()
{
	if (!InGameCanvas) return;

	// [Perf] 부지는 고정 → 화면 위 배지 위치는 오직 카메라 시점에 의존. 시점 불변이면 매 틱 재투영 스킵
	// (idle 게임이라 카메라가 대부분 정지 → 거의 매 틱 여기서 빠져나감). 배지 셋 변경 시엔 bForce로 1회 통과.
	if (APlayerController* PC = GetOwningPlayer())
	{
		FVector ViewLoc; FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		if (!bForcePlotBadgeReproject && ViewLoc.Equals(LastBadgeViewLoc, 0.1f) && ViewRot.Equals(LastBadgeViewRot, 0.01f))
		{
			return;
		}
		LastBadgeViewLoc = ViewLoc;
		LastBadgeViewRot = ViewRot;
		bForcePlotBadgeReproject = false;
	}

	const int32 Count = FMath::Min(ActivePlotBadgeCount, PlotPriceBadgePool.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		UPlotPriceBadgeWidget* Badge = PlotPriceBadgePool[i];
		if (!Badge) continue;

		const ACityPlotActor* Plot = Badge->GetPlot();
		if (!IsValid(Plot))
		{
			if (Badge->GetVisibility() != ESlateVisibility::Collapsed)
			{
				Badge->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}

		FVector2D CanvasLocal;
		const bool bOnScreen = ProjectPlotAnchorToCanvas(Plot, CanvasLocal);

		const ESlateVisibility DesiredVis = bOnScreen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (Badge->GetVisibility() != DesiredVis)
		{
			Badge->SetVisibility(DesiredVis);
		}

		if (bOnScreen)
		{
			if (UCanvasPanelSlot* BadgeSlot = Cast<UCanvasPanelSlot>(Badge->Slot))
			{
				BadgeSlot->SetPosition(CanvasLocal);
			}
		}
	}
}

// ============================================================
// 금고 게이지 — 건물 위 상시 표시 (길이=금고 채움, 색=감쇠)
// ============================================================

bool UInGameLayerWidget::ProjectBuildingGaugeGeometry(const ABuildingBaseActor* Building,
	FVector2D& OutCanvasLocal, float& OutScreenWidth) const
{
	OutCanvasLocal = FVector2D::ZeroVector;
	OutScreenWidth = 0.0f;
	if (!Building || !InGameCanvas)
	{
		return false;
	}

	UWorld* GaugeWorld = GetWorld();
	if (!GaugeWorld)
	{
		return false;
	}
	APlayerController* PC = GaugeWorld->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	// BubbleContainer 의 검증된 변환: 3D -> Viewport 로컬(DPI 보정) -> Absolute -> Canvas 로컬
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(GaugeWorld);
	const FGeometry CanvasGeo = InGameCanvas->GetCachedGeometry();

	auto ProjectOne = [&](const FVector& WorldPos, FVector2D& Out) -> bool
	{
		FVector2D ViewportPos;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldPos, ViewportPos, true))
		{
			return false;
		}
		Out = CanvasGeo.AbsoluteToLocal(ViewportGeo.LocalToAbsolute(ViewportPos));
		// 투영이 실패하면 좌표가 NaN 으로 나올 수 있다 — 아래 화면밖 판정은 NaN 비교가 전부 false 라 통과시켜 버린다
		return FMath::IsFinite(Out.X) && FMath::IsFinite(Out.Y);
	};

	const FVector AnchorWorld = Building->GetBubbleAnchorPosition();
	if (!ProjectOne(AnchorWorld, OutCanvasLocal))
	{
		return false;
	}

	const FVector2D CanvasSize = CanvasGeo.GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return false;
	}
	constexpr float OffscreenMargin = 200.0f;
	if (OutCanvasLocal.X < -OffscreenMargin || OutCanvasLocal.X > CanvasSize.X + OffscreenMargin ||
		OutCanvasLocal.Y < -OffscreenMargin || OutCanvasLocal.Y > CanvasSize.Y + OffscreenMargin)
	{
		return false;
	}

	// 화면상 건물 폭 = 옥상 바운드의 X 반경을 한 번 더 투영해 수평 거리로 환산.
	// 게이지가 건물보다 넓어지면 어느 건물 것인지 모호해지므로 폭의 기준이 필요하다.
	const FBox Roof = Building->GetRoofBounds();
	const FVector Extent = Roof.GetExtent();
	FVector2D EdgeLocal;
	if (ProjectOne(AnchorWorld + FVector(Extent.X, 0.0f, 0.0f), EdgeLocal))
	{
		OutScreenWidth = FMath::Abs(EdgeLocal.X - OutCanvasLocal.X) * 2.0f;
	}

	// NaN 이 후보 배열에 들어가면 DistSq 정렬의 strict weak ordering 이 깨져 TArray::Sort 가 UB 가 된다
	// (NaN != finite 이 항상 true → pred(A,B) 와 pred(B,A) 가 둘 다 false = 비전이적 비교불가).
	return FMath::IsFinite(OutCanvasLocal.X) && FMath::IsFinite(OutCanvasLocal.Y)
		&& FMath::IsFinite(OutScreenWidth);
}

UVaultGaugeWidget* UInGameLayerWidget::GetOrCreateVaultGauge(int32 PoolIndex)
{
	if (VaultGaugePool.IsValidIndex(PoolIndex) && VaultGaugePool[PoolIndex])
	{
		return VaultGaugePool[PoolIndex];
	}

	if (!InGameCanvas || !TableMgr)
	{
		return nullptr;
	}

	TSubclassOf<UUserWidget> GaugeClass = TableMgr->GetWidgetClass(EWidgetType::VaultGauge);
	if (!GaugeClass)
	{
		// DT 행/WBP 미등록이면 게이지 없이 진행 + 경고 (PlotPriceBadge 와 동일한 graceful 스킵).
		// 호출부가 첫 실패에서 break 하므로 재평가 1회당 1줄 — 매 프레임/건물당이 아니다.
		UE_LOG(LogTemp, Warning,
			TEXT("[InGameLayer] VaultGauge 위젯 클래스 미등록(DT_WidgetClass 행 + UIE_VaultGauge 확인) — 게이지 생략."));
		return nullptr;
	}

	UVaultGaugeWidget* Gauge = CreateWidget<UVaultGaugeWidget>(this, GaugeClass);
	if (!Gauge)
	{
		return nullptr;
	}

	// ⚠ 지역변수를 Slot 으로 받으면 C4458 -> 빌드 실패 (UWidget 에 Slot 멤버가 있다)
	if (UCanvasPanelSlot* GaugeSlot = InGameCanvas->AddChildToCanvas(Gauge))
	{
		GaugeSlot->SetAutoSize(true);
		GaugeSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		// 월드 추적 오버레이는 HUD 크롬 뒤로 — BubbleContainer/PlotPriceBadge 와 동일한 -1
		GaugeSlot->SetZOrder(-1);
	}

	VaultGaugePool.Add(Gauge);
	return Gauge;
}

void UInGameLayerWidget::EvaluateGauge(UProjectOperationManager* OpMgr, int32 BuildingID,
	EGaugeHealth& OutHealth, float& OutProgress) const
{
	OutProgress = 0.0f;
	bool bHasOp = false;
	if (OpMgr)
	{
		if (const FOperationData* Op = OpMgr->GetOperationByBuildingID(BuildingID))
		{
			bHasOp = true;
			OutProgress = ComputeOperationProgress(Op->ElapsedTime, Op->TotalOperationTime);
		}
	}
	OutHealth = ClassifyGaugeHealth(OutProgress, bHasOp);
}

float UInGameLayerWidget::GetVaultGaugeZoomValue() const
{
	const APlayerController* OwningController = GetOwningPlayer();
	const APlayerCamera* CameraPawn = OwningController
		? Cast<APlayerCamera>(OwningController->GetPawn())
		: nullptr;
	return CameraPawn && CameraPawn->MovementInputHandler
		? CameraPawn->MovementInputHandler->GetZoomValue()
		: 0.0f;
}

void UInGameLayerWidget::UpdateVaultGaugeLOD()
{
	const EVaultGaugeLOD NextLOD = ResolveVaultGaugeLOD(
		GetVaultGaugeZoomValue(), CurrentVaultGaugeLOD);
	if (NextLOD != CurrentVaultGaugeLOD)
	{
		CurrentVaultGaugeLOD = NextLOD;
		if (!bVaultGaugeInitialRefreshPending)
		{
			bVaultGaugeReevalPending = true;
		}
	}
}

void UInGameLayerWidget::RequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger Trigger)
{
	if (ShouldRequestVaultGaugeReconcile(Trigger))
	{
		bVaultGaugeReevalPending = true;
	}
}

void UInGameLayerWidget::RefreshVaultGauges()
{
	// 재평가가 곧 만차 인계의 실행이다 — 어느 경로로 들어와도 대기 요청은 여기서 소화된다.
	bVaultGaugeReevalPending = false;

	if (!InGameCanvas)
	{
		return;
	}
	UWorld* GaugeWorld = GetWorld();
	if (!GaugeWorld)
	{
		return;
	}

	auto HideAll = [this]()
	{
		ActiveGaugeCandidates.Reset();
		for (UVaultGaugeWidget* G : VaultGaugePool)
		{
			if (G && G->GetVisibility() != ESlateVisibility::Collapsed)
			{
				G->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		// 빈 후보 목록에 동기화 = 모든 리프트 해제. 여기서 빠뜨리면 게이지가 사라진 뒤 버블만 떠 있게 된다.
		ApplyBubbleLiftForActiveGauges();
	};

	// Hidden에서는 일반 게이지를 모두 숨기되, 설명 중인 단일 예약 대상만 기존 풀에 남긴다.
	if (CurrentVaultGaugeLOD == EVaultGaugeLOD::Hidden
		&& TutorialReservedGaugeBuildingIndex == INDEX_NONE)
	{
		HideAll();
		return;
	}

	UEntityManager* EntityMgr = GaugeWorld->GetSubsystem<UEntityManager>();
	UCGGameInstance* GI = Cast<UCGGameInstance>(GaugeWorld->GetGameInstance());
	UProjectOperationManager* OpMgr = GI ? GI->GetSubsystem<UProjectOperationManager>() : nullptr;
	if (!EntityMgr || !OpMgr)
	{
		HideAll();
		return;
	}

	// ---- 1) 후보 산출 ----
	TArray<FGaugeCandidate> Cands;
	const TArray<ABuildingBaseActor*>& Buildings = EntityMgr->GetBuildings();
	Cands.Reserve(Buildings.Num());

	for (ABuildingBaseActor* Building : Buildings)
	{
		if (!IsValid(Building))
		{
			continue;
		}

		// 프로젝트 업종(게임/금융/IT)만 후보 — 미입주/모뉴먼트(None)와 제조업(전자/반도체/자동차)을 함께 걷어낸다.
		// ⚠ 버블의 `!= None` 을 복제하면 안 된다: 버블은 뒤에 금액 검사가 붙어 제조업이 암묵적으로 걸러지지만
		// 게이지엔 금액 검사가 없다. 제조업은 FOperationData 를 아예 안 가져 영구 회색 Idle = "정지" 로 읽히고
		// (돌고 있는 공장에 대한 거짓말) 중앙 캡 16 슬롯을 정보량 0 으로 점유한다.
		// 술어는 BuildingManagePanelWidget 의 업종별 수익/주문 분기와 같은 IsProjectType — 갈라질 수 없게 공유한다.
		const ECompanyType CompanyType = Building->GetCompanyType();
		if (!IsProjectType(CompanyType))
		{
			continue;
		}

		const int32 Index = Building->GetBuildingIndex();
		if (!ShouldIncludeGaugeAtLOD(CurrentVaultGaugeLOD, Index, TutorialReservedGaugeBuildingIndex))
		{
			continue;
		}

		// 금고 상태는 Bar 와 직교 — 만액 버블과 공존한다(2026-08-12 축 전환)
		const bool bHasActiveOperation = OpMgr->HasActiveOperation(Index);
		if (!ShouldDisplayVaultGauge(bHasActiveOperation))
		{
			continue;
		}

		EGaugeHealth Health = EGaugeHealth::Idle;
		float OpProgress = 0.0f;
		EvaluateGauge(OpMgr, Index, Health, OpProgress);
		const bool bSelected = MarkerBuilding.Get() == Building;
		const bool bTutorialReserved = Index == TutorialReservedGaugeBuildingIndex;
		const EVaultGaugePresentation Presentation = ResolveGaugePresentation(
			CurrentVaultGaugeLOD, bSelected, bTutorialReserved);
		if (Presentation == EVaultGaugePresentation::Pearl && Health == EGaugeHealth::Idle)
		{
			continue;
		}

		FGaugeCandidate C;
		C.BuildingIndex = Index;
		C.BuildingPtr = Building;   // 매 틱 추적이 GetBuildingByIndex 선형 스캔을 반복하지 않게 여기서 캐시
		C.Health = Health;
		C.Presentation = Presentation;
		C.Progress = OpProgress;
		if (!ProjectBuildingGaugeGeometry(Building, C.CanvasPos, C.ScreenWidth))
		{
			continue;
		}
		Cands.Add(C);
	}

	// ---- 2) 중앙 근접 정렬 + 캡 절단 ----
	const FVector2D CanvasSize = InGameCanvas->GetCachedGeometry().GetLocalSize();
	const FVector2D ScreenCenter = CanvasSize * 0.5f;
	SelectGaugesByProximity(Cands, ScreenCenter, MaxConcurrentGauges, TutorialReservedGaugeBuildingIndex);

	// ---- 3) 값·색 주입 ----
	int32 Used = 0;
	for (const FGaugeCandidate& C : Cands)
	{
		UVaultGaugeWidget* Gauge = GetOrCreateVaultGauge(Used);
		if (!Gauge)
		{
			break;   // 클래스 미등록 — 경고는 GetOrCreate 가 이미 냈다
		}

		Gauge->SetPresentation(C.Progress, C.Health, C.Presentation);

		// ⚠ 가시화보다 먼저 위치·폭을 걸어야 한다 — NativeTick 은 UpdateVaultGaugePositions 를 이 함수보다
		// 먼저 돌리므로, 여기서 안 걸면 새로 만든/재배정된 게이지가 한 프레임 캔버스 좌상단(0,0) 또는
		// 이전 건물 자리에 뜬다. 좌표·폭은 이미 후보에 들어 있어 재투영도 필요 없다.
		ApplyGaugeTransform(Gauge, C.CanvasPos, C.ScreenWidth);

		if (Gauge->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			// 클릭은 아래 월드/버블로 통과해야 한다 — 게이지는 순수 표시물
			Gauge->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		++Used;
	}

	// 남는 풀은 접어둔다
	for (int32 i = Used; i < VaultGaugePool.Num(); ++i)
	{
		if (VaultGaugePool[i] && VaultGaugePool[i]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			VaultGaugePool[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 매 틱 위치 추적이 순회할 범위 확정
	Cands.SetNum(Used);
	ActiveGaugeCandidates = MoveTemp(Cands);

	// 후보가 확정된 직후에만 리프트를 다시 세운다 ― 멤버십의 단일 소유자가 여기 하나여야 갈라지지 않는다
	ApplyBubbleLiftForActiveGauges();

	// 후보 집합이 바뀌었으니 시점 캐시와 무관하게 다음 틱 1회는 재투영시킨다 (형제 배지의 bForce 와 같은 역할)
	bForceVaultGaugeReproject = true;
}

void UInGameLayerWidget::ApplyBubbleLiftForActiveGauges()
{
	if (!BubbleContainer)
	{
		return;
	}

	// 후보 목록이 곧 리프트 대상 ― 직전에 올려둔 건물은 여기서 전부 0 으로 되돌아간다
	BubbleContainer->ClearAllExtraLifts();
	for (const FGaugeCandidate& C : ActiveGaugeCandidates)
	{
		BubbleContainer->SetExtraLiftForBuilding(
			C.BuildingIndex,
			ComputeBubbleLiftPx(C.Presentation, BubbleLiftForBarPx, BubbleLiftForPearlPx));
	}
}

void UInGameLayerWidget::RefreshVaultGaugeValues()
{
	UProjectOperationManager* OpMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr;
	if (!OpMgr)
	{
		return;
	}

	for (int32 i = 0; i < ActiveGaugeCandidates.Num(); ++i)
	{
		if (!VaultGaugePool.IsValidIndex(i))
		{
			break;
		}
		UVaultGaugeWidget* Gauge = VaultGaugePool[i];
		if (!Gauge || Gauge->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		FGaugeCandidate& C = ActiveGaugeCandidates[i];
		EvaluateGauge(OpMgr, C.BuildingIndex, C.Health, C.Progress);
		// 값 주입은 SetPresentation 한 경로만 ― 위젯이 마지막 적용값을 캐시하므로 밖에서 바를 직접 만지면 캐시가 어긋난다
		Gauge->SetPresentation(C.Progress, C.Health, C.Presentation);
	}
}

void UInGameLayerWidget::ApplyGaugeTransform(UVaultGaugeWidget* Gauge, const FVector2D& CanvasPos, float ScreenWidth) const
{
	if (!Gauge)
	{
		return;
	}

	// ⚠ 슬롯 Position 이 아니라 RenderTranslation — 슬롯 위치 쓰기는 캔버스 레이아웃을 무효화한다.
	// 게이지 N개 × 매 프레임이면 그 비용이 그대로 곱해진다.
	Gauge->SetRenderTranslation(FVector2D(CanvasPos.X, CanvasPos.Y - GaugeLiftPx));

	// 원근에 따라 폭을 조정 — 줌 중에도 즉시 따라오게 매 틱 계산한다(투영 2회, 게이지 수는 캡으로 묶여 있다)
	Gauge->SetGaugeWidth(ComputeGaugeWidth(ScreenWidth, GaugeWidthRatio, MinGaugeWidth, MaxGaugeWidth));
}

void UInGameLayerWidget::UpdateVaultGaugePositions()
{
	if (ActiveGaugeCandidates.Num() == 0)
	{
		return;
	}

	const int32 Count = FMath::Min(ActiveGaugeCandidates.Num(), VaultGaugePool.Num());
	bool bNeedsReproject = InGameCanvas != nullptr;

	// [Perf] 건물은 고정 → 화면 위 게이지 위치는 오직 카메라 시점에 의존. 형제 UpdatePlotBadgePositions 와 같은
	// 시점 캐시로 매 틱 재투영을 스킵한다. 후보 만료 판정은 아래 단일 루프에서 항상 먼저 처리한다.
	// 후보 재배정 후에는 bForce 로 1회 통과 — 그때는 위치가 아니라 대상이 바뀌었다.
	if (bNeedsReproject)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			bNeedsReproject = bForceVaultGaugeReproject
				|| !ViewLoc.Equals(LastGaugeViewLoc, 0.1f)
				|| !ViewRot.Equals(LastGaugeViewRot, 0.01f);
			if (bNeedsReproject)
			{
				LastGaugeViewLoc = ViewLoc;
				LastGaugeViewRot = ViewRot;
				bForceVaultGaugeReproject = false;
			}
		}
	}

	for (int32 i = 0; i < Count; ++i)
	{
		ABuildingBaseActor* Building = ActiveGaugeCandidates[i].BuildingPtr.Get();
		UVaultGaugeWidget* Gauge = VaultGaugePool[i];
		const EVaultGaugeCandidateFrameAction FrameAction = ResolveVaultGaugeCandidateFrameAction(
			IsValid(Building),
			bNeedsReproject);
		switch (FrameAction)
		{
		case EVaultGaugeCandidateFrameAction::CollapseAndReconcile:
			if (Gauge && Gauge->GetVisibility() != ESlateVisibility::Collapsed)
			{
				Gauge->SetVisibility(ESlateVisibility::Collapsed);
			}
			RequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::TrackedBuildingExpired);
			continue;
		case EVaultGaugeCandidateFrameAction::SkipReprojection:
			continue;
		case EVaultGaugeCandidateFrameAction::ProjectCachedBuilding:
			break;
		default:
			continue;
		}

		if (!Gauge)
		{
			continue;
		}

		// ⚠ 현재 가시성으로 스킵하지 말 것 — 오프스크린 분기가 접은 게이지를 바로 그 가드가 다시 걸러
		// 화면으로 돌아와도 다음 재평가(최대 5초)까지 못 살아난다. 투영을 먼저 하고 결과로 가시성을 정한다.
		// 되살림이 안전한 근거: i < Count 는 RefreshVaultGauges 가 "떠야 한다" 고 확정한 목록(Used 로 절단)이라
		// 의도적으로 안 쓰는 풀 항목은 이 범위에 애초에 들어오지 않는다.
		// 액터는 후보에 캐시된 약참조로 — 인덱스 조회는 선형 스캔이라 200건물 × 캡 16 이면 프레임당 수천 비교다.

		FVector2D CanvasLocal;
		float ScreenWidth = 0.0f;
		const bool bOnScreen = IsValid(Building)
			&& ProjectBuildingGaugeGeometry(Building, CanvasLocal, ScreenWidth);

		// 가시화 전에 위치·폭 — 순서가 뒤집히면 한 프레임 옛 좌표로 보인다.
		if (bOnScreen)
		{
			ApplyGaugeTransform(Gauge, CanvasLocal, ScreenWidth);
		}

		const ESlateVisibility DesiredVis = bOnScreen
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (Gauge->GetVisibility() != DesiredVis)
		{
			// 같은 값 재대입 방어 — Slate invalidate 누적 회피
			Gauge->SetVisibility(DesiredVis);
		}
	}
}

void UInGameLayerWidget::HandleWarehouseUpdated(int32 BuildingID, float Amount, float Capacity)
{
	// Bar 는 시간 축이라 창고 값과 무관하다 ― 여기서 할 일은 만액 시 버블 인계 재평가뿐이다.
	// ⚠ 전이(비만액 <-> 만액)에서만 올린다. 매니저가 활성 운영마다 1Hz 로 쏘므로 만액이면 무조건 올리면
	// 만액 건물 하나가 ~200건물 버블 스윕(EvaluateAllBuildingBubbles)을 매초 고정시킨다 = 5초 폴링의 5배.
	// "그냥 올리면 되지" 로 되돌리지 말 것.
	const bool bNowFull = (Capacity > 0.0f && Amount >= Capacity);
	const bool bWasFull = FullVaultBuildings.Contains(BuildingID);
	if (bNowFull != bWasFull)
	{
		if (bNowFull)
		{
			FullVaultBuildings.Add(BuildingID);
		}
		else
		{
			FullVaultBuildings.Remove(BuildingID);
		}
		bBubbleReevalPending = true;
	}
}

void UInGameLayerWidget::RefreshRevenueRateChip()
{
	// 미배치(WBP 트리 paste 전)면 조용히 스킵 — PlotPriceBadge 와 같은 롤아웃 패턴
	if (!RevenueRateChip)
	{
		return;
	}

	UCGGameInstance* RateGI = Cast<UCGGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr);
	UProjectOperationManager* RateOpMgr = RateGI ? RateGI->GetSubsystem<UProjectOperationManager>() : nullptr;
	if (!RateOpMgr)
	{
		return;
	}

	// 신규 계산 0줄 — 이미 존재하는 회사 전체 유량을 그대로 넘긴다. 접힘/포맷/펄스는 칩이 소유.
	RevenueRateChip->SetRate(RateOpMgr->GetCompanyNetPerSec());
}

UWidget* UInGameLayerWidget::GetTutorialRevenueBubbleWidget(int32 BuildingIndex) const
{
	return BubbleContainer ? BubbleContainer->GetBubbleWidgetForAnchor(BuildingIndex) : nullptr;
}

UWidget* UInGameLayerWidget::GetRevenueRateChipWidget() const
{
	return RevenueRateChip;
}

UWidget* UInGameLayerWidget::GetVaultGaugeWidgetForBuilding(int32 BuildingIndex) const
{
	// 후보 i 번째가 풀 i 번째 위젯 — RefreshVaultGauges 가 세우는 대응이다
	const int32 Idx = ActiveGaugeCandidates.IndexOfByPredicate(
		[BuildingIndex](const FGaugeCandidate& C) { return C.BuildingIndex == BuildingIndex; });
	if (Idx == INDEX_NONE || !VaultGaugePool.IsValidIndex(Idx))
	{
		return nullptr;
	}
	UVaultGaugeWidget* Gauge = VaultGaugePool[Idx];
	return (Gauge && Gauge->GetVisibility() != ESlateVisibility::Collapsed) ? Gauge : nullptr;
}

void UInGameLayerWidget::ReserveTutorialVaultGauge(int32 BuildingIndex)
{
	if (BuildingIndex == INDEX_NONE)
	{
		return;
	}
	TutorialReservedGaugeBuildingIndex = BuildingIndex;
	bVaultGaugeReevalPending = true;
}

void UInGameLayerWidget::ReleaseTutorialVaultGauge()
{
	if (TutorialReservedGaugeBuildingIndex == INDEX_NONE)
	{
		return;
	}
	TutorialReservedGaugeBuildingIndex = INDEX_NONE;
	bVaultGaugeReevalPending = true;
}

UWidget* UInGameLayerWidget::GetBubbleWidgetForBuilding(int32 BuildingIndex) const
{
	return BubbleContainer ? BubbleContainer->GetBubbleWidgetForBuilding(BuildingIndex) : nullptr;
}

EBubbleType UInGameLayerWidget::EvaluateBubbleTypeForBuilding(int32 BuildingIndex) const
{
	UWorld* World = GetWorld();
	if (!World) return EBubbleType::None;

	UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return EBubbleType::None;

	ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(BuildingIndex);
	if (!Building) return EBubbleType::None;

	ECompanyType CompanyType = Building->GetCompanyType();

	UCGGameInstance* GI = Cast<UCGGameInstance>(World->GetGameInstance());
	if (!GI) return EBubbleType::None;

	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OpMgr) return EBubbleType::None;

	// OfficeDataMap 은 함수 상단 1회 조회 — ReportPending/NoProject 판정 공유 (5초 폴링 비용 절약)
	const FOfficeSaveData* OfficeData = nullptr;
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			OfficeData = SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
		}
	}

	// VaultFull (우선순위 1) — 금고가 용량 이상으로 찬 경우
	if (CompanyType != ECompanyType::None)
	{
		const float StoredRevenue = OpMgr->GetStoredRevenue(BuildingIndex);
		const float Capacity = OpMgr->CalculateWarehouseCapacity(BuildingIndex);
		if (Capacity > 0.0f && StoredRevenue >= Capacity)
		{
			return EBubbleType::VaultFull;
		}
	}

	// ReportPending (우선순위 2) — 건물별 미확인 결산서 (GenerateReport 가 set, ConsumeReport 가 clear)
	if (OfficeData && OfficeData->bHasPendingReport)
	{
		return EBubbleType::ReportPending;
	}

	// NoProject(Zzz) 버블 폐기 (2026-07-23 사용자 결정) — 유휴는 창문 발광 off(어두움)로 구분.
	// 버블+불빛 이중 신호 제거. 행동 유도가 필요한 결산 대기(ReportPending)만 버블 유지.

	return EBubbleType::None;
}

void UInGameLayerWidget::OnOperationCompletedForBubble(int32 BuildingID, const FOperationData& Data)
{
	bBubbleReevalPending = true;
	RequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::OperationCompleted);

	UpdateBuildingWindowLight(BuildingID, false);
}

void UInGameLayerWidget::OnOperationStartedForBubble(int32 BuildingID)
{
	bBubbleReevalPending = true;
	RequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::OperationStarted);

	UpdateBuildingWindowLight(BuildingID, true);
}

void UInGameLayerWidget::UpdateBuildingWindowLight(int32 BuildingID, bool bActive)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	if (ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(BuildingID))
	{
		// 제조업은 운영 신호와 무관하게 상시 점등 — 아래 헬퍼가 단일 판정처
		UProjectOperationManager* OpMgr = World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr;
		Building->SetWindowLightActive(bActive || ShouldWindowLightBeOn(Building, OpMgr));
	}
}

bool UInGameLayerWidget::ShouldWindowLightBeOn(ABuildingBaseActor* Building, UProjectOperationManager* OpMgr)
{
	if (!Building)
	{
		return false;
	}

	// 창문 발광은 2026-07-23 부터 '유휴 표시'를 겸한다(Zzz 버블 폐기 → 어두움으로 구분).
	// 그런데 제조업(반도체/자동차/전자)은 운영(Operation) 페이즈 자체가 없어 영원히 꺼진 채 남는다 —
	// 정상 가동 중인 공장이 죽은 건물로 보이는 오독이라, 제조업은 상시 점등으로 뺀다.
	if (IsManufacturingType(Building->GetCompanyType()))
	{
		return true;
	}

	return OpMgr && OpMgr->HasActiveOperation(Building->GetBuildingIndex());
}

void UInGameLayerWidget::OnRevenueCollectedForBubble(int64 Amount)
{
	// 수익 수령 시 어떤 건물인지 모름 → 다음 틱 전체 재평가
	bBubbleReevalPending = true;
}

// ============================================================
// 버블 클릭 액션 핸들러
// ============================================================

void UInGameLayerWidget::HandleActiveBubbleMembershipChanged()
{
	bVaultGaugeReevalPending = true;
}

void UInGameLayerWidget::HandleBubbleAction(int32 BuildingIndex, EBubbleType Type)
{
	switch (Type)
	{
	case EBubbleType::VaultFull:
		ExecuteVaultCollection(BuildingIndex);
		break;
	case EBubbleType::ReportPending:
		// 결산 확인 지점 — MainMap 엔 결산 UI가 없어 관리패널 진입이 동선 (오피스 입장 시 OfficeMainWidget 이 pending 결산 모달 자동 표시)
		ExecuteBuildingInteraction(BuildingIndex);
		break;
	default:
		// NoProject 포함 — 부지 가격 배지는 BubbleContainer 가 아닌 전용 위젯(UPlotPriceBadgeWidget)이라 이 경로로 안 옴.
		ExecuteBuildingInteraction(BuildingIndex);
		break;
	}
}

void UInGameLayerWidget::ExecuteVaultCollection(int32 BuildingIndex)
{
	if (CurrentCoinFlyout) return;

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OpMgr) return;

	// 수금 전에 Money 위젯 억제 — 수금 시 HandleResourceChanged 가 카운터를 즉시 풀로 점프시키는 것 방지
	// (코인 도착마다 점진 상승시키려면 수금 순간의 갱신을 먹어야 함). 실패 경로에선 해제.
	bSuppressMoneyUpdate = true;

	// 수금 실행 (내부에서 StoreResource + 델리게이트 브로드캐스트)
	int64 CollectedAmount = OpMgr->CollectAllWarehouseByBuilding(BuildingIndex);
	if (CollectedAmount <= 0) { bSuppressMoneyUpdate = false; return; }

	// 수거 델타는 DT Money 아이콘/색의 전용 무음 FundsToast로 표시한다.
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ShowFundsToast(CollectedAmount);
	}

	PlayCoinFlyoutAnimation(CollectedAmount);

	// 버블 제거
	if (BubbleContainer)
	{
		BubbleContainer->DismissBubble(BuildingIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("[InGameLayerWidget] ExecuteVaultCollection - BuildingID=%d, Amount=%lld, Coins=%d"),
		BuildingIndex, CollectedAmount, CoinAnimTotalCoins);
}

void UInGameLayerWidget::PlayCoinFlyoutAnimation(int64 CollectedAmount)
{
	// 진행 중 플라이아웃이 있으면 그쪽 완료 콜백이 suppress 를 해제하므로 여기선 유지한 채 반환
	if (CurrentCoinFlyout) return;

	// 이하 불발 경로는 호출자가 켠 suppress 를 해제하고 반환 — 안 하면 Money 카운터 영구 동결(667 게이트)
	if (CollectedAmount <= 0) { bSuppressMoneyUpdate = false; return; }

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) { bSuppressMoneyUpdate = false; return; }

	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!RMgr) { bSuppressMoneyUpdate = false; return; }

	// OldMoney 역산 (이미 수금 완료된 상태)
	int64 CurrentMoney = RMgr->GetResourceAmount(EResourceType::Money);
	CoinAnimOldMoney = CurrentMoney - CollectedAmount;
	CoinAnimCollectedAmount = CollectedAmount;

	// 수거 총액 "+N" 플로팅 숫자 (개별/전체 수거 공통 경로 — 여기 1곳이 둘 다 커버)
	SpawnGainPopup(EResourceType::Money, CollectedAmount);

	// Money 위젯을 OldMoney로 되돌림 (시각적)
	bSuppressMoneyUpdate = true;
	if (UIE_Resource_Money)
	{
		UIE_Resource_Money->SetValue(CoinAnimOldMoney);
	}

	// 코인 수 계산: log10 스케일, 최소 3 ~ 최대 CoinFlyoutMaxIcons. 계수/상한은 UI_InGameLayer WBP 에서 PIE 튜닝 가능.
	// 기본(계수 12.5·상한 100): 1억원(10^8)에서 상한 도달 — 대량 수거일수록 코인 우수수(트레일러 대비).
	const float LogScaled = FMath::LogX(10.0f, static_cast<float>(FMath::Max<int64>(CollectedAmount, 10))) * CoinFlyoutScalePerDecade;
	CoinAnimTotalCoins = FMath::Clamp(FMath::RoundToInt(LogScaled), 3, FMath::Max(3, CoinFlyoutMaxIcons));
	CoinAnimArrivedCoins = 0;

	// 코인 텍스처 로드
	UTexture2D* CoinTexture = nullptr;
	if (TableMgr)
	{
		bool bSuccess = false;
		FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bSuccess);
		if (bSuccess && !ResInfo.Icon.IsNull())
		{
			CoinTexture = ResInfo.Icon.LoadSynchronous();
		}
	}

	// 코인 플라이아웃 위젯 생성 — 실패 시 suppress 해제 + 실보유액 복원(위에서 OldMoney 로 되돌려놨으므로)
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	CurrentCoinFlyout = PC ? CreateWidget<UCoinFlyoutContainerWidget>(PC) : nullptr;
	if (!CurrentCoinFlyout)
	{
		bSuppressMoneyUpdate = false;
		if (UIE_Resource_Money)
		{
			UIE_Resource_Money->SetValue(RMgr->GetResourceAmount(EResourceType::Money));
		}
		return;
	}

	CurrentCoinFlyout->AddToViewport(999);

	// 콜백 바인딩
	CurrentCoinFlyout->OnCoinArrived.BindUObject(this, &UInGameLayerWidget::OnCoinArrivedCallback);
	CurrentCoinFlyout->OnAllCoinsComplete.BindUObject(this, &UInGameLayerWidget::OnAllCoinsCompleteCallback);

	// Money 아이콘 위치 계산 → 타겟 위치
	FVector2D TargetPos = FVector2D::ZeroVector;
	if (UIE_Resource_Money)
	{
		UIE_Resource_Money->CalculateIconScreenPos();
		TargetPos = UIE_Resource_Money->GetCachedIconScreenPos();
	}

	// 분출 반경/버스트/도착편차 주입(넓은 구역 · 한번에 · 도착마다 롤업) — WBP 에서 런타임 튜닝
	CurrentCoinFlyout->SpawnRadius = CoinFlyoutSpawnRadius;
	CurrentCoinFlyout->StaggerDelay = CoinFlyoutStagger;
	CurrentCoinFlyout->ArrivalSpread = CoinFlyoutArrivalSpread;

	CurrentCoinFlyout->StartFlyout(TargetPos, CoinAnimTotalCoins, CoinTexture);
}

void UInGameLayerWidget::ExecuteAllVaultCollection()
{
	// 이미 플라이아웃 진행 중이면 중복 방지 (단일 버블 클릭과 동일 규칙)
	if (CurrentCoinFlyout) return;

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OpMgr) return;

	// 수금 전에 Money 위젯 억제 — 코인 도착마다 카운터가 점진 상승하도록(수금 즉시 풀 점프 방지). 실패 경로에선 해제.
	bSuppressMoneyUpdate = true;

	const int64 Total = OpMgr->CollectAllStoredRevenue();

	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();

	if (Total <= 0)
	{
		bSuppressMoneyUpdate = false;
		if (UIMgr)
		{
			// "없습니다" = 상태 경고 (Failed 는 "~부족합니다/~할 수 없습니다" 류 거부 전용)
			UIMgr->ShowNotification(
				NSLOCTEXT("Notification", "NoRevenueToCollect", "수거할 수익이 없습니다"),
				2.0f,
				ENotificationType::Warning);
		}
		return;
	}

	// 전체 수거도 단일 수거와 같은 전용 무음 FundsToast 경로를 사용한다.
	if (UIMgr)
	{
		UIMgr->ShowFundsToast(Total);
	}

	PlayCoinFlyoutAnimation(Total);

	// 개별 수거는 DismissBubble 로 특정 버블만 꺼주는데, 전체 수거는 Revenue 가 0 된 모든 건물 재평가
	// OpMgr 내부 OnRevenueCollected 브로드캐스트 → OnRevenueCollectedForBubble → EvaluateAllBuildingBubbles 자동 호출
	// 별도 Dismiss 불필요

	UE_LOG(LogTemp, Log, TEXT("[InGameLayerWidget] ExecuteAllVaultCollection - Total=%lld, Coins=%d"),
		Total, CoinAnimTotalCoins);
}

void UInGameLayerWidget::BeginMoneyDripFocus()
{
	bSuppressMoneyUpdate = true;
}

void UInGameLayerWidget::EndMoneyDripFocus()
{
	bSuppressMoneyUpdate = false;

	// 억제 동안 놓친 변동 흡수 — 실보유액으로 동기화
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			if (UIE_Resource_Money)
			{
				UIE_Resource_Money->SetValue(RMgr->GetResourceAmount(EResourceType::Money));
			}
		}
	}
}

void UInGameLayerWidget::PlayMoneyDripLanding(int64 Delta)
{
	if (Delta <= 0 || !UIE_Resource_Money) return;

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			UIE_Resource_Money->SetValueAnimated(RMgr->GetResourceAmount(EResourceType::Money));
		}
	}
	// 드립 착지 신호 — 절반 강도 펀치 (기본 0.08은 1초 케이던스에 과했음, 사용자 피드백 왕복으로 확정)
	UIE_Resource_Money->PlayBump(0.05f);
	SpawnGainPopup(EResourceType::Money, Delta);
}

void UInGameLayerWidget::ExecuteBuildingInteraction(int32 BuildingIndex)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(BuildingIndex);
	if (!Building) return;

	AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(World->GetFirstPlayerController());
	if (!MainPC || MainPC->GetCurrentInputMode() != EInputMode::Normal) return;

	Building->OpenBuildingUI(MainPC);
}

void UInGameLayerWidget::OnCoinArrivedCallback(int32 CoinIndex)
{
	CoinAnimArrivedCoins++;

	// 도착한 비율에 따라 Money 카운터 점진적으로 증가
	float Ratio = static_cast<float>(CoinAnimArrivedCoins) / static_cast<float>(CoinAnimTotalCoins);
	int64 DisplayMoney = CoinAnimOldMoney + FMath::RoundToInt64(CoinAnimCollectedAmount * Ratio);

	if (UIE_Resource_Money)
	{
		UIE_Resource_Money->SetValue(DisplayMoney);
	}
}

void UInGameLayerWidget::OnAllCoinsCompleteCallback()
{
	bSuppressMoneyUpdate = false;

	// 실제 현재 Money 값으로 복원
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (GI)
	{
		UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
		if (RMgr && UIE_Resource_Money)
		{
			UIE_Resource_Money->SetValue(RMgr->GetResourceAmount(EResourceType::Money));
		}
	}

	CurrentCoinFlyout = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[InGameLayerWidget] Coin flyout animation completed"));
}

void UInGameLayerWidget::HandleHQLevelUp(int32 NewLevel)
{
	if (PlayerLevelText)
	{
		PlayerLevelText->SetText(FText::FromString(
			FString::Printf(TEXT("%d"), NewLevel)));
	}

	// 레벨업 = 건설 상한 +N → 카운터 분모가 즉시 올라가야 보상이 체감된다
	RefreshBuildingCount();
}

void UInGameLayerWidget::RefreshBuildingCount()
{
	// 빌딩 추가/제거 시 새 빌딩의 버블 재평가 델리게이트도 함께 wire-up (idempotent)
	// 만액 집합도 함께 비운다 — 파괴된 인덱스가 남으면 재사용된 인덱스의 첫 전이를 삼킨다
	FullVaultBuildings.Reset();
	RequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::BuildingListChanged);
	RewireBuildingBubbleSubscriptions();
	EvaluateAllBuildingBubbles();

	if (!UIE_Resource_Building) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	// 현재 건물 수: EntityManager의 런타임 Buildings 배열
	int32 CurrentCount = 0;
	if (UWorld* World = GetWorld())
	{
		if (UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>())
		{
			CurrentCount = EntityMgr->GetBuildings().Num();
		}
	}

	// 건물 수 상한 폐지(2026-08-14) — 분모가 없으므로 보유 개수만 표시한다.
	UIE_Resource_Building->SetValue(CurrentCount);
}

void UInGameLayerWidget::UpdateProfileImage(int32 ImageID)
{
	if (!ProfileImage || !TableMgr) return;

	bool bSuccess = false;
	FProfileImageData ImgData = TableMgr->GetProfileImageData(ImageID, bSuccess);
	if (bSuccess && !ImgData.Icon.IsNull())
	{
		UTexture2D* Tex = ImgData.Icon.LoadSynchronous();
		if (Tex)
		{
			ProfileImage->SetBrushFromTexture(Tex);
		}
	}
}

void UInGameLayerWidget::OnProfileImageBtnClicked()
{
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> PanelClass = TableMgr->GetWidgetClass(EWidgetType::ProfileImagePanel);
	if (!PanelClass) return;

	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UIMgr->GetUIBase())
		{
			TSubclassOf<UCommonActivatableWidget> ActivatableClass(PanelClass);
			UIMgr->GetUIBase()->PushPromptClass(ActivatableClass);
		}
	}
}

