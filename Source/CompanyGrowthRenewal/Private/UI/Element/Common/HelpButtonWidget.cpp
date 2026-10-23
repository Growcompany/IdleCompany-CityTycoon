#include "UI/Element/Common/HelpButtonWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Engine/GameViewportClient.h"
#include "CommonActivatableWidget.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/HelpTable.h"
#include "UI/Element/Common/GuideTooltipWidget.h"
#include "UI/Element/Common/HelpTableProvider.h"
#include "UI/Element/Common/TouchTargetUtils.h"
#include "UI/HUD/GuideTooltipPlacement.h"
#include "UI/Panel/HelpTablePanelWidget.h"
#include "UI/UIBase.h"

void UHelpButtonWidget::ResolveMode()
{
	UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const FHelpTable* Row = TableMgr ? TableMgr->GetHelpRow(HelpKey) : nullptr;

	LinkWidget = Row ? Row->LinkWidget : EWidgetType::None;
	Caption = Row ? Row->Body : FText::GetEmpty();

	// 아우터 체인을 타고 올라가며 표를 줄 수 있는 소유 패널을 찾는다.
	// GetParent() 는 패널 트리 안의 부모일 뿐이라 UserWidget 경계를 못 넘는다.
	// 루프인 이유 = "?" 가 헤더 부품 WBP 안에 중첩되면 첫 UserWidget 은 패널이 아니라 헤더다
	Provider = nullptr;
	FHelpTableInfo Probe;
	for (UUserWidget* Outer = GetTypedOuter<UUserWidget>(); Outer; Outer = Outer->GetTypedOuter<UUserWidget>())
	{
		if (IHelpTableProvider* Candidate = Cast<IHelpTableProvider>(Outer))
		{
			// 인터페이스를 구현했다는 것과 지금 이 키에 줄 표가 있다는 것은 별개다 ―
			// 건물이 아직 안 물린 패널은 false 를 돌려주고, 그때는 글 한 장으로 내려간다
			if (Candidate->GetHelpTableInfo(HelpKey, Probe)) { Provider = Outer; }
			break;
		}
	}

	Mode = ResolveHelpMode(Provider != nullptr, LinkWidget != EWidgetType::None,
		Row && !Row->Body.IsEmpty());
}

void UHelpButtonWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// BindWidget 이 required 라 WBP 경로엔 항상 있다 ― 베이스를 직접 띄우는 하네스만 여기로 온다
	if (!RootSizeBox || !GlyphSize)
	{
		return;
	}

	UITouchTarget::ApplyGlyphAndTouchExtent(RootSizeBox, GlyphSize, HelpBtn, GlyphExtent, TouchExtent);
}

void UHelpButtonWidget::SetHelpKey(FName InKey)
{
	if (HelpKey == InKey) { return; }
	HelpKey = InKey;
	// 열려 있던 말풍선은 옛 키의 답이다
	HidePopover();
	ApplyHelpKey();
}

void UHelpButtonWidget::ApplyHelpKey()
{
	ResolveMode();
	if (HelpBtn)
	{
		HelpBtn->OnClicked.RemoveDynamic(this, &UHelpButtonWidget::HandleClicked);
	}

	if (Mode == EHelpMode::None)
	{
		// 동작 없는 "?" 를 화면에 내보내지 않는다 ― 이 부품이 13개 패널에서 죽어 있던 사고의 재발 방지
		SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Warning, TEXT("[Help] '%s' 내용 없음 ― 버튼 숨김"), *HelpKey.ToString());
		return;
	}

	SetVisibility(AuthoredVisibility);
	if (HelpBtn)
	{
		HelpBtn->OnClicked.AddDynamic(this, &UHelpButtonWidget::HandleClicked);
	}
}

void UHelpButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bAuthoredVisibilityCaptured)
	{
		bAuthoredVisibilityCaptured = true;
		AuthoredVisibility = GetVisibility();
	}
	ApplyHelpKey();
}

void UHelpButtonWidget::NativeDestruct()
{
	if (HelpBtn)
	{
		HelpBtn->OnClicked.RemoveDynamic(this, &UHelpButtonWidget::HandleClicked);
	}
	HidePopover();
	Super::NativeDestruct();
}

void UHelpButtonWidget::HandleClicked()
{
	OpenHelp();
}

void UHelpButtonWidget::OpenHelp()
{
	// 열려 있는 말풍선을 닫는 탭이면 재판정하지 않는다 ― 토글 대상이 바뀌면 안 닫힌다
	if (!Popover) { ResolveMode(); }

	switch (Mode)
	{
	case EHelpMode::Table:
		ShowHelpTable();
		return;

	case EHelpMode::Panel:
		OpenLinkedPanel();
		return;

	case EHelpMode::Prose:
		// 말풍선은 토글 ― 열려 있으면 "?" 재탭으로 닫힌다
		if (Popover) { HidePopover(); } else { ShowPopover(); }
		return;

	default:
		return;
	}
}

void UHelpButtonWidget::ShowHelpTable()
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	TSubclassOf<UUserWidget> TableClass = TableMgr
		? TableMgr->GetWidgetClass(EWidgetType::HelpTablePanel) : nullptr;
	if (!UIBase || !TableClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Help] '%s' 표 창 자원 없음 (UIBase %d / 클래스 %d)"),
			*HelpKey.ToString(), UIBase != nullptr, TableClass != nullptr);
		return;
	}

	UCommonActivatableWidget* Pushed = UIBase->PushPromptClass(
		TSubclassOf<UCommonActivatableWidget>(TableClass.Get()));
	if (UHelpTablePanelWidget* TablePanel = Cast<UHelpTablePanelWidget>(Pushed))
	{
		TablePanel->Configure(Provider, HelpKey, Caption);
	}
}

void UHelpButtonWidget::OpenLinkedPanel()
{
	// 단계 C (계획 T-C2). 단계 A 의 DT_Help 는 LinkWidget 이 전부 None 이라 여기 못 온다
	UE_LOG(LogTemp, Warning, TEXT("[Help] '%s' 연결 패널 미구현 ― 단계 C"), *HelpKey.ToString());
}

void UHelpButtonWidget::ShowPopover()
{
	UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const FHelpTable* Row = TableMgr ? TableMgr->GetHelpRow(HelpKey) : nullptr;
	TSubclassOf<UUserWidget> TipClass = TableMgr
		? TableMgr->GetWidgetClass(EWidgetType::GuideTooltip) : nullptr;
	if (!Row || !TipClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Help] '%s' 팝오버 자원 없음 (행 %d / 클래스 %d)"),
			*HelpKey.ToString(), Row != nullptr, TipClass != nullptr);
		return;
	}

	// 바깥 탭 수신기를 말풍선보다 먼저(아래 ZOrder) 깐다 ― 딤 없이 입력만 받는다
	if (UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		DismissCatcher = NewObject<UButton>(this);
		FButtonStyle ClearStyle;
		ClearStyle.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
		ClearStyle.Hovered = ClearStyle.Normal;
		ClearStyle.Pressed = ClearStyle.Normal;
		DismissCatcher->SetStyle(ClearStyle);
		DismissCatcher->OnClicked.AddDynamic(this, &UHelpButtonWidget::HandleDismissClicked);
		// UButton 에는 AddToViewport 가 없다(UUserWidget 전용). 뷰포트 오버레이 슬롯은 Fill 이라 전면을 덮는다
		ViewportClient->AddViewportWidgetContent(DismissCatcher->TakeWidget(), PopoverZOrder - 1);
	}

	Popover = CreateWidget<UGuideTooltipWidget>(GetOwningPlayer(), TipClass);
	if (!Popover)
	{
		HidePopover();
		return;
	}
	Popover->SetContent(Row->Eyebrow, Row->Body);
	Popover->SetVisibility(ESlateVisibility::HitTestInvisible);
	Popover->AddToViewport(PopoverZOrder);

	// "?" 자신을 앵커로 삼는다. AbsoluteToLocal 패턴 ― ScreenPos÷ViewportScale 은 SafeZone/DPI 를 못 잡는다
	const FGeometry& BtnGeo = GetCachedGeometry();
	const FVector2D AnchorSize = BtnGeo.GetLocalSize();
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D AnchorCenter =
		ViewportGeo.AbsoluteToLocal(BtnGeo.LocalToAbsolute(AnchorSize * 0.5f));

	Popover->ForceLayoutPrepass();
	const FVector2D TipSize = Popover->GetDesiredSize();
	const FVector2D ViewportSize = ViewportGeo.GetLocalSize();

	// 꼬리는 위 ― "?" 는 헤더 상단에 있으므로 말풍선이 그 아래로 내려온다
	const FVector2D Pos = ComputeGuideTooltipPosition(
		AnchorCenter, AnchorSize, TipSize, EGuideTooltipDir::Up,
		ViewportSize, PopoverSafeMargin, PopoverGap);
	Popover->SetPositionInViewport(Pos, /*bRemoveDPIScale=*/false);

	// 꼬리 오프셋은 클램프와 한 쌍이다 ― 위에서 말풍선을 밀어 넣은 만큼 꼬리를 "?" 쪽으로 되돌리지
	// 않으면 엉뚱한 데를 가리킨다. Dir=Up 은 가로축이므로 X 성분을 넘긴다
	Popover->SetTail(EGuideTooltipDir::Up,
		ComputeGuideTailOffset(AnchorCenter.X, Pos.X, TipSize.X, PopoverTailSize, PopoverTailCornerInset));
}

bool UHelpButtonWidget::GetPopoverViewportRect(FVector2D& OutPosition, FVector2D& OutSize, FVector2D& OutViewport) const
{
	if (!Popover) { return false; }
	const FGeometry& TipGeo = Popover->GetCachedGeometry();
	// 위치와 뷰포트 크기를 같은 지오메트리에서 뽑아야 서로 같은 좌표계다
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(const_cast<UHelpButtonWidget*>(this));
	OutPosition = ViewportGeo.AbsoluteToLocal(TipGeo.LocalToAbsolute(FVector2D::ZeroVector));
	OutSize = TipGeo.GetLocalSize();
	OutViewport = ViewportGeo.GetLocalSize();
	return true;
}

void UHelpButtonWidget::HidePopover()
{
	if (DismissCatcher)
	{
		DismissCatcher->OnClicked.RemoveDynamic(this, &UHelpButtonWidget::HandleDismissClicked);
		if (UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			ViewportClient->RemoveViewportWidgetContent(DismissCatcher->TakeWidget());
		}
		DismissCatcher = nullptr;
	}
	if (Popover)
	{
		Popover->RemoveFromParent();
		Popover = nullptr;
	}
}

void UHelpButtonWidget::HandleDismissClicked()
{
	HidePopover();
}
