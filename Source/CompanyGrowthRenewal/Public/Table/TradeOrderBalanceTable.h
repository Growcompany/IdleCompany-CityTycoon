// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TradeOrderBalanceTable.generated.h"

/**
 * FTradeOrderBalanceData
 * 무역 주문서 시스템 밸런스 노브 모음 (단일 row).
 *
 * Why DT 분리: 디자이너가 코드 빌드 없이 CSV reimport 만으로 패치 가능.
 * Row name 컨벤션: "Default" 1개. 추후 모드/난이도별로 row 분기 가능.
 */
USTRUCT(BlueprintType)
struct FTradeOrderBalanceData : public FTableRowBase
{
	GENERATED_BODY()

	// ────────── Tier 가중 랜덤 ──────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier",
		meta = (ClampMin = "1.0", ClampMax = "5.0",
		ToolTip = "해금된 티어 중 최신 티어가 더 자주 뽑히게 만드는 가중치 밑값. Weight[T]=Base^(T-1). 2.0=최신 티어 약 50% 점유, 1.5=분포 평탄, 3.0+=최신 압도적."))
	float TierWeightBase = 2.0f;

	// ────────── 보드 정원 (티어별 동시 보유 수) ──────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board",
		meta = (ToolTip = "보드에 동시에 떠 있을 수 있는 Normal 등급 주문 최대 개수. 만료/완료로 빠지면 자동 보충."))
	int32 MaxActive_Normal = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board",
		meta = (ToolTip = "보드에 동시에 떠 있을 수 있는 Urgent(긴급) 등급 주문 최대 개수. 짧은 만료시간 + 높은 보상."))
	int32 MaxActive_Urgent = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board",
		meta = (ToolTip = "보드에 동시에 떠 있을 수 있는 VIP 등급 주문 최대 개수. 다이아몬드 보너스가 붙는 희귀 주문."))
	int32 MaxActive_VIP = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board",
		meta = (ToolTip = "플레이어가 동시에 '수락' 상태로 진행할 수 있는 주문 슬롯 수. 슬롯 가득 차면 신규 수락 불가."))
	int32 MaxAcceptedSlots = 3;

	// ────────── 스폰 주기 ──────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn",
		meta = (ClampMin = "1.0",
		ToolTip = "신규 주문 생성 체크 주기 (초). 매 주기마다 정원 미달인 티어를 채움. 작을수록 보드가 빠르게 채워짐. 디버깅 시 5~10초 권장."))
	float SpawnCheckIntervalSec = 60.0f;

	// ────────── Normal 티어 ──────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal",
		meta = (ToolTip = "Normal 주문 1건의 요구 수량 최소치 (개)."))
	int32 Normal_QtyMin = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal",
		meta = (ToolTip = "Normal 주문 1건의 요구 수량 최대치 (개). 실제 수량은 Min~Max 사이 균등 랜덤."))
	int32 Normal_QtyMax = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal",
		meta = (ToolTip = "Normal 주문 보상 배율 최소치. 기본 단가에 곱해짐 (1.3 = 30% 추가)."))
	float Normal_RewardMultMin = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal",
		meta = (ToolTip = "Normal 주문 보상 배율 최대치. 실제 배율은 Min~Max 사이 균등 랜덤."))
	float Normal_RewardMultMax = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal",
		meta = (ToolTip = "Normal 주문 만료까지 남은 시간 최소치 (시간). 게임 내 실시간 기준."))
	float Normal_DurationHoursMin = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal",
		meta = (ToolTip = "Normal 주문 만료까지 남은 시간 최대치 (시간)."))
	float Normal_DurationHoursMax = 12.0f;

	// ────────── Urgent 티어 ──────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urgent",
		meta = (ToolTip = "Urgent 주문 1건의 요구 수량 최소치 (개). Normal 보다 큼."))
	int32 Urgent_QtyMin = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urgent",
		meta = (ToolTip = "Urgent 주문 1건의 요구 수량 최대치 (개)."))
	int32 Urgent_QtyMax = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urgent",
		meta = (ToolTip = "Urgent 주문 보상 배율 최소치. Normal 보다 높게 설정 권장 (위험 보상 비례)."))
	float Urgent_RewardMultMin = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urgent",
		meta = (ToolTip = "Urgent 주문 보상 배율 최대치."))
	float Urgent_RewardMultMax = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urgent",
		meta = (ToolTip = "Urgent 주문 만료까지 남은 시간 최소치 (시간). Normal 보다 짧음 (긴급성)."))
	float Urgent_DurationHoursMin = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urgent",
		meta = (ToolTip = "Urgent 주문 만료까지 남은 시간 최대치 (시간)."))
	float Urgent_DurationHoursMax = 3.0f;

	// ────────── VIP 티어 ──────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VIP",
		meta = (ToolTip = "VIP 주문 1건의 요구 수량 최소치 (개). 적은 양 + 큰 보상 + 다이아 보너스."))
	int32 VIP_QtyMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VIP",
		meta = (ToolTip = "VIP 주문 1건의 요구 수량 최대치 (개)."))
	int32 VIP_QtyMax = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VIP",
		meta = (ToolTip = "VIP 주문 보상 배율 최소치. 가장 높은 단가 곱."))
	float VIP_RewardMultMin = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VIP",
		meta = (ToolTip = "VIP 주문 보상 배율 최대치."))
	float VIP_RewardMultMax = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VIP",
		meta = (ToolTip = "VIP 주문 만료까지 남은 시간 (시간). 단일 값 — Min/Max 분리 없이 고정."))
	float VIP_DurationHours = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VIP",
		meta = (ToolTip = "VIP 주문 완수 시 추가 다이아몬드 보상 최소치 (개)."))
	int32 VIP_DiamondMin = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VIP",
		meta = (ToolTip = "VIP 주문 완수 시 추가 다이아몬드 보상 최대치 (개). 실제 보상은 Min~Max 균등 랜덤."))
	int32 VIP_DiamondMax = 80;

	// ────────── 콤보 보너스 배율 ──────────
	// 1회 콤보는 x1.0 코드 폴백 (튜닝 의미 낮음). 2회+ 만 DT 노출.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo",
		meta = (ToolTip = "2연속 납품 시 보상 배율. 첫 콤보 보너스 — 너무 높으면 운빨, 너무 낮으면 콤보 의미 없음."))
	float ComboMul_2 = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo",
		meta = (ToolTip = "3연속 납품 시 보상 배율. 일반 플레이어가 도달 가능한 마일스톤."))
	float ComboMul_3 = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo",
		meta = (ToolTip = "4연속 납품 시 보상 배율."))
	float ComboMul_4 = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo",
		meta = (ToolTip = "5회 이상 연속 납품 시 보상 배율 (콤보 천장). 6회 이상도 동일 배율 적용."))
	float ComboMul_5Plus = 2.0f;
};
