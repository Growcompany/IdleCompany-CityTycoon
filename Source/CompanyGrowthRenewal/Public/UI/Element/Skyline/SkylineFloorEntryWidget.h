#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "SkylineFloorEntryWidget.generated.h"

class UCommonTextBlock;
class UPanelWidget;
class USizeBox;
class UWidget;

UENUM(BlueprintType)
enum class ESkylineFloorState : uint8
{
	Cleared = 0,   // 이미 넘은 층 — 점등 + 체크
	Current,       // 지금 서 있는 층 — 열의 중심
	Locked         // 아직 못 간 층 — 요구치만
};

/**
 * 스카이라인 층 슬래브 1장 (WBP: UIE_SkylineFloorEntry).
 * 로비의 층 카드 열과 사무실 씬의 층 스파인이 같은 부품을 쓴다 — 두 곳이 다르게 생기면 같은 층으로 안 읽힌다.
 *
 * 상태별 생김새는 배경 3장으로 WBP 가 소유하고, C++ 은 어느 장을 켤지와 글자만 고른다
 * (정적 스타일은 WBP, C++ 세터 금지 — 플레이북 §3).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USkylineFloorEntryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetFloor(int32 Floor, float Requirement, ESkylineFloorState State, bool bIsBoss, bool bIsNextGate, const FText& TenantName);
	void SetCompactMode(bool bCompact);

	/** 돌파 순간 슬래브가 위에서 떨어져 꽂힌다. Reduce Motion 이면 틱 없이 제자리에 둔다. */
	void PlaySlamIn(float DurationSeconds);

	/** T01 = 0~1 진행도에 대한 세로 오프셋(px). 범위 밖은 양끝 값으로 물린다. */
	static float GetSlamOffsetY(float T01);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> FloorText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> FloorSize = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RequirementText = nullptr;

	/** 배경 3장을 한 스택에 겹쳐 두는 자리. 겹쳐 두어야 상태가 바뀌어도 슬래브 크기가 안 흔들린다.
	 *  UPanelWidget 으로 받아 Overlay·Border 어느 쪽으로 저작해도 바인딩된다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> StateRoot = nullptr;

	/** 입주 기업 플레이버. 산업 조건이 아니라 이름일 뿐이다(v0.6.1 에서 조건 삭제). */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TenantText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BossTag = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GateTag = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ClearedMark = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ClearedBG = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CurrentBG = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockedBG = nullptr;

private:
	void RefreshPresentation();

	// 낙하가 도는 동안만 0 보다 크다 — 평소엔 틱이 즉시 빠진다
	float SlamElapsed = 0.f;
	float SlamDuration = 0.f;

	ESkylineFloorState FloorState = ESkylineFloorState::Locked;
	bool bCompactMode = false;
	bool bHasRequirement = false;
	bool bHasTenantName = false;
	bool bNextGate = false;

	// 무명 네임스페이스 헬퍼는 유니티 TU 병합에서 재정의가 된다 — 클래스 안에 둔다
	static ESlateVisibility Show(bool bVisible);
};
