#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Table/EmployeeEquipmentTable.h"
#include "Utils/EmployeeEquipmentVisuals.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "Data/EmployeeEquipmentData.h"
#include "CommonTextBlock.h"
#include "CoreGlobals.h"
#include "HAL/PlatformTime.h"

void CGEquipmentIcon::ApplySlotButton(UObject* Consumer, UIconWithButtonWidget* Button, const UTableManagerSubsystem* Tables,
	EEmployeeEquipmentSlot Part, const FEmployeeEquipmentInstance* Item, TObjectPtr<UMaterialInstanceDynamic>& Material,
	bool bWearsBaseAppearance)
{
	if (!Button) return;
	const auto* Row = Tables ? Tables->FindEmployeeEquipmentSlot(Part) : nullptr;
	const auto* Definition = Tables && Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	const auto* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
	const FText SlotLabel = !Item && Row ? Row->DisplayName : FText();
	Button->SetButtonText(SlotLabel);
	if (auto* Label = Cast<UCommonTextBlock>(Button->GetWidgetFromName(TEXT("ButtonText"))))
		Label->SetVisibility(SlotLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Button->SetTextColor(FSlateColor(FLinearColor(0.0252f, 0.0369f, 0.0612f, 1.f)));
	ApplyGradeFrame(Cast<UImage>(Button->GetWidgetFromName(TEXT("GradeFrame"))), Grade, Grade ? Grade->Color : FLinearColor::Transparent);
	UObject* Icon = Resolve(Consumer, Definition && !Definition->Icon.IsNull() ? Definition->Icon.LoadSynchronous() : nullptr,
		Definition ? Tables->FindEmployeeEquipmentGrade(VisualGrade(Tables, *Definition)) : nullptr, EEmployeeEquipmentIconContext::Idle, Material);
	Button->SetIconResource(Icon);
	Button->SetIconColor(FLinearColor::White);
	if (!Icon) Apply(Cast<UImage>(Button->GetWidgetFromName(TEXT("IconImage"))), nullptr);
	Button->SetIconVisibility(Icon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	const bool bHasEnhancement = Item && Item->EnhanceLevel > 0;
	// 강화 배지 자리를 공유한다 — 장비가 없을 때만 비므로 "기본 외형 착용" 표기와 겹치지 않는다.
	Button->SetCountText(bHasEnhancement
		? FText::Format(NSLOCTEXT("EmployeeEquipment", "SlotEnhancement", "+{0}"), FText::AsNumber(Item->EnhanceLevel))
		: (!Item && bWearsBaseAppearance) ? NSLOCTEXT("EmployeeEquipment", "SlotBaseAppearance", "기본") : FText());
}

EEmployeeEquipmentGrade CGEquipmentIcon::VisualGrade(const UTableManagerSubsystem* Tables, const FEmployeeEquipmentDefinitionRow& Definition)
{
	const auto* Appearance = Tables ? Tables->FindEmployeeEquipmentAppearance(Definition.AppearanceId) : nullptr;
	if (Appearance && Tables->FindEmployeeEquipmentGrade(Appearance->VisualGrade)) return Appearance->VisualGrade;
	UE_LOG(LogTemp, Warning, TEXT("Equipment icon visual row missing: definition=%s appearance=%s"), *Definition.DefinitionId.ToString(), *Definition.AppearanceId.ToString());
	return EEmployeeEquipmentGrade::Count;
}

UObject* CGEquipmentIcon::Resolve(UObject* Consumer, UTexture2D* Texture, const FEmployeeEquipmentGradeRow* Grade,
	EEmployeeEquipmentIconContext Context, TObjectPtr<UMaterialInstanceDynamic>& Material, bool bTriggerPulse)
{
	UMaterialInterface* Parent = Texture && Grade && !Grade->IconMaterial.IsNull() ? Grade->IconMaterial.LoadSynchronous() : nullptr;
	if (!Parent)
	{
		if (Texture && Grade && !Grade->IconMaterial.IsNull()) UE_LOG(LogTemp, Warning, TEXT("Equipment icon material failed to load: %s"), *Grade->IconMaterial.ToString());
		Material = nullptr;
		return Texture;
	}
	if (!Material || Material->Parent != Parent)
	{
		Material = UMaterialInstanceDynamic::Create(Parent, Consumer);
		if (Material) Material->SetScalarParameterValue(TEXT("EquipmentFXPulseStart"), -1000.f);
	}
	if (!Material) return Texture;
	const float Strength = Context == EEmployeeEquipmentIconContext::Reveal ? Grade->IconRevealStrength
		: Context == EEmployeeEquipmentIconContext::Focused ? Grade->IconFocusedStrength : Grade->IconIdleStrength;
	FEmployeeEquipmentVisuals::ApplyParameters(Material, *Grade, Strength);
	Material->SetTextureParameterValue(TEXT("IconTexture"), Texture);
	if (Context == EEmployeeEquipmentIconContext::Idle)
		Material->SetScalarParameterValue(TEXT("EquipmentFXPulseStart"), -1000.f);
	if (bTriggerPulse && Context != EEmployeeEquipmentIconContext::Idle && Strength > 0.f) TriggerPulse(Material);
	return Material;
}

void CGEquipmentIcon::TriggerPulse(UMaterialInstanceDynamic* Material)
{
	if (Material) Material->SetScalarParameterValue(TEXT("EquipmentFXPulseStart"), static_cast<float>(FPlatformTime::Seconds() - GStartTime));
}

void CGEquipmentIcon::Apply(UImage* Image, UObject* Resource)
{
	if (!Image) return;
	// Clearing a recycled icon must preserve its authored size and UV region.
	FSlateBrush Brush = Image->GetBrush();
	Brush.SetResourceObject(Resource);
	Image->SetBrush(Brush);
	Image->SetVisibility(Resource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void CGEquipmentIcon::ApplyGradeFrame(UImage* Image, const FEmployeeEquipmentGradeRow* Grade, const FLinearColor& Color)
{
	if (!Image) return;
	UMaterialInstanceDynamic* Material = Grade ? Image->GetDynamicMaterial() : Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject());
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("LineCol"), Grade ? Color : FLinearColor::Transparent);
		Material->SetScalarParameterValue(TEXT("GlintGain"), 0.f);
		Material->SetScalarParameterValue(TEXT("GlintSpeed"), 0.f);
	}
	else if (Grade) UE_LOG(LogTemp, Warning, TEXT("Equipment grade frame material missing: %s"), *Image->GetPathName());
	// Keep the authored material and dimensions so pooled entries reuse their existing MID.
	Image->SetVisibility(Grade && Material ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
