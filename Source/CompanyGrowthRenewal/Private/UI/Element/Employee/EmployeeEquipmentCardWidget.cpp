#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Table/EmployeeEquipmentTable.h"

namespace
{
	// 빈 칸 면 — 밝은 목록 판 한 단 아래의 중립 웰.
	// ⚠ 다크 차콜(#2C3444)은 쓰지 않는다: 크림 판 위에서 검은 박스로 읽혀 "아직 안 채운 칸"이 아니라
	// "깨진 칸"으로 보인다(사용자 판정 2026-09-24). 빈 칸은 눈에 띄면 안 되고 격자만 유지하면 된다.
	const FLinearColor EmptySlotFace = FLinearColor::FromSRGBColor(FColor(0xE4, 0xE9, 0xF0));
}

void UEmployeeEquipmentCardWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UEmployeeEquipmentItemObject* Item = Cast<UEmployeeEquipmentItemObject>(ListItemObject);
	SetItemData(Item);
}

void UEmployeeEquipmentCardWidget::SetItemData(const UEmployeeEquipmentItemObject* Item)
{
	const bool bNewItem = Item && (!bHasItemData || CurrentInstanceID != Item->InstanceID
		|| CurrentDefinitionID != Item->DefinitionID || CurrentTexture != Item->Icon);
	bHasItemData = Item != nullptr;
	bChoiceItem = Item && Item->Grade == EEmployeeEquipmentGrade::Count && !Item->ChoiceLabel.IsEmpty();
	const bool bShowWornByOther = Item && !bChoiceItem && Item->bEquipped && Item->bWornByOther;
	const bool bShowEquipped = Item && !bChoiceItem && Item->bEquipped && !Item->bWornByOther;
	if (EquippedBadge) EquippedBadge->SetVisibility(bShowEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (WearerBand) WearerBand->SetVisibility(bShowWornByOther ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (WearerText) WearerText->SetVisibility(bShowWornByOther ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (EnhanceScaleBox) EnhanceScaleBox->SetUserSpecifiedScale(bShowWornByOther ? WornEnhanceScale : 1.f);
	if (EquippedText)
	{
		EquippedText->SetText(bShowEquipped ? NSLOCTEXT("EquipmentUI", "EquippedBadge", "장착") : FText());
		EquippedText->SetVisibility(bShowEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 면 색은 등급 행이 준비된 뒤라야 정해진다 — MID 만 여기서 확보하고 주입은 아래에서 한다.
	UMaterialInstanceDynamic* SurfaceMaterial = nullptr;
	if (CardBackground)
	{
		// 빈 칸도 면 색을 받아야 하므로 아이템 유무와 무관하게 MID 를 만든다.
		SurfaceMaterial = CardBackground->GetDynamicMaterial();
		if (!SurfaceMaterial && Item)
			UE_LOG(LogTemp, Warning, TEXT("Equipment card surface material missing: %s"), *GetPathName());
	}
	if (!Item)
	{
		// 빈 칸은 판보다 한 단 어두운 중립 웰이다 — 격자만 유지하고 시선은 안 끈다.
		if (SurfaceMaterial) SurfaceMaterial->SetVectorParameterValue(TEXT("GradeColor"), EmptySlotFace);
		RefreshGradeText(nullptr);
		CGEquipmentIcon::ApplyGradeFrame(GradeFrame, nullptr, FLinearColor::Transparent);
		CurrentTexture = nullptr;
		IconMaterial = nullptr;
		CurrentGrade = EEmployeeEquipmentGrade::Common;
		CurrentIconContext = EEmployeeEquipmentIconContext::Idle;
		CurrentInstanceID.Invalidate();
		CurrentDefinitionID = NAME_None;
		CGEquipmentIcon::Apply(ItemImage, nullptr);
		CGEquipmentIcon::Apply(ChoiceImage, nullptr);
		if (SelectionMark) SelectionMark->SetVisibility(ESlateVisibility::Collapsed);
		if (LockMark) LockMark->SetVisibility(ESlateVisibility::Collapsed);
		for (UCommonTextBlock* Label : { EnhanceText.Get(), ChoiceText.Get() })
		{
			if (!Label) continue;
			Label->SetText(FText());
			Label->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	CurrentTexture = Item->Icon;
	CurrentGrade = Item->VisualGrade;
	CurrentIconContext = Item->IconContext;
	CurrentInstanceID = Item->InstanceID;
	CurrentDefinitionID = Item->DefinitionID;
	if (bNewItem && IconMaterial) IconMaterial->SetScalarParameterValue(TEXT("EquipmentFXPulseStart"), -1000.f);
	const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const auto* ActualGrade = Tables && Item->Grade != EEmployeeEquipmentGrade::Count ? Tables->FindEmployeeEquipmentGrade(Item->Grade) : nullptr;
	// 등급은 면 전체가 진다 — 테두리 한 줄로는 흰 판 위에서 타일들이 한 덩어리로 읽힌다.
	// 선택 항목(Grade == Count)은 등급이 없으니 저작된 중립색을 그대로 둔다.
	// MID 는 풀링 엔트리에 붙어 살아 있다 — 등급 행 조회가 실패했을 때 건너뛰면 직전 장비의 등급색 면이 남는다.
	if (SurfaceMaterial && Item->Grade != EEmployeeEquipmentGrade::Count)
		SurfaceMaterial->SetVectorParameterValue(TEXT("GradeColor"),
			ActualGrade ? ActualGrade->Color : Item->GradeColor);
	CGEquipmentIcon::ApplyGradeFrame(GradeFrame, ActualGrade, Item->GradeColor);
	if (LockMark) LockMark->SetVisibility(Item->bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (SelectionMark) SelectionMark->SetVisibility(IsListItemSelected() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const bool bEntrySelected = IsListItemSelected();
	RefreshIcon(bEntrySelected, bNewItem && CurrentIconContext == EEmployeeEquipmentIconContext::Reveal && !Item->bDeferRevealPulse);
	RefreshGradeText(ActualGrade);
	if (!ActualGrade && Item->Grade != EEmployeeEquipmentGrade::Count)
		UE_LOG(LogTemp, Warning, TEXT("Equipment grade text row missing: definition=%s grade=%d"), *Item->DefinitionID.ToString(), static_cast<int32>(Item->Grade));
	if (ChoiceText)
	{
		ChoiceText->SetText(bChoiceItem ? Item->ChoiceLabel : FText());
		ChoiceText->SetVisibility(bChoiceItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (EnhanceText)
	{
		EnhanceText->SetText(FText::Format(NSLOCTEXT("EquipmentUI", "Enhancement", "+{0}"), FText::AsNumber(Item->EnhancementLevel)));
		EnhanceText->SetVisibility(Item->EnhancementLevel > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UEmployeeEquipmentCardWidget::RefreshGradeText(const FEmployeeEquipmentGradeRow* Grade)
{
	if (!GradeText) return;
	const bool bShowGrade = Grade && !Grade->DisplayName.IsEmpty();
	if (GradeBadge)
	{
		GradeBadge->SetBrushColor(bShowGrade ? Grade->Color : FLinearColor::Transparent);
		GradeBadge->SetVisibility(bShowGrade ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	UMaterialInstanceDynamic* FontMaterial = bShowGrade ? GradeText->GetDynamicFontMaterial()
		: Cast<UMaterialInstanceDynamic>(GradeText->GetFont().FontMaterial);
	if (FontMaterial)
	{
		FontMaterial->SetVectorParameterValue(TEXT("GradeColor"), FLinearColor::White);
		FontMaterial->SetScalarParameterValue(TEXT("ShineStrength"), 0.f);
		FontMaterial->SetScalarParameterValue(TEXT("RainbowStrength"), 0.f);
		FontMaterial->SetScalarParameterValue(TEXT("SparkleStrength"), 0.f);
		FontMaterial->SetScalarParameterValue(TEXT("FlowSpeed"), 0.f);
	}
	else if (bShowGrade)
		UE_LOG(LogTemp, Warning, TEXT("Equipment grade font material missing: %s"), *GradeText->GetPathName());
	// Font.FontMaterial retains the widget-owned MID across pooled entry updates.
	GradeText->SetText(bShowGrade ? Grade->DisplayName : FText());
	GradeText->SetVisibility(bShowGrade ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UEmployeeEquipmentCardWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);
	if (SelectionMark) SelectionMark->SetVisibility(bHasItemData && bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RefreshIcon(bIsSelected, false);
}

void UEmployeeEquipmentCardWidget::TriggerIconPulse()
{
	if (bHasItemData && !bChoiceItem && CurrentTexture)
	{
		if (CurrentIconContext == EEmployeeEquipmentIconContext::Idle && IconMaterial)
		{
			const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
			const auto* Grade = Tables ? Tables->FindEmployeeEquipmentGrade(CurrentGrade) : nullptr;
			if (Grade) IconMaterial->SetScalarParameterValue(TEXT("EquipmentFXStrength"), Grade->IconFocusedStrength);
		}
		CGEquipmentIcon::TriggerPulse(IconMaterial);
	}
}

void UEmployeeEquipmentCardWidget::RefreshIcon(bool bEntrySelected, bool bTriggerPulse)
{
	const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const auto* Grade = !bChoiceItem && Tables ? Tables->FindEmployeeEquipmentGrade(CurrentGrade) : nullptr;
	const auto Context = bEntrySelected && CurrentIconContext == EEmployeeEquipmentIconContext::Idle ? EEmployeeEquipmentIconContext::Focused : CurrentIconContext;
	UObject* Resource = CGEquipmentIcon::Resolve(this, CurrentTexture, Grade, Context, IconMaterial, bTriggerPulse);
	CGEquipmentIcon::Apply(ItemImage, bChoiceItem ? nullptr : Resource);
	if (bChoiceItem && ChoiceImage && CurrentTexture)
	{
		FSlateBrush ChoiceBrush = ChoiceImage->GetBrush();
		ChoiceBrush.ImageSize = FVector2D(CurrentTexture->GetSizeX(), CurrentTexture->GetSizeY());
		ChoiceImage->SetBrush(ChoiceBrush);
	}
	CGEquipmentIcon::Apply(ChoiceImage, bChoiceItem ? Resource : nullptr);
}

void UEmployeeEquipmentCardWidget::NativeOnEntryReleased()
{
	SetItemData(nullptr);
	IUserListEntry::NativeOnEntryReleased();
}

void UEmployeeEquipmentCardWidget::NativeDestruct()
{
	SetItemData(nullptr);
	Super::NativeDestruct();
}
