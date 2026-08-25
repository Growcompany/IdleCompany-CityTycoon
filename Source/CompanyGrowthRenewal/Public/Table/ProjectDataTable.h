#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Enum/CompanyType.h"
#include "Enum/ProjectStepType.h"
#include "ProjectDataTable.generated.h"

/**
 * FProjectData
 * 프로젝트 메타데이터 (스테이지 기획서에서 추출)
 * - 프로젝트명, 서브명, 드랍 아이템, 대표 이미지
 * - 요구점수/소요시간/직능가중치는 이 행이 SOT.
 */
USTRUCT(BlueprintType)
struct FProjectData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 회사 타입 (IT, Game, Semiconductor 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Project")
	ECompanyType CompanyType = ECompanyType::None;

	// 프로젝트 인덱스 (1-100)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Project")
	int32 ProjectIndex = 0;

	// 프로젝트명 (예: "첫 웹사이트", "반응형 웹")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Project")
	FText ProjectName;

	// 서브명 (예: "정적 웹페이지", "CSS 프레임워크")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Project")
	FText SubName;

	// 프로젝트 대표 이미지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	// 4단계 완료 시 드랍 아이템 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FName DropItemId;

	// 산업 내 variant 식별자 (금융/IT 만 사용. 단일 산업은 NAME_None)
	// DT_StepDisplayName 의 VariantKey 와 매칭하여 Step 표시명 분기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Project")
	FName VariantKey = NAME_None;

	// 축A — 장르 (GDS 발견형 착수). 빈값이면 미태깅.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Project")
	FName Genre = NAME_None;

	// 축B — 소재
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Project")
	FName Material = NAME_None;

	// 프로젝트 Step 타이머 시간 (초). 0 이면 매니저의 DefaultStepDuration fallback
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timer")
	float Duration = 0.0f;

	// 게임 런타임 소비처 0 (개발비 폐지 2026-08-15). Tools/Balance 시뮬레이터가 이 컬럼을 경제 입력으로
	// 읽고 reseed_project_curve.js 가 필수 컬럼으로 검증하므로, 시뮬레이터를 무료착수 모델로 옮길 때 함께 지운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Money")
	int32 DevelopmentCost = 0;

	// ────────────────────────────────────────
	// 단계별 요구 점수
	// ────────────────────────────────────────

	// 1단계 요구 점수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	int32 RequiredScore_Step1 = 100;

	// 2단계 요구 점수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	int32 RequiredScore_Step2 = 100;

	// 3단계 요구 점수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	int32 RequiredScore_Step3 = 100;

	// 4단계 요구 점수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	int32 RequiredScore_Step4 = 100;

	// ────────────────────────────────────────
	// 제작 직능 요구 가중치 (0~5) — 프로젝트마다 다름. 직능포인트 시스템 소비 예정.
	// ────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	int32 Weight_Plan = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	int32 Weight_Dev = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	int32 Weight_Graphics = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	int32 Weight_Sound = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	int32 Weight_Server = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	int32 Weight_QA = 0;

	// 피치 후보 자격 = 카드가 그릴 수 있는 최소 콘텐츠를 갖춘 행.
	// 행 자체가 기준이라 자가 치유된다 — CSV 를 저작하면 코드 변경 없이 후보에 합류한다.
	bool IsPitchReady() const
	{
		if (Genre.IsNone() || Material.IsNone()) { return false; }
		return (Weight_Plan + Weight_Dev + Weight_Graphics
		      + Weight_Sound + Weight_Server + Weight_QA) > 0;
	}

	// 단계 번호로 요구 점수 조회 (1~4)
	int32 GetRequiredScore(int32 StepNumber) const
	{
		switch (StepNumber)
		{
		case 1: return RequiredScore_Step1;
		case 2: return RequiredScore_Step2;
		case 3: return RequiredScore_Step3;
		case 4: return RequiredScore_Step4;
		default: return RequiredScore_Step1;
		}
	}

	FProjectData()
		: CompanyType(ECompanyType::None)
		, ProjectIndex(0)
		, ProjectName(FText::GetEmpty())
		, SubName(FText::GetEmpty())
		, DropItemId(NAME_None)
		, VariantKey(NAME_None)
		, Genre(NAME_None)
		, Material(NAME_None)
		, Duration(0.0f)
		, DevelopmentCost(0)
		, RequiredScore_Step1(100)
		, RequiredScore_Step2(100)
		, RequiredScore_Step3(100)
		, RequiredScore_Step4(100)
		, Weight_Plan(0)
		, Weight_Dev(0)
		, Weight_Graphics(0)
		, Weight_Sound(0)
		, Weight_Server(0)
		, Weight_QA(0)
	{}
};
