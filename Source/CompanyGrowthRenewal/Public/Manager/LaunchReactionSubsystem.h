#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/StageProgressData.h"
#include "Table/ReviewCommentTable.h"
#include "LaunchReactionSubsystem.generated.h"

class UTableManagerSubsystem;

// 레일에 한 장씩 흘러오는 반응 1건 (비평가 = 이름+점수, SNS = 닉). Band = 칩 밴드색 키(High/Mid/Low).
USTRUCT(BlueprintType)
struct FLaunchReaction
{
	GENERATED_BODY()

	UPROPERTY() FName Kind;      // Critic | Sns
	UPROPERTY() FText Source;    // 비평가 이름 | @닉
	UPROPERTY() int32 Score = 0; // 비평가만 1~10, SNS 0
	UPROPERTY() FText Comment;
	UPROPERTY() FName Band;      // High | Mid | Low
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLaunchReactionReady, int32, BuildingID, const FLaunchReaction&, Reaction);

/**
 * 출시 반응 드립 (specs/2026-08-22 §4). OfficeMap 전용 WorldSubsystem.
 * StartLaunch(RequestLaunchConfirm) 말미에 PrepareReactions 로 7건을 뽑아 두고, 리빌이 닫히면 BeginDrip 이
 * 타이머로 OnReactionReady 를 쏜다. 구독자(OfficeMainWidget)가 없거나 다른 건물이면 그 건은 소실(의도 — 저장 없음).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULaunchReactionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// 비평가 4(CriticScores 순) + SNS 3 을 DT_ReviewComment 선택 엔진으로 뽑아 BuildingID 큐에 담는다(기존 큐 교체)
	void PrepareReactions(int32 BuildingID, const FStageProgressData& StageData);
	// 리빌 닫힘 시 호출 — 스케줄(첫 건 +4s, 이후 8~12s) 시작. 큐가 없으면 no-op
	void BeginDrip(int32 BuildingID);
	// 운영 종료/레벨 이탈 — 타이머·큐 제거
	void CancelDrip(int32 BuildingID);

	UPROPERTY(BlueprintAssignable, Category = "Launch Reaction")
	FOnLaunchReactionReady OnReactionReady;

	// 토큰 값 + 활성 컨텍스트(Best/Worst/Discovery) — LaunchConfirmWidget::BuildReviewTokens 에서 이동(리뷰 페이지 폐기)
	static void BuildReviewTokens(const FStageProgressData& StageData, UTableManagerSubsystem* TableMgr,
		FReviewTokenValues& OutTokens, TSet<FName>& OutContexts);

private:
	struct FReactionQueue
	{
		TArray<FLaunchReaction> Items;
		TArray<float> Schedule;
		int32 Next = 0;
		FTimerHandle Timer;
	};
	TMap<int32, FReactionQueue> Queues;

	void ArmNext(int32 BuildingID);
	void FireNext(int32 BuildingID);
};
