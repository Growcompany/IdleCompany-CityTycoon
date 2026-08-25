#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/TimerHandle.h"
#include "CityAcquisitionManager.generated.h"

struct FCityCompanyData;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCompanyCleared, int32 /*Key*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCompanyVisualCleared, int32 /*Key*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCompanyProgressChanged, int32 /*Key*/);

UENUM(BlueprintType)
enum class EAcqState : uint8 { NotAcquired, Milking, Depleted, Cleared };

USTRUCT()
struct FCityAcqRuntime
{
    GENERATED_BODY()
    EAcqState State = EAcqState::NotAcquired;
    int64 RTotal = 0;       // 굴린 총수익(숨김)
    int64 RRemaining = 0;
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UCityAcquisitionManager : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    bool CanAcquire(int32 Key) const;
    bool Acquire(int32 Key);
    EAcqState GetState(int32 Key) const;
    void GetYieldRange(int32 Key, int32& OutMin, int32& OutMax, int32& OutEv) const;
    // ⚠ OutRemaining = 숨긴 R 잔여 (스펙 §3.1 비공개). UI 표시/역산 금지 — 정산 원장·디버그 전용.
    //    금고 표시는 GetPotAmount 사용.
    bool GetCompanyProgress(int32 Key, int64& OutCost, int64& OutEarned, int64& OutRemaining, bool& OutDepleted) const;
    // 드립 기대치 — Chunk=1회 적립액, ExpectedPerSec=Chunk×확률(크리트 기대 포함, 초당 기대 적립액),
    // SecondsPerDrip=드립 1회 사이 기대 간격. 관리 패널 속도 라인이 표시 단위를 고르는 데 쓴다
    // (티어가 오르면 드립이 수분에 한 번이라 "초당" 표기가 멈춘 화면과 어긋난다).
    bool GetDripInfo(int32 Key, int64& OutChunkAmount, int64& OutExpectedPerSec, float& OutSecondsPerDrip) const;
    // 지급 가능 금고(판돈) = RTotal - RRemaining. 캐시아웃 전까지 지갑에 안 들어간다 (D8).
    int64 GetPotAmount(int32 Key) const;
    AActor* GetCompanyActor(int32 Key) const;
    void GetOccupantKeysForPlot(FName PlotId, TArray<int32>& OutKeys) const;

    // 부지 인수 게이트 0의 단일 구현 — 입주 회사가 없거나 전원 Cleared 면 true.
    // 진입점 2개(TryPurchase / CanBePurchasedNow)가 이걸 공유한다. 갈라지면 소프트락이 난다.
    bool ArePlotOccupantsCleared(FName PlotId) const;

    // ⚠ 아래 3종 재진입 계약: 구독자는 핸들러 안에서 Runtime 을 바꾸는 호출(Acquire/DebugClearAllCompanies) 금지 — 순회 중 브로드캐스트라 이터레이터 무효화.
	// 회사 정리(철거) 완료 — 부지 게이트/UI 갱신용
	FOnCompanyCleared OnCompanyCleared;
	// 회사 Actor가 실제로 Hidden/소멸된 시점 — 빈 부지 소품 등 시각 점유 해제용
	FOnCompanyVisualCleared OnCompanyVisualCleared;
    // 진행도 변경(드립 적립 / 고갈 전환) — 열려 있는 관리 패널 실시간 갱신용. 매 틱이 아니라 실제 변동 시에만 브로드캐스트.
    FOnCompanyProgressChanged OnCompanyProgressChanged;
    void Demolish(int32 Key);

    // [보석] 남은 회수를 즉시 100% 까지 진행 — 파는 것은 시간뿐이고 총 회수액(RTotal)은 인수 시점에
    // 이미 굴려져 있어 결과가 달라지지 않는다. 반환: 실행됐으면 true.
    bool SkipRecovery(int32 Key);

    // 즉시 완료 보석 가격 = 티어 × 10. 소요 시간은 티어에 지수로 늘고 가격은 선형이라 후반일수록 이득 —
    // 실제로 답답한 구간에서만 쓰이게 하려는 의도적 기울기.
    int32 GetSkipDiamondCost(int32 Key) const;

    // [치트] 모든 도시 회사를 즉시 Cleared 로. 데모 연출 없이 LoadFromGame 의 Cleared 복원과 동일한 즉시 숨김
    // (건물 hidden + 콜리전 off) + 회사별 OnCompanyCleared 브로드캐스트(비콘 제거/부지 게이트 갱신) + 클릭 프록시 제거 + 단일 저장.
    // 반환 = 새로 Cleared 된 회사 수.
    int32 DebugClearAllCompanies();

    // [치트] Milking 회사의 회수율을 Pct% 까지 즉시 진행(금고만 참, 지갑 무변동). 100 이상이면 Depleted 전환
    void DebugFillPot(int32 Key, int32 Pct);

    void OnCompanyClicked(int32 Key);

    // 관리 패널(BottomStack) 오픈 — OnCompanyClicked(Milking/Depleted) 재클릭과 인수 모달의 확인 클릭 체인이 공유하는 단일 진입점
    // (카메라 좌측 프레이밍도 이 함수가 함께 책임진다). 계약: 중복 push 가드는 호출자 책임 — 새 호출자가 늘면 이중 push 위험.
    void OpenManagePanel(int32 Key);

    void SaveToGame();   // Runtime -> GameData + persist
    void LoadFromGame();

protected:
    UPROPERTY() TMap<int32, FCityAcqRuntime> Runtime;
    UPROPERTY() TArray<AActor*> ClickProxies;
    FTSTicker::FDelegateHandle TickHandle;
    FTimerHandle ProxySpawnHandle;
    int32 DripTicksSinceSave = 0;  // 금고가 유일 장부가 되면서 생긴 롤백 노출을 10초 스로틀로 제한

    bool Tick(float Dt);

    // 드립 1틱 — 온라인 티커와 보석 압축이 공유하는 단일 구현. 갈라지면 "보석으로 건너뛴 결과"가
    // 실제로 기다린 결과와 달라진다. bSilent 면 VFX/브로드캐스트 생략(압축은 한 프레임에 수만 틱을 돈다).
    // 반환: 상태가 바뀌었으면(고갈) true. bOutDripped = 이번 틱에 적립이 있었나.
    bool StepDrip(int32 Key, FCityAcqRuntime& R, const FCityCompanyData& D, bool bSilent, bool& bOutDripped);

    void SettleOffline(float TotalGained, float OfflineSeconds);

    // 드립(Chunk 적립) 시 건물 상단 코인 분출 VFX + 코인 음. 크리트면 스케일 확대
    void PlayDripFeedback(int32 Key, bool bCrit);

	// 클릭 프록시 스폰 — OnWorldBeginPlay 에서 타이머로 지연 호출(Director 매핑 완료 보장)
	void SpawnClickProxies();
	void HandleCompanyVisualCleared(int32 Key);

    class UResourceItemManager* ResMgr() const;
    class UTableManagerSubsystem* TableMgr() const;
    class USpawnManager* SpawnMgr() const;
    class UCityCompanyDirector* Director() const;
};
