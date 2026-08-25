#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "UI/HUD/GuideTooltipPlacement.h"
#include "UI/Element/Common/GuideTooltipWidget.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Materials/MaterialInstanceDynamic.h"

const TCHAR* UPanelIntroOverlayWidget::CutoutMaterialPath =
	TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_SpotlightCutout.M_UI_SpotlightCutout");

// DT 컬럼용 enum 은 GuideTooltipPlacement.h 의 것을 그대로 못 쓴다(UENUM 아님) — static_cast 로 넘기므로 순서가 계약이다
static_assert(static_cast<uint8>(EPanelIntroTailDir::Up) == static_cast<uint8>(EGuideTooltipDir::Up), "TailDir 순서 불일치");
static_assert(static_cast<uint8>(EPanelIntroTailDir::Down) == static_cast<uint8>(EGuideTooltipDir::Down), "TailDir 순서 불일치");
static_assert(static_cast<uint8>(EPanelIntroTailDir::Left) == static_cast<uint8>(EGuideTooltipDir::Left), "TailDir 순서 불일치");
static_assert(static_cast<uint8>(EPanelIntroTailDir::Right) == static_cast<uint8>(EGuideTooltipDir::Right), "TailDir 순서 불일치");

bool UPanelIntroOverlayWidget::StartIntro(FName InPanelKey, UUserWidget* InOwnerPanel)
{
	PanelKey = InPanelKey;
	OwnerPanel = InOwnerPanel;

	const UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TArray<FPanelIntroTable>* Found = TableMgr ? TableMgr->GetPanelIntroSteps(PanelKey) : nullptr;
	if (!Found || Found->Num() == 0 || !InOwnerPanel)
	{
		return false;
	}
	Steps = *Found;

	EnsureLayers();
	if (AdvanceButton)
	{
		AdvanceButton->OnClicked.RemoveDynamic(this, &UPanelIntroOverlayWidget::HandleAdvanceClicked);
		AdvanceButton->OnClicked.AddDynamic(this, &UPanelIntroOverlayWidget::HandleAdvanceClicked);
	}

	EnsureCutoutMID();
	StepCursor = 0;
	ShowStep(0);

	// 전 스텝의 앵커를 못 찾으면 ShowStep 이 곧장 종료로 흘러간다 — 그 경우 재생 자체가 없었던 것으로 본다
	return !bFinished;
}

void UPanelIntroOverlayWidget::NativeDestruct()
{
	if (AdvanceButton)
	{
		AdvanceButton->OnClicked.RemoveDynamic(this, &UPanelIntroOverlayWidget::HandleAdvanceClicked);
	}
	// 예외 경로로 죽어도 미션 오버레이가 영구히 숨겨지지 않도록 복구 신호는 반드시 한 번 나간다
	if (!bFinished)
	{
		bFinished = true;
		OnIntroFinished.Broadcast();
	}
	Super::NativeDestruct();
}

void UPanelIntroOverlayWidget::EnsureLayers()
{
	if (DimImage && AdvanceButton)
	{
		return;
	}
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!RootCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PanelIntro] 루트가 CanvasPanel 이 아니라 딤/탭 레이어를 못 만듭니다"));
		return;
	}

	auto StretchFull = [](UCanvasPanelSlot* CSlot, int32 Z)
	{
		if (!CSlot) { return; }
		CSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CSlot->SetOffsets(FMargin(0.f));
		CSlot->SetAlignment(FVector2D::ZeroVector);
		CSlot->SetZOrder(Z);
	};

	if (!DimImage && WidgetTree)
	{
		DimImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		// 리소스 없는 브러시는 흰 풀스크린 사각형으로 그려진다 — MID 연결에 성공해야 보인다
		DimImage->SetVisibility(ESlateVisibility::Collapsed);
		StretchFull(RootCanvas->AddChildToCanvas(DimImage), DimZOrder);
	}
	if (!AdvanceButton && WidgetTree)
	{
		AdvanceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

		// 화면 전체를 덮되 보이지 않아야 한다 — 기본 버튼 배경이 그대로면 회백 사각이 화면을 덮는다
		FSlateBrush ClearBrush;
		ClearBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle ClearStyle;
		ClearStyle.SetNormal(ClearBrush).SetHovered(ClearBrush).SetPressed(ClearBrush).SetDisabled(ClearBrush);
		AdvanceButton->SetStyle(ClearStyle);

		StretchFull(RootCanvas->AddChildToCanvas(AdvanceButton), 0);
	}
}

void UPanelIntroOverlayWidget::EnsureCutoutMID()
{
	if (CutoutMID || bCutoutLoadAttempted)
	{
		return;
	}
	bCutoutLoadAttempted = true;

	// 중괄호 초기화 — 괄호면 함수 선언으로 파싱된다(most vexing parse)
	const TSoftObjectPtr<UMaterialInterface> MatPath{ FSoftObjectPath(CutoutMaterialPath) };
	UMaterialInterface* BaseMat = MatPath.LoadSynchronous();
	if (!BaseMat)
	{
		UE_LOG(LogTemp, Error, TEXT("[PanelIntro] 컷아웃 머티리얼 로드 실패: %s — 패키지 미포함 의심. 딤 생략."), CutoutMaterialPath);
		return;
	}

	CutoutMID = UMaterialInstanceDynamic::Create(BaseMat, this);
	if (DimImage && CutoutMID)
	{
		DimImage->SetBrushFromMaterial(CutoutMID);
		// 브러시가 붙은 뒤에야 보인다 (흰 사각형 방지). 딤이 입력을 먹으면 탭이 죽어 소프트락
		DimImage->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 이 오버레이는 항상 구멍 1개다 — 2·3번 구멍 파라미터를 여기서 한 번 눌러두지 않으면 유령 구멍이 남는다
		for (const TCHAR* Name : { TEXT("HoleHW2"), TEXT("HoleHH2"), TEXT("HoleHW3"), TEXT("HoleHH3") })
		{
			CutoutMID->SetScalarParameterValue(Name, 0.0f);
		}
		CutoutMID->SetScalarParameterValue(TEXT("RadiusPx"), HoleRadiusPx);
		CutoutMID->SetScalarParameterValue(TEXT("FeatherPx"), HoleFeatherPx);
		CutoutMID->SetScalarParameterValue(TEXT("DimA"), DimAlpha);
	}
}

void UPanelIntroOverlayWidget::ShowStep(int32 Index)
{
	UUserWidget* Owner = OwnerPanel.Get();
	if (!Owner)
	{
		FinishIntro();
		return;
	}

	// 앵커를 못 찾은 스텝은 건너뛴다 — 배선 실수를 조용히 삼키지 않고 경고를 남긴다
	while (Steps.IsValidIndex(Index))
	{
		const FPanelIntroTable& Step = Steps[Index];
		if (UWidget* Anchor = ResolveAnchor(Owner, Step.AnchorName))
		{
			StepCursor = Index;
			CurrentAnchor = Anchor;

			if (!Tooltip)
			{
				const UTableManagerSubsystem* TableMgr = GetGameInstance()
					? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
				TSubclassOf<UUserWidget> TipClass = TableMgr
					? TableMgr->GetWidgetClass(EWidgetType::GuideTooltip) : nullptr;
				if (TipClass)
				{
					Tooltip = CreateWidget<UGuideTooltipWidget>(GetOwningPlayer(), TipClass);
					if (Tooltip)
					{
						// 탭은 전면 버튼이 받는다 — 툴팁이 입력을 먹으면 그 위에서 다음으로 못 넘어간다
						Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
						if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget()))
						{
							if (UCanvasPanelSlot* TipSlot = RootCanvas->AddChildToCanvas(Tooltip))
							{
								TipSlot->SetAutoSize(true);
								TipSlot->SetAlignment(FVector2D::ZeroVector);
							}
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[PanelIntro] EWidgetType::GuideTooltip 미등록 — 말풍선 생략(딤/구멍/탭은 정상)"));
				}
			}

			if (Tooltip)
			{
				Tooltip->SetContent(Step.Eyebrow, Step.Body);
			}
			PendingTailDir = Step.TailDir;
			bTooltipPlacePending = true;
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[PanelIntro] '%s' 스텝 %d 앵커 '%s' 를 패널에서 찾지 못해 건너뜁니다"),
			*PanelKey.ToString(), Step.StepIndex, *Step.AnchorName.ToString());
		++Index;
	}

	FinishIntro();
}

UWidget* UPanelIntroOverlayWidget::ResolveAnchor(UUserWidget* Owner, FName AnchorName) const
{
	// 호스트 우선 — 런타임에 만들어 붙인 위젯은 패널 WidgetTree 에 없어 GetWidgetFromName 이 못 찾는다
	if (AnchorResolver.IsBound())
	{
		if (UWidget* Resolved = AnchorResolver.Execute(AnchorName))
		{
			return Resolved;
		}
	}
	return Owner ? Owner->GetWidgetFromName(AnchorName) : nullptr;
}

void UPanelIntroOverlayWidget::HandleAdvanceClicked()
{
	if (bFinished)
	{
		return;
	}
	ShowStep(StepCursor + 1);
}

void UPanelIntroOverlayWidget::FinishIntro()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	// 앵커를 하나도 못 찾았으면 본 것으로 치지 않는다 — 배선을 고치면 다음 오픈에 다시 재생된다
	const bool bShownAtLeastOnce = CurrentAnchor.IsValid();
	if (bShownAtLeastOnce)
	{
		if (UPanelIntroSubsystem* IntroMgr = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr)
		{
			IntroMgr->MarkSeen(PanelKey);
		}
	}

	OnIntroFinished.Broadcast();
	RemoveFromParent();
}

bool UPanelIntroOverlayWidget::ComputeAnchorRect(const UWidget* Target, FVector2D& OutCenter, FVector2D& OutSize) const
{
	if (!Target)
	{
		return false;
	}
	const FGeometry& TargetGeo = Target->GetCachedGeometry();
	const FVector2D LocalSize = TargetGeo.GetLocalSize();
	if (LocalSize.X <= 1.f || LocalSize.Y <= 1.f)
	{
		return false; // 아직 레이아웃 전
	}

	// AbsoluteToLocal 패턴 — ScreenPos÷ViewportScale 은 SafeZone/DPI 를 못 잡는다
	const FGeometry& MyGeo = GetTickSpaceGeometry();
	const FVector2D TopLeft = MyGeo.AbsoluteToLocal(TargetGeo.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D BottomRight = MyGeo.AbsoluteToLocal(TargetGeo.LocalToAbsolute(LocalSize));

	OutSize = BottomRight - TopLeft;
	OutCenter = (TopLeft + BottomRight) * 0.5f;
	return true;
}

void UPanelIntroOverlayWidget::PlaceTooltip(const FVector2D& AnchorCenter, const FVector2D& AnchorSize, EPanelIntroTailDir Dir)
{
	if (!Tooltip)
	{
		return;
	}
	UCanvasPanelSlot* TipSlot = Cast<UCanvasPanelSlot>(Tooltip->Slot);
	if (!TipSlot)
	{
		return;
	}

	const FVector2D TipSize = Tooltip->GetDesiredSize();
	const FVector2D ScreenSize = GetTickSpaceGeometry().GetLocalSize();
	const EGuideTooltipDir TipDir = static_cast<EGuideTooltipDir>(static_cast<uint8>(Dir));

	const FVector2D Pos = ComputeGuideTooltipPosition(
		AnchorCenter, AnchorSize, TipSize, TipDir, ScreenSize, TooltipSafeMargin, TooltipGap);
	TipSlot->SetPosition(Pos);

	// 클램프로 툴팁이 밀린 만큼 꼬리를 대상 쪽으로 되돌린다 — 안 하면 말풍선이 엉뚱한 데를 가리킨다
	const FGuideTailPlacement Tail = ResolveGuideTailPlacement(TipDir);
	const float Offset = Tail.bAlongX
		? ComputeGuideTailOffset(AnchorCenter.X, Pos.X, TipSize.X, TailSize, TailCornerInset)
		: ComputeGuideTailOffset(AnchorCenter.Y, Pos.Y, TipSize.Y, TailSize, TailCornerInset);
	Tooltip->SetTail(TipDir, Offset);
}

void UPanelIntroOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bFinished || !CutoutMID)
	{
		return;
	}
	const UWidget* Anchor = CurrentAnchor.Get();
	FVector2D Center, Size;
	if (!Anchor || !ComputeAnchorRect(Anchor, Center, Size))
	{
		return;
	}

	const FVector2D FullSize = MyGeometry.GetLocalSize();
	const FVector2D HoleHalf = Size * 0.5f + FVector2D(HolePadding, HolePadding);

	CutoutMID->SetScalarParameterValue(TEXT("Wpx"), static_cast<float>(FullSize.X));
	CutoutMID->SetScalarParameterValue(TEXT("Hpx"), static_cast<float>(FullSize.Y));
	CutoutMID->SetScalarParameterValue(TEXT("HoleCX"), static_cast<float>(Center.X));
	CutoutMID->SetScalarParameterValue(TEXT("HoleCY"), static_cast<float>(Center.Y));
	CutoutMID->SetScalarParameterValue(TEXT("HoleHW"), static_cast<float>(HoleHalf.X));
	CutoutMID->SetScalarParameterValue(TEXT("HoleHH"), static_cast<float>(HoleHalf.Y));

	// desired size 가 확정된 뒤에야 배치한다 — 그전엔 아래꼬리가 대상에서 떨어진다
	if (Tooltip)
	{
		const FVector2D TipSize = Tooltip->GetDesiredSize();
		if (TipSize.X > 1.f && TipSize.Y > 1.f)
		{
			bTooltipPlacePending = false;
			PlaceTooltip(Center, Size, PendingTailDir);
		}
	}
}
