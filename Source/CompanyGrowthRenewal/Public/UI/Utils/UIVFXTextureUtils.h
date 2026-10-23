// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UImage;
class UTableManagerSubsystem;

/** DT_UIVFXTexture 소비 헬퍼 (FWidgetAnimationUtils 와 같은 정적 유틸 결). */
struct COMPANYGROWTHRENEWAL_API FUIVFXTextureUtils
{
	// 텍스처만 교체한다 — 저작한 틴트/ImageSize 가 살아 있어야 하는 자리(출석 이펙트 무대)가 쓴다
	static bool SetTextureKeepStyle(const UTableManagerSubsystem* TableMgr, FName Key, UImage* Image);
};
