#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/FunnyPartSlot.h"
#include "FunnyPartPoolTable.generated.h"

class USkeletalMesh;

/**
 * 개인 시드축 파츠 풀. EmployeeID 결정적 해시로 슬롯마다 한 행을 뽑는다.
 * 필드명이 Slot 이 아니라 PartSlot 인 이유: UWidget::Slot 셰도잉 혼동 방지(빌드 -WarningsAsErrors).
 */
USTRUCT(BlueprintType)
struct FFunnyPartPoolTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFunnyPartSlot PartSlot = EFunnyPartSlot::Body;

	// 헤어만 갈린다. 나머지는 Any 로 두면 성별 무관 후보가 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFunnyPartGender Gender = EFunnyPartGender::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USkeletalMesh> Mesh;

	// 등장 가중치. 0 이면 후보에서 제외(에셋을 지우지 않고 끄는 스위치).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Weight = 1;
};
