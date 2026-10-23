#include "UI/Element/Employee/EquipmentDetailWindowWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Employee/EquipmentDeltaRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Engine/GameInstance.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "CommonTextBlock.h"

void UEquipmentDetailWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (DetailCloseButton) DetailCloseButton->OnCloseClicked.AddDynamic(this, &ThisClass::HandleClose);
	if (DetailPrimaryButton) DetailPrimaryButton->OnClicked().AddUObject(this, &ThisClass::HandlePrimary);
	if (DetailSecondaryButton) DetailSecondaryButton->OnClicked().AddUObject(this, &ThisClass::HandleSecondary);
}

void UEquipmentDetailWindowWidget::NativeDestruct()
{
	if (DetailCloseButton) DetailCloseButton->OnCloseClicked.RemoveAll(this);
	for (UButtonWidget* Button : {DetailPrimaryButton.Get(), DetailSecondaryButton.Get()})
		if (Button) Button->OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

UWidget* UEquipmentDetailWindowWidget::GetPrimaryButtonWidget() const
{
	return DetailPrimaryButton.Get();
}

void UEquipmentDetailWindowWidget::SetDockOffset(float RightOffset)
{
	bStandalonePresentation = true;
	// 세로 선 앵커에서 Left 는 위치 X, Right 는 폭이다.
	if (UCanvasPanelSlot* DockSlot = Cast<UCanvasPanelSlot>(DetailDock ? DetailDock->Slot.Get() : nullptr))
	{
		DockSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		DockSlot->SetAlignment(FVector2D(1.f, 0.f));
		DockSlot->SetOffsets(FMargin(-RightOffset, 40.f, 700.f, 40.f));
	}
	if (USizeBox* DockSize = Cast<USizeBox>(DetailDock))
	{
		DockSize->ClearWidthOverride();
		DockSize->SetMinDesiredWidth(660.f);
	}
	for (const FName ChromeName : { FName(TEXT("DetailHeaderSize")), FName(TEXT("DetailShellGrid")) })
	{
		if (UWidget* Chrome = GetWidgetFromName(ChromeName)) Chrome->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (UBorder* Shell = Cast<UBorder>(GetWidgetFromName(TEXT("DetailShell"))))
	{
		Shell->SetPadding(FMargin(36.f, 26.f, 36.f, 34.f));
	}
	if (UWidget* Plate = GetWidgetFromName(TEXT("DetailPlateWrap")))
	{
		if (UVerticalBoxSlot* PlateSlot = Cast<UVerticalBoxSlot>(Plate->Slot)) PlateSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	}
}

void UEquipmentDetailWindowWidget::Configure(const FEquipmentDetailContent& Content)
{
	if (UScrollBox* BodyScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("DetailBodyScroll")))) BodyScroll->ScrollToStart();
	const auto Line = [](UCommonTextBlock* Text, const FText& Value)
	{
		if (!Text) return;
		Text->SetText(Value);
		Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	};

	Line(SelectedNameText, Content.Name);
	Line(SelectedGradeText, Content.GradeName);
	Line(SelectedPartText, Content.PartName);
	if (bStandalonePresentation) Line(SelectedEnhanceText, Content.EnhanceText);
	Line(SelectedStateText, Content.State);
	if (SelectedStateText)
	{
		if (!bStateInkCaptured)
		{
			StateInkDefault = SelectedStateText->GetColorAndOpacity();
			bStateInkCaptured = true;
		}
		// 다른 직원이 낀 장비는 상태를 경고색으로 ― 「가져와 장착」이 그 직원 칸을 비운다는 것을 먼저 보게
		SelectedStateText->SetColorAndOpacity(Content.bHeldByOther
			? FSlateColor(FLinearColor::FromSRGBColor(FColor(0xC2, 0x41, 0x0C))) : StateInkDefault);
	}
	Line(BodyLabelText, Content.BodyLabel);
	Line(NoticeText, Content.Notice);
	Line(ItemUniqueEffectsText, Content.ItemUniqueEffects);
	if (ComparisonSection) ComparisonSection->SetVisibility(Content.bShowComparison ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (ItemEffectsSection) ItemEffectsSection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (NoticePanel) NoticePanel->SetVisibility(Content.Notice.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	const auto ApplyIcon = [this](UImage* Image, UTexture2D* Texture, const FEmployeeEquipmentGradeRow* Grade,
		EEmployeeEquipmentIconContext Context)
	{
		if (!Image) return;
		TObjectPtr<UMaterialInstanceDynamic> Material = Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject());
		CGEquipmentIcon::Apply(Image, CGEquipmentIcon::Resolve(this, Texture, Grade, Context, Material));
	};
	ApplyIcon(SelectedIcon, Content.Icon, Content.VisualGradeRow, EEmployeeEquipmentIconContext::Focused);
	CGEquipmentIcon::ApplyGradeFrame(SelectedGradeFrame, Content.GradeRow, Content.GradeColor);

	// 교체 스트립 — 왼쪽은 이 부위에 지금 낀 것, 오른쪽은 고른 것. 이미 장착 중이면 비교 상대가 없다.
	if (SwapRow) SwapRow->SetVisibility(Content.bShowComparison && Content.bShowSwap ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Line(CurrentLabelText, Content.CurrentLabel);
	Line(CurrentNameText, Content.CurrentName);
	Line(AfterLabelText, Content.AfterLabel);
	Line(AfterNameText, Content.AfterName);
	ApplyIcon(CurrentIcon, Content.CurrentIcon, Content.CurrentVisualGradeRow, EEmployeeEquipmentIconContext::Idle);
	ApplyIcon(AfterIcon, Content.Icon, Content.VisualGradeRow, EEmployeeEquipmentIconContext::Focused);

	// 별은 등급 칸 수만큼 채운다 — 이름 없이 등급을 한눈에 읽는 레퍼런스 문법.
	// 각 별은 크기 고정용 SizeBox 안에 들어 있으므로 한 겹 파고들어야 한다.
	if (UPanelWidget* Row = Cast<UPanelWidget>(StarRow))
	{
		for (int32 Index = 0; Index < Row->GetChildrenCount(); ++Index)
		{
			UWidget* Child = Row->GetChildAt(Index);
			UImage* Star = Cast<UImage>(Child);
			if (!Star)
			{
				if (UContentWidget* Box = Cast<UContentWidget>(Child)) Star = Cast<UImage>(Box->GetContent());
			}
			if (Star) Star->SetColorAndOpacity(Index < Content.Stars
				? FLinearColor(1.f, 1.f, 1.f, 1.f) : FLinearColor(1.f, 1.f, 1.f, .16f));
		}
	}

	RebuildRows(Content);

	UButtonWidget* Buttons[] = {DetailPrimaryButton, DetailSecondaryButton};
	const FText Labels[] = {Content.Primary, Content.Secondary};
	const bool Enabled[] = {Content.bPrimaryEnabled, Content.bSecondaryEnabled};
	// 스타일·글자 크기·잉크·외곽선은 WBP 인스턴스가 들고 있다 — 여기서 덮으면 에셋 튜닝이 매 갱신마다 지워진다.
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		UButtonWidget* Button = Buttons[Index];
		if (!Button) continue;
		Button->SetButtonText(Labels[Index]);
		Button->SetIsEnabled(Enabled[Index]);
		Button->SetVisibility(Labels[Index].IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UEquipmentDetailWindowWidget::TriggerSelectedIconPulse()
{
	if (SelectedIcon)
		CGEquipmentIcon::TriggerPulse(Cast<UMaterialInstanceDynamic>(SelectedIcon->GetBrush().GetResourceObject()));
}

void UEquipmentDetailWindowWidget::RebuildRows(const FEquipmentDetailContent& Content)
{
	UTableManagerSubsystem* TableManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableManager) return;

	const TSubclassOf<UUserWidget> RowClass = TableManager->GetWidgetClass(EWidgetType::EquipmentDeltaRow);
	const auto FillRows = [this, RowClass](UPanelWidget* RowBox, const TArray<FEquipmentDeltaRow>& Rows)
	{
		if (!RowBox) return;
		RowBox->ClearChildren();
		if (RowClass)
		{
			for (const FEquipmentDeltaRow& Row : Rows)
			{
				if (auto* Entry = CreateWidget<UEquipmentDeltaRowWidget>(this, RowClass))
				{
					Entry->Configure(Row);
					// 행 간격은 행이 아니라 부모가 준다 — 첫 행 위에는 띄우지 않는다.
					if (auto* RowSlot = Cast<UVerticalBoxSlot>(RowBox->AddChild(Entry)))
						RowSlot->SetPadding(FMargin(0.f, RowBox->GetChildrenCount() > 1 ? 10.f : 0.f, 0.f, 0.f));
				}
			}
		}
		RowBox->SetVisibility(RowBox->GetChildrenCount() > 0
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	};
	FillRows(ItemEffectRowsBox, Content.ItemEffectRows);
	FillRows(DeltaRowsBox, Content.Rows);
	if (ItemNoEffectsText)
	{
		ItemNoEffectsText->SetText(Content.ItemNoEffectsText);
		ItemNoEffectsText->SetVisibility(Content.ItemNoEffectsText.IsEmpty()
			|| (ItemEffectRowsBox && ItemEffectRowsBox->GetChildrenCount() > 0)
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// 변화 행이 하나도 없을 때만 대체 문구를 세운다 — 표와 문구가 같이 보이면 안 된다.
	const bool bHasRows = DeltaRowsBox && DeltaRowsBox->GetChildrenCount() > 0;
	if (NoChangeText)
	{
		NoChangeText->SetText(Content.NoChangeText);
		NoChangeText->SetVisibility(!bHasRows && !Content.NoChangeText.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (UniqueChipBox)
	{
		UniqueChipBox->ClearChildren();
		const TSubclassOf<UUserWidget> ChipClass = TableManager->GetWidgetClass(EWidgetType::EquipmentUniqueChip);
		if (ChipClass)
		{
			for (const FEquipmentUniqueChange& Change : Content.Uniques)
			{
				if (auto* Chip = CreateWidget<UEquipmentUniqueChipWidget>(this, ChipClass))
				{
					Chip->Configure(Change);
					UniqueChipBox->AddChild(Chip);
				}
			}
		}
		UniqueChipBox->SetVisibility(UniqueChipBox->GetChildrenCount() > 0
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UEquipmentDetailWindowWidget::HandlePrimary() { OnPrimary.ExecuteIfBound(); }
void UEquipmentDetailWindowWidget::HandleSecondary() { OnSecondary.ExecuteIfBound(); }
void UEquipmentDetailWindowWidget::HandleClose() { OnClose.ExecuteIfBound(); }
