// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FatigueConfig.generated.h"

/**
 * 직원 피로도(Part A — 살아있는 사무실) 전역 튜닝값.
 * 단일 DataAsset(하드참조) — 기획자 조정용. 산업/타입 무관 전역 상수라 DT 아닌 DataAsset.
 */
UCLASS(BlueprintType)
class COMPANYGROWTHRENEWAL_API UFatigueConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// 개발(Stage) 중 타이핑 시 초당 피로 증가. 피로는 "개발 크런치" 스탯이라 방치/운영에선 누적되지 않음.
	// 체력 0 기준값 — 실제 증가 = 이값 / (1 + Stamina * StaminaGainScale)
	UPROPERTY(EditAnywhere, Category = "Accrual")
	float GainPerSec = 12.0f;

	// 개발 밖(방치/운영) 초당 회복. 느려야 연타 개발의 누적이 성립함(빠르면 판 사이에 리셋돼 무의미).
	UPROPERTY(EditAnywhere, Category = "Accrual")
	float SelfRestRecoverPerSec = 0.8f;

	// Stamina 1당 누적 감쇠 계수 (GainScale = 1 / (1 + Stamina * 이값)) — ★15 평균(140)에서 격차 5배.
	// 이 식 자체가 이미 포화형이라 곡선 전환 불필요. ★ 밴드(2026-07-27) 로 유효값 35→140 이 되며 0.114→0.029.
	UPROPERTY(EditAnywhere, Category = "Accrual")
	float StaminaGainScale = 0.029f;

	// Tired 밴드 진입 임계
	UPROPERTY(EditAnywhere, Category = "Bands")
	float TiredThreshold = 40.0f;

	// 피로→출력 커플링 — Tired 초과 구간 선형 감산 최대 비율 (피로 100에서 출력 ×(1−이값))
	UPROPERTY(EditAnywhere, Category = "Bands")
	float MaxFatiguePenalty = 0.5f;

	// Slacking 밴드 진입 임계 = 다운(꾸벅/딴짓) 발동점.
	// 머리 위 피로바가 Fatigue/100 을 그대로 그리므로 100 이어야 "바가 꽉 차면 쓰러진다"가 화면과 일치한다
	// (구 80 은 바가 80% 에서 멈춘 채 쓰러져 보였음 — 2026-07-29 사용자 지적).
	UPROPERTY(EditAnywhere, Category = "Bands")
	float SlackingThreshold = 100.0f;

	// 캐치 탭 1회당 피로 부분 차감 (하드 0 리셋 아님). 늘어짐 총회복량과 맞춰야 "탭=순이득, 차이는 6초"가 성립.
	UPROPERTY(EditAnywhere, Category = "Intervention")
	float TapRelief = 60.0f;

	// 머티리얼 MID 갱신 최소 변화량 (0~1) — throttle
	UPROPERTY(EditAnywhere, Category = "Visual")
	float MidUpdateDelta = 0.05f;

	// 동시에 빨강으로 보일 수 있는 최대 워커 수 (화면 나그 방지)
	UPROPERTY(EditAnywhere, Category = "Tuning")
	int32 MaxVisibleRedWorkers = 6;

	// 슬랙 루프 — Slacking 도달 후 "캐치 윈도우"(초). 이 안에 클릭하면 다시 일, 못 하면 일어나 배회.
	UPROPERTY(EditAnywhere, Category = "Slack")
	float SlackTelegraphDuration = 3.0f;

	// 캐치 실패 시 자리에서 늘어지는 시간(초). 이 동안 기여 0 — 개발 판보다 짧아야 만회 가능.
	UPROPERTY(EditAnywhere, Category = "Slack")
	float SlackSlumpDuration = 6.0f;

	// 늘어짐 중 초당 회복 — Duration 과 곱해 TapRelief 와 비슷해야 탭/미탭의 차이가 "시간 손해"로만 남음.
	UPROPERTY(EditAnywhere, Category = "Slack")
	float SlumpRecoverPerSec = 10.0f;

	// ── 폭주(뛰쳐나감) — 피로 사슬과 독립. 체력이 조는 것을, 침착성이 뛰쳐나감을 각각 소유한다 ──

	// 개발(Stage) 중 직원별로 매초 굴리는 폭주 확률.
	// 판이 15~50초라 "기대 발생 시간"이 아니라 판당 인원으로 잡는다: 판당 기대 = N명 × (1 − (1−이값)^판길이).
	// 0.018 = 30초 판·5명에서 초반(침착성 5) 약 1.85명 / ★13+ 상한 도달 시 0.26명 — 초반은 험하고 키우면 잦아든다.
	// ⚠ 인원수에 선형 — 10명이면 판당 2명. 너무 잦으면 인원 보정(√N)이나 사무실 단위 굴림으로 옮기는 것이 정석.
	// ⚠ 구 BoltChance(0.35)는 "늘어짐을 끝까지 놓쳤을 때"의 조건부 꼬리 확률이라 의미가 다르다 — 그 값을 여기 쓰면 상시 난장판이 된다.
	UPROPERTY(EditAnywhere, Category = "Bolt")
	float BoltChancePerSec = 0.018f;

	// 침착성 감쇠의 초반 기울기. 2026-07-27 부터 식이 포화곡선이다:
	//   실효 = BoltChancePerSec × (1 − MaxBoltReduction × V/(V+K)),  K = MaxBoltReduction / 이값
	// 구 Min(V×이값, Cap) 은 K 지점에서 절벽이었다(그 위로 포인트가 0원). 곡선은 캡에 안 닿지만 계속 오른다.
	// 0.030 유지 = 초반 기울기 불변 + 반효과 지점 K=30.
	UPROPERTY(EditAnywhere, Category = "Bolt")
	float ComposureBoltScale = 0.030f;

	// 폭주 감쇠 점근 상한. 완전 면역이면 살아있는 사무실 연출 자체가 죽으므로 하한을 남긴다.
	// 포화곡선이라 실제로 도달하지 않는다 — ★15(유효 140) 에서 0.74 수준.
	UPROPERTY(EditAnywhere, Category = "Bolt")
	float MaxBoltReduction = 0.9f;

	// 뛰쳐나간(배회) 직원이 스스로 자리로 돌아오기까지의 시간(초). 그 동안 기여 0 — 탭으로 잡으면 즉시 복귀해 손해 단축.
	UPROPERTY(EditAnywhere, Category = "Bolt")
	float BoltDuration = 8.0f;
};
