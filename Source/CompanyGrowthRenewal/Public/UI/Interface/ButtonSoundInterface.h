// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "ButtonSoundInterface.generated.h"

class USoundManagerSubsystem;

// UInterface 클래스 (언리얼 리플렉션용)
UINTERFACE(MinimalAPI, Blueprintable)
class UButtonSoundInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 버튼 사운드 인터페이스
 * - 상속만 하면 NativeOnClicked/Hovered 에서 자동 사운드 재생
 * - GetClickSoundTag/GetHoverSoundTag 오버라이드로 버튼별 커스터마이징
 * - UI.Sound.* 계층의 GameplayTag 키 사용 (재컴파일 없이 .ini 로 신규 등록 가능)
 */
class COMPANYGROWTHRENEWAL_API IButtonSoundInterface
{
	GENERATED_BODY()

public:
	// ===== 커스터마이징 가능한 함수들 (오버라이드 선택) =====

	/** 클릭 사운드 태그 반환 (기본: UI.Sound.Button.Click) */
	virtual FGameplayTag GetClickSoundTag() const;

	/** 호버 사운드 태그 반환 (기본: UI.Sound.Button.Hover) */
	virtual FGameplayTag GetHoverSoundTag() const;

	/** 클릭 사운드 재생 여부 (오버라이드하여 끌 수 있음) */
	virtual bool ShouldPlayClickSound() const
	{
		return true;
	}

	/** 호버 사운드 재생 여부 (오버라이드하여 끌 수 있음) */
	virtual bool ShouldPlayHoverSound() const
	{
		return true;
	}

	// ===== 실제 사운드 재생 함수 (기본 구현 제공, 보통 오버라이드 불필요) =====

	/** 클릭 사운드 재생 (NativeOnClicked에서 호출) */
	void PlayClickSound(const UObject* WorldContext);

	/** 호버 사운드 재생 (NativeOnHovered에서 호출) */
	void PlayHoverSound(const UObject* WorldContext);

private:
	/** 내부 헬퍼: SoundManager를 통한 사운드 재생 */
	void PlayButtonSound(const UObject* WorldContext, FGameplayTag SoundTag);
};
