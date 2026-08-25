// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "CenterPivotWidgetStack.generated.h"

/**
 * 전이(Zoom) 스케일 기준점을 위젯 중앙으로 고정한 CommonActivatableWidgetStack.
 * UI_Base 의 3스택(Main/Prompt/Bottom)이 사용.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCenterPivotWidgetStack : public UCommonActivatableWidgetStack
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
