// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NotificationType.generated.h"

// 알림 타입 (색상 구분용)
UENUM(BlueprintType)
enum class ENotificationType : uint8
{
	Normal		UMETA(DisplayName = "Normal"),		// 흰색 (기본)
	Success		UMETA(DisplayName = "Success"),		// 초록색 (성공)
	Warning		UMETA(DisplayName = "Warning"),		// 주황색 (경고)
	Failed		UMETA(DisplayName = "Failed")		// 빨간색 (실패)
};
