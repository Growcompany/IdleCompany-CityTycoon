#pragma once

#include "CoreMinimal.h"

namespace CGR::CF1CaptureSavePolicy
{
	bool ShouldSuppressSaveWrites(const FString& CommandLine, const FString& ProjectSavedDir);
	bool ShouldSuppressOnlineRequests(const FString& CommandLine, const FString& ProjectSavedDir);
}
