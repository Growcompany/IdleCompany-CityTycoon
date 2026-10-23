#if WITH_EDITOR

#include "UI/Panel/EmployeeEquipmentPanelWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentListWidget.h"
#include "UI/Panel/ShopPanelWidget.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Panel/EmployeeWindowWidget.h"
#include "UI/Panel/EnhanceStarforceModalWidget.h"
#include "UI/Panel/PotentialOddsWidget.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Gacha/GachaCaptureStage.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "UI/Element/Employee/PotentialResetWorkspaceWidget.h"
#include "UI/Panel/PotentialTierUpOverlayWidget.h"
#include "Enum/WidgetType.h"
#include "UI/Element/Employee/EmployeeLevelUpCardWidget.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "Components/ListView.h"
#include "Data/EntityCardData.h"
#include "Data/EmployeeRosterRules.h"
#include "Data/EmployeePotentialData.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "CommonButtonBase.h"
#include "CommonTileView.h"
#include "CommonTextBlock.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Utils/EmployeeEquipmentAppearanceResolver.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ShopItemTable.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ComboBoxString.h"
#include "Components/TileView.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/CanvasPanel.h"
#include "Components/GridPanel.h"
#include "Components/WrapBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/WidgetSwitcher.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
// Justification / LineHeightPercentage 는 UTextLayoutWidget 의 protected 필드다 — 리플렉션 읽기용
#include "UObject/UnrealType.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

// EmployeeWindowWidget.cpp 가 마지막 빈자리 동기화에 실제로 쓴 폭/예산. 생산은 라이브 뷰포트 지오메트리로,
// 이 하네스는 캡처 해상도(2304/2560/3120)의 지오메트리로 잰다 — 두 폭이 갈리면 같은 목록이 두 예산을 갖는다.
// 판정을 생산이 쓴 예산으로 하고 둘 다 detail 에 남겨야 "결함" 과 "해상도 차이" 가 구분된다.
extern double GEmployeeRosterSyncWidth;
extern int32 GEmployeeRosterSyncBudget;
extern int32 GEmployeeRosterSyncHireWanted;

// 같은 파일이 내보내는 재동기화 진입점 — 캡처 해상도로 한 번 그린 뒤 그 폭으로 빈자리를 다시 맞춘다(룰링 b)
void SyncEmployeeRosterHireTilesForCapture(UListView* Strip, UObject* ItemOuter, int32 RemainingSeats);

static TArray<FString> ParseEquipmentUIArguments(const TArray<FString>& Args)
{
	const FString Joined = FString::Join(Args, TEXT(" "));
	const TCHAR* Cursor = *Joined;
	TArray<FString> Parsed; FString Token;
	while (FParse::Token(Cursor, Token, false)) Parsed.Add(Token);
	return Parsed;
}

// 잠재 명함 타일은 판 전체를 덮는 투명 버튼이 입력을 받는다 — 실제 클릭 핸들러 경로를 그대로 태운다
static bool ClickPotentialCardTile(UUserWidget* Potential, int32 Index)
{
	UButtonWidget* TileButton = Potential ? Cast<UButtonWidget>(Potential->GetWidgetFromName(*FString::Printf(TEXT("CardTileButton%d"), Index))) : nullptr;
	UFunction* Click = TileButton ? TileButton->FindFunction(TEXT("HandleButtonClicked")) : nullptr;
	if (!Click) return false;
	TileButton->ProcessEvent(Click, nullptr);
	return true;
}

static bool SaveEquipmentUIPNG(UTextureRenderTarget2D* RenderTarget, const FString& Path)
{
	FImage Image;
	if (!FImageUtils::GetRenderTargetImage(RenderTarget, Image) || Image.Format != ERawImageFormat::BGRA8)
	{
		UE_LOG(LogTemp, Error, TEXT("EquipmentUI PNG readback requires BGRA8: %s"), *Path);
		return false;
	}
	// FWidgetRenderer(true) already wrote gamma-encoded bytes into this linear-tagged target.
	Image.GammaSpace = EGammaSpace::sRGB;
	TArray64<uint8> Bytes;
	return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
}

// Uses the real PIE instance and registered classes. No equipment transaction or save is issued.
struct FEmployeeEquipmentUIValidation : TSharedFromThis<FEmployeeEquipmentUIValidation>
{
	TWeakObjectPtr<UWorld> PlayWorld;
	TStrongObjectPtr<UEmployeeEquipmentPanelWidget> Panel;
	TStrongObjectPtr<UEmployeeWindowWidget> EmbeddedWindow;
	TStrongObjectPtr<UShopPanelWidget> ShopWindow;
	TWeakObjectPtr<UUIBase> ShopRouteBase;
	TStrongObjectPtr<UEmployeeWindowWidget> ShopRouteWindow;
	TWeakObjectPtr<UShopPanelWidget> ShopRouteShop;
	TWeakObjectPtr<UPotentialResetWorkspaceWidget> ShopRoutePotential;
	TWeakObjectPtr<UCommonActivatableWidget> ShopRoutePreviousPrompt;
	TMap<EItemType, int32> ShopRouteItemsBefore;
	int32 ShopRoutePhase = 0, ShopRoutePromptCount = 0;
	double ShopRouteDeadline = 0.0;
	TWeakObjectPtr<UUserWidget> RenderRoot;
	TWeakObjectPtr<UEmployeeEquipmentWorkspaceWidget> Workspace;
	TWeakObjectPtr<UEmployeeEquipmentWorkspaceWidget> ActiveWorkspace;
	TUniquePtr<FWidgetRenderer> Renderer;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	TSharedPtr<SOverlay> Surface;
	// _fx_tierup 장면에서만 화면 위에 얹는 전체 화면 등급 상승 연출
	TStrongObjectPtr<UPotentialTierUpOverlayWidget> TierUpOverlayFixture;
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Captures, Checks;
	TArray<FGuid> ItemIds;
	FString OutputDirectory, CurrentScenario;
	int32 EmployeeId = INDEX_NONE, CaptureIndex = 0, CurrentWidth = 2304, CurrentHeight = 1440;
	float CurrentDPIScale = 1.f;
	// 2560 = 사용자 PIE 1920x1080 의 가상 폭 — 실제로 보는 화면이라 인라인 기준에 넣는다
	TArray<FIntPoint> CaptureSizes = {FIntPoint(2304, 1440), FIntPoint(2560, 1440), FIntPoint(3120, 1440)};
	FVector2D LogicalCaptureSize() const { return FVector2D(CurrentWidth, CurrentHeight) / CurrentDPIScale; }
	double PreparedAtSeconds = 0.0;
	double CaptureReadyAtSeconds = 0.0;
	uint64 PreparedFrame = 0;
	bool bScenarioReady = false, bInlineValidation = false, bMobileValidation = false, bPotentialValidation = false, bTrainingValidation = false, bColumnValidation = false;
	TArray<FString> Scenarios = { TEXT("equipment"), TEXT("inventory"), TEXT("gacha"), TEXT("gacha_advanced"), TEXT("gacha_premium"), TEXT("ticket_quote"), TEXT("shop"), TEXT("comparison"), TEXT("enhance"), TEXT("bulk"), TEXT("bulk_confirm"), TEXT("odds"), TEXT("results"), TEXT("reveal"), TEXT("employee_window_training"), TEXT("employee_window_equipment") };

	void Check(const FString& Name, bool bPassed, const FString& Detail = FString())
	{
		auto Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("name"), Name); Value->SetBoolField(TEXT("passed"), bPassed); Value->SetStringField(TEXT("detail"), Detail);
		Checks.Add(MakeShared<FJsonValueObject>(Value));
	}

	void ValidateOwnedList(UWorld* InWorld, const TArray<FString>& Args)
	{
		auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
		auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		auto* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
		if (!Equipment || !Tables || !Employees || Employees->GetAllEmployees().IsEmpty()) return;
		EmployeeId = Args.Num() > 1 && Args[1] != TEXT("auto") ? FCString::Atoi(*Args[1]) : Employees->GetAllEmployees()[0].EmployeeID;
		if (!Employees->GetEmployeeData(EmployeeId)) return;
		const FEmployeeEquipmentState Original = Equipment->GetState();
		const auto WindowClass = Tables->GetWidgetClass(EWidgetType::EmployeeWindow);
		TStrongObjectPtr<UEmployeeWindowWidget> Window(WindowClass ? CreateWidget<UEmployeeWindowWidget>(InWorld->GetFirstPlayerController(), WindowClass) : nullptr);
		if (!Window.IsValid()) { UE_LOG(LogTemp, Error, TEXT("EquipmentUI owned-list: EmployeeWindow class is unavailable")); return; }
		ON_SCOPE_EXIT
		{
			Equipment->RestoreState(Original);
			Window->DeactivateWidget();
			Window->RemoveFromParent();
		};
		Window->AddToViewport(200);
		Window->ActivateWidget();
		Window->OpenWithContext(EmployeeId, true);
		auto* List = Window->EquipmentList.Get();
		if (!List || !List->InventoryView || !List->PartCombo || !List->GradeCombo)
		{
			UE_LOG(LogTemp, Error, TEXT("EquipmentUI owned-list: required list widgets are missing"));
			return;
		}
		Report->SetStringField(TEXT("source"), TEXT("Registered EmployeeWindow and real owned equipment IDs. Generated card HandleButtonClicked exercises the CommonButton-to-list path; physical pointer hit testing is separate."));
		Check(TEXT("owned_list_uses_common_button_aware_tile_view"), List->InventoryView->IsA<UCommonTileView>());
		Report->SetStringField(TEXT("fixture_policy"), TEXT("Slot, missing-item and empty-state checks use synchronous in-memory equipment copies, restored before yielding. Effect semantics use copied items in an unregistered transient manager. No grants, transactions, equipment broadcasts or saves."));
		Report->SetNumberField(TEXT("employee_id"), EmployeeId);
		Report->SetNumberField(TEXT("owned_instance_count"), Original.Instances.Num());
		OutputDirectory = Args.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentOwnedListValidation")) : Args[0];
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*OutputDirectory);
		auto* SaveManager = Instance->GetSubsystem<USaveLoadManager>();
		const USaveGame_GameData* SavedGame = SaveManager ? SaveManager->GetCurrentSaveData() : nullptr;
		const FEmployeeEquipmentState SavedOriginal = SavedGame ? SavedGame->GameData.EmployeeEquipmentState : FEmployeeEquipmentState();
		TArray<TSharedPtr<FJsonValue>> SavedIds, OwnedItems;
		bool bMatchesSaved = SavedGame && Original.Instances.Num() == SavedOriginal.Instances.Num();
		for (const auto& Pair : SavedOriginal.Instances)
		{
			SavedIds.Add(MakeShared<FJsonValueString>(Pair.Key.ToString()));
			const auto* LiveItem = Original.Instances.Find(Pair.Key);
			bMatchesSaved &= LiveItem && FEmployeeEquipmentInstance::StaticStruct()->CompareScriptStruct(&Pair.Value, LiveItem, 0);
		}
		Report->SetArrayField(TEXT("saved_instance_ids"), SavedIds);
		Report->SetNumberField(TEXT("saved_instance_count"), SavedOriginal.Instances.Num());
		Check(TEXT("live_owned_instances_match_loaded_save_snapshot"), bMatchesSaved);
		if (Args.Num() > 2)
		{
			TStrongObjectPtr<USaveGame_GameData> Backup(Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromSlot(Args[2], 0)));
			Report->SetStringField(TEXT("backup_slot"), Args[2]);
			Report->SetNumberField(TEXT("backup_instance_count"), Backup.IsValid() ? Backup->GameData.EmployeeEquipmentState.Instances.Num() : -1);
			Check(TEXT("saved_equipment_state_matches_precheck_backup"), Backup.IsValid() && SavedGame
				&& FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Backup->GameData.EmployeeEquipmentState, &SavedOriginal, 0));
		}
		TArray<int32> ExpectedCounts;
		ExpectedCounts.Init(0, static_cast<int32>(EEmployeeEquipmentSlot::Count) + 1);
		TArray<TSharedPtr<FJsonValue>> OwnedIds, DisplayedIds;
		bool bAllDefinitionsValid = true;
		for (const auto& Pair : Original.Instances)
		{
			OwnedIds.Add(MakeShared<FJsonValueString>(Pair.Key.ToString()));
			auto ItemRecord = MakeShared<FJsonObject>();
			ItemRecord->SetStringField(TEXT("instance_id"), Pair.Key.ToString());
			ItemRecord->SetStringField(TEXT("definition_id"), Pair.Value.DefinitionId.ToString());
			ItemRecord->SetNumberField(TEXT("enhancement_level"), Pair.Value.EnhanceLevel);
			ItemRecord->SetNumberField(TEXT("equipped_employee_id"), Equipment->GetInstanceEmployee(Pair.Key));
			OwnedItems.Add(MakeShared<FJsonValueObject>(ItemRecord));
			const auto* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
			bAllDefinitionsValid &= Definition != nullptr && Pair.Key.IsValid() && Pair.Key == Pair.Value.InstanceId;
			if (!Definition) continue;
			++ExpectedCounts[0];
			const int32 PartIndex = static_cast<int32>(Definition->EquipmentSlot) + 1;
			if (ExpectedCounts.IsValidIndex(PartIndex)) ++ExpectedCounts[PartIndex];
			else bAllDefinitionsValid = false;
		}
		Check(TEXT("owned_instances_have_matching_ids_and_catalog_rows"), bAllDefinitionsValid);
		bool bExactOwnedList = List->InventoryView->GetNumItems() == Original.Instances.Num();
		bool bEquippedStatesMatch = true;
		TSet<FGuid> SeenIds;
		for (UObject* Value : List->InventoryView->GetListItems())
		{
			const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Value);
			const auto* Item = Entry ? Original.Instances.Find(Entry->InstanceID) : nullptr;
			const auto* Definition = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
			bEquippedStatesMatch &= Entry && Entry->bEquipped == (Equipment->GetInstanceEmployee(Entry->InstanceID) != INDEX_NONE);
			bExactOwnedList &= Entry && Item && Definition && !SeenIds.Contains(Entry->InstanceID)
				&& Entry->DefinitionID == Item->DefinitionId && Entry->Grade == Definition->Grade && Entry->EnhancementLevel == Item->EnhanceLevel;
			if (Entry)
			{
				SeenIds.Add(Entry->InstanceID);
				DisplayedIds.Add(MakeShared<FJsonValueString>(Entry->InstanceID.ToString()));
			}
		}
		Report->SetArrayField(TEXT("owned_instance_ids"), OwnedIds);
		Report->SetArrayField(TEXT("owned_items"), OwnedItems);
		Report->SetArrayField(TEXT("displayed_instance_ids"), DisplayedIds);
		Check(TEXT("displayed_list_exactly_matches_owned_instances"), bExactOwnedList);
		Check(TEXT("entry_equipped_states_match_actual_holders"), bEquippedStatesMatch);
		Check(TEXT("owned_count_header_matches_visible_ownership"), List->OwnedCountText
			&& List->OwnedCountText->GetText().ToString() == FText::AsNumber(ExpectedCounts[0]).ToString() + TEXT("개"));
		Check(TEXT("part_dropdown_visible_and_has_all_parts"), List->PartCombo->IsVisible()
			&& List->PartCombo->GetOptionCount() == ExpectedCounts.Num() && List->PartCombo->GetSelectedIndex() == 0);
		bool bGradesMatch = List->GradeCombo->IsVisible() && List->GradeCombo->GetSelectedIndex() == 0
			&& List->GradeCombo->GetOptionCount() == static_cast<int32>(EEmployeeEquipmentGrade::Count) + 1;
		for (int32 Index = 0; Index < static_cast<int32>(EEmployeeEquipmentGrade::Count); ++Index)
		{
			const auto* Grade = Tables->FindEmployeeEquipmentGrade(static_cast<EEmployeeEquipmentGrade>(Index));
			bGradesMatch &= Grade && List->GradeCombo->GetOptionAtIndex(Index + 1) == Grade->DisplayName.ToString();
		}
		Check(TEXT("grade_dropdown_visible_with_datatable_grade_names"), bGradesMatch);

		auto CheckSelection = [&](const FString& Name, const FGuid& ExpectedId)
		{
			const auto* Selected = List->InventoryView->GetSelectedItem<UEmployeeEquipmentItemObject>();
			bool bMatches = List->SelectedInstanceId == ExpectedId && Window->InlineDetailInstanceId == ExpectedId;
			if (!ExpectedId.IsValid())
			{
				Check(Name, bMatches && !Selected && (!Window->InlineDetail || !Window->InlineDetail->GetParent())
					&& Window->EquipDetailEmpty && Window->EquipDetailEmpty->IsVisible());
				return;
			}
			FEquipmentDetailContent Expected;
			bMatches &= Selected && Selected->InstanceID == ExpectedId && Window->InlineDetail
				&& Window->InlineDetail->GetParent() == Window->EquipDetailHost
				&& Window->EquipDetailEmpty && Window->EquipDetailEmpty->GetVisibility() == ESlateVisibility::Collapsed
				&& CGEquipmentDetail::BuildContent(*Equipment, *Tables, Employees, EmployeeId, ExpectedId, Expected);
			auto TextMatches = [&](FName WidgetName, const FText& Text)
			{
				const auto* Label = Window->InlineDetail ? Cast<UCommonTextBlock>(Window->InlineDetail->GetWidgetFromName(WidgetName)) : nullptr;
				return Label && Label->GetText().EqualTo(Text);
			};
			auto IconMatches = [&](FName WidgetName, UTexture2D* Texture, const FEmployeeEquipmentGradeRow* VisualGrade, bool bFocused)
			{
				const auto* Icon = Window->InlineDetail ? Cast<UImage>(Window->InlineDetail->GetWidgetFromName(WidgetName)) : nullptr;
				UObject* Resource = Icon ? Icon->GetBrush().GetResourceObject() : nullptr;
				const auto* ExpectedMaterial = Texture && VisualGrade && !VisualGrade->IconMaterial.IsNull()
					? VisualGrade->IconMaterial.LoadSynchronous() : nullptr;
				auto* Material = Cast<UMaterialInstanceDynamic>(Resource);
				return Icon && (ExpectedMaterial
					? Material && Material->Parent == ExpectedMaterial && Material->K2_GetTextureParameterValue(TEXT("IconTexture")) == Texture
						&& FMath::IsNearlyEqual(Material->K2_GetScalarParameterValue(TEXT("EquipmentFXStrength")),
							bFocused ? VisualGrade->IconFocusedStrength : VisualGrade->IconIdleStrength)
					: Resource == Texture);
			};
			bMatches &= TextMatches(TEXT("SelectedNameText"), Expected.Name) && TextMatches(TEXT("SelectedGradeText"), Expected.GradeName)
				&& TextMatches(TEXT("SelectedPartText"), Expected.PartName) && TextMatches(TEXT("SelectedEnhanceText"), FText())
				&& TextMatches(TEXT("SelectedStateText"), Expected.State)
				&& IconMatches(TEXT("SelectedIcon"), Expected.Icon, Expected.VisualGradeRow, true)
				&& IconMatches(TEXT("AfterIcon"), Expected.Icon, Expected.VisualGradeRow, true)
				&& IconMatches(TEXT("CurrentIcon"), Expected.CurrentIcon, Expected.CurrentVisualGradeRow, false);
			const auto* Detail = Window->InlineDetail.Get();
			const auto* Enhance = Detail ? Detail->GetWidgetFromName(TEXT("SelectedEnhanceText")) : nullptr;
			const auto* ItemRows = Detail ? Cast<UPanelWidget>(Detail->GetWidgetFromName(TEXT("ItemEffectRowsBox"))) : nullptr;
			const auto* ItemEmpty = Detail ? Cast<UCommonTextBlock>(Detail->GetWidgetFromName(TEXT("ItemNoEffectsText"))) : nullptr;
			const auto* ItemUnique = Detail ? Cast<UCommonTextBlock>(Detail->GetWidgetFromName(TEXT("ItemUniqueEffectsText"))) : nullptr;
			const auto* ItemSection = Detail ? Detail->GetWidgetFromName(TEXT("ItemEffectsSection")) : nullptr;
			const auto* Comparison = Detail ? Detail->GetWidgetFromName(TEXT("ComparisonSection")) : nullptr;
			const auto* Notice = Detail ? Detail->GetWidgetFromName(TEXT("NoticePanel")) : nullptr;
			bMatches &= Enhance && Enhance->GetVisibility() == ESlateVisibility::Collapsed
				&& ItemRows && ItemRows->GetChildrenCount() == Expected.ItemEffectRows.Num()
				&& ItemEmpty && ItemEmpty->GetText().EqualTo(Expected.ItemNoEffectsText)
				&& ItemEmpty->GetVisibility() == (Expected.ItemEffectRows.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed)
				&& ItemSection && ItemSection->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
				&& ItemUnique && ItemUnique->GetText().EqualTo(Expected.ItemUniqueEffects)
				&& ItemUnique->GetVisibility() == (Expected.ItemUniqueEffects.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible)
				&& Comparison && Comparison->GetVisibility() == (Expected.bShowComparison ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed)
				&& Notice && Notice->GetVisibility() == (Expected.Notice.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
			Check(Name, bMatches, ExpectedId.ToString());
		};
		auto CaptureOwnedList = [&](const FString& Scenario, bool bSelectionNotificationSettled = true)
		{
			FWidgetRenderer LayoutRenderer(true);
			for (int32 Width : {2304, 3120})
			{
				TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget(FWidgetRenderer::CreateTargetFor(FVector2D(Width, 1440), TF_Bilinear, false));
				if (!RenderTarget.IsValid()) { Check(TEXT("owned_list_render_target"), false); continue; }
				const auto SurfaceWidget = Window->TakeWidget();
				for (int32 Pass = 0; Pass < 3; ++Pass) LayoutRenderer.DrawWidget(RenderTarget.Get(), SurfaceWidget, FVector2D(Width, 1440), 0.f);
				const FString Filename = FString::Printf(TEXT("%s_%d.png"), *Scenario, Width);
				const bool bSaved = SaveEquipmentUIPNG(RenderTarget.Get(), FPaths::Combine(OutputDirectory, Filename));
				Check(Scenario + FString::Printf(TEXT("_%d_capture_saved"), Width), bSaved);
				auto CaptureRecord = MakeShared<FJsonObject>();
				CaptureRecord->SetStringField(TEXT("file"), Filename);
				CaptureRecord->SetNumberField(TEXT("width"), Width);
				CaptureRecord->SetNumberField(TEXT("height"), 1440);
				CaptureRecord->SetNumberField(TEXT("part_width"), List->PartCombo->GetCachedGeometry().GetLocalSize().X);
				CaptureRecord->SetNumberField(TEXT("grade_width"), List->GradeCombo->GetCachedGeometry().GetLocalSize().X);
				Captures.Add(MakeShared<FJsonValueObject>(CaptureRecord));
				const auto* Selected = List->InventoryView->GetSelectedItem<UEmployeeEquipmentItemObject>();
				if (Selected)
				{
					const auto* Card = List->InventoryView->GetEntryWidgetFromItem<UEmployeeEquipmentCardWidget>(Selected);
					const auto* Ring = Card ? Cast<UImage>(Card->GetWidgetFromName(TEXT("SelectionMark"))) : nullptr;
					// SObjectTableRow delivers selection to the entry on its next Slate active timer.
					if (bSelectionNotificationSettled)
						Check(Scenario + FString::Printf(TEXT("_%d_selected_tile_ring_visible"), Width), Ring
							&& Ring->GetVisibility() == ESlateVisibility::HitTestInvisible && Ring->GetBrush().GetResourceObject());
					else
						CaptureRecord->SetBoolField(TEXT("selection_ring_requires_live_slate_tick_verification"), true);
					const auto* Detail = Window->InlineDetail.Get();
					const auto* Dock = Detail ? Detail->GetWidgetFromName(TEXT("DetailDock")) : nullptr;
					const auto* BodyScroll = Detail ? Detail->GetWidgetFromName(TEXT("DetailBodyScroll")) : nullptr;
					const auto* Rows = Detail ? Cast<UPanelWidget>(Detail->GetWidgetFromName(TEXT("ItemEffectRowsBox"))) : nullptr;
					bool bInsideHost = false;
					if (Dock && Window->EquipDetailHost)
					{
						const FGeometry& HostGeometry = Window->EquipDetailHost->GetCachedGeometry();
						const FGeometry& DockGeometry = Dock->GetCachedGeometry();
						const FVector2D Start = HostGeometry.AbsoluteToLocal(DockGeometry.LocalToAbsolute(FVector2D::ZeroVector));
						const FVector2D End = HostGeometry.AbsoluteToLocal(DockGeometry.LocalToAbsolute(DockGeometry.GetLocalSize()));
						const FVector2D HostSize = HostGeometry.GetLocalSize();
						bInsideHost = HostSize.X > 0.f && HostSize.Y > 0.f && End.X > Start.X && End.Y > Start.Y
							&& Start.X >= -.5f && Start.Y >= -.5f && End.X <= HostSize.X + .5f && End.Y <= HostSize.Y + .5f;
						CaptureRecord->SetNumberField(TEXT("detail_host_width"), HostSize.X);
						CaptureRecord->SetNumberField(TEXT("detail_host_height"), HostSize.Y);
						CaptureRecord->SetNumberField(TEXT("detail_left_in_host"), Start.X);
						CaptureRecord->SetNumberField(TEXT("detail_right_in_host"), End.X);
					}
					Check(Scenario + FString::Printf(TEXT("_%d_inline_detail_stays_inside_host"), Width), bInsideHost);
					float RequiredBodyHeight = 0.f;
					if (BodyScroll)
					{
						const FGeometry& ScrollGeometry = BodyScroll->GetCachedGeometry();
						const auto* FirstRow = Rows ? Rows->GetChildAt(0) : nullptr;
						const auto* LastRow = Rows ? Rows->GetChildAt(FMath::Min(1, Rows->GetChildrenCount() - 1)) : nullptr;
						if (!FirstRow || !LastRow)
							FirstRow = LastRow = Detail->GetWidgetFromName(TEXT("ItemNoEffectsText"));
						if (FirstRow && LastRow)
						{
							const FVector2D Start = ScrollGeometry.AbsoluteToLocal(FirstRow->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector));
							const FVector2D End = ScrollGeometry.AbsoluteToLocal(LastRow->GetCachedGeometry().LocalToAbsolute(LastRow->GetDesiredSize()));
							RequiredBodyHeight = End.Y - Start.Y;
						}
						CaptureRecord->SetNumberField(TEXT("detail_effects_height"), ScrollGeometry.GetLocalSize().Y);
						CaptureRecord->SetNumberField(TEXT("detail_readable_rows_height"), RequiredBodyHeight);
					}
					Check(Scenario + FString::Printf(TEXT("_%d_inline_effects_show_readable_rows"), Width), BodyScroll && RequiredBodyHeight > 0.f
						&& BodyScroll->GetCachedGeometry().GetLocalSize().Y + .5f >= RequiredBodyHeight);
				}
			}
		};
		CheckSelection(TEXT("entry_keeps_selection_empty_until_an_item_is_picked"), FGuid());
		CaptureOwnedList(TEXT("owned_initial"));
		// 핸들러 직접 호출로 포인터 상태와 회전을 검증한다. 실제 Slate 라우팅/캡처는 이 검사 범위 밖이다.
		AGachaCaptureStage* PortraitStage = Window->LiveStage;
		AFunnyOfficeworker* PortraitWorker = nullptr;
		int32 StageWorkerCount = 0;
		const USceneCaptureComponent2D* PortraitCapture = PortraitStage
			? PortraitStage->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
		if (PortraitCapture)
		{
			for (const TObjectPtr<AActor>& ShownActor : PortraitCapture->ShowOnlyActors)
			{
				if (AFunnyOfficeworker* ShownWorker = Cast<AFunnyOfficeworker>(ShownActor.Get()))
				{
					PortraitWorker = ShownWorker;
					++StageWorkerCount;
				}
			}
		}
		const FGeometry PortraitGeometry = Window->HeroPortraitImage
			? Window->HeroPortraitImage->GetCachedGeometry() : FGeometry();
		const FVector2D PortraitSize = PortraitGeometry.GetLocalSize();
		const bool bPortraitReady = PortraitStage && PortraitStage->IsHeldBy(Window.Get())
			&& StageWorkerCount == 1 && PortraitWorker && Window->HeroPortraitImage
			&& Window->HeroPortraitImage->GetVisibility() == ESlateVisibility::Visible
			&& PortraitSize.X > 0.f && PortraitSize.Y > 0.f;
		Check(TEXT("portrait_rotation_has_single_owned_worker_and_valid_image_geometry"), bPortraitReady);
		if (bPortraitReady)
		{
			const FVector2D Center = PortraitGeometry.LocalToAbsolute(PortraitSize * .5f);
			const FVector2D Right = PortraitGeometry.LocalToAbsolute(FVector2D(PortraitSize.X * .6f, PortraitSize.Y * .5f));
			const FVector2D Down = PortraitGeometry.LocalToAbsolute(FVector2D(PortraitSize.X * .6f, PortraitSize.Y * .6f));
			const FVector2D Outside = PortraitGeometry.LocalToAbsolute(FVector2D(-10.f, -10.f));
			const TSet<FKey> PressedButtons = { EKeys::LeftMouseButton };
			auto Mouse = [&PressedButtons](const FVector2D& Position, const FVector2D& Previous)
			{
				return FPointerEvent(0, Position, Previous, PressedButtons, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
			};
			auto Touch = [](uint32 PointerIndex, const FVector2D& Position, const FVector2D& Previous)
			{
				return FPointerEvent(0, PointerIndex, Position, Previous, 1.f, true);
			};
			const float InitialYaw = PortraitWorker->GetActorRotation().Yaw;
			Check(TEXT("portrait_rotation_rejects_wrong_holder"), !PortraitStage->RotatePreview(PortraitWorker, 30.f)
				&& FMath::IsNearlyEqual(PortraitWorker->GetActorRotation().Yaw, InitialYaw));
			Check(TEXT("portrait_rotation_ignores_press_outside_image"),
				!Window->NativeOnMouseButtonDown(PortraitGeometry, Mouse(Outside, Outside)).IsEventHandled()
				&& Window->HeroRotationPointerIndex == INDEX_NONE);
			const bool bMouseDown = Window->NativeOnMouseButtonDown(PortraitGeometry, Mouse(Center, Center)).IsEventHandled();
			const bool bMouseMove = Window->NativeOnMouseMove(PortraitGeometry, Mouse(Right, Center)).IsEventHandled();
			const float MouseYaw = PortraitWorker->GetActorRotation().Yaw;
			Check(TEXT("portrait_mouse_horizontal_drag_rotates_by_image_width"), bMouseDown && bMouseMove
				&& FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(InitialYaw, MouseYaw), -18.f, .1f));
			Window->NativeOnMouseMove(PortraitGeometry, Mouse(Down, Right));
			Check(TEXT("portrait_mouse_vertical_drag_does_not_rotate"),
				FMath::IsNearlyEqual(PortraitWorker->GetActorRotation().Yaw, MouseYaw, .01f));
			Window->NativeOnMouseButtonUp(PortraitGeometry, Mouse(Down, Down));
			Window->NativeOnMouseMove(PortraitGeometry, Mouse(Right, Down));
			Check(TEXT("portrait_mouse_release_stops_rotation"), Window->HeroRotationPointerIndex == INDEX_NONE
				&& FMath::IsNearlyEqual(PortraitWorker->GetActorRotation().Yaw, MouseYaw, .01f));
			const bool bTouchDown = Window->NativeOnTouchStarted(PortraitGeometry, Touch(1, Center, Center)).IsEventHandled();
			Window->NativeOnTouchMoved(PortraitGeometry, Touch(2, Right, Center));
			Window->NativeOnTouchEnded(PortraitGeometry, Touch(2, Right, Right));
			Check(TEXT("portrait_second_finger_cannot_rotate_or_end_first"), bTouchDown
				&& Window->HeroRotationPointerIndex == 1
				&& FMath::IsNearlyEqual(PortraitWorker->GetActorRotation().Yaw, MouseYaw, .01f));
			Window->NativeOnTouchMoved(PortraitGeometry, Touch(1, Right, Center));
			const float TouchYaw = PortraitWorker->GetActorRotation().Yaw;
			Check(TEXT("portrait_matching_touch_rotates"),
				FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(MouseYaw, TouchYaw), -18.f, .1f));
			Window->NativeOnMouseCaptureLost(FCaptureLostEvent(0, 1));
			Window->NativeOnTouchMoved(PortraitGeometry, Touch(1, Down, Right));
			Check(TEXT("portrait_capture_loss_stops_rotation"), Window->HeroRotationPointerIndex == INDEX_NONE
				&& FMath::IsNearlyEqual(PortraitWorker->GetActorRotation().Yaw, TouchYaw, .01f));
			PortraitStage->RotatePreview(Window.Get(), FMath::FindDeltaAngleDegrees(TouchYaw, InitialYaw));
		}
		for (int32 Pass = 0; Pass < 3; ++Pass) List->Refresh();
		Check(TEXT("repeated_refresh_keeps_exact_part_counts"), List->PartCounts == ExpectedCounts);

		for (int32 PartIndex = 1; PartIndex < ExpectedCounts.Num(); ++PartIndex)
		{
			const FGuid PreviousId = List->SelectedInstanceId;
			List->PartCombo->SetSelectedIndex(PartIndex);
			FGuid ExpectedSelection;
			bool bFiltered = !List->bAllSlots && static_cast<int32>(List->CurrentSlot) + 1 == PartIndex
				&& List->InventoryView->GetNumItems() == ExpectedCounts[PartIndex];
			for (UObject* Value : List->InventoryView->GetListItems())
			{
				const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Value);
				const auto* Item = Entry ? Original.Instances.Find(Entry->InstanceID) : nullptr;
				const auto* Definition = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
				bFiltered &= Definition && static_cast<int32>(Definition->EquipmentSlot) + 1 == PartIndex;
				if (Entry && Entry->InstanceID == PreviousId) ExpectedSelection = PreviousId;
			}
			Check(FString::Printf(TEXT("part_%d_lists_only_owned_matching_instances"), PartIndex), bFiltered);
			CheckSelection(FString::Printf(TEXT("part_%d_preserves_visible_selection_or_clears_detail"), PartIndex), ExpectedSelection);
		}
		List->PartCombo->SetSelectedIndex(0);
		Check(TEXT("filter_changes_do_not_accumulate_counts"), List->PartCounts == ExpectedCounts);
		for (int32 PartIndex = 0; PartIndex < ExpectedCounts.Num(); ++PartIndex)
		{
			List->PartCombo->SetSelectedIndex(PartIndex);
			TArray<int32> ExpectedGradeCounts;
			ExpectedGradeCounts.Init(0, static_cast<int32>(EEmployeeEquipmentGrade::Count) + 1);
			for (const auto& Pair : Original.Instances)
			{
				const auto* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
				if (!Definition || (PartIndex != 0 && static_cast<int32>(Definition->EquipmentSlot) + 1 != PartIndex)) continue;
				++ExpectedGradeCounts[0];
				const int32 GradeIndex = static_cast<int32>(Definition->Grade) + 1;
				if (ExpectedGradeCounts.IsValidIndex(GradeIndex)) ++ExpectedGradeCounts[GradeIndex];
			}
			for (int32 GradeIndex = 0; GradeIndex < ExpectedGradeCounts.Num(); ++GradeIndex)
			{
				const FGuid PreviousId = List->SelectedInstanceId;
				List->GradeCombo->SetSelectedIndex(GradeIndex);
				bool bMatches = List->InventoryView->GetNumItems() == ExpectedGradeCounts[GradeIndex]
					&& List->GradeCounts == ExpectedGradeCounts && List->PartCounts == ExpectedCounts;
				FGuid ExpectedSelection;
				for (UObject* Value : List->InventoryView->GetListItems())
				{
					const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Value);
					const auto* Item = Entry ? Original.Instances.Find(Entry->InstanceID) : nullptr;
					const auto* Definition = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
					bMatches &= Definition && (PartIndex == 0 || static_cast<int32>(Definition->EquipmentSlot) + 1 == PartIndex)
						&& (GradeIndex == 0 || static_cast<int32>(Definition->Grade) + 1 == GradeIndex);
					if (Entry && Entry->InstanceID == PreviousId) ExpectedSelection = PreviousId;
				}
				Check(FString::Printf(TEXT("filter_part_%d_grade_%d_owned_rows_and_counts"), PartIndex, GradeIndex), bMatches);
				CheckSelection(FString::Printf(TEXT("filter_part_%d_grade_%d_selection_and_detail"), PartIndex, GradeIndex), ExpectedSelection);
				if (PartIndex == 0 && GradeIndex == 1 && ExpectedSelection.IsValid()) CaptureOwnedList(TEXT("owned_common_filtered"));
			}
		}
		List->SelectAll();
		// 홈 초상화의 여섯 실제 버튼에서 장비 칼럼·부위 필터·장착품 상세까지 이어지는 경로.
		TArray<const FEmployeeEquipmentDefinitionRow*> SlotDefinitions;
		SlotDefinitions.Init(nullptr, static_cast<int32>(EEmployeeEquipmentSlot::Count));
		for (const auto& Pair : Tables->GetEmployeeEquipmentDefinitions())
		{
			const int32 Index = static_cast<int32>(Pair.Value.EquipmentSlot);
			if (SlotDefinitions.IsValidIndex(Index) && !SlotDefinitions[Index]) SlotDefinitions[Index] = &Pair.Value;
		}
		const bool bAllSlotDefinitions = !SlotDefinitions.Contains(nullptr);
		Check(TEXT("hero_slot_navigation_has_all_six_catalog_parts"), bAllSlotDefinitions);
		if (bAllSlotDefinitions)
		{
			FEmployeeEquipmentState SlotFixture = Original;
			SlotFixture.Loadouts.Remove(EmployeeId);
			TArray<FGuid> SlotItemIds;
			for (int32 Index = 0; Index < SlotDefinitions.Num(); ++Index)
			{
				FEmployeeEquipmentInstance FixtureItem;
				FixtureItem.InstanceId = FGuid::NewGuid();
				FixtureItem.DefinitionId = SlotDefinitions[Index]->DefinitionId;
				FixtureItem.bLocked = Index == 0;
				SlotFixture.Instances.Add(FixtureItem.InstanceId, FixtureItem);
				SlotFixture.Loadouts.FindOrAdd(EmployeeId).Slots.Add(static_cast<EEmployeeEquipmentSlot>(Index), FixtureItem.InstanceId);
				SlotItemIds.Add(FixtureItem.InstanceId);
			}
			Equipment->RestoreState(SlotFixture);
			UButtonWidget* HeroSlots[] = {Window->HeroEquipmentSlot0, Window->HeroEquipmentSlot1, Window->HeroEquipmentSlot2,
				Window->HeroEquipmentSlot3, Window->HeroEquipmentSlot4, Window->HeroEquipmentSlot5};
			auto ClickHeroSlot = [](UButtonWidget* HeroSlot)
			{
				if (!HeroSlot) return;
				if (UFunction* ClickHandler = HeroSlot->FindFunction(TEXT("HandleButtonClicked")))
					HeroSlot->ProcessEvent(ClickHandler, nullptr);
				else
					HeroSlot->OnClicked().Broadcast();
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(HeroSlots); ++Index)
			{
				Window->ShowEmployeePage(EEmployeeWindowPage::Home);
				const int32 OtherGrade = (static_cast<int32>(SlotDefinitions[Index]->Grade) + 1)
					% static_cast<int32>(EEmployeeEquipmentGrade::Count);
				List->GradeCombo->SetSelectedIndex(OtherGrade + 1);
				UButtonWidget* HeroSlot = HeroSlots[Index];
				Check(FString::Printf(TEXT("hero_slot_%d_click_bound"), Index), HeroSlot && HeroSlot->GetIsEnabled()
					&& HeroSlot->OnClicked().IsBoundToObject(Window.Get()));
				ClickHeroSlot(HeroSlot);
				Check(FString::Printf(TEXT("hero_slot_%d_click_opens_equipped_part_and_clears_grade"), Index),
					Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment && List->ContextEmployeeId == EmployeeId
					&& !List->bAllSlots && static_cast<int32>(List->CurrentSlot) == Index
					&& List->PartCombo->GetSelectedIndex() == Index + 1
					&& List->CurrentGrade == EEmployeeEquipmentGrade::Count && List->GradeCombo->GetSelectedIndex() == 0);
				CheckSelection(FString::Printf(TEXT("hero_slot_%d_click_selects_exact_equipped_detail"), Index), SlotItemIds[Index]);
			}
			FEmployeeEquipmentState EmptySlotFixture = SlotFixture;
			EmptySlotFixture.Instances.Empty();
			EmptySlotFixture.Loadouts.Empty();
			Equipment->RestoreState(EmptySlotFixture);
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(HeroSlots); ++Index)
			{
				Window->ShowEmployeePage(EEmployeeWindowPage::Home);
				ClickHeroSlot(HeroSlots[Index]);
				Check(FString::Printf(TEXT("hero_slot_%d_empty_owned_opens_filtered_list"), Index),
					Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment && !List->bAllSlots
					&& static_cast<int32>(List->CurrentSlot) == Index && List->InventoryView->GetNumItems() == 0);
				CheckSelection(FString::Printf(TEXT("hero_slot_%d_empty_owned_clears_detail"), Index), FGuid());
			}
			Equipment->RestoreState(Original);
			Window->ShowEmployeePage(EEmployeeWindowPage::Equipment);
			List->SelectAll();
		}
		Report->SetBoolField(TEXT("second_owned_item_available"), List->InventoryView->GetNumItems() > 1);
		if (List->InventoryView->GetNumItems() > 1)
		{
			auto ReadPulseStart = [](UUserWidget* Widget, FName IconName)
			{
				const UImage* Icon = Widget ? Cast<UImage>(Widget->GetWidgetFromName(IconName)) : nullptr;
				auto* Material = Icon ? Cast<UMaterialInstanceDynamic>(Icon->GetBrush().GetResourceObject()) : nullptr;
				return Material ? Material->K2_GetScalarParameterValue(TEXT("EquipmentFXPulseStart")) : -1000.f;
			};
			const auto* First = Cast<UEmployeeEquipmentItemObject>(List->InventoryView->GetItemAt(0));
			const FGuid FirstId = First->InstanceID;
			const bool bFirstCardDeferred = !List->InventoryView->GetEntryWidgetFromItem<UEmployeeEquipmentCardWidget>(First);
			Window->OpenEquipmentItem(EmployeeId, FirstId);
			const float FirstDetailPulse = ReadPulseStart(Window->InlineDetail, TEXT("SelectedIcon"));
			Check(TEXT("explicit_new_item_open_starts_inline_detail_pulse"), FirstDetailPulse >= 0.f);
			Report->SetBoolField(TEXT("selection_before_card_generation_exercised"), bFirstCardDeferred);
			List->SelectAll();
			List->Refresh();
			Check(TEXT("refresh_before_card_generation_does_not_restart_detail_pulse"),
				ReadPulseStart(Window->InlineDetail, TEXT("SelectedIcon")) == FirstDetailPulse);
			auto* Second = Cast<UEmployeeEquipmentItemObject>(List->InventoryView->GetItemAt(1));
			const FGuid SecondId = Second->InstanceID;
			List->InventoryView->ScrollIndexIntoView(1);
			FWidgetRenderer ClickRenderer(true);
			TStrongObjectPtr<UTextureRenderTarget2D> ClickTarget(FWidgetRenderer::CreateTargetFor(FVector2D(2304, 1440), TF_Bilinear, false));
			if (ClickTarget.IsValid())
			{
				for (int32 Pass = 0; Pass < 3; ++Pass)
					ClickRenderer.DrawWidget(ClickTarget.Get(), Window->TakeWidget(), FVector2D(2304, 1440), 0.f);
			}
			auto* SecondCard = List->InventoryView->GetEntryWidgetFromItem<UEmployeeEquipmentCardWidget>(Second);
			UFunction* ClickFunction = SecondCard ? SecondCard->FindFunction(TEXT("HandleButtonClicked")) : nullptr;
			Check(TEXT("second_tile_generated_and_button_click_available"), ClickFunction && SecondCard->IsInteractionEnabled()
				&& SecondCard->GetListItem<UEmployeeEquipmentItemObject>() == Second);
			const float BeforeCardPulse = ReadPulseStart(SecondCard, TEXT("ItemImage"));
			const float BeforeDetailPulse = ReadPulseStart(Window->InlineDetail, TEXT("SelectedIcon"));
			if (ClickFunction) SecondCard->ProcessEvent(ClickFunction, nullptr);
			CheckSelection(TEXT("second_tile_click_updates_selection_and_detail"), SecondId);
			const float CardPulse = ReadPulseStart(SecondCard, TEXT("ItemImage"));
			const float DetailPulse = ReadPulseStart(Window->InlineDetail, TEXT("SelectedIcon"));
			Check(TEXT("new_tile_click_starts_card_and_inline_detail_pulse"), CardPulse >= 0.f && DetailPulse >= 0.f
				&& CardPulse > BeforeCardPulse && DetailPulse > BeforeDetailPulse);
			if (ClickFunction) SecondCard->ProcessEvent(ClickFunction, nullptr);
			Check(TEXT("same_tile_click_does_not_restart_card_or_detail_pulse"),
				ReadPulseStart(SecondCard, TEXT("ItemImage")) == CardPulse
				&& ReadPulseStart(Window->InlineDetail, TEXT("SelectedIcon")) == DetailPulse);
			CaptureOwnedList(TEXT("owned_second_selected"), false);
			List->Refresh();
			CheckSelection(TEXT("refresh_restores_selection_by_instance_id"), SecondId);
			Check(TEXT("refresh_does_not_restart_inline_detail_pulse"),
				ReadPulseStart(Window->InlineDetail, TEXT("SelectedIcon")) == DetailPulse);
			Window->OpenEquipmentItem(EmployeeId, SecondId);
			CheckSelection(TEXT("external_item_open_synchronizes_list_and_detail"), SecondId);
			Check(TEXT("same_item_external_open_does_not_restart_inline_detail_pulse"),
				ReadPulseStart(Window->InlineDetail, TEXT("SelectedIcon")) == DetailPulse);
		}
		bool bSelectedOtherRosterItem = false;
		for (UObject* RosterItem : Window->RosterListView->GetListItems())
		{
			const auto* CardData = Cast<UEntityCardData>(RosterItem);
			if (!CardData || CardData->bIsHireSlot || CardData->EmployeeInfo.EmployeeID == EmployeeId) continue;
			Window->RosterListView->SetSelectedItem(RosterItem);
			bSelectedOtherRosterItem = true;
			Check(TEXT("equipment_roster_selection_updates_selected_and_preview_employee"),
				Window->SelectedEmployeeID == CardData->EmployeeInfo.EmployeeID
				&& Window->LiveViewEmployeeID == CardData->EmployeeInfo.EmployeeID
				&& Window->LiveStage && Window->LiveStage->IsHeldBy(Window.Get()));
			Window->SelectEmployee(EmployeeId);
			break;
		}
		Check(TEXT("equipment_roster_other_selection_exercised"), bSelectedOtherRosterItem);
		for (const FEmployeeInstance& OtherEmployee : Employees->GetAllEmployees())
		{
			if (OtherEmployee.EmployeeID == EmployeeId) continue;
			const int32 PreviousEmployeeId = EmployeeId;
			const FGuid PreviousSelection = List->SelectedInstanceId;
			Window->SelectEmployee(OtherEmployee.EmployeeID);
			EmployeeId = OtherEmployee.EmployeeID;
			Check(TEXT("employee_switch_reconfigures_equipment_context"), List->ContextEmployeeId == EmployeeId);
			Check(TEXT("equipment_roster_switch_updates_live_portrait_employee"),
				Window->LiveViewEmployeeID == EmployeeId && Window->LiveStage && Window->LiveStage->IsHeldBy(Window.Get()));
			CheckSelection(TEXT("employee_switch_refreshes_selected_item_comparison"), PreviousSelection);
			Window->SelectEmployee(PreviousEmployeeId);
			EmployeeId = PreviousEmployeeId;
			Check(TEXT("equipment_roster_return_updates_live_portrait_employee"),
				Window->LiveViewEmployeeID == EmployeeId && Window->LiveStage && Window->LiveStage->IsHeldBy(Window.Get()));
			break;
		}

		if (!Original.Instances.IsEmpty())
		{
			List->SelectAll();
			FEmployeeEquipmentState Removed = Original;
			Removed.Instances.Remove(List->SelectedInstanceId);
			Equipment->RestoreState(Removed);
			List->Refresh();
			CheckSelection(TEXT("removed_selection_clears_detail_without_picking_another_item"), FGuid());
		}
		FEmployeeEquipmentState Empty = Original;
		Empty.Instances.Empty();
		Empty.Loadouts.Empty();
		Equipment->RestoreState(Empty);
		List->Refresh();
		Check(TEXT("empty_inventory_has_no_tiles_and_zero_counts"), List->InventoryView->GetNumItems() == 0
			&& !List->PartCounts.ContainsByPredicate([](int32 Count) { return Count != 0; }));
		CheckSelection(TEXT("empty_inventory_clears_previous_detail"), FGuid());
		Equipment->RestoreState(Original);
		List->SelectAll();
		CheckSelection(TEXT("restored_inventory_keeps_selection_empty"), FGuid());
		const auto* Economy = Tables->GetEmployeeEquipmentEconomy();
		const FEmployeeEquipmentInstance* UniqueSource = nullptr;
		const FEmployeeEquipmentDefinitionRow* UniqueDefinition = nullptr;
		FEquipmentEffectLoadout ValidEffects;
		for (const auto& Pair : Original.Instances)
		{
			const auto* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
			if (!Economy || !Definition || Definition->UniqueEffectIds.IsEmpty()) continue;
			const auto Effects = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(
				*Definition, Economy->MaxEnhanceLevel, Tables->GetEmployeeEquipmentEffects(), *Economy);
			if (!Effects.bValid || Effects.UniqueEffects.IsEmpty()) continue;
			UniqueSource = &Pair.Value;
			UniqueDefinition = Definition;
			ValidEffects = Effects;
			break;
		}
		Report->SetBoolField(TEXT("copied_unique_effect_fixture_available"), UniqueSource != nullptr);
		if (UniqueSource)
		{
			TStrongObjectPtr<UGameInstance> FixtureInstance(NewObject<UGameInstance>());
			TStrongObjectPtr<UEmployeeEquipmentManagerSubsystem> FixtureEquipment(
				NewObject<UEmployeeEquipmentManagerSubsystem>(FixtureInstance.Get()));
			FEmployeeEquipmentInstance ValidItem = *UniqueSource;
			ValidItem.EnhanceLevel = Economy->MaxEnhanceLevel;
			FEmployeeEquipmentState EffectFixture;
			EffectFixture.Instances.Add(ValidItem.InstanceId, ValidItem);
			EffectFixture.Loadouts.FindOrAdd(EmployeeId).Slots.Add(UniqueDefinition->EquipmentSlot, ValidItem.InstanceId);
			FixtureEquipment->RestoreState(EffectFixture);
			FEquipmentDetailContent Content;
			bool bUniqueDetails = CGEquipmentDetail::BuildContent(*FixtureEquipment, *Tables, Employees, EmployeeId, ValidItem.InstanceId, Content)
				&& Content.ItemEffectRows.Num() == ValidEffects.BasicValues.Num()
				&& Content.Rows.IsEmpty() && Content.Uniques.IsEmpty() && !Content.bShowComparison;
			const FString UniqueDetails = Content.ItemUniqueEffects.ToString();
			int32 NoticePosition = 0;
			for (FName EffectId : UniqueDefinition->UniqueEffectIds)
			{
				const auto* Effect = ValidEffects.UniqueEffects.Find(EffectId);
				if (!Effect) { bUniqueDetails = false; continue; }
				for (const FText& Label : {Effect->DisplayName, Effect->Description})
				{
					if (Label.IsEmpty()) continue;
					const FString Text = Label.ToString();
					const int32 Found = UniqueDetails.Find(Text, ESearchCase::CaseSensitive, ESearchDir::FromStart, NoticePosition);
					bUniqueDetails &= Found != INDEX_NONE;
					if (Found != INDEX_NONE) NoticePosition = Found + Text.Len();
				}
			}
			Check(TEXT("valid_equipped_item_lists_intrinsic_effects_and_unique_descriptions"), bUniqueDetails
				&& !Content.Notice.ToString().Contains(UniqueDetails), ValidItem.DefinitionId.ToString());
			Check(TEXT("item_effects_remain_visible_without_target_employee"),
				CGEquipmentDetail::BuildContent(*FixtureEquipment, *Tables, Employees, INDEX_NONE, ValidItem.InstanceId, Content)
				&& Content.ItemEffectRows.Num() == ValidEffects.BasicValues.Num()
				&& Content.ItemUniqueEffects.ToString() == UniqueDetails
				&& !Content.bShowComparison && !Content.bShowSwap && Content.Rows.IsEmpty(), ValidItem.DefinitionId.ToString());
			EffectFixture.Instances.FindChecked(ValidItem.InstanceId).EnhanceLevel = Economy->MaxEnhanceLevel + 1;
			FixtureEquipment->RestoreState(EffectFixture);
			Check(TEXT("invalid_equipped_item_shows_unavailable_effects_separately_from_notice"),
				CGEquipmentDetail::BuildContent(*FixtureEquipment, *Tables, Employees, EmployeeId, ValidItem.InstanceId, Content)
				&& Content.ItemEffectRows.IsEmpty() && Content.ItemUniqueEffects.IsEmpty()
				&& Content.ItemNoEffectsText.ToString() == TEXT("장비 효과를 확인할 수 없습니다")
				&& Content.Notice.ToString().Contains(TEXT("효과가 적용되지 않습니다"))
				&& !Content.bShowComparison);
			FEmployeeEquipmentInstance Candidate = ValidItem;
			Candidate.InstanceId = FGuid::NewGuid();
			Candidate.EnhanceLevel = Economy->MaxEnhanceLevel;
			EffectFixture.Instances.Add(Candidate.InstanceId, Candidate);
			bool bOtherInvalidAvailable = false;
			for (const auto& Pair : Original.Instances)
			{
				const auto* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
				if (!Definition || Definition->EquipmentSlot == UniqueDefinition->EquipmentSlot) continue;
				FEmployeeEquipmentInstance OtherInvalid = Pair.Value;
				OtherInvalid.EnhanceLevel = Economy->MaxEnhanceLevel + 1;
				EffectFixture.Instances.Add(OtherInvalid.InstanceId, OtherInvalid);
				EffectFixture.Loadouts.FindChecked(EmployeeId).Slots.Add(Definition->EquipmentSlot, OtherInvalid.InstanceId);
				FixtureEquipment->RestoreState(EffectFixture);
				Check(TEXT("remaining_invalid_loadout_shows_notice_without_zero_change_rows"),
					CGEquipmentDetail::BuildContent(*FixtureEquipment, *Tables, Employees, EmployeeId, Candidate.InstanceId, Content)
					&& Content.Rows.IsEmpty() && Content.Uniques.IsEmpty()
					&& Content.bShowComparison && !Content.ItemEffectRows.IsEmpty()
					&& Content.NoChangeText.ToString() == TEXT("적용되는 장비 효과 없음")
					&& Content.Notice.ToString().Contains(TEXT("변경 후 장착 구성에 유효하지 않은 장비")));
				EffectFixture.Instances.Remove(OtherInvalid.InstanceId);
				EffectFixture.Loadouts.FindChecked(EmployeeId).Slots.Remove(Definition->EquipmentSlot);
				bOtherInvalidAvailable = true;
				break;
			}
			Report->SetBoolField(TEXT("copied_second_part_fixture_available"), bOtherInvalidAvailable);
			FixtureEquipment->RestoreState(EffectFixture);
			bool bRecovery = CGEquipmentDetail::BuildContent(*FixtureEquipment, *Tables, Employees, EmployeeId, Candidate.InstanceId, Content)
				&& !Content.Rows.IsEmpty() && Content.Uniques.Num() == ValidEffects.UniqueEffects.Num()
				&& Content.ItemEffectRows.Num() == ValidEffects.BasicValues.Num() && Content.bShowComparison
				&& Content.Notice.ToString().Contains(TEXT("교체하면 표시된 효과가 적용됩니다"));
			int32 ExpectedRows = 0;
			for (const auto& Pair : ValidEffects.BasicValues)
			{
				if (FMath::IsNearlyZero(Pair.Value)) continue;
				++ExpectedRows;
				const auto* Effect = Tables->FindEmployeeEquipmentEffect(Pair.Key);
				const auto* Row = Content.Rows.FindByPredicate([Effect](const FEquipmentDeltaRow& Value) { return Effect && Value.Name.EqualTo(Effect->DisplayName); });
				const int32 Digits = CGEquipmentDetail::FractionDigits(CGEquipmentDetail::DisplayValue(*Effect, Pair.Value));
				bRecovery &= Row && Row->bUp
					&& Row->Before.ToString() == CGEquipmentDetail::FormatEffectValue(*Effect, 0.f, Digits)
					&& Row->After.ToString() == CGEquipmentDetail::FormatEffectValue(*Effect, Pair.Value, Digits);
			}
			for (const auto& Pair : ValidEffects.UniqueEffects)
				bRecovery &= Content.Uniques.ContainsByPredicate([&Pair](const FEquipmentUniqueChange& Change) { return Change.bGained && Change.Name.EqualTo(Pair.Value.DisplayName); });
			Check(TEXT("last_invalid_item_replacement_previews_zero_to_valid_effects"), bRecovery && Content.Rows.Num() == ExpectedRows);
		}
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		Window->ShowEmployeePage(EEmployeeWindowPage::Equipment);
		if (FMotionGate::IsReduced())
		{
			Check(TEXT("owned_list_reduced_motion_switch_is_immediate"), Window->PageTransitionElapsed < 0.f
				&& !Window->PageTransitionTarget.IsValid() && FMath::IsNearlyEqual(Window->EquipPlateSize->GetRenderOpacity(), 1.f));
		}
		else
		{
			const float StartedAt = Window->PageTransitionElapsed;
			Window->ShowEmployeePage(EEmployeeWindowPage::Equipment);
			Check(TEXT("owned_list_same_page_keeps_active_transition"), StartedAt >= 0.f
				&& FMath::IsNearlyEqual(Window->PageTransitionElapsed, StartedAt)
				&& Window->PageTransitionTarget.Get() == Window->EquipPlateSize.Get());
			Window->ShowEmployeePage(EEmployeeWindowPage::Home);
			Check(TEXT("owned_list_rapid_back_restores_old_plate"),
				FMath::IsNearlyEqual(Window->EquipPlateSize->GetRenderOpacity(), 1.f)
				&& Window->PageTransitionTarget.Get() == Window->GrowthVBox.Get());
			Window->ShowEmployeePage(EEmployeeWindowPage::Equipment);
			Check(TEXT("owned_list_rapid_reopen_starts_new_plate"),
				FMath::IsNearlyEqual(Window->GrowthVBox->GetRenderOpacity(), 1.f)
				&& Window->PageTransitionTarget.Get() == Window->EquipPlateSize.Get()
				&& FMath::IsNearlyEqual(Window->EquipPlateSize->GetRenderOpacity(), .35f));
		}
		Window->DeactivateWidget();
		Check(TEXT("owned_list_close_restores_page_opacity"), Window->PageTransitionElapsed < 0.f
			&& !Window->PageTransitionTarget.IsValid()
			&& FMath::IsNearlyEqual(Window->GrowthVBox->GetRenderOpacity(), 1.f)
			&& FMath::IsNearlyEqual(Window->EquipPlateSize->GetRenderOpacity(), 1.f));
		Check(TEXT("validation_preserves_equipment_state"), FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Original, &Equipment->GetState(), 0));
		Check(TEXT("validation_preserves_loaded_save_snapshot"), SavedGame
			&& FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&SavedOriginal, &SavedGame->GameData.EmployeeEquipmentState, 0));
		bool bPassed = true;
		for (const auto& Value : Checks)
		{
			const auto Result = Value->AsObject();
			if (!Result->GetBoolField(TEXT("passed")))
			{
				bPassed = false;
				UE_LOG(LogTemp, Warning, TEXT("EquipmentUI owned-list failed: %s"), *Result->GetStringField(TEXT("name")));
			}
		}
		Report->SetBoolField(TEXT("passed"), bPassed);
		Report->SetArrayField(TEXT("checks"), Checks);
		Report->SetArrayField(TEXT("captures"), Captures);
		OutputDirectory = Args.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentOwnedListValidation")) : Args[0];
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*OutputDirectory);
		FString Json;
		FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		const bool bSaved = FFileHelper::SaveStringToFile(Json, *FPaths::Combine(OutputDirectory, TEXT("owned-list.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogTemp, Display, TEXT("EquipmentUI owned-list %s: owned=%d checks=%d saved=%d %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Original.Instances.Num(), Checks.Num(), bSaved, *OutputDirectory);
	}

	void CheckInlineInspection(UEmployeeEquipmentWorkspaceWidget* Work, UEmployeeManager* Employees)
	{
		if (!Work || !Work->IsInspectorActive() || !Work->InspectorEquipButton || !Work->InspectorEnhanceButton
			|| !Work->InspectorDetailButton || ItemIds.IsEmpty()) { Check(TEXT("inline_inspector_and_owned_item_fixture"), false); return; }
		auto* Equipment = Work->EquipmentManager.Get();
		auto* Resources = Work->GetGameInstance()->GetSubsystem<UResourceItemManager>();
		auto* Items = Work->GetGameInstance()->GetSubsystem<UItemInventoryManager>();
		const auto* Item = Equipment->FindInstance(ItemIds[0]);
		const auto* Definition = Item ? Work->TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
		if (!Definition || !Resources || !Items) { Check(TEXT("inline_runtime_dependencies"), false); return; }
		const FGuid ItemId = Item->InstanceId;
		const FEmployeeEquipmentState Original = Equipment->GetState();
		const auto ResourceSnapshot = Resources->GetAllResources();
		const auto ItemSnapshot = Items->GetAllItems();
		const TArray<FEmployeeInstance> RosterSnapshot = Employees->GetAllEmployees();
		UWidget* OriginalToolbar = Work->WorkspaceToolbar.Get();
		auto CheckUtilityPlacement = [&](const FString& CheckName, bool bAtBottom)
		{
			UOverlay* ExpectedHost = bAtBottom ? Work->WorkspaceBottomUtilityHost.Get() : Work->WorkspaceTopUtilityHost.Get();
			const auto* UtilitySlot = OriginalToolbar ? Cast<UOverlaySlot>(OriginalToolbar->Slot) : nullptr;
			Check(CheckName, OriginalToolbar && Work->WorkspaceToolbar == OriginalToolbar && ExpectedHost
				&& Work->WorkspaceTopUtilityHost && Work->WorkspaceBottomUtilityHost
				&& OriginalToolbar->GetParent() == ExpectedHost && ExpectedHost->GetChildrenCount() == 1
				&& (bAtBottom ? Work->WorkspaceTopUtilityHost->GetChildrenCount() : Work->WorkspaceBottomUtilityHost->GetChildrenCount()) == 0
				&& Work->WorkspaceTopUtilityHost->GetVisibility() == (bAtBottom ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible)
				&& Work->WorkspaceBottomUtilityHost->GetVisibility() == (bAtBottom ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed)
				&& UtilitySlot && UtilitySlot->GetHorizontalAlignment() == HAlign_Fill && UtilitySlot->GetVerticalAlignment() == VAlign_Fill);
		};
		FEmployeeEquipmentState Fixture = Original;
		Fixture.Loadouts.Empty();
		Equipment->RestoreState(Fixture);
		auto StateMatches = [&]() { return FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Fixture, &Equipment->GetState(), 0); };
		auto ButtonLabel = [](UButtonWidget* Button) { const auto* Label = Button ? Cast<UCommonTextBlock>(Button->GetWidgetFromName(TEXT("ButtonText"))) : nullptr; return Label ? Label->GetText().ToString() : FString(); };
		Work->Configure(EmployeeId, EEquipmentWorkspaceView::Employee);
		CheckUtilityPlacement(TEXT("employee_utilities_below_content"), true);
		Work->ClearInspectionSelection();
		Check(TEXT("inline_empty_has_no_action_or_preview_override"), !Work->SelectedInstanceId.IsValid() && !Work->Dialog
			&& Work->InspectorPane && Work->InspectorPane->GetVisibility() == ESlateVisibility::Collapsed
			&& Work->InspectorEquipButton && !Work->InspectorEquipButton->GetIsEnabled());
		UEmployeeEquipmentItemObject* ClickedEntry = nullptr;
		for (const auto& Entry : Work->Entries) if (Entry->InstanceID == ItemId) { ClickedEntry = Entry.Get(); break; }
		if (ClickedEntry) Work->HandleItemClicked(ClickedEntry);
		Check(TEXT("employee_tile_click_selects_inline_without_modal_or_equipping"), ClickedEntry && Work->SelectedInstanceId == ItemId
			&& !Work->Dialog && Work->InspectorNameText && !Work->InspectorNameText->GetText().IsEmpty()
			&& Work->InspectorEquipButton->GetIsEnabled() && StateMatches());
		FEmployeeEquipmentDialogView CompactView;
		const bool bHasCompactView = Work->BuildItemDetailView(ItemId, CompactView, true);
		const FString InlineBody = Work->InspectorBodyText ? Work->InspectorBodyText->GetText().ToString() : FString();
		const FString CompactComparison = CompactView.Comparison.ToString();
		const bool bCompactHeading = Work->IsDrawerPresentation()
			? InlineBody.StartsWith(CompactComparison) && !InlineBody.Contains(TEXT("장착 시 변화")) && !InlineBody.StartsWith(TEXT("미장착"))
			: InlineBody.Contains(TEXT("장착 시 변화"));
		Check(TEXT("inline_comparison_prioritizes_changes_without_repeating_item_effects"), bHasCompactView && !CompactView.Comparison.IsEmpty()
			&& bCompactHeading && InlineBody.Contains(CompactComparison)
			&& !InlineBody.Contains(TEXT("장비 효과")) && !InlineBody.Contains(TEXT("\n\n"))
			&& (CompactView.Notice.IsEmpty() || InlineBody.Contains(CompactView.Notice.ToString())));
		if (Work->IsDrawerPresentation())
		{
			TArray<FString> ComparisonLines;
			CompactComparison.ParseIntoArrayLines(ComparisonLines, true);
			Check(TEXT("drawer_stat_name_and_value_share_the_first_line"), !ComparisonLines.IsEmpty()
				&& (!CompactComparison.Contains(TEXT("→")) || ComparisonLines[0].Contains(TEXT("→"))));
		}
		Check(TEXT("visual_equipment_selection_marks_uncommitted_preview"), Definition->AppearanceId.IsNone()
			|| (Work->PreviewLabelText && Work->PreviewLabelText->IsVisible() && Work->PreviewStage));
		Work->RefreshInventory();
		const auto* RefreshedEntry = Work->InventoryView->GetSelectedItem<UEmployeeEquipmentItemObject>();
		Check(TEXT("inline_selection_restored_by_guid_after_list_rebuild"), RefreshedEntry && RefreshedEntry != ClickedEntry
			&& RefreshedEntry->InstanceID == ItemId && Work->SelectedInstanceId == ItemId);
		Work->HandleInspectorDetail();
		Check(TEXT("inline_detail_uses_existing_dialog"), Work->Dialog && Work->SelectedInstanceId == ItemId);
		FEmployeeEquipmentDialogView FullView;
		const bool bHasFullView = Work->BuildItemDetailView(ItemId, FullView);
		const auto* DetailEffects = Work->Dialog ? Cast<UCommonTextBlock>(Work->Dialog->GetWidgetFromName(TEXT("EffectsText"))) : nullptr;
		const auto* DetailComparison = Work->Dialog ? Cast<UCommonTextBlock>(Work->Dialog->GetWidgetFromName(TEXT("ComparisonText"))) : nullptr;
		Check(TEXT("detail_retains_full_item_effects_and_loadout_comparison"), bHasFullView && DetailEffects && DetailComparison
			&& DetailEffects->GetText().EqualTo(FullView.Effects) && DetailComparison->GetText().EqualTo(FullView.Comparison)
			&& !FullView.Effects.IsEmpty() && FullView.Comparison.ToString().Contains(TEXT("장비 합산")));
		if (Work->Dialog) Work->Dialog->OnClose.ExecuteIfBound();
		Check(TEXT("detail_close_returns_to_same_inline_selection"), !Work->Dialog && Work->SelectedInstanceId == ItemId);
		Work->HandleInspectorEnhance();
		Check(TEXT("inline_enhance_quotes_existing_transaction_without_commit"), Work->Dialog
			&& Work->PendingRequest.Command == EEquipmentCommand::Enhance && Work->PendingRequest.InstanceId == ItemId && StateMatches());
		if (Work->Dialog) Work->Dialog->OnClose.ExecuteIfBound();
		Check(TEXT("enhance_close_returns_to_same_inline_selection"), !Work->Dialog && Work->SelectedInstanceId == ItemId);
		Work->HandleInspectorEnhance();
		if (Work->Dialog) Work->Dialog->OnTertiary.ExecuteIfBound();
		Check(TEXT("enhance_cancel_returns_to_same_inline_selection"), !Work->Dialog && Work->SelectedInstanceId == ItemId && StateMatches());
		Fixture.Loadouts.FindOrAdd(EmployeeId).Slots.Add(Definition->EquipmentSlot, ItemId);
		Equipment->RestoreState(Fixture); Work->Refresh(); Work->OpenEquipmentItem(ItemId);
		Check(TEXT("own_equipped_item_has_remove_action_without_preview_commit"), Equipment->GetInstanceEmployee(ItemId) == EmployeeId
			&& ButtonLabel(Work->InspectorEquipButton).Contains(TEXT("해제")) && StateMatches());
		const int32 OtherGradeIndex = (static_cast<int32>(Definition->Grade) + 1) % static_cast<int32>(EEmployeeEquipmentGrade::Count);
		Work->GradeFilterCombo->SetSelectedIndex(OtherGradeIndex + 1);
		const bool bHadIncompatibleGradeFilter = Work->GradeFilter != EEmployeeEquipmentGrade::Count && Work->GradeFilter != Definition->Grade;
		Work->HandleSlot(static_cast<int32>(Definition->EquipmentSlot));
		Check(TEXT("slot_opens_filtered_list_without_comparison_and_preserves_grade_choice"), bHadIncompatibleGradeFilter
			&& Work->GradeFilter == static_cast<EEmployeeEquipmentGrade>(OtherGradeIndex) && !Work->SelectedInstanceId.IsValid() && !Work->Dialog
			&& Work->InspectorPane->GetVisibility() == ESlateVisibility::Collapsed && !Work->bAllSlots
			&& Work->CurrentSlot == Definition->EquipmentSlot && StateMatches());
		Work->HandleSlot((static_cast<int32>(Definition->EquipmentSlot) + 1) % static_cast<int32>(EEmployeeEquipmentSlot::Count));
		Check(TEXT("empty_slot_clears_inspector_selection"), !Work->SelectedInstanceId.IsValid() && !Work->InspectorEquipButton->GetIsEnabled());
		TArray<FEmployeeInstance> RosterFixture = RosterSnapshot;
		int32 OtherId = INDEX_NONE;
		for (const auto& Employee : RosterFixture) if (Employee.EmployeeID != EmployeeId) { OtherId = Employee.EmployeeID; break; }
		if (OtherId == INDEX_NONE) { FEmployeeInstance Other = RosterFixture[0]; Other.EmployeeID = MAX_int32 - 1; OtherId = Other.EmployeeID; RosterFixture.Add(Other); Employees->SetEmployeeList(RosterFixture); }
		Fixture.Loadouts.Empty(); Fixture.Loadouts.FindOrAdd(OtherId).Slots.Add(Definition->EquipmentSlot, ItemId);
		Equipment->RestoreState(Fixture); Work->Configure(EmployeeId, EEquipmentWorkspaceView::Employee); Work->OpenEquipmentItem(ItemId);
		Check(TEXT("inline_inspector_preserves_other_employee_holder"), Work->InspectorBodyText
			&& Work->InspectorBodyText->GetText().ToString().Contains(Employees->GetEmployeeFullName(OtherId) + TEXT(" 착용")));
		Work->HandleInspectorEquip();
		Check(TEXT("other_employee_equipment_requires_transfer_confirmation"), Work->Dialog && Work->PendingRequest.Command == EEquipmentCommand::SetSlot
			&& Work->PendingRequest.EmployeeId == EmployeeId && Work->PendingRequest.ExpectedSourceEmployeeId == OtherId && StateMatches());
		if (Work->Dialog) Work->Dialog->OnSecondary.ExecuteIfBound();
		Check(TEXT("transfer_cancel_preserves_owner_and_inline_selection"), !Work->Dialog && Work->SelectedInstanceId == ItemId && StateMatches());
		Work->GradeFilterCombo->SetSelectedIndex(1);
		Check(TEXT("grade_filter_discards_previous_preview_selection"), !Work->SelectedInstanceId.IsValid());
		Work->GradeFilterCombo->SetSelectedIndex(0);
		Work->OpenEquipmentItem(FGuid::NewGuid());
		Check(TEXT("stale_item_clears_inline_actions"), !Work->SelectedInstanceId.IsValid() && !Work->InspectorEquipButton->GetIsEnabled());
		Employees->SetEmployeeList({}); Work->Configure(MAX_int32, EEquipmentWorkspaceView::Employee);
		Check(TEXT("zero_employee_clears_stale_context_and_disables_equipment_action"), Work->CurrentEmployeeId == INDEX_NONE
			&& !Work->SelectedInstanceId.IsValid() && !Work->InspectorEquipButton->GetIsEnabled());
		Employees->SetEmployeeList(RosterSnapshot); Equipment->RestoreState(Original);
		Work->Configure(EmployeeId, EEquipmentWorkspaceView::Employee); Work->ShowDetail(ItemId);
		CheckUtilityPlacement(TEXT("employee_reuses_bottom_utility_toolbar"), true);
		Check(TEXT("employee_keeps_existing_detail_modal"), !Work->IsInspectorActive() && Work->Dialog);
		Work->DismissDialog(); Work->Configure(EmployeeId, EEquipmentWorkspaceView::Gacha);
		CheckUtilityPlacement(TEXT("gacha_moves_same_toolbar_to_top"), false);
		Check(TEXT("gacha_still_binds_one_ten_thirty_pull_controls"), Work->PullOneButton && Work->PullTenButton && Work->PullThirtyButton);
		Work->Configure(EmployeeId, EEquipmentWorkspaceView::Employee);
		CheckUtilityPlacement(TEXT("employee_return_restores_same_toolbar_to_bottom"), true);
		Check(TEXT("inspection_does_not_change_resources_tickets_or_saved_equipment"), Resources->GetAllResources().OrderIndependentCompareEqual(ResourceSnapshot)
			&& Items->GetAllItems().OrderIndependentCompareEqual(ItemSnapshot)
			&& FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Original, &Equipment->GetState(), 0),
			TEXT("Ownership and roster fixtures are restored synchronously before any frame yield. No confirmation commit is invoked."));
	}

	void CheckRosterEnhancementRefresh(UEmployeeWindowWidget* Window, UEmployeeManager* Employees)
	{
		if (!Window || !Employees || !Window->RosterListView) return;
		UEntityCardData* Item = nullptr;
		for (UObject* Candidate : Window->RosterListView->GetListItems())
		{
			auto* CardData = Cast<UEntityCardData>(Candidate);
			if (CardData && !CardData->bIsHireSlot && CardData->EmployeeInfo.EmployeeID == Window->SelectedEmployeeID)
			{
				Item = CardData;
				break;
			}
		}
		FEmployeeInstance* Employee = Item ? Employees->FindEmployee(Item->EmployeeInfo.EmployeeID) : nullptr;
		if (!Item || !Employee) { Check(TEXT("roster_enhancement_fixture_exists"), false); return; }
		const FEmployeeInstance OriginalEmployee = *Employee;
		const int32 OriginalSelection = Window->SelectedEmployeeID;
		UObject* OriginalSelectedItem = Window->RosterListView->GetSelectedItem();
		const TArray<UObject*> OriginalItems = Window->RosterListView->GetListItems();
		const float OriginalScrollOffset = Window->RosterListView->GetScrollOffset();
		const bool bOriginalLiveView = Window->bLiveHeroViewEnabled;
		Window->bLiveHeroViewEnabled = false;
		const TSharedRef<SWidget> RosterSlate = Window->RosterListView->TakeWidget();
		FWidgetRenderer RosterRenderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> RosterTarget(FWidgetRenderer::CreateTargetFor(FVector2D(600.f, 420.f), TF_Bilinear, false));
		auto DrawRoster = [&]()
		{
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				RosterRenderer.DrawWidget(RosterTarget.Get(), RosterSlate, FVector2D(600.f, 420.f), 0.f);
				FlushRenderingCommands();
			}
		};
		if (RosterTarget.IsValid())
		{
			Window->RosterListView->ScrollIndexIntoView(OriginalItems.IndexOfByKey(Item));
			DrawRoster();
			for (const bool bSelected : { true, false })
			{
				Window->SelectedEmployeeID = bSelected ? OriginalSelection : INDEX_NONE;
				for (const int32 Enhancement : { 0, 15, 0 })
				{
					Employee->EnhancementLevel = Enhancement;
					Window->HandleEmployeeStatsChanged(Employee->EmployeeID);
					const FString Key = FString::Printf(TEXT("roster_%s_enhancement_%d"), bSelected ? TEXT("selected") : TEXT("other"), Enhancement);
					Check(Key + TEXT("_item_snapshot_updated"), Item->EmployeeInfo.EnhancementLevel == Enhancement);
					auto CheckCard = [&](const FString& Suffix)
					{
						auto* Card = Window->RosterListView->GetEntryWidgetFromItem<UEmployeeRosterCardWidget>(Item);
						auto* Stars = Card ? Cast<UCommonTextBlock>(Card->GetWidgetFromName(TEXT("StarsText"))) : nullptr;
						UWidget* Chip = Card ? Card->GetWidgetFromName(TEXT("StarsChip")) : nullptr;
						Check(Key + Suffix, Stars && Chip
							&& Stars->GetText().ToString() == (Enhancement > 0 ? TEXT("15") : TEXT(""))
							&& Stars->GetVisibility() == (Enhancement > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed)
							&& Chip->GetVisibility() == Stars->GetVisibility());
					};
					CheckCard(TEXT("_visible_card"));
					Window->RosterListView->RegenerateAllEntries();
					DrawRoster();
					CheckCard(TEXT("_recycled_card_keeps_latest_value"));
					Check(Key + TEXT("_preserves_list_and_selection"), Window->RosterListView->GetListItems() == OriginalItems
						&& Window->RosterListView->GetSelectedItem() == OriginalSelectedItem);
				}
			}
			Employee->Level = OriginalEmployee.Level + 1;
			Window->HandleEmployeeLevelUp(Employee->EmployeeID, Employee->Level);
			Check(TEXT("roster_levelup_updates_recycling_snapshot"), Item->EmployeeInfo.Level == Employee->Level);
		}
		else Check(TEXT("roster_enhancement_render_target"), false);
		*Employee = OriginalEmployee;
		Window->SelectedEmployeeID = OriginalSelection;
		Window->bLiveHeroViewEnabled = bOriginalLiveView;
		Window->HandleEmployeeStatsChanged(OriginalSelection);
		Window->RosterListView->SetScrollOffset(OriginalScrollOffset);
	}

	// 라벨은 부품 안의 CommonTextBlock 이 들고 있다 ― 공용 버튼에 getter 를 새로 뚫지 않는다.
	static FString EquipmentToggleLabel(const UEmployeeWindowWidget* Window)
	{
		const auto* Label = Window && Window->HeroEquipmentOpenButton
			? Cast<UCommonTextBlock>(Window->HeroEquipmentOpenButton->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
		return Label ? Label->GetText().ToString() : FString();
	}

	// 장비 = 오른쪽 칼럼 교체 (2026-09-25). 창이 아니라 판이 바뀐다.
	void CheckEquipmentColumn(UEmployeeWindowWidget* Window, UEmployeeManager* Employees)
	{
		if (!Window || !Window->HeroEquipmentOpenButton || !Window->EquipmentList || !Window->EquipPlateSize || !Window->GrowthVBox)
		{
			Check(TEXT("equipment_column_widgets_bound"), false);
			return;
		}
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		Check(TEXT("column_home_shows_growth_hides_equipment"),
			Window->GrowthVBox->GetVisibility() != ESlateVisibility::Collapsed
			&& Window->EquipPlateSize->GetVisibility() == ESlateVisibility::Collapsed
			&& EquipmentToggleLabel(Window) == TEXT("장비"));

		Window->HandleHeroEquipmentOpen();
		Check(TEXT("column_equipment_swaps_plate_and_flips_label"),
			Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment
			&& Window->GrowthVBox->GetVisibility() == ESlateVisibility::Collapsed
			&& Window->EquipPlateSize->GetVisibility() != ESlateVisibility::Collapsed
			&& EquipmentToggleLabel(Window) == TEXT("능력"));
		if (!FMotionGate::IsReduced())
		{
			const float StartedAt = Window->PageTransitionElapsed;
			Window->ShowEmployeePage(EEmployeeWindowPage::Equipment);
			Check(TEXT("column_same_page_does_not_restart_transition"), StartedAt >= 0.f
				&& FMath::IsNearlyEqual(Window->PageTransitionElapsed, StartedAt)
				&& Window->PageTransitionTarget.Get() == Window->EquipPlateSize.Get());
			Window->ShowEmployeePage(EEmployeeWindowPage::Home);
			Check(TEXT("column_rapid_return_restores_hidden_plate_opacity"),
				FMath::IsNearlyEqual(Window->EquipPlateSize->GetRenderOpacity(), 1.f)
				&& Window->PageTransitionTarget.Get() == Window->GrowthVBox.Get());
			Window->ShowEmployeePage(EEmployeeWindowPage::Equipment);
			Check(TEXT("column_rapid_reentry_targets_visible_plate"),
				FMath::IsNearlyEqual(Window->GrowthVBox->GetRenderOpacity(), 1.f)
				&& Window->PageTransitionTarget.Get() == Window->EquipPlateSize.Get()
				&& FMath::IsNearlyEqual(Window->EquipPlateSize->GetRenderOpacity(), .35f));
		}
		// 이 개편의 요점 ― 장비를 보는 동안에도 초상과 장착 6칸이 남아 있어야 한다.
		Check(TEXT("column_equipment_keeps_portrait_and_slots"),
			Window->HeroPortraitImage && Window->HeroPortraitImage->IsVisible()
			&& Window->HeroEquipmentSlot0 && Window->HeroEquipmentSlot0->IsVisible());

		for (int32 Index = 0; Index < static_cast<int32>(EEmployeeEquipmentSlot::Count); ++Index)
		{
			Window->HandleHeroEquipmentSlot(Index);
			Check(FString::Printf(TEXT("column_slot_%d_filters_that_part"), Index),
				Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment
				&& !Window->EquipmentList->bAllSlots
				&& static_cast<int32>(Window->EquipmentList->CurrentSlot) == Index);
		}

		if (!ItemIds.IsEmpty())
		{
			Window->HandleHeroEquipmentOpen();
			Window->HandleHeroEquipmentOpen();
			Window->HandleEquipmentItemPicked(ItemIds[0]);
			Check(TEXT("column_pick_opens_inline_detail"),
				Window->InlineDetailInstanceId == ItemIds[0] && Window->InlineDetail != nullptr
				&& Window->EquipDetailHost && Window->EquipDetailHost->GetVisibility() != ESlateVisibility::Collapsed
				&& Window->EquipDetailEmpty && Window->EquipDetailEmpty->GetVisibility() == ESlateVisibility::Collapsed);
			// 뒤로가기는 상세부터 접는다 ― 한 번에 홈까지 가면 고르던 자리를 잃는다.
			Window->NativeOnHandleBackAction();
			Check(TEXT("column_back_closes_detail_before_leaving"),
				Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment && !Window->InlineDetailInstanceId.IsValid());
			Window->NativeOnHandleBackAction();
			Check(TEXT("column_back_again_returns_to_growth"),
				Window->CurrentEmployeePage == EEmployeeWindowPage::Home
				&& Window->GrowthVBox->GetVisibility() != ESlateVisibility::Collapsed);
		}
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
	}

	// 2026-09-26 이식. 장비가 「별도 창 + 워크스페이스」에서 「오른쪽 칼럼 교체 + 인라인 상세」로
	// 바뀌면서 이 구간의 단언 33건 중 12건은 **대상 위젯 자체가 사라졌다** — 직원 피커·부위/등급 콤보·
	// 워크스페이스 툴바·인스펙터 페인·가챠 뷰·워크스페이스 전용 프리뷰 스테이지. 지운 자리마다 이유를 남긴다.
	// 칼럼 스왑 자체의 계약은 CheckEquipmentColumn 이 맡고, 여기는 홈 슬롯·이벤트·빈 명단을 본다.
	void CheckMobileManagement(UEmployeeWindowWidget* Window, UEmployeeManager* Employees)
	{
		auto* List = Window ? Window->EquipmentList.Get() : nullptr;
		auto* Tables = Window && Window->GetGameInstance() ? Window->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!List || !Tables || !Window->EquipmentManager || !Window->HeroEquipmentOpenButton
			|| !Window->HeroEquipmentSlot0 || ItemIds.IsEmpty())
		{
			Check(TEXT("mobile_management_controls_bound"), false);
			return;
		}
		Report->SetBoolField(TEXT("mobile_management_section_ported"), true);
		CheckEquipmentColumn(Window, Employees);

		const auto RosterSnapshot = Employees->GetAllEmployees();
		const auto EquipmentSnapshot = Window->EquipmentManager->GetState();
		CheckRosterEnhancementRefresh(Window, Employees);
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		Check(TEXT("mobile_home_keeps_level_up_and_tutorial_targets"), Window->DisciplineCard
			&& Window->DisciplineCard->GetWidgetFromName(TEXT("LevelUpButton"))
			&& Window->GetEnhanceButtonWidget() == Window->EnhanceButton
			&& Window->RosterListView && Window->HeroPortraitImage && Window->PotentialWorkspace
			&& Window->PotentialWorkspace->GetWidgetFromName(TEXT("PotLine0"))
			&& Window->PotentialWorkspace->GetWidgetFromName(TEXT("PotLine1"))
			&& Window->PotentialWorkspace->GetWidgetFromName(TEXT("PotLine2"))
			&& Window->HeroEquipmentSlot0->IsVisible() && Window->HeroEquipmentSlot0->GetIsEnabled()
			&& !Window->GetWidgetFromName(TEXT("EquipmentManageButton"))
			&& Window->PotentialWorkspace->GetParent() == Window->PotentialHost);
		// 칼럼 교체라 무대를 반납하지 않는다 — 워크스페이스로 넘기던 절반은 대상이 사라져 지웠다.
		Check(TEXT("mobile_home_owns_shared_character_stage"), Window->LiveStage && Window->LiveStage->IsHeldBy(Window));

		UButtonWidget* HomeSlots[] = {Window->HeroEquipmentSlot0, Window->HeroEquipmentSlot1, Window->HeroEquipmentSlot2,
			Window->HeroEquipmentSlot3, Window->HeroEquipmentSlot4, Window->HeroEquipmentSlot5};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(HomeSlots); ++Index)
		{
			Check(FString::Printf(TEXT("home_slot_%d_bound_and_enabled"), Index), HomeSlots[Index] && HomeSlots[Index]->GetIsEnabled()
				&& Cast<UIconWithButtonWidget>(HomeSlots[Index]) && HomeSlots[Index]->OnClicked().IsBoundToObject(Window));
			if (HomeSlots[Index]) HomeSlots[Index]->OnClicked().Broadcast();
			// 빈 칸을 눌러도 그 부위로 들어가야 한다 — 「낄 게 없으니 안 열린다」가 아니라 「그 부위 목록」이다.
			Check(FString::Printf(TEXT("home_slot_%d_opens_same_employee_part_including_empty_slots"), Index), Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment
				&& Window->SelectedEmployeeID == EmployeeId && !List->bAllSlots && static_cast<int32>(List->CurrentSlot) == Index
				&& List->PartCombo && List->PartCombo->GetSelectedIndex() == Index + 1
				&& (List->InventoryView->GetNumItems() == 0
					? !List->SelectedInstanceId.IsValid() && !Window->InlineDetailInstanceId.IsValid()
					: List->SelectedInstanceId.IsValid() && Window->InlineDetailInstanceId == List->SelectedInstanceId));
			Window->NativeOnHandleBackAction();
			if (Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment) Window->NativeOnHandleBackAction();
		}
		Check(TEXT("home_equipment_button_visible_enabled_and_bound"), Window->HeroEquipmentOpenButton->IsVisible()
			&& Window->HeroEquipmentOpenButton->GetIsEnabled() && Window->HeroEquipmentOpenButton->OnClicked().IsBoundToObject(Window));
		Window->HandleHeroEquipmentOpen();
		Check(TEXT("home_equipment_button_opens_same_employee_all_parts_and_grades"), Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment
			&& Window->SelectedEmployeeID == EmployeeId && List->ContextEmployeeId == EmployeeId && List->bAllSlots
			&& List->CurrentGrade == EEmployeeEquipmentGrade::Count
			&& (List->InventoryView->GetNumItems() == 0
				? !List->SelectedInstanceId.IsValid() && !Window->InlineDetailInstanceId.IsValid()
				: List->SelectedInstanceId.IsValid() && Window->InlineDetailInstanceId == List->SelectedInstanceId)
			&& List->InventoryView && List->InventoryView->GetNumItems() == EquipmentSnapshot.Instances.Num());
		Window->NativeOnHandleBackAction();
		if (Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment) Window->NativeOnHandleBackAction();
		Check(TEXT("home_equipment_button_back_returns_to_same_portrait"), Window->CurrentEmployeePage == EEmployeeWindowPage::Home
			&& Window->SelectedEmployeeID == EmployeeId && Window->HeroEquipmentOpenButton->IsVisible()
			&& Window->HeroEquipmentOpenButton->GetIsEnabled() && Window->LiveStage && Window->LiveStage->IsHeldBy(Window));

		// [삭제] mobile_manage_opens_same_employee_without_second_employee_picker
		//        — EmployeeSelectorRow/EmployeeCombo 는 워크스페이스의 물건이었다. 칼럼엔 피커가 아예 없다.
		// [삭제] mobile_equipment_takes_shared_stage_after_home_releases · panel_preserves_home_and_reuses_hero_portrait
		//        — 무대를 워크스페이스로 넘기던 동작 자체가 없어졌다(칼럼은 창의 무대를 그대로 쓴다).
		//        대체 단언 = CheckEquipmentColumn 의 column_equipment_keeps_portrait_and_slots.
		// [삭제] drawer_equipment_effects_* 3건 — WorkspaceToolbar/EffectsButton/Dialog 가 사라졌다.
		// [삭제] gacha_* 3건 — 가챠는 칼럼에 없다(단독 창·채용 패널이 맡는다).
		// [삭제] mobile_hidden_employee_picker_cannot_retarget_context — 피커가 없어 재조준할 경로가 없다.
		// [삭제] panel_close_clears_preview_and_preserves_employee_input — 워크스페이스 프리뷰가 없다.
		// [삭제] mobile_system_back_closes_comparison_before_leaving_equipment
		//        — column_back_closes_detail_before_leaving 이 같은 계약을 새 이름으로 본다.

		const auto* Item = Window->EquipmentManager->FindInstance(ItemIds[0]);
		const auto* Definition = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
		if (!Definition) { Check(TEXT("mobile_management_item_fixture"), false); return; }

		Window->HandleHeroEquipmentSlot(static_cast<int32>(Definition->EquipmentSlot));
		Check(TEXT("mobile_slot_first_opens_matching_list_with_selected_detail"), !List->bAllSlots && List->CurrentSlot == Definition->EquipmentSlot
			&& List->SelectedInstanceId.IsValid() && Window->InlineDetailInstanceId == List->SelectedInstanceId
			&& Window->EquipDetailEmpty && Window->EquipDetailEmpty->GetVisibility() == ESlateVisibility::Collapsed
			&& List->PartCombo && List->PartCombo->GetSelectedIndex() == static_cast<int32>(Definition->EquipmentSlot) + 1);
		Window->CloseInlineDetail();

		FWidgetRenderer LayoutRenderer(true);
		for (int32 Width : {2304, 3120})
		{
			TStrongObjectPtr<UTextureRenderTarget2D> LayoutTarget(FWidgetRenderer::CreateTargetFor(FVector2D(Width, 1440), TF_Bilinear, false));
			if (!LayoutTarget.IsValid()) { Check(TEXT("mobile_layout_target_created"), false); continue; }
			const auto SurfaceWidget = Window->TakeWidget();
			LayoutRenderer.DrawWidget(LayoutTarget.Get(), SurfaceWidget, FVector2D(Width, 1440), 0.f);
			const FVector2D ListSizeBefore = List->InventoryView->GetCachedGeometry().GetLocalSize();
			Window->HandleEquipmentItemPicked(ItemIds[0]);
			LayoutRenderer.DrawWidget(LayoutTarget.Get(), SurfaceWidget, FVector2D(Width, 1440), 0.f);
			const FVector2D ListSizeSelected = List->InventoryView->GetCachedGeometry().GetLocalSize();
			const FGeometry& WindowGeometry = Window->GetCachedGeometry();
			const FVector2D ListEnd = WindowGeometry.AbsoluteToLocal(List->InventoryView->GetCachedGeometry().LocalToAbsolute(ListSizeSelected));
			const FVector2D DetailStart = WindowGeometry.AbsoluteToLocal(Window->EquipDetailHost->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector));
			// 상세는 목록 「아래」가 아니라 「옆」에 선다(사용자 결정) — 목록이 가로로 줄고 상세가 그 오른쪽이다.
			Check(FString::Printf(TEXT("mobile_%d_selection_details_stand_beside_list"), Width),
				ListSizeSelected.X < ListSizeBefore.X && FMath::IsNearlyEqual(ListSizeBefore.Y, ListSizeSelected.Y, .5f)
				&& Window->EquipDetailHost->IsVisible() && ListEnd.X <= DetailStart.X + .5f
				&& FMath::IsNearlyEqual(List->InventoryView->GetEntryWidth(), 176.f));
			Window->CloseInlineDetail();
			LayoutRenderer.DrawWidget(LayoutTarget.Get(), SurfaceWidget, FVector2D(Width, 1440), 0.f);
			Check(FString::Printf(TEXT("mobile_%d_close_preserves_filter_and_list_geometry"), Width), !Window->InlineDetailInstanceId.IsValid()
				&& Window->EquipDetailEmpty && Window->EquipDetailEmpty->GetVisibility() != ESlateVisibility::Collapsed && !List->bAllSlots
				&& List->CurrentSlot == Definition->EquipmentSlot && !List->SelectedInstanceId.IsValid()
				&& ListSizeBefore.Equals(List->InventoryView->GetCachedGeometry().GetLocalSize(), .5f));
		}

		Window->HandleEquipmentItemPicked(ItemIds[0]);
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		Check(TEXT("panel_common_x_closes_entire_equipment_sheet"), Window->CurrentEmployeePage == EEmployeeWindowPage::Home
			&& !Window->InlineDetailInstanceId.IsValid() && Window->LiveStage && Window->LiveStage->IsHeldBy(Window));

		Window->HandleHeroEquipmentSlot(0);
		TArray<FEmployeeInstance> RosterFixture = RosterSnapshot;
		FEmployeeInstance Other = RosterFixture[0]; Other.EmployeeID = MAX_int32 - 2; RosterFixture.Add(Other);
		Employees->SetEmployeeList(RosterFixture);
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		FEmployeeEquipmentState HomeFixture = EquipmentSnapshot;
		HomeFixture.Loadouts.Empty();
		HomeFixture.Loadouts.FindOrAdd(EmployeeId).Slots.Add(Definition->EquipmentSlot, ItemIds[0]);
		if (FEmployeeEquipmentInstance* ProtectedItem = HomeFixture.Instances.Find(ItemIds[0])) ProtectedItem->bLocked = true;
		Window->EquipmentManager->RestoreState(HomeFixture);
		Window->EquipmentManager->OnEquipmentChanged.Broadcast();
		auto* HomeIcon = Cast<UImage>(HomeSlots[static_cast<int32>(Definition->EquipmentSlot)]->GetWidgetFromName(TEXT("IconImage")));
		Check(TEXT("home_equipment_event_updates_visible_slot_and_live_appearance"), HomeIcon && HomeIcon->IsVisible()
			&& HomeIcon->GetBrush().GetResourceObject() && Window->LiveViewAppearanceKey == FEmployeeEquipmentAppearanceResolver::BuildKey(
				HomeFixture, Tables->GetEmployeeEquipmentDefinitions(), Tables->GetEmployeeEquipmentAppearances(), EmployeeId));
		auto* HomeSlotLabel = Cast<UCommonTextBlock>(HomeSlots[static_cast<int32>(Definition->EquipmentSlot)]->GetWidgetFromName(TEXT("ButtonText")));
		auto* HomeGradeFrame = Cast<UImage>(HomeSlots[static_cast<int32>(Definition->EquipmentSlot)]->GetWidgetFromName(TEXT("GradeFrame")));
		Check(TEXT("equipped_slot_shows_opaque_item_without_part_label"), HomeSlotLabel && HomeSlotLabel->GetText().IsEmpty()
			&& HomeSlotLabel->GetVisibility() == ESlateVisibility::Collapsed && HomeIcon && FMath::IsNearlyEqual(HomeIcon->GetColorAndOpacity().A, 1.f)
			&& HomeGradeFrame && HomeGradeFrame->IsVisible() && FMath::IsNearlyEqual(HomeGradeFrame->GetColorAndOpacity().A, 1.f)
			&& FMath::IsNearlyEqual(HomeSlots[static_cast<int32>(Definition->EquipmentSlot)]->GetRenderOpacity(), 1.f));
		const int32 OtherGrade = (static_cast<int32>(Definition->Grade) + 1) % static_cast<int32>(EEmployeeEquipmentGrade::Count);
		if (List->GradeCombo) List->GradeCombo->SetSelectedIndex(OtherGrade + 1);
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		HomeSlots[static_cast<int32>(Definition->EquipmentSlot)]->OnClicked().Broadcast();
		Check(TEXT("equipped_protected_slot_click_resets_grade_and_opens_exact_item"),
			Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment && List->ContextEmployeeId == EmployeeId
			&& !List->bAllSlots && List->CurrentSlot == Definition->EquipmentSlot
			&& List->CurrentGrade == EEmployeeEquipmentGrade::Count && List->GradeCombo && List->GradeCombo->GetSelectedIndex() == 0
			&& List->SelectedInstanceId == ItemIds[0] && Window->InlineDetailInstanceId == ItemIds[0]
			&& Window->EquipDetailEmpty && Window->EquipDetailEmpty->GetVisibility() == ESlateVisibility::Collapsed);
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		Window->SelectEmployee(Other.EmployeeID);
		Check(TEXT("home_roster_selection_clears_previous_employee_gear"), HomeIcon && HomeIcon->GetVisibility() == ESlateVisibility::Collapsed
			&& Window->SelectedEmployeeID == Other.EmployeeID);
		const auto* EmptyPart = Tables->FindEmployeeEquipmentSlot(Definition->EquipmentSlot);
		Check(TEXT("empty_slot_restores_opaque_datatable_part_label_after_employee_change"), EmptyPart && HomeSlotLabel
			&& HomeSlotLabel->GetText().EqualTo(EmptyPart->DisplayName) && HomeSlotLabel->GetVisibility() == ESlateVisibility::HitTestInvisible
			&& FMath::IsNearlyEqual(HomeSlotLabel->GetColorAndOpacity().GetSpecifiedColor().A, 1.f));
		Window->HandleHeroEquipmentSlot(static_cast<int32>(Definition->EquipmentSlot));
		Check(TEXT("home_other_employee_empty_slot_keeps_its_own_context"), List->ContextEmployeeId == Other.EmployeeID
			&& List->CurrentSlot == Definition->EquipmentSlot && List->SelectedInstanceId.IsValid()
			&& Window->InlineDetailInstanceId == List->SelectedInstanceId);
		Window->NativeOnHandleBackAction();
		Window->SelectEmployee(EmployeeId);
		Check(TEXT("home_reselect_restores_equipped_slot"), HomeIcon && HomeIcon->IsVisible());
		Window->EquipmentManager->RestoreState(EquipmentSnapshot);
		Window->EquipmentManager->OnEquipmentChanged.Broadcast();

		// 없는 직원으로 Configure 해도 다른 직원으로 흘러가면 안 된다 — 남의 장비를 보여주는 사고다.
		List->Configure(MAX_int32);
		Check(TEXT("mobile_invalid_employee_never_falls_back_to_another_employee"), List->ContextEmployeeId == INDEX_NONE
			&& !List->SelectedInstanceId.IsValid() && List->InventoryView && List->InventoryView->GetNumItems() == 0);
		Employees->SetEmployeeList({});
		Window->HandleRosterChanged();
		Check(TEXT("mobile_zero_employee_returns_to_disabled_home"), Window->CurrentEmployeePage == EEmployeeWindowPage::Home
			&& Window->SelectedEmployeeID == INDEX_NONE && !Window->HeroEquipmentSlot0->GetIsEnabled());
		Window->HandleHeroEquipmentOpen();
		Check(TEXT("home_equipment_button_zero_employee_stays_disabled_and_cannot_open"), !Window->HeroEquipmentOpenButton->GetIsEnabled()
			&& Window->CurrentEmployeePage == EEmployeeWindowPage::Home && Window->SelectedEmployeeID == INDEX_NONE);
		Employees->SetEmployeeList(RosterSnapshot);
		Window->OpenWithContext(EmployeeId, true);
		Check(TEXT("mobile_management_does_not_change_equipment"), FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&EquipmentSnapshot, &Window->EquipmentManager->GetState(), 0));
	}

	void CheckInlinePotential(UEmployeeWindowWidget* Window, UEmployeeManager* Employees)
	{
		Window->ShowEmployeePage(EEmployeeWindowPage::Home);
		UPotentialResetWorkspaceWidget* Potential = Window->PotentialWorkspace;
		UItemInventoryManager* Items = GetItemInventory();
		const UTableManagerSubsystem* Tables = Window->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (!Potential || !Items || !Tables)
		{
			Check(TEXT("inline_potential_dependencies"), false);
			return;
		}
		const int32 OriginalEmployeeId = Window->SelectedEmployeeID;
		const EItemType OriginalCard = Potential->GetSelectedCard();
		const auto OriginalItems = Items->GetAllItems();
		auto BeforeStats = Employees->OnEmployeeStatsChanged;
		auto BeforeItems = Items->OnItemChanged;
		const int32 BeforeStatsCount = BeforeStats.RemoveAll(Potential);
		const int32 BeforeItemsCount = BeforeItems.RemoveAll(Potential);
		ApplyBusinessCardFixture(12, 2, 0);
		const auto FixtureItems = Items->GetAllItems();
		ON_SCOPE_EXIT
		{
			Check(TEXT("inline_potential_selection_checks_do_not_consume_items"), Items->GetAllItems().OrderIndependentCompareEqual(FixtureItems),
				TEXT("Compared before fixture restoration. ResetButton is never clicked; only card selection, odds and local page/lifecycle controls execute."));
			RestoreBusinessCardFixture();
			Items->SetAllItems(OriginalItems);
			Window->SelectEmployee(OriginalEmployeeId);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (BusinessCardTypes[Index] != OriginalCard) continue;
				ClickPotentialCardTile(Potential, Index);
			}
		};
		Check(TEXT("inline_potential_single_host"), Window->PotentialHost && Window->PotentialHost->GetChildrenCount() == 1
			&& Potential->GetParent() == Window->PotentialHost && !Window->GetWidgetFromName(TEXT("RerollButton")));
		UButtonWidget* ResetButton = Cast<UButtonWidget>(Potential->GetWidgetFromName(TEXT("ResetButton")));
		Check(TEXT("inline_potential_reset_bound_without_execution"), ResetButton && ResetButton->OnClicked().IsBoundToObject(Potential));
		Check(TEXT("inline_potential_removed_card_caps_and_reset_prose"),
			!Potential->GetWidgetFromName(TEXT("CardCap0")) && !Potential->GetWidgetFromName(TEXT("CardCap1"))
			&& !Potential->GetWidgetFromName(TEXT("CardCap2")) && !Potential->GetWidgetFromName(TEXT("ResetSubText")));
		Check(TEXT("inline_potential_removed_short_status_and_item_slots"), !Potential->GetWidgetFromName(TEXT("StatusText"))
			&& !Potential->GetWidgetFromName(TEXT("CardSlot0")) && !Potential->GetWidgetFromName(TEXT("CardCellPlate0")),
			TEXT("명함 부족 문구 대신 재설정 버튼이 구매로 바뀐다. 명함 칸은 아이템 카드가 아니라 타일 판 전체가 입력을 받는다."));
		const UWidget* CostChip = Potential->GetWidgetFromName(TEXT("ResetCostChip"));
		const UWidget* ShopIcon = Potential->GetWidgetFromName(TEXT("ResetShopIcon"));
		const auto* ResetLabel = Cast<UCommonTextBlock>(Potential->GetWidgetFromName(TEXT("ResetLabelText")));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const bool bClicked = ClickPotentialCardTile(Potential, Index);
			int32 VisibleCheckCount = 0;
			for (int32 OtherIndex = 0; OtherIndex < 3; ++OtherIndex)
			{
				const UWidget* OtherCheck = Potential->GetWidgetFromName(*FString::Printf(TEXT("CardCheck%d"), OtherIndex));
				if (OtherCheck && OtherCheck->GetVisibility() == ESlateVisibility::HitTestInvisible) ++VisibleCheckCount;
				Check(FString::Printf(TEXT("inline_potential_card_%d_check_%d_matches_selection"), Index, OtherIndex),
					OtherCheck && OtherCheck->GetVisibility() == (OtherIndex == Index ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden));
			}
			const int32 Count = Items->GetItemCount(BusinessCardTypes[Index]);
			FShopItemTable CardDefinition;
			const bool bDefinition = Tables->GetShopItemByItemType(BusinessCardTypes[Index], CardDefinition);
			const auto* Name = Cast<UCommonTextBlock>(Potential->GetWidgetFromName(*FString::Printf(TEXT("CardName%d"), Index)));
			const auto* CountLabel = Cast<UCommonTextBlock>(Potential->GetWidgetFromName(*FString::Printf(TEXT("CardCountText%d"), Index)));
			const auto* Chance = Cast<UCommonTextBlock>(Potential->GetWidgetFromName(*FString::Printf(TEXT("CardChanceText%d"), Index)));
			// 0장이어도 버튼은 산다 — 같은 자리에서 그 명함 구매로 바뀐다
			const bool bActionMatchesCount = ResetLabel && CostChip && ShopIcon && (Count > 0
				? ResetLabel->GetText().EqualTo(NSLOCTEXT("PotentialResetWorkspace", "ResetAction", "재설정")) && CostChip->IsVisible() && !ShopIcon->IsVisible()
				: bDefinition && ResetLabel->GetText().EqualTo(FText::Format(NSLOCTEXT("PotentialResetWorkspace", "BuyCard", "{0} 구매"), CardDefinition.DisplayName))
					&& !CostChip->IsVisible() && ShopIcon->IsVisible());
			Check(FString::Printf(TEXT("inline_potential_card_%d_selection_name_count_action"), Index), bClicked
				&& VisibleCheckCount == 1 && Potential->GetSelectedCard() == BusinessCardTypes[Index]
				&& ResetButton && ResetButton->GetIsEnabled()
				&& bDefinition && Name && Name->GetText().EqualTo(CardDefinition.DisplayName)
				&& CountLabel && CountLabel->GetText().EqualTo(FText::AsNumber(Count))
				&& Chance && Chance->GetText().ToString().EndsWith(TEXT("%")) && bActionMatchesCount);
			if (Count == 0)
			{
				const UWidget* Art = Potential->GetWidgetFromName(*FString::Printf(TEXT("CardArt%d"), Index));
				Check(TEXT("inline_potential_zero_card_stays_selected_and_dims_art"), Art
					&& FMath::IsNearlyEqual(Art->GetRenderOpacity(), 0.42f, 0.01f)
					&& !Potential->GetWidgetFromName(*FString::Printf(TEXT("CardCellLock%d"), Index)));
			}
		}
		UButtonWidget* OddsButton = Cast<UButtonWidget>(Potential->GetWidgetFromName(TEXT("OddsButton")));
		UFunction* OddsClick = OddsButton ? OddsButton->FindFunction(TEXT("HandleButtonClicked")) : nullptr;
		if (OddsClick) OddsButton->ProcessEvent(OddsClick, nullptr);
		Check(TEXT("inline_potential_odds_opens_from_home"), Window->CurrentEmployeePage == EEmployeeWindowPage::Home
			&& Window->OpenOddsPanel.IsValid() && Window->OpenOddsPanel->IsInViewport());
		const bool bHandledBack = Window->NativeOnHandleBackAction();
		Check(TEXT("inline_potential_back_closes_only_odds"), bHandledBack && !Window->OpenOddsPanel.IsValid()
			&& Window->IsActivated() && Window->CurrentEmployeePage == EEmployeeWindowPage::Home);
		Window->HandleHeroEquipmentOpen();
		Check(TEXT("inline_potential_equipment_hides_growth"), Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment
			&& Window->GrowthVBox && Window->GrowthVBox->GetVisibility() == ESlateVisibility::Collapsed);
		Window->HandleHeroEquipmentOpen();
		Check(TEXT("inline_potential_equipment_return_preserves_selected_card"), Window->CurrentEmployeePage == EEmployeeWindowPage::Home
			&& Window->PotentialWorkspace == Potential && Potential->GetSelectedCard() == EItemType::BusinessCardBlack
			&& ResetButton && ResetButton->GetIsEnabled() && ShopIcon && ShopIcon->IsVisible());
		for (const FEmployeeInstance& OtherEmployee : Employees->GetAllEmployees())
		{
			if (OtherEmployee.EmployeeID == OriginalEmployeeId) continue;
			Window->SelectEmployee(OtherEmployee.EmployeeID);
			const auto* Tier = Cast<UCommonTextBlock>(Potential->GetWidgetFromName(TEXT("TierText")));
			Check(TEXT("inline_potential_employee_selection_retargets_same_workspace"), Window->SelectedEmployeeID == OtherEmployee.EmployeeID
				&& Window->PotentialWorkspace == Potential && Potential->GetSelectedCard() == EItemType::BusinessCardBlack
				&& Tier && Tier->GetText().ToString().StartsWith(FLootBoxRarityUtility::GetKoreanName(OtherEmployee.PotentialAbility.CurrentRarity)));
			break;
		}
		Window->SelectEmployee(OriginalEmployeeId);
		Window->bHasExplicitEmployeeContext = true;
		Window->bPotentialReturningFromShop = true;
		Window->DeactivateWidget();
		Window->ActivateWidget();
		auto AfterStats = Employees->OnEmployeeStatsChanged;
		auto AfterItems = Items->OnItemChanged;
		Check(TEXT("inline_potential_shop_return_preserves_single_workspace_and_subscriptions"),
			Window->PotentialWorkspace == Potential && Potential->GetSelectedCard() == EItemType::BusinessCardBlack
			&& Window->SelectedEmployeeID == OriginalEmployeeId && BeforeStatsCount == 1 && BeforeItemsCount == 1
			&& AfterStats.RemoveAll(Potential) == 1 && AfterItems.RemoveAll(Potential) == 1,
			TEXT("Exercises the existing shop-return flag through DeactivateWidget/ActivateWidget; actual stack click routing is separate."));
		auto* ShopButton = Cast<UButtonWidget>(Potential->GetWidgetFromName(TEXT("ShopButton")));
		Check(TEXT("inline_potential_shop_action_bound"), ShopButton && ShopButton->OnClicked().IsBoundToObject(Potential)
			&& Potential->OnRequestShop.IsBoundToObject(Window), TEXT("No shop purchase or reset transaction is executed."));
	}

	void CheckEmployeeWindow(UWorld* InWorld, UTableManagerSubsystem* Tables, UEmployeeManager* Employees)
	{
		const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeWindow);
		TStrongObjectPtr<UEmployeeWindowWidget> Window(Class ? CreateWidget<UEmployeeWindowWidget>(InWorld->GetFirstPlayerController(), Class) : nullptr);
		if (!Window.IsValid()) { Check(TEXT("employee_window_registered"), false); return; }
		Window->AddToViewport(200);
		Window->OpenWithContext(EmployeeId, true);
		Window->ActivateWidget();
		auto* FirstList = Window->EquipmentList.Get();
		if (bInlineValidation)
		{
			// 인라인 인스펙터는 워크스페이스(단독 창)의 물건이었다 — 창은 이제 제 인라인 상세를 갖는다.
			if (bPotentialValidation) CheckInlinePotential(Window.Get(), Employees);
			else CheckMobileManagement(Window.Get(), Employees);
			if (!bPotentialValidation && !ItemIds.IsEmpty()) Window->HandleEquipmentItemPicked(ItemIds[0]);
			Window->DeactivateWidget();
			Check(TEXT("window_close_clears_inline_detail_and_stage"),
				!Window->InlineDetailInstanceId.IsValid() && !Window->LiveStage);
			Window->RemoveFromParent();
			return;
		}
		if (Window->HeroEquipmentSlot0 && FirstList)
		{
			auto BeforeBindings = Window->EquipmentManager->OnEquipmentChanged;
			const int32 BeforeCount = BeforeBindings.RemoveAll(Window.Get());
			for (int32 Pass = 0; Pass < 5; ++Pass)
			{
				Window->ShowEmployeePage(EEmployeeWindowPage::Home);
				Window->HandleHeroEquipmentSlot(0);
				Window->NativeOnHandleBackAction();
			}
			// 목록은 BindWidget 이라 재진입해도 하나뿐이다 — 옛 계약(워크스페이스 1개)의 새 이름이다.
			Check(TEXT("page_reentry_single_list"), FirstList == Window->EquipmentList && Window->CurrentEmployeePage == EEmployeeWindowPage::Equipment);
			auto AfterBindings = Window->EquipmentManager->OnEquipmentChanged;
			const int32 AfterCount = AfterBindings.RemoveAll(Window.Get());
			Check(TEXT("page_reentry_single_equipment_subscription"), BeforeCount == 1 && AfterCount == 1, FString::Printf(TEXT("Exact subscriber counts from delegate copies: before=%d after=%d. Preview actor allocations are excluded."), BeforeCount, AfterCount));
			for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EEmployeeEquipmentSlot::Count); ++SlotIndex)
			{
				const auto* Equipped = Window->EquipmentManager->GetEquippedInstance(EmployeeId, static_cast<EEmployeeEquipmentSlot>(SlotIndex));
				const auto* Definition = Equipped ? Tables->FindEmployeeEquipmentDefinition(Equipped->DefinitionId) : nullptr;
				const auto* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
				auto* SlotButton = Cast<UButtonWidget>(Window->GetWidgetFromName(FName(FString::Printf(TEXT("HeroEquipmentSlot%d"), SlotIndex))));
				auto* GradeFrame = SlotButton ? Cast<UImage>(SlotButton->GetWidgetFromName(TEXT("GradeFrame"))) : nullptr;
				auto* FrameMID = GradeFrame ? Cast<UMaterialInstanceDynamic>(GradeFrame->GetBrush().GetResourceObject()) : nullptr;
				Check(FString::Printf(TEXT("equipped_slot_%d_actual_grade_frame"), SlotIndex), GradeFrame && (Grade
					? FrameMID && FrameMID->K2_GetVectorParameterValue(TEXT("LineCol")).Equals(Grade->Color) && GradeFrame->GetVisibility() == ESlateVisibility::HitTestInvisible
					: GradeFrame->GetVisibility() == ESlateVisibility::Collapsed));
			}
			// Local page transitions use the same handlers as the mobile controls; prompt routing is checked separately.
			for (EEmployeeWindowPage Page : {EEmployeeWindowPage::Home, EEmployeeWindowPage::Equipment})
			{
				Window->ShowEmployeePage(Page);
				Check(FString::Printf(TEXT("employee_local_page_%d_preserves_context"), static_cast<int32>(Page)), Window->SelectedEmployeeID == EmployeeId
					&& Window->CurrentEmployeePage == Page
					&& Window->PotentialWorkspace && Window->PotentialWorkspace->GetParent() == Window->PotentialHost
					&& (Page == EEmployeeWindowPage::Home || FirstList->ContextEmployeeId == EmployeeId));
			}
			// [삭제] employee_storage_toggles_inventory_view_in_place — 보관함/인벤토리 뷰는 워크스페이스 기능이다.
			//        칼럼은 「이 직원이 낄 수 있는 것」만 보여주고 전체 보관함은 단독 창이 맡는다.
			if (auto* PC = Cast<AMainMapPlayerController>(InWorld->GetFirstPlayerController()))
			{
				Window->HandleHeroEquipmentOpen();
				Window->DeactivateWidget();
				const auto ShopClass = Tables->GetWidgetClass(EWidgetType::ShopPanel);
				TStrongObjectPtr<UShopPanelWidget> Shop(ShopClass ? CreateWidget<UShopPanelWidget>(PC, ShopClass) : nullptr);
				if (Shop.IsValid())
				{
					Shop->AddToViewport(210); Shop->ActivateWidget(); Shop->DeactivateWidget(); Shop->RemoveFromParent();
				}
				Window->ActivateWidget();
				Check(TEXT("employee_shop_reactivation_restores_ui_input"), Shop.IsValid() && PC->GetCurrentInputMode() == EInputMode::UI
					&& Window->IsActivated() && Window->SelectedEmployeeID == EmployeeId,
					TEXT("Real ShopPanel/EmployeeWindow activation lifecycle, no purchase. Stack click routing is checked separately in PIE."));
			}
			for (const FEmployeeInstance& Other : Employees->GetAllEmployees())
			{
				if (Other.EmployeeID == EmployeeId) continue;
				FEmployeeInstance* LiveOther = Employees->GetEmployeeData(Other.EmployeeID);
				if (!LiveOther) continue;
				const int32 OriginalBuilding = LiveOther->AssignedBuildingIndex;
				const int32 TestBuilding = Window->BuildingIndex + 10000;
				LiveOther->AssignedBuildingIndex = TestBuilding;
				Window->OpenWithContext(Other.EmployeeID, true);
				Window->DeactivateWidget(); Window->ActivateWidget();
				Check(TEXT("cross_building_shop_return_context"), Window->BuildingIndex == TestBuilding && Window->SelectedEmployeeID == Other.EmployeeID
					&& FirstList->ContextEmployeeId == Other.EmployeeID);
				Window->ShowEmployeePage(EEmployeeWindowPage::Home);
				Check(TEXT("cross_building_selection_to_training"), Window->BuildingIndex == TestBuilding && Window->SelectedEmployeeID == Other.EmployeeID, TEXT("AssignedBuildingIndex changed only in live employee memory during this synchronous check, then restored. No save or cache write."));
				LiveOther->AssignedBuildingIndex = OriginalBuilding;
				Window->OpenWithContext(EmployeeId, true);
				break;
			}
		}
		else Check(TEXT("employee_navigation_binding"), false);
		Window->ReleaseHeroLiveView();
		Window->DeactivateWidget();
		Window->RemoveFromParent();
	}

	void ClosePotentialShopRoute()
	{
		auto* PromptStack = ShopRouteBase.IsValid()
			? Cast<UCommonActivatableWidgetContainerBase>(ShopRouteBase->GetWidgetFromName(TEXT("PromptStack"))) : nullptr;
		if (PromptStack)
		{
			if (ShopRouteWindow.IsValid() && PromptStack->GetWidgetList().Contains(ShopRouteWindow.Get())) PromptStack->RemoveWidget(*ShopRouteWindow.Get());
			if (ShopRouteShop.IsValid() && PromptStack->GetWidgetList().Contains(ShopRouteShop.Get())) PromptStack->RemoveWidget(*ShopRouteShop.Get());
		}
		ShopRouteWindow.Reset();
		ShopRouteShop.Reset();
		ShopRoutePotential.Reset();
		ShopRouteBase.Reset();
		ShopRoutePreviousPrompt.Reset();
		ShopRouteItemsBefore.Reset();
		ShopRoutePhase = 0;
	}

	void BeginPotentialShopRoute()
	{
		auto* Instance = PlayWorld->GetGameInstance();
		auto* UI = Instance->GetSubsystem<UUIManagerSubsystem>();
		const auto* Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
		ShopRouteBase = UI ? UI->GetUIBase() : nullptr;
		const auto Class = Tables ? Tables->GetWidgetClass(EWidgetType::EmployeeWindow) : nullptr;
		if (!ShopRouteBase.IsValid() || !Class || !GetItemInventory())
		{
			Check(TEXT("inline_potential_real_shop_route_dependencies"), false);
			return;
		}
		ShopRoutePromptCount = ShopRouteBase->GetPromptStackCount();
		ShopRoutePreviousPrompt = ShopRouteBase->GetActivePromptWidget();
		ShopRouteWindow.Reset(Cast<UEmployeeWindowWidget>(ShopRouteBase->PushPromptClass(Class.Get(), [this](UCommonActivatableWidget& Widget)
		{
			CastChecked<UEmployeeWindowWidget>(&Widget)->OpenWithContext(EmployeeId, false);
		})));
		Check(TEXT("inline_potential_real_shop_route_employee_push"), ShopRouteWindow.IsValid());
		if (!ShopRouteWindow.IsValid()) { ClosePotentialShopRoute(); return; }
		ShopRoutePhase = 1;
		ShopRouteDeadline = FPlatformTime::Seconds() + 5.0;
	}

	void TickPotentialShopRoute()
	{
		if (!ShopRouteBase.IsValid() || !ShopRouteWindow.IsValid() || FPlatformTime::Seconds() > ShopRouteDeadline)
		{
			Check(TEXT("inline_potential_real_shop_route_completed_in_time"), false, FString::Printf(TEXT("phase=%d, timeout=5s per transition"), ShopRoutePhase));
			ClosePotentialShopRoute();
			return;
		}
		UCommonActivatableWidget* ActivePrompt = ShopRouteBase->GetActivePromptWidget();
		if (ShopRoutePhase == 1 && ActivePrompt == ShopRouteWindow.Get() && ShopRouteWindow->IsActivated())
		{
			ShopRouteWindow->ShowEmployeePage(EEmployeeWindowPage::Home);
			ShopRoutePotential = ShopRouteWindow->PotentialWorkspace;
			auto* Potential = ShopRoutePotential.Get();
			auto* ShopButton = Potential ? Cast<UButtonWidget>(Potential->GetWidgetFromName(TEXT("ShopButton"))) : nullptr;
			UFunction* ShopClick = ShopButton ? ShopButton->FindFunction(TEXT("HandleButtonClicked")) : nullptr;
			if (!ShopClick || !ClickPotentialCardTile(Potential, 2)) { Check(TEXT("inline_potential_real_shop_route_controls"), false); ClosePotentialShopRoute(); return; }
			ShopRouteItemsBefore = GetItemInventory()->GetAllItems();
			ShopButton->ProcessEvent(ShopClick, nullptr);
			auto* PromptStack = Cast<UCommonActivatableWidgetContainerBase>(ShopRouteBase->GetWidgetFromName(TEXT("PromptStack")));
			if (PromptStack && PromptStack->GetNumWidgets() == ShopRoutePromptCount + 2)
				ShopRouteShop = Cast<UShopPanelWidget>(PromptStack->GetWidgetList().Last());
			Check(TEXT("inline_potential_real_shop_button_pushes_shop"), ShopRouteShop.IsValid());
			if (!ShopRouteShop.IsValid()) { ClosePotentialShopRoute(); return; }
			ShopRoutePhase = 2;
			ShopRouteDeadline = FPlatformTime::Seconds() + 5.0;
		}
		else if (ShopRoutePhase == 2 && ShopRouteShop.IsValid() && ActivePrompt == ShopRouteShop.Get() && ShopRouteShop->IsActivated())
		{
			Check(TEXT("inline_potential_real_shop_suspends_employee"), !ShopRouteWindow->IsActivated()
				&& ShopRouteWindow->bPotentialReturningFromShop && ShopRouteWindow->SelectedEmployeeID == EmployeeId);
			auto* Header = Cast<UPanelHeaderWidget>(ShopRouteShop->GetWidgetFromName(TEXT("PanelHeader")));
			auto* CloseWidget = Header ? Header->GetCloseButton() : nullptr;
			auto* CloseButton = CloseWidget ? Cast<UButton>(CloseWidget->GetWidgetFromName(TEXT("CloseBtn"))) : nullptr;
			Check(TEXT("inline_potential_real_shop_close_button_bound"), CloseButton && CloseButton->OnClicked.IsBound());
			if (!CloseButton || !CloseButton->OnClicked.IsBound()) { ClosePotentialShopRoute(); return; }
			CloseButton->OnClicked.Broadcast();
			ShopRoutePhase = 3;
			ShopRouteDeadline = FPlatformTime::Seconds() + 5.0;
		}
		else if (ShopRoutePhase == 3 && ActivePrompt == ShopRouteWindow.Get() && ShopRouteWindow->IsActivated())
		{
			auto* Items = GetItemInventory();
			auto* Employees = PlayWorld->GetGameInstance()->GetSubsystem<UEmployeeManager>();
			auto StatsBindings = Employees->OnEmployeeStatsChanged;
			auto ItemBindings = Items->OnItemChanged;
			Check(TEXT("inline_potential_real_shop_return_restores_context"), ShopRoutePotential.IsValid()
				&& ShopRouteWindow->PotentialWorkspace == ShopRoutePotential.Get() && ShopRouteWindow->SelectedEmployeeID == EmployeeId
				&& ShopRouteWindow->CurrentEmployeePage == EEmployeeWindowPage::Home && !ShopRouteWindow->bPotentialReturningFromShop
				&& ShopRoutePotential->GetSelectedCard() == EItemType::BusinessCardBlack
				&& StatsBindings.RemoveAll(ShopRoutePotential.Get()) == 1 && ItemBindings.RemoveAll(ShopRoutePotential.Get()) == 1
				&& (!ShopRouteShop.IsValid() || !ShopRouteShop->IsActivated()) && ShopRouteBase->GetPromptStackCount() == ShopRoutePromptCount + 1);
			Check(TEXT("inline_potential_real_shop_route_does_not_consume_items"), Items->GetAllItems().OrderIndependentCompareEqual(ShopRouteItemsBefore),
				TEXT("Real PromptStack ShopButton -> ShopPanel header close -> EmployeeWindow return. No purchase or reset is clicked."));
			ShopRouteWindow->DeactivateWidget();
			ShopRoutePhase = 4;
			ShopRouteDeadline = FPlatformTime::Seconds() + 5.0;
		}
		else if (ShopRoutePhase == 4 && ActivePrompt == ShopRoutePreviousPrompt.Get()
			&& ShopRouteBase->GetPromptStackCount() == ShopRoutePromptCount)
		{
			Check(TEXT("inline_potential_real_shop_route_restores_prompt_stack"), !ShopRouteWindow->IsActivated());
			ClosePotentialShopRoute();
		}
	}

	bool Start(UWorld* InWorld, const TArray<FString>& Args)
	{
		PlayWorld = InWorld;
		auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
		auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		auto* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
		auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		bColumnValidation = Args.Num() > 2 && Args[2] == TEXT("employee-columns");
		bTrainingValidation = bColumnValidation || (Args.Num() > 2 && Args[2] == TEXT("employee-training"));
		if (!Tables || !Employees || ((!bTrainingValidation || bColumnValidation) && !Equipment) || !InWorld || !InWorld->GetFirstPlayerController())
		{
			UE_LOG(LogTemp, Error, TEXT("EquipmentUI start: missing runtime dependency GI=%d tables=%d employees=%d equipment=%d player=%d"), Instance != nullptr, Tables != nullptr, Employees != nullptr, Equipment != nullptr, InWorld && InWorld->GetFirstPlayerController() != nullptr);
			return false;
		}
		const TArray<FEmployeeInstance> EmployeeList = Employees->GetAllEmployees();
		EmployeeId = Args.Num() > 1 && Args[1] != TEXT("auto") ? FCString::Atoi(*Args[1]) : EmployeeList.IsEmpty() ? INDEX_NONE : EmployeeList[0].EmployeeID;
		bPotentialValidation = Args.Num() > 2 && Args[2] == TEXT("employee-potential");
		bMobileValidation = Args.Num() > 2 && Args[2] == TEXT("employee-mobile");
		bInlineValidation = bPotentialValidation || bMobileValidation || (Args.Num() > 2 && Args[2] == TEXT("employee-inline"));
		if (bInlineValidation) Scenarios = {TEXT("employee_window_training"), TEXT("employee_starforce_reference"), TEXT("employee_window_empty"), TEXT("employee_window_selected"), TEXT("employee_window_detail"), TEXT("employee_window_swap"), TEXT("employee_window_enhance"), TEXT("employee_window_appearance"), TEXT("employee_window_gacha"), TEXT("employee_window_lean_home"), TEXT("employee_window_stress"), TEXT("employee_window_potential"), TEXT("employee_window_potential_empty")};
		if (bMobileValidation)
		{
			Scenarios = {TEXT("employee_window_training"), TEXT("employee_window_populated"), TEXT("employee_window_selected"), TEXT("employee_window_lean_home"), TEXT("employee_window_stress"), TEXT("employee_window_potential")};
			CaptureSizes = {FIntPoint(1280, 720), FIntPoint(1600, 720), FIntPoint(1680, 720),
				FIntPoint(1920, 1080), FIntPoint(2400, 1080), FIntPoint(2520, 1080)};
		}
		if (bPotentialValidation)
		{
			// _fx_* = 재설정 연출을 한 시점에 세워 그린 프레임(줄 다시 뽑기·고점 강조·전체 화면 등급 상승 착지/유지)
			Scenarios = {TEXT("employee_window_lean_home"), TEXT("employee_window_potential"), TEXT("employee_window_potential_empty"),
				TEXT("employee_window_potential_fx_lines"), TEXT("employee_window_potential_fx_top"),
				TEXT("employee_window_potential_fx_tierup_hit"), TEXT("employee_window_potential_fx_tierup_hold")};
			CaptureSizes = {FIntPoint(2304, 1440), FIntPoint(3120, 1440)};
		}
		if (bTrainingValidation)
		{
			Scenarios = {TEXT("employee_window_training"), TEXT("employee_window_stress")};
			if (bColumnValidation) Scenarios.Add(TEXT("employee_window_selected"));
			CaptureSizes = {FIntPoint(2304, 1440), FIntPoint(3120, 1440)};
		}
		if (!Employees->GetEmployeeData(EmployeeId)) { UE_LOG(LogTemp, Error, TEXT("EquipmentUI start: employee %d not found; owned employee count=%d, parsed arguments=%d"), EmployeeId, EmployeeList.Num(), Args.Num()); return false; }
		OutputDirectory = Args.Num() > 0 ? Args[0] : FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentUIValidation"));
		if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*OutputDirectory)) { UE_LOG(LogTemp, Error, TEXT("EquipmentUI start: cannot create output directory %s"), *OutputDirectory); return false; }
		Report->SetStringField(TEXT("source"), TEXT("Actual PIE GameInstance / DT_WidgetClass / FWidgetRenderer. Desktop virtual-layout validation; not physical-phone validation."));
		Report->SetStringField(TEXT("capture_mode"), bColumnValidation ? TEXT("employee-columns") : bTrainingValidation ? TEXT("employee-training") : bPotentialValidation ? TEXT("employee-potential") : bMobileValidation ? TEXT("employee-mobile") : bInlineValidation ? TEXT("employee-inline") : TEXT("full"));
		if (bMobileValidation)
		{
			Report->SetStringField(TEXT("source"), TEXT("Actual PIE widgets rendered at phone viewport pixel sizes with UUserInterfaceSettings DPI scaling. Native Slate glyph rasterization; not physical-phone validation."));
			Report->SetStringField(TEXT("coordinates"), TEXT("Widget x/y/width/height are root logical UI units. screen_* are output pixels. font_size is font points; nominal_font_size_px is points times layout scale, not measured glyph height. measured_line_*_px use Slate font metrics at the effective layout scale."));
			Report->SetStringField(TEXT("safe_area_policy"), TEXT("Authored SafeZone and 72-unit insets are retained. Desktop safe area is used; physical device notches, OS density and touch input are not emulated."));
			Report->SetStringField(TEXT("fixture_policy"), TEXT("Populated view uses the original employee's stats with enhancement15, three legal Legendary potential options and24 cached roster entries reusing owned names/portraits. EmployeeManager data is restored synchronously before every frame yield. No transaction, recruitment, save or backend request is issued."));
		}
		Report->SetNumberField(TEXT("employee_id"), EmployeeId);
		if (bTrainingValidation)
		{
			if (bColumnValidation)
			{
				for (const auto& Pair : Equipment->GetState().Instances) ItemIds.Add(Pair.Key);
				ItemIds.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
				Report->SetNumberField(TEXT("owned_instance_count"), ItemIds.Num());
				if (ItemIds.IsEmpty()) { Check(TEXT("column_capture_owned_item_available"), false); return false; }
			}
			Renderer = MakeUnique<FWidgetRenderer>(true);
			return true;
		}
		Report->SetNumberField(TEXT("equipment_revision_before"), Equipment->GetState().Revision);
		for (const auto& Pair : Equipment->GetState().Instances) ItemIds.Add(Pair.Key);
		ItemIds.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
		Report->SetNumberField(TEXT("owned_instance_count"), ItemIds.Num());
		CheckEmployeeWindow(InWorld, Tables, Employees);
		if (bPotentialValidation)
		{
			Renderer = MakeUnique<FWidgetRenderer>(true);
			BeginPotentialShopRoute();
			return true;
		}
		const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentPanel);
		Panel.Reset(Class ? CreateWidget<UEmployeeEquipmentPanelWidget>(InWorld->GetFirstPlayerController(), Class) : nullptr);
		if (!Panel.IsValid()) { UE_LOG(LogTemp, Error, TEXT("EquipmentUI start: failed to create registered panel class %s"), *GetNameSafe(Class.Get())); return false; }
		Panel->AddToViewport(200);
		Panel->OpenWithContext(EmployeeId);
		Panel->ActivateWidget();
		auto* Host = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("WorkspaceHost")));
		Workspace = Host && Host->GetChildrenCount() ? Cast<UEmployeeEquipmentWorkspaceWidget>(Host->GetChildAt(0)) : nullptr;
		if (!Workspace.IsValid()) { UE_LOG(LogTemp, Error, TEXT("EquipmentUI start: WorkspaceHost=%s child count=%d, expected EmployeeEquipmentWorkspaceWidget"), *GetNameSafe(Host), Host ? Host->GetChildrenCount() : -1); Panel->DeactivateWidget(); Panel->RemoveFromParent(); return false; }
		Renderer = MakeUnique<FWidgetRenderer>(true);
		return true;
	}

	void CloseEmbeddedWindow()
	{
		if (!EmbeddedWindow.IsValid()) return;
		TWeakObjectPtr<UEnhanceStarforceModalWidget> ReferenceModal = EmbeddedWindow->OpenEnhanceModal;
		EmbeddedWindow->DeactivateWidget();
		if (ReferenceModal.IsValid())
			Check(TEXT("starforce_reference_closes_with_employee_window"), !ReferenceModal->IsInViewport() && !EmbeddedWindow->OpenEnhanceModal.IsValid());
		EmbeddedWindow->RemoveFromParent();
		EmbeddedWindow.Reset();
	}

	void ApplyPopulatedEmployeeFixture(bool bPopulateRoster)
	{
		UEmployeeWindowWidget* Window = EmbeddedWindow.Get();
		UEmployeeManager* Employees = PlayWorld.IsValid() ? PlayWorld->GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
		if (!Window || !Employees) return;
		const TArray<FEmployeeInstance> Original = Employees->GetAllEmployees();
		TArray<FEmployeeInstance> Fixture = Original;
		FEmployeeInstance* Selected = Fixture.FindByPredicate([this](const FEmployeeInstance& Employee) { return Employee.EmployeeID == EmployeeId; });
		if (!Selected) return;
		for (const FEmployeeInstance& Employee : Original)
			if (Employee.EmployeeName.Len() > Selected->EmployeeName.Len()) Selected->EmployeeName = Employee.EmployeeName;
		Selected->EnhancementLevel = UEmployeeManager::MaxEnhancementLevel;
		Selected->Level = FMath::Max(Selected->Level, 30);
		Selected->PotentialAbility.CurrentRarity = ELootBoxRarity::Legendary;
		Selected->PotentialAbility.Options.Reset();
		float MinimumValue = 0.f, MaximumValue = 0.f;
		UEmployeePotentialHelper::GetValueRangeForRarity(ELootBoxRarity::Legendary, MinimumValue, MaximumValue);
		for (EPotentialOptionType OptionType : {EPotentialOptionType::CriticalChance, EPotentialOptionType::CriticalDamage, EPotentialOptionType::WorkEfficiency})
		{
			FPotentialOptionLine Option;
			Option.OptionType = OptionType;
			Option.Rarity = ELootBoxRarity::Legendary;
			Option.Value = MaximumValue;
			Selected->PotentialAbility.Options.Add(Option);
		}
		Employees->SetEmployeeList(Fixture);
		ON_SCOPE_EXIT
		{
			Employees->SetEmployeeList(Original);
			if (bPopulateRoster)
			{
				const TArray<FEmployeeInstance> Restored = Employees->GetAllEmployees();
				bool bRestored = Restored.Num() == Original.Num();
				for (int32 Index = 0; bRestored && Index < Original.Num(); ++Index)
					bRestored = FEmployeeInstance::StaticStruct()->CompareScriptStruct(&Original[Index], &Restored[Index], 0);
				Check(FString::Printf(TEXT("mobile_%d_%d_fixture_restored_before_frame_yield"), CurrentWidth, CurrentHeight), bRestored);
			}
		};
		Window->RefreshSelected();
		if (bPopulateRoster && Window->RosterListView)
		{
			// List entries hold employee snapshots, so pagination can be exercised without adding employees to the manager.
			Window->RefreshRoster();
			for (int32 Index = Window->RosterListView->GetNumItems(); Index < 24; ++Index)
			{
				UEntityCardData* CardData = NewObject<UEntityCardData>(Window);
				CardData->EmployeeInfo = Fixture[Index % Fixture.Num()];
				Window->RosterListView->AddItem(CardData);
			}
		}
	}

	TMap<EItemType, int32> CardFixtureBackup;
	// 복원은 다음 시나리오의 PrepareScenario 에서도 일어나므로, fixture 를 건 시나리오/해상도를 따로 붙들어 둔다.
	FString CardFixtureScenario;
	int32 CardFixtureWidth = 0;
	bool bCardFixtureActive = false;
	static constexpr EItemType BusinessCardTypes[3] = {EItemType::BusinessCardPaper, EItemType::BusinessCardGold, EItemType::BusinessCardBlack};

	UItemInventoryManager* GetItemInventory() const
	{
		UGameInstance* Instance = PlayWorld.IsValid() ? PlayWorld->GetGameInstance() : nullptr;
		return Instance ? Instance->GetSubsystem<UItemInventoryManager>() : nullptr;
	}

	// 명함 수량만 기존 AddItem/SpendItem 경로로 맞춘다. bShouldSave=false 라 저장 요청은 발생하지 않는다.
	void ApplyBusinessCardFixture(int32 PaperCount, int32 GoldCount, int32 BlackCount)
	{
		UItemInventoryManager* Items = GetItemInventory();
		if (!Items || bCardFixtureActive) return;
		const int32 Wanted[3] = {PaperCount, GoldCount, BlackCount};
		CardFixtureScenario = CurrentScenario;
		CardFixtureWidth = CurrentWidth;
		CardFixtureBackup.Reset();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const int32 Current = Items->GetItemCount(BusinessCardTypes[Index]);
			CardFixtureBackup.Add(BusinessCardTypes[Index], Current);
			const int32 Delta = Wanted[Index] - Current;
			if (Delta > 0) Items->AddItem(BusinessCardTypes[Index], Delta, false, true);
			else if (Delta < 0) Items->SpendItem(BusinessCardTypes[Index], -Delta, false);
		}
		bCardFixtureActive = true;
	}

	void RestoreBusinessCardFixture()
	{
		if (!bCardFixtureActive) return;
		bCardFixtureActive = false;
		UItemInventoryManager* Items = GetItemInventory();
		bool bRestored = Items != nullptr;
		if (Items)
		{
			for (const TPair<EItemType, int32>& Entry : CardFixtureBackup)
			{
				const int32 Delta = Entry.Value - Items->GetItemCount(Entry.Key);
				if (Delta > 0) Items->AddItem(Entry.Key, Delta, false, true);
				else if (Delta < 0) Items->SpendItem(Entry.Key, -Delta, false);
				bRestored &= Items->GetItemCount(Entry.Key) == Entry.Value;
			}
		}
		Check(FString::Printf(TEXT("%s_%d_business_card_fixture_restored"), *CardFixtureScenario, CardFixtureWidth), bRestored,
			TEXT("Business card counts changed only in ItemInventoryManager memory with bShouldSave=false, then restored synchronously after the capture. No save or backend request is issued."));
		CardFixtureBackup.Reset();
		CardFixtureScenario.Reset();
		CardFixtureWidth = 0;
	}

	// 스트레스 픽스처 — 가장 긴 이름 / 가장 큰 수치 / 6칸 장착으로 텍스트 피팅을 실제로 압박한다.
	// 전부 메모리 값 교체(SetEmployeeList · RestoreState)라 트랜잭션도 저장도 리비전 변화도 없다.
	static constexpr int32 StressEmployeeCount = 6;
	static constexpr int32 StressHeroIndex = 3;
	static constexpr int32 StressStatPoints = 999;
	static constexpr int32 StressEquipEnhanceLevel = 12;
	TArray<FEmployeeInstance> StressEmployeeBackup, StressEmployees;
	FEmployeeEquipmentState StressEquipmentBackup, StressEquipment;
	// 합성 직원 ID 로 만들어 둔 증명사진 사본 — 복원에서 지운다(없으면 타일 3·4 가 검은 실루엣으로 남는다)
	TArray<FString> StressPortraitFiles;
	bool bStressFixtureActive = false;

	void BuildStressFixture(UEmployeeManager* Employees, UTableManagerSubsystem* Tables)
	{
		// 2 · 4 · 6 · 9 자 — 타일 이름 밴드와 이름 행이 같은 픽스처에서 동시에 시험된다
		const TCHAR* const StressNames[StressEmployeeCount] = {TEXT("김철"), TEXT("박영희다"), TEXT("이무개삼사오"),
			TEXT("최장이름테스트직원"), TEXT("한글여섯자임"), TEXT("정사각")};
		const ELootBoxRarity StressRarities[StressEmployeeCount] = {ELootBoxRarity::Common, ELootBoxRarity::Rare,
			ELootBoxRarity::Epic, ELootBoxRarity::Legendary, ELootBoxRarity::Rare, ELootBoxRarity::Epic};
		const int32 StressLevels[StressEmployeeCount] = {12, 30, 12, 30, 12, 30};

		StressEmployees = StressEmployeeBackup;
		int32 NextEmployeeId = 1;
		for (const FEmployeeInstance& Employee : StressEmployees) NextEmployeeId = FMath::Max(NextEmployeeId, Employee.EmployeeID + 1);
		// 초상 포인터는 원본 사본이 그대로 들고 간다 — 끊으면 매니저 목록을 갈아 끼우는 동안 텍스처가 GC 된다
		while (StressEmployees.Num() < StressEmployeeCount)
		{
			const FEmployeeInstance& Source = StressEmployees[StressEmployees.Num() % StressEmployeeBackup.Num()];
			FEmployeeInstance Extra = Source;
			Extra.EmployeeID = NextEmployeeId++;
			// 초상은 Saved/Portraits/{ID}.png 라 새 ID 로는 파일이 없다 — 원본 사진을 그 ID 로 복사해 둔다
			const FString PortraitDir = FPaths::ProjectSavedDir() / TEXT("Portraits");
			const FString SourcePng = PortraitDir / (FString::FromInt(Source.EmployeeID) + TEXT(".png"));
			const FString ClonePng = PortraitDir / (FString::FromInt(Extra.EmployeeID) + TEXT(".png"));
			if (FPaths::FileExists(SourcePng) && !FPaths::FileExists(ClonePng)
				&& IFileManager::Get().Copy(*ClonePng, *SourcePng) == COPY_OK)
			{
				StressPortraitFiles.Add(ClonePng);
			}
			StressEmployees.Add(Extra);
		}
		const int32 SelectedIndex = StressEmployees.IndexOfByPredicate(
			[this](const FEmployeeInstance& Employee) { return Employee.EmployeeID == EmployeeId; });
		if (SelectedIndex == INDEX_NONE) { StressEmployees.Reset(); return; }
		StressEmployees.Swap(SelectedIndex, StressHeroIndex);
		StressEmployees.SetNum(StressEmployeeCount);

		for (int32 Index = 0; Index < StressEmployeeCount; ++Index)
		{
			FEmployeeInstance& Employee = StressEmployees[Index];
			Employee.EmployeeName = StressNames[Index];
			Employee.SpawnRarity = StressRarities[Index];
			Employee.Level = StressLevels[Index];
			Employee.EnhancementLevel = UEmployeeManager::MaxEnhancementLevel;
		}

		FEmployeeInstance& Hero = StressEmployees[StressHeroIndex];
		Hero.UnspentStatPoints = 99999;
		const int32 DisciplineCount = static_cast<int32>(EProductionDiscipline::Count);
		Hero.DisciplinePoints.Init(99, DisciplineCount);
		Hero.Stats.WorkSpeed = StressStatPoints;
		Hero.Stats.CritChance = StressStatPoints;
		Hero.Stats.Composure = StressStatPoints;
		Hero.Stats.ExpGain = StressStatPoints;
		Hero.Stats.Stamina = StressStatPoints;
		Hero.Stats.CritDamage = StressStatPoints;
		Hero.PotentialAbility.CurrentRarity = ELootBoxRarity::Legendary;
		Hero.PotentialAbility.Options.Reset();
		float MinimumValue = 0.f, MaximumValue = 0.f;
		UEmployeePotentialHelper::GetValueRangeForRarity(ELootBoxRarity::Legendary, MinimumValue, MaximumValue);
		// 가장 긴 옵션 이름 하나로 3줄을 채운다 — 잠재 줄은 이름과 값이 한 행에서 경쟁하는 유일한 자리다
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FPotentialOptionLine Option;
			Option.OptionType = EPotentialOptionType::CriticalDamage;
			Option.Rarity = ELootBoxRarity::Legendary;
			Option.Value = MaximumValue;
			Hero.PotentialAbility.Options.Add(Option);
		}

		if (bTrainingValidation) return;
		StressEquipment = StressEquipmentBackup;
		FEmployeeEquipmentLoadout& Loadout = StressEquipment.Loadouts.FindOrAdd(EmployeeId);
		for (int32 SlotIndex = 0; SlotIndex < 6; ++SlotIndex)
		{
			const EEmployeeEquipmentSlot Part = static_cast<EEmployeeEquipmentSlot>(SlotIndex);
			FName PickedId = NAME_None;
			EEmployeeEquipmentGrade PickedGrade = EEmployeeEquipmentGrade::Common;
			for (const TPair<FName, FEmployeeEquipmentDefinitionRow>& Entry : Tables->GetEmployeeEquipmentDefinitions())
			{
				if (Entry.Value.EquipmentSlot != Part) continue;
				if (PickedId.IsNone() || Entry.Value.Grade > PickedGrade) { PickedId = Entry.Key; PickedGrade = Entry.Value.Grade; }
			}
			if (PickedId.IsNone()) continue;
			FEmployeeEquipmentInstance Item;
			Item.InstanceId = FGuid::NewGuid();
			Item.DefinitionId = PickedId;
			Item.EnhanceLevel = StressEquipEnhanceLevel;
			StressEquipment.Instances.Add(Item.InstanceId, Item);
			Loadout.Slots.Add(Part, Item.InstanceId);
		}
	}

	void ApplyStressFixture()
	{
		UEmployeeWindowWidget* Window = EmbeddedWindow.Get();
		UGameInstance* Instance = PlayWorld.IsValid() ? PlayWorld->GetGameInstance() : nullptr;
		UEmployeeManager* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
		auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!Window || !Employees || !Tables || (!bTrainingValidation && !Equipment)) return;
		if (!bStressFixtureActive)
		{
			StressEmployeeBackup = Employees->GetAllEmployees();
			if (Equipment && !bTrainingValidation) StressEquipmentBackup = Equipment->GetState();
			if (StressEmployeeBackup.IsEmpty()) return;
			BuildStressFixture(Employees, Tables);
			if (StressEmployees.IsEmpty()) { Check(CurrentScenario + TEXT("_stress_fixture"), false, TEXT("selected employee missing from the manager list")); return; }
			bStressFixtureActive = true;
			// 목록 재구성은 선택/펀치를 흔든다 — UI 갱신은 첫 적용에서 한 번만
			for (auto& Employee : StressEmployees) Employee.Experience = 123456789.0;
			Employees->SetEmployeeList(StressEmployees);
			if (Equipment && !bTrainingValidation) Equipment->RestoreState(StressEquipment);
			Window->RefreshRoster();
			Window->RefreshSelected();
			if (!bTrainingValidation) Window->RefreshHeroEquipment();
			return;
		}
		// 프레임마다 매니저 값만 다시 밀어 넣는다 — 창이 스스로 갱신할 때 원본으로 되돌아가지 않게
		Employees->SetEmployeeList(StressEmployees);
		if (Equipment && !bTrainingValidation) Equipment->RestoreState(StressEquipment);
	}

	void RestoreStressFixture()
	{
		if (!bStressFixtureActive) return;
		bStressFixtureActive = false;
		UGameInstance* Instance = PlayWorld.IsValid() ? PlayWorld->GetGameInstance() : nullptr;
		UEmployeeManager* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
		auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		if (Employees)
		{
			Employees->SetEmployeeList(StressEmployeeBackup);
		}
		if (Equipment && !bTrainingValidation) Equipment->RestoreState(StressEquipmentBackup);
		const bool bRestored = Employees && (bTrainingValidation || Equipment)
			&& Employees->GetAllEmployees().Num() == StressEmployeeBackup.Num()
			&& (bTrainingValidation || Equipment->GetState().Revision == StressEquipmentBackup.Revision);
		Check(FString::Printf(TEXT("employee_window_stress_%d_fixture_restored"), CurrentWidth), bRestored,
			bTrainingValidation ? TEXT("Employee list and individual experience restored in memory; no transaction or save.")
				: TEXT("Employee list, individual experience and equipment state are swapped in memory only, then restored. No transaction, save or backend request is issued."));
		for (const FString& ClonePath : StressPortraitFiles) IFileManager::Get().Delete(*ClonePath, false, true, true);
		StressPortraitFiles.Reset();
		StressEmployees.Reset();
		StressEmployeeBackup.Reset();
	}

	// UMG 가 안 노출하는 슬레이트 부품(스크롤바 등)을 이름으로 찾는다 — UWidget::GetCachedWidget 이 입구다
	static TSharedPtr<SWidget> FindSlateChildOfType(const TSharedPtr<SWidget>& Root, const FName TypeName)
	{
		if (!Root.IsValid()) return nullptr;
		if (Root->GetType() == TypeName) return Root;
		FChildren* Children = Root->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			if (TSharedPtr<SWidget> Found = FindSlateChildOfType(Children->GetChildAt(Index), TypeName)) return Found;
		}
		return nullptr;
	}

	static bool LocalRectOf(const FGeometry& RootGeometry, const UWidget* TargetWidget, FVector2D& OutStart, FVector2D& OutSize)
	{
		if (!TargetWidget) return false;
		const FGeometry& Geometry = TargetWidget->GetCachedGeometry();
		OutStart = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
		OutSize = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize())) - OutStart;
		return true;
	}

	// 판 안쪽 여백 계약 — 판 이름 / 여백을 재는 내용 위젯 / 기대 4면(음수 = 그 변은 이 위젯이 대표하지 않는다)
	struct FPlatePaddingSpec { const TCHAR* PlateName; const TCHAR* ContentName; double Left, Top, Right, Bottom; };

	// 걸으면서 모으는 것들 — 텍스트/면(버튼·이미지)/논리 부모(겹침 판정의 최근접 공통 조상용)
	struct FFitTargets
	{
		TArray<UTextBlock*> Texts;
		TArray<UWidget*> Solids;
		TMap<UWidget*, UWidget*> ParentOf;
	};

	// 연출 이미지는 글자 위를 지나가도록 저작된 것이고(스윕·글린트·글로우), 알파 0 인 것은 화면에 없다.
	// 히어로 초상은 RT 라 이름 행 높이 구간이 빈 알파다 — 상자를 면으로 세면 전 시나리오 영구 오탐이 된다.
	static bool IsDecorationName(const UWidget* Widget)
	{
		const FString Name = Widget->GetName();
		return Name.Contains(TEXT("Shine")) || Name.Contains(TEXT("Sparkle")) || Name.Contains(TEXT("Glow"))
			|| Name.Contains(TEXT("Glint")) || Name.Contains(TEXT("HeroPortraitImage"));
	}

	// 알파 0 제외는 **면(이미지·버튼)에만** 건다. 텍스트는 페이드 시작 프레임(RefreshHeroStats 가 바뀐 행의
	// 보너스 칩을 RenderOpacity 0 에서 띄운다)에 걸리면 ⑮ 가 지목한 그 칩이 조용히 검사에서 빠진다(T9).
	static bool IsDecorationFace(const UWidget* Widget)
	{
		return Widget->GetRenderOpacity() <= 0.01f || IsDecorationName(Widget);
	}

	void WalkFitTargets(UWidget* Widget, UWidget* LogicalParent, bool bParentVisible, TSet<UWidget*>& Visited, FFitTargets& Out)
	{
		if (!Widget || Visited.Contains(Widget)) return;
		Visited.Add(Widget);
		if (LogicalParent) Out.ParentOf.Add(Widget, LogicalParent);
		const ESlateVisibility WidgetVisibility = Widget->GetVisibility();
		const bool bVisible = bParentVisible && WidgetVisibility != ESlateVisibility::Collapsed
			&& WidgetVisibility != ESlateVisibility::Hidden;
		if (bVisible)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Widget))
			{
				if (!IsDecorationName(Widget)) Out.Texts.Add(Label);
			}
			// 면 = 글자가 올라타면 안 되는 것들. 버튼(UserWidget 변형 포함)과 이미지.
			else if ((Widget->IsA<UImage>() || Widget->IsA<UButtonWidget>() || Widget->IsA<UCommonButtonBase>())
				&& !IsDecorationFace(Widget)) Out.Solids.Add(Widget);
		}
		if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			if (UserWidget->WidgetTree) WalkFitTargets(UserWidget->WidgetTree->RootWidget, Widget, bVisible, Visited, Out);
		}
		if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget)) WalkFitTargets(Switcher->GetActiveWidget(), Widget, bVisible, Visited, Out);
		else if (UPanelWidget* ChildHost = Cast<UPanelWidget>(Widget)) for (UWidget* Child : ChildHost->GetAllChildren()) WalkFitTargets(Child, Widget, bVisible, Visited, Out);
		if (UListViewBase* List = Cast<UListViewBase>(Widget))
		{
			// 로스터 TileView 는 가상화로 재활용/스톡 엔트리를 화면 밖(2560)이나 메인 영역(2304 1열)에 주차한다 —
			// 그 스톡 엔트리의 잉크가 메인 판 위로 겹쳐 보이는 v2_no_text_overlap 오탐을 만든다. 로스터는 전용 게이트
			// (roster_name_ellipsis/lv_badge/hire_ink/roster_scrollbar/v3_roster_vertical_grid)가 따로 있으므로 범용 fit/overlap 걸음에서 뺀다.
			if (List->GetName() != TEXT("RosterListView"))
				for (UUserWidget* Entry : List->GetDisplayedEntryWidgets()) WalkFitTargets(Entry, Widget, bVisible, Visited, Out);
		}
	}

	// 말줄임은 "설정" 이 아니라 "발동" 이 기준이다 — AutoWrap 이 켜져 있거나 WrapTextAt > 0 이면
	// Slate 는 말줄임 대신 감싸기를 택해 글자를 전체 폭으로 그린다(17차 ① 이 이 전제로 통과했다).
	static bool EllipsisActuallyFires(const UTextBlock* Label)
	{
		return Label->GetTextOverflowPolicy() == ETextOverflowPolicy::Ellipsis
			&& !Label->GetAutoWrapText() && Label->GetWrapTextAt() <= 0.f;
	}

	// #1 — 위 세 축이 전부 참인데도 말줄임이 안 걸리는 경우가 있다: 가운데 정렬이면 Slate 가 초과분을
	// 좌우로 균등하게 흘리고 클립이 양끝 글자를 반쪽으로 자른다(로스터 이름 alloc 121 / 잉크 125).
	// UTextLayoutWidget::Justification 은 protected 이고 게터가 없어 리플렉션이 헤더를 안 건드리는 유일한 읽기 경로다.
	static ETextJustify::Type JustificationOf(const UTextBlock* Label)
	{
		if (!Label) return ETextJustify::Left;
		const FByteProperty* Field = CastField<FByteProperty>(Label->GetClass()->FindPropertyByName(TEXT("Justification")));
		return Field ? static_cast<ETextJustify::Type>(Field->GetPropertyValue_InContainer(Label)) : ETextJustify::Left;
	}

	// 말줄임이 우측 한 곳에서만 접히는 조건 — 좌정렬(RTL 은 이 창의 범위 밖)
	static bool EllipsisFoldsOnOneSide(const UTextBlock* Label)
	{
		return EllipsisActuallyFires(Label) && JustificationOf(Label) == ETextJustify::Left;
	}

	// N8 — LineHeightPercentage 도 protected 다. 1 미만이면 라인박스만 줄고 잉크는 상자 아래로 샌다.
	static float LineHeightPercentageOf(const UTextBlock* Label)
	{
		if (!Label) return 1.f;
		const FFloatProperty* Field = CastField<FFloatProperty>(Label->GetClass()->FindPropertyByName(TEXT("LineHeightPercentage")));
		return Field ? Field->GetPropertyValue_InContainer(Label) : 1.f;
	}

	// 글자가 실제로 칠하는 폭/높이. 말줄임·자동 줄바꿈이 걸린 축은 잘려 나가므로 배정 폭을 넘지 않는다.
	static FVector2D InkSizeOf(const UTextBlock* Label, const FVector2D& Allocated)
	{
		FVector2D Ink = Label->GetDesiredSize();
		const bool bWraps = Label->GetAutoWrapText();
		// 가운데 정렬은 말줄임이 걸려 있어도 초과분을 좌우로 흘린다 — 그 텍스트의 잉크는 배정으로 접히지 않는다(#1)
		if (bWraps || (EllipsisActuallyFires(Label) && JustificationOf(Label) != ETextJustify::Center))
			Ink.X = FMath::Min(Ink.X, Allocated.X);
		// 높이는 라인 박스(폰트 메트릭 여백 포함)가 아니라 글리프가 차지하는 근사 잉크 — 1.15 배.
		// 단 감싸진 텍스트는 줄 수만큼 자란다 — 한 줄 근사로 접으면 세로 넘침이 영영 통과한다(R7)
		if (!bWraps) Ink.Y = FMath::Min(Ink.Y, static_cast<double>(Label->GetFont().Size) * 1.15);
		return Ink;
	}

	// 겹쳐 그리라고 있는 부모(Overlay/Canvas) 아래의 서로 다른 가지는 의도된 적층이다 — 위반이 아니다.
	static bool IsDeliberateStack(const TMap<UWidget*, UWidget*>& ParentOf, UWidget* First, UWidget* Second)
	{
		TArray<UWidget*> FirstChain;
		for (UWidget* Cursor = First; Cursor; Cursor = ParentOf.FindRef(Cursor)) FirstChain.Add(Cursor);
		if (FirstChain.Contains(Second)) return true;               // 조상-자손 = 라벨이 자기 면 위에 있는 정상
		for (UWidget* Cursor = Second; Cursor; Cursor = ParentOf.FindRef(Cursor))
		{
			if (Cursor == First) return true;
			const int32 Found = FirstChain.Find(Cursor);
			if (Found != INDEX_NONE) return Cursor->IsA<UOverlay>() || Cursor->IsA<UCanvasPanel>();
		}
		return false;
	}

	// 사용자 판정 "글자 튀어나감 / 정렬 / 패딩 미준수" 의 측정축. 세 검사 모두 모든 시나리오에서 돈다.
	void CheckTextFitOverlapAndPadding()
	{
		// v2_* 는 직원창 라운드의 게이트다 — 타 세션이 소유한 창(employee_starforce_reference 등)까지
		// 우리 게이트가 판정하면 남의 결함으로 이 라운드가 탄다(§G 스코프 가드)
		if (!CurrentScenario.StartsWith(TEXT("employee_window_"))) return;
		UUserWidget* FitRoot = EmbeddedWindow.IsValid() ? static_cast<UUserWidget*>(EmbeddedWindow.Get()) : RenderRoot.Get();
		if (!FitRoot) return;
		TSet<UWidget*> Visited;
		FFitTargets Targets;
		WalkFitTargets(FitRoot, nullptr, true, Visited, Targets);
		if (EmbeddedWindow.IsValid() && EmbeddedWindow->OpenEnhanceModal.IsValid())
			WalkFitTargets(EmbeddedWindow->OpenEnhanceModal.Get(), nullptr, true, Visited, Targets);
		// 드로어 본문은 호스트의 런타임 자식이라 트리 순회에서 빠질 수 있다 — 명시적으로 한 번 더 건다
		if (EmbeddedWindow.IsValid() && EmbeddedWindow->PotentialWorkspace)
			WalkFitTargets(EmbeddedWindow->PotentialWorkspace, nullptr, true, Visited, Targets);
		if (ActiveWorkspace.IsValid()) WalkFitTargets(ActiveWorkspace.Get(), nullptr, true, Visited, Targets);

		// 희망 크기와 할당 크기는 둘 다 그 위젯의 로컬 공간이다 — 루트로 환산하면 ScaleBox 아래에서 축이 갈린다
		int32 TextsChecked = 0;
		TArray<FString> FitFailures, AllocFailures;
		for (const UTextBlock* Label : Targets.Texts)
		{
			if (Label->GetText().IsEmpty()) continue;
			const FVector2D Allocated = Label->GetCachedGeometry().GetLocalSize();
			const FVector2D Ink = InkSizeOf(Label, Allocated);
			// 말줄임이 실제로 걸린 글자는 잉크가 배정으로 접힌다 — 배정이 한 글자도 못 담으면 그 클램프가
			// "안 넘쳤다" 는 거짓 신호가 된다(N1: alloc 0 인 이름이 형제 라벨 위에 그대로 그려졌다)
			const double OneGlyph = static_cast<double>(Label->GetFont().Size);
			if (EllipsisActuallyFires(Label) && Label->GetDesiredSize().X > Allocated.X + 1.0
				&& Allocated.X < OneGlyph && AllocFailures.Num() < 8)
				AllocFailures.Add(FString::Printf(TEXT("%s [%s] alloc %.1f < one glyph %.0f (desired %.1f)"), *Label->GetName(),
					*Label->GetText().ToString().Left(12), Allocated.X, OneGlyph, Label->GetDesiredSize().X));
			if (Allocated.X < 1.0 || Allocated.Y < 1.0 || (Ink.X < 1.0 && Ink.Y < 1.0)) continue;
			++TextsChecked;
			const bool bFits = Ink.X <= Allocated.X + 1.0 && Ink.Y <= Allocated.Y + 1.0;
			if (!bFits && FitFailures.Num() < 12)
				FitFailures.Add(FString::Printf(TEXT("%s [%s] ink %.1fx%.1f (font %d) alloc %.1fx%.1f"), *Label->GetName(),
					*Label->GetText().ToString().Left(12), Ink.X, Ink.Y, Label->GetFont().Size, Allocated.X, Allocated.Y));
		}
		Check(FString::Printf(TEXT("%s_%d_v2_text_fit"), *CurrentScenario, CurrentWidth), FitFailures.IsEmpty(),
			FString::Printf(TEXT("%d visible texts measured (ink = desired clamped only when ellipsis actually fires: AutoWrap off + WrapTextAt 0; height = font x 1.15, wrapped text uses desired; font x 1.15 is a ~3px-generous approximation - eyeball the PNG when the margin is under 4px); %s"),
				TextsChecked, FitFailures.IsEmpty() ? TEXT("all inside their box") : *FString::Join(FitFailures, TEXT(" | "))));
		Check(FString::Printf(TEXT("%s_%d_v2_text_alloc_min"), *CurrentScenario, CurrentWidth), AllocFailures.IsEmpty(),
			FString::Printf(TEXT("%d visible texts; truncating labels must be allocated at least one glyph; %s"),
				TextsChecked, AllocFailures.IsEmpty() ? TEXT("every truncating label has room for a glyph") : *FString::Join(AllocFailures, TEXT(" | "))));

		// 겹침은 배정 상자가 아니라 **잉크**로 본다 — 배정이 좁아도 글자가 밖으로 칠하면 그게 결함이다.
		// 상대는 다른 텍스트와 면(버튼·이미지) 둘 다. Overlay/Canvas 아래의 의도된 적층은 뺀다.
		const FGeometry& OverlapRoot = FitRoot->GetCachedGeometry();
		struct FInkRect { UWidget* Widget; FVector2D Start, Size; bool bText; };
		TArray<FInkRect> Rects;
		for (UTextBlock* Label : Targets.Texts)
		{
			if (Label->GetText().IsEmpty()) continue;
			FVector2D Start, Allocated;
			if (!LocalRectOf(OverlapRoot, Label, Start, Allocated)) continue;
			if (Allocated.X < 1.0 || Allocated.Y < 1.0) continue;
			// 잉크(희망 크기)는 위젯 로컬 공간, Start/Allocated 는 루트 공간이다 — 섞으면 DPI 스케일 아래에서
			// 잉크가 항상 배정보다 작아 보여 규칙이 "배정 상자만 본다" 로 조용히 되돌아간다(R4)
			const FVector2D LocalAllocated = Label->GetCachedGeometry().GetLocalSize();
			FVector2D Ink = InkSizeOf(Label, LocalAllocated);
			Ink.X *= LocalAllocated.X > 0.0 ? Allocated.X / LocalAllocated.X : 1.0;
			Ink.Y *= LocalAllocated.Y > 0.0 ? Allocated.Y / LocalAllocated.Y : 1.0;
			// 배정보다 넓은 잉크는 가운데 정렬에서 좌우로 같이 새 나간다 — 상자를 양쪽으로 벌린다
			Start.X -= FMath::Max(0.0, Ink.X - Allocated.X) * 0.5;
			Ink.X = FMath::Max(Ink.X, Allocated.X);
			Ink.Y = FMath::Min(Ink.Y, Allocated.Y);
			Rects.Add({Label, Start, Ink, true});
		}
		for (UWidget* Solid : Targets.Solids)
		{
			FVector2D Start, Size;
			if (!LocalRectOf(OverlapRoot, Solid, Start, Size)) continue;
			if (Size.X < 4.0 || Size.Y < 4.0) continue;
			Rects.Add({Solid, Start, Size, false});
		}
		int32 PairsChecked = 0;
		TArray<FString> OverlapFailures, DrawerOverlapFailures;
		for (int32 First = 0; First < Rects.Num(); ++First)
		{
			if (!Rects[First].bText) continue;
			for (int32 Second = 0; Second < Rects.Num(); ++Second)
			{
				if (Second == First || (Rects[Second].bText && Second < First)) continue;
				if (IsDeliberateStack(Targets.ParentOf, Rects[First].Widget, Rects[Second].Widget)) continue;
				++PairsChecked;
				const double OverlapX = FMath::Min(Rects[First].Start.X + Rects[First].Size.X, Rects[Second].Start.X + Rects[Second].Size.X)
					- FMath::Max(Rects[First].Start.X, Rects[Second].Start.X);
				const double OverlapY = FMath::Min(Rects[First].Start.Y + Rects[First].Size.Y, Rects[Second].Start.Y + Rects[Second].Size.Y)
					- FMath::Max(Rects[First].Start.Y, Rects[Second].Start.Y);
				if (OverlapX <= 1.0 || OverlapY <= 1.0) continue;
				const FString Pair = FString::Printf(TEXT("%s ink x %s%s overlap %.1fx%.1f"), *Rects[First].Widget->GetName(),
					*Rects[Second].Widget->GetName(), Rects[Second].bText ? TEXT(" ink") : TEXT(" face"), OverlapX, OverlapY);
				// 장비 드로어 인스펙터(Inspector*)는 Phase 2-A 소유다 — 우리 창 안이지만 이 라운드의 게이트가 아니다.
				// 정보성 체크로 갈라 detail 에는 남긴다(소유 세션 전달용).
				const bool bDrawerInspector = Rects[First].Widget->GetName().StartsWith(TEXT("Inspector"))
					|| Rects[Second].Widget->GetName().StartsWith(TEXT("Inspector"));
				if (bDrawerInspector) { if (DrawerOverlapFailures.Num() < 8) DrawerOverlapFailures.Add(Pair); }
				else if (OverlapFailures.Num() < 8) OverlapFailures.Add(Pair);
			}
		}
		Check(FString::Printf(TEXT("%s_%d_v2_no_text_overlap"), *CurrentScenario, CurrentWidth), OverlapFailures.IsEmpty(),
			FString::Printf(TEXT("%d text-vs-(text|face) pairs from %d rects (ellipsis text is compared as its allocated box; Inspector* pairs go to v2_no_text_overlap_equipment_drawer); %s"),
				PairsChecked, Rects.Num(), OverlapFailures.IsEmpty() ? TEXT("no intersection") : *FString::Join(OverlapFailures, TEXT(" | "))));
		// 게이트가 아니라 기록이다 — 소유가 다른 판(장비 인스펙터)의 겹침을 이 라운드가 대신 고치지 않는다
		Check(FString::Printf(TEXT("%s_%d_v2_no_text_overlap_equipment_drawer"), *CurrentScenario, CurrentWidth), true,
			DrawerOverlapFailures.IsEmpty()
				? FString(TEXT("informational (Phase 2-A owns the equipment inspector); no Inspector* intersection"))
				: FString::Printf(TEXT("informational (Phase 2-A owns the equipment inspector - hand to hwan-a3): %s"),
					*FString::Join(DrawerOverlapFailures, TEXT(" | "))));

		UEmployeeWindowWidget* Window = EmbeddedWindow.Get();
		if (!Window) return;
		// 성장 카드의 내부 배치는 전용 바인딩·CTA 검사에서 확인한다.
		// v3.2 세로 직원 카드 · 로스터 전 화면비 2열 — 레일 판은 세로 TileView 를 16 인셋으로 감싸고, 히어로 판 내용은 HeroVBox.
		// 잠재 판은 growrow 를 FILL 하고 내용은 상단 정렬이라 PotFooter 하단 여백이 고정 30 이 아니다 → 하단 검사 해제(-1).
		static const FPlatePaddingSpec PlateSpecs[] = {
			{TEXT("RosterPlate"), TEXT("RosterListView"), 16.0, 16.0, 16.0, 16.0},
			{TEXT("HeroPlate"), TEXT("HeroVBox"), 24.0, 24.0, 24.0, 24.0},
			{TEXT("PotentialPanel"), TEXT("TargetRowSize"), 30.0, 24.0, 30.0, -1.0},
			{TEXT("PotentialPanel"), TEXT("PotLine0"), 30.0, -1.0, 30.0, -1.0},
			{TEXT("PotentialPanel"), TEXT("CardRowSize"), 30.0, -1.0, 30.0, -1.0},
			{TEXT("PotentialPanel"), TEXT("OddsWellSize"), 30.0, -1.0, 30.0, -1.0},
			{TEXT("PotentialPanel"), TEXT("ResetButtonSize"), 30.0, -1.0, 30.0, 24.0}};
		bool bPadding = true;
		FString PaddingDetail;
		for (const FPlatePaddingSpec& Spec : PlateSpecs)
		{
			const UWidget* PlateWidget = Window->GetWidgetFromName(Spec.PlateName);
			const UWidget* ContentWidget = Window->GetWidgetFromName(Spec.ContentName);
			if (!ContentWidget && Window->DisciplineCard) ContentWidget = Window->DisciplineCard->GetWidgetFromName(Spec.ContentName);
			if (!ContentWidget && Window->PotentialWorkspace) ContentWidget = Window->PotentialWorkspace->GetWidgetFromName(Spec.ContentName);
			FVector2D PlateStart, PlateSize, ContentStart, ContentSize;
			const bool bRects = LocalRectOf(Window->GetCachedGeometry(), PlateWidget, PlateStart, PlateSize)
				&& LocalRectOf(Window->GetCachedGeometry(), ContentWidget, ContentStart, ContentSize);
			if (!bRects)
			{
				bPadding = false;
				PaddingDetail += FString::Printf(TEXT("%s/%s missing; "), Spec.PlateName, Spec.ContentName);
				continue;
			}
			const double Measured[4] = {ContentStart.X - PlateStart.X, ContentStart.Y - PlateStart.Y,
				PlateStart.X + PlateSize.X - (ContentStart.X + ContentSize.X),
				PlateStart.Y + PlateSize.Y - (ContentStart.Y + ContentSize.Y)};
			const double Expected[4] = {Spec.Left, Spec.Top, Spec.Right, Spec.Bottom};
			PaddingDetail += FString::Printf(TEXT("%s/%s L%.1f T%.1f R%.1f B%.1f; "), Spec.PlateName, Spec.ContentName,
				Measured[0], Measured[1], Measured[2], Measured[3]);
			for (int32 Side = 0; Side < 4; ++Side)
				if (Expected[Side] >= 0.0) bPadding = bPadding && FMath::Abs(Measured[Side] - Expected[Side]) <= 1.0;
		}
		Check(FString::Printf(TEXT("%s_%d_v2_plate_padding_24"), *CurrentScenario, CurrentWidth), bPadding, PaddingDetail);
	}

	// 띠 게이트 — 빈자리 예산·스크롤바·Lv 배지·빈자리 잉크. 드로어가 열려도 띠는 보이므로 전 직원창 시나리오에서 돌린다.
	void CheckRosterStrip(UEmployeeWindowWidget* Window, const FGeometry& RootGeometry)
	{
		// v2.3 ⑫ — 빈자리 타일은 띠의 가시 폭을 채운다(구 상한 3 은 띠 절반을 죽은 여백으로 남겼다).
		// 죽은 영역이 타일 한 장(피치 153)을 넘으면 실패. 정원이 먼저 바닥나면 그건 정상이라 통과시킨다.
		int32 HireTiles = 0, EntryTiles = 0;
		double StripWidth = 0.0, StripHeight = 0.0;
		if (const UListView* HireRoster = Window->RosterListView)
		{
			const FVector2D StripSize = HireRoster->GetCachedGeometry().GetLocalSize();
			StripWidth = StripSize.X;
			StripHeight = StripSize.Y;
			for (const UUserWidget* Entry : HireRoster->GetDisplayedEntryWidgets())
			{
				const UEmployeeRosterCardWidget* Tile = Cast<UEmployeeRosterCardWidget>(Entry);
				HireTiles += (Tile && Tile->IsHireSlot()) ? 1 : 0;
				++EntryTiles;
			}
		}
		// v3.1 세로 2열 격자 — 생산(EmployeeWindowWidget.VisibleRosterTileBudget)과 같은 식: 열 × (보이는 행 + 스크롤 버퍼)
		const double TileW = 150.0, TileH = 200.0, TileGap = 12.0;
		const int32 StripColumns = StripWidth >= TileW ? FMath::Max(1, FMath::FloorToInt32((StripWidth + TileGap) / (TileW + TileGap))) : 0;
		const int32 StripVisRows = StripHeight >= TileH ? FMath::FloorToInt32((StripHeight + TileGap) / (TileH + TileGap)) : 1;
		const int32 StripBudget = StripColumns > 0 ? StripColumns * FMath::Max(1, StripVisRows + 8) : 0;
		const int32 StripItems = Window->RosterListView ? Window->RosterListView->GetNumItems() : 0;
		// 표시 엔트리는 가상화로 잘린다 — 예산 판정은 목록 아이템(빈자리 플래그)으로 센다
		int32 HireItems = 0;
		if (Window->RosterListView) for (const UObject* Item : Window->RosterListView->GetListItems())
		{
			const UEntityCardData* CardData = Cast<UEntityCardData>(Item);
			HireItems += (CardData && CardData->bIsHireSlot) ? 1 : 0;
		}
		int32 OwnedEmployees = 0, SeatCapacity = 0, RemainingSeats = 0;
		if (Window->EmployeeManager)
		{
			OwnedEmployees = Window->EmployeeManager->GetEmployeesInBuilding(Window->BuildingIndex).Num()
				+ Window->EmployeeManager->GetUnassignedEmployeesInBuilding(Window->BuildingIndex).Num();
			SeatCapacity = Window->EmployeeManager->GetBuildingEmployeeCapacity(Window->BuildingIndex, /*bLogIfZero*/ false);
			RemainingSeats = FEmployeeRosterRules::HireSlotCount(OwnedEmployees, SeatCapacity);
		}
		// 탈출구는 "빈자리를 못 만들었다" 가 아니라 "만들 자리가 없다" 여야 한다(R6).
		// 판정은 개수를 **정확히** 맞췄는가다: hire == clamp(budget − employees, 0, remaining).
		// 직원만으로 예산을 넘는 것은 정상이다(가로 드래그 스크롤) — 금지는 **빈자리**가 예산과 어긋나는 것이다.
		// 판정 기준은 **렌더된 띠 폭**이다 — 캡처 직전 `ResyncRosterForCaptureSize()` 가 생산의 재계산을
		// 그 폭으로 돌리고 다시 그렸으므로, 생산 기록 폭/예산과 하네스 폭/예산은 같아야 한다(detail 에 병기).
		const int32 EmployeeItems = StripItems - HireItems;
		const int32 ExpectedHire = StripBudget > 0
			? FMath::Clamp(StripBudget - EmployeeItems, 0, FMath::Max(RemainingSeats, 0)) : HireItems;
		const bool bExactFill = StripBudget <= 0 || (HireItems == ExpectedHire && StripItems == EmployeeItems + HireItems);
		const bool bNoOverrun = StripBudget == 0 || StripItems <= FMath::Max(StripBudget, EmployeeItems);
		Check(FString::Printf(TEXT("%s_%d_v21_hire_tiles_fill_strip"), *CurrentScenario, CurrentWidth), bExactFill,
			FString::Printf(TEXT("%d items (%d hire items, %d employee items, %d hire tiles shown, %d shown); harness strip %.0fpx = budget %d; production measured %.0fpx = budget %d (wanted %d); want hire %d; employees %d / capacity %d, remaining seats %d; dead %.0fpx; fits rendered width=%d"),
				StripItems, HireItems, EmployeeItems, HireTiles, EntryTiles, StripWidth, StripBudget,
				GEmployeeRosterSyncWidth, GEmployeeRosterSyncBudget, GEmployeeRosterSyncHireWanted, ExpectedHire,
				OwnedEmployees, SeatCapacity, RemainingSeats,
				FMath::Max(StripWidth - (StripItems * 153.0 - 12.0), 0.0), bNoOverrun ? 1 : 0));

		// N2 — 타일이 예산을 넘기면 TileView 가 가로 스크롤바를 띄우고 그 바가 이름 밴드를 가로로 자른다.
		// 저작 두께 0 만으로는 트랙이 남는다 — C++ NativeConstruct 가 SetScrollbarVisibility(Collapsed) 로 끈다.
		// 런타임 전용 축이라 저작으로는 못 박지만 **읽을 수는 있다** — STableViewBase 가 들고 있는 SScrollBar 를
		// 슬레이트 자식 트리에서 찾아 가시성을 직접 본다(GetScrollbarVisibility 는 needed 여부라 쓰면 안 된다).
		const UListView* Strip = Window->RosterListView;
		const TSharedPtr<SWidget> ScrollBarSlate = FindSlateChildOfType(Strip ? Strip->GetCachedWidget() : TSharedPtr<SWidget>(),
			FName(TEXT("SScrollBar")));
		const bool bScrollBarCollapsed = ScrollBarSlate.IsValid() && ScrollBarSlate->GetVisibility() == EVisibility::Collapsed;
		Check(FString::Printf(TEXT("%s_%d_v2_roster_scrollbar_hidden"), *CurrentScenario, CurrentWidth),
			bScrollBarCollapsed,
			FString::Printf(TEXT("strip is a %s; SScrollBar %s, visibility %s (want Collapsed); items %d vs rendered budget %d (production %.0fpx/%d)"),
				Strip ? *Strip->GetClass()->GetName() : TEXT("<null>"),
				ScrollBarSlate.IsValid() ? TEXT("found") : TEXT("MISSING in the strip's slate tree"),
				ScrollBarSlate.IsValid() ? *ScrollBarSlate->GetVisibility().ToString() : TEXT("-"),
				StripItems, StripBudget, GEmployeeRosterSyncWidth, GEmployeeRosterSyncBudget));

		// ⑨ Lv 배지 — 알약 판(LvBadgeBG)이 상자를 채우고 타일 안에 있어야 한다.
		// 18차의 46.7x83.4 세로 슬리버는 살아남은 45도 RenderTransform 이었다(92x26 을 45도 돌리면 그 값이 나온다).
		int32 BadgesChecked = 0;
		TArray<FString> BadgeFailures;
		FString BadgeDetail;
		if (Strip) for (const UUserWidget* Entry : Strip->GetDisplayedEntryWidgets())
		{
			const UEmployeeRosterCardWidget* Tile = Cast<UEmployeeRosterCardWidget>(Entry);
			if (!Tile || Tile->IsHireSlot()) continue;
			const UWidget* BadgeFace = Entry->GetWidgetFromName(TEXT("LvBadgeBG"));
			const UWidget* BadgeBox = Entry->GetWidgetFromName(TEXT("LvBadgeBox"));
			const UWidget* TileFace = Entry->GetWidgetFromName(TEXT("RowSizeBox"));
			FVector2D FaceStart, FaceSize, BoxStart, BoxSize, TileStart, TileSize;
			if (!LocalRectOf(RootGeometry, BadgeFace, FaceStart, FaceSize)
				|| !LocalRectOf(RootGeometry, BadgeBox, BoxStart, BoxSize)
				|| !LocalRectOf(RootGeometry, TileFace, TileStart, TileSize)) continue;
			++BadgesChecked;
			const bool bFillsBox = FMath::Abs(FaceSize.X - BoxSize.X) <= 2.0 && FMath::Abs(FaceSize.Y - BoxSize.Y) <= 2.0;
			const bool bPillShape = FMath::Abs(FaceSize.Y - 32.0) <= 2.0
				&& FMath::Abs(FaceSize.X - 80.0) <= 2.0;
			const bool bInsideTile = FaceStart.X >= TileStart.X - 2.0 && FaceStart.Y >= TileStart.Y - 2.0
				&& FaceStart.X + FaceSize.X <= TileStart.X + TileSize.X + 2.0
				&& FaceStart.Y + FaceSize.Y <= TileStart.Y + TileSize.Y + 2.0;
			// N8 — 판만 보고 안의 숫자를 안 봐서 "베이스라인이 알약 밖 3px" 이 통과했다. 줄높이 해킹(LHP<1)이
			// 라인박스만 줄이고 잉크를 아래로 미는 축이라 그 값을 단언한다(잉크 bbox 자체는 검수자 PNG).
			const UTextBlock* LevelLabel = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("LevelText")));
			const float LevelLine = LevelLabel ? LineHeightPercentageOf(LevelLabel) : 1.f;
			const bool bLevelLine = FMath::IsNearlyEqual(LevelLine, 1.f, 0.01f);
			FVector2D LevelStart, LevelSize;
			const bool bLevelRect = LocalRectOf(RootGeometry, LevelLabel, LevelStart, LevelSize);
			// 세로 중앙 — 클램프된 라인박스가 알약 중심에서 벗어나면 숫자가 한쪽으로 쏠린다
			const bool bLevelCentered = !bLevelRect
				|| FMath::Abs((LevelStart.Y + LevelSize.Y * 0.5) - (FaceStart.Y + FaceSize.Y * 0.5)) <= 2.0;
			if (BadgeDetail.Len() < 360)
				BadgeDetail += FString::Printf(TEXT("BG %.1fx%.1f @%.1f,%.1f box %.1fx%.1f tile %.1f,%.1f %.1fx%.1f text %.1fx%.1f @%.1f line %.2f; "),
					FaceSize.X, FaceSize.Y, FaceStart.X, FaceStart.Y, BoxSize.X, BoxSize.Y, TileStart.X, TileStart.Y, TileSize.X, TileSize.Y,
					bLevelRect ? LevelSize.X : -1.0, bLevelRect ? LevelSize.Y : -1.0, bLevelRect ? LevelStart.Y : -1.0, LevelLine);
			if ((!bFillsBox || !bPillShape || !bInsideTile || !bLevelLine || !bLevelCentered) && BadgeFailures.Num() < 4)
				BadgeFailures.Add(FString::Printf(TEXT("%s fills_box=%d pill=%d inside=%d line1.0=%d centered=%d"),
					*BadgeFace->GetName(), bFillsBox ? 1 : 0, bPillShape ? 1 : 0, bInsideTile ? 1 : 0,
					bLevelLine ? 1 : 0, bLevelCentered ? 1 : 0));
		}
		Check(FString::Printf(TEXT("%s_%d_v2_lv_badge_geometry"), *CurrentScenario, CurrentWidth),
			BadgeFailures.IsEmpty(),
			FString::Printf(TEXT("%d badges measured (want 80x32 +-2, filling LvBadgeBox, inside the tile, LevelText LineHeightPercentage 1.0 and vertically centred in the pill); %s%s"),
				BadgesChecked, *BadgeDetail, BadgeFailures.IsEmpty() ? TEXT("all pills") : *FString::Join(BadgeFailures, TEXT(" | "))));

		// #1 — 로스터 타일 이름. 같은 창의 좌정렬 `HeroNameText` 는 `최장…` 으로 접히는데 이 이름만 양끝이
		// 반쪽으로 잘렸다(alloc 121 / 잉크 125, 말줄임 글리프 0). 원인은 가운데 정렬이라 축을 그대로 단언한다:
		// 희망이 배정 안이거나(안 넘침), 넘친다면 말줄임이 **한쪽으로만** 접히는 구성(Ellipsis + 좌정렬)이어야 한다.
		int32 NamesChecked = 0;
		TArray<FString> NameEllipsisFailures;
		FString NameEllipsisDetail;
		if (Strip) for (const UUserWidget* Entry : Strip->GetDisplayedEntryWidgets())
		{
			const UEmployeeRosterCardWidget* Tile = Cast<UEmployeeRosterCardWidget>(Entry);
			if (!Tile || Tile->IsHireSlot()) continue;
			const UTextBlock* NameLabel = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("NameText")));
			if (!NameLabel || NameLabel->GetText().IsEmpty()) continue;
			const double Alloc = NameLabel->GetCachedGeometry().GetLocalSize().X;
			const double Desired = NameLabel->GetDesiredSize().X;
			if (Alloc < 1.0) continue;
			++NamesChecked;
			const bool bFolds = EllipsisFoldsOnOneSide(NameLabel);
			const double AllocHeight = NameLabel->GetCachedGeometry().GetLocalSize().Y;
			const double GlyphHeight = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()
				->Measure(NameLabel->GetText().ToString(), NameLabel->GetFont()).Y;
			const bool bVerticalFit = AllocHeight + 1.0 >= GlyphHeight
				&& FMath::IsNearlyEqual(LineHeightPercentageOf(NameLabel), 1.f, 0.01f);
			const bool bOk = (Desired <= Alloc + 1.0 || bFolds) && bVerticalFit;
			if (!bOk && NameEllipsisFailures.Num() < 4)
				NameEllipsisFailures.Add(FString::Printf(TEXT("[%s] desired %.1f alloc %.1f justification %d ellipsis=%d height %.1f glyph %.1f line %.2f"),
					*NameLabel->GetText().ToString().Left(12), Desired, Alloc,
					static_cast<int32>(JustificationOf(NameLabel)), EllipsisActuallyFires(NameLabel) ? 1 : 0,
					AllocHeight, GlyphHeight, LineHeightPercentageOf(NameLabel)));
			if (NameEllipsisDetail.Len() < 260)
				NameEllipsisDetail += FString::Printf(TEXT("[%s] alloc %.0f/desired %.0f just %d folds %d; "),
					*NameLabel->GetText().ToString().Left(6), Alloc, Desired,
					static_cast<int32>(JustificationOf(NameLabel)), bFolds ? 1 : 0);
		}
		Check(FString::Printf(TEXT("%s_%d_v2_roster_name_ellipsis"), *CurrentScenario, CurrentWidth),
			NameEllipsisFailures.IsEmpty(),
			FString::Printf(TEXT("%d tile names measured (desired <= alloc, or Ellipsis + Justification Left so the fold happens on one side only - the ellipsis glyph itself is the reviewer's PNG); %s%s"),
				NamesChecked, *NameEllipsisDetail,
				NameEllipsisFailures.IsEmpty() ? TEXT("all fold cleanly") : *FString::Join(NameEllipsisFailures, TEXT(" | "))));

		// ㉔ 빈자리 잉크는 저작 뮤트 그대로여야 한다 — C++ SetHireSlot 이 런타임 틴트로 덮으면 여기서 갈린다.
		// 대비비는 검수자가 PNG 로 본다(펄스 RenderOpacity 가 잉크를 배경 쪽으로 끌어내리므로 그 값도 남긴다).
		// 잉크는 저작 뮤트 #A7B1BE 풀알파여야 한다 — 알파만 보면 C++ 가 어두운 색으로 덮어써도 통과한다(T7).
		const FLinearColor HireInkExpected = FLinearColor::FromSRGBColor(FColor(0xA7, 0xB1, 0xBE));
		FString HireInkDetail;
		bool bHireInk = true;
		int32 HireTilesInspected = 0;
		if (Strip) for (const UUserWidget* Entry : Strip->GetDisplayedEntryWidgets())
		{
			const UEmployeeRosterCardWidget* Tile = Cast<UEmployeeRosterCardWidget>(Entry);
			if (!Tile || !Tile->IsHireSlot()) continue;
			++HireTilesInspected;
			const UWidget* HireLayerWidget = Entry->GetWidgetFromName(TEXT("HireLayer"));
			for (const TCHAR* InkName : {TEXT("HirePlus"), TEXT("HireText")})
			{
				const UTextBlock* InkLabel = Cast<UTextBlock>(Entry->GetWidgetFromName(InkName));
				if (!InkLabel) { bHireInk = false; HireInkDetail += FString::Printf(TEXT("%s missing; "), InkName); continue; }
				const FLinearColor Ink = InkLabel->GetColorAndOpacity().GetSpecifiedColor();
				const bool bMatches = FMath::Abs(Ink.R - HireInkExpected.R) <= 0.02f && FMath::Abs(Ink.G - HireInkExpected.G) <= 0.02f
					&& FMath::Abs(Ink.B - HireInkExpected.B) <= 0.02f && Ink.A >= 0.9f;
				bHireInk = bHireInk && bMatches;
				if (!bMatches && HireInkDetail.Len() < 320)
					HireInkDetail += FString::Printf(TEXT("%s OVERRIDDEN rgba %.3f,%.3f,%.3f,%.3f; "), InkName, Ink.R, Ink.G, Ink.B, Ink.A);
				else if (HireTilesInspected == 1 && HireInkDetail.Len() < 260)
					HireInkDetail += FString::Printf(TEXT("%s rgba %.3f,%.3f,%.3f,%.3f; "), InkName, Ink.R, Ink.G, Ink.B, Ink.A);
			}
			if (HireLayerWidget && HireTilesInspected == 1 && HireInkDetail.Len() < 300)
				HireInkDetail += FString::Printf(TEXT("HireLayer render opacity %.2f; "), HireLayerWidget->GetRenderOpacity());
		}
		if (HireTilesInspected > 0)
			HireInkDetail += FString::Printf(TEXT("%d hire tiles inspected (want #A7B1BE +-0.02 linear %.3f,%.3f,%.3f, alpha >= 0.9)"),
				HireTilesInspected, HireInkExpected.R, HireInkExpected.G, HireInkExpected.B);
		Check(FString::Printf(TEXT("%s_%d_v2_hire_ink_not_overridden"), *CurrentScenario, CurrentWidth), bHireInk,
			HireInkDetail.IsEmpty() ? TEXT("no hire tile on this strip") : *HireInkDetail);

		// ㉔ 잉크 속성은 정확한데 화면 대비비가 3.26 이었다 — `HireLayer` 의 펄스 RenderOpacity 0.57 이
		// 잉크를 타일 면 쪽으로 끌어내렸기 때문이다(0.57x167 + 0.43x31 = 108.5 = 실측 109).
		// 색 단언만으로는 영영 못 잡는 축이라 합성 계수 자체에 하한을 건다(펄스 골 0.82, 불가능 상태 1.0).
		double LowestPulse = -1.0;
		int32 PulseLayersMeasured = 0;
		FString PulseDetail;
		if (Strip) for (const UUserWidget* Entry : Strip->GetDisplayedEntryWidgets())
		{
			const UEmployeeRosterCardWidget* Tile = Cast<UEmployeeRosterCardWidget>(Entry);
			if (!Tile || !Tile->IsHireSlot()) continue;
			const UWidget* HireLayerWidget = Entry->GetWidgetFromName(TEXT("HireLayer"));
			if (!HireLayerWidget) continue;
			const double Opacity = HireLayerWidget->GetRenderOpacity();
			LowestPulse = PulseLayersMeasured == 0 ? Opacity : FMath::Min(LowestPulse, Opacity);
			++PulseLayersMeasured;
			if (PulseDetail.Len() < 200) PulseDetail += FString::Printf(TEXT("%.2f "), Opacity);
		}
		Check(FString::Printf(TEXT("%s_%d_v2_hire_pulse_floor"), *CurrentScenario, CurrentWidth),
			PulseLayersMeasured == 0 || LowestPulse >= 0.8,
			PulseLayersMeasured == 0 ? TEXT("no hire tile on this strip")
				: *FString::Printf(TEXT("%d HireLayer opacities [%s] lowest %.2f (want >= 0.80 so the muted ink keeps CR 4.5 on the tile face at the pulse trough)"),
					PulseLayersMeasured, *PulseDetail.TrimEnd(), LowestPulse));
	}

	// 직원 열 게이트 — 이름 행 예산(②)·값 열 정렬(⑮)·연출 좌표(⑭/N6)·잠재 알림 마크(N7).
	void CheckHeroColumn(UEmployeeWindowWidget* Window, const FGeometry& RootGeometry)
	{
		const FString Missing = TEXT("widget not authored yet");
		// ② v3.2 이름 블록 2줄 — 위 NameRow(이름 상한 300 + 스페이서 + 해고 89), 아래 TagRow(직급·부서·등급 칩 상한 160).
		FVector2D ContentStart, ContentSize, NameRowStart, NameRowSize, TagRowStart, TagRowSize;
		FVector2D FireStart, FireSize, SpacerStart, SpacerSize, NameStart, NameSize;
		// v3.2 세로 직원 카드 — 이름 블록이 자리를 다투는 열은 카드 세로 스택(HeroVBox)이다
		const bool bContent = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("HeroVBox")), ContentStart, ContentSize);
		const bool bNameRowRect = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("NameRow")), NameRowStart, NameRowSize);
		const bool bTagRowRect = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("TagRow")), TagRowStart, TagRowSize);
		const bool bFire = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("FireBtnSize")), FireStart, FireSize);
		const bool bSpacer = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("NameRowSpacer")), SpacerStart, SpacerSize);
		const UTextBlock* NameLabel = Cast<UTextBlock>(Window->GetWidgetFromName(TEXT("HeroNameText")));
		const bool bName = LocalRectOf(RootGeometry, NameLabel, NameStart, NameSize);
		const double ContentRight = ContentStart.X + ContentSize.X;
		FVector2D ChipStart[3] = {FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector};
		FVector2D ChipSize[3] = {FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector};
		bool bChips = true;
		FString ChipDetail;
		int32 ChipCount = 0;
		for (const TCHAR* ChipName : {TEXT("RankChipSize"), TEXT("DeptChipSize"), TEXT("HeroRarityChipSize")})
		{
			if (LocalRectOf(RootGeometry, Window->GetWidgetFromName(ChipName), ChipStart[ChipCount], ChipSize[ChipCount]))
				ChipDetail += FString::Printf(TEXT("%s %.1f@%.1f; "), ChipName, ChipSize[ChipCount].X, ChipStart[ChipCount].X);
			else { bChips = false; ChipDetail += FString::Printf(TEXT("%s missing; "), ChipName); }
			++ChipCount;
		}
		bool bChipsFit = bChips && bContent;
		double MinChipGap = -1.0;
		if (bChips)
		{
			// 저작 순서를 믿지 않고 실제 좌표로 줄세워 이웃 간격을 잰다
			TArray<int32> Order = {0, 1, 2};
			Order.Sort([&ChipStart](const int32& A, const int32& B) { return ChipStart[A].X < ChipStart[B].X; });
			MinChipGap = FMath::Min(ChipStart[Order[1]].X - (ChipStart[Order[0]].X + ChipSize[Order[0]].X),
				ChipStart[Order[2]].X - (ChipStart[Order[1]].X + ChipSize[Order[1]].X));
			for (int32 Index = 0; Index < 3; ++Index)
				bChipsFit = bChipsFit && ChipSize[Index].X <= 162.0 && ChipStart[Index].X >= ContentStart.X - 0.5
					&& ChipStart[Index].X + ChipSize[Index].X <= ContentRight + 0.5;
			bChipsFit = bChipsFit && MinChipGap >= 8.0;
		}
		// T8 — 잉크는 말줄임이 걸리면 배정으로 접혀 항진명제가 된다. 비클램프 희망 폭으로 본다.
		const double NameAlloc = bName && NameLabel ? NameLabel->GetCachedGeometry().GetLocalSize().X : -1.0;
		const double NameDesired = bName && NameLabel ? NameLabel->GetDesiredSize().X : -1.0;
		const bool bNameTruncatesCleanly = bName && NameLabel
			&& (NameDesired <= NameAlloc + 1.0 || EllipsisActuallyFires(NameLabel));
		const bool bNameBudget = bNameTruncatesCleanly && NameAlloc <= 302.0 && NameAlloc >= 120.0;
		const bool bFireFits = bFire && bContent && FMath::Abs(FireSize.X - 89.0) <= 2.0
			&& FireStart.X + FireSize.X <= ContentRight + 0.5;
		// 두 줄로 쪼갠 목적 자체 — 칩 줄은 이름/해고 줄 아래에 서야 한다
		const bool bRowsStacked = bNameRowRect && bTagRowRect && TagRowStart.Y >= NameRowStart.Y + NameRowSize.Y - 0.5;
		const bool bNameRow = bFireFits && bChipsFit && bNameBudget && bRowsStacked;
		Check(FString::Printf(TEXT("%s_%d_v2_hero_name_row_budget"), *CurrentScenario, CurrentWidth), bNameRow,
			bContent && bFire && bName && bChips && bNameRowRect && bTagRowRect
				? FString::Printf(TEXT("content right %.1f; FireBtnSize %.1f wide (want 89), right %.1f; chips %smin gap %.1f (want >= 8, each <= 160); NameRowSpacer %.1f; name desired(unclamped) %.1f / alloc %.1f (want 120~300, ellipsis configured=%d); NameRow bottom %.1f -> TagRow top %.1f (%.1f tall)"),
					ContentRight, FireSize.X, FireStart.X + FireSize.X, *ChipDetail, MinChipGap,
					bSpacer ? SpacerSize.X : -1.0, NameDesired, NameAlloc,
					NameLabel && EllipsisActuallyFires(NameLabel) ? 1 : 0,
					NameRowStart.Y + NameRowSize.Y, TagRowStart.Y, TagRowSize.Y)
				: Missing);

		// 능력치 값 열의 우단을 하나로 정렬한다.
		double ValueMin = 0.0, ValueMax = 0.0;
		int32 ValuesMeasured = 0;
		FString ColumnDetail;
		for (int32 Index = 0; Index < static_cast<int32>(EEmployeeStatIndex::Count); ++Index)
		{
			FVector2D ValueStart, ValueSize;
			if (!LocalRectOf(RootGeometry, Window->GetWidgetFromName(*FString::Printf(TEXT("HeroStatValue%d"), Index)), ValueStart, ValueSize)) continue;
			const double Right = ValueStart.X + ValueSize.X;
			ValueMin = ValuesMeasured == 0 ? Right : FMath::Min(ValueMin, Right);
			ValueMax = ValuesMeasured == 0 ? Right : FMath::Max(ValueMax, Right);
			++ValuesMeasured;
			ColumnDetail += FString::Printf(TEXT("%d:%.1f "), Index, Right);
		}
		const double ValueSpread = ValuesMeasured > 0 ? ValueMax - ValueMin : 0.0;
		Check(FString::Printf(TEXT("%s_%d_v2_stat_value_column_aligned"), *CurrentScenario, CurrentWidth),
			ValuesMeasured == static_cast<int32>(EEmployeeStatIndex::Count) && ValueSpread <= 2.0,
			FString::Printf(TEXT("%d value rows, right edges %s spread %.1f (want <= 2)"), ValuesMeasured, *ColumnDetail, ValueSpread));
		Check(FString::Printf(TEXT("%s_%d_six_basic_stat_rows"), *CurrentScenario, CurrentWidth),
			Window->GetWidgetFromName(TEXT("HeroStatValue5")) != nullptr
				&& Window->GetWidgetFromName(TEXT("HeroStatValue6")) == nullptr);

		// ⑭/N6 연출 좌표 — 스윕이 꺼져 있어도 마지막 트랜슬레이션이 남으면 버튼 밖(캔버스 밖)에 유령 사각형이 선다.
		bool bShines = true;
		FString ShineDetail;
		const double RootRight = RootGeometry.GetLocalSize().X;
		for (const TCHAR* ShineName : {TEXT("EnhanceShine")})
		{
			const UWidget* Shine = Window->GetWidgetFromName(ShineName);
			// 호스트는 생산(DriveShineSweep)이 클램프 기준으로 삼는 조상과 **같은 규칙**으로 고른다:
			// 샤인보다 넓은 첫 조상. 조부모 고정으로 재면 클램프 기준과 판정 기준이 다른 컨테이너가 된다.
			const double ShineWidth = Shine ? FMath::Max(Shine->GetCachedGeometry().GetLocalSize().X, 1.0) : 1.0;
			const UWidget* Host = nullptr;
			for (const UWidget* Ancestor = Shine ? Shine->GetParent() : nullptr; Ancestor; Ancestor = Ancestor->GetParent())
			{
				if (Ancestor->GetCachedGeometry().GetLocalSize().X > ShineWidth) { Host = Ancestor; break; }
			}
			FVector2D ShineStart, ShineSize, HostStart, HostSize;
			if (!LocalRectOf(RootGeometry, Shine, ShineStart, ShineSize) || !LocalRectOf(RootGeometry, Host, HostStart, HostSize))
			{
				bShines = false;
				ShineDetail += FString::Printf(TEXT("%s missing; "), ShineName);
				continue;
			}
			// 좌측 진입(−스윕폭)은 구현이 의도적으로 남긴 모션이고 호스트가 클립하므로 보이지 않는다.
			// 그래서 판정은 ① 호스트가 정말 클립하는가 ② 우변이 호스트 안인가 ③ 우변이 캔버스 안인가 셋이다.
			const bool bHostClips = Host->GetClipping() == EWidgetClipping::ClipToBounds;
			const double ShineRight = ShineStart.X + ShineSize.X;
			const bool bInside = bHostClips
				&& ShineStart.X >= HostStart.X - ShineSize.X - 1.0
				&& ShineRight <= HostStart.X + HostSize.X + 1.0 && ShineRight <= RootRight + 1.0
				&& ShineStart.Y >= HostStart.Y - 1.0 && ShineStart.Y + ShineSize.Y <= HostStart.Y + HostSize.Y + 1.0;
			bShines = bShines && bInside;
			ShineDetail += FString::Printf(TEXT("%s %.1f..%.1f in %s %.1f..%.1f clip=%d (opacity %.2f); "), ShineName,
				ShineStart.X, ShineRight, *Host->GetName(), HostStart.X, HostStart.X + HostSize.X,
				bHostClips ? 1 : 0, Shine->GetRenderOpacity());
		}
		Check(FString::Printf(TEXT("%s_%d_v2_shine_inside_container"), *CurrentScenario, CurrentWidth), bShines, ShineDetail);

		const UPotentialResetWorkspaceWidget* Potential = Window->PotentialWorkspace;
		const EItemType RerollCard = Potential ? Potential->GetSelectedCard() : EItemType::BusinessCardPaper;
		const UItemInventoryManager* Items = GetItemInventory();
		const int32 RerollCardCount = Items ? Items->GetItemCount(RerollCard) : 0;
		// 0장이면 비활성이 아니라 구매로 바뀐다 — 버튼 활성은 직원 유무만 따르고, 모드는 소비 칩/상점 아이콘이 말한다
		const bool bEmployee = Window->GetSelectedEmployee() != nullptr;
		const UButtonWidget* ResetButton = Potential ? Cast<UButtonWidget>(Potential->GetWidgetFromName(TEXT("ResetButton"))) : nullptr;
		const UWidget* CostChip = Potential ? Potential->GetWidgetFromName(TEXT("ResetCostChip")) : nullptr;
		const UWidget* ShopIcon = Potential ? Potential->GetWidgetFromName(TEXT("ResetShopIcon")) : nullptr;
		const bool bBuyMode = bEmployee && RerollCardCount <= 0;
		Check(FString::Printf(TEXT("%s_%d_inline_potential_reset_mode"), *CurrentScenario, CurrentWidth),
			ResetButton && ResetButton->GetIsEnabled() == bEmployee && CostChip && ShopIcon
			&& CostChip->IsVisible() == (bEmployee && !bBuyMode) && ShopIcon->IsVisible() == bBuyMode,
			FString::Printf(TEXT("selected card %d count %d, employee %d, buy mode %d"), static_cast<int32>(RerollCard), RerollCardCount,
				bEmployee ? 1 : 0, bBuyMode ? 1 : 0));

	}

	void CheckStatAllocation(UEmployeeWindowWidget* Window, const FGeometry& RootGeometry)
	{
		if (!Window || Window->CurrentEmployeePage != EEmployeeWindowPage::Home) return;
		const FEmployeeInstance* Employee = Window->GetSelectedEmployee();
		const UCommonTextBlock* Available = Cast<UCommonTextBlock>(Window->GetWidgetFromName(TEXT("AvailableStatPointsText")));
		const UImage* AvailableIcon = Cast<UImage>(Window->GetWidgetFromName(TEXT("AvailableSPIcon")));
		const bool bBalance = Employee && Window->EmployeeManager && Available && AvailableIcon
			&& AvailableIcon->IsVisible() && AvailableIcon->GetBrush().GetResourceObject()
			&& Available->GetText().EqualTo(FText::AsNumber(
				Window->EmployeeManager->GetAvailableStatPoints(Employee->EmployeeID)));
		Check(FString::Printf(TEXT("%s_%d_stat_points_balance"), *CurrentScenario, CurrentWidth), bBalance,
			Available ? Available->GetText().ToString() : TEXT("missing SP balance"));

		FVector2D PlateStart, PlateSize, CardStart, CardSize;
		const bool bPlate = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("SkillPlate")), PlateStart, PlateSize);
		const bool bCard = LocalRectOf(RootGeometry, Window->DisciplineCard, CardStart, CardSize);
		Check(FString::Printf(TEXT("%s_%d_level_left_of_stat_allocation"), *CurrentScenario, CurrentWidth),
			bPlate && bCard && CardSize.X >= 1.0 && PlateSize.X >= 1.0 && CardStart.X + CardSize.X <= PlateStart.X + 1.0,
			TEXT("The level-up card remains in the portrait column, left of the stat investment panel."));
		for (int32 Index = 0; Index < static_cast<int32>(EEmployeeStatIndex::Count); ++Index)
		{
			const UButtonWidget* Plus = Cast<UButtonWidget>(Window->GetWidgetFromName(*FString::Printf(TEXT("StatPlusButton%d"), Index)));
			const bool bCanAllocate = Employee && Window->EmployeeManager
				&& Window->EmployeeManager->CanAllocateStatPoint(Employee->EmployeeID, static_cast<EEmployeeStatIndex>(Index));
			Check(FString::Printf(TEXT("%s_%d_stat_%d_action_state"), *CurrentScenario, CurrentWidth, Index),
				Plus && Plus->GetIsEnabled() == bCanAllocate, TEXT("Each + button follows its selected employee's spend eligibility."));
			FVector2D ButtonStart, ButtonSize;
			const bool bButton = LocalRectOf(RootGeometry, Plus, ButtonStart, ButtonSize);
			const bool bInside = bPlate && bButton && ButtonSize.X >= 44.0 && ButtonSize.Y >= 44.0
				&& ButtonStart.X >= PlateStart.X - 1.0 && ButtonStart.Y >= PlateStart.Y - 1.0
				&& ButtonStart.X + ButtonSize.X <= PlateStart.X + PlateSize.X + 1.0
				&& ButtonStart.Y + ButtonSize.Y <= PlateStart.Y + PlateSize.Y + 1.0;
			Check(FString::Printf(TEXT("%s_%d_stat_%d_action_inside_panel"), *CurrentScenario, CurrentWidth, Index), bInside,
				TEXT("The + action has at least a 44-unit target inside the central panel."));
			FVector2D RowStart, RowSize;
			const bool bRow = LocalRectOf(RootGeometry,
				Window->GetWidgetFromName(*FString::Printf(TEXT("StatInvestPlate%d"), Index)), RowStart, RowSize);
			Check(FString::Printf(TEXT("%s_%d_stat_%d_row_inside_panel"), *CurrentScenario, CurrentWidth, Index),
				bPlate && bRow && RowStart.X >= PlateStart.X - 1.0 && RowStart.Y >= PlateStart.Y - 1.0
				&& RowStart.X + RowSize.X <= PlateStart.X + PlateSize.X + 1.0
				&& RowStart.Y + RowSize.Y <= PlateStart.Y + PlateSize.Y + 1.0,
				TEXT("Each complete stat row fits inside the central panel."));
			bool bPreviewFits = bPlate;
			FString Detail;
			const TCHAR* const Prefixes[] = { TEXT("HeroStatValue"), TEXT("HeroStatPointText"), TEXT("HeroStatNextEffectText") };
			for (const TCHAR* Prefix : Prefixes)
			{
				const UCommonTextBlock* Label = Cast<UCommonTextBlock>(Window->GetWidgetFromName(*FString::Printf(TEXT("%s%d"), Prefix, Index)));
				FVector2D TextStart, TextSize;
				const bool bRect = LocalRectOf(RootGeometry, Label, TextStart, TextSize);
				const FVector2D LocalTextSize = Label && bRect ? Label->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
				FVector2D Ink = Label && bRect ? InkSizeOf(Label, LocalTextSize) : FVector2D::ZeroVector;
				Ink.X *= LocalTextSize.X > 0.0 ? TextSize.X / LocalTextSize.X : 1.0;
				Ink.Y *= LocalTextSize.Y > 0.0 ? TextSize.Y / LocalTextSize.Y : 1.0;
				const double EffectiveFontSize = Label && LocalTextSize.Y > 0.0
					? Label->GetFont().Size * TextSize.Y / LocalTextSize.Y : 0.0;
				const double MinReadableFontSize = FCString::Strcmp(Prefix, TEXT("HeroStatPointText")) == 0 ? 16.0 : 20.0;
				const bool bFits = Label && !Label->GetText().IsEmpty() && bRect && bPlate
					&& EffectiveFontSize >= MinReadableFontSize
					&& Ink.X <= TextSize.X + 1.0 && Ink.Y <= TextSize.Y + 1.0
					&& TextStart.X >= PlateStart.X - 1.0 && TextStart.Y >= PlateStart.Y - 1.0
					&& TextStart.X + TextSize.X <= PlateStart.X + PlateSize.X + 1.0
					&& TextStart.Y + TextSize.Y <= PlateStart.Y + PlateSize.Y + 1.0;
				bPreviewFits &= bFits;
				if (!bFits) Detail += FString::Printf(TEXT("%s%d preview missing or clipped; "), Prefix, Index);
			}
			Check(FString::Printf(TEXT("%s_%d_stat_%d_preview_fits"), *CurrentScenario, CurrentWidth, Index), bPreviewFits,
				Detail.IsEmpty() ? TEXT("Comparison arrow and next effect are visible and fit.") : Detail);
		}
	}

	// 성장 카드가 레벨·개인 경험치·업무력 비교와 실제 레벨업 CTA를 함께 제공하는지 검사한다.
	void CheckLevelUpCard(UEmployeeWindowWidget* Window, const FGeometry& RootGeometry)
	{
		const UEmployeeLevelUpCardWidget* Card = Window ? Window->DisciplineCard : nullptr;
		if (!Card)
		{
			Check(FString::Printf(TEXT("%s_%d_level_card_bound"), *CurrentScenario, CurrentWidth), false);
			return;
		}
		const TCHAR* const RequiredNames[] = {
			TEXT("CurrentLevelText"), TEXT("NextLevelText"), TEXT("NextLevelGroup"),
			TEXT("PrimaryDisciplineText"), TEXT("CurrentWorkPowerText"), TEXT("NextWorkPowerText"),
			TEXT("CurrentStatPointText"), TEXT("NextStatPointText"), TEXT("ExperienceText"),
			TEXT("LevelUpCostText"), TEXT("ExperienceBar"), TEXT("PrimaryDisciplineIcon"), TEXT("LevelUpButton")};
		TArray<FString> MissingNames;
		for (const TCHAR* Name : RequiredNames)
		{
			if (!Card->GetWidgetFromName(Name)) MissingNames.Add(Name);
		}
		Check(FString::Printf(TEXT("%s_%d_level_card_bound"), *CurrentScenario, CurrentWidth), MissingNames.IsEmpty(),
			MissingNames.IsEmpty() ? TEXT("level, training experience, work power and action bindings present") : FString::Join(MissingNames, TEXT(", ")));
		if (const FEmployeeInstance* Employee = Window->GetSelectedEmployee())
		{
			const UCommonTextBlock* TrainingText = Cast<UCommonTextBlock>(Card->GetWidgetFromName(TEXT("ExperienceText")));
			const UCommonTextBlock* CostText = Cast<UCommonTextBlock>(Card->GetWidgetFromName(TEXT("LevelUpCostText")));
			const UButtonWidget* Action = Cast<UButtonWidget>(Card->GetWidgetFromName(TEXT("LevelUpButton")));
			const bool bCanLevelUp = Window->EmployeeManager && Window->EmployeeManager->CanLevelUpEmployee(Employee->EmployeeID);
			// 경험치 부족은 일시 조건이라 비활성 대신 거부 모드(클릭 시 사유 안내)로 막는다.
			const bool bActionReady = Action && Action->GetIsEnabled() && !Action->HasRejectReason();
			Check(FString::Printf(TEXT("%s_%d_level_card_training_state"), *CurrentScenario, CurrentWidth),
				TrainingText && !TrainingText->GetText().IsEmpty() && CostText && !CostText->GetText().IsEmpty()
					&& Action && bActionReady == bCanLevelUp,
				FString::Printf(TEXT("training text=%d, cost text=%d, action enabled=%d, reject=%d, can level up=%d"),
					TrainingText && !TrainingText->GetText().IsEmpty(), CostText && !CostText->GetText().IsEmpty(),
					Action && Action->GetIsEnabled(), Action && Action->HasRejectReason(), bCanLevelUp));
			// 막대 하나가 보유/필요 비율을, 버튼 문구가 막힌 이유를 말한다.
			const UProgressBar* Bar = Cast<UProgressBar>(Card->GetWidgetFromName(TEXT("ExperienceBar")));
			const int32 LevelCost = Window->EmployeeManager ? Window->EmployeeManager->GetLevelUpCost(Employee->EmployeeID) : 0;
			const double Pool = Window->EmployeeManager ? Window->EmployeeManager->GetExperience(Employee->EmployeeID) : 0.0;
			const float ExpectedPercent = LevelCost > 0 ? static_cast<float>(FMath::Clamp(Pool / LevelCost, 0.0, 1.0)) : 0.f;
			const UCommonTextBlock* ActionLabel = Action ? Cast<UCommonTextBlock>(Action->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
			const FString ExpectedLabel = bCanLevelUp ? TEXT("레벨업") : (LevelCost > 0 && Pool < LevelCost ? TEXT("경험치 부족") : FString());
			Check(FString::Printf(TEXT("%s_%d_level_card_bar_and_label_follow_state"), *CurrentScenario, CurrentWidth),
				Bar && FMath::IsNearlyEqual(Bar->GetPercent(), ExpectedPercent, 0.001f)
					&& ActionLabel && (ExpectedLabel.IsEmpty() || ActionLabel->GetText().ToString() == ExpectedLabel),
				FString::Printf(TEXT("bar=%.3f expected=%.3f label=%s expected=%s"), Bar ? Bar->GetPercent() : -1.f, ExpectedPercent,
					ActionLabel ? *ActionLabel->GetText().ToString() : TEXT("missing"), *ExpectedLabel));
		}
		if (Window->CurrentEmployeePage != EEmployeeWindowPage::Home) return;
		CheckStatAllocation(Window, RootGeometry);
		const UWidget* CardWidget = Window->GetWidgetFromName(TEXT("DisciplineCard"));
		const UWidget* LevelUpButton = Card->GetWidgetFromName(TEXT("LevelUpButton"));
		FVector2D CardStart = FVector2D::ZeroVector, CardSize = FVector2D::ZeroVector;
		FVector2D ButtonStart = FVector2D::ZeroVector, ButtonSize = FVector2D::ZeroVector;
		const bool bCard = LocalRectOf(RootGeometry, CardWidget, CardStart, CardSize);
		const bool bButton = LocalRectOf(RootGeometry, LevelUpButton, ButtonStart, ButtonSize);
		const bool bButtonInside = bCard && bButton && ButtonSize.X >= 44.0 && ButtonSize.Y >= 44.0
			&& ButtonStart.X >= CardStart.X - 1.0 && ButtonStart.Y >= CardStart.Y - 1.0
			&& ButtonStart.X + ButtonSize.X <= CardStart.X + CardSize.X + 1.0
			&& ButtonStart.Y + ButtonSize.Y <= CardStart.Y + CardSize.Y + 1.0;
		Check(FString::Printf(TEXT("%s_%d_level_up_button_inside_card"), *CurrentScenario, CurrentWidth), bButtonInside,
			bCard && bButton ? FString::Printf(TEXT("button %.1fx%.1f inside card %.1fx%.1f"), ButtonSize.X, ButtonSize.Y, CardSize.X, CardSize.Y)
				: TEXT("level-up card or button has no geometry"));
		if (bTrainingValidation)
		{
			const TCHAR* const TextNames[] = {
				TEXT("CurrentLevelText"), TEXT("NextLevelText"),
				TEXT("PrimaryDisciplineText"), TEXT("CurrentWorkPowerText"), TEXT("NextWorkPowerText"),
				TEXT("CurrentStatPointText"), TEXT("NextStatPointText"),
				TEXT("ExperienceText"), TEXT("LevelUpCostText")};
			bool bTextFits = bCard;
			FString FitDetail;
			for (const TCHAR* Name : TextNames)
			{
				const UCommonTextBlock* Label = Cast<UCommonTextBlock>(Card->GetWidgetFromName(Name));
				FVector2D TextStart = FVector2D::ZeroVector, TextSize = FVector2D::ZeroVector;
				const bool bRect = LocalRectOf(RootGeometry, Label, TextStart, TextSize);
				const FVector2D Ink = Label && bRect ? InkSizeOf(Label, Label->GetCachedGeometry().GetLocalSize()) : FVector2D::ZeroVector;
				const bool bFits = bCard && Label && !Label->GetText().IsEmpty() && bRect
					&& Ink.X <= TextSize.X + 1.0 && Ink.Y <= TextSize.Y + 1.0
					&& TextStart.X >= CardStart.X - 1.0 && TextStart.Y >= CardStart.Y - 1.0
					&& TextStart.X + TextSize.X <= CardStart.X + CardSize.X + 1.0
					&& TextStart.Y + TextSize.Y <= CardStart.Y + CardSize.Y + 1.0;
				bTextFits &= bFits;
				if (!bFits) FitDetail += FString::Printf(TEXT("%s text=%s ink=%.1fx%.1f alloc=%.1fx%.1f; "), Name,
					Label ? *Label->GetText().ToString() : TEXT("missing"), Ink.X, Ink.Y, TextSize.X, TextSize.Y);
			}
			Check(FString::Printf(TEXT("%s_%d_level_card_text_visible_and_fits"), *CurrentScenario, CurrentWidth), bTextFits,
				FitDetail.IsEmpty() ? TEXT("All nine level-up labels fit inside the card.") : FitDetail);
		}
	}
	// 성장 카드와 잠재 패널이 저작되지 않았다면 크래시 대신 실패로 기록한다.
	void CheckInlinePotentialGeometry()
	{
		const UEmployeeWindowWidget* Window = EmbeddedWindow.Get();
		const UPotentialResetWorkspaceWidget* Potential = Window ? Window->PotentialWorkspace.Get() : nullptr;
		if (!Window || !Potential || !Window->PotentialHost)
		{
			Check(CurrentScenario + TEXT("_inline_potential_geometry_bound"), false);
			return;
		}
		const FGeometry& RootGeometry = Window->GetCachedGeometry();
		{
			// 라벨·소비 칩·상점 아이콘 묶음은 버튼 가운데에 서고, 켜진 쪽은 실제로 그려져야 한다.
			// 감싼 칸이 접혀 아이콘만 켜진 회귀·접힌 칸이 여백만 남긴 쏠림은 가시성 플래그로 못 잡아 배치로 잰다
			const UWidget* ResetButtonWidget = Potential->GetWidgetFromName(TEXT("ResetButton"));
			const UWidget* ResetContent = Potential->GetWidgetFromName(TEXT("ResetContent"));
			const UWidget* ShopIcon = Potential->GetWidgetFromName(TEXT("ResetShopIcon"));
			const UWidget* CostChip = Potential->GetWidgetFromName(TEXT("ResetCostChip"));
			FVector2D ButtonStart = FVector2D::ZeroVector, ButtonSize = FVector2D::ZeroVector;
			FVector2D ContentStart = FVector2D::ZeroVector, ContentSize = FVector2D::ZeroVector;
			const bool bRects = LocalRectOf(RootGeometry, ResetButtonWidget, ButtonStart, ButtonSize)
				&& LocalRectOf(RootGeometry, ResetContent, ContentStart, ContentSize);
			const double CenterOffset = bRects ? (ContentStart.X + ContentSize.X * .5) - (ButtonStart.X + ButtonSize.X * .5) : 0.0;
			const bool bBuyMode = ShopIcon && ShopIcon->IsVisible();
			const UWidget* ModeWidget = bBuyMode ? ShopIcon : CostChip;
			FVector2D ModeStart = FVector2D::ZeroVector, ModeSize = FVector2D::ZeroVector;
			const bool bModeDrawn = ModeWidget && ModeWidget->IsVisible() && LocalRectOf(RootGeometry, ModeWidget, ModeStart, ModeSize)
				&& ModeSize.X >= 40.0 && ModeSize.Y >= 40.0;
			Check(FString::Printf(TEXT("%s_%d_inline_potential_reset_content_centered_and_drawn"), *CurrentScenario, CurrentWidth),
				bRects && FMath::Abs(CenterOffset) <= 2.0 && bModeDrawn,
				FString::Printf(TEXT("%s %.0fx%.0f, content %.1f wide, center offset %.1f"), bBuyMode ? TEXT("shop icon") : TEXT("cost chip"),
					ModeSize.X, ModeSize.Y, ContentSize.X, CenterOffset));
		}
		FVector2D HostStart, HostSize;
		const bool bHost = LocalRectOf(RootGeometry, Window->PotentialHost, HostStart, HostSize);
		bool bInside = bHost;
		FString BoundsDetail;
		const TCHAR* RequiredContent[] = {TEXT("TargetRowSize"), TEXT("PotLine0"), TEXT("PotLine1"), TEXT("PotLine2"),
			TEXT("CardCell0"), TEXT("CardCell1"), TEXT("CardCell2"), TEXT("OddsWellSize"), TEXT("ResetButton"), TEXT("OddsButton"), TEXT("ShopButton")};
		for (const TCHAR* Name : RequiredContent)
		{
			const UWidget* Content = Potential->GetWidgetFromName(Name);
			FVector2D ContentStart, ContentSize;
			const bool bRect = Content && Content->IsVisible() && LocalRectOf(RootGeometry, Content, ContentStart, ContentSize);
			const bool bFits = bRect && ContentStart.X >= HostStart.X - 1.0 && ContentStart.Y >= HostStart.Y - 1.0
				&& ContentStart.X + ContentSize.X <= HostStart.X + HostSize.X + 1.0
				&& ContentStart.Y + ContentSize.Y <= HostStart.Y + HostSize.Y + 1.0;
			bInside &= bFits;
			if (!bFits) BoundsDetail += FString::Printf(TEXT("%s missing or outside %.1fx%.1f host; "), Name, HostSize.X, HostSize.Y);
		}
		Check(FString::Printf(TEXT("%s_%d_inline_potential_content_inside_host"), *CurrentScenario, CurrentWidth), bInside,
			BoundsDetail.IsEmpty() ? TEXT("Header, 3 rows, 3 cards and actions fit inside the inline host.") : BoundsDetail);
		struct FHeightSpec { const TCHAR* Name; double Height; };
		const FHeightSpec Heights[] = {{TEXT("PotLine0"), 112.0}, {TEXT("PotLine1"), 112.0}, {TEXT("PotLine2"), 112.0},
			{TEXT("CardCell0"), 300.0}, {TEXT("OddsWellSize"), 160.0},
			{TEXT("ResetButton"), 96.0}, {TEXT("OddsButton"), 72.0}, {TEXT("ShopButton"), 72.0}};
		for (const FHeightSpec& Spec : Heights)
		{
			FVector2D ContentStart, ContentSize;
			const bool bRect = LocalRectOf(RootGeometry, Potential->GetWidgetFromName(Spec.Name), ContentStart, ContentSize);
			Check(FString::Printf(TEXT("%s_%d_inline_%s_height"), *CurrentScenario, CurrentWidth, Spec.Name),
				bRect && FMath::IsNearlyEqual(ContentSize.Y, Spec.Height, 1.0),
				FString::Printf(TEXT("height %.1f expected %.0f"), bRect ? ContentSize.Y : -1.0, Spec.Height));
		}
		const UButtonWidget* ResetButton = Cast<UButtonWidget>(Potential->GetWidgetFromName(TEXT("ResetButton")));
		const UItemInventoryManager* Items = GetItemInventory();
		const int32 Count = Items ? Items->GetItemCount(Potential->GetSelectedCard()) : 0;
		Check(FString::Printf(TEXT("%s_%d_inline_reset_enabled_with_employee"), *CurrentScenario, CurrentWidth),
			ResetButton && ResetButton->GetIsEnabled() == (Window->GetSelectedEmployee() != nullptr),
			FString::Printf(TEXT("selected card count %d — 0 turns the same button into purchase"), Count));
		// 타일은 열 폭을 셋으로 나눠 채운다(2304 에서 열이 가장 좁다) — 같은 폭·같은 높이·간격 14·호스트 안
		FVector2D CardStarts[3], CardSizes[3];
		bool bCardRow = true;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const UWidget* Cell = Potential->GetWidgetFromName(*FString::Printf(TEXT("CardCell%d"), Index));
			bCardRow &= LocalRectOf(RootGeometry, Cell, CardStarts[Index], CardSizes[Index]);
		}
		if (bCardRow)
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				bCardRow &= FMath::IsNearlyEqual(CardSizes[Index].X, CardSizes[0].X, 1.0)
					&& CardSizes[Index].X >= 140.0
					&& FMath::IsNearlyEqual(CardSizes[Index].Y, 300.0, 1.0)
					&& CardStarts[Index].X >= HostStart.X - 1.0
					&& CardStarts[Index].X + CardSizes[Index].X <= HostStart.X + HostSize.X + 1.0;
				if (Index > 0)
				{
					const double Gap = CardStarts[Index].X - CardStarts[Index - 1].X - CardSizes[Index - 1].X;
					bCardRow &= FMath::IsNearlyEqual(Gap, 14.0, 1.0)
						&& FMath::IsNearlyEqual(CardStarts[Index].Y, CardStarts[0].Y, 1.0);
				}
			}
		}
		Check(FString::Printf(TEXT("%s_%d_inline_potential_cards_equal_and_separated"), *CurrentScenario, CurrentWidth), bCardRow,
			bCardRow ? FString::Printf(TEXT("Three %.1fx300 tiles have 14-unit gaps and fit inside PotentialHost."), CardSizes[0].X)
				: FString(TEXT("tile width/height/gap mismatch or missing")));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const UWidget* CardCheck = Potential->GetWidgetFromName(*FString::Printf(TEXT("CardCheck%d"), Index));
			const UWidget* CardArt = Potential->GetWidgetFromName(*FString::Printf(TEXT("CardArt%d"), Index));
			const auto* Name = Cast<UCommonTextBlock>(Potential->GetWidgetFromName(*FString::Printf(TEXT("PotLineName%d"), Index)));
			const auto* Value = Cast<UCommonTextBlock>(Potential->GetWidgetFromName(*FString::Printf(TEXT("PotLineValue%d"), Index)));
			const UWidget* Line = Potential->GetWidgetFromName(*FString::Printf(TEXT("PotLine%d"), Index));
			FVector2D NameStart, NameSize, ValueStart, ValueSize;
			const bool bTextRects = LocalRectOf(RootGeometry, Name, NameStart, NameSize) && LocalRectOf(RootGeometry, Value, ValueStart, ValueSize);
			Check(FString::Printf(TEXT("%s_%d_inline_potential_line_%d_readable"), *CurrentScenario, CurrentWidth, Index),
				Line && Name && Value && !Name->GetText().IsEmpty() && !Value->GetText().IsEmpty()
				&& FMath::IsNearlyEqual(Line->GetRenderOpacity(), 1.f) && bTextRects && NameStart.X + NameSize.X <= ValueStart.X + 1.0);
			const bool bSelected = BusinessCardTypes[Index] == Potential->GetSelectedCard();
			Check(FString::Printf(TEXT("%s_%d_inline_potential_card_%d_current_selection"), *CurrentScenario, CurrentWidth, Index),
				CardCheck && CardCheck->GetVisibility() == (bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden));
			if (Items && Items->GetItemCount(BusinessCardTypes[Index]) == 0)
			{
				Check(FString::Printf(TEXT("%s_%d_inline_potential_card_%d_zero_count_presentation"), *CurrentScenario, CurrentWidth, Index),
					CardArt && FMath::IsNearlyEqual(CardArt->GetRenderOpacity(), 0.42f, 0.01f)
					&& CardCheck && CardCheck->IsVisible() == bSelected);
			}
		}
		CheckPotentialPlateFinish(Potential, RootGeometry);
	}

	// 판 마감 — 판 한 장·명함 원래 비율·숫자 글꼴·모서리 px (사용자 지적 "테두리 드럽게 남아있다·명함 눌려있다·글씨 이상하다")
	void CheckPotentialPlateFinish(const UUserWidget* Potential, const FGeometry& RootGeometry)
	{
		// 바깥 Border 가 외곽선을 또 그리면 판 재질 모서리와 어긋나 반달 얼룩이 남는다
		FString LayerDetail;
		const TCHAR* PlateBorders[] = {TEXT("PotLine0"), TEXT("PotLine1"), TEXT("PotLine2"),
			TEXT("CardTile0"), TEXT("CardTile1"), TEXT("CardTile2"), TEXT("OddsWell")};
		for (const TCHAR* BorderName : PlateBorders)
		{
			const UBorder* PlateBorder = Cast<UBorder>(Potential->GetWidgetFromName(BorderName));
			if (!PlateBorder || PlateBorder->Background.DrawAs != ESlateBrushDrawType::NoDrawType)
				LayerDetail += FString::Printf(TEXT("%s %s; "), BorderName, PlateBorder ? TEXT("draws") : TEXT("missing"));
		}
		Check(FString::Printf(TEXT("%s_%d_inline_potential_plates_single_layer"), *CurrentScenario, CurrentWidth), LayerDetail.IsEmpty(),
			LayerDetail.IsEmpty() ? TEXT("7 plate borders draw nothing; the plate material alone draws fill, edge and lip.") : LayerDetail);

		// 재질 길이 단위 = 이미지 짧은 변 절반 — 환산이 맞으면 반경×절반이 시안 px 와 같다
		struct FRadiusSpec { const TCHAR* Name; double RadiusPx; };
		const FRadiusSpec Radii[] = {{TEXT("PotLineBG0"), 16.0}, {TEXT("CardTileBG0"), 20.0}, {TEXT("OddsWellBG"), 18.0}};
		for (const FRadiusSpec& Spec : Radii)
		{
			UImage* PlateImage = Cast<UImage>(Potential->GetWidgetFromName(Spec.Name));
			UMaterialInstanceDynamic* PlateMaterial = PlateImage ? PlateImage->GetDynamicMaterial() : nullptr;
			const FVector2D PlateSize = PlateImage ? PlateImage->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
			const double Half = FMath::Min(PlateSize.X, PlateSize.Y) * .5;
			const double RadiusPx = PlateMaterial ? PlateMaterial->K2_GetScalarParameterValue(TEXT("CornerRadius")) * Half : -1.0;
			const FString MaterialName = PlateMaterial && PlateMaterial->Parent ? PlateMaterial->Parent->GetName() : FString(TEXT("none"));
			Check(FString::Printf(TEXT("%s_%d_inline_potential_%s_corner_px"), *CurrentScenario, CurrentWidth, Spec.Name),
				Half >= 1.0 && FMath::IsNearlyEqual(RadiusPx, Spec.RadiusPx, 0.5) && MaterialName.StartsWith(TEXT("MI_EmployeePotential")),
				FString::Printf(TEXT("%s %.0fx%.0f radius %.2fpx expected %.0f"), *MaterialName, PlateSize.X, PlateSize.Y, RadiusPx, Spec.RadiusPx));
		}

		// 명함 텍스처는 정사각(288²) 가운데 카드 — 보이는 텍스처 영역과 그리는 상자의 가로·세로 배율이 같아야 카드가 안 눌린다
		FString ArtDetail;
		bool bArtRatio = true;
		struct FArtSpec { const TCHAR* Name; double MinWidth; const TCHAR* Holder; };
		const FArtSpec Arts[] = {{TEXT("CardArt0"), 110.0, nullptr}, {TEXT("CardArt1"), 110.0, nullptr}, {TEXT("CardArt2"), 110.0, nullptr},
			{TEXT("ResetCostIcon"), 40.0, TEXT("ResetCostChip")}};
		for (const FArtSpec& Spec : Arts)
		{
			const UImage* Art = Cast<UImage>(Potential->GetWidgetFromName(Spec.Name));
			// 0장이면 재설정 버튼이 구매로 바뀌며 소비 칩이 접힌다 — 그림 자신은 보이는 상태라 담는 칩으로 가른다
			const UWidget* Holder = Spec.Holder ? Potential->GetWidgetFromName(Spec.Holder) : nullptr;
			if ((Art && !Art->IsVisible()) || (Holder && !Holder->IsVisible())) continue;
			const UTexture* Texture = Art ? Cast<UTexture>(Art->GetBrush().GetResourceObject()) : nullptr;
			FBox2f UV(ForceInit);
			if (Art) UV = Art->GetBrush().GetUVRegion();
			const FVector2f UVSize = UV.bIsValid ? UV.GetSize() : FVector2f(1.f, 1.f);
			FVector2D ArtStart, ArtSize;
			const bool bRect = LocalRectOf(RootGeometry, Art, ArtStart, ArtSize);
			const double ScaleX = Texture && bRect ? ArtSize.X / (UVSize.X * Texture->GetSurfaceWidth()) : 0.0;
			const double ScaleY = Texture && bRect ? ArtSize.Y / (UVSize.Y * Texture->GetSurfaceHeight()) : 0.0;
			const double Stretch = ScaleY > 0.0 ? ScaleX / ScaleY : 0.0;
			bArtRatio &= FMath::IsNearlyEqual(Stretch, 1.0, 0.03) && ArtSize.X >= Spec.MinWidth;
			ArtDetail += FString::Printf(TEXT("%s %.1fx%.1f stretch %.3f; "), Spec.Name, bRect ? ArtSize.X : -1.0, bRect ? ArtSize.Y : -1.0, Stretch);
		}
		Check(FString::Printf(TEXT("%s_%d_inline_potential_card_art_true_ratio"), *CurrentScenario, CurrentWidth), bArtRatio, ArtDetail);

		// 숫자=Pretendard(직원창 업무력·스탯과 같다)·문구=NEXON — 줄 값이 「★3 해금」 같은 문구면 같은 줄 이름과 같은 글꼴
		auto FontNameOf = [](const UTextBlock* Text) { return Text && Text->GetFont().FontObject ? Text->GetFont().FontObject->GetName() : FString(TEXT("none")); };
		FString FontDetail;
		const TCHAR* NumberTexts[] = {TEXT("CardCountText0"), TEXT("CardCountText1"), TEXT("CardCountText2"),
			TEXT("CardChanceText0"), TEXT("CardChanceText1"), TEXT("CardChanceText2"), TEXT("OddsValueText")};
		for (const TCHAR* TextName : NumberTexts)
		{
			const FString FontName = FontNameOf(Cast<UTextBlock>(Potential->GetWidgetFromName(TextName)));
			if (!FontName.Contains(TEXT("Pretendard"))) FontDetail += FString::Printf(TEXT("%s=%s; "), TextName, *FontName);
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const UTextBlock* Name = Cast<UTextBlock>(Potential->GetWidgetFromName(*FString::Printf(TEXT("PotLineName%d"), Index)));
			const UTextBlock* Value = Cast<UTextBlock>(Potential->GetWidgetFromName(*FString::Printf(TEXT("PotLineValue%d"), Index)));
			if (!Name || !Value) { FontDetail += FString::Printf(TEXT("line %d missing; "), Index); continue; }
			const FString ValueText = Value->GetText().ToString();
			bool bPhrase = false;
			for (const TCHAR Ch : ValueText) bPhrase |= Ch >= 0xAC00 && Ch <= 0xD7A3;
			const FString ValueFont = FontNameOf(Value);
			const bool bRight = bPhrase ? ValueFont == FontNameOf(Name) : ValueFont.Contains(TEXT("Pretendard"));
			if (!bRight) FontDetail += FString::Printf(TEXT("PotLineValue%d '%s'=%s (name %s); "), Index, *ValueText, *ValueFont, *FontNameOf(Name));
		}
		Check(FString::Printf(TEXT("%s_%d_inline_potential_number_and_phrase_fonts"), *CurrentScenario, CurrentWidth), FontDetail.IsEmpty(),
			FontDetail.IsEmpty() ? TEXT("Numbers use Pretendard like the window stats; phrase values share the line name's NEXON.") : FontDetail);
	}

	void CheckLeanHomeAndPotentialGeometry()
	{
		UEmployeeWindowWidget* Window = EmbeddedWindow.Get();
		if (!Window) return;
		const FGeometry& RootGeometry = Window->GetCachedGeometry();
		const FString Missing = TEXT("widget not authored yet");
		// 띠는 드로어가 열려도 계속 보인다 — 홈 시나리오에만 걸면 ⑫ 의 절반(드로어 8 시나리오)이 사각에 남는다
		if (CurrentScenario.StartsWith(TEXT("employee_window_")))
		{
			CheckRosterStrip(Window, RootGeometry);
			CheckHeroColumn(Window, RootGeometry);
			CheckLevelUpCard(Window, RootGeometry);
		}
		if (CurrentScenario == TEXT("employee_window_lean_home"))
		{
			// v2 는 GrowthPlate 를 추방하고 스킬/잠재 판을 따로 세운다 — 재설정 알약의 기준면은 SkillPlate 다
			const UWidget* SkillPlate = Window->GetWidgetFromName(TEXT("SkillPlate"));
			FVector2D PlateStart, PlateSize;
			const bool bPlate = LocalRectOf(RootGeometry, SkillPlate, PlateStart, PlateSize);
			if (CurrentWidth == 2304)
			{
				// v3.2 세로 직원 카드 — HeroColBox 는 LayoutHBox 의 26% 비중 열이다
				const UWidget* HeroColumn = Window->GetWidgetFromName(TEXT("HeroColBox"));
				FVector2D HeroStart, HeroSize;
				const bool bHero = LocalRectOf(RootGeometry, HeroColumn, HeroStart, HeroSize);
				FVector2D HeroLayoutStart, HeroLayoutSize;
				const bool bHeroLayout = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("LayoutHBox")), HeroLayoutStart, HeroLayoutSize);
				Check(FString::Printf(TEXT("%s_%d_v3_hero_col_fills"), *CurrentScenario, CurrentWidth),
					bHero && bHeroLayout && FMath::Abs(HeroSize.X - HeroLayoutSize.X * 0.26) <= HeroLayoutSize.X * 0.03,
					bHero && bHeroLayout ? FString::Printf(TEXT("HeroColBox width %.1f (want 26%% of layout %.1f)"), HeroSize.X, HeroLayoutSize.X) : Missing);
				// v3.2 로스터 전 화면비 2열 — 타일 150x200, 열 수는 레일 폭이 정한다(floor((폭+간격)/(타일+간격))).
				const UListView* Roster = Window->RosterListView;
				const double RosterWidth = Roster ? Roster->GetCachedGeometry().GetLocalSize().X : 0.0;
				const int32 ExpectedColumns = RosterWidth >= 150.0 ? FMath::Max(1, FMath::FloorToInt32((RosterWidth + 12.0) / (150.0 + 12.0))) : 1;
				bool bAnyEntry = false;
				double LeadingEntryX = 0.0;
				FVector2D LeadingEntrySize = FVector2D::ZeroVector;
				TSet<int32> ColumnXs;
				int32 EntryCount = 0;
				if (Roster) for (const UUserWidget* Entry : Roster->GetDisplayedEntryWidgets())
				{
					const UWidget* CardFace = Entry ? Entry->GetWidgetFromName(TEXT("RowSizeBox")) : nullptr;
					FVector2D EntryStart, EntrySize;
					if (!LocalRectOf(RootGeometry, CardFace ? CardFace : Entry, EntryStart, EntrySize)) continue;
					if (EntrySize.X < 1.0 || EntrySize.Y < 1.0) continue;   // 가상화 스톡(0크기) 엔트리 제외
					if (!bAnyEntry || EntryStart.X < LeadingEntryX) { LeadingEntryX = EntryStart.X; LeadingEntrySize = EntrySize; }
					ColumnXs.Add(FMath::RoundToInt(EntryStart.X / 8.0));   // 열 구분(8px 버킷)
					bAnyEntry = true;
					++EntryCount;
				}
				const bool bColumns = ColumnXs.Num() == ExpectedColumns;   // 레일 폭이 허용하는 열 수와 정확히 일치
				const bool bTileSize = FMath::IsNearlyEqual(LeadingEntrySize.X, 150.0, 2.0) && FMath::IsNearlyEqual(LeadingEntrySize.Y, 200.0, 2.0);
				Check(FString::Printf(TEXT("%s_%d_v3_roster_vertical_grid"), *CurrentScenario, CurrentWidth), bAnyEntry && bColumns && bTileSize,
					bAnyEntry ? FString::Printf(TEXT("%d entries, %d columns (rail %.0fpx -> expect %d), tile %.1f x %.1f (want 150x200)"), EntryCount, ColumnXs.Num(), RosterWidth, ExpectedColumns, LeadingEntrySize.X, LeadingEntrySize.Y) : Missing);
			}
			if (CurrentWidth == 2304 || CurrentWidth == 2560 || CurrentWidth == 3120)
			{
				// v3.1 — GrowthCol(growrow)은 우측 메인을 채운다(스킬 66% + 잠재 34%)
				const UWidget* GrowthColumn = Window->GetWidgetFromName(TEXT("GrowthCol"));
				FVector2D GrowthStart, GrowthSize;
				const bool bGrowth = LocalRectOf(RootGeometry, GrowthColumn, GrowthStart, GrowthSize);
				Check(FString::Printf(TEXT("%s_%d_v2_growth_col_min_1100"), *CurrentScenario, CurrentWidth), bGrowth && GrowthSize.X >= 1100.0,
					bGrowth ? FString::Printf(TEXT("GrowthCol width %.1f"), GrowthSize.X) : Missing);
				// v3.1 — layout(레일 | 메인)이 뷰포트를 FILL 한다(구 BodyHBox 2348 캡 폐기)
				const UWidget* Layout = Window->GetWidgetFromName(TEXT("LayoutHBox"));
				FVector2D LayoutStart, LayoutSize;
				const bool bLayout = LocalRectOf(RootGeometry, Layout, LayoutStart, LayoutSize);
				Check(FString::Printf(TEXT("%s_%d_v3_layout_fills"), *CurrentScenario, CurrentWidth), bLayout && LayoutSize.X >= 2000.0,
					bLayout ? FString::Printf(TEXT("LayoutHBox width %.1f (fills viewport; want >=2000)"), LayoutSize.X) : Missing);
			}
			const UWidget* LevelUpButton = Window->DisciplineCard
				? Window->DisciplineCard->GetWidgetFromName(TEXT("LevelUpButton")) : nullptr;

			// v2.2 ⑥ — 등급은 타일 위 액센트 바가 아니라 타일을 두르는 프레임이다: 상단 밴드 6개 전부 Collapsed,
			// 프레임(GradeFrame)은 빈 슬롯 포함 6/6 가시. 타일 면은 88 정사각(v3.2 — 100→88, 무대 상하 26 여백).
			int32 VisibleFrames = 0, CollapsedBands = 0, SquareTiles = 0;
			FString FrameDetail;
			for (int32 SlotIndex = 0; SlotIndex < 6; ++SlotIndex)
			{
				const UUserWidget* SlotTile = Cast<UUserWidget>(Window->GetWidgetFromName(*FString::Printf(TEXT("HeroEquipmentSlot%d"), SlotIndex)));
				const UWidget* SlotBand = SlotTile ? SlotTile->GetWidgetFromName(TEXT("SlotRarityBand")) : nullptr;
				const UWidget* SlotFrame = SlotTile ? SlotTile->GetWidgetFromName(TEXT("GradeFrame")) : nullptr;
				CollapsedBands += (!SlotBand || SlotBand->GetVisibility() == ESlateVisibility::Collapsed) ? 1 : 0;
				VisibleFrames += (SlotFrame && SlotFrame->GetVisibility() != ESlateVisibility::Collapsed
					&& SlotFrame->GetVisibility() != ESlateVisibility::Hidden) ? 1 : 0;
				const UWidget* TileFace = SlotTile ? SlotTile->GetWidgetFromName(TEXT("HomeSlotVisualSize")) : nullptr;
				FVector2D TileStart, TileSize;
				const bool bTileRect = LocalRectOf(RootGeometry, TileFace, TileStart, TileSize);
				SquareTiles += (bTileRect && FMath::IsNearlyEqual(TileSize.X, 88.0, 1.0)
					&& FMath::IsNearlyEqual(TileSize.Y, 88.0, 1.0)) ? 1 : 0;
				FrameDetail += FString::Printf(TEXT("%d:%.0fx%.0f "), SlotIndex,
					bTileRect ? TileSize.X : -1.0, bTileRect ? TileSize.Y : -1.0);
			}
			Check(FString::Printf(TEXT("%s_%d_v22_slot_frame_visible"), *CurrentScenario, CurrentWidth),
				VisibleFrames == 6 && CollapsedBands == 6,
				FString::Printf(TEXT("%d/6 frames visible, %d/6 bands collapsed"), VisibleFrames, CollapsedBands));
			Check(FString::Printf(TEXT("%s_%d_v22_slot_tiles_square"), *CurrentScenario, CurrentWidth), SquareTiles == 6,
				FString::Printf(TEXT("%d/6 tiles 88x88 (%s)"), SquareTiles, *FrameDetail));

			// v2.9 v2_slot_stage_margin — 슬롯 면이 무대(StageBox) 상/하 가장자리에서 각 >=16 (위/아래 슬롯이 붙지 않게)
			const UWidget* StageBoxWidget = Window->GetWidgetFromName(TEXT("StageBox"));
			FVector2D StageStart, StageSize;
			double SlotTopMargin = -1.0, SlotBottomMargin = -1.0;
			if (LocalRectOf(RootGeometry, StageBoxWidget, StageStart, StageSize))
			{
				double FaceTop = 0.0, FaceBottom = 0.0; bool bAnyFace = false;
				for (int32 SlotIndex = 0; SlotIndex < 6; ++SlotIndex)
				{
					const UUserWidget* MarginTile = Cast<UUserWidget>(Window->GetWidgetFromName(*FString::Printf(TEXT("HeroEquipmentSlot%d"), SlotIndex)));
					const UWidget* MarginFace = MarginTile ? MarginTile->GetWidgetFromName(TEXT("HomeSlotVisualSize")) : nullptr;
					FVector2D FaceStart, FaceSize;
					if (!LocalRectOf(RootGeometry, MarginFace, FaceStart, FaceSize)) continue;
					if (!bAnyFace) { FaceTop = FaceStart.Y; FaceBottom = FaceStart.Y + FaceSize.Y; bAnyFace = true; }
					else { FaceTop = FMath::Min(FaceTop, FaceStart.Y); FaceBottom = FMath::Max(FaceBottom, FaceStart.Y + FaceSize.Y); }
				}
				if (bAnyFace)
				{
					SlotTopMargin = FaceTop - StageStart.Y;
					SlotBottomMargin = StageStart.Y + StageSize.Y - FaceBottom;
				}
			}
			Check(FString::Printf(TEXT("%s_%d_v2_slot_stage_margin"), *CurrentScenario, CurrentWidth),
				SlotTopMargin >= 15.5 && SlotBottomMargin >= 15.5,
				FString::Printf(TEXT("slot face->stage margin top %.1f bottom %.1f (target >=16)"), SlotTopMargin, SlotBottomMargin));

			// v2.9 v2_pedestal_visible — 절차식 알약 폐기, Glow_Oval 쿨 발판. 존재+가시+브러시 리소스가 Glow_Oval 이어야 한다
			const UImage* PedestalImage = Cast<UImage>(Window->GetWidgetFromName(TEXT("HeroPedestal")));
			const UObject* PedestalResource = PedestalImage ? PedestalImage->GetBrush().GetResourceObject() : nullptr;
			Check(FString::Printf(TEXT("%s_%d_v2_pedestal_visible"), *CurrentScenario, CurrentWidth),
				PedestalImage && PedestalImage->IsVisible() && PedestalResource && PedestalResource->GetName() == TEXT("Glow_Oval"),
				FString::Printf(TEXT("pedestal present=%d visible=%d res=%s (expect Glow_Oval cool disc, not procedural pill)"),
					PedestalImage ? 1 : 0, (PedestalImage && PedestalImage->IsVisible()) ? 1 : 0,
					PedestalResource ? *PedestalResource->GetName() : TEXT("null")));

			// 행동 역할별 버튼 높이를 확인한다.
			const UWidget* LadderWidgets[] = {
				Window->GetWidgetFromName(TEXT("EnhanceButton")), Window->GetWidgetFromName(TEXT("HeroEquipmentOpenButton")),
				LevelUpButton,
				Window->PotentialWorkspace ? Window->PotentialWorkspace->GetWidgetFromName(TEXT("ResetButton")) : nullptr,
				Window->GetWidgetFromName(TEXT("FireButton")) };
			const double LadderExpected[] = { 96.0, 96.0, 44.0, 96.0, 56.0 };
			const TCHAR* const LadderNames[] = { TEXT("Enhance"), TEXT("Equipment"), TEXT("LevelUp"), TEXT("Reroll"), TEXT("Fire") };
			bool bLadder = true;
			FString LadderDetail;
			for (int32 LadderIndex = 0; LadderIndex < UE_ARRAY_COUNT(LadderWidgets); ++LadderIndex)
			{
				FVector2D LadderStart, LadderSize;
				const bool bRect = LocalRectOf(RootGeometry, LadderWidgets[LadderIndex], LadderStart, LadderSize);
				bLadder = bLadder && bRect && (LadderIndex == 2
					? LadderSize.Y >= LadderExpected[LadderIndex]
					: FMath::IsNearlyEqual(LadderSize.Y, LadderExpected[LadderIndex], 1.0));
				LadderDetail += FString::Printf(TEXT("%s %.1f/%.0f "), LadderNames[LadderIndex], bRect ? LadderSize.Y : -1.0, LadderExpected[LadderIndex]);
			}
			LadderDetail += TEXT("(LevelUp checks the 44-unit touch minimum)");
			Check(FString::Printf(TEXT("%s_%d_v21_button_ladder"), *CurrentScenario, CurrentWidth), bLadder, LadderDetail);

			// v2.3 ㉑ — 성장/직원 열의 블록 간격 상수 정렬. 위젯 사각형 기준(잉크 아님), ±2.
			struct FBlockGapSpec { const TCHAR* Above; const TCHAR* Below; const TCHAR* Label; double Expected; };
			// v3.1 - stage->OVR 은 좌/우 열로 갈려 세로 간격이 아니다(제거). stats->action 은 BLOCK_GAP_LG(20).
			static const FBlockGapSpec BlockGaps[] = {
				{TEXT("PotLineSize2"), TEXT("CardSectionRowSize"), TEXT("potential lines->card section"), 36.0},
				{TEXT("DisciplineCard"), TEXT("ActionRow"), TEXT("level card->action"), 16.0}};
			bool bGaps = true;
			FString GapDetail;
			for (const FBlockGapSpec& Spec : BlockGaps)
			{
				const UWidget* Above = Window->GetWidgetFromName(Spec.Above);
				if (!Above && Window->DisciplineCard) Above = Window->DisciplineCard->GetWidgetFromName(Spec.Above);
				if (!Above && Window->PotentialWorkspace) Above = Window->PotentialWorkspace->GetWidgetFromName(Spec.Above);
				const UWidget* Below = Spec.Below ? Window->GetWidgetFromName(Spec.Below) : Window->GetWidgetFromName(TEXT("SkillPlate"));
				if (!Below && Spec.Below && Window->DisciplineCard) Below = Window->DisciplineCard->GetWidgetFromName(Spec.Below);
				if (!Below && Spec.Below && Window->PotentialWorkspace) Below = Window->PotentialWorkspace->GetWidgetFromName(Spec.Below);
				FVector2D AboveStart, AboveSize, BelowStart, BelowSize;
				if (!LocalRectOf(RootGeometry, Above, AboveStart, AboveSize) || !LocalRectOf(RootGeometry, Below, BelowStart, BelowSize))
				{
					bGaps = false;
					GapDetail += FString::Printf(TEXT("%s missing; "), Spec.Label);
					continue;
				}
				// 마지막 항목(스킬 판 하단)은 판의 아래 변에서 내용 아래 변까지를 잰다
				const double Gap = Spec.Below ? BelowStart.Y - (AboveStart.Y + AboveSize.Y)
					: (BelowStart.Y + BelowSize.Y) - (AboveStart.Y + AboveSize.Y);
				GapDetail += FString::Printf(TEXT("%s %.1f/%.0f; "), Spec.Label, Gap, Spec.Expected);
				bGaps = bGaps && FMath::Abs(Gap - Spec.Expected) <= 2.0;
			}
			Check(FString::Printf(TEXT("%s_%d_v2_block_gaps"), *CurrentScenario, CurrentWidth), bGaps, GapDetail);

			// ㉒ v2_no_dead_spacer — 성장 열의 세로 죽은 Fill 스페이서(PotFillSpacer/GrowthFillSpacer)가 추방됐는가.
			// 판을 내용 크기에 고정해 빈 공간을 없앤다. 가로 정렬 스페이서(PotSpacer/PotLineSpacer/PotFooterSpacer)는 유지.
			const bool bNoPotFill = Window->GetWidgetFromName(TEXT("PotFillSpacer")) == nullptr;
			const bool bNoGrowthFill = Window->GetWidgetFromName(TEXT("GrowthFillSpacer")) == nullptr;
			Check(FString::Printf(TEXT("%s_%d_v2_no_dead_spacer"), *CurrentScenario, CurrentWidth), bNoPotFill && bNoGrowthFill,
				FString::Printf(TEXT("PotFillSpacer null=%d, GrowthFillSpacer null=%d (vertical dead spacers must be banished)"),
					bNoPotFill ? 1 : 0, bNoGrowthFill ? 1 : 0));

			// §9-④ 상시 연출 예산 — 창이 재는 축(같은 함수를 1초 로그도 쓴다)이라 로그와 갈리지 않는다
			const int32 AlwaysOnEffects = Window->GetAlwaysOnEffectCount();
			Report->SetNumberField(TEXT("effects_always_on"), AlwaysOnEffects);
			Check(FString::Printf(TEXT("%s_%d_v2_effects_always_on_le_3"), *CurrentScenario, CurrentWidth), AlwaysOnEffects <= 3,
				FString::Printf(TEXT("%d always-on effects (budget 3: rarity glow / gold CTA sweep / hire pulse)"), AlwaysOnEffects));

			FVector2D LevelUpStart, LevelUpSize;
			const bool bLevelUpRect = LocalRectOf(RootGeometry, LevelUpButton, LevelUpStart, LevelUpSize);
			FVector2D HeroColumnStart, HeroColumnSize;
			const bool bHeroColumn = LocalRectOf(RootGeometry, Window->GetWidgetFromName(TEXT("HeroColBox")), HeroColumnStart, HeroColumnSize);
			const bool bInsideHero = bHeroColumn && bLevelUpRect
				&& LevelUpSize.X >= 1.0 && LevelUpSize.Y >= 1.0 && HeroColumnSize.X >= 1.0 && HeroColumnSize.Y >= 1.0
				&& LevelUpStart.X >= HeroColumnStart.X - 1.0 && LevelUpStart.Y >= HeroColumnStart.Y - 1.0
				&& LevelUpStart.X + LevelUpSize.X <= HeroColumnStart.X + HeroColumnSize.X + 1.0
				&& LevelUpStart.Y + LevelUpSize.Y <= HeroColumnStart.Y + HeroColumnSize.Y + 1.0;
			Check(FString::Printf(TEXT("%s_%d_level_up_button_inside_hero_column"), *CurrentScenario, CurrentWidth), bInsideHero,
				bHeroColumn && bLevelUpRect ? TEXT("Level-up remains inside the left portrait column.") : Missing);
		}
		if (!CurrentScenario.StartsWith(TEXT("employee_window_potential"))) return;
		// [삭제] home_odds_button — 확률표는 잠재 창 헤더로 옮겼고, 직원창의 구 문맥 버튼은 제거됐다.
		// ⑧ 드로어를 열어도 홈 초상은 살아 있어야 한다 — 잠재는 초상을 쓰지 않으니 무대 소유권을 뺏지 않는다
		const AGachaCaptureStage* HeroStage = Window->LiveStage;
		const USceneCaptureComponent2D* HeroCapture = HeroStage ? HeroStage->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
		const bool bHeroHeld = HeroStage && HeroStage->IsHeldBy(Window);
		const bool bHeroPainted = Window->HeroPortraitImage && Window->HeroPortraitImage->IsVisible()
			&& Window->HeroPortraitImage->GetRenderOpacity() > 0.f;
		// manifest 의 capture_active 와 같은 식이어야 한다 — 온디맨드 캡처로 바뀌면 한쪽만 갈린다(R15)
		const bool bHeroCapturing = HeroCapture && HeroCapture->IsActive() && HeroCapture->bCaptureEveryFrame;
		Check(FString::Printf(TEXT("%s_%d_v2_hero_portrait_persists_with_drawer"), *CurrentScenario, CurrentWidth),
			bHeroHeld && bHeroPainted && bHeroCapturing,
			FString::Printf(TEXT("stage_held=%d capture_active=%d portrait_visible=%d opacity=%.2f"), bHeroHeld ? 1 : 0,
				bHeroCapturing ? 1 : 0,
				Window->HeroPortraitImage && Window->HeroPortraitImage->IsVisible() ? 1 : 0,
				Window->HeroPortraitImage ? Window->HeroPortraitImage->GetRenderOpacity() : -1.f));


		// 워크스페이스 내부 바인딩은 friend 가 아니므로 공개 GetWidgetFromName 으로만 조회한다.
		const UPotentialResetWorkspaceWidget* Drawer = Window->PotentialWorkspace;
		const UButtonWidget* ResetButton = Drawer ? Cast<UButtonWidget>(Drawer->GetWidgetFromName(TEXT("ResetButton"))) : nullptr;
		FVector2D ResetStart, ResetSize;
		const bool bReset = LocalRectOf(RootGeometry, ResetButton, ResetStart, ResetSize);
		// v2.1 사다리: 주행동 96 (구 154 는 라이트 드로어 시절 값)
		Check(FString::Printf(TEXT("%s_%d_potential_drawer_reset_96"), *CurrentScenario, CurrentWidth),
			bReset && ResetSize.Y >= 95.5,
			bReset ? FString::Printf(TEXT("ResetButton height %.1f"), ResetSize.Y) : Missing);
		// 300 타일 = 그림+이름+보유 수+상승 확률 칩. 폭은 열을 셋으로 나눈 값이라 가장 좁은 2304 에서 하한을 본다
		bool bCardCells = Drawer != nullptr;
		double SmallestCellWidth = 0.0, SmallestCellHeight = 0.0;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const UWidget* CardCell = Drawer ? Drawer->GetWidgetFromName(*FString::Printf(TEXT("CardCell%d"), Index)) : nullptr;
			FVector2D CellStart, CellSize;
			if (!LocalRectOf(RootGeometry, CardCell, CellStart, CellSize)) { bCardCells = false; break; }
			SmallestCellWidth = Index == 0 ? CellSize.X : FMath::Min(SmallestCellWidth, CellSize.X);
			SmallestCellHeight = Index == 0 ? CellSize.Y : FMath::Min(SmallestCellHeight, CellSize.Y);
		}
		Check(FString::Printf(TEXT("%s_%d_potential_inline_card_cell"), *CurrentScenario, CurrentWidth),
			bCardCells && SmallestCellWidth >= 140.0 && SmallestCellHeight >= 299.0,
			bCardCells ? FString::Printf(TEXT("smallest CardCell %.1f x %.1f"), SmallestCellWidth, SmallestCellHeight) : Missing);
		if (CurrentScenario != TEXT("employee_window_potential_empty")) return;
		// 0장에서도 고른 명함은 "선택됨"으로 그려지고, 재설정 자리는 막힌 버튼이 아니라 그 명함 구매가 된다.
		// 선택 표시는 입력값이 아니라 실제 렌더 대상(체크 배지) 가시성으로 판정한다.
		const UWidget* FirstCheck = Drawer ? Drawer->GetWidgetFromName(TEXT("CardCheck0")) : nullptr;
		const UWidget* CostChip = Drawer ? Drawer->GetWidgetFromName(TEXT("ResetCostChip")) : nullptr;
		const UWidget* ShopIcon = Drawer ? Drawer->GetWidgetFromName(TEXT("ResetShopIcon")) : nullptr;
		const UCommonTextBlock* ResetLabel = Drawer ? Cast<UCommonTextBlock>(Drawer->GetWidgetFromName(TEXT("ResetLabelText"))) : nullptr;
		const bool bPurchaseLabel = ResetLabel && !ResetLabel->GetText().EqualTo(NSLOCTEXT("PotentialResetWorkspace", "ResetAction", "재설정"))
			&& ResetLabel->GetText().ToString().EndsWith(TEXT("구매"));
		Check(FString::Printf(TEXT("%s_%d_potential_empty_offers_purchase_and_keeps_selection"), *CurrentScenario, CurrentWidth),
			ResetButton && ResetButton->GetIsEnabled() && FirstCheck && FirstCheck->IsVisible()
			&& CostChip && !CostChip->IsVisible() && ShopIcon && ShopIcon->IsVisible() && bPurchaseLabel,
			ResetButton ? FString::Printf(TEXT("reset enabled=%d, check visible=%d, cost chip=%d, shop icon=%d, label=%s"),
				ResetButton->GetIsEnabled() ? 1 : 0, FirstCheck && FirstCheck->IsVisible() ? 1 : 0, CostChip && CostChip->IsVisible() ? 1 : 0,
				ShopIcon && ShopIcon->IsVisible() ? 1 : 0, ResetLabel ? *ResetLabel->GetText().ToString() : TEXT("missing")) : Missing);
	}

	// 재설정 연출 프레임 — 데이터를 바꾸지 않고 지금 직원으로 연출만 세운다. 시점은 승인 시안의 시간표 기준
	bool SeekPotentialRerollFx()
	{
		UPotentialResetWorkspaceWidget* Drawer = EmbeddedWindow.IsValid() ? EmbeddedWindow->PotentialWorkspace.Get() : nullptr;
		if (!Drawer || !CurrentScenario.StartsWith(TEXT("employee_window_potential_fx"))) return false;
		if (UPotentialTierUpOverlayWidget* Overlay = TierUpOverlayFixture.Get())
		{
			// 전체 화면 등급 상승 — 0.2초 = 칼끝이 꽂히고 띠가 열리는 순간, 0.6초 = 띠·글자가 자리 잡은 유지 구간
			Overlay->DebugSeek(CurrentScenario.EndsWith(TEXT("_hit")) ? .2f : .6f);
			FString OverlayDetail;
			Check(FString::Printf(TEXT("%s_%d_tierup_overlay_live"), *CurrentScenario, CurrentWidth), Overlay->DebugIsLive(OverlayDetail), OverlayDetail);
			return false;
		}
		if (CurrentScenario.EndsWith(TEXT("_lines"))) Drawer->DebugSeekRerollFx(false, .18f, 0);
		else Drawer->DebugSeekRerollFx(false, .36f, 1);
		const UWidget* Glow = Drawer->GetWidgetFromName(TEXT("PotLineGlow0"));
		const UWidget* Shine = Drawer->GetWidgetFromName(TEXT("PotLineShine0"));
		FString Detail;
		bool bLive = false;
		if (CurrentScenario.EndsWith(TEXT("_lines")))
		{
			bLive = Shine && Shine->GetRenderOpacity() > .3f;
			Detail = FString::Printf(TEXT("shine opacity %.2f"), Shine ? Shine->GetRenderOpacity() : -1.f);
		}
		else
		{
			bLive = Glow && Glow->GetRenderOpacity() > .3f;
			Detail = FString::Printf(TEXT("glow opacity %.2f"), Glow ? Glow->GetRenderOpacity() : -1.f);
		}
		Check(FString::Printf(TEXT("%s_%d_reroll_fx_layers_live"), *CurrentScenario, CurrentWidth), bLive, Detail);
		return true;
	}

	void EndPotentialRerollFx()
	{
		UPotentialResetWorkspaceWidget* Drawer = EmbeddedWindow.IsValid() ? EmbeddedWindow->PotentialWorkspace.Get() : nullptr;
		if (!Drawer) return;
		Drawer->DebugEndRerollFx();
		FString Detail;
		const bool bRest = Drawer->DebugIsRerollFxAtRest(Detail);
		Check(FString::Printf(TEXT("%s_%d_reroll_fx_returns_to_rest"), *CurrentScenario, CurrentWidth),
			bRest && Detail.StartsWith(TEXT("canvas=1 linelayers=15")), Detail);
	}

	// R2 — 드로어를 연 채 위에 패널이 쌓였다 닫히는 경로(비활성 → 재활성)에서도 무대를 되찾아야 한다.
	// 이 검사는 창 상태를 바꾸므로(등장 스태거 재시작) 다른 모든 검사 뒤에 마지막으로 돈다.
	void CheckHeroPortraitReactivate()
	{
		UEmployeeWindowWidget* Window = EmbeddedWindow.Get();
		// 연출 프레임은 같은 창 상태의 반복이라 재활성 검사는 기본 잠재 장면에서만 본다
		if (!Window || !CurrentScenario.StartsWith(TEXT("employee_window_potential")) || CurrentScenario.Contains(TEXT("_fx_"))) return;
		Window->DeactivateWidget();
		Window->ActivateWidget();
		const AGachaCaptureStage* RestoredStage = Window->LiveStage;
		const USceneCaptureComponent2D* RestoredCapture = RestoredStage ? RestoredStage->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
		const bool bRestoredHeld = RestoredStage && RestoredStage->IsHeldBy(Window);
		const bool bRestoredCapturing = RestoredCapture && RestoredCapture->IsActive() && RestoredCapture->bCaptureEveryFrame;
		const float RestoredOpacity = Window->HeroPortraitImage ? Window->HeroPortraitImage->GetRenderOpacity() : -1.f;
		// 재획득 직후 한 프레임은 워밍업 페이드가 0 에서 출발한다 — 프레임을 돌리지 않고 즉시 읽으므로
		// opacity 0 은 정상 과도 상태다. 페이드가 예약돼 있으면(warmup >= 0) 초상은 살아 있다.
		// 페이지는 판정하지 않는다 — NativeOnDeactivated 가 페이지를 Home 으로 되돌리는 것은 무대 소유권과
		// 별개의 UX 축이고(드로어가 닫힌 채 돌아온다) 룰링 대기 중이다. 값은 detail 에 남긴다.
		const bool bFadeScheduled = Window->LiveViewWarmupElapsed >= 0.f;
		Check(FString::Printf(TEXT("%s_%d_v2_hero_portrait_survives_reactivate"), *CurrentScenario, CurrentWidth),
			bRestoredHeld && bRestoredCapturing && (RestoredOpacity > 0.f || bFadeScheduled),
			FString::Printf(TEXT("after DeactivateWidget->ActivateWidget: page=%d (0=Home - deactivate resets the page, separate UX issue) stage_held=%d capture_active=%d opacity=%.2f warmup=%.2f"),
				static_cast<int32>(Window->CurrentEmployeePage), bRestoredHeld ? 1 : 0, bRestoredCapturing ? 1 : 0,
				RestoredOpacity, Window->LiveViewWarmupElapsed));
	}

	// 룰링 b — 캡처 해상도로 그린 직후, 그 폭으로 생산의 빈자리 재계산을 돌린다.
	// 엔트리 재생성은 STableViewBase::Tick 안에 있고 FWidgetRenderer 는 Paint 만 하므로,
	// 아이템을 바꾼 뒤 띠의 슬레이트 위젯을 한 번 틱해야 다음 draw 가 새 타일을 그린다.
	void ResyncRosterForCaptureSize()
	{
		UEmployeeWindowWidget* Window = EmbeddedWindow.Get();
		if (!Window || !Window->RosterListView || !Window->EmployeeManager) return;
		Window->UpdateColumnWidths(LogicalCaptureSize().X);
		const int32 OwnedCount = Window->EmployeeManager->GetEmployeesInBuilding(Window->BuildingIndex).Num()
			+ Window->EmployeeManager->GetUnassignedEmployeesInBuilding(Window->BuildingIndex).Num();
		const int32 Capacity = Window->EmployeeManager->GetBuildingEmployeeCapacity(Window->BuildingIndex, /*bLogIfZero*/ false);
		SyncEmployeeRosterHireTilesForCapture(Window->RosterListView, Window,
			FEmployeeRosterRules::HireSlotCount(OwnedCount, Capacity));
		if (const TSharedPtr<SWidget> StripSlate = Window->RosterListView->GetCachedWidget())
		{
			StripSlate->Tick(StripSlate->GetCachedGeometry(), FSlateApplication::Get().GetCurrentTime(), 0.f);
		}
		// 성장 카드도 캡처 폭에서 한 번 틱해 레벨/비용 텍스트의 배치를 갱신한다.
		if (Window->DisciplineCard)
		{
			if (const TSharedPtr<SWidget> CardSlate = Window->DisciplineCard->GetCachedWidget())
			{
				CardSlate->Tick(CardSlate->GetCachedGeometry(), FSlateApplication::Get().GetCurrentTime(), 0.f);
			}
		}
	}

	bool PrepareScenario()
	{
		RestoreBusinessCardFixture();
		RestoreStressFixture();
		CloseEmbeddedWindow();
		if (ShopWindow.IsValid()) { ShopWindow->DeactivateWidget(); ShopWindow->RemoveFromParent(); ShopWindow.Reset(); }
		auto* Work = Workspace.Get();
		if (!bPotentialValidation && !bTrainingValidation && !Work) return false;
		const auto* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (!Tables) return false;
		CurrentScenario = Scenarios[CaptureIndex % Scenarios.Num()];
		const FIntPoint CaptureSize = CaptureSizes[CaptureIndex / Scenarios.Num()];
		CurrentWidth = CaptureSize.X;
		CurrentHeight = CaptureSize.Y;
		CurrentDPIScale = bMobileValidation ? GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(CaptureSize) : 1.f;
		EEquipmentWorkspaceView View = EEquipmentWorkspaceView::Employee;
		if (CurrentScenario == TEXT("inventory") || CurrentScenario.StartsWith(TEXT("bulk")) || CurrentScenario == TEXT("results")) View = EEquipmentWorkspaceView::Inventory;
		else if (CurrentScenario.StartsWith(TEXT("gacha")) || CurrentScenario == TEXT("ticket_quote") || CurrentScenario == TEXT("odds") || CurrentScenario == TEXT("reveal")) View = EEquipmentWorkspaceView::Gacha;
		if (Work)
		{
			Work->DismissDialog();
			Work->Configure(EmployeeId, View);
		}
		ActiveWorkspace = Work;
		RenderRoot = Panel.Get();
		if (CurrentScenario.StartsWith(TEXT("gacha")))
		{
			const int32 Tier = CurrentScenario.Contains(TEXT("premium")) ? 2 : CurrentScenario.Contains(TEXT("advanced")) ? 1 : 0;
			Work->HandleTicketSelected(nullptr, Tier);
			Work->SetupBanners(); Work->RefreshGacha();
		}
		if (CurrentScenario == TEXT("ticket_quote"))
		{
			FEmployeeEquipmentPullQuote DisplayQuote;
			Work->EquipmentManager->QuotePull(Work->CurrentBannerId, 10, DisplayQuote);
			DisplayQuote.bCanPull = false;
			DisplayQuote.ErrorCode = TEXT("InsufficientTickets");
			DisplayQuote.AvailableTickets = 0;
			DisplayQuote.MissingTickets = 10;
			Work->ShowPullQuote(DisplayQuote, 10);
		}
		if (CurrentScenario == TEXT("shop"))
		{
			Work->SuspendPreview();
			const auto Class = Work->TableManager->GetWidgetClass(EWidgetType::ShopPanel);
			ShopWindow.Reset(Class ? CreateWidget<UShopPanelWidget>(PlayWorld->GetFirstPlayerController(), Class) : nullptr);
			if (!ShopWindow.IsValid()) { Check(TEXT("shop_registered"), false); return false; }
			ShopWindow->AddToViewport(210); ShopWindow->ActivateWidget();
			ShopWindow->SetEquipmentTicketContext(EItemType::EmployeeEquipmentTicketPremium, 10);
			RenderRoot = ShopWindow.Get();
		}
		if (CurrentScenario.StartsWith(TEXT("employee_window_")) || CurrentScenario == TEXT("employee_starforce_reference"))
		{
			if (Work) Work->SuspendPreview();
			const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeWindow);
			EmbeddedWindow.Reset(Class ? CreateWidget<UEmployeeWindowWidget>(PlayWorld->GetFirstPlayerController(), Class) : nullptr);
			if (!EmbeddedWindow.IsValid()) { Check(TEXT("embedded_window_class"), false); return false; }
			EmbeddedWindow->AddToViewport(210);
			EmbeddedWindow->OpenWithContext(EmployeeId, true);
			EmbeddedWindow->ActivateWidget();
			if (!bTrainingValidation && !EmbeddedWindow->HeroEquipmentSlot0)
			{
				Check(CurrentScenario + TEXT("_navigation_binding"), false);
				return false;
			}
			const EEmployeeWindowPage Page = CurrentScenario == TEXT("employee_window_training") || CurrentScenario == TEXT("employee_window_populated")
				|| CurrentScenario == TEXT("employee_starforce_reference") || CurrentScenario == TEXT("employee_window_lean_home")
				|| CurrentScenario == TEXT("employee_window_stress") ? EEmployeeWindowPage::Home
				: CurrentScenario.StartsWith(TEXT("employee_window_potential")) ? EEmployeeWindowPage::Home
				: EEmployeeWindowPage::Equipment;
			// 명함 수량은 잠재 드로어의 확률/버튼 상태와 홈 알림 도트를 결정하므로 페이지 전환 전에 맞춘다.
			if (CurrentScenario == TEXT("employee_window_potential_empty")) ApplyBusinessCardFixture(0, 0, 0);
			else if (CurrentScenario == TEXT("employee_window_potential") || CurrentScenario == TEXT("employee_window_lean_home")
				|| CurrentScenario.StartsWith(TEXT("employee_window_potential_fx"))) ApplyBusinessCardFixture(12, 2, 0);
			// 스트레스는 3자리 장수까지 밀어 본다 — 비용 칩과 잠재 정보 행이 같은 줄에서 자리를 다툰다
			else if (!bTrainingValidation && CurrentScenario == TEXT("employee_window_stress")) ApplyBusinessCardFixture(999, 999, 999);
			if (CurrentScenario == TEXT("employee_window_stress")) ApplyStressFixture();
			EmbeddedWindow->ShowEmployeePage(Page);
			if (bColumnValidation && Page == EEmployeeWindowPage::Equipment)
			{
				EmbeddedWindow->OpenEquipmentItem(EmployeeId, ItemIds[0]);
				Check(TEXT("column_capture_selected_owned_item_in_detail_host"), EmbeddedWindow->InlineDetail
					&& EmbeddedWindow->InlineDetail->GetParent() == EmbeddedWindow->EquipDetailHost);
			}
			Check(CurrentScenario + TEXT("_page_content"), EmbeddedWindow->CurrentEmployeePage == Page
				&& (bTrainingValidation || (EmbeddedWindow->PotentialWorkspace && EmbeddedWindow->PotentialWorkspace->GetParent() == EmbeddedWindow->PotentialHost
					&& EmbeddedWindow->GetWidgetFromName(TEXT("LayoutHBox")) && EmbeddedWindow->GetWidgetFromName(TEXT("LayoutHBox"))->IsVisible())));
			RenderRoot = EmbeddedWindow.Get();
			// 창은 더 이상 워크스페이스를 만들지 않는다(칼럼 교체, 2026-09-25) — 여기서 덮어쓰지 않고
			// 단독 패널이 만든 것을 그대로 쓴다. 창 쪽 계약은 CheckEquipmentColumn 이 따로 본다.
			if (!bPotentialValidation && !bTrainingValidation && !Work) { Check(CurrentScenario + TEXT("_workspace"), false); return false; }
			ActiveWorkspace = Work;
			if (CurrentScenario == TEXT("employee_starforce_reference"))
			{
				EmbeddedWindow->OnEnhanceClicked();
				Check(TEXT("starforce_reference_uses_original_employee_enhancement_modal"), EmbeddedWindow->OpenEnhanceModal.IsValid()
					&& !EmbeddedWindow->OpenEnhanceModal->IsActivated() && !Work->Dialog
					&& EmbeddedWindow->CurrentEmployeePage == EEmployeeWindowPage::Home);
			}
			if (CurrentScenario == TEXT("employee_window_gacha")) Work->HandleGacha();
			if (bInlineValidation && Page == EEmployeeWindowPage::Equipment && CurrentScenario != TEXT("employee_window_gacha"))
			{
				Work->ClearInspectionSelection();
				if (CurrentScenario != TEXT("employee_window_empty"))
				{
					if (ItemIds.IsEmpty()) { Check(CurrentScenario + TEXT("_fixture"), false); return false; }
					FGuid Pick = ItemIds[0];
					// 교체 비교는 '지금 안 낀 것을, 뭔가 끼어 있는 부위에' 골라야 나온다.
					// ItemIds[0] 은 장착품이라 '변화 없음' 만 찍힌다.
					// 외형은 장착 중일 때만 버튼이 뜬다 — 이 시나리오는 낀 것을 골라야 한다.
					if (CurrentScenario == TEXT("employee_window_appearance"))
					{
						bool bWornFound = false;
						for (const FGuid& Candidate : ItemIds)
						{
							const auto* Instance = Work->EquipmentManager->FindInstance(Candidate);
							const auto* Def = Instance ? Work->TableManager->FindEmployeeEquipmentDefinition(Instance->DefinitionId) : nullptr;
							if (!Def) continue;
							const auto* Worn = Work->EquipmentManager->GetEquippedInstance(EmployeeId, Def->EquipmentSlot);
							if (Worn && Worn->InstanceId == Candidate) { Pick = Candidate; bWornFound = true; break; }
						}
						Check(TEXT("appearance_fixture_has_worn_item"), bWornFound);
					}
					if (CurrentScenario == TEXT("employee_window_swap"))
					{
						bool bFound = false;
						for (const FGuid& Candidate : ItemIds)
						{
							const auto* Instance = Work->EquipmentManager->FindInstance(Candidate);
							const auto* Def = Instance ? Work->TableManager->FindEmployeeEquipmentDefinition(Instance->DefinitionId) : nullptr;
							if (!Def) continue;
							const auto* Worn = Work->EquipmentManager->GetEquippedInstance(EmployeeId, Def->EquipmentSlot);
							if (Worn && Worn->InstanceId != Candidate) { Pick = Candidate; bFound = true; break; }
						}
						Check(TEXT("swap_fixture_has_unworn_item_in_occupied_slot"), bFound);
					}
					Work->OpenEquipmentItem(Pick);
					if (CurrentScenario == TEXT("employee_window_detail")) Work->HandleInspectorDetail();
					// 상세 창의 [외형] 이 여는 창을 실제로 띄워 찍는다.
					if (CurrentScenario == TEXT("employee_window_appearance")) Work->ShowAppearancePicker();
					if (CurrentScenario == TEXT("employee_window_enhance")) Work->HandleInspectorEnhance();
				}
			}
		}
		if (CurrentScenario == TEXT("comparison") || CurrentScenario == TEXT("enhance"))
		{
			if (ItemIds.IsEmpty()) { Check(CurrentScenario + TEXT("_fixture"), false, TEXT("No owned equipment. No items are granted by capture.")); return false; }
			Work->SelectedInstanceId = ItemIds[0];
			if (CurrentScenario == TEXT("comparison")) Work->ShowDetail(ItemIds[0]); else Work->ShowEnhance();
		}
		else if (CurrentScenario.StartsWith(TEXT("bulk")))
		{
			Work->ShowBulk();
			TArray<UObject*> Selection;
			for (const auto& Entry : Work->DialogEntries) { if (Selection.Num() == 2) break; Selection.Add(Entry.Get()); }
			if (Work->Dialog) Work->Dialog->RestoreSelectedItems(Selection);
			Work->UpdateBulkSelection();
			if (CurrentScenario == TEXT("bulk_confirm"))
			{
				if (Selection.IsEmpty()) { Check(TEXT("bulk_confirm_fixture"), false, TEXT("No dismantle-eligible owned equipment.")); return false; }
				Work->ShowBulkConfirm();
				Work->ShowBulk();
				Check(TEXT("bulk_confirmation_back_preserves_selection"), Work->Dialog && Work->Dialog->GetSelectedItems().Num() == Selection.Num());
				Work->ShowBulkConfirm();
			}
		}
		else if (CurrentScenario == TEXT("odds")) Work->ShowOdds();
		else if (CurrentScenario == TEXT("results") || CurrentScenario == TEXT("reveal"))
		{
			if (ItemIds.IsEmpty()) { Check(CurrentScenario + TEXT("_fixture"), false, TEXT("No owned equipment; result rendering reuses existing items without a pull.")); return false; }
			Work->LastResults.Empty();
			for (int32 Index = 0; Index < FMath::Min(10, ItemIds.Num()); ++Index) Work->LastResults.Add(ItemIds[Index]);
			if (CurrentScenario == TEXT("results")) Work->ShowResults();
			else
			{
				// 공개 연출은 박자로 10회 9초 남짓 ― 결과 상태를 바로 깔아 같은 2.2초 안에 정리된 화면을 찍는다
				FGachaRevealAwardContext Award; Award.bOwnedBefore.Init(true, Work->LastResults.Num());
				Work->ShowResultReveal(Award, Work->CurrentTicketType, Work->CurrentEmployeeId);
				if (Work->RevealPresentation) Work->RevealPresentation->DebugSkipToResult();
			}
		}
		Surface = SNew(SOverlay);
		Surface->AddSlot()[RenderRoot->TakeWidget()];
		if (EmbeddedWindow.IsValid() && EmbeddedWindow->OpenEnhanceModal.IsValid())
			Surface->AddSlot()[EmbeddedWindow->OpenEnhanceModal->TakeWidget()];
		if (Work && Work->Dialog) Surface->AddSlot()[Work->Dialog->TakeWidget()];
		if (Work && Work->RevealPresentation) Surface->AddSlot()[Work->RevealPresentation->TakeWidget()];
		TierUpOverlayFixture.Reset();
		if (CurrentScenario.Contains(TEXT("_fx_tierup")))
		{
			UGameInstance* Instance = PlayWorld.IsValid() ? PlayWorld->GetGameInstance() : nullptr;
			UTableManagerSubsystem* TableMgr = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
			const TSubclassOf<UUserWidget> OverlayClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PotentialTierUpOverlay) : nullptr;
			UPotentialTierUpOverlayWidget* Overlay = OverlayClass && OverlayClass->IsChildOf(UPotentialTierUpOverlayWidget::StaticClass())
				? CreateWidget<UPotentialTierUpOverlayWidget>(PlayWorld.Get(), OverlayClass) : nullptr;
			Check(CurrentScenario + TEXT("_overlay_registered"), Overlay != nullptr, TEXT("DT_WidgetClass PotentialTierUpOverlay"));
			if (Overlay)
			{
				TierUpOverlayFixture.Reset(Overlay);
				Surface->AddSlot()[Overlay->TakeWidget()];
				Overlay->Play(ELootBoxRarity::Rare, ELootBoxRarity::Epic, false);
				Overlay->DebugSeek(0.f);
			}
		}
		if (bMobileValidation && CurrentScenario == TEXT("employee_window_populated")) ApplyPopulatedEmployeeFixture(true);
		Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(CurrentWidth, CurrentHeight), TF_Bilinear, false));
		PreparedAtSeconds = FPlatformTime::Seconds();
		CaptureReadyAtSeconds = PreparedAtSeconds + (CurrentScenario == TEXT("reveal") ? 2.2 : .65);
		PreparedFrame = GFrameCounter;
		return Target.IsValid();
	}

	void GatherWidget(UWidget* Widget, const FGeometry& RootGeometry, bool bParentVisible, bool bParentInteractive, TSet<UWidget*>& Visited, TArray<TSharedPtr<FJsonValue>>& Values)
	{
		if (!Widget || Visited.Contains(Widget)) return;
		Visited.Add(Widget);
		const ESlateVisibility Visibility = Widget->GetVisibility();
		const bool bVisible = bParentVisible && Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden;
		const bool bInteractive = bParentInteractive && Visibility != ESlateVisibility::HitTestInvisible;
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D Position = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D End = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
		const FVector2D Size = End - Position;
		auto Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("name"), Widget->GetName()); Value->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
		Value->SetStringField(TEXT("path"), Widget->GetPathName());
		Value->SetNumberField(TEXT("x"), Position.X); Value->SetNumberField(TEXT("y"), Position.Y); Value->SetNumberField(TEXT("width"), Size.X); Value->SetNumberField(TEXT("height"), Size.Y);
		Value->SetBoolField(TEXT("visible"), bVisible);
		if (bMobileValidation)
		{
			Value->SetNumberField(TEXT("screen_x"), Position.X * CurrentDPIScale);
			Value->SetNumberField(TEXT("screen_y"), Position.Y * CurrentDPIScale);
			Value->SetNumberField(TEXT("screen_width"), Size.X * CurrentDPIScale);
			Value->SetNumberField(TEXT("screen_height"), Size.Y * CurrentDPIScale);
			Value->SetNumberField(TEXT("render_opacity"), Widget->GetRenderOpacity());
			if (const UTextBlock* Label = Cast<UTextBlock>(Widget))
			{
				const FSlateFontInfo Font = Label->GetFont();
				const float LayoutScale = Geometry.GetAccumulatedLayoutTransform().GetScale();
				const FString LabelText = Label->GetText().ToString();
				TArray<FString> Lines;
				LabelText.ParseIntoArrayLines(Lines, true);
				const FString FirstLine = Lines.IsEmpty() ? FString() : Lines[0];
				const FVector2D Measured = FirstLine.IsEmpty() ? FVector2D::ZeroVector
					: FVector2D(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(FirstLine, Font, LayoutScale));
				Value->SetStringField(TEXT("text"), LabelText);
				Value->SetNumberField(TEXT("font_size"), Font.Size);
				Value->SetNumberField(TEXT("font_layout_scale"), LayoutScale);
				Value->SetNumberField(TEXT("nominal_font_size_px"), Font.Size * LayoutScale);
				Value->SetNumberField(TEXT("measured_line_width_px"), Measured.X);
				Value->SetNumberField(TEXT("measured_line_height_px"), Measured.Y);
			}
		}
		if (const auto* TileList = Cast<UTileView>(Widget))
		{
			Value->SetNumberField(TEXT("entry_width"), TileList->GetEntryWidth());
			Value->SetNumberField(TEXT("entry_height"), TileList->GetEntryHeight());
			Value->SetNumberField(TEXT("horizontal_spacing"), TileList->GetHorizontalEntrySpacing());
			Value->SetNumberField(TEXT("vertical_spacing"), TileList->GetVerticalEntrySpacing());
		}
		const bool bTouch = bVisible && bInteractive && Visibility != ESlateVisibility::SelfHitTestInvisible && (Widget->IsA<UCommonButtonBase>() || Widget->IsA<UButton>() || Widget->IsA<UComboBoxString>());
		Value->SetBoolField(TEXT("touch_target"), bTouch);
		if (bTouch)
		{
			Value->SetBoolField(TEXT("minimum_154"), Size.X >= 153.5f && Size.Y >= 153.5f);
			const FVector2D LogicalSize = LogicalCaptureSize();
			Value->SetBoolField(TEXT("within_canvas"), Position.X >= -.5f && Position.Y >= -.5f && End.X <= LogicalSize.X + .5f && End.Y <= LogicalSize.Y + .5f);
		}
		Values.Add(MakeShared<FJsonValueObject>(Value));
		if (auto* UserWidget = Cast<UUserWidget>(Widget)) if (UserWidget->WidgetTree) GatherWidget(UserWidget->WidgetTree->RootWidget, RootGeometry, bVisible, bInteractive, Visited, Values);
		if (auto* Switcher = Cast<UWidgetSwitcher>(Widget)) GatherWidget(Switcher->GetActiveWidget(), RootGeometry, bVisible, bInteractive, Visited, Values);
		else if (auto* PanelWidget = Cast<UPanelWidget>(Widget)) for (UWidget* Child : PanelWidget->GetAllChildren()) GatherWidget(Child, RootGeometry, bVisible, bInteractive, Visited, Values);
		if (auto* List = Cast<UListViewBase>(Widget)) for (UUserWidget* Entry : List->GetDisplayedEntryWidgets()) GatherWidget(Entry, RootGeometry, bVisible, bInteractive, Visited, Values);
	}

	void Capture()
	{
		const FString FileName = FString::Printf(TEXT("%s_%dx%d.png"), *CurrentScenario, CurrentWidth, CurrentHeight);
		const bool bSaved = SaveEquipmentUIPNG(Target.Get(), FPaths::Combine(OutputDirectory, FileName));
		auto Record = MakeShared<FJsonObject>();
		Record->SetNumberField(TEXT("prepared_frame"), static_cast<double>(PreparedFrame));
		Record->SetNumberField(TEXT("captured_frame"), static_cast<double>(GFrameCounter));
		Record->SetNumberField(TEXT("seconds_since_preparation"), FPlatformTime::Seconds() - PreparedAtSeconds);
		Record->SetStringField(TEXT("scenario"), CurrentScenario); Record->SetStringField(TEXT("png"), FileName); Record->SetBoolField(TEXT("saved"), bSaved); Record->SetNumberField(TEXT("width"), CurrentWidth); Record->SetNumberField(TEXT("height"), CurrentHeight);
		Record->SetNumberField(TEXT("dpi_scale"), CurrentDPIScale);
		Record->SetNumberField(TEXT("logical_width"), LogicalCaptureSize().X);
		Record->SetNumberField(TEXT("logical_height"), LogicalCaptureSize().Y);
		if (bMobileValidation && EmbeddedWindow.IsValid())
		{
			const FVector2D RootSize = EmbeddedWindow->GetCachedGeometry().GetLocalSize();
			Check(FString::Printf(TEXT("%s_%d_%d_dpi_layout_matches_viewport"), *CurrentScenario, CurrentWidth, CurrentHeight), RootSize.Equals(LogicalCaptureSize(), .5f));
			bool bLockedLinesReadable = true;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const UCommonTextBlock* Name = Cast<UCommonTextBlock>((EmbeddedWindow->PotentialWorkspace ? EmbeddedWindow->PotentialWorkspace->GetWidgetFromName(*FString::Printf(TEXT("PotLineName%d"), Index)) : nullptr));
				const UWidget* Line = (EmbeddedWindow->PotentialWorkspace ? EmbeddedWindow->PotentialWorkspace->GetWidgetFromName(*FString::Printf(TEXT("PotLine%d"), Index)) : nullptr);
				if (Name && Name->GetText().ToString() == TEXT("잠김")) bLockedLinesReadable &= Line && FMath::IsNearlyEqual(Line->GetRenderOpacity(), 1.f);
			}
			Check(FString::Printf(TEXT("%s_%d_%d_locked_potential_avoids_parent_text_fade"), *CurrentScenario, CurrentWidth, CurrentHeight), bLockedLinesReadable);
			if (CurrentScenario == TEXT("employee_window_populated"))
			{
				int32 BonusCount = 0, PotentialCount = 0;
				for (int32 Index = 0; Index < static_cast<int32>(EEmployeeStatIndex::Count); ++Index)
				{
					const UCommonTextBlock* Bonus = Cast<UCommonTextBlock>(EmbeddedWindow->GetWidgetFromName(*FString::Printf(TEXT("HeroStatBonus%d"), Index)));
					if (Bonus && !Bonus->GetText().IsEmpty() && Bonus->IsVisible() && (!Bonus->GetParent() || Bonus->GetParent()->IsVisible())) ++BonusCount;
				}
				for (int32 Index = 0; Index < 3; ++Index)
				{
					const UCommonTextBlock* Name = Cast<UCommonTextBlock>((EmbeddedWindow->PotentialWorkspace ? EmbeddedWindow->PotentialWorkspace->GetWidgetFromName(*FString::Printf(TEXT("PotLineName%d"), Index)) : nullptr));
					const UCommonTextBlock* Value = Cast<UCommonTextBlock>((EmbeddedWindow->PotentialWorkspace ? EmbeddedWindow->PotentialWorkspace->GetWidgetFromName(*FString::Printf(TEXT("PotLineValue%d"), Index)) : nullptr));
					const UWidget* Line = (EmbeddedWindow->PotentialWorkspace ? EmbeddedWindow->PotentialWorkspace->GetWidgetFromName(*FString::Printf(TEXT("PotLine%d"), Index)) : nullptr);
					if (Name && Value && Line && !Name->GetText().IsEmpty() && Value->GetText().ToString().StartsWith(TEXT("+"))
						&& FMath::IsNearlyEqual(Line->GetRenderOpacity(), 1.f)) ++PotentialCount;
				}
				const int32 RosterItems = EmbeddedWindow->RosterListView ? EmbeddedWindow->RosterListView->GetNumItems() : 0;
				const int32 DisplayedItems = EmbeddedWindow->RosterListView ? EmbeddedWindow->RosterListView->GetDisplayedEntryWidgets().Num() : 0;
				auto FixtureRecord = MakeShared<FJsonObject>();
				FixtureRecord->SetNumberField(TEXT("visible_bonus_count"), BonusCount);
				FixtureRecord->SetNumberField(TEXT("active_potential_count"), PotentialCount);
				FixtureRecord->SetNumberField(TEXT("roster_items"), RosterItems);
				FixtureRecord->SetNumberField(TEXT("displayed_roster_items"), DisplayedItems);
				Record->SetObjectField(TEXT("populated_fixture"), FixtureRecord);
				Check(FString::Printf(TEXT("mobile_%d_%d_populated_bonus_potential_and_roster_present"), CurrentWidth, CurrentHeight),
					BonusCount == 5 && PotentialCount == 3 && RosterItems >= 24 && DisplayedItems > 0 && DisplayedItems < RosterItems);
			}
		}
		if (EmbeddedWindow.IsValid() && EmbeddedWindow->CurrentEmployeePage != EEmployeeWindowPage::Home
			&& ActiveWorkspace.IsValid() && ActiveWorkspace->CurrentView != EEquipmentWorkspaceView::Gacha)
		{
			// [삭제] *_drawer_keeps_hero_visible — 드로어가 우측 860 이라 왼쪽에 히어로가 남는다는 전제였다.
			// 장비/잠재가 각자 선 창이 되면서 창이 화면을 덮으므로 "뒤에 히어로가 보인다"는 성립하지 않는다.
			// 초상 자체의 생존은 panel_preserves_home_and_reuses_hero_portrait 가 계속 검증한다.
		}
		// 잠재 드로어는 초상을 쓰지 않으므로 홈 무대를 그대로 들고 있어야 한다 — 그래서 여기서도 기록한다
		if (EmbeddedWindow.IsValid() && EmbeddedWindow->CurrentEmployeePage == EEmployeeWindowPage::Home)
		{
			auto HeroPreview = MakeShared<FJsonObject>();
			const auto* HeroStage = EmbeddedWindow->LiveStage;
			const auto* HeroCapture = HeroStage ? HeroStage->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
			HeroPreview->SetBoolField(TEXT("stage_held_by_employee_window"), HeroStage && HeroStage->IsHeldBy(EmbeddedWindow.Get()));
			HeroPreview->SetNumberField(TEXT("portrait_opacity"), EmbeddedWindow->HeroPortraitImage ? EmbeddedWindow->HeroPortraitImage->GetRenderOpacity() : -1.f);
			HeroPreview->SetBoolField(TEXT("capture_active"), HeroCapture && HeroCapture->IsActive() && HeroCapture->bCaptureEveryFrame);
			Record->SetObjectField(TEXT("hero_preview"), HeroPreview);
		}
		if (const auto* Work = ActiveWorkspace.Get())
		{
			auto Preview = MakeShared<FJsonObject>();
			const auto* Stage = IsValid(Work->PreviewStage) ? Work->PreviewStage.Get() : nullptr;
			const auto* SceneCapture = Stage ? Stage->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
			Preview->SetStringField(TEXT("workspace"), Work->GetPathName());
			Preview->SetBoolField(TEXT("enabled"), Work->bPreviewEnabled);
			Preview->SetBoolField(TEXT("stage_valid"), Stage && !Stage->IsActorBeingDestroyed());
			Preview->SetBoolField(TEXT("stage_held_by_workspace"), Stage && Stage->IsHeldBy(Work));
			Preview->SetNumberField(TEXT("warmup_seconds"), Work->PreviewWarmup);
			Preview->SetNumberField(TEXT("portrait_opacity"), Work->GetPreviewImage() ? Work->GetPreviewImage()->GetRenderOpacity() : -1.f);
			Preview->SetBoolField(TEXT("uses_home_portrait"), Work->DrawerPortraitTarget.IsValid());
			Preview->SetBoolField(TEXT("capture_active"), SceneCapture && SceneCapture->IsActive() && SceneCapture->bCaptureEveryFrame);
			Preview->SetNumberField(TEXT("show_only_actor_count"), SceneCapture ? SceneCapture->ShowOnlyActors.Num() : 0);
			Record->SetObjectField(TEXT("employee_preview"), Preview);
			if (CurrentScenario == TEXT("employee_window_selected") && Work->IsDrawerPresentation())
			{
				const UWidget* BodyScroll = Work->GetWidgetFromName(TEXT("InspectorBodyScroll"));
				const UCommonTextBlock* BodyText = Work->InspectorBodyText.Get();
				TArray<FString> BodyLines;
				if (BodyText) BodyText->GetText().ToString().ParseIntoArrayLines(BodyLines, true);
				const FString FirstLine = BodyLines.IsEmpty() ? FString() : BodyLines[0];
				const FVector2D Available = BodyScroll ? BodyScroll->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
				const double TextWidth = BodyText ? BodyText->GetCachedGeometry().GetLocalSize().X : 0.0;
				const FVector2D Measured = BodyText && !FirstLine.IsEmpty()
					? FVector2D(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(FirstLine, BodyText->GetFont())) : FVector2D::ZeroVector;
				const bool bFirstLineFits = !FirstLine.IsEmpty() && Measured.X > 0.0 && Measured.Y > 0.0
					&& Measured.X <= FMath::Min(Available.X, TextWidth) + .5 && Measured.Y <= Available.Y + .5;
				auto LineRecord = MakeShared<FJsonObject>();
				LineRecord->SetStringField(TEXT("text"), FirstLine);
				LineRecord->SetNumberField(TEXT("measured_width"), Measured.X);
				LineRecord->SetNumberField(TEXT("measured_height"), Measured.Y);
				LineRecord->SetNumberField(TEXT("available_width"), FMath::Min(Available.X, TextWidth));
				LineRecord->SetNumberField(TEXT("available_height"), Available.Y);
				LineRecord->SetBoolField(TEXT("fits_without_scrolling"), bFirstLineFits);
				Record->SetObjectField(TEXT("inspector_first_line"), LineRecord);
				Check(FString::Printf(TEXT("%s_%d_first_stat_line_fits_without_scrolling"), *CurrentScenario, CurrentWidth), bFirstLineFits, FirstLine);
			}
		}
		TArray<TSharedPtr<FJsonValue>> GeometryValues; TSet<UWidget*> Visited;
		const FGeometry& RootGeometry = RenderRoot->GetCachedGeometry();
		GatherWidget(RenderRoot.Get(), RootGeometry, true, true, Visited, GeometryValues);
		if (EmbeddedWindow.IsValid() && EmbeddedWindow->OpenEnhanceModal.IsValid())
		{
			Record->SetStringField(TEXT("reference_modal"), EmbeddedWindow->OpenEnhanceModal->GetClass()->GetPathName());
			GatherWidget(EmbeddedWindow->OpenEnhanceModal.Get(), RootGeometry, true, true, Visited, GeometryValues);
		}
		if (ActiveWorkspace.IsValid() && ActiveWorkspace->Dialog) GatherWidget(ActiveWorkspace->Dialog, RootGeometry, true, true, Visited, GeometryValues);
		if (ActiveWorkspace.IsValid() && ActiveWorkspace->RevealPresentation) GatherWidget(ActiveWorkspace->RevealPresentation, RootGeometry, true, true, Visited, GeometryValues);
		Record->SetArrayField(TEXT("widgets"), GeometryValues); Captures.Add(MakeShared<FJsonValueObject>(Record));
		if (bTrainingValidation) CheckLevelUpCard(EmbeddedWindow.Get(), RootGeometry);
		else if (bPotentialValidation) CheckInlinePotentialGeometry();
		else
		{
			CheckLeanHomeAndPotentialGeometry();
			CheckTextFitOverlapAndPadding();
		}
		CheckHeroPortraitReactivate();
		RestoreBusinessCardFixture();
		UE_LOG(LogTemp, Display, TEXT("EquipmentUI capture %s: %s"), *FileName, bSaved ? TEXT("saved") : TEXT("FAILED"));
	}

	void Finish()
	{
		ClosePotentialShopRoute();
		RestoreBusinessCardFixture();
		RestoreStressFixture();
		CloseEmbeddedWindow();
		if (ShopWindow.IsValid()) { ShopWindow->DeactivateWidget(); ShopWindow->RemoveFromParent(); ShopWindow.Reset(); }
		if (Workspace.IsValid())
		{
			Workspace->DismissDialog(); Workspace->SuspendPreview();
		}
		const auto* Instance = PlayWorld.IsValid() ? PlayWorld->GetGameInstance() : nullptr;
		const auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		if (Equipment && Report->HasField(TEXT("equipment_revision_before")))
			Check(TEXT("equipment_revision_unchanged"), Report->GetNumberField(TEXT("equipment_revision_before")) == Equipment->GetState().Revision);
		if (Panel.IsValid()) { Panel->DeactivateWidget(); Panel->RemoveFromParent(); }
		Report->SetArrayField(TEXT("captures"), Captures); Report->SetArrayField(TEXT("checks"), Checks);
		Report->SetBoolField(TEXT("finished"), true);
		bool bPassed = Captures.Num() == Scenarios.Num() * CaptureSizes.Num();
		for (const auto& Value : Checks) bPassed &= Value->AsObject()->GetBoolField(TEXT("passed"));
		for (const auto& Value : Captures) bPassed &= Value->AsObject()->GetBoolField(TEXT("saved"));
		Report->SetBoolField(TEXT("passed"), bPassed);
		FString Json; FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		FFileHelper::SaveStringToFile(Json, *FPaths::Combine(OutputDirectory, TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Surface.Reset(); Target.Reset(); Renderer.Reset(); Panel.Reset();
		UE_LOG(LogTemp, Display, TEXT("EquipmentUI validation finished: %s"), *OutputDirectory);
	}

	bool Tick(float DeltaSeconds)
	{
		if (!PlayWorld.IsValid() || (!bPotentialValidation && !bTrainingValidation && !Workspace.IsValid()) || CaptureIndex >= Scenarios.Num() * CaptureSizes.Num()) { Finish(); return false; }
		if (ShopRoutePhase != 0) { TickPotentialShopRoute(); return true; }
		if (!bScenarioReady)
		{
			bScenarioReady = PrepareScenario();
			if (!bScenarioReady) { ++CaptureIndex; return true; }
			// 준비 호출 전의 긴 프레임 시간을 새 캡처 무대의 워밍업으로 계산하지 않는다.
			return true;
		}
		if (bMobileValidation && CurrentScenario == TEXT("employee_window_populated")) ApplyPopulatedEmployeeFixture(false);
		if (CurrentScenario == TEXT("employee_window_stress")) ApplyStressFixture();
		Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), CurrentDPIScale, FVector2D(CurrentWidth, CurrentHeight), DeltaSeconds);
		if (FPlatformTime::Seconds() >= CaptureReadyAtSeconds && GFrameCounter >= PreparedFrame + 3)
		{
			// 룰링 b — 위의 draw 가 캡처 폭으로 지오메트리를 갱신했다. 그 폭으로 생산의 빈자리 재계산을
			// 한 번 돌리고 **다시 그린 프레임**을 저장한다(생산의 틱은 라이브 뷰포트 폭으로만 돈다).
			ResyncRosterForCaptureSize();
			const bool bFxFrame = SeekPotentialRerollFx();
			Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), CurrentDPIScale, FVector2D(CurrentWidth, CurrentHeight), 0.f);
			// ScaleBox의 prepass는 직전 paint 영역을 사용하므로 폭 변경 뒤 같은 영역에서 두 번 더 안정화한다.
			for (int32 LayoutPass = 0; LayoutPass < 2; ++LayoutPass)
			{
				Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), CurrentDPIScale, FVector2D(CurrentWidth, CurrentHeight), 0.f);
			}
			Capture();
			if (bFxFrame) EndPotentialRerollFx();
			TierUpOverlayFixture.Reset();
			++CaptureIndex; bScenarioReady = false;
		}
		return true;
	}
};

static TSharedPtr<FEmployeeEquipmentUIValidation> GEquipmentUIValidation;

struct FEquipmentDropdownValidation
{
	TStrongObjectPtr<UEmployeeEquipmentPanelWidget> Panel;
	TWeakObjectPtr<UComboBoxString> Combo;
	FString Directory;
	float Elapsed = 0.f;
	bool bOpened = false;

	void GatherRows(const TSharedRef<SWidget>& Widget, const FGeometry& RootGeometry, TArray<TSharedPtr<FJsonValue>>& Rows)
	{
		if (Widget->GetTypeAsString().Contains(TEXT("SComboRow")))
		{
			const FGeometry& Geometry = Widget->GetCachedGeometry();
			const FVector2D Start = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D End = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
			auto Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("width"), End.X - Start.X); Row->SetNumberField(TEXT("height"), End.Y - Start.Y);
			Row->SetNumberField(TEXT("row_local_height"), Geometry.GetLocalSize().Y);
			Row->SetNumberField(TEXT("screen_height"), Geometry.LocalToAbsolute(Geometry.GetLocalSize()).Y - Geometry.LocalToAbsolute(FVector2D::ZeroVector).Y);
			Row->SetNumberField(TEXT("y"), Start.Y); Row->SetBoolField(TEXT("minimum_154"), End.X - Start.X >= 153.5f && End.Y - Start.Y >= 153.5f);
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		FChildren* ChildWidgets = Widget->GetChildren();
		for (int32 Index = 0; ChildWidgets && Index < ChildWidgets->Num(); ++Index) GatherRows(ChildWidgets->GetChildAt(Index), RootGeometry, Rows);
	}

	bool Tick(float DeltaSeconds)
	{
		Elapsed += DeltaSeconds;
		if (!Panel.IsValid() || !Combo.IsValid() || !Panel->GetWorld()) return false;
		if (Elapsed < .7f) return true;
		const auto SlateCombo = StaticCastSharedPtr<SComboBox<TSharedPtr<FString>>>(Combo->GetCachedWidget());
		if (!SlateCombo.IsValid()) return false;
		if (!bOpened) { SlateCombo->SetIsOpen(true); bOpened = true; Elapsed = 0.f; return true; }
		const auto MenuWindow = SlateCombo->GetMenuWindow();
		auto Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("opened"), Combo->IsOpen());
		Report->SetStringField(TEXT("source"), TEXT("Live SComboRow bounds converted with the originating Combo geometry before offscreen rendering; authored UI units, same 154 minimum as the closed control."));
		Report->SetNumberField(TEXT("combo_local_height"), Combo->GetCachedGeometry().GetLocalSize().Y);
		Report->SetNumberField(TEXT("combo_layout_scale"), Combo->GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale());
		TArray<TSharedPtr<FJsonValue>> Rows;
		bool bSaved = false;
		if (MenuWindow.IsValid())
		{
			const TSharedRef<SWidget> Popup = MenuWindow->GetContent();
			GatherRows(Popup, Combo->GetCachedGeometry(), Rows);
			const FVector2D Size = MenuWindow->GetClientSizeInScreen();
			FWidgetRenderer Renderer(true);
			TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, false));
			if (Target.IsValid())
			{
				Renderer.DrawWidget(Target.Get(), Popup, Size, DeltaSeconds);
				bSaved = SaveEquipmentUIPNG(Target.Get(), FPaths::Combine(Directory, TEXT("employee_dropdown_open.png")));
			}
		}
		Report->SetBoolField(TEXT("saved"), bSaved); Report->SetArrayField(TEXT("rows"), Rows);
		bool bPassed = bSaved && !Rows.IsEmpty();
		for (const auto& Row : Rows) bPassed &= Row->AsObject()->GetBoolField(TEXT("minimum_154"));
		Report->SetBoolField(TEXT("passed"), bPassed);
		FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
		FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("dropdown-manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		SlateCombo->SetIsOpen(false);
		Panel->DeactivateWidget(); Panel->RemoveFromParent(); Panel.Reset();
		UE_LOG(LogTemp, Display, TEXT("EquipmentUI dropdown validation %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Directory);
		return false;
	}
};

// 정의는 아래에 있다 ― static 이라 전방 선언이 없으면 여기서 못 찾는다(유니티 빌드에서 모듈 전체가 막힌다).
static UWorld* FindEquipmentCaptureWorld(UWorld* CommandWorld);

// 직원창을 띄우고 오른쪽 칼럼을 장비로 연다 ― 칼럼 교체를 눈으로 볼 때 쓴다.
// CheatManager Exec 가 아니라 콘솔 명령인 이유: -ExecCmds 는 LoadingMap 단계에서 발사되는데
// 그때는 PlayerController 의 CheatManager 가 아직 없어 Exec 가 조용히 버려진다.
static FAutoConsoleCommandWithWorldAndArgs GOpenEmployeeWindow(
	TEXT("EmployeeUI.OpenWindow"), TEXT("Open UI_EmployeeWindow on the right column. EmployeeUI.OpenWindow [employee_id|auto] [equipment|growth]. Waits for the save to load."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* CommandWorld)
{
	const TArray<FString> Parsed = ParseEquipmentUIArguments(Args);
	TSharedRef<float> Waited = MakeShared<float>(0.f);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Parsed, Waited](float DeltaSeconds)
	{
		*Waited += DeltaSeconds;
		UWorld* InWorld = FindEquipmentCaptureWorld(nullptr);
		auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
		auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		auto* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
		auto* UIMgr = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
		const bool bReady = Tables && Employees && UIMgr && UIMgr->GetUIBase() && !Employees->GetAllEmployees().IsEmpty();
		if (!bReady)
		{
			if (*Waited < 120.f) return true;
			UE_LOG(LogTemp, Error, TEXT("EmployeeUI.OpenWindow: not ready after %.0fs (tables=%d employees=%d uibase=%d)"),
				*Waited, Tables != nullptr, Employees != nullptr, UIMgr && UIMgr->GetUIBase());
			return false;
		}

		const TArray<FEmployeeInstance> All = Employees->GetAllEmployees();
		const int32 TargetId = Parsed.Num() > 0 && Parsed[0] != TEXT("auto") ? FCString::Atoi(*Parsed[0]) : All[0].EmployeeID;
		const bool bEquipment = !(Parsed.Num() > 1 && Parsed[1] == TEXT("growth"));

		const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeWindow);
		if (!Class) { UE_LOG(LogTemp, Error, TEXT("EmployeeUI.OpenWindow: EmployeeWindow is not registered")); return false; }
		auto* Window = Cast<UEmployeeWindowWidget>(UIMgr->GetUIBase()->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Class.Get())));
		if (!Window) { UE_LOG(LogTemp, Error, TEXT("EmployeeUI.OpenWindow: prompt stack refused the window")); return false; }
		Window->OpenWithContext(TargetId, bEquipment);
		UE_LOG(LogTemp, Display, TEXT("EmployeeUI.OpenWindow: employee=%d equipment=%d waited=%.1fs"), TargetId, bEquipment, *Waited);
		return false;
	}));
}));

static FAutoConsoleCommandWithWorldAndArgs GCaptureEquipmentDropdown(
	TEXT("EquipmentUI.CaptureDropdown"), TEXT("Capture the actual open employee dropdown and its row geometry in PIE."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* CommandWorld)
	{
		UWorld* InWorld = CommandWorld && CommandWorld->WorldType == EWorldType::PIE ? CommandWorld : nullptr;
		if (!InWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) { InWorld = Context.World(); break; }
		auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
		auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!Tables || !InWorld->GetFirstPlayerController() || GEquipmentUIValidation) return;
		auto Run = MakeShared<FEquipmentDropdownValidation>();
		const TArray<FString> Parsed = ParseEquipmentUIArguments(Args);
		Run->Directory = Parsed.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentUIValidation")) : Parsed[0];
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Run->Directory);
		const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentPanel);
		Run->Panel.Reset(Class ? CreateWidget<UEmployeeEquipmentPanelWidget>(InWorld->GetFirstPlayerController(), Class) : nullptr);
		if (!Run->Panel.IsValid()) return;
		Run->Panel->AddToViewport(220); Run->Panel->OpenWithContext(INDEX_NONE, EEquipmentWorkspaceView::Employee); Run->Panel->ActivateWidget();
		auto* Host = Cast<UPanelWidget>(Run->Panel->GetWidgetFromName(TEXT("WorkspaceHost")));
		auto* Work = Host && Host->GetChildrenCount() ? Cast<UUserWidget>(Host->GetChildAt(0)) : nullptr;
		Run->Combo = Work ? Cast<UComboBoxString>(Work->GetWidgetFromName(TEXT("EmployeeCombo"))) : nullptr;
		if (!Run->Combo.IsValid()) { Run->Panel->DeactivateWidget(); Run->Panel->RemoveFromParent(); return; }
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float DeltaSeconds) { return Run->Tick(DeltaSeconds); }));
	}));

static FAutoConsoleCommandWithWorldAndArgs GClaimStarterEquipmentUI(
	TEXT("CGR.EquipmentUI.ClaimStarter"), TEXT("Explicitly claim the one-time free starter via the normal equipment transaction. Requires PIE and tutorial completion."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* CommandWorld)
	{
		UWorld* InWorld = CommandWorld && CommandWorld->WorldType == EWorldType::PIE ? CommandWorld : nullptr;
		if (!InWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) { InWorld = Context.World(); break; }
		auto* Equipment = InWorld && InWorld->GetGameInstance() ? InWorld->GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		if (!Equipment) { UE_LOG(LogTemp, Error, TEXT("Equipment starter requires PIE.")); return; }
		const FEmployeeEquipmentResult Result = Equipment->Execute(Equipment->MakeRequest(EEquipmentCommand::ClaimStarter));
		UE_LOG(LogTemp, Display, TEXT("Equipment starter committed=%d error=%s granted=%d revision=%lld"), Result.bCommitted, *Result.ErrorCode.ToString(), Result.GrantedInstances.Num(), Result.Revision);
	}));
// 위젯을 렌더해 PNG 로 뽑는 데 PIE 여야 할 이유가 없다 — 스탠드얼론(-game)의 Game 월드도 받는다.
// 오히려 스탠드얼론은 실기기와 같은 렌더 경로라 룩이 더 정확하고, PIE 기동에 필요한 사람 손이 빠진다.
static UWorld* FindEquipmentCaptureWorld(UWorld* CommandWorld)
{
	const auto Usable = [](UWorld* World)
	{
		return World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
			&& World->GetGameInstance() && World->GetFirstPlayerController();
	};
	if (Usable(CommandWorld)) return CommandWorld;
	if (GEngine)
	{
		// PIE 를 먼저 찾는다 — 에디터에서 둘 다 살아 있으면 사람이 보고 있는 쪽이 PIE 다.
		for (EWorldType::Type Wanted : {EWorldType::PIE, EWorldType::Game})
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
				if (Context.WorldType == Wanted && Usable(Context.World())) return Context.World();
	}
	return nullptr;
}

static FAutoConsoleCommandWithWorldAndArgs GValidateEquipmentOwnedList(
	TEXT("EquipmentUI.ValidateOwnedList"), TEXT("Validate live owned IDs, both equipment filters, selection and detail without saving. [output_directory] [employee_id|auto] [optional_backup_slot]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* CommandWorld)
	{
		const TArray<FString> Parsed = ParseEquipmentUIArguments(Args);
		TSharedRef<float> Waited = MakeShared<float>(0.f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Parsed, Waited](float DeltaSeconds)
		{
			*Waited += DeltaSeconds;
			UWorld* InWorld = FindEquipmentCaptureWorld(nullptr);
			auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
			auto* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
			auto* Saves = Instance ? Instance->GetSubsystem<USaveLoadManager>() : nullptr;
			if (!Employees || Employees->GetAllEmployees().IsEmpty() || !Saves || !Saves->GetCurrentSaveData())
			{
				if (*Waited < 120.f) return true;
				UE_LOG(LogTemp, Error, TEXT("EquipmentUI owned-list: game and saved employee data unavailable after %.0fs"), *Waited);
				return false;
			}
			auto Run = MakeShared<FEmployeeEquipmentUIValidation>();
			Run->ValidateOwnedList(InWorld, Parsed);
			return false;
		}));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCaptureEquipmentUI(
	TEXT("EquipmentUI.Capture"), TEXT("Capture actual equipment UI from a PIE or standalone game world. EquipmentUI.Capture [output_directory] [employee_id|auto] [employee-inline|employee-mobile|employee-potential|employee-training|employee-columns]. Waits for the map to finish loading. No equipment transactions."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* CommandWorld)
	{
		if (GEquipmentUIValidation) { UE_LOG(LogTemp, Warning, TEXT("EquipmentUI capture already running.")); return; }
		const TArray<FString> Parsed = ParseEquipmentUIArguments(Args);
		// -ExecCmds 는 LoadingMap 단계에서 발사돼 본 맵·세이브가 아직 없다. 준비될 때까지 기다린다.
		TSharedRef<float> Waited = MakeShared<float>(0.f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Parsed, Waited](float DeltaSeconds)
		{
			if (GEquipmentUIValidation) return false;
			*Waited += DeltaSeconds;
			UWorld* InWorld = FindEquipmentCaptureWorld(nullptr);
			auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
			auto* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
			auto* Saves = Instance ? Instance->GetSubsystem<USaveLoadManager>() : nullptr;
			if (!InWorld || !Employees || Employees->GetAllEmployees().IsEmpty() || !Saves || !Saves->GetCurrentSaveData())
			{
				if (*Waited < 120.f) return true;
				UE_LOG(LogTemp, Error, TEXT("EquipmentUI start: game and saved employee data unavailable after %.0fs."), *Waited);
				return false;
			}
			auto Run = MakeShared<FEmployeeEquipmentUIValidation>();
			if (!Run->Start(InWorld, Parsed))
			{
				Run->Check(TEXT("capture_start"), false, TEXT("Runtime data was ready; initialization failed and is not retried."));
				if (!Run->OutputDirectory.IsEmpty()) Run->Finish();
				return false;
			}
			GEquipmentUIValidation = Run;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta)
			{
				const bool bContinue = Run->Tick(Delta);
				if (!bContinue) GEquipmentUIValidation.Reset();
				return bContinue;
			}));
			return false;
		}));
	}));

#endif
