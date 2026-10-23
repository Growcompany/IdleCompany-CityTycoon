#include "UI/Element/SideContent/BusinessTripCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/SideDungeonLootTable.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/LiveOps/TrainingRewardChipWidget.h"

namespace
{
	// unity 빌드가 익명 namespace 를 한 블롭으로 합치므로 이름을 모듈 고유로 둔다
	constexpr float TripLootIconSize = 36.f;
	// 카드 세로 예산이 2줄까지다 — 3줄이면 카드 밖으로 새어 선발 레일을 덮는다(09-25 캡처 실측)
	constexpr int32 TripLootMaxRows = 2;
	// 카드의 드랍 칩은 가로라 세로가 아니라 **폭**이 예산이다 — 2개가 상한이고 3개면 아트 칸을 민다
	constexpr int32 TripLootMaxTiles = 2;
	constexpr float TripLootTileGap = 18.f;
}

void UBusinessTripCardWidget::BuildLootPreview(UWidgetTree& InWidgetTree, UGameInstance* InGameInstance, UVerticalBox& InList,
	const TArray<FSideDungeonLootRow>& InRows, int32 InStage, const UCommonTextBlock* InSample)
{
	InList.ClearChildren();

	int32 Shown = 0;
	for (const FSideDungeonLootRow& Row : InRows)
	{
		if (InStage < Row.MinStage || InStage > Row.MaxStage) continue;
		// 카드 세로 예산이 3줄까지다 — 그 아래는 접는다(넘치면 Slate 가 이웃을 침범한다)
		if (Shown >= TripLootMaxRows) break;

		CGRewardChip::FDisplayEntry Entry;
		Entry.ItemType = Row.ItemType;
		Entry.ResourceType = Row.ResourceType;
		// 미리보기는 상한만 보여준다 — 범위를 둘 다 쓰면 줄이 길어진다
		Entry.Quantity = (Row.ItemType != EItemType::None) ? Row.ItemAmountMax : Row.ResourceAmountMax;

		UHorizontalBox* Line = InWidgetTree.ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		if (UObject* IconTex = CGRewardChip::ResolveIcon(InGameInstance, Entry))
		{
			UImage* IconImage = InWidgetTree.ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(IconTex);
			IconBrush.ImageSize = FVector2D(TripLootIconSize, TripLootIconSize);
			IconImage->SetBrush(IconBrush);

			if (UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(IconImage))
			{
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
		}

		UCommonTextBlock* Label = InWidgetTree.ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
		Label->SetText(FText::Format(NSLOCTEXT("BusinessTrip", "LootRow", "{0} · {1}"),
			CGRewardChip::FormatQuantity(Entry), GetSideDropOddsDisplayName(Row.Odds)));
		if (InSample)
		{
			// 폰트/색은 디자인타임 샘플에서 복사한다 — 코드가 폰트를 쥐지 않는다(타이포 §7)
			Label->SetFont(InSample->GetFont());
			Label->SetColorAndOpacity(InSample->GetColorAndOpacity());
		}
		// 줄이 칸보다 길면 잘라낸다 — Slate 기본은 클리핑이 없어 옆 카드를 침범한다
		Label->SetClipping(EWidgetClipping::ClipToBounds);

		if (UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		if (UVerticalBoxSlot* LineSlot = InList.AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.f, Shown ? 6.f : 0.f, 0.f, 0.f));
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		++Shown;
	}
}

void UBusinessTripCardWidget::SetLootPreview(const TArray<FSideDungeonLootRow>& InRows, int32 InStage)
{
	if (!RewardList) return;
	RewardList->ClearChildren();

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> ChipClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::TripRewardChip) : nullptr;
	if (!ChipClass)
	{
		// 조용히 비우면 "드랍이 없는 카드" 로 읽힌다 — DT 행 누락은 시끄럽게 드러나야 한다
		UE_LOG(LogTemp, Warning, TEXT("[BusinessTrip] DT_WidgetClass 에 TripRewardChip 행이 없어 드랍을 못 보여준다"));
		return;
	}

	int32 Shown = 0;
	for (const FSideDungeonLootRow& Row : InRows)
	{
		if (InStage < Row.MinStage || InStage > Row.MaxStage) continue;
		// 카드 가로 예산이 칩 2개다 — 3개면 아트 칸을 밀어낸다
		if (Shown >= TripLootMaxTiles) break;

		UTrainingRewardChipWidget* Chip = CreateWidget<UTrainingRewardChipWidget>(this, ChipClass);
		if (!Chip) continue;
		Chip->ConfigurePreview(Row);
		if (UHorizontalBoxSlot* BoxSlot = RewardList->AddChildToHorizontalBox(Chip))
		{
			BoxSlot->SetPadding(FMargin(Shown ? TripLootTileGap : 0.f, 0.f, 0.f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}
		++Shown;
	}
}

FText UBusinessTripCardWidget::FormatTripDuration(int32 Seconds)
{
	if (Seconds < 3600)
	{
		return FText::Format(NSLOCTEXT("BusinessTrip", "DurMin", "{0}분"), FText::AsNumber(FMath::Max(Seconds / 60, 1)));
	}
	return FText::Format(NSLOCTEXT("BusinessTrip", "DurHour", "{0}시간"), FText::AsNumber(Seconds / 3600));
}

void UBusinessTripCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OnClicked().AddUObject(this, &UBusinessTripCardWidget::HandleClicked);
}

void UBusinessTripCardWidget::NativeDestruct()
{
	OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UBusinessTripCardWidget::SetCard(FName InCardKey, const FBusinessTripCardTable& InRow, const FText& InDisciplineName)
{
	CardKey = InCardKey;

	if (DestinationText) DestinationText->SetText(InRow.DisplayName);
	if (DurationText) DurationText->SetText(FormatTripDuration(InRow.DurationSeconds));

	if (ConditionText)
	{
		ConditionText->SetText(FText::Format(
			NSLOCTEXT("BusinessTrip", "CardCondition", "인원 {0} · {1} 합 {2} 이상"),
			FText::AsNumber(InRow.CrewCount), InDisciplineName, FText::AsNumber(InRow.RequiredScoreSum)));
	}

	if (ExperienceText)
	{
		ExperienceText->SetText(FText::Format(
			NSLOCTEXT("BusinessTrip", "CardExp", "경험치 +{0}"), FText::AsNumber(FMath::RoundToInt(InRow.ExperienceReward))));
	}

	if (DestinationArt)
	{
		if (UTexture2D* Art = InRow.CardArt.LoadSynchronous())
		{
			DestinationArt->SetBrushFromTexture(Art, false);
			DestinationArt->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 빈 셀 = 의도적 없음. 공용 그림으로 덮으면 목적지가 안 갈린다
			DestinationArt->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	SetCardState(EBusinessTripCardState::Normal);
}

void UBusinessTripCardWidget::SetSelected(bool bInSelected)
{
	if (SelectedRing) SelectedRing->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UBusinessTripCardWidget::SetCardState(EBusinessTripCardState InState)
{
	if (!StatusStrip) return;

	if (InState == EBusinessTripCardState::Normal)
	{
		StatusStrip->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	StatusStrip->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (StatusText)
	{
		// 인원 미달과 직능 미달은 고치는 방법이 달라서 문구를 나눈다
		StatusText->SetText(InState == EBusinessTripCardState::CrewShort
			? NSLOCTEXT("BusinessTrip", "CrewShort", "직원 부족")
			: NSLOCTEXT("BusinessTrip", "DiscShort", "직능 부족"));
	}
}

void UBusinessTripCardWidget::HandleClicked()
{
	OnTripCardClicked.Broadcast(CardKey);
}
