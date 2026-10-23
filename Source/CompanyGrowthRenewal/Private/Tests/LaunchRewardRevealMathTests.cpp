#include "Misc/AutomationTest.h"
#include "Utils/LaunchRewardRevealMath.h"
#include "Data/StageProgressData.h"
#include "JsonObjectConverter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

// 아이템 보상의 수량은 Amount 가 아니라 ItemAmount 다 (ULaunchLootManagerSubsystem::RollAndGrant 와 동일 계약)
static FMissionReward MakeItem(EItemType Type, int32 Amount = 1)
{
	FMissionReward R;
	R.ItemType = Type;
	R.ItemAmount = Amount;
	return R;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRevealOrderTest,
	"CGR.Launch.RewardReveal.Order",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRevealOrderTest::RunTest(const FString& Parameters)
{
	TArray<FMissionReward> In;
	In.Add(MakeItem(EItemType::RecruitTicketPremium));
	In.Add(MakeItem(EItemType::RecruitTicketNormal));
	In.Add(MakeItem(EItemType::SkinTicketNormal));
	In.Add(MakeItem(EItemType::RecruitTicketNormal));
	In.Add(MakeItem(EItemType::BuildingTraitTicketAdvanced));

	const TArray<FMissionReward> Out = LaunchRewardRevealMath::BuildRevealOrder(In);
	TestEqual(TEXT("같은 ItemType 병합 → 4장"), Out.Num(), 4);
	TestEqual(TEXT("첫 장 = 일반 채용권"), Out[0].ItemType, EItemType::RecruitTicketNormal);
	TestEqual(TEXT("병합 수량 2"), Out[0].ItemAmount, 2);
	TestEqual(TEXT("마지막 = 스킨N(서열 6, 최고가 마지막)"), Out.Last().ItemType, EItemType::SkinTicketNormal);
	TestEqual(TEXT("서열: 프리미엄(4) < 특성A(5) < 스킨N(6)"), Out[1].ItemType, EItemType::RecruitTicketPremium);
	TestEqual(TEXT("서열 3번째 = 특성A"), Out[2].ItemType, EItemType::BuildingTraitTicketAdvanced);
	TestEqual(TEXT("서열 4번째 = 스킨N"), Out[3].ItemType, EItemType::SkinTicketNormal);
	TestEqual(TEXT("빈 입력 → 빈 출력"), LaunchRewardRevealMath::BuildRevealOrder({}).Num(), 0);

	// 자원 보상은 수량이 Amount 에 있다 — 병합이 두 수량 축을 각각 합치는지 확인
	FMissionReward Cash;
	Cash.ResourceType = EResourceType::Money;
	Cash.Amount = 5;
	const TArray<FMissionReward> CashOut = LaunchRewardRevealMath::BuildRevealOrder({Cash, Cash});
	TestEqual(TEXT("같은 자원 병합 → 1장"), CashOut.Num(), 1);
	TestEqual(TEXT("자원 수량 합산 10"), CashOut[0].Amount, (int64)10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRevealStampRunTest,
	"CGR.Launch.RewardReveal.StampRun",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRevealStampRunTest::RunTest(const FString& Parameters)
{
	const TArray<float> T = LaunchRewardRevealMath::StampRunTimes(5, 2.15f, 0.12f, 0.9f, 0.10f);
	TestEqual(TEXT("개수"), T.Num(), 5);
	TestTrue(TEXT("첫 장 = Start"), FMath::IsNearlyEqual(T[0], 2.15f));
	TestTrue(TEXT("2장 간격 0.12"), FMath::IsNearlyEqual(T[1] - T[0], 0.12f, 0.0001f));
	TestTrue(TEXT("3장 간격 0.108 (가속)"), FMath::IsNearlyEqual(T[2] - T[1], 0.108f, 0.0001f));
	TestTrue(TEXT("하한 0.10 적용 (4→5장: 0.12*0.9^3=0.087 → 0.10)"), FMath::IsNearlyEqual(T[4] - T[3], 0.10f, 0.0001f));
	TestEqual(TEXT("0장"), LaunchRewardRevealMath::StampRunTimes(0, 2.15f, 0.12f, 0.9f, 0.10f).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRLaunchLootPreviewSaveTest,
	"CGR.Launch.LootPreview.SaveRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRLaunchLootPreviewSaveTest::RunTest(const FString& Parameters)
{
	// 저장/재진입이 이미 공개한 보상을 잃으면 재추첨하거나 표시와 다른 보상을 지급하게 된다.
	const FString Fixture = TEXT(R"({"reviewScore":35,"earnedExperience":278.5,"bLaunchLootPrepared":true,"pendingLaunchLoot":[{"itemType":"RecruitTicketAdvanced","itemAmount":2},{"itemType":"RecruitTicketPremium","itemAmount":1}]})");
	FStageProgressData Before;
	if (!TestTrue(TEXT("준비된 출시 결과 입력"), FJsonObjectConverter::JsonObjectStringToUStruct(Fixture, &Before))) return false;

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	SaveArchive.ArIsSaveGame = true;
	SaveArchive.ArNoDelta = true;
	FStageProgressData::StaticStruct()->SerializeItem(SaveArchive, &Before, nullptr);

	FStageProgressData After;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	FStageProgressData::StaticStruct()->SerializeItem(LoadArchive, &After, nullptr);

	TSharedPtr<FJsonObject> Restored = FJsonObjectConverter::UStructToJsonObject(After);
	bool bPrepared = false;
	const bool bHasPrepared = Restored.IsValid() && Restored->TryGetBoolField(TEXT("bLaunchLootPrepared"), bPrepared);
	TestTrue(TEXT("로드한 결과도 추첨 완료 상태"), bHasPrepared && bPrepared);
	const TArray<TSharedPtr<FJsonValue>>* Rewards = nullptr;
	if (!TestTrue(TEXT("공개한 보상은 세이브에 남음"), Restored.IsValid() && Restored->TryGetArrayField(TEXT("pendingLaunchLoot"), Rewards))) return false;
	if (!TestEqual(TEXT("보상 종류 보존"), Rewards->Num(), 2)) return false;
	TestEqual(TEXT("고급 채용권 수량 보존"), (*Rewards)[0]->AsObject()->GetIntegerField(TEXT("itemAmount")), 2);
	TestEqual(TEXT("프리미엄 채용권 수량 보존"), (*Rewards)[1]->AsObject()->GetIntegerField(TEXT("itemAmount")), 1);
	TestEqual(TEXT("고급 채용권 타입 보존"), (*Rewards)[0]->AsObject()->GetStringField(TEXT("itemType")), FString(TEXT("RecruitTicketAdvanced")));
	TestEqual(TEXT("실제 적립된 개인 경험치 합계도 결과와 함께 저장"), After.EarnedExperience, 278.5f);

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("저장 복원 테스트 월드"), TestWorld)) return false;
	ON_SCOPE_EXIT { TestWorld->DestroyWorld(false); };
	UOfficeStageProgressManager* Stage = NewObject<UOfficeStageProgressManager>(TestWorld);
	After.ProjectID = After.ProjectNumber = 1;
	After.ProjectName = TEXT("Pending launch");
	After.ActiveMode = EProjectMode::InHouse;
	After.CompanyType = ECompanyType::Game;
	After.CurrentStep = 1;
	After.StepDuration = 15.f;
	FStepRoundData CompletedStep;
	CompletedStep.DisciplineSlot = 0;
	CompletedStep.AcquiredScore = CompletedStep.TargetScore;
	CompletedStep.bIsCompleted = true;
	After.Steps = { CompletedStep };
	Stage->SetProgressData(After);
	TestEqual(TEXT("공개한 결과는 출시 대기로 복원"), Stage->GetLifecycle(), EProjectLifecycle::LaunchPending);
	TestEqual(TEXT("복원된 창에 같은 전리품 2종"), Stage->GetProgressData().PendingLaunchLoot.Num(), 2);
	TestEqual(TEXT("복원된 창에 같은 개인 경험치 합계"), Stage->GetLastRoundEmployeeExp(), 278.5f);
	TestFalse(TEXT("복원은 개발 타이머를 재시작하지 않음"), Stage->IsTimerRunning());
	After.bLaunchLootPrepared = false;
	Stage->SetProgressData(After);
	TestEqual(TEXT("미완성 개발은 대기 상태로 복원"), Stage->GetLifecycle(), EProjectLifecycle::Idle);
	TestEqual(TEXT("취소된 결과의 전리품은 남지 않음"), Stage->GetProgressData().PendingLaunchLoot.Num(), 0);
	TestEqual(TEXT("취소된 결과의 경험치 표시는 남지 않음"), Stage->GetLastRoundEmployeeExp(), 0.0f);
	return true;
}

#endif
