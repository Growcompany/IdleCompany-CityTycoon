#if WITH_EDITOR && !UE_BUILD_SHIPPING

#include "UI/Panel/RankingPanelWidget.h"
#include "UI/Element/Cards/RankingEntryCardWidget.h"
#include "UI/Ranking/RankingBuildingPreviewRenderer.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/WidgetSwitcher.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace RankingPanelLayoutValidation
{
static bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage)
{
	FWidgetRenderer Renderer(true);
	const FVector2D DrawSize(Size.X, Size.Y);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
	if (!Target.IsValid()) return false;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, Pass == 0 ? 1.f : 0.f);
		FlushRenderingCommands();
	}
	if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage)) return false;
	OutImage.GammaSpace = EGammaSpace::sRGB;
	return true;
}

static TSharedPtr<FJsonObject> Measure(UUserWidget& Panel, const TCHAR* WidgetName)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	UWidget* Widget = Panel.GetWidgetFromName(FName(WidgetName));
	Result->SetBoolField(TEXT("found"), Widget != nullptr);
	if (Widget)
	{
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D Position = Panel.GetCachedGeometry().AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D Size = Geometry.GetLocalSize();
		Result->SetNumberField(TEXT("left"), Position.X);
		Result->SetNumberField(TEXT("top"), Position.Y);
		Result->SetNumberField(TEXT("width"), Size.X);
		Result->SetNumberField(TEXT("height"), Size.Y);
		Result->SetNumberField(TEXT("bottom"), Position.Y + Size.Y);
	}
	return Result;
}

static FRankingEntry Sample(int32 Index, int32 TabIndex)
{
	FRankingEntry Entry;
	Entry.PlayFabId = FString::Printf(TEXT("ranking-ui-fixture-%d"), Index);
	Entry.Rank = Index + 1;
	Entry.DisplayName = Index == 0 ? TEXT("세계로성장하는아주긴이름의대한민국회사") : FString::Printf(TEXT("성장하는 회사 %02d"), Index + 1);
	Entry.TotalRevenue = 987654321098765LL / (Index + 1) / (TabIndex + 1);
	Entry.HQLevel = 99 - Index;
	Entry.ProfileImageID = 0;
	return Entry;
}

static bool ValidateRepresentativeSelection(UTableManagerSubsystem* Tables, TArray<TSharedPtr<FJsonValue>>& Checks)
{
	FCitySnapshotBuilding ShortBuilding;
	ShortBuilding.InteractableName = TEXT("B1");
	ShortBuilding.AppliedSkinID = 100;
	ShortBuilding.LifetimeRevenueEarned = 200.;
	FCitySnapshotBuilding TallBuilding = ShortBuilding;
	TallBuilding.AppliedSkinID = 101;
	TallBuilding.Scale = FVector(2.);
	TallBuilding.LifetimeRevenueEarned = 100.;
	bool bPassed = true;
	auto CheckWinner = [&](const TCHAR* Name, const TArray<FCitySnapshotBuilding>& Buildings, int32 ExpectedSkin)
	{
		FCitySnapshot Snapshot;
		Snapshot.Buildings = Buildings;
		FCitySnapshotBuilding Selected;
		const bool bCorrect = URankingBuildingPreviewRenderer::SelectRepresentative(Snapshot, Tables, Selected)
			&& Selected.AppliedSkinID == ExpectedSkin;
		TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
		Record->SetStringField(TEXT("name"), Name);
		Record->SetBoolField(TEXT("passed"), bCorrect);
		Checks.Add(MakeShared<FJsonValueObject>(Record));
		bPassed &= bCorrect;
	};
	CheckWinner(TEXT("revenue_beats_height"), {TallBuilding, ShortBuilding}, 100);
	TallBuilding.LifetimeRevenueEarned = ShortBuilding.LifetimeRevenueEarned;
	CheckWinner(TEXT("equal_revenue_uses_height"), {ShortBuilding, TallBuilding}, 101);
	ShortBuilding.LifetimeRevenueEarned = TallBuilding.LifetimeRevenueEarned = 0.;
	CheckWinner(TEXT("zero_revenue_uses_height"), {ShortBuilding, TallBuilding}, 101);
	TallBuilding.LifetimeRevenueEarned = std::numeric_limits<double>::quiet_NaN();
	ShortBuilding.LifetimeRevenueEarned = 1.;
	CheckWinner(TEXT("nan_revenue_is_not_a_winner"), {TallBuilding, ShortBuilding}, 100);
	TallBuilding.LifetimeRevenueEarned = std::numeric_limits<double>::infinity();
	CheckWinner(TEXT("infinite_revenue_is_not_a_winner"), {TallBuilding, ShortBuilding}, 100);
	TallBuilding.LifetimeRevenueEarned = -50.;
	ShortBuilding.LifetimeRevenueEarned = 0.;
	CheckWinner(TEXT("nonpositive_revenue_falls_back_to_height"), {ShortBuilding, TallBuilding}, 101);
	FCitySnapshotBuilding InvalidBuilding = ShortBuilding;
	InvalidBuilding.InteractableName = NAME_None;
	InvalidBuilding.LifetimeRevenueEarned = 1.e20;
	CheckWinner(TEXT("invalid_mesh_cannot_win_on_revenue"), {InvalidBuilding, TallBuilding}, 101);
	TallBuilding = ShortBuilding;
	TallBuilding.AppliedSkinID = 101;
	CheckWinner(TEXT("exact_ties_preserve_input_order"), {TallBuilding, ShortBuilding}, 101);
	return bPassed;
}

static void Capture(UWorld* InWorld)
{
	UWorld* CaptureWorld = InWorld && InWorld->WorldType == EWorldType::PIE ? InWorld : nullptr;
	if (!CaptureWorld && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE) { CaptureWorld = Context.World(); break; }
		}
	}
	UGameInstance* GameInstance = CaptureWorld ? CaptureWorld->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableManager = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableManager)
	{
		UE_LOG(LogTemp, Error, TEXT("RankingUI.Capture requires an existing PIE game instance."));
		return;
	}
	const TSubclassOf<UUserWidget> PanelClass = TableManager->GetWidgetClass(EWidgetType::RankingPanel);
	if (!PanelClass) return;
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RankingUIValidation"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<TSharedPtr<FJsonValue>> SparseChecks;
	TArray<TSharedPtr<FJsonValue>> SelectionChecks;
	bool bAllPassed = ValidateRepresentativeSelection(TableManager, SelectionChecks);
	FCitySnapshotBuilding EmptySelection;
	bAllPassed &= !URankingBuildingPreviewRenderer::SelectRepresentative(FCitySnapshot(), TableManager, EmptySelection);
	for (const int32 Width : {2304, 3120})
	{
		TStrongObjectPtr<URankingPanelWidget> Panel(CreateWidget<URankingPanelWidget>(GameInstance, PanelClass));
		if (!Panel.IsValid()) { bAllPassed = false; continue; }
		// WBP 기본값이 바뀌어도 Slate 구성 중 자동 활성화로 점수가 업로드되지 않게 한다.
		FBoolProperty* AutoActivateProperty = FindFProperty<FBoolProperty>(Panel->GetClass(), TEXT("bAutoActivate"));
		if (!AutoActivateProperty)
		{
			UE_LOG(LogTemp, Error, TEXT("RankingUI.Capture: bAutoActivate property missing; refusing to construct capture."));
			bAllPassed = false;
			continue;
		}
		AutoActivateProperty->SetPropertyValue_InContainer(Panel.Get(), false);
		UScrollBox* Scrolls[] = {
			Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("RevenueLeaderboardScrollBox"))),
			Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("LevelLeaderboardScrollBox")))};
		UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Panel->GetWidgetFromName(TEXT("ContentSwitcher")));
		if (!Scrolls[0] || !Scrolls[1] || !Switcher) { bAllPassed = false; continue; }
		// NativeConstruct의 초기 탭 선택은 빈 목록을 조회한다. TakeWidget 전에 채워 읽기 요청도 피한다.
		for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
		{
			Scrolls[TabIndex]->ClearChildren();
			Scrolls[TabIndex]->AddChild(NewObject<USpacer>(Panel.Get()));
		}
		const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		const TCHAR* MyNames[] = {TEXT("MyRankingEntryCard"), TEXT("MyLevelRankingEntryCard")};
		for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
		{
			TArray<FRankingEntry> Entries;
			for (int32 Index = 0; Index < 10; ++Index) Entries.Add(Sample(Index, TabIndex));
			FRankingEntry OwnEntry = Sample(9, TabIndex);
			OwnEntry.DisplayName = TEXT("내 회사");
			OwnEntry.Rank = 1234;
			Panel->ApplyLeaderboard(TabIndex, Entries, OwnEntry);
			bool bRenderedFixtureUsesRevenuePriority = true;
			for (int32 Rank = 1; Rank <= 3; ++Rank)
			{
				FCitySnapshot Snapshot;
				FCitySnapshotBuilding Building;
				Building.InteractableName = FName(*FString::Printf(TEXT("B%d"), Rank));
				Building.Body_Module_Copies = Rank == 1 ? 8 : 3;
				Building.AppliedSkinID = 99 + Rank;
				Building.LifetimeRevenueEarned = 200.;
				FCitySnapshotBuilding TallerLowRevenueBuilding = Building;
				TallerLowRevenueBuilding.Scale = FVector(2.);
				TallerLowRevenueBuilding.LifetimeRevenueEarned = 100.;
				Snapshot.Buildings.Add(TallerLowRevenueBuilding);
				Snapshot.Buildings.Add(Building);
				FCitySnapshotBuilding Selected;
				bRenderedFixtureUsesRevenuePriority &= URankingBuildingPreviewRenderer::SelectRepresentative(Snapshot, TableManager, Selected)
					&& Selected.InteractableName == Building.InteractableName && Selected.Body_Module_Copies == Building.Body_Module_Copies
					&& Selected.Scale.Equals(Building.Scale) && Selected.LifetimeRevenueEarned == 200.;
				Panel->ApplyPodiumSnapshot(Entries[Rank - 1].PlayFabId, Snapshot);
			}
			// 실제 그룹을 선택하므로 선택 상태와 스위처가 같은 경로로 바뀐다. 픽스처 목록은 이미 채워져 있다.
			if (FObjectPropertyBase* GroupProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("TabButtonGroup")))
			{
				if (UCommonButtonGroupBase* Group = Cast<UCommonButtonGroupBase>(GroupProperty->GetObjectPropertyValue_InContainer(Panel.Get())))
				{
					Group->SelectButtonAtIndex(TabIndex);
				}
			}
			const bool bTabSwitched = Switcher->GetActiveWidgetIndex() == TabIndex;
			FImage RenderedImage;
			TArray64<uint8> Bytes;
			const FString Filename = FString::Printf(TEXT("ranking_%d_%s.png"), Width, TabIndex == 0 ? TEXT("marketcap") : TEXT("weekly"));
			const bool bSaved = DrawToImage(PanelSlate, FIntPoint(Width, 1440), RenderedImage)
				&& FImageUtils::CompressImage(Bytes, TEXT("png"), RenderedImage)
				&& FFileHelper::SaveArrayToFile(Bytes, *(Directory / Filename));
			TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
			Record->SetStringField(TEXT("image"), Filename);
			Record->SetBoolField(TEXT("saved"), bSaved);
			Record->SetNumberField(TEXT("width"), Width);
			Record->SetNumberField(TEXT("height"), 1440);
			Record->SetNumberField(TEXT("tab"), TabIndex);
			Record->SetBoolField(TEXT("podium_fixture_uses_revenue_priority"), bRenderedFixtureUsesRevenuePriority);
			Record->SetNumberField(TEXT("active_tab"), Switcher->GetActiveWidgetIndex());
			Record->SetBoolField(TEXT("group_selection_changed_content"), bTabSwitched);
			const auto MyBox = Measure(*Panel, MyNames[TabIndex]);
			const auto ScrollBox = Measure(*Panel, TabIndex == 0 ? TEXT("RevenueLeaderboardScrollBox") : TEXT("LevelLeaderboardScrollBox"));
			const auto FirstTab = Measure(*Panel, TEXT("MarketCapTab"));
			const auto SecondTab = Measure(*Panel, TEXT("WeeklyRevenueTab"));
			Record->SetObjectField(TEXT("my_card"), MyBox);
			Record->SetObjectField(TEXT("scroll"), ScrollBox);
			Record->SetObjectField(TEXT("marketcap_tab"), FirstTab);
			Record->SetObjectField(TEXT("weekly_tab"), SecondTab);
			const bool bFooter = MyBox->GetBoolField(TEXT("found")) && MyBox->GetNumberField(TEXT("height")) > 0
				&& MyBox->GetNumberField(TEXT("top")) >= ScrollBox->GetNumberField(TEXT("bottom")) - 1
				&& MyBox->GetNumberField(TEXT("bottom")) <= 1441;
			const bool bTabsHorizontal = FirstTab->GetBoolField(TEXT("found")) && SecondTab->GetBoolField(TEXT("found"))
				&& FMath::Abs(FirstTab->GetNumberField(TEXT("top")) - SecondTab->GetNumberField(TEXT("top"))) < 2
				&& SecondTab->GetNumberField(TEXT("left")) >= FirstTab->GetNumberField(TEXT("left")) + FirstTab->GetNumberField(TEXT("width")) - 1;
			Record->SetBoolField(TEXT("footer_below_scroll"), bFooter);
			Record->SetBoolField(TEXT("tabs_horizontal"), bTabsHorizontal);
			int32 ListEntryCount = 0;
			bool bListStartsAtFour = false;
			for (UWidget* Child : Scrolls[TabIndex]->GetAllChildren())
			{
				if (URankingEntryCardWidget* EntryCard = Cast<URankingEntryCardWidget>(Child))
				{
					if (ListEntryCount == 0)
					{
						const UTextBlock* RankText = Cast<UTextBlock>(EntryCard->GetWidgetFromName(TEXT("RankText")));
						bListStartsAtFour = RankText && RankText->GetText().ToString().Contains(TEXT("4"));
					}
					++ListEntryCount;
				}
			}
			bool bPodiumReady = true;
			for (int32 Rank = 1; Rank <= 3; ++Rank)
			{
				const FString WidgetName = FString::Printf(TEXT("Podium%d%dBuilding"), TabIndex, Rank);
				UImage* BuildingImage = Cast<UImage>(Panel->GetWidgetFromName(FName(*WidgetName)));
				UMaterialInstanceDynamic* Material = BuildingImage ? Cast<UMaterialInstanceDynamic>(BuildingImage->GetBrush().GetResourceObject()) : nullptr;
				UTexture* Texture = nullptr;
				const bool bHasRT = Material && Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("RT")), Texture)
					&& Cast<UTextureRenderTarget2D>(Texture);
				bPodiumReady &= bHasRT;
				Record->SetBoolField(FString::Printf(TEXT("podium_%d_has_actual_rt"), Rank), bHasRT);
				Record->SetObjectField(WidgetName, Measure(*Panel, *WidgetName));
			}
			Record->SetNumberField(TEXT("list_entry_count"), ListEntryCount);
			Record->SetBoolField(TEXT("list_starts_at_four"), bListStartsAtFour);
			int32 RemainingStageCount = 0;
			for (TActorIterator<AActor> ActorIt(CaptureWorld); ActorIt; ++ActorIt)
			{
				if (ActorIt->ActorHasTag(TEXT("RankingPreviewCapture")) && !ActorIt->IsActorBeingDestroyed()) ++RemainingStageCount;
			}
			Record->SetNumberField(TEXT("remaining_capture_stages"), RemainingStageCount);
			bAllPassed &= bSaved && bFooter && bTabsHorizontal && bTabSwitched && bPodiumReady && ListEntryCount == 7 && bListStartsAtFour && RemainingStageCount == 0 && bRenderedFixtureUsesRevenuePriority;
			Captures.Add(MakeShared<FJsonValueObject>(Record));
		}
		// 정상 네 장을 저장한 뒤에만 인원 부족 상태를 만든다. 이전 시상대의 이미지와 이름이 남으면 실패다.
		if (Width == 3120)
		{
			for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
			{
				for (int32 EntryCount = 0; EntryCount <= 2; ++EntryCount)
				{
					TArray<FRankingEntry> SparseEntries;
					for (int32 Index = 0; Index < EntryCount; ++Index) SparseEntries.Add(Sample(Index, TabIndex));
					Panel->ApplyLeaderboard(TabIndex, SparseEntries, Sample(9, TabIndex));
					bool bNoStaleContent = true;
					for (int32 Rank = EntryCount + 1; Rank <= 3; ++Rank)
					{
						const FString Prefix = FString::Printf(TEXT("Podium%d%d"), TabIndex, Rank);
						const UImage* BuildingImage = Cast<UImage>(Panel->GetWidgetFromName(FName(*(Prefix + TEXT("Building")))));
						const UTextBlock* NameText = Cast<UTextBlock>(Panel->GetWidgetFromName(FName(*(Prefix + TEXT("Name")))));
						const bool bImageCleared = BuildingImage && (!BuildingImage->IsVisible()
							|| BuildingImage->GetBrush().GetResourceObject() == nullptr);
						const bool bNameCleared = NameText && NameText->GetText().ToString() != Sample(Rank - 1, TabIndex).DisplayName;
						bNoStaleContent &= bImageCleared && bNameCleared;
					}
					int32 RemainingRows = 0;
					for (UWidget* Child : Scrolls[TabIndex]->GetAllChildren())
					{
						if (Cast<URankingEntryCardWidget>(Child)) ++RemainingRows;
					}
					TSharedPtr<FJsonObject> SparseRecord = MakeShared<FJsonObject>();
					SparseRecord->SetNumberField(TEXT("tab"), TabIndex);
					SparseRecord->SetNumberField(TEXT("entries"), EntryCount);
					SparseRecord->SetBoolField(TEXT("missing_podium_content_cleared"), bNoStaleContent);
					SparseRecord->SetNumberField(TEXT("list_rows"), RemainingRows);
					SparseChecks.Add(MakeShared<FJsonValueObject>(SparseRecord));
					bAllPassed &= bNoStaleContent && RemainingRows == 0;
				}
			}
		}
	}
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("passed"), bAllPassed && Captures.Num() == 4);
	Report->SetArrayField(TEXT("captures"), Captures);
	Report->SetArrayField(TEXT("sparse_checks"), SparseChecks);
	Report->SetArrayField(TEXT("representative_selection_checks"), SelectionChecks);
	FString Json;
	FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")));
	UE_LOG(LogTemp, Display, TEXT("RankingUI.Capture: passed=%d, captures=%d, directory=%s"), bAllPassed, Captures.Num(), *Directory);
}

static FAutoConsoleCommandWithWorld GCaptureCommand(TEXT("RankingUI.Capture"),
	TEXT("Capture ranking WBP fixtures at 2304/3120 x 1440; requires existing PIE. No activation or score writes."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&Capture));
}

#endif
