// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Effects/CatchRingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/SizeBoxSlot.h"
#include "Styling/SlateTypes.h"
#include "Rendering/DrawElements.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"

namespace
{
	// 위젯 로컬 크기 (정사각형, 중심 = 링 중심 = 직원 몸통 앵커). 박스 전체가 클릭 영역 — 직원 머리~다리를 덮게 넉넉히.
	constexpr float BoxSize = 220.f;

	// 링 기본 반경(펄스 0 위상) + 펄스 진폭. 작은 스틱맨/탑다운 오피스 카메라에서 읽히게 큼직하게.
	constexpr float BaseRadius = 68.f;
	constexpr float RadiusPulse = 16.f;     // scale 맥동 진폭
	constexpr float RingThickness = 9.f;
	constexpr int32 RingSegments = 40;       // 원 근사 다각형 변 수
	constexpr float PulsePeriod = 0.9f;      // 한 맥동 주기(초) — 긴급한 톤

	// 주황 -> 빨강 (linear). 펄스 위상에 따라 보간 + 알파 맥동.
	const FLinearColor ColorOrange(1.0f, 0.45f, 0.06f, 1.0f);
	const FLinearColor ColorRed(0.95f, 0.10f, 0.06f, 1.0f);

	// 캐치 성공 파열 잔상 — 짧고 굵게 터지고 사라진다. 색은 화이트 스냅(다크 오피스 배경 대비, 블루=기능색이라 배제).
	constexpr float BurstDuration = 0.14f;
	constexpr float BurstEndRadius = 130.f;
	constexpr float BurstEndThickness = 2.f;

	// N각형 폐곡선 폴리라인 (마지막 점 = 첫 점, 복사본 Add 로 dangling assert 회피)
	void DrawRing(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness)
	{
		if (Radius <= 1.f) return;

		TArray<FVector2D> Pts;
		Pts.Reserve(RingSegments + 1);
		for (int32 i = 0; i <= RingSegments; ++i)
		{
			const float Angle = (2.f * PI) * (static_cast<float>(i) / static_cast<float>(RingSegments));
			Pts.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}

		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), Pts,
			ESlateDrawEffect::None, Color, true, Thickness);
	}
}

TSharedRef<SWidget> UCatchRingWidget::RebuildWidget()
{
	// 무인 자가 트리 — 페인트 영역 확보용 고정 크기 루트 + 전체 영역 투명 클릭 버튼.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CatchRingRoot"));
		Root->SetWidthOverride(BoxSize);
		Root->SetHeightOverride(BoxSize);

		HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CatchRingHitButton"));

		// 버튼 배경/테두리를 완전 투명으로 — 비주얼은 NativePaint 링이 담당, 버튼은 히트 영역만.
		// 투명 Box 브러시(NoDrawType 아님) — geometry 기반 히트테스트는 유지하면서 시각만 0.
		FButtonStyle TransparentStyle;
		FSlateBrush ClearBrush;
		ClearBrush.DrawAs = ESlateBrushDrawType::Box;
		ClearBrush.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		TransparentStyle.SetNormal(ClearBrush);
		TransparentStyle.SetHovered(ClearBrush);
		TransparentStyle.SetPressed(ClearBrush);
		TransparentStyle.SetDisabled(ClearBrush);
		HitButton->SetStyle(TransparentStyle);

		if (USizeBoxSlot* RootSlot = Cast<USizeBoxSlot>(Root->AddChild(HitButton)))
		{
			RootSlot->SetHorizontalAlignment(HAlign_Fill);
			RootSlot->SetVerticalAlignment(VAlign_Fill);
		}

		WidgetTree->RootWidget = Root;
	}

	return Super::RebuildWidget();
}

void UCatchRingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HitButton)
	{
		HitButton->OnClicked.AddDynamic(this, &UCatchRingWidget::HandleRingClicked);
	}
}

void UCatchRingWidget::NativeDestruct()
{
	if (HitButton)
	{
		HitButton->OnClicked.RemoveDynamic(this, &UCatchRingWidget::HandleRingClicked);
	}
	Super::NativeDestruct();
}

void UCatchRingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// NativePaint 는 const — 상태 갱신은 여기서만. 파열 중엔 펄스 위상을 멈춰 잔상이 맥동에 흔들리지 않게 한다.
	if (bBursting)
	{
		BurstElapsed += InDeltaTime;
	}
	else
	{
		PulseElapsed += InDeltaTime;
	}
}

bool UCatchRingWidget::IsBurstActive() const
{
	return bBursting && BurstElapsed < BurstDuration;
}

void UCatchRingWidget::SetWorker(AOfficeworker* InWorker)
{
	const bool bAnchorChanged = (Worker.Get() != InWorker);
	Worker = InWorker;

	// 파열이 끝난 링의 재사용(같은 직원이 다시 늘어지는 경우 포함) → 펄스 상태로 복귀.
	// OfficeLayer 는 파열 진행 중인 링에 SetWorker 를 호출하지 않으므로 진행 중 리셋은 발생하지 않는다.
	if (bAnchorChanged || !IsBurstActive())
	{
		bBursting = false;
		BurstElapsed = 0.0f;
	}

	if (bAnchorChanged)
	{
		PulseElapsed = 0.0f;
	}
}

void UCatchRingWidget::HandleRingClicked()
{
	// 이중 탭 가드 — HitButton 을 접으면 슬레이트 히트테스트가 프레임 경계에서 튀므로 플래그로만 막는다.
	if (bBursting) return;

	// "확 깨움" — 앵커 직원의 피로 차감 + 즉시 업무 복귀. 링은 파열 잔상이 끝난 뒤 OfficeLayer 가 회수.
	EFatigueSlackPhase CaughtPhase = EFatigueSlackPhase::None;
	if (AOfficeworker* W = Worker.Get())
	{
		if (W->BehaviorComponent)
		{
			// ApplyTapRelief 가 ExitSlackToWork 로 페이즈를 None 으로 돌리므로 먼저 읽는다
			CaughtPhase = W->BehaviorComponent->GetFatigueSlackPhase();
			W->BehaviorComponent->ApplyTapRelief();
		}
	}

	bBursting = true;
	BurstElapsed = 0.0f;

	OnCaught.Broadcast(CaughtPhase);
}

int32 UCatchRingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 DrawLayer = MaxLayer + 1;

	const FVector2D Center(BoxSize * 0.5f, BoxSize * 0.5f);

	if (bBursting)
	{
		// EaseOutQuad — 클릭 프레임에 확 튀어나가고 끝에서 잦아든다.
		const float T = FMath::Clamp(BurstElapsed / BurstDuration, 0.f, 1.f);
		const float Ease = 1.f - (1.f - T) * (1.f - T);

		const float BurstRadius = FMath::Lerp(BaseRadius, BurstEndRadius, Ease);
		const float BurstThickness = FMath::Lerp(RingThickness, BurstEndThickness, Ease);
		const FLinearColor BurstColor(1.f, 1.f, 1.f, 1.f - Ease);

		DrawRing(OutDrawElements, DrawLayer, AllottedGeometry, Center, BurstRadius, BurstColor, BurstThickness);
		return DrawLayer;
	}

	// 0~1 삼각파 맥동 (수축->팽창 반복) — sin 기반으로 부드럽게
	const float Phase = FMath::Fmod(PulseElapsed, PulsePeriod) / PulsePeriod;
	const float Wave = 0.5f * (1.f - FMath::Cos(Phase * 2.f * PI));   // 0->1->0

	// 메인 링: 팽창할수록 빨강 + 약간 옅어짐(긴급 점멸감)
	const float MainRadius = BaseRadius + RadiusPulse * Wave;
	const FLinearColor MainColor = FMath::Lerp(ColorOrange, ColorRed, Wave) * FLinearColor(1.f, 1.f, 1.f, FMath::Lerp(1.0f, 0.55f, Wave));
	DrawRing(OutDrawElements, DrawLayer, AllottedGeometry, Center, MainRadius, MainColor, RingThickness);

	// 바깥 잔상 링: 메인보다 더 팽창 + 더 옅게 (쇼크웨이브 잔향)
	const float EchoRadius = BaseRadius + RadiusPulse * (1.f + Wave);
	const FLinearColor EchoColor = ColorRed * FLinearColor(1.f, 1.f, 1.f, 0.30f * (1.f - Wave));
	DrawRing(OutDrawElements, DrawLayer, AllottedGeometry, Center, EchoRadius, EchoColor, RingThickness * 0.7f);

	return DrawLayer;
}
