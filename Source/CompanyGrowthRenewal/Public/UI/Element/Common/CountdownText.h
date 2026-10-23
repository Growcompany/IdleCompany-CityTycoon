// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UCommonTextBlock;

/**
 * "다음 출석까지 03:12:44" 같은 남은 시간 표시 — 1초 월드 타이머 + FGameClockRules::FormatCountdown.
 * 표시 전용이라 데이터 폴링 금지 규칙의 예외다(값은 시계가 소유하고 여기서는 렌더만 한다).
 * 소유자(위젯)의 멤버로 두고 활성/비활성 쌍에서 Start/Stop 한다 — 소유자보다 오래 살지 않는다.
 */
struct COMPANYGROWTHRENEWAL_API FCountdownTicker
{
	FCountdownTicker() = default;

	// 소유자가 Stop 을 빠뜨려도 타이머가 살아남지 않게 한다
	~FCountdownTicker() { Stop(); }

	// 타이머 핸들을 복사하면 같은 타이머를 두 곳이 소유하게 된다 — 소유자 멤버로만 쓸 것
	FCountdownTicker(const FCountdownTicker&) = delete;
	FCountdownTicker& operator=(const FCountdownTicker&) = delete;

	/** InSecondsProvider = 남은 초를 주는 게터. InPrefix 는 숫자 앞에 붙는 안내 문구. */
	void Start(UWorld* InWorld, UCommonTextBlock* InText, TFunction<int64()> InSecondsProvider,
		const FString& InPrefix = FString());

	void Stop();

	/** 타이머를 기다리지 않고 지금 한 번 그린다 (표시 전체를 새로 채울 때). */
	void Refresh() const;

private:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UCommonTextBlock> Text;
	TFunction<int64()> SecondsProvider;
	FString Prefix;
	FTimerHandle Handle;
};
