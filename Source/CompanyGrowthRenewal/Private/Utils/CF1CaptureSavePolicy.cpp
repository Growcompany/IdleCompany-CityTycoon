#include "Utils/CF1CaptureSavePolicy.h"

#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace CGR::CF1CaptureSavePolicy
{
namespace
{
bool TryGetUniqueSwitchValue(
	const TArray<FString>& Switches,
	const TCHAR* Key,
	FString& OutValue,
	const bool bRequireExactKeyCase = false)
{
	int32 MatchCount = 0;
	FString MatchedValue;

	for (const FString& Switch : Switches)
	{
		FString SwitchKey;
		FString SwitchValue;
		if (Switch.Split(TEXT("="), &SwitchKey, &SwitchValue))
		{
			if (!SwitchKey.Equals(Key, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		else
		{
			if (!Switch.Equals(Key, ESearchCase::IgnoreCase))
			{
				continue;
			}
			SwitchValue.Reset();
		}
		if (bRequireExactKeyCase && !SwitchKey.Equals(Key, ESearchCase::CaseSensitive))
		{
			return false;
		}

		++MatchCount;
		MatchedValue = MoveTemp(SwitchValue);
	}

	if (MatchCount != 1 || MatchedValue.IsEmpty())
	{
		return false;
	}

	OutValue = MoveTemp(MatchedValue);
	return true;
}

bool HasUniqueExactBareSwitch(const TArray<FString>& Switches, const TCHAR* Key)
{
	int32 MatchCount = 0;
	for (const FString& Switch : Switches)
	{
		FString SwitchKey;
		FString SwitchValue;
		const bool bHasValue = Switch.Split(TEXT("="), &SwitchKey, &SwitchValue);
		const FString& ComparableKey = bHasValue ? SwitchKey : Switch;
		if (!ComparableKey.Equals(Key, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (bHasValue || !Switch.Equals(Key, ESearchCase::CaseSensitive))
		{
			return false;
		}

		++MatchCount;
	}

	return MatchCount == 1;
}

bool IsLowercaseHex(const FString& Value, const int32 ExpectedLength)
{
	if (Value.Len() != ExpectedLength)
	{
		return false;
	}

	for (const TCHAR Character : Value)
	{
		const bool bAsciiDigit = Character >= TEXT('0') && Character <= TEXT('9');
		const bool bLowerHexLetter = Character >= TEXT('a') && Character <= TEXT('f');
		if (!bAsciiDigit && !bLowerHexLetter)
		{
			return false;
		}
	}

	return true;
}

bool IsRoadNightSpineContract(
	const TArray<FString>& Switches,
	const FString& ProjectSavedDir)
{
	FString RunId;
	FString SavedDirSuffix;
	if (!TryGetUniqueSwitchValue(
			Switches, TEXT("RoadNightSpineRunId"), RunId, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("saveddirsuffix"), SavedDirSuffix, true)
		|| !IsLowercaseHex(RunId, 32))
	{
		return false;
	}

	const FString ExpectedSuffix = TEXT("RoadNightSpine") + RunId;
	if (!SavedDirSuffix.Equals(ExpectedSuffix, ESearchCase::CaseSensitive))
	{
		return false;
	}

	FString NormalizedSavedDir = ProjectSavedDir;
	FPaths::NormalizeDirectoryName(NormalizedSavedDir);
	return FPaths::GetCleanFilename(NormalizedSavedDir).Equals(
		TEXT("Saved_") + ExpectedSuffix, ESearchCase::CaseSensitive);
}

bool IsOfficeExteriorCaptureContract(
	const TArray<FString>& Switches,
	const FString& ProjectSavedDir)
{
	FString RunId;
	FString SavedDirSuffix;
	if (!TryGetUniqueSwitchValue(
			Switches, TEXT("OfficeExteriorCaptureRunId"), RunId, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("saveddirsuffix"), SavedDirSuffix, true)
		|| !IsLowercaseHex(RunId, 32))
	{
		return false;
	}

	const FString ExpectedSuffix = TEXT("OfficeExteriorCapture") + RunId;
	if (!SavedDirSuffix.Equals(ExpectedSuffix, ESearchCase::CaseSensitive))
	{
		return false;
	}

	FString NormalizedSavedDir = ProjectSavedDir;
	FPaths::NormalizeDirectoryName(NormalizedSavedDir);
	return FPaths::GetCleanFilename(NormalizedSavedDir).Equals(
		TEXT("Saved_") + ExpectedSuffix, ESearchCase::CaseSensitive);
}

bool IsMainMapVisualProductionCaptureContract(
	const TArray<FString>& Switches,
	const FString& ProjectSavedDir)
{
	if (!HasUniqueExactBareSwitch(Switches, TEXT("game"))
		|| !HasUniqueExactBareSwitch(Switches, TEXT("vulkan"))
		|| !HasUniqueExactBareSwitch(Switches, TEXT("FeatureLevelES31")))
	{
		return false;
	}

	FString CaptureValue;
	FString RunId;
	FString ManifestSha256;
	FString ManifestBase64;
	FString CaptureOutputBase64;
	FString DeviceProfile;
	FString SavedDirSuffix;
	if (!TryGetUniqueSwitchValue(
			Switches, TEXT("CGRProductionCapture"), CaptureValue, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("CGRProductionCaptureIsolationRunId"), RunId, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("CGRProductionManifestSha256"), ManifestSha256, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("CGRProductionManifestBase64"), ManifestBase64, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("CGRProductionCaptureOutputBase64"), CaptureOutputBase64, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("DeviceProfile"), DeviceProfile, true)
		|| !TryGetUniqueSwitchValue(
			Switches, TEXT("saveddirsuffix"), SavedDirSuffix, true))
	{
		return false;
	}

	if (!CaptureValue.Equals(TEXT("1"), ESearchCase::CaseSensitive)
		|| !IsLowercaseHex(RunId, 32)
		|| !IsLowercaseHex(ManifestSha256, 64)
		|| ManifestBase64.IsEmpty()
		|| CaptureOutputBase64.IsEmpty()
		|| !DeviceProfile.Equals(TEXT("Android_High"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	const FString ExpectedSuffix = TEXT("MainMapVisualProductionCapture") + RunId;
	if (!SavedDirSuffix.Equals(ExpectedSuffix, ESearchCase::CaseSensitive))
	{
		return false;
	}

	FString NormalizedSavedDir = ProjectSavedDir;
	FPaths::NormalizeDirectoryName(NormalizedSavedDir);
	return FPaths::GetCleanFilename(NormalizedSavedDir).Equals(
		TEXT("Saved_") + ExpectedSuffix, ESearchCase::CaseSensitive);
}
}

static bool IsExactIsolatedCapture(const FString& CommandLine, const FString& ProjectSavedDir)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse(*CommandLine, Tokens, Switches);
	if (IsMainMapVisualProductionCaptureContract(Switches, ProjectSavedDir))
	{
		return true;
	}
	if (IsOfficeExteriorCaptureContract(Switches, ProjectSavedDir))
	{
		return true;
	}
	if (IsRoadNightSpineContract(Switches, ProjectSavedDir))
	{
		return true;
	}

	FString Probe;
	FString Experiment;
	FString Adopted;
	FString Stage;
	FString RunId;
	FString SeedSlotSha256;
	FString MutationSignature;
	FString SavedDirSuffix;
	if (!TryGetUniqueSwitchValue(Switches, TEXT("CGRProbe"), Probe)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRExperiment"), Experiment)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRAdopted"), Adopted)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRStage"), Stage)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRRunId"), RunId)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRSeedSlotSha256"), SeedSlotSha256)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRMutationSignature"), MutationSignature)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("saveddirsuffix"), SavedDirSuffix))
	{
		return false;
	}

	if (Probe != TEXT("MobilePCLikeAB")
		|| Experiment != TEXT("CF1")
		|| Adopted != TEXT("E2,L2")
		|| !IsLowercaseHex(SeedSlotSha256, 64))
	{
		return false;
	}

	FGuid ParsedRunId;
	if (RunId.Len() != 36
		|| !FGuid::ParseExact(RunId, EGuidFormats::DigitsWithHyphensLower, ParsedRunId)
		|| !ParsedRunId.ToString(EGuidFormats::DigitsWithHyphensLower).Equals(
			RunId, ESearchCase::CaseSensitive))
	{
		return false;
	}

	FString ExpectedPrefix;
	FString ExpectedMutationSignature;
	if (Stage.Equals(TEXT("BASELINE_SAFETY"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1BaselineSafety");
		ExpectedMutationSignature = TEXT("sha256:023bfdb101eb76f61041a14e95b3b5ab446ae80afad0acce500aea843e5df8ef");
	}
	else if (Stage.Equals(TEXT("AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1AB");
		ExpectedMutationSignature = TEXT("sha256:023bfdb101eb76f61041a14e95b3b5ab446ae80afad0acce500aea843e5df8ef");
	}
	else if (Stage.Equals(TEXT("LUT_SCHEDULE_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1LutScheduleAB");
		ExpectedMutationSignature = TEXT("sha256:bc11df6c96dd567836c3c5165da01f0479306338d55f69550cbcd2d5c4267c06");
	}
	else if (Stage.Equals(TEXT("DAWN_GAIN_MIDTONES_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1DawnGainMidtonesAB");
		ExpectedMutationSignature = TEXT("sha256:c6f8617456d25e0cd5976e50334fee3aacf7fc3cb0aeef5206dc2325f48af4d5");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BLOOM_THRESHOLD_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBloomThresholdAB");
		ExpectedMutationSignature = TEXT("sha256:65eca5c7fec394446e645188c5ee7c3faaf0dd47016a6951e640077e41fd9cad");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBloomIntensityAB");
		ExpectedMutationSignature = TEXT("sha256:1bb8186dfaba1dc9337bf0d93dd295305e5e314c4d32fc33cfee56e29bf95aa5");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowEmissiveAB");
		ExpectedMutationSignature = TEXT("sha256:edaed16d1863468473c4af61b9f0a8a4269ce029185789c7713ccbc627a217b0");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_FINE_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowEmissiveFineAB");
		ExpectedMutationSignature = TEXT("sha256:cec082d28be2d0192664b9843c95eea361e6f31c6ad0dad0c86b062a72db4618");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_CHROMA_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowEmissiveChromaAB");
		ExpectedMutationSignature = TEXT("sha256:9449cbf0594371f6e0ae26d373e231fac57545a1eb3e2e9ffe5f7de7249caeeb");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_EMISSIVE_PALETTE_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowEmissivePaletteAB");
		ExpectedMutationSignature = TEXT("sha256:5f29d73a96a1f58513f48528362d496158d55aab234731a562c646b3f3d609b9");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BLOOM5_TINT_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBloom5TintAB");
		ExpectedMutationSignature = TEXT("sha256:66fe1abdbfb78669bad7443d5ecc2c5ffb15cdd8950792de060d60b0a02adc22");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BLOOM4_TINT_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBloom4TintAB");
		ExpectedMutationSignature = TEXT("sha256:a54ef08516b5321e7904429e04204ba20d9ca717ae4e62a782ccf5aaa85d88c6");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BLOOM_INTENSITY_LIT_SOURCE_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBloomIntensityLitSourceAB");
		ExpectedMutationSignature = TEXT("sha256:9cc4740b0f1a1d057cf46ad4e044c2a9e89159ff8340a7eb43839af743aaef78");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BAKED_HALO_STRENGTH_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBakedHaloStrengthAB");
		ExpectedMutationSignature = TEXT("sha256:414a80c3abbc84192dc2bbbfc82b0fed3284f25cae4a357e5b830f3bae51e134");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BAKED_HALO_ENERGY_BALANCE_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBakedHaloEnergyBalanceAB");
		ExpectedMutationSignature = TEXT("sha256:ac9bea9975108422a69eaf93a6da86eb6bbdcace2f32a2ac625562c3dc708d16");
	}
	else if (Stage.Equals(TEXT("NIGHT_WINDOW_BAKED_HALO_HEADROOM_BAND_AB"), ESearchCase::CaseSensitive))
	{
		ExpectedPrefix = TEXT("CF1NightWindowBakedHaloHeadroomBandAB");
		ExpectedMutationSignature = TEXT("sha256:bff28cbc8a0e613ba8a94b64608cfe5d3f5a22daa7f0a1d2806f59cbf68923c3");
	}
	else
	{
		return false;
	}
	if (!MutationSignature.Equals(ExpectedMutationSignature, ESearchCase::CaseSensitive))
	{
		return false;
	}

	const FString ExpectedSuffix = ExpectedPrefix + ParsedRunId.ToString(EGuidFormats::DigitsLower);
	if (!SavedDirSuffix.Equals(ExpectedSuffix, ESearchCase::CaseSensitive))
	{
		return false;
	}

	FString NormalizedSavedDir = ProjectSavedDir;
	FPaths::NormalizeDirectoryName(NormalizedSavedDir);
	return FPaths::GetCleanFilename(NormalizedSavedDir).Equals(
		TEXT("Saved_") + ExpectedSuffix, ESearchCase::CaseSensitive);
}

bool ShouldSuppressSaveWrites(const FString& CommandLine, const FString& ProjectSavedDir)
{
	return IsExactIsolatedCapture(CommandLine, ProjectSavedDir);
}

bool ShouldSuppressOnlineRequests(const FString& CommandLine, const FString& ProjectSavedDir)
{
	return IsExactIsolatedCapture(CommandLine, ProjectSavedDir);
}
}
