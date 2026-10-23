#pragma once

#include "CoreMinimal.h"
#include "Enum/FunnyPartSlot.h"
#include "Data/EmployeeTypes.h"

// 얼굴 판 MID 의 개인 차이 값 — 같은 그림이라도 눈 크기·높이가 조금씩 달라야 복사한 얼굴로 안 보인다.
struct FFunnyFaceVariation
{
	float EyeScale = 1.f;
	float EyeOffsetY = 0.f;
	float MouthOffsetY = 0.f;
};

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

	// EyeScale 0.92~1.08 · EyeOffsetY −0.02~0.02 · MouthOffsetY −0.015~0.02 (얼굴 판 UV 단위).
	static FFunnyFaceVariation PickFaceVariation(int32 EmployeeID);

	// 깜빡임 간격 난수 시드 — 사람마다 박자가 달라야 사무실 전원이 같은 순간에 눈을 감지 않는다.
	static int32 FaceBlinkSeed(int32 EmployeeID);

	// 연속 직원 ID StyleCount개마다 모든 스타일을 한 번씩 배정한다. 빈 목록은 INDEX_NONE.
	static int32 PickFaceStyle(int32 EmployeeID, int32 StyleCount);
};
