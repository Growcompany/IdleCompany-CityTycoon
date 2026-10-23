#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GameSaveData.h"
#include "Engine/GameInstance.h"
#include "Manager/SettingsManagerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaHandOpenSettingSnapshotTest,
	"CGR.Gacha.Reveal.Flow.HandOpenSettingSnapshot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaHandOpenSettingSnapshotTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	USettingsManagerSubsystem* SettingsMgr = NewObject<USettingsManagerSubsystem>(GameInstance);
	TestFalse(TEXT("새 설정 기본값은 자동 열기"), SettingsMgr->IsHandOpenBox());

	SettingsMgr->SetHandOpenBox(true);
	FGameSaveData EnabledSnapshot;
	SettingsMgr->CollectSaveData(EnabledSnapshot);
	TestTrue(TEXT("첫 슬롯 생성 전에도 손 긋기 설정이 저장 스냅샷에 포함됨"), EnabledSnapshot.GameSettings.bHandOpenBox);

	SettingsMgr->SetHandOpenBox(false);
	FGameSaveData DisabledSnapshot;
	SettingsMgr->CollectSaveData(DisabledSnapshot);
	TestFalse(TEXT("다시 끈 값도 저장 스냅샷에 반영됨"), DisabledSnapshot.GameSettings.bHandOpenBox);
	return true;
}

#endif
