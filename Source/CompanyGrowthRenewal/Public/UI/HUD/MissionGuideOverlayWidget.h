#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MissionGuideOverlayWidget.generated.h"

class UMissionManagerSubsystem;
class UMaterialInstanceDynamic;
class UCanvasPanel;
class UButton;
class UImage;
class UGuideTooltipWidget;
class UGestureHintWidget;

/** 설명 페이즈 한 틱의 처분. */
enum class EGuideExplainAction : uint8
{
	Idle,      // 설명 페이즈가 아님
	Show,      // 타겟이 모였다 — 딤 + 구멍 + 툴팁
	Wait,      // 아직 안 모였다 — 다음 틱에 다시 묻는다
	Timeout,   // 끝내 안 모였다 — 자동 통과
};

/**
 * ⚠ Wait 가 무한히 계속되면 미션이 갇힌다 — 플레이어가 자리를 비운 사이 운영이 끝나면 진행 Bar 가
 * 사라져 타겟이 영영 안 모이고, 설명 페이즈를 넘길 입력이 존재하지 않는다(세이브 삭제 외 탈출로 없음).
 * 그래서 미해결 누적이 한계를 넘으면 Timeout 으로 자동 통과시킨다.
 *
 * ⚠ TimeoutSeconds 는 카메라 대기까지 포함한 절대값이다 — 글라이드가 끝내 안 끝나도 탈출한다.
 */
inline EGuideExplainAction ResolveGuideExplainAction(
	bool bPhaseActive, bool bTargetsResolved, bool bCameraSettled,
	float UnresolvedSeconds, float TimeoutSeconds)
{
	if (!bPhaseActive)
	{
		return EGuideExplainAction::Idle;
	}
	if (bTargetsResolved && bCameraSettled)
	{
		return EGuideExplainAction::Show;
	}
	return (UnresolvedSeconds >= TimeoutSeconds) ? EGuideExplainAction::Timeout : EGuideExplainAction::Wait;
}

/** 다음 설명 스텝. 마지막이면 INDEX_NONE ― 호출자는 그때 다음 페이즈로 넘긴다. */
inline int32 ResolveNextExplainStep(int32 CurrentStep, int32 StepCount)
{
	const int32 Next = CurrentStep + 1;
	return (Next < StepCount) ? Next : INDEX_NONE;
}

enum class EGuideExplainAdvanceAction : uint8
{
	WaitForNextTarget,
	CommitNextStep,
	FinishExplainPhase,
};

/** 다음 설명 타겟이 준비된 프레임에만 스텝을 바꿔 빈 딤 프레임을 만들지 않는다. */
inline EGuideExplainAdvanceAction ResolveGuideExplainAdvanceAction(
	int32 CurrentStep,
	int32 StepCount,
	bool bNextTargetReady)
{
	if (ResolveNextExplainStep(CurrentStep, StepCount) == INDEX_NONE)
	{
		return EGuideExplainAdvanceAction::FinishExplainPhase;
	}
	return bNextTargetReady
		? EGuideExplainAdvanceAction::CommitNextStep
		: EGuideExplainAdvanceAction::WaitForNextTarget;
}

/**
 * 미션 가이드 오버레이 — 현 페이즈의 하이라이트 타겟 위젯을 소프트 유도.
 * 타겟이 CommonButton 계열이면 그 버튼 스타일 브러시(텍스처 둥근 코너 포함)를 복사해
 * 바깥으로 퍼지는 잔상 3겹으로 그린다(버튼 모양 그대로 퍼지는 잔상). 브러시 추출 실패/비버튼
 * 타겟이면 라운드렉트 아웃라인 물결 3겹으로 폴백. 텍스트 없음.
 * 또한 매니저가 월드 액터 스포트라이트 대상을 지정하면(GetCurrentSpotlightActor) 화면을 어둡게
 * 딤 처리하고 그 액터 위만 부드러운 라운드박스로 컷아웃해 밝게 비춘다(M_UI_SpotlightCutout MID).
 * 스포트라이트 활성 동안 컷아웃은 숨쉬듯 브리딩하고, 구멍 위에 바운싱하는 아래 방향 셰브론 2겹을
 * 그린다. 멘토 라인 텍스트는 상단 Notification 상주 알림이 담당(매니저 UpdateGuideNotification).
 * 풀스크린 + HitTestInvisible (입력 완전 통과 — 강제 차단 없음).
 * 매니저가 CreateWidget 직후 InitGuideOverlay 호출 → AddToViewport(9000, 프롬프트 모달 위).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMissionGuideOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitGuideOverlay(UMissionManagerSubsystem* InManager);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TWeakObjectPtr<UMissionManagerSubsystem> Manager;

	// 타겟 렉트(내 로컬 공간 — 틱에서 캐시, 페인트에서 읽기만)
	bool bHasTarget = false;
	FVector2D TargetCenterLocal = FVector2D::ZeroVector;
	FVector2D TargetSizeLocal = FVector2D::ZeroVector;

	// 타겟 버튼 스타일의 Normal 베이스 브러시 복사본 (틱에서 캐시, 페인트에서 잔상으로 드로잉)
	// 텍스처를 강참조하진 않지만 그리는 동안 타겟+스타일이 살아 있어 안전 — 타겟 invalid 시 bHasBrush 리셋이 가드
	bool bHasBrush = false;
	FSlateBrush TargetBrush;

	// 물결 위상 (0~1 주기 반복) — 물결 i의 실제 위상은 여기에 i/WaveCount 스태거를 더해 wrap
	float PulsePhase = 0.f;

	static constexpr int32 WaveCount = 3;        // 동시에 퍼지는 물결 겹 수 (1/3씩 위상 엇갈림)
	static constexpr float PulseCycle = 1.2f;    // 물결 1겹 수명 주기(초)
	static constexpr float BasePadding = 6.f;    // 타겟 윤곽에서 물결 시작 안쪽 여백 px
	static constexpr float WaveMaxExpand = 18.f; // 물결 최대 바깥 확장 px (phase 1 시점)
	static constexpr float CornerRadius = 20.f;  // 라운드렉트 기본 코너 반경 px (확장 오프셋이 가산됨)

	// ===== 월드 액터 스포트라이트 (화면 딤 + 라운드박스 컷아웃) =====
	// 첫 사용 시 소프트 로드 후 MID 생성, 멤버로 캐시(GC 보호). 로드 실패 시 스포트라이트 영구 생략(딤 없음).
	void EnsureSpotlightMID();

	// 액터 바운드 8코너를 화면 투영해 구멍 렉트 산출 + MID 파라미터 갱신 (틱에서 호출)
	// DimScale = 딤 알파 배율(1=기존 체인 가이드, 미션판 안내는 2초에 걸쳐 0으로 흘려보낸다)
	void UpdateSpotlight(AActor* SpotActor, float DimScale = 1.f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpotlightMID = nullptr;

	// 컷아웃 머티리얼 로드 시도 완료 여부 (실패해도 매 틱 재시도하지 않도록)
	bool bSpotlightLoadAttempted = false;

	// 화면 딤 = 루트 캔버스 최하단 자식(브러시 = SpotlightMID). NativePaint 로 그리면 툴팁 같은 자식 위에 깔린다 —
	// SCompoundWidget::OnPaint 가 자식 트리를 먼저 그리고 그 MaxLayer 를 NativePaint 의 LayerId 로 넘기기 때문.
	void EnsureDimLayer();

	// bHasSpotlight 가 매 틱 재계산되므로 딤 가시성도 매 틱 맞춘다 (없을 때 화면이 계속 어두우면 안 된다)
	void SyncDimVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UImage> DimImage = nullptr;

	// 캔버스는 ZOrder 오름차순으로 그린다 — 음수라야 차단 레이어/툴팁(기본 0) 아래로 고정된다
	static constexpr int32 DimZOrder = -100;

	// 이번 프레임 딤을 원하는가 — 구멍 개수와 무관한 독립 축이다(구멍 0개 딤이 존재한다, ShowFullDim).
	// 실제 가시성은 SyncDimVisibility 가 정한다 — 딤 레이어(DimImage)가 붙었고 MID 가 있을 때만 보인다.
	// 아래 단일 구멍 렉트는 내 로컬 공간(틱에서 캐시→페인트에서 읽기)
	bool bHasSpotlight = false;
	FVector2D HoleCenterLocal = FVector2D::ZeroVector;
	FVector2D HoleSizeLocal = FVector2D::ZeroVector;

	// 지금 뚫려 있는 구멍 목록(최대 3) — 모든 스포트라이트 경로가 자기 구멍을 여기 게시한다(단일 구멍도 1칸 배열).
	// 0개일 수 있다: 카메라 대기 중 ShowFullDim 은 딤만 깔고 구멍을 뚫지 않는다 —
	// 딤 여부는 위 bHasSpotlight 축이 답하므로 빈 배열을 그 대용으로 읽지 말 것.
	// [0] 은 위 단일 필드와 동일. 게이트를 다구멍으로 넓힐 때 이 배열을 순회할 것 — ApplyBlockMode 는 아직 [0] 하나만 연다.
	static constexpr int32 MaxSpotlightHoles = 3;
	TArray<FVector2D> HoleCentersLocal;
	TArray<FVector2D> HoleSizesLocal;

	static constexpr float SpotlightPadding = 28.f;  // 액터 화면 렉트 바깥 여백 px
	static constexpr float SpotlightRadiusPx = 28.f; // 컷아웃 코너 라운드 px
	static constexpr float SpotlightFeatherPx = 26.f;// 컷아웃 가장자리 소프트 px
	static constexpr float SpotlightDimAlpha = 0.55f;// 구멍 밖 딤 알파

	// 소프트 로드 경로 (CLAUDE.md 에셋 로딩 규칙 — TSoftObjectPtr + LoadSynchronous)
	static const TCHAR* SpotlightMaterialPath;

	// ===== 스포트라이트 juice (브리딩 / 바운싱 셰브론) — 스포트라이트 활성 동안만 =====
	// 펄스 위상(PulsePhase)과 별개의 누적 시간. 브리딩/셰브론 바운스 sin 위상 소스 (틱에서 누적).
	float SpotlightElapsed = 0.f;

	static constexpr float BreatheCycle = 1.6f;      // 컷아웃 브리딩 1주기(초)
	static constexpr float BreatheAmpPx = 6.f;       // 컷아웃 half-size 가산 진폭 px
	static constexpr float ChevronBounceCycle = 1.0f;// 셰브론 바운스 1주기(초, 브리딩과 다른 주기)
	static constexpr float ChevronBounceAmpPx = 8.f; // 셰브론 ±바운스 진폭 px

	// 바운싱 셰브론 꼭짓점(바운스 가산 전 기준) — UpdateSpotlight 에서 구멍 top 위로 산출
	FVector2D ChevronTipLocal = FVector2D::ZeroVector;

	// ===== 입력 게이트 / 클레임 (2026-06-21) =====
	// 위젯 기준 스포트라이트 구멍 산출(딤/패딩/브리딩/셰브론 파라미터화). 클레임=트래커 크게, 진행중 버튼=라이트 딤.
	// 성공 시 bHasSpotlight=true + OutCenter/OutSize(로컬 렉트) 반환(호출자가 링에 사용).
	void UpdateSpotlightFromWidget(UWidget* TargetWidget, float DimAlpha, float Pad, float BreatheAmp, bool bChevron,
		FVector2D& OutCenter, FVector2D& OutSize);

	// 여러 타겟을 한 딤에 뚫는다. 셰브론/브리딩 없음 — 설명 페이즈는 대상이 여럿이라 화살표가 클러터가 된다
	void UpdateSpotlightFromWidgets(const TArray<UWidget*>& Targets, float DimAlpha, float Pad,
		TArray<FVector2D>& OutCenters, TArray<FVector2D>& OutSizes);

	// 구멍 없는 풀스크린 딤 — 카메라 글라이드 동안 "지금부터 설명" 신호를 먼저 깐다.
	// UpdateSpotlightFromWidgets 에 빈 배열을 넘기면 Wpx/DimA 를 세우기 전에 조기 반환해 딤이 안 켜진다.
	// 반환 false = 딤을 못 깔았다(머티리얼 누락/레이아웃 미산출) — 호출자는 입력 차단도 걷어야 한다
	bool ShowFullDim(float DimAlpha);
	// 위젯 하이라이트 타겟 해석(NativeTick 후반부 추출 — early-return 없이 멤버 플래그만 세팅)
	void ResolveWidgetTarget(UMissionManagerSubsystem* Mgr, UWidget* Target, float DimScale = 1.f);

	// 미션판(체인 종료 후) 안내. 그렸으면 true — 호출자는 그대로 return 한다
	bool TickTrackedGoalGuide(float InDeltaTime);

	// NativeTick 본체 — 분기마다 early return 이라 딤 동기화는 호출자가 뒤에서 한 번에 한다
	void UpdateGuideState(float InDeltaTime);

	// 입력 차단 레이어 — 코드 생성(루트 패널에 투명 버튼 자식). 구멍 밖 4버튼=차단 / 풀스크린 1버튼=클레임 탭.
	enum class EBlockMode : uint8 { None, Gate, Claim };
	void EnsureBlockerLayer();
	void ApplyBlockMode(EBlockMode Mode, const FVector2D& HoleMin, const FVector2D& HoleMax);

	UFUNCTION()
	void OnClaimButtonClicked();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> BlockerCanvas = nullptr;
	UPROPERTY(Transient) TObjectPtr<UButton> BlockTop = nullptr;
	UPROPERTY(Transient) TObjectPtr<UButton> BlockBottom = nullptr;
	UPROPERTY(Transient) TObjectPtr<UButton> BlockLeft = nullptr;
	UPROPERTY(Transient) TObjectPtr<UButton> BlockRight = nullptr;
	UPROPERTY(Transient) TObjectPtr<UButton> ClaimButton = nullptr;
	bool bBlockerReady = false;

	// NativePaint 가 클레임 분기(셰브론 확대)에 사용
	bool bClaimMode = false;

	// 이번 프레임 스포트라이트에 셰브론을 그릴지 (액터/클레임=true, 진행중 버튼 라이트 딤=false — 패널 위 화살표 클러터 회피)
	bool bSpotlightChevron = false;

	// 게이트 구멍을 타겟 윤곽보다 살짝 키워 탭 여유 확보
	static constexpr float GateHolePadding = 8.f;

	// 소프트 존 지연 힌트 — 페이즈 진입 후 이 시간 동안 무진전일 때만 가이드 점등 (스펙 2026-08-02 §4)
	static constexpr float HintDelaySeconds = 5.f;

	// 미션판 안내 — 점등 직후 이 시간에 걸쳐 딤이 걷힌다. 이후엔 링/셰브론만 남는다
	// (자유 플레이 중이라 화면을 계속 어둡게 잠그지 않는다)
	static constexpr float TrackedDimSeconds = 2.0f;

	// 미션판 안내 누적 시간 + 직전 대상 (대상이 바뀌면 딤을 처음부터 다시 태운다)
	float TrackedElapsed = 0.f;
	FName LastTrackedID = NAME_None;

	// 진행 중 일반 버튼 하이라이트 딤 — 액터(0.55)/클레임(0.63)보다 약간 옅게 (2026-06-21 사용자 조정: 0.40→0.52, "더 진하게")
	static constexpr float WidgetSpotlightDimAlpha = 0.52f;

	// 클레임 전용 튜닝 (살짝 더 진한 딤 + 큰 스포트라이트/브리딩 — "엄청 크게크게")
	static constexpr float ClaimDimAlpha = 0.63f;        // 기존 0.55 대비 살짝만 진하게
	static constexpr float ClaimSpotlightPadding = 48.f; // 트래커를 크게 감싸는 컷아웃 여백
	static constexpr float ClaimBreatheAmpPx = 12.f;     // 클레임 컷아웃 브리딩 진폭

	// ===== M7 설명 페이즈 (딤 1장 + 구멍 1 + 툴팁 1, 탭하면 다음 설명) =====
	void LayoutExplainTooltips(const TArray<FVector2D>& Centers, const TArray<FVector2D>& Sizes,
		const TArray<struct FGuideExplainCopy>& Copy);
	void ClearExplainTooltips();
	UGuideTooltipWidget* GetOrCreateExplainTooltip(int32 Index);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGuideTooltipWidget>> ExplainTooltipPool;

	// 설명이 아직 성립하지 않은 채 흐른 시간 — 타겟 미해결과 카메라 글라이드 대기를 한 축에 함께 누적한다
	// (Wait 진입 사유가 둘). ExplainResolveTimeout 을 넘으면 설명을 자동 통과시킨다
	float ExplainUnresolvedSeconds = 0.f;

	// 직전 틱의 설명 스텝 — 스텝이 바뀌면 위 예산을 새로 준다(안 그러면 앞 설명이 쓴 시간을 뒷 설명이 물려받는다).
	// 매니저에서 밀지 않고 여기서 감지한다 — 결합을 늘리지 않는 쪽
	int32 LastExplainStep = INDEX_NONE;

	// DT_WidgetClass 행 누락 경고 1회 래치 — 매 틱 × 툴팁 3장이면 로그가 프레임마다 쏟아진다
	bool bExplainTooltipClassMissingLogged = false;

	// 연출 박자가 아니라 소프트락 탈출용 상한 ― 대기 경로에 카메라 글라이드가 들어와 5초로는 정상 경로까지 잘렸다
	static constexpr float ExplainResolveTimeout = 12.f;   // 소프트락 방지 자동 통과 한계(초)
	static constexpr float ExplainSafeMargin = 72.f;       // 화면 가장자리(코너 라운딩 대비)
	static constexpr float ExplainTooltipGap = 30.f;       // 구멍 가장자리 ↔ 툴팁
	static constexpr float ExplainTailSize = 30.f;
	static constexpr float ExplainTailCornerInset = 24.f;  // 꼬리가 코너 라운딩을 안 침범하게

	// ===== 제스처 힌트 (스펙 2026-08-22) — 손끝을 앵커 액터 투영 중심에 둔다. 딤/게이트 상태와 독립 =====
	UGestureHintWidget* GetOrCreateGestureHint();
	// 액터 바운드 중심을 내 로컬로 투영. 화면 밖/레이아웃 미산출이면 false
	bool ProjectActorCenterToLocal(AActor* Actor, FVector2D& OutLocal) const;
	void UpdateGestureHint(UMissionManagerSubsystem* Mgr);
	// UpdateGuideState 는 분기마다 return 이라 표시 여부를 플래그로 모아 NativeTick 끝에서 한 번에 접는다 (딤과 같은 패턴)
	void SyncGestureHintVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UGestureHintWidget> GestureHint = nullptr;

	bool bGestureShownThisTick = false;
	bool bGestureHintClassMissingLogged = false;
};
