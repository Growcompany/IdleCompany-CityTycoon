// Score Orb 비행 연출 컨테이너 위젯
// 직원 위치에서 해당 카테고리 StatRow로 글로우 구슬이 날아가는 효과
// WBP_ScoreOrbContainer에서 OrbMaterialRefs를 에디터에서 지정 (모바일 쿠킹 안전)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreOrbContainerWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UMaterialInterface;
class UTextBlock;
class UFont;
class UTexture2D;

// 버스트 대표(첫 orb) 도착 알림 — 착지 정산(점수 플러시+펀치)의 트리거
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnScoreOrbArrived, int32 /*StepNumber*/, bool /*bIsCritical*/);

// 컨페티 버스트 스타일
// SideCannons: 하단 양옆 대포 분출 (착지 정산 기존 거동)
// MedalBurst: 지정 중심 원환에서 방사 분출 + 웜톤 팔레트 + 스타 조각 믹스 (레벨업 축하)
enum class EConfettiStyle : uint8 { SideCannons, MedalBurst };

UCLASS()
class COMPANYGROWTHRENEWAL_API UScoreOrbContainerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 구슬 다중 스폰 (시작 위치 주변에서 여러 개가 시차를 두고 날아감)
	void SpawnOrbBurst(FVector2D StartPos, FVector2D TargetPos, int32 StepNumber, bool bIsCritical, int32 OrbCount = 5);

	// 점수 숫자 "+N" 스폰 (스크린스페이스, C++ 상승/페이드 애니). orb와 같은 시작점.
	void SpawnScoreNumber(FVector2D StartPos, int64 Amount, int32 StepNumber, bool bIsCritical);

	/**
	 * 이벤트 점수 보너스용 드라마틱 burst (CoinFlyout 패턴)
	 * - Phase 1: ScreenCenter에서 반경 EventPopOutRadius로 팝아웃 (EventPopOutDuration)
	 * - Phase 2: TargetPos로 EventFlightDuration 동안 베지어 비행
	 * - 일반 SpawnOrbBurst 대비 더 큰 Orb / 더 긴 비행 / 더 넓은 스프레드 / 더 많은 개수
	 */
	void SpawnEventOrbBurst(FVector2D ScreenCenter, FVector2D TargetPos, int32 StepNumber, int32 OrbCount);

	// OrbCanvas의 CachedGeometry 반환 (외부에서 좌표 변환 시 사용)
	FGeometry GetCanvasGeometry() const;

	// 버스트당 첫 orb가 타겟에 도착한 순간 1회 (OfficeMainWidget이 pending 점수를 플러시)
	FOnScoreOrbArrived OnScoreOrbArrived;

	// 완료 축하 컨페티 — 분출→중력 낙하→팔랑임/트윙클→페이드. CenterNorm은 MedalBurst 전용(캔버스 정규화 좌표).
	void PlayConfettiBurst(int32 PieceCount, float GoldRatio,
		EConfettiStyle Style = EConfettiStyle::SideCannons,
		FVector2D CenterNorm = FVector2D(0.5f, 0.47f));

	// 도장 텍스트 팝("달성!") — EaseOutBack 스케일인+기울임+유지+페이드. 폰트는 호출측 소유 복사본.
	void SpawnStampText(FVector2D CenterPos, const FText& Text, const FLinearColor& Color, const FSlateFontInfo& Font);

private:
	// 개별 구슬을 하나 생성 (내부용)
	void SpawnSingleOrb(FVector2D StartPos, FVector2D TargetPos, int32 StepNumber,
		bool bIsCritical, float Delay, bool bNotifyArrival = false);

	// 개별 구슬 애니메이션 데이터
	struct FOrbAnimData
	{
		UImage* OrbImage = nullptr;
		UImage* GlowImage = nullptr;   // 글로우 후광
		UCanvasPanelSlot* OrbSlot = nullptr;   // 매 프레임 Cast 회피 (생성 시 1회 캐시)
		UCanvasPanelSlot* GlowSlot = nullptr;
		FVector2D StartPos;
		FVector2D TargetPos;
		float Duration;
		float Elapsed = 0.0f;
		float Delay = 0.0f;           // 스폰 후 대기 시간 (시차 스폰용)
		float ArcHeight;
		float Scale = 1.0f;
		float WobbleOffset = 0.0f;
		bool bCompleted = false;
		int32 StepNumber = 0;
		bool bIsCritical = false;
		bool bNotifyArrival = false;   // 버스트 대표만 true — 도착 시 OnScoreOrbArrived 1회

		// === 이벤트용 2단계 애니메이션 ===
		bool bEventStyle = false;        // true면 PopOut 단계 활성
		FVector2D PopOutTargetPos;       // Phase 1 종료 위치 (PopOut 후 위치 = 비행 시작점)
		float PopOutDuration = 0.0f;     // Phase 1 시간 (0이면 단일 단계)
		bool bPopOutDone = false;        // Phase 전환 플래그
	};

	UPROPERTY()
	UCanvasPanel* OrbCanvas = nullptr;

	TArray<FOrbAnimData> ActiveOrbs;

	// 점수 숫자 애니메이션 데이터
	struct FScoreNumberAnimData
	{
		UTextBlock* Text = nullptr;
		UCanvasPanelSlot* Slot = nullptr;
		FVector2D StartPos = FVector2D::ZeroVector;
		float Elapsed = 0.0f;
		float Lifetime = 1.2f;
		float RiseDistance = 70.0f;
	};

	TArray<FScoreNumberAnimData> ActiveNumbers;

	// 컨페티 조각 애니메이션
	struct FConfettiAnimData
	{
		UImage* Image = nullptr;
		UCanvasPanelSlot* Slot = nullptr;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D Velocity = FVector2D::ZeroVector;
		float SpinRate = 0.0f;   // deg/s
		float Angle = 0.0f;
		float Elapsed = 0.0f;
		float Lifetime = 2.0f;
		float FlutterRate = 0.0f;      // rad/s — 종이: X스케일 뒤집힘 / 스타: 트윙클 펄스
		float FlutterPhase = 0.0f;
		float TerminalFall = 1.0e9f;   // 낙하 종단속도 (종이 공기저항, 대포 스타일은 무제한)
		float HorizontalDrag = 0.0f;   // 초당 수평 감쇠 비율
		bool bStar = false;            // true=스타 조각(트윙클), false=종이 조각(팔랑임)
	};
	TArray<FConfettiAnimData> ActiveConfetti;

	// MedalBurst 스타 조각 텍스처 캐시 (Polygon_01~06, 흰색 온 투명 → 틴트 안전)
	UPROPERTY()
	TArray<UTexture2D*> CachedConfettiStarTextures;

	// 도장 텍스트 애니메이션
	struct FStampAnimData
	{
		UTextBlock* Text = nullptr;
		float Elapsed = 0.0f;
	};
	TArray<FStampAnimData> ActiveStamps;

	// 숫자 폰트 — UFont 컴포지트(LilitaOne_Font). FSlateFontInfo 는 IFontProviderInterface 구현체(UFont)만 해석 — UFontFace 직접 지정 시 기본 폰트로 조용히 폴백.
	UPROPERTY()
	UObject* NumberFont = nullptr;

	// WBP에서 에디터로 지정 (쿠커가 참조 그래프 자동 추적)
	UPROPERTY(EditDefaultsOnly, Category = "ScoreOrb")
	TArray<TSoftObjectPtr<UMaterialInterface>> OrbMaterialRefs;

	UPROPERTY()
	UMaterialInterface* CachedOrbMaterials[3] = { nullptr, nullptr, nullptr };

	// 구슬 ID 카운터 (고유 이름 생성용)
	int32 OrbIdCounter = 0;

	// 상수
	static constexpr float OrbSize = 56.0f;
	static constexpr float GlowSizeMultiplier = 2.2f;  // 글로우 후광 크기 배율
	static constexpr float FlightDuration = 1.0f;
	static constexpr float BaseArcHeight = 90.0f;
	static constexpr float CriticalScaleMultiplier = 1.4f;
	static constexpr float WobbleBase = 18.0f;
	static constexpr float BurstSpreadRadius = 25.0f;   // 다중 스폰 시 시작점 퍼짐 범위
	static constexpr float BurstDelayPerOrb = 0.04f;    // 구슬 간 시차
	// 점수 숫자 튜닝 상수는 .cpp 파일 스코프로 이동 (값 변경 = cpp-only = Live Coding 주입)

	// === 이벤트용 (드라마틱) ===
	static constexpr float EventOrbScale = 1.5f;          // 이벤트 Orb는 1.5배 큼
	static constexpr float EventPopOutRadius = 220.0f;    // 화면 중앙에서 튀어나가는 반경
	static constexpr float EventPopOutDuration = 0.22f;   // Phase 1 시간 (EaseOutBack)
	static constexpr float EventFlightDuration = 1.4f;    // Phase 2 비행 시간
	static constexpr float EventStaggerDelay = 0.07f;     // 이벤트 Orb 간 시차
	static constexpr float EventArcHeight = 60.0f;        // 비행 호 높이
};
