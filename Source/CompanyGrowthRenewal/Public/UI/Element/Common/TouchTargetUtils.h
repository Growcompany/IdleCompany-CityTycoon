// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class USizeBox;
class UWidget;

/**
 * 글리프 버튼(닫기 "X" · 도움말 "?") 2층 구조의 실치수 주입.
 * 표시 크기는 Glyph 로 고정하고 터치 영역만 Touch 로 넓힌다.
 */
namespace UITouchTarget
{
	/** SynchronizeProperties 에서 호출 — RootSizeBox/GlyphSize 가 없으면 아무것도 하지 않는다. */
	COMPANYGROWTHRENEWAL_API void ApplyGlyphAndTouchExtent(USizeBox* RootSizeBox, USizeBox* GlyphSize,
		UWidget* Button, float GlyphExtent, float TouchExtent);
}
