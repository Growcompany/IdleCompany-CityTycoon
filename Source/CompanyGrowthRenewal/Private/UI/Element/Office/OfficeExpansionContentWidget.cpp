#include "UI/Element/Office/OfficeExpansionContentWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Enum/ResourceType.h"

void UOfficeExpansionContentWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (bHasPreview)
    {
        SetExpansionPreview(PreviewCurrent, PreviewNext, PreviewMaximum, PreviewCost);
    }
}

bool UOfficeExpansionContentWidget::SetExpansionPreview(int32 Current, int32 Next, int32 Maximum, int64 Cost)
{
    UImage* IconImage = CostResource ? Cast<UImage>(CostResource->GetWidgetFromName(TEXT("ResourceImage"))) : nullptr;
    if (!CurrentValueText || !NextValueText || !MaximumText || !CostResource
        || !IconImage || Current < 0 || Next <= Current || Next > Maximum || Cost <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[OfficeExpansionContent] Invalid preview data or missing UIE_OfficeExpansionContent binding"));
        return false;
    }

    FSlateBrush IconBrush = IconImage->GetBrush();
    IconBrush.SetResourceObject(nullptr);
    IconImage->SetBrush(IconBrush);
    CostResource->SetResourceType(EResourceType::Diamond, false);
    if (!IconImage->GetBrush().GetResourceObject())
    {
        UE_LOG(LogTemp, Error, TEXT("[OfficeExpansionContent] Diamond icon is missing from resource data"));
        return false;
    }

    PreviewCurrent = Current;
    PreviewNext = Next;
    PreviewMaximum = Maximum;
    PreviewCost = Cost;
    bHasPreview = true;
    CurrentValueText->SetText(FText::Format(
        NSLOCTEXT("OfficeExpansion", "CurrentCount", "{0}칸"), FText::AsNumber(Current)));
    NextValueText->SetText(FText::Format(
        NSLOCTEXT("OfficeExpansion", "NextCount", "{0}칸"), FText::AsNumber(Next)));
    MaximumText->SetText(FText::Format(
        NSLOCTEXT("OfficeExpansion", "ConfirmMaximum", "최대 {0}칸"), FText::AsNumber(Maximum)));
    CostResource->SetValueSilent(Cost);
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    return true;
}

void UOfficeExpansionContentWidget::ClearPreview()
{
    if (!bHasPreview && GetVisibility() == ESlateVisibility::Collapsed) return;
    bHasPreview = false;
    PreviewCurrent = 0;
    PreviewNext = 0;
    PreviewMaximum = 0;
    PreviewCost = 0;
    if (CurrentValueText) CurrentValueText->SetText(FText::GetEmpty());
    if (NextValueText) NextValueText->SetText(FText::GetEmpty());
    if (MaximumText) MaximumText->SetText(FText::GetEmpty());
    if (CostResource) CostResource->SetValueSilent(0);
    SetVisibility(ESlateVisibility::Collapsed);
}
