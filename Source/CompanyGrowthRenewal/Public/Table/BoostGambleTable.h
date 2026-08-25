#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BoostGambleTable.generated.h"

// 개발 이벤트 효과 타입 — 리졸브/패널 분기의 단일 스위치 (spec 2026-07-12).
// SuccessFrac/FailFrac 은 타입별로 의미 재해석: 점수 ±% / 수익 배율 ± / 리콜 ↓.
UENUM(BlueprintType)
enum class EDevEventEffect : uint8
{
	ScoreSwing   UMETA(DisplayName = "점수 스윙"),   // 즉시 ±개발 점수 (현행 기계)
	TimeExtend   UMETA(DisplayName = "시간 연장"),   // 타이머 +TimeSeconds + 점수 ±
	TimeCut      UMETA(DisplayName = "시간 단축"),   // 타이머 −TimeSeconds, 실패 시 점수 −(리콜)
	PayoffGamble UMETA(DisplayName = "수익 도박"),   // EventRewardMultiplier ± (개발 점수 무관)
};

// 부스트 도박 시나리오 (산업 인격 — 개발 ~40% 지점 타임드 결단).
// 같은 Industry 여러 행 = 풀 → 프로젝트마다 1개 랜덤. CSV 컬럼명 = UPROPERTY명.
USTRUCT(BlueprintType)
struct FBoostGambleRow : public FTableRowBase
{
	GENERATED_BODY()

	// "Game"/"Semiconductor"/... (StringToCompanyType 파싱)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	FString Industry;

	// 효과 타입 — 리졸브/패널이 이 값으로 분기. CSV엔 enum 이름(ScoreSwing/TimeExtend/...).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	EDevEventEffect EffectType = EDevEventEffect::ScoreSwing;

	// 모달 제목 (세계관 서사 — 큰 글씨)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	FText Title;

	// 한 줄 득실 (담백한 위트)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	FText Message;

	// 지른다 버튼 라벨 (세계관 리스크 동사: 야근/레버리지/물량 공세…)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	FText GoLabel;

	// 안전 버튼 라벨 (세계관 안전 동사: 휴식/현금 보유/재고 관리…)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	FText SafeLabel;

	// 성공 확률 (0~1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	float SuccessChance = 0.6f;

	// 성공 magnitude — 점수↑ 비율(스텝 목표합 대비) / 수익 배율↑. (TimeCut 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	float SuccessFrac = 0.22f;

	// 실패 magnitude — 점수↓ 비율 / 수익 배율↓ / 리콜 하락.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	float FailFrac = 0.16f;

	// 타이머 증감 초 — TimeExtend(연장)/TimeCut(단축)만 사용, 그 외 0.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	float TimeSeconds = 0.0f;

	// 지른다 확정 비용(Money) — 야근수당 등. 0이면 무료. 여력 없으면 지른다 비활성.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BoostGamble")
	int32 GoCost = 0;
};
