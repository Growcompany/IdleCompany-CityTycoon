// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UUserWidget;

/**
 * 리스트 첫 등장 연출 — 순번만큼 늦게 시작하는 페이드 + 우측 슬라이드인.
 * 값은 URankingEntryCardWidget / UGoalTrackerRowWidget 이 각자 들고 있던 것과 동일하다(세 번째 소비자 규칙으로 추출).
 * 항목 위젯의 멤버로 두고 PlayIntro 에서 Play, NativeTick 에서 Tick 한다.
 */
struct COMPANYGROWTHRENEWAL_API FListIntroStagger
{
	static constexpr float Duration = 0.28f;
	static constexpr float SlideX = 60.f;
	static constexpr float Stagger = 0.06f;
	static constexpr float MaxDelay = 0.48f;   // 화면 밖 항목까지 끝없이 늦어지지 않게 캡

	/** 시작 상태(투명 + 우측 오프셋)를 즉시 적용한다 — 첫 틱 전 제자리 1프레임 깜빡임 방지. */
	void Play(UUserWidget& Widget, int32 OrderIndex);

	/** 재생 중이면 진행시키고 true. 끝나면 트랜스폼/알파를 원복한다(항목은 풀에서 재사용된다). */
	bool Tick(UUserWidget& Widget, float DeltaTime);

	bool IsPlaying() const { return bPlaying; }

private:
	float Elapsed = 0.f;
	float Delay = 0.f;
	bool bPlaying = false;
};
