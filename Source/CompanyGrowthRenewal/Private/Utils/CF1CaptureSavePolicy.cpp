#include "Utils/CF1CaptureSavePolicy.h"

#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace CGR::CF1CaptureSavePolicy
{
namespace
{
bool TryGetUniqueSwitchValue(const TArray<FString>& Switches, const TCHAR* Key, FString& OutValue)
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

bool IsLowercaseSha256(const FString& Value)
{
	if (Value.Len() != 64)
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
}

bool ShouldSuppressSaveWrites(const FString& CommandLine, const FString& ProjectSavedDir)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse(*CommandLine, Tokens, Switches);

	FString Probe;
	FString Experiment;
	FString Adopted;
	FString Stage;
	FString RunId;
	FString SeedSlotSha256;
	FString SavedDirSuffix;
	if (!TryGetUniqueSwitchValue(Switches, TEXT("CGRProbe"), Probe)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRExperiment"), Experiment)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRAdopted"), Adopted)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRStage"), Stage)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRRunId"), RunId)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("CGRSeedSlotSha256"), SeedSlotSha256)
		|| !TryGetUniqueSwitchValue(Switches, TEXT("saveddirsuffix"), SavedDirSuffix))
	{
		return false;
	}

	if (Probe != TEXT("MobilePCLikeAB")
		|| Experiment != TEXT("CF1")
		|| Adopted != TEXT("E2,L2")
		|| !IsLowercaseSha256(SeedSlotSha256))
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
	if (Stage == TEXT("BASELINE_SAFETY"))
	{
		ExpectedPrefix = TEXT("CF1BaselineSafety");
	}
	else if (Stage == TEXT("AB"))
	{
		ExpectedPrefix = TEXT("CF1AB");
	}
	else
	{
		return false;
	}

	const FString ExpectedSuffix = ExpectedPrefix + ParsedRunId.ToString(EGuidFormats::DigitsLower);
	if (SavedDirSuffix != ExpectedSuffix)
	{
		return false;
	}

	FString NormalizedSavedDir = ProjectSavedDir;
	FPaths::NormalizeDirectoryName(NormalizedSavedDir);
	return FPaths::GetCleanFilename(NormalizedSavedDir) == TEXT("Saved_") + ExpectedSuffix;
}
}
