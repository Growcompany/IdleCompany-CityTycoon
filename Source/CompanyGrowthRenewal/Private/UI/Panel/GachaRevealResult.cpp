#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "Enum/WidgetType.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"

namespace GachaRevealResultDetail
{
	UWidget* Child(UWidgetTree* Tree, const TCHAR* Name)
	{
		return Tree ? Tree->FindWidget(FName(Name)) : nullptr;
	}

	void PutText(UWidgetTree* Tree, const TCHAR* Name, const FText& Value)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Child(Tree, Name)))
		{
			Text->SetText(Value);
			Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}

	void Show(UWidgetTree* Tree, const TCHAR* Name, bool bVisible)
	{
		if (UWidget* Item = Child(Tree, Name))
		{
			Item->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

FText UGachaRevealPresentationWidget::BuildSummaryLine() const
{
	if (Entries.IsEmpty()) { return FText::GetEmpty(); }
	int32 BestGrade = 0;
	int32 NewCount = 0;
	int32 DuplicateCount = 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FGachaRevealEntry& Entry = Entries[Index];
		BestGrade = FMath::Max(BestGrade, Entry.Grade);
		NewCount += Entry.bNew ? 1 : 0;
		DuplicateCount += bSkinDuplicates.IsValidIndex(Index) && bSkinDuplicates[Index] ? 1 : 0;
	}
	const FText Best = GradeLabel(BestGrade);
	if (Kind == EGachaRevealKind::Equipment)
	{
		return AwardContext.bOwnedBefore.Num() == Entries.Num()
			? FText::Format(NSLOCTEXT("GachaReveal", "EquipmentSummary", "최고 {0} · 새 장비 {1}개 · 장착 추천 {2}개"),
				Best, FText::AsNumber(NewCount), FText::AsNumber(GetRecommendedCount()))
			: FText::Format(NSLOCTEXT("GachaReveal", "BestOnly", "최고 {0}"), Best);
	}
	if (AwardContext.MileageGained == INDEX_NONE || (Kind == EGachaRevealKind::Trait && AwardContext.bOwnedBefore.Num() != Entries.Num()))
	{
		return FText::Format(NSLOCTEXT("GachaReveal", "BestOnly", "최고 {0}"), Best);
	}
	if (Kind == EGachaRevealKind::Trait)
	{
		return FText::Format(NSLOCTEXT("GachaReveal", "TraitSummary", "최고 {0} · 새 특성 {1}개 · 마일리지 +{2}P"),
			Best, FText::AsNumber(NewCount), FText::AsNumber(AwardContext.MileageGained));
	}
	return FText::Format(NSLOCTEXT("GachaReveal", "SkinSummary", "최고 {0} · 새 스킨 {1}개 · 중복 {2}개 → 마일리지 +{3}P"),
		Best, FText::AsNumber(NewCount), FText::AsNumber(DuplicateCount), FText::AsNumber(AwardContext.MileageGained));
}

int32 UGachaRevealPresentationWidget::BestResultIndex() const
{
	if (Entries.IsEmpty()) { return INDEX_NONE; }
	// 대표 카드는 화면 정렬 토글과 무관하게 시안의 등급·NEW·추천·뽑힌 순서를 따른다.
	return GachaRevealModel::SortedOrder(Entries, EGachaRevealSort::Grade, Recommended)[0];
}

void UGachaRevealPresentationWidget::ConfigureResultDetails()
{
	using namespace GachaRevealResultDetail;
	if (!WidgetTree || Entries.IsEmpty()) { return; }
	if (SortGradeControl.IsValid()) { SortGradeControl->SetIsSelected(SortMode == EGachaRevealSort::Grade, false); }
	if (SortOrderControl.IsValid()) { SortOrderControl->SetIsSelected(SortMode == EGachaRevealSort::PullOrder, false); }
	const int32 BestIndex = BestResultIndex();
	const FGachaRevealCardData& Best = CardData[BestIndex];
	const FText BestGrade = GradeLabel(Entries[BestIndex].Grade);
	PutText(WidgetTree, TEXT("SummaryBestGrade"), BestGrade);
	int32 NewCount = 0;
	int32 DuplicateCount = 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FGachaRevealEntry& Entry = Entries[Index];
		NewCount += Entry.bNew ? 1 : 0;
		DuplicateCount += bSkinDuplicates.IsValidIndex(Index) && bSkinDuplicates[Index] ? 1 : 0;
	}
	PutText(WidgetTree, TEXT("SummaryNew"), Kind == EGachaRevealKind::Skin || AwardContext.bOwnedBefore.Num() == Entries.Num()
		? FText::Format(Kind == EGachaRevealKind::Equipment
			? NSLOCTEXT("GachaReveal", "NewEquipment", "새 장비 {0}개")
			: Kind == EGachaRevealKind::Trait ? NSLOCTEXT("GachaReveal", "NewTrait", "새 특성 {0}개")
			: NSLOCTEXT("GachaReveal", "NewSkin", "새 스킨 {0}개"), FText::AsNumber(NewCount)) : FText::GetEmpty());
	PutText(WidgetTree, TEXT("SummaryRecommend"), Kind == EGachaRevealKind::Equipment && RevealEmployeeId != INDEX_NONE
		? FText::Format(NSLOCTEXT("GachaReveal", "Recommended", "장착 추천 {0}개"), FText::AsNumber(GetRecommendedCount()))
		: FText::GetEmpty());
	FText MileageLine;
	if (Kind == EGachaRevealKind::Trait && AwardContext.MileageGained != INDEX_NONE)
	{
		MileageLine = FText::Format(NSLOCTEXT("GachaReveal", "TraitMileageSummary", "마일리지 +{0}P"), FText::AsNumber(AwardContext.MileageGained));
	}
	else if (Kind == EGachaRevealKind::Skin && AwardContext.MileageGained != INDEX_NONE)
	{
		MileageLine = FText::Format(NSLOCTEXT("GachaReveal", "SkinMileageSummary", "중복 {0}개 → 마일리지 +{1}P"),
			FText::AsNumber(DuplicateCount), FText::AsNumber(AwardContext.MileageGained));
	}
	PutText(WidgetTree, TEXT("SummaryMileage"), MileageLine);
	Show(WidgetTree, TEXT("TallySummary"), Entries.Num() > 1);
	const TArray<int32> ShownGrades = GachaRevealModel::TallyGrades(AwardContext.GradeWeights, Entries);
	bool bPrevious = false;
	for (int32 Grade = GachaRevealModel::MaxGrade; Grade >= 0; --Grade)
	{
		const bool bShowGrade = ShownGrades.Contains(Grade);
		if (TallyCellWidgets.IsValidIndex(Grade) && TallyCellWidgets[Grade])
		{
			TallyCellWidgets[Grade]->SetVisibility(bShowGrade ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			TallyCellWidgets[Grade]->SetRenderOpacity(1.f);
		}
		const bool bHasCount = TallyCounts.IsValidIndex(Grade) && TallyCounts[Grade] > 0;
		if (TallyLatinTexts.IsValidIndex(Grade) && TallyLatinTexts[Grade]) { TallyLatinTexts[Grade]->SetRenderOpacity(bHasCount ? 1.f : .4f); }
		if (TallyKoreanTexts.IsValidIndex(Grade) && TallyKoreanTexts[Grade]) { TallyKoreanTexts[Grade]->SetRenderOpacity(bHasCount ? 1.f : .4f); }
		if (TallyCountTexts.IsValidIndex(Grade) && TallyCountTexts[Grade]) { TallyCountTexts[Grade]->SetRenderOpacity(bHasCount ? 1.f : .28f); }
		Show(WidgetTree, *FString::Printf(TEXT("TallySep%d"), Grade), bShowGrade && bPrevious);
		if (TallyLatinTexts.IsValidIndex(Grade) && TallyLatinTexts[Grade])
		{
			TallyLatinTexts[Grade]->SetVisibility(bShowGrade && !IsKoreanGrades() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (TallyKoreanTexts.IsValidIndex(Grade) && TallyKoreanTexts[Grade])
		{
			TallyKoreanTexts[Grade]->SetVisibility(bShowGrade && IsKoreanGrades() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		bPrevious |= bShowGrade;
	}
	if (UGachaRevealCardWidget* BestCard = Cast<UGachaRevealCardWidget>(Child(WidgetTree, TEXT("BestCard"))))
	{
		BestCard->Configure(Best, EGachaCardSize::Best);
		BestCard->SetFaceUp(true);
	}
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(BestPanel->Slot))
	{
		PanelSlot->SetSize(Entries.Num() > 10 ? FVector2D(430.f, 988.f) : FVector2D(470.f, 988.f));
	}
	if (USizeBox* CardBox = Cast<USizeBox>(Child(WidgetTree, TEXT("BestCardBox"))))
	{
		CardBox->SetWidthOverride(Entries.Num() > 10 ? 338.f : 378.f);
		CardBox->SetHeightOverride(Entries.Num() > 10 ? 338.f : 378.f);
	}
	PutText(WidgetTree, TEXT("BestName"), Best.Name);
	const FString FullMeta = EntryMeta[BestIndex].ToString();
	FString BestPart;
	FString EquipmentNoun;
	if (!FullMeta.Split(TEXT(" · "), &BestPart, &EquipmentNoun)) { BestPart = FullMeta; }
	PutText(WidgetTree, TEXT("BestMeta"), FText::Format(NSLOCTEXT("GachaReveal", "BestMeta", "{0} · {1}"), BestGrade, FText::FromString(BestPart)));
	PutText(WidgetTree, TEXT("BestEffectValue"), Best.EffectLine);
	PutText(WidgetTree, TEXT("BestUnique"), Best.UniqueLine);
	PutText(WidgetTree, TEXT("SingleName"), Best.Name);
	PutText(WidgetTree, TEXT("SingleMeta"), EntryMeta[BestIndex]);
	PutText(WidgetTree, TEXT("SingleEffectValue"), Best.EffectLine);
	PutText(WidgetTree, TEXT("SingleUnique"), Best.UniqueLine);
	PutText(WidgetTree, TEXT("SingleGrade"), !IsKoreanGrades() ? BestGrade : FText::GetEmpty());
	PutText(WidgetTree, TEXT("SingleGradeKor"), IsKoreanGrades() ? BestGrade : FText::GetEmpty());
	const bool bCanEquip = Kind == EGachaRevealKind::Equipment && RevealEmployeeId != INDEX_NONE && !bRecommendedEquipped && !Recommended.IsEmpty();
	if (EquipControl.IsValid())
	{
		EquipControl->SetVisibility(Entries.Num() > 1 && bCanEquip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		EquipControl->SetButtonText(FText::Format(NSLOCTEXT("GachaReveal", "EquipRecommended", "추천 장비 {0}개 장착"), FText::AsNumber(GetRecommendedCount())));
		EquipControl->SetIsEnabled(true);
	}
	if (SingleEquipControl.IsValid())
	{
		SingleEquipControl->SetVisibility(Entries.Num() == 1 && bCanEquip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		SingleEquipControl->SetButtonText(NSLOCTEXT("GachaReveal", "EquipNow", "바로 장착"));
		SingleEquipControl->SetIsEnabled(true);
	}
	for (const TCHAR* Prefix : {TEXT("Best"), TEXT("Single")})
	{
		UWidgetSwitcher* Compare = Cast<UWidgetSwitcher>(Child(WidgetTree, *FString::Printf(TEXT("%sCompare"), Prefix)));
		if (!Compare) { continue; }
		if (Kind == EGachaRevealKind::Equipment && RevealEmployeeId != INDEX_NONE)
		{
			Compare->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Compare->SetActiveWidgetIndex(Entries[BestIndex].Grade > Entries[BestIndex].EquippedGrade ? 0 : 1);
			Show(WidgetTree, *FString::Printf(TEXT("%sCompareGoodIconBox"), Prefix), true);
			PutText(WidgetTree, *FString::Printf(TEXT("%sCompareGoodText"), Prefix),
				FText::Format(NSLOCTEXT("GachaReveal", "CompareGood", "현재 장착 {0} → {1}"),
					Entries[BestIndex].EquippedGrade == INDEX_NONE ? NSLOCTEXT("GachaReveal", "EmptySlot", "없음") : GradeLabel(Entries[BestIndex].EquippedGrade), BestGrade));
			PutText(WidgetTree, *FString::Printf(TEXT("%sComparePlainText"), Prefix), NSLOCTEXT("GachaReveal", "NotBetter", "지금 장착한 장비와 같거나 낮음"));
		}
		else if (Kind == EGachaRevealKind::Skin && Entries[BestIndex].MileageGained > 0)
		{
			Compare->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Compare->SetActiveWidgetIndex(2);
			PutText(WidgetTree, *FString::Printf(TEXT("%sCompareDupText"), Prefix), FText::Format(
				NSLOCTEXT("GachaReveal", "DuplicateMileage", "이미 보유 → 마일리지 +{0}P"), FText::AsNumber(Entries[BestIndex].MileageGained)));
		}
		else if (Entries[BestIndex].bNew &&
			((Kind == EGachaRevealKind::Trait && AwardContext.bOwnedBefore.Num() == Entries.Num() && !AwardContext.bOwnedBefore[BestIndex]) ||
			(Kind == EGachaRevealKind::Skin && bSkinDuplicates.Num() == Entries.Num() && !bSkinDuplicates[BestIndex])))
		{
			Compare->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Compare->SetActiveWidgetIndex(0);
			Show(WidgetTree, *FString::Printf(TEXT("%sCompareGoodIconBox"), Prefix), false);
			PutText(WidgetTree, *FString::Printf(TEXT("%sCompareGoodText"), Prefix),
				Kind == EGachaRevealKind::Trait ? NSLOCTEXT("GachaReveal", "NewTraitComparison", "새로 얻은 특성입니다")
				: NSLOCTEXT("GachaReveal", "NewSkinComparison", "새로 얻은 스킨입니다"));
		}
		else { Compare->SetVisibility(ESlateVisibility::Collapsed); }
	}
	UpdateResultGauge();
}

void UGachaRevealPresentationWidget::UpdateResultGauge()
{
	using namespace GachaRevealResultDetail;
	if (!WidgetTree || ResultClockStartMs < 0.f) { return; }
	const float P = FMath::Clamp((ClockMs() - ResultClockStartMs) / GachaRevealTiming::SettleCount, 0.f, 1.f);
	int32 Before = 0;
	int32 After = 0;
	int32 Limit = 0;
	if (Kind == EGachaRevealKind::Equipment)
	{
		Before = AwardContext.PityBefore;
		After = AwardContext.PityAfter;
		Limit = AwardContext.PityThreshold;
		if (Before < 0 || After < 0 || Limit <= 0) { Show(WidgetTree, TEXT("PityPanel"), false); return; }
		PutText(WidgetTree, TEXT("PityLabel"), NSLOCTEXT("GachaReveal", "SSSPity", "SSS 보장"));
		const int32 HitCount = PityHitCount();
		const float HitPhase = P * (HitCount + 1);
		const int32 HitSegment = FMath::FloorToInt(HitPhase);
		const bool bAtHit = HitSegment < HitCount && HitPhase - HitSegment >= .85f;
		PutText(WidgetTree, TEXT("PityCount"), bAtHit ? NSLOCTEXT("GachaReveal", "PityReached", "달성") : FText::AsNumber(FMath::Max(0, Limit - After)));
		PutText(WidgetTree, TEXT("PityUnit"), bAtHit ? FText::GetEmpty() : NSLOCTEXT("GachaReveal", "PityRemaining", "회 남음"));
		PutText(WidgetTree, TEXT("PityDelta"), FText::Format(NSLOCTEXT("GachaReveal", "PityDelta", "−{0}"), FText::AsNumber(Entries.Num())));
		Show(WidgetTree, TEXT("PityFill"), true);
		Show(WidgetTree, TEXT("MileageFill"), false);
	}
	else
	{
		After = AwardContext.MileageBalance;
		Before = After - AwardContext.MileageGained;
		Limit = Kind == EGachaRevealKind::Trait ? FGachaTraitData::MileageLegendaryBox : FGachaSkinData::MileageLegendaryBox;
		if (After < 0 || AwardContext.MileageGained < 0) { Show(WidgetTree, TEXT("PityPanel"), false); return; }
		PutText(WidgetTree, TEXT("PityLabel"), NSLOCTEXT("GachaReveal", "Mileage", "마일리지"));
		PutText(WidgetTree, TEXT("PityCount"), FText::Format(NSLOCTEXT("GachaReveal", "MileageGauge", "{0}/{1}"), FText::AsNumber(After), FText::AsNumber(Limit)));
		PutText(WidgetTree, TEXT("PityUnit"), FText::GetEmpty());
		PutText(WidgetTree, TEXT("PityDelta"), FText::Format(NSLOCTEXT("GachaReveal", "MileageDelta", "+{0}"), FText::AsNumber(AwardContext.MileageGained)));
		Show(WidgetTree, TEXT("PityFill"), false);
		Show(WidgetTree, TEXT("MileageFill"), true);
	}
	Show(WidgetTree, TEXT("PityPanel"), true);
	if (USizeBox* Fill = Cast<USizeBox>(Child(WidgetTree, TEXT("PityFillBox"))))
	{
		const float Value = Kind == EGachaRevealKind::Equipment ? PityFillValueAt(P)
			: FMath::Lerp(static_cast<float>(Before), static_cast<float>(After), P);
		Fill->SetWidthOverride(420.f * FMath::Clamp(Value / FMath::Max(1, Limit), 0.f, 1.f));
	}
}

int32 UGachaRevealPresentationWidget::PityHitCount() const
{
	int32 Hits = 0;
	for (const FGachaRevealEntry& Entry : Entries)
	{
		Hits += Entry.Grade == static_cast<int32>(EEmployeeEquipmentGrade::Ascendant) ? 1 : 0;
	}
	return Hits;
}

float UGachaRevealPresentationWidget::PityFillValueAt(float Progress) const
{
	const int32 Before = AwardContext.PityBefore;
	const int32 After = AwardContext.PityAfter;
	const int32 Limit = AwardContext.PityThreshold;
	if (Before < 0 || After < 0 || Limit <= 0) { return 0.f; }
	const int32 AscendantHits = PityHitCount();
	if (AscendantHits == 0) { return FMath::Lerp(static_cast<float>(Before), static_cast<float>(After), FMath::Clamp(Progress, 0.f, 1.f)); }
	const float Segment = FMath::Clamp(Progress, 0.f, 1.f) * (AscendantHits + 1);
	const int32 HitSegment = FMath::Min(FMath::FloorToInt(Segment), AscendantHits);
	if (HitSegment == AscendantHits)
	{
		return FMath::Lerp(0.f, static_cast<float>(After), Segment - AscendantHits);
	}
	return FMath::Lerp(HitSegment == 0 ? static_cast<float>(Before) : 0.f,
		static_cast<float>(Limit), Segment - HitSegment);
}

void UGachaRevealPresentationWidget::OnSortGradeClicked()
{
	SortMode = EGachaRevealSort::Grade;
	ApplySettledLayout(true);
	SortMoveStartMs = ClockMs();
	if (SortGradeControl.IsValid()) { SortGradeControl->SetIsSelected(true); }
	if (SortOrderControl.IsValid()) { SortOrderControl->SetIsSelected(false); }
}

void UGachaRevealPresentationWidget::OnSortOrderClicked()
{
	SortMode = EGachaRevealSort::PullOrder;
	ApplySettledLayout(true);
	SortMoveStartMs = ClockMs();
	if (SortGradeControl.IsValid()) { SortGradeControl->SetIsSelected(false); }
	if (SortOrderControl.IsValid()) { SortOrderControl->SetIsSelected(true); }
}

void UGachaRevealPresentationWidget::OnEquipClicked() { EquipRecommended(); }

FEmployeeEquipmentRequest UGachaRevealPresentationWidget::MakeRecommendedRequest(int32 Index, UEmployeeEquipmentManagerSubsystem& Equipment) const
{
	FEmployeeEquipmentRequest Request = Equipment.MakeRequest(EEquipmentCommand::SetSlot);
	Request.EmployeeId = RevealEmployeeId;
	Request.EquipmentSlot = static_cast<EEmployeeEquipmentSlot>(Entries[Index].SlotKey);
	Request.InstanceId = EquipmentInstanceIds[Index];
	Request.ExpectedSourceEmployeeId = Equipment.GetInstanceEmployee(Request.InstanceId);
	return Request;
}

TArray<int32> UGachaRevealPresentationWidget::PendingRecommendedIndices(const UEmployeeEquipmentManagerSubsystem& Equipment) const
{
	TArray<int32> Pending;
	for (int32 Index : GachaRevealModel::PickRecommended(Entries))
	{
		if (!EquipmentInstanceIds.IsValidIndex(Index) || !EquipmentInstanceIds[Index].IsValid()) { Pending.Add(Index); continue; }
		const EEmployeeEquipmentSlot EquipmentSlot = static_cast<EEmployeeEquipmentSlot>(Entries[Index].SlotKey);
		const FEmployeeEquipmentInstance* Worn = Equipment.GetEquippedInstance(RevealEmployeeId, EquipmentSlot);
		if (!Worn || Worn->InstanceId != EquipmentInstanceIds[Index]) { Pending.Add(Index); }
	}
	return Pending;
}

void UGachaRevealPresentationWidget::EquipRecommended()
{
	if (Kind != EGachaRevealKind::Equipment || RevealEmployeeId == INDEX_NONE || Recommended.IsEmpty()) { return; }
	UGameInstance* GI = GetGameInstance();
	UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	if (!Equipment) { return; }
	int32 EquippedCount = 0;
	for (int32 Index : PendingRecommendedIndices(*Equipment))
	{
		if (!EquipmentInstanceIds.IsValidIndex(Index) || !EquipmentInstanceIds[Index].IsValid()) { continue; }
		const FEmployeeEquipmentRequest Request = MakeRecommendedRequest(Index, *Equipment);
		if (Equipment->Execute(Request).bCommitted) { ++EquippedCount; }
	}
	const int32 Remaining = PendingRecommendedIndices(*Equipment).Num();
	bRecommendedEquipped = Remaining == 0;
	const FText ButtonLabel = bRecommendedEquipped ? NSLOCTEXT("GachaReveal", "Equipped", "장착 완료")
		: FText::Format(NSLOCTEXT("GachaReveal", "EquipRecommended", "추천 장비 {0}개 장착"), FText::AsNumber(Remaining));
	const FText SingleLabel = bRecommendedEquipped ? NSLOCTEXT("GachaReveal", "Equipped", "장착 완료")
		: NSLOCTEXT("GachaReveal", "EquipNow", "바로 장착");
	if (EquipControl.IsValid()) { EquipControl->SetButtonText(ButtonLabel); EquipControl->SetIsEnabled(!bRecommendedEquipped); }
	if (SingleEquipControl.IsValid()) { SingleEquipControl->SetButtonText(SingleLabel); SingleEquipControl->SetIsEnabled(!bRecommendedEquipped); }
	if (EquippedCount == 0) { return; }
	if (UUIManagerSubsystem* UI = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UEmployeeManager* Employees = GI->GetSubsystem<UEmployeeManager>();
		const FText Name = Employees ? FText::FromString(Employees->GetEmployeeFullName(RevealEmployeeId)) : FText::GetEmpty();
		UI->ShowNotification(FText::Format(NSLOCTEXT("GachaReveal", "EquipToast", "{0}에게 장비 {1}개를 장착했습니다"),
			Name, FText::AsNumber(EquippedCount)), 3.f, ENotificationType::Success);
	}
}

void UGachaRevealPresentationWidget::ShowCardTooltip(int32 Index, FVector2D AnchorAbs)
{
	if (!CardData.IsValidIndex(Index)) { return; }
	const UTableManagerSubsystem* Tables = ResolveTables();
	const TSubclassOf<UUserWidget> TooltipClass = Tables ? Tables->GetWidgetClass(EWidgetType::ItemTooltip) : nullptr;
	if (!TooltipClass) { UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] ItemTooltip 미등록")); return; }
	if (!ActiveTooltip) { ActiveTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass); }
	if (!ActiveTooltip) { return; }
	const FGachaRevealCardData& Card = CardData[Index];
	FText Description = FText::Format(NSLOCTEXT("GachaReveal", "TooltipMeta", "{0} · {1}\n{2}"),
		GradeLabel(Card.Grade), EntryMeta.IsValidIndex(Index) ? EntryMeta[Index] : FText::GetEmpty(), Card.EffectLine);
	if (!Card.UniqueLine.IsEmpty())
	{
		Description = FText::Format(NSLOCTEXT("GachaReveal", "TooltipUnique", "{0}\n고유 효과 · {1}"), Description, Card.UniqueLine);
	}
	if (Card.bRecommended && Entries.IsValidIndex(Index))
	{
		Description = FText::Format(NSLOCTEXT("GachaReveal", "TooltipCompare", "{0}\n장착 추천: 현재 {1} → {2}"),
			Description, Entries[Index].EquippedGrade == INDEX_NONE ? NSLOCTEXT("GachaReveal", "EmptySlot", "없음") : GradeLabel(Entries[Index].EquippedGrade), GradeLabel(Card.Grade));
	}
	if (Kind == EGachaRevealKind::Skin && Card.MileageGained > 0)
	{
		Description = FText::Format(NSLOCTEXT("GachaReveal", "TooltipMileage", "{0}\n중복 마일리지 +{1}P"), Description, FText::AsNumber(Card.MileageGained));
	}
	ActiveTooltip->ShowAt(AnchorAbs, Card.Name, Card.Icon.LoadSynchronous(), Description, 0, 2.5f, ETooltipAnchor::BelowAnchor, 1001);
}
