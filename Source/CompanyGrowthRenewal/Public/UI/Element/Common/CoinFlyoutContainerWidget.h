// 코인 플라이아웃 컨테이너 위젯
// 화면 중앙에서 코인 이미지들이 Money 아이콘으로 날아가는 수집 연출

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CoinFlyoutContainerWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UTextBlock;
class UHorizontalBox;

// 코인 도착 시 콜백 (도착한 코인 인덱스)
DECLARE_DELEGATE_OneParam(FOnCoinArrived, int32);
// 모든 코인 완료 콜백
DECLARE_DELEGATE(FOnAllCoinsComplete);
// 스트리밍 코인 1개 도착 콜백 (운영 수익 드립)
DECLARE_DELEGATE(FOnStreamCoinArrived);

UCLASS()
class COMPANYGROWTHRENEWAL_API UCoinFlyoutContainerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 코인 날리기 시작
	void StartFlyout(FVector2D InTargetPos, int32 InNumCoins, UTexture2D* InCoinTexture);

	// 스트리밍 모드: 임의 시작점(직원 위치)에서 타겟(돈 아이콘)으로 코인 1개 발사.
	// 원샷 StartFlyout 과 독립 — 컨테이너 상주, 도착 코인은 즉시 제거. 좌표는 둘 다 Absolute.
	// SourceId = 스로틀 분리 키(직원 ID 등). INDEX_NONE 이면 호출자 전체가 한 게이트를 공유.
	void SpawnStreamCoin(FVector2D StartAbsolute, FVector2D TargetAbsolute, UTexture2D* InCoinTexture, int32 SourceId = INDEX_NONE);

	// 수익 텍스트 "+N" — 아이콘과 한국식 자금 축약값을 일시적 게임 피드백용 NEXON Bold로 표시한다. 코인과 같은 시작점.
	void SpawnIncomeText(FVector2D StartAbsolute, int64 Amount, UTexture2D* InMoneyTexture, FLinearColor MoneyColor);

	// 외부 콜백
	FOnCoinArrived OnCoinArrived;
	FOnAllCoinsComplete OnAllCoinsComplete;
	FOnStreamCoinArrived OnStreamCoinArrived;

	// 스트림 코인 표시 크기 — 스폰 전에 세팅. 비행 중 RenderScale 0.85 가 곱해지므로 체감은 이 값의 85%.
	float StreamCoinDrawSize = 64.0f;

	// 원샷 플라이아웃 튜닝(StartFlyout 전에 InGameLayer 가 세팅) — 넓은 구역에서 한번에 터지는 느낌 + 도착 롤업.
	float SpawnRadius = 700.0f;    // 스폰 반경(넓을수록 화면 넓게 흩뿌림)
	float StaggerDelay = 0.01f;    // 코인 간 출발 간격(작을수록 한번에 터짐)
	float ArrivalSpread = 0.4f;    // 비행 시간 랜덤 편차(클수록 도착이 퍼져 카운터가 다다닥 롤업)

protected:
	virtual bool Initialize() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// 개별 코인 애니메이션 데이터
	struct FCoinAnimData
	{
		UImage* CoinImage = nullptr;
		UCanvasPanelSlot* CachedSlot = nullptr;  // 매 프레임 Cast 회피 (생성 시 1회 캐시)
		FVector2D StartPos_Absolute;  // 화면 중앙 (Absolute 좌표)
		FVector2D SpawnPos_Absolute;  // 팝아웃 도착 위치 (Absolute 좌표)
		float Delay;                  // 출발 지연 (초)
		float Duration;               // Phase 2 이동 시간 (초)
		float Elapsed = 0.0f;         // 경과 시간
		bool bStarted = false;        // 이동 시작 여부
		bool bCompleted = false;      // 도착 완료 여부
	};

	// 스트리밍 코인 데이터 (원샷과 분리 — 인덱스/제거 시맨틱이 달라서)
	struct FStreamCoinData
	{
		UImage* CoinImage = nullptr;
		UCanvasPanelSlot* CachedSlot = nullptr;
		FVector2D StartAbs = FVector2D::ZeroVector;
		FVector2D TargetAbs = FVector2D::ZeroVector;
		float Elapsed = 0.0f;
		float Duration = 0.9f;
	};

	UPROPERTY()
	UCanvasPanel* CoinCanvas = nullptr;

	TArray<FCoinAnimData> CoinAnimations;
	// 수익 텍스트 애니메이션 데이터
	struct FIncomeTextData
	{
		UHorizontalBox* Row = nullptr;
		UCanvasPanelSlot* CachedSlot = nullptr;
		FVector2D StartAbs = FVector2D::ZeroVector;
		float Elapsed = 0.0f;
		float Duration = 1.1f;
	};

	TArray<FStreamCoinData> StreamCoins;
	TArray<FIncomeTextData> IncomeTexts;
	int32 StreamCoinIdCounter = 0;

	// 일시적 게임 피드백 수익 텍스트용 NEXON Bold 폰트
	UPROPERTY()
	UObject* IncomeFont = nullptr;
	// 스폰 스로틀 시각을 SourceId(직원 ID) 별로 분리 — 단일 타이머면 먼저 틱한 직원이 계속 이겨 나머지가 침묵한다.
	TMap<int32, float> LastStreamSpawnBySource;
	FVector2D TargetPosition_Absolute;
	float TotalElapsed = 0.0f;
	int32 CoinsCompleted = 0;
	int32 TotalCoins = 0;
	bool bAnimating = false;

	// 개별 코인 도착 처리
	void OnSingleCoinArrived(int32 CoinIndex);

	// 코인 도착음 — 연타 감쇠 + 피치 랜덤. 원샷/스트림 두 경로가 상태를 공유해 서로 겹쳐 터지지 않게 한다
	void PlayCoinArriveSound(float MinGap);

	// 감쇠 상태는 인스턴스 멤버 — 컨테이너가 동시에 여러 개 살아 있어도 서로 간섭하지 않게
	float LastCoinArriveSoundTime = -100.0f;
	float CoinArriveSoundVolume = 1.0f;

	// 상수 (StaggerDelay/SpawnRadius/ArrivalSpread 는 위 public 멤버로 이동 — 런타임 튜닝)
	static constexpr float CoinSize = 72.0f;
	static constexpr float FlightDuration = 1.2f;     // Phase 2 비행 시간
	static constexpr float PopOutDuration = 0.15f;    // Phase 1 팝아웃 시간
	static constexpr float FadeOutDelay = 0.3f;       // 모든 코인 도착 후 위젯 제거 딜레이

	// === 코인 도착음 rapid-fire 감쇠 (100개 도착 스프레드가 그대로 100발이 되던 스팸 차단) ===
	static constexpr float CoinSoundMinGap = 0.08f;      // 원샷 도착음 최소 간격
	static constexpr float CoinSoundDecay = 0.7f;        // 연타마다 볼륨 배율
	static constexpr float CoinSoundVolumeFloor = 0.35f; // 감쇠 하한
	static constexpr float CoinSoundResetGap = 0.8f;     // 이만큼 조용하면 볼륨 1.0 리셋

	// === 스트리밍 코인 (운영 수익 드립) ===
	static constexpr int32 MaxStreamCoins = 30;       // 동시 비행 상한 (초과 스폰 스킵)
	// 미사용 — 실제 드로우 크기는 StreamCoinDrawSize 가 대체 (레거시 정리 대기)
	static constexpr float StreamCoinSize = 44.0f;    // 드립 코인은 원샷(72)보다 작게
	static constexpr float StreamFlightDuration = 0.9f;
	static constexpr float StreamPopHeight = 70.0f;   // 시작점 위로 볼록한 베지어 제어점 높이
	static constexpr float StreamSoundMinGap = 0.25f; // 착지음 최소 간격
	static constexpr float StreamSpawnMinGap = 0.4f;  // SourceId 당 스폰 최소 간격 (직원 수익 틱 ~1s 보다 짧아 매 틱 통과)

	// === 수익 텍스트 "+N" ===
	static constexpr int32 MaxIncomeTexts = 24;       // 전역 상한
	static constexpr float IncomeTextLifetime = 1.1f;
	static constexpr float IncomeTextRise = 64.0f;
	static constexpr float IncomeIconSize = 48.0f;
	static constexpr float IncomeIconGap = 8.0f;
	static constexpr int32 IncomeFontSize = 36;
	static constexpr float IncomeTextFadeStart = 0.55f;
};
