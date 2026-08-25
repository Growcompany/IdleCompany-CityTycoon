#include "Global/CGDevSettings.h"
#include "Engine/DataTable.h"

ECGStartMode UCGDevSettings::GetEffectiveStartMode()
{
#if UE_BUILD_SHIPPING
	// 출시 빌드 — dev 설정 무력화. 항상 정식 튜토리얼.
	return ECGStartMode::FullTutorial;
#else
	const UCGDevSettings* Settings = GetDefault<UCGDevSettings>();
	return Settings ? Settings->StartMode : ECGStartMode::FullTutorial;
#endif
}

bool UCGDevSettings::ShouldWipeSaveOnLaunch()
{
#if UE_BUILD_SHIPPING
	// 출시 빌드 — 설정과 무관하게 절대 지우지 않는다.
	return false;
#else
	const UCGDevSettings* Settings = GetDefault<UCGDevSettings>();
	return Settings ? Settings->bWipeSaveOnLaunch : false;
#endif
}

TArray<FString> UCGDevSettings::GetMissionIDOptions() const
{
	// 에디터 전용 — Project Settings 드롭다운이 열릴 때만 호출(런타임/쿠킹 무관). 그래서 LoadObject 문자열 경로 허용.
	TArray<FString> Options;
	Options.Add(TEXT(""));   // 미사용 (빈 값 = NAME_None = 기본 StartMode 동작)

	if (const UDataTable* MissionDT = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/Mission/DT_Mission.DT_Mission")))
	{
		for (const FName& RowName : MissionDT->GetRowNames())
		{
			Options.Add(RowName.ToString());
		}
	}
	return Options;
}

TArray<FString> UCGDevSettings::GetResourceScenarioOptions() const
{
	// 에디터 전용 — Project Settings 드롭다운이 열릴 때만 호출(런타임/쿠킹 무관). 그래서 LoadObject 문자열 경로 허용.
	TArray<FString> Options;
	Options.Add(TEXT(""));   // 미적용 (빈 값 = NAME_None = 자원 미적용)

	if (const UDataTable* ScenarioDT = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/_Dev/Table/DT_TestResourceScenarios.DT_TestResourceScenarios")))
	{
		for (const FName& RowName : ScenarioDT->GetRowNames())
		{
			Options.Add(RowName.ToString());
		}
	}
	return Options;
}

TArray<FString> UCGDevSettings::GetProgressPresetOptions() const
{
	// 에디터 전용 — Project Settings 드롭다운이 열릴 때만 호출(런타임/쿠킹 무관). 그래서 LoadObject 문자열 경로 허용.
	TArray<FString> Options;
	Options.Add(TEXT(""));   // 미적용 (빈 값 = NAME_None = 프리셋 미적용)

	if (const UDataTable* PresetDT = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/_Dev/Table/DT_DevProgressPreset.DT_DevProgressPreset")))
	{
		for (const FName& RowName : PresetDT->GetRowNames())
		{
			Options.Add(RowName.ToString());
		}
	}
	return Options;
}
