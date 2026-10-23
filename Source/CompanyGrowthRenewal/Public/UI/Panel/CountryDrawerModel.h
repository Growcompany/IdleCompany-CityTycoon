#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Enum/ResourceType.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"

class UCostActionButtonWidget;

struct FCountryInfoTable;

// 국가 서랍 한 장의 규칙. 위젯 없이 시험하려고 뺐고, 표시 문자열도 여기 한 곳에만 둔다.
namespace CountryDrawer
{
	// 탭 번호는 시설에 고정이다(월드맵이 OnTabChanged 번호로 무대 모듈을 고른다). 0 = 주 시설.
	constexpr int32 TabPrimary = 0;
	constexpr int32 TabFactory = 1;
	constexpr int32 TabMine = 2;
	constexpr int32 TabPort = 3;

	// 순서 = 공장 → 광산 → 항구.
	COMPANYGROWTHRENEWAL_API TArray<EDioramaModule> FacilitiesOf(const FCountryInfoTable& Row);
	COMPANYGROWTHRENEWAL_API bool ShouldShowTabs(const TArray<EDioramaModule>& Facilities);
	COMPANYGROWTHRENEWAL_API int32 TabIndexOf(EDioramaModule Facility);
	// 그 나라에 없는 시설·0 번이면 첫 시설. 시설이 없으면 false.
	COMPANYGROWTHRENEWAL_API bool FacilityForTab(int32 TabIndex, const TArray<EDioramaModule>& Facilities, EDioramaModule& OutFacility);
	COMPANYGROWTHRENEWAL_API bool IsAllOil(const TArray<EResourceType>& Resources);

	COMPANYGROWTHRENEWAL_API FText MineTabLabel(bool bAllOil);
	COMPANYGROWTHRENEWAL_API FText FactoryTitle(const FText& IndustryName);
	COMPANYGROWTHRENEWAL_API FText FactorySummary(int32 Slots, int32 Running, int32 Ready);
	COMPANYGROWTHRENEWAL_API FText MineTitle(bool bAllOil, int32 LineCount);
	COMPANYGROWTHRENEWAL_API FText MineSummary(const TArray<FText>& ResourceNames, bool bAutoClaim, int64 Capacity);
	COMPANYGROWTHRENEWAL_API FText PortTitle(const FText& CountryName);
	COMPANYGROWTHRENEWAL_API FText PortSummary(float PriceMultiplier, int32 OrderCount);

	COMPANYGROWTHRENEWAL_API FText MineHeroSummary(int32 Lines, int32 Full);

	enum class EDrawerPage : uint8 { Production, Upgrade };

	COMPANYGROWTHRENEWAL_API bool ShowsPageToggle(EDioramaModule Facility);
	// 강화 페이지를 보는 동안 생산 쪽에 받을 것이 생기면 토글에 점을 찍는다.
	COMPANYGROWTHRENEWAL_API bool ShowsProductionDot(EDrawerPage Current, int64 PendingClaim);
	// 막대 = 라인 전체 진행(완료 개수 + 지금 만드는 1개). 1개가 끝날 때 0 으로 되돌아가지 않게.
	COMPANYGROWTHRENEWAL_API float LineProgress01(int64 CurrentQty, int64 TargetQty, double UnitFraction01);
	COMPANYGROWTHRENEWAL_API FText DurationLabel(const FTimespan& Duration);
	COMPANYGROWTHRENEWAL_API FText FactoryRemainingLabel(const FTimespan& Remaining);
	COMPANYGROWTHRENEWAL_API FText MineFullEtaLabel(const FTimespan& UntilFull);
	COMPANYGROWTHRENEWAL_API FText CompletedStatus();
	COMPANYGROWTHRENEWAL_API FText FullStatus();
	COMPANYGROWTHRENEWAL_API FText BarCountLabel(int64 Current, int64 Target);
	COMPANYGROWTHRENEWAL_API FText EmptyLineDetail();
	COMPANYGROWTHRENEWAL_API FText EmptyMineDetail();
	// 라인 증설이 최대 미만일 때만 다음 1줄을 잠긴 채 보여준다.
	COMPANYGROWTHRENEWAL_API bool ShowsLockedLine(int32 CurrentSlots, int32 MaxSlots);
	COMPANYGROWTHRENEWAL_API FText LockedLineName(int32 Number);
	COMPANYGROWTHRENEWAL_API FText LockedLineDetail();
	// 강화 슬롯 잠금 덮개는 해금 조건 전용이다 ― 최대 레벨은 잠금이 아니라 회색 「최대」 버튼으로 보인다.
	COMPANYGROWTHRENEWAL_API bool UpgradeSlotLocked(bool bMaxed);
	COMPANYGROWTHRENEWAL_API FText MaxLevelLabel();
	// 즉시 완료 확인 창의 제목이자 확인 버튼 라벨
	COMPANYGROWTHRENEWAL_API FText InstantFinishTitle();
	COMPANYGROWTHRENEWAL_API FText CancelLabel();
	COMPANYGROWTHRENEWAL_API FText InstantFinishMessage(int32 Diamonds, int64 RemainingQty, bool bMine);
	COMPANYGROWTHRENEWAL_API FText ProductionLimitBand(const FText& ResourceName, int64 Have, int32 MaxQty);

	COMPANYGROWTHRENEWAL_API FText ClaimFactoryLabel();
	COMPANYGROWTHRENEWAL_API FText ClaimMineLabel();
	COMPANYGROWTHRENEWAL_API FText SellAllLabel();
	COMPANYGROWTHRENEWAL_API FText EmptyLineName();
	COMPANYGROWTHRENEWAL_API FText StartLineLabel();
	COMPANYGROWTHRENEWAL_API FText StartMineLabel();
	// 광산 빈 칸이 캘 자원 = 나라 풀(DT 순서) 중 아직 라인이 없는 첫 자원. 없으면 None.
	COMPANYGROWTHRENEWAL_API EResourceType NextMineResource(const TArray<EResourceType>& Pool, const TArray<EResourceType>& Used);
	// spec §2.8: 이름 = 「<등급> 주문 · <제품명>」(제품명이 없으면 「<등급> 주문」), 설명 = 「<N>개 납품 · 보상 ×<배율>」
	COMPANYGROWTHRENEWAL_API FText OrderName(const FText& TierLabel, const FText& ProductName);
	COMPANYGROWTHRENEWAL_API FText OrderDetail(int64 Quantity, float RewardMultiplier);
	COMPANYGROWTHRENEWAL_API FText OrderStatus(bool bDeliverable);
	// 서랍 버튼 흰 칩 숫자 = 진한 잉크 #1B2330(자원 UIColor 는 리튬·다이아 원석처럼 흰 칩에 안 읽힌다), 받을 것(받기·모두 받기·모두 판매) = 「+」
	COMPANYGROWTHRENEWAL_API void StyleChipCount(UCostActionButtonWidget* Button, bool bIncoming);
	COMPANYGROWTHRENEWAL_API FText GoodsQuantity(int64 Quantity);
	COMPANYGROWTHRENEWAL_API FText NoQuickStartOrder();
	COMPANYGROWTHRENEWAL_API FText QuickStartDetail();
	COMPANYGROWTHRENEWAL_API FText QuickStartQuantity(int32 Quantity);

	struct FQuickStartCandidate
	{
		int32 OrderID = 0;
		ECompanyType Company = ECompanyType::None;
		int32 ProjectIndex = 0;
		int64 UnitValue = 0;
		int32 MaxProducible = 0;
	};

	// 마지막에 쓴 제품과 같은 업종·제품 중 만들 수 있는 것, 없으면 단가가 가장 높은 것. 동률은 OrderID 작은 쪽.
	// 반환 = Candidates 인덱스, 만들 수 있는 것이 없으면 INDEX_NONE.
	COMPANYGROWTHRENEWAL_API int32 PickQuickStart(const TArray<FQuickStartCandidate>& Candidates,
		ECompanyType LastCompany, int32 LastProjectIndex);
}
