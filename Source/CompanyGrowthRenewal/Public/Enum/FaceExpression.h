#pragma once

#include "CoreMinimal.h"
#include "FaceExpression.generated.h"

/**
 * 스틱 직원 얼굴 표정 — T_FaceAtlas 의 표정 인덱스와 1:1 (enum 값 = 아틀라스 열 오프셋).
 *
 * 머리 위 버블(EWorkerBubbleType)과 분리된 이유: 버블은 "무슨 버프를 받았나"를 알리는 UI 어휘고,
 * 얼굴은 "지금 이 사람이 어떤 상태인가"를 보여주는 연출 어휘다. 둘을 한 enum 으로 묶었더니
 * 개발 중 전원이 Working 으로 고정돼 표정이 아예 안 바뀌었고, 졸음/폭주처럼 얼굴에만 필요한
 * 표정을 추가할 자리도 없었다.
 *
 * ⚠ 값 추가/재배치 = 아틀라스 재생성 필수. 순서가 곧 팩 순서다
 *   (Tools/FaceAtlas/face_defs.json 의 expressions[].col 과 동일해야 하며, 생성기가 대조해 경고한다).
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

	Max			UMETA(Hidden)
};
