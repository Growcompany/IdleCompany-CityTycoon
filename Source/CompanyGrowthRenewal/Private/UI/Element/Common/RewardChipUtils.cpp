#include "UI/Element/Common/RewardChipUtils.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

#include "Global/GlobalUtilFunctions.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"

namespace
{
	// 보상 수치 텍스트 톤 (= 트래커 폴백 RewardText: NEXON Regular 15 / OnCream_Sub).
	// 아이콘 모드와 폴백이 같은 폰트/색을 쓰도록 한 곳에 모음. 폰트 로드 실패 시 SetFont 생략(폴백).
	void StyleRewardValueText(UCommonTextBlock* Text)
	{
		if (!Text)
		{
			return;
		}

		if (UFont* NexonRegular = TSoftObjectPtr<UFont>(FSoftObjectPath(
				TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous())
		{
			FSlateFontInfo Font(NexonRegular, 15);
			Font.TypefaceFontName = TEXT("Default");
			Text->SetFont(Font);
		}
		// 보상 수치 색 — 기존 텍스트 톤 (0.75,0.75,0.75,0.9)
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 0.9f)));
	}
}

namespace CGRewardChip
{
	TArray<FDisplayEntry> BuildDisplayEntries(const TArray<FMissionReward>& Rewards)
	{
		TArray<FDisplayEntry> Entries;
		Entries.Reserve(Rewards.Num());

		for (const FMissionReward& Reward : Rewards)
		{
			if (Reward.ResourceType != EResourceType::None && Reward.Amount > 0)
			{
				FDisplayEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.ResourceType = Reward.ResourceType;
				Entry.Quantity = Reward.Amount;
			}

			if (Reward.ItemType != EItemType::None && Reward.ItemAmount > 0)
			{
				FDisplayEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.ItemType = Reward.ItemType;
				Entry.Quantity = Reward.ItemAmount;
			}
		}

		return Entries;
	}

	bool RenderIcons(UWidgetTree& InWidgetTree, UGameInstance* GameInstance,
		UHorizontalBox& InRewardBox, const TArray<FMissionReward>& Rewards)
	{
		// 디자인 타임 샘플(WBP RewardSampleIcon/Value)의 크기·폰트를 읽어 런타임 생성 위젯에 적용 — 에디터에서 설정한 크기 존중.
		// (ClearChildren 으로 샘플을 지우기 전에 먼저 측정)
		FVector2D SampleIconSize(24.f, 24.f);
		FSlateFontInfo SampleValueFont;
		bool bHasSampleFont = false;
		for (int32 i = 0; i < InRewardBox.GetChildrenCount(); ++i)
		{
			UWidget* Child = InRewardBox.GetChildAt(i);
			if (const UImage* Img = Cast<UImage>(Child))
			{
				const FVector2D S = Img->GetBrush().GetImageSize();
				if (S.X > 0.f && S.Y > 0.f)
				{
					SampleIconSize = S;
				}
			}
			else if (const UTextBlock* Txt = Cast<UTextBlock>(Child))
			{
				SampleValueFont = Txt->GetFont();
				bHasSampleFont = true;
			}
		}

		InRewardBox.ClearChildren();

		const TArray<FDisplayEntry> Entries = BuildDisplayEntries(Rewards);
		if (Entries.Num() == 0)
		{
			return false;
		}

		UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;

		bool bAny = false;

		for (const FDisplayEntry& Entry : Entries)
		{
			// 아이콘 — Icon 소프트 참조가 있고 로드되면 표시. 없거나 실패하면 그 항목은 수치만(아이콘이 정체성이라 표시명 생략).
			UTexture2D* IconTex = nullptr;
			if (TableMgr)
			{
				if (Entry.ResourceType != EResourceType::None)
				{
					bool bFound = false;
					const FResourceInfo Info = TableMgr->GetResourceInfo(Entry.ResourceType, bFound);
					if (bFound && !Info.Icon.IsNull())
					{
						IconTex = Info.Icon.LoadSynchronous();
					}
				}
				else if (Entry.ItemType != EItemType::None)
				{
					bool bFound = false;
					IconTex = TableMgr->GetItemIcon(Entry.ItemType, bFound);
				}
			}

			if (IconTex)
			{
				UImage* IconImage = InWidgetTree.ConstructWidget<UImage>(UImage::StaticClass());
				FSlateBrush IconBrush;
				IconBrush.SetResourceObject(IconTex);
				IconBrush.ImageSize = SampleIconSize;
				IconImage->SetBrush(IconBrush);

				if (UHorizontalBoxSlot* IconSlot = InRewardBox.AddChildToHorizontalBox(IconImage))
				{
					IconSlot->SetVerticalAlignment(VAlign_Center);
					// 페어 사이 12px(앞에 항목이 있을 때만), 아이콘-수치 사이 4px
					IconSlot->SetPadding(FMargin(bAny ? 12.f : 0.f, 0.f, 4.f, 0.f));
				}
			}

			UCommonTextBlock* ValueText = InWidgetTree.ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
			StyleRewardValueText(ValueText);
			ValueText->SetText(Entry.ItemType != EItemType::None
				? FText::Format(NSLOCTEXT("Reward", "Qty", "x{0}"), FText::AsNumber(Entry.Quantity))
				: UGlobalUtilFunctions::AbbreviateNumber(Entry.Quantity));
			// WBP 샘플 값 텍스트의 폰트(크기 포함)가 있으면 존중 — 에디터에서 키운 폰트 반영
			if (bHasSampleFont)
			{
				ValueText->SetFont(SampleValueFont);
			}

			if (UHorizontalBoxSlot* ValueSlot = InRewardBox.AddChildToHorizontalBox(ValueText))
			{
				ValueSlot->SetVerticalAlignment(VAlign_Center);
				// 아이콘이 없으면 이 수치가 페어의 시작 — 앞 항목이 있으면 12px 간격
				ValueSlot->SetPadding(FMargin((!IconTex && bAny) ? 12.f : 0.f, 0.f, 0.f, 0.f));
			}

			bAny = true;
		}

		return bAny;
	}
}
