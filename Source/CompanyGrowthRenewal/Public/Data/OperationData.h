#pragma once

#include "CoreMinimal.h"
#include "Enum/OperationState.h"
#include "Enum/QualityGrade.h"
#include "OperationData.generated.h"

/**
 * 프로젝트 운영 데이터
 * 출시 후 운영 단계에서 프로젝트의 상태를 추적
 *
 * 주의: 창고/피로도는 Building별로 관리 (FOfficeSaveData 참조)
 */
USTRUCT(BlueprintType)
struct FOperationData
{
	GENERATED_BODY()

	// ========== 기본 정보 ==========

	// 프로젝트 ID (DataTable 연동)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Identity")
	int32 ProjectID = 0;

	// 프로젝트 번호 (몇 번째 프로젝트)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Identity")
	int32 ProjectNumber = 1;

	// 스테이지 번호 (프로젝트 내 스테이지)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Identity")
	int32 StageNumber = 1;

	// 프로젝트 이름
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Identity")
	FString ProjectName;

	// 소속 건물 ID
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Identity")
	int32 BuildingID = 0;

	// ========== 운영 시간 ==========

	// 총 운영 시간 (초)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Time")
	float TotalOperationTime = 0.0f;

	// 경과 시간 (초)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Time")
	float ElapsedTime = 0.0f;

	// 남은 시간 (초)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Time")
	float RemainingTime = 0.0f;

	// ========== 수익 ==========

	// 기본 초당 수익
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Revenue")
	float BaseRevenuePerSecond = 0.0f;

	// 실제 초당 수익 (보정 적용 후)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Revenue")
	float ActualRevenuePerSecond = 0.0f;

	// ========== 상태 ==========

	// 운영 상태
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|State")
	EOperationState State = EOperationState::None;

	// ========== 품질 ==========

	// 품질 점수 (0.5 ~ 2.0)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Quality")
	float QualityScore = 1.0f;

	// 품질 등급
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Quality")
	EQualityGrade QualityGrade = EQualityGrade::C;

	// ========== 보정 계수 ==========

	// 직원 배율 (배치된 직원 스탯 기반)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Multiplier")
	float EmployeeMultiplier = 1.0f;

	// 장식 배율 (오피스 장식 효과 기반)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Multiplier")
	float DecorationMultiplier = 1.0f;

	// 랜덤 배율 (이벤트, 버프 등)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Multiplier")
	float RandomMultiplier = 1.0f;

	// ========== 결산 데이터 (운영 중 누적) ==========

	// 총 누적 수익
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Report")
	float TotalRevenueEarned = 0.0f;

	// 직능별 결산 표시 — 착수 시 weight>0 활성 직능 순서(출시 스텝 제외). 병렬 배열, [i]=i번째 활성 직능.
	// 표시명은 담지 않는다 — 라벨 SOT 는 DT_DisciplineDisplay(GetDisciplineDisplayName) 하나뿐이다.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Report")
	TArray<float> DisciplineScores;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Report")
	TArray<float> DisciplineTargets;

	// 압축된 위 배열을 고정 축(6칸)에 되흩뿌리기 위한 원래 슬롯 번호 — 없으면 성긴 직능 구성에서 값과 라벨이 어긋난다
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Report")
	TArray<int32> DisciplineSlots;

	// ========== 산업 인격 곡선 노브 (DT_IndustryProfile, StartOperation 에서 캐시) ==========

	// 감쇠 반감기 비율(운영시간 대비). 기본 0.5. IT 0.85(긴 꼬리)/게임 0.28(빨리 식음).
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Industry")
	float HalfLifeFrac = 0.5f;

	// 운영 중 시장 변동성(진동 진폭). 0=잔잔, 금융 0.42(출렁). 진동은 평균 보존.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation|Industry")
	float Volatility = 0.0f;

	// ========== 헬퍼 함수 ==========

	/**
	 * 남은 시간을 문자열로 변환 (MM:SS 형식)
	 */
	FString GetRemainingTimeString() const
	{
		int32 TotalSeconds = FMath::FloorToInt(RemainingTime);
		int32 Minutes = TotalSeconds / 60;
		int32 Seconds = TotalSeconds % 60;
		return FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
	}

	/**
	 * 운영 진행률 (0.0 ~ 1.0)
	 */
	float GetOperationProgress() const
	{
		return TotalOperationTime > 0.0f ? FMath::Clamp(ElapsedTime / TotalOperationTime, 0.0f, 1.0f) : 0.0f;
	}

	/**
	 * 총 배율 계산
	 */
	float GetTotalMultiplier() const
	{
		return EmployeeMultiplier * DecorationMultiplier * RandomMultiplier;
	}

	FOperationData()
		: ProjectID(0)
		, ProjectNumber(1)
		, StageNumber(1)
		, ProjectName(TEXT(""))
		, BuildingID(0)
		, TotalOperationTime(0.0f)
		, ElapsedTime(0.0f)
		, RemainingTime(0.0f)
		, BaseRevenuePerSecond(0.0f)
		, ActualRevenuePerSecond(0.0f)
		, State(EOperationState::None)
		, QualityScore(1.0f)
		, QualityGrade(EQualityGrade::C)
		, EmployeeMultiplier(1.0f)
		, DecorationMultiplier(1.0f)
		, RandomMultiplier(1.0f)
		, TotalRevenueEarned(0.0f)
		, HalfLifeFrac(0.5f), Volatility(0.0f)
	{
	}
};
