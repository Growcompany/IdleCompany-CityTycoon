#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

/**
 * 코치마크 말풍선의 꼬리 방향 = 대상이 툴팁의 어느 쪽에 있는가.
 * Up 이면 대상이 위에 있고 툴팁은 그 아래에 놓인다.
 */
enum class EGuideTooltipDir : uint8
{
	Up,
	Down,
	Left,
	Right,
};

/**
 * 클램프 전 원위치 — 방향별 배치 규칙의 단일 출처.
 * 배치와 "자리가 있는가" 판정이 이 한 벌을 공유해야 둘이 갈라지지 않는다.
 */
inline FVector2D ComputeGuideTooltipRawPosition(
	const FVector2D& AnchorCenter,
	const FVector2D& AnchorSize,
	const FVector2D& TipSize,
	EGuideTooltipDir Dir,
	float Gap)
{
	const FVector2D AnchorHalf = AnchorSize * 0.5f;

	// default: 를 쓰지 않는다 ― 방향이 늘면 -WarningsAsErrors 가 여기를 짚어주도록.
	FVector2D Pos = FVector2D::ZeroVector;
	switch (Dir)
	{
	case EGuideTooltipDir::Up:
		Pos = FVector2D(AnchorCenter.X - TipSize.X * 0.5f, AnchorCenter.Y + AnchorHalf.Y + Gap);
		break;
	case EGuideTooltipDir::Down:
		Pos = FVector2D(AnchorCenter.X - TipSize.X * 0.5f, AnchorCenter.Y - AnchorHalf.Y - Gap - TipSize.Y);
		break;
	case EGuideTooltipDir::Left:
		Pos = FVector2D(AnchorCenter.X + AnchorHalf.X + Gap, AnchorCenter.Y - TipSize.Y * 0.5f);
		break;
	case EGuideTooltipDir::Right:
		Pos = FVector2D(AnchorCenter.X - AnchorHalf.X - Gap - TipSize.X, AnchorCenter.Y - TipSize.Y * 0.5f);
		break;
	}
	return Pos;
}

/**
 * 대상 렉트 옆에 툴팁을 놓고 화면 안전 박스 안으로 클램프한다.
 *
 * ⚠ 아래꼬리(Down)는 툴팁 높이를 빼야 대상에 붙는다 — 높이를 모른 채 어림값으로 잡으면
 * 툴팁이 대상에서 떨어져 엉뚱한 곳에 뜬다. 호출자는 desired size 가 확정된 뒤(다음 틱) 부를 것.
 *
 * ⚠⚠ 이 클램프는 **앵커를 모른다.** 대상이 화면 가장자리에 있으면 툴팁이 안쪽으로 밀려
 * 자기가 가리키는 대상 위를 덮는다. 그래서 호출자는 먼저 ResolveGuideTooltipSide 로
 * 방향을 정하고 그 결과를 여기와 SetTail 양쪽에 넘겨야 한다.
 */
inline FVector2D ComputeGuideTooltipPosition(
	const FVector2D& AnchorCenter,
	const FVector2D& AnchorSize,
	const FVector2D& TipSize,
	EGuideTooltipDir Dir,
	const FVector2D& ScreenSize,
	float SafeMargin,
	float Gap)
{
	FVector2D Pos = ComputeGuideTooltipRawPosition(AnchorCenter, AnchorSize, TipSize, Dir, Gap);

	// 툴팁이 화면보다 크면 Max 가 SafeMargin 아래로 내려가므로 하한을 먼저 세운다(음수 좌표 방지)
	const FVector2D MaxPos = ScreenSize - FVector2D(SafeMargin, SafeMargin) - TipSize;
	Pos.X = FMath::Clamp(Pos.X, SafeMargin, FMath::Max(SafeMargin, MaxPos.X));
	Pos.Y = FMath::Clamp(Pos.Y, SafeMargin, FMath::Max(SafeMargin, MaxPos.Y));
	return Pos;
}

/** 반대편 — Up↔Down, Left↔Right. 뒤집어도 꼬리가 미끄러지는 축은 그대로다(가로짝/세로짝). */
inline EGuideTooltipDir GetOppositeGuideTooltipDir(EGuideTooltipDir Dir)
{
	switch (Dir)
	{
	case EGuideTooltipDir::Up:    return EGuideTooltipDir::Down;
	case EGuideTooltipDir::Down:  return EGuideTooltipDir::Up;
	case EGuideTooltipDir::Left:  return EGuideTooltipDir::Right;
	case EGuideTooltipDir::Right: return EGuideTooltipDir::Left;
	}
	return Dir;
}

/**
 * 이 방향에 툴팁이 클램프 없이 들어가는가.
 *
 * **배치 축만 본다.** 미끄러짐 축(Up/Down 의 X, Left/Right 의 Y)이 클램프되는 건 정상이고,
 * ComputeGuideTailOffset 이 꼬리를 대상 쪽으로 되돌려 보정한다. 보정 짝이 없는 건 배치 축뿐이고,
 * 그 축이 클램프되면 툴팁이 대상 위로 올라탄다.
 */
inline bool HasGuideTooltipRoom(
	EGuideTooltipDir Dir,
	const FVector2D& AnchorCenter,
	const FVector2D& AnchorSize,
	const FVector2D& TipSize,
	const FVector2D& ScreenSize,
	float SafeMargin,
	float Gap)
{
	const FVector2D Raw = ComputeGuideTooltipRawPosition(AnchorCenter, AnchorSize, TipSize, Dir, Gap);
	const bool bVertical = (Dir == EGuideTooltipDir::Up || Dir == EGuideTooltipDir::Down);

	const double Start = bVertical ? Raw.Y : Raw.X;
	const double Extent = bVertical ? TipSize.Y : TipSize.X;
	const double Limit = bVertical ? ScreenSize.Y : ScreenSize.X;
	return Start >= SafeMargin && (Start + Extent) <= (Limit - SafeMargin);
}

/**
 * 선호 방향에 자리가 없으면 반대편으로 뒤집는다.
 *
 * ⚠ 클램프는 앵커를 모른다 — 뒤집지 않으면 화면 가장자리 대상에서 툴팁이 자기 대상을 덮는다.
 * (좌단 건물 + Right = 툴팁이 왼쪽으로 못 가 SafeMargin 에 박히고 Bar 를 가로지른다.)
 * 결과 Dir 은 ComputeGuideTooltipPosition 과 SetTail 양쪽에 **같이** 넘길 것 —
 * ResolveGuideTailPlacement 의 모서리 앵커가 방향마다 다르다.
 */
inline EGuideTooltipDir ResolveGuideTooltipSide(
	EGuideTooltipDir Preferred,
	const FVector2D& AnchorCenter,
	const FVector2D& AnchorSize,
	const FVector2D& TipSize,
	const FVector2D& ScreenSize,
	float SafeMargin,
	float Gap)
{
	if (HasGuideTooltipRoom(Preferred, AnchorCenter, AnchorSize, TipSize, ScreenSize, SafeMargin, Gap))
	{
		return Preferred;
	}

	// 양쪽 다 없으면(툴팁이 화면보다 크거나 안전 박스가 좁으면) 선호를 유지한다 —
	// 어차피 클램프되므로, 여기서 뒤집으면 결과는 같으면서 방향만 화면마다 튄다.
	const EGuideTooltipDir Flipped = GetOppositeGuideTooltipDir(Preferred);
	return HasGuideTooltipRoom(Flipped, AnchorCenter, AnchorSize, TipSize, ScreenSize, SafeMargin, Gap)
		? Flipped : Preferred;
}

struct FGuideTooltipLayout
{
	FVector2D Position = FVector2D::ZeroVector;
	EGuideTooltipDir Direction = EGuideTooltipDir::Up;
	bool bAvoidsContent = false;
};

/** 말풍선이 피할 것 ― 덮으면 안 되는 것과, 덜 가리는 쪽을 고를 때만 보는 읽는 내용을 나눈다. */
struct FGuideTooltipAvoid
{
	// 대상 구멍·손 그림·닫기(X)·먼저 놓인 말풍선·출시 보상
	TArray<FSlateRect> Hard;
	// 패널 글자·목록·강화 행 ― 대상 옆 자리끼리 비교할 때만 쓴다
	TArray<FSlateRect> Soft;
	// 누를 수 있는 다른 버튼 ― 글자보다 무겁게(ControlWeight 배) 본다
	TArray<FSlateRect> Controls;
	static constexpr double ControlWeight = 3.;
};

/** 대상을 감싼 묶음(대상이 든 강화 행·목록)은 피할 내용이 아니다 ― 그 줄을 설명하는 말풍선이 그 위에 놓여도 된다. */
inline TArray<FSlateRect> ExcludeGuideTooltipAnchorOwners(const TArray<FSlateRect>& Rects, const FVector2D& AnchorCenter)
{
	TArray<FSlateRect> Result;
	Result.Reserve(Rects.Num());
	for (const FSlateRect& Rect : Rects)
	{
		if (!(AnchorCenter.X >= Rect.Left && AnchorCenter.X <= Rect.Right && AnchorCenter.Y >= Rect.Top && AnchorCenter.Y <= Rect.Bottom))
		{
			Result.Add(Rect);
		}
	}
	return Result;
}

inline double SumGuideTooltipOverlap(const FVector2D& Pos, const FVector2D& Size, const TArray<FSlateRect>& Rects, double Pad)
{
	double Sum = 0.;
	for (const FSlateRect& Rect : Rects)
	{
		const double Width = FMath::Max(0., FMath::Min(Pos.X + Size.X, double(Rect.Right) + Pad) - FMath::Max(Pos.X, double(Rect.Left) - Pad));
		const double Height = FMath::Max(0., FMath::Min(Pos.Y + Size.Y, double(Rect.Bottom) + Pad) - FMath::Max(Pos.Y, double(Rect.Top) - Pad));
		Sum += Width * Height;
	}
	return Sum;
}

/**
 * 대상 옆 네 방향과 각 방향의 미끄러짐 위치를 비교한다.
 * 대상에서 MaxRetreat 넘게 물러나지 않는다 ― 내용을 피하려고 멀어지면 무엇을 가리키는지 잃는다.
 * 비교 순서: 덮으면 안 되는 것 → 읽는 내용 → 물러난 거리 → 방향 순번 → 미끄러진 거리.
 */
inline FGuideTooltipLayout ResolveGuideTooltipLayoutAvoiding(
	EGuideTooltipDir Preferred, const FVector2D& AnchorCenter, const FVector2D& AnchorSize,
	const FVector2D& TipSize, const FVector2D& ScreenSize, float SafeMargin, float Gap,
	const FGuideTooltipAvoid& Avoid, float MaxRetreat = 48.f, float ContentGap = 12.f)
{
	const EGuideTooltipDir FallbackDir = ResolveGuideTooltipSide(
		Preferred, AnchorCenter, AnchorSize, TipSize, ScreenSize, SafeMargin, Gap);
	FGuideTooltipLayout Best{ComputeGuideTooltipPosition(
		AnchorCenter, AnchorSize, TipSize, FallbackDir, ScreenSize, SafeMargin, Gap), FallbackDir, false};
	const FVector2D MaxPos = ScreenSize - FVector2D(SafeMargin) - TipSize;
	if (MaxPos.X < SafeMargin || MaxPos.Y < SafeMargin)
	{
		return Best;
	}
	const bool bPreferredVertical = Preferred == EGuideTooltipDir::Up || Preferred == EGuideTooltipDir::Down;
	const EGuideTooltipDir Directions[] = {Preferred, GetOppositeGuideTooltipDir(Preferred),
		bPreferredVertical ? EGuideTooltipDir::Left : EGuideTooltipDir::Up,
		bPreferredVertical ? EGuideTooltipDir::Right : EGuideTooltipDir::Down};
	const TArray<FSlateRect> SoftNear = ExcludeGuideTooltipAnchorOwners(Avoid.Soft, AnchorCenter);
	const TArray<FSlateRect> ControlsNear = ExcludeGuideTooltipAnchorOwners(Avoid.Controls, AnchorCenter);
	constexpr int32 KeyCount = 5;
	double BestKey[KeyCount] = {};
	bool bFound = false;
	for (int32 DirectionIndex = 0; DirectionIndex < UE_ARRAY_COUNT(Directions); ++DirectionIndex)
	{
		const EGuideTooltipDir Dir = Directions[DirectionIndex];
		if (!HasGuideTooltipRoom(Dir, AnchorCenter, AnchorSize, TipSize, ScreenSize, SafeMargin, Gap))
		{
			continue;
		}
		const bool bVertical = Dir == EGuideTooltipDir::Up || Dir == EGuideTooltipDir::Down;
		const FVector2D Base = ComputeGuideTooltipPosition(
			AnchorCenter, AnchorSize, TipSize, Dir, ScreenSize, SafeMargin, Gap);
		// 툴팁은 대상의 반대편에 놓이므로 물러나는 방향도 대상에서 멀어지는 쪽이다
		FVector2D Away = FVector2D::ZeroVector;
		switch (Dir)
		{
		case EGuideTooltipDir::Up: Away.Y = 1.; break;
		case EGuideTooltipDir::Down: Away.Y = -1.; break;
		case EGuideTooltipDir::Left: Away.X = 1.; break;
		case EGuideTooltipDir::Right: Away.X = -1.; break;
		}
		for (const double Retreat : {0., MaxRetreat * 0.5, double(MaxRetreat)})
		{
			const FVector2D Row = Base + Away * Retreat;
			auto ConsiderPosition = [&](const FVector2D& Pos)
			{
				if (Pos.X < SafeMargin || Pos.X > MaxPos.X || Pos.Y < SafeMargin || Pos.Y > MaxPos.Y) return;
				// 꼬리가 가리키는 축을 대상에서 완전히 벗어난 위치까지 옮기지 않는다.
				const double Along = bVertical ? AnchorCenter.X : AnchorCenter.Y;
				const double Start = bVertical ? Pos.X : Pos.Y;
				const double Extent = bVertical ? TipSize.X : TipSize.Y;
				if (Along < Start || Along > Start + Extent) return;
				const double Key[KeyCount] = {
					SumGuideTooltipOverlap(Pos, TipSize, Avoid.Hard, 0.),
					SumGuideTooltipOverlap(Pos, TipSize, SoftNear, ContentGap)
						+ FGuideTooltipAvoid::ControlWeight * SumGuideTooltipOverlap(Pos, TipSize, ControlsNear, ContentGap),
					Retreat,
					double(DirectionIndex),
					FMath::Abs(bVertical ? Pos.X - Base.X : Pos.Y - Base.Y)};
				bool bBetter = !bFound;
				for (int32 KeyIndex = 0; !bBetter && KeyIndex < KeyCount; ++KeyIndex)
				{
					if (Key[KeyIndex] < BestKey[KeyIndex] - 0.5) bBetter = true;
					else if (Key[KeyIndex] > BestKey[KeyIndex] + 0.5) break;
				}
				if (bBetter)
				{
					Best = {Pos, Dir, Key[0] <= KINDA_SMALL_NUMBER && Key[1] <= KINDA_SMALL_NUMBER};
					FMemory::Memcpy(BestKey, Key, sizeof(BestKey));
					bFound = true;
				}
			};
			auto ConsiderSlide = [&](double Slide)
			{
				FVector2D Pos = Row;
				if (bVertical) Pos.X = FMath::Clamp(Slide, double(SafeMargin), MaxPos.X);
				else Pos.Y = FMath::Clamp(Slide, double(SafeMargin), MaxPos.Y);
				ConsiderPosition(Pos);
			};
			ConsiderPosition(Row);
			ConsiderSlide(SafeMargin);
			ConsiderSlide(bVertical ? MaxPos.X : MaxPos.Y);
			for (const TArray<FSlateRect>* Group : {&Avoid.Hard, &SoftNear, &ControlsNear})
			{
				for (const FSlateRect& Rect : *Group)
				{
					ConsiderSlide((bVertical ? Rect.Left - TipSize.X : Rect.Top - TipSize.Y) - ContentGap);
					ConsiderSlide((bVertical ? Rect.Right : Rect.Bottom) + ContentGap);
				}
			}
		}
	}
	return Best;
}

/** 읽는 내용만 넘기는 호출 ― 덮으면 안 되는 것이 따로 없을 때. */
inline FGuideTooltipLayout ResolveGuideTooltipLayout(
	EGuideTooltipDir Preferred, const FVector2D& AnchorCenter, const FVector2D& AnchorSize,
	const FVector2D& TipSize, const FVector2D& ScreenSize, float SafeMargin, float Gap,
	const TArray<FSlateRect>& AvoidRects, float ContentGap = 12.f)
{
	FGuideTooltipAvoid Avoid;
	Avoid.Soft = AvoidRects;
	return ResolveGuideTooltipLayoutAvoiding(Preferred, AnchorCenter, AnchorSize, TipSize, ScreenSize,
		SafeMargin, Gap, Avoid, 48.f, ContentGap);
}

/**
 * 툴팁 안에서 꼬리가 놓일 오프셋(축 방향 시작점).
 *
 * ⚠ 클램프와 한 쌍이다 — 위 함수가 툴팁을 밀어 넣은 만큼 꼬리를 대상 쪽으로 되돌리지 않으면
 * 말풍선이 엉뚱한 데를 가리킨다. 비율 3종(2304~3120)을 한 트리로 커버하는 핵심.
 * 형태는 VaultGauge 의 ComputeFillTipOffset 과 같은 계열이다.
 */
inline float ComputeGuideTailOffset(
	float AnchorAlong,
	float TipStart,
	float TipExtent,
	float TailSize,
	float CornerInset)
{
	const float Ideal = AnchorAlong - TipStart - TailSize * 0.5f;
	const float MaxOffset = FMath::Max(CornerInset, TipExtent - CornerInset - TailSize);
	return FMath::Clamp(Ideal, CornerInset, MaxOffset);
}

/** 꼬리를 툴팁의 어느 모서리에 어떤 각도로 붙일지 ― Dir 하나에서 전부 파생된다. */
struct FGuideTailPlacement
{
	float Angle = 0.0f;                             // 기준(0도) = 위를 가리키는 삼각형
	FVector2D AnchorPoint = FVector2D::ZeroVector;  // 점 앵커 (Min==Max)
	FVector2D Alignment = FVector2D::ZeroVector;
	bool bAlongX = true;                            // Offset 을 X 에 실을지 Y 에 실을지
};

/**
 * ⚠ 교차축을 0 으로 고정하면 안 된다 ― 툴팁은 대상의 반대편에 놓이므로(Down 이면 대상 위,
 * Right 면 대상 왼쪽) 꼬리는 그 반대 모서리에 붙어야 한다. 앵커와 얼라인먼트를 함께 옮겨야
 * 네 방향 모두 해당 모서리에 밀착한 채 축 방향으로만 미끄러진다.
 * ComputeGuideTailOffset 이 시작 모서리 기준 값이라 부호를 뒤집을 필요는 없다.
 */
inline FGuideTailPlacement ResolveGuideTailPlacement(EGuideTooltipDir Dir)
{
	// default: 를 쓰지 않는다 ― 방향이 늘면 -WarningsAsErrors 가 여기를 짚어주도록.
	switch (Dir)
	{
	case EGuideTooltipDir::Up:    return { 0.0f,   FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), true  };
	case EGuideTooltipDir::Down:  return { 180.0f, FVector2D(0.0, 1.0), FVector2D(0.0, 1.0), true  };
	case EGuideTooltipDir::Left:  return { 270.0f, FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), false };
	case EGuideTooltipDir::Right: return { 90.0f,  FVector2D(1.0, 0.0), FVector2D(1.0, 0.0), false };
	}
	return {};
}
