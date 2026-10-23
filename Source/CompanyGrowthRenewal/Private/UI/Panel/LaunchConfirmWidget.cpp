#include "UI/Panel/LaunchConfirmWidget.h"
#include "UI/UIBase.h"
#include "CommonTextBlock.h"
#include "UI/Element/Common/QualityGradeMedalUtils.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "UI/Element/Common/DisciplineBarWidget.h"
#include "UI/Element/Office/ReviewCardWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/LaunchReactionSubsystem.h"
#include "Table/ProjectDataTable.h"
#include "Table/ResourceInfo.h"
#include "Table/ShopItemTable.h"
#include "Utils/LaunchRewardRevealMath.h"
#include "Core/CGGameInstance.h"
#include "Enum/QualityGrade.h"
#include "Enum/ProjectMode.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CommonButtonBase.h"
#include "CommonBorder.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Utils/FWidgetAnimationUtils.h"

namespace
{
	FText FormatEarnedExperience(float Amount)
	{
		FNumberFormattingOptions NumberOptions;
		NumberOptions.SetMinimumFractionalDigits(0);
		NumberOptions.SetMaximumFractionalDigits(Amount < 0.01f ? 6 : 2);
		return FText::Format(
			NSLOCTEXT("LaunchConfirm", "EarnedExperience", "직원 경험치 +{0}"),
			FText::AsNumber(static_cast<double>(Amount), &NumberOptions));
	}

	// 리빌 펀치 진폭 — S등급만 상향 (클라이맥스 강조)
	constexpr float RevealPopScale = 1.16f;
	constexpr float RevealPopScaleGrand = 1.24f;

	// 슬라이드인 시작 오프셋 (좌 → 우)
	constexpr float RevealSlideOffsetX = -46.0f;

	// 사운드 스팸 게이트 — 이 간격 안에 겹치는 리빌 사운드는 버린다
	constexpr float RevealSoundMinInterval = 0.09f;

	constexpr float LaunchCelebrationDuration = 1.08f;
	constexpr float LaunchCelebrationFlareAttack = .08f;
	constexpr float LaunchCelebrationFlareExpand = .25f;
	constexpr float LaunchCelebrationSparkDuration = .26f;
	constexpr float LaunchCelebrationSparkPeriod = .28f;
	constexpr int32 LaunchCelebrationSparkBursts = 3;
	constexpr float LaunchCelebrationSparkStarts[2] = { .12f, .22f };
	// 등급 도장: 칼끝 빛이 제목에 꽂힌 뒤 떨어지기 시작한다
	constexpr float LaunchGradeStampDelay = .30f;
	constexpr float LaunchStampSquashTime = .035f;
	constexpr float LaunchStampSettleTime = .12f;
	constexpr float LaunchGradeRecoilDuration = .22f;
	// 착지 뒤 눌림·정착·표지 반동이 모두 끝나는 시각
	constexpr float LaunchStampSettleEnd = .32f;
	const FVector2D LaunchStampFallOffset(-68.f, -180.f);

	// 칼끝 빛
	constexpr float StrikeThrustTravel = .12f;
	constexpr float StrikeDuration = .45f;
	constexpr float StrikeImpactDuration = .32f;
	constexpr int32 StrikeImpactShardCount = 8;
	constexpr int32 StrikeThrustCount = 12;
	constexpr int32 StrikeBurstCount = 8;
	// 수직 성분을 눌러 제목 둘레에 납작한 타원으로 꽂히게 한다
	constexpr float StrikeVerticalSquash = .62f;
	constexpr float StrikeThrustAngles[StrikeThrustCount] = { -168.f, -142.f, -118.f, -62.f, -38.f, -14.f, 14.f, 38.f, 62.f, 118.f, 142.f, 166.f };
	constexpr float StrikeBurstAngles[StrikeBurstCount] = { -160.f, -110.f, -70.f, -20.f, 20.f, 70.f, 110.f, 160.f };
	// T_UI_FX_Blade 에서 흰 심이 차지하는 높이 비율 — 나머지는 금색 번짐
	constexpr float StrikeBladeCoreFraction = .36f;

	// 예상 총수익 곱셈: 0 → 기본 수익(0.28초) → 도장 착지 + 0.03초에 배율을 곱해 총액까지(0.36초)
	constexpr float LaunchRevenueBaseCountTime = .28f;
	constexpr float LaunchMultiplyDelay = .03f;
	constexpr float LaunchRevenueMultiplyCountTime = .36f;
	// 금빛·칩·줄 강조가 모두 사라지는 시각(곱하는 순간 기준)
	constexpr float LaunchRevenueMultiplyEnd = 1.1f;
	const FLinearColor LaunchMultiplyGold(1.f, .672f, .195f);      // #FFD67A
	const FLinearColor LaunchRowGlowColor(1.f, .62f, .165f, .26f);  // #FFCF71 @26%

	// 분야 바 차오름
	constexpr float LaunchBarRevealStart = .10f;
	constexpr float LaunchBarRevealStagger = .08f;
	constexpr float LaunchBarRevealTime = .42f;
	constexpr float LaunchBarFlashTime = .25f;

	float StrikeEaseOutCubic(float T)
	{
		return 1.f - FMath::Pow(1.f - FMath::Clamp(T, 0.f, 1.f), 3.f);
	}

	void HideStrikeLayer(const TWeakObjectPtr<UImage>& Layer)
	{
		if (UImage* Image = Layer.Get())
		{
			Image->SetRenderOpacity(0.f);
			Image->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f));
		}
	}

	// 칼끝: Tip 이 뾰족한 끝. 이미지 피벗(1,0.5)·캔버스 정렬(1,0.5) 계약이라 끝을 축으로 회전·축소된다.
	void DriveStrikeBlade(UImage* Blade, const FVector2D& Tip, float AngleRadians, float Length, float CoreThickness, float Alpha)
	{
		const UCanvasPanelSlot* BladeSlot = Blade ? Cast<UCanvasPanelSlot>(Blade->Slot) : nullptr;
		if (!BladeSlot || BladeSlot->GetSize().X < 1.f || BladeSlot->GetSize().Y < 1.f) { return; }
		const FVector2D Base = BladeSlot->GetSize();
		Blade->SetRenderTransform(FWidgetTransform(Tip,
			FVector2D(Length / Base.X, CoreThickness / StrikeBladeCoreFraction / Base.Y), FVector2D::ZeroVector,
			FMath::RadiansToDegrees(AngleRadians)));
		Blade->SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
	}

	void DriveStrikeGlow(UImage* Glow, const FVector2D& Center, float ScaleX, float ScaleY, float Alpha)
	{
		if (!Glow) { return; }
		Glow->SetRenderTransform(FWidgetTransform(Center, FVector2D(ScaleX, ScaleY), FVector2D::ZeroVector, 0.f));
		Glow->SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
	}

	// 번쩍임 = 부드러운 원광 + 바깥으로 뻗는 칼끝 네 개(가로가 더 길다)
	void DriveStrikeFlash(const TArray<TWeakObjectPtr<UImage>>& Blades, UImage* Glow, const FVector2D& Center,
		float Phase, float FromSize, float ToSize, float Rotation, float Intensity)
	{
		const bool bLive = Phase >= 0.f && Phase <= 1.f;
		const float Size = FMath::Lerp(FromSize, ToSize, StrikeEaseOutCubic(Phase));
		const float Alpha = bLive ? (Phase < .15f ? Phase / .15f : FMath::Square(1.f - (Phase - .15f) / .85f)) * Intensity : 0.f;
		if (Glow)
		{
			const UCanvasPanelSlot* GlowSlot = Cast<UCanvasPanelSlot>(Glow->Slot);
			const float GlowBase = GlowSlot ? FMath::Max(1.f, GlowSlot->GetSize().X) : Size;
			DriveStrikeGlow(Glow, Center, Size / GlowBase, Size / GlowBase, Alpha * .6f);
		}
		for (int32 Index = 0; Index < Blades.Num(); ++Index)
		{
			const float Angle = Rotation + Index * UE_HALF_PI;
			const float Length = Size * (Index % 2 == 0 ? .78f : .4f);
			DriveStrikeBlade(Blades[Index].Get(), Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Length,
				Angle, Length, Size * .075f, Alpha);
		}
	}

}

ULaunchConfirmWidget::ULaunchConfirmWidget()
{
	bIsBackHandler = true;
}

void ULaunchConfirmWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터 디자인 타임에도 MessageText 숨김
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->HideMessageText();
	}
}

void ULaunchConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateLaunchCelebration(-1.f);
	CollectStrikeLayers();
	ResetStrikeFx();
	if (LaunchMulChip) { LaunchMulChip->SetRenderOpacity(0.f); }
	HideStrikeLayer(LaunchMulStreak);

	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->bAutoRemove = false;
		ConfirmCancelWidget->HideMessageText();

		// 프롬프트 스택 풀 재사용 시 NativeConstruct 재진입 — 중복 바인딩 가드 (중복되면 [출시하기] 1클릭에 확정이 2회 발화)
		ConfirmCancelWidget->OnConfirm.RemoveAll(this);
		ConfirmCancelWidget->OnConfirm.AddUObject(this, &ULaunchConfirmWidget::OnDialogConfirmed);
		ConfirmCancelWidget->OnCancel.RemoveAll(this);
		ConfirmCancelWidget->OnCancel.AddUObject(this, &ULaunchConfirmWidget::OnDialogCancelled);
	}

	// X 닫기 버튼 바인딩
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &ULaunchConfirmWidget::OnCloseBtnClicked);
	}

}

void ULaunchConfirmWidget::NativeDestruct()
{
	ResetResultEntry();
	SkipRevealQueue();
	UpdateLaunchCelebration(-1.f);
	ResetStrikeFx();
	RevealQueue.Reset();
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->OnConfirm.RemoveAll(this);
		ConfirmCancelWidget->OnCancel.RemoveAll(this);
	}
	if (CloseButton) CloseButton->OnClicked().RemoveAll(this);
	if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		M->UnregisterLaunchConfirm(this);
	}
	Super::NativeDestruct();
}

void ULaunchConfirmWidget::BeginResultEntry()
{
	ResetResultEntry();
	if (!ConfirmCancelWidget || USettingsManagerSubsystem::IsReduceMotion(this)) return;
	UWidget* Surface = ConfirmCancelWidget->GetWidgetFromName(TEXT("ModalFrame"));
	if (!Surface) Surface = ConfirmCancelWidget->GetWidgetFromName(TEXT("SizeBox_128"));
	if (!Surface) return;
	EntrySurface = Surface;
	EntryBaseTransform = Surface->GetRenderTransform();
	EntryBaseOpacity = Surface->GetRenderOpacity();
	EntryElapsed = 0.f;
	FWidgetTransform Transform = EntryBaseTransform;
	Transform.Translation.Y += 24.f;
	Surface->SetRenderTransform(Transform);
	Surface->SetRenderOpacity(0.f);
}

void ULaunchConfirmWidget::ResetResultEntry()
{
	if (UWidget* Surface = EntrySurface.Get())
	{
		Surface->SetRenderTransform(EntryBaseTransform);
		Surface->SetRenderOpacity(EntryBaseOpacity);
	}
	EntrySurface.Reset();
	EntryElapsed = .2f;
}

void ULaunchConfirmWidget::NativeOnDeactivated()
{
	ResetResultEntry();
	SkipRevealQueue();
	UpdateLaunchCelebration(-1.f);
	ResetStrikeFx();
	RevealQueue.Reset();
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->UnregisterLaunchConfirm(this);
	}
	Super::NativeOnDeactivated();
}

UWidget* ULaunchConfirmWidget::GetConfirmButtonWidget() const
{
	return ConfirmCancelWidget ? ConfirmCancelWidget->GetConfirmButtonWidget() : nullptr;
}

UWidget* ULaunchConfirmWidget::GetDiscardButtonWidget() const
{
	return bFailurePhase && ConfirmCancelWidget ? ConfirmCancelWidget->GetCancelButtonWidget() : nullptr;
}

bool ULaunchConfirmWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	OnDialogCancelled();
	return true;
}

UWidget* ULaunchConfirmWidget::NativeGetDesiredFocusTarget() const
{
	return !bFailurePhase && GetConfirmButtonWidget() ? GetConfirmButtonWidget() : Super::NativeGetDesiredFocusTarget();
}

void ULaunchConfirmWidget::SetManufacturingHidden(UWidget* Widget)
{
	if (!Widget) return;
	ManufacturingHiddenWidgets.Emplace(TWeakObjectPtr<UWidget>(Widget), Widget->GetVisibility());
	Widget->SetVisibility(ESlateVisibility::Collapsed);
}

void ULaunchConfirmWidget::CaptureManufacturingBox(USizeBox* Box)
{
	if (!Box) return;
	FManufacturingBoxSize Saved;
	Saved.Box = Box;
	Saved.bWidthOverride = Box->IsWidthOverride();
	Saved.Width = Box->GetWidthOverride();
	Saved.bHeightOverride = Box->IsHeightOverride();
	Saved.Height = Box->GetHeightOverride();
	ManufacturingBoxSizes.Add(Saved);
}

void ULaunchConfirmWidget::ApplyManufacturingResultLayout()
{
	USizeBox* ProjectBox = Cast<USizeBox>(GetWidgetFromName(TEXT("LaunchProjectWidth")));
	USizeBox* ResultBox = Cast<USizeBox>(GetWidgetFromName(TEXT("Col1Box")));
	UHorizontalBoxSlot* ProjectSlot = ProjectBox ? Cast<UHorizontalBoxSlot>(ProjectBox->Slot) : nullptr;
	if (!ProjectBox || !ResultBox || !ProjectSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] Manufacturing result layout needs LaunchProjectWidth and Col1Box."));
		return;
	}
	CaptureManufacturingBox(ProjectBox);
	CaptureManufacturingBox(ResultBox);
	ManufacturingProjectSlot = ProjectSlot;
	ManufacturingProjectPadding = ProjectSlot->GetPadding();
	ManufacturingProjectSlotValue = ProjectSlot->GetSize().Value;
	bManufacturingProjectSlotFill = ProjectSlot->GetSize().SizeRule == ESlateSizeRule::Fill;
	ManufacturingProjectSlotAlignment = static_cast<uint8>(ProjectSlot->GetHorizontalAlignment());

	ProjectBox->SetWidthOverride(800.f);
	ResultBox->ClearHeightOverride();
	ProjectSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ProjectSlot->SetPadding(FMargin(0.f));
	ProjectSlot->SetHorizontalAlignment(HAlign_Center);

	for (UWidget* TextHost : { GetWidgetFromName(TEXT("LaunchNameMax")), static_cast<UWidget*>(Text_ManufacturingResearchHint) })
	{
		if (UVerticalBoxSlot* TextSlot = TextHost ? Cast<UVerticalBoxSlot>(TextHost->Slot) : nullptr)
		{
			ManufacturingTextSlotAlignments.Emplace(TWeakObjectPtr<UVerticalBoxSlot>(TextSlot),
				static_cast<uint8>(TextSlot->GetHorizontalAlignment()));
			TextSlot->SetHorizontalAlignment(TextHost == Text_ManufacturingResearchHint ? HAlign_Fill : HAlign_Center);
		}
	}
}

void ULaunchConfirmWidget::RestoreManufacturingResultLayout()
{
	for (const FManufacturingBoxSize& Saved : ManufacturingBoxSizes)
	{
		if (USizeBox* Box = Saved.Box.Get())
		{
			if (Saved.bWidthOverride) Box->SetWidthOverride(Saved.Width);
			else Box->ClearWidthOverride();
			if (Saved.bHeightOverride) Box->SetHeightOverride(Saved.Height);
			else Box->ClearHeightOverride();
		}
	}
	ManufacturingBoxSizes.Reset();
	if (UHorizontalBoxSlot* ProjectSlot = ManufacturingProjectSlot.Get())
	{
		FSlateChildSize OriginalSize(bManufacturingProjectSlotFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic);
		OriginalSize.Value = ManufacturingProjectSlotValue;
		ProjectSlot->SetSize(OriginalSize);
		ProjectSlot->SetPadding(ManufacturingProjectPadding);
		ProjectSlot->SetHorizontalAlignment(static_cast<EHorizontalAlignment>(ManufacturingProjectSlotAlignment));
	}
	ManufacturingProjectSlot.Reset();
	for (const TPair<TWeakObjectPtr<UVerticalBoxSlot>, uint8>& Saved : ManufacturingTextSlotAlignments)
	{
		if (UVerticalBoxSlot* TextSlot = Saved.Key.Get())
		{
			TextSlot->SetHorizontalAlignment(static_cast<EHorizontalAlignment>(Saved.Value));
		}
	}
	ManufacturingTextSlotAlignments.Reset();
}

void ULaunchConfirmWidget::RestoreManufacturingResultVisuals()
{
	RestoreManufacturingResultLayout();
	for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>& Saved : ManufacturingHiddenWidgets)
	{
		if (UWidget* Widget = Saved.Key.Get()) Widget->SetVisibility(Saved.Value);
	}
	ManufacturingHiddenWidgets.Reset();
	if (bHasOriginalLaunchCompletionTitle && Text_LaunchCompletionTitle)
	{
		Text_LaunchCompletionTitle->SetText(OriginalLaunchCompletionTitle);
	}
	bHasOriginalLaunchCompletionTitle = false;
	if (Text_ManufacturingResearchHint) Text_ManufacturingResearchHint->SetVisibility(ESlateVisibility::Collapsed);
	bManufacturingResearchResult = false;
}

void ULaunchConfirmWidget::SetManufacturingResearchResult(ECompanyType CompanyType, int32 ProjectIndex)
{
	SkipRevealQueue();
	RestoreManufacturingResultVisuals();
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->UnregisterLaunchConfirm(this);
	}
	bManufacturingResearchResult = true;
	bManufacturingResultAnswered = false;
	bFailurePhase = false;
	RevealQueue.Reset();
	RevealBars.Reset();
	RevealBarStarts.Reset();
	RevenueCountStartAt = -1.f;
	UpdateLaunchCelebration(-1.f);
	ResetStrikeFx();
	if (LaunchResultScroll) LaunchResultScroll->ScrollToStart();
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(0);
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->RestoreDefaultTexts();
		ConfirmCancelWidget->SetConfirmOnly(false);
		ConfirmCancelWidget->SetCancelOnly(false);
		ConfirmCancelWidget->SetTitle(NSLOCTEXT("LaunchConfirm", "ManufacturingResearchResult", "제품 개발 결과"));
		ConfirmCancelWidget->SetConfirmButtonText(NSLOCTEXT("LaunchConfirm", "ManufacturingQuantityAction", "수량 확보하기"));
		ConfirmCancelWidget->SetCancelButtonText(NSLOCTEXT("LaunchConfirm", "ManufacturingDismiss", "닫기"));
	}
	SetConfirmInteractionEnabled(true);
	if (CloseButton) CloseButton->SetVisibility(ESlateVisibility::Visible);

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	bool bFound = false;
	const FProjectData Product = TableMgr ? TableMgr->GetProjectData(CompanyType, ProjectIndex, bFound) : FProjectData();
	if (!bFound) UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] Manufacturing product row missing: CompanyType=%d ProjectIndex=%d"), static_cast<int32>(CompanyType), ProjectIndex);
	if (Text_ProjectName) Text_ProjectName->SetText(bFound ? Product.ProjectName : FText::GetEmpty());
	if (Image_ProjectCover)
	{
		UTexture2D* Icon = bFound && !Product.Icon.IsNull() ? Product.Icon.LoadSynchronous() : nullptr;
		Image_ProjectCover->SetBrushFromTexture(Icon, true);
		Image_ProjectCover->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text_ManufacturingResearchHint)
	{
		Text_ManufacturingResearchHint->SetText(NSLOCTEXT("LaunchConfirm", "ManufacturingQuantityHint", "이제 이 제품의 생산 수량을 확보할 수 있습니다"));
		Text_ManufacturingResearchHint->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] UI_LaunchConfirmPanel needs Text_ManufacturingResearchHint for the manufacturing result."));
	}

	if (Text_LaunchCompletionTitle)
	{
		OriginalLaunchCompletionTitle = Text_LaunchCompletionTitle->GetText();
		bHasOriginalLaunchCompletionTitle = true;
		Text_LaunchCompletionTitle->SetText(NSLOCTEXT("LaunchConfirm", "ManufacturingResearchSuccess", "제품 개발 성공!"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] UI_LaunchConfirmPanel needs Text_LaunchCompletionTitle for the manufacturing result."));
	}
	SetManufacturingHidden(GetWidgetFromName(TEXT("LaunchGradeColumn")));
	SetManufacturingHidden(LaunchGradeStamp);
	SetManufacturingHidden(Text_Experience);
	SetManufacturingHidden(GetWidgetFromName(TEXT("StepWell")));
	SetManufacturingHidden(GetWidgetFromName(TEXT("LaunchFinanceWidth")));
	SetManufacturingHidden(GetWidgetFromName(TEXT("LaunchFinanceColumn")));
	SetManufacturingHidden(GetWidgetFromName(TEXT("LaunchIncomeHost")));
	SetManufacturingHidden(GetWidgetFromName(TEXT("LaunchLootHost")));
	SetManufacturingHidden(LaunchLootPanel);
	SetManufacturingHidden(GetWidgetFromName(TEXT("LaunchReviewIdentityRow")));
	ApplyManufacturingResultLayout();

	BeginResultEntry();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>()) SoundMgr->PlaySound(FName("Event_Launch"));
	}
	RevealClock = 0.f;
	RevealEndTime = LaunchCelebrationDuration;
	LastRevealSoundClock = -10.f;
	QueueReveal(LaunchCompletionHeader, 0.f, true);
	QueueReveal(LaunchCoverSurface, .12f, true);
	QueueReveal(Text_ProjectName, .28f, false, FName("Event_ScoreSweep"));
	QueueReveal(Text_ManufacturingResearchHint, .38f);
	bRevealActive = RevealQueue.Num() > 0 || LaunchFlare || LaunchSpark1 || LaunchSpark2;
	if (USettingsManagerSubsystem::IsReduceMotion(this)) SkipRevealQueue();
}

void ULaunchConfirmWidget::SetPreviewData(const FStageProgressData& StageData, int32 ProjectNumber)
{
	SkipRevealQueue();
	RestoreManufacturingResultVisuals();
	bManufacturingResultAnswered = false;
	// 풀 재진입이나 성공→실패 전환 시 이전 리빌 시계와 장식을 먼저 비운다.
	bRevealActive = false;
	RevealQueue.Reset();
	RevealBars.Reset();
	RevealBarStarts.Reset();
	RevenueCountStartAt = -1.f;
	UpdateLaunchCelebration(-1.f);
	ResetStrikeFx();
	if (LaunchResultScroll) { LaunchResultScroll->ScrollToStart(); }
	const FWidgetTransform IdentityTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f);
	for (UWidget* CalcRow : { static_cast<UWidget*>(RevRow_Multiplier), static_cast<UWidget*>(RevRow_Duration), static_cast<UWidget*>(RevRow_BaseRevenue) })
	{
		if (!CalcRow) { continue; }
		CalcRow->SetRenderOpacity(1.f);
		CalcRow->SetRenderTransform(IdentityTransform);
	}
	BeginResultEntry();
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	// 프로젝트 이름
	if (Text_ProjectName)
	{
		Text_ProjectName->SetText(FText::FromString(StageData.ProjectName));
	}

	if (Image_ProjectCover)
	{
		UTexture2D* IconTexture = nullptr;
		if (UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr)
		{
			bool bSuccess = false;
			const FProjectData ProjectData = (StageData.CompanyType != ECompanyType::None)
				? TableMgr->GetProjectData(StageData.CompanyType, ProjectNumber, bSuccess)
				: TableMgr->ResolveProjectData(ProjectNumber, bSuccess);
			if (bSuccess && !ProjectData.Icon.IsNull())
			{
				IconTexture = ProjectData.Icon.LoadSynchronous();
			}
		}
		Image_ProjectCover->SetBrushFromTexture(IconTexture, true);
		Image_ProjectCover->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 운영 시작과 같은 건물·품질·수익 조건으로 결과 화면을 채운다.
	UProjectOperationManager* OpMgr = GI ? GI->GetSubsystem<UProjectOperationManager>() : nullptr;
	const UCGGameInstance* ManagedGI = UCGGameInstance::GetInstance();
	const int32 BuildingID = ManagedGI ? ManagedGI->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	const FOperationLaunchForecast Forecast = OpMgr
		? OpMgr->GetLaunchForecast(StageData, BuildingID) : FOperationLaunchForecast();
	const EQualityGrade Grade = OpMgr ? Forecast.EffectiveGrade : StageData.CalculateQualityGrade();

	const FString GradeLetter = QualityGradeToAlphabetString(Grade);
	if (Text_QualityGrade)
	{
		Text_QualityGrade->SetText(FText::FromString(GradeLetter));
	}
	ApplyGradeStampColor(GradeLetterToColor(GradeLetter));
	ApplyGradeMedal(StageData.CompanyType, Grade);

	// 분야별 성과 — 축 고정 6칸(EProductionDiscipline 슬롯 순서). 비활성 분야는 빈 트랙으로
	// "요구 없음"을 보여 픽칭 카드 DevPlate와 실루엣이 이어진다.
	{
		UDisciplineBarWidget* DiscBars[6] = { DiscBar1, DiscBar2, DiscBar3, DiscBar4, DiscBar5, DiscBar6 };

		UTableManagerSubsystem* DiscTableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		const ECompanyType Industry = StageData.CompanyType;

		for (int32 SlotIdx = 0; SlotIdx < UE_ARRAY_COUNT(DiscBars); ++SlotIdx)
		{
			if (!DiscBars[SlotIdx]) { continue; }

			// 라벨 SOT = DT_DisciplineDisplay — 매니저 스텝 표시명과 같은 출처(패키지 빌드 안전)
			const FText Name = DiscTableMgr
				? DiscTableMgr->GetDisciplineDisplayName(Industry, SlotIdx) : FText::GetEmpty();

			const FStepRoundData* Found = StageData.Steps.FindByPredicate(
				[SlotIdx](const FStepRoundData& S) { return S.DisciplineSlot == SlotIdx; });

			if (Found)
			{
				// 채움색 = 직능 슬롯 위치색 (orb·플로팅 텍스트와 단일 출처 GetStepColor)
				DiscBars[SlotIdx]->SetInfo(Name, Found->AcquiredScore, Found->TargetScore,
					UGlobalUtilFunctions::GetStepColor(SlotIdx + 1));
			}
			else
			{
				DiscBars[SlotIdx]->SetEmpty(Name);
			}
		}
	}

	{
		// 자체개발/제조업: 운영 정보 표시
		const float OperationSeconds = Forecast.OperationSeconds;

		if (Text_OperationTime)
		{
			int32 TotalSeconds = FMath::FloorToInt(OperationSeconds);
			int32 Minutes = TotalSeconds / 60;
			int32 Seconds = TotalSeconds % 60;

			FString TimeStr;
			if (Minutes >= 60)
			{
				int32 Hours = Minutes / 60;
				int32 Mins = Minutes % 60;
				TimeStr = FString::Printf(TEXT("%d시간 %d분"), Hours, Mins);
			}
			else if (Minutes > 0)
			{
				TimeStr = FString::Printf(TEXT("%d분 %d초"), Minutes, Seconds);
			}
			else
			{
				TimeStr = FString::Printf(TEXT("%d초"), Seconds);
			}
			Text_OperationTime->SetText(FText::FromString(TimeStr));
		}

		// 곱셈 연출은 실제 운영 피크에서 품질 배율만 제외한 전망값을 시작점으로 쓴다.
		PendingBaseRevenue = static_cast<int64>(Forecast.BaseTotalRevenue);
		if (Text_BaseRevenue)
		{
			Text_BaseRevenue->SetText(FText::AsNumber(PendingBaseRevenue));
		}
		FNumberFormattingOptions MultiplierOpts;
		MultiplierOpts.SetMinimumFractionalDigits(0);
		MultiplierOpts.SetMaximumFractionalDigits(2);
		PendingMultiplierText = FText::Format(NSLOCTEXT("LaunchConfirm", "QualityMultiplier", "×{0}"),
			FText::AsNumber(Forecast.QualityMultiplier, &MultiplierOpts));
		if (Text_QualityMultiplier) { Text_QualityMultiplier->SetText(PendingMultiplierText); }
		if (LaunchMulChip) { LaunchMulChip->SetText(PendingMultiplierText); }
		if (Text_QualityMultiplierLabel)
		{
			Text_QualityMultiplierLabel->SetText(FText::Format(
				NSLOCTEXT("LaunchConfirm", "QualityMultiplierLabel", "품질 배율 ({0})"), FText::FromString(GradeLetter)));
		}

		PendingTotalRevenue = static_cast<int64>(Forecast.TotalRevenue);
		if (Text_TotalRevenue)
		{
			Text_TotalRevenue->SetText(FText::AsNumber(PendingTotalRevenue));
		}
	}

	// 작품 정보와 출시 전리품을 같은 결과 페이지에서 보여준다.
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(0);
	}
	// 풀 재사용 대비: 직전 실패 페이즈가 Collapsed 로 남긴 X/취소 버튼 복원
	if (CloseButton)
	{
		CloseButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->SetConfirmOnly(false);
		// 짝 필수 — 직전 실패 페이즈의 SetCancelOnly 가 Confirm 을 접은 채 남으면
		// 다음 프로젝트가 미달이 아니어도 [출시하기] 가 영영 사라진다 (2026-08-12)
		ConfirmCancelWidget->SetCancelOnly(false);
	}
	// 직전 리빌이 잠가둔 확인 버튼 복원 (풀 재사용 대비)
	SetConfirmInteractionEnabled(true);

	// 풀 재사용 대비 — 직전 실패 페이즈("추가 개발")가 덮어쓴 제목·라벨을 WBP 기본값으로 되돌린다.
	// Default* 는 NativePreConstruct 에서만 적용되는데 재사용 시엔 PreConstruct 가 다시 돌지 않는다.
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->RestoreDefaultTexts();
	}

	// 수준 미달 = 출시 경로를 아예 열지 않는다 — 미리보기를 건너뛰고 실패 페이지로 직행
	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	bFailurePhase = StageMgr && StageMgr->IsLaunchBlocked();
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->RegisterLaunchConfirm(this);
	}
	if (!bFailurePhase)
	{
		if (USoundManagerSubsystem* SoundMgr = GI ? GI->GetSubsystem<USoundManagerSubsystem>() : nullptr)
		{
			SoundMgr->PlaySound(FName("Event_Launch"));
		}
	}
	if (Text_Experience)
	{
		const float Earned = StageData.EarnedExperience;
		Text_Experience->SetVisibility(!bFailurePhase && FMath::IsFinite(Earned) && Earned > 0.0f
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bFailurePhase && FMath::IsFinite(Earned) && Earned > 0.0f)
		{
			Text_Experience->SetText(FormatEarnedExperience(Earned));
		}
	}
	if (bFailurePhase)
	{
		if (ContentSwitcher)
		{
			ContentSwitcher->SetActiveWidgetIndex(FailurePageIndex);
		}
		FillFailureSection(StageData);
	}
	else
	{
		if (ConfirmCancelWidget) { ConfirmCancelWidget->SetConfirmOnly(true); }
		if (CloseButton) { CloseButton->SetVisibility(ESlateVisibility::Collapsed); }
		FillLaunchLootSection(StageData);
		QueueRevenueReveal(PendingTotalRevenue);
	}
}

void ULaunchConfirmWidget::FillLaunchLootSection(const FStageProgressData& StageData)
{
	UGameInstance* GameInstance = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!LaunchLootRow) { return; }
	LaunchLootRow->ClearChildren();
	const bool bHasExperience = Text_Experience && Text_Experience->IsVisible();
	if (LaunchLootPanel)
	{
		LaunchLootPanel->SetVisibility(bHasExperience
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StageData.PendingLaunchLoot.IsEmpty()) { return; }
	const TSubclassOf<UUserWidget> CardClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::ItemCardQtyInside) : nullptr;
	if (!TableMgr || !CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] ItemCardQtyInside is not registered"));
		return;
	}
	for (const FMissionReward& Reward : LaunchRewardRevealMath::BuildRevealOrder(StageData.PendingLaunchLoot))
	{
		UItemCardSlotWidget* Card = CreateWidget<UItemCardSlotWidget>(this, CardClass);
		if (!Card) { continue; }
		bool bIconFound = false;
		UObject* Icon = nullptr;
		FText Quantity, Name, Description;
		if (Reward.ItemType != EItemType::None)
		{
			Icon = TableMgr->GetItemIconResource(Reward.ItemType, bIconFound);
			FShopItemTable ItemRow;
			if (TableMgr->GetShopItemByItemType(Reward.ItemType, ItemRow)) { Name = ItemRow.DisplayName; }
			Quantity = FText::Format(NSLOCTEXT("LaunchConfirm", "LootQuantity", "x{0}"), FText::AsNumber(Reward.ItemAmount));
		}
		else
		{
			const FResourceInfo ResourceInfo = TableMgr->GetResourceInfo(Reward.ResourceType, bIconFound);
			Icon = bIconFound && !ResourceInfo.Icon.IsNull() ? ResourceInfo.Icon.LoadSynchronous() : nullptr;
			Name = ResourceInfo.DisplayName;
			Description = ResourceInfo.Description;
			Quantity = UGlobalUtilFunctions::AbbreviateNumber(Reward.Amount);
		}
		if (!bIconFound || !Icon) { UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] Missing launch reward icon")); }
		Card->SetIconResource(Icon);
		Card->SetLabel(Quantity);
		if (LaunchLootCardStyle && Card->GetCardWidget())
		{
			Card->GetCardWidget()->SetStyle(LaunchLootCardStyle);
		}
		Card->SetQuantityBackgroundStyle(LaunchLootQuantityStyle);
		Card->SetLabelFontSize(LaunchLootLabelFontSize);
		Card->SetTooltipInfo(Name, Description, Icon);
		Card->SetRenderTransformPivot(FVector2D(.5f, .5f));
		const bool bFirstCard = LaunchLootRow->GetChildrenCount() == 0;
		if (UHorizontalBoxSlot* CardSlot = LaunchLootRow->AddChildToHorizontalBox(Card))
		{
			CardSlot->SetPadding(FMargin(bFirstCard ? 0.f : 20.f, 0.f, 0.f, 0.f));
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	if (LaunchLootPanel && LaunchLootRow->GetChildrenCount() > 0)
	{
		LaunchLootPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void ULaunchConfirmWidget::UpdateLaunchCelebration(float Time)
{
	const FWidgetTransform Identity(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f);
	if (Time < 0.f || Time >= LaunchCelebrationDuration)
	{
		for (UImage* Effect : { LaunchFlare, LaunchSpark1, LaunchSpark2 })
		{
			if (!Effect) { continue; }
			Effect->SetRenderOpacity(0.f);
			Effect->SetRenderTransform(Identity);
		}
		return;
	}

	if (LaunchFlare)
	{
		const float Expand = FWidgetAnimationUtils::EaseOutQuad(FMath::Clamp(Time / LaunchCelebrationFlareExpand, 0.f, 1.f));
		const float Height = FWidgetAnimationUtils::EaseOutQuad(FMath::Clamp(Time / LaunchCelebrationFlareAttack, 0.f, 1.f));
		const float Fade = FMath::Clamp((LaunchCelebrationDuration - Time) /
			(LaunchCelebrationDuration - LaunchCelebrationFlareExpand), 0.f, 1.f);
		FWidgetTransform FlareTransform = Identity;
		FlareTransform.Scale = FVector2D(FMath::Lerp(.08f, 1.25f, Expand),
			FMath::Lerp(.12f, 1.f, Height) * FMath::Lerp(.12f, 1.f, Fade));
		FlareTransform.Translation = FVector2D(FMath::Lerp(-35.f, 35.f, Time / LaunchCelebrationDuration),
			FMath::Lerp(5.f, -5.f, Expand));
		LaunchFlare->SetRenderTransform(FlareTransform);
		LaunchFlare->SetRenderOpacity(.85f * FMath::Min(Time / LaunchCelebrationFlareAttack, 1.f) * Fade);
	}
	UImage* Sparkles[] = { LaunchSpark1, LaunchSpark2 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sparkles); ++Index)
	{
		if (!Sparkles[Index]) { continue; }
		const float LocalTime = Time - LaunchCelebrationSparkStarts[Index];
		const int32 BurstIndex = FMath::FloorToInt(LocalTime / LaunchCelebrationSparkPeriod);
		const float BurstTime = LocalTime - BurstIndex * LaunchCelebrationSparkPeriod;
		if (LocalTime < 0.f || BurstIndex >= LaunchCelebrationSparkBursts || BurstTime >= LaunchCelebrationSparkDuration)
		{
			Sparkles[Index]->SetRenderOpacity(0.f);
			Sparkles[Index]->SetRenderTransform(Identity);
			continue;
		}
		const float Phase = FMath::Clamp(BurstTime / LaunchCelebrationSparkDuration, 0.f, 1.f);
		const float Drift = FWidgetAnimationUtils::EaseOutQuad(Phase);
		const float Direction = Index == 0 ? 1.f : -1.f;
		FWidgetAnimationUtils::DriveSparklePop(Sparkles[Index], Phase);
		FWidgetTransform SparkTransform = Sparkles[Index]->GetRenderTransform();
		SparkTransform.Scale *= BurstIndex == 0 ? 1.25f : .9f;
		SparkTransform.Translation = FVector2D(Direction * (BurstIndex * 80.f + Drift * 12.f),
			(BurstIndex == 1 ? 14.f : -6.f) - Drift * 18.f);
		SparkTransform.Angle *= Direction;
		Sparkles[Index]->SetRenderTransform(SparkTransform);
	}
}

void ULaunchConfirmWidget::BeginGradeStamp()
{
	ResetGradeStamp();
	if (!LaunchGradeStamp || !LaunchCoverSurface || !LaunchGradeStamp->IsVisible()
		|| USettingsManagerSubsystem::IsReduceMotion(this))
	{
		return;
	}
	GradeStampBaseTransform = LaunchGradeStamp->GetRenderTransform();
	GradeCoverBaseTransform = LaunchCoverSurface->GetRenderTransform();
	GradeStampBaseOpacity = LaunchGradeStamp->GetRenderOpacity();
	GradeStampElapsed = 0.f;
	bGradeStampActive = true;
	LaunchGradeStamp->SetRenderOpacity(0.f);
}

void ULaunchConfirmWidget::ResetGradeStamp()
{
	if (bGradeStampActive)
	{
		if (LaunchGradeStamp)
		{
			LaunchGradeStamp->SetRenderTransform(GradeStampBaseTransform);
			LaunchGradeStamp->SetRenderOpacity(GradeStampBaseOpacity);
		}
		if (LaunchCoverSurface) { LaunchCoverSurface->SetRenderTransform(GradeCoverBaseTransform); }
	}
	bGradeStampActive = false;
	GradeStampElapsed = 0.f;
}

void ULaunchConfirmWidget::TickGradeStamp(float DeltaTime)
{
	if (!bGradeStampActive) { return; }
	if (!LaunchGradeStamp || !LaunchCoverSurface || USettingsManagerSubsystem::IsReduceMotion(this))
	{
		ResetGradeStamp();
		return;
	}

	const float Fall = StrikeFx.StampFallTime;
	const float PreviousMotion = GradeStampElapsed - LaunchGradeStampDelay;
	GradeStampElapsed += FMath::Max(0.f, DeltaTime);
	const float MotionTime = GradeStampElapsed - LaunchGradeStampDelay;
	if (MotionTime < 0.f) { return; }
	const float SinceContact = MotionTime - Fall;
	if (PreviousMotion < Fall && SinceContact >= 0.f)
	{
		PlayRevealSound(FName("Event_LootPop"), .9f, true);
		// 착지 순간 배지 중심에서 충격을 터뜨린다. 배지는 이때 저작 위치에 있다.
		if (LaunchStampFx)
		{
			const FGeometry& FxGeometry = LaunchStampFx->GetCachedGeometry();
			const FGeometry& StampGeometry = LaunchGradeStamp->GetCachedGeometry();
			if (FxGeometry.GetLocalSize().X >= 1.f && StampGeometry.GetLocalSize().X >= 1.f)
			{
				ImpactCenter = FxGeometry.AbsoluteToLocal(StampGeometry.LocalToAbsolute(StampGeometry.GetLocalSize() * .5f));
				ImpactTime = 0.f;
			}
		}
	}
	if (SinceContact >= LaunchStampSettleEnd)
	{
		ResetGradeStamp();
		return;
	}

	// 위에서 크게 떨어지며 가속 → 착지에서 살짝 눌렸다 정착. 0.14초 낙하 동안만 배율이 1을 넘는다.
	float Scale, Angle;
	FVector2D Offset = FVector2D::ZeroVector;
	float Opacity = 1.f;
	if (SinceContact < 0.f)
	{
		const float Phase = MotionTime / Fall;
		const float Accel = Phase * Phase;
		Scale = FMath::Lerp(StrikeFx.StampStartScale, 1.f, Accel);
		Angle = FMath::Lerp(-20.f, -3.f, FWidgetAnimationUtils::EaseOutQuad(Phase));
		Offset = FMath::Lerp(LaunchStampFallOffset, FVector2D::ZeroVector, Accel);
		Opacity = FMath::Clamp(Phase / .25f, 0.f, 1.f);
	}
	else
	{
		Scale = SinceContact < LaunchStampSquashTime
			? FMath::Lerp(1.f, .92f, SinceContact / LaunchStampSquashTime)
			: FMath::Lerp(.92f, 1.f, StrikeEaseOutCubic((SinceContact - LaunchStampSquashTime) / LaunchStampSettleTime));
		Angle = FMath::Lerp(-3.f, 0.f, StrikeEaseOutCubic(SinceContact / LaunchStampSettleTime));
	}
	FWidgetTransform StampTransform = GradeStampBaseTransform;
	StampTransform.Scale *= Scale;
	StampTransform.Angle += Angle;
	StampTransform.Translation += Offset;
	LaunchGradeStamp->SetRenderTransform(StampTransform);
	LaunchGradeStamp->SetRenderOpacity(GradeStampBaseOpacity * Opacity);

	// 표지 묶음이 도장에 눌렸다 튀어 오른다
	FWidgetTransform CoverTransform = GradeCoverBaseTransform;
	if (SinceContact >= 0.f && SinceContact < LaunchGradeRecoilDuration)
	{
		const float Recoil = SinceContact / LaunchGradeRecoilDuration;
		CoverTransform.Translation.Y += Recoil < .25f ? FMath::Lerp(0.f, 12.f, Recoil / .25f)
			: Recoil < .6f ? FMath::Lerp(12.f, -4.f, (Recoil - .25f) / .35f)
			: FMath::Lerp(-4.f, 0.f, (Recoil - .6f) / .4f);
		CoverTransform.Translation.X += FMath::Sin(SinceContact * 90.f) * 4.4f * (1.f - Recoil);
	}
	LaunchCoverSurface->SetRenderTransform(CoverTransform);
}

void ULaunchConfirmWidget::CollectStrikeLayers()
{
	auto Gather = [this](const TCHAR* Prefix, int32 Count, TArray<TWeakObjectPtr<UImage>>& Out)
	{
		Out.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (UImage* Layer = Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("%s%02d"), Prefix, Index)))))
			{
				Out.Add(Layer);
			}
		}
	};
	Gather(TEXT("LaunchThrust"), StrikeThrustCount, ThrustBlades);
	Gather(TEXT("LaunchBurst"), StrikeBurstCount, BurstBlades);
	Gather(TEXT("LaunchTitleFlash"), 4, TitleFlashBlades);
	Gather(TEXT("LaunchImpactShard"), StrikeImpactShardCount, ImpactShards);
	Gather(TEXT("LaunchImpactFlash"), 4, ImpactFlashBlades);
	TitleGlow = Cast<UImage>(GetWidgetFromName(TEXT("LaunchTitleGlow")));
	ImpactRing = Cast<UImage>(GetWidgetFromName(TEXT("LaunchImpactRing")));
	ImpactGlow = Cast<UImage>(GetWidgetFromName(TEXT("LaunchImpactGlow")));
	if (LaunchBackdropFx && ThrustBlades.Num() != StrikeThrustCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] Strike FX canvas has %d/%d thrust blades"), ThrustBlades.Num(), StrikeThrustCount);
	}
}

void ULaunchConfirmWidget::ResetStrikeFx()
{
	StrikeTime = -1.f;
	ImpactTime = -1.f;
	for (const TArray<TWeakObjectPtr<UImage>>* Group : { &ThrustBlades, &BurstBlades, &TitleFlashBlades, &ImpactShards, &ImpactFlashBlades })
	{
		for (const TWeakObjectPtr<UImage>& Layer : *Group) { HideStrikeLayer(Layer); }
	}
	HideStrikeLayer(TitleGlow);
	HideStrikeLayer(ImpactRing);
	HideStrikeLayer(ImpactGlow);
}

void ULaunchConfirmWidget::TickStrikeFx(float DeltaTime)
{
	if (StrikeTime < 0.f || !LaunchBackdropFx || !LaunchCompletionHeader) { return; }
	if (USettingsManagerSubsystem::IsReduceMotion(this))
	{
		ResetStrikeFx();
		return;
	}
	// 첫 레이아웃 전에는 제목 위치가 0이다 — 시계를 세우고 다음 프레임에 시작한다
	const FGeometry& StageGeometry = LaunchBackdropFx->GetCachedGeometry();
	const FGeometry& TitleGeometry = LaunchCompletionHeader->GetCachedGeometry();
	if (StageGeometry.GetLocalSize().X < 1.f || TitleGeometry.GetLocalSize().X < 1.f) { return; }
	const FVector2D Center = StageGeometry.AbsoluteToLocal(
		TitleGeometry.LocalToAbsolute(TitleGeometry.GetLocalSize() * .5f));

	StrikeTime += FMath::Max(0.f, DeltaTime);
	const float Time = StrikeTime;
	const float Intensity = StrikeFx.Intensity;

	// 칼끝 여러 줄이 바깥에서 제목으로 가속하며 꽂히고, 닿으면 짧아지며 사라진다
	for (int32 Index = 0; Index < ThrustBlades.Num(); ++Index)
	{
		const float Radians = FMath::DegreesToRadians(StrikeThrustAngles[Index]);
		const FVector2D Direction(FMath::Cos(Radians), FMath::Sin(Radians) * StrikeVerticalSquash);
		const float Phase = (Time - .012f * (Index % 4)) / StrikeThrustTravel;
		if (Phase < 0.f)
		{
			DriveStrikeBlade(ThrustBlades[Index].Get(), Center, 0.f, 1.f, 1.f, 0.f);
			continue;
		}
		const float After = (Phase - 1.f) / .7f;
		const float Radius = Phase < 1.f ? FMath::Lerp(1800.f, 340.f, Phase * Phase)
			: FMath::Lerp(340.f, 280.f, FWidgetAnimationUtils::EaseOutQuad(FMath::Clamp(After, 0.f, 1.f)));
		const float Alpha = Phase < 1.f ? FMath::Clamp(Phase * 3.f, 0.f, 1.f) : 1.f - FMath::Clamp(After, 0.f, 1.f);
		const float Length = Phase < 1.f ? 760.f
			: FMath::Lerp(760.f, 120.f, FWidgetAnimationUtils::EaseOutQuad(FMath::Clamp(After, 0.f, 1.f)));
		DriveStrikeBlade(ThrustBlades[Index].Get(), Center + Direction * Radius,
			FMath::Atan2(-Direction.Y, -Direction.X), Length, 26.f, Alpha * Intensity);
	}

	DriveStrikeFlash(TitleFlashBlades, TitleGlow.Get(), Center, (Time - StrikeThrustTravel) / .22f, 300.f, 640.f, 0.f, Intensity);

	// 닿는 순간 짧은 칼끝이 바깥으로 튀어 나간다
	for (int32 Index = 0; Index < BurstBlades.Num(); ++Index)
	{
		const float Radians = FMath::DegreesToRadians(StrikeBurstAngles[Index]);
		const FVector2D Direction(FMath::Cos(Radians), FMath::Sin(Radians) * StrikeVerticalSquash);
		const float Phase = (Time - StrikeThrustTravel - Index * .006f) / .2f;
		const bool bLive = Phase >= 0.f && Phase <= 1.f;
		const float Clamped = FMath::Clamp(Phase, 0.f, 1.f);
		DriveStrikeBlade(BurstBlades[Index].Get(), Center + Direction * FMath::Lerp(180.f, 600.f, StrikeEaseOutCubic(Clamped)),
			FMath::Atan2(Direction.Y, Direction.X), FMath::Lerp(240.f, 60.f, Clamped), 16.f, bLive ? (1.f - Clamped) * Intensity : 0.f);
	}

	if (Time >= StrikeDuration)
	{
		// 제목 쪽 층만 거둔다 — 도장 충격은 자기 시계로 따로 끝난다
		for (const TArray<TWeakObjectPtr<UImage>>* Group : { &ThrustBlades, &BurstBlades, &TitleFlashBlades })
		{
			for (const TWeakObjectPtr<UImage>& Layer : *Group) { HideStrikeLayer(Layer); }
		}
		HideStrikeLayer(TitleGlow);
		StrikeTime = -1.f;
	}
}

void ULaunchConfirmWidget::TickImpactFx(float DeltaTime)
{
	if (ImpactTime < 0.f || !LaunchStampFx) { return; }
	ImpactTime += FMath::Max(0.f, DeltaTime);
	const float Phase = ImpactTime / StrikeImpactDuration;
	const float Intensity = StrikeFx.Intensity;
	if (Phase >= 1.f || USettingsManagerSubsystem::IsReduceMotion(this))
	{
		for (const TArray<TWeakObjectPtr<UImage>>* Group : { &ImpactShards, &ImpactFlashBlades })
		{
			for (const TWeakObjectPtr<UImage>& Layer : *Group) { HideStrikeLayer(Layer); }
		}
		HideStrikeLayer(ImpactRing);
		HideStrikeLayer(ImpactGlow);
		ImpactTime = -1.f;
		return;
	}
	const float Spread = StrikeEaseOutCubic(Phase);
	if (UImage* Ring = ImpactRing.Get())
	{
		const UCanvasPanelSlot* RingSlot = Cast<UCanvasPanelSlot>(Ring->Slot);
		const float MaxRadius = RingSlot ? RingSlot->GetSize().X * .5f : 172.f;
		const float Radius = FMath::Lerp(60.f, 172.f, Spread);
		DriveStrikeGlow(Ring, ImpactCenter, Radius / MaxRadius, Radius * .9f / MaxRadius, (1.f - Phase) * .9f * Intensity);
	}
	for (int32 Index = 0; Index < ImpactShards.Num(); ++Index)
	{
		const float Radians = Index * UE_TWO_PI / StrikeImpactShardCount + .3f;
		const FVector2D Direction(FMath::Cos(Radians), FMath::Sin(Radians) * .9f);
		DriveStrikeBlade(ImpactShards[Index].Get(), ImpactCenter + Direction * FMath::Lerp(68.f, 184.f, Spread),
			FMath::Atan2(Direction.Y, Direction.X), FMath::Lerp(80.f, 20.f, Phase), 12.f, (1.f - Phase) * .95f * Intensity);
	}
	DriveStrikeFlash(ImpactFlashBlades, ImpactGlow.Get(), ImpactCenter, Phase, 140.f, 260.f, UE_PI * .25f, Intensity);
}

void ULaunchConfirmWidget::DriveMultiplyFx(float SinceMultiply)
{
	const float Since = SinceMultiply;
	auto Glow = [](UBorder* Row, float Amount)
	{
		if (!Row) { return; }
		FLinearColor Color = LaunchRowGlowColor;
		Color.A *= FMath::Clamp(Amount, 0.f, 1.f);
		Row->SetBrushColor(Color);
	};
	// 1단계 동안 기본 수익 줄, 곱하는 순간부터 배율 줄이 빛난다
	Glow(RevRowGlow_Base, RevealClock >= RevenueCountStartAt ? (Since < 0.f ? 1.f : 1.f - Since / .12f) : 0.f);
	Glow(RevRowGlow_Multiplier, Since >= 0.f ? (Since < .5f ? 1.f : 1.f - (Since - .5f) / .4f) : 0.f);

	if (Text_TotalRevenue && bTotalColorCaptured)
	{
		const float Gold = Since < 0.f ? 0.f : Since < .45f ? 1.f : 1.f - FMath::Clamp((Since - .45f) / .5f, 0.f, 1.f);
		const FLinearColor Base = TotalRevenueBaseColor.IsColorSpecified() ? TotalRevenueBaseColor.GetSpecifiedColor() : FLinearColor::White;
		Text_TotalRevenue->SetColorAndOpacity(Gold > 0.f ? FSlateColor(FMath::Lerp(Base, LaunchMultiplyGold, Gold)) : TotalRevenueBaseColor);
		const float Pop = Since >= LaunchMultiplyDelay && Since < LaunchMultiplyDelay + .2f
			? FMath::Lerp(.93f, 1.f, StrikeEaseOutCubic((Since - LaunchMultiplyDelay) / .2f)) : 1.f;
		Text_TotalRevenue->SetRenderScale(FVector2D(Pop, Pop));
	}

	if (!LaunchFrontFx || !Text_TotalRevenue) { return; }
	const FGeometry& FxGeometry = LaunchFrontFx->GetCachedGeometry();
	const FGeometry& TotalGeometry = Text_TotalRevenue->GetCachedGeometry();
	if (FxGeometry.GetLocalSize().X < 1.f || TotalGeometry.GetLocalSize().X < 1.f) { return; }
	// 칩은 총액 글자의 오른쪽 위 모서리에 걸친다
	const FVector2D ChipAt = FxGeometry.AbsoluteToLocal(
		TotalGeometry.LocalToAbsolute(FVector2D(TotalGeometry.GetLocalSize().X, 0.f))) + FVector2D(18.f, 6.f);
	if (LaunchMulChip)
	{
		const float Appear = Since - LaunchMultiplyDelay;
		const float Alpha = Appear < 0.f ? 0.f : Appear < .05f ? Appear / .05f : 1.f - FMath::Clamp((Since - .75f) / .35f, 0.f, 1.f);
		const float Scale = FMath::Lerp(.92f, 1.f, StrikeEaseOutCubic(Appear / .12f));
		LaunchMulChip->SetRenderTransform(FWidgetTransform(ChipAt, FVector2D(Scale, Scale), FVector2D::ZeroVector, 0.f));
		LaunchMulChip->SetRenderOpacity(Alpha);
	}
	// 도장에서 칩으로 칼끝 한 줄이 쏘아져 간다
	if (LaunchMulStreak && LaunchGradeStamp && Since >= 0.f && Since < .2f)
	{
		const FGeometry& StampGeometry = LaunchGradeStamp->GetCachedGeometry();
		const FVector2D From = FxGeometry.AbsoluteToLocal(StampGeometry.LocalToAbsolute(StampGeometry.GetLocalSize() * .5f));
		const FVector2D Delta = ChipAt - From;
		const float Distance = Delta.Size();
		if (Distance > 1.f)
		{
			const FVector2D Direction = Delta / Distance;
			const float Head = Distance * StrikeEaseOutCubic(Since / .07f);
			const float Alpha = Since < .07f ? 1.f : 1.f - (Since - .07f) / .13f;
			DriveStrikeBlade(LaunchMulStreak, From + Direction * Head, FMath::Atan2(Direction.Y, Direction.X),
				FMath::Min(Head, Distance * .6f), 22.f, Alpha * StrikeFx.Intensity);
		}
	}
	else if (LaunchMulStreak)
	{
		HideStrikeLayer(LaunchMulStreak);
	}
}

void ULaunchConfirmWidget::FinishRevenueFx()
{
	for (UBorder* Row : { RevRowGlow_Base, RevRowGlow_Multiplier })
	{
		if (!Row) { continue; }
		FLinearColor Color = LaunchRowGlowColor;
		Color.A = 0.f;
		Row->SetBrushColor(Color);
	}
	if (Text_TotalRevenue && bTotalColorCaptured)
	{
		Text_TotalRevenue->SetColorAndOpacity(TotalRevenueBaseColor);
		Text_TotalRevenue->SetRenderScale(FVector2D(1.f, 1.f));
	}
	if (LaunchMulChip) { LaunchMulChip->SetRenderOpacity(0.f); }
	HideStrikeLayer(LaunchMulStreak);
	for (const TWeakObjectPtr<UDisciplineBarWidget>& Bar : RevealBars)
	{
		if (UDisciplineBarWidget* DiscBar = Bar.Get())
		{
			DiscBar->SetRevealProgress(1.f);
			DiscBar->SetFillFlash(0.f);
		}
	}
}

bool ULaunchConfirmWidget::TickRevealBars(float Clock)
{
	bool bBusy = false;
	for (int32 Index = 0; Index < RevealBars.Num(); ++Index)
	{
		UDisciplineBarWidget* DiscBar = RevealBars[Index].Get();
		if (!DiscBar) { continue; }
		const float Start = RevealBarStarts[Index];
		const float Phase = (Clock - Start) / LaunchBarRevealTime;
		DiscBar->SetRevealProgress(StrikeEaseOutCubic(Phase));
		float Flash = 0.f;
		const float Ratio = DiscBar->GetAchievedRatio();
		if (Ratio >= 1.f)
		{
			// ease-out cubic 이 1/Ratio 에 닿는 시각 = 표시값이 100%를 넘는 순간
			const float Cross = Start + LaunchBarRevealTime * (1.f - FMath::Pow(1.f - 1.f / Ratio, 1.f / 3.f));
			const float SinceCross = Clock - Cross;
			Flash = SinceCross >= 0.f ? 1.f - FMath::Clamp(SinceCross / LaunchBarFlashTime, 0.f, 1.f) : 0.f;
			bBusy |= SinceCross < LaunchBarFlashTime;
		}
		DiscBar->SetFillFlash(Flash);
		bBusy |= Phase < 1.f;
	}
	return bBusy;
}

void ULaunchConfirmWidget::QueueRevenueReveal(int64 TotalRevenue)
{
	// 큐/시계/탭 스킵이 NativeTick 에 이미 물려 있다 — 상태만 되감아 재사용한다.
	RevealQueue.Reset();
	RevealClock = 0.0f;
	bRevealActive = false;
	RevenueCountStartAt = -1.0f;
	RevealEndTime = 0.0f;
	LastRevealSoundClock = -10.0f;
	UpdateLaunchCelebration(-1.f);
	BeginGradeStamp();
	ResetStrikeFx();
	StrikeTime = LaunchBackdropFx && !USettingsManagerSubsystem::IsReduceMotion(this) ? 0.f : -1.f;
	QueueReveal(LaunchCompletionHeader, 0.f, true);
	if (LaunchLootRow)
	{
		const TArray<float> CardTimes = LaunchRewardRevealMath::StampRunTimes(
			LaunchLootRow->GetChildrenCount(), .62f, .12f, .9f, .10f);
		for (int32 Index = 0; Index < CardTimes.Num(); ++Index)
		{
			QueueReveal(LaunchLootRow->GetChildAt(Index), CardTimes[Index], true, FName("Event_LootPop"),
				false, 0.f, .65f, true);
		}
	}
	RevealEndTime = FMath::Max(RevealEndTime, LaunchCelebrationDuration);

	// 요약 총액은 상세 계산 행과 독립적으로 먼저 드러난다.
	const float StartAt = 0.12f;
	if (RevTotalBlock)
	{
		QueueReveal(RevTotalBlock, StartAt, false, FName("Event_ScoreSweep"));
	}
	if (Text_TotalRevenue && TotalRevenue > 0)
	{
		RevenueCountTarget = TotalRevenue;
		RevenueBaseTarget = PendingBaseRevenue;
		RevenueCountStartAt = StartAt;
		RevenueMultiplyAt = LaunchGradeStampDelay + StrikeFx.StampFallTime;
		if (!bTotalColorCaptured)
		{
			TotalRevenueBaseColor = Text_TotalRevenue->GetColorAndOpacity();
			bTotalColorCaptured = true;
		}
		Text_TotalRevenue->SetText(FText::AsNumber(static_cast<int64>(0)));
		RevealEndTime = FMath::Max(RevealEndTime, RevenueMultiplyAt + LaunchRevenueMultiplyEnd);
	}
	RevealBars.Reset();
	RevealBarStarts.Reset();
	for (UDisciplineBarWidget* DiscBar : { DiscBar1, DiscBar2, DiscBar3, DiscBar4, DiscBar5, DiscBar6 })
	{
		if (!DiscBar || !DiscBar->IsActiveDiscipline()) { continue; }
		RevealBarStarts.Add(LaunchBarRevealStart + LaunchBarRevealStagger * RevealBars.Num());
		RevealBars.Add(DiscBar);
		DiscBar->SetRevealProgress(0.f);
		DiscBar->SetFillFlash(0.f);
	}
	if (RevealBarStarts.Num() > 0)
	{
		RevealEndTime = FMath::Max(RevealEndTime, RevealBarStarts.Last() + LaunchBarRevealTime + LaunchBarFlashTime);
	}

	// 확인 버튼은 잠그지 않는다 — 여기 숫자는 못 보고 눌러도 손해가 없고,
	// 매 프로젝트마다 반복되는 화면이라 대기를 강제하면 금방 성가셔진다. 탭하면 즉시 완료된다.

	// QueueReveal 이 총액 블록을 opacity 0 으로 눕혀놨으므로, 이 줄이 없으면 NativeTick 이 조기반환해 영영 안 깨운다.
	// 카운트업 대상(Text_TotalRevenue)은 큐에 안 들어가므로 별도 조건 — RevTotalBlock 미배치 시에도 총액은 굴러야 한다.
	bRevealActive = RevealQueue.Num() > 0 || RevenueCountStartAt >= 0.0f || RevealBars.Num() > 0
		|| LaunchFlare || LaunchSpark1 || LaunchSpark2;
	if (USettingsManagerSubsystem::IsReduceMotion(this)) SkipRevealQueue();
}

void ULaunchConfirmWidget::FillFailureSection(const FStageProgressData& StageData)
{
	// 사내 테스트 리포트 — 미달 분야만 게이지 행으로. 채움 = 획득/목표,
	// 최소선 마커는 WBP 고정 50%(MinimumScore = TargetScore×0.5 계약 — 출시실패 시스템 §1).
	UStatRowWidget* FailRows[6] = { FailStatRow1, FailStatRow2, FailStatRow3, FailStatRow4, FailStatRow5, FailStatRow6 };
	TArray<FString> ShortNames;
	int32 RowIdx = 0;
	for (const FStepRoundData& S : StageData.Steps)
	{
		if (S.DisciplineSlot == INDEX_NONE || S.HasPassedMinimum())
		{
			continue;
		}
		// UI 표기는 "분야" — 코드/기획 용어인 직능(EProductionDiscipline)을 그대로 노출하지 않는다
		const FString DisciplineName = S.StepName.IsEmpty() ? TEXT("분야") : S.StepName;
		ShortNames.Add(DisciplineName);
		if (RowIdx < 6 && FailRows[RowIdx])
		{
			FailRows[RowIdx]->SetVisibility(ESlateVisibility::HitTestInvisible);
			// SetCurrentProgress 가 StatValueText 를 "획득/목표"로 덮으므로 바 먼저, 표기("획득 / 최소")는 나중
			FailRows[RowIdx]->SetCurrentProgress(S.AcquiredScore, S.TargetScore);
			FailRows[RowIdx]->SetStatInfo(DisciplineName, FString::Printf(TEXT("%d / %d"),
				FMath::FloorToInt(S.AcquiredScore), FMath::CeilToInt(S.MinimumScore)));
			// SetStatIcon 이 IconTexture 까지 갱신해서 늦은 ApplyIconProperties 가 도로 접지 않는다
			FailRows[RowIdx]->SetStatIcon(UGlobalUtilFunctions::GetDisciplineIcon(S.DisciplineSlot));
			++RowIdx;
		}
	}
	for (int32 Index = RowIdx; Index < 6; ++Index)
	{
		if (FailRows[Index]) { FailRows[Index]->SetVisibility(ESlateVisibility::Collapsed); }
	}

	if (Text_FailSummary)
	{
		Text_FailSummary->SetText(FText::FromString(
			FString::Printf(TEXT("%s 분야가 최소 기준에 미치지 못했습니다 ― 추가 개발로 보완할 수 있습니다."),
				*FString::Join(ShortNames, TEXT(" · ")))));
	}

	// 테스터 의견 2장 — 실패 페이지의 서사(출시 전이라 외부 반응 금지). Worst 컨텍스트 행이 우선 후보.
	UGameInstance* TokenGI = GetGameInstance();
	UTableManagerSubsystem* TokenTableMgr = TokenGI ? TokenGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FReviewTokenValues TesterTokens;
	TSet<FName> TesterContexts;
	ULaunchReactionSubsystem::BuildReviewTokens(StageData, TokenTableMgr, TesterTokens, TesterContexts);
	TArray<FText> TesterNicks, TesterComments;
	if (TokenTableMgr)
	{
		TokenTableMgr->GetReviewComments(FName(TEXT("Tester")), StageData.CompanyType, FName(TEXT("Fail")),
			TesterContexts, 2, TesterTokens, TesterNicks, TesterComments);
	}
	UReviewCardWidget* TesterCards[2] = { TesterCard1, TesterCard2 };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UReviewCardWidget* Card = TesterCards[Index];
		if (!Card) { continue; }
		if (!TesterNicks.IsValidIndex(Index))
		{
			Card->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
		Card->SetSnsData(TesterNicks[Index], TesterComments[Index]);   // 사내 직함 — @ 접두사 없음
	}

	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	const int32 Cost = StageMgr ? StageMgr->GetRetryDiamondCost() : 0;
	const bool bRetryLeft = StageMgr && StageMgr->CanRetryDevelopment();

	bool bCanAfford = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			bCanAfford = ResMgr->HasResource(EResourceType::Diamond, Cost);
		}
	}

	if (RetryCostChip)
	{
		RetryCostChip->SetVisibility(bRetryLeft ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		RetryCostChip->SetResourceType(EResourceType::Diamond);
		RetryCostChip->SetValue(Cost);
		RetryCostChip->SetCanAfford(bCanAfford);
	}

	if (ConfirmCancelWidget)
	{
		// 실패 페이지는 자체 타이틀(FailTitle)을 가지므로 프레임 제목을 비운다 —
		// 제목을 갈아끼우지 않는 이유: TitleText 가 이 WBP 에서 Collapsed 로 baked 됐을 수 있고,
		// 그러면 SetTitle 은 가시성을 못 바꿔 타이틀이 통째로 사라진다. 비우기는 어느 쪽이든 안전하다.
		ConfirmCancelWidget->SetTitle(FText::GetEmpty());

		// 프레임의 두 버튼을 재해석 — 확인 = 추가 개발, 취소 = 폐기
		ConfirmCancelWidget->SetConfirmButtonText(FText::FromString(TEXT("추가 개발")));
		ConfirmCancelWidget->SetCancelButtonText(FText::FromString(TEXT("폐기")));
		// 컨티뉴를 이미 썼거나 다이아가 모자라면 남는 출구는 폐기뿐
		ConfirmCancelWidget->SetCancelOnly(!bRetryLeft || !bCanAfford);
	}

	// 실패·추가 개발에도 현재 프로젝트에서 실제 적립한 총량을 보여 준다.
	if (Text_FailXp)
	{
		const float Earned = StageData.EarnedExperience;
		if (!FMath::IsFinite(Earned) || Earned <= 0.0f)
		{
			// 지급이 없었으면 성장을 말할 근거가 없다 — "+0" 을 남기느니 줄을 접는다
			Text_FailXp->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Text_FailXp->SetText(FormatEarnedExperience(Earned));
			Text_FailXp->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	else if (!bFailXpWarned)
	{
		bFailXpWarned = true;
		UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] Text_FailXp 미바인딩 ― pb_failxp.py 실행 필요"));
	}
}

void ULaunchConfirmWidget::UpdateStampMaterialSize()
{
	if (!StampBG && !StampLine && !StampGlint)
	{
		return;
	}

	// 스탬프는 캡션 길이에 따라 폭이 달라진다 — 위젯을 고정하면 글자가 넘치므로(2026-07-29 실측)
	// 위젯은 콘텐츠 크기로 두고 SDF 에 실제 크기를 넣는다.
	const FVector2D LocalSize = StampBG ? StampBG->GetCachedGeometry().GetLocalSize()
		: StampLine->GetCachedGeometry().GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastStampMatSize, 0.5f))
	{
		return;
	}
	LastStampMatSize = LocalSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (UImage* Layer : { StampBG, StampLine, StampGlint })
	{
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

void ULaunchConfirmWidget::ApplyGradeMedal(ECompanyType CompanyType, EQualityGrade Grade)
{
	CGQualityGradeMedal::Apply(GetGameInstance(), CompanyType, Grade, LetterPlate, Text_QualityGrade);
	if (LaunchGradeStamp)
	{
		LaunchGradeStamp->SetVisibility(LetterPlate && LetterPlate->GetVisibility() != ESlateVisibility::Collapsed
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ULaunchConfirmWidget::ApplyGradeStampColor(const FLinearColor& GradeColor)
{
	// 스탬프는 SDF 머티리얼 3층이라 SetBrushColor 가 아니라 MID 파라미터로 색이 들어간다.
	// RoundedBox 가 단색 채움만 돼서 그라데이션/글린트가 안 나오는 것이 머티리얼로 간 이유.
	FLinearColor Deep = GradeColor * 0.62f;
	Deep.A = 1.0f;

	if (StampBG)
	{
		if (UMaterialInstanceDynamic* MID = StampBG->GetDynamicMaterial())
		{
			// M_UI_ChipBG 의 채움 파라미터는 TintCol 하나뿐이다. 없는 이름에 넣으면 조용히 무시돼
			// 모든 등급이 baked 색으로 남는다 (M_UIPanel_Rounded 의 FillTop/FillBottom 과 혼동 주의).
			// 세로 그라데이션은 StampSheen/StampShade 두 장이 담당.
			MID->SetVectorParameterValue(TEXT("TintCol"), GradeColor);
		}
	}
	if (StampLine)
	{
		if (UMaterialInstanceDynamic* MID = StampLine->GetDynamicMaterial())
		{
			MID->SetVectorParameterValue(TEXT("LineCol"), Deep);
		}
	}

}

void ULaunchConfirmWidget::QueueReveal(UWidget* Widget, float StartAt, bool bPop, FName SoundKey,
	bool bGrand, float SoundLead, float SoundVolume, bool bSkipSilent)
{
	if (!Widget) { return; }
	// 레이아웃 자리는 유지한 채 투명으로 대기 — 등장 순간까지 리플로 없음
	Widget->SetRenderOpacity(0.0f);
	if (bPop)
	{
		const float PopScale = bGrand ? RevealPopScaleGrand : RevealPopScale;
		Widget->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(PopScale, PopScale), FVector2D::ZeroVector, 0.0f));
	}
	else
	{
		Widget->SetRenderTranslation(FVector2D(RevealSlideOffsetX, 0.0f));
	}
	FRevealItem Item;
	Item.Widget = Widget;
	Item.StartAt = StartAt;
	Item.bPop = bPop;
	Item.bGrand = bGrand;
	Item.SoundKey = SoundKey;
	Item.SoundLead = SoundLead;
	Item.SoundVolume = SoundVolume;
	Item.bSkipSilent = bSkipSilent;
	RevealQueue.Add(Item);

	RevealEndTime = FMath::Max(RevealEndTime, StartAt + Item.Duration);
}

void ULaunchConfirmWidget::PlayRevealSound(FName SoundKey, float VolumeScale, bool bForce)
{
	if (SoundKey.IsNone()) { return; }
	// 연속 등장 구간에서 클립이 뭉쳐 스팸이 되는 걸 차단 (클라이맥스는 예외)
	if (!bForce && (RevealClock - LastRevealSoundClock) < RevealSoundMinInterval) { return; }

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlaySoundWithVolume(SoundKey, VolumeScale);
		}
	}
	LastRevealSoundClock = RevealClock;
}

void ULaunchConfirmWidget::SetConfirmInteractionEnabled(bool bEnable)
{
	if (UCommonButtonBase* ConfirmBtn = Cast<UCommonButtonBase>(GetConfirmButtonWidget()))
	{
		ConfirmBtn->SetIsInteractionEnabled(bEnable);
	}
}

void ULaunchConfirmWidget::SkipRevealQueue()
{
	ResetGradeStamp();
	ResetStrikeFx();
	if (!bRevealActive) { return; }

	for (FRevealItem& Item : RevealQueue)
	{
		// 미재생 사운드는 버린다 — 단 클라이맥스(펀치)는 1회 보상 재생.
		// bSkipSilent 항목은 제외 — 여러 건이 한 프레임에 뭉쳐 터진다
		if (!Item.bStarted)
		{
			Item.bStarted = true;
			if (Item.bPop && !Item.bSkipSilent) { PlayRevealSound(Item.SoundKey, Item.SoundVolume, true); }
		}
		if (UWidget* W = Item.Widget.Get())
		{
			W->SetRenderOpacity(1.0f);
			W->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.0f, 1.0f), FVector2D::ZeroVector, 0.0f));
		}
	}

	// 카운트업은 NativeTick 이 굴리는데 여기서 bRevealActive 를 끄므로, 최종값은 스킵이 직접 확정해야 한다
	if (Text_TotalRevenue && RevenueCountStartAt >= 0.0f && RevenueCountTarget > 0)
	{
		Text_TotalRevenue->SetText(FText::AsNumber(RevenueCountTarget));
	}

	FinishRevenueFx();
	RevealClock = RevealEndTime;
	UpdateLaunchCelebration(-1.f);
	bRevealActive = false;
	SetConfirmInteractionEnabled(true);
}

FReply ULaunchConfirmWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 리빌 중 탭 = 스킵 (최종 상태로 시간 점프)
	if (bRevealActive || bGradeStampActive || StrikeTime >= 0.f)
	{
		SkipRevealQueue();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void ULaunchConfirmWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (EntryElapsed < .2f)
	{
		EntryElapsed = USettingsManagerSubsystem::IsReduceMotion(this) ? .2f : FMath::Min(.2f, EntryElapsed + InDeltaTime);
		if (UWidget* Surface = EntrySurface.Get())
		{
			const float Ease = 1.f - FMath::Pow(1.f - EntryElapsed / .2f, 3.f);
			FWidgetTransform Transform = EntryBaseTransform;
			Transform.Translation.Y += 24.f * (1.f - Ease);
			Surface->SetRenderTransform(Transform);
			Surface->SetRenderOpacity(EntryBaseOpacity * Ease);
		}
	}
	if (bRevealActive && USettingsManagerSubsystem::IsReduceMotion(this)) SkipRevealQueue();
	TickGradeStamp(InDeltaTime);
	TickStrikeFx(InDeltaTime);
	TickImpactFx(InDeltaTime);

	// ⚠ 아래 리빌 조기반환보다 앞에 — 미리보기 페이지(리빌 비활성)에서도 스탬프 크기는 맞아야 한다
	UpdateStampMaterialSize();

	if (!bRevealActive) { return; }
	RevealClock += InDeltaTime;
	UpdateLaunchCelebration(RevealClock);

	bool bAllDone = true;
	for (FRevealItem& Item : RevealQueue)
	{
		UWidget* W = Item.Widget.Get();
		if (!W) { continue; }

		// 사운드 판정은 T<0 early-out 앞에서 — SoundLead 만큼 앞서 재생해 어택이 착지에 겹치게 한다
		if (!Item.bStarted && RevealClock >= (Item.StartAt - Item.SoundLead))
		{
			Item.bStarted = true;
			PlayRevealSound(Item.SoundKey, Item.SoundVolume, Item.bPop);
		}

		const float T = (RevealClock - Item.StartAt) / Item.Duration;
		if (T < 0.0f) { bAllDone = false; continue; }

		const float Clamped = FMath::Clamp(T, 0.0f, 1.0f);
		const float Ease = 1.0f - FMath::Pow(1.0f - Clamped, 3.0f);   // ease-out cubic
		W->SetRenderOpacity(Ease);
		if (Item.bPop)
		{
			const float PopStart = Item.bGrand ? RevealPopScaleGrand : RevealPopScale;
			const float Scale = PopStart - (PopStart - 1.0f) * Ease;
			W->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Scale, Scale), FVector2D::ZeroVector, 0.0f));
		}
		else
		{
			W->SetRenderTranslation(FVector2D(RevealSlideOffsetX * (1.0f - Ease), 0.0f));
		}
		if (Clamped < 1.0f) { bAllDone = false; }
	}

	// 예상 총수익: 0 → 기본 수익, 도장 착지에 배율을 곱해 → 총액
	if (Text_TotalRevenue && RevenueCountStartAt >= 0.0f && RevenueCountTarget > 0)
	{
		const float Since = RevealClock - RevenueMultiplyAt;
		const double Base = static_cast<double>(RevenueBaseTarget);
		const double Value = Since < 0.0f
			? Base * StrikeEaseOutCubic((RevealClock - RevenueCountStartAt) / LaunchRevenueBaseCountTime)
			: Base + (static_cast<double>(RevenueCountTarget) - Base)
				* StrikeEaseOutCubic((Since - LaunchMultiplyDelay) / LaunchRevenueMultiplyCountTime);
		Text_TotalRevenue->SetText(FText::AsNumber(static_cast<int64>(FMath::RoundToDouble(Value))));
		DriveMultiplyFx(Since);
		if (Since < LaunchRevenueMultiplyEnd) { bAllDone = false; }
	}
	if (TickRevealBars(RevealClock)) { bAllDone = false; }
	if (RevealClock < LaunchCelebrationDuration && (LaunchFlare || LaunchSpark1 || LaunchSpark2))
	{
		bAllDone = false;
	}

	if (bAllDone)
	{
		FinishRevenueFx();
		UpdateLaunchCelebration(-1.f);
		bRevealActive = false;
		SetConfirmInteractionEnabled(true);
	}
}

void ULaunchConfirmWidget::OnDialogConfirmed()
{
	if (bManufacturingResearchResult)
	{
		if (bManufacturingResultAnswered) return;
		bManufacturingResultAnswered = true;
		DeactivateWidget();
		OnManufacturingResearchResultClosed.Broadcast(true);
		return;
	}
	// 실패 페이지에서는 확인 = [추가 개발]. 개발이 재개되므로 모달은 닫는다.
	// ⚠ 닫기 먼저, broadcast 나중 — 핸들러가 다음 모달을 push 할 때 이 위젯이 스택 top 이어야 잔류/부활이 없다.
	DeactivateWidget();
	if (bFailurePhase)
	{
		OnLaunchRetryRequested.Broadcast();
		return;
	}
	// [출시하기] = 공개한 보상 지급과 운영 전환을 한 번에 확정한다.
	OnLaunchConfirmed.Broadcast();
}

void ULaunchConfirmWidget::OnDialogCancelled()
{
	if (bManufacturingResearchResult)
	{
		if (bManufacturingResultAnswered) return;
		bManufacturingResultAnswered = true;
		DeactivateWidget();
		OnManufacturingResearchResultClosed.Broadcast(false);
		return;
	}
	OnLaunchCancelled.Broadcast();
	DeactivateWidget();
}

void ULaunchConfirmWidget::OnCloseBtnClicked()
{
	OnDialogCancelled();
}
