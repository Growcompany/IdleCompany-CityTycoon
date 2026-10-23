#if WITH_EDITOR

#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "CommonTextBlock.h"
#include "CoreGlobals.h"
#include "Components/Image.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	UImage* EquipmentFXImage(UEmployeeEquipmentCardWidget* Card)
	{
		return Card ? Cast<UImage>(Card->GetWidgetFromName(TEXT("ItemImage"))) : nullptr;
	}

	UMaterialInstanceDynamic* EquipmentGradeFontMaterial(const UCommonTextBlock* Label)
	{
		return Label ? Cast<UMaterialInstanceDynamic>(Label->GetFont().FontMaterial) : nullptr;
	}

	bool EquipmentGradeFontMatches(const UCommonTextBlock* Label, const FEmployeeEquipmentGradeRow* Grade)
	{
		auto* FontMaterial = EquipmentGradeFontMaterial(Label);
		return Label && FontMaterial
			&& FontMaterial->K2_GetVectorParameterValue(TEXT("GradeColor")).Equals(FLinearColor::White)
			&& FMath::IsNearlyZero(FontMaterial->K2_GetScalarParameterValue(TEXT("ShineStrength")))
			&& FMath::IsNearlyZero(FontMaterial->K2_GetScalarParameterValue(TEXT("RainbowStrength")))
			&& FMath::IsNearlyZero(FontMaterial->K2_GetScalarParameterValue(TEXT("SparkleStrength")))
			&& FMath::IsNearlyZero(FontMaterial->K2_GetScalarParameterValue(TEXT("FlowSpeed")))
			&& Label->GetText().EqualTo(Grade ? Grade->DisplayName : FText())
			&& Label->GetVisibility() == (Grade ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	struct FEquipmentFXCapture
	{
		TWeakObjectPtr<UWorld> PlayWorld;
		TArray<TStrongObjectPtr<UEmployeeEquipmentCardWidget>> Cards;
		TUniquePtr<FWidgetRenderer> Renderer;
		TStrongObjectPtr<UTextureRenderTarget2D> Target;
		TSharedPtr<SVerticalBox> Surface;
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		FString Directory;
		int32 Frame = 0;
		int64 RevisionBefore = 0;
		double LastCaptureTime = 0.0;
		float WarmupRemaining = .5f;
		bool bSaved = true;
		bool bGradeFrames = false;
		FVector2D CaptureSize = FVector2D(1280.f, 720.f);
		TArray<TSharedPtr<FJsonValue>> Timestamps;

		bool Start(UWorld* InWorld)
		{
			PlayWorld = InWorld;
			auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
			const auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
			const auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
			if (!Tables || !Equipment || !InWorld->GetFirstPlayerController()) return false;
			RevisionBefore = Equipment->GetState().Revision;
			const FEmployeeEquipmentDefinitionRow* Hat = nullptr;
			for (const auto& Pair : Tables->GetEmployeeEquipmentDefinitions())
			{
				if (Pair.Value.EquipmentSlot == EEmployeeEquipmentSlot::Head && !Pair.Value.Icon.IsNull()
					&& (!Hat || Pair.Key.LexicalLess(Hat->DefinitionId))) Hat = &Pair.Value;
			}
			const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentCard);
			UTexture2D* Texture = Hat ? Hat->Icon.LoadSynchronous() : nullptr;
			if (!Class || !Texture) return false;
			Directory = FPaths::Combine(FPaths::ProjectSavedDir(), bGradeFrames ? TEXT("EmployeeEquipmentFX/GradeFrames") : TEXT("EmployeeEquipmentFX/UISequence"));
			if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory)) return false;
			Surface = SNew(SVerticalBox);
			const TArray<EEmployeeEquipmentGrade> Grades = bGradeFrames
				? TArray<EEmployeeEquipmentGrade>{ EEmployeeEquipmentGrade::Common, EEmployeeEquipmentGrade::Uncommon, EEmployeeEquipmentGrade::Rare, EEmployeeEquipmentGrade::Epic, EEmployeeEquipmentGrade::Legendary, EEmployeeEquipmentGrade::Ascendant }
				: TArray<EEmployeeEquipmentGrade>{ EEmployeeEquipmentGrade::Common, EEmployeeEquipmentGrade::Legendary, EEmployeeEquipmentGrade::Ascendant };
			CaptureSize = bGradeFrames ? FVector2D(1728.f, 608.f) : FVector2D(864.f, 304.f);
			bool bAllGradeSurfaceColors = true;
			for (int32 RowIndex = 0; RowIndex < (bGradeFrames ? 2 : 1); ++RowIndex)
			{
				const TSharedRef<SHorizontalBox> CardRow = SNew(SHorizontalBox);
				Surface->AddSlot().FillHeight(1.f)[CardRow];
				for (EEmployeeEquipmentGrade Grade : Grades)
				{
					const auto* Row = Tables->FindEmployeeEquipmentGrade(Grade);
					if (!Row) return false;
					auto* Card = CreateWidget<UEmployeeEquipmentCardWidget>(InWorld->GetFirstPlayerController(), Class);
					if (!Card) return false;
					Cards.Emplace(Card);
					CardRow->AddSlot().FillWidth(1.f).Padding(12.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[SNew(SBox).WidthOverride(256.f).HeightOverride(256.f)[Card->TakeWidget()]];
					auto* Item = NewObject<UEmployeeEquipmentItemObject>(Card);
					Item->DefinitionID = Hat->DefinitionId; Item->Icon = Texture; Item->Grade = Grade;
					Item->VisualGrade = bGradeFrames ? EEmployeeEquipmentGrade::Common : Grade;
					Item->GradeName = Row->DisplayName; Item->GradeColor = Row->Color;
					Item->IconContext = EEmployeeEquipmentIconContext::Focused;
					Card->SetItemData(Item);
					const auto* GradeLabel = Cast<UCommonTextBlock>(Card->GetWidgetFromName(TEXT("GradeText")));
					Report->SetBoolField(FString::Printf(TEXT("grade_%d_row_%d_font_uses_actual_grade_data"), static_cast<int32>(Grade), RowIndex),
						EquipmentGradeFontMatches(GradeLabel, Row));
					if (bGradeFrames)
					{
						IUserListEntry::UpdateItemSelection(*Card, RowIndex == 1);
						auto* FrameImage = Cast<UImage>(Card->GetWidgetFromName(TEXT("GradeFrame")));
						auto* FrameMID = FrameImage ? Cast<UMaterialInstanceDynamic>(FrameImage->GetBrush().GetResourceObject()) : nullptr;
						Report->SetBoolField(FString::Printf(TEXT("grade_%d_row_%d_frame_color"), static_cast<int32>(Grade), RowIndex), FrameMID
							&& FrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(Row->Color)
							&& FrameImage->GetVisibility() == ESlateVisibility::HitTestInvisible);
						const auto* CardSurfaceImage = Cast<UImage>(Card->GetWidgetFromName(TEXT("CardBackground")));
						auto* CardSurfaceMID = CardSurfaceImage ? Cast<UMaterialInstanceDynamic>(CardSurfaceImage->GetBrush().GetResourceObject()) : nullptr;
						bAllGradeSurfaceColors &= CardSurfaceMID && CardSurfaceMID->K2_GetVectorParameterValue(TEXT("GradeColor")).Equals(Row->Color);
					}
				}
			}
			if (bGradeFrames) Report->SetBoolField(TEXT("all_grade_surfaces_use_actual_grade_color"), bAllGradeSurfaceColors);
			auto* TestCard = Cards.Last().Get();
			const auto* Ascendant = Tables->FindEmployeeEquipmentGrade(EEmployeeEquipmentGrade::Ascendant);
			const auto* Legendary = Tables->FindEmployeeEquipmentGrade(EEmployeeEquipmentGrade::Legendary);
			const auto* Common = Tables->FindEmployeeEquipmentGrade(EEmployeeEquipmentGrade::Common);
			if (!Ascendant || !Legendary || !Common) return false;
			auto* Fixture = NewObject<UEmployeeEquipmentItemObject>(TestCard);
			Fixture->Icon = Texture; Fixture->Grade = EEmployeeEquipmentGrade::Ascendant;
			Fixture->VisualGrade = EEmployeeEquipmentGrade::Ascendant; Fixture->GradeColor = Ascendant->Color;
			TestCard->SetItemData(Fixture);
			const auto* GradeText = Cast<UCommonTextBlock>(TestCard->GetWidgetFromName(TEXT("GradeText")));
			auto* InitialFontMID = EquipmentGradeFontMaterial(GradeText);
			Report->SetBoolField(TEXT("grade_font_mid_owned_by_text_block"), InitialFontMID && InitialFontMID->GetOuter() == GradeText);
			auto* Image = EquipmentFXImage(TestCard);
			const auto* CardSurfaceImage = Cast<UImage>(TestCard->GetWidgetFromName(TEXT("CardBackground")));
			auto* InitialSurfaceMID = CardSurfaceImage ? Cast<UMaterialInstanceDynamic>(CardSurfaceImage->GetBrush().GetResourceObject()) : nullptr;
			const FSlateBrush OriginalSurfaceBrush = CardSurfaceImage ? CardSurfaceImage->GetBrush() : FSlateBrush();
			auto* FrameImage = Cast<UImage>(TestCard->GetWidgetFromName(TEXT("GradeFrame")));
			auto* InitialFrameMID = FrameImage ? Cast<UMaterialInstanceDynamic>(FrameImage->GetBrush().GetResourceObject()) : nullptr;
			Report->SetBoolField(TEXT("ascendant_frame_color_and_static_line"), InitialFrameMID
				&& InitialFrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(Ascendant->Color)
				&& FMath::IsNearlyZero(InitialFrameMID->K2_GetScalarParameterValue(TEXT("GlintGain")))
				&& FMath::IsNearlyZero(InitialFrameMID->K2_GetScalarParameterValue(TEXT("GlintSpeed"))));
			const FSlateBrush OriginalBrush = Image ? Image->GetBrush() : FSlateBrush();
			auto* InitialMID = Image ? Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject()) : nullptr;
			Report->SetBoolField(TEXT("ascendant_material_applied"), InitialMID && InitialMID->K2_GetTextureParameterValue(TEXT("IconTexture")) == Texture);
			Fixture->Grade = EEmployeeEquipmentGrade::Legendary; Fixture->GradeColor = Legendary->Color;
			Fixture->VisualGrade = EEmployeeEquipmentGrade::Legendary; Fixture->IconContext = EEmployeeEquipmentIconContext::Focused;
			Fixture->bLocked = true; Fixture->EnhancementLevel = 5;
			TestCard->SetItemData(Fixture);
			auto* LegendaryMID = Image ? Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject()) : nullptr;
			Report->SetBoolField(TEXT("same_parent_mid_reused_and_rainbow_reset"), InitialMID && LegendaryMID == InitialMID && LegendaryMID->K2_GetScalarParameterValue(TEXT("EquipmentFXRainbow")) == 0.f);
			IUserListEntry::UpdateItemSelection(*TestCard, true);
			const auto* SelectionMark = Cast<UImage>(TestCard->GetWidgetFromName(TEXT("SelectionMark")));
			const bool bSelectionMarkVisibleOnCallback = SelectionMark && SelectionMark->GetVisibility() == ESlateVisibility::HitTestInvisible;
			Report->SetBoolField(TEXT("selection_focus_strength"), LegendaryMID && Legendary && FMath::IsNearlyEqual(LegendaryMID->K2_GetScalarParameterValue(TEXT("EquipmentFXStrength")), Legendary->IconFocusedStrength));
			TestCard->TriggerIconPulse();
			const float PulseStartBeforeRebind = LegendaryMID ? LegendaryMID->K2_GetScalarParameterValue(TEXT("EquipmentFXPulseStart")) : -1000.f;
			Report->SetNumberField(TEXT("focused_pulse_start_before_rebind"), PulseStartBeforeRebind);
			TestCard->SetItemData(Fixture);
			Report->SetBoolField(TEXT("same_selected_item_rebind_does_not_restart_pulse"), LegendaryMID
				&& PulseStartBeforeRebind >= 0.f
				&& FMath::IsNearlyEqual(LegendaryMID->K2_GetScalarParameterValue(TEXT("EquipmentFXPulseStart")), PulseStartBeforeRebind));
			const auto* LockMark = Cast<UImage>(TestCard->GetWidgetFromName(TEXT("LockMark")));
			const auto* EnhanceText = Cast<UCommonTextBlock>(TestCard->GetWidgetFromName(TEXT("EnhanceText")));
			const FLinearColor GradeLabelColor = GradeText ? GradeText->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::Transparent;
			Report->SetBoolField(TEXT("font_mid_reused_and_rainbow_sparkle_reset_on_legendary"), InitialFontMID
				&& EquipmentGradeFontMaterial(GradeText) == InitialFontMID && EquipmentGradeFontMatches(GradeText, Legendary));
			Report->SetBoolField(TEXT("lock_and_enhancement_marks_visible_when_present"), LockMark && LockMark->GetVisibility() == ESlateVisibility::HitTestInvisible
				&& EnhanceText && EnhanceText->GetVisibility() == ESlateVisibility::HitTestInvisible && EnhanceText->GetText().ToString() == TEXT("+5"));
			Report->SetBoolField(TEXT("selection_preserves_grade_frame"), InitialFrameMID && FrameImage->GetBrush().GetResourceObject() == InitialFrameMID
				&& InitialFrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(Legendary->Color)
				&& bSelectionMarkVisibleOnCallback);
			Report->SetBoolField(TEXT("selection_preserves_grade_surface"), InitialSurfaceMID && CardSurfaceImage->GetBrush().GetResourceObject() == InitialSurfaceMID
				&& InitialSurfaceMID->K2_GetVectorParameterValue(TEXT("GradeColor")).Equals(Legendary->Color));
			Fixture->Grade = EEmployeeEquipmentGrade::Common; Fixture->GradeColor = Common->Color;
			Fixture->VisualGrade = EEmployeeEquipmentGrade::Common; Fixture->bLocked = false; Fixture->EnhancementLevel = 0;
			TestCard->SetItemData(Fixture);
			IUserListEntry::UpdateItemSelection(*TestCard, false);
			Report->SetBoolField(TEXT("recycle_hides_unlocked_zero_enhancement_and_unselected_marks"), LockMark && LockMark->GetVisibility() == ESlateVisibility::Collapsed
				&& EnhanceText && EnhanceText->GetVisibility() == ESlateVisibility::Collapsed && SelectionMark && SelectionMark->GetVisibility() == ESlateVisibility::Collapsed);
			Report->SetBoolField(TEXT("recycle_common_frame_reuses_mid_and_stops_glint"), InitialFrameMID && FrameImage->GetBrush().GetResourceObject() == InitialFrameMID
				&& InitialFrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(Common->Color)
				&& InitialFrameMID->K2_GetScalarParameterValue(TEXT("GlintGain")) == 0.f
				&& InitialFrameMID->K2_GetScalarParameterValue(TEXT("GlintSpeed")) == 0.f);
			Report->SetBoolField(TEXT("grade_surface_reuses_mid_and_updates_common_color"), InitialSurfaceMID && CardSurfaceImage->GetBrush().GetResourceObject() == InitialSurfaceMID
				&& InitialSurfaceMID->K2_GetVectorParameterValue(TEXT("GradeColor")).Equals(Common->Color));
			Report->SetBoolField(TEXT("grade_label_preserves_authored_color"), GradeText && GradeText->GetColorAndOpacity().GetSpecifiedColor().Equals(GradeLabelColor)
				&& !GradeLabelColor.Equals(Legendary->Color) && !GradeLabelColor.Equals(Common->Color));
			Report->SetBoolField(TEXT("font_mid_reused_and_all_fx_stopped_on_common"), InitialFontMID
				&& EquipmentGradeFontMaterial(GradeText) == InitialFontMID && EquipmentGradeFontMatches(GradeText, Common));
			auto* OrdinaryMID = Image ? Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject()) : nullptr;
			bool bFXEnabled = true; FGuid ExpressionId;
			Report->SetBoolField(TEXT("ordinary_icon_uses_static_outline_only"), OrdinaryMID
				&& OrdinaryMID->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("EquipmentFXEnabled")), bFXEnabled, ExpressionId)
				&& !bFXEnabled && OrdinaryMID->K2_GetTextureParameterValue(TEXT("IconTexture")) == Texture);
			bool bHighFXEnabled = false;
			Report->SetBoolField(TEXT("high_grade_fx_static_switch_enabled"), LegendaryMID
				&& LegendaryMID->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("EquipmentFXEnabled")), bHighFXEnabled, ExpressionId) && bHighFXEnabled);
			IUserListEntry::ReleaseEntry(*TestCard);
			Report->SetBoolField(TEXT("release_preserves_font_mid_and_resets_neutral_parameters"), InitialFontMID
				&& EquipmentGradeFontMaterial(GradeText) == InitialFontMID && EquipmentGradeFontMatches(GradeText, nullptr));
			Report->SetBoolField(TEXT("release_keeps_surface_mid_and_resets_neutral_color"), InitialSurfaceMID && CardSurfaceImage->GetBrush().GetResourceObject() == InitialSurfaceMID
				&& InitialSurfaceMID->K2_GetVectorParameterValue(TEXT("GradeColor")).Equals(FLinearColor::FromSRGBColor(FColor(0xE4, 0xE9, 0xF0))));
			Report->SetBoolField(TEXT("release_clears_brush_lock_and_selection"), Image && !Image->GetBrush().GetResourceObject() && Image->GetVisibility() == ESlateVisibility::Collapsed
				&& LockMark && LockMark->GetVisibility() == ESlateVisibility::Collapsed && SelectionMark && SelectionMark->GetVisibility() == ESlateVisibility::Collapsed
				&& EnhanceText && EnhanceText->GetText().IsEmpty() && EnhanceText->GetVisibility() == ESlateVisibility::Collapsed);
			Report->SetBoolField(TEXT("release_hides_and_resets_grade_frame"), InitialFrameMID && FrameImage->GetVisibility() == ESlateVisibility::Collapsed
				&& InitialFrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(FLinearColor::Transparent)
				&& InitialFrameMID->K2_GetScalarParameterValue(TEXT("GlintGain")) == 0.f
				&& InitialFrameMID->K2_GetScalarParameterValue(TEXT("GlintSpeed")) == 0.f);
			IUserListEntry::UpdateItemSelection(*TestCard, true);
			Report->SetBoolField(TEXT("released_entry_ignores_late_selection_visual"), SelectionMark && SelectionMark->GetVisibility() == ESlateVisibility::Collapsed);
			Fixture->Grade = EEmployeeEquipmentGrade::Count; Fixture->GradeName = FText();
			Fixture->VisualGrade = EEmployeeEquipmentGrade::Ascendant;
			Fixture->ChoiceLabel = NSLOCTEXT("EquipmentFXQA", "ManualChoice", "기획\n25 / 100");
			TestCard->SetItemData(Fixture);
			const auto* ChoiceImage = Cast<UImage>(TestCard->GetWidgetFromName(TEXT("ChoiceImage")));
			Report->SetBoolField(TEXT("manual_choice_keeps_grade_font_hidden_and_neutral"), InitialFontMID
				&& EquipmentGradeFontMaterial(GradeText) == InitialFontMID && EquipmentGradeFontMatches(GradeText, nullptr));
			const auto* ChoiceText = Cast<UCommonTextBlock>(TestCard->GetWidgetFromName(TEXT("ChoiceText")));
			Report->SetBoolField(TEXT("manual_choice_preserves_label_and_raw_portrait_without_equipment_chrome"), ChoiceImage
				&& ChoiceImage->GetVisibility() == ESlateVisibility::HitTestInvisible && ChoiceImage->GetBrush().GetResourceObject() == Texture
				&& FVector2D(ChoiceImage->GetBrush().ImageSize).Equals(FVector2D(Texture->GetSizeX(), Texture->GetSizeY()))
				&& ChoiceText && ChoiceText->GetVisibility() == ESlateVisibility::HitTestInvisible && ChoiceText->GetText().EqualTo(Fixture->ChoiceLabel)
				&& GradeText && GradeText->GetVisibility() == ESlateVisibility::Collapsed && FrameImage && FrameImage->GetVisibility() == ESlateVisibility::Collapsed
				&& InitialSurfaceMID && InitialSurfaceMID->K2_GetVectorParameterValue(TEXT("GradeColor")).Equals(FLinearColor::FromSRGBColor(FColor(0xE4, 0xE9, 0xF0)))
				&& Image && Image->GetVisibility() == ESlateVisibility::Collapsed);
			Fixture->Icon = nullptr;
			TestCard->SetItemData(Fixture);
			Report->SetBoolField(TEXT("manual_discipline_without_icon_remains_readable"), ChoiceImage && ChoiceImage->GetVisibility() == ESlateVisibility::Collapsed
				&& ChoiceText && ChoiceText->GetVisibility() == ESlateVisibility::HitTestInvisible && !ChoiceText->GetText().IsEmpty());
			IUserListEntry::ReleaseEntry(*TestCard);
			Report->SetBoolField(TEXT("release_clears_manual_choice_content"), ChoiceImage && !ChoiceImage->GetBrush().GetResourceObject()
				&& ChoiceImage->GetVisibility() == ESlateVisibility::Collapsed && ChoiceText && ChoiceText->GetText().IsEmpty() && ChoiceText->GetVisibility() == ESlateVisibility::Collapsed);
			Fixture->Grade = static_cast<EEmployeeEquipmentGrade>(255);
			Fixture->GradeName = Ascendant->DisplayName;
			Fixture->ChoiceLabel = FText();
			TestCard->SetItemData(Fixture);
			Report->SetBoolField(TEXT("invalid_grade_ignores_stale_cached_label_and_resets_font"), InitialFontMID
				&& EquipmentGradeFontMaterial(GradeText) == InitialFontMID && EquipmentGradeFontMatches(GradeText, nullptr));
			Fixture->Icon = Texture;
			Fixture->Grade = EEmployeeEquipmentGrade::Ascendant;
			Fixture->VisualGrade = bGradeFrames ? EEmployeeEquipmentGrade::Common : EEmployeeEquipmentGrade::Ascendant; Fixture->bLocked = false;
			Fixture->DefinitionID = Hat->DefinitionId;
			Fixture->GradeName = Ascendant->DisplayName; Fixture->GradeColor = Ascendant->Color;
			Fixture->IconContext = EEmployeeEquipmentIconContext::Focused;
			TestCard->SetItemData(Fixture);
			Report->SetBoolField(TEXT("font_mid_restored_after_choice_and_invalid_grade"), InitialFontMID
				&& EquipmentGradeFontMaterial(GradeText) == InitialFontMID && EquipmentGradeFontMatches(GradeText, Ascendant));
			Report->SetBoolField(TEXT("equipment_reuse_hides_manual_choice_content"), ChoiceImage && ChoiceImage->GetVisibility() == ESlateVisibility::Collapsed
				&& ChoiceText && ChoiceText->GetText().IsEmpty() && ChoiceText->GetVisibility() == ESlateVisibility::Collapsed
				&& InitialSurfaceMID && InitialSurfaceMID->K2_GetVectorParameterValue(TEXT("GradeColor")).Equals(Ascendant->Color)
				&& GradeText && GradeText->GetVisibility() == ESlateVisibility::HitTestInvisible && Image && Image->GetVisibility() == ESlateVisibility::HitTestInvisible);
			if (bGradeFrames) IUserListEntry::UpdateItemSelection(*TestCard, true);
			if (Image)
			{
				const FSlateBrush& FinalBrush = Image->GetBrush();
				Report->SetNumberField(TEXT("brush_width_before_recycle"), OriginalBrush.ImageSize.X);
				Report->SetNumberField(TEXT("brush_width_after_recycle"), FinalBrush.ImageSize.X);
				Report->SetNumberField(TEXT("brush_height_before_recycle"), OriginalBrush.ImageSize.Y);
				Report->SetNumberField(TEXT("brush_height_after_recycle"), FinalBrush.ImageSize.Y);
				Report->SetBoolField(TEXT("recycle_preserves_brush_geometry"), OriginalBrush.ImageSize == FinalBrush.ImageSize
					&& OriginalBrush.DrawAs == FinalBrush.DrawAs && FBox2f(OriginalBrush.GetUVRegion()) == FBox2f(FinalBrush.GetUVRegion()));
				Report->SetBoolField(TEXT("recycle_preserves_surface_brush_geometry"), CardSurfaceImage && OriginalSurfaceBrush.ImageSize == CardSurfaceImage->GetBrush().ImageSize
					&& OriginalSurfaceBrush.DrawAs == CardSurfaceImage->GetBrush().DrawAs && FBox2f(OriginalSurfaceBrush.GetUVRegion()) == FBox2f(CardSurfaceImage->GetBrush().GetUVRegion()));
			}
			const auto DialogClass = Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentDialog);
			TStrongObjectPtr<UEmployeeEquipmentDialogWidget> TestDialog(DialogClass ? CreateWidget<UEmployeeEquipmentDialogWidget>(InWorld->GetFirstPlayerController(), DialogClass) : nullptr);
			if (!TestDialog.IsValid()) return false;
			FEmployeeEquipmentDialogView DialogView;
			DialogView.CurrentIcon = Texture; DialogView.SelectedIcon = Texture;
			DialogView.CurrentGrade = EEmployeeEquipmentGrade::Legendary; DialogView.SelectedGrade = EEmployeeEquipmentGrade::Ascendant;
			DialogView.CurrentVisualGrade = EEmployeeEquipmentGrade::Common; DialogView.SelectedVisualGrade = EEmployeeEquipmentGrade::Common;
			TestDialog->Configure(DialogView);
			bool bModalGradesSeparated = true;
			for (const TCHAR* Side : {TEXT("Current"), TEXT("Selected")})
			{
				auto* ModalFrame = Cast<UImage>(TestDialog->GetWidgetFromName(FName(FString(Side) + TEXT("GradeFrame"))));
				auto* ModalIcon = Cast<UImage>(TestDialog->GetWidgetFromName(FName(FString(Side) + TEXT("IconImage"))));
				auto* FrameMID = ModalFrame ? Cast<UMaterialInstanceDynamic>(ModalFrame->GetBrush().GetResourceObject()) : nullptr;
				auto* IconMID = ModalIcon ? Cast<UMaterialInstanceDynamic>(ModalIcon->GetBrush().GetResourceObject()) : nullptr;
				const auto* ActualGrade = FString(Side) == TEXT("Current") ? Legendary : Ascendant;
				bModalGradesSeparated &= FrameMID && IconMID && FrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(ActualGrade->Color)
					&& IconMID->K2_GetScalarParameterValue(TEXT("EquipmentFXRainbow")) == 0.f
					&& IconMID->K2_GetScalarParameterValue(TEXT("EquipmentFXShine")) == 0.f;
			}
			Report->SetBoolField(TEXT("modal_actual_grade_frames_with_common_visual_icons"), bModalGradesSeparated);
			TestDialog->Configure(FEmployeeEquipmentDialogView());
			bool bModalFramesCleared = true;
			for (const TCHAR* FrameName : {TEXT("CurrentGradeFrame"), TEXT("SelectedGradeFrame")})
			{
				auto* ModalFrame = Cast<UImage>(TestDialog->GetWidgetFromName(FrameName));
				auto* FrameMID = ModalFrame ? Cast<UMaterialInstanceDynamic>(ModalFrame->GetBrush().GetResourceObject()) : nullptr;
				bModalFramesCleared &= FrameMID && ModalFrame->GetVisibility() == ESlateVisibility::Collapsed
					&& FrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(FLinearColor::Transparent)
					&& FrameMID->K2_GetScalarParameterValue(TEXT("GlintGain")) == 0.f;
			}
			Report->SetBoolField(TEXT("modal_empty_reuse_clears_grade_frames"), bModalFramesCleared);
			for (const auto& Card : Cards)
			{
				const auto* GradeLabel = Cast<UCommonTextBlock>(Card->GetWidgetFromName(TEXT("GradeText")));
				if (auto* FontMaterial = EquipmentGradeFontMaterial(GradeLabel))
					if (FMaterialResource* Resource = FontMaterial->GetMaterialResource(GMaxRHIFeatureLevel)) Resource->FinishCompilation();
				for (UImage* CardImage : {EquipmentFXImage(Card.Get()), Cast<UImage>(Card->GetWidgetFromName(TEXT("GradeFrame"))), Cast<UImage>(Card->GetWidgetFromName(TEXT("CardBackground")))})
				{
					if (auto* Material = CardImage ? Cast<UMaterialInstanceDynamic>(CardImage->GetBrush().GetResourceObject()) : nullptr)
						if (FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIFeatureLevel)) Resource->FinishCompilation();
				}
			}
			Renderer = MakeUnique<FWidgetRenderer>(true);
			Target.Reset(FWidgetRenderer::CreateTargetFor(CaptureSize, TF_Bilinear, false));
			if (!Target.IsValid()) return false;
			Target->ClearColor = FLinearColor(0.01096f, 0.016f, 0.02956f, 1.f);
			LastCaptureTime = FPlatformTime::Seconds();
			Report->SetStringField(TEXT("source"), TEXT("Actual registered EquipmentCard widgets; visual-only grade override on one shared source texture. No inventory transaction/save."));
			Report->SetStringField(TEXT("definition"), Hat->DefinitionId.ToString());
			Report->SetStringField(TEXT("texture"), Texture->GetPathName());
			Report->SetNumberField(TEXT("width"), CaptureSize.X); Report->SetNumberField(TEXT("height"), CaptureSize.Y);
			Report->SetNumberField(TEXT("card_count"), Cards.Num()); Report->SetBoolField(TEXT("grade_frame_matrix"), bGradeFrames);
			return true;
		}

		void Finish()
		{
			if (Renderer && Target.IsValid() && Surface)
			{
				for (int32 Index = 0; Index < 3; ++Index)
				{
					for (const auto& Card : Cards)
					{
						UImage* Image = EquipmentFXImage(Card.Get());
						if (auto* Material = Image ? Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject()) : nullptr)
						{
							Material->SetScalarParameterValue(TEXT("EquipmentFXPulseStart"), -1000.f);
						}
					}
					Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), CaptureSize, 0.f);
					Report->SetBoolField(FString::Printf(TEXT("fixed_phase_%d_saved"), Index), SaveImage(FString::Printf(TEXT("phase_%d.png"), Index)));
				}
			}
			const auto* Equipment = PlayWorld.IsValid() && PlayWorld->GetGameInstance() ? PlayWorld->GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
			Report->SetBoolField(TEXT("equipment_revision_unchanged"), Equipment && Equipment->GetState().Revision == RevisionBefore);
			Report->SetNumberField(TEXT("frames"), Frame); Report->SetBoolField(TEXT("all_pngs_saved"), bSaved && Frame == 40);
			Report->SetArrayField(TEXT("capture_time_seconds"), Timestamps);
			FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
			FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			Surface.Reset(); Target.Reset(); Renderer.Reset(); Cards.Empty();
			UE_LOG(LogTemp, Display, TEXT("EquipmentFX UI sequence finished: %d frames at %s"), Frame, *Directory);
		}

		bool SaveImage(const FString& Name)
		{
			FImage Image; TArray64<uint8> PNG;
			if (!FImageUtils::GetRenderTargetImage(Target.Get(), Image) || Image.Format != ERawImageFormat::BGRA8) return false;
			Image.GammaSpace = EGammaSpace::sRGB;
			return FImageUtils::CompressImage(PNG, TEXT("png"), Image)
				&& FFileHelper::SaveArrayToFile(PNG, *FPaths::Combine(Directory, Name));
		}

		bool Tick(float DeltaSeconds)
		{
			if (!PlayWorld.IsValid() || Frame >= 40) { Finish(); return false; }
			Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), CaptureSize, DeltaSeconds);
			if (WarmupRemaining > 0.f)
			{
				WarmupRemaining -= DeltaSeconds;
				LastCaptureTime = FPlatformTime::Seconds();
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			if (Now - LastCaptureTime < .1) return true;
			LastCaptureTime = Now;
			if (!bGradeFrames && Frame == 0)
			{
				Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), CaptureSize, 0.f);
				Report->SetBoolField(TEXT("baseline_png_saved"), SaveImage(TEXT("baseline.png")));
			}
			if (!bGradeFrames && Frame == 5)
			{
				for (int32 Index = 1; Index < Cards.Num(); ++Index)
				{
					Cards[Index]->TriggerIconPulse();
					UImage* Icon = EquipmentFXImage(Cards[Index].Get());
					UMaterialInstanceDynamic* MID = Icon ? Cast<UMaterialInstanceDynamic>(Icon->GetBrush().GetResourceObject()) : nullptr;
					const FString Prefix = FString::Printf(TEXT("pulse_card_%d_"), Index);
					Report->SetBoolField(Prefix + TEXT("mid_present"), MID != nullptr);
					if (MID)
					{
						Report->SetNumberField(Prefix + TEXT("start"), MID->K2_GetScalarParameterValue(TEXT("EquipmentFXPulseStart")));
						Report->SetNumberField(Prefix + TEXT("shine"), MID->K2_GetScalarParameterValue(TEXT("EquipmentFXShine")));
						Report->SetNumberField(Prefix + TEXT("strength"), MID->K2_GetScalarParameterValue(TEXT("EquipmentFXStrength")));
					}
				}
				Report->SetNumberField(TEXT("pulse_clock_at_trigger"), FPlatformTime::Seconds() - GStartTime);
				Report->SetNumberField(TEXT("pulse_trigger_frame"), Frame);
			}
			if (bGradeFrames && Frame == 0)
			{
				bool bAuthoredSizes = true, bSimpleCardTree = true, bSelectionMarks = true;
				int32 CardIndex = 0;
				for (const auto& Card : Cards)
				{
					const auto* CardFrame = Card ? Cast<UImage>(Card->GetWidgetFromName(TEXT("GradeFrame"))) : nullptr;
					const auto* CardImage = EquipmentFXImage(Card.Get());
					const auto* CardSelection = Card ? Cast<UImage>(Card->GetWidgetFromName(TEXT("SelectionMark"))) : nullptr;
					bAuthoredSizes &= Card->GetCachedGeometry().GetLocalSize().Equals(FVector2D(256.f), .1f)
						&& CardFrame && CardFrame->GetCachedGeometry().GetLocalSize().Equals(FVector2D(256.f), .1f)
						&& CardImage && CardImage->GetCachedGeometry().GetLocalSize().Equals(FVector2D(244.f), .1f);
					bSimpleCardTree &= CardImage && !Card->GetWidgetFromName(TEXT("IconCard")) && !Card->GetWidgetFromName(TEXT("NameText")) && !Card->GetWidgetFromName(TEXT("OwnerText"));
					bSelectionMarks &= CardSelection && CardSelection->GetVisibility() == (CardIndex++ >= 6 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				}
				Report->SetBoolField(TEXT("grade_frame_matrix_uses_authored_256_square_cards_and_244_icons"), bAuthoredSizes);
				Report->SetBoolField(TEXT("cards_have_direct_image_without_nested_item_card_or_name_owner_labels"), bSimpleCardTree);
				Report->SetBoolField(TEXT("grade_matrix_selection_marks_match_rows"), bSelectionMarks);
			}
			const bool bFrameSaved = SaveImage(FString::Printf(TEXT("frame_%03d.png"), Frame));
			if (!bGradeFrames && (Frame == 6 || Frame == 7))
			{
				for (int32 Index = 1; Index < Cards.Num(); ++Index)
				{
					UImage* Icon = EquipmentFXImage(Cards[Index].Get());
					UMaterialInstanceDynamic* MID = Icon ? Cast<UMaterialInstanceDynamic>(Icon->GetBrush().GetResourceObject()) : nullptr;
					if (MID) Report->SetNumberField(FString::Printf(TEXT("pulse_card_%d_frame_%d_start"), Index, Frame),
						MID->K2_GetScalarParameterValue(TEXT("EquipmentFXPulseStart")));
				}
				Report->SetNumberField(FString::Printf(TEXT("pulse_frame_%d_clock"), Frame), FPlatformTime::Seconds() - GStartTime);
			}
			bSaved &= bFrameSaved; Timestamps.Add(MakeShared<FJsonValueNumber>(Now)); ++Frame;
			return true;
		}
	};

	TSharedPtr<FEquipmentFXCapture> GEquipmentFXCapture;
	void StartEquipmentFXCapture(UWorld* CommandWorld, bool bGradeFrames)
	{
		if (GEquipmentFXCapture) { UE_LOG(LogTemp, Warning, TEXT("EquipmentFX UI capture already running")); return; }
		UWorld* InWorld = CommandWorld && CommandWorld->WorldType == EWorldType::PIE ? CommandWorld : nullptr;
		if (!InWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) { InWorld = Context.World(); break; }
		auto Run = MakeShared<FEquipmentFXCapture>();
		Run->bGradeFrames = bGradeFrames;
		if (!Run->Start(InWorld)) { UE_LOG(LogTemp, Error, TEXT("EquipmentFX UI capture requires PIE, registered cards and loaded equipment catalog/icon")); return; }
		GEquipmentFXCapture = Run;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float DeltaSeconds)
		{
			const bool bContinue = Run->Tick(DeltaSeconds); if (!bContinue) GEquipmentFXCapture.Reset(); return bContinue;
		}));
	}
	FAutoConsoleCommandWithWorld GCaptureEquipmentFXUI(TEXT("CGR.EquipmentFX.CaptureUI"),
		TEXT("Read-only PIE grade FX comparison, 864x304, 40 real-time frames; Saved/EmployeeEquipmentFX/UISequence."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld) { StartEquipmentFXCapture(CommandWorld, false); }));
	FAutoConsoleCommandWithWorld GCaptureEquipmentGradeFrames(TEXT("CGR.EquipmentFX.CaptureGradeFrames"),
		TEXT("Read-only six-grade square cards, unselected/selected rows at authored 256x256; Saved/EmployeeEquipmentFX/GradeFrames."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld) { StartEquipmentFXCapture(CommandWorld, true); }));
}

#endif
