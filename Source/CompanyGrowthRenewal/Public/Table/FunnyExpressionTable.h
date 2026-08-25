#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/FaceExpression.h"
#include "FunnyExpressionTable.generated.h"

class USkeletalMesh;

/**
 * 표정 → 눈썹 메시. 팩 바디는 눈이 UV 에 구워져 있어 눈썹 각도만 감정 채널이다.
 * ⚠ Neutral 행의 EyebrowMesh 는 비워둘 것 — 채우면 개인 시드 눈썹 변주가 전원 같은 눈썹으로 죽는다.
 */
USTRUCT(BlueprintType)
struct FFunnyExpressionTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWorkerFaceExpression Expression = EWorkerFaceExpression::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USkeletalMesh> EyebrowMesh;
};
