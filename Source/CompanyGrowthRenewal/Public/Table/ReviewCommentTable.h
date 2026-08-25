#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ReviewCommentTable.generated.h"

// 토큰 치환 값 묶음 — GetReviewComments 가 Comment 의 {토큰}을 이 값으로 치환
USTRUCT(BlueprintType)
struct FReviewTokenValues
{
	GENERATED_BODY()

	FString ProjectName;
	FString GenreText;
	FString BestStepName;
	FString WorstStepName;
	FString IndustryText;
};

// 출시 리뷰/실패 코멘트 통합 풀 (specs/2026-08-02 §2). CSV 컬럼명 = UPROPERTY명.
// 이모지 금지, 대시는 ―(U+2015). 코멘트는 캐릭터 발화라 게임톤 허용.
USTRUCT(BlueprintType)
struct FReviewCommentRow : public FTableRowBase
{
	GENERATED_BODY()

	// 소비처: "Critic"(비평가 한줄평) / "Sns"(SNS 반응) / "Tester"(실패 테스터 의견)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Review")
	FName Slot = NAME_None;

	// 빈값 = 전 산업 공용. "Game"/"IT"/"Finance"/"Automobile"/"Electronics"/"Semiconductor"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Review")
	FString Industry;

	// Critic="High/Mid/Low"(개별점수 8-10/5-7/1-4) · Sns="SA/B/CD"(조합등급) · Tester="Fail"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Review")
	FName Band = NAME_None;

	// 상황 게이트: "None"=무조건 / "Best"(최고분야 달성률≥90%) / "Worst"(최저<60%) / "Discovery"(첫 발견)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Review")
	FName Context = NAME_None;

	// Critic 슬롯은 무시(이름=DT_CriticDisplay 소유), Tester=사내 직함("QA팀장")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Review")
	FText Nick;

	// {프로젝트} {장르} {최고분야} {최저분야} {산업} 토큰 허용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Review")
	FText Comment;
};
