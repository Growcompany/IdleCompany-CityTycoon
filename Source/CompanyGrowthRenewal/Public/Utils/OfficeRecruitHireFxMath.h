#pragma once

#include "CoreMinimal.h"
#include "Slate/WidgetTransform.h"

// 채용 버튼 피드백과 책상 위 사원증 배치 계산.
namespace OfficeRecruitHireFx
{
	constexpr float CardW = 560.f;
	constexpr float CardH = 773.f;
	constexpr float DimTarget = 0.62f;
	constexpr float DimInDuration = 0.20f;
	constexpr float DimOutDuration = 0.16f;
	constexpr float CalmFadeDuration = 0.10f;
	constexpr float PressDownTime = 0.06f;
	constexpr float PressUpTime = 0.12f;
	constexpr float PressScale = 0.94f;
	constexpr float ShakeDuration = 0.34f;

	struct FCardPose
	{
		FVector2D Center = FVector2D::ZeroVector;
		float Scale = 0.f;
	};

	inline float EaseOutCubic(float T) { const float U = 1.f - FMath::Clamp(T, 0.f, 1.f); return 1.f - U * U * U; }
	// 화면 좌표의 책상 위에 원본 사원증을 놓는다. X/Y canvas 배율이 달라도 화면 안쪽 여백을 유지한다.
	inline FCardPose SeatCardPose(const FVector2D& ViewportSize, const FVector2D& SeatPoint, bool bProjected,
		const FVector2D& CanvasPerViewport = FVector2D(1.f, 1.f))
	{
		const float Width = FMath::IsFinite(ViewportSize.X) ? FMath::Max(1.f, static_cast<float>(ViewportSize.X)) : 1.f;
		const float Height = FMath::IsFinite(ViewportSize.Y) ? FMath::Max(1.f, static_cast<float>(ViewportSize.Y)) : 1.f;
		const float CanvasX = FMath::IsFinite(CanvasPerViewport.X) && CanvasPerViewport.X > 0.f ? static_cast<float>(CanvasPerViewport.X) : 1.f;
		const float CanvasY = FMath::IsFinite(CanvasPerViewport.Y) && CanvasPerViewport.Y > 0.f ? static_cast<float>(CanvasPerViewport.Y) : 1.f;
		const float Margin = FMath::Min(FMath::Max(12.f, Height * 0.02f), FMath::Min(Width, Height) * 0.5f);
		const float AvailableWidth = FMath::Max(0.f, Width - 2.f * Margin);
		const float AvailableHeight = FMath::Max(0.f, Height - 2.f * Margin);
		FCardPose Pose;
		Pose.Scale = FMath::Min(FMath::Min(Width * 0.20f, AvailableWidth) * CanvasX / CardW,
			FMath::Min(Height * 0.28f, AvailableHeight) * CanvasY / CardH);
		const float HalfW = CardW * Pose.Scale / CanvasX * 0.5f;
		const float HalfH = CardH * Pose.Scale / CanvasY * 0.5f;
		const float SeatX = bProjected && FMath::IsFinite(SeatPoint.X) ? static_cast<float>(SeatPoint.X) : Width * 0.5f;
		const float SeatY = bProjected && FMath::IsFinite(SeatPoint.Y) ? static_cast<float>(SeatPoint.Y) : Height * 0.5f;
		Pose.Center.X = FMath::Clamp(SeatX, Margin + HalfW, Width - Margin - HalfW);
		Pose.Center.Y = bProjected && FMath::IsFinite(SeatPoint.X) && FMath::IsFinite(SeatPoint.Y)
			? FMath::Clamp(SeatY - 8.f - HalfH, Margin + HalfH, Height - Margin - HalfH)
			: Height * 0.5f;
		return Pose;
	}

	// 다른 창에 밀렸다 돌아와 요청 끝 알림을 놓친 대기 상태를 풀어도 되는가.
	inline bool ShouldReleaseStaleRequest(bool bPending, bool bEffectIdle, bool bFlowBusy) { return bPending && bEffectIdle && !bFlowBusy; }

	enum class EHireBlock : uint8 { None, NoTickets, NoDiamonds };

	// 단건 버튼이 막힌 이유. 빈 좌석 0 은 버튼을 열어 두고 누를 때 토스트로 안내하므로 문구를 띄우지 않는다.
	inline EHireBlock SingleHireBlock(int32 Tickets, int32 FreeSeats, bool bDiamondTier, int64 Diamonds, int32 DiamondCost)
	{
		if (FreeSeats < 1 || Tickets > 0) return EHireBlock::None;
		if (!bDiamondTier) return EHireBlock::NoTickets;
		return Diamonds < DiamondCost ? EHireBlock::NoDiamonds : EHireBlock::None;
	}

	inline bool BatchNeedsTickets(int32 Tickets) { return Tickets < 2; }

	inline float PressScaleAt(float Time)
	{
		if (Time <= 0.f) return 1.f;
		if (Time < PressDownTime) return FMath::Lerp(1.f, PressScale, EaseOutCubic(Time / PressDownTime));
		const float Up = (Time - PressDownTime) / PressUpTime;
		return Up >= 1.f ? 1.f : FMath::Lerp(PressScale, 1.f, EaseOutCubic(Up));
	}

	inline float ShakeOffsetAt(float Time)
	{
		static constexpr float Keys[] = { 0.f, -12.f, 11.f, -7.f, 6.f, -2.f, 0.f };
		constexpr int32 Segments = static_cast<int32>(UE_ARRAY_COUNT(Keys)) - 1;
		if (Time <= 0.f || Time >= ShakeDuration) return 0.f;
		const float Position = Time / ShakeDuration * Segments;
		const int32 Segment = FMath::Min(FMath::FloorToInt32(Position), Segments - 1);
		return FMath::Lerp(Keys[Segment], Keys[Segment + 1], Position - Segment);
	}

	inline float DimOpacityAt(float Time) { return DimTarget * EaseOutCubic(Time / DimInDuration); }

	// 빈 좌석 = min(빈 책상, 남은 수용 인원). 0일 때 어느 쪽이 막는지 알려야 눌러도 무반응이 안 된다.
	enum class ESeatBlock : uint8 { None, Capacity, Desks };

	inline ESeatBlock SeatBlock(int32 FreeDesks, int32 FreeCapacity)
	{
		if (FreeCapacity < 1) return ESeatBlock::Capacity;
		return FreeDesks < 1 ? ESeatBlock::Desks : ESeatBlock::None;
	}

	// 빈 자리 0 안내는 쉬는 창에서만 — 채용이 성공하면 자리가 0 이 되므로 요청·연출 중에는 띄우지 않는다.
	inline bool ShouldShowSeatGuide(ESeatBlock Block, bool bRequestPending, bool bFxIdle)
	{
		return Block != ESeatBlock::None && !bRequestPending && bFxIdle;
	}

	// 좌석 배정 완료 카드들을 순서대로 공개한다. SincePreviousReveal 음수는 앞 카드 미공개.
	constexpr float CardRevealGap = 0.30f;
	constexpr float SeatCardReadDuration = 3.f;
	constexpr float SeatCardFadeDuration = 0.30f;
	inline bool CanRevealCard(bool bRequested, bool bFirst, float SincePreviousReveal)
	{
		return bRequested && (bFirst || SincePreviousReveal >= CardRevealGap);
	}
	// 공개 직후에는 온전히 읽히고, 읽기 시간이 지난 뒤에만 자연스럽게 사라진다.
	inline float SeatCardOpacity(float Age)
	{
		if (Age >= SeatCardReadDuration + SeatCardFadeDuration) return 0.f;
		return Age <= SeatCardReadDuration ? 1.f
			: 1.f - FMath::Clamp((Age - SeatCardReadDuration) / SeatCardFadeDuration, 0.f, 1.f);
	}
	// 실제 카드는 불투명한 채 0.94배에서 자리 잡는다.
	constexpr float RevealStartScale = 0.94f;
	inline float RevealScale(float RevealAlpha) { return FMath::Lerp(RevealStartScale, 1.f, EaseOutCubic(RevealAlpha)); }

	// UE 5.4 FWidgetTransform applies scale, then angle-based shear; keep its transformed corners inside SeatCardPose.
	inline FWidgetTransform SeatCardRockAt(float Age, bool bReduceMotion)
	{
		if (bReduceMotion || Age <= 0.f || Age >= SeatCardReadDuration + SeatCardFadeDuration)
		{
			return FWidgetTransform();
		}
		const float FadeOut = FMath::Clamp((SeatCardReadDuration + SeatCardFadeDuration - Age) / SeatCardFadeDuration, 0.f, 1.f);
		const float Envelope = EaseOutCubic(Age / 0.20f) * FadeOut * FadeOut * (3.f - 2.f * FadeOut);
		const float Yaw = 22.f * FMath::Sin(2.f * PI * Age / 2.4f) * Envelope;
		const float ShearAngle = 2.5f * Yaw / 22.f;
		const float WidthScale = FMath::Cos(FMath::DegreesToRadians(Yaw));
		const float ShearSlope = FMath::Tan(FMath::DegreesToRadians(ShearAngle));
		const float WidthExtent = WidthScale * CardW + FMath::Abs(ShearSlope) * CardH;
		const float BoundsScale = FMath::Min(1.f, CardW / WidthExtent);
		return FWidgetTransform(FVector2D::ZeroVector, FVector2D(WidthScale, 1.f) * BoundsScale,
			FVector2D(ShearAngle, 0.f), 0.f);
	}
}
