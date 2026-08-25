#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Table/PanelIntroTable.h"
#include "PanelIntroOverlayWidget.generated.h"

class UImage;
class UButton;
class UMaterialInstanceDynamic;
class UGuideTooltipWidget;

DECLARE_MULTICAST_DELEGATE(FOnPanelIntroFinished);

// 호스트가 자기 WidgetTree 에 없는 앵커를 이름으로 되돌려 준다 (런타임 생성 행 등). 미바인딩이면 기존 경로만 쓴다
DECLARE_DELEGATE_RetVal_OneParam(UWidget*, FPanelIntroAnchorResolver, FName);

/**
 * 패널 최초 진입 코치마크 러너 — 딤 + 구멍 1개 + 말풍선 1장, 화면 아무 데나 탭하면 다음 스텝.
 * 미션 가이드 오버레이와 독립이다(설계 §2.4) — 매니저를 모르고 소유 패널만 안다.
 * 설계 = docs/superpowers/specs/2026-08-12-panel-intro-coachmark-design.md
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPanelIntroOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// InOwnerPanel = AnchorName 을 GetWidgetFromName 으로 찾을 대상.
	// 재생할 스텝이 하나도 없으면 false — 호출자가 뷰포트에서 걷어내고 MarkSeen 도 하지 않는다
	bool StartIntro(FName InPanelKey, UUserWidget* InOwnerPanel);

	// StartIntro 전에 걸어야 첫 스텝부터 적용된다. 이름을 모르면 nullptr 를 돌려 기존 조회로 넘길 것
	void SetAnchorResolver(const FPanelIntroAnchorResolver& InResolver) { AnchorResolver = InResolver; }

	// 마지막 스텝 종료(또는 중도 정리) 시 1회. 호출자가 미션 오버레이 가시성을 되돌린다
	FOnPanelIntroFinished OnIntroFinished;

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 딤/버튼은 WBP 가 아니라 코드가 만든다 — WBP 는 루트 캔버스만 있으면 된다(미션 오버레이 EnsureDimLayer 선례)
	UPROPERTY(Transient)
	TObjectPtr<UImage> DimImage = nullptr;

	// 전면 탭 — 구멍 밖 입력 차단 겸 "다음". 설명 중엔 패널 조작을 받지 않는다
	UPROPERTY(Transient)
	TObjectPtr<UButton> AdvanceButton = nullptr;

private:
	// 루트 캔버스에 딤/탭 레이어를 1회 생성. 루트가 CanvasPanel 이 아니면 아무것도 못 한다
	void EnsureLayers();

	UFUNCTION()
	void HandleAdvanceClicked();

	// 앵커를 못 찾으면 그 스텝을 건너뛴다(조용히 넘기지 않고 경고를 남긴다)
	void ShowStep(int32 Index);
	UWidget* ResolveAnchor(UUserWidget* Owner, FName AnchorName) const;
	void FinishIntro();
	void EnsureCutoutMID();
	bool ComputeAnchorRect(const UWidget* Target, FVector2D& OutCenter, FVector2D& OutSize) const;
	void PlaceTooltip(const FVector2D& AnchorCenter, const FVector2D& AnchorSize, EPanelIntroTailDir Dir);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CutoutMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UGuideTooltipWidget> Tooltip = nullptr;

	TWeakObjectPtr<UUserWidget> OwnerPanel;
	TWeakObjectPtr<UWidget> CurrentAnchor;

	FPanelIntroAnchorResolver AnchorResolver;

	TArray<FPanelIntroTable> Steps;
	FName PanelKey;
	int32 StepCursor = 0;
	bool bCutoutLoadAttempted = false;
	bool bFinished = false;

	// 툴팁 desired size 는 다음 틱에야 확정된다 — 그전에 배치하면 아래꼬리가 대상에서 떨어진다
	// (GuideTooltipPlacement.h 헤더 경고)
	bool bTooltipPlacePending = false;
	EPanelIntroTailDir PendingTailDir = EPanelIntroTailDir::Up;

	static const TCHAR* CutoutMaterialPath;

	static constexpr float DimAlpha = 0.62f;
	static constexpr float HolePadding = 16.f;
	static constexpr float HoleRadiusPx = 28.f;
	static constexpr float HoleFeatherPx = 26.f;
	static constexpr float TooltipGap = 30.f;
	static constexpr float TooltipSafeMargin = 72.f;
	// 꼬리 폭 — svg_guidetail_render.js 가 30 기준으로 구운 텍스처와 맞춰야 중심 정렬이 맞는다
	static constexpr float TailSize = 30.f;
	static constexpr float TailCornerInset = 20.f;
	// 캔버스는 ZOrder 오름차순 — 딤이 음수라야 말풍선(기본 0) 아래로 고정된다 (미션 오버레이와 동일 규칙)
	static constexpr int32 DimZOrder = -100;
};
