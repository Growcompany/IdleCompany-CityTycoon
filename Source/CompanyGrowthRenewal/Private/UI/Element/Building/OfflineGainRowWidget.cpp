// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/OfflineGainRowWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/EntityManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BuildableCardTable.h"
#include "Table/CompanyInfoTable.h"
#include "Table/ResourceInfo.h"

namespace
{
	// 지출 레드 — InGameLayerWidget 손실 표기와 동일 정의 재사용(신규 색 발명 금지)
	const FLinearColor OfflineLossInk(1.0f, 0.35f, 0.35f, 1.0f);
}

void UOfflineGainRowWidget::SetRowData(const FOfflineGainEntry& Entry)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// 인덱스로 액터를 찾고, 액터의 FName BuildingID 로 카드 행 조회
	// (운영 매니저의 "BuildingID" 파라미터는 인덱스지만, 카드 테이블 키는 FName 이라 액터 경유가 필수)
	FBuildableCardTable CardData;
	bool bCardFound = false;
	// 업종은 건물 카드가 아니라 액터가 들고 있다 — FBuildableCardTable 은 산업 무관 cosmetic(카탈로그 §7)
	FCompanyInfoTable CompanyInfo;
	bool bCompanyFound = false;
	UWorld* WorldPtr = GetWorld();
	if (UEntityManager* EntityMgr = WorldPtr ? WorldPtr->GetSubsystem<UEntityManager>() : nullptr)
	{
		if (ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(Entry.BuildingIndex))
		{
			bCardFound = TableMgr && TableMgr->GetBuildableInfo(Building->GetBuildingID(), CardData);

			const ECompanyType Industry = Building->GetCompanyType();
			if (TableMgr && Industry != ECompanyType::None)
			{
				CompanyInfo = TableMgr->GetCompanyInfo(Industry, bCompanyFound);
			}
		}
	}

	if (BuildingNameText)
	{
		BuildingNameText->SetText(bCardFound
			? FText::FromName(CardData.Name)
			: FText::FromString(FString::Printf(TEXT("건물 %d"), Entry.BuildingIndex + 1)));
	}

	if (BuildingIcon && bCardFound && !CardData.UIIcon.IsNull())
	{
		if (UTexture2D* IconTex = CardData.UIIcon.LoadSynchronous())
		{
			BuildingIcon->SetBrushFromTexture(IconTex);
		}
	}

	// 업종 칩 — 데이터가 없으면 통째로 접는다(빈 아이콘/빈 글자가 행에 남지 않게)
	const ESlateVisibility IndustryVis =
		bCompanyFound ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

	if (IndustryGlyph)
	{
		IndustryGlyph->SetVisibility(IndustryVis);
		if (bCompanyFound)
		{
			if (!CompanyInfo.GlyphIcon.IsNull())
			{
				if (UTexture2D* GlyphTex = CompanyInfo.GlyphIcon.LoadSynchronous())
				{
					IndustryGlyph->SetBrushFromTexture(GlyphTex);
				}
			}
			IndustryGlyph->SetColorAndOpacity(CompanyInfo.AccentColor);
		}
	}

	if (IndustryText)
	{
		IndustryText->SetVisibility(IndustryVis);
		if (bCompanyFound)
		{
			IndustryText->SetText(CompanyInfo.DisplayName);
		}
	}

	if (GainText)
	{
		GainText->SetText(FText::FromString(FString::Printf(TEXT("+%s"),
			*UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(Entry.ActualGain),
				ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));

		// 재화 델타 색은 DT_Resource 가 SOT — 코드에 hex 를 굽지 않는다
		if (TableMgr)
		{
			bool bResOk = false;
			const FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bResOk);
			if (bResOk)
			{
				GainText->SetColorAndOpacity(FSlateColor(ResInfo.UIColor));
			}
		}
	}

	const bool bHasLoss = Entry.LossByVault > 0.0f;
	if (LossRow)
	{
		LossRow->SetVisibility(bHasLoss ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LossAmountText && bHasLoss)
	{
		// 손실은 Floor — 올림하면 과장이 된다
		LossAmountText->SetText(UGlobalUtilFunctions::AbbreviateNumber(
			static_cast<int64>(Entry.LossByVault), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
		LossAmountText->SetColorAndOpacity(FSlateColor(OfflineLossInk));
	}

	if (LifespanTag)
	{
		LifespanTag->SetVisibility(Entry.bLifespanBound ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
