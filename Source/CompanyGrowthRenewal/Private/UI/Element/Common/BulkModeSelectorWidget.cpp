// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Common/BulkModeSelectorWidget.h"

#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateTypes.h"
#include "Engine/Font.h"

namespace
{
	// === 비노브 상수 (노브 승격분은 헤더 UPROPERTY — WBP Class Defaults 가 SOT) ===
	constexpr float CheckCornerRadius = 8.f;
	constexpr float CheckOutlineWidth = 2.f;
	constexpr float PlateOutlineWidth = 2.f;
	constexpr float BoxToLabelGap = 12.f;
	constexpr float RowVerticalPadding = 6.f;

	// FSlateBrush DrawAs=RoundedBox + OutlineSettings 직접 구성 (FSlateRoundedBoxBrush 동치, 의존 최소)
	FSlateBrush MakeRoundedBox(const FLinearColor& Fill, float Radius, const FLinearColor& OutlineColor, float OutlineWidth, const FVector2D& InSize)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
		Brush.OutlineSettings.Width = OutlineWidth;
		Brush.ImageSize = InSize;
		return Brush;
	}

	// 체크 1행 구성 (박스 + 간격 + 라벨), VerticalBox 에 상하 패딩 6 으로 추가
	UCheckBox* BuildCheckRow(UWidgetTree* Tree, UVerticalBox* Stack, const TCHAR* CheckName, const FString& InLabel,
		const FSlateBrush& CheckedBrush, const FSlateBrush& UncheckedBrush, UFont* LabelFont,
		const FLinearColor& InLabelInk, int32 InLabelFontSize)
	{
		if (!Tree || !Stack)
		{
			return nullptr;
		}

		UCheckBox* Check = Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), CheckName);
		if (!Check)
		{
			return nullptr;
		}

		FCheckBoxStyle Style;
		Style.CheckBoxType = ESlateCheckBoxType::CheckBox;
		Style.CheckedImage = CheckedBrush;
		Style.CheckedHoveredImage = CheckedBrush;
		Style.CheckedPressedImage = CheckedBrush;
		Style.UncheckedImage = UncheckedBrush;
		Style.UncheckedHoveredImage = UncheckedBrush;
		Style.UncheckedPressedImage = UncheckedBrush;
		Style.UndeterminedImage = UncheckedBrush;
		Style.UndeterminedHoveredImage = UncheckedBrush;
		Style.UndeterminedPressedImage = UncheckedBrush;
		Style.Padding = FMargin(BoxToLabelGap, 0.f, 0.f, 0.f);       // 박스↔라벨 간격
		Style.ForegroundColor = FSlateColor(FLinearColor::White);   // 라벨 색은 CommonTextBlock 이 직접 지정 → 스타일 틴트 방지
		Check->SetWidgetStyle(Style);                               // 직접 WidgetStyle 대입은 deprecated(5.1) → 세터

		UCommonTextBlock* Label = Tree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), *FString::Printf(TEXT("%s_Label"), CheckName));
		if (Label)
		{
			Label->SetText(FText::FromString(InLabel));
			if (LabelFont)
			{
				Label->SetFont(FSlateFontInfo(LabelFont, InLabelFontSize));
			}
			Label->SetColorAndOpacity(FSlateColor(InLabelInk));
			Check->SetContent(Label);
		}

		if (UVerticalBoxSlot* BoxSlot = Stack->AddChildToVerticalBox(Check))
		{
			BoxSlot->SetPadding(FMargin(0.f, RowVerticalPadding, 0.f, RowVerticalPadding));
			BoxSlot->SetHorizontalAlignment(HAlign_Left);
		}
		return Check;
	}
}

TSharedRef<SWidget> UBulkModeSelectorWidget::RebuildWidget()
{
	// 빈 트리 전제 — 디자이너가 트리를 넣었으면 BindWidgetOptional 로 그걸 사용.
	// 팩토리 생성 WBP 의 기본 빈 CanvasPanel 루트는 빈 트리로 간주 (자식 있으면 디자이너 저작으로 존중)
	if (WidgetTree)
	{
		bool bTreatAsEmpty = (WidgetTree->RootWidget == nullptr);
		if (!bTreatAsEmpty)
		{
			if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
			{
				bTreatAsEmpty = (RootCanvas->GetChildrenCount() == 0);
			}
		}
		if (bTreatAsEmpty)
		{
			BuildSelfTree();
		}
	}
	return Super::RebuildWidget();
}

void UBulkModeSelectorWidget::BuildSelfTree()
{
	if (!WidgetTree)
	{
		return;
	}

	// 플레이트(루트) — RoundedBox + 헤어라인. 색/반경/패딩은 노브(WBP Class Defaults) 가 SOT.
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BulkModePlate"));
	Plate->SetBrush(MakeRoundedBox(PlateFill, PlateCornerRadius, PlateHairline, PlateOutlineWidth, FVector2D::ZeroVector));
	// UBorder 이중 패딩 함정: 콘텐츠 패딩은 Border Padding 한 곳에서만 (BorderSlot 에 별도 패딩 주지 않음)
	Plate->SetPadding(PlateContentPadding);
	WidgetTree->RootWidget = Plate;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BulkModeStack"));
	Plate->SetContent(Stack);

	// NEXON Bold — TSoftObjectPtr + LoadSynchronous (LoadObject 문자열 금지, 쿠킹 안전)
	TSoftObjectPtr<UFont> FontPath = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font")));
	UFont* LabelFont = FontPath.LoadSynchronous();

	const FVector2D BoxSize(CheckBoxSize, CheckBoxSize);
	const FSlateBrush CheckedBrush = MakeRoundedBox(CheckFillSelected, CheckCornerRadius, FLinearColor(0.f, 0.f, 0.f, 0.f), 0.f, BoxSize);
	const FSlateBrush UncheckedBrush = MakeRoundedBox(CheckWellUnselected, CheckCornerRadius, CheckHairline, CheckOutlineWidth, BoxSize);

	BulkCheck_x1  = BuildCheckRow(WidgetTree, Stack, TEXT("BulkCheck_x1"),  TEXT("x1"),  CheckedBrush, UncheckedBrush, LabelFont, LabelInk, LabelFontSize);
	BulkCheck_x10 = BuildCheckRow(WidgetTree, Stack, TEXT("BulkCheck_x10"), TEXT("x10"), CheckedBrush, UncheckedBrush, LabelFont, LabelInk, LabelFontSize);
	BulkCheck_x50 = BuildCheckRow(WidgetTree, Stack, TEXT("BulkCheck_x50"), TEXT("x50"), CheckedBrush, UncheckedBrush, LabelFont, LabelInk, LabelFontSize);
}

void UBulkModeSelectorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindChecks();
	SyncVisuals();
}

void UBulkModeSelectorWidget::NativeDestruct()
{
	UnbindChecks();
	Super::NativeDestruct();
}

void UBulkModeSelectorWidget::BindChecks()
{
	if (bChecksBound)
	{
		return;
	}
	if (BulkCheck_x1)  { BulkCheck_x1->OnCheckStateChanged.AddDynamic(this, &UBulkModeSelectorWidget::OnCheck_x1); }
	if (BulkCheck_x10) { BulkCheck_x10->OnCheckStateChanged.AddDynamic(this, &UBulkModeSelectorWidget::OnCheck_x10); }
	if (BulkCheck_x50) { BulkCheck_x50->OnCheckStateChanged.AddDynamic(this, &UBulkModeSelectorWidget::OnCheck_x50); }
	if (!BulkCheck_x1 && !BulkCheck_x10 && !BulkCheck_x50)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BulkModeSelector] 체크 미배치 — 자가 트리/디자이너 트리 둘 다 없음 (WBP 확인)"));
	}
	bChecksBound = true;
}

void UBulkModeSelectorWidget::UnbindChecks()
{
	if (!bChecksBound)
	{
		return;
	}
	if (BulkCheck_x1)  { BulkCheck_x1->OnCheckStateChanged.RemoveDynamic(this, &UBulkModeSelectorWidget::OnCheck_x1); }
	if (BulkCheck_x10) { BulkCheck_x10->OnCheckStateChanged.RemoveDynamic(this, &UBulkModeSelectorWidget::OnCheck_x10); }
	if (BulkCheck_x50) { BulkCheck_x50->OnCheckStateChanged.RemoveDynamic(this, &UBulkModeSelectorWidget::OnCheck_x50); }
	bChecksBound = false;
}

void UBulkModeSelectorWidget::OnCheck_x1(bool bIsChecked)  { HandleCheckChanged(EEnhanceBulkMode::x1,  BulkCheck_x1,  bIsChecked); }
void UBulkModeSelectorWidget::OnCheck_x10(bool bIsChecked) { HandleCheckChanged(EEnhanceBulkMode::x10, BulkCheck_x10, bIsChecked); }
void UBulkModeSelectorWidget::OnCheck_x50(bool bIsChecked) { HandleCheckChanged(EEnhanceBulkMode::x50, BulkCheck_x50, bIsChecked); }

void UBulkModeSelectorWidget::HandleCheckChanged(EEnhanceBulkMode Mode, UCheckBox* Sender, bool bIsChecked)
{
	// 라디오 규약: 현재 모드의 체크 해제 시도는 되돌림 (항상 하나는 선택)
	if (!bIsChecked)
	{
		if (Mode == CurrentMode && Sender)
		{
			Sender->SetIsChecked(true);
		}
		return;
	}

	// 이미 선택된 모드 재체크 — 상태만 정합 유지
	if (Mode == CurrentMode)
	{
		SyncVisuals();
		return;
	}

	CurrentMode = Mode;
	SyncVisuals();
	OnModeChanged.Broadcast(CurrentMode);
}

void UBulkModeSelectorWidget::SyncVisuals()
{
	if (BulkCheck_x1)  { BulkCheck_x1->SetIsChecked(CurrentMode == EEnhanceBulkMode::x1); }
	if (BulkCheck_x10) { BulkCheck_x10->SetIsChecked(CurrentMode == EEnhanceBulkMode::x10); }
	if (BulkCheck_x50) { BulkCheck_x50->SetIsChecked(CurrentMode == EEnhanceBulkMode::x50); }
}

void UBulkModeSelectorWidget::SetMode(EEnhanceBulkMode InMode)
{
	CurrentMode = InMode;
	SyncVisuals();
}
