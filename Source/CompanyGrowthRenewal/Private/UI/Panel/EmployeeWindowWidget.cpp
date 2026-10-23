// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/EmployeeWindowWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentListWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "UI/Panel/EnhanceStarforceModalWidget.h"
#include "UI/Panel/PotentialOddsWidget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "TimerManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "UI/UIBase.h"
#include "Manager/EmployeeManager.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Utils/EmployeeEquipmentAppearanceResolver.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Enum/GachaTier.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Panel/OfficeRecruitmentModalWidget.h"
#include "Manager/EntityManager.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Data/EntityCardData.h"
#include "Data/EmployeeRosterRules.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EmployeeTypes.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/ResourceType.h"
#include "Enum/ItemType.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"
#include "UI/Element/Employee/EmployeeLevelUpCardWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Employee/PotentialResetWorkspaceWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "CommonButtonBase.h"
#include "UI/Gacha/GachaCaptureStage.h"
#include "Manager/ItemInventoryManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/ListView.h"
#include "Components/Image.h"
#include "Components/ComboBoxString.h"
#include "Enum/ProductionDiscipline.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/BorderSlot.h"
#include "Components/ProgressBar.h"
#include "CommonTextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

// 캡처 하네스(다른 TU)가 "생산이 어떤 폭으로 빈자리 예산을 냈는지" 를 읽는 축. 하네스는 캡처 해상도의
// 지오메트리로 다시 재므로, 라이브 뷰포트 폭과 갈리면 같은 목록이 두 예산을 갖는다 — 그 차이가
// 결함인지 해상도 차이인지는 생산이 쓴 폭을 남겨야만 매니페스트에서 갈린다. 헤더 변경 없이 TU 를 건너는 통로.
double GEmployeeRosterSyncWidth = -1.0;
int32 GEmployeeRosterSyncBudget = -1;
int32 GEmployeeRosterSyncHireWanted = -1;

namespace
{
	// P2 강화 스펙의 레벨 캡 — 지금은 "Lv.N / 30" 표시 전용, 실제 XP 캡은 P2 에서

	// ===== 값 변화 연출 상수 (spec §9) =====
	constexpr float SlotSparkleLife  = 0.5f;   // 장비 교체 스파클 1회 팝
	constexpr float OvrCountDuration = 0.6f;   // 업무력 숫자 갱신 시간
	constexpr float StatCountDuration = 0.4f;  // 능력치 카운트업 = 행 하이라이트와 같은 길이
	constexpr float StatHighlightPeak = 0.06f; // 행 화이트 하이라이트 최대 알파
	constexpr float StatBonusFadeDuration = 0.3f; // 보너스 칩 페이드인
	constexpr float CtaSweepPeriod = 6.f;      // 골드 CTA 샤인 주기
	constexpr float CtaSweepDuration = 0.6f;   // 그 중 스윕이 지나가는 구간
	constexpr float CtaSweepAlpha = 0.18f;

	FLinearColor GetEmployeeHomeRarityTextColor(ELootBoxRarity Rarity)
	{
		// 밝은 배지는 공용 등급 잉크의 색상을 유지하고 본문 대비에 맞게 명도만 낮춘다.
		FLinearColor Ink = FLootBoxRarityUtility::GetRarityInkOnLight(Rarity) * 0.18f;
		Ink.A = 1.f;
		return Ink;
	}

	// 업무력 숫자 표기 — RefreshHero 와 카운트업 틱이 같은 포맷을 사용한다.
	FText FormatOverallValue(int32 Value)
	{
		return FText::FromString(FString::Printf(TEXT("%d"), Value));
	}

	// 좌→우 글린트 스윕 공통 구동. T01 = 0~1, 밖이면 알파 0 으로 끄고 false 반환.
	bool DriveShineSweep(UImage* Shine, float T01, float Alpha)
	{
		if (!Shine)
		{
			return false;
		}
		if (T01 < 0.f || T01 >= 1.f)
		{
			// 알파만 내리면 마지막 트랜슬레이션이 남아 버튼 밖(캔버스 밖까지) 유령 사각형이 선다 — 좌표도 되돌린다
			Shine->SetRenderOpacity(0.f);
			Shine->SetRenderTranslation(FVector2D::ZeroVector);
			return false;
		}
		// 폭은 샤인 이미지 자신의 캐시 지오메트리 — 부모(알약/버튼)에 맞춰 저작되므로 한 판을 정확히 건넌다
		const float Width = FMath::Max(static_cast<float>(Shine->GetCachedGeometry().GetLocalSize().X), 1.f);
		// 스윕 끝점은 컨테이너 우변 − 스윕폭 — 여기서 멈추지 않으면 렌더 사각형이 Wrap(클립) 밖으로 나간다
		float TrackWidth = Width;
		for (const UWidget* Ancestor = Shine->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
		{
			const float AncestorWidth = static_cast<float>(Ancestor->GetCachedGeometry().GetLocalSize().X);
			if (AncestorWidth > TrackWidth) { TrackWidth = AncestorWidth; break; }
		}
		const float SweepEnd = FMath::Max(TrackWidth - Width, 0.f);
		Shine->SetRenderTranslation(FVector2D(FMath::Lerp(-Width, SweepEnd, FWidgetAnimationUtils::EaseOutQuad(T01)), 0.f));
		Shine->SetRenderOpacity(Alpha);
		return true;
	}

	// v3.1 세로 명함 타일 = 저작 엔트리 224x300 + 간격 12 (apply_v2_growth.ROSTER / apply_v2_home.ROSTER_ENTRY_GAP)
	constexpr double RosterTileWidth = 150.0;
	constexpr double RosterTileHeight = 200.0;
	constexpr double RosterTileGap = 12.0;

	// 좌측 세로 레일의 2열 스크롤 격자가 담을 수 있는 타일 수. 폭에서 열 수, 높이에서 보이는 행을 구하고
	// 세로 스크롤이라 화면 밖 행도 유효하므로 스크롤 버퍼(+8행)를 더해 남은 정원을 전부 담는다(구 가로 1행 폭 예산에서 전환).
	// 첫 배치 전에는 폭이 0 이라 0 을 돌려준다(NativeTick 이 실측 후 SyncRosterHireTiles 로 맞춘다).
	int32 VisibleRosterTileBudget(const UListView* Strip)
	{
		if (!Strip)
		{
			return 0;
		}
		const FVector2D StripSize = Strip->GetCachedGeometry().GetLocalSize();
		if (StripSize.X < RosterTileWidth)
		{
			return 0;
		}
		const int32 Columns = FMath::Max(1,
			FMath::FloorToInt32((StripSize.X + RosterTileGap) / (RosterTileWidth + RosterTileGap)));
		const int32 VisibleRows = StripSize.Y >= RosterTileHeight
			? FMath::FloorToInt32((StripSize.Y + RosterTileGap) / (RosterTileHeight + RosterTileGap))
			: 1;
		// 보이는 행 + 스크롤 버퍼 — clamp(예산−보유, 0, 남은정원) 이 남은 정원 전부를 빈자리로 깔게 한다
		return Columns * FMath::Max(1, VisibleRows + 8);
	}

	// 목록 구성(직원 타일 / 빈자리 타일) — 배열 복사 없이 아이템 플래그만 읽는다
	void CountRosterItems(const UListView* Strip, int32& OutEmployeeTiles, int32& OutHireTiles)
	{
		OutEmployeeTiles = 0;
		OutHireTiles = 0;
		if (!Strip)
		{
			return;
		}
		for (const UObject* Item : Strip->GetListItems())
		{
			const UEntityCardData* CardData = Cast<UEntityCardData>(Item);
			if (!CardData) continue;
			if (CardData->bIsHireSlot) ++OutHireTiles; else ++OutEmployeeTiles;
		}
	}

	// 띠 재충전은 예산이나 구성이 바뀐 틱에만 — 정원이 고갈돼 예산을 영영 못 채우는 상태에서
	// 매 틱 직원 배열을 두 번 복사하지 않게. 멤버 추가는 헤더 변경이라 띠별 파일 스코프 캐시를 쓴다.
	TMap<TWeakObjectPtr<const UListView>, FIntVector> RosterSyncCache;

	bool RosterSyncNeeded(const UListView* Strip, int32 Budget)
	{
		// 폭이 아직 0(첫 배치 전)이면 예산을 모른다 — 그 상태를 캐시에 적지 말고 다음 틱으로 미룬다
		if (!Strip || Budget <= 0)
		{
			return false;
		}
		// 죽은 약참조는 삽입 때마다 걷는다 — 8개 임계는 레벨 전환/재오픈이 반복되면 stale 을 남겼다
		for (auto It = RosterSyncCache.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) It.RemoveCurrent(); }
		int32 EmployeeTiles = 0, HireTiles = 0;
		CountRosterItems(Strip, EmployeeTiles, HireTiles);
		// 아이템 "수" 가 아니라 구성(직원·빈자리)을 비교한다 — 수가 같은데 구성만 바뀐 갱신을 건너뛰면
		// 빈자리 개수가 예산과 어긋난 채 굳는다
		const FIntVector Now(Budget, EmployeeTiles, HireTiles);
		FIntVector& Last = RosterSyncCache.FindOrAdd(Strip, FIntVector(-1, -1, -1));
		if (Last == Now)
		{
			return false;
		}
		Last = Now;
		return true;
	}

	// 빈자리 타일은 남은 정원과 가시 폭 중 작은 쪽까지 채운다 — 죽은 여백이 띠의 절반을 먹지 않게.
	// 목표 개수를 **정확히** 맞춘다(초과분 제거·부족분 추가). 목록을 통째로 다시 만들지 않으므로
	// 직원 타일의 선택/펀치는 흔들리지 않는다.
	void SyncRosterHireTiles(UListView* Strip, UObject* ItemOuter, int32 RemainingSeats)
	{
		const int32 Budget = VisibleRosterTileBudget(Strip);
		if (!Strip || !ItemOuter || Budget <= 0)
		{
			return;
		}
		int32 EmployeeTiles = 0, HireTiles = 0;
		CountRosterItems(Strip, EmployeeTiles, HireTiles);
		const int32 Wanted = FMath::Clamp(Budget - EmployeeTiles, 0, FMath::Max(RemainingSeats, 0));
		GEmployeeRosterSyncWidth = Strip->GetCachedGeometry().GetLocalSize().X;
		GEmployeeRosterSyncBudget = Budget;
		GEmployeeRosterSyncHireWanted = Wanted;
		if (HireTiles == Wanted)
		{
			return;
		}
		TArray<UObject*> Items = Strip->GetListItems();
		for (int32 Extra = HireTiles; Extra > Wanted; --Extra)
		{
			for (int32 Index = Items.Num() - 1; Index >= 0; --Index)
			{
				const UEntityCardData* CardData = Cast<UEntityCardData>(Items[Index]);
				if (CardData && CardData->bIsHireSlot) { Strip->RemoveItem(Items[Index]); Items.RemoveAt(Index); break; }
			}
		}
		for (int32 Missing = HireTiles; Missing < Wanted; ++Missing)
		{
			UEntityCardData* HireData = NewObject<UEntityCardData>(ItemOuter);
			HireData->bIsHireSlot = true;
			Strip->AddItem(HireData);
		}
	}
}

// 캡처 하네스(다른 TU)가 "방금 그린 폭으로 빈자리를 다시 맞춰라" 고 부르는 진입점.
// 생산은 라이브 뷰포트 폭으로 틱하고 FWidgetRenderer 는 위젯을 틱하지 않으므로, 캡처 해상도에서는
// 이 호출이 없으면 타일 수가 렌더 폭과 구조적으로 어긋난다. 헤더 변경 없이 TU 를 건너는 통로.
void SyncEmployeeRosterHireTilesForCapture(UListView* Strip, UObject* ItemOuter, int32 RemainingSeats);
void SyncEmployeeRosterHireTilesForCapture(UListView* Strip, UObject* ItemOuter, int32 RemainingSeats)
{
	SyncRosterHireTiles(Strip, ItemOuter, RemainingSeats);
}

void UEmployeeWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CurrentEmployeePage = EEmployeeWindowPage::Home;
	if (HeroEquipmentOpenButton) HeroEquipmentOpenButton->OnClicked().AddUObject(this, &ThisClass::HandleHeroEquipmentOpen);
	if (EquipmentList) EquipmentList->OnItemPicked.BindUObject(this, &ThisClass::HandleEquipmentItemPicked);
	ShowEquipmentColumn(false);
	UButtonWidget* HeroSlots[] = {HeroEquipmentSlot0, HeroEquipmentSlot1, HeroEquipmentSlot2, HeroEquipmentSlot3, HeroEquipmentSlot4, HeroEquipmentSlot5};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(HeroSlots); ++Index)
	{
		if (HeroSlots[Index]) HeroSlots[Index]->OnClicked().AddUObject(this, &ThisClass::HandleHeroEquipmentSlot, Index);
	}
	// 가로 스크롤바는 띠 하단 = 타일 이름 밴드를 덮어 이름을 반으로 자른다 — 스크롤은 드래그로만.
	// 저작에서 못 박을 수 없는 축이다(SetScrollbarVisibility 는 런타임 전용, 두께 0 만으로는 트랙이 남는다)
	if (RosterListView) RosterListView->SetScrollbarVisibility(ESlateVisibility::Collapsed);

	// 스탯 셀 탭 → 효과 툴팁. 페이로드 바인딩이라 핸들러 하나로 6칸 처리(UFUNCTION 6개 불필요)
	for (int32 i = 0; i < NumEmployeeStats; ++i)
	{
		if (UCommonButtonBase* Btn = Cast<UCommonButtonBase>(
			GetWidgetFromName(*FString::Printf(TEXT("HeroStatBtn%d"), i))))
		{
			Btn->OnClicked().AddUObject(this, &UEmployeeWindowWidget::HandleStatInfoTapped, i);
		}
	}
	UButtonWidget* StatPlusButtons[] = { StatPlusButton0, StatPlusButton1, StatPlusButton2, StatPlusButton3, StatPlusButton4, StatPlusButton5 };
	for (int32 i = 0; i < UE_ARRAY_COUNT(StatPlusButtons); ++i)
	{
		if (StatPlusButtons[i]) StatPlusButtons[i]->OnClicked().AddUObject(this, &ThisClass::HandleStatPlusClicked, i);
	}
	if (UBorder* WorkPowerBorder = Cast<UBorder>(GetWidgetFromName(TEXT("OvrPill"))))
	{
		WorkPowerBorder->SetVisibility(ESlateVisibility::Visible);
		WorkPowerBorder->OnMouseButtonDownEvent.BindDynamic(this, &UEmployeeWindowWidget::HandleWorkPowerTapped);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
		EquipmentManager = GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
	}
	if (EquipmentManager) EquipmentManager->OnEquipmentChanged.AddUObject(this, &ThisClass::HandleHeroEquipmentChanged);

	if (RosterListView)
	{
		RosterListView->SetSelectionMode(ESelectionMode::Single);
		RosterListView->OnEntryWidgetGenerated().AddUObject(this, &UEmployeeWindowWidget::OnRosterEntryGenerated);
		RosterListView->OnItemSelectionChanged().AddUObject(this, &UEmployeeWindowWidget::OnRosterItemSelectionChanged);
	}

	if (RosterSortCycleBtn)
	{
		RosterSortCycleBtn->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnRosterSortCycleClicked);
	}
	UpdateRosterSortLabel();

	if (RosterFilterCombo)
	{
		RosterFilterCombo->OnSelectionChanged.AddDynamic(this, &UEmployeeWindowWidget::OnRosterFilterChanged);
	}

	if (EnhanceButton)
	{
		EnhanceButton->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnEnhanceClicked);
	}
	if (FireButton)
	{
		FireButton->OnClicked().AddUObject(this, &UEmployeeWindowWidget::OnFireClicked);
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UEmployeeWindowWidget::OnCloseDelegate);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.AddUObject(this, &UEmployeeWindowWidget::HandleEmployeeStatsChanged);
		EmployeeManager->OnEmployeeDisciplineChanged.AddUObject(this, &UEmployeeWindowWidget::HandleEmployeeDisciplineChanged);
		EmployeeManager->OnEmployeeLevelUp.AddUObject(this, &UEmployeeWindowWidget::HandleEmployeeLevelUp);
		EmployeeManager->OnEmployeeRosterChanged.AddUObject(this, &UEmployeeWindowWidget::HandleRosterChanged);
		EmployeeManager->OnEmployeePortraitUpdated.AddUObject(this, &ThisClass::HandleHeroPortraitChanged);
	}

	if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResourceMgr->OnResourceChanged.AddUObject(this, &UEmployeeWindowWidget::HandleResourceChanged);
	}

	if (UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>())
	{
		ItemMgr->OnItemChanged.AddUObject(this, &ThisClass::HandleItemChanged);
	}

	EnsurePotentialWorkspace();
	CacheEffectWidgets();
}

void UEmployeeWindowWidget::CacheEffectWidgets()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// 연출 이미지는 전부 알파 0 에서 출발한다 — 저작 실수로 켜진 채 남아도 여기서 잡힌다
	auto Lookup = [this](const FString& Name) -> UImage*
	{
		UImage* Found = Cast<UImage>(GetWidgetFromName(*Name));
		if (Found)
		{
			Found->SetRenderOpacity(0.f);
		}
		return Found;
	};
	auto LookupIndexed = [&Lookup](const TCHAR* Prefix, int32 Index) { return Lookup(FString(Prefix) + FString::FromInt(Index)); };

	UButtonWidget* HeroSlots[] = {HeroEquipmentSlot0, HeroEquipmentSlot1, HeroEquipmentSlot2, HeroEquipmentSlot3, HeroEquipmentSlot4, HeroEquipmentSlot5};
	for (int32 i = 0; i < 6; ++i)
	{
		HeroSlotFrameImages[i] = HeroSlots[i] ? Cast<UImage>(HeroSlots[i]->GetWidgetFromName(TEXT("GradeFrame"))) : nullptr;
		SlotSparkleImages[i] = LookupIndexed(TEXT("SlotSparkle"), i);
		if (i < NumEmployeeStats) StatRowBGImages[i] = LookupIndexed(TEXT("HeroStatRowBG"), i);
		// 스파클/샤인 텍스처는 DT_UIVFXTexture 단일 진실 (WBP 엔 빈 Image 만 배치)
		if (TableMgr) { TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SlotSparkleImages[i].Get()); }
	}
	// 능력치 행 글리프. Lookup 을 쓰면 안 된다 — 저 람다는 연출 이미지용이라 알파를 0 으로 눕힌다
	static const TCHAR* StatIconPaths[] = {
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Stat/T_UIIcon_Stat_Speed.T_UIIcon_Stat_Speed"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Stat/T_UIIcon_Stat_Luck.T_UIIcon_Stat_Luck"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Stat/T_UIIcon_Stat_Composure.T_UIIcon_Stat_Composure"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Stat/T_UIIcon_Stat_Exp.T_UIIcon_Stat_Exp"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Stat/T_UIIcon_Stat_Stamina.T_UIIcon_Stat_Stamina"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Stat/T_UIIcon_Stat_Focus.T_UIIcon_Stat_Focus"),
	};
	static_assert(UE_ARRAY_COUNT(StatIconPaths) == static_cast<int32>(EEmployeeStatIndex::Count),
		"능력치 아이콘 수가 EEmployeeStatIndex 와 어긋난다");
	for (int32 i = 0; i < UE_ARRAY_COUNT(StatIconPaths); ++i)
	{
		UImage* StatIcon = Cast<UImage>(GetWidgetFromName(*(FString(TEXT("HeroStatIcon")) + FString::FromInt(i))));
		UTexture2D* IconTex = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(StatIconPaths[i])).LoadSynchronous();
		if (!StatIcon || !IconTex)
		{
			continue;
		}
		StatIcon->SetBrushFromTexture(IconTex, false);   // WBP 의 ImageSize 28 을 유지한다
		StatIcon->SetVisibility(ESlateVisibility::HitTestInvisible);   // Visible 이면 행 전체 툴팁 탭을 먹는다
	}

	EnhanceShineImage = Lookup(TEXT("EnhanceShine"));

	// 필터 칩 SDF — 크기 재주입이 틱 경로라 이름 조회는 여기 1회뿐 (알파 0 대상 아님)
	FilterChipSizeWidget = GetWidgetFromName(TEXT("FilterChipBG"));
	FilterChipLayers[0] = Cast<UImage>(GetWidgetFromName(TEXT("FilterChipBG")));
	FilterChipLayers[1] = Cast<UImage>(GetWidgetFromName(TEXT("FilterChipLine")));
	if (TableMgr)
	{
		TableMgr->ApplyUIVFXTexture(TEXT("CTAShine"), EnhanceShineImage.Get());
	}
}

void UEmployeeWindowWidget::SyncEffectBaselineOwner()
{
	if (EffectBaselineEmployeeID == SelectedEmployeeID)
	{
		return;
	}
	ResetValueEffectBaselines();
	EffectBaselineEmployeeID = SelectedEmployeeID;
}

void UEmployeeWindowWidget::ResetValueEffectBaselines()
{
	OvrCount.Finish();
	LastOverallValue = INDEX_NONE;

	StarChipPunch = FScalePunchAnimation();
	LastStarCount = INDEX_NONE;

	UCommonTextBlock* StatBonuses[NumEmployeeStats] = { HeroStatBonus0, HeroStatBonus1, HeroStatBonus2, HeroStatBonus3, HeroStatBonus4, HeroStatBonus5 };
	for (int32 i = 0; i < NumEmployeeStats; ++i)
	{
		StatCount[i].Finish();
		LastStatValues[i] = INDEX_NONE;
		StatHighlightElapsed[i] = -1.f;
		StatBonusFadeElapsed[i] = -1.f;
		SlotSparkleElapsed[i] = -1.f;
		if (StatRowBGImages[i].IsValid()) { StatRowBGImages[i]->SetRenderOpacity(0.f); }
		if (SlotSparkleImages[i].IsValid()) { SlotSparkleImages[i]->SetRenderOpacity(0.f); }
		// 페이드 중 직원이 바뀌면 보너스 칩이 반투명으로 굳는다
		if (StatBonuses[i]) { StatBonuses[i]->SetRenderOpacity(1.f); }
	}
	LastSlotItemIds.Reset();

}

bool UEmployeeWindowWidget::CanAffordAnyHire() const
{
	const URecruitmentManagerSubsystem* RecruitMgr =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	if (!RecruitMgr)
	{
		return false;
	}
	// 한 티어라도 뽑을 수 있으면 "채용 가능" — 채용권만 있는 플레이어를 정적으로 두면 안내가 죽는다
	return RecruitMgr->CanExecuteGachaPull(EGachaTier::Normal)
		|| RecruitMgr->CanExecuteGachaPull(EGachaTier::Advanced)
		|| RecruitMgr->CanExecuteGachaPull(EGachaTier::Premium);
}

int32 UEmployeeWindowWidget::GetAlwaysOnEffectCount() const
{
	int32 Count = 0;
	// 아래 세 게이트는 NativeTick 의 구동 조건과 같은 식이어야 로그와 실제 연출이 갈리지 않는다
	if (HeroRarityGlow && HeroRarityGlow->GetVisibility() != ESlateVisibility::Hidden)
	{
		++Count;
	}
	// CTA 샤인 스윕 — 강화와 레벨업 중 한 버튼에서만 돈다.
	const bool bLevelUpSweep = DisciplineCard && DisciplineCard->IsLevelUpReady() && DisciplineCard->GetLevelUpShineImage();
	const bool bEnhanceSweep = EnhanceShineImage.IsValid() && EnhanceButton && EnhanceButton->GetIsEnabled();
	if (bLevelUpSweep || bEnhanceSweep)
	{
		++Count;
	}
	if (RosterListView && CanAffordAnyHire())
	{
		for (const UUserWidget* Entry : RosterListView->GetDisplayedEntryWidgets())
		{
			const UEmployeeRosterCardWidget* Card = Cast<UEmployeeRosterCardWidget>(Entry);
			if (Card && Card->IsHireSlot())
			{
				++Count;
				break;
			}
		}
	}
	return Count;
}

void UEmployeeWindowWidget::RefreshHireSlotAffordability()
{
	if (!RosterListView)
	{
		return;
	}
	const bool bAffordable = CanAffordAnyHire();
	for (UUserWidget* Entry : RosterListView->GetDisplayedEntryWidgets())
	{
		UEmployeeRosterCardWidget* Card = Cast<UEmployeeRosterCardWidget>(Entry);
		if (Card && Card->IsHireSlot())
		{
			Card->SetHireSlot(bAffordable);
		}
	}
}

void UEmployeeWindowWidget::NativeDestruct()
{
	FinishPageTransition();
	if (HeroEquipmentOpenButton) HeroEquipmentOpenButton->OnClicked().RemoveAll(this);
	if (EquipmentList) EquipmentList->OnItemPicked.Unbind();
	UButtonWidget* HeroSlots[] = {HeroEquipmentSlot0, HeroEquipmentSlot1, HeroEquipmentSlot2, HeroEquipmentSlot3, HeroEquipmentSlot4, HeroEquipmentSlot5};
	for (UButtonWidget* HeroSlot : HeroSlots) if (HeroSlot) HeroSlot->OnClicked().RemoveAll(this);
	if (EquipmentManager) EquipmentManager->OnEquipmentChanged.RemoveAll(this);
	HeroSlotIconMaterials.Empty();
	bHasExplicitEmployeeContext = false;
	PendingEquipmentEmployeeId = INDEX_NONE;
	PendingEquipmentInstanceId.Invalidate();
	if (RosterListView)
	{
		RosterListView->OnEntryWidgetGenerated().RemoveAll(this);
		RosterListView->OnItemSelectionChanged().RemoveAll(this);
	}

	if (RosterSortCycleBtn)
	{
		RosterSortCycleBtn->OnClicked().RemoveAll(this);
	}

	if (RosterFilterCombo)
	{
		RosterFilterCombo->OnSelectionChanged.RemoveDynamic(this, &UEmployeeWindowWidget::OnRosterFilterChanged);
	}

	if (PotentialWorkspace)
	{
		PotentialWorkspace->Suspend();
		PotentialWorkspace->OnRequestOddsTable.Unbind();
		PotentialWorkspace->OnRequestShop.Unbind();
	}

	// 스탯 셀 탭 타깃 — NativeConstruct 의 AddUObject 와 쌍. 빠뜨리면 창 재오픈마다 누적돼 1탭에 N번 발화한다
	for (int32 i = 0; i < NumEmployeeStats; ++i)
	{
		if (UCommonButtonBase* Btn = Cast<UCommonButtonBase>(
			GetWidgetFromName(*FString::Printf(TEXT("HeroStatBtn%d"), i))))
		{
			Btn->OnClicked().RemoveAll(this);
		}
	}
	UButtonWidget* StatPlusButtons[] = { StatPlusButton0, StatPlusButton1, StatPlusButton2, StatPlusButton3, StatPlusButton4, StatPlusButton5 };
	for (UButtonWidget* StatPlusButton : StatPlusButtons)
	{
		if (StatPlusButton) StatPlusButton->OnClicked().RemoveAll(this);
	}
	if (UBorder* WorkPowerBorder = Cast<UBorder>(GetWidgetFromName(TEXT("OvrPill"))))
	{
		WorkPowerBorder->OnMouseButtonDownEvent.Unbind();
	}

	if (EnhanceButton)
	{
		EnhanceButton->OnClicked().RemoveAll(this);
	}
	if (FireButton)
	{
		FireButton->OnClicked().RemoveAll(this);
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UEmployeeWindowWidget::OnCloseDelegate);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeDisciplineChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeLevelUp.RemoveAll(this);
		EmployeeManager->OnEmployeeRosterChanged.RemoveAll(this);
		EmployeeManager->OnEmployeePortraitUpdated.RemoveAll(this);
	}

	// teardown 중엔 GameInstance 가 먼저 사라질 수 있음
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.RemoveAll(this);
		}
		if (UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>())
		{
			ItemMgr->OnItemChanged.RemoveAll(this);
		}
	}

	// Deactivated 를 안 거치고 파괴되는 경로 안전망 (idempotent)
	ReleaseHeroLiveView();

	Super::NativeDestruct();
}

// DT_PanelIntro.PanelKey. 유니티 빌드 셰도잉을 피하려 이름을 길게 둔다
static const FName PanelIntroKey_EmployeeWindow(TEXT("EmployeeWindow"));

void UEmployeeWindowWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (auto* PC = Cast<AMainMapPlayerController>(GetOwningPlayer())) PC->GoToUIMode();

	// 위에 쌓인 패널이 닫혀 재활성될 때도 최신 상태 반영 (채용 P3 대비)
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!bHasExplicitEmployeeContext) BuildingIndex = GameInstance ? GameInstance->GetCurrentManagedBuildingIndex() : INDEX_NONE;

	// 부서 표시명은 CompanyType(=BuildingIndex) 산업별 — 옵션 채움은 필터 적용(RefreshRoster) 전에
	SetupRosterFilterCombo();
	RefreshRoster();
	RefreshWallet();
	StartIntroStagger();
	ShowEmployeePage(CurrentEmployeePage);
	bPotentialReturningFromShop = false;
	ApplyPendingEquipmentItem();

	// M9 튜토리얼 — 직원창 열림 등록+전이 (튜토리얼 외 no-op)
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyEmployeeWindowOpened(this);
	}

	// 최초 진입 코치마크 — 등장 스태거(≈0.46s)가 끝난 뒤 딤을 덮는다
	if (UPanelIntroSubsystem* IntroMgr = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>())
	{
		if (IntroMgr->ShouldPlay(PanelIntroKey_EmployeeWindow) && GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				PanelIntroTimer, this, &UEmployeeWindowWidget::TryPlayPanelIntro, 0.5f, false);
		}
	}
}

void UEmployeeWindowWidget::TryPlayPanelIntro()
{
	// 타이머 대기 중 창이 닫혔을 수 있다
	if (!IsActivated())
	{
		return;
	}
	UPanelIntroSubsystem* IntroMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	if (!IntroMgr)
	{
		return;
	}
	PanelIntroOverlay = IntroMgr->PlayIntro(this, PanelIntroKey_EmployeeWindow, /*bIgnoreSeen=*/false);
	if (UPanelIntroOverlayWidget* Overlay = PanelIntroOverlay.Get())
	{
		Overlay->OnIntroFinished.AddUObject(this, &UEmployeeWindowWidget::HandlePanelIntroFinished);
	}
}

void UEmployeeWindowWidget::HandlePanelIntroFinished()
{
	PanelIntroOverlay.Reset();
}

void UEmployeeWindowWidget::StartIntroStagger()
{
	// 컬럼 4개 이름 조회 — WBP 개편으로 이름이 사라지면 해당 컬럼만 자연 제외
	static const FName PanelNames[] = { FName("RosterStrip"), FName("HeroColBox"), FName("SkillPlateSize"), FName("PotentialPanel") };
	IntroPanels.Reset();
	for (const FName& Name : PanelNames)
	{
		if (UWidget* Panel = GetWidgetFromName(Name))
		{
			Panel->SetRenderOpacity(0.f);
			Panel->SetRenderTranslation(FVector2D(0.f, IntroSlide));
			IntroPanels.Add(Panel);
		}
	}
	IntroElapsed = IntroPanels.Num() > 0 ? 0.f : -1.f;
}

void UEmployeeWindowWidget::UpdateColumnWidths(float RootWidth)
{
	const UWidget* WorkspacePanel = GrowthVBox ? GrowthVBox->GetParent() : nullptr;
	const USizeBox* WorkspaceSize = Cast<USizeBox>(EquipPlateSize);
	const UPanelWidget* ColumnRow = HeroColBox ? HeroColBox->GetParent() : nullptr;
	const UBorderSlot* ContentSlot = EmployeeContentSize ? Cast<UBorderSlot>(EmployeeContentSize->Slot) : nullptr;
	if (!HeroColBox || !WorkspacePanel || !WorkspaceSize || !ColumnRow || !ContentSlot) return;

	const float MinDetailWidth = HeroColBox->GetMinDesiredWidth();
	const float MaxDetailWidth = HeroColBox->GetMaxDesiredWidth();
	const float MinWorkspaceWidth = WorkspaceSize->GetMinDesiredWidth();
	if (RootWidth <= 0.f || MinDetailWidth <= 0.f
		|| MaxDetailWidth < MinDetailWidth || MinWorkspaceWidth <= 0.f) return;

	const FMargin ContentPadding = ContentSlot->GetPadding();
	float ColumnBudget = RootWidth - ContentPadding.Left - ContentPadding.Right;
	for (int32 ColumnIndex = 0; ColumnIndex < ColumnRow->GetChildrenCount(); ++ColumnIndex)
	{
		const UWidget* ColumnWidget = ColumnRow->GetChildAt(ColumnIndex);
		if (!ColumnWidget) continue;
		if (const UHorizontalBoxSlot* ColumnSlot = Cast<UHorizontalBoxSlot>(ColumnWidget->Slot))
		{
			const FMargin ColumnPadding = ColumnSlot->GetPadding();
			ColumnBudget -= ColumnPadding.Left + ColumnPadding.Right;
		}
		if (ColumnWidget != HeroColBox && ColumnWidget != WorkspacePanel)
		{
			ColumnBudget -= ColumnWidget->GetDesiredSize().X;
		}
	}
	// 두 페이지가 공유하는 열 예산에서 최소 가독 폭을 뺀 여유를 절반씩 나눈다.
	const float ExtraWidth = FMath::Max(0.f, ColumnBudget - MinDetailWidth - MinWorkspaceWidth);
	const float TargetDetailWidth = FMath::Clamp(MinDetailWidth + ExtraWidth * 0.5f, MinDetailWidth, MaxDetailWidth);
	if (!FMath::IsNearlyEqual(HeroColBox->GetWidthOverride(), TargetDetailWidth, 0.5f))
	{
		HeroColBox->SetWidthOverride(TargetDetailWidth);
	}
	const float DetailGrowth = MaxDetailWidth > MinDetailWidth
		? (TargetDetailWidth - MinDetailWidth) / (MaxDetailWidth - MinDetailWidth) : 0.f;
	UButtonWidget* Actions[] = { StatPlusButton0, StatPlusButton1, StatPlusButton2, StatPlusButton3, StatPlusButton4, StatPlusButton5 };
	for (const UButtonWidget* ActionWidget : Actions)
	{
		USizeBox* ActionSize = ActionWidget ? Cast<USizeBox>(ActionWidget->GetParent()) : nullptr;
		if (!ActionSize || ActionSize->GetMaxDesiredWidth() <= 0.f) continue;
		const float ActionWidth = FMath::Lerp(ActionSize->GetMinDesiredWidth(), ActionSize->GetMaxDesiredWidth(), DetailGrowth);
		if (!FMath::IsNearlyEqual(ActionSize->GetWidthOverride(), ActionWidth, 0.5f)) ActionSize->SetWidthOverride(ActionWidth);
	}
}

void UEmployeeWindowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateColumnWidths(MyGeometry.GetLocalSize().X);
	if (HeroRotationPointerIndex != INDEX_NONE && (IsCloseRequested() || !IsActivated()
		|| !IsValid(LiveStage) || !LiveStage->IsHeldBy(this)))
	{
		CancelHeroRotation();
	}
	if (PageTransitionElapsed >= 0.f)
	{
		PageTransitionElapsed += InDeltaTime;
		const float Progress = FMath::Clamp(PageTransitionElapsed / 0.15f, 0.f, 1.f);
		if (Progress >= 1.f || FMotionGate::IsReduced() || !PageTransitionTarget.IsValid())
		{
			FinishPageTransition();
		}
		else
		{
			PageTransitionTarget->SetRenderOpacity(FMath::Lerp(0.35f, 1.f, FWidgetAnimationUtils::EaseOutQuad(Progress)));
		}
	}

	UpdateFilterChipMaterialSize(MyGeometry);
	UpdateSurfaceMaterialSizes();
	UpdateHeroEquipmentFrameMaterialSize();

	// 띠 폭은 RefreshRoster 시점에 아직 0 이고 해상도 변경으로도 바뀐다 — 예산과 어긋난 틱에만 맞춘다.
	// 드로어가 열려 있어도 띠는 계속 보이므로 페이지로 가르지 않는다(홈 경로만 채우면 드로어가 반쯤 빈 판이 된다).
	if (RosterListView && EmployeeManager && RosterSyncNeeded(RosterListView, VisibleRosterTileBudget(RosterListView)))
	{
		const int32 OwnedCount = EmployeeManager->GetEmployeesInBuilding(BuildingIndex).Num()
			+ EmployeeManager->GetUnassignedEmployeesInBuilding(BuildingIndex).Num();
		const int32 Capacity = EmployeeManager->GetBuildingEmployeeCapacity(BuildingIndex, /*bLogIfZero*/ false);
		SyncRosterHireTiles(RosterListView, this, FEmployeeRosterRules::HireSlotCount(OwnedCount, Capacity));
	}

	// 등장 스태거
	if (IntroElapsed >= 0.f)
	{
		IntroElapsed += InDeltaTime;

		bool bAllDone = true;
		for (int32 i = 0; i < IntroPanels.Num(); ++i)
		{
			UWidget* Panel = IntroPanels[i].Get();
			if (!Panel)
			{
				continue;
			}
			const float T = FMath::Clamp((IntroElapsed - i * IntroStagger) / FMath::Max(IntroDuration, 0.01f), 0.f, 1.f);
			const float Ease = 1.f - FMath::Cube(1.f - T);
			Panel->SetRenderOpacity(Ease);
			Panel->SetRenderTranslation(FVector2D(0.f, (1.f - Ease) * IntroSlide));
			bAllDone &= (T >= 1.f);
		}

		if (bAllDone)
		{
			IntroElapsed = -1.f;
		}
	}

	// 등급 글로우 브리딩 — 색/기본알파는 UpdateHeroLiveView 가, 호흡은 RenderOpacity 가 소유(간섭 없음)
	if (HeroRarityGlow && HeroRarityGlow->GetVisibility() != ESlateVisibility::Hidden)
	{
		GlowBreatheTime += InDeltaTime;
		const float Breathe = 1.f - GlowBreatheAmp * 0.5f * (1.f - FMath::Cos(2.f * PI * GlowBreatheHz * GlowBreatheTime));
		HeroRarityGlow->SetRenderOpacity(Breathe);
	}

	// 라이브 뷰 워밍업 페이드인 (캡처 첫 프레임 노출 정착 가림)
	if (LiveViewWarmupElapsed >= 0.f && HeroPortraitImage)
	{
		LiveViewWarmupElapsed += InDeltaTime;
		const float T = FMath::Clamp((LiveViewWarmupElapsed - LiveViewWarmupHold) / FMath::Max(LiveViewWarmupFade, 0.01f), 0.f, 1.f);
		HeroPortraitImage->SetRenderOpacity(T);
		if (T >= 1.f)
		{
			LiveViewWarmupElapsed = -1.f;
		}
	}

	// ===== 장비 슬롯 펀치 + 등급색 스파클 (§9-4) =====
	{
		UButtonWidget* HeroSlots[] = {HeroEquipmentSlot0, HeroEquipmentSlot1, HeroEquipmentSlot2, HeroEquipmentSlot3, HeroEquipmentSlot4, HeroEquipmentSlot5};
		bool bAnyPunch = false;
		for (int32 i = 0; i < 6; ++i)
		{
			if (SlotPunch[i].IsPlaying())
			{
				bAnyPunch = true;
				if (HeroSlots[i]) { HeroSlots[i]->SetRenderScale(FVector2D(SlotPunch[i].Tick(InDeltaTime))); }
				else { SlotPunch[i].Tick(InDeltaTime); }
			}

			if (SlotSparkleElapsed[i] >= 0.f)
			{
				SlotSparkleElapsed[i] += InDeltaTime;
				UImage* Sparkle = SlotSparkleImages[i].Get();
				const float T = SlotSparkleElapsed[i] / SlotSparkleLife;
				if (!Sparkle || T >= 1.f)
				{
					if (Sparkle) { Sparkle->SetRenderOpacity(0.f); }
					SlotSparkleElapsed[i] = -1.f;
				}
				else
				{
					// MissionTracker 완료 팝과 같은 문법: 0→1.2→0 스케일 + 90도 회전 + sin 알파
					const float Scale = (T < 0.4f) ? FMath::Lerp(0.f, 1.2f, T / 0.4f) : FMath::Lerp(1.2f, 0.f, (T - 0.4f) / 0.6f);
					Sparkle->SetRenderScale(FVector2D(Scale));
					Sparkle->SetRenderTransformAngle(T * 90.f);
					Sparkle->SetRenderOpacity(1.f);
					const FLinearColor& Tint = SlotSparkleColor[i];
					Sparkle->SetColorAndOpacity(FLinearColor(Tint.R, Tint.G, Tint.B, FMath::Sin(PI * T)));
				}
			}
		}
		if (bAnyPunch)
		{
			bSlotPunchScaleDirty = true;
		}
		else if (bSlotPunchScaleDirty)
		{
			for (UButtonWidget* HeroSlot : HeroSlots)
			{
				if (HeroSlot) { HeroSlot->SetRenderScale(FVector2D::UnitVector); }
			}
			bSlotPunchScaleDirty = false;
		}
	}

	// ===== 업무력 카운트업 =====
	if (OvrCount.IsPlaying())
	{
		const int32 Shown = FMath::RoundToInt(OvrCount.Tick(InDeltaTime));
		if (OverallText) { OverallText->SetText(FormatOverallValue(Shown)); }
	}

	// ===== ★ 칩 펀치 (§9-5) =====
	if (StarChipPunch.IsPlaying())
	{
		if (StarBadge) { StarBadge->SetRenderScale(FVector2D(StarChipPunch.Tick(InDeltaTime))); }
		else { StarChipPunch.Tick(InDeltaTime); }
		bStarChipScaleDirty = true;
	}
	else if (bStarChipScaleDirty)
	{
		if (StarBadge) { StarBadge->SetRenderScale(FVector2D::UnitVector); }
		bStarChipScaleDirty = false;
	}

	// ===== 능력치 카운트업 + 행 하이라이트 (§9-6) =====
	{
		UCommonTextBlock* StatValues[NumEmployeeStats] = { HeroStatValue0, HeroStatValue1, HeroStatValue2, HeroStatValue3, HeroStatValue4, HeroStatValue5 };
		UCommonTextBlock* StatBonuses[NumEmployeeStats] = { HeroStatBonus0, HeroStatBonus1, HeroStatBonus2, HeroStatBonus3, HeroStatBonus4, HeroStatBonus5 };
		for (int32 i = 0; i < NumEmployeeStats; ++i)
		{
			if (StatCount[i].IsPlaying())
			{
				// 포맷은 RefreshHeroStats 와 같은 단일 출처(GetStatFeltText) — 소수·% 규칙을 복제하지 않는다
				const int32 Shown = FMath::RoundToInt(StatCount[i].Tick(InDeltaTime));
				if (StatValues[i])
				{
					StatValues[i]->SetText(UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(i), Shown));
				}
			}

			if (StatHighlightElapsed[i] >= 0.f)
			{
				StatHighlightElapsed[i] += InDeltaTime;
				UImage* RowBG = StatRowBGImages[i].Get();
				const float T = StatHighlightElapsed[i] / StatCountDuration;
				if (!RowBG || T >= 1.f)
				{
					if (RowBG) { RowBG->SetRenderOpacity(0.f); }
					StatHighlightElapsed[i] = -1.f;
				}
				else
				{
					RowBG->SetRenderOpacity(StatHighlightPeak * (1.f - T));
				}
			}

			if (StatBonusFadeElapsed[i] >= 0.f)
			{
				StatBonusFadeElapsed[i] += InDeltaTime;
				UCommonTextBlock* BonusText = StatBonuses[i];
				const float T = StatBonusFadeElapsed[i] / StatBonusFadeDuration;
				if (!BonusText || T >= 1.f)
				{
					if (BonusText) { BonusText->SetRenderOpacity(1.f); }
					StatBonusFadeElapsed[i] = -1.f;
				}
				else
				{
					BonusText->SetRenderOpacity(FWidgetAnimationUtils::EaseOutQuad(T));
				}
			}
		}
	}

	// ===== 골드 CTA 샤인 스윕 (§9-9, 글로우 브리딩과 함께 유이한 상시 틱) =====
	CtaSweepClock += InDeltaTime;
	if (CtaSweepClock >= CtaSweepPeriod)
	{
		CtaSweepClock -= CtaSweepPeriod;
	}
	{
		// 스윕은 "지금 누를 수 있다"는 신호 — 비활성 버튼에서 빛나면 거짓말이 된다.
		// 레벨업이 가능하면 스윕을 레벨업 버튼으로 옮겨 상시 스윕은 늘 하나만 돈다(§9-④ 예산).
		const float SweepT = CtaSweepClock / CtaSweepDuration;
		const bool bLevelUpReady = DisciplineCard && DisciplineCard->IsLevelUpReady();
		const bool bEnhanceReady = !bLevelUpReady && EnhanceButton && EnhanceButton->GetIsEnabled();
		DriveShineSweep(EnhanceShineImage.Get(), bEnhanceReady ? SweepT : -1.f, CtaSweepAlpha);
		DriveShineSweep(DisciplineCard ? DisciplineCard->GetLevelUpShineImage() : nullptr, bLevelUpReady ? SweepT : -1.f, CtaSweepAlpha);
	}

#if !UE_BUILD_SHIPPING
	// 수용 기준 §9-④ — 상시 연출(글로우·CTA 스윕·빈자리 펄스) 외에 남아 도는 연출이 없는지 1초마다 찍는다
	EffectBudgetLogClock += InDeltaTime;
	if (EffectBudgetLogClock >= 1.f)
	{
		EffectBudgetLogClock = 0.f;
		int32 TransientEffects = (IntroElapsed >= 0.f ? 1 : 0) + (LiveViewWarmupElapsed >= 0.f ? 1 : 0)
			+ (OvrCount.IsPlaying() ? 1 : 0)
			+ (StarChipPunch.IsPlaying() ? 1 : 0);
		for (int32 i = 0; i < NumEmployeeStats; ++i)
		{
			TransientEffects += (SlotPunch[i].IsPlaying() ? 1 : 0) + (SlotSparkleElapsed[i] >= 0.f ? 1 : 0)
				+ (StatCount[i].IsPlaying() ? 1 : 0) + (StatHighlightElapsed[i] >= 0.f ? 1 : 0)
				+ (StatBonusFadeElapsed[i] >= 0.f ? 1 : 0);
		}
		UE_LOG(LogTemp, Display, TEXT("[EmployeeWindow] 상시 연출 %d / 일시 연출 %d"),
			GetAlwaysOnEffectCount(), TransientEffects);
	}
#endif
}

void UEmployeeWindowWidget::NativeOnDeactivated()
{
	FinishPageTransition();
	// 상점 복귀 상태와 별개로 숨겨진 잠재 영역의 응답 연출은 취소한다.
	const bool bHasReturn = bPotentialReturningFromShop;
	if (PotentialWorkspace) PotentialWorkspace->Suspend();
	if (!bHasReturn)
	{
		bHasExplicitEmployeeContext = false;
		CurrentEmployeePage = EEmployeeWindowPage::Home;
	}
	// 인트로도 뷰포트 오버레이라 스택이 안 거둬간다. RemoveFromParent → NativeDestruct 가 복구 신호를 쏜다
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PanelIntroTimer);
	}
	if (UPanelIntroOverlayWidget* Intro = PanelIntroOverlay.Get())
	{
		Intro->RemoveFromParent();
	}
	// 오버레이가 이미 죽어 신호를 못 쏘는 경로 대비 ― 남은 핸들을 놓는다(미션 오버레이 복구는 PanelIntroSubsystem 소유)
	HandlePanelIntroFinished();

	// 직원창이 닫히면(ESC 포함) 위에 뜬 강화 모달도 함께 정리 — 뷰포트 오버레이라 스택이 안 거둬줌
	if (UEnhanceStarforceModalWidget* Modal = OpenEnhanceModal.Get())
	{
		Modal->RemoveFromParent();
	}
	OpenEnhanceModal.Reset();

	if (UPotentialOddsWidget* Odds = OpenOddsPanel.Get())
	{
		Odds->RemoveFromParent();
	}
	OpenOddsPanel.Reset();

	// 닫힘 시 스태거 중단 + 트랜스폼 원복 (재오픈은 StartIntroStagger 가 다시 세팅)
	IntroElapsed = -1.f;
	for (const TWeakObjectPtr<UWidget>& Weak : IntroPanels)
	{
		if (UWidget* Panel = Weak.Get())
		{
			Panel->SetRenderOpacity(1.f);
			Panel->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	// 캡처는 활성 동안만 — 닫힐 때 무대 반납(가챠가 인계 중이면 no-op)
	ReleaseHeroLiveView();

	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
		if (!bHasReturn && PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

	Super::NativeOnDeactivated();
}

FEmployeeInstance* UEmployeeWindowWidget::GetSelectedEmployee() const
{
	return (EmployeeManager && SelectedEmployeeID >= 0) ? EmployeeManager->FindEmployee(SelectedEmployeeID) : nullptr;
}

UWidget* UEmployeeWindowWidget::GetLevelUpCardWidget() const
{
	if (!DisciplineCard)
	{
		return nullptr;
	}
	if (!DisciplineCard->GetLevelUpButtonWidget())
	{
		return nullptr;
	}
	return DisciplineCard;
}

UWidget* UEmployeeWindowWidget::GetLevelUpButtonWidget() const
{
	return DisciplineCard ? DisciplineCard->GetLevelUpButtonWidget() : nullptr;
}

UWidget* UEmployeeWindowWidget::GetEnhanceButtonWidget() const
{
	return EnhanceButton;
}

UWidget* UEmployeeWindowWidget::GetStatPointTargetWidget() const
{
	if (!EmployeeManager || EmployeeManager->GetAvailableStatPoints(SelectedEmployeeID) <= 0)
	{
		return nullptr;
	}
	// 특정 능력치를 대신 선택하지 않고, 실제 버튼들의 공통 부모를 안내한다.
	const UButtonWidget* StatButtons[] = { StatPlusButton0, StatPlusButton1, StatPlusButton2,
		StatPlusButton3, StatPlusButton4, StatPlusButton5 };
	UWidget* CommonAncestor = nullptr;
	for (const UButtonWidget* StatButton : StatButtons)
	{
		if (!StatButton || !StatButton->GetIsEnabled()) { continue; }
		if (!CommonAncestor) { CommonAncestor = StatButton->GetParent(); continue; }
		while (CommonAncestor)
		{
			const UWidget* CurrentAncestor = StatButton;
			while (CurrentAncestor && CurrentAncestor != CommonAncestor)
			{
				CurrentAncestor = CurrentAncestor->GetParent();
			}
			if (CurrentAncestor) { break; }
			CommonAncestor = CommonAncestor->GetParent();
		}
	}
	return CommonAncestor;
}

void UEmployeeWindowWidget::RefreshRoster()
{
	if (!RosterListView || !EmployeeManager)
	{
		return;
	}

	// 착석 + 벤치 전부 (같은 건물 소속)
	TArray<FEmployeeInstance> Roster = EmployeeManager->GetEmployeesInBuilding(BuildingIndex);
	Roster.Append(EmployeeManager->GetUnassignedEmployeesInBuilding(BuildingIndex));

	// 정원 헤더/빈자리 수는 필터와 무관한 실제 보유 수 기준
	const int32 OwnedCount = Roster.Num();

	// 필터 → 정렬 순서 고정 (부서 필터 먼저, 그 다음 정렬 모드)
	if (RosterFilterDiscipline != EProductionDiscipline::Count)
	{
		Roster.RemoveAll([this](const FEmployeeInstance& E)
		{
			return GetPrimaryDisciplineSlot(E) != static_cast<int32>(RosterFilterDiscipline);
		});
	}

	Roster.Sort([this](const FEmployeeInstance& A, const FEmployeeInstance& B)
	{
		switch (RosterSortMode)
		{
		case ERosterSortMode::Rarity: return A.SpawnRarity > B.SpawnRarity;
		case ERosterSortMode::Name:   return A.EmployeeName < B.EmployeeName;
		default:                      return A.Level > B.Level; // 레벨순 기본
		}
	});

	// 선택 유지 — 사라졌으면(해고 등) 종합 1위로
	const bool bSelectedAlive = Roster.ContainsByPredicate(
		[this](const FEmployeeInstance& E) { return E.EmployeeID == SelectedEmployeeID; });
	if (!bSelectedAlive && CurrentEmployeePage == EEmployeeWindowPage::Home)
	{
		SelectedEmployeeID = Roster.Num() > 0 ? Roster[0].EmployeeID : -1;
	}
	else if (!EmployeeManager->GetEmployeeData(SelectedEmployeeID)) SelectedEmployeeID = INDEX_NONE;

	RosterListView->ClearListItems();
	UObject* SelectedItem = nullptr;
	for (const FEmployeeInstance& EmployeeData : Roster)
	{
		UEntityCardData* CardData = NewObject<UEntityCardData>(this);
		CardData->EmployeeInfo = EmployeeData;
		RosterListView->AddItem(CardData);
		if (EmployeeData.EmployeeID == SelectedEmployeeID)
		{
			SelectedItem = CardData;
		}
	}
	if (SelectedItem)
	{
		RosterListView->SetSelectedItem(SelectedItem);
	}

	// 정원 − 보유 = 빈자리 타일. 로스터가 이 건물 기준이라 정원도 같은 건물 기준이어야 한다
	const int32 Capacity = EmployeeManager->GetBuildingEmployeeCapacity(BuildingIndex, /*bLogIfZero*/ false);
	// 빈자리 타일은 상한 3 이 아니라 띠의 가시 폭까지 채운다(구 상한은 띠 절반을 죽은 여백으로 남겼다).
	// 첫 갱신에는 띠 폭이 아직 0 이라 예산을 모른다 — NativeTick 의 SyncRosterHireTiles 가 실폭으로 맞춘다.
	const int32 RemainingSeats = FEmployeeRosterRules::HireSlotCount(OwnedCount, Capacity);
	const int32 TileBudget = VisibleRosterTileBudget(RosterListView);
	// 폭이 0(첫 배치 전)이면 예산을 모른다 — 추측으로 3장을 깔면 그 3장이 틱의 목표 개수와 싸운다.
	// 계산을 미루고 실폭이 잡힌 첫 틱의 SyncRosterHireTiles 가 한 번에 정확히 맞춘다.
	const int32 HireSlots = TileBudget > 0 ? FMath::Clamp(TileBudget - Roster.Num(), 0, RemainingSeats) : 0;
	if (TileBudget > 0)
	{
		GEmployeeRosterSyncWidth = RosterListView->GetCachedGeometry().GetLocalSize().X;
		GEmployeeRosterSyncBudget = TileBudget;
		GEmployeeRosterSyncHireWanted = HireSlots;
	}
	for (int32 HireIndex = 0; HireIndex < HireSlots; ++HireIndex)
	{
		UEntityCardData* HireData = NewObject<UEntityCardData>(this);
		HireData->bIsHireSlot = true;
		RosterListView->AddItem(HireData);
	}

	if (RosterHeaderText)
	{
		RosterHeaderText->SetText(FText::Format(NSLOCTEXT("EmployeeWindow", "RosterHeader", "직원 {0} / {1}"), OwnedCount, Capacity));
	}

	RefreshSelected();
}

void UEmployeeWindowWidget::SetupRosterFilterCombo()
{
	if (!RosterFilterCombo)
	{
		return;
	}

	// 배정 빌딩 CompanyType — 부서 표시명이 산업별이라 필요
	const ECompanyType CompanyType = EmployeeManager
		? EmployeeManager->GetCompanyTypeForBuilding(BuildingIndex) : ECompanyType::None;
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// 재오픈 시 다른 회사일 수 있어 매번 재구성 — GetDepartmentDisplayName 은 매핑 없어도 DepartmentToString 폴백이라 항상 비지 않음
	RosterFilterCombo->ClearOptions();
	RosterFilterCombo->AddOption(TEXT("전체"));
	for (int32 i = 0; i < GetDisciplineCountForCompany(CompanyType); ++i)
	{
		const FText DisplayName = TableMgr ? TableMgr->GetDisciplineDisplayName(CompanyType, i) : FText::GetEmpty();
		RosterFilterCombo->AddOption(DisplayName.ToString());
	}
	if (static_cast<int32>(RosterFilterDiscipline) >= GetDisciplineCountForCompany(CompanyType))
	{
		RosterFilterDiscipline = EProductionDiscipline::Count;
	}

	const int32 SelectIndex = (RosterFilterDiscipline == EProductionDiscipline::Count)
		? 0 : static_cast<int32>(RosterFilterDiscipline) + 1;
	RosterFilterCombo->SetSelectedIndex(SelectIndex);
}

void UEmployeeWindowWidget::OnRosterSortCycleClicked()
{
	switch (RosterSortMode)
	{
	case ERosterSortMode::Level:  RosterSortMode = ERosterSortMode::Rarity; break;
	case ERosterSortMode::Rarity: RosterSortMode = ERosterSortMode::Name;   break;
	default:                      RosterSortMode = ERosterSortMode::Level; break;
	}

	UpdateRosterSortLabel();
	RefreshRoster();
}

void UEmployeeWindowWidget::OnRosterFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 Index = RosterFilterCombo ? RosterFilterCombo->GetSelectedIndex() : 0;
	RosterFilterDiscipline = (Index <= 0) ? EProductionDiscipline::Count : static_cast<EProductionDiscipline>(Index - 1);
	RefreshRoster();
}

void UEmployeeWindowWidget::UpdateRosterSortLabel()
{
	if (!RosterSortCycleBtn)
	{
		return;
	}

	FString Label;
	switch (RosterSortMode)
	{
	case ERosterSortMode::Rarity: Label = TEXT("등급순"); break;
	case ERosterSortMode::Name:   Label = TEXT("이름순"); break;
	default:                      Label = TEXT("레벨순"); break;
	}
	// ▾(U+25BE)는 NEXON cmap 에 없어 빈 사각형이 된다(ttf 실측) — 수록된 하향 삼각은 ▼/▽ 뿐이다
	RosterSortCycleBtn->SetButtonText(FText::FromString(Label + TEXT(" ▼")));
}

void UEmployeeWindowWidget::RefreshSelected()
{
	RefreshHero();
	RefreshHeroEquipment();
	RefreshHeroStats();
	if (PotentialWorkspace && IsActivated() && CurrentEmployeePage == EEmployeeWindowPage::Home)
	{
		PotentialWorkspace->Configure(SelectedEmployeeID);
	}
	if (DisciplineCard) { DisciplineCard->Configure(SelectedEmployeeID); }
}

void UEmployeeWindowWidget::RefreshHero()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();

	SyncEffectBaselineOwner();
	UpdateHeroLiveView(Employee);

	if (HeroNameText)
	{
		HeroNameText->SetText(Employee ? FText::FromString(Employee->EmployeeName) : FText::FromString(TEXT("직원 없음")));
	}

	const int32 Enh = Employee ? Employee->EnhancementLevel : 0;
	if (HeroEnhancementText)
	{
		HeroEnhancementText->SetText(Employee ? FText::AsNumber(Enh) : FText::GetEmpty());
	}
	if (HeroEnhancementIcon)
	{
		static const TSoftObjectPtr<UTexture2D> FilledTexPtr(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Rating/T_UIIcon_Star_SfFilled.T_UIIcon_Star_SfFilled")));
		static const TSoftObjectPtr<UTexture2D> EmptyTexPtr(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Rating/T_UIIcon_Star_SfEmpty.T_UIIcon_Star_SfEmpty")));
		HeroEnhancementIcon->SetBrushFromTexture(Employee ? (Enh > 0 ? FilledTexPtr : EmptyTexPtr).LoadSynchronous() : nullptr);
	}
	if (StarBadge)
	{
		StarBadge->SetVisibility(Employee ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

		// 강화가 증가할 때만 별 아이콘과 수치 배지를 함께 강조한다.
		if (!Employee)
		{
			LastStarCount = INDEX_NONE;
		}
		else if (CurrentEmployeePage == EEmployeeWindowPage::Home)
		{
			if (LastStarCount != INDEX_NONE && Enh > LastStarCount)
			{
				StarChipPunch.Start(1.15f, 0.25f);
			}
			LastStarCount = Enh;
		}
		else if (LastStarCount == INDEX_NONE)
		{
			LastStarCount = Enh;   // 드로어 안에서는 기준선을 밀지 않는다(복귀 refresh 가 차이를 본다)
		}
	}

	if (HeroSubText)
	{
		if (Employee)
		{
			HeroSubText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Employee->SpawnRarity)));
			// 라이트 플레이트 위 가독 — 등급색 원색(일반=실버)은 플레이트와 동화, ×0.55 다크 파생(레이더 스트로크 문법)
			FLinearColor SubColor = FLootBoxRarityUtility::GetRarityColor(Employee->SpawnRarity) * 0.55f;
			// ×0.55 는 채도색(희귀~신화)엔 듣지만 일반(무채색 회색)은 #A1A1A1 = 카드 대비 2.36:1 로 실패한다.
			// 캡션 잉크 하한 #5A6B7D 의 상대휘도까지 눌러 전 등급이 5:1 이상을 확보(등급 코딩은 색상比 유지).
			const float CaptionFloorLum = 0.1417f;
			const float SubLum = 0.2126f * SubColor.R + 0.7152f * SubColor.G + 0.0722f * SubColor.B;
			if (SubLum > CaptionFloorLum)
			{
				SubColor *= CaptionFloorLum / SubLum;
			}
			SubColor.A = 1.f;
			HeroSubText->SetColorAndOpacity(FSlateColor(SubColor));
		}
		else
		{
			HeroSubText->SetText(FText::GetEmpty());
		}
	}

	// FC 재질 — 스테이지 등급 틴트: 면 16% / 프레임 42% 다크 / 젬 원색 (목업 승인 2026-07-25, sRGB 혼합→linear)
	{
		const FLinearColor RaritySrgb = FLootBoxRarityUtility::GetRarityColor(
			Employee ? Employee->SpawnRarity : ELootBoxRarity::Common);
		auto S2L = [](float C) { return FMath::Pow((C + 0.055f) / 1.055f, 2.4f); };
		auto MixL = [&](const FLinearColor& BaseSrgb, float Ratio, float Alpha)
		{
			const FLinearColor M = BaseSrgb * (1.f - Ratio) + RaritySrgb * Ratio;
			return FLinearColor(S2L(M.R), S2L(M.G), S2L(M.B), Alpha);
		};
		if (UImage* Face = Cast<UImage>(GetWidgetFromName(TEXT("PortraitPlateBG"))))
		{
			FSlateBrush FaceBrush = Face->GetBrush();
			FaceBrush.TintColor = FSlateColor(MixL(FLinearColor(0.90f, 0.92f, 0.94f), 0.16f, 1.f));
			Face->SetBrush(FaceBrush);
		}
		if (UImage* StageFrame = Cast<UImage>(GetWidgetFromName(TEXT("StageFrame"))))
		{
			FSlateBrush FrameBrush = StageFrame->GetBrush();
			FrameBrush.OutlineSettings.Color = FSlateColor(MixL(FLinearColor(0.50f, 0.55f, 0.62f), 0.42f, 0.5f));
			StageFrame->SetBrush(FrameBrush);
		}
		if (UImage* Gem = Cast<UImage>(GetWidgetFromName(TEXT("HeroRarityGem"))))
		{
			Gem->SetColorAndOpacity(FLinearColor(S2L(RaritySrgb.R), S2L(RaritySrgb.G), S2L(RaritySrgb.B), 1.f));
		}
	}

	if (HeroPortraitRim)
	{
		HeroPortraitRim->SetColorAndOpacity(Employee
			? EmployeeRarityDisplay::Color(Employee->SpawnRarity)
			: FLinearColor::Transparent);
	}

	if (HeroRarityChipText)
	{
		HeroRarityChipText->SetText(Employee
			? FText::FromString(FLootBoxRarityUtility::GetKoreanName(Employee->SpawnRarity))
			: FText::GetEmpty());
		const FLinearColor ChipColor = GetEmployeeHomeRarityTextColor(
			Employee ? Employee->SpawnRarity : ELootBoxRarity::Common);
		HeroRarityChipText->SetColorAndOpacity(FSlateColor(ChipColor));
	}

	if (HeroRankText)
	{
		const FString TitleName = Employee
			? UEmployeeTypeHelper::GetJobTitleDisplayName(UEmployeeTypeHelper::GetJobTitleFromLevel(Employee->Level))
			: FString();
		HeroRankText->SetText(FText::FromString(TitleName));
	}

	// 직능 칩 = DisciplinePoints 최고값의 enum DisplayName + 글리프 (로스터 카드와 동일 argmax — FIFA 포지션 문법, 2026-07-25)
	if (DeptChipText)
	{
		FText DiscName;
		int32 BestIdx = -1;
		if (Employee)
		{
			int32 BestVal = 0;
			for (int32 i = 0; i < Employee->DisciplinePoints.Num(); ++i)
			{
				if (Employee->DisciplinePoints[i] > BestVal) { BestVal = Employee->DisciplinePoints[i]; BestIdx = i; }
			}
			if (BestIdx >= 0)
			{
				// 표시명 SOT = DT_DisciplineDisplay (슬롯 의미는 enum 고정, 표시만 산업별 재해석)
				UGameInstance* GI = GetGameInstance();
				UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
				const ECompanyType Industry = EmployeeManager
					? EmployeeManager->GetCompanyTypeForBuilding(Employee->AssignedBuildingIndex) : ECompanyType::None;
				if (TableMgr)
				{
					DiscName = TableMgr->GetDisciplineDisplayName(Industry, BestIdx);
				}
			}
		}
		// 칩을 접으면 명패 높이가 출렁인다 — 값이 없으면 자리표시 문구로 채운다
		DeptChipText->SetText(DiscName.IsEmpty() ? FText::FromString(TEXT("미배치")) : DiscName);

		if (UImage* ChipIcon = Cast<UImage>(GetWidgetFromName(TEXT("DeptChipIcon"))))
		{
			UTexture2D* DiscTex = DisciplineCard
				? DisciplineCard->GetDisciplineIconTexture(GetDisciplineIconSlot(BestIdx, Employee->DisciplinePoints.Num()))
				: nullptr;
			ChipIcon->SetBrushFromTexture(DiscTex);
			ChipIcon->SetVisibility(DiscTex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	if (OverallText)
	{
		// 업무력은 직원의 최고 직능 포인트이며 로스터 카드와 같은 값을 표시한다.
		int32 BestDisc = 0;
		if (Employee)
		{
			for (const int32 P : Employee->DisciplinePoints) { BestDisc = FMath::Max(BestDisc, P); }
		}
		// 업무력 라벨은 WBP가 담당하며 여기는 숫자만 갱신한다.
		if (!Employee)
		{
			OverallText->SetText(FText::GetEmpty());
			LastOverallValue = INDEX_NONE;
		}
		else if (CurrentEmployeePage == EEmployeeWindowPage::Home
			&& LastOverallValue != INDEX_NONE && LastOverallValue != BestDisc)
		{
			// 최고 직능 값이 바뀌면 틱에서 숫자를 부드럽게 갱신한다.
			OvrCount.Start(static_cast<float>(LastOverallValue), static_cast<float>(BestDisc), OvrCountDuration);
			LastOverallValue = BestDisc;
		}
		else
		{
			// 카운트업 중이면 확정값을 쓰지 않는다 — 썼다가 다음 틱에 중간값으로 되돌아가 숫자가 튄다
			if (!OvrCount.IsPlaying())
			{
				OverallText->SetText(FormatOverallValue(BestDisc));
			}
			// 드로어 안에서는 기준선을 밀지 않는다 — 복귀 refresh 가 차이를 본다
			if (CurrentEmployeePage == EEmployeeWindowPage::Home || LastOverallValue == INDEX_NONE)
			{
				LastOverallValue = BestDisc;
			}
		}
	}

	if (EnhanceButton)
	{
		EnhanceButton->SetIsEnabled(Employee != nullptr);
	}
	if (FireButton)
	{
		FireButton->SetIsEnabled(Employee != nullptr);
	}

	RefreshExpOnly();

}

void UEmployeeWindowWidget::UpdateHeroLiveView(FEmployeeInstance* Employee)
{
	// 미니 스테이지 장식(글로우/접지 그림자)은 캐릭터와 운명 공동체
	const ESlateVisibility StageVis = Employee ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
	if (HeroRarityGlow)
	{
		// FC 재질 v9x: 무대광=전 등급 (스테이지가 등급색 재질면이라 일반 실버도 흰 끼 없음 — 일반만 저알파)
		const bool bShowGlow = Employee != nullptr;
		HeroRarityGlow->SetVisibility(bShowGlow ? StageVis : ESlateVisibility::Hidden);
		if (bShowGlow)
		{
			FLinearColor GlowColor = EmployeeRarityDisplay::Color(Employee->SpawnRarity);
			GlowColor.A = (Employee->SpawnRarity == ELootBoxRarity::Common) ? 0.18f : 0.28f;
			HeroRarityGlow->SetColorAndOpacity(GlowColor);
		}
	}
	if (HeroGroundShadow)
	{
		HeroGroundShadow->SetVisibility(StageVis);
	}
	if (!HeroPortraitImage)
	{
		return;
	}

	if (!Employee)
	{
		ReleaseHeroLiveView();
		HeroPortraitImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// 라이브 뷰 비활성(노브) 시 정적 초상화 폴백 — 흰색 렌더 재발 시 이 노브로 즉시 우회 (현재 기본 = 라이브뷰 on)
	if (!bLiveHeroViewEnabled)
	{
		ReleaseHeroLiveView();
		LiveViewWarmupElapsed = -1.f;
		HeroPortraitImage->SetRenderOpacity(1.f);
		UTexture2D* Portrait = UEmployeeRosterCardWidget::LoadPortraitTexture(Employee->EmployeeID, HeroPortraitSize);
		if (Portrait)
		{
			HeroPortraitImage->SetBrushFromTexture(Portrait);
			HeroPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			HeroPortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	// 비활성 중(닫히는 중/전환) 무대 재획득 금지 — 재활성 시 NativeOnActivated 경로가 다시 채움
	if (!IsActivated())
	{
		return;
	}

	const auto* EquipmentTables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	const FString AppearanceKey = EquipmentManager && EquipmentTables ? FEmployeeEquipmentAppearanceResolver::BuildKey(
		EquipmentManager->GetState(), EquipmentTables->GetEmployeeEquipmentDefinitions(), EquipmentTables->GetEmployeeEquipmentAppearances(), Employee->EmployeeID) : FString();
	// 외형 키가 같으면 재화/능력치 갱신에 무대를 다시 시작하지 않는다.
	if (IsValid(LiveStage) && LiveStage->IsHeldBy(this)
		&& Employee->EmployeeID == LiveViewEmployeeID
		&& Employee->EnhancementLevel == LiveViewEnhancementLevel
		&& AppearanceKey == LiveViewAppearanceKey)
	{
		return;
	}

	CancelHeroRotation();
	UTextureRenderTarget2D* Rt = nullptr;
	if (UClass* StageCls = LiveStageClass.LoadSynchronous())
	{
		LiveStage = AGachaCaptureStage::AcquireShared(this, GetWorld(), StageCls);
		if (LiveStage)
		{
			// Idle(숨쉬기) — 제자리 걷기는 정면 카메라에서 러닝머신처럼 어색 (2026-07-20 PIE 피드백)
			Rt = LiveStage->BeginReveal(*Employee, EStageAnimMode::Idle);
		}
	}

	if (!Rt)
	{
		// 무대 불가(클래스 로드/스폰 실패) — 증명사진 PNG 폴백
		LiveViewEmployeeID = -1;
		LiveViewEnhancementLevel = -1;
		LiveViewWarmupElapsed = -1.f;
		HeroPortraitImage->SetRenderOpacity(1.f);
		UTexture2D* Portrait = UEmployeeRosterCardWidget::LoadPortraitTexture(Employee->EmployeeID, HeroPortraitSize);
		if (Portrait)
		{
			HeroPortraitImage->SetBrushFromTexture(Portrait);
			HeroPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			HeroPortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	LiveViewEmployeeID = Employee->EmployeeID;
	LiveViewEnhancementLevel = Employee->EnhancementLevel;
	LiveViewAppearanceKey = AppearanceKey;

	if (!LiveViewMID)
	{
		// RT 알파 반전(캐릭터=투명/배경=불투명) 보정 — 가챠 리빌과 동일 컷아웃 기법(OneMinus a)
		TSoftObjectPtr<UMaterialInterface> MatPath(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT.M_GachaRevealRT")));
		if (UMaterialInterface* Mat = MatPath.LoadSynchronous())
		{
			LiveViewMID = UMaterialInstanceDynamic::Create(Mat, this);
		}
	}

	if (LiveViewMID)
	{
		LiveViewMID->SetTextureParameterValue(FName("RT"), Rt);
		HeroPortraitImage->SetBrushFromMaterial(LiveViewMID);
	}
	else
	{
		FSlateBrush Brush = HeroPortraitImage->GetBrush();
		Brush.SetResourceObject(Rt); // 폴백(머티리얼 미생성): 알파 반전 상태 직표시
		HeroPortraitImage->SetBrush(Brush);
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] LiveView MID 생성 실패 — M_GachaRevealRT 로드 확인 (RT 원본 직표시)"));
	}

	// 캡처 첫 프레임(노출/TAA 정착) 가림 — 홀드 후 페이드인 (구 "흰색 렌더" 과도기 대응)
	HeroPortraitImage->SetRenderOpacity(0.f);
	LiveViewWarmupElapsed = 0.f;
	HeroPortraitImage->SetVisibility(ESlateVisibility::Visible);
}

void UEmployeeWindowWidget::ReleaseHeroLiveView()
{
	CancelHeroRotation();
	LiveViewWarmupElapsed = -1.f;
	if (IsValid(LiveStage))
	{
		LiveStage->ReleaseShared(this); // 가챠가 인계했으면 no-op — 그쪽이 반납
	}
	LiveStage = nullptr;
	LiveViewEmployeeID = -1;
	LiveViewEnhancementLevel = -1;
	LiveViewAppearanceKey.Reset();
}

FReply UEmployeeWindowWidget::BeginHeroRotation(const FPointerEvent& PointerEvent)
{
	if (HeroRotationPointerIndex != INDEX_NONE || !IsActivated() || IsCloseRequested()
		|| !HeroPortraitImage || HeroPortraitImage->GetVisibility() != ESlateVisibility::Visible
		|| LiveViewEmployeeID != SelectedEmployeeID || !IsValid(LiveStage) || !LiveStage->IsHeldBy(this))
	{
		return FReply::Unhandled();
	}
	const FGeometry& PortraitGeometry = HeroPortraitImage->GetCachedGeometry();
	if (PortraitGeometry.GetLocalSize().X <= 0.f || !PortraitGeometry.IsUnderLocation(PointerEvent.GetScreenSpacePosition()))
	{
		return FReply::Unhandled();
	}
	const TSharedPtr<SWidget> CaptureWidget = GetCachedWidget();
	if (!CaptureWidget.IsValid()) { return FReply::Unhandled(); }
	HeroRotationPointerIndex = PointerEvent.GetPointerIndex();
	HeroRotationUserIndex = PointerEvent.GetUserIndex();
	bHeroRotationTouch = PointerEvent.IsTouchEvent();
	HeroRotationLastPosition = PointerEvent.GetScreenSpacePosition();
	return FReply::Handled().CaptureMouse(CaptureWidget.ToSharedRef());
}

bool UEmployeeWindowWidget::IsHeroRotationPointer(const FPointerEvent& PointerEvent) const
{
	return HeroRotationPointerIndex != INDEX_NONE && HeroRotationPointerIndex == PointerEvent.GetPointerIndex()
		&& HeroRotationUserIndex == PointerEvent.GetUserIndex() && bHeroRotationTouch == PointerEvent.IsTouchEvent();
}

FReply UEmployeeWindowWidget::MoveHeroRotation(const FPointerEvent& PointerEvent)
{
	if (!IsHeroRotationPointer(PointerEvent)) { return FReply::Unhandled(); }
	if (!IsActivated() || IsCloseRequested() || !HeroPortraitImage || !IsValid(LiveStage)
		|| LiveViewEmployeeID != SelectedEmployeeID || !LiveStage->IsHeldBy(this))
	{
		CancelHeroRotation();
		return FReply::Handled().ReleaseMouseCapture();
	}
	const FGeometry& PortraitGeometry = HeroPortraitImage->GetCachedGeometry();
	const FVector2D CurrentPosition = PointerEvent.GetScreenSpacePosition();
	const float DeltaX = PortraitGeometry.AbsoluteToLocal(CurrentPosition).X
		- PortraitGeometry.AbsoluteToLocal(HeroRotationLastPosition).X;
	HeroRotationLastPosition = CurrentPosition;
	// 한 이미지 폭을 드래그하면 반 바퀴. DPI·화면 비율이 달라도 같은 비율로 회전한다.
	const float DeltaYaw = -180.f * DeltaX / FMath::Max(1.f, static_cast<float>(PortraitGeometry.GetLocalSize().X));
	LiveStage->RotatePreview(this, DeltaYaw);
	return FReply::Handled();
}

FReply UEmployeeWindowWidget::EndHeroRotation(const FPointerEvent& PointerEvent)
{
	if (!IsHeroRotationPointer(PointerEvent)) { return FReply::Unhandled(); }
	HeroRotationPointerIndex = HeroRotationUserIndex = INDEX_NONE;
	return FReply::Handled().ReleaseMouseCapture();
}

void UEmployeeWindowWidget::CancelHeroRotation()
{
	const int32 PointerIndex = HeroRotationPointerIndex;
	const int32 UserIndex = HeroRotationUserIndex;
	HeroRotationPointerIndex = HeroRotationUserIndex = INDEX_NONE;
	if (PointerIndex == INDEX_NONE || !FSlateApplication::IsInitialized()) { return; }
	const TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(UserIndex);
	if (SlateUser && SlateUser->DoesWidgetHaveCapture(GetCachedWidget(), PointerIndex))
	{
		SlateUser->ReleaseCapture(PointerIndex);
	}
}

FReply UEmployeeWindowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FReply Reply = BeginHeroRotation(InMouseEvent);
		if (Reply.IsEventHandled()) { return Reply; }
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UEmployeeWindowWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply Reply = MoveHeroRotation(InMouseEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UEmployeeWindowWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FReply Reply = EndHeroRotation(InMouseEvent);
		if (Reply.IsEventHandled()) { return Reply; }
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UEmployeeWindowWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const FReply Reply = BeginHeroRotation(InGestureEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

FReply UEmployeeWindowWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const FReply Reply = MoveHeroRotation(InGestureEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UEmployeeWindowWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const FReply Reply = EndHeroRotation(InGestureEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

void UEmployeeWindowWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	if (HeroRotationUserIndex == CaptureLostEvent.UserIndex && HeroRotationPointerIndex == CaptureLostEvent.PointerIndex)
	{
		HeroRotationPointerIndex = HeroRotationUserIndex = INDEX_NONE;
	}
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UEmployeeWindowWidget::RefreshExpOnly()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();

	if (LevelText)
	{
		LevelText->SetText(Employee
			? FText::FromString(FString::Printf(TEXT("Lv.%d"), Employee->Level))
			: FText::FromString(TEXT("-")));
	}

	if (HeroLevelMaxText) HeroLevelMaxText->SetText(FText::GetEmpty());
}

void UEmployeeWindowWidget::RefreshHeroStats()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	const int32 EnhB = Employee ? UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel) : 0;
	const int32 AvailableSP = Employee && EmployeeManager
		? EmployeeManager->GetAvailableStatPoints(Employee->EmployeeID) : 0;
	if (AvailableStatPointsText)
	{
		AvailableStatPointsText->SetText(FText::AsNumber(AvailableSP));
	}

	SyncEffectBaselineOwner();
	const bool bHomeVisible = CurrentEmployeePage == EEmployeeWindowPage::Home;

	UCommonTextBlock* Names[NumEmployeeStats]   = { HeroStatName0, HeroStatName1, HeroStatName2, HeroStatName3, HeroStatName4, HeroStatName5 };
	UCommonTextBlock* Values[NumEmployeeStats]  = { HeroStatValue0, HeroStatValue1, HeroStatValue2, HeroStatValue3, HeroStatValue4, HeroStatValue5 };
	UCommonTextBlock* Bonuses[NumEmployeeStats] = { HeroStatBonus0, HeroStatBonus1, HeroStatBonus2, HeroStatBonus3, HeroStatBonus4, HeroStatBonus5 };
	UCommonTextBlock* Descs[NumEmployeeStats]   = { HeroStatDesc0, HeroStatDesc1, HeroStatDesc2, HeroStatDesc3, HeroStatDesc4, HeroStatDesc5 };
	UCommonTextBlock* PointTexts[NumEmployeeStats] = { HeroStatPointText0, HeroStatPointText1, HeroStatPointText2, HeroStatPointText3, HeroStatPointText4, HeroStatPointText5 };
	UCommonTextBlock* NextEffects[NumEmployeeStats] = { HeroStatNextEffectText0, HeroStatNextEffectText1, HeroStatNextEffectText2, HeroStatNextEffectText3, HeroStatNextEffectText4, HeroStatNextEffectText5 };
	UButtonWidget* StatPlusButtons[NumEmployeeStats] = { StatPlusButton0, StatPlusButton1, StatPlusButton2, StatPlusButton3, StatPlusButton4, StatPlusButton5 };

	for (int32 i = 0; i < NumEmployeeStats; ++i)
	{
		if (Names[i])
		{
			Names[i]->SetText(UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(i)));
		}

		// 토글은 칩(부모 Border)째 — 텍스트만 숨기면 빈 알약이 남는다 (BadgePlateOrSelf)
		UWidget* BonusToggle = (Bonuses[i] && Bonuses[i]->GetParent() && Bonuses[i]->GetParent()->IsA<UBorder>())
			? static_cast<UWidget*>(Bonuses[i]->GetParent()) : Bonuses[i];

		if (!Employee)
		{
			if (Values[i])   { Values[i]->SetText(FText::GetEmpty()); }
			if (Descs[i])    { Descs[i]->SetText(FText::GetEmpty()); }
			if (PointTexts[i]) { PointTexts[i]->SetText(FText::GetEmpty()); }
			if (NextEffects[i]) { NextEffects[i]->SetText(FText::GetEmpty()); }
			if (StatPlusButtons[i]) { StatPlusButtons[i]->SetIsEnabled(false); }
			if (BonusToggle) { BonusToggle->SetVisibility(ESlateVisibility::Collapsed); }
			LastStatValues[i] = INDEX_NONE;
			continue;
		}

		const int32 BaseValue = UEmployeeStatsHelper::GetStatValueByIndex(Employee->Stats, static_cast<uint8>(i));
		const int32 EffectiveValue = static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(BaseValue) + EnhB, MIN_int32, MAX_int32));
		const int32 NextEffectiveValue = EffectiveValue < MAX_int32 ? EffectiveValue + 1 : EffectiveValue;
		if (PointTexts[i])
		{
			PointTexts[i]->SetText(BaseValue < MAX_int32 ? FText::FromString(TEXT(">")) : FText::GetEmpty());
		}
		if (NextEffects[i])
		{
			NextEffects[i]->SetText(BaseValue < MAX_int32
				? UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(i), NextEffectiveValue)
				: FText::GetEmpty());
		}
		if (StatPlusButtons[i])
		{
			StatPlusButtons[i]->SetIsEnabled(EmployeeManager && EmployeeManager->CanAllocateStatPoint(
				Employee->EmployeeID, static_cast<EEmployeeStatIndex>(i)));
			// 비용 칩 숫자는 버튼 WBP 안(UIE_EmployeeStatInvestButton.CostText) — 능력치 Lv 마다 비용이 오른다
			if (UTextBlock* CostText = Cast<UTextBlock>(StatPlusButtons[i]->GetWidgetFromName(TEXT("CostText"))); CostText && EmployeeManager)
			{
				CostText->SetText(FText::AsNumber(EmployeeManager->GetStatPointCost(
					Employee->EmployeeID, static_cast<EEmployeeStatIndex>(i))));
			}
		}

		// 값 = 체감 단위 단독(2026-08-13). 원시 포인트는 "52" 처럼 아무 느낌이 없어 카드에서 감췄다 —
		// 포인트가 필요한 표면(종합·강화 모달)은 GetStatValueByIndex 를 직접 쓴다.
		bool bValueChanged = false;
		if (Values[i])
		{
			if (bHomeVisible && LastStatValues[i] != INDEX_NONE && LastStatValues[i] != EffectiveValue)
			{
				// 값이 바뀐 순간 — 카운트업 + 행 화이트 하이라이트 (§9-6). 틱이 같은 포맷 함수로 숫자를 덮어쓴다
				StatCount[i].Start(static_cast<float>(LastStatValues[i]), static_cast<float>(EffectiveValue), StatCountDuration);
				StatHighlightElapsed[i] = 0.f;
				bValueChanged = true;
				LastStatValues[i] = EffectiveValue;
			}
			else
			{
				// 카운트업 중이면 확정값을 쓰지 않는다 — 썼다가 다음 틱에 중간값으로 되돌아가 숫자가 튄다
				if (!StatCount[i].IsPlaying())
				{
					Values[i]->SetText(UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(i), EffectiveValue));
				}
				if (bHomeVisible || LastStatValues[i] == INDEX_NONE)
				{
					LastStatValues[i] = EffectiveValue;
				}
			}
		}

		// 그린칩도 같은 체감 효과의 델타. 업무속도와 이탈 확률은 감소가 이득이다.
		if (Bonuses[i])
		{
			const FText DeltaText = UEmployeeStatsHelper::GetStatFeltDeltaText(
				static_cast<uint8>(i), BaseValue, EffectiveValue);
			Bonuses[i]->SetText(FText::Format(NSLOCTEXT("EmployeeWindow", "StarforceBonus", "★{0}"), DeltaText));
			// 표시 소수점에서 0 으로 접히는 보정은 보정이 아니다 — "+0.00초" 줄은 접는다.
			// 보너스는 수치 아래 줄이라 Collapsed 여야 글자 묶음이 행 세로 가운데에 선다(Hidden 은 빈 줄 높이를 남긴다).
			const FString DeltaString = DeltaText.ToString();
			const bool bDeltaReads = DeltaString.FindLastCharByPredicate(
				[](TCHAR Character) { return Character >= TEXT('1') && Character <= TEXT('9'); }) != INDEX_NONE;
			BonusToggle->SetVisibility(
				bDeltaReads ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

			// 값이 바뀐 행의 보너스 칩만 페이드인 — 숨겨진 칩을 페이드해봐야 보이지 않는다 (§9-6)
			if (bValueChanged && bDeltaReads)
			{
				StatBonusFadeElapsed[i] = 0.f;
				Bonuses[i]->SetRenderOpacity(0.f);
			}
		}

		if (Descs[i])
		{
			// 효과 문구는 계수(EmployeeStatTuning/DA_FatigueConfig)에서 계산 — 하드코딩 0
			Descs[i]->SetText(UEmployeeStatsHelper::GetStatEffectText(static_cast<uint8>(i), EffectiveValue));
		}
	}
}

void UEmployeeWindowWidget::UpdateFilterChipMaterialSize(const FGeometry& /*MyGeometry*/)
{
	// 키라인이 그려지는 면의 지오메트리를 사용한다. 바깥 터치 영역은 더 클 수 있다.
	UWidget* ChipBackground = FilterChipSizeWidget.Get();
	if (!ChipBackground)
	{
		return;
	}

	const FVector2D LocalSize = ChipBackground->GetCachedGeometry().GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastFilterChipSize, 0.5f))
	{
		return;
	}
	LastFilterChipSize = LocalSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (const TWeakObjectPtr<UImage>& LayerPtr : FilterChipLayers)
	{
		UImage* Layer = LayerPtr.Get();
		if (!Layer)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = Layer->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(WpxParam, LocalSize.X);
			MID->SetScalarParameterValue(HpxParam, LocalSize.Y);
		}
	}
}

void UEmployeeWindowWidget::UpdateSurfaceMaterialSizes()
{
	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));

	// v2.7 다크 판/칩 SDF 키라인·면 (M_UI_Chip 계열). 헤더 멤버 추가 없이 이름 조회 + MID 파라미터
	// 리드백(Wpx)으로 크기 변화 프레임만 재주입한다 (FilterChip 캐시 패턴의 무-헤더 확장, LC 전역 재정의 함정 회피).
	struct FSdfSurface { const TCHAR* Name; float RadiusPx; bool bKeyline; };
	static const FSdfSurface SdfSurfaces[] = {
		{ TEXT("HeroPlateLine"),   20.f, true },
		{ TEXT("SkillPlateLine"),  20.f, true },
		{ TEXT("PotPlateLine"),    20.f, true },
		{ TEXT("RosterSortFace"),  14.f, false },   // 칩 면 — 크기만 (MI 베이크 색/반경 유지)
		{ TEXT("RosterSortLine"),  14.f, false },   // 칩 키라인 — 크기만 (필터 칩과 동급 서브틀)
	};
	static const FName RadiusParam(TEXT("RadiusPx"));
	static const FName ThickParam(TEXT("ThickPx"));
	static const FName BaseAParam(TEXT("BaseA"));
	static const FName GlintGainParam(TEXT("GlintGain"));
	static const FName FadeStartParam(TEXT("FadeStartPx"));
	static const FName FadeEndParam(TEXT("FadeEndPx"));
	static const FName LineColParam(TEXT("LineCol"));
	for (const FSdfSurface& Surface : SdfSurfaces)
	{
		UImage* Image = Cast<UImage>(GetWidgetFromName(Surface.Name));
		if (!Image)
		{
			continue;
		}
		const FVector2D LocalSize = Image->GetCachedGeometry().GetLocalSize();
		if (LocalSize.X < 1.f || LocalSize.Y < 1.f)
		{
			continue;
		}
		UMaterialInstanceDynamic* MID = Image->GetDynamicMaterial();
		if (!MID)
		{
			continue;
		}
		// MID 의 현재 Wpx 를 읽어 크기 변화가 있을 때만 주입 (헤더 캐시 멤버 없이 멱등).
		if (FMath::IsNearlyEqual(MID->K2_GetScalarParameterValue(WpxParam), LocalSize.X, 0.5f))
		{
			continue;
		}
		MID->SetScalarParameterValue(WpxParam, LocalSize.X);
		MID->SetScalarParameterValue(HpxParam, LocalSize.Y);
		if (Surface.bKeyline)
		{
			// 강화 모달급 키라인 — 화이트 α.18, 두께 2px, 판 반경, 글린트 정지, 페이드 off
			MID->SetScalarParameterValue(RadiusParam, Surface.RadiusPx);
			MID->SetScalarParameterValue(ThickParam, 2.f);
			MID->SetScalarParameterValue(BaseAParam, 0.18f);
			MID->SetScalarParameterValue(GlintGainParam, 0.f);
			MID->SetScalarParameterValue(FadeStartParam, 99999.f);
			MID->SetScalarParameterValue(FadeEndParam, 100000.f);
			MID->SetVectorParameterValue(LineColParam, FLinearColor::White);
		}
	}
}

void UEmployeeWindowWidget::HandleStatInfoTapped(int32 StatIndex)
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	if (!Employee || StatIndex < 0 || StatIndex >= NumEmployeeStats)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return;
	}

	const int32 EnhB = UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
	const int32 EffectiveValue =
		UEmployeeStatsHelper::GetStatValueByIndex(Employee->Stats, static_cast<uint8>(StatIndex)) + EnhB;

	// 앵커 = 탭한 셀의 화면 중심. 커서 좌표는 모바일 터치에서 갱신되지 않아 (0,0) 에 뜬다.
	FVector2D AnchorAbs = FVector2D::ZeroVector;
	if (UWidget* Btn = GetWidgetFromName(*FString::Printf(TEXT("HeroStatBtn%d"), StatIndex)))
	{
		const FGeometry& Geo = Btn->GetCachedGeometry();
		AnchorAbs = Geo.LocalToAbsolute(Geo.GetLocalSize() * 0.5f);
	}

	if (!ActiveStatTooltip)
	{
		const TSubclassOf<UUserWidget> TooltipClass = TableMgr->GetWidgetClass(EWidgetType::ItemTooltip);
		if (!TooltipClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] EWidgetType::ItemTooltip 미등록"));
			return;
		}
		ActiveStatTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass);
	}

	if (ActiveStatTooltip)
	{
		ActiveStatTooltip->ShowAt(
			AnchorAbs,
			UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(StatIndex)),
			nullptr,
			UEmployeeStatsHelper::GetStatEffectText(static_cast<uint8>(StatIndex), EffectiveValue),
			/*BasePrice*/ 0, /*Duration*/ 2.5f, ETooltipAnchor::BelowAnchor);
	}
}

FEventReply UEmployeeWindowWidget::HandleWorkPowerTapped(FGeometry MyGeometry, const FPointerEvent& /*PointerEvent*/)
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	if (!Employee)
	{
		return FEventReply(true);
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return FEventReply(true);
	}

	const ECompanyType Industry = EmployeeManager
		? EmployeeManager->GetCompanyTypeForBuilding(Employee->AssignedBuildingIndex) : ECompanyType::None;
	FString Description;
	int32 BestIndex = INDEX_NONE;
	int32 BestPoints = 0;
	FText BestName;
	for (int32 Index = 0; Index < Employee->DisciplinePoints.Num(); ++Index)
	{
		const int32 Points = Employee->DisciplinePoints.IsValidIndex(Index) ? Employee->DisciplinePoints[Index] : 0;
		const FText Name = TableMgr->GetDisciplineDisplayName(Industry, Index);
		if (Name.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] Missing discipline display name: industry=%d slot=%d"),
				static_cast<int32>(Industry), Index);
			return FEventReply(true);
		}
		if (BestIndex == INDEX_NONE || Points > BestPoints)
		{
			BestIndex = Index;
			BestPoints = Points;
			BestName = Name;
		}
		if (!Description.IsEmpty()) Description += TEXT("\n");
		Description += FString::Printf(TEXT("%s  %d"), *Name.ToString(), Points);
	}

	Description += FString::Printf(TEXT("\n\n업무력 = 최고 직능 %s %d"), *BestName.ToString(), BestPoints);
	Description += TEXT("\nPitch 카드의 팀 점수는 팀 전체의 값입니다.");

	if (!ActiveStatTooltip)
	{
		const TSubclassOf<UUserWidget> TooltipClass = TableMgr->GetWidgetClass(EWidgetType::ItemTooltip);
		if (!TooltipClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] EWidgetType::ItemTooltip 미등록"));
			return FEventReply(true);
		}
		ActiveStatTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass);
	}
	if (ActiveStatTooltip)
	{
		const FVector2D AnchorAbs = MyGeometry.LocalToAbsolute(
			FVector2D(MyGeometry.GetLocalSize().X * 0.5f, MyGeometry.GetLocalSize().Y));
		ActiveStatTooltip->ShowAt(AnchorAbs, NSLOCTEXT("EmployeeWindow", "WorkPowerTipTitle", "업무력"),
			nullptr, FText::FromString(Description), 0, 4.0f, ETooltipAnchor::BelowAnchor);
	}
	return FEventReply(true);
}

void UEmployeeWindowWidget::HandleItemChanged(EItemType Type, int32 NewCount, int32 Delta)
{
	if (Type == EItemType::RecruitTicketNormal || Type == EItemType::RecruitTicketAdvanced || Type == EItemType::RecruitTicketPremium)
	{
		RefreshHireSlotAffordability();
	}
}

void UEmployeeWindowWidget::RefreshWallet()
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		return;
	}

	if (WalletMoneyChip)
	{
		WalletMoneyChip->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Money));
	}
}

void UEmployeeWindowWidget::SelectEmployee(int32 EmployeeID)
{
	if (EmployeeID < 0 || EmployeeID == SelectedEmployeeID)
	{
		return;
	}

	SelectedEmployeeID = EmployeeID;

	// ListView 아이템 선택 동기화 → 카드 하이라이트는 엔트리가 스스로 갱신
	if (RosterListView)
	{
		for (UObject* Item : RosterListView->GetListItems())
		{
			// 빈자리 타일은 기본 EmployeeID(0) 를 들고 있어 ID 0 질의에 잘못 걸린다
			UEntityCardData* CardData = Cast<UEntityCardData>(Item);
			if (CardData && !CardData->bIsHireSlot && CardData->EmployeeInfo.EmployeeID == EmployeeID)
			{
				RosterListView->SetSelectedItem(Item);
				break;
			}
		}
	}

	RefreshSelected();
	if (CurrentEmployeePage == EEmployeeWindowPage::Equipment && EquipmentList)
	{
		EquipmentList->Configure(SelectedEmployeeID);
	}
}

void UEmployeeWindowWidget::OnRosterCardClicked(int32 EmployeeID)
{
	SelectEmployee(EmployeeID);
}

void UEmployeeWindowWidget::OnRosterEntryGenerated(UUserWidget& EntryWidget)
{
	if (UEmployeeRosterCardWidget* Card = Cast<UEmployeeRosterCardWidget>(&EntryWidget))
	{
		Card->OnRosterCardClicked.RemoveDynamic(this, &UEmployeeWindowWidget::OnRosterCardClicked);
		Card->OnRosterCardClicked.AddDynamic(this, &UEmployeeWindowWidget::OnRosterCardClicked);
		Card->OnRosterHireSlotClicked.RemoveDynamic(this, &UEmployeeWindowWidget::OnHireSlotClicked);
		Card->OnRosterHireSlotClicked.AddDynamic(this, &UEmployeeWindowWidget::OnHireSlotClicked);

		// 빈자리 펄스 판정은 창이 소유한다 — 카드는 채용 재화를 모른다 (§9-3)
		if (Card->IsHireSlot())
		{
			Card->SetHireSlot(CanAffordAnyHire());
		}
	}
}

void UEmployeeWindowWidget::OnRosterItemSelectionChanged(UObject* Item)
{
	UEntityCardData* CardData = Cast<UEntityCardData>(Item);
	if (bRestoringRosterSelection || !RosterListView || !CardData)
	{
		return;
	}
	if (!CardData->bIsHireSlot)
	{
		SelectEmployee(CardData->EmployeeInfo.EmployeeID);
		return;
	}

	// 빈자리 타일은 선택 대상이 아니다 — 선택 링을 직전 직원으로 되돌린다(없으면 해제). 되돌리는 동안 재진입 차단
	bRestoringRosterSelection = true;
	UObject* PreviousItem = nullptr;
	for (UObject* Candidate : RosterListView->GetListItems())
	{
		UEntityCardData* CandidateData = Cast<UEntityCardData>(Candidate);
		if (CandidateData && !CandidateData->bIsHireSlot && CandidateData->EmployeeInfo.EmployeeID == SelectedEmployeeID)
		{
			PreviousItem = Candidate;
			break;
		}
	}
	if (PreviousItem)
	{
		RosterListView->SetSelectedItem(PreviousItem);
	}
	else
	{
		RosterListView->ClearSelection();
	}
	bRestoringRosterSelection = false;
}

void UEmployeeWindowWidget::OnHireSlotClicked()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!TableMgr || !UIMgr || !UIMgr->GetUIBase())
	{
		return;
	}

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::OfficeRecruitmentModal);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] OfficeRecruitmentModal 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	// 채용 후 직원이 문에서 뛰어 들어오는 연출이 오피스에서 보여야 한다 — 책상 패널처럼 닫고 연다
	const int32 HireBuildingIndex = BuildingIndex;
	DeactivateWidget();
	UCommonActivatableWidget* Pushed = UIMgr->GetUIBase()->PushPromptClass(Cls.Get());
	if (UOfficeRecruitmentModalWidget* Modal = Cast<UOfficeRecruitmentModalWidget>(Pushed))
	{
		Modal->SetBuildingIndex(HireBuildingIndex);
	}
}

void UEmployeeWindowWidget::OnEnhanceClicked()
{
	if (SelectedEmployeeID < 0)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::EnhanceStarforceModal);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] EnhanceStarforceModal 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	// PromptStack 은 스위처식(top 만 표시)이라 push 하면 직원창이 사라짐 —
	// 모달은 뷰포트 오버레이로 직원창 위에 띄운다 (자체 딤이 아래 입력 차단, 닫힘 시 스스로 제거)
	// ⚠ ActivateWidget 금지: 뷰포트 root 활성화가 CommonUI 기본 입력컨피그(Menu)를 적용,
	//   닫힌 뒤에도 잔류해 게임 입력(카메라/월드탭)이 영구 차단됨 — 가챠 리빌 등 선례와 동일하게 비활성 오버레이로
	if (UEnhanceStarforceModalWidget* Modal = CreateWidget<UEnhanceStarforceModalWidget>(GetOwningPlayer(), Cls.Get()))
	{
		Modal->AddToViewport(120);
		Modal->Configure(SelectedEmployeeID);
		OpenEnhanceModal = Modal;
	}
}

void UEmployeeWindowWidget::OnFireClicked()
{
	FEmployeeInstance* Employee = GetSelectedEmployee();
	if (!Employee)
	{
		return;
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr)
	{
		return;
	}

	// 대상을 여는 시점에 고정 — 모달이 떠 있는 동안 로스터가 갱신되면 선택이 옮겨간다
	const int32 TargetID = Employee->EmployeeID;

	// ShowConfirmDialog 는 AddToViewport(10000) 이라 직원창이 살아 있다 (PromptStack push 는 스위처식이라 창이 꺼짐)
	UIMgr->ShowConfirmDialog(
		NSLOCTEXT("EmployeeWindow", "FireTitle", "직원 해고"),
		FText::FromString(FString::Printf(TEXT("%s 직원을 해고합니다. 되돌릴 수 없습니다."), *Employee->EmployeeName)),
		FSimpleDelegate::CreateUObject(this, &UEmployeeWindowWidget::HandleFireConfirmed, TargetID),
		FSimpleDelegate(), EConfirmDialogIcon::Delete);
}

void UEmployeeWindowWidget::HandleFireConfirmed(int32 EmployeeID)
{
	if (!EmployeeManager)
	{
		return;
	}

	// 로스터 재구성/재선택은 FireEmployee 의 OnEmployeeRosterChanged 브로드캐스트가 담당
	EmployeeManager->FireEmployee(EmployeeID);
}

void UEmployeeWindowWidget::OpenOddsTable(int32 EmployeeID)
{
	FEmployeeInstance* Employee = EmployeeManager ? EmployeeManager->FindEmployee(EmployeeID) : nullptr;
	if (!Employee)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::PotentialOddsPanel);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeWindow] PotentialOddsPanel 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	// 연타 시 딤이 겹치지 않도록 열려 있는 확률표를 걷고 새로 연다.
	if (UPotentialOddsWidget* Prev = OpenOddsPanel.Get())
	{
		Prev->RemoveFromParent();
		OpenOddsPanel.Reset();
	}

	// 강화 모달과 동일 — 뷰포트 오버레이. ⚠ ActivateWidget 금지(입력컨피그 잔류로 게임 입력 영구 차단)
	// CreateWidget<T> 는 타입 불일치 시 assert 즉사 — Cast 로 완만히 실패시킨다 (2026-08-06 LC 크래시)
	if (UPotentialOddsWidget* Panel = Cast<UPotentialOddsWidget>(CreateWidget(GetOwningPlayer(), Cls.Get())))
	{
		Panel->AddToViewport(120);
		OpenOddsPanel = Panel;
	}
}

void UEmployeeWindowWidget::OnCloseDelegate()
{
	CloseWithAnimation();
}

void UEmployeeWindowWidget::HandleEmployeeStatsChanged(int32 EmployeeID)
{
	RefreshRosterEmployee(EmployeeID);
	if (EmployeeID != SelectedEmployeeID)
	{
		return;
	}

	// 투자/자동배분/리롤 — 종합·스탯·잠재 부분 갱신 (로스터 재정렬은 다음 오픈)
	RefreshSelected();
}

void UEmployeeWindowWidget::HandleEmployeeDisciplineChanged(int32 EmployeeID)
{
	if (EmployeeID != SelectedEmployeeID)
	{
		return;
	}

	// 직능 투자/리셋 — argmax 부서가 바뀔 수 있어 부서칩·종합 갱신. 카드 6행은 카드 자체 구독으로 갱신됨.
	RefreshHero();
}

void UEmployeeWindowWidget::HandleEmployeeLevelUp(int32 EmployeeID, int32 NewLevel)
{
	RefreshRosterEmployee(EmployeeID);

	if (EmployeeID == SelectedEmployeeID)
	{
		RefreshHero();
		RefreshHeroStats();
	}
}

void UEmployeeWindowWidget::RefreshRosterEmployee(int32 EmployeeID)
{
	if (!RosterListView || !EmployeeManager) return;
	const FEmployeeInstance* Employee = EmployeeManager->FindEmployee(EmployeeID);
	if (!Employee) return;
	for (UObject* ListItem : RosterListView->GetListItems())
	{
		UEntityCardData* CardData = Cast<UEntityCardData>(ListItem);
		if (!CardData || CardData->bIsHireSlot || CardData->EmployeeInfo.EmployeeID != EmployeeID) continue;
		CardData->EmployeeInfo = *Employee;
		if (UEmployeeRosterCardWidget* Card = RosterListView->GetEntryWidgetFromItem<UEmployeeRosterCardWidget>(ListItem))
		{
			Card->SetEmployeeInfo(*Employee);
		}
	}
}

void UEmployeeWindowWidget::HandleRosterChanged()
{
	RefreshRoster();
	if (CurrentEmployeePage != EEmployeeWindowPage::Home && !GetSelectedEmployee()) ShowEmployeePage(EEmployeeWindowPage::Home);
}

void UEmployeeWindowWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	// 채용 가능 판정에 들어가는 재화는 다이아뿐 — 다른 재화까지 로스터를 훑으면 헛일이다
	if (Type == EResourceType::Diamond)
	{
		RefreshHireSlotAffordability();
	}

	if (Type != EResourceType::Money)
	{
		return;
	}

	RefreshWallet();
}

void UEmployeeWindowWidget::HandleStatPlusClicked(int32 StatIndex)
{
	if (!EmployeeManager || StatIndex < 0 || StatIndex >= static_cast<int32>(EEmployeeStatIndex::Count)) return;
	const FEmployeeInstance* Employee = GetSelectedEmployee();
	if (!Employee) return;
	EmployeeManager->AllocateStatPoint(Employee->EmployeeID, static_cast<EEmployeeStatIndex>(StatIndex));
}
