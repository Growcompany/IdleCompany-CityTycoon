#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/GestureHintTypes.h"
#include "UI/Element/Common/GestureHintWidget.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "UI/Element/Gacha/GachaRevealBoxWidget.h"
#include "UI/Element/Gacha/GachaRevealFXLayer.h"
#include "UI/UISoundTags.h"
#include "Enum/WidgetType.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/SoundCategory.h"
#include "Enum/BuildingTraitTarget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Table/BuildingSkinData.h"
#include "Table/SoundTable.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Sound/SoundBase.h"

namespace GachaRevealWidgetDetail
{
	namespace GT = GachaRevealTiming;
	using GachaRevealCardLayout::CubicBezier;

	constexpr float StageHeight = 1440.f;
	constexpr float LandY = 800.f;
	constexpr float SpotSize = 560.f;
	constexpr float SpotOffsetX = 620.f;
	constexpr float SpotTop = 330.f;
	FGachaRevealRect SpotlightRect(float StageWidth, bool bSingle)
	{
		return bSingle
			? FGachaRevealRect{ StageWidth / 2.f - 360.f, 450.f, 220.f, 220.f }
			: FGachaRevealRect{ StageWidth / 2.f - SpotOffsetX, SpotTop, SpotSize, SpotSize };
	}
	constexpr float FlapLongestMs = 320.f; // 시안 LID.B.ms ― 앞 날개(300)는 같은 프레임 열을 20ms 늦게 끝낸다(Task 8 §우려 6)
	constexpr float CloseFadeMs = GT::CloseFade;

	float Clamp01(float V) { return FMath::Clamp(V, 0.f, 1.f); }
	float Progress(float Now, float Start, float DurationMs) { return DurationMs <= 0.f ? 1.f : Clamp01((Now - Start) / DurationMs); }
	float EaseOut(float T) { return CubicBezier(0.f, 0.f, .58f, 1.f, T); }
	float EaseIn(float T) { return CubicBezier(.42f, 0.f, 1.f, 1.f, T); }
	float EaseInOutQuad(float T) { return T < .5f ? 2.f * T * T : 1.f - FMath::Square(-2.f * T + 2.f) / 2.f; }
	FLinearColor Hex(const TCHAR* InHex) { return FLinearColor::FromSRGBColor(FColor::FromHex(InHex)); }

	// 시안 :798-799 ― 아래 패널 brightness(.6)(CSS ease 250) 위에 #dim(ease-out 260, 가운데 알파 .9).
	// 공개 화면은 패널을 못 만지므로 패널이 어두워지는 몫을 딤 불투명도에 접는다: 남는 패널 (1-.4p)(1-.9q) = 1-.9o. blur(6px)는 옮기지 않는다
	constexpr float ThrowPanelDarken = .4f;
	constexpr float DimCenterAlpha = .9f;
	float ThrowDimOpacity(float ElapsedMs)
	{
		const float PanelP = CubicBezier(.25f, .1f, .25f, 1.f, Progress(ElapsedMs, 0.f, GT::ThrowPanelBlur));
		const float DimP = EaseOut(Progress(ElapsedMs, 0.f, GT::ThrowDimFade));
		const float PanelLeft = (1.f - ThrowPanelDarken * PanelP) * (1.f - DimCenterAlpha * DimP);
		return FMath::Min(1.f, (1.f - PanelLeft) / DimCenterAlpha);
	}

	// 시안 keyframes 의 offset 사이를 직선으로 잇는다(WAAPI 기본) ― 전체 easing 은 호출자가 먼저 건다
	template <int32 N, typename TValue>
	TValue Keyframes(const float (&Offsets)[N], const TValue (&Values)[N], float P)
	{
		for (int32 Index = 1; Index < N; ++Index)
		{
			if (P <= Offsets[Index])
			{
				const float Span = Offsets[Index] - Offsets[Index - 1];
				const float Local = Span > 0.f ? (P - Offsets[Index - 1]) / Span : 1.f;
				return FMath::Lerp(Values[Index - 1], Values[Index], Local);
			}
		}
		return Values[N - 1];
	}

	FGachaRevealGradeColors Grade(const FLinearColor& Ui, const TCHAR* Fx, const TCHAR* Ink)
	{
		FGachaRevealGradeColors Out;
		Out.Ui = Ui;
		Out.Fx = Hex(Fx);
		Out.Ink = Hex(Ink);
		return Out;
	}

	// 시안 rgbLin(...hx(...)) = 16진수를 linear 값 그대로
	FLinearColor HexAsLinear(const TCHAR* InHex)
	{
		const FColor C = FColor::FromHex(InHex);
		return FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f);
	}

	int32 TierOf(ELootBoxRarity Rarity)
	{
		switch (Rarity)
		{
		case ELootBoxRarity::Common: return 0;
		case ELootBoxRarity::Unusual: return 1;
		case ELootBoxRarity::Rare: return 2;
		case ELootBoxRarity::Epic: return 3;
		case ELootBoxRarity::Legendary: return 4;
		case ELootBoxRarity::Mythic: return 5;
		default: return 0;
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
#define GACHA_REVEAL_COUNT_EVENT(Event) CountTestEvent(Event)
#else
#define GACHA_REVEAL_COUNT_EVENT(Event)
#endif

UGachaRevealPresentationWidget::UGachaRevealPresentationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace GachaRevealWidgetDetail;
	// 화면에 바로 올린 층이라 공용 뒤로가기(FindTopWindow)가 닫기 수단 없는 이 창을 못 고른다 ― 스스로 받는다
	bIsBackHandler = true;
	bAutoActivate = true;

	EquipmentPalette = {
		Grade(FLinearColor(.65f, .68f, .72f), TEXT("#D6DEE8"), TEXT("#5D6775")),
		Grade(FLinearColor(.24f, .68f, .38f), TEXT("#44E08C"), TEXT("#2C7E4E")),
		Grade(FLinearColor(.18f, .47f, .9f), TEXT("#4AA2FF"), TEXT("#2A64B0")),
		Grade(FLinearColor(.64f, .31f, .9f), TEXT("#B86BFF"), TEXT("#7D3DBA")),
		Grade(FLinearColor(1.f, .68f, .14f), TEXT("#FFB628"), TEXT("#A86A00")),
		Grade(FLinearColor(1.f, .34f, .24f), TEXT("#FF5B3D"), TEXT("#B5321C")) };
	TraitPalette = {
		Grade(HexAsLinear(TEXT("#A6A6A6")), TEXT("#D3D8DE"), TEXT("#5E6670")),
		Grade(HexAsLinear(TEXT("#4CD964")), TEXT("#4CE07A"), TEXT("#2A7E3E")),
		Grade(HexAsLinear(TEXT("#34AADC")), TEXT("#3FB6F0"), TEXT("#1F6E96")),
		Grade(HexAsLinear(TEXT("#AF52DE")), TEXT("#BF66F5"), TEXT("#7A37A6")),
		Grade(HexAsLinear(TEXT("#FFCC00")), TEXT("#FFC21A"), TEXT("#A37800")),
		Grade(HexAsLinear(TEXT("#FF2D55")), TEXT("#FF4466"), TEXT("#B01E3B")) };
}

// ============================================================================
// 수명
// ============================================================================

void UGachaRevealPresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveExtraWidgets();
	Runner.OnBeatStarted = [this](int32 Index, const FGachaRevealBeatSpec& Beat) { HandleBeatStarted(Index, Beat); };
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &UGachaRevealPresentationWidget::OnConfirmClicked);
	}
	if (SkipButton)
	{
		SkipButton->OnClicked().RemoveAll(this);
		SkipButton->OnClicked().AddUObject(this, &UGachaRevealPresentationWidget::HandleSkipClicked);
	}
	if (!Runner.IsRunning())
	{
		ResetStage();
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* Missions = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			Missions->RegisterGachaReveal(this);
		}
	}
}

void UGachaRevealPresentationWidget::NativeDestruct()
{
	// 파괴로 끝나도 닫힘 통지는 반드시 간다 ― 패널이 뽑기 단추 잠금을 풀 수 있게
	if (IsPresentationOpen() && !bCloseNotified)
	{
		bCloseNotified = true;
		OnPresentationClosed.ExecuteIfBound();
	}
	Runner.Reset();
	Runner.OnBeatStarted = nullptr;
	StopZipLoop();
	if (FX)
	{
		FX->Release();
	}
	ClearDealtCards();
	ClearStamps();
	if (ConfirmButton) { ConfirmButton->OnClicked().RemoveAll(this); }
	if (SkipButton) { SkipButton->OnClicked().RemoveAll(this); }
	if (SortGradeControl.IsValid()) { SortGradeControl->OnClicked().RemoveAll(this); }
	if (SortOrderControl.IsValid()) { SortOrderControl->OnClicked().RemoveAll(this); }
	if (EquipControl.IsValid()) { EquipControl->OnClicked().RemoveAll(this); }
	if (SingleEquipControl.IsValid()) { SingleEquipControl->OnClicked().RemoveAll(this); }
	if (ActiveTooltip)
	{
		ActiveTooltip->RemoveFromParent();
		ActiveTooltip = nullptr;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* Missions = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			Missions->UnregisterGachaReveal(this);
		}
	}
	// 실행·닫기 도중 파괴돼도 같은 객체를 다시 열고 닫을 수 있게 닫힘 상태까지 비운다(통지는 위에서 끝남)
	ResetPresentation();
	Super::NativeDestruct();
}

void UGachaRevealPresentationWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// 남이 비활성화해도(스택 정리·레벨 전환) 닫기 경로를 거쳐 실행기를 비운다 ― 안 비우면 다음 Setup 이 영영 거절된다
	if (!bClosing && Runner.IsRunning())
	{
		CloseSelf();
		FinishClose();
	}
}

void UGachaRevealPresentationWidget::ResolveExtraWidgets()
{
	if (!WidgetTree)
	{
		return;
	}
	auto Find = [this](const TCHAR* Name) -> UWidget*
	{
		UWidget* Found = WidgetTree->FindWidget(FName(Name));
		if (!Found)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] WBP 자식 %s 없음 ― author_wbp.py reveal 저작 확인"), Name);
		}
		return Found;
	};
	ShadeImage = Cast<UImage>(Find(TEXT("SpotlightShade")));
	SpotContentPanel = Find(TEXT("SpotlightContent"));
	TitleTextBlock = Cast<UTextBlock>(Find(TEXT("HeaderTitleText")));
	SinglePanel = Find(TEXT("SingleInfo"));
	StampLatinTemplate = Cast<UTextBlock>(Find(TEXT("StampLatin")));
	StampKoreanTemplate = Cast<UTextBlock>(Find(TEXT("StampKorean")));
	HandTextBlock = Find(TEXT("HandGuideText"));
	TallyCountTexts.SetNum(GachaRevealModel::MaxGrade + 1);
	TallyLatinTexts.SetNum(GachaRevealModel::MaxGrade + 1);
	TallyKoreanTexts.SetNum(GachaRevealModel::MaxGrade + 1);
	TallyCellWidgets.SetNum(GachaRevealModel::MaxGrade + 1);
	for (int32 G = 0; G <= GachaRevealModel::MaxGrade; ++G)
	{
		TallyCountTexts[G] = Cast<UTextBlock>(Find(*FString::Printf(TEXT("TallyCount%d"), G)));
		TallyLatinTexts[G] = Cast<UTextBlock>(Find(*FString::Printf(TEXT("TallyGrade%d"), G)));
		TallyKoreanTexts[G] = Cast<UTextBlock>(Find(*FString::Printf(TEXT("TallyGradeKor%d"), G)));
		TallyCellWidgets[G] = Find(*FString::Printf(TEXT("TallyCell%d"), G));
	}
	SortGradeControl = Cast<UButtonWidget>(Find(TEXT("SortGradeButton")));
	SortOrderControl = Cast<UButtonWidget>(Find(TEXT("SortOrderButton")));
	EquipControl = Cast<UButtonWidget>(Find(TEXT("EquipButton")));
	SingleEquipControl = Cast<UButtonWidget>(Find(TEXT("SingleEquipButton")));
	if (SortGradeControl.IsValid()) { SortGradeControl->OnClicked().RemoveAll(this); SortGradeControl->OnClicked().AddUObject(this, &UGachaRevealPresentationWidget::OnSortGradeClicked); }
	if (SortOrderControl.IsValid()) { SortOrderControl->OnClicked().RemoveAll(this); SortOrderControl->OnClicked().AddUObject(this, &UGachaRevealPresentationWidget::OnSortOrderClicked); }
	if (EquipControl.IsValid()) { EquipControl->OnClicked().RemoveAll(this); EquipControl->OnClicked().AddUObject(this, &UGachaRevealPresentationWidget::OnEquipClicked); }
	if (SingleEquipControl.IsValid()) { SingleEquipControl->OnClicked().RemoveAll(this); SingleEquipControl->OnClicked().AddUObject(this, &UGachaRevealPresentationWidget::OnEquipClicked); }
}

const UTableManagerSubsystem* UGachaRevealPresentationWidget::ResolveTables() const
{
	if (TablesOverride.IsValid())
	{
		return TablesOverride.Get();
	}
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
}

bool UGachaRevealPresentationWidget::ResolveReduceMotion() const
{
	return ReduceMotionOverride.IsSet() ? ReduceMotionOverride.GetValue() : USettingsManagerSubsystem::IsReduceMotion(this);
}

bool UGachaRevealPresentationWidget::IsHandOpenBoxSetting(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return false;
	}
	const UWorld* CurrentWorld = WorldContext->GetWorld();
	const UGameInstance* GameInstance = CurrentWorld ? CurrentWorld->GetGameInstance() : nullptr;
	const USettingsManagerSubsystem* SettingsMgr = GameInstance ? GameInstance->GetSubsystem<USettingsManagerSubsystem>() : nullptr;
	return SettingsMgr && SettingsMgr->IsHandOpenBox();
}

// ============================================================================
// 진입
// ============================================================================

void UGachaRevealPresentationWidget::SetLaunchFrom(FVector2D CanvasPos, float Scale)
{
	LaunchPos = CanvasPos;
	LaunchScale = Scale;
}

bool UGachaRevealPresentationWidget::RejectIfOpen(const TCHAR* Source) const
{
	if (!IsPresentationOpen())
	{
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 공개 화면이 이미 열려 있어 %s 결과 표시 요청을 무시함(패널이 열린 동안 뽑기를 막아야 함)"), Source);
	return true;
}

void UGachaRevealPresentationWidget::SetupPullTrait(const TArray<FBuildingTraitGachaResult>& Results, bool bInAdvanced, const FGachaRevealAwardContext& Award)
{
	if (RejectIfOpen(TEXT("특성")))
	{
		return;
	}
	const UTableManagerSubsystem* Tables = ResolveTables();
	AwardContext = Award;
	EquipmentInstanceIds.Reset();
	bSkinDuplicates.Reset();
	RevealEmployeeId = INDEX_NONE;
	TArray<FGachaRevealEntry> NewEntries;
	TArray<FGachaRevealCardData> NewCards;
	TArray<FText> NewMeta;
	int32 SourceIndex = 0;
	for (const FBuildingTraitGachaResult& Result : Results)
	{
		const int32 AwardIndex = SourceIndex++;
		FBuildingTraitTableRow Row;
		if (!Tables || !Tables->GetBuildingTraitData(Result.ResultTraitID, Row))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 특성 표 행 없음: %s"), *Result.ResultTraitID.ToString());
			continue;
		}
		FGachaRevealEntry& Entry = NewEntries.AddDefaulted_GetRef();
		Entry.PullIndex = NewEntries.Num() - 1;
		Entry.Grade = GachaRevealWidgetDetail::TierOf(Result.Rarity);
		Entry.bNew = Award.bOwnedBefore.IsValidIndex(AwardIndex) && !Award.bOwnedBefore[AwardIndex];
		FGachaRevealCardData& Card = NewCards.AddDefaulted_GetRef();
		Card.Kind = EGachaRevealKind::Trait;
		Card.Grade = Entry.Grade;
		Card.Name = Row.DisplayName;
		Card.EffectLine = FormatTraitEffectText(GetTargetForCategory(Row.Category), Row.BaseEffect);
		Card.Icon = Row.Icon;
		Card.bNew = Entry.bNew;
		NewMeta.Add(NSLOCTEXT("GachaReveal", "TraitNoun", "특성"));
	}
	const FText Title = FText::Format(NSLOCTEXT("GachaReveal", "Title", "{0} {1} 뽑기 · {2}회"),
		bInAdvanced ? NSLOCTEXT("GachaReveal", "TierAdvanced", "고급") : NSLOCTEXT("GachaReveal", "TierNormal", "일반"),
		NSLOCTEXT("GachaReveal", "TraitNoun", "특성"), FText::AsNumber(NewEntries.Num()));
	BeginReveal(EGachaRevealKind::Trait, bInAdvanced ? EGachaBoxType::TraitAdvanced : EGachaBoxType::TraitNormal, Title, NewEntries, NewCards, NewMeta);
}

void UGachaRevealPresentationWidget::SetupPullSkin(const TArray<FBuildingSkinGachaResult>& Results, bool bInAdvanced, const FGachaRevealAwardContext& Award)
{
	if (RejectIfOpen(TEXT("스킨")))
	{
		return;
	}
	const UTableManagerSubsystem* Tables = ResolveTables();
	AwardContext = Award;
	EquipmentInstanceIds.Reset();
	bSkinDuplicates.Reset();
	RevealEmployeeId = INDEX_NONE;
	TArray<FGachaRevealEntry> NewEntries;
	TArray<FGachaRevealCardData> NewCards;
	TArray<FText> NewMeta;
	for (const FBuildingSkinGachaResult& Result : Results)
	{
		bool bFound = false;
		const FBuildingSkinData Row = Tables ? Tables->GetBuildingSkinData(Result.ResultSkinID, bFound) : FBuildingSkinData();
		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 스킨 표 행 없음: %d"), Result.ResultSkinID);
			continue;
		}
		FGachaRevealEntry& Entry = NewEntries.AddDefaulted_GetRef();
		Entry.PullIndex = NewEntries.Num() - 1;
		Entry.Grade = GachaRevealWidgetDetail::TierOf(Result.Rarity);
		Entry.bNew = !Result.bDuplicate;
		Entry.MileageGained = Result.bDuplicate ? Result.MileageGained : 0;
		bSkinDuplicates.Add(Result.bDuplicate);
		FGachaRevealCardData& Card = NewCards.AddDefaulted_GetRef();
		Card.Kind = EGachaRevealKind::Skin;
		Card.Grade = Entry.Grade;
		Card.Name = Row.DisplayName;
		Card.Icon = Row.Icon;
		Card.bNew = Entry.bNew;
		Card.MileageGained = Entry.MileageGained;
		NewMeta.Add(NSLOCTEXT("GachaReveal", "SkinNoun", "스킨"));
	}
	const FText Title = FText::Format(NSLOCTEXT("GachaReveal", "Title", "{0} {1} 뽑기 · {2}회"),
		bInAdvanced ? NSLOCTEXT("GachaReveal", "TierAdvanced", "고급") : NSLOCTEXT("GachaReveal", "TierNormal", "일반"),
		NSLOCTEXT("GachaReveal", "SkinNoun", "스킨"), FText::AsNumber(NewEntries.Num()));
	BeginReveal(EGachaRevealKind::Skin, bInAdvanced ? EGachaBoxType::SkinAdvanced : EGachaBoxType::SkinNormal, Title, NewEntries, NewCards, NewMeta);
}

void UGachaRevealPresentationWidget::BeginReveal(EGachaRevealKind InKind, EGachaBoxType InBoxType, const FText& InTitle,
	const TArray<FGachaRevealEntry>& InEntries, const TArray<FGachaRevealCardData>& InCards, const TArray<FText>& InMeta)
{
	ResolveExtraWidgets();
	Runner.OnBeatStarted = [this](int32 Index, const FGachaRevealBeatSpec& Beat) { HandleBeatStarted(Index, Beat); };
	if (InEntries.IsEmpty())
	{
		// 지급 0건(리플레이·전부 표 누락) ― 빈 화면을 띄우지 않고 바로 닫힘을 알린다
		UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 보여 줄 결과가 없어 공개 화면을 닫음"));
		bClosing = true;
		FinishClose();
		return;
	}
	const bool bReduce = ResolveReduceMotion();
	if (!Runner.Setup(InEntries, bReduce, IsHandOpenBoxSetting(this), bUpgrade))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 박자 실행기가 Setup 을 거절함(실행 중)"));
		return;
	}
	Kind = InKind;
	BoxType = InBoxType;
	bEquipmentReveal = InKind == EGachaRevealKind::Equipment;
	bReduceMotionActive = bReduce;
	Entries = InEntries;
	CardData = InCards;
	const UTableManagerSubsystem* IconTables = ResolveTables();
	for (FGachaRevealCardData& Card : CardData)
	{
		if (Card.Icon.IsNull() || Card.IconUVRegion.IsSet()) continue;
		const FGachaIconBoundsRow* Bounds = IconTables ? IconTables->GetGachaIconBounds(Card.Icon.ToSoftObjectPath()) : nullptr;
		if (Bounds) Card.IconUVRegion = Bounds->UVRegion();
		else if (!IconTables) UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 아이콘 경계 표 없음: %s"), *Card.Icon.ToString());
	}
	EntryMeta = InMeta;
	bRecommendedEquipped = false;
	Recommended.Reset();
	if (bEquipmentReveal)
	{
		for (int32 Index : GachaRevealModel::PickRecommended(Entries))
		{
			Recommended.Add(Index);
		}
	}
	for (int32 Index = 0; Index < CardData.Num(); ++Index)
	{
		CardData[Index].GradeText = GradeLabel(CardData[Index].Grade);
		CardData[Index].bRecommended = Recommended.Contains(Index);
	}
	SortMode = EGachaRevealSort::Grade;
	ActiveBeat = INDEX_NONE;
	LastEventMs = -1.f;
#if WITH_DEV_AUTOMATION_TESTS
	TestEventCounts.Reset();
#endif
	SetVisibility(ESlateVisibility::Visible);
	SetRenderOpacity(1.f);
	ResetStage();
	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(InTitle);
	}

	if (!FX)
	{
		FX = NewObject<UGachaRevealFXLayer>(this);
	}
	if (!FX->IsReady())
	{
		// 여는 연출 동안 한 번(이미지 330장·텍스처 13장) ― 첫 효과 때 만들면 착지 순간 끊긴다(Task 9 §6 규칙 7)
		FX->Init(FXLayer, UnderCanvas, PillarCanvas);
		FX->SetShakeTarget(ContentRoot);
		FX->AddShakeTarget(SpotlightRoot);
	}
	FX->Clear();
	FX->SetIntensity(SparkScale, bReduce ? 0.f : ShakeScale, 1.f);

	if (BoxWidget)
	{
		BoxWidget->SetBoxType(BoxType);
		BoxWidget->SetVisibility(bReduce ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	CreateDealtCards();
	TallyCounts.Init(0, GachaRevealModel::MaxGrade + 1);
	TallyBumpClockMs.Init(-1.f, GachaRevealModel::MaxGrade + 1);
	if (!bReduce)
	{
		// 첫 틱 전 한 프레임이라도 상자가 착지점에 보이지 않게 출발 자세를 먼저 깐다
		UpdateThrow(0.f);
	}
}

void UGachaRevealPresentationWidget::ResetStage()
{
	Fades.Reset();
	ClearStamps();
	bSlitOn = false;
	bBlade = false;
	SlitProgress = 0.f;
	bHandDragging = false;
	bHandCutDone = false;
	HandRealMs = 0.f;
	DealClockStartMs = LidClockStartMs = SpotlightReleaseClockMs = SettleClockStartMs = ResultClockStartMs = InfoClockMs = -1.f;
	CurrentBox = FBoxPose();
	auto Show = [](UWidget* Widget, ESlateVisibility NewVisibility, float Opacity)
	{
		if (Widget)
		{
			Widget->SetVisibility(NewVisibility);
			Widget->SetRenderOpacity(Opacity);
			Widget->SetRenderTranslation(FVector2D::ZeroVector);
		}
	};
	Show(DimBG, ESlateVisibility::Visible, 0.f);
	if (ContentRoot) { ContentRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible); ContentRoot->SetRenderOpacity(1.f); }
	if (CardLayer) { CardLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible); CardLayer->SetRenderOpacity(1.f); }
	if (StampLayer) { StampLayer->SetRenderOpacity(1.f); }
	for (UWidget* Panel : { static_cast<UWidget*>(HeaderTitle.Get()), SortToggle.Get(), static_cast<UWidget*>(TallyRow.Get()), static_cast<UWidget*>(BestPanel.Get()), SinglePanel.Get(), static_cast<UWidget*>(BottomBar.Get()) })
	{
		Show(Panel, ESlateVisibility::Collapsed, 1.f);
	}
	if (SpotlightRoot) { SpotlightRoot->SetVisibility(ESlateVisibility::Collapsed); SpotlightRoot->SetRenderOpacity(1.f); }
	Show(ShadeImage, ESlateVisibility::Visible, 0.f);
	Show(SpotContentPanel, ESlateVisibility::SelfHitTestInvisible, 0.f);
	Show(SpotlightInfo, ESlateVisibility::SelfHitTestInvisible, 0.f);
	if (SpotlightCard)
	{
		SpotlightCard->SetRenderTransform(FWidgetTransform());
		SpotlightCard->SetRenderOpacity(0.f);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SpotlightCard->Slot))
		{
			const FGachaRevealRect Rect = GachaRevealWidgetDetail::SpotlightRect(StageWidth(), false);
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetPosition(FVector2D(Rect.X, Rect.Y));
			CanvasSlot->SetSize(FVector2D(Rect.W, Rect.H));
		}
	}
	Show(SkipButton, ESlateVisibility::Collapsed, 0.f);
	Show(HandGuide, ESlateVisibility::Collapsed, 1.f);
	Show(HandTextBlock, ESlateVisibility::Collapsed, 0.f);
	Show(ConfirmButton, ESlateVisibility::Hidden, 0.f);
	if (WidgetTree)
	{
		Show(WidgetTree->FindWidget(TEXT("TallySummary")), ESlateVisibility::Collapsed, 1.f);
		for (const TCHAR* Name : { TEXT("SummaryBestGrade"), TEXT("SummaryNew"), TEXT("SummaryRecommend"), TEXT("SummaryMileage") })
		{
			if (UTextBlock* Text = Cast<UTextBlock>(WidgetTree->FindWidget(FName(Name))))
			{
				Text->SetText(FText::GetEmpty());
				Text->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	for (int32 Grade = 0; Grade <= GachaRevealModel::MaxGrade; ++Grade)
	{
		if (TallyCellWidgets.IsValidIndex(Grade) && TallyCellWidgets[Grade]) { TallyCellWidgets[Grade]->SetRenderOpacity(1.f); }
		if (TallyLatinTexts.IsValidIndex(Grade) && TallyLatinTexts[Grade]) { TallyLatinTexts[Grade]->SetRenderOpacity(.4f); }
		if (TallyKoreanTexts.IsValidIndex(Grade) && TallyKoreanTexts[Grade]) { TallyKoreanTexts[Grade]->SetRenderOpacity(.4f); }
		if (TallyCountTexts.IsValidIndex(Grade) && TallyCountTexts[Grade])
		{
			TallyCountTexts[Grade]->SetText(FText::AsNumber(0));
			TallyCountTexts[Grade]->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			TallyCountTexts[Grade]->SetRenderOpacity(.28f);
			TallyCountTexts[Grade]->SetRenderScale(FVector2D(1.f));
		}
	}
}

void UGachaRevealPresentationWidget::CreateDealtCards()
{
	ClearDealtCards();
	CardMotion.SetNum(Entries.Num());
	const UTableManagerSubsystem* Tables = ResolveTables();
	const TSubclassOf<UUserWidget> CardClass = Tables ? Tables->GetWidgetClass(EWidgetType::GachaRevealCard) : nullptr;
	if (!CardClass || !CardClass->IsChildOf(UGachaRevealCardWidget::StaticClass()) || !CardLayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] EWidgetType::GachaRevealCard 미등록 ― 카드 없이 진행"));
		return;
	}
	const TArray<FGachaRevealRect> Slots = GachaRevealModel::ComputeSlots(Entries.Num(), FMath::RoundToFloat(StageWidth()), false);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		UGachaRevealCardWidget* Card = CreateWidget<UGachaRevealCardWidget>(this, CardClass);
		if (!Card)
		{
			DealtCards.Add(nullptr);
			continue;
		}
		Card->Configure(CardData.IsValidIndex(Index) ? CardData[Index] : FGachaRevealCardData(), DealtCardSize());
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
		Card->SetRenderOpacity(0.f);
		if (UCanvasPanelSlot* CanvasSlot = CardLayer->AddChildToCanvas(Card))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetAutoSize(false);
			if (Slots.IsValidIndex(Index))
			{
				CanvasSlot->SetPosition(FVector2D(Slots[Index].X, Slots[Index].Y));
				CanvasSlot->SetSize(FVector2D(Slots[Index].W, Slots[Index].H));
			}
		}
		if (UWidget* Visual = Card->GetVisualRoot())
		{
			Visual->SetRenderTransformPivot(FVector2D(.5f, .5f));
		}
		if (Slots.IsValidIndex(Index)) { CardMotion[Index].Cell = Slots[Index]; }
		DealtCards.Add(Card);
	}
}

void UGachaRevealPresentationWidget::ClearDealtCards()
{
	for (UGachaRevealCardWidget* Card : DealtCards)
	{
		if (Card) { Card->RemoveFromParent(); }
	}
	DealtCards.Reset();
	CardMotion.Reset();
}

// ============================================================================
// 박자
// ============================================================================

void UGachaRevealPresentationWidget::HandleBeatStarted(int32 Index, const FGachaRevealBeatSpec& Beat)
{
	if (ActiveBeat != INDEX_NONE && ActiveBeat != Index)
	{
		// 한 틱에 여러 박자를 지나도 앞 박자의 1회성 사건(뚜껑 소리·착지 흔들림·도장 타격)을 빠뜨리지 않는다. 뒤로가기 건너뛰기는 소리를 내지 않는다
		// 정리 다음이 Result 라 뒤로가기도 번호로는 순차다 ― 뛰는 중이면 번호와 상관없이 비순차로 친다
		const bool bSequential = !bJumpingToResult && Index == ActiveBeat + 1;
		const TArray<FGachaRevealBeatSpec>& Beats = Runner.GetPlan().Beats;
		if (bSequential && Beats.IsValidIndex(ActiveBeat))
		{
			EventNowMs = Beats[ActiveBeat].DurationMs;
			FireBeatEvents(ActiveBeat, LastEventMs, Beats[ActiveBeat].DurationMs);
		}
		FinishBeat(ActiveBeat);
	}
	ActiveBeat = Index;
	LastEventMs = -1.f;
	EventNowMs = 0.f;
	if (Beat.Beat == EGachaRevealBeat::Result)
	{
		EnterResultState();
	}
}

void UGachaRevealPresentationWidget::FinishBeat(int32 Index)
{
	const TArray<FGachaRevealBeatSpec>& Beats = Runner.GetPlan().Beats;
	if (!Beats.IsValidIndex(Index))
	{
		return;
	}
	switch (Beats[Index].Beat)
	{
	case EGachaRevealBeat::Throw:
		UpdateThrow(Beats[Index].DurationMs);
		break;
	case EGachaRevealBeat::Cut:
		bBlade = false;
		SlitProgress = 1.f;
		break;
	case EGachaRevealBeat::Deal:
		// 시안 cascade() 첫머리 ― S 이상은 뒤집지 않고 뒷면 빛만 숨쉰다(1회 뽑기도 같다)
		for (int32 CardIndex = 0; CardIndex < DealtCards.Num(); ++CardIndex)
		{
			if (DealtCards[CardIndex] && Entries.IsValidIndex(CardIndex) && Entries[CardIndex].Grade >= GachaRevealModel::HighGrade)
			{
				DealtCards[CardIndex]->SetHoldGlow(true);
			}
		}
		break;
	case EGachaRevealBeat::Spotlight:
	{
		if (SpotlightRoot) { SpotlightRoot->SetVisibility(ESlateVisibility::Collapsed); }
		if (SpotlightCard) { SpotlightCard->SetRenderOpacity(0.f); SpotlightCard->SetRenderTransform(FWidgetTransform()); }
		const int32 Top = Runner.GetPlan().TopIndex;
		if (Runner.GetPlan().Layout == EGachaRevealLayout::Single && DealtCards.IsValidIndex(Top) && DealtCards[Top])
		{
			// 1회 뽑기는 돌아가지 않고 단독 장면 자리의 카드가 결과 카드가 된다(시안 :1036)
			CardMotion[Top].Cell = GachaRevealWidgetDetail::SpotlightRect(StageWidth(), true);
			CardMotion[Top].bHidden = false;
			CardMotion[Top].bFaceUp = true;
			CardMotion[Top].FlipStartMs = -1.f;
			DealtCards[Top]->Configure(CardData[Top], EGachaCardSize::Single);
			DealtCards[Top]->SetFaceUp(true);
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DealtCards[Top]->Slot))
			{
				CanvasSlot->SetPosition(FVector2D(CardMotion[Top].Cell.X, CardMotion[Top].Cell.Y));
				CanvasSlot->SetSize(FVector2D(CardMotion[Top].Cell.W, CardMotion[Top].Cell.H));
			}
		}
		break;
	}
	default:
		break;
	}
}

void UGachaRevealPresentationWidget::StepTick(float DeltaSeconds)
{
	if (bClosing)
	{
		TickClose(DeltaSeconds);
		return;
	}
	if (!Runner.IsRunning())
	{
		return;
	}
	Runner.Advance(DeltaSeconds);
	if (!Runner.IsRunning() || bClosing)
	{
		return;
	}
	HandRealMs += DeltaSeconds * 1000.f;
	UpdateStage();
	if (FX)
	{
		FX->Tick(DeltaSeconds);
	}
}

void UGachaRevealPresentationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Width = MyGeometry.GetLocalSize().X;
	if (Width > 1.f)
	{
		LastGeometryWidth = Width;
	}
	StepTick(InDeltaTime);
}

void UGachaRevealPresentationWidget::DebugAdvanceTime(float Seconds)
{
	// 실행기는 한 번에 0.1초까지만 쓴다 ― 프레임처럼 잘게 나눠야 화면 갱신이 박자마다 돈다
	while (Seconds > 0.f)
	{
		const float Step = FMath::Min(Seconds, 1.f / 60.f);
		StepTick(Step);
		Seconds -= Step;
	}
}

void UGachaRevealPresentationWidget::DebugSkipToResult()
{
	JumpToResult();
}

void UGachaRevealPresentationWidget::JumpToResult()
{
	TGuardValue<bool> JumpGuard(bJumpingToResult, true);
	if (Runner.SkipToResult())
	{
		UpdateStage();
	}
}

void UGachaRevealPresentationWidget::UpdateStage()
{
	const FGachaRevealBeatSpec* Spec = Runner.GetCurrentBeat();
	if (!Spec)
	{
		return;
	}
	const float T = Runner.GetBeatTimeMs();
	EventNowMs = T;
	if (T > LastEventMs)
	{
		FireBeatEvents(Runner.GetBeatIndex(), LastEventMs, T);
		LastEventMs = T;
	}
	switch (Spec->Beat)
	{
	case EGachaRevealBeat::Throw: UpdateThrow(T); break;
	case EGachaRevealBeat::Cut: UpdateCut(T); break;
	case EGachaRevealBeat::Upgrade: UpdateUpgrade(T); break;
	case EGachaRevealBeat::Lid: UpdateLid(T); break;
	case EGachaRevealBeat::Deal: UpdateDeal(T); break;
	case EGachaRevealBeat::Cascade: UpdateCascade(T); break;
	case EGachaRevealBeat::Stamp: UpdateStamp(T); break;
	case EGachaRevealBeat::Spotlight: UpdateSpotlight(T); break;
	case EGachaRevealBeat::Settle: UpdateSettle(T); break;
	case EGachaRevealBeat::Result: UpdateResult(T); break;
	}
	UpdateBoxLid();
	UpdateCards();
	UpdateStamps();
	UpdateFades();
	UpdateTallyBumps();
	UpdateHandGuide();
	if (FX)
	{
		FX->SetSlit(bSlitOn, BoxPoint(EGachaSeam::Left), BoxPoint(EGachaSeam::Right), SlitProgress, SlitColor, SlitIntensity, bBlade);
	}
}

void UGachaRevealPresentationWidget::FireBeatEvents(int32 Index, float FromMs, float ToMs)
{
	using namespace GachaRevealWidgetDetail;
	const FGachaRevealPlan& Plan = Runner.GetPlan();
	if (!Plan.Beats.IsValidIndex(Index))
	{
		return;
	}
	const FGachaRevealBeatSpec& Spec = Plan.Beats[Index];
	auto Crossed = [FromMs, ToMs](float AtMs) { return FromMs < AtMs && AtMs <= ToMs; };
	const float W = StageWidth();
	const int32 Best = FMath::Max(0, Plan.BestGrade);

	switch (Spec.Beat)
	{
	case EGachaRevealBeat::Throw:
		if (Crossed(0.f))
		{
			ShowThrowDim(AnchorAt(0.f));
			ShowFade(SkipButton, true, GT::SkipShowFade, ESlateVisibility::Visible, AnchorAt(0.f));
		}
		if (Crossed(GT::ThrowLift + GT::ThrowExit))
		{
			const float HeaderAt = AnchorAt(GT::ThrowLift + GT::ThrowExit);
			ShowFade(HeaderTitle, true, GT::ThrowHeaderFade, ESlateVisibility::SelfHitTestInvisible, HeaderAt);
			if (Plan.Count > 1) { ShowFade(TallyRow, true, GT::ThrowTallyFade, ESlateVisibility::SelfHitTestInvisible, HeaderAt); }
		}
		if (Crossed(GT::ThrowLift + GT::ThrowExit + GT::ThrowDrop))
		{
			// 시안 :807-808 착지 ― 소리·흔들림 16·양옆 먼지
			PlayTag(CGUISoundTags::GachaBoxLand);
			Shake(16.f);
			const FLinearColor Dust = Hex(TEXT("#C9A27A"));
			Sparks(FVector2D(W / 2.f - 250.f, 1000.f), 6, Dust, PI * .9f, PI * 1.15f, 300.f, 700.f, .25f, .45f, 6.f, 12.f, 300.f);
			Sparks(FVector2D(W / 2.f + 250.f, 1000.f), 6, Dust, -PI * .15f, PI * .1f, 300.f, 700.f, .25f, .45f, 6.f, 12.f, 300.f);
		}
		break;

	case EGachaRevealBeat::Cut:
	{
		const float CutRunMs = Plan.bHandOpen ? 0.f : GT::Cut;
		if (Crossed(0.f))
		{
			const int32 Leak = Plan.UpgradeSteps.Num() > 0 ? Plan.UpgradeSteps[0] : INDEX_NONE;
			bSlitOn = true;
			bBlade = true;
			SlitProgress = 0.f;
			SlitColor = LeakColor(Leak);
			SlitIntensity = Leak < 0 ? .55f : .85f;
			if (!Plan.bHandOpen)
			{
				PlayTag(CGUISoundTags::GachaBoxZip);
			}
			else
			{
				ShowFade(HandTextBlock, true, GT::HandTextShow, ESlateVisibility::HitTestInvisible, AnchorAt(0.f));
				if (HandGuide) { HandGuide->SetGesture(EGestureHintKind::Drag); HandGuide->SetVisibility(ESlateVisibility::HitTestInvisible); }
			}
		}
		if (!Plan.bHandOpen && Crossed(CutRunMs))
		{
			CompleteHandCutEvent();
		}
		// 승급을 끈 노브: 틈 빛이 마지막 쉼 140 시작에 최고 등급 색으로 바뀐다 ― 최고가 A 이하(단계 없음)면 기본 색 그대로(시안 :832·:814)
		if (!Plan.bUpgrade && Plan.UpgradeSteps.Num() > 0 && Crossed(Spec.DurationMs - GT::UpgradeTail))
		{
			SlitColor = LeakColor(Plan.UpgradeSteps.Last());
		}
		break;
	}

	case EGachaRevealBeat::Upgrade:
	{
		const int32 Steps = FMath::Max(0, Plan.UpgradeSteps.Num() - 1);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			if (!Crossed(Step * (GT::UpgradeGap + GT::UpgradePress) + GT::UpgradeGap))
			{
				continue;
			}
			const int32 Level = Step + 1;
			PlayTag(Level == 1 ? CGUISoundTags::GachaBoxJolt1 : CGUISoundTags::GachaBoxJolt2);
			Shake(10.f + Level * 4.f);
			SlitColor = LeakColor(Plan.UpgradeSteps[Level]);
			SlitIntensity = 1.f;
			const FVector2D Mid = (BoxPoint(EGachaSeam::Left) + BoxPoint(EGachaSeam::Right)) * .5f;
			Sparks(Mid, 18, SlitColor, -PI, 0.f, 600.f, 1500.f, .18f, .42f, 3.f, 7.f, 1400.f, 300.f);
			if (FX) { FX->Glow(Mid, 420.f, .5f, SlitColor, .7f); }
		}
		break;
	}

	case EGachaRevealBeat::Lid:
		if (Crossed(0.f))
		{
			// 시안 openLid :897-914
			const FLinearColor Col = Best >= GachaRevealModel::HighGrade ? FxColor(Best) : (Best == 2 ? FxColor(2) : Hex(TEXT("#E9F1FF")));
			LidGlowColor = Col;
			LidClockStartMs = AnchorAt(0.f);
			PlayTag(Best >= GachaRevealModel::HighGrade ? CGUISoundTags::GachaBoxLidHigh : CGUISoundTags::GachaBoxLid);
			bSlitOn = false;
			bBlade = false;
			const FVector2D Top = BoxPoint(EGachaSeam::Top);
			const bool bHigh = Best >= GachaRevealModel::HighGrade;
			if (FX)
			{
				FX->Beam(Top, bHigh ? 520.f : 300.f, Col, bHigh ? 1.f : .55f, ScaledSec(bHigh ? GT::LidBeamHigh : GT::LidBeamLow));
			}
			static const int32 LidSparks[] = { 10, 12, 16, 26, 40, 56 };
			Sparks(Top, LidSparks[FMath::Clamp(Best, 0, 5)], Col, -PI * .85f, -PI * .15f, 900.f, 2200.f, .2f, .5f, 3.f, 7.f, 1400.f, 220.f);
			if (bHigh)
			{
				if (FX)
				{
					FX->Curtain(Col, .62f, ScaledSec(GT::LidCurtain), EGachaCurtainShape::Lid);
					FX->Ring(Top, 60.f, 900.f, .5f, Col, 14.f);
				}
				Shake(14.f + (Best - 3) * 6.f);
			}
			else
			{
				Shake(6.f);
			}
			if (FX) { FX->Glow(Top, 500.f, .6f, Col, .8f); }
		}
		break;

	case EGachaRevealBeat::Deal:
		if (Crossed(0.f))
		{
			DealClockStartMs = AnchorAt(0.f);
			DealOrigin = BoxPoint(EGachaSeam::Top);
			PlayTag(Plan.Layout == EGachaRevealLayout::Single ? CGUISoundTags::GachaBoxDeal1
				: (Plan.Layout == EGachaRevealLayout::Thirty ? CGUISoundTags::GachaBoxDeal30 : CGUISoundTags::GachaBoxDeal10));
			const TArray<FGachaRevealRect> Slots = GachaRevealModel::ComputeSlots(Plan.Count, FMath::RoundToFloat(W), false);
			for (int32 CardIndex = 0; CardIndex < CardMotion.Num(); ++CardIndex)
			{
				if (Slots.IsValidIndex(CardIndex)) { CardMotion[CardIndex].Cell = Slots[CardIndex]; }
				CardMotion[CardIndex].DealRotation = FMath::FRandRange(-25.f, 25.f);
			}
		}
		break;

	case EGachaRevealBeat::Cascade:
	{
		const bool bSingle = Plan.Layout == EGachaRevealLayout::Single;
		if (bSingle)
		{
			// 1회·A 이하 = 시안 single() :1038
			if (Crossed(0.f)) { FlipCard(0, GT::SingleFlip, AnchorAt(0.f)); }
			if (Crossed(GT::SingleFlip) && Entries.IsValidIndex(0))
			{
				const int32 G = Entries[0].Grade;
				if (DealtCards.IsValidIndex(0) && DealtCards[0]) { DealtCards[0]->PlayFlash(.8f); }
				if (G == 2)
				{
					Sparks(CardCenter(0), 14, FxColor(2), 0.f, UE_TWO_PI, 700.f, 1800.f);
					PlayTag(CGUISoundTags::GachaBoxFlipA);
				}
				else
				{
					PlayTok(1.36f);
				}
				BumpTally(G, AnchorAt(GT::SingleFlip));
			}
			break;
		}
		const float Stagger = Plan.Layout == EGachaRevealLayout::Thirty ? GT::CascadeStagger30 : GT::CascadeStagger10;
		int32 LowOrder = 0;
		for (int32 CardIndex = 0; CardIndex < Entries.Num(); ++CardIndex)
		{
			const int32 G = Entries[CardIndex].Grade;
			if (G >= GachaRevealModel::HighGrade)
			{
				continue;
			}
			const float At = LowOrder * Stagger;
			++LowOrder;
			if (Crossed(At)) { FlipCard(CardIndex, GT::CascadeFlip, AnchorAt(At)); }
			if (!Crossed(At + GT::CascadeBumpDelay))
			{
				continue;
			}
			BumpTally(G, AnchorAt(At + GT::CascadeBumpDelay));
			if (G == 2)
			{
				const FVector2D C = CardCenter(CardIndex);
				Sparks(C, 8, FxColor(2), 0.f, UE_TWO_PI, 400.f, 1000.f, .12f, .28f, 2.f, 4.f);
				if (FX && CardMotion.IsValidIndex(CardIndex)) { FX->Glow(C, CardMotion[CardIndex].Cell.W * .9f, .35f, FxColor(2), .6f); }
				if (DealtCards.IsValidIndex(CardIndex) && DealtCards[CardIndex]) { DealtCards[CardIndex]->PlayFlash(.55f); }
				PlayTag(CGUISoundTags::GachaBoxFlipA);
			}
			else
			{
				PlayTok(G > 0 ? 1.f : .77f);
			}
		}
		break;
	}

	case EGachaRevealBeat::Stamp:
	{
		if (!Plan.StampGroups.IsValidIndex(Spec.Group))
		{
			break;
		}
		const int32 G = Plan.StampGroups[Spec.Group].Key;
		const TArray<int32>& Members = Plan.StampGroups[Spec.Group].Value;
		if (Crossed(0.f))
		{
			for (int32 Member : Members)
			{
				if (DealtCards.IsValidIndex(Member) && DealtCards[Member]) { DealtCards[Member]->PlayHoldRise(GT::StampHot); }
			}
			PlayTag(CGUISoundTags::GachaBoxRiseStamp);
		}
		if (Crossed(GT::StampRise))
		{
			for (int32 Order = 0; Order < Members.Num(); ++Order)
			{
				const int32 Member = Members[Order];
				if (DealtCards.IsValidIndex(Member) && DealtCards[Member]) { DealtCards[Member]->SetHoldGlow(false); }
				const float SlotW = CardMotion.IsValidIndex(Member) ? CardMotion[Member].Cell.W : 0.f;
				SpawnStamp(CardCenter(Member), G, SlotW * (Plan.Layout == EGachaRevealLayout::Thirty ? .55f : .5f),
					AnchorAt(GT::StampRise + Order * GT::StampGap), false, false);
			}
		}
		for (int32 Order = 0; Order < Members.Num(); ++Order)
		{
			if (!Crossed(GT::StampRise + Order * GT::StampGap))
			{
				continue;
			}
			const int32 Member = Members[Order];
			const FVector2D C = CardCenter(Member);
			const float SlotW = CardMotion.IsValidIndex(Member) ? CardMotion[Member].Cell.W : 0.f;
			FlipCard(Member, GT::StampFlip, AnchorAt(GT::StampRise + Order * GT::StampGap));
			if (DealtCards.IsValidIndex(Member) && DealtCards[Member]) { DealtCards[Member]->PlayFlash(.9f); }
			Sparks(C, 14, FxColor(G), 0.f, UE_TWO_PI, 700.f, 1700.f);
			if (FX) { FX->Ring(C, SlotW * .4f, SlotW * 1.3f, .38f, FxColor(G), 8.f); }
			BumpTally(G, AnchorAt(GT::StampRise + Order * GT::StampGap));
		}
		if (Crossed(GT::StampRise + GT::StampHitDelay))
		{
			// 묶음당 소리 1번·흔들림 1번(설계서 §4-7)
			PlayTag(CGUISoundTags::GachaBoxStamp);
			Shake(12.f + (G - 3) * 5.f);
		}
		break;
	}

	case EGachaRevealBeat::Spotlight:
	{
		const int32 Top = Plan.TopIndex;
		if (!Entries.IsValidIndex(Top))
		{
			break;
		}
		const int32 G = Entries[Top].Grade;
		const bool bSingle = Plan.Layout == EGachaRevealLayout::Single;
		const float Rise = G >= 5 ? GT::SpotlightRiseSSS : (G >= 4 ? GT::SpotlightRiseSS : GT::SpotlightRiseS);
		const float ThrowAt = FMath::Max(0.f, Rise - GT::SpotlightThrow);
		const float LandAt = ThrowAt + GT::SpotlightThrow;
		const float InfoAt = LandAt + GT::SpotlightPress + (G >= 5 ? GT::SpotlightSSSHitDelay : 0.f);
		const float ReleaseAt = Spec.WaitAtMs + Spec.WaitTimeoutMs;
		const FGachaRevealRect SpotRect = SpotlightRect(W, bSingle);
		const FVector2D C(SpotRect.X + SpotRect.W / 2.f, SpotRect.Y + SpotRect.H / 2.f);
		const FLinearColor Fx = FxColor(G);
		if (Crossed(0.f))
		{
			if (SpotlightRoot) { SpotlightRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
			ShowFade(ShadeImage, true, GT::SpotlightDim, ESlateVisibility::Visible, AnchorAt(0.f));
			ShowFade(SpotContentPanel, true, GT::SpotlightDim, ESlateVisibility::SelfHitTestInvisible, AnchorAt(0.f));
			if (SpotlightInfo) { SpotlightInfo->SetRenderOpacity(0.f); }
			if (SpotlightCard)
			{
				SpotlightCard->Configure(CardData[Top], bSingle ? EGachaCardSize::Single : EGachaCardSize::Spot);
				SpotlightCard->SetFaceUp(true);
				SpotlightCard->SetRenderOpacity(0.f);
				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SpotlightCard->Slot))
				{
					CanvasSlot->SetPosition(FVector2D(SpotRect.X, SpotRect.Y));
					CanvasSlot->SetSize(FVector2D(SpotRect.W, SpotRect.H));
				}
			}
			FillSpotlightInfo(Top);
			if (DealtCards.IsValidIndex(Top) && DealtCards[Top]) { DealtCards[Top]->PlayHoldRise(GT::SpotlightDim); }
			PlayTag(G >= 5 ? CGUISoundTags::GachaBoxRiseSSS : (G >= 4 ? CGUISoundTags::GachaBoxRiseSS : CGUISoundTags::GachaBoxRiseS));
		}
		if (Crossed(ThrowAt) && SpotlightCard)
		{
			SpotlightCard->SetRenderOpacity(1.f);
		}
		if (Crossed(LandAt))
		{
			// 시안 :997-1007 박힘
			PlayTag(G >= 5 ? CGUISoundTags::GachaBoxHitSSS : (G >= 4 ? CGUISoundTags::GachaBoxHitSS : CGUISoundTags::GachaBoxHitS));
			Shake(G >= 5 ? 32.f : (G >= 4 ? 24.f : 16.f));
			if (SpotlightCard) { SpotlightCard->PlayFlash(1.f); }
			if (FX)
			{
				FX->Ring(C, 200.f, G >= 4 ? 1300.f : 900.f, .55f, Fx, G >= 4 ? 18.f : 12.f);
				FX->Glow(C, 900.f, .7f, Fx, .8f);
			}
			Sparks(C, G >= 5 ? 70 : (G >= 4 ? 50 : 32), Fx, 0.f, UE_TWO_PI, 1100.f, 2800.f, .22f, .55f, 4.f, 9.f, 1400.f, 200.f);
			if (G >= 4 && FX)
			{
				FX->Curtain(Fx, G >= 5 ? .7f : .55f, ScaledSec(GT::SpotlightCurtain), EGachaCurtainShape::Spot);
				FX->PillarOn(FVector2D(C.X, StageHeight), Fx, ScaledSec(GT::SpotlightPillarRise), ScaledSec(GT::SpotlightPillarBreatheDelay));
			}
		}
		if (G >= 5 && Crossed(LandAt + GT::SpotlightPress + GT::SpotlightSSSHitDelay))
		{
			Shake(22.f);
			Sparks(C, 40, Hex(TEXT("#FFE2B0")), 0.f, UE_TWO_PI, 1200.f, 2600.f, .2f, .45f, 3.f, 7.f, 1400.f, 300.f);
			if (FX) { FX->Ring(C, 300.f, 1500.f, .6f, Hex(TEXT("#FFD8A0")), 14.f); }
		}
		if (Crossed(InfoAt))
		{
			SpawnStamp(FVector2D(W / 2.f + 20.f, SpotTop + 80.f), G, IsKoreanGrades() ? 120.f : 190.f, AnchorAt(InfoAt + GT::SpotlightStampDelay), true, true);
		}
		if (Crossed(InfoAt + GT::SpotlightStampSoundDelay))
		{
			PlayTag(CGUISoundTags::GachaBoxStamp);
			InfoClockMs = AnchorAt(InfoAt + GT::SpotlightInfoDelay);
			BumpTally(G, AnchorAt(InfoAt + GT::SpotlightStampSoundDelay));
			if (bSingle && CardMotion.IsValidIndex(Top)) { CardMotion[Top].bHidden = true; }
		}
		if (Crossed(ReleaseAt))
		{
			SpotlightReleaseClockMs = AnchorAt(ReleaseAt);
			for (FStampMotion& Stamp : StampMotions)
			{
				if (Stamp.FadeStartMs < 0.f) { Stamp.FadeStartMs = SpotlightReleaseClockMs; }
			}
			ShowFade(SpotlightInfo, false, GT::SpotlightOutFade, ESlateVisibility::SelfHitTestInvisible, SpotlightReleaseClockMs);
			if (FX) { FX->PillarOff(ScaledSec(GT::SpotlightPillarOut)); }
		}
		const float UndimAt = ReleaseAt + (bSingle ? 0.f : GT::SpotlightReturn);
		if (!bSingle && Crossed(UndimAt) && DealtCards.IsValidIndex(Top) && DealtCards[Top])
		{
			DealtCards[Top]->SetHoldGlow(false);
			DealtCards[Top]->SetFaceUp(true);
			DealtCards[Top]->PlayFlash(.8f);
			CardMotion[Top].bFaceUp = true;
			CardMotion[Top].FlipStartMs = -1.f;
			PlayTok(1.36f);
			if (SpotlightCard) { SpotlightCard->SetRenderOpacity(0.f); }
		}
		if (Crossed(UndimAt))
		{
			ShowFade(ShadeImage, false, GT::SpotlightUndim, ESlateVisibility::Visible, AnchorAt(UndimAt));
			ShowFade(SpotContentPanel, false, GT::SpotlightUndim, ESlateVisibility::SelfHitTestInvisible, AnchorAt(UndimAt));
		}
		break;
	}

	case EGachaRevealBeat::Settle:
		if (Crossed(0.f))
		{
			SettleClockStartMs = AnchorAt(0.f);
			ShowFade(SkipButton, false, GT::SkipHideFade, ESlateVisibility::Visible, SettleClockStartMs);
		}
		if (Crossed(GT::SettleWait))
		{
			ApplySettledLayout(true);
			if (Plan.Count > 1) { ShowFade(SortToggle, true, GT::SettleSortToggleFade, ESlateVisibility::SelfHitTestInvisible, AnchorAt(GT::SettleWait)); }
			UWidget* BestWidget = Plan.Count > 1 ? static_cast<UWidget*>(BestPanel.Get()) : SinglePanel.Get();
			if (BestWidget) { BestWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible); BestWidget->SetRenderOpacity(1.f); }
		}
		if (Crossed(GT::SettleWait + GT::SettleBest))
		{
			PlayTag(CGUISoundTags::GachaBoxSettle);
			Shake(5.f);
			if (BottomBar) { BottomBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
		}
		break;

	default:
		break;
	}
}

// ============================================================================
// 박자별 화면
// ============================================================================

void UGachaRevealPresentationWidget::UpdateThrow(float T)
{
	using namespace GachaRevealWidgetDetail;
	const float W = StageWidth();
	const FVector2D Launch = LaunchPos.X < 0.f ? FVector2D(W / 2.f, LaunchPos.Y) : LaunchPos;
	const float LiftEnd = GT::ThrowLift;
	const float ExitEnd = LiftEnd + GT::ThrowExit;
	const float DropEnd = ExitEnd + GT::ThrowDrop;
	FBoxPose Pose;
	if (T < LiftEnd)
	{
		const float P = CubicBezier(.2f, .8f, .3f, 1.f, Progress(T, 0.f, GT::ThrowLift));
		Pose.Pos = FMath::Lerp(Launch, Launch - FVector2D(0.f, 140.f), P);
		Pose.Scale = FMath::Lerp(LaunchScale, .62f, P);
	}
	else if (T < ExitEnd)
	{
		const float P = CubicBezier(.6f, 0.f, 1.f, .6f, Progress(T, LiftEnd, GT::ThrowExit));
		Pose.Pos = FMath::Lerp(Launch - FVector2D(0.f, 140.f), FVector2D(Launch.X, -500.f), P);
		Pose.Scale = .62f;
		Pose.Angle = FMath::Lerp(0.f, 4.f, P);
	}
	else if (T < DropEnd)
	{
		const float P = CubicBezier(.55f, 0.f, 1.f, .45f, Progress(T, ExitEnd, GT::ThrowDrop));
		Pose.Pos = FMath::Lerp(FVector2D(W / 2.f, -700.f), FVector2D(W / 2.f, LandY), P);
		Pose.Angle = FMath::Lerp(-10.f, 0.f, P);
	}
	else
	{
		Pose.Pos = FVector2D(W / 2.f, LandY);
		Pose = BoxPress(Pose, .94f, 16.f, Progress(T, DropEnd, GT::ThrowPress));
	}
	ApplyBoxPose(Pose, 1.f);
}

void UGachaRevealPresentationWidget::UpdateCut(float T)
{
	using namespace GachaRevealWidgetDetail;
	ApplyBoxPose(FBoxPose{ FVector2D(StageWidth() / 2.f, LandY), 1.f, 0.f }, 1.f);
	if (Runner.GetPlan().bHandOpen || !bBlade)
	{
		return;
	}
	const float Previous = SlitProgress;
	SlitProgress = EaseInOutQuad(Progress(T, 0.f, GT::Cut));
	if (SlitProgress > Previous)
	{
		const FVector2D Blade = FMath::Lerp(BoxPoint(EGachaSeam::Left), BoxPoint(EGachaSeam::Right), SlitProgress);
		Sparks(Blade, 4, SlitColor, PI * .95f, PI * 1.45f, 500.f, 1400.f, .12f, .3f, 2.f, 5.f);
	}
}

void UGachaRevealPresentationWidget::UpdateUpgrade(float T)
{
	using namespace GachaRevealWidgetDetail;
	FBoxPose Pose{ FVector2D(StageWidth() / 2.f, LandY), 1.f, 0.f };
	const float StepMs = GT::UpgradeGap + GT::UpgradePress;
	const float Local = FMath::Fmod(T, StepMs) - GT::UpgradeGap;
	const int32 Steps = FMath::Max(0, Runner.GetPlan().UpgradeSteps.Num() - 1);
	if (T < Steps * StepMs && Local >= 0.f)
	{
		Pose = BoxPress(Pose, .95f, 10.f, Progress(Local, 0.f, GT::UpgradePress));
	}
	ApplyBoxPose(Pose, 1.f);
}

void UGachaRevealPresentationWidget::UpdateLid(float T)
{
	using namespace GachaRevealWidgetDetail;
	ApplyBoxPose(BoxPress(FBoxPose{ FVector2D(StageWidth() / 2.f, LandY), 1.f, 0.f }, .96f, 10.f, Progress(T, 0.f, GT::LidPress)), 1.f);
}

void UGachaRevealPresentationWidget::UpdateBoxLid()
{
	using namespace GachaRevealWidgetDetail;
	if (!BoxWidget || LidClockStartMs < 0.f)
	{
		return;
	}
	// 날개·안쪽 빛은 뚜껑 박자(240)보다 길어 시계 기준점으로 그린다(R3)
	const float Now = ClockMs();
	BoxWidget->SetLid(Progress(Now, LidClockStartMs, FlapLongestMs));
	BoxWidget->SetInnerGlow(LidGlowColor, Progress(Now, LidClockStartMs, GT::LidInnerGlowFade));
}

void UGachaRevealPresentationWidget::UpdateDeal(float T)
{
	using namespace GachaRevealWidgetDetail;
	(void)T;
	if (DealClockStartMs < 0.f)
	{
		return;
	}
	// 상자는 120 뒤 아래로 1400 떨어지며 사라진다 ― 박자를 넘을 수 있어 시계 기준(시안 :931-933)
	const float DropStart = DealClockStartMs + GT::DealBoxDropDelay;
	const float Raw = Progress(ClockMs(), DropStart, GT::DealBoxDrop);
	const FBoxPose Landed{ FVector2D(StageWidth() / 2.f, LandY), 1.f, 0.f };
	if (ClockMs() < DropStart)
	{
		ApplyBoxPose(Landed, 1.f);
		return;
	}
	const float P = CubicBezier(.6f, 0.f, 1.f, .6f, Raw);
	FBoxPose Pose = Landed;
	Pose.Pos.Y += 1400.f * P;
	Pose.Angle = 6.f * P;
	ApplyBoxPose(Pose, 1.f - EaseIn(Raw));
	if (Raw >= 1.f && BoxWidget)
	{
		BoxWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGachaRevealPresentationWidget::UpdateCascade(float T)
{
	(void)T;
	UpdateDeal(0.f);
}

void UGachaRevealPresentationWidget::UpdateStamp(float T)
{
	(void)T;
	UpdateDeal(0.f);
}

void UGachaRevealPresentationWidget::UpdateSpotlight(float T)
{
	using namespace GachaRevealWidgetDetail;
	UpdateDeal(0.f);
	const FGachaRevealPlan& Plan = Runner.GetPlan();
	const FGachaRevealBeatSpec* Spec = Runner.GetCurrentBeat();
	const int32 Top = Plan.TopIndex;
	if (!Spec || !Entries.IsValidIndex(Top) || !SpotlightCard)
	{
		return;
	}
	const int32 G = Entries[Top].Grade;
	const float W = StageWidth();
	const float Rise = G >= 5 ? GT::SpotlightRiseSSS : (G >= 4 ? GT::SpotlightRiseSS : GT::SpotlightRiseS);
	const float ThrowAt = FMath::Max(0.f, Rise - GT::SpotlightThrow);
	const float LandAt = ThrowAt + GT::SpotlightThrow;
	const float ReleaseAt = Spec->WaitAtMs + Spec->WaitTimeoutMs;
	const FGachaRevealRect SpotRect = SpotlightRect(W, Plan.Layout == EGachaRevealLayout::Single);

	FWidgetTransform Xf;
	if (T < LandAt)
	{
		// 화면 왼쪽 위 밖에서 −24°로 던져 박는다(시안 :994-995)
		const float P = CubicBezier(.55f, 0.f, 1.f, .45f, Progress(T, ThrowAt, GT::SpotlightThrow));
		Xf.Translation = FMath::Lerp(FVector2D(-SpotRect.X - SpotRect.W - 200.f, -280.f), FVector2D::ZeroVector, P);
		Xf.Angle = FMath::Lerp(-24.f, 0.f, P);
	}
	else if (T < LandAt + GT::SpotlightPress)
	{
		static const float Offsets[] = { 0.f, .35f, 1.f };
		static const FVector2D Moves[] = { FVector2D::ZeroVector, FVector2D(0.f, 18.f), FVector2D::ZeroVector };
		static const float Scales[] = { 1.f, .94f, 1.f };
		const float P = EaseOut(Progress(T, LandAt, GT::SpotlightPress));
		Xf.Translation = Keyframes(Offsets, Moves, P);
		Xf.Scale = FVector2D(Keyframes(Offsets, Scales, P));
	}
	else if (Plan.Layout != EGachaRevealLayout::Single && T >= ReleaseAt && CardMotion.IsValidIndex(Top))
	{
		// 큰 카드가 자기 칸으로 줄어들며 돌아간다(시안 :1027-1029) ― 시안 원점 0,0 을 가운데 축으로 바꿔 같은 결과
		const FGachaRevealRect& Target = CardMotion[Top].Cell;
		const float P = CubicBezier(.5f, 0.f, .3f, 1.f, Progress(T, ReleaseAt, GT::SpotlightReturn));
		const FVector2D From(SpotRect.X + SpotRect.W / 2.f, SpotRect.Y + SpotRect.H / 2.f);
		const FVector2D To(Target.X + Target.W / 2.f, Target.Y + Target.H / 2.f);
		Xf.Translation = FMath::Lerp(FVector2D::ZeroVector, To - From, P);
		Xf.Scale = FVector2D(FMath::Lerp(1.f, Target.W / SpotRect.W, P));
	}
	SpotlightCard->SetRenderTransformPivot(FVector2D(.5f, .5f));
	SpotlightCard->SetRenderTransform(Xf);

	if (SpotlightInfo && InfoClockMs >= 0.f && SpotlightReleaseClockMs < 0.f)
	{
		// 정보 밀려 들어오기는 대기 시작과 겹친다 ― 탭으로 박자 시간이 뛰어도 튀지 않게 시계 기준(R6)
		const float P = CubicBezier(.2f, .8f, .3f, 1.f, Progress(ClockMs(), InfoClockMs, GT::SpotlightInfoSlide));
		SpotlightInfo->SetRenderOpacity(P);
		SpotlightInfo->SetRenderTranslation(FVector2D(70.f * (1.f - P), 0.f));
	}
}

void UGachaRevealPresentationWidget::UpdateSettle(float T)
{
	using namespace GachaRevealWidgetDetail;
	if (SettleClockStartMs < 0.f)
	{
		return;
	}
	const FGachaRevealPlan& Plan = Runner.GetPlan();
	const float Now = ClockMs();
	const float SortStart = SettleClockStartMs + GT::SettleWait;
	if (Now >= SortStart)
	{
		const float P = CubicBezier(.3f, .7f, .2f, 1.f, Progress(Now, SortStart, GT::SettleSort));
		for (int32 Index = 0; Index < CardMotion.Num(); ++Index)
		{
			FCardMotion& Motion = CardMotion[Index];
			const FGachaRevealRect& A = Motion.SettleFrom;
			const FGachaRevealRect& B = Motion.Cell;
			const FGachaRevealRect R{ FMath::Lerp(A.X, B.X, P), FMath::Lerp(A.Y, B.Y, P), FMath::Lerp(A.W, B.W, P), FMath::Lerp(A.H, B.H, P) };
			if (DealtCards.IsValidIndex(Index) && DealtCards[Index])
			{
				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DealtCards[Index]->Slot))
				{
					CanvasSlot->SetPosition(FVector2D(R.X, R.Y));
					CanvasSlot->SetSize(FVector2D(R.W, R.H));
				}
			}
		}
		// 「이번 최고」가 화면 밖에서 날아와 박힌다(시안 :1053) ― 10·30회는 왼쪽, 1회는 오른쪽에서
		static const float Offsets[] = { 0.f, .8f, .9f, 1.f };
		const float Dir = Plan.Count > 1 ? -1.f : 1.f;
		const float Moves[] = { 900.f * Dir, 0.f, -10.f * Dir, 0.f };
		const float Fly = Keyframes(Offsets, Moves, CubicBezier(.55f, 0.f, 1.f, .45f, Progress(Now, SortStart, GT::SettleBest)));
		if (UWidget* BestWidget = Plan.Count > 1 ? static_cast<UWidget*>(BestPanel.Get()) : SinglePanel.Get())
		{
			BestWidget->SetRenderTranslation(FVector2D(Fly, 0.f));
		}
	}
	const float BottomStart = SortStart + GT::SettleBest;
	if (BottomBar && Now >= BottomStart)
	{
		const float P = CubicBezier(.2f, .8f, .3f, 1.f, Progress(Now, BottomStart, GT::SettleBottom));
		BottomBar->SetRenderOpacity(P);
		BottomBar->SetRenderTranslation(FVector2D(0.f, 160.f * (1.f - P)));
	}
	(void)T;
}

void UGachaRevealPresentationWidget::UpdateResult(float T)
{
	using namespace GachaRevealWidgetDetail;
	(void)T;
	UpdateSettle(0.f);
	UpdateResultGauge();
	if (SortMoveStartMs >= 0.f)
	{
		const float P = CubicBezier(.3f, .7f, .2f, 1.f, Progress(ClockMs(), SortMoveStartMs, GT::SettleSort));
		for (int32 Index = 0; Index < CardMotion.Num(); ++Index)
		{
			if (!DealtCards.IsValidIndex(Index) || !DealtCards[Index]) { continue; }
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DealtCards[Index]->Slot))
			{
				const FGachaRevealRect& From = CardMotion[Index].SettleFrom;
				const FGachaRevealRect& To = CardMotion[Index].Cell;
				CanvasSlot->SetPosition(FVector2D(FMath::Lerp(From.X, To.X, P), FMath::Lerp(From.Y, To.Y, P)));
				CanvasSlot->SetSize(FVector2D(FMath::Lerp(From.W, To.W, P), FMath::Lerp(From.H, To.H, P)));
			}
		}
		if (P >= 1.f) { SortMoveStartMs = -1.f; }
	}
	if (!ConfirmButton)
	{
		return;
	}
	// [확인]은 아래 띠 착지 + 500 부터(R8) ― 그 전 누름은 OnConfirmClicked 가 무시한다
	if (Runner.IsConfirmEnabled() && ConfirmButton->GetVisibility() != ESlateVisibility::Visible)
	{
		ConfirmButton->SetVisibility(ESlateVisibility::Visible);
		// Result 는 배속 1 이라 켜지는 순간 = 결과 시작 시계 + 500
		ShowFade(ConfirmButton, true, GT::SettleConfirmFade, ESlateVisibility::Visible, ResultClockStartMs + GT::SettleConfirmDelay);
	}
}

void UGachaRevealPresentationWidget::UpdateCards()
{
	using namespace GachaRevealWidgetDetail;
	const FGachaRevealPlan& Plan = Runner.GetPlan();
	const float Now = ClockMs();
	const float Stagger = Plan.Layout == EGachaRevealLayout::Thirty ? GT::DealStagger30 : (Plan.Layout == EGachaRevealLayout::Ten ? GT::DealStagger10 : 0.f);
	const bool bInSettleOrResult = SettleClockStartMs >= 0.f || ResultClockStartMs >= 0.f;
	for (int32 Index = 0; Index < DealtCards.Num(); ++Index)
	{
		UGachaRevealCardWidget* Card = DealtCards[Index];
		if (!Card || !CardMotion.IsValidIndex(Index))
		{
			continue;
		}
		FCardMotion& Motion = CardMotion[Index];
		if (Motion.bHidden || (!bInSettleOrResult && DealClockStartMs < 0.f))
		{
			Card->SetRenderOpacity(0.f);
			continue;
		}
		const float Start = DealClockStartMs + Index * Stagger;
		if (!bInSettleOrResult && Now < Start)
		{
			Card->SetRenderOpacity(0.f);
			continue;
		}
		Card->SetRenderOpacity(1.f);
		if (!bInSettleOrResult)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Card->Slot))
			{
				CanvasSlot->SetPosition(FVector2D(Motion.Cell.X, Motion.Cell.Y));
				CanvasSlot->SetSize(FVector2D(Motion.Cell.W, Motion.Cell.H));
			}
		}
		FWidgetTransform Xf;
		const float Raw = Progress(Now, Start, GT::DealFlight);
		if (Raw < 1.f && !bInSettleOrResult)
		{
			// 상자 입구에서 0.3배로 나와 포물선(중간에 220 위)으로 칸에 박힌다(시안 :924-929)
			const FVector2D Center(Motion.Cell.X + Motion.Cell.W / 2.f, Motion.Cell.Y + Motion.Cell.H / 2.f);
			const FVector2D D = DealOrigin - Center;
			static const float Offsets[] = { 0.f, .45f, .85f, .93f, 1.f };
			const FVector2D Moves[] = { D, FVector2D(D.X * .45f, D.Y * .45f - 220.f), FVector2D::ZeroVector, FVector2D(0.f, 6.f), FVector2D::ZeroVector };
			static const float Scales[] = { .3f, .75f, 1.f, .96f, 1.f };
			const float Angles[] = { Motion.DealRotation, Motion.DealRotation * .4f, 0.f, 0.f, 0.f };
			const float P = CubicBezier(.25f, .6f, .35f, 1.f, Raw);
			Xf.Translation = Keyframes(Offsets, Moves, P);
			Xf.Scale = FVector2D(Keyframes(Offsets, Scales, P));
			Xf.Angle = Keyframes(Offsets, Angles, P);
		}
		if (UWidget* Visual = Card->GetVisualRoot())
		{
			Visual->SetRenderTransform(Xf);
		}
		if (Motion.FlipStartMs >= 0.f)
		{
			const float FlipP = Progress(Now, Motion.FlipStartMs, Motion.FlipMs);
			Card->SetFlip(CubicBezier(.3f, .6f, .3f, 1.f, FlipP));
			if (FlipP >= 1.f)
			{
				Motion.FlipStartMs = -1.f;
				Motion.bFaceUp = true;
				Card->SetFaceUp(true);
			}
		}
	}
}

void UGachaRevealPresentationWidget::FlipCard(int32 Index, float DurationMs, float StartClockMs)
{
	GACHA_REVEAL_COUNT_EVENT(TEXT("Flip"));
	if (!CardMotion.IsValidIndex(Index) || CardMotion[Index].bFaceUp)
	{
		return;
	}
	CardMotion[Index].FlipStartMs = StartClockMs;
	CardMotion[Index].FlipMs = DurationMs;
}

// ============================================================================
// 정리·결과
// ============================================================================

void UGachaRevealPresentationWidget::ApplySettledLayout(bool bAnimate)
{
	const TArray<int32> Order = GachaRevealModel::SortedOrder(Entries, SortMode, Recommended);
	const TArray<FGachaRevealRect> Slots = GachaRevealModel::ComputeSlots(Entries.Num(), FMath::RoundToFloat(StageWidth()), true);
	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		const int32 Index = Order[Rank];
		if (!CardMotion.IsValidIndex(Index) || !Slots.IsValidIndex(Rank))
		{
			continue;
		}
		FCardMotion& Motion = CardMotion[Index];
		Motion.SettleFrom = bAnimate ? Motion.Cell : Slots[Rank];
		if (bAnimate && SortMoveStartMs >= 0.f && DealtCards.IsValidIndex(Index) && DealtCards[Index])
		{
			if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DealtCards[Index]->Slot))
			{
				const FVector2D Pos = CanvasSlot->GetPosition();
				const FVector2D Size = CanvasSlot->GetSize();
				Motion.SettleFrom = FGachaRevealRect{static_cast<float>(Pos.X), static_cast<float>(Pos.Y), static_cast<float>(Size.X), static_cast<float>(Size.Y)};
			}
		}
		Motion.Cell = Slots[Rank];
		if (DealtCards.IsValidIndex(Index) && DealtCards[Index] && DealtCards[Index]->GetCardSize() != DealtCardSize())
		{
			// 단독 장면 카드의 묶음이 다르면 정착 후 결과 묶음으로 맞춘다.
			if (!bAnimate)
			{
				DealtCards[Index]->Configure(CardData[Index], DealtCardSize());
				DealtCards[Index]->SetFaceUp(true);
			}
		}
		if (!bAnimate && DealtCards.IsValidIndex(Index) && DealtCards[Index])
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DealtCards[Index]->Slot))
			{
				CanvasSlot->SetPosition(FVector2D(Motion.Cell.X, Motion.Cell.Y));
				CanvasSlot->SetSize(FVector2D(Motion.Cell.W, Motion.Cell.H));
			}
		}
	}
}

void UGachaRevealPresentationWidget::EnterResultState()
{
	// 정상 흐름·연출 감소·뒤로가기가 같은 길 ― 도장·단독 장면·정리가 돌았다고 가정하지 않고 최종 상태를 처음부터 깐다(R7)
	const bool bFromSettle = SettleClockStartMs >= 0.f;
	ResultClockStartMs = ClockMs();
	if (!bFromSettle && FX)
	{
		FX->Clear();
	}
	StopZipLoop();
	bSlitOn = false;
	bBlade = false;
	bHandDragging = false;
	Fades.Reset();
	ClearStamps();
	if (BoxWidget) { BoxWidget->SetVisibility(ESlateVisibility::Collapsed); }
	if (SpotlightRoot) { SpotlightRoot->SetVisibility(ESlateVisibility::Collapsed); }
	if (SpotlightCard) { SpotlightCard->SetRenderOpacity(0.f); SpotlightCard->SetRenderTransform(FWidgetTransform()); }
	if (HandGuide) { HandGuide->SetVisibility(ESlateVisibility::Collapsed); }
	if (HandTextBlock) { HandTextBlock->SetVisibility(ESlateVisibility::Collapsed); }
	if (SkipButton) { SkipButton->SetVisibility(ESlateVisibility::Collapsed); }
	if (DimBG) { DimBG->SetVisibility(ESlateVisibility::Visible); DimBG->SetRenderOpacity(1.f); }
	if (DealClockStartMs < 0.f)
	{
		DealClockStartMs = ClockMs() - 100000.f;
	}
	for (int32 Index = 0; Index < DealtCards.Num(); ++Index)
	{
		if (!CardMotion.IsValidIndex(Index)) { continue; }
		CardMotion[Index].bHidden = false;
		CardMotion[Index].bFaceUp = true;
		CardMotion[Index].FlipStartMs = -1.f;
		if (UGachaRevealCardWidget* Card = DealtCards[Index])
		{
			Card->SetHoldGlow(false);
			if (Card->GetCardSize() != DealtCardSize())
			{
				Card->Configure(CardData[Index], DealtCardSize());
			}
			Card->SetFaceUp(true);
			Card->SetRenderOpacity(1.f);
			if (UWidget* Visual = Card->GetVisualRoot()) { Visual->SetRenderTransform(FWidgetTransform()); }
		}
	}
	ApplySettledLayout(false);
	TallyCounts.Init(0, GachaRevealModel::MaxGrade + 1);
	for (const FGachaRevealEntry& Entry : Entries)
	{
		if (TallyCounts.IsValidIndex(Entry.Grade)) { ++TallyCounts[Entry.Grade]; }
	}
	for (int32 G = 0; G < TallyCountTexts.Num(); ++G)
	{
		if (TallyCountTexts[G] && TallyCounts.IsValidIndex(G)) { TallyCountTexts[G]->SetText(FText::AsNumber(TallyCounts[G])); TallyCountTexts[G]->SetRenderScale(FVector2D(1.f)); }
		if (TallyCountTexts[G]) { TallyCountTexts[G]->SetColorAndOpacity(FSlateColor(FLinearColor::White)); }
	}
	TallyBumpClockMs.Init(-1.f, GachaRevealModel::MaxGrade + 1);
	SortMoveStartMs = -1.f;
	ShowResultChrome();
	ConfigureResultDetails();
	for (UGachaRevealCardWidget* Card : DealtCards)
	{
		if (Card) { Card->SetVisibility(ESlateVisibility::Visible); }
	}
	SettleClockStartMs = -1.f;
	if (ConfirmButton && !Runner.IsConfirmEnabled())
	{
		ConfirmButton->SetVisibility(ESlateVisibility::Hidden);
		ConfirmButton->SetRenderOpacity(0.f);
	}
}

void UGachaRevealPresentationWidget::ShowResultChrome()
{
	const int32 Count = Entries.Num();
	auto Put = [](UWidget* Widget, bool bShow)
	{
		if (!Widget) { return; }
		Widget->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		Widget->SetRenderOpacity(1.f);
		Widget->SetRenderTranslation(FVector2D::ZeroVector);
	};
	Put(HeaderTitle, true);
	Put(TallyRow, Count > 1);
	Put(SortToggle, Count > 1);
	Put(BestPanel, Count > 1);
	Put(SinglePanel, Count == 1);
	Put(BottomBar, true);
}

// ============================================================================
// 입력·닫기
// ============================================================================

void UGachaRevealPresentationWidget::HandleScreenTap()
{
	if (bClosing || !Runner.IsRunning())
	{
		return;
	}
	Runner.Tap();
}

void UGachaRevealPresentationWidget::HandleSkipClicked()
{
	if (bClosing || !Runner.IsRunning())
	{
		return;
	}
	JumpToResult();
}

FReply UGachaRevealPresentationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bClosing)
	{
		return FReply::Handled();
	}
	if (!Runner.IsRunning() || Runner.IsOnBeat(EGachaRevealBeat::Settle) || Runner.IsOnBeat(EGachaRevealBeat::Result))
	{
		if (Runner.IsOnBeat(EGachaRevealBeat::Result))
		{
			const FVector2D Pointer = InMouseEvent.GetScreenSpacePosition();
			const UWidget* BestCardWidget = WidgetTree ? WidgetTree->FindWidget(TEXT("BestCard")) : nullptr;
			if (Entries.Num() > 1 && BestPanel && BestPanel->IsVisible() && BestCardWidget &&
				BestCardWidget->GetCachedGeometry().IsUnderLocation(Pointer))
			{
				ShowCardTooltip(BestResultIndex(), Pointer);
				return FReply::Handled();
			}
			for (int32 Index = DealtCards.Num() - 1; Index >= 0; --Index)
			{
				if (DealtCards[Index] && DealtCards[Index]->GetCachedGeometry().IsUnderLocation(Pointer))
				{
					ShowCardTooltip(Index, Pointer);
					return FReply::Handled();
				}
			}
		}
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (Runner.GetPlan().bHandOpen && Runner.IsOnBeat(EGachaRevealBeat::Cut) && Runner.IsWaiting() && !bHandCutDone)
	{
		bHandDragging = true;
		HandLastX = 0.f;
		StartZipLoop();
		if (HandGuide) { HandGuide->SetVisibility(ESlateVisibility::Collapsed); }
		if (FXLayer) { UpdateHandCut(FXLayer->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), 16.f); }
		return FReply::Handled();
	}
	HandleScreenTap();
	return FReply::Handled();
}

FReply UGachaRevealPresentationWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bHandDragging && FXLayer)
	{
		UpdateHandCut(FXLayer->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), FMath::Max(1.f, FApp::GetDeltaTime() * 1000.f));
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UGachaRevealPresentationWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bHandDragging)
	{
		bHandDragging = false;
		SetZipLoop(SlitProgress, 0.f);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UGachaRevealPresentationWidget::UpdateHandCut(const FVector2D& StagePos, float DeltaRealMs)
{
	// 시안 handCut :880-889 ― 이음매에 수직 220 안, 지난 지점 −0.08 보다 앞이면 그만큼 갈라진다
	const FVector2D L = BoxPoint(EGachaSeam::Left);
	const FVector2D R = BoxPoint(EGachaSeam::Right);
	const FVector2D D = R - L;
	const float LenSq = D.SizeSquared();
	if (LenSq <= KINDA_SMALL_NUMBER || bHandCutDone)
	{
		return;
	}
	const float T = FVector2D::DotProduct(StagePos - L, D) / LenSq;
	const float Dist = FMath::Abs(FVector2D::CrossProduct(StagePos - L, D)) / FMath::Sqrt(LenSq);
	const float DragSpeed = HandLastX == 0.f ? 0.f : FMath::Abs(StagePos.X - HandLastX) / FMath::Max(1.f, DeltaRealMs) * .25f;
	HandLastX = StagePos.X;
	if (Dist >= 220.f || T <= SlitProgress - .08f)
	{
		return;
	}
	const float Next = FMath::Clamp(FMath::Max(SlitProgress, T), 0.f, 1.f);
	if (Next > SlitProgress)
	{
		Sparks(FMath::Lerp(L, R, Next), 3, SlitColor, PI * .95f, PI * 1.45f, 500.f, 1400.f, .12f, .3f, 2.f, 5.f);
	}
	SlitProgress = Next;
	SetZipLoop(Next, DragSpeed);
	if (Next >= .93f)
	{
		CompleteHandCutEvent();
	}
}

void UGachaRevealPresentationWidget::CompleteHandCutEvent()
{
	if (bHandCutDone)
	{
		return;
	}
	bHandCutDone = true;
	bHandDragging = false;
	SlitProgress = 1.f;
	bBlade = false;
	if (Runner.GetPlan().bHandOpen)
	{
		StopZipLoop();
		Runner.CompleteHandCut();
		if (HandGuide) { HandGuide->SetVisibility(ESlateVisibility::Collapsed); }
		ShowFade(HandTextBlock, false, GachaRevealTiming::HandTextHide, ESlateVisibility::HitTestInvisible, ClockMs());
	}
	// 시안 :818-819 가르기 끝 ― 끝점 금속 틱 + 불꽃 10
	PlayTag(CGUISoundTags::GachaBoxZipEnd);
	Sparks(BoxPoint(EGachaSeam::Right), 10, SlitColor, -PI * .4f, PI * .2f, 500.f, 1300.f);
}

void UGachaRevealPresentationWidget::UpdateHandGuide()
{
	if (!HandGuide || HandGuide->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	// 손 안내 반복은 배속을 안 받고 손 긋기 대기 동안 박자 시계가 멈추므로 실시간(R4)
	const float Phase = FMath::Fmod(HandRealMs, GachaRevealTiming::HandHintLoop) / GachaRevealTiming::HandHintLoop;
	const FVector2D Pos = FMath::Lerp(BoxPoint(EGachaSeam::Left), BoxPoint(EGachaSeam::Right), GachaRevealCardLayout::CubicBezier(.42f, 0.f, .58f, 1.f, Phase));
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HandGuide->Slot))
	{
		CanvasSlot->SetPosition(Pos);
	}
}

bool UGachaRevealPresentationWidget::NativeOnHandleBackAction()
{
	// 공용 처리기(FindTopWindow)는 닫기 수단(X·취소 이름) 없는 이 창을 고르지 못해 아래 패널이 먼저 닫힌다 ― 화면 맨 위 전체 화면 층이라 떠 있으면 스스로 받는다
	if (!IsInViewport())
	{
		return false;
	}
	return HandleBackRequest();
}

bool UGachaRevealPresentationWidget::HandleBackRequest()
{
	if (bClosing)
	{
		return true;
	}
	if (!IsPresentationOpen())
	{
		return false;
	}
	// 진행 중이면 결과까지 건너뛴 뒤 닫는다(검토 초점 2)
	JumpToResult();
	CloseSelf();
	return true;
}

UWidget* UGachaRevealPresentationWidget::GetConfirmButtonWidget() const
{
	return ConfirmButton;
}

void UGachaRevealPresentationWidget::OnConfirmClicked()
{
	// 켜지기 전에 시작된 누름(뽑기 단추 연타가 새는 것)은 무시한다(R8)
	if (bClosing || !Runner.IsConfirmEnabled())
	{
		return;
	}
	if (UGameInstance* GI = bEquipmentReveal ? nullptr : GetGameInstance())
	{
		if (UMissionManagerSubsystem* Missions = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			Missions->NotifyGachaRevealConfirmed();
		}
	}
	CloseSelf();
}

void UGachaRevealPresentationWidget::CloseSelf()
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	CloseElapsedMs = 0.f;
	Runner.Reset();
	StopZipLoop();
	// 효과 캔버스의 곱하기 장은 조상 투명도를 못 받는다 ― 닫기 페이드 전에 걷는다(Task 9 §6 규칙 6)
	if (FX) { FX->Clear(); }
	SetVisibility(ESlateVisibility::HitTestInvisible);
	CloseFadeTargets.Reset();
	CloseStartOpacity.Reset();
	for (UWidget* Widget : { static_cast<UWidget*>(DimBG.Get()), static_cast<UWidget*>(CardLayer.Get()), static_cast<UWidget*>(HeaderTitle.Get()), static_cast<UWidget*>(TallyRow.Get()),
		static_cast<UWidget*>(BestPanel.Get()), SinglePanel.Get(), static_cast<UWidget*>(BottomBar.Get()), SortToggle.Get(), static_cast<UWidget*>(SkipButton.Get()), static_cast<UWidget*>(StampLayer.Get()) })
	{
		if (Widget)
		{
			CloseFadeTargets.Add(Widget);
			CloseStartOpacity.Add(Widget->GetRenderOpacity());
		}
	}
	if (bReduceMotionActive || bDisableExitFade)
	{
		FinishClose();
	}
}

void UGachaRevealPresentationWidget::TickClose(float DeltaSeconds)
{
	CloseElapsedMs += DeltaSeconds * 1000.f;
	const float P = GachaRevealWidgetDetail::Progress(CloseElapsedMs, 0.f, GachaRevealWidgetDetail::CloseFadeMs);
	for (int32 Index = 0; Index < CloseFadeTargets.Num(); ++Index)
	{
		if (UWidget* Widget = CloseFadeTargets[Index].Get())
		{
			Widget->SetRenderOpacity(CloseStartOpacity[Index] * (1.f - GachaRevealWidgetDetail::EaseOut(P)));
		}
	}
	if (P >= 1.f)
	{
		FinishClose();
	}
}

void UGachaRevealPresentationWidget::FinishClose()
{
	if (!bClosing || bCloseNotified)
	{
		return;
	}
	bCloseNotified = true;
	Runner.Reset();
	if (FX) { FX->Release(); }
	if (ActiveTooltip)
	{
		ActiveTooltip->RemoveFromParent();
		ActiveTooltip = nullptr;
	}
	// 화면에 바로 붙인 활성 위젯이라 RemoveFromParent 만으로는 액션 라우터에서 안 빠진다
	if (IsActivated())
	{
		DeactivateWidget();
	}
	RemoveFromParent();
	OnPresentationClosed.ExecuteIfBound();
	ResetPresentation();
}

void UGachaRevealPresentationWidget::ResetPresentation()
{
	ClearDealtCards();
	ClearStamps();
	Fades.Reset();
	Entries.Reset();
	CardData.Reset();
	EntryMeta.Reset();
	EquipmentInstanceIds.Reset();
	bSkinDuplicates.Reset();
	AwardContext = FGachaRevealAwardContext();
	RevealEmployeeId = INDEX_NONE;
	bRecommendedEquipped = false;
	SortMoveStartMs = -1.f;
	ActiveBeat = INDEX_NONE;
	bClosing = false;
	bCloseNotified = false;
	CloseFadeTargets.Reset();
	CloseStartOpacity.Reset();
}

// ============================================================================
// 그리기 도우미
// ============================================================================

float UGachaRevealPresentationWidget::StageWidth() const
{
	return LastGeometryWidth > 1.f ? LastGeometryWidth : 2560.f;
}

float UGachaRevealPresentationWidget::AnchorAt(float BeatEventMs) const
{
	// 사건 시각의 시계값 ― 박자 시계가 그 사이 이어서 흘렀다고 본다(대기 탭으로 뛴 뒤의 사건도 뛴 뒤 기준)
	return ClockMs() - (EventNowMs - BeatEventMs);
}

float UGachaRevealPresentationWidget::ScaledSec(float Ms) const
{
	// 시안 T(ms) ― 효과층은 실시간으로 흐르므로 지금 배속을 길이에 미리 건다
	return Ms / FMath::Max(1.f, Runner.GetSpeed()) / 1000.f;
}

const FGachaRevealGradeColors& UGachaRevealPresentationWidget::GradeColors(int32 GradeIndex) const
{
	static const FGachaRevealGradeColors Fallback;
	const TArray<FGachaRevealGradeColors>& Palette = Kind == EGachaRevealKind::Equipment ? EquipmentPalette : TraitPalette;
	return Palette.IsValidIndex(GradeIndex) ? Palette[GradeIndex] : Fallback;
}

FText UGachaRevealPresentationWidget::GradeLabel(int32 GradeIndex) const
{
	// 등급 이름 = 개수 띠 칸에 저작된 글(시안 KINDS 를 굽기 때 옮김) ― 코드에 두 번 적지 않는다
	const TArray<TObjectPtr<UTextBlock>>& Labels = IsKoreanGrades() ? TallyKoreanTexts : TallyLatinTexts;
	return Labels.IsValidIndex(GradeIndex) && Labels[GradeIndex] ? Labels[GradeIndex]->GetText() : FText::GetEmpty();
}

EGachaCardSize UGachaRevealPresentationWidget::DealtCardSize() const
{
	switch (FGachaRevealBeatRunner::LayoutFor(Entries.Num()))
	{
	case EGachaRevealLayout::Single: return EGachaCardSize::Single;
	case EGachaRevealLayout::Ten: return EGachaCardSize::Ten;
	default: return EGachaCardSize::Thirty;
	}
}

FVector2D UGachaRevealPresentationWidget::CardCenter(int32 Index) const
{
	if (!CardMotion.IsValidIndex(Index))
	{
		return FVector2D::ZeroVector;
	}
	const FGachaRevealRect& Rect = CardMotion[Index].Cell;
	return FVector2D(Rect.X + Rect.W / 2.f, Rect.Y + Rect.H / 2.f);
}

FVector2D UGachaRevealPresentationWidget::BoxPoint(EGachaSeam Which) const
{
	if (!BoxWidget)
	{
		return CurrentBox.Pos;
	}
	// 상자 렌더 변형(위치·배율·회전, 축 = 기준점)을 직접 적용 ― 지오메트리 한 프레임 지연·헤드리스에서도 같은 값
	const FVector2D Local = (BoxWidget->GetSeamPoint(Which) - BoxWidget->GetSeamPoint(EGachaSeam::Origin)) * CurrentBox.Scale;
	return CurrentBox.Pos + Local.GetRotated(CurrentBox.Angle);
}

void UGachaRevealPresentationWidget::ApplyBoxPose(const FBoxPose& Pose, float Opacity)
{
	CurrentBox = Pose;
	if (!BoxWidget)
	{
		return;
	}
	// 칸 위치 = (W/2, 800) 착지점, 렌더 이동은 거기서의 차이
	FWidgetTransform Xf;
	Xf.Translation = Pose.Pos - FVector2D(StageWidth() / 2.f, GachaRevealWidgetDetail::LandY);
	Xf.Scale = FVector2D(Pose.Scale);
	Xf.Angle = Pose.Angle;
	BoxWidget->SetRenderTransform(Xf);
	BoxWidget->SetRenderOpacity(Opacity);
}

UGachaRevealPresentationWidget::FBoxPose UGachaRevealPresentationWidget::BoxPress(const FBoxPose& Base, float Depth, float Push, float Alpha) const
{
	// 시안 boxPress :731-734 ― 30% 지점에서 Push 만큼 내려가며 Depth 배로 눌렸다 돌아온다(전체 ease-out)
	static const float Offsets[] = { 0.f, .3f, 1.f };
	const float Pushes[] = { 0.f, Push, 0.f };
	const float Scales[] = { 1.f, Depth, 1.f };
	const float P = GachaRevealWidgetDetail::EaseOut(GachaRevealWidgetDetail::Clamp01(Alpha));
	FBoxPose Out = Base;
	Out.Pos.Y += GachaRevealWidgetDetail::Keyframes(Offsets, Pushes, P);
	Out.Scale *= GachaRevealWidgetDetail::Keyframes(Offsets, Scales, P);
	return Out;
}

FLinearColor UGachaRevealPresentationWidget::LeakColor(int32 GradeIndex) const
{
	return GradeIndex < 0 ? GachaRevealWidgetDetail::Hex(TEXT("#E3ECFF")) : FxColor(GradeIndex);
}

void UGachaRevealPresentationWidget::Sparks(FVector2D Pos, int32 Count, FLinearColor Color, float A0, float A1, float S0, float S1,
	float L0, float L1, float W0, float W1, float Gravity, float Jitter)
{
	if (!FX || bReduceMotionActive)
	{
		return;
	}
	FGachaSparkParams Params;
	Params.Count = Count;
	Params.Color = Color;
	Params.AngleMin = A0;
	Params.AngleMax = A1;
	Params.SpeedMin = S0;
	Params.SpeedMax = S1;
	Params.LifeMin = L0;
	Params.LifeMax = L1;
	Params.WidthMin = W0;
	Params.WidthMax = W1;
	Params.Gravity = Gravity;
	Params.Jitter = Jitter;
	FX->Sparks(Pos, Params);
}

void UGachaRevealPresentationWidget::Shake(float Px)
{
	GACHA_REVEAL_COUNT_EVENT(TEXT("Shake"));
	if (FX && !bReduceMotionActive)
	{
		FX->Shake(Px);
	}
}

void UGachaRevealPresentationWidget::BumpTally(int32 GradeIndex, float StartClockMs)
{
	if (!TallyCounts.IsValidIndex(GradeIndex))
	{
		return;
	}
	++TallyCounts[GradeIndex];
	if (TallyCellWidgets.IsValidIndex(GradeIndex) && TallyCellWidgets[GradeIndex]) { TallyCellWidgets[GradeIndex]->SetRenderOpacity(1.f); }
	if (TallyLatinTexts.IsValidIndex(GradeIndex) && TallyLatinTexts[GradeIndex]) { TallyLatinTexts[GradeIndex]->SetRenderOpacity(1.f); }
	if (TallyKoreanTexts.IsValidIndex(GradeIndex) && TallyKoreanTexts[GradeIndex]) { TallyKoreanTexts[GradeIndex]->SetRenderOpacity(1.f); }
	if (TallyCountTexts.IsValidIndex(GradeIndex) && TallyCountTexts[GradeIndex])
	{
		TallyCountTexts[GradeIndex]->SetText(FText::AsNumber(TallyCounts[GradeIndex]));
		TallyCountTexts[GradeIndex]->SetRenderOpacity(1.f);
		TallyCountTexts[GradeIndex]->SetRenderTransformPivot(FVector2D(.5f, .5f));
	}
	if (TallyBumpClockMs.IsValidIndex(GradeIndex))
	{
		TallyBumpClockMs[GradeIndex] = StartClockMs;
	}
}

void UGachaRevealPresentationWidget::UpdateTallyBumps()
{
	using namespace GachaRevealWidgetDetail;
	// 시안 bump :777 ― 숫자 .82 → 1, 등급 빛 색 → 흰색(260)
	for (int32 G = 0; G < TallyBumpClockMs.Num(); ++G)
	{
		if (TallyBumpClockMs[G] < 0.f || !TallyCountTexts.IsValidIndex(G) || !TallyCountTexts[G])
		{
			continue;
		}
		const float P = EaseOut(Progress(ClockMs(), TallyBumpClockMs[G], GT::TallyBump));
		TallyCountTexts[G]->SetRenderScale(FVector2D(FMath::Lerp(.82f, 1.f, P)));
		TallyCountTexts[G]->SetColorAndOpacity(FSlateColor(FMath::Lerp(FxColor(G), FLinearColor::White, P)));
		if (P >= 1.f)
		{
			TallyBumpClockMs[G] = -1.f;
		}
	}
}

void UGachaRevealPresentationWidget::SpawnStamp(FVector2D Pos, int32 GradeIndex, float SizePx, float StartClockMs, bool bKeep, bool bLeftAnchor)
{
	GACHA_REVEAL_COUNT_EVENT(TEXT("Stamp"));
	UTextBlock* Template = IsKoreanGrades() ? StampKoreanTemplate.Get() : StampLatinTemplate.Get();
	if (!Template || !StampLayer || !WidgetTree || SizePx <= 0.f)
	{
		return;
	}
	// 도장 본의 글꼴·외곽선·그림자를 복사해 크기·색만 바꾼다(10c §3.2) ― 카드 위 층이라 누르기를 막지 않게 HitTestInvisible
	UTextBlock* Stamp = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	const float TemplatePx = Template->GetFont().Size / GachaRevealCardLayout::PxToPt;
	const float Ratio = TemplatePx > 0.f ? SizePx / TemplatePx : 1.f;
	FSlateFontInfo Font = Template->GetFont();
	Font.Size = SizePx * GachaRevealCardLayout::PxToPt;
	Font.OutlineSettings.OutlineSize = FMath::RoundToInt(FMath::Max(6.f, SizePx * .09f) / 2.f);
	Font.OutlineSettings.OutlineColor = GradeColors(GradeIndex).Ink;
	Stamp->SetFont(Font);
	Stamp->SetText(GradeLabel(GradeIndex));
	Stamp->SetColorAndOpacity(FSlateColor(GradeColors(GradeIndex).Ui));
	Stamp->SetShadowOffset(Template->GetShadowOffset() * Ratio);
	Stamp->SetShadowColorAndOpacity(Template->GetShadowColorAndOpacity());
	Stamp->SetVisibility(ESlateVisibility::HitTestInvisible);
	Stamp->SetRenderOpacity(0.f);
	Stamp->SetRenderTransformPivot(bLeftAnchor ? FVector2D(.2f, .5f) : FVector2D(.5f, .5f));
	if (UCanvasPanelSlot* CanvasSlot = StampLayer->AddChildToCanvas(Stamp))
	{
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
		CanvasSlot->SetAlignment(bLeftAnchor ? FVector2D(0.f, .5f) : FVector2D(.5f, .5f));
		CanvasSlot->SetPosition(Pos);
	}
	StampTexts.Add(Stamp);
	FStampMotion& Motion = StampMotions.AddDefaulted_GetRef();
	Motion.Text = Stamp;
	Motion.StartMs = StartClockMs;
	Motion.bKeep = bKeep;
	Motion.bLeftAnchor = bLeftAnchor;
}

void UGachaRevealPresentationWidget::UpdateStamps()
{
	using namespace GachaRevealWidgetDetail;
	const float Now = ClockMs();
	for (int32 Index = StampMotions.Num() - 1; Index >= 0; --Index)
	{
		FStampMotion& Motion = StampMotions[Index];
		UTextBlock* Stamp = Motion.Text.Get();
		if (!Stamp)
		{
			StampMotions.RemoveAt(Index);
			continue;
		}
		if (Now < Motion.StartMs)
		{
			Stamp->SetRenderOpacity(0.f);
			continue;
		}
		// 시안 stampAt :969 ― 2.6배·−14° 에서 1배·−8° 로 떨어져 박힌다(300, 가속), 남지 않으면 380 머문 뒤 220 사라짐
		static const float Offsets[] = { 0.f, .55f, .7f, 1.f };
		static const float Opacity[] = { 0.f, 1.f, 1.f, 1.f };
		static const float Scales[] = { 2.6f, 1.f, .94f, 1.f };
		static const float Angles[] = { -14.f, -8.f, -8.f, -8.f };
		const float P = CubicBezier(.55f, 0.f, 1.f, .45f, Progress(Now, Motion.StartMs, GT::StampDrop));
		float Alpha = Keyframes(Offsets, Opacity, P);
		if (!Motion.bKeep && Motion.FadeStartMs < 0.f && Now >= Motion.StartMs + GT::StampDrop + GT::StampHold)
		{
			Motion.FadeStartMs = Motion.StartMs + GT::StampDrop + GT::StampHold;
		}
		if (Motion.FadeStartMs >= 0.f)
		{
			const float FadeMs = Motion.bKeep ? GT::SpotlightOutFade : GT::StampFade;
			const float FadeP = Progress(Now, Motion.FadeStartMs, FadeMs);
			Alpha *= 1.f - FadeP;
			if (FadeP >= 1.f)
			{
				Stamp->RemoveFromParent();
				StampTexts.Remove(Stamp);
				StampMotions.RemoveAt(Index);
				continue;
			}
		}
		Stamp->SetRenderOpacity(Alpha);
		FWidgetTransform Xf;
		Xf.Scale = FVector2D(Keyframes(Offsets, Scales, P));
		Xf.Angle = Keyframes(Offsets, Angles, P);
		Stamp->SetRenderTransform(Xf);
	}
}

void UGachaRevealPresentationWidget::ClearStamps()
{
	for (UTextBlock* Stamp : StampTexts)
	{
		if (Stamp) { Stamp->RemoveFromParent(); }
	}
	StampTexts.Reset();
	StampMotions.Reset();
}

void UGachaRevealPresentationWidget::FillSpotlightInfo(int32 Index)
{
	if (!WidgetTree || !CardData.IsValidIndex(Index))
	{
		return;
	}
	const FGachaRevealCardData& Data = CardData[Index];
	auto SetLine = [this](const TCHAR* Name, const FText& Text)
	{
		if (UTextBlock* Line = Cast<UTextBlock>(WidgetTree->FindWidget(FName(Name))))
		{
			Line->SetText(Text);
			Line->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	};
	SetLine(TEXT("SpotMeta"), EntryMeta.IsValidIndex(Index) ? EntryMeta[Index] : FText::GetEmpty());
	SetLine(TEXT("SpotName"), Data.Name);
	SetLine(TEXT("SpotEffect"), Data.EffectLine);
	SetLine(TEXT("SpotUnique"), Data.UniqueLine.IsEmpty() ? FText::GetEmpty()
		: FText::Format(NSLOCTEXT("GachaReveal", "UniqueLine", "고유 효과 · {0}"), Data.UniqueLine));
	if (UWidget* NewTag = WidgetTree->FindWidget(TEXT("SpotTagNew")))
	{
		NewTag->SetVisibility(Data.bNew ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidgetSwitcher* Compare = Cast<UWidgetSwitcher>(WidgetTree->FindWidget(TEXT("SpotTagCompare"))))
	{
		Compare->SetVisibility(ESlateVisibility::Collapsed);
		if (Kind == EGachaRevealKind::Equipment && RevealEmployeeId != INDEX_NONE && Entries.IsValidIndex(Index))
		{
			Compare->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Compare->SetActiveWidgetIndex(Entries[Index].Grade > Entries[Index].EquippedGrade ? 0 : 1);
			SetLine(TEXT("SpotTagGoodText"), FText::Format(NSLOCTEXT("GachaReveal", "SpotCompare", "장착 {0} → {1}"),
				Entries[Index].EquippedGrade == INDEX_NONE ? NSLOCTEXT("GachaReveal", "EmptySlot", "없음") : GradeLabel(Entries[Index].EquippedGrade), GradeLabel(Data.Grade)));
			SetLine(TEXT("SpotTagPlainText"), FText::Format(NSLOCTEXT("GachaReveal", "SpotPlain", "지금 장착 {0}"),
				Entries[Index].EquippedGrade == INDEX_NONE ? NSLOCTEXT("GachaReveal", "EmptySlot", "없음") : GradeLabel(Entries[Index].EquippedGrade)));
		}
		else if (Kind == EGachaRevealKind::Skin && Data.MileageGained > 0)
		{
			Compare->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Compare->SetActiveWidgetIndex(2);
			SetLine(TEXT("SpotTagDupText"), FText::Format(NSLOCTEXT("GachaReveal", "SpotDup", "중복 → 마일리지 +{0}P"), FText::AsNumber(Data.MileageGained)));
		}
	}
}

void UGachaRevealPresentationWidget::ShowThrowDim(float StartClockMs)
{
	ShowFade(DimBG, true, FMath::Max(GachaRevealTiming::ThrowDimFade, GachaRevealTiming::ThrowPanelBlur), ESlateVisibility::Visible, StartClockMs);
	if (!Fades.IsEmpty() && Fades.Last().Widget.Get() == static_cast<UWidget*>(DimBG.Get()))
	{
		Fades.Last().bThrowDim = true;
	}
}

void UGachaRevealPresentationWidget::ShowFade(UWidget* Widget, bool bShow, float DurationMs, ESlateVisibility ShownVisibility, float StartClockMs)
{
	if (!Widget)
	{
		return;
	}
	Fades.RemoveAll([Widget](const FFade& Fade) { return Fade.Widget.Get() == Widget; });
	if (bShow)
	{
		Widget->SetVisibility(ShownVisibility);
	}
	FFade& Fade = Fades.AddDefaulted_GetRef();
	Fade.Widget = Widget;
	Fade.From = Widget->GetRenderOpacity();
	Fade.To = bShow ? 1.f : 0.f;
	Fade.StartMs = StartClockMs;
	Fade.DurationMs = DurationMs;
	Fade.EndVisibility = bShow ? ShownVisibility : ESlateVisibility::Collapsed;
}

void UGachaRevealPresentationWidget::UpdateFades()
{
	using namespace GachaRevealWidgetDetail;
	const float Now = ClockMs();
	for (int32 Index = Fades.Num() - 1; Index >= 0; --Index)
	{
		FFade& Fade = Fades[Index];
		UWidget* Widget = Fade.Widget.Get();
		if (!Widget)
		{
			Fades.RemoveAt(Index);
			continue;
		}
		const float P = Progress(Now, Fade.StartMs, Fade.DurationMs);
		const float Eased = Fade.bThrowDim ? ThrowDimOpacity(FMath::Max(0.f, Now - Fade.StartMs)) : EaseOut(P);
		Widget->SetRenderOpacity(FMath::Lerp(Fade.From, Fade.To, Eased));
		if (P >= 1.f)
		{
			Widget->SetVisibility(Fade.EndVisibility);
			Fades.RemoveAt(Index);
		}
	}
}

void UGachaRevealPresentationWidget::PlayTag(const FGameplayTag& Tag, float VolumeScale)
{
	GACHA_REVEAL_COUNT_EVENT(Tag.GetTagName());
	// 연출 감소는 흔들림·박자만 줄인다(설계서 §4) ― 소리는 그대로
	const UGameInstance* GI = GetGameInstance();
	USoundManagerSubsystem* Sound = GI ? GI->GetSubsystem<USoundManagerSubsystem>() : nullptr;
	if (!Sound)
	{
		return;
	}
	if (FMath::IsNearlyEqual(VolumeScale, 1.f))
	{
		Sound->PlayUISound(Tag);
	}
	else
	{
		Sound->PlayUISoundWithParams(Tag, VolumeScale, 1.f);
	}
}

void UGachaRevealPresentationWidget::PlayTok(float VolumeScale)
{
	// 시안 wood() 는 칠 때마다 음높이가 달라 구운 변형 3개 중 하나 ― 볼륨 = 시안 세기 비(.17/.22/.3 → 0.77/1/1.36)
	const FGameplayTag Variants[] = { CGUISoundTags::GachaBoxTok, CGUISoundTags::GachaBoxTok2, CGUISoundTags::GachaBoxTok3 };
	GACHA_REVEAL_COUNT_EVENT(TEXT("Tok"));
	PlayTag(Variants[FMath::RandRange(0, 2)], VolumeScale);
}

void UGachaRevealPresentationWidget::StartZipLoop()
{
	const UGameInstance* GI = GetGameInstance();
	USoundManagerSubsystem* Sound = GI ? GI->GetSubsystem<USoundManagerSubsystem>() : nullptr;
	if (ZipLoopAudio || !Sound)
	{
		return;
	}
	bool bFound = false;
	const FUISoundTable Row = Sound->GetUISoundData(CGUISoundTags::GachaBoxZipLoop, bFound);
	USoundBase* Loop = bFound && !Row.Sound.IsNull() ? Row.Sound.LoadSynchronous() : nullptr;
	if (!Loop)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] UI.Sound.GachaBox.ZipLoop 행·소리 없음"));
		return;
	}
	// 반복 웨이브라 PlayUISound 로 쏘면 끝없이 돈다 ― 직접 쥐고 세기·정지를 맡는다(Task 4 §6-5)
	ZipLoopAudio = UGameplayStatics::CreateSound2D(this, Loop, 0.f);
	if (ZipLoopAudio)
	{
		ZipLoopAudio->Play();
	}
}

void UGachaRevealPresentationWidget::SetZipLoop(float Progress, float DragSpeed)
{
	(void)Progress;
	if (!ZipLoopAudio)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	const USoundManagerSubsystem* Sound = GI ? GI->GetSubsystem<USoundManagerSubsystem>() : nullptr;
	const float Base = Sound ? Sound->GetFinalVolume(ESoundCategory::UI, 1.f) : 1.f;
	ZipLoopAudio->SetVolumeMultiplier(Base * FMath::Clamp(DragSpeed, 0.f, 1.f));
}

void UGachaRevealPresentationWidget::StopZipLoop()
{
	if (ZipLoopAudio)
	{
		ZipLoopAudio->Stop();
		ZipLoopAudio = nullptr;
	}
}

#undef GACHA_REVEAL_COUNT_EVENT
