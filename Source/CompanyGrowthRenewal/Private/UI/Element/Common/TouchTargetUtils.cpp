// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/TouchTargetUtils.h"

#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Widget.h"

void UITouchTarget::ApplyGlyphAndTouchExtent(USizeBox* RootSizeBox, USizeBox* GlyphSize,
	UWidget* Button, float GlyphExtent, float TouchExtent)
{
	if (!RootSizeBox || !GlyphSize)
	{
		return;
	}

	const float Glyph = GlyphExtent;
	const float Touch = FMath::Max(TouchExtent, Glyph);

	RootSizeBox->SetWidthOverride(Glyph);
	RootSizeBox->SetHeightOverride(Glyph);
	GlyphSize->SetWidthOverride(Glyph);
	GlyphSize->SetHeightOverride(Glyph);

	// 음수 패딩은 desired 에서 빠지고 배치 때 사방으로 더해진다 ― 부모는 Glyph 만 보고 히트만 Touch 로 넓어진다.
	// Fill 이 아니면 AlignChild(LayoutUtils.h:126) 가 클램프 경로를 타서 히트가 조용히 Glyph 로 돌아간다
	const FMargin Inset(-(Touch - Glyph) * 0.5f);
	UPanelSlot* Slot = Button ? Button->Slot : nullptr;
	if (UOverlaySlot* BtnSlot = Cast<UOverlaySlot>(Slot))
	{
		BtnSlot->SetPadding(Inset);
		BtnSlot->SetHorizontalAlignment(HAlign_Fill);
		BtnSlot->SetVerticalAlignment(VAlign_Fill);
	}
	else if (USizeBoxSlot* ButtonSizeSlot = Cast<USizeBoxSlot>(Slot))
	{
		ButtonSizeSlot->SetPadding(Inset);
		ButtonSizeSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSizeSlot->SetVerticalAlignment(VAlign_Fill);
	}
}
