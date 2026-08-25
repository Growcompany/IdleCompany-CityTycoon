#include "UI/Panel/RewardRevealPresentationWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Table/ShopItemTable.h"
#include "Global/GlobalUtilFunctions.h"

URewardRevealPresentationWidget::URewardRevealPresentationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 박스 WBP 하드 참조 — 쿠킹 안전 (가챠 리빌 위젯 패턴)
	static ConstructorHelpers::FClassFinder<UUserWidget> BoxFinder(
		TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_ItemCard_QtyBelow"));
	if (BoxFinder.Succeeded())
	{
		BoxWidgetClass = BoxFinder.Class;
	}

	// 토스트 변형 — 검은 밴드 위 기준 (수량필 S3 #2A3447 / 카드↔수량 간격 / 글자 크기 자체 보유)
	static ConstructorHelpers::FClassFinder<UUserWidget> ToastBoxFinder(
		TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_ItemCard_QtyBelow_Toast"));
	if (ToastBoxFinder.Succeeded())
	{
		ToastBoxWidgetClass = ToastBoxFinder.Class;
	}
}

void URewardRevealPresentationWidget::SetupRewards(
	const TArray<FMissionReward>& InRewards,
	const FText& InTitle,
	bool bInToastMode,
	bool bInShowToastTitle)
{
	bToastMode = bInToastMode;
	HoldDelay          = bToastMode ? ToastHoldSeconds     : AutoCloseDelay;
	ActiveStagger      = bToastMode ? ToastStaggerSeconds  : StaggerInterval;
	ActiveCloseDuration= bToastMode ? ToastFadeOutSeconds  : CloseDuration;

	// 토스트는 게임 입력을 막지 않는다. SelfHitTestInvisible 은 자식(카드 내부 버튼)이 클릭을 삼켜
	// 아래 월드 입력을 가로채므로 부족하다 — 자식까지 통과시키는 HitTestInvisible 이어야 한다.
	// WBP 에서 유실돼도 동작이 깨지지 않도록 코드에서 못박는다 (RewardClaimSplashWidget 선례).
	if (bToastMode)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (TitleText)
	{
		// 토스트는 기본으로 타이틀을 접는다 — 미션 카드를 방금 탭한 직후라 맥락이 이미 명확하고,
		// 한 줄이 빠지면 밴드가 세로로 짧아져 화면을 덜 가린다.
		if (bToastMode && !bToastShowTitle && !bInShowToastTitle)
		{
			TitleText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
			TitleText->SetText(InTitle.IsEmpty() ? NSLOCTEXT("Reward", "RewardTitle", "보상 획득!") : InTitle);
		}
	}

	if (!BoxRow)
	{
		// 여기서 그냥 return 하면 홀드/페이드 타이머가 하나도 안 서서 위젯이 영원히 안 닫힌다
		// (모달이면 딤이 박혀 게임 입력 영구 차단). 스스로 물러나게 한다.
		UE_LOG(LogTemp, Warning, TEXT("[RewardReveal] BoxRow 바인딩 없음 — 보상 표시 생략하고 즉시 닫음"));
		BeginClose();
		return;
	}
	BoxRow->ClearChildren();
	Boxes.Reset();

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	for (const FMissionReward& R : InRewards)
	{
		// 자원(돈/다이아) 박스 — 툴팁 = 재화 이름/설명 (FResourceInfo)
		if (R.ResourceType != EResourceType::None && R.Amount > 0 && TableMgr)
		{
			bool bFound = false;
			const FResourceInfo Info = TableMgr->GetResourceInfo(R.ResourceType, bFound);
			UTexture2D* Icon = (bFound && !Info.Icon.IsNull()) ? Info.Icon.LoadSynchronous() : nullptr;
			// "+" 접두는 2026-07-27 사용자 기각 — 아이콘이 이미 재화를 말하고 보상 맥락이라 군더더기.
			MakeBox(Icon, UGlobalUtilFunctions::AbbreviateNumber(R.Amount), Info.DisplayName, Info.Description);
		}
		// 아이템(채용권 등) 박스 — 툴팁 = 아이템 이름 (DT_ShopItem, 설명 컬럼 없음)
		if (R.ItemType != EItemType::None && R.ItemAmount > 0 && TableMgr)
		{
			bool bFound = false;
			UTexture2D* Icon = TableMgr->GetItemIcon(R.ItemType, bFound);
			FShopItemTable ItemRow;
			const FText ItemName = TableMgr->GetShopItemByItemType(R.ItemType, ItemRow) ? ItemRow.DisplayName : FText::GetEmpty();
			MakeBox(Icon, FText::Format(NSLOCTEXT("Reward", "Qty", "x{0}"), FText::AsNumber(R.ItemAmount)), ItemName, FText::GetEmpty());
		}
	}

	// 등장 시작 — 전부 숨기고 스태거로 띄움
	for (UWidget* Box : Boxes)
	{
		if (Box) { Box->SetRenderOpacity(0.f); }
	}
	RevealIndex = 0;
	RevealTimer = 0.f;

	if (bToastMode)
	{
		// 밴드가 먼저 떠오르고 그 다음 카드가 얹힌다 (모달은 즉시 등장)
		SetRenderOpacity(0.f);
		FadeInElapsed = 0.f;
		bRevealing = false;
		AutoCloseElapsed = -1.f;
	}
	else
	{
		FadeInElapsed = -1.f;
		bRevealing = Boxes.Num() > 0;
		AutoCloseElapsed = bRevealing ? -1.f : 0.f;
	}

	// 카운트다운 초기 표시 (모달 전용 — 토스트 트리엔 CountdownText 가 없다)
	LastShownSec = FMath::CeilToInt(HoldDelay);
	if (CountdownText)
	{
		CountdownText->SetText(FText::Format(NSLOCTEXT("Reward", "Countdown", "{0}초 후 자동으로 받기"), FText::AsNumber(LastShownSec)));
	}
}

UItemCardSlotWidget* URewardRevealPresentationWidget::MakeBox(UTexture2D* Icon, const FText& Label, const FText& TooltipName, const FText& TooltipDesc, int32 TooltipPrice)
{
	// 토스트 변형이 없으면 원본으로 폴백 (미생성 상태에서도 보상은 뜬다)
	TSubclassOf<UUserWidget> CardClass = (bToastMode && ToastBoxWidgetClass) ? ToastBoxWidgetClass : BoxWidgetClass;
	if (!CardClass || !BoxRow)
	{
		return nullptr;
	}
	UItemCardSlotWidget* Box = CreateWidget<UItemCardSlotWidget>(this, CardClass);
	if (!Box)
	{
		return nullptr;
	}
	Box->SetIcon(Icon);
	Box->SetLabel(Label);

	// 토스트는 카드 크기/간격/필 색을 코드로 건드리지 않는다 — 전부 변형 WBP 소유.
	if (!bToastMode)
	{
		// 클릭 → 인라인 툴팁 활성화. ZOrder = 이 오버레이(10000)+1 → 딤 위로 띄움 (안 그러면 보상창 뒤에 가려짐).
		// 토스트는 HitTestInvisible 이라 클릭을 못 받는다 → 툴팁 opt-in 생략 (정확수치는 HUD 칩 클릭으로 제공).
		Box->SetTooltipInfo(TooltipName, TooltipDesc, Icon, TooltipPrice, /*ZOrder*/10001);
	}

	if (UHorizontalBoxSlot* BoxSlot = BoxRow->AddChildToHorizontalBox(Box))
	{
		BoxSlot->SetPadding(bToastMode ? FMargin(ToastCardGapPx * 0.5f, 0.f) : FMargin(12.f, 0.f));
		BoxSlot->SetVerticalAlignment(VAlign_Center);
	}
	Box->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Boxes.Add(Box);
	return Box;
}

void URewardRevealPresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ReceiveButton)
	{
		ReceiveButton->OnClicked().AddUObject(this, &URewardRevealPresentationWidget::OnReceiveClicked);
	}
}

void URewardRevealPresentationWidget::NativeDestruct()
{
	if (ReceiveButton)
	{
		ReceiveButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void URewardRevealPresentationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 0) 카운트다운 텍스트 — 초가 바뀔 때만 갱신 (5→4→3→2→1)
	if (CountdownText && !bClosing)
	{
		const float Remain = (AutoCloseElapsed < 0.f) ? HoldDelay : FMath::Max(HoldDelay - AutoCloseElapsed, 0.f);
		const int32 Secs = FMath::CeilToInt(Remain);
		if (Secs != LastShownSec)
		{
			LastShownSec = Secs;
			CountdownText->SetText(FText::Format(NSLOCTEXT("Reward", "Countdown", "{0}초 후 자동으로 받기"), FText::AsNumber(FMath::Max(Secs, 0))));
		}
	}

	// 1) 밴드 페이드인 (토스트 전용) — 끝나기 전엔 카드 스태거를 시작하지 않는다
	if (FadeInElapsed >= 0.f)
	{
		FadeInElapsed += InDeltaTime;
		const float T = FMath::Clamp(FadeInElapsed / FMath::Max(ToastFadeInSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
		SetRenderOpacity(T);
		if (T < 1.f)
		{
			return;
		}
		FadeInElapsed = -1.f;
		bRevealing = Boxes.Num() > 0;
		if (!bRevealing)
		{
			AutoCloseElapsed = 0.f;
		}
	}

	// 2) 스태거 등장
	if (bRevealing)
	{
		RevealTimer += InDeltaTime;
		while (RevealIndex < Boxes.Num() && RevealTimer >= RevealIndex * ActiveStagger)
		{
			if (UWidget* Box = Boxes[RevealIndex])
			{
				Box->SetRenderOpacity(1.f);
				Box->SetRenderScale(FVector2D(1.f, 1.f));
			}
			RevealIndex++;
		}
		if (RevealIndex >= Boxes.Num())
		{
			bRevealing = false;
			AutoCloseElapsed = 0.f;  // 등장 끝 → 홀드 카운트 시작
		}
	}

	// 3) 홀드 만료 → 닫기 (모달 5초 / 토스트 1.8초)
	if (!bClosing && AutoCloseElapsed >= 0.f)
	{
		AutoCloseElapsed += InDeltaTime;
		if (AutoCloseElapsed >= HoldDelay)
		{
			BeginClose();
		}
	}

	// 4) 페이드아웃 닫기
	if (bClosing)
	{
		CloseElapsed += InDeltaTime;
		const float T = FMath::Clamp(CloseElapsed / FMath::Max(ActiveCloseDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		SetRenderOpacity(1.f - T);
		if (T >= 1.f)
		{
			RemoveFromParent();
		}
	}
}

void URewardRevealPresentationWidget::OnReceiveClicked()
{
	BeginClose();
}

void URewardRevealPresentationWidget::BeginClose()
{
	if (bClosing) { return; }
	bClosing = true;
	CloseElapsed = 0.f;
}
