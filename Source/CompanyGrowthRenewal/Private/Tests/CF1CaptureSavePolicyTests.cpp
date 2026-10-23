#include "Misc/AutomationTest.h"
#include "Utils/CF1CaptureSavePolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr TCHAR BaselineRunId[] = TEXT("1e53524a-f665-49f9-ab4d-b47c54d7bd84");
constexpr TCHAR BaselineSuffix[] = TEXT("CF1BaselineSafety1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR AbSuffix[] = TEXT("CF1AB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR LutScheduleAbSuffix[] = TEXT("CF1LutScheduleAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR DawnGainMidtonesAbSuffix[] = TEXT("CF1DawnGainMidtonesAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBloomThresholdAbSuffix[] = TEXT("CF1NightWindowBloomThresholdAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBloomIntensityAbSuffix[] = TEXT("CF1NightWindowBloomIntensityAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowEmissiveAbSuffix[] = TEXT("CF1NightWindowEmissiveAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowEmissiveFineAbSuffix[] = TEXT("CF1NightWindowEmissiveFineAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowEmissiveChromaAbSuffix[] = TEXT("CF1NightWindowEmissiveChromaAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowEmissivePaletteAbSuffix[] = TEXT("CF1NightWindowEmissivePaletteAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBloom5TintAbSuffix[] = TEXT("CF1NightWindowBloom5TintAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBloom4TintAbSuffix[] = TEXT("CF1NightWindowBloom4TintAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBloomIntensityLitSourceAbSuffix[] = TEXT("CF1NightWindowBloomIntensityLitSourceAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBakedHaloStrengthAbSuffix[] = TEXT("CF1NightWindowBakedHaloStrengthAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBakedHaloEnergyBalanceAbSuffix[] = TEXT("CF1NightWindowBakedHaloEnergyBalanceAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR NightWindowBakedHaloHeadroomBandAbSuffix[] = TEXT("CF1NightWindowBakedHaloHeadroomBandAB1e53524af66549f9ab4db47c54d7bd84");
constexpr TCHAR RoadNightSpineRunId[] = TEXT("0123456789abcdef0123456789abcdef");
constexpr TCHAR RoadNightSpineSuffix[] = TEXT("RoadNightSpine0123456789abcdef0123456789abcdef");
constexpr TCHAR OfficeExteriorCaptureRunId[] = TEXT("abcdef0123456789abcdef0123456789");
constexpr TCHAR OfficeExteriorCaptureSuffix[] = TEXT("OfficeExteriorCaptureabcdef0123456789abcdef0123456789");
constexpr TCHAR ProductionCaptureRunId[] = TEXT("fedcba9876543210fedcba9876543210");
constexpr TCHAR ProductionCaptureSuffix[] = TEXT("MainMapVisualProductionCapturefedcba9876543210fedcba9876543210");
constexpr TCHAR ProductionManifestSha256[] = TEXT("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
constexpr TCHAR ProductionManifestBase64[] = TEXT("QzovUHJvamVjdC9tYW5pZmVzdC5qc29u");
constexpr TCHAR ProductionCaptureOutputBase64[] = TEXT("QzovUHJvamVjdC9DYXB0dXJl");
constexpr TCHAR SeedSha[] = TEXT("4a8dde3acf8f4d39bfba1e4ac5319a9954e91ce912d1eb33627a434fbd931b76");
constexpr TCHAR AbMutationSignature[] = TEXT("sha256:023bfdb101eb76f61041a14e95b3b5ab446ae80afad0acce500aea843e5df8ef");
constexpr TCHAR LutScheduleMutationSignature[] = TEXT("sha256:bc11df6c96dd567836c3c5165da01f0479306338d55f69550cbcd2d5c4267c06");
constexpr TCHAR DawnGainMutationSignature[] = TEXT("sha256:c6f8617456d25e0cd5976e50334fee3aacf7fc3cb0aeef5206dc2325f48af4d5");
constexpr TCHAR NightWindowBloomThresholdMutationSignature[] = TEXT("sha256:65eca5c7fec394446e645188c5ee7c3faaf0dd47016a6951e640077e41fd9cad");
constexpr TCHAR NightWindowBloomIntensityMutationSignature[] = TEXT("sha256:1bb8186dfaba1dc9337bf0d93dd295305e5e314c4d32fc33cfee56e29bf95aa5");
constexpr TCHAR NightWindowEmissiveMutationSignature[] = TEXT("sha256:edaed16d1863468473c4af61b9f0a8a4269ce029185789c7713ccbc627a217b0");
constexpr TCHAR NightWindowEmissiveFineMutationSignature[] = TEXT("sha256:cec082d28be2d0192664b9843c95eea361e6f31c6ad0dad0c86b062a72db4618");
constexpr TCHAR NightWindowEmissiveChromaMutationSignature[] = TEXT("sha256:9449cbf0594371f6e0ae26d373e231fac57545a1eb3e2e9ffe5f7de7249caeeb");
constexpr TCHAR NightWindowEmissivePaletteMutationSignature[] = TEXT("sha256:5f29d73a96a1f58513f48528362d496158d55aab234731a562c646b3f3d609b9");
constexpr TCHAR NightWindowBloom5TintMutationSignature[] = TEXT("sha256:66fe1abdbfb78669bad7443d5ecc2c5ffb15cdd8950792de060d60b0a02adc22");
constexpr TCHAR NightWindowBloom4TintMutationSignature[] = TEXT("sha256:a54ef08516b5321e7904429e04204ba20d9ca717ae4e62a782ccf5aaa85d88c6");
constexpr TCHAR NightWindowBloomIntensityLitSourceMutationSignature[] = TEXT("sha256:9cc4740b0f1a1d057cf46ad4e044c2a9e89159ff8340a7eb43839af743aaef78");
constexpr TCHAR NightWindowBakedHaloStrengthMutationSignature[] = TEXT("sha256:414a80c3abbc84192dc2bbbfc82b0fed3284f25cae4a357e5b830f3bae51e134");
constexpr TCHAR NightWindowBakedHaloEnergyBalanceMutationSignature[] = TEXT("sha256:ac9bea9975108422a69eaf93a6da86eb6bbdcace2f32a2ac625562c3dc708d16");
constexpr TCHAR NightWindowBakedHaloHeadroomBandMutationSignature[] = TEXT("sha256:bff28cbc8a0e613ba8a94b64608cfe5d3f5a22daa7f0a1d2806f59cbf68923c3");

const TCHAR* MutationSignatureForStage(const TCHAR* Stage)
{
	const FString StageValue(Stage);
	if (StageValue.Equals(TEXT("BASELINE_SAFETY"), ESearchCase::CaseSensitive)
		|| StageValue.Equals(TEXT("AB"), ESearchCase::CaseSensitive))
	{
		return AbMutationSignature;
	}
	if (StageValue.Equals(TEXT("LUT_SCHEDULE_AB"), ESearchCase::CaseSensitive))
	{
		return LutScheduleMutationSignature;
	}
	if (StageValue.Equals(TEXT("DAWN_GAIN_MIDTONES_AB"), ESearchCase::CaseSensitive))
	{
		return DawnGainMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBloomThresholdMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBloomIntensityMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowEmissiveMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_FINE_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowEmissiveFineMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_CHROMA_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowEmissiveChromaMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_PALETTE_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowEmissivePaletteMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBloom5TintMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBloom4TintMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_LIT_SOURCE_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBloomIntensityLitSourceMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBakedHaloStrengthMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BAKED_HALO_ENERGY_BALANCE_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBakedHaloEnergyBalanceMutationSignature;
	}
	if (StageValue.Equals(TEXT("NIGHT_WINDOW_BAKED_HALO_HEADROOM_BAND_AB"), ESearchCase::CaseSensitive))
	{
		return NightWindowBakedHaloHeadroomBandMutationSignature;
	}
	return TEXT("invalid-stage-signature");
}

FString MakeCommandLine(
	const TCHAR* Stage,
	const TCHAR* Suffix,
	const TCHAR* SignatureOverride = nullptr)
{
	return FString::Printf(
		TEXT("CompanyGrowthRenewal.exe -game -vulkan -FeatureLevelES31 ")
		TEXT("-CGRProbe=MobilePCLikeAB -CGRExperiment=CF1 -CGRAdopted=E2,L2 ")
		TEXT("-CGRStage=%s -CGRRunId=%s -CGRSeedSlotSha256=%s ")
		TEXT("-CGRMutationSignature=%s -saveddirsuffix=%s"),
		Stage,
		BaselineRunId,
		SeedSha,
		SignatureOverride != nullptr ? SignatureOverride : MutationSignatureForStage(Stage),
		Suffix);
}

FString MakeSavedDir(const TCHAR* Suffix)
{
	return FString::Printf(TEXT("C:/Project/Saved_%s/"), Suffix);
}

FString MakeRoadNightSpineCommandLine(
	const TCHAR* RunId = RoadNightSpineRunId,
	const TCHAR* Suffix = RoadNightSpineSuffix)
{
	return FString::Printf(
		TEXT("CompanyGrowthRenewal.exe -game -RoadNightSpineRunId=%s -saveddirsuffix=%s"),
		RunId,
		Suffix);
}

FString MakeOfficeExteriorCaptureCommandLine(
	const TCHAR* RunId = OfficeExteriorCaptureRunId,
	const TCHAR* Suffix = OfficeExteriorCaptureSuffix)
{
	return FString::Printf(
		TEXT("CompanyGrowthRenewal.exe -game -OfficeExteriorCaptureRunId=%s -saveddirsuffix=%s"),
		RunId,
		Suffix);
}

FString MakeMainMapVisualProductionCaptureCommandLine(
	const TCHAR* RunId = ProductionCaptureRunId,
	const TCHAR* ManifestSha256 = ProductionManifestSha256,
	const TCHAR* Suffix = ProductionCaptureSuffix,
	const TCHAR* ManifestBase64 = ProductionManifestBase64,
	const TCHAR* CaptureOutputBase64 = ProductionCaptureOutputBase64)
{
	return FString::Printf(
		TEXT("CompanyGrowthRenewal.exe /Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity ")
		TEXT("-game -vulkan -FeatureLevelES31 -DeviceProfile=Android_High ")
		TEXT("-CGRProductionCapture=1 ")
		TEXT("-CGRProductionCaptureIsolationRunId=%s -CGRProductionManifestSha256=%s ")
		TEXT("-CGRProductionManifestBase64=%s -CGRProductionCaptureOutputBase64=%s ")
		TEXT("-saveddirsuffix=%s"),
		RunId,
		ManifestSha256,
		ManifestBase64,
		CaptureOutputBase64,
		Suffix);
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
	TestTrue(
		TEXT("LUT_SCHEDULE_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("LUT_SCHEDULE_AB"), LutScheduleAbSuffix),
			MakeSavedDir(LutScheduleAbSuffix)));
	TestTrue(
		TEXT("DAWN_GAIN_MIDTONES_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("DAWN_GAIN_MIDTONES_AB"), DawnGainMidtonesAbSuffix),
			MakeSavedDir(DawnGainMidtonesAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB"), NightWindowBloomThresholdAbSuffix),
			MakeSavedDir(NightWindowBloomThresholdAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_AB"), NightWindowBloomIntensityAbSuffix),
			MakeSavedDir(NightWindowBloomIntensityAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_EMISSIVE_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_AB"), NightWindowEmissiveAbSuffix),
			MakeSavedDir(NightWindowEmissiveAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_EMISSIVE_FINE_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_FINE_AB"), NightWindowEmissiveFineAbSuffix),
			MakeSavedDir(NightWindowEmissiveFineAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_EMISSIVE_CHROMA_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_CHROMA_AB"), NightWindowEmissiveChromaAbSuffix),
			MakeSavedDir(NightWindowEmissiveChromaAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_EMISSIVE_PALETTE_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_PALETTE_AB"), NightWindowEmissivePaletteAbSuffix),
			MakeSavedDir(NightWindowEmissivePaletteAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB"), NightWindowBloom5TintAbSuffix),
			MakeSavedDir(NightWindowBloom5TintAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB"), NightWindowBloom4TintAbSuffix),
			MakeSavedDir(NightWindowBloom4TintAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_LIT_SOURCE_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_LIT_SOURCE_AB"), NightWindowBloomIntensityLitSourceAbSuffix),
			MakeSavedDir(NightWindowBloomIntensityLitSourceAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"), NightWindowBakedHaloStrengthAbSuffix),
			MakeSavedDir(NightWindowBakedHaloStrengthAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BAKED_HALO_ENERGY_BALANCE_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BAKED_HALO_ENERGY_BALANCE_AB"), NightWindowBakedHaloEnergyBalanceAbSuffix),
			MakeSavedDir(NightWindowBakedHaloEnergyBalanceAbSuffix)));
	TestTrue(
		TEXT("NIGHT_WINDOW_BAKED_HALO_HEADROOM_BAND_AB의 완전한 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BAKED_HALO_HEADROOM_BAND_AB"), NightWindowBakedHaloHeadroomBandAbSuffix),
			MakeSavedDir(NightWindowBakedHaloHeadroomBandAbSuffix)));

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
		TEXT("mutation signature가 없으면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(
				*FString::Printf(TEXT(" -CGRMutationSignature=%s"), AbMutationSignature),
				TEXT("")),
			SavedDir));
	TestFalse(
		TEXT("mutation signature가 중복되면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid + FString::Printf(
				TEXT(" -CGRMutationSignature=%s"), AbMutationSignature),
			SavedDir));
	TestFalse(
		TEXT("mutation signature의 대소문자가 다르면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(
				AbMutationSignature,
				TEXT("SHA256:023BFDB101EB76F61041A14E95B3B5AB446AE80AFAD0ACCE500AEA843E5DF8EF")),
			SavedDir));
	TestFalse(
		TEXT("mutation signature에 후행 문자가 있으면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(
				AbMutationSignature,
				TEXT("sha256:023bfdb101eb76f61041a14e95b3b5ab446ae80afad0acce500aea843e5df8efx")),
			SavedDir));
	TestFalse(
		TEXT("malformed mutation signature는 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(AbMutationSignature, TEXT("sha256:not-a-sha")),
			SavedDir));
	TestFalse(
		TEXT("다른 stage의 mutation signature는 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(
				AbMutationSignature,
				NightWindowEmissivePaletteMutationSignature),
			SavedDir));
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
	TestFalse(
		TEXT("LUT schedule stage에 AB suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("LUT_SCHEDULE_AB"), AbSuffix),
			MakeSavedDir(AbSuffix)));
	TestFalse(
		TEXT("dawn gain midtones stage에 LUT schedule suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("DAWN_GAIN_MIDTONES_AB"), LutScheduleAbSuffix),
			MakeSavedDir(LutScheduleAbSuffix)));
	TestFalse(
		TEXT("night window bloom stage에 dawn gain suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB"), DawnGainMidtonesAbSuffix),
			MakeSavedDir(DawnGainMidtonesAbSuffix)));
	TestFalse(
		TEXT("night window bloom stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_bloom_threshold_ab"),
				NightWindowBloomThresholdAbSuffix,
				NightWindowBloomThresholdMutationSignature),
			MakeSavedDir(NightWindowBloomThresholdAbSuffix)));
	TestFalse(
		TEXT("night window bloom intensity stage에 threshold suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_AB"), NightWindowBloomThresholdAbSuffix),
			MakeSavedDir(NightWindowBloomThresholdAbSuffix)));
	TestFalse(
		TEXT("night window bloom intensity stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_bloom_intensity_ab"),
				NightWindowBloomIntensityAbSuffix,
				NightWindowBloomIntensityMutationSignature),
			MakeSavedDir(NightWindowBloomIntensityAbSuffix)));
	TestFalse(
		TEXT("night window emissive stage에 bloom intensity suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_AB"), NightWindowBloomIntensityAbSuffix),
			MakeSavedDir(NightWindowBloomIntensityAbSuffix)));
	TestFalse(
		TEXT("night window emissive stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_emissive_ab"),
				NightWindowEmissiveAbSuffix,
				NightWindowEmissiveMutationSignature),
			MakeSavedDir(NightWindowEmissiveAbSuffix)));
	TestFalse(
		TEXT("night window emissive fine stage에 기존 emissive suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_FINE_AB"), NightWindowEmissiveAbSuffix),
			MakeSavedDir(NightWindowEmissiveAbSuffix)));
	TestFalse(
		TEXT("night window emissive fine stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_emissive_fine_ab"),
				NightWindowEmissiveFineAbSuffix,
				NightWindowEmissiveFineMutationSignature),
			MakeSavedDir(NightWindowEmissiveFineAbSuffix)));
	TestFalse(
		TEXT("night window emissive chroma stage에 fine emissive suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_CHROMA_AB"), NightWindowEmissiveFineAbSuffix),
			MakeSavedDir(NightWindowEmissiveFineAbSuffix)));
	TestFalse(
		TEXT("night window emissive chroma stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_emissive_chroma_ab"),
				NightWindowEmissiveChromaAbSuffix,
				NightWindowEmissiveChromaMutationSignature),
			MakeSavedDir(NightWindowEmissiveChromaAbSuffix)));
	TestFalse(
		TEXT("night window emissive palette stage에 chroma suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_PALETTE_AB"), NightWindowEmissiveChromaAbSuffix),
			MakeSavedDir(NightWindowEmissiveChromaAbSuffix)));
	TestFalse(
		TEXT("night window emissive palette stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_emissive_palette_ab"),
				NightWindowEmissivePaletteAbSuffix,
				NightWindowEmissivePaletteMutationSignature),
			MakeSavedDir(NightWindowEmissivePaletteAbSuffix)));
	TestFalse(
		TEXT("Bloom5 tint stage에 palette suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB"), NightWindowEmissivePaletteAbSuffix),
			MakeSavedDir(NightWindowEmissivePaletteAbSuffix)));
	TestFalse(
		TEXT("Bloom5 tint stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_bloom5_tint_ab"),
				NightWindowBloom5TintAbSuffix,
				NightWindowBloom5TintMutationSignature),
			MakeSavedDir(NightWindowBloom5TintAbSuffix)));
	TestFalse(
		TEXT("Bloom5 tint stage에 다른 signature를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB"),
				NightWindowBloom5TintAbSuffix,
				NightWindowEmissivePaletteMutationSignature),
			MakeSavedDir(NightWindowBloom5TintAbSuffix)));
	TestFalse(
		TEXT("Bloom4 tint stage에 Bloom5 suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB"), NightWindowBloom5TintAbSuffix),
			MakeSavedDir(NightWindowBloom5TintAbSuffix)));
	TestFalse(
		TEXT("Bloom4 tint stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_bloom4_tint_ab"),
				NightWindowBloom4TintAbSuffix,
				NightWindowBloom4TintMutationSignature),
			MakeSavedDir(NightWindowBloom4TintAbSuffix)));
	TestFalse(
		TEXT("Bloom4 tint stage에 다른 signature를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB"),
				NightWindowBloom4TintAbSuffix,
				NightWindowBloom5TintMutationSignature),
			MakeSavedDir(NightWindowBloom4TintAbSuffix)));
	TestFalse(
		TEXT("lit-source BloomIntensity stage에 Bloom4 suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_LIT_SOURCE_AB"), NightWindowBloom4TintAbSuffix),
			MakeSavedDir(NightWindowBloom4TintAbSuffix)));
	TestFalse(
		TEXT("lit-source BloomIntensity stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_bloom_intensity_lit_source_ab"),
				NightWindowBloomIntensityLitSourceAbSuffix,
				NightWindowBloomIntensityLitSourceMutationSignature),
			MakeSavedDir(NightWindowBloomIntensityLitSourceAbSuffix)));
	TestFalse(
		TEXT("lit-source BloomIntensity stage에 다른 signature를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_LIT_SOURCE_AB"),
				NightWindowBloomIntensityLitSourceAbSuffix,
				NightWindowBloom4TintMutationSignature),
			MakeSavedDir(NightWindowBloomIntensityLitSourceAbSuffix)));
	TestFalse(
		TEXT("baked halo stage에 lit-source suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"), NightWindowBloomIntensityLitSourceAbSuffix),
			MakeSavedDir(NightWindowBloomIntensityLitSourceAbSuffix)));
	TestFalse(
		TEXT("baked halo stage의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("night_window_baked_halo_strength_ab"),
				NightWindowBakedHaloStrengthAbSuffix,
				NightWindowBakedHaloStrengthMutationSignature),
			MakeSavedDir(NightWindowBakedHaloStrengthAbSuffix)));
	TestFalse(
		TEXT("baked halo stage에 다른 signature를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"),
				NightWindowBakedHaloStrengthAbSuffix,
				NightWindowBloomIntensityLitSourceMutationSignature),
			MakeSavedDir(NightWindowBakedHaloStrengthAbSuffix)));
	TestFalse(
		TEXT("H2 energy-balance stage에 H1 suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_ENERGY_BALANCE_AB"),
				NightWindowBakedHaloStrengthAbSuffix,
				NightWindowBakedHaloEnergyBalanceMutationSignature),
			MakeSavedDir(NightWindowBakedHaloStrengthAbSuffix)));
	TestFalse(
		TEXT("H2 energy-balance stage에 H1 signature를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_ENERGY_BALANCE_AB"),
				NightWindowBakedHaloEnergyBalanceAbSuffix,
				NightWindowBakedHaloStrengthMutationSignature),
			MakeSavedDir(NightWindowBakedHaloEnergyBalanceAbSuffix)));
	TestFalse(
		TEXT("H3 headroom-band stage에 H2 suffix를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_HEADROOM_BAND_AB"),
				NightWindowBakedHaloEnergyBalanceAbSuffix,
				NightWindowBakedHaloHeadroomBandMutationSignature),
			MakeSavedDir(NightWindowBakedHaloEnergyBalanceAbSuffix)));
	TestFalse(
		TEXT("H3 headroom-band stage에 H2 signature를 연결하면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_HEADROOM_BAND_AB"),
				NightWindowBakedHaloHeadroomBandAbSuffix,
				NightWindowBakedHaloEnergyBalanceMutationSignature),
			MakeSavedDir(NightWindowBakedHaloHeadroomBandAbSuffix)));

	const FString WrongCaseSuffix = FString(NightWindowBloomThresholdAbSuffix).Replace(
		TEXT("Bloom"), TEXT("bloom"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("night window bloom suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB"), *WrongCaseSuffix),
			MakeSavedDir(*WrongCaseSuffix)));
	TestFalse(
		TEXT("night window bloom Saved 디렉터리명의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB"), NightWindowBloomThresholdAbSuffix),
			MakeSavedDir(*WrongCaseSuffix)));

	const FString TrailingSuffix = FString(NightWindowBloomThresholdAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("night window bloom suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB"), *TrailingSuffix),
			MakeSavedDir(*TrailingSuffix)));

	const FString WrongCaseIntensitySuffix = FString(NightWindowBloomIntensityAbSuffix).Replace(
		TEXT("Intensity"), TEXT("intensity"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("night window bloom intensity suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_AB"), *WrongCaseIntensitySuffix),
			MakeSavedDir(*WrongCaseIntensitySuffix)));

	const FString TrailingIntensitySuffix = FString(NightWindowBloomIntensityAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("night window bloom intensity suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_AB"), *TrailingIntensitySuffix),
			MakeSavedDir(*TrailingIntensitySuffix)));

	const FString WrongCaseEmissiveSuffix = FString(NightWindowEmissiveAbSuffix).Replace(
		TEXT("Emissive"), TEXT("emissive"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("night window emissive suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_AB"), *WrongCaseEmissiveSuffix),
			MakeSavedDir(*WrongCaseEmissiveSuffix)));

	const FString TrailingEmissiveSuffix = FString(NightWindowEmissiveAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("night window emissive suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_AB"), *TrailingEmissiveSuffix),
			MakeSavedDir(*TrailingEmissiveSuffix)));

	const FString WrongCaseEmissiveFineSuffix = FString(NightWindowEmissiveFineAbSuffix).Replace(
		TEXT("Fine"), TEXT("fine"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("night window emissive fine suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_FINE_AB"), *WrongCaseEmissiveFineSuffix),
			MakeSavedDir(*WrongCaseEmissiveFineSuffix)));

	const FString TrailingEmissiveFineSuffix = FString(NightWindowEmissiveFineAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("night window emissive fine suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_FINE_AB"), *TrailingEmissiveFineSuffix),
			MakeSavedDir(*TrailingEmissiveFineSuffix)));

	const FString WrongCaseEmissiveChromaSuffix = FString(NightWindowEmissiveChromaAbSuffix).Replace(
		TEXT("Chroma"), TEXT("chroma"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("night window emissive chroma suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_CHROMA_AB"), *WrongCaseEmissiveChromaSuffix),
			MakeSavedDir(*WrongCaseEmissiveChromaSuffix)));

	const FString TrailingEmissiveChromaSuffix = FString(NightWindowEmissiveChromaAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("night window emissive chroma suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_CHROMA_AB"), *TrailingEmissiveChromaSuffix),
			MakeSavedDir(*TrailingEmissiveChromaSuffix)));

	const FString WrongCaseEmissivePaletteSuffix = FString(NightWindowEmissivePaletteAbSuffix).Replace(
		TEXT("Palette"), TEXT("palette"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("night window emissive palette suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_PALETTE_AB"), *WrongCaseEmissivePaletteSuffix),
			MakeSavedDir(*WrongCaseEmissivePaletteSuffix)));

	const FString TrailingEmissivePaletteSuffix = FString(NightWindowEmissivePaletteAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("night window emissive palette suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_EMISSIVE_PALETTE_AB"), *TrailingEmissivePaletteSuffix),
			MakeSavedDir(*TrailingEmissivePaletteSuffix)));

	const FString WrongCaseBloom5TintSuffix = FString(NightWindowBloom5TintAbSuffix).Replace(
		TEXT("Tint"), TEXT("tint"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("Bloom5 tint suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB"), *WrongCaseBloom5TintSuffix),
			MakeSavedDir(*WrongCaseBloom5TintSuffix)));

	const FString TrailingBloom5TintSuffix = FString(NightWindowBloom5TintAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("Bloom5 tint suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB"), *TrailingBloom5TintSuffix),
			MakeSavedDir(*TrailingBloom5TintSuffix)));

	const FString WrongCaseBloom4TintSuffix = FString(NightWindowBloom4TintAbSuffix).Replace(
		TEXT("Tint"), TEXT("tint"), ESearchCase::CaseSensitive);
	TestFalse(
		TEXT("Bloom4 tint suffix의 대소문자가 다르면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB"), *WrongCaseBloom4TintSuffix),
			MakeSavedDir(*WrongCaseBloom4TintSuffix)));

	const FString TrailingBloom4TintSuffix = FString(NightWindowBloom4TintAbSuffix) + TEXT("x");
	TestFalse(
		TEXT("Bloom4 tint suffix에 후행 문자가 있으면 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeCommandLine(TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB"), *TrailingBloom4TintSuffix),
			MakeSavedDir(*TrailingBloom4TintSuffix)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRoadNightSpineCaptureSavePolicyExactContractTest,
	"CGR.MainMapPreview.RoadNightSpine.SavePolicy.ExactContractSuppressesWrites",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRoadNightSpineCaptureSavePolicyExactContractTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("정확한 RoadNightSpine 격리 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeRoadNightSpineCommandLine(),
			MakeSavedDir(RoadNightSpineSuffix)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRoadNightSpineCaptureSavePolicyInvalidContractTest,
	"CGR.MainMapPreview.RoadNightSpine.SavePolicy.InvalidContractFailsClosed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRoadNightSpineCaptureSavePolicyInvalidContractTest::RunTest(const FString& Parameters)
{
	const FString Valid = MakeRoadNightSpineCommandLine();
	const FString SavedDir = MakeSavedDir(RoadNightSpineSuffix);

	TestFalse(
		TEXT("RoadNightSpine run id가 없으면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			FString::Printf(
				TEXT("CompanyGrowthRenewal.exe -game -saveddirsuffix=%s"),
				RoadNightSpineSuffix),
			SavedDir));
	TestFalse(
		TEXT("RoadNightSpine saved dir suffix가 없으면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			FString::Printf(
				TEXT("CompanyGrowthRenewal.exe -game -RoadNightSpineRunId=%s"),
				RoadNightSpineRunId),
			SavedDir));
	TestFalse(
		TEXT("RoadNightSpine run id가 중복되면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid + FString::Printf(TEXT(" -RoadNightSpineRunId=%s"), RoadNightSpineRunId),
			SavedDir));
	TestFalse(
		TEXT("RoadNightSpine saved dir suffix가 중복되면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid + FString::Printf(TEXT(" -saveddirsuffix=%s"), RoadNightSpineSuffix),
			SavedDir));
	TestFalse(
		TEXT("RoadNightSpine switch key의 대소문자가 다르면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid.Replace(
				TEXT("RoadNightSpineRunId"),
				TEXT("roadnightspinerunid"),
				ESearchCase::CaseSensitive),
			SavedDir));
	TestFalse(
		TEXT("RoadNightSpine run id에 대문자 hex가 있으면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeRoadNightSpineCommandLine(
				TEXT("0123456789ABCDEF0123456789ABCDEF"),
				TEXT("RoadNightSpine0123456789ABCDEF0123456789ABCDEF")),
			MakeSavedDir(TEXT("RoadNightSpine0123456789ABCDEF0123456789ABCDEF"))));
	TestFalse(
		TEXT("RoadNightSpine suffix prefix의 대소문자가 다르면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeRoadNightSpineCommandLine(
				RoadNightSpineRunId,
				TEXT("roadNightSpine0123456789abcdef0123456789abcdef")),
			MakeSavedDir(TEXT("roadNightSpine0123456789abcdef0123456789abcdef"))));
	TestFalse(
		TEXT("RoadNightSpine run id 길이가 31자면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeRoadNightSpineCommandLine(
				TEXT("0123456789abcdef0123456789abcde"),
				TEXT("RoadNightSpine0123456789abcdef0123456789abcde")),
			MakeSavedDir(TEXT("RoadNightSpine0123456789abcdef0123456789abcde"))));
	TestFalse(
		TEXT("RoadNightSpine run id 길이가 33자면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeRoadNightSpineCommandLine(
				TEXT("0123456789abcdef0123456789abcdef0"),
				TEXT("RoadNightSpine0123456789abcdef0123456789abcdef0")),
			MakeSavedDir(TEXT("RoadNightSpine0123456789abcdef0123456789abcdef0"))));
	TestFalse(
		TEXT("RoadNightSpine suffix run id가 다르면 저장을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			MakeRoadNightSpineCommandLine(
				RoadNightSpineRunId,
				TEXT("RoadNightSpineaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
			MakeSavedDir(TEXT("RoadNightSpineaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
	TestFalse(
		TEXT("RoadNightSpine 실행과 다른 Saved 디렉터리는 저장 억제 대상이 아니다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
			Valid,
			TEXT("C:/Project/Saved_RoadNightSpineaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorCaptureSavePolicyExactContractTest,
	"CGR.Office.Exterior.CaptureIsolation.ExactContractSuppressesWritesAndOnlineRequests",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeExteriorCaptureSavePolicyExactContractTest::RunTest(const FString& Parameters)
{
	const FString CommandLine = MakeOfficeExteriorCaptureCommandLine();
	const FString SavedDir = MakeSavedDir(OfficeExteriorCaptureSuffix);

	TestTrue(
		TEXT("정확한 Office exterior 격리 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(CommandLine, SavedDir));
	TestTrue(
		TEXT("정확한 Office exterior 격리 계약은 온라인 요청을 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(CommandLine, SavedDir));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorCaptureSavePolicyNearMissTest,
	"CGR.Office.Exterior.CaptureIsolation.NearMissFailsClosed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeExteriorCaptureSavePolicyNearMissTest::RunTest(const FString& Parameters)
{
	const FString Valid = MakeOfficeExteriorCaptureCommandLine();
	const FString SavedDir = MakeSavedDir(OfficeExteriorCaptureSuffix);

	const auto TestRejected = [this](
		const TCHAR* Description,
		const FString& CommandLine,
		const FString& ProjectSavedDir)
	{
		TestFalse(
			FString::Printf(TEXT("%s: 저장 쓰기"), Description),
			CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(CommandLine, ProjectSavedDir));
		TestFalse(
			FString::Printf(TEXT("%s: 온라인 요청"), Description),
			CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(CommandLine, ProjectSavedDir));
	};

	TestRejected(
		TEXT("Office exterior run id가 없으면 억제하지 않는다"),
		FString::Printf(
			TEXT("CompanyGrowthRenewal.exe -game -saveddirsuffix=%s"),
			OfficeExteriorCaptureSuffix),
		SavedDir);
	TestRejected(
		TEXT("Office exterior saved dir suffix가 없으면 억제하지 않는다"),
		FString::Printf(
			TEXT("CompanyGrowthRenewal.exe -game -OfficeExteriorCaptureRunId=%s"),
			OfficeExteriorCaptureRunId),
		SavedDir);
	TestRejected(
		TEXT("Office exterior run id가 중복되면 억제하지 않는다"),
		Valid + FString::Printf(
			TEXT(" -OfficeExteriorCaptureRunId=%s"), OfficeExteriorCaptureRunId),
		SavedDir);
	TestRejected(
		TEXT("Office exterior saved dir suffix가 중복되면 억제하지 않는다"),
		Valid + FString::Printf(
			TEXT(" -saveddirsuffix=%s"), OfficeExteriorCaptureSuffix),
		SavedDir);
	TestRejected(
		TEXT("Office exterior run id switch key의 대소문자가 다르면 억제하지 않는다"),
		Valid.Replace(
			TEXT("OfficeExteriorCaptureRunId"),
			TEXT("officeexteriorcapturerunid"),
			ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("Office exterior suffix switch key의 대소문자가 다르면 억제하지 않는다"),
		Valid.Replace(
			TEXT("saveddirsuffix"),
			TEXT("SavedDirSuffix"),
			ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("Office exterior run id에 대문자 hex가 있으면 억제하지 않는다"),
		MakeOfficeExteriorCaptureCommandLine(
			TEXT("ABCDEF0123456789ABCDEF0123456789"),
			TEXT("OfficeExteriorCaptureABCDEF0123456789ABCDEF0123456789")),
		MakeSavedDir(TEXT("OfficeExteriorCaptureABCDEF0123456789ABCDEF0123456789")));
	TestRejected(
		TEXT("Office exterior run id 길이가 31자면 억제하지 않는다"),
		MakeOfficeExteriorCaptureCommandLine(
			TEXT("abcdef0123456789abcdef012345678"),
			TEXT("OfficeExteriorCaptureabcdef0123456789abcdef012345678")),
		MakeSavedDir(TEXT("OfficeExteriorCaptureabcdef0123456789abcdef012345678")));
	TestRejected(
		TEXT("Office exterior run id 길이가 33자면 억제하지 않는다"),
		MakeOfficeExteriorCaptureCommandLine(
			TEXT("abcdef0123456789abcdef01234567890"),
			TEXT("OfficeExteriorCaptureabcdef0123456789abcdef01234567890")),
		MakeSavedDir(TEXT("OfficeExteriorCaptureabcdef0123456789abcdef01234567890")));
	TestRejected(
		TEXT("Office exterior suffix run id가 다르면 억제하지 않는다"),
		MakeOfficeExteriorCaptureCommandLine(
			OfficeExteriorCaptureRunId,
			TEXT("OfficeExteriorCaptureaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
		MakeSavedDir(TEXT("OfficeExteriorCaptureaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
	TestRejected(
		TEXT("Office exterior 실행과 다른 Saved 디렉터리는 억제하지 않는다"),
		Valid,
		TEXT("C:/Project/Saved_OfficeExteriorCaptureaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMainMapVisualProductionCaptureIsolationExactContractTest,
	"CGR.MainMapPreview.ProductionCaptureIsolation.ExactContractSuppressesWritesAndOnlineRequests",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMainMapVisualProductionCaptureIsolationExactContractTest::RunTest(
	const FString& Parameters)
{
	const FString CommandLine = MakeMainMapVisualProductionCaptureCommandLine();
	const FString SavedDir = MakeSavedDir(ProductionCaptureSuffix);

	TestTrue(
		TEXT("정확한 MainMap production 캡처 계약은 저장 쓰기를 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(CommandLine, SavedDir));
	TestTrue(
		TEXT("정확한 MainMap production 캡처 계약은 온라인 요청을 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(CommandLine, SavedDir));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMainMapVisualProductionCaptureIsolationNearMissTest,
	"CGR.MainMapPreview.ProductionCaptureIsolation.NearMissFailsClosed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMainMapVisualProductionCaptureIsolationNearMissTest::RunTest(
	const FString& Parameters)
{
	const FString Valid = MakeMainMapVisualProductionCaptureCommandLine();
	const FString SavedDir = MakeSavedDir(ProductionCaptureSuffix);
	const auto TestRejected = [this](
		const FString& Description,
		const FString& CandidateCommandLine,
		const FString& CandidateSavedDir)
	{
		TestFalse(
			Description + TEXT(": 저장 쓰기"),
			CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
				CandidateCommandLine,
				CandidateSavedDir));
		TestFalse(
			Description + TEXT(": 온라인 요청"),
			CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
				CandidateCommandLine,
				CandidateSavedDir));
	};

	TestRejected(
		TEXT("일반 MainMap 게임 실행은 억제하지 않는다"),
		TEXT("CompanyGrowthRenewal.exe /Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity ")
		TEXT("-game -vulkan -FeatureLevelES31"),
		TEXT("C:/Project/Saved/"));

	const TArray<FString> RequiredValuedSwitches = {
		TEXT("-DeviceProfile=Android_High"),
		TEXT("-CGRProductionCapture=1"),
		FString::Printf(
			TEXT("-CGRProductionCaptureIsolationRunId=%s"),
			ProductionCaptureRunId),
		FString::Printf(
			TEXT("-CGRProductionManifestSha256=%s"),
			ProductionManifestSha256),
		FString::Printf(
			TEXT("-CGRProductionManifestBase64=%s"),
			ProductionManifestBase64),
		FString::Printf(
			TEXT("-CGRProductionCaptureOutputBase64=%s"),
			ProductionCaptureOutputBase64),
		FString::Printf(TEXT("-saveddirsuffix=%s"), ProductionCaptureSuffix)
	};
	for (const FString& RequiredSwitch : RequiredValuedSwitches)
	{
		const FString SwitchWithLeadingSpace = FString(TEXT(" ")) + RequiredSwitch;
		TestRejected(
			FString::Printf(TEXT("%s 스위치가 없으면 억제하지 않는다"), *RequiredSwitch),
			Valid.Replace(
				*SwitchWithLeadingSpace,
				TEXT(""),
				ESearchCase::CaseSensitive),
			SavedDir);
		TestRejected(
			FString::Printf(TEXT("%s 스위치가 중복되면 억제하지 않는다"), *RequiredSwitch),
			Valid + SwitchWithLeadingSpace,
			SavedDir);
	}

	const TArray<TPair<FString, FString>> WrongCaseValuedKeys = {
		TPair<FString, FString>(TEXT("DeviceProfile"), TEXT("deviceProfile")),
		TPair<FString, FString>(TEXT("CGRProductionCapture"), TEXT("cgrProductionCapture")),
		TPair<FString, FString>(
			TEXT("CGRProductionCaptureIsolationRunId"),
			TEXT("cgrProductionCaptureIsolationRunId")),
		TPair<FString, FString>(
			TEXT("CGRProductionManifestSha256"),
			TEXT("cgrProductionManifestSha256")),
		TPair<FString, FString>(
			TEXT("CGRProductionManifestBase64"),
			TEXT("cgrProductionManifestBase64")),
		TPair<FString, FString>(
			TEXT("CGRProductionCaptureOutputBase64"),
			TEXT("cgrProductionCaptureOutputBase64")),
		TPair<FString, FString>(TEXT("saveddirsuffix"), TEXT("SavedDirSuffix"))
	};
	for (const TPair<FString, FString>& KeyCase : WrongCaseValuedKeys)
	{
		TestRejected(
			FString::Printf(TEXT("%s key의 대소문자가 다르면 억제하지 않는다"), *KeyCase.Key),
			Valid.Replace(
				*KeyCase.Key,
				*KeyCase.Value,
				ESearchCase::CaseSensitive),
			SavedDir);
	}

	const TArray<TPair<FString, FString>> RequiredBareSwitches = {
		TPair<FString, FString>(TEXT("-game"), TEXT("-Game")),
		TPair<FString, FString>(TEXT("-vulkan"), TEXT("-Vulkan")),
		TPair<FString, FString>(TEXT("-FeatureLevelES31"), TEXT("-featureleveles31"))
	};
	for (const TPair<FString, FString>& BareSwitch : RequiredBareSwitches)
	{
		const FString SwitchWithLeadingSpace = FString(TEXT(" ")) + BareSwitch.Key;
		TestRejected(
			FString::Printf(TEXT("%s bare switch가 없으면 억제하지 않는다"), *BareSwitch.Key),
			Valid.Replace(
				*SwitchWithLeadingSpace,
				TEXT(""),
				ESearchCase::CaseSensitive),
			SavedDir);
		TestRejected(
			FString::Printf(TEXT("%s bare switch가 중복되면 억제하지 않는다"), *BareSwitch.Key),
			Valid + SwitchWithLeadingSpace,
			SavedDir);
		TestRejected(
			FString::Printf(TEXT("%s bare switch의 대소문자가 다르면 억제하지 않는다"), *BareSwitch.Key),
			Valid.Replace(
				*BareSwitch.Key,
				*BareSwitch.Value,
				ESearchCase::CaseSensitive),
			SavedDir);
	}

	TestRejected(
		TEXT("game switch에 값이 붙으면 억제하지 않는다"),
		Valid.Replace(TEXT("-game"), TEXT("-game=1"), ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("DeviceProfile 값의 대소문자가 다르면 억제하지 않는다"),
		Valid.Replace(
			TEXT("DeviceProfile=Android_High"),
			TEXT("DeviceProfile=android_high"),
			ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("DeviceProfile이 Android_High가 아니면 억제하지 않는다"),
		Valid.Replace(
			TEXT("DeviceProfile=Android_High"),
			TEXT("DeviceProfile=Android_Default"),
			ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("production capture 값이 1이 아니면 억제하지 않는다"),
		Valid.Replace(
			TEXT("CGRProductionCapture=1"),
			TEXT("CGRProductionCapture=01"),
			ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("production run id에 대문자 hex가 있으면 억제하지 않는다"),
		MakeMainMapVisualProductionCaptureCommandLine(
			TEXT("FEDCBA9876543210FEDCBA9876543210"),
			ProductionManifestSha256,
			TEXT("MainMapVisualProductionCaptureFEDCBA9876543210FEDCBA9876543210")),
		MakeSavedDir(
			TEXT("MainMapVisualProductionCaptureFEDCBA9876543210FEDCBA9876543210")));
	TestRejected(
		TEXT("production run id 길이가 31자면 억제하지 않는다"),
		MakeMainMapVisualProductionCaptureCommandLine(
			TEXT("fedcba9876543210fedcba987654321"),
			ProductionManifestSha256,
			TEXT("MainMapVisualProductionCapturefedcba9876543210fedcba987654321")),
		MakeSavedDir(
			TEXT("MainMapVisualProductionCapturefedcba9876543210fedcba987654321")));
	TestRejected(
		TEXT("production manifest SHA에 대문자 hex가 있으면 억제하지 않는다"),
		MakeMainMapVisualProductionCaptureCommandLine(
			ProductionCaptureRunId,
			TEXT("0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF")),
		SavedDir);
	TestRejected(
		TEXT("production manifest SHA가 lowercase ASCII hex가 아니면 억제하지 않는다"),
		MakeMainMapVisualProductionCaptureCommandLine(
			ProductionCaptureRunId,
			TEXT("g123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")),
		SavedDir);
	TestRejected(
		TEXT("production manifest base64 값이 비어 있으면 억제하지 않는다"),
		Valid.Replace(
			*FString::Printf(
				TEXT("CGRProductionManifestBase64=%s"),
				ProductionManifestBase64),
			TEXT("CGRProductionManifestBase64="),
			ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("production capture output base64 값이 비어 있으면 억제하지 않는다"),
		Valid.Replace(
			*FString::Printf(
				TEXT("CGRProductionCaptureOutputBase64=%s"),
				ProductionCaptureOutputBase64),
			TEXT("CGRProductionCaptureOutputBase64="),
			ESearchCase::CaseSensitive),
		SavedDir);
	TestRejected(
		TEXT("production suffix run id가 다르면 억제하지 않는다"),
		MakeMainMapVisualProductionCaptureCommandLine(
			ProductionCaptureRunId,
			ProductionManifestSha256,
			TEXT("MainMapVisualProductionCaptureaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
		MakeSavedDir(
			TEXT("MainMapVisualProductionCaptureaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
	TestRejected(
		TEXT("production suffix에 후행 문자가 있으면 억제하지 않는다"),
		MakeMainMapVisualProductionCaptureCommandLine(
			ProductionCaptureRunId,
			ProductionManifestSha256,
			TEXT("MainMapVisualProductionCapturefedcba9876543210fedcba9876543210x")),
		MakeSavedDir(
			TEXT("MainMapVisualProductionCapturefedcba9876543210fedcba9876543210x")));
	TestRejected(
		TEXT("production 실행과 root Saved 디렉터리가 결합되면 억제하지 않는다"),
		Valid,
		TEXT("C:/Project/Saved/"));
	TestRejected(
		TEXT("production Saved leaf의 대소문자가 다르면 억제하지 않는다"),
		Valid,
		TEXT("C:/Project/Saved_mainMapVisualProductionCapturefedcba9876543210fedcba9876543210/"));
	TestRejected(
		TEXT("production Saved 디렉터리 아래의 중첩 leaf는 억제하지 않는다"),
		Valid,
		TEXT("C:/Project/Saved_MainMapVisualProductionCapturefedcba9876543210fedcba9876543210/Nested/"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCaptureOnlineIsolationPolicyExactContractTest,
	"CGR.MainMapPreview.CaptureIsolation.OnlineRequests.ExactContractsSuppress",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCaptureOnlineIsolationPolicyExactContractTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("정확한 CF1 계약은 온라인 요청을 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"),
				NightWindowBakedHaloStrengthAbSuffix),
			MakeSavedDir(NightWindowBakedHaloStrengthAbSuffix)));
	TestTrue(
		TEXT("정확한 H2 CF1 계약은 온라인 요청을 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_ENERGY_BALANCE_AB"),
				NightWindowBakedHaloEnergyBalanceAbSuffix),
			MakeSavedDir(NightWindowBakedHaloEnergyBalanceAbSuffix)));
	TestTrue(
		TEXT("정확한 H3 CF1 계약은 온라인 요청을 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			MakeCommandLine(
				TEXT("NIGHT_WINDOW_BAKED_HALO_HEADROOM_BAND_AB"),
				NightWindowBakedHaloHeadroomBandAbSuffix),
			MakeSavedDir(NightWindowBakedHaloHeadroomBandAbSuffix)));
	TestTrue(
		TEXT("정확한 RoadNightSpine 계약은 온라인 요청을 억제한다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			MakeRoadNightSpineCommandLine(),
			MakeSavedDir(RoadNightSpineSuffix)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCaptureOnlineIsolationPolicyNearMissTest,
	"CGR.MainMapPreview.CaptureIsolation.OnlineRequests.NearMissDoesNotSuppress",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCaptureOnlineIsolationPolicyNearMissTest::RunTest(const FString& Parameters)
{
	const FString ValidCf1 = MakeCommandLine(
		TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"),
		NightWindowBakedHaloStrengthAbSuffix);
	const FString ValidRoadNightSpine = MakeRoadNightSpineCommandLine();

	TestFalse(
		TEXT("일반 게임 실행은 온라인 요청을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			TEXT("CompanyGrowthRenewal.exe -game -vulkan"),
			TEXT("C:/Project/Saved/")));
	TestFalse(
		TEXT("CF1 명령줄과 Saved 디렉터리가 다르면 온라인 요청을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			ValidCf1,
			TEXT("C:/Project/Saved/")));
	TestFalse(
		TEXT("CF1 stage 스위치가 중복되면 온라인 요청을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			ValidCf1 + TEXT(" -CGRStage=NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"),
			MakeSavedDir(NightWindowBakedHaloStrengthAbSuffix)));
	TestFalse(
		TEXT("RoadNightSpine 명령줄과 Saved 디렉터리가 다르면 온라인 요청을 억제하지 않는다"),
		CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
			ValidRoadNightSpine,
			TEXT("C:/Project/Saved/")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
