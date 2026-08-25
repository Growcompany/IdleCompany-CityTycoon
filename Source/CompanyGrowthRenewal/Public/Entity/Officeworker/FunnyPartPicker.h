#pragma once

#include "CoreMinimal.h"
#include "Enum/FunnyPartSlot.h"
#include "Enum/FaceExpression.h"
#include "Data/EmployeeTypes.h"

/**
 * 개인 시드축 파츠 선택 — 월드/에셋 없이 도는 순수 로직이라 단위 테스트가 가능하다.
 * 액터(AFunnyOfficeworker)는 DT 조회와 메시 로드만 맡고 "무엇을 고를지"는 전부 여기서 정한다.
 */
struct COMPANYGROWTHRENEWAL_API FFunnyPartPicker
{
	// 축마다 다른 솔트를 써야 축끼리 상관이 사라져 조합 공간이 온전히 열린다.
	static uint32 Hash(int32 EmployeeID, uint32 Salt);

	static uint32 SaltFor(EFunnyPartSlot PartSlot);

	// 가중치 누적 픽. 빈 풀이거나 총 가중치가 0이면 INDEX_NONE.
	static int32 PickWeightedIndex(int32 EmployeeID, EFunnyPartSlot PartSlot, const TArray<int32>& Weights);

	static bool IsGenderMatch(EFunnyPartGender RowGender, EEmployeeGender Employee);

	// Neutral 만 개인 시드 눈썹을 살린다. 그 외 표정은 DT 눈썹이 덮는다.
	static bool ExpressionOverridesBrow(EWorkerFaceExpression Expression);
};
