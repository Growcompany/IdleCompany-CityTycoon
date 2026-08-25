#include "UI/HUD/MissionTrackerWidget.h"
#include "UI/HUD/MissionTrackerDisplayRules.h"

#include "CommonTextBlock.h"
#include "TimerManager.h"
#include "Animation/WidgetAnimation.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

#include "Manager/MissionManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Table/ShopItemTable.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Common/RewardChipUtils.h"

namespace
{
	// 배지 색 = 미션 상태 표시 (코드 소유): 진행 = 앰버 / 완료 순간 = 그린.
	// WBP 계약: "Badge_Outer"/"Badge_Core" UImage — 브러시는 흰색, 색은 ColorAndOpacity로만 입힘
	const FLinearColor BadgeAmber(1.f, 0.723055f, 0.194618f, 1.f);
	const FLinearColor BadgeGreen = FLinearColor::FromSRGBColor(FColor(0x22, 0xC5, 0x5E));

	void ApplyBadgeColor(UUserWidget& Owner, const FLinearColor& Color)
	{
		static const FName BadgeNames[] = { TEXT("Badge_Outer"), TEXT("Badge_Core") };
		for (const FName& Name : BadgeNames)
		{
			if (UImage* Badge = Cast<UImage>(Owner.GetWidgetFromName(Name)))
			{
				Badge->SetColorAndOpacity(Color);
			}
		}
	}
}

void UMissionTrackerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 빈 WBP(헤드리스 자동화 생성) 폴백 — RenderMission 등 기존 로직 동작 전에 멤버 확보
	BuildFallbackTree();

	// InitTracker 가 NativeConstruct 보다 먼저 불린 경우(오피스→MainMap 복귀 시 매니저가 임베드 트래커를
	// 즉시 잡아 InitTracker→RefreshFromManager 로 이미 표시해 둔 타이밍) 여기서 무조건 Collapsed 하면
	// 방금 그린 활성 트래커를 도로 숨겨버린다 → Manager 가 이미 묶였으면 현재 상태를 다시 반영한다.
	// (Manager 없음 = 아직 init 전 → 디자인타임 플레이스홀더 노출 방지 위해 숨김 유지)
	if (Manager.IsValid())
	{
		RefreshFromManager();
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMissionTrackerWidget::BuildFallbackTree()
{
	// WBP 에 동명 위젯이 하나라도 배치돼 있으면 디자이너 오버라이드를 존중하고 손대지 않는다.
	if (MissionTitleText || MentorLineText || ProgressText || RewardText)
	{
		return;
	}
	if (!WidgetTree)
	{
		return;
	}

	// === 헤드리스 폴백 트리 — 웜크림&주황 V1 정식 토큰 적용 ===
	// (UI_CHROME_WARM_CREAM_SPEC.md / UI_STYLE_CATALOG.md / UI_TYPOGRAPHY.md 기준)
	// 자원(보더 텍스처/폰트)은 TSoftObjectPtr+LoadSynchronous 로드. 못 찾으면 해당 항목만 폴백(soft-fail).

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = RootCanvas;
	}

	// 웜크림 토큰 (sRGB hex -> linear, FColor::FromHex 는 sRGB 바이트값)
	const FLinearColor PanelCream  = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FEFCE8"))); // Panel FillTop
	const FLinearColor InkPrimary  = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4B2E2B"))); // CUI_Text_OnCream
	const FLinearColor InkSub      = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8C5A3C"))); // CUI_Text_OnCream_Sub
	const FLinearColor AccentOrange = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F59E0B"))); // 시그니처 주황 (진행도)

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrackerCardBorder"));

	// 라운드 보더 텍스처(흰 베이스 9-slice)를 Box 브러시로 구성 -> 크림 틴트(곱셈).
	// 다크 베이스 Border_Round25 는 크림 곱셈 시 칙칙해지므로 흰 베이스 White 변형 사용.
	TSoftObjectPtr<UTexture2D> BorderTexPtr(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/UITextures/Border/BorderWhite_Round25.BorderWhite_Round25")));
	if (UTexture2D* BorderTex = BorderTexPtr.LoadSynchronous())
	{
		FSlateBrush CardBrush;
		CardBrush.SetResourceObject(BorderTex);
		CardBrush.DrawAs = ESlateBrushDrawType::Box;
		// 530px 텍스처 / 코너 반지름 ~32px => 9-slice 마진 0.06 (스펙 §4 UpgradeSlot 림 동일값)
		CardBrush.Margin = FMargin(0.06f);
		CardBrush.TintColor = FSlateColor(PanelCream);
		Card->SetBrush(CardBrush);
	}
	else
	{
		// 텍스처 로드 실패 시 단색 크림으로 soft-fail (사각이지만 색은 정식 토큰)
		Card->SetBrushColor(PanelCream);
	}
	// 라운드 코너 자원 안쪽으로 콘텐츠가 들어가도록 패딩 확보
	Card->SetPadding(FMargin(22.f, 16.f));

	if (UCanvasPanelSlot* CardSlot = RootCanvas->AddChildToCanvas(Card))
	{
		// 좌하단 앵커 고정 — 화면 크기와 무관하게 좌하단에서 (40, -180) 위로 띄움
		CardSlot->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
		CardSlot->SetAlignment(FVector2D(0.f, 1.f));
		CardSlot->SetPosition(FVector2D(40.f, -180.f));
		CardSlot->SetAutoSize(true);
	}

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TrackerVBox"));
	Card->SetContent(Box);

	// 타이포 SOT 폰트 소프트 로드 (UFont). NEXON 은 굵기별 UFont(typeface="Default"),
	// Pretendard 는 F_Pretendard 하나에 4 typeface(SemiBold 등). 못 찾으면 null -> 텍스트는 표시되되 폴백.
	UFont* NexonBold = TSoftObjectPtr<UFont>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"))).LoadSynchronous();
	UFont* NexonRegular = TSoftObjectPtr<UFont>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();
	UFont* Pretendard = TSoftObjectPtr<UFont>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/Font/Pretendard/F_Pretendard.F_Pretendard"))).LoadSynchronous();

	// 코드 생성 텍스트는 멤버 타입(TObjectPtr<UCommonTextBlock>)에 맞춰 반드시 UCommonTextBlock 으로 생성.
	// FontObject 가 null 이면 SetFont 시 텍스트가 사라지는 함정 회피 -> 폰트 로드 실패 시 SetFont 생략(스타일 기본값 유지).
	auto MakeText = [&](FName WidgetName, UFont* FontObj, FName Typeface, int32 FontSize, const FLinearColor& Color, float TopPad) -> UCommonTextBlock*
	{
		UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), WidgetName);

		if (FontObj)
		{
			FSlateFontInfo Font(FontObj, FontSize);
			Font.TypefaceFontName = Typeface;
			Text->SetFont(Font);
		}
		Text->SetColorAndOpacity(FSlateColor(Color));

		if (UVerticalBoxSlot* BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		}
		return Text;
	};

	// 제목=NEXON Bold 26 / 문구=NEXON Regular 17 / 진행도=Pretendard SemiBold 22 / 보상=NEXON Regular 15
	MissionTitleText = MakeText(TEXT("MissionTitleText"), NexonBold, TEXT("Default"), 26, InkPrimary, 0.f);
	MentorLineText   = MakeText(TEXT("MentorLineText"), NexonRegular, TEXT("Default"), 17, InkSub, 6.f);
	ProgressText     = MakeText(TEXT("ProgressText"), Pretendard, TEXT("SemiBold"), 22, AccentOrange, 6.f);
	RewardText       = MakeText(TEXT("RewardText"), NexonRegular, TEXT("Default"), 15, InkSub, 8.f);
}

void UMissionTrackerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 집중 모드 좌측 슬라이드 — 개발중이면 화면 밖으로, 아니면 제자리로 보간(+페이드)
	{
		const float Target = bFocusHidden ? 1.f : 0.f;
		if (!FMath::IsNearlyEqual(FocusSlideAlpha, Target, 0.001f))
		{
			FocusSlideAlpha = FMath::FInterpTo(FocusSlideAlpha, Target, InDeltaTime, FocusSlideSpeed);
			SetRenderTranslation(FVector2D(-FocusSlideAlpha * FocusSlideDistance, 0.f));
			SetRenderOpacity(1.f - FocusSlideAlpha);
		}
	}

	// 클레임 대기 강조 — 글린트 반복 스윕 + 카드 전체 펄스 + 초록/골드 외곽선 + 배경 틴트 전환 (탭 유도)
	if (bAwaitingClaim)
	{
		ClaimPulseTime += InDeltaTime;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(ClaimPulseTime * (2.f * PI / 1.1f));

		// 외곽선: 초록↔골드 시머 + 강한 알파 브리딩
		if (UImage* Outline = CardOutline.Get())
		{
			FLinearColor C = FMath::Lerp(BadgeGreen, BadgeAmber, Pulse);
			C.A = 0.55f + 0.45f * Pulse;
			Outline->SetColorAndOpacity(C);
		}

		// 배지 숨쉬기 (시선 유도)
		if (UWidget* Icon = MissionIconWidget.Get())
		{
			const float S = 1.f + 0.06f * Pulse;
			Icon->SetRenderScale(FVector2D(S, S));
		}

		// 배경 틴트: 흰색 → 연한 초록/골드 (완료를 색으로 즉시 읽히게)
		if (UImage* Bg = CardBgImage.Get())
		{
			const FLinearColor Tint = FMath::Lerp(FLinearColor::White,
				FLinearColor(0.78f, 1.f, 0.55f, 1.f), 0.18f + 0.18f * Pulse);
			Bg->SetColorAndOpacity(Tint);
		}

		// 글린트: 대기 내내 주기적 스윕 (강한 빛 플래시 반복) — 외곽선과 desync 위해 별도 주기
		if (UImage* Glint = CardGlint.Get())
		{
			const float P = FMath::Fmod(ClaimPulseTime, ClaimGlintPeriod) / ClaimGlintPeriod;
			float GA;
			if (P < 0.35f)      { GA = P / 0.35f; }                  // 빠른 상승
			else if (P < 0.8f)  { GA = 1.f - (P - 0.35f) / 0.45f; }  // 부드러운 하강
			else                { GA = 0.f; }                        // 짧은 암전
			FLinearColor C = Glint->GetColorAndOpacity();
			C.A = GA;
			Glint->SetColorAndOpacity(C);
		}
	}

	// 평소(연출 비활성)에는 즉시 빠져나간다 — HUD 상주 위젯이라 매 프레임 도는 비용 차단
	if (!bGlowActive)
	{
		return;
	}

	GlowElapsed += InDeltaTime;

	const float T = GlowElapsed;
	if (T >= GlowDuration)
	{
		StopCompleteGlow();
		return;
	}

	// --- CardGlint: 0 -> (RiseEnd 까지 선형) Peak -> (Duration 까지 ease-out) 0 ---
	float GlintAlpha;
	if (T <= GlintRiseEnd)
	{
		GlintAlpha = GlintPeak * (T / GlintRiseEnd);
	}
	else
	{
		// ease-out: 남은 구간을 정규화한 뒤 (1-x)^2 로 부드럽게 0 으로 감쇠
		const float Fall = (T - GlintRiseEnd) / (GlowDuration - GlintRiseEnd);
		const float Eased = (1.f - Fall) * (1.f - Fall);
		GlintAlpha = GlintPeak * Eased;
	}

	// --- CardOutline: Base(0.15) -> (RiseEnd 까지 선형) Peak(0.7) -> (Duration 까지 선형) Base ---
	float OutlineAlpha;
	if (T <= OutlineRiseEnd)
	{
		const float Rise = T / OutlineRiseEnd;
		OutlineAlpha = FMath::Lerp(OutlineBase, OutlinePeak, Rise);
	}
	else
	{
		const float Fall = (T - OutlineRiseEnd) / (GlowDuration - OutlineRiseEnd);
		OutlineAlpha = FMath::Lerp(OutlinePeak, OutlineBase, Fall);
	}

	if (UImage* Glint = CardGlint.Get())
	{
		FLinearColor C = Glint->GetColorAndOpacity();
		C.A = GlintAlpha;
		Glint->SetColorAndOpacity(C);
	}
	if (UImage* Outline = CardOutline.Get())
	{
		FLinearColor C = Outline->GetColorAndOpacity();
		C.A = OutlineAlpha;
		Outline->SetColorAndOpacity(C);
	}

	// --- 완료 스파클: 시차 팝 (스케일 0→1.2→0.9 + 살짝 회전, 후반 페이드아웃) ---
	auto DriveSparkle = [T](const TWeakObjectPtr<UImage>& Img, float StartTime, float BaseAngle)
	{
		UImage* W = Img.Get();
		if (!W)
		{
			return;
		}
		const float L = (T - StartTime) / SparkleLife;
		if (L < 0.f || L > 1.f)
		{
			W->SetRenderOpacity(0.f);
			return;
		}
		const float S = (L < 0.4f) ? FMath::Lerp(0.f, 1.2f, L / 0.4f)
		                           : FMath::Lerp(1.2f, 0.9f, (L - 0.4f) / 0.6f);
		W->SetRenderScale(FVector2D(S, S));
		W->SetRenderTransformAngle(BaseAngle + L * 90.f);
		W->SetRenderOpacity(FMath::Min(L * 5.f, 1.f) * (1.f - FMath::Max(0.f, (L - 0.7f) / 0.3f)));
	};
	DriveSparkle(SparkleA, 0.f, 0.f);
	DriveSparkle(SparkleB, SparkleStagger, 30.f);
	DriveSparkle(SparkleC, SparkleStagger * 2.f, -20.f);
}

void UMissionTrackerWidget::StartCompleteGlow()
{
	// 전부 없으면 연출할 게 없음 (이름 계약 미충족) — 틱도 켜지 않음
	if (!CardGlint.Get() && !CardOutline.Get() && !SparkleA.Get() && !SparkleB.Get() && !SparkleC.Get())
	{
		return;
	}

	bGlowActive = true;
	GlowElapsed = 0.f;

	// 시작 프레임을 즉시 기본값으로 세팅 (Glint 0 / Outline 0.15) — 첫 틱 전 깜빡임 방지
	if (UImage* Glint = CardGlint.Get())
	{
		FLinearColor C = Glint->GetColorAndOpacity();
		C.A = 0.f;
		Glint->SetColorAndOpacity(C);
	}
	if (UImage* Outline = CardOutline.Get())
	{
		FLinearColor C = Outline->GetColorAndOpacity();
		C.A = OutlineBase;
		Outline->SetColorAndOpacity(C);
	}
}

void UMissionTrackerWidget::StopCompleteGlow()
{
	bGlowActive = false;
	GlowElapsed = 0.f;

	// 기본값 복원 (Glint 0 / Outline 0.15 / Sparkle 0)
	if (UImage* Glint = CardGlint.Get())
	{
		FLinearColor C = Glint->GetColorAndOpacity();
		C.A = 0.f;
		Glint->SetColorAndOpacity(C);
	}
	if (UImage* Outline = CardOutline.Get())
	{
		FLinearColor C = Outline->GetColorAndOpacity();
		C.A = OutlineBase;
		Outline->SetColorAndOpacity(C);
	}
	for (const TWeakObjectPtr<UImage>& Sparkle : { SparkleA, SparkleB, SparkleC })
	{
		if (UImage* W = Sparkle.Get())
		{
			W->SetRenderOpacity(0.f);
		}
	}
}

void UMissionTrackerWidget::NativeDestruct()
{
	if (UMissionManagerSubsystem* Mgr = Manager.Get())
	{
		Mgr->OnMissionActivated.RemoveAll(this);
		Mgr->OnMissionCompleted.RemoveAll(this);
		Mgr->OnGuidePhaseChanged.RemoveAll(this);
		Mgr->OnMissionProgressChanged.RemoveAll(this);
		Mgr->OnMissionReadyToClaim.RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CompleteTimerHandle);
	}

	bGlowActive = false;

	Super::NativeDestruct();
}

void UMissionTrackerWidget::InitTracker(UMissionManagerSubsystem* InManager)
{
	if (!InManager)
	{
		return;
	}

	// 재진입 방어 — 같은 인스턴스를 다시 init 하면 누적 구독 방지
	if (UMissionManagerSubsystem* Prev = Manager.Get())
	{
		Prev->OnMissionActivated.RemoveAll(this);
		Prev->OnMissionCompleted.RemoveAll(this);
		Prev->OnGuidePhaseChanged.RemoveAll(this);
		Prev->OnMissionProgressChanged.RemoveAll(this);
		Prev->OnMissionReadyToClaim.RemoveAll(this);
	}

	Manager = InManager;

	// 완료 글로우 폴백용 순수 WBP 위젯 캐시 (이름 계약). 없으면 null -> 글로우만 생략.
	CardGlint = Cast<UImage>(GetWidgetFromName(TEXT("CardGlint")));
	CardOutline = Cast<UImage>(GetWidgetFromName(TEXT("CardOutline")));
	SparkleA = Cast<UImage>(GetWidgetFromName(TEXT("SparkleA")));
	SparkleB = Cast<UImage>(GetWidgetFromName(TEXT("SparkleB")));
	SparkleC = Cast<UImage>(GetWidgetFromName(TEXT("SparkleC")));
	for (const TWeakObjectPtr<UImage>& Sparkle : { SparkleA, SparkleB, SparkleC })
	{
		if (UImage* W = Sparkle.Get())
		{
			W->SetRenderOpacity(0.f);
		}
	}

	// 스파클 텍스처 주입 — DT_UIVFXTexture 단일 진실 (WBP엔 빈 Image만 배치)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleA.Get());
			TableMgr->ApplyUIVFXTexture(TEXT("SparkleSub"), SparkleB.Get());
			TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleC.Get());
		}
	}

	// 클레임 상태 시각용 캐시 (이름 계약 — 없으면 해당 연출만 생략)
	AccentBarImage = Cast<UImage>(GetWidgetFromName(TEXT("AccentBar")));
	MissionIconWidget = GetWidgetFromName(TEXT("MissionIcon"));
	CardBgImage = Cast<UImage>(GetWidgetFromName(TEXT("CardBg")));

	InManager->OnMissionActivated.AddUObject(this, &UMissionTrackerWidget::HandleMissionActivated);
	InManager->OnMissionCompleted.AddUObject(this, &UMissionTrackerWidget::HandleMissionCompleted);
	InManager->OnGuidePhaseChanged.AddUObject(this, &UMissionTrackerWidget::HandleGuidePhaseChanged);
	InManager->OnMissionProgressChanged.AddUObject(this, &UMissionTrackerWidget::HandleMissionProgressChanged);
	InManager->OnMissionReadyToClaim.AddUObject(this, &UMissionTrackerWidget::HandleMissionReadyToClaim);

	RefreshFromManager();

	// 이미 클레임 대기 상태로 init 됐다면(임베드 승격 타이밍 등) 상태 동기화
	if (InManager->IsReadyToClaim())
	{
		HandleMissionReadyToClaim(FMissionTable());
	}
}

void UMissionTrackerWidget::HandleMissionActivated(const FMissionTable& Mission)
{
	// 완료 연출 타이머가 진행 중이면 양보 — 매니저는 Completed 직후 곧바로 Activated 를 쏘므로
	// (CompleteActiveMission: SetActiveMission → Completed → Activated 순), 여기서 즉시 렌더하면
	// CompleteDelay 동안의 완료 연출이 잘린다. 타이머 콜백이 RefreshFromManager 로 같은 미션을 렌더한다.
	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(CompleteTimerHandle))
		{
			return;
		}
	}
	RenderMission(Mission);
}

void UMissionTrackerWidget::HandleMissionCompleted(FName CompletedID, const FMissionTable& Mission)
{
	// 보상 클레임 대기 강조 종료 — 배경 틴트 복원. 글린트/외곽선은 완료 글로우가 재구동
	bAwaitingClaim = false;
	if (bCardBgTintSaved)
	{
		if (UImage* Bg = CardBgImage.Get())
		{
			Bg->SetColorAndOpacity(SavedCardBgTint);
		}
		bCardBgTintSaved = false;
	}

	// 배지 = 상태색: 완료 순간 그린 (다음 미션 렌더 때 앰버 복귀)
	ApplyBadgeColor(*this, BadgeGreen);

	if (CompleteAnim)
	{
		// 디자이너 WBP 애니메이션이 있으면 그쪽 우선 — 코드 엔벨로프는 미작동
		PlayAnimation(CompleteAnim);
	}
	else
	{
		// WBP 애니가 없으면 코드가 같은 완료 글로우를 직접 구동 (NativeTick 엔벨로프)
		StartCompleteGlow();
	}

	// 완료 연출을 보여준 뒤 다음 미션으로 전환 (없으면 숨김)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CompleteTimerHandle, this,
			&UMissionTrackerWidget::OnCompleteDelayElapsed, CompleteDelay, false);
	}
	else
	{
		OnCompleteDelayElapsed();
	}
}

void UMissionTrackerWidget::HandleMissionReadyToClaim(const FMissionTable& /*Mission*/)
{
	if (bAwaitingClaim)
	{
		return;
	}
	bAwaitingClaim = true;
	ClaimPulseTime = 0.f;

	// 보상 클레임 대기 중에만 클릭 가능 상태로 전환 (평소엔 HitTestInvisible 소프트 HUD)
	SetVisibility(ESlateVisibility::Visible);

	// 완료 상태색 — 배지 그린 + 클레임 유도 문구 + 액센트 그린 (원색 백업 후, 다음 미션 렌더에서 복원)
	ApplyBadgeColor(*this, BadgeGreen);
	if (MentorLineText)
	{
		MentorLineText->SetText(NSLOCTEXT("MissionTracker", "TapToClaim", "미션 완료! 탭해서 보상 받기"));
		MentorLineText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (UImage* Accent = AccentBarImage.Get())
	{
		SavedAccentTint = Accent->GetBrush().TintColor;
		Accent->SetBrushTintColor(FSlateColor(BadgeGreen));
		bAccentTintSaved = true;
	}
	if (UImage* Bg = CardBgImage.Get())
	{
		SavedCardBgTint = Bg->GetColorAndOpacity();
		bCardBgTintSaved = true;
	}

	// 진행도 확정 표시 (50/50, 바 100%)
	UpdateProgress();
}

FReply UMissionTrackerWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bAwaitingClaim)
	{
		bAwaitingClaim = false;
		if (UMissionManagerSubsystem* Mgr = Manager.Get())
		{
			// → 보상 스플래시 + 지급 + Completed(글린트/그린) → CompleteDelay 후 다음 미션 렌더
			Mgr->ClaimActiveMission();
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMissionTrackerWidget::HandleGuidePhaseChanged()
{
	UpdateMentorLine();
}

void UMissionTrackerWidget::HandleMissionProgressChanged()
{
	UpdateProgress();
}

void UMissionTrackerWidget::OnCompleteDelayElapsed()
{
	RefreshFromManager();
}

void UMissionTrackerWidget::SetFocusHidden(bool bHidden)
{
	if (bFocusHidden == bHidden)
	{
		return;
	}
	bFocusHidden = bHidden;

	if (!bHidden)
	{
		// 복귀 — 미션 상태로 콘텐츠/가시성 재settle. 표시할 미션이 없으면(Collapsed)
		// 빈 카드가 슬라이드인하지 않도록 변환/알파를 즉시 원위치로 리셋(틱은 알파가 이미 0).
		RefreshFromManager();
		if (GetVisibility() == ESlateVisibility::Collapsed)
		{
			FocusSlideAlpha = 0.f;
			SetRenderTranslation(FVector2D::ZeroVector);
			SetRenderOpacity(1.f);
		}
	}
	// bHidden==true 는 NativeTick 이 알파 1 로 보간하며 슬라이드아웃 처리.
}

void UMissionTrackerWidget::RefreshFromManager()
{
	UMissionManagerSubsystem* Mgr = Manager.Get();
	if (!Mgr || !Mgr->HasActiveMission())
	{
		// 체인이 끝나면 이 카드는 할 일이 없다 — 미션판은 별도 위젯(UGoalTrackerWidget)이 그린다
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FMissionTable Mission;
	if (Mgr->GetActiveMission(Mission))
	{
		RenderMission(Mission);
	}
	else
	{
		// 체인이 끝나면 이 카드는 할 일이 없다 — 미션판은 별도 위젯(UGoalTrackerWidget)이 그린다
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMissionTrackerWidget::ResetCardVisuals()
{
	// 카드 내용이 바뀌는 시점이라 이전 완료 글로우는 여기서 끊는다 — 시각만 되돌리고 bGlowActive 를
	// 남기면 틱이 새 카드 위에서 글린트/스파클을 계속 구동한다
	StopCompleteGlow();

	if (UImage* Outline = CardOutline.Get())
	{
		Outline->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, OutlineBase));
	}
	if (UImage* Glint = CardGlint.Get())
	{
		FLinearColor C = Glint->GetColorAndOpacity();
		C.A = 0.f;
		Glint->SetColorAndOpacity(C);
	}
	if (UWidget* Icon = MissionIconWidget.Get())
	{
		Icon->SetRenderScale(FVector2D(1.f, 1.f));
	}
	if (bAccentTintSaved)
	{
		if (UImage* Accent = AccentBarImage.Get())
		{
			Accent->SetBrushTintColor(SavedAccentTint);
		}
		bAccentTintSaved = false;
	}
	if (bCardBgTintSaved)
	{
		if (UImage* Bg = CardBgImage.Get())
		{
			Bg->SetColorAndOpacity(SavedCardBgTint);
		}
		bCardBgTintSaved = false;
	}
}

void UMissionTrackerWidget::RenderMission(const FMissionTable& Mission)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 클레임 상태 해제 + 시각 복원 (외곽선 흰 0.15 / 배지 스케일 1 / 액센트 원색)
	bAwaitingClaim = false;
	ResetCardVisuals();

	// 새 미션 = 진행 상태 — 배지 앰버 복귀
	ApplyBadgeColor(*this, BadgeAmber);

	if (MissionTitleText)
	{
		MissionTitleText->SetText(Mission.Title);
	}

	UpdateMentorLine();
	UpdateProgress();

	// 중간 미션의 Rewards 는 진행 공급이고, 피날레만 플레이어 보상으로 표시한다.
	const FMissionTrackerRewardDisplayState RewardDisplay =
		FMissionTrackerDisplayRules::ResolveRewardDisplay(Mission);
	if (RewardLabel)
	{
		RewardLabel->SetText(RewardDisplay.Label);
	}

	// 보상 표시: RewardBox(HorizontalBox) 가 있으면 [아이콘+축약수치] 페어 동적 생성(아이콘 모드),
	// 없으면 기존 RewardText 1줄 요약(폴백). 두 경로는 상호배타 — 둘 다 있으면 RewardText 는 Collapsed.
	UHorizontalBox* RewardContainer = RewardBox
		? RewardBox.Get()
		: Cast<UHorizontalBox>(GetWidgetFromName(TEXT("RewardBox")));
	if (RewardContainer)
	{
		if (!RewardDisplay.bShowSection)
		{
			RewardContainer->ClearChildren();
		}
		const bool bAnyReward = RewardDisplay.bShowSection && WidgetTree
			&& CGRewardChip::RenderIcons(*WidgetTree, GetGameInstance(), *RewardContainer, Mission.Rewards);
		const ESlateVisibility RewardVisibility = bAnyReward
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed;
		RewardContainer->SetVisibility(RewardVisibility);
		if (RewardLabel)
		{
			RewardLabel->SetVisibility(RewardVisibility);
		}
		if (RewardSection)
		{
			RewardSection->SetVisibility(RewardVisibility);
		}

		// 아이콘 모드에선 텍스트 요약을 숨겨 이중 표시 방지
		if (RewardText)
		{
			RewardText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else if (RewardText)
	{
		const FText Summary = RewardDisplay.bShowSection
			? BuildRewardSummary(Mission.Rewards)
			: FText::GetEmpty();
		RewardText->SetText(Summary);
		RewardText->SetVisibility(Summary.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (RewardSection)
		{
			RewardSection->SetVisibility(Summary.IsEmpty()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
		}
	}
}

void UMissionTrackerWidget::UpdateMentorLine()
{
	// 클레임 대기 중엔 MentorLine 슬롯이 "탭해서 보상 받기" CTA를 표시 중 — 페이즈 변경 브로드캐스트가
	// 이 텍스트를 지우지 않도록 보존 (RenderMission/탭이 bAwaitingClaim 을 내릴 때까지).
	if (bAwaitingClaim)
	{
		return;
	}

	// 트래커는 본 미션 Title 만 고정 노출 — 단계별 멘토 라인은 상단 Notification 담당(혼동 방지).
	// 전용 MissionTitleText 가 헤드라인을 그리므로 MentorLine 슬롯은 숨긴다.
	// 단, 타이틀 위젯이 없는 WBP 면 MentorLine 슬롯에 Title 을 대신 표시한다.
	if (!MentorLineText)
	{
		return;
	}

	if (MissionTitleText)
	{
		MentorLineText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UMissionManagerSubsystem* Mgr = Manager.Get();
	FMissionTable Mission;
	const FText Title = (Mgr && Mgr->GetActiveMission(Mission)) ? Mission.Title : FText::GetEmpty();
	MentorLineText->SetText(Title);
	MentorLineText->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMissionTrackerWidget::UpdateProgress()
{
	int64 Current = 0;
	int64 Target = 0;
	UMissionManagerSubsystem* Mgr = Manager.Get();
	const bool bHasProgress = Mgr && Mgr->GetMissionProgress(Current, Target);

	if (ProgressText)
	{
		if (!bHasProgress)
		{
			ProgressText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			const FString CurrentStr = UGlobalUtilFunctions::AbbreviateNumber(Current).ToString();
			const FString TargetStr = UGlobalUtilFunctions::AbbreviateNumber(Target).ToString();
			ProgressText->SetText(FText::FromString(FString::Printf(TEXT("%s / %s"), *CurrentStr, *TargetStr)));
			ProgressText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// 진행 바 (이름 계약 "ProgressBar") — 텍스트와 독립 갱신. 목표 0(즉시형 미션)은 비율이 무의미하니 숨김.
	// 붙여넣기 시 이름이 리네임돼도(ProgressBar_1 등) 카드엔 바가 하나뿐이라 타입 탐색으로 구제.
	UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("ProgressBar")));
	if (!Bar && WidgetTree)
	{
		WidgetTree->ForEachWidget([&Bar](UWidget* W)
		{
			if (!Bar)
			{
				Bar = Cast<UProgressBar>(W);
			}
		});
	}
	if (Bar)
	{
		if (!bHasProgress || Target <= 0)
		{
			Bar->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Bar->SetPercent(FMath::Clamp(static_cast<float>(Current) / static_cast<float>(Target), 0.f, 1.f));
			Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

FText UMissionTrackerWidget::BuildRewardSummary(const TArray<FMissionReward>& Rewards) const
{
	const TArray<CGRewardChip::FDisplayEntry> Entries = CGRewardChip::BuildDisplayEntries(Rewards);
	if (Entries.Num() == 0)
	{
		return FText::GetEmpty();
	}

	UTableManagerSubsystem* TableMgr = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	}

	TArray<FString> Parts;
	Parts.Reserve(Entries.Num());

	for (const CGRewardChip::FDisplayEntry& Entry : Entries)
	{
		// 표시명은 DT 단일 진실 (코드 하드코딩 금지) — 없으면 해당 보상 항목 생략
		FString DisplayName;
		if (TableMgr)
		{
			if (Entry.ResourceType != EResourceType::None)
			{
				bool bFound = false;
				const FResourceInfo Info = TableMgr->GetResourceInfo(Entry.ResourceType, bFound);
				if (bFound && !Info.DisplayName.IsEmpty())
				{
					DisplayName = Info.DisplayName.ToString();
				}
			}
			else if (Entry.ItemType != EItemType::None)
			{
				FShopItemTable ItemRow;
				if (TableMgr->GetShopItemByItemType(Entry.ItemType, ItemRow)
					&& !ItemRow.DisplayName.IsEmpty())
				{
					DisplayName = ItemRow.DisplayName.ToString();
				}
			}
		}
		if (DisplayName.IsEmpty())
		{
			continue;
		}

		const FText QuantityText = Entry.ItemType != EItemType::None
			? FText::Format(NSLOCTEXT("Reward", "Qty", "x{0}"), FText::AsNumber(Entry.Quantity))
			: UGlobalUtilFunctions::AbbreviateNumber(Entry.Quantity);
		const FString QuantityString = QuantityText.ToString();
		Parts.Add(FString::Printf(TEXT("%s %s"), *DisplayName, *QuantityString));
	}

	if (Parts.Num() == 0)
	{
		return FText::GetEmpty();
	}

	const FString Joined = FString::Join(Parts, TEXT(" · "));
	return FText::FromString(FString::Printf(TEXT("보상: %s"), *Joined));
}
