// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/WorldMap/MapIndustryTagWidget.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CountryInfoTable.h"
#include "Table/CompanyInfoTable.h"
#include "Table/ResourceInfo.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "CommonTextBlock.h"

namespace MapIndustryTags
{
	bool IsAllOil(const TArray<EResourceType>& Resources)
	{
		if (Resources.Num() == 0)
		{
			return false;
		}
		for (const EResourceType Resource : Resources)
		{
			if (Resource != EResourceType::Oil)
			{
				return false;
			}
		}
		return true;
	}

	TArray<FMapIndustryTagSpec> BuildSpecs(const FCountryInfoTable& Row, bool bRefinery)
	{
		TArray<FMapIndustryTagSpec> Specs;

		if (Row.bSupportsFactory)
		{
			const ECompanyType Industry = UWorldMapPanelWidget::ResolveSpecialtyIndustry(Row);
			if (Industry != ECompanyType::None)
			{
				FMapIndustryTagSpec Spec;
				Spec.Kind = EMapIndustryTagKind::Factory;
				Spec.Industry = Industry;
				Specs.Add(Spec);
			}
		}

		if (Row.bSupportsMine)
		{
			TArray<EResourceType> Resources;
			for (const EResourceType Resource : Row.MinableResources)
			{
				if (Resource != EResourceType::None)
				{
					Resources.Add(Resource);
				}
			}
			if (Resources.Num() > 0)
			{
				FMapIndustryTagSpec Spec;
				Spec.Resources = Resources;
				if (IsAllOil(Resources))
				{
					Spec.Kind = EMapIndustryTagKind::Oil;
					Spec.bRefinery = bRefinery;
				}
				else
				{
					Spec.Kind = EMapIndustryTagKind::Mine;
					Spec.bMineWordOnly = Row.bSupportsFactory || Row.bIsTradeHub;
				}
				Specs.Add(Spec);
			}
		}

		if (Row.bIsTradeHub)
		{
			FMapIndustryTagSpec Spec;
			Spec.Kind = EMapIndustryTagKind::TradeHub;
			Specs.Add(Spec);
		}

		return Specs;
	}

	FText MineWord()
	{
		return NSLOCTEXT("WorldMap", "MapTagMine", "광산");
	}

	FText OilWord(bool bRefinery)
	{
		return bRefinery
			? NSLOCTEXT("WorldMap", "MapTagOilRefinery", "석유·정유")
			: NSLOCTEXT("WorldMap", "MapTagOil", "석유");
	}

	FText TradeHubWord()
	{
		return NSLOCTEXT("WorldMap", "MapTagTradeHub", "무역항");
	}

	FText JoinNames(const TArray<FText>& Names)
	{
		return FText::Join(NSLOCTEXT("WorldMap", "MapTagJoin", "·"), Names);
	}
}

void UMapIndustryTagWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if ((!IconRow || !LabelText) && !bMissingWarned)
	{
		bMissingWarned = true;
		UE_LOG(LogTemp, Warning, TEXT("[MapIndustryTag] IconRow/LabelText 가 WBP 트리에 없어 이름표 칸이 비어 보입니다."));
	}
}

void UMapIndustryTagWidget::ApplySpec(const FMapIndustryTagSpec& Spec, const UTableManagerSubsystem& TableMgr)
{
	TArray<UTexture2D*> Icons;
	FText Label;
	FLinearColor TextColor = FLinearColor::White;
	FLinearColor IconTint = FLinearColor::Transparent;

	auto LoadIcon = [&Icons](const TSoftObjectPtr<UTexture2D>& Soft)
	{
		if (!Soft.IsNull())
		{
			if (UTexture2D* Texture = Soft.LoadSynchronous())
			{
				Icons.Add(Texture);
			}
		}
	};

	switch (Spec.Kind)
	{
	case EMapIndustryTagKind::Factory:
	{
		bool bOk = false;
		const FCompanyInfoTable Info = TableMgr.GetCompanyInfo(Spec.Industry, bOk);
		if (!bOk)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MapIndustryTag] DT_CompanyInfo 에 %s 행이 없어 공장 칸을 비웁니다."),
				*UEnum::GetValueAsString(Spec.Industry));
			break;
		}
		LoadIcon(Info.GlyphIcon);
		Label = Info.DisplayName;
		TextColor = Info.AccentColor;
		// 산업 글리프는 흰 실루엣이라 글자와 같은 산업색으로 칠한다.
		IconTint = Info.AccentColor;
		break;
	}
	case EMapIndustryTagKind::Mine:
	case EMapIndustryTagKind::Oil:
	{
		TArray<FText> Names;
		for (const EResourceType Resource : Spec.Resources)
		{
			bool bOk = false;
			const FResourceInfo Info = TableMgr.GetResourceInfo(Resource, bOk);
			if (!bOk)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MapIndustryTag] DT_Resource 에 %s 행이 없습니다."),
					*UEnum::GetValueAsString(Resource));
				continue;
			}
			LoadIcon(Info.Icon);
			Names.Add(Info.DisplayName);
			// 유전 칸은 자원이 몇 개든 아이콘 하나면 된다.
			if (Spec.Kind == EMapIndustryTagKind::Oil)
			{
				break;
			}
		}
		if (Spec.Kind == EMapIndustryTagKind::Oil)
		{
			Label = MapIndustryTags::OilWord(Spec.bRefinery);
			TextColor = OilColor;
		}
		else
		{
			Label = Spec.bMineWordOnly ? MapIndustryTags::MineWord() : MapIndustryTags::JoinNames(Names);
			TextColor = MineColor;
		}
		break;
	}
	case EMapIndustryTagKind::TradeHub:
		if (HubIcon)
		{
			Icons.Add(HubIcon);
		}
		Label = MapIndustryTags::TradeHubWord();
		TextColor = HubColor;
		IconTint = HubColor;
		break;
	}

	SetTag(Icons, Label, TextColor, IconTint);
}

void UMapIndustryTagWidget::SetTag(const TArray<UTexture2D*>& Icons, const FText& Label,
	const FLinearColor& TextColor, const FLinearColor& IconTint)
{
	if (LabelText)
	{
		LabelText->SetText(Label);
		LabelText->SetColorAndOpacity(FSlateColor(TextColor));
		LabelText->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (!IconRow || !WidgetTree)
	{
		return;
	}
	IconRow->ClearChildren();
	const FVector2D IconExtent(IconSize, IconSize);
	for (int32 IconIndex = 0; IconIndex < Icons.Num(); ++IconIndex)
	{
		UTexture2D* Texture = Icons[IconIndex];
		if (!Texture)
		{
			continue;
		}
		UOverlay* IconStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* Halo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (!IconStack || !Halo || !Icon)
		{
			continue;
		}
		Halo->SetBrushFromTexture(Texture, false);
		Halo->SetDesiredSizeOverride(IconExtent);
		Halo->SetColorAndOpacity(IconHaloColor);
		Halo->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		Halo->SetRenderScale(FVector2D(IconHaloScale, IconHaloScale));
		Icon->SetBrushFromTexture(Texture, false);
		Icon->SetDesiredSizeOverride(IconExtent);
		Icon->SetColorAndOpacity(IconTint.A > KINDA_SMALL_NUMBER ? IconTint : FLinearColor::White);
		IconStack->AddChildToOverlay(Halo);
		IconStack->AddChildToOverlay(Icon);
		IconStack->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UHorizontalBoxSlot* IconSlot = IconRow->AddChildToHorizontalBox(IconStack))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(IconRow->GetChildrenCount() > 1 ? -IconOverlap : 0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
	IconRow->SetVisibility(IconRow->GetChildrenCount() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

FText UMapIndustryTagWidget::GetLabel() const
{
	return LabelText ? LabelText->GetText() : FText::GetEmpty();
}

int32 UMapIndustryTagWidget::GetIconCount() const
{
	return IconRow ? IconRow->GetChildrenCount() : 0;
}
