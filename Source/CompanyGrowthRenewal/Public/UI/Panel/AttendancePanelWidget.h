// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Common/CountdownText.h"
#include "UI/Element/LiveOps/AttendanceDayTileWidget.h"
#include "UI/Element/LiveOps/AttendanceMilestoneNodeWidget.h"
#include "UI/Panel/LiveOpsModalWidget.h"
#include "AttendancePanelWidget.generated.h"

class UButtonWidget;
class UCommonButtonStyle;
class UCommonTextBlock;
class UImage;
class UProgressBar;
class UUniformGridPanel;
struct FAttendanceTable;

namespace CGRewardChip
{
	struct FDisplayEntry;
}

// 수령 연출은 격자·버튼을 유예한다 — 스펙 §5 의 0.24/0.30 타이밍이 그 유예의 이유다
UENUM()
enum class EAttendanceApplyScope : uint8
{
	All,
	ExceptGrid
};

/**
 * 한 프레임의 출석 표시값 전부(아이콘 해석까지 끝난 상태). 서브시스템은 BuildView 만 읽고
 * ApplyView 는 이 구조체만 본다 ― 렌더 테스트가 매니저 없이 상태별 프레임을 세울 수 있는 이유다.
 */
struct COMPANYGROWTHRENEWAL_API FAttendanceView
{
	int32 NextDay = 1;
	int32 CycleDay = 0;
	bool bCanClaim = false;
	bool bClaimedToday = false;
	bool bServerTimeValid = true;

	// 28칸(Index = Day-1). 비어 있으면 격자를 건드리지 않는다
	TArray<FAttendanceDayView> Days;

	// 28 수령 직후의 다음 날은 Done 조건에도 걸린다 — 새 순환이면 2..28 을 비운다
	bool IsNewCycle() const;
	EAttendanceTileState TileState(int32 Day) const;
	// 오늘 일차 — 받기 전에는 받을 칸, 받은 뒤에는 방금 받은 칸
	int32 ShownDay() const;
	// 이번 순환에서 받은 칸 수 — 새 순환 첫날은 지난 28칸이 아직 Done 이어도 0 이다
	int32 ClaimedCount() const;
	// 아직 안 받은 첫 특별 칸(DT_Attendance bMilestone). 이번 순환에 더 없으면 nullptr
	const FAttendanceDayView* NextMilestoneDay() const;
	// 다음 특별 칸 일차 − 오늘 일차(받기 전후 같다). 다음 칸이 없거나 오늘 칸이면 0
	int32 DaysToNextMilestone() const;
	// 「특별 보상까지 N일」 / 「특별 보상을 모두 받았습니다」. 빈 FText = 줄을 접는다(오늘이 특별 칸 · 표 없음)
	FText MilestoneLine() const;
	EAttendanceNodeState MilestoneNodeState(int32 Day) const;
	// 완료판(+카운트다운)은 권위 시각이 있을 때만 — 시간이 없으면 「다음 출석까지」가 거짓 숫자를 센다
	bool ShowsClaimedPlate() const;
};

// 진행 막대 눈금 — 원판 중심과 채움 끝이 같은 식을 쓴다(스펙 ③). 트랙 폭 = 테 3 을 포함한 바깥 폭
struct COMPANYGROWTHRENEWAL_API FAttendanceTrackScale
{
	static constexpr double RimPx = 3.0;
	// 마지막 원판이 막대 안쪽 끝에 닿도록 눈금 길이에서 반지름을 뺀다
	static constexpr double NodeRadiusPx = 34.0;

	static double NodeCenterX(int32 Day, double TrackWidth);
	static float FillPercent(int32 Claimed, double TrackWidth);
};

// 칸에 마지막으로 넣은 표시 상태 — 같은 값이면 28칸을 다시 구성하지 않는다
struct FAttendanceTileKey
{
	int32 Day = 0;
	EAttendanceTileState State = EAttendanceTileState::Future;
	bool bMilestone = false;
	bool bApplied = false;

	bool operator==(const FAttendanceTileKey& Other) const
	{
		return bApplied == Other.bApplied && Day == Other.Day && State == Other.State && bMilestone == Other.bMilestone;
	}
};

/**
 * 출석 창(아담한 판) — 7×4 달력 + 아래 한 줄(이번 달 출석 · 특별 보상 원판 · [출석 보상 받기]).
 * 수령 경로는 둘 ― CTA 와 오늘 칸 탭(R33). 나머지 칸은 표시 전용이다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UAttendancePanelWidget : public ULiveOpsModalWidget
{
	GENERATED_BODY()

public:
	// 표시값 주입 — 서브시스템을 만지지 않는 순수 함수(R3). RefreshAll 이 BuildView 로 만들어 넘긴다
	void ApplyView(const FAttendanceView& View, EAttendanceApplyScope Scope = EAttendanceApplyScope::All);

	// 수령 연출 입구 — 매니저 수령 알림과 렌더 하네스가 같은 길로 들어온다. AfterClaim = 수령 직후 프레임
	void PlayClaimSequence(const FAttendanceView& AfterClaim, int32 ClaimedDay);

	// DT 한 행 → 칸 표시값. 아이콘 해석만 호출자가 고른다 — 하네스가 GameInstance 없이 같은 규칙으로 실물 픽스처를 만든다
	static void FillDayFromRow(FAttendanceDayView& DayView, const FAttendanceTable& Row,
		TFunctionRef<UObject*(const CGRewardChip::FDisplayEntry&)> ResolveIcon);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void RefreshAll() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> CalendarGrid = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CycleText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NextMilestoneText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UProgressBar> CycleBar = nullptr;

	// 진행 막대 위 특별 보상 원판 4개 — 어느 일차인지는 DT bMilestone 이 정한다(7·14·21·28 을 박지 않는다).
	// 부모는 막대와 왼쪽 끝이 같은 캔버스다: C++ 은 가로 위치만, 세로는 WBP 저작값 그대로 둔다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UAttendanceMilestoneNodeWidget> MilestoneNode0 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UAttendanceMilestoneNodeWidget> MilestoneNode1 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UAttendanceMilestoneNodeWidget> MilestoneNode2 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UAttendanceMilestoneNodeWidget> MilestoneNode3 = nullptr;

	// 막대(CycleBar) 바깥 폭 — 첫 ApplyView 는 레이아웃 전이라 지오메트리를 못 읽는다. 저작 폭과 같아야 한다(렌더 테스트가 대조)
	UPROPERTY(EditAnywhere, Category = "Attendance|Layout", meta = (ClampMin = 0.0))
	float ProgressTrackWidth = 904.f;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> ClaimShine = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> ClaimedPlate = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ClaimedCountdown = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ClaimedTitle = nullptr;

	// 골드 한 벌만 남는다 — D-5' 로 그레이 스왑이 폐기됐다(수령 완료 = ClaimedPlate 교체)
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<UCommonButtonStyle> ClaimStyle_Gold;

private:
	// 무명 네임스페이스 헬퍼는 유니티 TU 병합에서 재정의가 된다 — 클래스 안에 둔다
	static ESlateVisibility Show(bool bVisible);
	// 출석판은 재화에도 "x" 를 붙인다(시안 ④) — 아이템과 수량 줄이 같은 문법으로 읽혀야 한다
	static FText FormatTileQuantity(bool bResource, const FText& Quantity);

	void HandleAttendanceChanged();
	void HandleDayKey(const FString& NewDayKey);
	void HandleClaim();
	// 오늘 칸 탭 = 수령 (R33). 칸은 날짜만 실어 보내고 수령 여부 판정은 여기서 CTA 와 같은 경로로 모은다
	void HandleTileTapped(int32 Day);
	void BuildGridOnce();
	// 여기서만 서브시스템을 읽는다 — DT 행 조회와 아이콘 해석까지 끝낸 View 를 돌려준다
	FAttendanceView BuildView() const;
	// 카운트다운은 완료판 한 곳에서만 돈다 — 판이 숨으면 타이머도 멈춘다
	void RetargetCountdown(bool bClaimedPlateVisible);
	void ApplyClaimedTitle();

	void TickClaimSequence(float DeltaTime);
	void TickClaimShine(float DeltaTime);
	void ResetClaimShine();
	void ApplyProgress(const FAttendanceView& View);

	UPROPERTY()
	TArray<TObjectPtr<UAttendanceDayTileWidget>> Tiles;

	TArray<FAttendanceTileKey> TileKeys;
	FCountdownTicker Countdown;

	// CTA 샤인은 창이 열려 있고 감속 설정이 꺼진 동안만 갱신한다.
	bool bClaimShineTickEnabled = false;
	bool bReduceMotion = false;
	float ClaimShineTime = 0.f;

	// 수령 연출
	bool bAwaitClaimFx = false;
	bool bClaimFxPlaying = false;
	float ClaimFxElapsed = 0.f;
	int32 PendingClaimDay = 0;
	bool bEchoFired = false;
	bool bSwapFired = false;
	// 연출이 끝나는 0.45s 까지 들고 있는 "수령 후" 프레임 — 격자와 버튼이 스펙 타이밍에 맞춰 여기서 꺼내 쓴다
	FAttendanceView PendingView;
	bool bCountdownOnClaimedPlate = false;

	// 스펙 §11-3 타임라인 — 칸 도장 메아리(0.24) 와 버튼 → 완료판 교차(0.30~0.45)만 남았다
	static constexpr float EchoAt = 0.24f;
	static constexpr float SwapAt = 0.30f;
	static constexpr float SwapCrossfade = 0.15f;
	static constexpr float ClaimFxDuration = SwapAt + SwapCrossfade;

	// CTAShine 스윕은 HQ 관리판과 같은 값을 쓴다 (UI_VFX_SOURCEBOOK §2)
	static constexpr float ShinePeriod = 2.6f;
	static constexpr float ShineTravel = 0.6f;
	static constexpr float ShineSweepHalf = 240.f;
};
