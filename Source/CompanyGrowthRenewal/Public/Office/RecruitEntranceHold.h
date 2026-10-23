#pragma once

#include "CoreMinimal.h"

// 채용 성공 뒤 모달 퇴장 연출이 끝날 때까지 입장(카메라·문·카드)을 미룬다. 허락이 없어도 시한 뒤 반드시 시작한다.
struct FRecruitEntranceHold
{
	static constexpr float Timeout = 1.5f;

	void Request() { bRequested = true; }
	void Release() { bReleased = true; }
	void Reset() { *this = FRecruitEntranceHold(); }
	bool IsHeld() const { return bHeld; }
	bool DidTimeOut() const { return bTimedOut; }

	// 커밋 직후 호출. true 면 지금 입장을 시작한다.
	bool OnCommitted()
	{
		if (!bRequested || bReleased)
		{
			bHeld = false;
			return true;
		}
		bHeld = true;
		Elapsed = 0.f;
		return false;
	}

	// 보류 중 매 Tick. true 면 이번 Tick 에 입장을 시작한다.
	bool Tick(float DeltaTime)
	{
		if (!bHeld) return false;
		Elapsed += DeltaTime;
		if (!bReleased && Elapsed < Timeout) return false;
		bTimedOut = !bReleased;
		bHeld = false;
		return true;
	}

private:
	bool bRequested = false;
	bool bReleased = false;
	bool bHeld = false;
	bool bTimedOut = false;
	float Elapsed = 0.f;
};
