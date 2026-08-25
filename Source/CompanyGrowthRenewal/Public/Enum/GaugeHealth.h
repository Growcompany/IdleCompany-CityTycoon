#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "GaugeHealth.generated.h"

/**
 * 건물 위 금고 게이지의 운영 상태.
 * 채움 길이와 상태 둘 다 운영 진행 시간에서 나온다 ― 금고 만액은 VaultFull 버블이 따로 알린다.
 */
UENUM(BlueprintType)
enum class EGaugeHealth : uint8
{
	Earning    = 0 UMETA(DisplayName = "수익 중"),
	EndingSoon = 1 UMETA(DisplayName = "종료 임박"),
	Idle       = 2 UMETA(DisplayName = "정지"),
};

UENUM(BlueprintType)
enum class EVaultGaugePresentation : uint8
{
	Bar   = 0,
	Pearl = 1,
};

enum class EVaultGaugeLOD : uint8
{
	Uninitialized,
	Bar,
	Pearl,
	Hidden,
};

/** 비싼 후보 전체 재조정이 필요한 사건과 값만 갱신할 사건을 구분한다. */
enum class EVaultGaugeReconcileTrigger : uint8
{
	CompanyOrBubbleRefresh,
	OperationStarted,
	OperationCompleted,
	BuildingListChanged,
	TrackedBuildingExpired,
	WarehouseValueUpdated,
};

/** 매 틱 후보의 만료와 카메라 재투영 필요 여부를 함께 판정한다. */
enum class EVaultGaugeCandidateFrameAction : uint8
{
	CollapseAndReconcile,
	SkipReprojection,
	ProjectCachedBuilding,
};

inline float ResolveVaultGaugeBarOpacity(EGaugeHealth Health)
{
	return Health == EGaugeHealth::Idle ? 0.55f : 1.0f;
}

/**
 * Bar 는 프로젝트 진행 시간 축이라 금고 상태와 직교한다 — 만액 버블과 공존한다.
 * 술어가 한 항으로 줄어든 것이 축이 표시 조건과 정렬됐다는 증거다
 * (구 모델은 조건이 '운영 중'인데 길이는 돈을 말해 두 축이 어긋나 있었다).
 */
inline bool ShouldDisplayVaultGauge(bool bHasActiveOperation)
{
	return bHasActiveOperation;
}

inline bool ShouldRequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger Trigger)
{
	switch (Trigger)
	{
	case EVaultGaugeReconcileTrigger::CompanyOrBubbleRefresh:
	case EVaultGaugeReconcileTrigger::OperationStarted:
	case EVaultGaugeReconcileTrigger::OperationCompleted:
	case EVaultGaugeReconcileTrigger::BuildingListChanged:
	case EVaultGaugeReconcileTrigger::TrackedBuildingExpired:
		return true;
	case EVaultGaugeReconcileTrigger::WarehouseValueUpdated:
	default:
		return false;
	}
}

inline EVaultGaugeCandidateFrameAction ResolveVaultGaugeCandidateFrameAction(
	bool bCachedBuildingValid,
	bool bNeedsReproject)
{
	if (!bCachedBuildingValid)
	{
		return EVaultGaugeCandidateFrameAction::CollapseAndReconcile;
	}

	return bNeedsReproject
		? EVaultGaugeCandidateFrameAction::ProjectCachedBuilding
		: EVaultGaugeCandidateFrameAction::SkipReprojection;
}

/** 운영 경과율 0~1. TotalOperationTime 이 0 이하면 진행률이 정의되지 않으므로 0. */
inline float ComputeOperationProgress(float ElapsedTime, float TotalOperationTime)
{
	if (TotalOperationTime <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(ElapsedTime / TotalOperationTime, 0.0f, 1.0f);
}

/** 운영 경과율을 게이지 상태로 분류한다. */
inline EGaugeHealth ClassifyGaugeHealth(float OpProgress, bool bHasOperation)
{
	if (!bHasOperation)
	{
		return EGaugeHealth::Idle;
	}

	return FMath::Clamp(OpProgress, 0.0f, 1.0f) > 0.75f
		? EGaugeHealth::EndingSoon
		: EGaugeHealth::Earning;
}

/** 현재 LOD에 히스테리시스를 적용해 줌 변화 경계의 깜빡임을 막는다. */
inline EVaultGaugeLOD ResolveVaultGaugeLOD(float Zoom, EVaultGaugeLOD Current)
{
	const float Z = FMath::Clamp(Zoom, 0.0f, 1.0f);
	if (Current == EVaultGaugeLOD::Uninitialized)
	{
		return Z <= 0.50f
			? EVaultGaugeLOD::Bar
			: (Z <= 0.62f ? EVaultGaugeLOD::Pearl : EVaultGaugeLOD::Hidden);
	}
	if (Z > 0.64f)
	{
		return EVaultGaugeLOD::Hidden;
	}
	if (Z < 0.48f)
	{
		return EVaultGaugeLOD::Bar;
	}
	if (Current == EVaultGaugeLOD::Bar && Z > 0.52f)
	{
		return EVaultGaugeLOD::Pearl;
	}
	if (Current == EVaultGaugeLOD::Hidden && Z < 0.60f)
	{
		return EVaultGaugeLOD::Pearl;
	}
	return Current;
}

/** Hidden LOD에서도 튜토리얼이 설명할 단일 건물만은 기존 게이지 풀에 남긴다. */
inline bool ShouldIncludeGaugeAtLOD(
	EVaultGaugeLOD CurrentLOD,
	int32 BuildingIndex,
	int32 TutorialReservedBuildingIndex)
{
	return CurrentLOD != EVaultGaugeLOD::Hidden
		|| BuildingIndex == TutorialReservedBuildingIndex;
}

/** 튜토리얼 설명 대상은 Pearl로 축약하지 않고 진행 Bar 형태를 보장한다. */
inline EVaultGaugePresentation ResolveGaugePresentation(
	EVaultGaugeLOD CurrentLOD,
	bool bSelected,
	bool bTutorialReserved)
{
	return (CurrentLOD == EVaultGaugeLOD::Bar || bSelected || bTutorialReserved)
		? EVaultGaugePresentation::Bar
		: EVaultGaugePresentation::Pearl;
}

/**
 * 게이지 폭 = 건물 화면폭 비례 + 상하한 클램프.
 * 먼 건물에서 게이지가 건물보다 넓어지면 어느 건물 것인지 모호해지므로 상한이 필수.
 */
inline float ComputeGaugeWidth(float BuildingScreenWidth, float WidthRatio, float MinWidth, float MaxWidth)
{
	return FMath::Clamp(BuildingScreenWidth * WidthRatio, MinWidth, MaxWidth);
}

/** 버블이 Bar 를 덮지 않도록 올릴 높이. Pearl 은 점으로 강등돼 덜 올린다. */
inline float ComputeBubbleLiftPx(EVaultGaugePresentation Presentation, float BarLiftPx, float PearlLiftPx)
{
	return Presentation == EVaultGaugePresentation::Bar ? BarLiftPx : PearlLiftPx;
}

/** 좌측 정렬된 광택 캡 이미지의 게이지 내부 이동 오프셋을 계산한다. */
inline float ComputeFillTipOffset(float Width, float FillPct, float Inset, float TipWidth)
{
	const float SafeWidth = FMath::Max(0.0f, Width);
	const float SafeInset = FMath::Clamp(Inset, 0.0f, SafeWidth * 0.5f);
	const float InnerWidth = FMath::Max(0.0f, SafeWidth - SafeInset * 2.0f);
	const float MaxOffset = FMath::Max(SafeInset, SafeWidth - SafeInset - TipWidth);
	const float Offset = SafeInset
		+ FMath::Clamp(FillPct, 0.0f, 1.0f) * InnerWidth
		- TipWidth * 0.5f;
	return FMath::Clamp(Offset, SafeInset, MaxOffset);
}

/** 게이지 후보 1개 — 화면 투영 결과를 담아 정렬/절단에 넘긴다. */
struct FGaugeCandidate
{
	int32 BuildingIndex = INDEX_NONE;
	FVector2D CanvasPos = FVector2D::ZeroVector;
	float ScreenWidth = 0.0f;
	float DistSq = 0.0f;   // SelectGaugesByProximity 가 채운다
	EGaugeHealth Health = EGaugeHealth::Idle;
	EVaultGaugePresentation Presentation = EVaultGaugePresentation::Bar;
	float Progress = 0.0f;   // 운영 경과율 ― Bar 길이의 소스

	// 후보 생성 시 캐시. 매 틱 추적이 GetBuildingByIndex(선형 스캔)를 게이지마다 다시 돌리면
	// 200건물 × 캡 16 = 프레임당 수천 비교가 된다. 만료되면 슬롯을 접고 후보를 다시 산출한다.
	TWeakObjectPtr<class ABuildingBaseActor> BuildingPtr;
};

/**
 * 화면 중앙 근접 오름차순 정렬 후 상위 Cap 개만 남긴다. Cap <= 0 이면 무제한.
 *
 * ⚠ 우선순위 축을 버블에서 복제하지 말 것. 버블의 긴급도는 행동 순서이고,
 * 게이지는 화면에서 어느 활성 건물에 붙었는지가 먼저 읽혀야 한다.
 * 카메라 이동 중 배정이 튀지 않도록 계기판은 공간적 일관성으로 솎는다.
 *
 * 동점은 BuildingIndex 오름차순 — TArray::Sort 는 unstable 이라 명시적 tie-break 가 없으면
 * 프레임마다 순서가 뒤바뀌어 게이지가 깜빡인다.
 *
 * ⚠ 동점 판정은 정확 비교(!=) — IsNearlyEqual 은 비전이적(a≈b, b≈c 인데 a≉c)이라
 * DistSq 0.125 미만에서 비교자에 순환을 만들고 checked 빌드의 Sort 가 assert 한다.
 */
inline void SelectGaugesByProximity(
	TArray<FGaugeCandidate>& InOut,
	const FVector2D& ScreenCenter,
	int32 Cap,
	int32 PriorityBuildingIndex = INDEX_NONE)
{
	for (FGaugeCandidate& C : InOut)
	{
		C.DistSq = FVector2D::DistSquared(C.CanvasPos, ScreenCenter);
	}

	InOut.Sort([PriorityBuildingIndex](const FGaugeCandidate& A, const FGaugeCandidate& B)
	{
		const bool bAPriority = A.BuildingIndex == PriorityBuildingIndex;
		const bool bBPriority = B.BuildingIndex == PriorityBuildingIndex;
		if (bAPriority != bBPriority)
		{
			return bAPriority;
		}
		if (A.DistSq != B.DistSq)
		{
			return A.DistSq < B.DistSq;
		}
		return A.BuildingIndex < B.BuildingIndex;
	});

	if (Cap > 0 && InOut.Num() > Cap)
	{
		InOut.SetNum(Cap);
	}
}
