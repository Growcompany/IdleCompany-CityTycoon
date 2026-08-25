#pragma once

#include "CoreMinimal.h"
#include "BubbleType.generated.h"

/**
 * 건물 버블 타입
 * 낮은 값 = 높은 우선순위 (동시 조건 충족 시 가장 낮은 값만 표시)
 */
UENUM(BlueprintType)
enum class EBubbleType : uint8
{
	None            = 0  UMETA(DisplayName = "없음"),

	// MainMap 버블 (우선순위 순)
	VaultFull       = 1  UMETA(DisplayName = "금고 가득"),
	ReportPending   = 2  UMETA(DisplayName = "결산서 대기"),
	NoProject       = 4  UMETA(DisplayName = "진행중 프로젝트 없음"),

	// OfficeMap 버블 (향후 확장)
	EmptySeat       = 11 UMETA(DisplayName = "빈 좌석"),
};

/**
 * 직원(Officeworker) 머리 위 버블 타입
 * 우선순위: 작은 값 = 높은 우선순위
 * Kenney 이모트 30종을 카테고리별로 매핑 (UWorkerBubbleConfig)
 */
UENUM(BlueprintType)
enum class EWorkerBubbleType : uint8
{
	None        = 0  UMETA(DisplayName = "없음"),
	Critical    = 1  UMETA(DisplayName = "크리티컬"),       // exclamations, stars
	Boosted     = 2  UMETA(DisplayName = "능력 상승"),      // star, stars, faceHappy
	Happy       = 3  UMETA(DisplayName = "기분 좋음"),      // idea, hearts, laugh, music
	Angry       = 4  UMETA(DisplayName = "분노"),           // anger, faceAngry, exclamation
	EventEffect = 5  UMETA(DisplayName = "이벤트 영향"),    // alert, swirl, cloud
	Tired       = 6  UMETA(DisplayName = "피로"),           // sleep, sleeps, drop, drops
	Working     = 7  UMETA(DisplayName = "작업 중"),        // dots1/2/3, music
};
