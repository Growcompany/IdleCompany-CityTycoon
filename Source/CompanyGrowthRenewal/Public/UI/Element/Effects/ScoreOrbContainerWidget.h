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
class UOverlay;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextBlock;
class UFont;
class UTexture2D;
class UStageGoalTabWidget;
class UCommonTextStyle;

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
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// Office만 활성화한다. 점수/도착 알림은 기존 orb 경로가 계속 소유한다.
	void SetDevelopmentPresentationEnabled(bool bEnabled);
	// 상단 바 판 영역 — 개발 종료 순간 바를 훑는 광택과, 막을 바 쪽에서 옅게 두는 경계에 쓴다
	void SetDevelopmentStripRect(FVector2D MinLocal, FVector2D MaxLocal);
	// 칸 아래 서랍 탭. CellBottomLeftLocal = 칸 게이지 왼쪽 x · 상단 바 아래 가장자리 y(OrbCanvas 좌표)
	void PlayDevelopmentGoalTab(int32 CellIndex, FVector2D CellBottomLeftLocal, float CellWidth);
	void PlayDevelopmentFireworks(float Intensity);
	// 결과 창 바탕이 들어오는 동안 연출 막을 같은 시간에 거둬 밝기가 한 번만 바뀌게 한다
	void FadeDevelopmentDim();
	void ResetDevelopmentEffects();
	bool ValidateDevelopmentEffects(FString& OutError) const;

	// 구슬 다중 스폰 (시작 위치 주변에서 여러 개가 시차를 두고 날아감)
	void SpawnOrbBurst(FVector2D StartPos, FVector2D TargetPos, int32 StepNumber, bool bIsCritical, int32 OrbCount = 5);

	// 검수 하네스가 같은 캔버스에 비교용 요소를 꽂기 위한 통로. 게임 코드에서 쓰지 말 것.
	class UCanvasPanelSlot* AddNumberProbeSwatch(class UWidget* Probe);

	// 현재 스텝의 직능명 + 점수 "+N" 스폰. 라벨과 숫자가 함께 팝/상승/페이드한다.
	// 노출 이유 = 데미지 폰트 시각 검수를 게임플레이 진행 없이 스탠드얼론에서 직접 띄우기 위함.
	UFUNCTION(BlueprintCallable, Category = "ScoreOrb")
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
	friend class FOfficeDevelopmentEffectsLifecycleTest;
	friend class SDevelopmentEffectsLayer;
	// 불꽃 한 줄기. 위치는 매 프레임 시간의 닫힌 식으로 계산한다(되감기·배속에도 같은 모양)
	struct FDevelopmentStreak
	{
		float Angle = 0.f;
		float Speed = 0.f;
		float Lifetime = .5f;
		float Width = 2.4f;
		int32 BurstIndex = 0;
		uint8 Kind = 1;   // 0 짧은 줄기 · 1 주 줄기 · 2 멀리 뻗는 창
		bool bGlint = false;
	};
	struct FDevelopmentEmber
	{
		float Start = 0.f;
		float Lifetime = 1.f;
		float Angle = 0.f;
		float Radius = 0.f;
		float Drift = 0.f;
		float Phase = 0.f;
		int32 BurstIndex = 0;
	};
	bool bDevelopmentPresentation = false;
	bool bDevelopmentPaused = false;
	FVector2D DevelopmentStripMin = FVector2D::ZeroVector;
	FVector2D DevelopmentStripMax = FVector2D::ZeroVector;
	TArray<FDevelopmentStreak> DevelopmentStreaks;
	TArray<FDevelopmentEmber> DevelopmentEmbers;
	float DevelopmentFireworkElapsed = -1.f;
	float DevelopmentDimFade = -1.f;
	float GetDevelopmentDimAlpha() const;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UStageGoalTabWidget>> ActiveGoalTabs;
	void ClearGoalTabs();
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DevelopmentLightMaterial;
	void TickDevelopmentEffects(float DeltaTime);
	int32 PaintDevelopmentEffects(const FGeometry& Geometry, FSlateWindowElementList& Elements,
		int32 LayerId, const FWidgetStyle& WidgetStyle) const;

	// 개별 구슬을 하나 생성 (내부용)
	void SpawnSingleOrb(FVector2D StartPos, FVector2D TargetPos, int32 StepNumber,
		bool bIsCritical, float Delay, bool bNotifyArrival = false);

	// 개별 구슬 애니메이션 데이터
	struct FOrbAnimData
	{
		UImage* OrbImage = nullptr;
		UImage* GlowImage = nullptr;   // 글로우 후광
		UCanvasPanelSlot* OrbSlot = nullptr;   // 스폰 때 출발점 고정용. 이동은 렌더 트랜스폼이 맡는다 - 틱에서 쓰지 말 것
		UCanvasPanelSlot* GlowSlot = nullptr;
		FVector2D StartPos;        // 슬롯이 놓인 자리. 불변 ― 렌더 이동량의 기준이라 덮어쓰면 궤적이 통째로 어긋난다
		FVector2D FlightStartPos;  // 비행 베지어의 시작점. Phase 1 종료 시 팝아웃 지점으로 갱신된다
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
		TArray<FVector2D> DevelopmentTrail;

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
	// 림/본체/배후광을 Overlay 하나로 묶는다 — 두 글자 레이어가 서로 어긋날 수 없고, 상승·페이드도 루트 하나만 건드리면 된다
	struct FScoreNumberAnimData
	{
		UOverlay* Root = nullptr;
		UCanvasPanelSlot* Slot = nullptr;
		UImage* Burst = nullptr;        // 크리티컬 타격 버스트 — 글자와 다른(훨씬 짧은) 커브를 탄다
		float BurstAngle = 0.0f;
		TArray<UImage*> Sparks;
		FVector2D StartPos = FVector2D::ZeroVector;
		float Elapsed = 0.0f;
		float Lifetime = 1.2f;
		float RiseDistance = 70.0f;
		int64 Amount = 0;        // 겹침 해소 우선순위 — 큰 기여가 자리를 지키고 작은 쪽이 비킨다
		float Width = 0.0f;      // 조립하며 잰 실측 폭 — 겹침 판정용
		bool bCritical = false;
	};

	// 같은 자리에 몰린 숫자를 세로로 흩어 한 뭉터기가 되지 않게 한다. 반환값 = 확정된 시작 위치.
	FVector2D ResolveNumberOverlap(FVector2D Desired, int64 Amount, float Width, float Height);

	TArray<FScoreNumberAnimData> ActiveNumbers;

	// 컨페티 조각 애니메이션
	struct FConfettiAnimData
	{
		UImage* Image = nullptr;
		UCanvasPanelSlot* Slot = nullptr;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D StartPos = FVector2D::ZeroVector;   // 슬롯이 고정된 자리 — 렌더 이동량 계산 기준
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

	// === 데미지 숫자 글리프 ===
	// 글자를 폰트로 그리지 않고 미리 구운 이미지로 조립한다. Slate 는 아웃라인을 표시 크기에서
	// 즉석에 만들어 아틀라스에 굽는데, 화면 배율이 끼면 두께가 소수점이 되어 곡선마다 굵기가
	// 흔들리고, 림/키라인/채움을 TextBlock 3장으로 쌓으면 안티에일리어싱 경계가 세 번 겹친다.
	UPROPERTY()
	UMaterialInterface* GlyphMaterial = nullptr;

	UPROPERTY(Transient)
	TSubclassOf<UCommonTextStyle> NumberLabelStyle;

	UPROPERTY(Transient)
	TSubclassOf<UCommonTextStyle> CriticalNumberLabelStyle;

	// (크기 × 색 × 글자) 조합의 MID 캐시. 글자마다 텍스처가 달라 공유할 수 없으므로
	// 지연 생성해 끝까지 재사용한다 (스폰마다 만들면 할당이 폭증한다).
	UPROPERTY()
	TMap<int32, UMaterialInstanceDynamic*> GlyphMIDs;

	UMaterialInstanceDynamic* GetGlyphMID(int32 GlyphIndex, int32 ColorIndex, int32 SizeIndex);

	// 숫자 문자열을 글자 단위로 조립한다. 실패하면 nullptr.
	// 글자마다 Overlay 자식 하나 + 레이어 2~3장(검정 림 / 흰 키라인 / 그라디언트 채움).
	// ⚠ HorizontalBox + 음수 패딩으로 겹치려 하면 안 된다 — 음수 패딩은 "겹침"이 아니라
	//   "할당 폭 축소"로 읽혀, Auto 크기 자식들이 가로로 눌린다(2026-10-03 실측).
	//   렌더 트랜슬레이션은 레이아웃에 안 잡히므로 압축이 생길 수 없다.
	UOverlay* BuildNumberRow(const FString& Text, int32 StepNumber, bool bCritical,
		int32 FontSize, int32 Id, FVector2D& OutSize, float& OutFirstGlyphX);

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
