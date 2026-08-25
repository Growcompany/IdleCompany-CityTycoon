// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 위젯 애니메이션 유틸리티 (정적 함수)
 * UI 애니메이션에 사용되는 Easing 함수들을 제공
 */
struct COMPANYGROWTHRENEWAL_API FWidgetAnimationUtils
{
	// 기본 Easing - 점점 느려지는 감속 곡선
	static float EaseOutQuad(float T);

	// 오버슈트 효과 - 목표값을 살짝 넘었다가 돌아오는 효과 (스케일 펀치에 적합)
	static float EaseOutBack(float T);

	// 탄성 효과 - 스프링처럼 튀는 효과
	static float EaseOutElastic(float T);

	// 고무도장 강타 스케일 — 직원 가챠 "채용 확정"과 출시 판정 도장이 공유. T01 = 경과/지속(0~1).
	// 0~0.7 = 2.8→0.94 ease-in 낙하(찍힘), 0.7~1 = 0.94→1.0 복귀.
	static float StampInScale(float T01);
	// 도장 불투명도 — 초반 3배속으로 올라와 MaxOpacity 에서 멈춘다(잉크는 완전 불투명이 아님)
	static float StampInOpacity(float T01, float MaxOpacity = 0.94f);
	// 감쇠 셰이크 오프셋(px). [0,Duration) 밖이면 0. 호출자가 RenderTranslation 에 그대로 넣는다.
	static FVector2D ShakeOffset(float Elapsed, float Duration, float Amp);
};

/**
 * 숫자 카운트업 애니메이션 상태
 * 숫자가 FromValue에서 ToValue로 점진적으로 변하는 애니메이션
 */
struct COMPANYGROWTHRENEWAL_API FNumberCountUpAnimation
{
	// 애니메이션 파라미터
	float FromValue = 0.f;
	float ToValue = 0.f;
	float Duration = 0.3f;

	// 내부 상태
	float ElapsedTime = 0.f;
	bool bIsPlaying = false;

	// 애니메이션 시작
	void Start(float From, float To, float InDuration = 0.3f);

	// 매 프레임 업데이트, 반환값: 현재 표시할 값
	float Tick(float DeltaTime);

	// 즉시 완료 (현재 애니메이션을 건너뛰고 목표값으로 설정)
	void Finish();

	// 진행 중 여부 확인
	bool IsPlaying() const { return bIsPlaying; }

	// 현재 보간된 값 가져오기
	float GetCurrentValue() const;
};

/**
 * 스케일 펀치 애니메이션 상태
 * 위젯이 순간적으로 커졌다가 원래 크기로 돌아오는 효과
 */
struct COMPANYGROWTHRENEWAL_API FScalePunchAnimation
{
	// 애니메이션 파라미터
	float PunchScale = 1.2f;   // 최대 스케일 (1.0 = 원래 크기)
	float Duration = 0.2f;     // 전체 지속 시간

	// 내부 상태
	float ElapsedTime = 0.f;
	bool bIsPlaying = false;

	// 애니메이션 시작
	void Start(float InPunchScale = 1.2f, float InDuration = 0.2f);

	// 매 프레임 업데이트, 반환값: 현재 스케일 (1.0 -> PunchScale -> 1.0)
	float Tick(float DeltaTime);

	// 진행 중 여부 확인
	bool IsPlaying() const { return bIsPlaying; }

	// 현재 스케일 값 가져오기
	float GetCurrentScale() const;
};
