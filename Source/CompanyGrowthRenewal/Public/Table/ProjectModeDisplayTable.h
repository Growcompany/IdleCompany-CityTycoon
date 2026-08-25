#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ProjectMode.h"
#include "ProjectModeDisplayTable.generated.h"

/**
 * 프로젝트 진행 모드 (수주/자체개발) → Prefix + Suffix 표시명
 * Suffix 는 모드별로 다르게 변할 가능성이 있어 같이 DT 화 (현재는 모두 "개발 중").
 * 단일 진실 원천: DT_ProjectModeDisplay
 */
USTRUCT(BlueprintType)
struct FProjectModeDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	EProjectMode Mode = EProjectMode::None;

	// 프로젝트명 앞에 붙는 Prefix ("수주", "자체개발")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FString Prefix;

	// 프로젝트명 뒤에 붙는 Suffix ("개발 중")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FString Suffix;
};
