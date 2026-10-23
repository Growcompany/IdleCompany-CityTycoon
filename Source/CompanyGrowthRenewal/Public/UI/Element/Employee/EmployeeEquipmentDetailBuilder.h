#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Employee/EquipmentDetailWindowWidget.h"

class UEmployeeEquipmentManagerSubsystem;
class UTableManagerSubsystem;
class UEmployeeManager;
class UEmployeeEquipmentItemObject;
class UGameInstance;
struct FEmployeeEquipmentDefinitionRow;
struct FEmployeeEquipmentInstance;
struct FEmployeeEquipmentEffectRow;

/**
 * 타일 하나의 표시 데이터.
 *
 * 워크스페이스와 직원창 칼럼이 각자 만들고 있었는데, 한쪽이 Icon 과 VisualGrade 를 빠뜨려
 * 같은 장비가 창에 따라 다르게 그려졌다(2026-09-26 캡처에서 발견). 한 군데로 모은다.
 */
namespace CGEquipmentEntry
{
	COMPANYGROWTHRENEWAL_API UEmployeeEquipmentItemObject* Make(
		UObject* Outer,
		const UTableManagerSubsystem& Tables,
		const FEmployeeEquipmentDefinitionRow& Definition,
		const FEmployeeEquipmentInstance* Instance);
}

/**
 * 선택한 장비의 상세 내용을 만든다.
 *
 * 계산은 전부 FEmployeeEquipmentEffectEvaluator 가 하고 여기서는 표시값만 조립한다.
 * 위젯에 의존하지 않으므로 워크스페이스(가챠·직원판까지 딸려 오는 큰 위젯)를 만들지 않고도
 * 직원창 인라인 목록과 보관함이 같은 표를 그릴 수 있다.
 */
namespace CGEquipmentDetail
{
	/** 효과 수치 서식 — 최대 소수 2자리. 두 창이 같은 표를 그리므로 한 군데서 만든다. */
	COMPANYGROWTHRENEWAL_API FString Number(float Value, int32 MinDigits = 0);

	/** 이전·이후를 같은 자릿수로 적기 위한 값 — 0 과 0.05 가 나란히 있으면 비교가 안 읽힌다. */
	COMPANYGROWTHRENEWAL_API int32 FractionDigits(float Value);

	/** 효과의 원본 혜택량을 UI에서 보여줄 변화량으로 바꾼다. 게임 계산에는 사용하지 않는다. */
	COMPANYGROWTHRENEWAL_API float DisplayValue(const FEmployeeEquipmentEffectRow& Effect, float RawValue);

	/** 변화량의 부호와 DT 단위를 포함한 공용 표시 문자열. */
	COMPANYGROWTHRENEWAL_API FString FormatEffectValue(const FEmployeeEquipmentEffectRow& Effect, float RawValue, int32 MinDigits = 0);

	/** InstanceId 가 유효하지 않거나 DT 행이 없으면 false — 이때 Out 은 건드리지 않는다. */
	COMPANYGROWTHRENEWAL_API bool BuildContent(
		const UEmployeeEquipmentManagerSubsystem& Equipment,
		const UTableManagerSubsystem& Tables,
		UEmployeeManager* Employees,
		int32 EmployeeId,
		const FGuid& InstanceId,
		FEquipmentDetailContent& Out);

	/** 다른 직원이 낀 장비를 가져오기 전 확인 창 ― 누구에게서 가져오는지와 그 직원 칸이 빈다는 것을 한 번 보여 준다.
	 *  확인을 누르면 OnConfirm. 창을 못 띄우면 바로 OnConfirm(막지 않는다). */
	COMPANYGROWTHRENEWAL_API void ShowTransferConfirm(UGameInstance& GameInstance, int32 FromEmployeeId, int32 ToEmployeeId,
		const FEmployeeEquipmentDefinitionRow& Definition, FSimpleDelegate OnConfirm);
}
