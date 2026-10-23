#include "UI/HUD/MissionGuideOverlayWidget.h"

#include "Components/Widget.h"
#include "Rendering/DrawElements.h"
#include "CommonButtonBase.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateBrush.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "Entity/Factory/BrickFactory.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Officeworker/Officeworker.h"
#include "EngineUtils.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/UIBase.h"
#include "UI/HUD/GuideTooltipAvoidance.h"
#include "UI/Panel/LaunchConfirmWidget.h"
#include "UI/Panel/OfficeRecruitmentModalWidget.h"
#include "UI/Element/Common/GuideTooltipWidget.h"
#include "UI/Element/Common/GestureHintWidget.h"
#include "Player/MainMapPlayerController.h"
#include "Player/Components/PlacementHandler.h"
#include "GameFramework/Pawn.h"
#include "Enum/WidgetType.h"
#include "Engine/GameInstance.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"

const TCHAR* UMissionGuideOverlayWidget::SpotlightMaterialPath =
	TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_SpotlightCutout.M_UI_SpotlightCutout");

namespace
{
	// 자리에 멈춰 일하는 직원(머리 위로 떠오르는 점수 글씨 자리 포함)의 화면 사각형 ― 걷는 직원은 빼서 말풍선이 따라 흔들리지 않게
	void AppendSeatedWorkerRects(UWorld* World, APlayerController* PC, const FGeometry& OverlayGeo, const FGeometry& ViewportGeo, TArray<FSlateRect>& Out)
	{
		if (!World || !PC)
		{
			return;
		}
		for (TActorIterator<AOfficeworker> It(World); It; ++It)
		{
			const AOfficeworker* Worker = *It;
			if (!Worker || Worker->IsHidden() || Worker->GetVelocity().SizeSquared() > 25.0)
			{
				continue;
			}
			FVector Origin, Extent;
			Worker->GetActorBounds(true, Origin, Extent);
			Origin.Z += Extent.Z * 0.5;
			Extent.Z *= 1.5;
			FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
			FVector2D Max(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
			bool bAny = false;
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector Point = Origin + FVector((Corner & 1) ? Extent.X : -Extent.X,
					(Corner & 2) ? Extent.Y : -Extent.Y, (Corner & 4) ? Extent.Z : -Extent.Z);
				FVector2D ViewportPos;
				if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Point, ViewportPos, true))
				{
					continue;
				}
				const FVector2D Local = OverlayGeo.AbsoluteToLocal(ViewportGeo.LocalToAbsolute(ViewportPos));
				Min = FVector2D(FMath::Min(Min.X, Local.X), FMath::Min(Min.Y, Local.Y));
				Max = FVector2D(FMath::Max(Max.X, Local.X), FMath::Max(Max.Y, Local.Y));
				bAny = true;
			}
			if (bAny)
			{
				Out.Emplace(Min.X, Min.Y, Max.X, Max.Y);
			}
		}
	}

	void GetGuideActorBounds(const AActor* Actor, FVector& OutOrigin, FVector& OutExtent)
	{
		if (const ABrickFactory* Factory = Cast<ABrickFactory>(Actor))
		{
			if (const UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Factory->GetMainMeshComponent()))
			{
				if (const UStaticMesh* Mesh = Body->GetStaticMesh())
				{
					// 공장의 예약 부지 충돌 박스는 실제 외형보다 커서 안내 영역과 손 위치를 밀어낸다.
					const FBox WorldBounds = Mesh->GetBoundingBox().TransformBy(Body->GetComponentTransform());
					OutOrigin = WorldBounds.GetCenter();
					OutExtent = WorldBounds.GetExtent();
					return;
				}
			}
		}
		if (const ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(Actor))
		{
			const FBox WorldBounds = Building->GetBuildingWorldBounds();
			if (WorldBounds.IsValid)
			{
				OutOrigin = WorldBounds.GetCenter();
				OutExtent = WorldBounds.GetExtent();
				return;
			}
		}
		Actor->GetActorBounds(false, OutOrigin, OutExtent);
	}

	// Slate 부모 체인 포함 판정 — 중첩 UserWidget/네임드 슬롯까지 UMG 트리 그대로 따라간다
	bool IsSlateDescendantOf(const UWidget* Child, const UWidget* Ancestor)
	{
		if (!Child || !Ancestor)
		{
			return false;
		}
		const TSharedPtr<SWidget> AncestorSlate = Ancestor->GetCachedWidget();
		if (!AncestorSlate.IsValid())
		{
			return false;
		}
		for (TSharedPtr<SWidget> Cur = Child->GetCachedWidget(); Cur.IsValid(); Cur = Cur->GetParentWidget())
		{
			if (Cur == AncestorSlate)
			{
				return true;
			}
		}
		return false;
	}

	// 프롬프트가 떠 있어도 이 타겟은 가려지지 않는가 — 활성(최상단) 프롬프트 '안'의 버튼이거나,
	// UIBase 밖 뷰포트 오버레이(가챠 리빌 z1000 등)라 프롬프트보다 위에 그려지는 경우.
	// 타겟 없음(월드 액터 스포트라이트 페이즈)은 패널이 월드를 덮으므로 가려진 것으로 본다.
	bool IsGuideTargetAbovePrompt(const UWidget* Target, const UUIBase* UIBase)
	{
		if (!Target)
		{
			return false;
		}
		return IsSlateDescendantOf(Target, UIBase->GetActivePromptWidget())
			|| !IsSlateDescendantOf(Target, UIBase);
	}

	// 라운드렉트 윤곽을 닫힌 라인 루프로 드로잉 (4 직선 변 + 4 코너 아크, 1콜)
	// 텍스처/WBP 0개 — InGameLayerWidget 직접 드로잉 패턴
	void DrawRoundedRectOutline(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, const FVector2D& Size, float Radius, const FLinearColor& Color, float Thickness)
	{
		if (Size.X <= 1.f || Size.Y <= 1.f)
		{
			return;
		}

		// 코너 반경 클램프 (반경이 변 절반을 넘으면 라운드렉트가 뒤집힘)
		const float MaxRadius = FMath::Min(Size.X, Size.Y) * 0.5f;
		const float R = FMath::Clamp(Radius, 0.f, MaxRadius);

		const FVector2D Half = Size * 0.5f;
		// 코너 아크 중심 4개 (TL, TR, BR, BL) — 직선 변이 끝나는 안쪽 지점
		const FVector2D TL = Center + FVector2D(-Half.X + R, -Half.Y + R);
		const FVector2D TR = Center + FVector2D(Half.X - R, -Half.Y + R);
		const FVector2D BR = Center + FVector2D(Half.X - R, Half.Y - R);
		const FVector2D BL = Center + FVector2D(-Half.X + R, Half.Y - R);

		const int32 CornerSegments = 6;
		// 코너당 시작각(라디안) — 슬레이트 좌표는 Y가 아래로 증가하므로 위쪽이 -90도
		const float CornerStartAngles[4] = { PI, 1.5f * PI, 0.f, 0.5f * PI };
		const FVector2D CornerCenters[4] = { TL, TR, BR, BL };

		TArray<FVector2D> Points;
		Points.Reserve(4 * (CornerSegments + 1) + 1);
		for (int32 c = 0; c < 4; ++c)
		{
			const float StartAngle = CornerStartAngles[c];
			for (int32 s = 0; s <= CornerSegments; ++s)
			{
				const float Angle = StartAngle + (0.5f * PI) * (static_cast<float>(s) / CornerSegments);
				Points.Add(CornerCenters[c] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
			}
		}
		// 닫음점 — MakeLines 마지막 인자는 '닫힘'이 아니라 안티앨리어싱. 이게 없으면 BL→TL 좌변이 안 그려진다.
		// 주의: Points.Add(Points[0]) 은 TArray 자기 참조 어설션(컨테이너 수정 중 원소 참조) — 값 복사 후 Add.
		const FVector2D FirstPoint = Points[0];
		Points.Add(FirstPoint);

		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color, true, Thickness);
	}

	// 아래 방향 V(셰브론) — 3점 단일 폴리라인. Tip 이 V 꼭짓점(아래쪽).
	// SelectionChevronWidget::DrawChevronV 패턴 — 한 줄 폴리라인이라 알파 자기중첩 없음.
	// 미터 한계: HalfW/Depth 가 ~1.27 이상이어야 꺾임각이 76.5° 안이라 꼭짓점이 미터로 깔끔 (14/10=1.40 안전).
	void DrawDownChevron(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Tip, float InHalfW, float InDepth, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2D> V;
		V.Reserve(3);
		V.Add(Tip + FVector2D(-InHalfW, -InDepth));
		V.Add(Tip);
		V.Add(Tip + FVector2D(InHalfW, -InDepth));
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), V,
			ESlateDrawEffect::None, Color, true, Thickness);
	}

	// 위젯 렉트 → 오버레이 로컬 공간. AbsoluteToLocal 패턴이라 DPI/SafeZone/중첩 컨테이너가 자동 보정된다.
	// 단일/다구멍 두 경로가 공유 — 좌표 규칙을 한 곳에서만 고치도록. false = 타겟 없음 또는 아직 레이아웃 안 됨.
	bool ComputeWidgetRectLocal(const FGeometry& MyTickGeo, UWidget* Target, FVector2D& OutCenter, FVector2D& OutSize)
	{
		if (!Target)
		{
			return false;
		}
		const FVector2D FullSize = MyTickGeo.GetLocalSize();
		if (FullSize.X <= KINDA_SMALL_NUMBER || FullSize.Y <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const FGeometry& TG = Target->GetCachedGeometry();
		const FVector2D WSize = TG.GetLocalSize();
		if (WSize.X <= KINDA_SMALL_NUMBER || WSize.Y <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const FVector2D C0 = MyTickGeo.AbsoluteToLocal(TG.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D C1 = MyTickGeo.AbsoluteToLocal(TG.LocalToAbsolute(WSize));
		OutCenter = MyTickGeo.AbsoluteToLocal(TG.LocalToAbsolute(WSize * 0.5f));
		OutSize = FVector2D(FMath::Abs(C1.X - C0.X), FMath::Abs(C1.Y - C0.Y));
		return true;
	}
}

void UMissionGuideOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 기본 = 입력 완전 통과 (게이트/클레임 모드에서 NativeTick 이 SelfHitTestInvisible 로 올림)
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 딤을 차단 레이어보다 먼저 붙인다 — 삽입 순서는 ZOrder 동률에서만 의미가 있지만, 명시 ZOrder 와 겹쳐 안전
	EnsureDimLayer();

	// 입력 차단/클레임 버튼 레이어 코드 생성 (루트 패널에 투명 버튼 자식)
	EnsureBlockerLayer();
}

void UMissionGuideOverlayWidget::InitGuideOverlay(UMissionManagerSubsystem* InManager)
{
	Manager = InManager;
}

void UMissionGuideOverlayWidget::EnsureSpotlightMID()
{
	if (SpotlightMID || bSpotlightLoadAttempted)
	{
		return;
	}
	bSpotlightLoadAttempted = true; // 실패 시 매 틱 재로드 방지 (한 번만 시도)

	// 중괄호 초기화 — 괄호면 함수 선언으로 파싱됨(most vexing parse, C2228)
	const TSoftObjectPtr<UMaterialInterface> MatPath{ FSoftObjectPath(SpotlightMaterialPath) };
	UMaterialInterface* BaseMat = MatPath.LoadSynchronous();
	if (!BaseMat)
	{
		// 패키지에 머티리얼 미포함(쿠킹/스테이징 누락) 시 여기로 빠짐 → 딤+셰브론 둘 다 생략.
		// 모바일에서 무성(silent) 블랙홀이 되지 않도록 loud — logcat 에서 즉시 확인 (CLAUDE.md Loud failure).
		UE_LOG(LogTemp, Error, TEXT("[MissionGuide] 스포트라이트 머티리얼 로드 실패: %s — 패키지에 미포함 의심(클린 재쿠킹/재패키징 필요). 딤+셰브론 생략됨."), SpotlightMaterialPath);
		return;
	}

	// this 를 Outer 로 — MID 의 GC 수명은 SpotlightMID UPROPERTY 가 보장
	SpotlightMID = UMaterialInstanceDynamic::Create(BaseMat, this);

	// MID 가 태어나는 유일한 지점 — 딤 이미지 브러시도 여기서 한 번만 연결한다
	if (DimImage && SpotlightMID)
	{
		DimImage->SetBrushFromMaterial(SpotlightMID);
	}
}

void UMissionGuideOverlayWidget::EnsureDimLayer()
{
	if (DimImage || !WidgetTree)
	{
		return;
	}

	// ZOrder 로 최하단을 고정해야 해서 CanvasPanelSlot 이 필요 — 다른 패널이면 딤 생략(구멍/링/탭은 정상)
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!RootCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MissionGuide] 루트가 CanvasPanel 이 아님 — 화면 딤 비활성. UI_MissionGuideOverlay 루트를 CanvasPanel 로 설정 필요."));
		return;
	}

	UImage* Dim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	// 딤이 입력을 먹으면 "아무 데나 탭"이 죽어 소프트락 — 탭은 BlockerCanvas 의 버튼만 받는다
	Dim->SetVisibility(ESlateVisibility::Collapsed);
	// 리소스 없는 브러시는 흰 풀스크린 사각형으로 그려진다 — MID 가 먼저 생겼다면 여기서도 연결
	if (SpotlightMID)
	{
		Dim->SetBrushFromMaterial(SpotlightMID);
	}

	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
		DimSlot->SetAlignment(FVector2D::ZeroVector);
		DimSlot->SetAutoSize(false);
		// 툴팁/차단 레이어는 기본 ZOrder 0 — 동률이면 캔버스가 슬롯 주소로 순서를 가르므로 명시가 유일한 보장
		DimSlot->SetZOrder(DimZOrder);
	}

	DimImage = Dim;
}

void UMissionGuideOverlayWidget::SyncDimVisibility()
{
	if (!DimImage)
	{
		return;
	}

	// 구 NativePaint 가드(bHasSpotlight && SpotlightMID)와 동일 조건 — 스포트라이트가 없으면 화면이 밝아야 한다
	const ESlateVisibility Desired = (bHasSpotlight && SpotlightMID)
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (DimImage->GetVisibility() != Desired)
	{
		DimImage->SetVisibility(Desired);
	}
}

void UMissionGuideOverlayWidget::AdvanceSpotlightTransition(float DeltaSeconds)
{
	bSpotlightMoving = false;
	if (!bHasSpotlight || !SpotlightMID || !DimImage || HoleCentersLocal.Num() != 1
		|| !RequestedSpotlightTarget.IsValid())
	{
		SpotlightTransition.Reset();
		PreviousSpotlightTarget.Reset();
		return;
	}
	if (PreviousSpotlightTarget != RequestedSpotlightTarget)
	{
		SpotlightTransition.Retarget();
		PreviousSpotlightTarget = RequestedSpotlightTarget;
	}
	FVector2D DrawCenter = HoleCenterLocal;
	FVector2D DrawHalf = RequestedSpotlightHalf;
	SpotlightTransition.Advance(DrawCenter, DrawHalf, DeltaSeconds);
	bSpotlightMoving = !SpotlightTransition.IsSettled();
	SpotlightMID->SetScalarParameterValue(TEXT("HoleCX"), static_cast<float>(DrawCenter.X));
	SpotlightMID->SetScalarParameterValue(TEXT("HoleCY"), static_cast<float>(DrawCenter.Y));
	SpotlightMID->SetScalarParameterValue(TEXT("HoleHW"), static_cast<float>(DrawHalf.X));
	SpotlightMID->SetScalarParameterValue(TEXT("HoleHH"), static_cast<float>(DrawHalf.Y));
}

void UMissionGuideOverlayWidget::UpdateSpotlight(AActor* SpotActor, float DimScale)
{
	EnsureSpotlightMID();
	if (!SpotlightMID)
	{
		// 머티리얼 로드 실패 — 스포트라이트 생략(딤 없음)
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// 액터 AABB 8코너를 화면 투영해 위젯 로컬 렉트 산출 (CLAUDE.md 좌표 규칙)
	FVector BoundsOrigin;
	FVector BoundsExtent;
	GetGuideActorBounds(SpotActor, BoundsOrigin, BoundsExtent);

	const FGeometry& MyTickGeo = GetTickSpaceGeometry();
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);

	FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D Max(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
	bool bAnyProjected = false;

	for (int32 c = 0; c < 8; ++c)
	{
		const FVector Corner = BoundsOrigin + FVector(
			(c & 1) ? BoundsExtent.X : -BoundsExtent.X,
			(c & 2) ? BoundsExtent.Y : -BoundsExtent.Y,
			(c & 4) ? BoundsExtent.Z : -BoundsExtent.Z);

		FVector2D ViewportPos;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Corner, ViewportPos, true))
		{
			continue; // 이 코너는 화면 뒤/밖
		}
		bAnyProjected = true;

		const FVector2D Abs = ViewportGeo.LocalToAbsolute(ViewportPos);
		const FVector2D Local = MyTickGeo.AbsoluteToLocal(Abs);
		Min.X = FMath::Min(Min.X, Local.X);
		Min.Y = FMath::Min(Min.Y, Local.Y);
		Max.X = FMath::Max(Max.X, Local.X);
		Max.Y = FMath::Max(Max.Y, Local.Y);
	}

	if (!bAnyProjected)
	{
		// 액터가 완전히 화면 밖 — 이 프레임 스포트라이트 생략
		return;
	}

	// 렉트 + 패딩
	Min -= FVector2D(SpotlightPadding, SpotlightPadding);
	Max += FVector2D(SpotlightPadding, SpotlightPadding);

	const FVector2D Size = MyTickGeo.GetLocalSize();
	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
	{
		return; // 레이아웃 미산출
	}

	HoleCenterLocal = (Min + Max) * 0.5f;
	HoleSizeLocal = Max - Min;

	// 컷아웃 브리딩 — half-size 에 sin(누적시간·2π/주기)·진폭 가산 (구멍이 숨쉬듯 확장/수축).
	// SpotlightElapsed 는 펄스 위상과 별개 누적 시간(NativeTick 에서 UpdateSpotlight 호출 전에 갱신됨).
	const float Breathe = FMath::Sin(SpotlightElapsed * (2.f * PI / BreatheCycle)) * BreatheAmpPx;
	const FVector2D HoleHalf = HoleSizeLocal * 0.5f + FVector2D(Breathe, Breathe);
	RequestedSpotlightTarget = SpotActor;
	RequestedSpotlightHalf = HoleHalf;

	// 딤 0(미션판 안내 2초 경과)이면 알파 0 풀스크린 쿼드만 매 프레임 남는다 — 박스는 끄고 셰브론만 남긴다(체인은 DimScale=1 라 무영향)
	bHasSpotlight = (DimScale > KINDA_SMALL_NUMBER);

	// 안 그려지는 머티리얼에 매 프레임 쓰면 uniform 캐시만 재구축된다 — 구멍/셰브론 좌표 산출은 아래에서 계속 필요
	if (bHasSpotlight)
	{
		// 단일 구멍도 1칸 배열로 게시 — 게이트가 단일/다구멍을 같은 방식으로 순회할 수 있게
		HoleCentersLocal = { HoleCenterLocal };
		HoleSizesLocal = { HoleSizeLocal };

		SpotlightMID->SetScalarParameterValue(TEXT("Wpx"), static_cast<float>(Size.X));
		SpotlightMID->SetScalarParameterValue(TEXT("Hpx"), static_cast<float>(Size.Y));
		SpotlightMID->SetScalarParameterValue(TEXT("HoleCX"), static_cast<float>(HoleCenterLocal.X));
		SpotlightMID->SetScalarParameterValue(TEXT("HoleCY"), static_cast<float>(HoleCenterLocal.Y));
		SpotlightMID->SetScalarParameterValue(TEXT("HoleHW"), static_cast<float>(HoleHalf.X));
		SpotlightMID->SetScalarParameterValue(TEXT("HoleHH"), static_cast<float>(HoleHalf.Y));
		SpotlightMID->SetScalarParameterValue(TEXT("RadiusPx"), SpotlightRadiusPx);
		SpotlightMID->SetScalarParameterValue(TEXT("FeatherPx"), SpotlightFeatherPx);
		SpotlightMID->SetScalarParameterValue(TEXT("DimA"), SpotlightDimAlpha * DimScale);

		// 직전 프레임의 3구멍 값이 남으면 단일 구멍 페이즈에 유령 구멍이 보인다
		for (const TCHAR* Name : { TEXT("HoleHW2"), TEXT("HoleHH2"), TEXT("HoleHW3"), TEXT("HoleHH3") })
		{
			SpotlightMID->SetScalarParameterValue(Name, 0.0f);
		}
	}

	bSpotlightChevron = true; // 월드 액터 스포트라이트는 셰브론 동반

	// 셰브론 꼭짓점 — (브리딩 적용된) 구멍 top 위 26px. 줌인으로 구멍이 화면을 덮어도 화면 안 보정.
	// 멘토 텍스트는 상단 Notification 상주 알림이 담당 (매니저 UpdateGuideNotification).
	const float HoleTopY = HoleCenterLocal.Y - HoleHalf.Y;
	ChevronTipLocal = FVector2D(HoleCenterLocal.X, FMath::Max(HoleTopY - 26.f, 48.f));
}

void UMissionGuideOverlayWidget::UpdateSpotlightFromWidget(UWidget* TargetWidget, float DimAlpha, float Pad, float BreatheAmp, bool bChevron,
	FVector2D& OutCenter, FVector2D& OutSize)
{
	OutCenter = FVector2D::ZeroVector;
	OutSize = FVector2D::ZeroVector;

	EnsureSpotlightMID();
	if (!SpotlightMID || !TargetWidget)
	{
		return;
	}

	const FGeometry& MyTickGeo = GetTickSpaceGeometry();
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D LocalSize = FVector2D::ZeroVector;
	if (!ComputeWidgetRectLocal(MyTickGeo, TargetWidget, Center, LocalSize))
	{
		return;
	}
	const FVector2D FullSize = MyTickGeo.GetLocalSize();

	const FVector2D HalfBase = LocalSize * 0.5f + FVector2D(Pad, Pad);

	HoleCenterLocal = Center;
	HoleSizeLocal = HalfBase * 2.f;

	// 단일 구멍도 1칸 배열로 게시 — 게이트가 단일/다구멍을 같은 방식으로 순회할 수 있게
	HoleCentersLocal = { HoleCenterLocal };
	HoleSizesLocal = { HoleSizeLocal };

	const float Breathe = FMath::Sin(SpotlightElapsed * (2.f * PI / BreatheCycle)) * BreatheAmp;
	const FVector2D HoleHalf = HalfBase + FVector2D(Breathe, Breathe);
	RequestedSpotlightTarget = TargetWidget;
	RequestedSpotlightHalf = HoleHalf;

	SpotlightMID->SetScalarParameterValue(TEXT("Wpx"), static_cast<float>(FullSize.X));
	SpotlightMID->SetScalarParameterValue(TEXT("Hpx"), static_cast<float>(FullSize.Y));
	SpotlightMID->SetScalarParameterValue(TEXT("HoleCX"), static_cast<float>(HoleCenterLocal.X));
	SpotlightMID->SetScalarParameterValue(TEXT("HoleCY"), static_cast<float>(HoleCenterLocal.Y));
	SpotlightMID->SetScalarParameterValue(TEXT("HoleHW"), static_cast<float>(HoleHalf.X));
	SpotlightMID->SetScalarParameterValue(TEXT("HoleHH"), static_cast<float>(HoleHalf.Y));
	SpotlightMID->SetScalarParameterValue(TEXT("RadiusPx"), SpotlightRadiusPx);
	SpotlightMID->SetScalarParameterValue(TEXT("FeatherPx"), SpotlightFeatherPx);
	SpotlightMID->SetScalarParameterValue(TEXT("DimA"), DimAlpha);

	// 직전 프레임의 3구멍 값이 남으면 단일 구멍 페이즈에 유령 구멍이 보인다
	for (const TCHAR* Name : { TEXT("HoleHW2"), TEXT("HoleHH2"), TEXT("HoleHW3"), TEXT("HoleHH3") })
	{
		SpotlightMID->SetScalarParameterValue(Name, 0.0f);
	}

	bHasSpotlight = true;
	bSpotlightChevron = bChevron;

	const float HoleTopY = HoleCenterLocal.Y - HoleHalf.Y;
	ChevronTipLocal = FVector2D(HoleCenterLocal.X, FMath::Max(HoleTopY - 26.f, 48.f));

	OutCenter = Center;
	OutSize = LocalSize;
}

void UMissionGuideOverlayWidget::UpdateSpotlightFromWidgets(const TArray<UWidget*>& Targets, float DimAlpha, float Pad,
	TArray<FVector2D>& OutCenters, TArray<FVector2D>& OutSizes)
{
	OutCenters.Reset();
	OutSizes.Reset();

	EnsureSpotlightMID();
	if (!SpotlightMID)
	{
		return;
	}

	const FGeometry& MyTickGeo = GetTickSpaceGeometry();

	for (UWidget* Target : Targets)
	{
		if (OutCenters.Num() >= MaxSpotlightHoles)
		{
			break;   // 구멍 슬롯 소진 — 뒤는 볼 필요 없다
		}
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D LocalSize = FVector2D::ZeroVector;
		if (!ComputeWidgetRectLocal(MyTickGeo, Target, Center, LocalSize))
		{
			continue;   // 이 엔트리만 건너뜀 (null 또는 아직 레이아웃 안 됨)
		}
		if (OutCenters.IsEmpty())
		{
			RequestedSpotlightTarget = Target;
		}
		OutCenters.Add(Center);
		OutSizes.Add(LocalSize);
	}

	if (OutCenters.Num() == 0)
	{
		return;
	}

	// 헬퍼가 한 번이라도 성공했으면 FullSize > 0 이 보장된다
	const FVector2D FullSize = MyTickGeo.GetLocalSize();

	SpotlightMID->SetScalarParameterValue(TEXT("Wpx"), static_cast<float>(FullSize.X));
	SpotlightMID->SetScalarParameterValue(TEXT("Hpx"), static_cast<float>(FullSize.Y));
	SpotlightMID->SetScalarParameterValue(TEXT("RadiusPx"), SpotlightRadiusPx);
	SpotlightMID->SetScalarParameterValue(TEXT("FeatherPx"), SpotlightFeatherPx);
	SpotlightMID->SetScalarParameterValue(TEXT("DimA"), DimAlpha);

	static const TCHAR* const CXNames[] = { TEXT("HoleCX"), TEXT("HoleCX2"), TEXT("HoleCX3") };
	static const TCHAR* const CYNames[] = { TEXT("HoleCY"), TEXT("HoleCY2"), TEXT("HoleCY3") };
	static const TCHAR* const HWNames[] = { TEXT("HoleHW"), TEXT("HoleHW2"), TEXT("HoleHW3") };
	static const TCHAR* const HHNames[] = { TEXT("HoleHH"), TEXT("HoleHH2"), TEXT("HoleHH3") };

	for (int32 i = 0; i < MaxSpotlightHoles; ++i)
	{
		// 남는 슬롯은 반크기 0 — SDF 가 자연히 비활성이라 단일 구멍 호출부와 호환된다
		const bool bUsed = OutCenters.IsValidIndex(i);
		const FVector2D C = bUsed ? OutCenters[i] : FVector2D::ZeroVector;
		const FVector2D H = bUsed ? (OutSizes[i] * 0.5f + FVector2D(Pad, Pad)) : FVector2D::ZeroVector;
		SpotlightMID->SetScalarParameterValue(CXNames[i], static_cast<float>(C.X));
		SpotlightMID->SetScalarParameterValue(CYNames[i], static_cast<float>(C.Y));
		SpotlightMID->SetScalarParameterValue(HWNames[i], static_cast<float>(H.X));
		SpotlightMID->SetScalarParameterValue(HHNames[i], static_cast<float>(H.Y));
	}

	// 대입 후 제자리 가산 — 호출자가 이 멤버를 out 파라미터로 넘겨도(aliasing) 안전하다
	// (먼저 Reset 하면 그 out 배열이 곧 이 멤버라 순회가 0회 돌고 아래 [0] 이 빈 배열을 읽는다)
	HoleCentersLocal = OutCenters;
	HoleSizesLocal = OutSizes;
	for (FVector2D& S : HoleSizesLocal)
	{
		S += FVector2D(Pad, Pad) * 2.f;
	}
	HoleCenterLocal = HoleCentersLocal[0];
	HoleSizeLocal = HoleSizesLocal[0];
	RequestedSpotlightHalf = HoleSizeLocal * 0.5f;

	bHasSpotlight = true;
	bSpotlightChevron = false;   // 대상이 여럿이면 화살표는 클러터
}

bool UMissionGuideOverlayWidget::ShowFullDim(float DimAlpha)
{
	EnsureSpotlightMID();
	EnsureDimLayer();
	// 딤을 실제로 그리는 건 DimImage 다 — 루트가 CanvasPanel 이 아니면 EnsureDimLayer 가 만들지 않고
	// SyncDimVisibility 도 첫 줄에서 물러난다. MID 만 보고 true 를 내면 "안 보이는데 탭만 막히는" 상태가 된다
	if (!SpotlightMID || !DimImage)
	{
		return false;
	}
	const FVector2D FullSize = GetTickSpaceGeometry().GetLocalSize();
	if (FullSize.X <= KINDA_SMALL_NUMBER || FullSize.Y <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	SpotlightMID->SetScalarParameterValue(TEXT("Wpx"), static_cast<float>(FullSize.X));
	SpotlightMID->SetScalarParameterValue(TEXT("Hpx"), static_cast<float>(FullSize.Y));
	SpotlightMID->SetScalarParameterValue(TEXT("RadiusPx"), SpotlightRadiusPx);
	SpotlightMID->SetScalarParameterValue(TEXT("FeatherPx"), SpotlightFeatherPx);
	SpotlightMID->SetScalarParameterValue(TEXT("DimA"), DimAlpha);

	// 세 구멍 전부 반크기 0. 이것이 실제로 "구멍 없음" 이 되는 것은 2026-08-12 머티리얼 수정 이후다 —
	// 그 전에는 반크기 0 에서 SDF 가 붕괴해 중심에 밝은 얼룩을 그렸다
	for (const TCHAR* Name : { TEXT("HoleHW"), TEXT("HoleHH"),
		TEXT("HoleHW2"), TEXT("HoleHH2"), TEXT("HoleHW3"), TEXT("HoleHH3") })
	{
		SpotlightMID->SetScalarParameterValue(Name, 0.0f);
	}

	HoleCentersLocal.Reset();
	HoleSizesLocal.Reset();
	bHasSpotlight = true;
	bSpotlightChevron = false;
	return true;
}

void UMissionGuideOverlayWidget::ResolveWidgetTarget(UMissionManagerSubsystem* Mgr, UWidget* Target, float DimScale)
{
	if (!Target)
	{
		return;
	}

	// 게이트(IsInputGated)와 동일 판정 — 조작 불가 타겟은 없는 것과 동일 취급(딤/링/컷아웃 전부 생략, Idle과 동일)
	FString ActionableReason;
	if (!Mgr->IsGuideTargetActionable(Target, ActionableReason))
	{
		return;
	}

	// UUserWidget 래퍼(UpgradeBtnWidget 등)는 내부 "Btn"(CommonButton) 이 실제 비주얼 —
	// 잔상 렉트/브러시 모두 내부 버튼 기준으로 맞춘다 (이름 계약, 없으면 래퍼 그대로)
	UWidget* VisualTarget = Target;
	UCommonButtonBase* CommonBtn = Cast<UCommonButtonBase>(Target);
	if (!CommonBtn)
	{
		if (UUserWidget* Wrapper = Cast<UUserWidget>(Target))
		{
			if (UCommonButtonBase* InnerBtn = Cast<UCommonButtonBase>(Wrapper->GetWidgetFromName(TEXT("Btn"))))
			{
				CommonBtn = InnerBtn;
				VisualTarget = InnerBtn;
			}
		}
	}

	const FGeometry& TG = VisualTarget->GetCachedGeometry();
	const FVector2D Size = TG.GetLocalSize();
	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
	{
		// 화면에 아직 없음(레이아웃 미산출) — 그리지 않음
		return;
	}

	// 타겟 지오메트리 → 내 로컬 공간 (AbsoluteToLocal 패턴 — DPI/SafeZone 자동 보정)
	const FGeometry& MyTickGeo = GetTickSpaceGeometry();
	const FVector2D Center = MyTickGeo.AbsoluteToLocal(TG.LocalToAbsolute(Size * 0.5f));
	const FVector2D Corner0 = MyTickGeo.AbsoluteToLocal(TG.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D Corner1 = MyTickGeo.AbsoluteToLocal(TG.LocalToAbsolute(Size));

	TargetCenterLocal = Center;
	TargetSizeLocal = FVector2D(FMath::Abs(Corner1.X - Corner0.X), FMath::Abs(Corner1.Y - Corner0.Y));
	bHasTarget = true;

	// 일반 버튼 하이라이트에도 라이트 화면 딤 + 컷아웃. 외곽선은 유지하고 셰브론은 끈다.
	// 이 프레임에 이미 월드 액터 스포트라이트가 있으면(앞서 UpdateSpotlight) 그게 우선 — 중복 생략.
	// 딤이 0이면 링만 남기고 반투명 쿼드는 생략한다.
	if (!bHasSpotlight && DimScale > KINDA_SMALL_NUMBER)
	{
		FVector2D DimC, DimS;
		UpdateSpotlightFromWidget(VisualTarget, WidgetSpotlightDimAlpha * DimScale, SpotlightPadding, BreatheAmpPx, /*bChevron=*/false, DimC, DimS);
	}
}

void UMissionGuideOverlayWidget::EnsureBlockerLayer()
{
	if (bBlockerReady)
	{
		return;
	}

	UPanelWidget* Root = Cast<UPanelWidget>(GetRootWidget());
	if (!Root || !WidgetTree)
	{
		// 루트가 패널이 아니면 입력 게이트/클레임 탭 비활성(소프트 폴백). UI_MissionGuideOverlay 루트를 CanvasPanel 로.
		UE_LOG(LogTemp, Warning, TEXT("[MissionGuide] 루트가 패널 위젯이 아님 — 입력 게이트/클레임 탭 비활성. UI_MissionGuideOverlay 루트를 CanvasPanel 로 설정 필요."));
		return;
	}

	BlockerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Root->AddChild(BlockerCanvas);
	// 캔버스 자체는 히트테스트 통과 — 구멍(버튼 없는 영역)이 아래 위젯/월드로 패스스루되게.
	// (Visible 이면 빈 영역에서 캔버스가 히트를 먹어 패스스루가 깨짐)
	BlockerCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BlockerCanvas->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}

	// 투명(시각 없음) + 히트테스트 가능 버튼 — 누름을 소비해 차단/클레임. OnClicked 없어도 누름은 소비됨.
	auto MakeBlocker = [this](TObjectPtr<UButton>& OutBtn)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style;
		FSlateBrush Clear;
		Clear.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		Style.SetNormal(Clear);
		Style.SetHovered(Clear);
		Style.SetPressed(Clear);
		Style.SetDisabled(Clear);
		Btn->SetStyle(Style);
		BlockerCanvas->AddChild(Btn);
		if (UCanvasPanelSlot* BtnSlot = Cast<UCanvasPanelSlot>(Btn->Slot))
		{
			BtnSlot->SetAnchors(FAnchors(0.f, 0.f));
			BtnSlot->SetAlignment(FVector2D(0.f, 0.f));
			BtnSlot->SetAutoSize(false);
		}
		Btn->SetVisibility(ESlateVisibility::Collapsed);
		OutBtn = Btn;
	};

	MakeBlocker(BlockTop);
	MakeBlocker(BlockBottom);
	MakeBlocker(BlockLeft);
	MakeBlocker(BlockRight);
	MakeBlocker(ClaimButton);

	ClaimButton->OnClicked.AddDynamic(this, &UMissionGuideOverlayWidget::OnClaimButtonClicked);

	bBlockerReady = true;
}

void UMissionGuideOverlayWidget::ApplyBlockMode(EBlockMode Mode, const FVector2D& HoleMin, const FVector2D& HoleMax)
{
	CurrentBlockMode = Mode;
	if (!bBlockerReady)
	{
		// 게이트 레이어 없음 — 소프트(입력 통과)로만 동작
		SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	auto Hide = [](UButton* B) { if (B) { B->SetVisibility(ESlateVisibility::Collapsed); } };
	auto Place = [](UButton* B, const FVector2D& Pos, const FVector2D& Sz)
	{
		if (!B) { return; }
		if (Sz.X <= 1.f || Sz.Y <= 1.f) { B->SetVisibility(ESlateVisibility::Collapsed); return; }
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(B->Slot))
		{
			S->SetPosition(Pos);
			S->SetSize(Sz);
		}
		B->SetVisibility(ESlateVisibility::Visible);
	};

	if (Mode == EBlockMode::None)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		Hide(BlockTop); Hide(BlockBottom); Hide(BlockLeft); Hide(BlockRight); Hide(ClaimButton);
		return;
	}

	// 게이트/클레임 — 자식 버튼만 히트테스트(루트는 통과 → 구멍은 아래 위젯/월드로 패스스루)
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	const FVector2D Full = GetTickSpaceGeometry().GetLocalSize();

	if (Mode == EBlockMode::Claim)
	{
		// 수령 버튼 = 트래커 카드 렉트에만. 구 풀스크린 방식은 어떤 탭이든 수령시켜
		// "누른 적 없는데 보상이 받아지는" 오작동을 만들었다.
		// 바깥은 막지 않는다 — 클레임은 게이트가 아니라 "카드를 눌러 달라"는 안내일 뿐이라,
		// 차단막을 깔면 수령 전까지 카메라 이동/월드 클릭이 전부 죽는다(딤만 남고 먹통).
		const FVector2D Mn(FMath::Clamp(HoleMin.X, 0.f, Full.X), FMath::Clamp(HoleMin.Y, 0.f, Full.Y));
		const FVector2D Mx(FMath::Clamp(HoleMax.X, 0.f, Full.X), FMath::Clamp(HoleMax.Y, 0.f, Full.Y));

		Hide(BlockTop); Hide(BlockBottom); Hide(BlockLeft); Hide(BlockRight);
		Place(ClaimButton, Mn, Mx - Mn);
		return;
	}

	// EBlockMode::Gate — 구멍(타겟 렉트) 밖을 4분할로 차단
	Hide(ClaimButton);
	const FVector2D Mn(FMath::Clamp(HoleMin.X, 0.f, Full.X), FMath::Clamp(HoleMin.Y, 0.f, Full.Y));
	const FVector2D Mx(FMath::Clamp(HoleMax.X, 0.f, Full.X), FMath::Clamp(HoleMax.Y, 0.f, Full.Y));

	Place(BlockTop,    FVector2D(0.f, 0.f),   FVector2D(Full.X, Mn.Y));
	Place(BlockBottom, FVector2D(0.f, Mx.Y),  FVector2D(Full.X, Full.Y - Mx.Y));
	Place(BlockLeft,   FVector2D(0.f, Mn.Y),  FVector2D(Mn.X, Mx.Y - Mn.Y));
	Place(BlockRight,  FVector2D(Mx.X, Mn.Y), FVector2D(Full.X - Mx.X, Mx.Y - Mn.Y));
}

void UMissionGuideOverlayWidget::OnClaimButtonClicked()
{
	if (bSpotlightMoving)
	{
		return;
	}
	UMissionManagerSubsystem* Mgr = Manager.Get();
	if (!Mgr)
	{
		return;
	}

	// 설명 페이즈의 풀스크린 버튼은 "아무 데나 탭" 이라 수령 경로로 흘려보내면 안 된다.
	// 설명이 실제로 떠 있을 때(Show = 타겟 해결 + 카메라 도착)만 넘긴다 — 딤만 깔린 대기 중의 탭을
	// 받아주면 툴팁을 한 번도 못 보고 스킵된다. 안 넘어가도 갇히지 않는다: 타임아웃이 탈출로다
	if (Mgr->IsExplainPhaseActive())
	{
		const int32 CurrentStep = Mgr->GetExplainStep();
		UWidget* TapTarget = nullptr;
		FGuideExplainCopy TapCopy;
		const UGuideTooltipWidget* CurrentTip = ExplainTooltipPool.IsValidIndex(0) ? ExplainTooltipPool[0] : nullptr;
		// 읽기 시간은 실제 표시·이동 완료 틱에서만 쌓인다. 스텝 불일치는 같은 프레임의 연속 탭을 막는다.
		if (ExplainReadGate.CanAdvance(CurrentStep, ExplainReadSeconds)
			&& bTooltipShownThisTick && CurrentTip
			&& CurrentTip->GetVisibility() == ESlateVisibility::HitTestInvisible
			&& CurrentTip->GetRenderOpacity() > 0.f
			&& Mgr->IsExplainCameraSettled()
			&& Mgr->GetExplainTargetForStep(CurrentStep, TapTarget, TapCopy))
		{
			Mgr->AdvanceExplainPhase();
			ExplainReadGate.Reset();
		}
		return;   // 어느 쪽이든 클레임 수령 경로로 흘려보내지 않는다
	}

	if (Mgr->IsReadyToClaim())
	{
		Mgr->ClaimActiveMission();
	}
}

UGuideTooltipWidget* UMissionGuideOverlayWidget::GetOrCreateExplainTooltip(int32 Index)
{
	if (ExplainTooltipPool.IsValidIndex(Index))
	{
		return ExplainTooltipPool[Index];
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return nullptr;
	}
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::GuideTooltip);
	if (!Cls)
	{
		if (!bExplainTooltipClassMissingLogged)
		{
			bExplainTooltipClassMissingLogged = true;
			UE_LOG(LogTemp, Warning, TEXT("[GuideOverlay] EWidgetType::GuideTooltip 미등록 — 설명 툴팁 생략(딤/구멍/탭은 정상)"));
		}
		return nullptr;
	}

	UGuideTooltipWidget* Tip = CreateWidget<UGuideTooltipWidget>(GetOwningPlayer(), Cls);
	if (!Tip)
	{
		return nullptr;
	}
	// 루트가 HitTestInvisible 이므로 툴팁도 입력을 안 먹는다 — 탭은 Claim 풀스크린 버튼이 받는다
	Tip->SetVisibility(ESlateVisibility::Hidden);
	Tip->SetRenderOpacity(0.f);
	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget()))
	{
		if (UCanvasPanelSlot* NewSlot = RootCanvas->AddChildToCanvas(Tip))
		{
			NewSlot->SetAutoSize(true);
			NewSlot->SetAlignment(FVector2D::ZeroVector);
		}
	}
	ExplainTooltipPool.SetNum(Index + 1);
	ExplainTooltipPool[Index] = Tip;
	return Tip;
}

void UMissionGuideOverlayWidget::LayoutExplainTooltips(const TArray<FVector2D>& Centers,
	const TArray<FVector2D>& Sizes, const TArray<FGuideExplainCopy>& Copy, bool bKeepNearAnchor)
{
	// 구멍과 카피는 1:1 이어야 한다 — 하나라도 레이아웃 실패로 빠지면 뒤가 한 칸씩 밀려
	// 엉뚱한 대상에 엉뚱한 설명이 붙는다. 짝이 안 맞으면 툴팁만 접는다(딤/구멍/탭은 유지).
	if (Centers.Num() != Copy.Num() || Centers.Num() != Sizes.Num())
	{
		ClearExplainTooltips();
		return;
	}

	const FVector2D ScreenSize = GetTickSpaceGeometry().GetLocalSize();
	if (ScreenSize.X <= KINDA_SMALL_NUMBER || ScreenSize.Y <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Hard = 덮으면 안 되는 것(대상 구멍·손·닫기·보상·먼저 놓인 말풍선), Soft = 패널의 읽는 내용
	FGuideTooltipAvoid Avoid;
	if (UUIManagerSubsystem* UIMgr = !bKeepNearAnchor && GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		if (UUIBase* UIBase = UIMgr->GetUIBase())
		{
			UCommonActivatableWidget* Prompt = UIBase->GetActivePromptWidget();
			CollectGuideTooltipAvoidRects(Prompt ? static_cast<UWidget*>(Prompt) : UIBase, GetTickSpaceGeometry(), Avoid.Soft, &Avoid.Hard, &Avoid.Controls);
			if (!Prompt)
			{
				AppendSeatedWorkerRects(GetWorld(), GetOwningPlayer(), GetTickSpaceGeometry(),
					UWidgetLayoutLibrary::GetViewportWidgetGeometry(this), Avoid.Soft);
			}
			if (const ULaunchConfirmWidget* Launch = Cast<ULaunchConfirmWidget>(Prompt))
			{
				UWidget* Rewards = Launch->GetGuideRewardWidget();
				FVector2D RewardCenter, RewardSize;
				if (Rewards && Rewards->IsVisible() && ComputeWidgetRectLocal(GetTickSpaceGeometry(), Rewards, RewardCenter, RewardSize))
				{
					Avoid.Hard.Emplace(RewardCenter.X - RewardSize.X * 0.5, RewardCenter.Y - RewardSize.Y * 0.5,
						RewardCenter.X + RewardSize.X * 0.5, RewardCenter.Y + RewardSize.Y * 0.5);
				}
			}
		}
	}
	// 구멍은 링이 그려지는 여백까지 ― 말풍선은 그 바깥에 붙는다
	for (int32 AnchorIndex = 0; AnchorIndex < Centers.Num(); ++AnchorIndex)
	{
		const FVector2D Half = Sizes[AnchorIndex] * 0.5 + FVector2D(SpotlightPadding, SpotlightPadding);
		Avoid.Hard.Emplace(Centers[AnchorIndex].X - Half.X, Centers[AnchorIndex].Y - Half.Y,
			Centers[AnchorIndex].X + Half.X, Centers[AnchorIndex].Y + Half.Y);
	}
	if (bHasSecondaryTarget)
	{
		const FVector2D Half = SecondaryTargetSizeLocal * 0.5;
		Avoid.Hard.Emplace(SecondaryTargetCenterLocal.X - Half.X, SecondaryTargetCenterLocal.Y - Half.Y,
			SecondaryTargetCenterLocal.X + Half.X, SecondaryTargetCenterLocal.Y + Half.Y);
	}
	FSlateRect HandRect;
	if (bGestureShownThisTick && GestureHint && GestureHint->GetHandRectLocal(GetTickSpaceGeometry(), HandRect))
	{
		Avoid.Hard.Add(HandRect);
	}
	// 월드에 붙은 운영 표시와 수익 칩은 UIBase 트리 밖에 있으므로 함께 보호한다.
	if (const UMissionManagerSubsystem* Mgr = Manager.Get(); Mgr && Mgr->IsExplainPhaseActive())
	{
		for (int32 StepIndex = 0; StepIndex < UMissionManagerSubsystem::ExplainStepCount; ++StepIndex)
		{
			UWidget* ExplainTarget = nullptr;
			FGuideExplainCopy ExplainCopy;
			FVector2D ExplainCenter, ExplainSize;
			if (Mgr->GetExplainTargetForStep(StepIndex, ExplainTarget, ExplainCopy)
				&& ComputeWidgetRectLocal(GetTickSpaceGeometry(), ExplainTarget, ExplainCenter, ExplainSize))
			{
				const FVector2D Half = ExplainSize * 0.5;
				Avoid.Hard.Emplace(ExplainCenter.X - Half.X, ExplainCenter.Y - Half.Y,
					ExplainCenter.X + Half.X, ExplainCenter.Y + Half.Y);
			}
		}
	}

	for (int32 i = 0; i < Centers.Num(); ++i)
	{
		UGuideTooltipWidget* Tip = GetOrCreateExplainTooltip(i);
		if (!Tip)
		{
			continue;
		}
		if (Copy[i].Body.IsEmpty())
		{
			Tip->HideGuide();
			continue;
		}
		const bool bContentChanged = Tip->SetContent(Copy[i].Eyebrow, Copy[i].Body);
		if (bContentChanged || Tip->GetVisibility() == ESlateVisibility::Collapsed
			|| Tip->GetVisibility() == ESlateVisibility::Hidden)
		{
			Tip->SetVisibility(ESlateVisibility::Hidden);
			Tip->SetRenderOpacity(0.f);
			Tip->ForceLayoutPrepass();
		}

		// 콘텐츠 변경은 숨긴 상태의 prepass로 측정하고, 유효한 위치를 얻은 뒤에만 등장시킨다.
		const FVector2D TipSize = Tip->GetDesiredSize();
		if (TipSize.X <= KINDA_SMALL_NUMBER || TipSize.Y <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// 구멍 = 대상 렉트 + 스포트라이트 패딩. 툴팁은 밝은 구멍 바깥에 붙어야 한다
		const FVector2D AnchorSize = Sizes[i] + FVector2D(SpotlightPadding, SpotlightPadding) * 2.f;

		const FGuideTooltipLayout Placement = ResolveGuideTooltipLayoutAvoiding(Copy[i].Dir, Centers[i], AnchorSize,
			TipSize, ScreenSize, ExplainSafeMargin, ExplainTooltipGap, Avoid, ExplainMaxRetreat);
		const EGuideTooltipDir Dir = Placement.Direction;
		const FVector2D Pos = Placement.Position;

		const bool bVertical = (Dir == EGuideTooltipDir::Up || Dir == EGuideTooltipDir::Down);
		const float TailOffset = ComputeGuideTailOffset(
			static_cast<float>(bVertical ? Centers[i].X : Centers[i].Y),
			static_cast<float>(bVertical ? Pos.X : Pos.Y),
			static_cast<float>(bVertical ? TipSize.X : TipSize.Y),
			ExplainTailSize, ExplainTailCornerInset);
		Tip->SetTail(Dir, TailOffset);

		if (UCanvasPanelSlot* TipSlot = Cast<UCanvasPanelSlot>(Tip->Slot))
		{
			TipSlot->SetPosition(Pos);
			Avoid.Hard.Emplace(Pos.X, Pos.Y, Pos.X + TipSize.X, Pos.Y + TipSize.Y);
			bTooltipShownThisTick = true;
			Tip->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	for (int32 i = Centers.Num(); i < ExplainTooltipPool.Num(); ++i)
	{
		if (ExplainTooltipPool[i] && ExplainTooltipPool[i]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			ExplainTooltipPool[i]->HideGuide();
		}
	}
}

bool UMissionGuideOverlayWidget::GetVisibleTooltipRect(FSlateRect& OutAbsoluteRect) const
{
	const UGuideTooltipWidget* Tip = ExplainTooltipPool.IsValidIndex(0) ? ExplainTooltipPool[0].Get() : nullptr;
	if (!bTooltipShownThisTick || bSpotlightMoving || !Tip
		|| Tip->GetVisibility() != ESlateVisibility::HitTestInvisible || Tip->GetRenderOpacity() <= 0.f)
	{
		return false;
	}
	const FGeometry& TipGeo = Tip->GetCachedGeometry();
	const FVector2D TipSize = TipGeo.GetLocalSize();
	if (TipSize.X <= KINDA_SMALL_NUMBER || TipSize.Y <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	OutAbsoluteRect = TipGeo.GetRenderBoundingRect();
	return true;
}

void UMissionGuideOverlayWidget::ClearExplainTooltips()
{
	for (UGuideTooltipWidget* Tip : ExplainTooltipPool)
	{
		if (Tip && Tip->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Tip->HideGuide();
		}
	}
}

bool UMissionGuideOverlayWidget::TickTrackedGoalGuide(float InDeltaTime)
{
	UMissionManagerSubsystem* Mgr = Manager.Get();
	UGoalBoardSubsystem* GoalMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>() : nullptr;
	if (!Mgr || !GoalMgr)
	{
		return false;
	}

	const FName TrackedID = GoalMgr->GetTrackedGoalID();
	if (TrackedID.IsNone())
	{
		LastTrackedID = NAME_None;
		TrackedElapsed = 0.f;
		return false;
	}

	const FGoalGuideStep GuideStep = Mgr->GetTrackedGoalStep();
	// 현재 프롬프트 안의 행동은 안내하되, 그 뒤에 가려진 대상은 비추지 않는다.
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UUIBase* UIBase = UIMgr->GetUIBase())
		{
			if (UIBase->GetPromptStackCount() > 0 && !IsGuideTargetAbovePrompt(GuideStep.Anchor, UIBase))
			{
				ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
				return true;   // 그리지 않되 체인 경로로 흘려보내지도 않는다
			}
		}
	}

	// 대상이 바뀌면 딤을 처음부터 다시 — 새 타겟에서 어텐션 펄스가 한 번 더 돈다
	if (TrackedID != LastTrackedID)
	{
		LastTrackedID = TrackedID;
		TrackedElapsed = 0.f;
	}
	TrackedElapsed += InDeltaTime;
	SpotlightElapsed += InDeltaTime;
	PulsePhase = FMath::Fmod(PulsePhase + InDeltaTime / PulseCycle, 1.f);

	// 딤은 TrackedDimSeconds 에 걸쳐 선형으로 걷힌다. 이후엔 링/셰브론만 남는다
	const float DimScale = FMath::Clamp(1.f - (TrackedElapsed / TrackedDimSeconds), 0.f, 1.f);

	AActor* SpotActor = GuideStep.Spot;
	if (IsValid(SpotActor))
	{
		UpdateSpotlight(SpotActor, DimScale);
		UpdateGestureHintForActor(EGestureHintKind::Tap, SpotActor);
		if (bGestureShownThisTick)
		{
			bSpotlightChevron = false;
		}
	}
	else if (UWidget* TargetWidget = GuideStep.Anchor)
	{
		// 체인 가이드와 같은 함수 — 브러시 추출/래퍼 언랩/조작가능 안전망을 그대로 태운다
		ResolveWidgetTarget(Mgr, TargetWidget, DimScale);
	}
	const FText GuideLine = Mgr->GetTrackedGoalStepLine();
	if (!GuideLine.IsEmpty() && (bHasTarget || bHasSpotlight
		|| (IsValid(SpotActor) && RequestedSpotlightTarget.Get() == SpotActor)))
	{
		FGuideExplainCopy HintCopy;
		HintCopy.Body = GuideLine;
		HintCopy.Dir = EGuideTooltipDir::Down;
		LayoutExplainTooltips(
			{bHasTarget ? TargetCenterLocal : HoleCenterLocal},
			{bHasTarget ? TargetSizeLocal : HoleSizeLocal}, {HintCopy});
	}

	// 입력 차단 없음 — 자유 플레이 중이므로 게이트를 걸지 않는다
	ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
	return true;
}

UGestureHintWidget* UMissionGuideOverlayWidget::GetOrCreateGestureHint()
{
	if (GestureHint) return GestureHint;
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!RootCanvas) return nullptr;
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> Cls = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::GestureHint) : nullptr;
	if (!Cls)
	{
		if (!bGestureHintClassMissingLogged)
		{
			bGestureHintClassMissingLogged = true;
			UE_LOG(LogTemp, Warning, TEXT("[GuideOverlay] EWidgetType::GestureHint 미등록 — 제스처 힌트 생략(딤/링은 정상)"));
		}
		return nullptr;
	}
	UGestureHintWidget* Hint = CreateWidget<UGestureHintWidget>(GetOwningPlayer(), Cls);
	if (!Hint) return nullptr;
	Hint->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(Hint))
	{
		HintSlot->SetAutoSize(true);
		HintSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	}
	else return nullptr;
	GestureHint = Hint;
	return Hint;
}

bool UMissionGuideOverlayWidget::ProjectActorCenterToLocal(AActor* Actor, FVector2D& OutLocal) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(Actor) || !PC) return false;
	FVector BoundsOrigin, BoundsExtent;
	GetGuideActorBounds(Actor, BoundsOrigin, BoundsExtent);
	FVector2D ViewportPos;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, BoundsOrigin, ViewportPos, true)) return false;
	// 월드 컨텍스트로 PC 를 넘긴다 — const 메서드라 this 를 UObject* 로 못 준다(같은 월드라 결과는 동일)
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(PC);
	const FGeometry& MyTickGeo = GetTickSpaceGeometry();
	const FVector2D Size = MyTickGeo.GetLocalSize();
	if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER) return false;
	OutLocal = MyTickGeo.AbsoluteToLocal(ViewportGeo.LocalToAbsolute(ViewportPos));
	return OutLocal.X >= 0.f && OutLocal.Y >= 0.f && OutLocal.X <= Size.X && OutLocal.Y <= Size.Y;
}

void UMissionGuideOverlayWidget::UpdateGestureHint(UMissionManagerSubsystem* Mgr)
{
	const EGestureHintKind Kind = Mgr->GetCurrentGesture();
	if (Kind == EGestureHintKind::None) return;
	// 플레이어가 그 제스처를 실제로 하는 동안은 숨긴다 — 누르고 있는 손 위에 손 그림이 겹치면 안내가 아니라 방해다 (2026-08-23 사용자).
	// 홀드 = 공장 생산 중(PC 입력 모드 Factory), 드래그 = 배치 프리뷰 드래그 중. 탭 이동은 드래그가 아니라 유지.
	if (const AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(GetOwningPlayer()))
	{
		if (Kind == EGestureHintKind::Hold && MainPC->GetCurrentInputMode() == EInputMode::Factory) return;
		if (Kind == EGestureHintKind::Drag)
		{
			const APawn* Pawn = MainPC->GetPawn();
			const UPlacementHandler* Handler = Pawn ? Pawn->FindComponentByClass<UPlacementHandler>() : nullptr;
			if (Handler && Handler->IsDraggingPlacement()) return;
		}
	}
	UpdateGestureHintForActor(Kind, Mgr->GetCurrentGestureAnchorActor());
}

void UMissionGuideOverlayWidget::UpdateGestureHintForActor(EGestureHintKind Kind, AActor* Actor)
{
	FVector2D Center;
	if (!ProjectActorCenterToLocal(Actor, Center)) return;
	UGestureHintWidget* Hint = GetOrCreateGestureHint();
	if (!Hint) return;
	UCanvasPanelSlot* HintSlot = Cast<UCanvasPanelSlot>(Hint->Slot);
	if (!HintSlot) return;
	Hint->SetGesture(Kind);
	Hint->SetLabel(FText::GetEmpty());
	// 월드 힌트는 모바일에서 크게 — 공장 홀드는 더 크게(2.25), 탭/프리뷰 드래그 1.75. 오피스 캐치 힌트(라벨 동반)는 1.0.
	// 피벗 기본 (0.5,0.5) = 손끝이라 스케일해도 앵커가 안 움직인다.
	const float HintScale = (Kind == EGestureHintKind::Hold) ? 2.25f : 1.75f;
	Hint->SetRenderScale(FVector2D(HintScale, HintScale));
	HintSlot->SetPosition(Center);
	if (Hint->GetVisibility() != ESlateVisibility::HitTestInvisible) Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
	bGestureShownThisTick = true;
}

void UMissionGuideOverlayWidget::SyncGestureHintVisibility()
{
	if (!bGestureShownThisTick && GestureHint && GestureHint->GetVisibility() != ESlateVisibility::Collapsed)
	{
		GestureHint->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMissionGuideOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateGuideState(InDeltaTime);
	AdvanceSpotlightTransition(InDeltaTime);
	if (bSpotlightMoving)
	{
		bTooltipShownThisTick = false;
		bGestureShownThisTick = false;
		// 이동하는 밝은 영역이 지나가는 버튼은 입력 대상으로 사용하지 않는다.
		if (CurrentBlockMode != EBlockMode::None)
		{
			ApplyBlockMode(EBlockMode::Gate, FVector2D::ZeroVector, FVector2D::ZeroVector);
		}
	}
	if (!bTooltipShownThisTick)
	{
		ClearExplainTooltips();
	}
	else
	{
		for (UGuideTooltipWidget* Tip : ExplainTooltipPool)
		{
			if (Tip && Tip->GetVisibility() == ESlateVisibility::HitTestInvisible)
			{
				Tip->AdvanceReveal(InDeltaTime);
			}
		}
	}
	UMissionManagerSubsystem* Mgr = Manager.Get();
	const bool bExplainActive = Mgr && Mgr->IsExplainPhaseActive();
	const int32 ExplainStep = bExplainActive ? Mgr->GetExplainStep() : INDEX_NONE;
	const UGuideTooltipWidget* ExplainTip = ExplainTooltipPool.IsValidIndex(0) ? ExplainTooltipPool[0] : nullptr;
	const bool bExplainVisible = bExplainActive && ExplainStep == LastExplainStep
		&& bTooltipShownThisTick && !bSpotlightMoving && ExplainTip
		&& ExplainTip->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& ExplainTip->GetRenderOpacity() > 0.f;
	ExplainReadGate.Update(ExplainStep, bExplainVisible, InDeltaTime);

	// 딤은 자식 위젯이라 상태를 가시성으로 옮겨야 한다 — 본체는 분기마다 return 이라 여기 한 곳에서만 맞춘다
	SyncDimVisibility();
	SyncGestureHintVisibility();
}

void UMissionGuideOverlayWidget::UpdateGuideState(float InDeltaTime)
{
	bHasTarget = false;
	bHasSecondaryTarget = false;
	bHasSpotlight = false;
	bClaimMode = false;
	bSpotlightChevron = false;
	bGestureShownThisTick = false;
	bTooltipShownThisTick = false;
	RequestedSpotlightTarget.Reset();
	// 구멍 배열도 매 프레임 비운다 — 지난 프레임 구멍이 남으면 안 되기 때문이지, "비었다 = 딤 없음" 이라서가 아니다.
	// 구멍 0개인 딤이 존재한다(ShowFullDim, 카메라 대기) — 딤 가시성은 (딤 레이어와 MID 가 있을 때) bHasSpotlight 가 정한다
	HoleCentersLocal.Reset();
	HoleSizesLocal.Reset();

	UMissionManagerSubsystem* Mgr = Manager.Get();

	if (Mgr && Mgr->IsRecruitmentPresentationActive())
	{
		ClearExplainTooltips();
		ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	if (!Mgr)
	{
		SpotlightElapsed = 0.f; // 비활성 동안 위상 리셋 (다음 활성 시 0 부터 시작)
		ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
		return;
	}

	if (!Mgr->HasActiveMission())
	{
		// 체인이 없을 때만 미션판 안내가 산다 (체인 중에는 기존 가이드가 화면을 소유)
		if (TickTrackedGoalGuide(InDeltaTime))
		{
			return;
		}
		// 안내도 없음 — 위상 리셋(다음 점등 시 0부터). 안내가 살아 있을 땐 리셋하면 브리딩/바운스가 죽는다
		SpotlightElapsed = 0.f;
		LastTrackedID = NAME_None;
		TrackedElapsed = 0.f;
		ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
		return;
	}

	// ===== 클레임 모드 — 트래커 카드를 크게 비추고, 수령은 그 카드 위에서만 (바깥 탭은 차단막이 삼킴) =====
	if (Mgr->IsReadyToClaim())
	{
		// 배치 모드/프롬프트 모달이 살아 있는 동안엔 연출을 미룬다.
		// 풀스크린 수령 버튼이 z9000 이라 그 화면으로 향한 탭을 가로채 오작동 수령이 된다
		// (연속 배치 중 다음 책상 탭, 가챠 리빌 [확인] 탭 등). 화면이 비면 다음 틱에 정상 진입.
		if (!Mgr->IsClaimPresentable())
		{
			SpotlightElapsed = 0.f;
			ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
			return;
		}

		bClaimMode = true;
		SpotlightElapsed += InDeltaTime;
		PulsePhase += InDeltaTime / PulseCycle;
		PulsePhase = FMath::Fmod(PulsePhase, 1.f);

		FVector2D ClaimC, ClaimS;
		UpdateSpotlightFromWidget(Mgr->GetClaimSpotlightTarget(), ClaimDimAlpha, ClaimSpotlightPadding, ClaimBreatheAmpPx,
			/*bChevron=*/true, ClaimC, ClaimS);
		if (!bHasSpotlight)
		{
			// 카드 렉트를 못 구하면 수령 버튼을 놓을 자리가 없다.
			// 딤만 깔고 아무데나 눌리게 두느니 물러난다(카드가 보일 때 다시 들어온다).
			bClaimMode = false;
			SpotlightElapsed = 0.f;
			ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
			return;
		}

		// 트래커 둘레 폴백 링도 함께 (스포트라이트 + 셰브론 + 링)
		TargetCenterLocal = ClaimC;
		TargetSizeLocal = ClaimS;
		bHasTarget = true;

		// 수령 버튼은 카드 위에만 — 풀스크린이면 배치/패널로 향한 탭까지 삼켜 "누른 적 없는 수령"이 된다.
		ApplyBlockMode(EBlockMode::Claim, ClaimC - ClaimS * 0.5f, ClaimC + ClaimS * 0.5f);
		return;
	}

	// ===== 프롬프트에 가려진 타겟이면 가이드는 물러난다 (2026-07-27, 판정 정밀화 2026-08-12) =====
	// 게이트는 "기반 UI 의 타겟으로 유도"하는 장치라, 그 위에 모달이 올라오면 구멍이 옛 타겟에 고정돼
	// 모달 버튼까지 막아버린다(부지 인수 ConfirmCancel 이 안 눌리던 사고). 클레임(위 분기)은 예외 —
	// 그건 패널 위에서도 눌려야 하는 게 목적이다.
	// ⚠ 프롬프트 '존재'만으로 물러나면 프롬프트 안의 타겟(채용 패널 [채용], 건설 모달 카드, 기획 보드 추천 카드,
	// 출시 확인)까지 통째로 죽는다 — PushPromptClass 로 열리는 패널이 전부 여기 해당. 가려질 때만 물러난다.
	UWidget* HighlightTarget = Mgr->GetCurrentHighlightTarget();
	if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		if (UUIBase* UIBase = UIMgr->GetUIBase())
		{
			// PromptStack 만 본다. MainStack 에는 기반 레이어(InGameLayer/OfficeLayer)가 상주해서
			// 항상 >0 이라, 같이 보면 가이드가 영원히 물러나 딤이 아예 안 뜬다.
			if (UIBase->GetPromptStackCount() > 0 && !IsGuideTargetAbovePrompt(HighlightTarget, UIBase))
			{
				SpotlightElapsed = 0.f;
				ClearExplainTooltips();   // 설명 중 프롬프트가 올라오면 툴팁이 그 위에 남는다
				ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
				return;   // 딤/스포트라이트도 그리지 않음 (bHas* 전부 false 유지)
			}
		}
	}

	// ===== 소프트 존 지연 힌트 (스펙 2026-08-02 §4) =====
	// 페이즈 진입 후 잠시 가이드를 감춘다 — 스스로 진행하면 가이드 0 노출, 헤매면 HintDelay 뒤 점등.
	// 점등 시 시각은 기존 하이라이트 경로 그대로(차단버튼만 없음 — IsInputGated 가 소프트 존에서 false).
	// 지연을 쓸지는 매니저가 미션별로 판단한다(UsesDelayedHint) — 재진입 수렴형은 즉시 점등.
	if (Mgr->UsesDelayedHint() && Mgr->GetPhaseElapsedSeconds() < HintDelaySeconds)
	{
		SpotlightElapsed = 0.f;
		ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
		return;
	}

	// 진행 중 스포트라이트 브리딩/셰브론 위상 — 액터/위젯 공용 누적 (UpdateSpotlight* 의 sin 가산보다 먼저)
	SpotlightElapsed += InDeltaTime;

	// ===== M7 설명 페이즈 — 딤 1장 + 구멍 1 + 툴팁 1. 탭하면 다음 설명 =====
	// 동시에 셋을 띄우면 같은 건물 위 대상끼리 툴팁이 겹친다(가장자리 클램프가 안쪽으로 밀면 확실히 포갠다) —
	// 한 장씩 순차로 돌려 겹칠 대상 자체를 없앤다
	{
		Mgr->ResolvePendingExplainAdvance(ExplainResolveTimeout);
		const bool bPhaseActive = Mgr->IsExplainPhaseActive();
		const int32 Step = bPhaseActive ? Mgr->GetExplainStep() : INDEX_NONE;
		if (Step != LastExplainStep)
		{
			// 스텝마다 타임아웃 예산을 새로 준다 — 앞 설명이 쓴 시간이 뒷 설명을 잘라내면 안 된다
			LastExplainStep = Step;
			ExplainUnresolvedSeconds = 0.f;
		}

		UWidget* ExplainTarget = nullptr;
		FGuideExplainCopy ExplainCopy;
		const bool bResolved = bPhaseActive && Mgr->GetExplainTargetForStep(Step, ExplainTarget, ExplainCopy);

		// 페이즈 밖에서는 카메라를 묻지 않는다 — 조회가 헛돌 뿐 아니라 Idle 판정에 아무 영향이 없다
		const bool bCameraSettled = !bPhaseActive || Mgr->IsExplainCameraSettled();

		switch (ResolveGuideExplainAction(bPhaseActive, bResolved, bCameraSettled,
			ExplainUnresolvedSeconds, ExplainResolveTimeout))
		{
		case EGuideExplainAction::Show:
		{
			// ⚠ 멤버 배열(HoleCentersLocal/HoleSizesLocal)을 out 으로 넘기지 말 것 — 진입 Reset 과
			// 제자리 패딩 가산이 호출자 배열까지 건드려 헤더가 약속한 불변식이 깨진다
			const TArray<UWidget*> StepTargets = { ExplainTarget };
			const TArray<FGuideExplainCopy> StepCopy = { ExplainCopy };
			TArray<FVector2D> Centers;
			TArray<FVector2D> Sizes;
			UpdateSpotlightFromWidgets(StepTargets, SpotlightDimAlpha, SpotlightPadding, Centers, Sizes);
			LayoutExplainTooltips(Centers, Sizes, StepCopy);
			// 타겟은 있어도 툴팁 에셋/레이아웃이 끝내 안 뜨면 탭으로 진행할 수 없으므로 탈출시킨다.
			ExplainUnresolvedSeconds = bTooltipShownThisTick ? 0.f : ExplainUnresolvedSeconds + InDeltaTime;
			if (ExplainUnresolvedSeconds >= ExplainResolveTimeout)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[MissionGuide] 설명 툴팁이 %.0f초간 뜨지 않아 자동 통과합니다(스텝 %d)."),
					ExplainResolveTimeout, Step + 1);
				ExplainUnresolvedSeconds = 0.f;
				Mgr->AdvanceExplainPhase(false);
				ClearExplainTooltips();
				return;
			}
			// 구멍이 하나라도 게이트(구멍 밖 4분할)를 쓰지 않는다 — 여기서 원하는 건 "아무 데나 탭" 이다.
			// 오수령 위험은 없다: OnClaimButtonClicked 가 설명 페이즈를 먼저 가로채 진행만 시킨다.
			ApplyBlockMode(EBlockMode::Claim, FVector2D::ZeroVector, GetTickSpaceGeometry().GetLocalSize());
			return;
		}
		case EGuideExplainAction::Wait:
			ExplainUnresolvedSeconds += InDeltaTime;
			ClearExplainTooltips();
			// 카메라가 가는 동안 "지금부터 설명" 신호를 먼저 깐다 — 구멍은 도착 후에
			if (ShowFullDim(SpotlightDimAlpha))
			{
				// ⚠ ZeroVector 를 넘기면 Place 의 Sz<=1 가드에 걸려 버튼이 접힌다 = 그 자체로 소프트락
				ApplyBlockMode(EBlockMode::Claim, FVector2D::ZeroVector, GetTickSpaceGeometry().GetLocalSize());
			}
			else
			{
				// 딤을 못 깔았다(머티리얼 누락 / 첫 틱 지오메트리 0) — 화면 변화가 0 인데 전면 탭까지 삼키면
				// 타임아웃까지 먹통으로 보인다. 보여줄 게 없으면 입력도 삼키지 않는다(클레임 폴백과 같은 원칙)
				ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
			}
			return;
		case EGuideExplainAction::Timeout:
			// 사유를 안 가르면 카메라만 안 온 경우까지 "타겟 미해결" 로 찍혀 다음 사람이 원인을 잘못 짚는다
			if (bResolved)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[MissionGuide] 카메라 이동이 %.0f초 안에 끝나지 않아 이 설명을 자동 통과합니다(스텝 %d)."),
					ExplainResolveTimeout, Step + 1);
			}
			else
			{
				// 이 스텝 타겟이 끝내 안 떴다(운영 종료로 Bar 소멸 / 칩 미배치) — 넘길 입력이 없어 갇히므로 자동 통과.
				// 페이즈째 건너뛰지 않으므로 남은 설명은 살아난다(열화가 완만하다)
				UE_LOG(LogTemp, Warning,
					TEXT("[MissionGuide] 설명 타겟이 %.0f초간 뜨지 않아 이 설명을 자동 통과합니다(스텝 %d)."),
					ExplainResolveTimeout, Step + 1);
			}
			ExplainUnresolvedSeconds = 0.f;
			Mgr->AdvanceExplainPhase(false);
			ClearExplainTooltips();
			break;
		case EGuideExplainAction::Idle:
			ExplainUnresolvedSeconds = 0.f;   // 툴팁 접기는 틱 상단 단일 지점이 담당
			break;
		}
	}

	// ===== 진행 중 — 월드 액터 스포트라이트 (배경 딤 + 컷아웃 + 셰브론). 있으면 위젯 딤보다 우선 =====
	if (AActor* SpotActor = Mgr->GetCurrentSpotlightActor())
	{
		UpdateSpotlight(SpotActor);
	}

	// ===== 진행 중 — 위젯 하이라이트 타겟 (잔상 링 + 라이트 딤/컷아웃, 셰브론 X) =====
	// 배치/Grind 는 딤 0 — 부지 pulse·강화 패널을 가리지 않는다 (스펙 D4)
	ResolveWidgetTarget(Mgr, HighlightTarget, Mgr->GetGuideDimScale());
	if (UWidget* SecondaryTarget = Mgr->GetCurrentSecondaryHighlightTarget())
	{
		FString Reason;
		if (SecondaryTarget != HighlightTarget && Mgr->IsGuideTargetActionable(SecondaryTarget, Reason))
		{
			bHasSecondaryTarget = ComputeWidgetRectLocal(GetTickSpaceGeometry(), SecondaryTarget,
				SecondaryTargetCenterLocal, SecondaryTargetSizeLocal);
		}
	}
	UpdateGestureHint(Mgr);
	if (bGestureShownThisTick)
	{
		bSpotlightChevron = false;
	}

	// 목표와 진행도는 트래커에 두고, 조작 대상에는 짧은 설명만 붙인다.
	const FText GuideLine = Mgr->GetCurrentGuideLine();
	if (!GuideLine.IsEmpty())
	{
		FVector2D HintCenter;
		FVector2D HintSize;
		bool bHintAnchorReady = false;
		FString HintTargetReason;
		if (HighlightTarget && Mgr->IsGuideTargetActionable(HighlightTarget, HintTargetReason))
		{
			bHintAnchorReady = ComputeWidgetRectLocal(GetTickSpaceGeometry(), HighlightTarget, HintCenter, HintSize);
		}
		else if (bHasSpotlight)
		{
			HintCenter = HoleCenterLocal;
			HintSize = HoleSizeLocal;
			bHintAnchorReady = true;
		}
		else if (ProjectActorCenterToLocal(Mgr->GetCurrentGestureAnchorActor(), HintCenter))
		{
			HintSize = FVector2D(100.f, 100.f);
			bHintAnchorReady = true;
		}
		if (bHintAnchorReady)
		{
			FGuideExplainCopy HintCopy;
			HintCopy.Body = GuideLine;
			HintCopy.Dir = EGuideTooltipDir::Down;
			const UOfficeRecruitmentModalWidget* RecruitModal = HighlightTarget
				? HighlightTarget->GetTypedOuter<UOfficeRecruitmentModalWidget>() : nullptr;
			const bool bRecruitPull = Mgr->GetActiveConditionType() == EMissionConditionType::RecruitEmployees
				&& RecruitModal && HighlightTarget == RecruitModal->GetPullButtonMultiWidget();
			LayoutExplainTooltips({HintCenter}, {HintSize}, {HintCopy}, bRecruitPull);
		}
	}

	if (bHasTarget || bHasSecondaryTarget)
	{
		PulsePhase += InDeltaTime / PulseCycle;
		PulsePhase = FMath::Fmod(PulsePhase, 1.f);
	}

	// ===== 입력 게이트 — 현재 타겟(구멍)만 통과, 나머지 UI 잠금 =====
	// 구멍 = 위젯 타겟이면 그 렉트(+여유), 아니면 액터 스포트라이트 렉트. 둘 다 없으면 게이트 OFF(소프트락 방지).
	if (Mgr->IsInputGated() && (bHasTarget || bHasSpotlight))
	{
		FVector2D HoleMin, HoleMax;
		if (bHasSpotlight)
		{
			// 스포트라이트(컷아웃) 렉트 = 보이는 밝은 영역 → 클릭 구멍을 그에 맞춤 (버튼/액터 공통)
			HoleMin = HoleCenterLocal - HoleSizeLocal * 0.5f;
			HoleMax = HoleCenterLocal + HoleSizeLocal * 0.5f;
		}
		else
		{
			// 컷아웃 없음(머티리얼 로드 실패 등) — 타겟 렉트 + 탭 여유
			const FVector2D Pad(GateHolePadding, GateHolePadding);
			HoleMin = TargetCenterLocal - TargetSizeLocal * 0.5f - Pad;
			HoleMax = TargetCenterLocal + TargetSizeLocal * 0.5f + Pad;
		}
		ApplyBlockMode(EBlockMode::Gate, HoleMin, HoleMax);
	}
	else
	{
		ApplyBlockMode(EBlockMode::None, FVector2D::ZeroVector, FVector2D::ZeroVector);
	}
}

int32 UMissionGuideOverlayWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bSpotlightMoving)
	{
		return MaxLayer;
	}

	// 딤(컷아웃 MID)은 여기서 그리지 않는다 — 루트 캔버스 최하단 자식 DimImage 가 담당.
	// 여기서 그리면 MaxLayer 가 이미 자식 트리를 지난 값이라 툴팁 3장까지 0.55 검정 워시를 뒤집어쓴다.

	// ===== 스포트라이트 juice — 바운싱 셰브론 (딤 위 레이어). 버튼 라이트 딤(bSpotlightChevron=false)은 셰브론 생략 =====
	// 딤이 0으로 걷혀 컷아웃이 생략돼도 대상은 계속 가리켜야 한다 — bHasSpotlight 가드 밖으로 분리
	if (bSpotlightChevron && SpotlightMID)
	{
		const int32 JuiceLayer = MaxLayer + 1;

		const FLinearColor RingColor = FLinearColor::FromSRGBColor(FColor(0x57, 0xD9, 0xFF));

		// 바운싱 셰브론 — 아래 방향 V 2겹. sin 바운스(브리딩과 다른 주기). 클레임 모드는 크게(엄청 크게크게).
		const float ChevHalf  = bClaimMode ? 34.f : 24.f;
		const float ChevDepth = bClaimMode ? 24.f : 17.f;
		const float ChevThick = bClaimMode ? 7.f  : 6.f;
		const float Bounce = FMath::Sin(SpotlightElapsed * (2.f * PI / ChevronBounceCycle)) * ChevronBounceAmpPx;
		const FVector2D Tip = ChevronTipLocal + FVector2D(0.f, Bounce);
		DrawDownChevron(OutDrawElements, JuiceLayer, AllottedGeometry, Tip, ChevHalf, ChevDepth, RingColor, ChevThick);
		// 위쪽 잔상 V (작고 옅게) — SelectionChevron 의 echo 패턴
		const FVector2D Tip2 = Tip + FVector2D(0.f, -ChevDepth - 9.f);
		DrawDownChevron(OutDrawElements, JuiceLayer, AllottedGeometry, Tip2, ChevHalf * 0.8f, ChevDepth * 0.8f,
			FLinearColor(RingColor.R, RingColor.G, RingColor.B, 0.5f), ChevThick - 1.f);

		MaxLayer = JuiceLayer;
	}

	if (!bHasTarget && !bHasSecondaryTarget)
	{
		return MaxLayer;
	}

	const int32 RingLayer = MaxLayer + 1;

	const FLinearColor RingColor = FLinearColor::FromSRGBColor(FColor(0x57, 0xD9, 0xFF));
	const FLinearColor HaloColor = FLinearColor::FromSRGBColor(FColor(0x0C, 0x6F, 0xB2));

	const float Pulse = 0.8f + 0.2f * FMath::Cos(PulsePhase * 2.f * PI);
	const auto DrawTargetOutline = [&](const FVector2D& Center, const FVector2D& Size)
	{
		const FVector2D OutlineSize = Size + FVector2D(2.f * BasePadding, 2.f * BasePadding);
		const float Radius = CornerRadius + BasePadding;
		// 선의 안쪽 끝도 타겟 밖에 두어 라벨·가격·아이콘의 원래 색을 보존한다.
		for (int32 WaveIndex = 0; WaveIndex < WaveCount; ++WaveIndex)
		{
			const float Phase = FMath::Frac(PulsePhase + static_cast<float>(WaveIndex) / WaveCount);
			const float Expand = BasePadding + Phase * WaveMaxExpand;
			const float Fade = FMath::Square(1.f - Phase);
			const FVector2D WaveSize = Size + FVector2D(2.f * Expand, 2.f * Expand);
			const float WaveRadius = CornerRadius + Expand;
			DrawRoundedRectOutline(OutDrawElements, RingLayer, AllottedGeometry, Center, WaveSize, WaveRadius,
				FLinearColor(0.06f, 0.08f, 0.10f, 0.4f * Fade), 9.f);
			DrawRoundedRectOutline(OutDrawElements, RingLayer, AllottedGeometry, Center, WaveSize, WaveRadius,
				FLinearColor(HaloColor.R, HaloColor.G, HaloColor.B, 0.32f * Fade), 7.f);
			DrawRoundedRectOutline(OutDrawElements, RingLayer, AllottedGeometry, Center, WaveSize, WaveRadius,
				FLinearColor(RingColor.R, RingColor.G, RingColor.B, 0.9f * Fade), 4.f);
		}
		DrawRoundedRectOutline(OutDrawElements, RingLayer, AllottedGeometry, Center, OutlineSize, Radius,
			FLinearColor(0.06f, 0.08f, 0.10f, 0.65f), 12.f);
		DrawRoundedRectOutline(OutDrawElements, RingLayer, AllottedGeometry, Center, OutlineSize, Radius,
			FLinearColor(HaloColor.R, HaloColor.G, HaloColor.B, 0.65f * Pulse), 9.f);
		DrawRoundedRectOutline(OutDrawElements, RingLayer, AllottedGeometry, Center, OutlineSize, Radius,
			FLinearColor(RingColor.R, RingColor.G, RingColor.B, 1.f), 4.f);
	};
	if (bHasTarget)
	{
		DrawTargetOutline(TargetCenterLocal, TargetSizeLocal);
	}
	if (bHasSecondaryTarget)
	{
		DrawTargetOutline(SecondaryTargetCenterLocal, SecondaryTargetSizeLocal);
	}

	return RingLayer;
}
