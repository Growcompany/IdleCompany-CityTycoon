#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FunnyWardrobeTable.generated.h"

UENUM(BlueprintType)
enum class EFunnyWardrobeCategory : uint8
{
	Top		UMETA(DisplayName = "상의"),
	Bottom	UMETA(DisplayName = "하의"),
	Accent	UMETA(DisplayName = "포인트"),

	Max		UMETA(Hidden)
};

/**
 * 직원 평상복 팔레트 — 부서가 아니라 개인(EmployeeID)으로 뽑는다.
 * 부서 DT(FFunnyWorkerCosmeticTable)의 OuterwearTint/PantsTint 를 대체하며, 등급 연출(골드/림)은 그대로 그쪽이 소유한다.
 */
USTRUCT(BlueprintType)
struct FFunnyWardrobeTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFunnyWardrobeCategory Category = EFunnyWardrobeCategory::Top;

	// linear. M_FunnyToon 이 Brightness 1.25 를 곱해 Unlit 으로 내보내므로 0.80 을 넘는 채널은 흰색으로 날아간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor::White;

	// 0 이면 뽑히지 않는다 — 행을 지우면 재임포트 때 행 수가 바뀌어 막히므로 끄는 쪽을 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Weight = 1;
};
