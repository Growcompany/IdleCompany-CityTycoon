#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Enum/QuestTypes.h"
#include "Table/LiveOpsTables.h"
#include "Data/LiveOpsSaveData.h"
#include "LiveOpsSubsystem.generated.h"

/**
 * 라이브옵스 콘텐츠 — 기간 이벤트(누적 출석/수집/핫타임) · 공지 · 글로벌 메일 한 곳.
 * 정의는 DT 가 기본값이고 PlayFab TitleData 가 오면 키 단위로 통째 덮는다(읽기 전용, 클라는 쓰지 않는다).
 * 활성/만료 판정과 지급은 UGameClockSubsystem 의 권위 시각이 유효할 때만 한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULiveOpsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnLiveOpsContentChanged);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHotTimeChanged, float /*Multiplier*/);

	FOnLiveOpsContentChanged OnLiveOpsContentChanged;
	FOnHotTimeChanged OnHotTimeChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void GetActiveEvents(TArray<FLiveOpsEventDef>& OutEvents) const;
	// 누적 출석 이벤트는 저장된 시작 누계와의 차이를 Points 에 채워 돌려준다
	FLiveOpsEventProgress GetEventProgress(const FString& Id) const;

	UFUNCTION(BlueprintCallable, Category = "LiveOps")
	bool ClaimEventMilestone(const FString& Id, int32 Index);

	// 운영 수익 배율 — 1분 타이머가 갱신하는 캐시 (틱에서 호출되므로 계산하지 않는다)
	UFUNCTION(BlueprintPure, Category = "LiveOps")
	float GetRevenueMultiplierNow() const { return CachedRevenueMultiplier; }

	// 고정 공지 먼저, 그 다음 최신순
	void GetNotices(TArray<FLiveOpsNoticeDef>& OutNotices) const;

	UFUNCTION(BlueprintPure, Category = "LiveOps")
	bool IsNoticeRead(const FString& Id) const;

	UFUNCTION(BlueprintCallable, Category = "LiveOps")
	void MarkNoticeRead(const FString& Id);

	UFUNCTION(BlueprintPure, Category = "LiveOps")
	bool HasUnreadNotice() const;

	UFUNCTION(BlueprintPure, Category = "LiveOps")
	bool HasClaimableEvent() const;

	// TitleData 재요청. bForce 가 아니면 마지막 요청 후 10분 안에는 무시한다
	void RefreshFromServer(bool bForce = false);

	// SaveLoadManager 핸드오프 (평구조체)
	const FLiveOpsSaveData& GetLiveOpsData() const { return Save; }
	void SetLiveOpsData(const FLiveOpsSaveData& InData) { Save = InData; }

	// 치트: 활성 수집 이벤트 점수 직접 가산
	bool DevAddEventPoints(const FString& Id, int64 Amount);

private:
	void HandleGameDataLoaded();
	void HandleCounter(EQuestCounter Counter, int64 Amount);
	void HandleAttendanceChanged();
	void HandleDayKey(const FString& NewDayKey);
	void HandleServerTimeValidityChanged(bool bValid);
	void HandleLogin(bool bSuccess);
	void HandleTitleData(const TMap<FString, FString>& Data);
	void HandleMinuteTick();

	void LoadFromTables();
	// 글로벌 메일 수신 · 진행 초기화 · 종료 이벤트 회수 · 핫타임 갱신. 반환 = 콘텐츠 변경을 방송했는지
	bool ApplyContent();
	void RecomputeHotTime();
	const FLiveOpsEventDef* FindEvent(const FString& Id) const;
	// 저장된 진행에 유형별 계산분(누적 출석)을 얹은 값
	FLiveOpsEventProgress EffectiveProgress(const FLiveOpsEventDef& Def) const;
	int64 NowUnix() const;
	bool IsClockAuthoritative() const;
	// 수령 가능하면 true, 아니면 토스트를 띄우고 false
	bool CheckClaimableOrNotify() const;
	// 스로틀 기준 시각 — 권위 시각이 있으면 그것, 없으면 플랫폼 시간
	double RefreshClockSeconds() const;

	TArray<FLiveOpsEventDef> Events;
	TArray<FLiveOpsNoticeDef> Notices;
	TArray<FLiveOpsMailDef> Mails;

	FLiveOpsSaveData Save;
	// 서버 시각·TitleData 가 세이브 로드보다 먼저 도착할 수 있다 — 기본값 Save 를 건드리면 실제 세이브를 덮는다
	bool bLoaded = false;
	float CachedRevenueMultiplier = 1.0f;
	double LastRefreshSeconds = 0.0;
	FTimerHandle ContentTimerHandle;

	static constexpr float ContentTickSeconds = 60.0f;
	static constexpr double RefreshThrottleSeconds = 600.0;
};
