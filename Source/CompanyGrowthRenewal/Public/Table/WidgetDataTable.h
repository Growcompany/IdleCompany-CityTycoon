// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/WidgetType.h"
#include "WidgetDataTable.generated.h"

/**
 *
 */
USTRUCT(BlueprintType)
struct FWidgetDataTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EWidgetType WidgetType;
	
	/** 생성할 위젯 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget")
	TSubclassOf<UUserWidget> WidgetClass;
};
