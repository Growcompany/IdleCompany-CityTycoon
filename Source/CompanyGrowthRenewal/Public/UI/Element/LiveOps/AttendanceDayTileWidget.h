// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "AttendanceDayTileWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UItemCardSlotWidget;
class UWidget;

UENUM()
enum class EAttendanceTileState : uint8
{
	Future,
	Today,
	// 오늘 받은 칸 — 그림은 Done 과 같고 패널의 수령 연출(도장 떨어짐)과 재탭 잠금만 가른다
	ClaimedToday,
	Done
};

/** 오늘 칸 탭 = 수령 (R33). CTA 와 같은 `HandleClaim` 으로 들어가므로 인자는 날짜 하나면 된다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttendanceTileTapped, int32 /*Day*/);

/** 상태 → 레이어 가시성. 위젯이 없어도 판정할 수 있게 순수 구조체로 뽑았다(렌더 테스트가 이걸 직접 민다). */
struct COMPANYGROWTHRENEWAL_API FAttendanceTileVisibility
{
	// 판 5벌 중 정확히 한 장 — 머리띠 색까지 구워져 있어 상태 = 판 한 장이다
	bool bPlateNormal = false;
	bool bPlateToday = false;
	bool bPlateGold = false;
	bool bPlateDone = false;
	bool bPlateGoldDone = false;
	bool bBadgeDefault = false;
	bool bBadgeToday = false;
	bool bBadgeMilestone = false;
	bool bTodayRing = false;
	// 감속 설정 끄기는 위젯이 한다 — 표는 전역 설정을 읽지 않는다
	bool bTodaySweep = false;
	bool bSeal = false;
	bool bIconDimmed = false;

	static FAttendanceTileVisibility Resolve(EAttendanceTileState State, bool bMilestone);
};

/**
 * 한 칸의 **해석이 끝난** 표시 데이터. 아이콘 해석은 서브시스템을 타므로 뷰 생성 단계에서 끝내고
 * 위젯은 주입만 받는다 — 그래서 렌더 하네스가 GameInstance 없이 프레임을 세울 수 있다.
 */
struct COMPANYGROWTHRENEWAL_API FAttendanceDayView
{
	int32 Day = 1;
	bool bMilestone = false;
	EAttendanceTileState State = EAttendanceTileState::Future;
	// 해석 실패면 null — 카드를 접는다(WBP 에 구워진 플레이스홀더가 정확한 수량과 함께 보이는 오표시를 막는다)
	TWeakObjectPtr<UObject> PrimaryIcon;
	TWeakObjectPtr<UObject> ExtraIcon;
	FText PrimaryLabel;
	FText ExtraLabel;
	bool bHasExtra = false;
	bool bRowFound = false;
};

/**
 * 출석 달력 1칸. 판 5벌 · 머리띠 날짜 3벌 · 보상 카드 · 결재 도장 · 오늘 링/빛띠.
 * 색·크기는 WBP 가 소유하고 C++ 은 가시성 · 상태 잉크 선택 · 데이터 · 입력 · 도장 곡선만 고른다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UAttendanceDayTileWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetDayView(const FAttendanceDayView& Day);

	// 격자 메아리 — 도장이 위에서 떨어져 붙는다. 감속 설정이면 이동 없이 불투명도만 올린다
	void PlayStampEcho(bool bReduceMotion);

	// 오늘 칸만 입력을 받는다 — 나머지 칸은 뒤의 웰이 그대로 히트를 먹는다 (R33)
	void SetTappable(bool bInTappable);

	FOnAttendanceTileTapped OnTapped;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 아이콘 상자 — 그림은 투명 여백을 뺀 실제 비율로 이 안에 꽉 찬다(카드 본체 bFitIconToContent). 보상 둘이면 폭 반 · 높이 0.72
	UPROPERTY(EditAnywhere, Category = "Attendance|Layout", meta = (ClampMin = 0.0))
	float IconBoxWidth = 146.f;

	UPROPERTY(EditAnywhere, Category = "Attendance|Layout", meta = (ClampMin = 0.0))
	float IconBoxHeight = 108.f;

	// 상태 잉크는 WBP 가 저작한다 — 머리띠 색이 판마다 달라 글자색도 상태로 고른다
	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor DayInkNormal = FSlateColor::UseForeground();

	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor DayInkToday = FSlateColor::UseForeground();

	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor DayInkMilestone = FSlateColor::UseForeground();

	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor DayInkDone = FSlateColor::UseForeground();

	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor DayInkMilestoneDone = FSlateColor::UseForeground();

	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor QtyInkDone = FSlateColor::UseForeground();

	// 받은 칸 그림 곱색 — 덮개는 투명 여백 밑 칸 면까지 네모로 어둡게 해서 그림 자체에 곱한다 (스펙 §8.5-1)
	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FLinearColor IconDimTint = FLinearColor::White;

	// 놓인 −14° 는 도장 텍스처에 구워져 있다 — 여기 각도는 떨어지기 시작할 때 그 위에 더 기우는 양이다
	UPROPERTY(EditAnywhere, Category = "Attendance|Seal", meta = (ClampMin = 0.0))
	float SealDropPx = 70.f;

	UPROPERTY(EditAnywhere, Category = "Attendance|Seal")
	float SealExtraTiltDeg = 22.f;

	UPROPERTY(EditAnywhere, Category = "Attendance|Seal", meta = (ClampMin = 0.01))
	float SealDropDuration = 0.42f;

	UPROPERTY(EditAnywhere, Category = "Attendance|Sweep", meta = (ClampMin = 0.1))
	float SweepPeriod = 2.8f;

	// --- v1 트리에 이미 있는 이름 = required 유지 ---
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DayText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MilestoneDayText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UItemCardSlotWidget> RewardCard = nullptr;

	// 결재 도장 그림 — 이름은 v1 그대로 둔다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> DoneMark = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TodayRing = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DayTodayText = nullptr;

	// 머리띠 날짜 3벌 — 한 벌만 켠다 (R31)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> DayBadge = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> DayBadgeToday = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> MilestoneBadge = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> MilestoneCrown = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TilePlate = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TilePlateDone = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TilePlateGold = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> ExtraRewardBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> ExtraRewardIcon = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ExtraRewardQty = nullptr;

	// 탭 프레스 피드백 테 — 누른 순간에만 가시성 토글로 켠다 (R33)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TodayHaloFlash = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TilePlateToday = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TilePlateGoldDone = nullptr;

	// 칸 폭으로 자르는 부모 안에서 RenderTranslation 으로만 움직인다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> TodaySweep = nullptr;

private:
	// 무명 네임스페이스 헬퍼는 유니티 TU 병합에서 재정의가 된다 — 클래스 안에 둔다
	static ESlateVisibility Show(bool bVisible);
	void ApplyIconLayout(const FAttendanceDayView& Day);
	void ApplyStateInks(const FAttendanceTileVisibility& Vis);
	void CacheAuthoredLooks();
	void ApplySealPose(float T01);
	void ResetSealPose();
	void TickSweep(const FGeometry& MyGeometry, float InDeltaTime);
	void PlaceSweep(double InTileWidth);

	// 메아리 중에만 일한다(틱 자체는 매 프레임 오지만 첫 줄에서 빠진다) — 28칸이 매번 일하면 창 하나가 프레임을 먹는다
	bool bEchoPlaying = false;
	bool bEchoReduced = false;
	float EchoElapsed = 0.f;

	bool bSweepActive = false;
	float SweepElapsed = 0.f;

	// 받은 칸에서 바꾼 수량 잉크·그림 곱색을 되돌릴 저작값 — 칸은 재사용되므로 처음 그리기 전에 한 번 읽어 둔다
	bool bAuthoredLooksCached = false;
	FSlateColor AuthoredQtyInk;
	FSlateColor AuthoredExtraQtyInk;
	FLinearColor AuthoredIconTint = FLinearColor::White;
	FLinearColor AuthoredExtraIconTint = FLinearColor::White;

	// 탭 대상 여부와 마지막으로 그린 날짜 — 브로드캐스트가 실어 보낼 값이다
	bool bTappable = false;
	int32 CurrentDay = 0;

	bool bPressFlashPlaying = false;
	float PressFlashElapsed = 0.f;

	static constexpr float PairBoxHeightRatio = 0.72f;
};
