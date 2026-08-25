// 건물 상단 말풍선 버블 컨테이너 위젯
// 위치 추적/애니메이션은 Container, 비주얼은 BubbleElementWidget(WBP)

#include "UI/Element/Chat/BubbleContainerWidget.h"
#include "UI/Element/Chat/BubbleElementWidget.h"
#include "UI/Element/Chat/BubbleIconConfig.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Manager/EntityManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Interfaces/BubbleAnchorProvider.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCamera.h"
#include "Player/Components/MovementInputHandler.h"

bool UBubbleContainerWidget::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (!bSuccess) return false;

	// WidgetTree 루트에 CanvasPanel 구성
	BubbleCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BubbleCanvas"));
	if (BubbleCanvas)
	{
		WidgetTree->RootWidget = BubbleCanvas;
	}

	// === DataAsset에서 버블 아이콘 설정 로드 ===
	BubbleIconConfigPath = TSoftObjectPtr<UBubbleIconConfig>(
		FSoftObjectPath(TEXT("/Game/CompanyGrowth/Data/DA_BubbleIconConfig.DA_BubbleIconConfig")));

	if (!BubbleIconConfigPath.IsNull())
	{
		UBubbleIconConfig* Config = BubbleIconConfigPath.LoadSynchronous();
		if (Config)
		{
			// 배경 텍스처 로드
			if (!Config->BubbleBG.IsNull())
			{
				CachedBubbleBG = Config->BubbleBG.LoadSynchronous();
			}

			// 기본 아이콘 로드
			if (!Config->DefaultIcon.IsNull())
			{
				CachedDefaultIcon = Config->DefaultIcon.LoadSynchronous();
			}

			// 타입별 아이콘 텍스처 로드
			for (const auto& Pair : Config->IconMap)
			{
				if (!Pair.Value.IsNull())
				{
					UTexture2D* LoadedTex = Pair.Value.LoadSynchronous();
					if (LoadedTex)
					{
						CachedBubbleIcons.Add(Pair.Key, LoadedTex);
					}
				}
			}

			// 타입별 셸 머티리얼 로드
			for (const auto& Pair : Config->ShellMap)
			{
				if (!Pair.Value.IsNull())
				{
					if (UMaterialInterface* LoadedMat = Pair.Value.LoadSynchronous())
					{
						CachedShells.Add(Pair.Key, LoadedMat);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BubbleContainerWidget: DA_BubbleIconConfig not found! Create it in editor."));
		}
	}

	return bSuccess;
}

FGeometry UBubbleContainerWidget::GetCanvasGeometry() const
{
	if (BubbleCanvas)
	{
		return BubbleCanvas->GetCachedGeometry();
	}
	return FGeometry();
}

// ============================================================
// 3D 건물 위치 → Canvas 로컬 좌표 변환
// AbsoluteToLocal 패턴 사용 (ScreenPos/ViewportScale 직접 나누기 금지)
// ============================================================
FVector2D UBubbleContainerWidget::GetBubbleScreenPosition(const FBubbleAnimData& Data) const
{
	UWorld* World = GetWorld();
	if (!World) return FVector2D::ZeroVector;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return FVector2D::ZeroVector;

	// 앵커 월드 위치 해석: 앵커 경로면 IBubbleAnchorProvider로, 아니면 빌딩 경로(EntityManager)
	FVector AnchorWorld;
	if (Data.bUsesAnchor)
	{
		// 앵커 액터가 파괴되면 화면 밖 센티넬로 숨김 (빌딩 경로로 흘러 (0,0)에 박히는 것 방지)
		if (!Data.AnchorActor.IsValid()) return FVector2D(-9999.0f, -9999.0f);

		const IBubbleAnchorProvider* Provider = Cast<IBubbleAnchorProvider>(Data.AnchorActor.Get());
		if (!Provider) return FVector2D(-9999.0f, -9999.0f);
		AnchorWorld = Provider->GetBubbleAnchorPosition();
	}
	else
	{
		UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
		if (!EntityMgr) return FVector2D::ZeroVector;

		ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(Data.BuildingIndex);
		if (!IsValid(Building)) return FVector2D(-9999.0f, -9999.0f);

		AnchorWorld = Building->GetBubbleAnchorPosition();
	}

	FVector WorldPos = AnchorWorld + FVector(0.0f, 0.0f, WorldZOffset);

	// 3D → Viewport 로컬 (논리 픽셀, DPI 보정 포함)
	FVector2D ViewportPos;
	bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC, WorldPos, ViewportPos, true);
	if (!bProjected) return FVector2D(-9999.0f, -9999.0f);

	// Viewport 로컬 → Absolute → Canvas 로컬
	FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(World);
	FVector2D AbsolutePos = ViewportGeo.LocalToAbsolute(ViewportPos);

	if (!BubbleCanvas) return FVector2D::ZeroVector;
	FGeometry CanvasGeo = BubbleCanvas->GetCachedGeometry();
	FVector2D CanvasLocal = CanvasGeo.AbsoluteToLocal(AbsolutePos);

	// 줌아웃 시 카메라 거리 증가 + 탑다운 각도 → WorldZOffset의 화면 투영 거의 0
	// → 스크린 공간에서 ZoomValue에 비례하는 오프셋으로 보정
	APlayerCamera* CamPawn = Cast<APlayerCamera>(PC->GetPawn());
	if (CamPawn && CamPawn->MovementInputHandler)
	{
		float ZoomValue = CamPawn->MovementInputHandler->GetZoomValue();
		// Ease-Out: 줌아웃 초반에 빠르게 보정, 끝에서 완만하게 수렴
		float EasedZoom = FMath::InterpEaseOut(0.0f, 1.0f, ZoomValue, 2.0f);
		float ScreenOffset = FMath::Lerp(ScreenYOffset_Close, ScreenYOffset_Far, EasedZoom);
		CanvasLocal.Y -= ScreenOffset;
	}

	// 앵커 경로(책상 등)에는 게이지가 붙지 않으므로 빌딩 경로만 리프트한다
	if (!Data.bUsesAnchor)
	{
		if (const float* ExtraLift = ExtraLiftByBuilding.Find(Data.BuildingIndex))
		{
			CanvasLocal.Y -= *ExtraLift;
		}
	}

	return CanvasLocal;
}

void UBubbleContainerWidget::SetExtraLiftForBuilding(int32 BuildingIndex, float Lift)
{
	// 0 이하는 항목 제거 ― 맵 크기를 게이지 캡에 묶어 둔다
	if (Lift > 0.0f)
	{
		ExtraLiftByBuilding.Add(BuildingIndex, Lift);
	}
	else
	{
		ExtraLiftByBuilding.Remove(BuildingIndex);
	}
}

void UBubbleContainerWidget::ClearAllExtraLifts()
{
	ExtraLiftByBuilding.Reset();
}

bool UBubbleContainerWidget::IsBuildingAnchorOnScreen(int32 BuildingIndex) const
{
	// 빌딩 경로(bUsesAnchor=false)로 프로브 — 위치 해석은 EntityManager 가 한다
	FBubbleAnimData Probe;
	Probe.BuildingIndex = BuildingIndex;
	return IsBuildingOnScreen(GetBubbleScreenPosition(Probe));
}

bool UBubbleContainerWidget::IsBuildingOnScreen(const FVector2D& CanvasLocal) const
{
	if (!BubbleCanvas) return false;

	FVector2D CanvasSize = BubbleCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f) return false;

	return CanvasLocal.X >= -OffscreenMargin
		&& CanvasLocal.X <= CanvasSize.X + OffscreenMargin
		&& CanvasLocal.Y >= -OffscreenMargin
		&& CanvasLocal.Y <= CanvasSize.Y + OffscreenMargin;
}

// ============================================================
// 버블 생성 — CreateWidget<BubbleElementWidget> 패턴
// ============================================================
void UBubbleContainerWidget::CreateBubble(int32 Key, AActor* Anchor, EBubbleType Type)
{
	if (!BubbleCanvas || Type == EBubbleType::None) return;

	// lazy-init: TableManager에서 BubbleElement WBP 클래스 캐싱
	if (!CachedBubbleElementClass)
	{
		UGameInstance* GI = GetGameInstance();
		if (!GI) return;

		UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
		if (!TableMgr)
		{
			UE_LOG(LogTemp, Warning, TEXT("BubbleContainerWidget: TableManagerSubsystem not found!"));
			return;
		}

		CachedBubbleElementClass = TableMgr->GetWidgetClass(EWidgetType::BubbleElement);
		if (!CachedBubbleElementClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("BubbleContainerWidget: BubbleElement not found in DataTable!"));
			return;
		}
	}

	// Element 위젯 생성
	UBubbleElementWidget* NewBubble = CreateWidget<UBubbleElementWidget>(this, CachedBubbleElementClass);
	if (!NewBubble) return;

	// Canvas에 추가
	UCanvasPanelSlot* CanvasSlot = BubbleCanvas->AddChildToCanvas(NewBubble);
	if (!CanvasSlot)
	{
		BubbleCanvas->RemoveChild(NewBubble);
		return;
	}

	CanvasSlot->SetAutoSize(true);
	CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	CanvasSlot->SetPosition(FVector2D::ZeroVector);

	// 셸 머티리얼 + 글리프
	UMaterialInterface** FoundShell = CachedShells.Find(Type);
	UTexture2D** FoundIcon = CachedBubbleIcons.Find(Type);
	NewBubble->SetBubbleVisual(
		FoundShell ? *FoundShell : nullptr,
		CachedBubbleBG,
		FoundIcon ? *FoundIcon : CachedDefaultIcon);

	// 클릭 이벤트 바인딩
	NewBubble->SetBubbleInfo(Key, Type);
	NewBubble->OnBubbleClicked.BindUObject(this, &UBubbleContainerWidget::HandleBubbleClicked);

	// 스케일 애니메이션 Pivot (꼬리 위치 기준)
	NewBubble->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));

	// 초기 상태: 완전히 숨긴 후 Appear 애니메이션 시작
	NewBubble->SetRenderScale(FVector2D(0.0f, 0.0f));
	NewBubble->SetRenderOpacity(0.0f);

	// 애니메이션 데이터 등록
	FBubbleAnimData AnimData;
	AnimData.BuildingIndex = Key;
	AnimData.AnchorActor = Anchor;
	AnimData.bUsesAnchor = (Anchor != nullptr);
	AnimData.CurrentType = Type;
	AnimData.BubbleWidget = NewBubble;
	AnimData.CachedSlot = CanvasSlot;
	AnimData.AnimState = EBubbleAnimState::Appearing;
	AnimData.AnimElapsed = 0.0f;
	// 키로 위상을 어긋내 여러 버블이 한 박자로 뛰는 것을 막는다
	AnimData.IdleTime = FMath::Abs(Key % 10) * BouncePhaseStep;
	AnimData.PendingType = EBubbleType::None;
	AnimData.bIsOffscreen = false;

	ActiveBubbles.Add(Key, AnimData);
	OnActiveBubbleMembershipChanged.Broadcast();
}

// ============================================================
// 버블 제거 (Disappear 애니메이션 시작)
// ============================================================
void UBubbleContainerWidget::RemoveBubble(int32 BuildingIndex)
{
	FBubbleAnimData* Data = ActiveBubbles.Find(BuildingIndex);
	if (!Data) return;

	// 이미 사라지는 중이면 무시
	if (Data->AnimState == EBubbleAnimState::Disappearing) return;

	Data->AnimState = EBubbleAnimState::Disappearing;
	Data->AnimElapsed = 0.0f;
	Data->PendingType = EBubbleType::None;
}

// ============================================================
// 외부 API: 건물 버블 상태 갱신
// ============================================================
void UBubbleContainerWidget::UpdateBubbleForBuilding(int32 BuildingIndex, EBubbleType NewType)
{
	UpdateBubbleInternal(BuildingIndex, nullptr, NewType);
}

void UBubbleContainerWidget::UpdateBubbleForAnchor(int32 Key, AActor* Anchor, EBubbleType NewType)
{
	UpdateBubbleInternal(Key, Anchor, NewType);
}

AActor* UBubbleContainerWidget::GetBubbleAnchorActor(int32 Key) const
{
	if (const FBubbleAnimData* Data = ActiveBubbles.Find(Key))
	{
		return Data->AnchorActor.Get();
	}
	return nullptr;
}

bool UBubbleContainerWidget::HasActiveBubbleForBuilding(int32 BuildingIndex) const
{
	const FBubbleAnimData* Data = ActiveBubbles.Find(BuildingIndex);
	return Data && !Data->bUsesAnchor;
}

UWidget* UBubbleContainerWidget::GetFirstBubbleWidget() const
{
	for (const TPair<int32, FBubbleAnimData>& Pair : ActiveBubbles)
	{
		if (Pair.Value.BubbleWidget)
		{
			return Pair.Value.BubbleWidget;
		}
	}
	return nullptr;
}

UWidget* UBubbleContainerWidget::GetBubbleWidgetForBuilding(int32 BuildingIndex) const
{
	const FBubbleAnimData* Data = ActiveBubbles.Find(BuildingIndex);
	if (!Data || Data->bUsesAnchor || Data->AnimState == EBubbleAnimState::Disappearing)
	{
		return nullptr;
	}
	return Data->BubbleWidget;
}

UWidget* UBubbleContainerWidget::GetBubbleWidgetForAnchor(int32 Key) const
{
	if (const FBubbleAnimData* Data = ActiveBubbles.Find(Key))
	{
		return Data->BubbleWidget;
	}
	return nullptr;
}

void UBubbleContainerWidget::UpdateBubbleInternal(int32 Key, AActor* Anchor, EBubbleType NewType)
{
	FBubbleAnimData* Existing = ActiveBubbles.Find(Key);
	if (Existing && NewType != EBubbleType::None)
	{
		// 동일 키 재사용 시 앵커 액터 포인터 최신화
		Existing->AnchorActor = Anchor;
		Existing->bUsesAnchor = (Anchor != nullptr);
	}

	if (Existing && Existing->AnimState == EBubbleAnimState::Disappearing)
	{
		const FBubbleDisappearingTransition Transition =
			ResolveDisappearingBubbleTransition(Existing->CurrentType, NewType);
		Existing->AnimState = Transition.NextAnimState;
		Existing->PendingType = Transition.PendingType;
		if (Transition.bResetAnimElapsed)
		{
			Existing->AnimElapsed = 0.0f;
		}
		return;
	}

	if (NewType == EBubbleType::None)
	{
		// 버블 제거
		if (Existing)
		{
			RemoveBubble(Key);
		}
		return;
	}

	if (!Existing)
	{
		// 새 버블 생성
		CreateBubble(Key, Anchor, NewType);
		return;
	}

	if (Existing->CurrentType == NewType)
	{
		// 동일 타입 → 변경 없음
		return;
	}

	// 타입 전환: 현재 버블 Disappear → 완료 시 새 타입으로 재생성
	Existing->AnimState = EBubbleAnimState::Disappearing;
	Existing->AnimElapsed = 0.0f;
	Existing->PendingType = NewType;
}

// ============================================================
// 전체 표시/숨김 (모달 팝업 시)
// ============================================================
void UBubbleContainerWidget::SetAllBubblesVisible(bool bVisible)
{
	bGlobalVisible = bVisible;

	ESlateVisibility NewVis = bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	for (auto& Pair : ActiveBubbles)
	{
		FBubbleAnimData& Data = Pair.Value;
		if (!Data.BubbleWidget) continue;
		if (Data.LastAppliedVisibility == NewVis) continue;
		Data.BubbleWidget->SetVisibility(NewVis);
		Data.LastAppliedVisibility = NewVis;
	}
}

// ============================================================
// NativeTick: 위치 추적 + 애니메이션 업데이트
// ============================================================
void UBubbleContainerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!BubbleCanvas || !bGlobalVisible) return;
	if (ActiveBubbles.Num() == 0) return;

	// Canvas 크기가 아직 계산되지 않았으면 스킵
	FVector2D CanvasSize = BubbleCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f) return;

	UWorld* BubbleWorld = GetWorld();
	UEntityManager* EntityMgr = BubbleWorld ? BubbleWorld->GetSubsystem<UEntityManager>() : nullptr;

	// SetVisibility는 같은 값 재호출에도 Invalidate를 누적시키므로 직전 값과 다를 때만 호출
	auto ApplyVisibility = [](FBubbleAnimData& Data, ESlateVisibility NewVis)
	{
		if (Data.LastAppliedVisibility == NewVis) return;
		Data.BubbleWidget->SetVisibility(NewVis);
		Data.LastAppliedVisibility = NewVis;
	};

	// 완료된 Disappear 버블 수집 (순회 중 삭제 방지)
	TArray<int32> BubblesToRemove;

	for (auto& Pair : ActiveBubbles)
	{
		FBubbleAnimData& Data = Pair.Value;
		if (!Data.BubbleWidget) continue;

		const bool bSourceMissing = Data.bUsesAnchor
			? !Data.AnchorActor.IsValid()
			: EntityMgr && !IsValid(EntityMgr->GetBuildingByIndex(Data.BuildingIndex));
		if (ApplyMissingBubbleSourcePolicy(bSourceMissing, Data.AnimState, Data.PendingType))
		{
			Data.AnimElapsed = 0.0f;
		}

		FVector2D CanvasLocal = GetBubbleScreenPosition(Data);
		const bool bOnScreen = !bSourceMissing && IsBuildingOnScreen(CanvasLocal);

		if (!bOnScreen)
		{
			ApplyVisibility(Data, ESlateVisibility::Collapsed);
			Data.bIsOffscreen = true;
		}
		else
		{
			// 화면 안으로 복귀
			if (Data.bIsOffscreen)
			{
				Data.bIsOffscreen = false;
			}

			// Idle 상태에서만 Visible (클릭 가능), 애니메이션 중에는 SelfHitTestInvisible
			ESlateVisibility DesiredVis = (Data.AnimState == EBubbleAnimState::Idle)
				? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible;
			ApplyVisibility(Data, DesiredVis);

			// Canvas 슬롯 위치 갱신 (CreateBubble 시 캐싱한 슬롯 사용 — Cast 회피)
			if (Data.CachedSlot)
			{
				Data.CachedSlot->SetPosition(CanvasLocal);
			}
		}

		if (!ShouldAdvanceBubbleAnimation(bOnScreen, Data.AnimState))
		{
			continue;
		}

		// 애니메이션 상태 머신
		UpdateBubbleAnimation(Data, InDeltaTime);

		// Disappear 완료 체크
		if (Data.AnimState == EBubbleAnimState::Disappearing
			&& Data.AnimElapsed >= DisappearDuration)
		{
			// 위젯 트리에서 제거 (PendingType 처리는 아래 일괄 루프에서)
			if (Data.BubbleWidget)
			{
				BubbleCanvas->RemoveChild(Data.BubbleWidget);
			}

			BubblesToRemove.Add(Pair.Key);
		}
	}

	// 일괄 제거 및 전환 재생성
	for (int32 Key : BubblesToRemove)
	{
		FBubbleAnimData* RemovedData = ActiveBubbles.Find(Key);
		EBubbleType PendingType = EBubbleType::None;
		AActor* PendingAnchor = nullptr;
		if (RemovedData)
		{
			PendingType = RemovedData->PendingType;
			PendingAnchor = RemovedData->AnchorActor.Get();
		}

		const int32 RemovedCount = ActiveBubbles.Remove(Key);
		if (RemovedCount > 0)
		{
			OnActiveBubbleMembershipChanged.Broadcast();
		}

		// 전환: 새 타입으로 재생성 (앵커 유지)
		if (PendingType != EBubbleType::None)
		{
			CreateBubble(Key, PendingAnchor, PendingType);
		}
	}
}

// ============================================================
// 애니메이션 상태 머신
// ============================================================
void UBubbleContainerWidget::UpdateBubbleAnimation(FBubbleAnimData& Data, float DeltaTime)
{
	Data.AnimElapsed += DeltaTime;

	switch (Data.AnimState)
	{
	case EBubbleAnimState::Appearing:
	{
		float Alpha = FMath::Clamp(Data.AnimElapsed / AppearDuration, 0.0f, 1.0f);

		float Scale;
		float Opacity;
		float YOffset;

		if (Alpha < 0.6f)
		{
			// 0~60%: 0 → BounceScale로 확대, 페이드 인, Y 오프셋 감소
			float SubAlpha = Alpha / 0.6f;
			float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, SubAlpha, 2.0f);
			Scale = FMath::Lerp(0.0f, BounceScale, EasedAlpha);
			Opacity = FMath::Clamp(SubAlpha / 0.67f, 0.0f, 1.0f);  // 40% 지점에서 완전 불투명
			YOffset = FMath::Lerp(10.0f, 0.0f, EasedAlpha);
		}
		else
		{
			// 60~100%: BounceScale → 1.0 (바운스 복귀)
			float SubAlpha = (Alpha - 0.6f) / 0.4f;
			float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, SubAlpha, 2.0f);
			Scale = FMath::Lerp(BounceScale, 1.0f, EasedAlpha);
			Opacity = 1.0f;
			YOffset = 0.0f;
		}

		if (Data.BubbleWidget)
		{
			Data.BubbleWidget->SetRenderScale(FVector2D(Scale, Scale));
			Data.BubbleWidget->SetRenderOpacity(Opacity);
			Data.BubbleWidget->SetRenderTranslation(FVector2D(0.0f, YOffset));
		}

		// Appear 완료 → Idle 전환
		if (Alpha >= 1.0f)
		{
			Data.AnimState = EBubbleAnimState::Idle;
			Data.AnimElapsed = 0.0f;
			Data.IdleTime = 0.0f;
		}
		break;
	}

	case EBubbleAnimState::Idle:
	{
		Data.IdleTime += DeltaTime;

		if (Data.BubbleWidget)
		{
			// 루트 트랜스폼은 Appear/Disappear 채널 — 바운스는 BounceRoot 로 분리해야
			// 접지 그림자가 같이 떠오르지 않는다
			Data.BubbleWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
			Data.BubbleWidget->SetRenderOpacity(1.0f);
			Data.BubbleWidget->SetRenderTranslation(FVector2D::ZeroVector);

			const float Phase = 2.0f * PI * Data.IdleTime / BounceCycle;
			Data.BubbleWidget->ApplyIdleBounce(FMath::Sin(Phase) * BounceAmplitude);
		}
		break;
	}

	case EBubbleAnimState::Disappearing:
	{
		float Alpha = FMath::Clamp(Data.AnimElapsed / DisappearDuration, 0.0f, 1.0f);
		float EasedAlpha = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f);

		float Scale = FMath::Lerp(1.0f, 0.0f, EasedAlpha);
		float Opacity = FMath::Lerp(1.0f, 0.0f, EasedAlpha);

		if (Data.BubbleWidget)
		{
			Data.BubbleWidget->SetRenderScale(FVector2D(Scale, Scale));
			Data.BubbleWidget->SetRenderOpacity(Opacity);
		}
		// 완료 처리는 NativeTick에서 수행
		break;
	}
	}
}

// ============================================================
// 버블 클릭 릴레이 (Element → Container → InGameLayerWidget)
// ============================================================
void UBubbleContainerWidget::HandleBubbleClicked(int32 BuildingIndex, EBubbleType Type)
{
	OnBubbleAction.ExecuteIfBound(BuildingIndex, Type);
}

void UBubbleContainerWidget::DismissBubble(int32 BuildingIndex)
{
	RemoveBubble(BuildingIndex);
}
