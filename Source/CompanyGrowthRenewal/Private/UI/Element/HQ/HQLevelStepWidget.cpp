// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/HQ/HQLevelStepWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"

void UHQLevelStepWidget::SetStep(int32 Level, const FText& Unlock, bool bCurrent)
{
	if (LevelText) LevelText->SetText(FText::AsNumber(Level));
	// UMG 는 한글을 음절 단위로 줄바꿈한다 — 괄호 설명은 새 줄에서 시작해 단어가 갈라지지 않게 한다
	if (UnlockText) UnlockText->SetText(FText::FromString(Unlock.ToString().Replace(TEXT(" ("), TEXT("\n("))));

	if (CardLip)
	{
		// 저작 브러시가 이미 립 색을 들고 있어 BrushColor 로 곱하면 색이 두 번 들어간다 — 브러시 색 자체를 바꾼다
		FSlateBrush LipBrush = CardLip->Background;
		LipBrush.TintColor = FSlateColor(bCurrent ? CurrentLipColor : LipColor);
		CardLip->SetBrush(LipBrush);
	}
	if (CardFace)
	{
		// 면색은 WBP 저작값 그대로, 테두리 색만 현재 칸 강조로 바꾼다
		FSlateBrush FaceBrush = CardFace->Background;
		FaceBrush.OutlineSettings.Color = FSlateColor(bCurrent ? CurrentRimColor : RimColor);
		CardFace->SetBrush(FaceBrush);
	}
	if (LevelTile) LevelTile->SetBrushColor(bCurrent ? CurrentTileColor : TileColor);

	const FSlateColor Ink(bCurrent ? CurrentTileInk : TileInk);
	if (LevelPrefixText) LevelPrefixText->SetColorAndOpacity(Ink);
	if (LevelText) LevelText->SetColorAndOpacity(Ink);
}
