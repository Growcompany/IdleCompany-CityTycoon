#pragma once

#include "CoreMinimal.h"
#include "FaceExpression.generated.h"

/**
 * 직원 얼굴 표정 — DT_FunnyExpression 의 행 키. AFunnyOfficeworker 가 이 값으로 얼굴 판의 눈·입 칸을 고른다.
 *
 * 머리 위 버블(EWorkerBubbleType)과 분리된 이유: 버블은 "무슨 버프를 받았나"를 알리는 UI 어휘고,
 * 얼굴은 "지금 이 사람이 어떤 상태인가"를 보여주는 연출 어휘다. 둘을 한 enum 으로 묶었더니
 * 개발 중 전원이 Working 으로 고정돼 표정이 아예 안 바뀌었고, 졸음/폭주처럼 얼굴에만 필요한
 * 표정을 추가할 자리도 없었다.
 *
 * ⚠ 값 추가 = DT_FunnyExpression 에 같은 이름의 행(눈·입 칸)도 등록. 행이 없으면 직전 얼굴이 남고 Warning 이 뜬다.
 */
UENUM(BlueprintType)
enum class EWorkerFaceExpression : uint8
{
	Neutral		UMETA(DisplayName = "기본"),
	Focused		UMETA(DisplayName = "집중"),
	Happy		UMETA(DisplayName = "웃음"),
	Wink		UMETA(DisplayName = "의욕(윙크)"),
	Angry		UMETA(DisplayName = "화남"),
	Tired		UMETA(DisplayName = "지침"),
	Surprised	UMETA(DisplayName = "놀람"),
	Worried		UMETA(DisplayName = "걱정"),
	Asleep		UMETA(DisplayName = "잠"),
	Drowsy		UMETA(DisplayName = "졸림"),
	Panic		UMETA(DisplayName = "허둥"),
	Determined	UMETA(DisplayName = "불끈"),
	StarEyes	UMETA(DisplayName = "별눈"),
	Sneaky		UMETA(DisplayName = "곁눈질"),
	Sulky		UMETA(DisplayName = "삐침"),
	Dizzy		UMETA(DisplayName = "어지러움"),

	Max			UMETA(Hidden)
};
