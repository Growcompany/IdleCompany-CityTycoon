#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"

class UGameInstance;
class UBorder;
class UCommonTextBlock;

namespace CGQualityGradeMedal
{
	// 출시와 결산이 같은 DT 재질을 사용하며 미매핑 시 이전 등급 표시를 숨긴다.
	void Apply(UGameInstance* GI, ECompanyType CompanyType, EQualityGrade Grade,
		UBorder* LetterPlate, UCommonTextBlock* Text_QualityGrade);
}
