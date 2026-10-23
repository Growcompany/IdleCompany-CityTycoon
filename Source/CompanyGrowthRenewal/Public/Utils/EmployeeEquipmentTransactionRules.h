#pragma once
#include "CoreMinimal.h"
#include "Data/EmployeeEquipmentData.h"
#include "Table/EmployeeEquipmentTable.h"
#include "Misc/SecureHash.h"

namespace EmployeeEquipmentRules
{
    // 공유 가드는 저장부터 최종 알림까지 유지되어 콜백이 진행 중인 커밋을 덮어쓰지 못한다.
    template<typename SnapshotType, typename PersistCallback, typename ApplyCallback, typename PublishCallback>
    bool CommitCandidate(bool& bCommitInProgress, const SnapshotType& Candidate, PersistCallback&& Persist, ApplyCallback&& Apply, PublishCallback&& Publish)
    {
        if (bCommitInProgress) return false;
        TGuardValue<bool> CommitGuard(bCommitInProgress, true);
        if (!Persist(Candidate)) return false;
        Apply(Candidate);
        Publish(Candidate);
        return true;
    }

    // 신입 지급 후보 = 지정 등급의 정의 전체. TMap 순회 순서는 보장이 없어 정렬해야 같은 시드가 같은 장비를 준다.
    inline TArray<FName> BuildRecruitGiftPool(const TMap<FName, FEmployeeEquipmentDefinitionRow>& Definitions, EEmployeeEquipmentGrade Grade)
    {
        TArray<FName> Pool;
        for (const auto& Pair : Definitions)
            if (Pair.Value.Grade == Grade) Pool.Add(Pair.Key);
        Pool.Sort([](const FName& Left, const FName& Right) { return Left.LexicalLess(Right); });
        return Pool;
    }

    inline FString FingerprintRequest(const FEmployeeEquipmentRequest& Request)
    {
        // 명시적 필드 순서로 정규화하여 재시도가 같은 명령과 대상을 가리키는지 검증한다.
        FString Canonical = FString::Printf(TEXT("%d|%lld|%s|%s|%d|%d|%d|%d|%s|%d|%d|%d|%d|%d|%s"),
            int32(Request.Command), Request.ExpectedRevision, *Request.CatalogVersion.ToString(),
            *Request.BannerId.ToString(), Request.Count, Request.EmployeeId, Request.ExpectedSourceEmployeeId, int32(Request.EquipmentSlot),
            *Request.InstanceId.ToString(), int32(Request.MaterialMode), Request.ExpectedLevel, Request.bLocked,
            Request.bConfirmEnhancedDismantle, int32(Request.AppearanceMode), *Request.AppearanceId.ToString());
        TArray<FString> MaterialIds;
        for (const FGuid& Id : Request.InstanceIds) MaterialIds.Add(Id.ToString());
        MaterialIds.Sort();
        for (const FString& Id : MaterialIds) Canonical += TEXT("|") + Id;
        return FMD5::HashAnsiString(*Canonical);
    }

    inline EEmployeeEquipmentGrade AdvancePity(FEmployeeEquipmentPityState& Pity, EEmployeeEquipmentGrade Natural, int32 Epic, int32 Legendary, int32 Ascendant)
    {
        Pity.SinceEpic = FMath::Clamp(Pity.SinceEpic, 0, Epic - 1) + 1;
        Pity.SinceLegendary = FMath::Clamp(Pity.SinceLegendary, 0, Legendary - 1) + 1;
        Pity.SinceAscendant = FMath::Clamp(Pity.SinceAscendant, 0, Ascendant - 1) + 1;
        uint8 Result = static_cast<uint8>(Natural);
        if (Pity.SinceEpic >= Epic) Result = FMath::Max(Result, static_cast<uint8>(EEmployeeEquipmentGrade::Epic));
        if (Pity.SinceLegendary >= Legendary) Result = FMath::Max(Result, static_cast<uint8>(EEmployeeEquipmentGrade::Legendary));
        if (Pity.SinceAscendant >= Ascendant) Result = static_cast<uint8>(EEmployeeEquipmentGrade::Ascendant);
        if (Result >= static_cast<uint8>(EEmployeeEquipmentGrade::Epic)) Pity.SinceEpic = 0;
        if (Result >= static_cast<uint8>(EEmployeeEquipmentGrade::Legendary)) Pity.SinceLegendary = 0;
        if (Result >= static_cast<uint8>(EEmployeeEquipmentGrade::Ascendant)) Pity.SinceAscendant = 0;
        return static_cast<EEmployeeEquipmentGrade>(Result);
    }

    inline bool IsEquipmentTicket(EItemType Item)
    {
        return Item == EItemType::EmployeeEquipmentTicketNormal || Item == EItemType::EmployeeEquipmentTicketAdvanced || Item == EItemType::EmployeeEquipmentTicketPremium;
    }

    inline bool QuoteTicketPull(FName BannerId, EItemType TicketType, int32 Count, int32 AvailableTickets, FEmployeeEquipmentPullQuote& OutQuote)
    {
        OutQuote = {};
        OutQuote.BannerId = BannerId;
        OutQuote.TicketType = TicketType;
        if (BannerId.IsNone() || !IsEquipmentTicket(TicketType) || (Count != 1 && Count != 10 && Count != 30) || AvailableTickets < 0)
        {
            OutQuote.ErrorCode = TEXT("InvalidPullRequest");
            return false;
        }
        OutQuote.RequiredTickets = Count;
        OutQuote.AvailableTickets = AvailableTickets;
        OutQuote.MissingTickets = FMath::Max(0, Count - AvailableTickets);
        OutQuote.bCanPull = OutQuote.MissingTickets == 0;
        if (!OutQuote.bCanPull) OutQuote.ErrorCode = TEXT("InsufficientTickets");
        return true;
    }

    inline bool ValidateBannerSettings(const FEmployeeEquipmentBannerRow& Banner)
    {
        if (Banner.BannerId.IsNone() || Banner.PityGroupId.IsNone() || !IsEquipmentTicket(Banner.TicketType) ||
            Banner.GradeWeights.Num() != int32(EEmployeeEquipmentGrade::Count) || Banner.EpicPity <= 0 ||
            Banner.LegendaryPity <= Banner.EpicPity || Banner.AscendantPity <= Banner.LegendaryPity ||
            (Banner.EndsAtUtc.GetTicks() > 0 && Banner.StartsAtUtc.GetTicks() > 0 && Banner.EndsAtUtc <= Banner.StartsAtUtc)) return false;
        int32 Sum = 0;
        for (int32 Weight : Banner.GradeWeights)
        {
            if (Weight < 0 || Weight > 10000) return false;
            Sum += Weight;
        }
        return Sum == 10000;
    }

    inline bool IsGradeReachable(const FEmployeeEquipmentBannerRow& Banner, EEmployeeEquipmentGrade Grade)
    {
        const int32 Index = int32(Grade);
        if (!Banner.GradeWeights.IsValidIndex(Index)) return false;
        if (Banner.GradeWeights[Index] > 0) return true;
        if (Grade < EEmployeeEquipmentGrade::Epic) return false;
        for (int32 Lower = 0; Lower < Index; ++Lower)
            if (Banner.GradeWeights[Lower] > 0) return true;
        return false;
    }

    inline TArray<int32> NextGradeWeights(const FEmployeeEquipmentBannerRow& Banner, const FEmployeeEquipmentPityState& CurrentPity)
    {
        if (!ValidateBannerSettings(Banner)) return {};
        TArray<int32> Weights;
        Weights.Init(0, int32(EEmployeeEquipmentGrade::Count));
        for (int32 Index = 0; Index < Weights.Num(); ++Index)
        {
            FEmployeeEquipmentPityState Pity = CurrentPity;
            const EEmployeeEquipmentGrade Result = AdvancePity(Pity, EEmployeeEquipmentGrade(Index), Banner.EpicPity, Banner.LegendaryPity, Banner.AscendantPity);
            Weights[int32(Result)] += Banner.GradeWeights[Index];
        }
        return Weights;
    }

    inline FString WeeklyPeriodId(const FDateTime& Utc)
    {
        const FDateTime Kst = Utc + FTimespan::FromHours(9);
        const FDateTime Midnight(Kst.GetYear(), Kst.GetMonth(), Kst.GetDay());
        return (Midnight - FTimespan::FromDays(static_cast<int32>(Kst.GetDayOfWeek()))).ToString(TEXT("%Y-%m-%d"));
    }
}
