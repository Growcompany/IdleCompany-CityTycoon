#pragma once

#include "CoreMinimal.h"
#include "ChoiceArchetype.generated.h"

/**
 * 프로젝트 이벤트 선택지(FProjectEventChoice)의 "행동 아키타입".
 * 24개 EProjectEvent × 3선택지(72) 를 20개 공용 아이콘으로 묶기 위한 키.
 * 카드 위젯이 이 값으로 DT_EventChoiceIcon 에서 아이콘 텍스처를 조회한다.
 * 단일 진실 원천: DT_EventChoiceIcon (TableManagerSubsystem::GetEventChoiceIcon)
 */
UENUM(BlueprintType)
enum class EChoiceArchetype : uint8
{
	None       UMETA(DisplayName = "없음"),

	Fix        UMETA(DisplayName = "수리/긴급수정"),
	Reject     UMETA(DisplayName = "거절/기각"),
	Keep       UMETA(DisplayName = "유지/현상유지"),
	Adopt      UMETA(DisplayName = "채택/수용"),
	TeamRally  UMETA(DisplayName = "팀 총동원"),
	Inspire    UMETA(DisplayName = "영감/아이디어"),
	Negotiate  UMETA(DisplayName = "협상/중재"),
	Overtime   UMETA(DisplayName = "야근/강행"),
	Coffee     UMETA(DisplayName = "휴식/재충전"),
	Party      UMETA(DisplayName = "회식/자축"),
	Train      UMETA(DisplayName = "교육/멘토링"),
	Pivot      UMETA(DisplayName = "방향전환/우회"),
	Upgrade    UMETA(DisplayName = "장비 업그레이드"),
	Infra      UMETA(DisplayName = "인프라/서버"),
	Shield     UMETA(DisplayName = "보안/법무"),
	QAReview   UMETA(DisplayName = "검수/리뷰"),
	Plan       UMETA(DisplayName = "기획/전략"),
	Patch      UMETA(DisplayName = "부분수정/패치"),
	Push       UMETA(DisplayName = "강행돌파/투입"),
	Extend     UMETA(DisplayName = "기한 연장"),
	Fever      UMETA(DisplayName = "피버 발동"),

	Max        UMETA(Hidden)
};
