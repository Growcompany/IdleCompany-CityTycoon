// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonInputBaseTypes.h"
#include "CGCommonUIInputData.generated.h"

/**
 * CommonUI 의 기본 클릭/백 액션 출처.
 *
 * 이 클래스가 `[/Script/CommonInput.CommonInputSettings] InputData` 에 꽂혀 있어야
 * `UCommonActivatableWidget::bIsBackHandler` 가 실제로 동작한다. 경로는
 *   BindBackAction -> CommonInputSettings::GetDefaultBackAction -> InputData CDO -> DefaultBackAction(행 핸들)
 * 이고, 비어 있으면 행 이름이 None 이라 액션 라우터가 바인딩 생성을 거부하며
 * `LogUIActionRouter: Cannot create action binding ... no action provided` 만 남긴다.
 * 그 상태에서는 ESC·안드로이드 뒤로가기가 위젯에 **도달하지 않는다** ― 위젯 쪽 핸들러를 직접 호출하는
 * 테스트는 통과하므로 값 검증으로는 드러나지 않는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCGCommonUIInputData : public UCommonUIInputData
{
	GENERATED_BODY()

public:
	UCGCommonUIInputData();
};
