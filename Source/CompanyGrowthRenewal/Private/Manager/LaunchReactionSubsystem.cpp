#include "Manager/LaunchReactionSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Utils/LaunchReactionMath.h"
#include "TimerManager.h"

bool ULaunchReactionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->GetMapName().Contains(TEXT("OfficeMap"));
	}
	return false;
}

void ULaunchReactionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		for (auto& Pair : Queues) { World->GetTimerManager().ClearTimer(Pair.Value.Timer); }
	}
	Queues.Empty();
	Super::Deinitialize();
}

void ULaunchReactionSubsystem::BuildReviewTokens(const FStageProgressData& StageData, UTableManagerSubsystem* TableMgr,
	FReviewTokenValues& OutTokens, TSet<FName>& OutContexts)
{
	OutTokens.ProjectName = StageData.ProjectName;
	OutTokens.GenreText = StageData.Genre.IsNone() ? FString() : StageData.Genre.ToString();
	if (TableMgr)
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(StageData.CompanyType, bFound);
		if (bFound) { OutTokens.IndustryText = Info.DisplayName.ToString(); }
	}
	// 최고/최저 = 활성 직능의 달성률 극값 (표시명은 StepName — DT 주도)
	float BestRate = -1.0f;
	float WorstRate = FLT_MAX;
	for (const FStepRoundData& S : StageData.Steps)
	{
		if (S.DisciplineSlot == INDEX_NONE || S.Weight <= 0) { continue; }
		const float Rate = S.GetAchievementRate();
		if (Rate > BestRate) { BestRate = Rate; OutTokens.BestStepName = S.StepName; }
		if (Rate < WorstRate) { WorstRate = Rate; OutTokens.WorstStepName = S.StepName; }
	}
	// 문구가 상황을 배신하지 않게 게이트 — 실제로 잘했을/모자랐을 때만 해당 토큰 행이 후보
	OutContexts.Reset();
	if (BestRate >= 0.9f) { OutContexts.Add(FName(TEXT("Best"))); }
	if (WorstRate < 0.6f && WorstRate < FLT_MAX) { OutContexts.Add(FName(TEXT("Worst"))); }
	if (StageData.bFirstDiscovery) { OutContexts.Add(FName(TEXT("Discovery"))); }
}

void ULaunchReactionSubsystem::PrepareReactions(int32 BuildingID, const FStageProgressData& StageData)
{
	CancelDrip(BuildingID);
	if (StageData.ReviewScore <= 0 || StageData.CriticScores.Num() < 4) { return; }

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchReaction] TableManager 없음 - 반응 생략"));
		return;
	}

	FReviewTokenValues Tokens;
	TSet<FName> Contexts;
	BuildReviewTokens(StageData, TableMgr, Tokens, Contexts);

	FReactionQueue Queue;

	// 비평가 4 — 밴드별 벌크 인출 후 pop (독립 추첨이면 같은 문구가 두 카드에 뜬다)
	const TArray<FText> CriticNames = TableMgr->GetCriticNames(StageData.CompanyType);
	TMap<FName, TArray<FText>> QuotePool;
	for (int32 i = 0; i < 4; ++i)
	{
		if (!CriticNames.IsValidIndex(i) || CriticNames[i].IsEmpty() || !StageData.CriticScores.IsValidIndex(i)) { continue; }
		const int32 Score = StageData.CriticScores[i];
		const FName Band = (Score >= 8) ? FName(TEXT("High")) : (Score >= 5) ? FName(TEXT("Mid")) : FName(TEXT("Low"));
		if (!QuotePool.Contains(Band))
		{
			TArray<FText> Nicks, Quotes;
			TableMgr->GetReviewComments(FName(TEXT("Critic")), StageData.CompanyType, Band, Contexts, 4, Tokens, Nicks, Quotes);
			QuotePool.Add(Band, MoveTemp(Quotes));
		}
		TArray<FText>* Pool = QuotePool.Find(Band);
		if (!Pool || Pool->Num() == 0) { continue; }
		FLaunchReaction R;
		R.Kind = FName(TEXT("Critic"));
		R.Source = CriticNames[i];
		R.Score = Score;
		R.Comment = Pool->Pop();
		R.Band = Band;
		Queue.Items.Add(MoveTemp(R));
	}

	// SNS 3 — 품질등급 밴드 풀(SA/B/CD), 칩 색은 High/Mid/Low 로 사상
	const EQualityGrade Grade = StageData.CalculateQualityGrade();
	const FName SnsBand = (Grade == EQualityGrade::S || Grade == EQualityGrade::A) ? FName(TEXT("SA"))
		: (Grade == EQualityGrade::B) ? FName(TEXT("B")) : FName(TEXT("CD"));
	const FName SnsColorBand = (SnsBand == FName(TEXT("SA"))) ? FName(TEXT("High")) : (SnsBand == FName(TEXT("B"))) ? FName(TEXT("Mid")) : FName(TEXT("Low"));
	TArray<FText> SnsNicks, SnsComments;
	TableMgr->GetReviewComments(FName(TEXT("Sns")), StageData.CompanyType, SnsBand, Contexts, 3, Tokens, SnsNicks, SnsComments);
	for (int32 i = 0; i < SnsNicks.Num() && i < SnsComments.Num(); ++i)
	{
		FLaunchReaction R;
		R.Kind = FName(TEXT("Sns"));
		R.Source = FText::FromString(FString::Printf(TEXT("@%s"), *SnsNicks[i].ToString()));
		R.Comment = SnsComments[i];
		R.Band = SnsColorBand;
		Queue.Items.Add(MoveTemp(R));
	}

	if (Queue.Items.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchReaction] 반응 0건 (DT_ReviewComment/CriticDisplay 산업 매핑 확인)"));
		return;
	}
	Queue.Schedule = LaunchReactionMath::BuildReactionSchedule(Queue.Items.Num(), StageData.ProjectID);
	Queues.Add(BuildingID, MoveTemp(Queue));
	UE_LOG(LogTemp, Log, TEXT("[LaunchReaction] Prepared %d reactions for building %d"), Queues[BuildingID].Items.Num(), BuildingID);
}

void ULaunchReactionSubsystem::BeginDrip(int32 BuildingID)
{
	if (!Queues.Contains(BuildingID)) { return; }
	Queues[BuildingID].Next = 0;
	ArmNext(BuildingID);
}

void ULaunchReactionSubsystem::CancelDrip(int32 BuildingID)
{
	if (FReactionQueue* Queue = Queues.Find(BuildingID))
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(Queue->Timer); }
		Queues.Remove(BuildingID);
	}
}

void ULaunchReactionSubsystem::ArmNext(int32 BuildingID)
{
	FReactionQueue* Queue = Queues.Find(BuildingID);
	UWorld* World = GetWorld();
	if (!Queue || !World) { return; }
	if (Queue->Next >= Queue->Items.Num()) { Queues.Remove(BuildingID); return; }
	// 스케줄은 드립 시작 기준 누적 시각 → 이전 건과의 차로 지연 환산
	const float Prev = (Queue->Next == 0) ? 0.f : Queue->Schedule[Queue->Next - 1];
	const float Delay = FMath::Max(0.05f, Queue->Schedule[Queue->Next] - Prev);
	World->GetTimerManager().SetTimer(Queue->Timer,
		FTimerDelegate::CreateUObject(this, &ULaunchReactionSubsystem::FireNext, BuildingID), Delay, false);
}

void ULaunchReactionSubsystem::FireNext(int32 BuildingID)
{
	FReactionQueue* Queue = Queues.Find(BuildingID);
	if (!Queue || Queue->Next >= Queue->Items.Num()) { return; }
	const FLaunchReaction Reaction = Queue->Items[Queue->Next++];
	OnReactionReady.Broadcast(BuildingID, Reaction);
	ArmNext(BuildingID);
}
