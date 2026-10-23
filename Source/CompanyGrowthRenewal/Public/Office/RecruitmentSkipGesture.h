#pragma once

#include "CoreMinimal.h"
#include "Containers/Set.h"

// 채용 연출 중 같은 포인터의 짧은 두 탭만 한 번의 건너뛰기로 인정한다.
struct FRecruitmentSkipGesture
{
	void Reset()
	{
		bArmed = false;
		bConsumed = false;
		ClearCandidate();
		ActivePointers.Reset();
		EarliestTime = 0.0;
	}

	void Arm(double Now)
	{
		Reset();
		bArmed = true;
		EarliestTime = Now + GuardDuration;
	}

	void Begin(double Now, FVector2D Position, int32 PointerIndex)
	{
		const bool bAlreadyHeld = ActivePointers.Contains(PointerIndex);
		ActivePointers.Add(PointerIndex);
		if (!bArmed || bConsumed)
		{
			return;
		}
		if (Now < EarliestTime || !IsPrimaryPointer(PointerIndex) ||
			bAlreadyHeld || ActivePointers.Num() != 1 || bPressed)
		{
			ClearCandidate();
			return;
		}

		bPressed = true;
		PressTime = Now;
		PressPosition = Position;
		PressPointer = PointerIndex;
	}

	void Move(FVector2D Position, int32 PointerIndex)
	{
		if (bPressed && (PointerIndex != PressPointer || !IsWithinRadius(PressPosition, Position)))
		{
			ClearCandidate();
		}
	}

	bool End(double Now, FVector2D Position, int32 PointerIndex)
	{
		const bool bWasHeld = ActivePointers.Remove(PointerIndex) != 0;
		if (!bArmed || bConsumed)
		{
			return false;
		}
		if (!bWasHeld || ActivePointers.Num() != 0 || !bPressed ||
			Now < EarliestTime || PointerIndex != PressPointer ||
			Now < PressTime || Now - PressTime > TapDuration ||
			!IsWithinRadius(PressPosition, Position))
		{
			ClearCandidate();
			return false;
		}

		bPressed = false;
		if (bHasFirstTap && PointerIndex == FirstPointer &&
			Now >= FirstReleaseTime && Now - FirstReleaseTime <= DoubleTapDuration &&
			IsWithinRadius(FirstPosition, Position))
		{
			bConsumed = true;
			bHasFirstTap = false;
			return true;
		}

		// 멀거나 늦은 유효 탭은 다음 두 번 누르기의 첫 탭이 될 수 있다.
		bHasFirstTap = true;
		FirstReleaseTime = Now;
		FirstPosition = Position;
		FirstPointer = PointerIndex;
		return false;
	}

private:
	static constexpr double GuardDuration = 0.5;
	static constexpr double TapDuration = 0.35;
	static constexpr double DoubleTapDuration = 0.35;
	static constexpr float TapRadiusSquared = 32.f * 32.f;

	static bool IsPrimaryPointer(int32 PointerIndex)
	{
		return PointerIndex == INDEX_NONE || PointerIndex == 0;
	}

	static bool IsWithinRadius(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D::DistSquared(A, B) <= TapRadiusSquared;
	}

	void ClearCandidate()
	{
		bPressed = false;
		bHasFirstTap = false;
	}

	bool bArmed = false;
	bool bConsumed = false;
	bool bPressed = false;
	bool bHasFirstTap = false;
	double EarliestTime = 0.0;
	double PressTime = 0.0;
	double FirstReleaseTime = 0.0;
	FVector2D PressPosition = FVector2D::ZeroVector;
	FVector2D FirstPosition = FVector2D::ZeroVector;
	int32 PressPointer = INDEX_NONE;
	int32 FirstPointer = INDEX_NONE;
	TSet<int32> ActivePointers;
};
