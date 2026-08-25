#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Cards/ItemCardWidget.h"
#include "UI/Element/Building/BuildingSkinCardWidget.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "UI/UISoundTags.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Enum/WidgetType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Enum/BuildingTraitTarget.h"
#include "Table/BuildingSkinData.h"
#include "Enum/LootBoxRarity.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/NamedSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "CommonButtonBase.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 카드 스파클 (기존 유지)
	constexpr float SparkleDuration = 0.5f;
	constexpr float SparkleStartScale = 17.0f;
	constexpr float SparkleEndScale = 5.8f;
	constexpr float SparkleSpinDeg = 220.f;
	constexpr float MultiStagger = 0.12f;
	constexpr float MultiSparkleSize = 140.f;

	// 풀 스테이징 — 승인 목업 확정값 (specs/2026-07-10, 치수는 캔버스 2560x1440 환산)
	constexpr float BuildBase = 0.55f; // 0.9 = 과함/지루 판정 (2026-07-11 PIE)
	constexpr float FlashDuration = 0.14f;
	constexpr float PunchScale = 1.3f;
	constexpr float PunchDuration = 0.3f;
	constexpr float CardFadeDuration = 0.075f;
	constexpr float SettlePad = 0.34f;
	constexpr float ButtonDelay = 0.5f;
	constexpr float ButtonFadeDuration = 0.35f;
	constexpr float ButtonRise = 24.f;
	constexpr float DimFadeDuration = 0.3f;
	constexpr float BackglowIntensity = 0.7f;
	// 카드 뒤 FX 는 desired size 32 고정(레이아웃 불침투) + RenderScale 로 카드 영역 비례 사이징
	constexpr float CardFXBaseSize = 32.f;
	constexpr float BackglowMarginW = 420.f;
	constexpr float BackglowMarginH = 140.f;
	constexpr float ShockwaveDuration = 0.55f;
	constexpr float ShakeDuration = 0.28f;
	constexpr float ShakeAmp = 12.f;
	// 아웃라인 트레일 글린트 (레어+ 등급색) — 카드 내장 TrailLine(테두리 순환 광) 점등
	constexpr int32 GlintTierMin = 2;
	constexpr float GlintDelay = 0.5f;
	constexpr float GlintDuration = 0.45f;
	constexpr float GlintTrailCount = 1.f;  // MI 기본 2줄기 = 산만 → 단방향 1줄기
	constexpr float GlintTrailSpeed = 0.6f; // MI 기본 1.0 = 과속, 0.35 = 답답 (PIE 튜닝)
	// 트레일 궤도 스케일 — 카드 타입별 (마스크 여백이 판 크기 대비 다르게 먹힘, PIE 확정값)
	constexpr float GlintTrailScaleTrait = 1.0f;  // 특성 카드는 원래 정합
	constexpr float GlintTrailScaleSkin = 1.25f;
	// 카드 계층 연출 — 아이콘 지연 팝 / 그림자 착지 / 개별 배후 글로우(에픽+) / 화이트 블룸(레전드리+)
	constexpr float IconPopDelay = 0.15f;
	constexpr float IconPopDuration = 0.22f;
	constexpr float IconPopScale = 1.25f;
	constexpr float IconPopAngleDeg = -10.f;
	constexpr float RingPopDuration = 0.45f;
	constexpr float CardRiseY = 36.f;
	constexpr float OutlineFlashDuration = 0.55f;
	constexpr float PerGlowMaxOpacity = 0.45f;
	constexpr float BloomDuration = 0.2f;
	constexpr float RarityTextPunchScale = 1.7f;
	constexpr float RarityTextDuration = 0.32f;
	constexpr int32 SparkDotCount = 10;
	constexpr float SparkDotSize = 18.f;

	// 런타임 생성 UImage 의 레이아웃 크기는 Brush.ImageSize 로 고정할 것 —
	// SetDesiredSizeOverride 는 Slate 미생성 시점(ConstructWidget 직후) 호출이 유실됨 (2026-07-10 에픽 간격 버그 실측)
	void SetImageLayoutSize(UImage* Img, const FVector2D& Size)
	{
		if (!Img) { return; }
		FSlateBrush Brush = Img->GetBrush();
		Brush.ImageSize = Size;
		Img->SetBrush(Brush);
	}
}

UGachaRevealPresentationWidget::UGachaRevealPresentationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 등급 → 스파클 텍스처(공통)
	static ConstructorHelpers::FObjectFinder<UTexture2D> Sp011(TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Sparkle/star_011.star_011"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Sp08(TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Sparkle/star_08.star_08"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Sp06(TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Sparkle/star_06.star_06"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Sp07(TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Sparkle/star_07.star_07"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Sp09(TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Sparkle/star_09.star_09"));
	if (Sp011.Succeeded()) RaritySparkleMap.Add(ELootBoxRarity::Common, Sp011.Object);
	if (Sp08.Succeeded())  RaritySparkleMap.Add(ELootBoxRarity::Unusual, Sp08.Object);
	if (Sp06.Succeeded())  RaritySparkleMap.Add(ELootBoxRarity::Rare, Sp06.Object);
	if (Sp07.Succeeded())  RaritySparkleMap.Add(ELootBoxRarity::Epic, Sp07.Object);
	if (Sp09.Succeeded())  { RaritySparkleMap.Add(ELootBoxRarity::Legendary, Sp09.Object); RaritySparkleMap.Add(ELootBoxRarity::Mythic, Sp09.Object); }

	// 스파클 머티리얼 — 하드 참조로 잡아 쿠킹 누락 방지 (런타임 LoadObject 문자열 경로 금지)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SparkleMatF(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_Sparkle_Additive.M_UI_Sparkle_Additive"));
	if (SparkleMatF.Succeeded()) { SparkleMaterial = SparkleMatF.Object; }

	// 수렴 스파크 도트 — 하드 참조 (쿠킹 자동 포함). 축포는 RaritySparkleMap 별 텍스처 재사용 (낙하 콘페티 폐기로 Polygon 참조 제거).
	static ConstructorHelpers::FObjectFinder<UTexture2D> SparkDotF(TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Glow/circle_05.circle_05"));
	if (SparkDotF.Succeeded()) { SparkDotTexture = SparkDotF.Object; }

	// 등급 → 트레이트 카드 Style
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> CommonF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Common.CUI_Style_Button_Trait_Common_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> UnusualF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Unusual.CUI_Style_Button_Trait_Unusual_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> RareF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Rare.CUI_Style_Button_Trait_Rare_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> EpicF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Epic.CUI_Style_Button_Trait_Epic_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> LegendaryF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Legendary.CUI_Style_Button_Trait_Legendary_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> MythicF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Mythic.CUI_Style_Button_Trait_Mythic_C"));
	if (CommonF.Succeeded())    RarityStyleMap.Add(ELootBoxRarity::Common,    CommonF.Class);
	if (UnusualF.Succeeded())   RarityStyleMap.Add(ELootBoxRarity::Unusual,   UnusualF.Class);
	if (RareF.Succeeded())      RarityStyleMap.Add(ELootBoxRarity::Rare,      RareF.Class);
	if (EpicF.Succeeded())      RarityStyleMap.Add(ELootBoxRarity::Epic,      EpicF.Class);
	if (LegendaryF.Succeeded()) RarityStyleMap.Add(ELootBoxRarity::Legendary, LegendaryF.Class);
	if (MythicF.Succeeded())    RarityStyleMap.Add(ELootBoxRarity::Mythic,    MythicF.Class);

	static ConstructorHelpers::FClassFinder<UItemCardWidget> TraitCardF(TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_TraitCard.UIE_TraitCard_C"));
	if (TraitCardF.Succeeded()) TraitCardClass = TraitCardF.Class;
	static ConstructorHelpers::FClassFinder<UBuildingSkinCardWidget> SkinCardF(TEXT("/Game/CompanyGrowth/UI/Elements/Building/UIE_BuildingSkinCard.UIE_BuildingSkinCard_C"));
	if (SkinCardF.Succeeded()) SkinCardClass = SkinCardF.Class;
}

void UGachaRevealPresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		TraitManager = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
		SkinManager = GI->GetSubsystem<UBuildingSkinManagerSubsystem>();
	}

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &UGachaRevealPresentationWidget::OnConfirmClicked);
	}
	if (UIE_CloseButton && !UIE_CloseButton->OnCloseClicked.IsBound())
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UGachaRevealPresentationWidget::OnCloseClicked);
	}

	// 스테이징 FX 텍스처 주입 — DT_UIVFXTexture 단일 진실 (WBP엔 빈 Image만 배치)
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		TableMgr->ApplyUIVFXTexture(TEXT("GachaBuildupGlow"), BuildupGlow);
		TableMgr->ApplyUIVFXTexture(TEXT("GachaCardGlow"), CardBackGlow); // Glow_Oval — 비균등 스케일 안전 (방사형 링 구조 없음)
		TableMgr->ApplyUIVFXTexture(TEXT("GachaShockwave"), ShockwaveRing);
		TableMgr->ApplyUIVFXTexture(TEXT("GachaShockwave"), ShockwaveRing2);
	}
	// 카드 뒤 FX 는 CardAreaOverlay 의 측정 크기를 부풀리면 안 됨 — 실크기는 RenderScale 로만
	SetImageLayoutSize(CardBackGlow, FVector2D(CardFXBaseSize, CardFXBaseSize));
	// 첫 페인트 전 안전 초기화 (Setup 이 같은 프레임에 재초기화)
	ResetStagingVisuals();

	// 튜토리얼 M11/M12 — 연출 [확인] 하이라이트 + 확인 클릭 통지를 위해 미션 매니저에 자기 등록
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* M = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			M->RegisterGachaReveal(this);
		}
	}
}

void UGachaRevealPresentationWidget::NativeDestruct()
{
	if (ConfirmButton) ConfirmButton->OnClicked().RemoveAll(this);
	if (UIE_CloseButton) UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	if (ActiveTooltip)
	{
		ActiveTooltip->RemoveFromParent();
		ActiveTooltip = nullptr;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* M = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			M->UnregisterGachaReveal(this);
		}
	}
	Super::NativeDestruct();
}

int32 UGachaRevealPresentationWidget::TierOf(ELootBoxRarity R)
{
	switch (R)
	{
	case ELootBoxRarity::Common:    return 0;
	case ELootBoxRarity::Unusual:   return 1;
	case ELootBoxRarity::Rare:      return 2;
	case ELootBoxRarity::Epic:      return 3;
	case ELootBoxRarity::Legendary: return 4;
	case ELootBoxRarity::Mythic:    return 5;
	default:                        return 0;
	}
}

float UGachaRevealPresentationWidget::BuildMulOf(ELootBoxRarity R)
{
	switch (R)
	{
	case ELootBoxRarity::Epic:      return 1.1f;
	case ELootBoxRarity::Legendary: return 1.2f;
	case ELootBoxRarity::Mythic:    return 1.3f;
	default:                        return 1.0f;
	}
}

float UGachaRevealPresentationWidget::RaritySizeMul(ELootBoxRarity R)
{
	switch (R)
	{
	case ELootBoxRarity::Common:    return 0.50f;
	case ELootBoxRarity::Unusual:   return 0.62f;
	case ELootBoxRarity::Rare:      return 0.78f;
	case ELootBoxRarity::Epic:      return 0.95f;
	case ELootBoxRarity::Legendary: return 1.20f;
	case ELootBoxRarity::Mythic:    return 1.50f;
	default:                        return 0.78f;
	}
}

UUserWidget* UGachaRevealPresentationWidget::MakeTraitCard(const FBuildingTraitGachaResult& R)
{
	if (!TraitCardClass) { return nullptr; }
	UItemCardWidget* Card = CreateWidget<UItemCardWidget>(this, TraitCardClass);
	if (!Card) { return nullptr; }
	Card->SetItemID(R.ResultTraitID);

	// 클릭 툴팁용 표시 데이터(이름/설명/아이콘) 캡처 — 설명은 (대상, 수치)에서 생성한다.
	FText TipName, TipDesc;
	UTexture2D* TipIcon = nullptr;
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		FBuildingTraitTableRow Row;
		if (TableMgr->GetBuildingTraitData(R.ResultTraitID, Row))
		{
			TipName = Row.DisplayName;
			TipDesc = FormatTraitEffectText(GetTargetForCategory(Row.Category), Row.BaseEffect);
			if (!Row.Icon.IsNull())
			{
				if (UTexture2D* Tex = Row.Icon.LoadSynchronous()) { Card->SetIcon(Tex); TipIcon = Tex; }
			}
		}
	}
	if (TSubclassOf<UCommonButtonStyle>* StylePtr = RarityStyleMap.Find(R.Rarity))
	{
		if (*StylePtr) { Card->SetStyle(*StylePtr); }
	}

	// 클릭 → 인라인 툴팁 (카드의 purpose-built 단일캐스트 델리게이트 사용).
	// 아이콘은 TWeakObjectPtr 로 캡처 — 카드 brush 가 살려주는 것에 의존하지 않고 죽으면 null 로 안전 강하.
	TWeakObjectPtr<UGachaRevealPresentationWidget> WeakThis(this);
	TWeakObjectPtr<UTexture2D> TipIconWeak(TipIcon);
	TWeakObjectPtr<UItemCardWidget> WeakCard(Card);
	Card->OnItemCardClicked.BindLambda([WeakThis, TipName, TipDesc, TipIconWeak, WeakCard](FName /*ItemID*/)
	{
		if (UGachaRevealPresentationWidget* Strong = WeakThis.Get())
		{
			// 탭 juice — 살짝 눌림 후 복귀
			if (UItemCardWidget* C = WeakCard.Get())
			{
				C->SetRenderScale(FVector2D(0.955f, 0.955f));
				if (UWorld* World = C->GetWorld())
				{
					FTimerHandle Tmp;
					TWeakObjectPtr<UItemCardWidget> W2 = WeakCard;
					World->GetTimerManager().SetTimer(Tmp, FTimerDelegate::CreateLambda([W2]()
					{
						if (UItemCardWidget* CC = W2.Get()) { CC->SetRenderScale(FVector2D(1.f, 1.f)); }
					}), 0.1f, false);
				}
			}
			Strong->ShowCardTooltip(TipName, TipDesc, TipIconWeak.Get());
		}
	});
	return Card;
}

UUserWidget* UGachaRevealPresentationWidget::MakeSkinCard(const FBuildingSkinGachaResult& R)
{
	if (!SkinCardClass) { return nullptr; }
	UBuildingSkinCardWidget* Card = CreateWidget<UBuildingSkinCardWidget>(this, SkinCardClass);
	if (!Card) { return nullptr; }

	// 클릭 툴팁용 표시 데이터(이름/아이콘) 캡처 — 스킨은 DT 에 설명 컬럼 없음(이름만).
	FText TipName;
	UTexture2D* TipIcon = nullptr;
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		bool bOk = false;
		const FBuildingSkinData Row = TableMgr->GetBuildingSkinData(R.ResultSkinID, bOk);
		if (bOk)
		{
			Card->SetSkinData(Row, /*bIsUnlocked=*/true);
			TipName = Row.DisplayName;
			if (!Row.Icon.IsNull()) { TipIcon = Row.Icon.LoadSynchronous(); }
		}
	}

	// 클릭 → 인라인 툴팁. 스킨 카드는 전용 델리게이트가 없으므로 CommonButtonBase 의 OnClicked() 사용
	// (BuildingManagePanel 과 동일 패턴). AddWeakLambda(this) → 위젯 파괴 시 자동 해제.
	// 아이콘은 TWeakObjectPtr 로 캡처 (카드 brush 의존 제거, 죽으면 null 안전 강하).
	TWeakObjectPtr<UTexture2D> TipIconWeak(TipIcon);
	TWeakObjectPtr<UBuildingSkinCardWidget> WeakCard(Card);
	Card->OnClicked().AddWeakLambda(this, [this, TipName, TipIconWeak, WeakCard]()
	{
		// 탭 juice — 살짝 눌림 후 복귀
		if (UBuildingSkinCardWidget* C = WeakCard.Get())
		{
			C->SetRenderScale(FVector2D(0.955f, 0.955f));
			if (UWorld* World = C->GetWorld())
			{
				FTimerHandle Tmp;
				TWeakObjectPtr<UBuildingSkinCardWidget> W2 = WeakCard;
				World->GetTimerManager().SetTimer(Tmp, FTimerDelegate::CreateLambda([W2]()
				{
					if (UBuildingSkinCardWidget* CC = W2.Get()) { CC->SetRenderScale(FVector2D(1.f, 1.f)); }
				}), 0.1f, false);
			}
		}
		ShowCardTooltip(TipName, FText::GetEmpty(), TipIconWeak.Get());
	});
	return Card;
}

void UGachaRevealPresentationWidget::SetupPullTrait(const TArray<FBuildingTraitGachaResult>& Results, bool bInAdvanced)
{
	bIsSkin = false;
	bAdvanced = bInAdvanced;

	TArray<UUserWidget*> Cards;
	TArray<ELootBoxRarity> Rarities;
	for (const FBuildingTraitGachaResult& R : Results)
	{
		if (UUserWidget* C = MakeTraitCard(R)) { Cards.Add(C); Rarities.Add(R.Rarity); }
	}

	if (ResultRarityText && Results.Num() == 1)
	{
		ResultRarityText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Results[0].Rarity)));
		ResultRarityText->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityColor(Results[0].Rarity)));
	}

	BeginReveal(Cards, Rarities);
}

void UGachaRevealPresentationWidget::SetupPullSkin(const TArray<FBuildingSkinGachaResult>& Results, bool bInAdvanced)
{
	bIsSkin = true;
	bAdvanced = bInAdvanced;

	TArray<UUserWidget*> Cards;
	TArray<ELootBoxRarity> Rarities;
	for (const FBuildingSkinGachaResult& R : Results)
	{
		if (UUserWidget* C = MakeSkinCard(R)) { Cards.Add(C); Rarities.Add(R.Rarity); }
	}

	if (ResultRarityText && Results.Num() == 1)
	{
		FString RarityStr = FLootBoxRarityUtility::GetKoreanName(Results[0].Rarity);
		if (Results[0].bDuplicate) { RarityStr += FString::Printf(TEXT("  (보유 중복 → 마일리지 +%d)"), Results[0].MileageGained); }
		ResultRarityText->SetText(FText::FromString(RarityStr));
		ResultRarityText->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityColor(Results[0].Rarity)));
	}

	BeginReveal(Cards, Rarities);
}

void UGachaRevealPresentationWidget::ShowCardTooltip(const FText& Name, const FText& Desc, UTexture2D* Icon)
{
	// 표시할 텍스트가 전혀 없으면(DT 이름/설명 셀 비어있음) 빈 팝오버 대신 아무것도 안 띄움.
	if (Name.IsEmpty() && Desc.IsEmpty()) { return; }

	UGameInstance* GI = GetGameInstance();
	if (!GI) { return; }
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) { return; }

	if (!ActiveTooltip)
	{
		const TSubclassOf<UUserWidget> TooltipClass = TableMgr->GetWidgetClass(EWidgetType::ItemTooltip);
		if (!TooltipClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] EWidgetType::ItemTooltip 미등록"));
			return;
		}
		ActiveTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass);
	}
	if (!ActiveTooltip) { return; }

	// 앵커 = 클릭/탭 지점 그대로 아래로 드롭 (UResourceWidget 과 동일 좌표계).
	// ZOrder = 이 리빌 위젯(AddToViewport 1000)+1 → 카드 위로 띄움 (안 그러면 리빌창 뒤에 가려짐).
	const FVector2D AbsPos = UGlobalUtilFunctions::GetPointerAbsolutePosition();
	ActiveTooltip->ShowAt(AbsPos, Name, Icon, Desc, /*BasePrice*/0, /*Duration*/2.5f, ETooltipAnchor::BelowAnchor, /*ZOrder*/1001);
}

void UGachaRevealPresentationWidget::BeginReveal(const TArray<UUserWidget*>& Cards, const TArray<ELootBoxRarity>& Rarities)
{
	RevealRarities = Rarities;

	RevealCards.Reset();
	RevealSparkles.Reset();
	RevealSparkleMIDs.Reset();
	RevealSparkleElapsed.Reset();
	CardRevealedFlags.Reset();
	CardGlintImages.Reset();

	const bool bSingle = (Cards.Num() == 1);
	if (ResultRarityText)
	{
		ResultRarityText->SetVisibility(bSingle ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ResultRarityText->SetRenderOpacity(0.f);
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	if (CardSlot && WidgetTree && Cards.Num() > 0)
	{
		// CardSlot 에 가로 row 주입 (1장이면 1칸). 등장 페이드/펀치는 InnerClip 컨테이너에 건다.
		// 자식 순서 계약 (틱이 인덱스로 접근): Wrap = [PopRing(0), PerGlow(10연차,1)?, Beam(레전드리+)?, InnerClip, Sp] / InnerClip = [Card, Bloom(레전드리+)?]
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		for (int32 i = 0; i < Cards.Num(); ++i)
		{
			UUserWidget* Card = Cards[i];
			if (!Card) { continue; }
			const int32 CardTier = Rarities.IsValidIndex(i) ? TierOf(Rarities[i]) : 0;

			UImage* Sp = WidgetTree->ConstructWidget<UImage>();
			if (Sp)
			{
				Sp->SetVisibility(ESlateVisibility::Collapsed);
				Sp->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				FSlateBrush Brush = Sp->GetBrush();
				Brush.ImageSize = FVector2D(MultiSparkleSize, MultiSparkleSize);
				Sp->SetBrush(Brush);
			}

			// ⚠ Wrap/InnerClip 에 ClipToBounds 금지 — 스파클 버스트(17x)가 사각형으로 잘리고 카드 Outline(-4px)도 잘림 (2026-07-10 PIE 실측)
			// InnerClip = 등장 페이드/펀치 대상 컨테이너
			UOverlay* Wrap = WidgetTree->ConstructWidget<UOverlay>();

			// 등장 링 팝 (전 카드, 등급색) — 항상 index 0 (TableMgr 없어도 자리 유지해 인덱스 계약 보존)
			if (UImage* PopRing = WidgetTree->ConstructWidget<UImage>())
			{
				PopRing->SetVisibility(ESlateVisibility::HitTestInvisible);
				PopRing->SetRenderOpacity(0.f);
				if (TableMgr) { TableMgr->ApplyUIVFXTexture(TEXT("GachaShockwave"), PopRing); }
				SetImageLayoutSize(PopRing, FVector2D(CardFXBaseSize, CardFXBaseSize));
				if (Rarities.IsValidIndex(i)) { PopRing->SetColorAndOpacity(FLootBoxRarityUtility::GetRarityColor(Rarities[i])); }
				if (UOverlaySlot* RSlot = Wrap->AddChildToOverlay(PopRing))
				{
					RSlot->SetHorizontalAlignment(HAlign_Center);
					RSlot->SetVerticalAlignment(VAlign_Center);
				}
			}

			// 카드별 등급색 번짐 (10연차 전 카드 — 카드 자리에서 등장 순간 번짐. 단일은 줄 배후광이 담당). Wrap index 1.
			if (TableMgr && Cards.Num() > 1)
			{
				if (UImage* PerGlow = WidgetTree->ConstructWidget<UImage>())
				{
					PerGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
					PerGlow->SetRenderOpacity(0.f);
					TableMgr->ApplyUIVFXTexture(TEXT("GachaCardGlow"), PerGlow);
					SetImageLayoutSize(PerGlow, FVector2D(CardFXBaseSize, CardFXBaseSize));
					PerGlow->SetColorAndOpacity(FLootBoxRarityUtility::GetRarityColor(Rarities[i]));
					if (UOverlaySlot* PGSlot = Wrap->AddChildToOverlay(PerGlow))
					{
						PGSlot->SetHorizontalAlignment(HAlign_Center);
						PGSlot->SetVerticalAlignment(VAlign_Center);
					}
				}
			}

			// 수직 빛기둥 (레전드리+) — 카드 뒤에서 위로 솟는 갓레이. 인덱스 = 10연차 2 / 단일 1 (PerGlow 유무)
			if (CardTier >= 4 && TableMgr)
			{
				if (UImage* Beam = WidgetTree->ConstructWidget<UImage>())
				{
					Beam->SetVisibility(ESlateVisibility::HitTestInvisible);
					Beam->SetRenderOpacity(0.f);
					TableMgr->ApplyUIVFXTexture(TEXT("GachaBeam"), Beam);
					SetImageLayoutSize(Beam, FVector2D(CardFXBaseSize, CardFXBaseSize));
					Beam->SetColorAndOpacity(FLootBoxRarityUtility::GetRarityColor(Rarities.IsValidIndex(i) ? Rarities[i] : ELootBoxRarity::Legendary));
					if (UOverlaySlot* BmSlot = Wrap->AddChildToOverlay(Beam))
					{
						BmSlot->SetHorizontalAlignment(HAlign_Center);
						BmSlot->SetVerticalAlignment(VAlign_Center);
					}
				}
			}

			UOverlay* InnerClip = WidgetTree->ConstructWidget<UOverlay>();
			InnerClip->SetRenderOpacity(0.f);
			if (UOverlaySlot* CSlot = InnerClip->AddChildToOverlay(Card))
			{
				CSlot->SetHorizontalAlignment(HAlign_Center);
				CSlot->SetVerticalAlignment(VAlign_Center);
			}

			// 화이트 블룸 플래시 (레전드리+) — 등장 순간 소프트 오벌 화이트가 확 떴다 식음. InnerClip index 1.
			if (CardTier >= 4 && TableMgr)
			{
				if (UImage* Bloom = WidgetTree->ConstructWidget<UImage>())
				{
					Bloom->SetVisibility(ESlateVisibility::HitTestInvisible);
					Bloom->SetRenderOpacity(0.f);
					TableMgr->ApplyUIVFXTexture(TEXT("GachaCardGlow"), Bloom);
					SetImageLayoutSize(Bloom, FVector2D(CardFXBaseSize, CardFXBaseSize));
					Bloom->SetColorAndOpacity(FLinearColor::White);
					if (UOverlaySlot* BSlot = InnerClip->AddChildToOverlay(Bloom))
					{
						BSlot->SetHorizontalAlignment(HAlign_Center);
						BSlot->SetVerticalAlignment(VAlign_Center);
					}
				}
			}

			// 아웃라인 트레일 글린트 (레어+) — 카드 내장 TrailLine(MI_GoodUIIconTrail_ThickLine, 테두리를 코너까지 순환)을
			// 각 카드 등급색으로 점등. 스킨 카드엔 TrailLine 없음 → null 스킵 (WBP에 추가하면 자동 적용).
			UImage* Glint = nullptr;
			if (Rarities.IsValidIndex(i) && TierOf(Rarities[i]) >= GlintTierMin)
			{
				Glint = Cast<UImage>(Card->GetWidgetFromName(TEXT("TrailLine")));
				if (Glint)
				{
					if (UMaterialInterface* TrailMat = Cast<UMaterialInterface>(Glint->GetBrush().GetResourceObject()))
					{
						UMaterialInstanceDynamic* TrailMID = UMaterialInstanceDynamic::Create(TrailMat, this);
						TrailMID->SetVectorParameterValue(FName("Color"), FLootBoxRarityUtility::GetRarityColor(Rarities[i]));
						TrailMID->SetScalarParameterValue(FName("Trail Number"), GlintTrailCount);
						TrailMID->SetScalarParameterValue(FName("TimeSpeed"), GlintTrailSpeed);
						Glint->SetBrushFromMaterial(TrailMID);
					}
					const float TrailScale = bIsSkin ? GlintTrailScaleSkin : GlintTrailScaleTrait;
					Glint->SetRenderScale(FVector2D(TrailScale, TrailScale));
					Glint->SetVisibility(ESlateVisibility::Collapsed);
					Glint->SetRenderOpacity(0.f);
				}
			}

			// 아웃라인 섬광 준비 (전 카드) — 카드 내장 Outline(카드 모양 글로우 보더)에 등급색 MID 주입, 점화는 틱이 담당
			if (Rarities.IsValidIndex(i))
			{
				if (UBorder* OutlineB = Cast<UBorder>(Card->GetWidgetFromName(TEXT("Outline"))))
				{
					TSoftObjectPtr<UMaterialInterface> OutlineMat(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_GlowOutline_Blue.M_UI_GlowOutline_Blue")));
					if (UMaterialInterface* OMat = OutlineMat.LoadSynchronous())
					{
						UMaterialInstanceDynamic* OMID = UMaterialInstanceDynamic::Create(OMat, this);
						const FLinearColor RC = FLootBoxRarityUtility::GetRarityColor(Rarities[i]);
						OMID->SetVectorParameterValue(FName("Outline Color"), RC);
						OMID->SetVectorParameterValue(FName("Glow Color"), RC);
						OutlineB->SetBrushFromMaterial(OMID);
					}
					OutlineB->SetRenderOpacity(0.f);
				}
			}

			if (UOverlaySlot* ISlot = Wrap->AddChildToOverlay(InnerClip))
			{
				ISlot->SetHorizontalAlignment(HAlign_Center);
				ISlot->SetVerticalAlignment(VAlign_Center);
			}
			if (Sp)
			{
				if (UOverlaySlot* SSlot = Wrap->AddChildToOverlay(Sp))
				{
					SSlot->SetHorizontalAlignment(HAlign_Center);
					SSlot->SetVerticalAlignment(VAlign_Center);
				}
			}

			if (UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(Wrap))
			{
				BoxSlot->SetPadding(FMargin(6.f, 0.f));
				BoxSlot->SetVerticalAlignment(VAlign_Center);
			}

			RevealCards.Add(Card);
			RevealSparkles.Add(Sp);
			RevealSparkleMIDs.Add(nullptr);
			RevealSparkleElapsed.Add(-1.f);
			CardRevealedFlags.Add(false);
			CardGlintImages.Add(Glint); // 레어 미만 = nullptr 자리 유지 (인덱스 정렬)
		}
		CardSlot->SetContent(Row); // NamedSlot(UContentWidget) 에 row 주입(기존 내용 교체)
	}

	BeginStaging();
}

void UGachaRevealPresentationWidget::BeginStaging()
{
	// 최고 등급이 무대 색/강도를 결정 (10연차 = 빌드업 1회)
	BestRarity = ELootBoxRarity::Common;
	for (ELootBoxRarity R : RevealRarities)
	{
		if (TierOf(R) > TierOf(BestRarity)) { BestRarity = R; }
	}
	BestColor = FLootBoxRarityUtility::GetRarityColor(BestRarity);
	bReduceMotion = USettingsManagerSubsystem::IsReduceMotion(this);

	const int32 CardCount = FMath::Max(RevealCards.Num(), 1);
	BuildDuration = BuildBase * BuildMulOf(BestRarity);
	RevealStartTime = BuildDuration + FlashDuration * 0.4f;
	SettleTime = RevealStartTime + (CardCount - 1) * MultiStagger + SettlePad;
	ButtonsTime = SettleTime + ButtonDelay;

	StageTime = 0.f;
	bFlashFired = false;
	bButtonsShown = false;
	bStagingActive = true;

	ResetStagingVisuals();
	RebuildFXPools();

	// 딤 강화 — WBP 기본 0.82는 뒤 패널(천장 바/버튼)이 비쳐 산만 (PIE 캡처 실측, 0.92→0.96 재상향)
	if (DimBG) { DimBG->SetBrushTintColor(FSlateColor(FLinearColor(0.008f, 0.011f, 0.02f, 0.96f))); }

	// 등급색 텔레그래프 — 빌드업부터 결과 색 노출 (설계 확정)
	if (BuildupGlow) { BuildupGlow->SetColorAndOpacity(BestColor); }
	if (CardBackGlow) { CardBackGlow->SetColorAndOpacity(BestColor); }
	if (ShockwaveRing) { ShockwaveRing->SetColorAndOpacity(BestColor); }
	if (ShockwaveRing2) { ShockwaveRing2->SetColorAndOpacity(BestColor); }

	PlayUITag(CGUISoundTags::GachaBuildup);
}

void UGachaRevealPresentationWidget::ResetStagingVisuals()
{
	if (DimBG) { DimBG->SetRenderOpacity(0.f); }
	auto Hide = [](UImage* Img) { if (Img) { Img->SetRenderOpacity(0.f); } };
	Hide(BuildupGlow); Hide(FlashOverlay); Hide(ShockwaveRing); Hide(ShockwaveRing2);
	Hide(CardBackGlow);
	if (CenterVBox) { CenterVBox->SetRenderTranslation(FVector2D::ZeroVector); }

	// 여운 페이즈 전까지 조작 차단 — 버튼/닫기 숨김
	auto HideButton = [](UWidget* W) { if (W) { W->SetVisibility(ESlateVisibility::Collapsed); W->SetRenderOpacity(0.f); } };
	HideButton(ConfirmButton);
	HideButton(UIE_CloseButton);
}

void UGachaRevealPresentationWidget::RebuildFXPools()
{
	auto ClearPool = [](TArray<TObjectPtr<UImage>>& Pool)
	{
		for (UImage* Img : Pool) { if (Img) { Img->RemoveFromParent(); } }
		Pool.Reset();
	};
	ClearPool(SparkImages);
	SparkParams.Reset();
	if (!FXLayer || !WidgetTree) { return; }
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// 빌드업 수렴 스파크
	if (SparkDotTexture)
	{
		for (int32 i = 0; i < SparkDotCount; ++i)
		{
			UImage* Img = WidgetTree->ConstructWidget<UImage>();
			if (!Img) { continue; }
			Img->SetBrushFromTexture(SparkDotTexture, false);
			SetImageLayoutSize(Img, FVector2D(SparkDotSize, SparkDotSize));
			Img->SetColorAndOpacity(BestColor);
			Img->SetRenderOpacity(0.f);
			if (UCanvasPanelSlot* CSlot = FXLayer->AddChildToCanvas(Img)) { CSlot->SetAutoSize(true); }
			FGachaSparkDot Dot;
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			Dot.Dir = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
			Dot.RadiusFrac = FMath::FRandRange(0.44f, 0.78f);
			Dot.Delay = FMath::FRandRange(0.f, 0.3f);
			SparkParams.Add(Dot);
			SparkImages.Add(Img);
		}
	}

	// 빌드업 수렴 링(3) + 등급 텍스트 좌우 라인(2) — SparkImages 풀 꼬리 고정 순서로 추가
	// (SparkParams 미등록 → 스파크 드라이브가 스킵. 드라이브는 꼬리 인덱스 [N-5..N-3]=링, [N-2..N-1]=라인 계약)
	for (int32 r = 0; r < 3; ++r)
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		if (!Img) { continue; }
		Img->SetVisibility(ESlateVisibility::HitTestInvisible);
		Img->SetRenderOpacity(0.f);
		if (TableMgr) { TableMgr->ApplyUIVFXTexture(TEXT("GachaShockwave"), Img); }
		SetImageLayoutSize(Img, FVector2D(CardFXBaseSize, CardFXBaseSize));
		Img->SetColorAndOpacity(BestColor);
		if (UCanvasPanelSlot* CSlot = FXLayer->AddChildToCanvas(Img)) { CSlot->SetAutoSize(true); }
		SparkImages.Add(Img);
	}
	for (int32 l = 0; l < 2; ++l)
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>();
		if (!Img) { continue; }
		Img->SetVisibility(ESlateVisibility::HitTestInvisible);
		Img->SetRenderOpacity(0.f);
		if (TableMgr) { TableMgr->ApplyUIVFXTexture(TEXT("GachaCardGlow"), Img); }
		SetImageLayoutSize(Img, FVector2D(CardFXBaseSize, CardFXBaseSize));
		Img->SetColorAndOpacity(FMath::Lerp(BestColor, FLinearColor::White, 0.3f));
		if (UCanvasPanelSlot* CSlot = FXLayer->AddChildToCanvas(Img)) { CSlot->SetAutoSize(true); }
		SparkImages.Add(Img);
	}
}

void UGachaRevealPresentationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bStagingActive)
	{
		UpdateStaging(InDeltaTime);
	}

	// 카드별 스파클 애니
	for (int32 i = 0; i < RevealSparkleElapsed.Num(); ++i)
	{
		if (RevealSparkleElapsed[i] < 0.f) { continue; }
		RevealSparkleElapsed[i] += InDeltaTime;
		const float t = FMath::Clamp(RevealSparkleElapsed[i] / SparkleDuration, 0.f, 1.f);
		if (RevealSparkles.IsValidIndex(i) && RevealRarities.IsValidIndex(i)) { ApplySparkleFrame(RevealSparkles[i], t, RaritySizeMul(RevealRarities[i])); }
		if (t >= 1.f)
		{
			RevealSparkleElapsed[i] = -1.f;
			if (RevealSparkles.IsValidIndex(i) && RevealSparkles[i]) { RevealSparkles[i]->SetVisibility(ESlateVisibility::Collapsed); }
		}
	}
}

void UGachaRevealPresentationWidget::UpdateStaging(float DeltaTime)
{
	StageTime += DeltaTime;
	const float T = StageTime;
	const float B = BuildDuration;
	const int32 Tier = TierOf(BestRarity);

	if (DimBG) { DimBG->SetRenderOpacity(FMath::Clamp(T / DimFadeDuration, 0.f, 1.f)); }

	// ① 빌드업 — 등급색 글로우 수렴 (끝 30% 긴장 펄스), 플래시 직후 급소멸
	if (BuildupGlow)
	{
		if (T < B)
		{
			const float K = T / B;
			const float E = FWidgetAnimationUtils::EaseOutQuad(K);
			const float S = 2.4f - 1.75f * E;
			BuildupGlow->SetRenderScale(FVector2D(S, S));
			BuildupGlow->SetRenderOpacity(0.15f + 0.75f * K);
		}
		else
		{
			BuildupGlow->SetRenderOpacity(FMath::Clamp(1.f - (T - B) / 0.12f, 0.f, 1.f) * 0.9f);
		}
	}

	// 수렴 스파크 입자
	const FVector2D LayerSize = FXLayer ? FXLayer->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const FVector2D Center = LayerSize * 0.5f;
	for (int32 i = 0; i < SparkImages.Num(); ++i)
	{
		UImage* Img = SparkImages[i];
		if (!Img || !SparkParams.IsValidIndex(i)) { continue; }
		if (T >= B || LayerSize.Y < 1.f)
		{
			Img->SetRenderOpacity(0.f);
			continue;
		}
		const FGachaSparkDot& Dot = SparkParams[i];
		const float K = FMath::Clamp((T - Dot.Delay) / FMath::Max(B - 0.05f, 0.05f), 0.f, 1.f);
		const float E = FWidgetAnimationUtils::EaseOutQuad(K);
		const FVector2D Pos = Center + Dot.Dir * (Dot.RadiusFrac * LayerSize.Y) * (1.f - E) - FVector2D(SparkDotSize * 0.5f, SparkDotSize * 0.5f);
		if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Img->Slot)) { CSlot->SetPosition(Pos); }
		Img->SetRenderOpacity(K <= 0.f ? 0.f : 0.85f * (1.f - E * 0.75f));
	}

	// 빌드업 수렴 링 — 바깥 큰 링이 중앙으로 가속 수축하며 진해짐 (SparkImages 꼬리 [N-5..N-3] 계약)
	if (FXLayer && LayerSize.Y > 1.f && SparkImages.Num() >= 5)
	{
		for (int32 r = 0; r < 3; ++r)
		{
			UImage* Ring = SparkImages[SparkImages.Num() - 5 + r];
			if (!Ring) { continue; }
			const float RK = (T - r * 0.15f) / FMath::Max(B * 0.55f, 0.2f);
			if (T >= B || RK < 0.f || RK > 1.f)
			{
				Ring->SetRenderOpacity(0.f);
				continue;
			}
			const float RScale = (LayerSize.Y * (0.25f + 1.05f * (1.f - RK * RK))) / CardFXBaseSize;
			Ring->SetRenderScale(FVector2D(RScale, RScale));
			if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Ring->Slot))
			{
				CSlot->SetPosition(Center - FVector2D(CardFXBaseSize * 0.5f, CardFXBaseSize * 0.5f));
			}
			Ring->SetRenderOpacity(0.45f * (0.4f + 0.6f * RK));
		}
	}

	// ② 플래시 + 임팩트 사운드(1회) — 한 순간 = 한 소리 (스팅어는 카드 등장으로 이동)
	if (T >= B && !bFlashFired)
	{
		bFlashFired = true;
		PlayUITag(CGUISoundTags::GachaFlash);
	}
	if (FlashOverlay)
	{
		FlashOverlay->SetRenderOpacity((T >= B && T < B + FlashDuration) ? (1.f - (T - B) / FlashDuration) : 0.f);
	}

	// 쇼크웨이브 링 (레어+, 신화 = 더블)
	auto DriveRing = [T](UImage* Ring, float T0, float Dur, float MaxScale)
	{
		if (!Ring) { return; }
		if (T < T0) { Ring->SetRenderOpacity(0.f); return; }
		const float K = FMath::Clamp((T - T0) / Dur, 0.f, 1.f);
		const float E = FWidgetAnimationUtils::EaseOutQuad(K);
		Ring->SetRenderOpacity(0.9f * (1.f - K));
		const float S = 0.25f + MaxScale * E;
		Ring->SetRenderScale(FVector2D(S, S));
	};
	if (Tier >= 2) { DriveRing(ShockwaveRing, B, ShockwaveDuration, 2.4f); }
	else if (ShockwaveRing) { ShockwaveRing->SetRenderOpacity(0.f); }
	if (Tier >= 5) { DriveRing(ShockwaveRing2, B + 0.12f, 0.6f, 3.1f); }
	else if (ShockwaveRing2) { ShockwaveRing2->SetRenderOpacity(0.f); }

	// 셰이크 — 루트 대신 CenterVBox (풀스크린 딤 가장자리 노출 방지), 연출 감소 게이트
	if (CenterVBox)
	{
		if (!bReduceMotion && Tier >= 4 && T >= B && T < B + ShakeDuration)
		{
			const float K = 1.f - (T - B) / ShakeDuration;
			const float M = ShakeAmp * K;
			CenterVBox->SetRenderTranslation(FVector2D(FMath::Sin(T * 93.f) * M, FMath::Cos(T * 87.f) * M));
		}
		else
		{
			CenterVBox->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	// ③ 공개 — 카드 스케일펀치(InnerClip 대상 — 클립 rect 동반 스케일) + 스파클 트리거 + 글린트 스윕
	for (int32 i = 0; i < RevealCards.Num(); ++i)
	{
		const float T0 = RevealStartTime + i * MultiStagger;
		UUserWidget* Card = RevealCards[i];
		UWidget* CardBlock = Card ? Card->GetParent() : nullptr;
		const int32 CardTier = RevealRarities.IsValidIndex(i) ? TierOf(RevealRarities[i]) : 0;
		if (T >= T0 && CardRevealedFlags.IsValidIndex(i) && !CardRevealedFlags[i])
		{
			CardRevealedFlags[i] = true;
			StartCardSparkle(i);
			// 한 순간 = 한 소리: 단일=등급 스팅어 / 10연차=일반 "통"·레어+ "팅" (교체, 레이어 금지)
			if (RevealCards.Num() == 1)
			{
				PlayUITag(StingTagForTier(CardTier));
			}
			else
			{
				PlayUITag(CardTier >= GlintTierMin ? CGUISoundTags::GachaShine : CGUISoundTags::GachaCardPop);
			}
		}
		const FVector2D CardSizeL = Card ? Card->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
		UPanelWidget* WrapPanel = CardBlock ? Cast<UPanelWidget>(CardBlock->GetParent()) : nullptr;
		if (CardBlock && T >= T0)
		{
			const float K = FMath::Clamp((T - T0) / PunchDuration, 0.f, 1.f);
			const float E = FWidgetAnimationUtils::EaseOutQuad(K);
			CardBlock->SetRenderOpacity(FMath::Clamp((T - T0) / CardFadeDuration, 0.f, 1.f));
			const float S = PunchScale + (1.f - PunchScale) * FWidgetAnimationUtils::EaseOutBack(K);
			CardBlock->SetRenderScale(FVector2D(S, S));
			// 아래서 떠오르며 등장 (방향성). 부유(idle bobbing)는 시도 후 기각 — 어색 (2026-07-11)
			CardBlock->SetRenderTranslation(FVector2D(0.f, CardRiseY * (1.f - E)));
		}
		// 수직 빛기둥 구동 (레전드리+) — 공개 0.3s 후 페이드인, 숨쉬는 갓레이
		if (CardTier >= 4 && WrapPanel && CardSizeL.Y > 1.f && T >= T0)
		{
			const int32 BeamIdx = (RevealCards.Num() > 1) ? 2 : 1;
			if (UImage* Beam = Cast<UImage>(WrapPanel->GetChildAt(BeamIdx)))
			{
				const float Since = T - T0;
				Beam->SetRenderScale(FVector2D(CardSizeL.X * 0.8f / CardFXBaseSize, CardSizeL.Y * 3.0f / CardFXBaseSize));
				Beam->SetRenderTranslation(FVector2D(0.f, -CardSizeL.Y * 1.15f));
				const float Breathe3 = 0.75f + 0.25f * FMath::Sin(Since * 1.4f);
				Beam->SetRenderOpacity(0.32f * Breathe3 * FMath::Clamp((Since - 0.3f) / 0.6f, 0.f, 1.f));
			}
		}
		// 등장 링 팝 — 카드 뒤에서 등급색 링이 퍼짐 (Wrap index 0)
		if (WrapPanel && T >= T0 && CardSizeL.Y > 1.f)
		{
			if (UImage* PopRing = Cast<UImage>(WrapPanel->GetChildAt(0)))
			{
				const float RK = FMath::Clamp((T - T0) / RingPopDuration, 0.f, 1.f);
				const float RS = CardSizeL.Y * (0.5f + 1.3f * FWidgetAnimationUtils::EaseOutQuad(RK)) / CardFXBaseSize;
				PopRing->SetRenderScale(FVector2D(RS, RS));
				PopRing->SetRenderOpacity(0.85f * (1.f - RK));
			}
		}
		if (Card && T >= T0)
		{
			// 아이콘 계층 팝 — 카드 정착보다 한 박자 늦게, 살짝 비틀리며 (카드 → 내용물 2단 등장)
			if (UWidget* Icon = Card->GetWidgetFromName(TEXT("EntityImage")))
			{
				const float IK = FMath::Clamp((T - T0 - IconPopDelay) / IconPopDuration, 0.f, 1.f);
				Icon->SetRenderOpacity(FMath::Clamp((T - T0 - IconPopDelay) / 0.06f, 0.f, 1.f));
				const float IB = FWidgetAnimationUtils::EaseOutBack(IK);
				const float IS = IconPopScale + (1.f - IconPopScale) * IB;
				Icon->SetRenderScale(FVector2D(IS, IS));
				Icon->SetRenderTransformAngle(IconPopAngleDeg * (1.f - IB));
			}
			// 그림자 착지 — 펀치 동안 눌렸다가 정착과 함께 원복 (접지감)
			if (UWidget* Shadow = Card->GetWidgetFromName(TEXT("Shadow_Img")))
			{
				const float SE = FWidgetAnimationUtils::EaseOutQuad(FMath::Clamp((T - T0) / PunchDuration, 0.f, 1.f));
				Shadow->SetRenderScale(FVector2D(0.6f + 0.4f * SE, 0.6f + 0.4f * SE));
				Shadow->SetRenderOpacity(0.5f + 0.5f * SE);
			}
			// 아웃라인 섬광 — 등장 순간 카드 모양 테두리가 등급색으로 확 켜졌다 식음
			// (카드 자체 RefreshOutline 이 Collapsed 초기화 → 창 안에선 매 틱 가시성 강제)
			if (UWidget* OutlineW = Card->GetWidgetFromName(TEXT("Outline")))
			{
				const float OFK = (T - T0) / OutlineFlashDuration;
				if (OFK < 1.f)
				{
					if (OutlineW->GetVisibility() != ESlateVisibility::HitTestInvisible)
					{
						OutlineW->SetVisibility(ESlateVisibility::HitTestInvisible);
					}
					OutlineW->SetRenderOpacity(FMath::Square(1.f - FMath::Clamp(OFK, 0.f, 1.f)));
				}
				else
				{
					OutlineW->SetRenderOpacity(0.f);
				}
			}
			// 카드별 등급색 번짐 (Wrap index 1 — 자식 순서 계약, 10연차만 존재)
			// 카드 등장 순간 그 자리에서 확 번졌다가(1.5x 과열) 브리딩 상태로 정착. 등급 높을수록 넓고 진하게.
			if (WrapPanel)
			{
				if (UImage* PerGlow = Cast<UImage>(WrapPanel->GetChildAt(1)))
				{
					if (CardSizeL.X > 1.f)
					{
						const float WMul = 1.25f + 0.15f * CardTier;
						const float HMul = 1.35f + 0.2f * CardTier;
						PerGlow->SetRenderScale(FVector2D(CardSizeL.X * WMul / CardFXBaseSize, CardSizeL.Y * HMul / CardFXBaseSize));
					}
					const float Since = T - T0;
					if (Since <= 0.f)
					{
						PerGlow->SetRenderOpacity(0.f);
					}
					else
					{
						const float BaseOp = 0.30f + 0.07f * CardTier;
						const float Pop = (Since < 0.35f) ? FMath::Lerp(1.5f, 1.f, Since / 0.35f) : 1.f;
						const float Breathe2 = 0.9f + 0.1f * FMath::Sin(Since * 1.8f);
						PerGlow->SetRenderOpacity(FMath::Min(BaseOp * Pop * Breathe2 * FMath::Clamp(Since / 0.12f, 0.f, 1.f), 0.9f));
					}
				}
			}
			// 화이트 블룸 (레전드리+, InnerClip index 1 — 자식 순서 계약)
			if (CardTier >= 4)
			{
				if (UPanelWidget* InnerPanel = Cast<UPanelWidget>(CardBlock))
				{
					if (UImage* Bloom = Cast<UImage>(InnerPanel->GetChildAt(1)))
					{
						const float BK = (T - T0) / BloomDuration;
						if (CardSizeL.X > 1.f)
						{
							Bloom->SetRenderScale(FVector2D(CardSizeL.X * 1.15f / CardFXBaseSize, CardSizeL.Y * 1.15f / CardFXBaseSize));
						}
						Bloom->SetRenderOpacity((BK < 1.f) ? (1.f - BK) * 0.95f : 0.f);
					}
				}
			}
		}
		// 트레일 글린트 점등 — 카드 정착 후 페이드인, 이후엔 머티리얼이 테두리를 계속 순환
		if (CardGlintImages.IsValidIndex(i) && CardGlintImages[i])
		{
			UImage* Trail = CardGlintImages[i];
			const float GK = (T - T0 - GlintDelay) / GlintDuration;
			if (GK > 0.f)
			{
				if (Trail->GetVisibility() == ESlateVisibility::Collapsed) { Trail->SetVisibility(ESlateVisibility::HitTestInvisible); }
				Trail->SetRenderOpacity(FMath::Clamp(GK, 0.f, 1.f));
			}
			else
			{
				Trail->SetRenderOpacity(0.f);
			}
		}
	}

	// 등급 텍스트 펀치인 (단일 뽑기만 — Visibility 는 BeginReveal 이 결정)
	if (ResultRarityText && ResultRarityText->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const float K = FMath::Clamp((T - RevealStartTime) / RarityTextDuration, 0.f, 1.f);
		ResultRarityText->SetRenderOpacity(K);
		const float S = RarityTextPunchScale - (RarityTextPunchScale - 1.f) * FWidgetAnimationUtils::EaseOutBack(K);
		ResultRarityText->SetRenderScale(FVector2D(S, S));

		// 레전드리+ — 텍스트 좌우로 등급색 라인이 쫙 펼쳐짐 (SparkImages 꼬리 [N-2..N-1] 계약)
		if (Tier >= 4 && FXLayer && LayerSize.Y > 1.f && SparkImages.Num() >= 2)
		{
			const FVector2D TxtSize = ResultRarityText->GetCachedGeometry().GetLocalSize();
			if (TxtSize.X > 1.f)
			{
				const FVector2D TxtCenter = FXLayer->GetCachedGeometry().AbsoluteToLocal(
					ResultRarityText->GetCachedGeometry().LocalToAbsolute(TxtSize * 0.5f));
				const float LE = FWidgetAnimationUtils::EaseOutQuad(FMath::Clamp((T - RevealStartTime - 0.1f) / 0.45f, 0.f, 1.f));
				const float LineW = 60.f + 380.f * LE;
				for (int32 l = 0; l < 2; ++l)
				{
					UImage* Line = SparkImages[SparkImages.Num() - 2 + l];
					if (!Line) { continue; }
					const float Side = (l == 0) ? -1.f : 1.f;
					Line->SetRenderScale(FVector2D(LineW / CardFXBaseSize, 6.f / CardFXBaseSize));
					if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Line->Slot))
					{
						CSlot->SetPosition(TxtCenter + FVector2D(
							Side * (TxtSize.X * 0.5f + 60.f + LineW * 0.5f) - CardFXBaseSize * 0.5f,
							-CardFXBaseSize * 0.5f));
					}
					Line->SetRenderOpacity(0.8f * LE);
				}
			}
		}
	}

	// ④ 여운 — 배후광 브리딩 / 회전 광선(에픽+) / 비네트(레전드리+)
	// 크기 = 카드 영역 실측 비례 (RenderScale — 레이아웃 불침투). 브리딩 = 밝기만(스케일 펄스 없음).
	const float TReveal = T - RevealStartTime;
	const FVector2D CardArea = CardSlot ? CardSlot->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const bool bCardAreaValid = CardArea.X > 1.f && CardArea.Y > 1.f;
	// 줄 배후광 (등급색 번짐) — 단일 뽑기 전용 (10연차는 카드별 번짐이 담당 — "카드 자리에서" 사용자 확정), 등급 비례
	if (CardBackGlow)
	{
		if (RevealCards.Num() > 1)
		{
			CardBackGlow->SetRenderOpacity(0.f);
		}
		else
		{
		const float SpreadMul = 0.8f + 0.3f * Tier;   // 일반 0.8x → 신화 2.3x 번짐 폭
		const float GlowMul = 0.85f + 0.2f * Tier;    // 일반 0.85x → 신화 1.85x 세기
		if (bCardAreaValid)
		{
			CardBackGlow->SetRenderScale(FVector2D(
				(CardArea.X + BackglowMarginW * SpreadMul) / CardFXBaseSize,
				(CardArea.Y + BackglowMarginH * SpreadMul) / CardFXBaseSize));
		}
		if (TReveal >= 0.f && Tier >= 1)
		{
			const float Breathe = 0.5f + 0.5f * FMath::Sin(TReveal * 1.8f);
			CardBackGlow->SetRenderOpacity(FMath::Min(
				BackglowIntensity * (0.28f + 0.16f * Breathe) * GlowMul * FMath::Clamp(TReveal / 0.5f, 0.f, 1.f), 0.85f));
		}
		else
		{
			// 일반 등급도 완전 암전은 피함 (미세 배후광)
			CardBackGlow->SetRenderOpacity((TReveal >= 0.f) ? 0.12f * BackglowIntensity : 0.f);
		}
		}
	}
	// 버튼/닫기 지연 등장 (+페이드/라이즈) — 그 전까지 오조작 차단
	if (!bButtonsShown && T >= ButtonsTime)
	{
		bButtonsShown = true;
		if (ConfirmButton) { ConfirmButton->SetVisibility(ESlateVisibility::Visible); }
		if (UIE_CloseButton) { UIE_CloseButton->SetVisibility(ESlateVisibility::Visible); }
	}
	if (bButtonsShown)
	{
		const float K = FMath::Clamp((T - ButtonsTime) / ButtonFadeDuration, 0.f, 1.f);
		const FVector2D Rise(0.f, ButtonRise * (1.f - K));
		auto FadeIn = [K, Rise](UWidget* W) { if (W) { W->SetRenderOpacity(K); W->SetRenderTranslation(Rise); } };
		FadeIn(ConfirmButton);
		FadeIn(UIE_CloseButton);
	}
}

void UGachaRevealPresentationWidget::SkipToSettle()
{
	if (!bFlashFired)
	{
		bFlashFired = true;
		PlayUITag(StingTagForTier(TierOf(BestRarity)));
	}
	// 미공개 카드는 스파클 없이 결과 상태로 (진행 중 스파클은 자연 종료, 등장 대상 = InnerClip)
	for (int32 i = 0; i < RevealCards.Num(); ++i)
	{
		if (CardRevealedFlags.IsValidIndex(i)) { CardRevealedFlags[i] = true; }
		if (UUserWidget* Card = RevealCards[i])
		{
			if (UWidget* CardBlock = Card->GetParent())
			{
				CardBlock->SetRenderOpacity(1.f);
				CardBlock->SetRenderScale(FVector2D(1.f, 1.f));
				CardBlock->SetRenderTranslation(FVector2D::ZeroVector);
			}
		}
	}
	StageTime = SettleTime;
}

FReply UGachaRevealPresentationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빌드업~공개 중 탭 = 스킵 (여운 상태로 시간 점프)
	if (bStagingActive && StageTime < SettleTime)
	{
		SkipToSettle();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGachaRevealPresentationWidget::PlayUITag(const FGameplayTag& Tag)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* Snd = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			Snd->PlayUISound(Tag);
		}
	}
}

FGameplayTag UGachaRevealPresentationWidget::StingTagForTier(int32 Tier) const
{
	if (Tier >= 4) { return CGUISoundTags::GachaResultLegendary; }
	if (Tier == 3) { return CGUISoundTags::GachaResultEpic; }
	return CGUISoundTags::GachaResultRare;
}

void UGachaRevealPresentationWidget::StartCardSparkle(int32 Index)
{
	if (!RevealSparkles.IsValidIndex(Index) || !RevealRarities.IsValidIndex(Index)) { return; }
	UImage* Sp = RevealSparkles[Index];
	const ELootBoxRarity R = RevealRarities[Index];
	if (!Sp || !RaritySparkleMap.Contains(R)) { return; }

	UMaterialInstanceDynamic* MID = RevealSparkleMIDs.IsValidIndex(Index) ? RevealSparkleMIDs[Index].Get() : nullptr;
	ApplySparkleBrush(Sp, MID, R);
	if (RevealSparkleMIDs.IsValidIndex(Index)) { RevealSparkleMIDs[Index] = MID; }

	RevealSparkleElapsed[Index] = 0.f;
	Sp->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplySparkleFrame(Sp, 0.f, RaritySizeMul(R));
}

void UGachaRevealPresentationWidget::ApplySparkleBrush(UImage* Img, UMaterialInstanceDynamic*& MID, ELootBoxRarity Rarity)
{
	if (!Img) { return; }
	UTexture2D* Tex = nullptr;
	if (TObjectPtr<UTexture2D>* Found = RaritySparkleMap.Find(Rarity)) { Tex = Found->Get(); }
	if (!Tex) { return; }

	const FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(Rarity);
	if (SparkleMaterial)
	{
		if (!MID || MID->Parent != SparkleMaterial) { MID = UMaterialInstanceDynamic::Create(SparkleMaterial, this); }
		MID->SetTextureParameterValue(FName("Tex"), Tex);
		MID->SetVectorParameterValue(FName("Tint"), RarityColor);
		Img->SetColorAndOpacity(FLinearColor::White);
		Img->SetBrushFromMaterial(MID);
	}
	else
	{
		Img->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
		Img->SetColorAndOpacity(RarityColor);
	}
}

void UGachaRevealPresentationWidget::ApplySparkleFrame(UImage* Img, float T, float SizeMul)
{
	if (!Img) { return; }
	const float Ease = 1.f - FMath::Square(1.f - T); // easeOut
	const float Scale = FMath::Lerp(SparkleStartScale, SparkleEndScale, Ease) * SizeMul;
	const float Angle = FMath::Lerp(0.f, SparkleSpinDeg, T);
	const float Op = (T < 0.6f) ? 1.f : FMath::Lerp(1.f, 0.f, (T - 0.6f) / 0.4f);
	Img->SetRenderScale(FVector2D(Scale, Scale));
	Img->SetRenderTransformAngle(Angle);
	Img->SetRenderOpacity(Op);
}

UWidget* UGachaRevealPresentationWidget::GetConfirmButtonWidget() const
{
	return ConfirmButton;
}

void UGachaRevealPresentationWidget::OnConfirmClicked()
{
	// 튜토리얼 M11/M12 — ConfirmPull→CloseGacha 전이 (연출 닫기 전에 통지)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* M = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			M->NotifyGachaRevealConfirmed();
		}
	}
	CloseSelf();
}
void UGachaRevealPresentationWidget::OnCloseClicked() { CloseSelf(); }

void UGachaRevealPresentationWidget::CloseSelf()
{
	// CommonActivatableWidget 을 뷰포트에 직접 붙였으므로 RemoveFromParent 만으로는
	// CommonUI 액션 라우터 스택에서 안 빠진다 → Menu 입력 컨피그가 남아 EInputMode 와 무관하게
	// 월드 입력(카메라 이동/빌딩 클릭)이 전면 차단된다(강화모달에서 같은 사고 이력).
	// 활성 상태가 아니면 no-op 이라 무해하다.
	if (IsActivated())
	{
		DeactivateWidget();
	}
	RemoveFromParent();
}
