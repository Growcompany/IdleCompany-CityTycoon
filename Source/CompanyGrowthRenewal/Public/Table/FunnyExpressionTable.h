#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/FaceExpression.h"
#include "FunnyExpressionTable.generated.h"

/**
 * 표정 → 얼굴 판 그림 칸(T_FaceEyes/T_FaceMouth 4×4 아틀라스 순서 0~15). 메시를 갈지 않고 MID 칸만 바꿔 표정 전환이 공짜다.
 */
USTRUCT(BlueprintType)
struct FFunnyExpressionTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWorkerFaceExpression Expression = EWorkerFaceExpression::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "15"))
	int32 EyesCell = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "15"))
	int32 MouthCell = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBlush = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowBlink = false;
};
