#include "UI/Panel/CityCompanyInfoWidget.h"
#include "CommonTextBlock.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/UISoundTags.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Table/CityCompanyData.h"
#include "Enum/ResourceType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"

namespace
{
	// 라이트 플레이트 잉크 (#232C39 본문 / #46566A 보조 / #5A6B7D 캡션) — 전부 linear
	const FLinearColor CardInkColor(0.016795f, 0.025334f, 0.041069f, 1.f);
	const FLinearColor CardInkBodyColor(0.061254f, 0.093067f, 0.144161f, 1.f);
	const FLinearColor CardInkCapColor(0.102263f, 0.147063f, 0.205079f, 1.f);

	// 팔레트 레드 #C22B2E — 위치로 뜻이 갈린다(카드 안=손해 가능 / 카드 밖=자금 부족)
	const FLinearColor PaletteRedColor(0.539424f, 0.024301f, 0.027475f, 1.f);

	// 착지 골드 #B8770E — 라이트 플레이트 대비 3.47:1 (구 #C98A18 은 2.95:1 로 폐기)
	const FLinearColor LandGoldColor(0.479320f, 0.184475f, 0.004391f, 1.f);

	constexpr float RevealCountUpSeconds = 0.9f;
	constexpr float RevealPunchScale = 1.12f;
	constexpr float RevealPunchSeconds = 0.22f;
}

void UCityCompanyInfoWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자인 타임에 MessageText 숨김(ContentSlot 사용 패턴)
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->HideMessageText();
	}
}

void UCityCompanyInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->bAutoRemove = false;
		ConfirmCancelWidget->HideMessageText();
		ConfirmCancelWidget->SetTitle(FText::FromString(TEXT("회사 인수")));
		ConfirmCancelWidget->SetConfirmButtonText(FText::FromString(TEXT("인수")));
		ConfirmCancelWidget->SetCancelButtonText(FText::FromString(TEXT("취소")));
		// SetCancelOnly 호출 금지 — 확인·취소 둘 다 노출
		ConfirmCancelWidget->OnConfirm.AddUObject(this, &UCityCompanyInfoWidget::HandleAcquireConfirmed);
		ConfirmCancelWidget->OnCancel.AddUObject(this, &UCityCompanyInfoWidget::HandleCancelled);
	}
}

void UCityCompanyInfoWidget::NativeDestruct()
{
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->OnConfirm.RemoveAll(this);
		ConfirmCancelWidget->OnCancel.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UCityCompanyInfoWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 모달이 열리는 순간 UI 모드 전환 — 닫힘 시 NativeOnDeactivated 가 Normal 복원
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
		{
			PC->GoToUIMode();
		}
	}
}

void UCityCompanyInfoWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// 관리 패널로 잇는 체인이면 Push 만 하고 GoToNormalMode 는 건너뜀 — 관리 패널 NativeOnActivated 가 다시 GoToUIMode 를 불러
	// Normal→UI 로 한 프레임 튀며 그 사이 월드 입력이 열리는 사고(스치는 탭이 뒤 빌딩에 꽂힘)를 막는다.
	// 이 생략이 안전한 전제: PromptStack 에 이 모달 아래 다른 위젯이 없다 — OnCompanyClicked 의
	// GetPromptStackCount()>0 가드가 그걸 보장한다(가드가 없어지면 이 분기는 조용히 회귀한다).
	if (bPendingManagePanelOpen)
	{
		bPendingManagePanelOpen = false;
		if (UCityAcquisitionManager* Acq = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr)
		{
			Acq->OpenManagePanel(CompanyKey);
		}
		return;
	}

	// 연속으로 다른 모달이 열리는 경우가 아니면 UI 모드 해제
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
		{
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}
}

void UCityCompanyInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 리빌 애니메이션 구동 전용 — 끝나면 아무 상태도 만지지 않는다(데이터 폴링 아님)
	if (!bRevealRunning)
	{
		return;
	}

	if (RevealCountUp.IsPlaying())
	{
		const float Alpha = RevealCountUp.Tick(InDeltaTime);
		const double Span = double(RevealToAmount) - double(RevealFromAmount);
		ApplyReturnAmount(RevealFromAmount + int64(Span * double(Alpha)));

		if (!RevealCountUp.IsPlaying())
		{
			ApplyReturnAmount(RevealToAmount);
			HandleRevealLanded();
		}
		return;
	}

	if (RevealPunch.IsPlaying())
	{
		const float PunchValue = RevealPunch.Tick(InDeltaTime);
		if (RevealPunchTarget)
		{
			RevealPunchTarget->SetRenderScale(FVector2D(PunchValue, PunchValue));
		}

		if (!RevealPunch.IsPlaying())
		{
			if (RevealPunchTarget)
			{
				RevealPunchTarget->SetRenderScale(FVector2D(1.f, 1.f));
			}
			bRevealRunning = false;
			HandleRevealFinished();
		}
		return;
	}

	// 어느 채널도 안 돌면 리빌이 끝난 것 — 안전망
	bRevealRunning = false;
	HandleRevealFinished();
}

UWidget* UCityCompanyInfoWidget::GetAcquireButtonWidget() const
{
	return ConfirmCancelWidget ? ConfirmCancelWidget->GetConfirmButtonWidget() : nullptr;
}

void UCityCompanyInfoWidget::ConfigureForCompany(int32 InKey)
{
	CompanyKey = InKey;

	// 위젯 풀 재사용 리셋 — 안 하면 다음 회사 모달이 열리자마자 Confirm 이 "확인" 라벨로 뜨거나
	// 첫 클릭이 곧장 관리 패널로 빠지고, Cancel 이 이전 회사에서 잠기거나 숨겨진 채로 남는다.
	bAwaitingResultConfirm = false;
	bPendingManagePanelOpen = false;
	bRevealRunning = false;
	RevealCountUp.Finish();
	if (RevealPunchTarget)
	{
		RevealPunchTarget->SetRenderScale(FVector2D(1.f, 1.f)); // 이전 회사의 펀치 스케일이 남으면 숫자가 커진 채로 뜬다
	}
	RevealPunchTarget = nullptr;

	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->SetConfirmButtonText(FText::FromString(TEXT("인수")));
		ConfirmCancelWidget->SetConfirmOnly(false); // 리빌 때 숨긴 Cancel 을 되살림
		if (UWidget* CancelW = ConfirmCancelWidget->GetCancelButtonWidget())
		{
			CancelW->SetIsEnabled(true);
		}
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI) { return; }

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) { return; }

	FCityCompanyData D;
	if (!TableMgr->GetCityCompanyData(InKey, D)) { return; }

	bool bCompanyOk = false;
	const FCompanyInfoTable CompanyInfo = TableMgr->GetCompanyInfo(D.Industry, bCompanyOk);

	if (Text_CompanyName)
	{
		Text_CompanyName->SetText(FText::FromString(D.GetDisplayName()));
	}

	// 산업명 = DT_CompanyInfo.DisplayName 단일 진실. 구 enum DisplayName 조회는 UMETA 가 에디터 전용이라
	// 패키징 빌드에서 "Game"/"Finance" 로 떨어졌다. 미등록이면 비우고 경고(코드 폴백 금지).
	if (Chip_Industry)
	{
		const FText IndustryName = bCompanyOk ? CompanyInfo.DisplayName : FText::GetEmpty();
		if (IndustryName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[CityCompanyInfo] DT_CompanyInfo DisplayName 없음 (Industry=%d)"), static_cast<int32>(D.Industry));
		}
		Chip_Industry->SetText(IndustryName);
	}

	if (Image_CompanyIcon)
	{
		UTexture2D* IconTex = D.CompanyIcon.IsNull() ? nullptr : D.CompanyIcon.LoadSynchronous();
		FLinearColor IconTint = FLinearColor::White;

		// 로고 미등록 행(향후 확장분) 방어 — 산업 글리프로 대체
		if (!IconTex && bCompanyOk && !CompanyInfo.GlyphIcon.IsNull())
		{
			IconTex = CompanyInfo.GlyphIcon.LoadSynchronous();
			IconTint = CompanyInfo.AccentColor;
		}

		if (IconTex)
		{
			Image_CompanyIcon->SetBrushFromTexture(IconTex);
			Image_CompanyIcon->SetColorAndOpacity(IconTint);
			Image_CompanyIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_CompanyIcon->SetVisibility(ESlateVisibility::Collapsed);
		}

		// 아이콘도 글리프도 없으면 빈 근흑 사각만 남는다 — 로고 칩(SizeBox 조상)째 접는다
		UWidget* LogoChip = nullptr;
		for (UPanelWidget* Ancestor = Image_CompanyIcon->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
		{
			if (Ancestor->IsA<USizeBox>()) { LogoChip = Ancestor; break; }
		}
		if (LogoChip)
		{
			LogoChip->SetVisibility(IconTex ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	// 등급색은 표시되는 본전이상%(GetBreakEvenPct) 에서 파생 — 카드 윗변 라인에만 주입한다.
	// 금액에 칠하면 "이 돈이 위험하다" 로 읽힌다.
	const int32 BreakEven = D.GetBreakEvenPct();
	FLinearColor GradeColor;
	if      (BreakEven >= 90) { GradeColor = FLinearColor(0.027f,    0.342f,    0.105f,    1.f); }
	else if (BreakEven >= 75) { GradeColor = LandGoldColor; }
	else if (BreakEven >= 50) { GradeColor = FLinearColor(0.644f,    0.141f,    0.013f,    1.f); }
	else                      { GradeColor = FLinearColor(0.540f,    0.024f,    0.027f,    1.f); }

	if (Image_GradeLine)
	{
		Image_GradeLine->SetColorAndOpacity(GradeColor);
	}

	if (Resource_Cost)
	{
		Resource_Cost->AmountRoundMode = ENumberRoundMode::Ceil; // 비용은 올림 표시
		Resource_Cost->SetResourceType(EResourceType::Money, /*bUseTypeColor=*/false); // DT 초록은 다크 HUD 전제 — 라이트 카드에선 인스턴스 잉크색 사용
		Resource_Cost->SetValue(D.AcquisitionCost);
	}

	if (Text_ReturnLabel)
	{
		Text_ReturnLabel->SetText(FText::FromString(TEXT("예상 회수")));
	}

	ApplyReturnAmount(D.GetExpectedReturn());
	if (Text_ReturnValue) { Text_ReturnValue->SetColorAndOpacity(FSlateColor(CardInkColor)); }
	if (Text_ReturnUnit)  { Text_ReturnUnit->SetColorAndOpacity(FSlateColor(CardInkColor)); }

	const int64 MinReturn = D.GetMinReturn();
	// 손해 가능(최소 회수 < 인수 비용)은 최소 쪽 블록에만 칠한다 — 최대값은 이 딜의 가장 좋은 결과라
	// 경고색이 물들면 뜻이 뒤집힌다. CommonTextBlock 에 런별 색이 없어 블록을 둘로 나눠 뒀다.
	if (Text_MinPart)
	{
		Text_MinPart->SetText(FText::FromString(FString::Printf(TEXT("최소 %s"),
			*UGlobalUtilFunctions::AbbreviateNumber(MinReturn).ToString())));
		Text_MinPart->SetColorAndOpacity(FSlateColor(MinReturn < D.AcquisitionCost ? PaletteRedColor : CardInkBodyColor));
		Text_MinPart->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // 인스턴스 재사용 시 리빌의 Collapsed 해제
	}
	if (Text_MaxPart)
	{
		Text_MaxPart->SetText(FText::FromString(FString::Printf(TEXT(" · 최대 %s"),
			*UGlobalUtilFunctions::AbbreviateNumber(D.GetMaxReturn()).ToString())));
		Text_MaxPart->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_EstTime)
	{
		Text_EstTime->SetText(FText::FromString(FString::Printf(TEXT("약 %s에 걸쳐 들어옵니다"),
			*UGlobalUtilFunctions::FormatEstimateDurationKorean(D.GetExpectedSeconds()).ToString())));
	}

	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	const int64 OwnedMoney = ResMgr ? ResMgr->GetResourceAmount(EResourceType::Money) : 0;
	const bool bShortOnMoney = ResMgr && OwnedMoney < D.AcquisitionCost; // 매니저가 없으면 "부족"으로 단정하지 않는다

	if (Text_Owned)
	{
		// 카드 밖 레드 = "못 산다". 얼마 모자란지까지 말한다 — 상단 HUD 보유액은 딤+블러 아래라 안 보인다.
		Text_Owned->SetText(FText::FromString(bShortOnMoney
			? FString::Printf(TEXT("보유 %s ― %s 부족"),
				*UGlobalUtilFunctions::AbbreviateNumber(OwnedMoney).ToString(),
				*UGlobalUtilFunctions::AbbreviateNumber(D.AcquisitionCost - OwnedMoney, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil).ToString())
			: FString::Printf(TEXT("보유 %s"), *UGlobalUtilFunctions::AbbreviateNumber(OwnedMoney).ToString())));
		Text_Owned->SetColorAndOpacity(FSlateColor(bShortOnMoney ? PaletteRedColor : CardInkCapColor));
	}

	// 인수 게이트 — 확인 버튼 상태
	if (ConfirmCancelWidget)
	{
		UCityAcquisitionManager* Acq = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
		const bool bCanAcquire = Acq && Acq->CanAcquire(InKey);

		if (UButtonWidget* ConfirmBtn = Cast<UButtonWidget>(ConfirmCancelWidget->GetConfirmButtonWidget()))
		{
			// 자금만 모자란 거면 버튼을 살려 둔다 — 비활성이면 왜 못 사는지 말할 기회가 없다.
			// 이미 인수됨/DT 행 없음 같은 구조적 불가는 그대로 비활성.
			ConfirmBtn->SetIsEnabled(bCanAcquire || bShortOnMoney); // 재호출 대비 매번 명시적 토글
			if (!bCanAcquire && bShortOnMoney)
			{
				ConfirmBtn->SetRejectInsufficient(EResourceType::Money, D.AcquisitionCost);
			}
			else
			{
				ConfirmBtn->ClearRejectReason();
			}
		}
	}
}

void UCityCompanyInfoWidget::ApplyReturnAmount(int64 Amount)
{
	FString NumberPart;
	FString UnitPart;
	UGlobalUtilFunctions::SplitAbbreviatedNumber(Amount, NumberPart, UnitPart);

	if (Text_ReturnValue)
	{
		Text_ReturnValue->SetText(FText::FromString(NumberPart));
	}
	if (Text_ReturnUnit)
	{
		Text_ReturnUnit->SetText(FText::FromString(UnitPart));
		Text_ReturnUnit->SetVisibility(UnitPart.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UCityCompanyInfoWidget::HandleAcquireConfirmed()
{
	// 리빌 완료 후 Confirm 이 "확인" 으로 바뀐 상태의 클릭 — 관리 패널로 전환.
	// 닫기(DeactivateWidget)를 먼저 호출하고 push 는 NativeOnDeactivated 에서 — 순서를 바꾸면 안 된다.
	if (bAwaitingResultConfirm)
	{
		bAwaitingResultConfirm = false;
		bPendingManagePanelOpen = true;
		DeactivateWidget();
		return;
	}

	UCityAcquisitionManager* Acq = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!Acq || !Acq->Acquire(CompanyKey))
	{
		// Acquire 내부 CanAcquire 재검사에서 탈락. 조용히 닫으면 인수된 줄 알게 되므로
		// 사유를 알리고 창은 열어 둔다 (자금이 채워지면 그대로 다시 누를 수 있다).
		if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			UIMgr->ShowRejectNotification(
				NSLOCTEXT("CityCompanyInfo", "AcquireFailed", "지금은 인수할 수 없습니다"));
		}
		return;
	}

	// 굴린 회수액 역산 — 인수 직후라 Earned=0 이므로 Remaining 이 곧 총액이다.
	// 원 굴림값이 아니라 RTotal 에서 뽑는다: 플레이어가 실제로 받는 값이라 이쪽이 정확하다.
	int64 RolledTotal = 0;
	RevealPct = 0;
	int64 Cost = 0, Earned = 0, Remaining = 0;
	bool bDepleted = false;
	if (Acq->GetCompanyProgress(CompanyKey, Cost, Earned, Remaining, bDepleted) && Cost > 0)
	{
		RolledTotal = Earned + Remaining;
		RevealPct = FMath::RoundToInt(RolledTotal * 100.0 / static_cast<double>(Cost));
	}

	// 리빌 중 중복 입력 차단 + Cancel 은 아예 숨김 — 인수가 이미 끝나 취소할 대상이 없고, 결과 확인 화면에 버튼은 "확인" 하나여야 한다
	if (ConfirmCancelWidget)
	{
		if (UWidget* ConfirmW = ConfirmCancelWidget->GetConfirmButtonWidget())
		{
			ConfirmW->SetIsEnabled(false);
		}
		ConfirmCancelWidget->SetConfirmOnly(true);
	}

	if (RolledTotal > 0)
	{
		// 예상 회수 → 실제 회수 카운트업. 시작값은 화면에 이미 떠 있는 예상 회수액이라
		// 숫자가 "예상에서 실제로 굴러가는" 것으로 읽힌다.
		FCityCompanyData D;
		UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		RevealFromAmount = (TableMgr && TableMgr->GetCityCompanyData(CompanyKey, D)) ? D.GetExpectedReturn() : RolledTotal;
		RevealToAmount = RolledTotal;

		if (Text_ReturnLabel) { Text_ReturnLabel->SetText(FText::FromString(TEXT("실제 회수"))); }
		// 결과가 나왔으므로 구간은 무의미
		if (Text_MinPart) { Text_MinPart->SetVisibility(ESlateVisibility::Collapsed); }
		if (Text_MaxPart) { Text_MaxPart->SetVisibility(ESlateVisibility::Collapsed); }

		bRevealRunning = true;
		RevealCountUp.Start(0.f, 1.f, RevealCountUpSeconds); // 알파만 애니 — 실값은 int64 보간
		return; // 닫기는 확인 클릭 후
	}

	HandleRevealFinished();
}

void UCityCompanyInfoWidget::HandleRevealLanded()
{
	// 본전 이상이면 골드, 미만이면 레드 — 카드 안에서 이 색이 붙는 유일한 순간(결과 확정)
	const FLinearColor LandColor = (RevealPct >= 100) ? LandGoldColor : PaletteRedColor;
	if (Text_ReturnValue) { Text_ReturnValue->SetColorAndOpacity(FSlateColor(LandColor)); }
	if (Text_ReturnUnit)  { Text_ReturnUnit->SetColorAndOpacity(FSlateColor(LandColor)); }

	// 펀치는 코인+숫자+단위를 묶은 행에 건다 — 개별 텍스트를 각자 스케일하면 사이가 벌어진다
	RevealPunchTarget = Text_ReturnValue;
	if (Text_ReturnValue && Text_ReturnValue->GetParent())
	{
		RevealPunchTarget = Text_ReturnValue->GetParent();
	}
	RevealPunch.Start(RevealPunchScale, RevealPunchSeconds);

	// 착지 성패음 — 인수=투자 결과라 돈 계열: 본전 이상 성취음, 미만 은은한 경고
	if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
	{
		SoundMgr->PlayUISound(RevealPct >= 100 ? CGUISoundTags::RewardCompanyLevelUp : CGUISoundTags::NotificationWarning);
	}
}

void UCityCompanyInfoWidget::HandleRevealFinished()
{
	// 인수 후 갈 곳은 항상 관리 패널 — 결과를 띄운 채 대기하다 확인 클릭 시 전환한다.
	// 역산이 실패해 리빌을 못 켠 경우도 이 경로로 합류 — 리빌만 생략될 뿐 폴백 목적지는 동일하다.
	bAwaitingResultConfirm = true;
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->SetConfirmButtonText(FText::FromString(TEXT("확인")));
		if (UWidget* ConfirmW = ConfirmCancelWidget->GetConfirmButtonWidget())
		{
			ConfirmW->SetIsEnabled(true); // Cancel 은 리빌 시작 때 SetConfirmOnly 로 숨겨진 상태 유지
		}
	}
}

void UCityCompanyInfoWidget::HandleCancelled()
{
	DeactivateWidget();
}
