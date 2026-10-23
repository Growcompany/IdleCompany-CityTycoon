#include "UI/Element/Employee/EmployeeEquipmentListWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "CommonTextBlock.h"
#include "Components/TileView.h"
#include "Blueprint/WidgetTree.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "EmployeeEquipmentList"

namespace
{
	void ApplyEquipmentListChipSize(UImage* Face, UImage* Line, FVector2D& LastSize)
	{
		if (!Face) return;
		const FVector2D Size = Face->GetCachedGeometry().GetLocalSize();
		if (Size.X < 1.f || Size.Y < 1.f || Size.Equals(LastSize, .5f)) return;
		LastSize = Size;
		for (UImage* Layer : {Face, Line})
		{
			if (!Layer) continue;
			if (UMaterialInstanceDynamic* Material = Layer->GetDynamicMaterial())
			{
				Material->SetScalarParameterValue(TEXT("Wpx"), Size.X);
				Material->SetScalarParameterValue(TEXT("Hpx"), Size.Y);
			}
		}
	}
}

void UEmployeeEquipmentListWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	// 선택박스가 디자이너에서도 「전체 부위」를 보여야 칸 폭을 눈으로 잰다.
	RebuildPartOptions();
	RebuildGradeOptions();
}

void UEmployeeEquipmentListWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (PartCombo && !PartCombo->OnSelectionChanged.IsAlreadyBound(this, &ThisClass::HandlePartChanged))
	{
		PartCombo->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GeneratePartRow);
		PartCombo->OnSelectionChanged.AddDynamic(this, &ThisClass::HandlePartChanged);
	}
	if (GradeCombo && !GradeCombo->OnSelectionChanged.IsAlreadyBound(this, &ThisClass::HandleGradeChanged))
	{
		GradeCombo->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GenerateGradeRow);
		GradeCombo->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGradeChanged);
	}
	if (InventoryView)
	{
		InventoryView->SetSelectionMode(ESelectionMode::Single);
		InventoryView->OnItemClicked().AddUObject(this, &ThisClass::HandleItemClicked);
		InventoryView->OnEntryWidgetGenerated().AddUObject(this, &ThisClass::HandleInventoryEntryGenerated);
	}
}

void UEmployeeEquipmentListWidget::NativeDestruct()
{
	PendingSelectionPulseId.Invalidate();
	if (InventoryView)
	{
		InventoryView->OnItemClicked().RemoveAll(this);
		InventoryView->OnEntryWidgetGenerated().RemoveAll(this);
	}
	if (PartCombo)
	{
		PartCombo->OnSelectionChanged.RemoveAll(this);
		PartCombo->OnGenerateWidgetEvent.Unbind();
	}
	if (GradeCombo)
	{
		GradeCombo->OnSelectionChanged.RemoveAll(this);
		GradeCombo->OnGenerateWidgetEvent.Unbind();
	}
	Super::NativeDestruct();
}

void UEmployeeEquipmentListWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ApplyEquipmentListChipSize(PartFilterChipBG, PartFilterChipLine, LastPartChipSize);
	ApplyEquipmentListChipSize(GradeFilterChipBG, GradeFilterChipLine, LastGradeChipSize);
	if (InventoryView)
	{
		const float ViewWidth = InventoryView->GetCachedGeometry().GetLocalSize().X;
		if (ViewWidth > 1.f)
		{
			// Reserve scrollbar space and include UE's half-spacing in each tile's pitch.
			const float EntrySide = FMath::Max(1.f,
				(ViewWidth - 20.f) / 4.f - InventoryView->GetHorizontalEntrySpacing() * .5f);
			if (!FMath::IsNearlyEqual(InventoryView->GetEntryWidth(), EntrySide, .25f))
			{
				InventoryView->SetEntryWidth(EntrySide);
				InventoryView->SetEntryHeight(EntrySide);
			}
		}
	}
}

void UEmployeeEquipmentListWidget::RebuildPartOptions()
{
	if (!PartCombo) return;
	UGameInstance* GI = GetGameInstance();
	const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	TArray<FString> Wanted;
	Wanted.Add(LOCTEXT("AllParts", "전체 부위").ToString());
	for (int32 Index = 0; Index < static_cast<int32>(EEmployeeEquipmentSlot::Count); ++Index)
	{
		const auto PartSlot = static_cast<EEmployeeEquipmentSlot>(Index);
		const auto* Row = Tables ? Tables->FindEmployeeEquipmentSlot(PartSlot) : nullptr;
		// DT 가 비면 코드 폴백을 만들지 않는다 ― 빈 칸이 그대로 드러나야 임포트 누락이 보인다.
		Wanted.Add(Row ? Row->DisplayName.ToString() : FString());
	}
	if (Wanted != PartLabels)
	{
		PartLabels = Wanted;
		bUpdatingCombo = true;
		PartCombo->ClearOptions();
		for (const FString& Label : PartLabels) PartCombo->AddOption(Label);
		bUpdatingCombo = false;
	}
	PartCounts.Init(0, PartLabels.Num());
}

void UEmployeeEquipmentListWidget::RebuildGradeOptions()
{
	if (!GradeCombo) return;
	const UGameInstance* GI = GetGameInstance();
	const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TArray<FString> Wanted = {LOCTEXT("AllGrades", "전체 등급").ToString()};
	for (int32 Index = 0; Index < static_cast<int32>(EEmployeeEquipmentGrade::Count); ++Index)
	{
		const auto* Row = Tables ? Tables->FindEmployeeEquipmentGrade(static_cast<EEmployeeEquipmentGrade>(Index)) : nullptr;
		Wanted.Add(Row ? Row->DisplayName.ToString() : FString());
	}
	if (Wanted != GradeLabels)
	{
		GradeLabels = MoveTemp(Wanted);
		bUpdatingCombo = true;
		GradeCombo->ClearOptions();
		for (const FString& Label : GradeLabels) GradeCombo->AddOption(Label);
		bUpdatingCombo = false;
	}
	GradeCounts.Init(0, GradeLabels.Num());
}

UWidget* UEmployeeEquipmentListWidget::GeneratePartRow(FString Item)
{
	const int32 Index = PartLabels.IndexOfByKey(Item);
	return MakeFilterRow(Item, PartCombo, PartCounts.IsValidIndex(Index) ? PartCounts[Index] : 0);
}

UWidget* UEmployeeEquipmentListWidget::GenerateGradeRow(FString Item)
{
	const int32 Index = GradeLabels.IndexOfByKey(Item);
	return MakeFilterRow(Item, GradeCombo, GradeCounts.IsValidIndex(Index) ? GradeCounts[Index] : 0);
}

UWidget* UEmployeeEquipmentListWidget::MakeFilterRow(const FString& Item, UComboBoxString* Combo, int32 ItemCount)
{
	if (!WidgetTree || !Combo) return nullptr;
	const FSlateFontInfo Font = Combo->GetFont();
	const FSlateColor Ink = Combo->GetForegroundColor();

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();

	UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
	Label->SetFont(Font);
	Label->SetColorAndOpacity(Ink);
	Label->SetText(FText::FromString(Item));
	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 칩으로는 못 싣던 것 ― 부위마다 몇 개인지. 0 개인 부위를 헛으로 누르지 않게 한다.
	UCommonTextBlock* Count = WidgetTree->ConstructWidget<UCommonTextBlock>();
	FSlateFontInfo CountFont = Font;
	CountFont.Size = FMath::Max(1, static_cast<int32>(Font.Size) - 4);
	Count->SetFont(CountFont);
	Count->SetColorAndOpacity(Ink);
	Count->SetText(FText::Format(LOCTEXT("OwnedCount", "{0}개"), FText::AsNumber(ItemCount)));
	if (UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(Count))
	{
		CountSlot->SetHorizontalAlignment(HAlign_Right);
		CountSlot->SetVerticalAlignment(VAlign_Center);
		CountSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
		CountSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	return Row;
}

void UEmployeeEquipmentListWidget::HandlePartChanged(FString Item, ESelectInfo::Type SelectType)
{
	if (bUpdatingCombo) return;
	const int32 Index = PartLabels.IndexOfByKey(Item);
	if (Index < 0) return;
	bAllSlots = (Index == 0);
	if (!bAllSlots) CurrentSlot = static_cast<EEmployeeEquipmentSlot>(Index - 1);
	Refresh();
}

void UEmployeeEquipmentListWidget::HandleGradeChanged(FString Item, ESelectInfo::Type SelectType)
{
	if (bUpdatingCombo) return;
	const int32 Index = GradeLabels.IndexOfByKey(Item);
	if (Index < 0) return;
	CurrentGrade = Index == 0 ? EEmployeeEquipmentGrade::Count : static_cast<EEmployeeEquipmentGrade>(Index - 1);
	Refresh();
}

void UEmployeeEquipmentListWidget::Configure(int32 EmployeeId)
{
	ContextEmployeeId = EmployeeId;
	Refresh();
}

void UEmployeeEquipmentListWidget::SelectPart(EEmployeeEquipmentSlot EquipmentSlot)
{
	const int32 Index = static_cast<int32>(EquipmentSlot);
	if (Index < 0 || Index >= static_cast<int32>(EEmployeeEquipmentSlot::Count)) return;
	bAllSlots = false;
	CurrentSlot = EquipmentSlot;
	Refresh();
}

void UEmployeeEquipmentListWidget::SelectAll()
{
	bAllSlots = true;
	CurrentGrade = EEmployeeEquipmentGrade::Count;
	Refresh();
}

void UEmployeeEquipmentListWidget::ShowAllParts()
{
	bAllSlots = true;
	Refresh();
}

int32 UEmployeeEquipmentListWidget::GetVisibleItemCount() const
{
	return InventoryView ? InventoryView->GetNumItems() : 0;
}

void UEmployeeEquipmentListWidget::ClearSelection()
{
	SelectedInstanceId.Invalidate();
	PendingSelectionPulseId.Invalidate();
	if (InventoryView) InventoryView->ClearSelection();
}

void UEmployeeEquipmentListWidget::HandleItemClicked(UObject* Item)
{
	const UEmployeeEquipmentItemObject* Entry = Cast<UEmployeeEquipmentItemObject>(Item);
	if (!Entry || !Entry->InstanceID.IsValid()) return;
	SelectItem(Entry->InstanceID);
}

bool UEmployeeEquipmentListWidget::SelectItem(const FGuid& InstanceId, bool bScrollIntoView)
{
	if (!InventoryView || !InstanceId.IsValid()) return false;
	const FGuid RequestedId = InstanceId;
	for (UObject* Value : InventoryView->GetListItems())
	{
		const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Value);
		if (!Entry || Entry->InstanceID != RequestedId) continue;
		const bool bNewSelection = SelectedInstanceId != RequestedId;
		SelectedInstanceId = RequestedId;
		InventoryView->SetSelectedItem(Entry);
		if (bScrollIntoView) InventoryView->RequestScrollItemIntoView(Value);
		if (bNewSelection) PendingSelectionPulseId = RequestedId;
		{
			TGuardValue<bool> NotificationGuard(bNotifyingNewSelection, bNewSelection);
			OnItemPicked.ExecuteIfBound(RequestedId);
		}
		if (PendingSelectionPulseId == RequestedId)
		{
			if (UEmployeeEquipmentCardWidget* Card = InventoryView->GetEntryWidgetFromItem<UEmployeeEquipmentCardWidget>(Value))
				TryConsumeSelectionPulse(Card);
		}
		return true;
	}
	return false;
}

UWidget* UEmployeeEquipmentListWidget::GetEntryWidgetForInstance(const FGuid& InstanceId) const
{
	if (!InventoryView || !InstanceId.IsValid()) return nullptr;
	for (UObject* Value : InventoryView->GetListItems())
	{
		const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Value);
		if (Entry && Entry->InstanceID == InstanceId)
		{
			return InventoryView->GetEntryWidgetFromItem<UEmployeeEquipmentCardWidget>(Value);
		}
	}
	return nullptr;
}

void UEmployeeEquipmentListWidget::HandleInventoryEntryGenerated(UUserWidget& EntryWidget)
{
	TryConsumeSelectionPulse(Cast<UEmployeeEquipmentCardWidget>(&EntryWidget));
}

void UEmployeeEquipmentListWidget::TryConsumeSelectionPulse(UEmployeeEquipmentCardWidget* Card)
{
	if (!Card || !InventoryView || !PendingSelectionPulseId.IsValid()
		|| PendingSelectionPulseId != SelectedInstanceId) return;
	const UEmployeeEquipmentItemObject* Item = Card->GetListItem<UEmployeeEquipmentItemObject>();
	if (!Item || Item->InstanceID != PendingSelectionPulseId) return;
	bool bCurrentEntry = false;
	for (UObject* Current : InventoryView->GetListItems())
	{
		if (Current == Item) { bCurrentEntry = true; break; }
	}
	if (!bCurrentEntry) return;
	Card->TriggerIconPulse();
	PendingSelectionPulseId.Invalidate();
}

void UEmployeeEquipmentListWidget::Refresh()
{
	if (!InventoryView) return;
	UGameInstance* GI = GetGameInstance();
	auto* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables) return;

	RebuildPartOptions();
	RebuildGradeOptions();
	TArray<UEmployeeEquipmentItemObject*> Entries;
	for (const auto& Pair : Equipment->GetState().Instances)
	{
		const auto* Def = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
		if (!Def) continue;
		// 개수는 필터와 무관하게 전부 센다 ― 선택박스가 「지금 안 보는 부위에 몇 개 있는지」를 말해야 한다.
		if (PartCounts.Num() > 0) ++PartCounts[0];
		const int32 PartIndex = static_cast<int32>(Def->EquipmentSlot) + 1;
		if (PartCounts.IsValidIndex(PartIndex)) ++PartCounts[PartIndex];
		if (!bAllSlots && Def->EquipmentSlot != CurrentSlot) continue;
		if (!GradeCounts.IsEmpty()) ++GradeCounts[0];
		const int32 GradeIndex = static_cast<int32>(Def->Grade) + 1;
		if (GradeCounts.IsValidIndex(GradeIndex)) ++GradeCounts[GradeIndex];
		if (CurrentGrade != EEmployeeEquipmentGrade::Count && Def->Grade != CurrentGrade) continue;
		// 아이콘·표시등급까지 채우는 건 CGEquipmentEntry 하나뿐이다 — 직접 조립하면
		// 여기서 Icon 을 빠뜨린 것처럼 창마다 같은 장비가 다르게 그려진다(2026-09-26).
		UEmployeeEquipmentItemObject* Entry = CGEquipmentEntry::Make(this, *Tables, *Def, &Pair.Value);
		const int32 Wearer = Equipment->GetInstanceEmployee(Pair.Key);
		Entry->bEquipped = Wearer != INDEX_NONE;
		Entry->bWornByOther = Wearer != INDEX_NONE && ContextEmployeeId != INDEX_NONE && Wearer != ContextEmployeeId;
		Entries.Add(Entry);
	}
	// 이 직원이 낀 것 → 안 낀 것 → 다른 직원이 낀 것, 묶음 안은 등급 내림차순 → 강화 내림차순.
	// EEmployeeEquipmentGrade 는 Common(0) → Ascendant(5) 다.
	Entries.Sort([](const UEmployeeEquipmentItemObject& A, const UEmployeeEquipmentItemObject& B)
	{
		const auto Group = [](const UEmployeeEquipmentItemObject& Entry) { return !Entry.bEquipped ? 1 : (Entry.bWornByOther ? 2 : 0); };
		if (Group(A) != Group(B)) return Group(A) < Group(B);
		if (A.Grade != B.Grade) return A.Grade > B.Grade;
		if (A.EnhancementLevel != B.EnhancementLevel) return A.EnhancementLevel > B.EnhancementLevel;
		return A.InstanceID < B.InstanceID;
	});

	TArray<UObject*> List;
	List.Reserve(Entries.Num());
	for (auto* Entry : Entries) List.Add(Entry);
	// 보유분만 ― 빈 칸은 그리지 않는다(사용자 지시 2026-09-25, 구 "빈 칸도 타일" 규칙 철회).
	InventoryView->SetListItems(List);
	const int32 TotalOwned = PartCounts.IsEmpty() ? 0 : PartCounts[0];
	const bool bFiltered = !bAllSlots || CurrentGrade != EEmployeeEquipmentGrade::Count;
	if (OwnedCountText)
	{
		OwnedCountText->SetText(bFiltered
			? FText::Format(LOCTEXT("FilteredOwnedCount", "{0} / {1}개"), FText::AsNumber(Entries.Num()), FText::AsNumber(TotalOwned))
			: FText::Format(LOCTEXT("TotalOwnedCount", "{0}개"), FText::AsNumber(TotalOwned)));
	}
	if (EmptyListText)
	{
		EmptyListText->SetText(bFiltered
			? LOCTEXT("FilteredEmpty", "조건에 맞는 장비가 없습니다.")
			: LOCTEXT("OwnedEmpty", "보유 장비가 없습니다."));
		EmptyListText->SetVisibility(Entries.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 선택박스가 이름과 개수를 둘 다 들고 있어 아래 밴드가 중복이었다 ― 그 줄은 목록에 돌려줬다.
	if (PartCombo)
	{
		const int32 Wanted = bAllSlots ? 0 : static_cast<int32>(CurrentSlot) + 1;
		bUpdatingCombo = true;
		// Refresh popup counts and keep the selected label current.
		PartCombo->ClearSelection();
		PartCombo->SetSelectedIndex(Wanted);
		PartCombo->RefreshOptions();
		bUpdatingCombo = false;
	}
	if (GradeCombo)
	{
		const int32 Wanted = CurrentGrade == EEmployeeEquipmentGrade::Count ? 0 : static_cast<int32>(CurrentGrade) + 1;
		bUpdatingCombo = true;
		GradeCombo->ClearSelection();
		GradeCombo->SetSelectedIndex(Wanted);
		GradeCombo->RefreshOptions();
		bUpdatingCombo = false;
	}

	// 목록 객체는 매번 새로 생성되므로 보유 인스턴스 ID로 선택 표시와 상세를 함께 복원한다.
	// 복원 실패는 "선택 없음"이다 ― 첫 항목을 대신 골라 델리게이트를 쏘면 고르지도 않은 상세가 열린다
	// (워크스페이스 선례: 선택이 목록 밖이면 ClearInspectionSelection 만 한다).
	if (!SelectItem(SelectedInstanceId))
	{
		ClearSelection();
		OnItemPicked.ExecuteIfBound(FGuid());
	}
	OnListRefreshed.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
