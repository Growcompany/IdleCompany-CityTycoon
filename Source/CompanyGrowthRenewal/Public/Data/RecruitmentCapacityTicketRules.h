#pragma once

#include "CoreMinimal.h"

struct FRecruitmentCapacityTicketDecision
{
	int32 GrantCount = 0;
	int32 NewCreditedCapacity = 0;
};

struct COMPANYGROWTHRENEWAL_API FRecruitmentCapacityTicketRules
{
	static FRecruitmentCapacityTicketDecision Evaluate(int32 CurrentCapacity, int32 CreditedCapacity)
	{
		const int32 SafeCurrentCapacity = FMath::Max(0, CurrentCapacity);
		const int32 SafeCreditedCapacity = FMath::Max(0, CreditedCapacity);

		FRecruitmentCapacityTicketDecision Decision;
		Decision.GrantCount = FMath::Max(0, SafeCurrentCapacity - SafeCreditedCapacity);
		Decision.NewCreditedCapacity = FMath::Max(SafeCurrentCapacity, SafeCreditedCapacity);
		return Decision;
	}
};
