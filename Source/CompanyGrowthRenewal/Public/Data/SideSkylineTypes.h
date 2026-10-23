#pragma once

#include "CoreMinimal.h"
#include "Table/MissionTable.h"
#include "SideSkylineTypes.generated.h"

/** 등반을 막는 이유. UI 가 문구를 고르는 단일 기준 — 화면마다 다른 문장을 쓰지 않는다. */
UENUM(BlueprintType)
enum class ESkylineBlockReason : uint8
{
	None = 0,
	NoBuilding,   // 보유 건물 0 — 고를 회사가 없다
	NoCrew,       // 고른 회사에 배치 직원 0명
	NoEntry,      // 입장권 0
	HQLocked,
	MaxFloor,     // 최고 층 도달
	PowerShort    // 다음 한 층도 못 넘는다 — 입장권은 쓰지 않는다(설계 §0-4)
};

/**
 * 등반 1회의 결과. **계산은 한 번, 연출은 층 단위**(설계 v0.8 §모듈2) —
 * 사무실 씬은 이 구조체를 재생만 하고 다시 판정하지 않는다.
 */
USTRUCT(BlueprintType)
struct FSkylineRunResult
{
	GENERATED_BODY()

	/** 등반 직전의 최고 층. */
	UPROPERTY(BlueprintReadOnly)
	int32 FromFloor = 0;

	/** 등반 뒤의 최고 층. FromFloor 와 같으면 한 층도 못 올랐다는 뜻이다. */
	UPROPERTY(BlueprintReadOnly)
	int32 ReachedFloor = 0;

	/** 막힌 층(= ReachedFloor + 1). 최고 층에 닿았으면 0. */
	UPROPERTY(BlueprintReadOnly)
	int32 StopFloor = 0;

	/** 막힌 층의 요구치. 결과 카드가 "무엇 때문에 멈췄는지" 를 말하는 유일한 숫자. */
	UPROPERTY(BlueprintReadOnly)
	float StopRequirement = 0.0f;

	/** 이번 등반에 쓴 팀의 파워. 결과 카드가 요구치와 나란히 보여 준다. */
	UPROPERTY(BlueprintReadOnly)
	float TeamPower = 0.0f;

	/** 출전한 회사(건물). */
	UPROPERTY(BlueprintReadOnly)
	int32 BuildingIndex = INDEX_NONE;

	/** 이번에 넘은 보스 층들. 비면 관문을 하나도 못 넘었다. */
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> ClearedBossFloors;

	/** 이번에 처음 깬 층들(오름차순). 사무실 씬이 한 칸씩 재생하는 대상. */
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> ClearedFloors;

	/** 층별 1회성 보상을 전부 합친 것. 지급은 Climb 안에서 이미 끝났다. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FMissionReward> Rewards;

	/** 한 층이라도 올랐는가 = 입장권을 썼는가. */
	bool DidClimb() const { return ReachedFloor > FromFloor; }
};
