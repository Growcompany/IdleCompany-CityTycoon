#include "UI/Panel/CityCompanyManageWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/Element/Common/RadialProgressWidget.h"
#include "UI/Element/Common/CoinFlyoutContainerWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Manager/ResourceItemManager.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/UIBase.h"
#include "Enum/WidgetType.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/CityCompanyData.h"
#include "Table/ResourceInfo.h"
#include "Enum/ResourceType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"

namespace
{
	// 링 상태색 (linear) — 회수율 기본 블루 / 본전 근접(80%+) 앰버 / 본전 돌파(100%+) 골드
	const FLinearColor RingBlue(0.047f, 0.328f, 0.745f, 1.f);
	const FLinearColor RingAmber(0.745f, 0.323f, 0.047f, 1.f);
	const FLinearColor RingGold(0.768f, 0.451f, 0.053f, 1.f);
}

void UCityCompanyManageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 풀 재사용 인스턴스는 이전 세션의 CompanyKey 를 들고 들어온다 — 소유권은 매 세션 ConfigureForCompany 가 다시 준다
	bOwnsFocusTarget = false;

	if (Button_Demolish)
	{
		Button_Demolish->OnClicked().AddUObject(this, &UCityCompanyManageWidget::HandleDemolish);
	}
	if (Button_SkipRecovery)
	{
		Button_SkipRecovery->OnClicked().AddUObject(this, &UCityCompanyManageWidget::HandleSkipRecovery);
	}
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UCityCompanyManageWidget::HandleCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UCityCompanyManageWidget::HandleBackgroundClicked);
	}
}

void UCityCompanyManageWidget::NativeDestruct()
{
	if (Button_Demolish)
	{
		Button_Demolish->OnClicked().RemoveAll(this);
	}
	if (Button_SkipRecovery)
	{
		Button_SkipRecovery->OnClicked().RemoveAll(this);
	}
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UCityCompanyManageWidget::HandleBackgroundClicked);
	}
	if (DripCoinFlyout)
	{
		DripCoinFlyout->OnStreamCoinArrived.Unbind();
	}
	if (UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr)
	{
		AcqMgr->OnCompanyProgressChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UCityCompanyManageWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
		{
			PC->GoToUIMode();

			// 비활성화 때 해제한 가림 고스트를 복귀 시 되살린다 — 카메라는 건드리지 않는다(프레이밍은 이미 맞다)
			if (bOwnsFocusTarget)
			{
				UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
				APlayerCamera* Camera = Cast<APlayerCamera>(PC->GetPawn());
				if (AcqMgr && Camera)
				{
					Camera->RestoreFocusTarget(AcqMgr->GetCompanyActor(CompanyKey));
				}
			}
		}
	}

	// 패널이 열린 동안 Money 카운터 즉시 갱신 억제 — 코인 착지 순간에만 롤업 (인과 연출)
	if (UUIManagerSubsystem* UIM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		if (UInGameLayerWidget* Layer = UIM->GetInGameLayer())
		{
			Layer->BeginMoneyDripFocus();
		}
	}
}

void UCityCompanyManageWidget::NativeOnDeactivated()
{
	if (UUIManagerSubsystem* UIM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		if (UInGameLayerWidget* Layer = UIM->GetInGameLayer())
		{
			Layer->EndMoneyDripFocus();
		}
	}

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
		{
			// 가림 고스트 해제 — 모드 복원과 쌍 (복귀는 NativeOnActivated 가 재등록)
			if (APlayerCamera* Camera = Cast<APlayerCamera>(PC->GetPawn()))
			{
				Camera->ClearFocusTarget();
			}

			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}
	Super::NativeOnDeactivated();
}

UWidget* UCityCompanyManageWidget::GetDemolishButtonWidget() const
{
	return Button_Demolish;
}

void UCityCompanyManageWidget::ConfigureForCompany(int32 InKey)
{
	CompanyKey = InKey;

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI) { return; }

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!TableMgr || !AcqMgr) { return; }

	// 진행도 유효성 1차 검사 — Milking/Depleted 가 아니면 열 이유 없음
	int64 Cost = 0, Earned = 0, Remaining = 0;
	bool bDepleted = false;
	if (!AcqMgr->GetCompanyProgress(InKey, Cost, Earned, Remaining, bDepleted))
	{
		DeactivateWidget();
		return;
	}

	// 여는 쪽(OpenManagePanel)이 이 회사로 카메라 포커스를 걸어 둔다 — 이후 복귀 시 되살릴 대상이 확정되는 지점
	bOwnsFocusTarget = true;

	// 오픈 시점 Earned 를 델타 기준점으로 — 첫 프레임에 코인이 쏟아지지 않게
	LastEarnedForFx = Earned;
	PendingLandingDelta = 0;

	// 정적 정보(회사명/아이콘)는 변하지 않으므로 여기서 1회만 채움
	FCityCompanyData D;
	const bool bHasData = TableMgr->GetCityCompanyData(InKey, D);

	// 헤더 회사명
	if (Text_CompanyName)
	{
		Text_CompanyName->SetText(bHasData ? FText::FromString(D.GetDisplayName()) : FText::GetEmpty());
	}

	// 로고 아이콘 (없으면 Collapsed — 자리 유지 안 함)
	if (Image_CompanyIcon)
	{
		if (bHasData && !D.CompanyIcon.IsNull())
		{
			UTexture2D* IconTex = D.CompanyIcon.LoadSynchronous();
			if (IsValid(IconTex))
			{
				Image_CompanyIcon->SetBrushFromTexture(IconTex);
			}
			else
			{
				Image_CompanyIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			Image_CompanyIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 동적 진행도(링/퍼센트/칩/철거 버튼/드립 레이트) 목표 갱신 — 드립마다 재호출
	RefreshFromProgress();
	// 오픈 시엔 0→현재로 차오르지 않게 즉시 스냅(이후 드립부터 NativeTick 이 부드럽게 보간)
	DisplayedPct = TargetPct;
	ApplyDisplay(DisplayedPct);

	// 매니저 진행도 이벤트 구독 — 1초 드립/고갈 전환 시 푸시. 재사용 위젯 대비 중복 구독 가드.
	AcqMgr->OnCompanyProgressChanged.RemoveAll(this);
	AcqMgr->OnCompanyProgressChanged.AddUObject(this, &UCityCompanyManageWidget::HandleProgressChanged);
}

void UCityCompanyManageWidget::RefreshFromProgress()
{
	UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!AcqMgr) { return; }

	int64 Cost = 0, Earned = 0, Remaining = 0;
	bool bDepleted = false;
	if (!AcqMgr->GetCompanyProgress(CompanyKey, Cost, Earned, Remaining, bDepleted)) { return; }

	// 동적 목표만 갱신 — 실제 링/숫자는 NativeTick 이 DisplayedPct 를 lerp 로 따라가며 ApplyDisplay 로 그림
	CachedCost = Cost;
	CachedTotal = Earned + Remaining;
	TargetPct = (CachedTotal > 0) ? FMath::Clamp((float)((double)Earned / (double)CachedTotal), 0.f, 1.f) : 0.f;
	bDepletedCached = bDepleted;

	int64 Chunk = 0;
	float SecondsPerDrip = 0.f;
	AcqMgr->GetDripInfo(CompanyKey, Chunk, ExpectedPerSec, SecondsPerDrip);

	// 인수가(불변)는 즉시 표시
	if (Text_Cost)
	{
		Text_Cost->SetText(UGlobalUtilFunctions::AbbreviateNumber(Cost));
	}

	// 회수 속도 라인 — 티어가 올라갈수록 드립이 드물어져 "초당"이 0으로 반올림된다.
	// 시간 단위는 UGlobalUtilFunctions 가 크기에 맞춰 골라 준다(초당/분당/시간당).
	if (Text_DripRate)
	{
		if (bDepleted)
		{
			Text_DripRate->SetText(FText::FromString(TEXT("회수 완료")));
		}
		else
		{
			Text_DripRate->SetText(FText::FromString(FString::Printf(TEXT("%s 회수 중"),
				*UGlobalUtilFunctions::FormatRatePerBestUnit(ExpectedPerSec, SecondsPerDrip))));
		}
	}

	// 철거 = 캐시아웃이라 언제든 누를 수 있다(매니저는 Milking/Depleted 양쪽 허용).
	// 회수 중 철거는 남은 회수를 버리는 선택이므로 HandleDemolish 가 확인 모달로 한 번 막는다.
	// 라벨은 상태와 무관하게 "철거" 하나 — 상태마다 문구가 바뀌면 무엇이 달라졌는지 되짚게 만든다.
	if (Button_Demolish)
	{
		Button_Demolish->SetIsEnabled(true);
		Button_Demolish->SetButtonText(FText::FromString(TEXT("철거")));
	}

	// [보석] 즉시 완료 — 회수가 남아 있을 때만 살 게 있다. 고갈이면 접는다.
	// 가격 표시·afford 체크·부족 안내는 전부 UCostActionButtonWidget 이 한다(SetCost 한 줄).
	if (Button_SkipRecovery)
	{
		Button_SkipRecovery->SetVisibility(bDepleted ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		if (!bDepleted)
		{
			Button_SkipRecovery->SetCost(AcqMgr->GetSkipDiamondCost(CompanyKey), EResourceType::Diamond);
		}
	}
}

void UCityCompanyManageWidget::ApplyDisplay(float Pct)
{
	if (Ring_Remaining)
	{
		Ring_Remaining->SetPercent(Pct);
	}
	if (Text_Percent)
	{
		Text_Percent->SetText(FText::FromString(FString::Printf(TEXT("%.1f%%"), Pct * 100.f)));
	}

	// 누적 회수 / 잔여는 DisplayedPct × 총액으로 역산 — 링과 함께 부드럽게 흐름
	const int64 Earned = (int64)FMath::RoundToDouble((double)Pct * (double)CachedTotal);
	const int64 Remaining = FMath::Max((int64)0, CachedTotal - Earned);
	if (Text_Earned)
	{
		Text_Earned->SetText(UGlobalUtilFunctions::AbbreviateNumber(Earned));
	}
	if (Text_Remaining)
	{
		Text_Remaining->SetText(UGlobalUtilFunctions::AbbreviateNumber(Remaining));
	}

	// ETA = 잔여 ÷ 초당 기대 드립 (올림 — 잔여>0 인데 00:00 방지). Depleted 면 숨김.
	if (Text_Eta)
	{
		if (bDepletedCached || ExpectedPerSec <= 0)
		{
			Text_Eta->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Text_Eta->SetVisibility(ESlateVisibility::HitTestInvisible);
			// 올림 — 잔여>0 인데 "0초"로 보이지 않게. 티어 곡선이 들어오며 5분~24시간을 다 태우게 됐으므로
			// 로컬 사본 대신 표준 포매터를 쓴다(견적 카드의 예상 시간과 표기 규칙 일치).
			const int64 EtaSec = (Remaining + ExpectedPerSec - 1) / ExpectedPerSec;
			Text_Eta->SetText(FText::Format(
				NSLOCTEXT("CityAcq", "EtaFmt", "회수 완료까지 {0}"),
				UGlobalUtilFunctions::FormatDurationKorean((double)EtaSec)));
		}
	}

	// 본전 게이트 — 회수율(표시 Earned / 인수가) 100% 이상이면 골드 배지
	const double Recoup = (CachedCost > 0) ? (double)Earned / (double)CachedCost : 0.0;
	if (PaidoffBorder && Text_Paidoff)
	{
		if (Recoup >= 1.0)
		{
			PaidoffBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
			Text_Paidoff->SetText(FText::FromString(FString::Printf(TEXT("본전 돌파 +%d%%"),
				FMath::RoundToInt((Recoup - 1.0) * 100.0))));
		}
		else
		{
			PaidoffBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 링 상태색 — 본전 근접/돌파를 색으로 예고
	if (Ring_Remaining)
	{
		Ring_Remaining->SetFillColor(Recoup >= 1.0 ? RingGold : (Recoup >= 0.8 ? RingAmber : RingBlue));
	}
}

void UCityCompanyManageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 1초 드립마다 점프하는 TargetPct 를 매 프레임 부드럽게 따라가 링/숫자가 차오르는 느낌
	if (!FMath::IsNearlyEqual(DisplayedPct, TargetPct, 0.0005f))
	{
		DisplayedPct = FMath::FInterpTo(DisplayedPct, TargetPct, InDeltaTime, 2.0f);
		if (FMath::IsNearlyEqual(DisplayedPct, TargetPct, 0.0005f)) { DisplayedPct = TargetPct; }
		ApplyDisplay(DisplayedPct);
	}

	// 코인 착지 펀치 — 누적 회수 값 스케일 (종료 시 원복)
	if (EarnedPunch.IsPlaying() && Text_Earned)
	{
		const float S = EarnedPunch.Tick(InDeltaTime);
		Text_Earned->SetRenderScale(FVector2D(S, S));
		if (!EarnedPunch.IsPlaying())
		{
			Text_Earned->SetRenderScale(FVector2D(1.f, 1.f));
		}
	}
}

void UCityCompanyManageWidget::HandleProgressChanged(int32 ChangedKey)
{
	if (ChangedKey != CompanyKey) { return; }

	// Earned 증가분만큼 코인 연출 (고갈 전환 브로드캐스트는 델타 0 → 코인 없음)
	if (UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr)
	{
		int64 Cost = 0, Earned = 0, Remaining = 0;
		bool bDepleted = false;
		if (AcqMgr->GetCompanyProgress(CompanyKey, Cost, Earned, Remaining, bDepleted))
		{
			const int64 Delta = Earned - LastEarnedForFx;
			if (Delta > 0)
			{
				SpawnDripCoinFx(Delta);
			}
			LastEarnedForFx = Earned;
		}
	}

	RefreshFromProgress();
}

void UCityCompanyManageWidget::SpawnDripCoinFx(int64 Delta)
{
	// 스폰이 스로틀로 생략돼도 착지 시 일괄 정산되도록 먼저 누적
	PendingLandingDelta += Delta;

	if (!EffectCanvas || !LogoOverlay || !Chip2Border) { return; }

	// 상주 컨테이너 1회 생성 — AddToViewport 금지(팝업 위 그려짐), EffectCanvas 풀스트레치 자식
	if (!DripCoinFlyout)
	{
		DripCoinFlyout = CreateWidget<UCoinFlyoutContainerWidget>(this);
		if (!DripCoinFlyout) { return; }
		DripCoinFlyout->StreamCoinDrawSize = 64.f;   // 패널 위 드립은 HUD 기본(44)보다 크게
		DripCoinFlyout->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* FxSlot = EffectCanvas->AddChildToCanvas(DripCoinFlyout))
		{
			FxSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			FxSlot->SetOffsets(FMargin(0.f));
		}
		DripCoinFlyout->OnStreamCoinArrived.BindUObject(this, &UCityCompanyManageWidget::HandleStreamCoinArrived);
	}

	// Money 아이콘 텍스처 1회 로드 캐시 (OfficeLayer CachedCoinTexture 패턴)
	if (!CachedCoinTexture)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TM = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bOk = false;
				const FResourceInfo Info = TM->GetResourceInfo(EResourceType::Money, bOk);
				if (bOk && !Info.Icon.IsNull())
				{
					CachedCoinTexture = Info.Icon.LoadSynchronous();
				}
			}
		}
	}

	// 로고 중심 → 누적 회수 칩 중심 (Absolute 좌표 — 로컬 변환은 CoinFlyout 내부 담당)
	const FGeometry& LogoGeo = LogoOverlay->GetCachedGeometry();
	const FGeometry& ChipGeo = Chip2Border->GetCachedGeometry();
	const FVector2D StartAbs = LogoGeo.LocalToAbsolute(LogoGeo.GetLocalSize() * 0.5f);
	const FVector2D TargetAbs = ChipGeo.LocalToAbsolute(ChipGeo.GetLocalSize() * 0.5f);
	DripCoinFlyout->SpawnStreamCoin(StartAbs, TargetAbs, CachedCoinTexture);
}

void UCityCompanyManageWidget::HandleStreamCoinArrived()
{
	if (Text_Earned)
	{
		EarnedPunch.Start(1.16f, 0.3f);
	}

	const int64 Delta = PendingLandingDelta;
	PendingLandingDelta = 0;
	if (Delta <= 0) { return; }

	// 레이어 머니바 착지 — 카운트업 롤 + 펀치 + "+N" 플로팅
	if (UUIManagerSubsystem* UIM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		if (UInGameLayerWidget* Layer = UIM->GetInGameLayer())
		{
			Layer->PlayMoneyDripLanding(Delta);
		}
	}
}

void UCityCompanyManageWidget::HandleDemolish()
{
	UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!AcqMgr) { return; }

	const EAcqState CurState = AcqMgr->GetState(CompanyKey);

	// 회수가 끝났으면(Depleted) 포기할 게 없다 — 되물을 이유도 없다
	if (CurState == EAcqState::Depleted)
	{
		AcqMgr->Demolish(CompanyKey);
		DeactivateWidget();
		return;
	}

	// 회수 중 철거는 되돌릴 수 없고 남은 회수를 버린다 — 한 번 되묻는다
	if (CurState == EAcqState::Milking)
	{
		ShowDemolishConfirm();
	}
}

void UCityCompanyManageWidget::ShowDemolishConfirm()
{
	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!UIMgr || !UIMgr->GetUIBase() || !TableMgr || !AcqMgr) { return; }

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::ConfirmCancel);
	if (!Cls) { return; }

	UConfirmCancelWidget* Confirm = Cast<UConfirmCancelWidget>(UIMgr->GetUIBase()->PushPromptClass(Cls.Get()));
	if (!Confirm) { return; }

	// 프롬프트 스택은 위젯 풀 재사용 — 이전 소비자의 잔류 바인딩을 지우지 않으면 확인 1회에 핸들러가 N번 돈다.
	// ⚠ 버튼 표시 상태도 잔류한다: SetConfirmOnly/SetCancelOnly 는 버튼 Visibility 를 영구히 바꾸므로
	//    앞선 소비자가 켜 뒀으면 이 모달에 취소 버튼이 없는 채로 뜬다. 소유 시점에 명시적으로 되돌린다.
	Confirm->OnConfirm.Clear();
	Confirm->OnCancel.Clear();
	Confirm->SetConfirmOnly(false);
	Confirm->SetCancelOnly(false);

	// 잔여 회수액(RRemaining)은 비공개다 — "얼마가 사라지는지" 는 말하지 않고 "사라진다" 까지만.
	const int64 Pot = AcqMgr->GetPotAmount(CompanyKey);
	Confirm->SetTitle(NSLOCTEXT("CityAcq", "DemolishTitle", "철거"));
	Confirm->SetMessage(Pot > 0
		? FText::Format(NSLOCTEXT("CityAcq", "DemolishMsg", "지금 철거하면 {0}을 받고 남은 회수는 사라집니다."),
			UGlobalUtilFunctions::AbbreviateNumber(Pot))
		: NSLOCTEXT("CityAcq", "DemolishMsgEmpty", "아직 회수된 금액이 없습니다. 지금 철거하면 아무것도 받지 못합니다."));
	Confirm->SetConfirmButtonText(NSLOCTEXT("CityAcq", "DemolishConfirm", "철거"));
	Confirm->SetCancelButtonText(NSLOCTEXT("CityAcq", "DemolishCancel", "취소"));

	Confirm->OnConfirm.AddUObject(this, &UCityCompanyManageWidget::HandleDemolishConfirmed);
}

void UCityCompanyManageWidget::HandleSkipRecovery()
{
	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!UIMgr || !UIMgr->GetUIBase() || !TableMgr || !AcqMgr) { return; }
	if (AcqMgr->GetState(CompanyKey) != EAcqState::Milking) { return; }

	// 보석 부족은 버튼이 먼저 삼키고 부족액 토스트까지 띄운다(UCostActionButtonWidget::ShouldRejectInput).
	// 여기 도달했다는 건 통과했다는 뜻이라, 방어만 하고 알림은 중복 발행하지 않는다.
	const int32 Cost = AcqMgr->GetSkipDiamondCost(CompanyKey);
	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	if (ResMgr && !ResMgr->HasResource(EResourceType::Diamond, Cost)) { return; }

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::ConfirmCancel);
	if (!Cls) { return; }
	UConfirmCancelWidget* Confirm = Cast<UConfirmCancelWidget>(UIMgr->GetUIBase()->PushPromptClass(Cls.Get()));
	if (!Confirm) { return; }

	// 풀 재사용 리셋 — 바인딩뿐 아니라 버튼 표시 상태(SetConfirmOnly/SetCancelOnly)도 잔류한다
	Confirm->OnConfirm.Clear();
	Confirm->OnCancel.Clear();
	Confirm->SetConfirmOnly(false);
	Confirm->SetCancelOnly(false);

	// 한 줄로 끝낸다 — 파는 것이 시간뿐이라 되물을 위험 자체가 없다.
	Confirm->SetTitle(NSLOCTEXT("CityAcq", "SkipTitle", "즉시 완료"));
	Confirm->SetMessage(FText::Format(
		NSLOCTEXT("CityAcq", "SkipMsg", "보석 {0}개로 남은 회수를 한 번에 진행합니다."),
		FText::AsNumber(Cost)));
	Confirm->SetConfirmButtonText(NSLOCTEXT("CityAcq", "SkipConfirm", "즉시 완료"));
	Confirm->SetCancelButtonText(NSLOCTEXT("CityAcq", "SkipCancel", "취소"));

	Confirm->OnConfirm.AddUObject(this, &UCityCompanyManageWidget::HandleSkipConfirmed);
}

void UCityCompanyManageWidget::HandleSkipConfirmed()
{
	UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!AcqMgr) { return; }

	// 확인~클릭 사이 상태/잔액 변동은 매니저가 다시 검사한다 (보석 차감도 거기서)
	if (!AcqMgr->SkipRecovery(CompanyKey))
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			UIMgr->ShowRejectNotification(NSLOCTEXT("CityAcq", "SkipFailed", "지금은 즉시 완료할 수 없습니다"));
		}
		return;
	}
	// 패널은 열어 둔다 — 100% 로 찬 링을 보고 철거(수령)를 누르는 게 다음 행동이다.
	// 표시 갱신은 SkipRecovery 가 브로드캐스트한 OnCompanyProgressChanged 가 처리한다.
}

void UCityCompanyManageWidget::HandleDemolishConfirmed()
{
	// 확인을 누르기까지 사이에 고갈로 바뀌었어도 Demolish 는 양쪽 상태에서 유효하다
	if (UCityAcquisitionManager* AcqMgr = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr)
	{
		AcqMgr->Demolish(CompanyKey);
	}
	DeactivateWidget();
}

void UCityCompanyManageWidget::HandleCloseClicked()
{
	DeactivateWidget();
}

void UCityCompanyManageWidget::HandleBackgroundClicked()
{
	DeactivateWidget();
}
