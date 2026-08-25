// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "CommonButtonBase.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Enum/NotificationType.h"
#include "UI/UISoundTags.h"
#include "Core/CGGameInstance.h"

// 정적 색상 상수 정의
const FLinearColor UCostActionButtonWidget::ColorEnabled = FLinearColor(1.0f, 0.878f, 0.424f, 1.0f);   // #FFE06CFF
const FLinearColor UCostActionButtonWidget::ColorDisabled = FLinearColor(0.29f, 0.29f, 0.29f, 1.0f);  // #4A4A4AFF

void UCostActionButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터에서 설정한 ButtonText 적용 (미설정 시 기본값 "강화")
	if (BtnText)
	{
		BtnText->SetText(ButtonText);

		// bOverrideTextStyle=true인 인스턴스에서만 폰트/색상/패딩 오버라이드.
		// false면 WBP에서 잡아둔 기본 스타일을 보존.
		if (bOverrideTextStyle)
		{
			// FontObject가 nullptr이면 SetFont 시 텍스트가 사라지므로, 유효 폰트일 때만 적용.
			if (BtnTextFont.FontObject)
			{
				BtnText->SetFont(BtnTextFont);
			}
			BtnText->SetColorAndOpacity(BtnTextColor);

			// 슬롯 종류별 SetPadding 분기. UPanelSlot 공통 SetPadding이 없어서 명시적 캐스팅 필요.
			if (UPanelSlot* PanelSlot = BtnText->Slot)
			{
				if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
				{
					OverlaySlot->SetPadding(BtnTextPadding);
				}
				else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
				{
					HBoxSlot->SetPadding(BtnTextPadding);
				}
				else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
				{
					VBoxSlot->SetPadding(BtnTextPadding);
				}
			}
		}
	}

	// 디자이너에서 편집한 DefaultResourceType / DefaultCost를 내부 CostWidget에 전파.
	// 디자이너 프리뷰에서도 아이콘과 숫자가 즉시 갱신됨.
	CacheCostWidget();
	if (CachedCostWidget)
	{
		CachedCostWidget->AmountRoundMode = ENumberRoundMode::Ceil; // 비용은 올림(부족 시 "같은데 왜 못 사" 혼동 방지)
		CachedCostWidget->SetResourceType(DefaultResourceType, true);
		CachedCostWidget->SetValue(DefaultCost);
	}

	// Btn은 UCommonButtonBase 체인 → 직접 SetStyle 호출로 종료.
	if (Btn && ButtonStyleClass)
	{
		Btn->SetStyle(ButtonStyleClass);
	}
}

void UCostActionButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 기본적으로 Enabled 상태로 시작
	bIsEnabled = true;

	// 재구성 시 펀치 상태 초기화 — 첫 갱신은 항상 silent
	bAffordInitialized = false;
	bPunchSuppressed = false;
	AffordPunch = FScalePunchAnimation();
	SetRenderScale(FVector2D(1.f, 1.f));

	CacheCostWidget();
	UpdateBorderColor();

	// UButton 기반 (구형 WBP)
	if (UpgradeButton)
	{
		UpgradeButton->OnClicked.AddDynamic(this, &UCostActionButtonWidget::HandleButtonClicked);
		UpgradeButton->OnPressed.AddDynamic(this, &UCostActionButtonWidget::HandleButtonPressed);
		UpgradeButton->OnReleased.AddDynamic(this, &UCostActionButtonWidget::HandleButtonReleased);
	}

	// UCommonButtonBase 기반 (신형 WBP — UIE_UpgradeBtn1 등)
	// OnClicked()는 Native event 반환 메서드 — AddUObject로 바인딩
	if (Btn)
	{
		Btn->OnClicked().AddUObject(this, &UCostActionButtonWidget::HandleButtonClicked);
		Btn->OnPressed().AddUObject(this, &UCostActionButtonWidget::HandleButtonPressed);
		Btn->OnReleased().AddUObject(this, &UCostActionButtonWidget::HandleButtonReleased);
	}
}

void UCostActionButtonWidget::NativeDestruct()
{
	if (UpgradeButton)
	{
		UpgradeButton->OnClicked.RemoveDynamic(this, &UCostActionButtonWidget::HandleButtonClicked);
		UpgradeButton->OnPressed.RemoveDynamic(this, &UCostActionButtonWidget::HandleButtonPressed);
		UpgradeButton->OnReleased.RemoveDynamic(this, &UCostActionButtonWidget::HandleButtonReleased);
	}

	if (Btn)
	{
		Btn->OnClicked().RemoveAll(this);
		Btn->OnPressed().RemoveAll(this);
		Btn->OnReleased().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UCostActionButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (const UWorld* TickWorld = GetWorld())
	{
		LastTickTimeSeconds = TickWorld->GetTimeSeconds();
	}

	if (!AffordPunch.IsPlaying())
	{
		return;
	}

	const float PunchNow = AffordPunch.Tick(InDeltaTime);
	SetRenderScale(FVector2D(PunchNow, PunchNow));
	UpdateBorderColor();

	if (!AffordPunch.IsPlaying())
	{
		SetRenderScale(FVector2D(1.f, 1.f));
		UpdateBorderColor();
	}
}

void UCostActionButtonWidget::CacheCostWidget()
{
	CachedCostWidget = Cast<UResourceWidget>(CostWidget);
}

void UCostActionButtonWidget::SetEnabled(bool bEnabled)
{
	// 외부 소유자의 명시 선언 — 비용 게이트보다 우선
	bExplicitEnableOverride = bEnabled;
	ApplyEnabled(bEnabled);
}

void UCostActionButtonWidget::SetLockedState(bool bLocked)
{
	ApplyEnabled(!bLocked);
}

void UCostActionButtonWidget::ApplyEnabled(bool bEnabled)
{
	bIsEnabled = bEnabled;

	// 두 종류 버튼 모두 활성화/비활성화
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(bEnabled);
	}
	if (Btn)
	{
		Btn->SetIsEnabled(bEnabled);
	}

	// Border 색상 업데이트
	UpdateBorderColor();
}

void UCostActionButtonWidget::SetButtonText(const FText& Text)
{
	// MAX 해제 시 복구할 원래 라벨이므로 멤버에도 보관
	ButtonText = Text;

	if (BtnText && !bIsMaxLevel)
	{
		BtnText->SetText(Text);
	}
}

void UCostActionButtonWidget::SetCost(int64 Cost, EResourceType ResourceType, bool bCheckAfford)
{
	CurrentCost = Cost;
	CurrentResourceType = ResourceType;

	// Cast 는 여기서 1회 캐시 (NativeTick 에서 매 프레임 캐스팅 금지)
	if (!CachedCostWidget)
	{
		CacheCostWidget();
	}

	if (CachedCostWidget)
	{
		// 모든 자원 type 에 동일 처리 — DT_Resource.UIColor 가 단일 진실 (Box 포함).
		// Box row 의 UIColor 가 검정/짙은 색이면 자동 적용됨.
		CachedCostWidget->AmountRoundMode = ENumberRoundMode::Ceil; // 비용은 올림(부족 시 "같은데 왜 못 사" 혼동 방지)
		CachedCostWidget->SetResourceType(ResourceType, /*bUseTypeColor=*/true);
		CachedCostWidget->SetValue(Cost);
	}

	// 재화 체크 → 구매가능 상태 세터 (잠금 축과 분리, SetEnabled 는 건드리지 않음)
	if (bCheckAfford)
	{
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
		{
			if (UResourceItemManager* ResourceManager = GI->GetSubsystem<UResourceItemManager>())
			{
				const int64 CurrentAmount = ResourceManager->GetResourceAmount(ResourceType);
				SetCanAfford(CurrentAmount >= Cost);
			}
		}
	}
}

void UCostActionButtonWidget::SetCanAfford(bool bCanAfford)
{
	const UWorld* AffordWorld = GetWorld();
	const float Now = AffordWorld ? AffordWorld->GetTimeSeconds() : 0.f;

	// 위젯이 최근에 그려지지 않았다면 패널 오픈 프레임의 초기 채움 → 전 슬롯 동시 튐 방지
	const bool bSilentInit = !bAffordInitialized || (Now - LastTickTimeSeconds) > 0.25f;
	const bool bWasCanAfford = bLastCanAfford;

	bLastCanAfford = bCanAfford;
	bAffordInitialized = true;

	// CostWidget의 텍스트 색상도 변경 (빨간색/기본색)
	if (CachedCostWidget)
	{
		CachedCostWidget->SetCanAfford(bCanAfford);
	}

	const bool bBecameAffordable = bCanAfford && !bWasCanAfford;
	if (bBecameAffordable && !bSilentInit && !bPunchSuppressed && !bIsMaxLevel && bIsEnabled && !bExplicitEnableOverride)
	{
		AffordPunch.Start(AffordPunchScale, AffordPunchDuration);
	}

	UpdateBorderColor();
}

void UCostActionButtonWidget::SetMaxLevelState(bool bInIsMaxLevel)
{
	if (bIsMaxLevel == bInIsMaxLevel)
	{
		return;
	}
	bIsMaxLevel = bInIsMaxLevel;

	if (CostWidget)
	{
		CostWidget->SetVisibility(bIsMaxLevel ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (BtnText)
	{
		BtnText->SetText(bIsMaxLevel ? MaxLevelText : ButtonText);
	}

	if (bIsMaxLevel)
	{
		AffordPunch = FScalePunchAnimation();
		SetRenderScale(FVector2D(1.f, 1.f));
	}

	UpdateBorderColor();
}

void UCostActionButtonWidget::SetPunchSuppressed(bool bInSuppressed)
{
	bPunchSuppressed = bInSuppressed;

	if (bInSuppressed && AffordPunch.IsPlaying())
	{
		AffordPunch = FScalePunchAnimation();
		SetRenderScale(FVector2D(1.f, 1.f));
		UpdateBorderColor();
	}
}

void UCostActionButtonWidget::UpdateBorderColor()
{
	if (!Border_Light)
	{
		return;
	}

	const bool bBright = bIsEnabled && bLastCanAfford && !bIsMaxLevel;
	FLinearColor RimColor = bBright ? ColorEnabled : ColorDisabled;

	// 재구매 가능 전환 시 림 밝기 1회 상승 (펀치와 같은 곡선)
	if (AffordPunch.IsPlaying())
	{
		const float PunchRange = FMath::Max(AffordPunchScale - 1.f, KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp((AffordPunch.GetCurrentScale() - 1.f) / PunchRange, 0.f, 1.f);
		const float Boost = 1.f + 0.35f * Alpha;
		RimColor.R = FMath::Min(RimColor.R * Boost, 1.f);
		RimColor.G = FMath::Min(RimColor.G * Boost, 1.f);
		RimColor.B = FMath::Min(RimColor.B * Boost, 1.f);
	}

	Border_Light->Background.TintColor = FSlateColor(RimColor);
	Border_Light->SetBrush(Border_Light->Background);
}

bool UCostActionButtonWidget::ShouldRejectInput() const
{
	// 잠금/MAX 는 이미 눌리지 않는다 — 여기선 자금 부족만 처리.
	// 소유자가 명시적으로 켠 버튼(비용 게이트 미사용)은 삼키지 않는다.
	return bIsEnabled && !bExplicitEnableOverride && !bIsMaxLevel && bAffordInitialized && !bLastCanAfford;
}

void UCostActionButtonWidget::PlayInsufficientFeedback()
{
	const UWorld* FeedbackWorld = GetWorld();
	const float Now = FeedbackWorld ? FeedbackWorld->GetTimeSeconds() : 0.f;

	// 셰이크·사운드·토스트가 쿨다운을 공유 (Pressed+Clicked 이중 발화, 10Hz 홀드 연타 모두 차단)
	if (Now - LastRejectFeedbackTime < RejectFeedbackCooldown)
	{
		return;
	}
	LastRejectFeedbackTime = Now;

	if (CachedCostWidget)
	{
		CachedCostWidget->PlayAffordShake();
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
	{
		SoundMgr->PlayUISound(CGUISoundTags::ButtonDisabled);
	}

	// 부족분 문구 조립은 UIManager 로 옮겼다 — 같은 문구를 다른 거부 경로들과 공유한다.
	// 위 쿨다운은 셰이크/사운드용이고, 토스트 자체의 중복 억제는 헬퍼가 따로 한다.
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->NotifyInsufficientResource(CurrentResourceType, CurrentCost);
	}
}

void UCostActionButtonWidget::HandleButtonClicked()
{
	if (ShouldRejectInput())
	{
		PlayInsufficientFeedback();
		return;
	}
	OnClickedEvent.Broadcast();
}

void UCostActionButtonWidget::HandleButtonPressed()
{
	// Pressed 를 삼켜야 홀드 타이머 자체가 시작되지 않는다
	if (ShouldRejectInput())
	{
		PlayInsufficientFeedback();
		return;
	}
	OnPressedEvent.Broadcast();
}

void UCostActionButtonWidget::HandleButtonReleased()
{
	OnReleasedEvent.Broadcast();
}
