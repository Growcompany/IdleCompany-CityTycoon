// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "CommonTextBlock.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Blueprint/UserWidget.h"
#include "Global/GlobalUtilFunctions.h"

void UItemCardSlotWidget::SetItem(UTexture2D* InIcon, int32 InQuantity)
{
	SetIcon(InIcon);
	SetQuantity(InQuantity);
}

void UItemCardSlotWidget::SetIcon(UTexture2D* InIcon)
{
	if (UIE_ItemCard) UIE_ItemCard->SetIcon(InIcon);
}

void UItemCardSlotWidget::SetQuantity(int32 InQuantity)
{
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(InQuantity));
	}
}

void UItemCardSlotWidget::SetLabel(const FText& InLabel)
{
	if (QuantityText)
	{
		QuantityText->SetText(InLabel);
	}
}

void UItemCardSlotWidget::SetItemID(FName InItemID)
{
	if (UIE_ItemCard) UIE_ItemCard->SetItemID(InItemID);
}

void UItemCardSlotWidget::SetLocked(bool bInLocked)
{
	if (UIE_ItemCard) UIE_ItemCard->SetLocked(bInLocked);
}

void UItemCardSlotWidget::SetInsufficient(bool bInInsufficient)
{
	if (UIE_ItemCard) UIE_ItemCard->SetInsufficient(bInInsufficient);
}

void UItemCardSlotWidget::SetSelected(bool bInSelected)
{
	if (UIE_ItemCard) UIE_ItemCard->SetSelected(bInSelected);
}

void UItemCardSlotWidget::SetIconSize(FVector2D NewSize)
{
	if (UIE_ItemCard) UIE_ItemCard->SetIconSize(NewSize);
}

void UItemCardSlotWidget::SetCardSize(FVector2D InCardSize, FVector2D InIconSize)
{
	if (UIE_ItemCard) UIE_ItemCard->SetCardSize(InCardSize, InIconSize);
}

void UItemCardSlotWidget::SetLabelFontSize(int32 InSize)
{
	if (!QuantityText || InSize <= 0) { return; }
	FSlateFontInfo Font = QuantityText->GetFont();
	Font.Size = InSize;
	QuantityText->SetFont(Font);
}

void UItemCardSlotWidget::SetTooltipInfo(const FText& InName, const FText& InDesc, UTexture2D* InIcon, int32 InPrice, int32 InZOrder)
{
	TooltipName = InName;
	TooltipDesc = InDesc;
	TooltipIcon = InIcon;
	TooltipPrice = InPrice;
	TooltipZOrder = InZOrder;
	bTooltipEnabled = true;
}

void UItemCardSlotWidget::ShowTooltip()
{
	if (!bTooltipEnabled) return;
	// 표시할 내용이 전혀 없으면(이름/설명/단가 모두 빔) 빈 팝오버 대신 스킵.
	if (TooltipName.IsEmpty() && TooltipDesc.IsEmpty() && TooltipPrice <= 0) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	// 인스턴스 1개 재사용 (없으면 1회 생성). ShowAt 이 viewport add/위치/자동 dismiss 처리.
	if (!ActiveTooltip)
	{
		const TSubclassOf<UUserWidget> TooltipClass = TableMgr->GetWidgetClass(EWidgetType::ItemTooltip);
		if (!TooltipClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemCardSlot] EWidgetType::ItemTooltip 미등록"));
			return;
		}
		ActiveTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass);
	}
	if (!ActiveTooltip) return;

	// 앵커 = 유저가 클릭/탭한 지점 그대로 아래로 드롭 (UResourceWidget 과 동일 좌표계).
	const FVector2D AbsPos = UGlobalUtilFunctions::GetPointerAbsolutePosition();
	ActiveTooltip->ShowAt(AbsPos, TooltipName, TooltipIcon, TooltipDesc, TooltipPrice, 2.5f, ETooltipAnchor::BelowAnchor, TooltipZOrder);
}

void UItemCardSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 카드의 OnItemCardClicked 를 Slot 의 OnItemCardClicked 로 forward
	if (UIE_ItemCard)
	{
		TWeakObjectPtr<UItemCardSlotWidget> WeakThis(this);
		UIE_ItemCard->OnItemCardClicked.BindLambda([WeakThis](FName InID)
		{
			if (UItemCardSlotWidget* Strong = WeakThis.Get())
			{
				Strong->ShowTooltip();                       // 인라인 툴팁 (SetTooltipInfo 로 활성화된 경우)
				Strong->OnItemCardClicked.ExecuteIfBound(InID);
			}
		});
	}
}

void UItemCardSlotWidget::NativeDestruct()
{
	if (UIE_ItemCard)
	{
		UIE_ItemCard->OnItemCardClicked.Unbind();
	}
	if (ActiveTooltip)
	{
		ActiveTooltip->RemoveFromParent();
		ActiveTooltip = nullptr;
	}
	Super::NativeDestruct();
}
