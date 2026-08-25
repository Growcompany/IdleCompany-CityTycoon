#include "Misc/AutomationTest.h"
#include "Utils/CF1CaptureSavePolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr TCHAR BaselineRunId[] = TEXT("1e53524a-f665-49f9-ab4d-b47c54d7bd84");
constexpr TCHAR BaselineSuffix[] = TEXT("CF1BaselineSafety1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR AbSuffix[] = TEXT("CF1AB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR SeedSha[] = TEXT("4a8dde3acf8f4d39bfba1e4ac5319a9954e91ce912d1eb33627a434fbd931b76");

FString MakeCommandLine(const TCHAR* Stage, const TCHAR* Suffix)
{
	return FString::Printf(
		TEXT("CompanyGrowthRenewal.exe -game -vulkan -FeatureLevelES31 ")
		TEXT("-CGRProbe=MobilePCLikeAB -CGRExperiment=CF1 -CGRAdopted=E2,L2 ")
		TEXT("-CGRStage=%s -CGRRunId=%s -CGRSeedSlotSha256=%s -saveddirsuffix=%s"),
		Stage,
		BaselineRunId,
		SeedSha,
		Suffix);
}

FString MakeSavedDir(const TCHAR* Suffix)
{
	return FString::Printf(TEXT("C:/Project/Saved_%s/"), Suffix);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCF1CaptureSavePolicyExactContractTest,
	"CGR.MainMapPreview.CF1.SavePolicy.ExactContractSuppressesWrites",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCF1CaptureSavePolicyExactContractTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("BASELINE_SAFETY의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("BASELINE_SAFETY"), BaselineSuffix),
			MakeSavedDir(BaselineSuffix)));
	TestTrue(
		TEXT("AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("AB"), AbSuffix),
			MakeSavedDir(AbSuffix)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCF1CaptureSavePolicyIncompleteContractTest,
	"CGR.MainMapPreview.CF1.SavePolicy.IncompleteOrAmbiguousContractFailsClosed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCF1CaptureSavePolicyIncompleteContractTest::RunTest(const FString& Parameters)
{
	const FString Valid = MakeCommandLine(TEXT("BASELINE_SAFETY"), BaselineSuffix);
	const FString SavedDir = MakeSavedDir(BaselineSuffix);

	TestFalse(
		TEXT("일반 에디터 실행은 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			TEXT("CompanyGrowthRenewal.exe -game"), SavedDir));
	TestFalse(
		TEXT("일부 CF1 플래그만 있는 실행은 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			TEXT("CompanyGrowthRenewal.exe -CGRProbe=MobilePCLikeAB -CGRExperiment=CF1"),
			SavedDir));
	TestFalse(
		TEXT("동일 키가 중복된 계약은 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid + TEXT(" -CGRStage=BASELINE_SAFETY"), SavedDir));
	TestFalse(
		TEXT("다른 probe는 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(TEXT("MobilePCLikeAB"), TEXT("OtherProbe")), SavedDir));
	TestFalse(
		TEXT("다른 experiment는 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(TEXT("CGRExperiment=CF1"), TEXT("CGRExperiment=L2")), SavedDir));
	TestFalse(
		TEXT("다른 adopted 조합은 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(TEXT("CGRAdopted=E2,L2"), TEXT("CGRAdopted=E2")), SavedDir));
	TestFalse(
		TEXT("비정규 UUID는 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(BaselineRunId, TEXT("1E53524A-F665-49F9-AB4D-B47C54D7BD84")),
			SavedDir));
	TestFalse(
		TEXT("짧은 UUID는 파싱 전에 거부한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(BaselineRunId, TEXT("short")), SavedDir));
	TestFalse(
		TEXT("비정규 seed SHA는 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(SeedSha, TEXT("not-a-sha")), SavedDir));

	FString UnicodeDigitSha = SeedSha;
	UnicodeDigitSha[0] = static_cast<TCHAR>(0x0664);
	TestFalse(
		TEXT("유니코드 숫자는 lowercase ASCII SHA로 인정하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(SeedSha, *UnicodeDigitSha), SavedDir));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCF1CaptureSavePolicySavedDirBindingTest,
	"CGR.MainMapPreview.CF1.SavePolicy.SavedDirectoryMustMatchRun",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCF1CaptureSavePolicySavedDirBindingTest::RunTest(const FString& Parameters)
{
	const FString Baseline = MakeCommandLine(TEXT("BASELINE_SAFETY"), BaselineSuffix);

	TestFalse(
		TEXT("root Saved 디렉터리는 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Baseline, TEXT("C:/Project/Saved/")));
	TestFalse(
		TEXT("다른 run의 격리 Saved 디렉터리는 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Baseline,
			TEXT("C:/Project/Saved_CF1BaselineSafetyaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/")));
	TestFalse(
		TEXT("stage와 suffix prefix가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("BASELINE_SAFETY"), AbSuffix),
			MakeSavedDir(AbSuffix)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
