#include "UI/Panel/CountryDrawerModel.h"
#include "Table/CountryInfoTable.h"
#include "Global/GlobalUtilFunctions.h"
#include "Global/KoreanJosa.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"

namespace CountryDrawer
{
	namespace
	{
		FText JoinWithDot(const TArray<FText>& Parts)
		{
			TArray<FString> Strings;
			for (const FText& Part : Parts)
			{
				if (!Part.IsEmpty())
				{
					Strings.Add(Part.ToString());
				}
			}
			// 가운뎃점 U+00B7 — NEXON cmap 실측 보유 글리프
			return FText::FromString(FString::Join(Strings, TEXT(" · ")));
		}

		bool IsBetterValue(const FQuickStartCandidate& A, const FQuickStartCandidate& B)
		{
			return A.UnitValue > B.UnitValue || (A.UnitValue == B.UnitValue && A.OrderID < B.OrderID);
		}
	}

	TArray<EDioramaModule> FacilitiesOf(const FCountryInfoTable& Row)
	{
		TArray<EDioramaModule> Facilities;
		if (Row.bSupportsFactory)
		{
			Facilities.Add(EDioramaModule::Factory);
		}
		if (Row.bSupportsMine && Row.MinableResources.Num() > 0)
		{
			Facilities.Add(EDioramaModule::Mine);
		}
		if (Row.bIsTradeHub)
		{
			Facilities.Add(EDioramaModule::Port);
		}
		return Facilities;
	}

	bool ShouldShowTabs(const TArray<EDioramaModule>& Facilities)
	{
		return Facilities.Num() >= 2;
	}

	int32 TabIndexOf(EDioramaModule Facility)
	{
		switch (Facility)
		{
		case EDioramaModule::Factory: return TabFactory;
		case EDioramaModule::Mine: return TabMine;
		case EDioramaModule::Port: return TabPort;
		default: return TabPrimary;
		}
	}

	bool FacilityForTab(int32 TabIndex, const TArray<EDioramaModule>& Facilities, EDioramaModule& OutFacility)
	{
		if (Facilities.Num() == 0)
		{
			return false;
		}
		for (const EDioramaModule Facility : Facilities)
		{
			if (TabIndexOf(Facility) == TabIndex)
			{
				OutFacility = Facility;
				return true;
			}
		}
		OutFacility = Facilities[0];
		return true;
	}

	bool IsAllOil(const TArray<EResourceType>& Resources)
	{
		if (Resources.Num() == 0)
		{
			return false;
		}
		for (const EResourceType Resource : Resources)
		{
			if (Resource != EResourceType::Oil)
			{
				return false;
			}
		}
		return true;
	}

	FText MineTabLabel(bool bAllOil)
	{
		return bAllOil
			? NSLOCTEXT("CountryDrawer", "TabOilField", "유전")
			: NSLOCTEXT("CountryDrawer", "TabMine", "광산");
	}

	FText FactoryTitle(const FText& IndustryName)
	{
		if (IndustryName.IsEmpty())
		{
			return NSLOCTEXT("CountryDrawer", "FactoryTitlePlain", "공장");
		}
		return FText::Format(NSLOCTEXT("CountryDrawer", "FactoryTitle", "{0} 공장"), IndustryName);
	}

	FText FactorySummary(int32 Slots, int32 Running, int32 Ready)
	{
		TArray<FText> Parts;
		Parts.Add(FText::Format(NSLOCTEXT("CountryDrawer", "FactorySlots", "라인 {0}개"), FText::AsNumber(Slots)));
		if (Running > 0)
		{
			Parts.Add(FText::Format(NSLOCTEXT("CountryDrawer", "FactoryRunning", "생산 중 {0}"), FText::AsNumber(Running)));
		}
		if (Ready > 0)
		{
			Parts.Add(FText::Format(NSLOCTEXT("CountryDrawer", "FactoryReady", "완료 {0}"), FText::AsNumber(Ready)));
		}
		return JoinWithDot(Parts);
	}

	FText MineHeroSummary(int32 Lines, int32 Full)
	{
		if (Lines <= 0)
		{
			return NSLOCTEXT("CountryDrawer", "MineNoLines", "채광 라인을 설치해 주세요");
		}
		TArray<FText> Parts;
		Parts.Add(FText::Format(NSLOCTEXT("CountryDrawer", "MineLines", "라인 {0}개"), FText::AsNumber(Lines)));
		if (Full > 0)
		{
			Parts.Add(FText::Format(NSLOCTEXT("CountryDrawer", "MineFull", "가득 참 {0}"), FText::AsNumber(Full)));
		}
		return JoinWithDot(Parts);
	}

	bool ShowsPageToggle(EDioramaModule Facility)
	{
		return Facility != EDioramaModule::Port;
	}

	bool ShowsProductionDot(EDrawerPage Current, int64 PendingClaim)
	{
		return Current == EDrawerPage::Upgrade && PendingClaim > 0;
	}

	float LineProgress01(int64 CurrentQty, int64 TargetQty, double UnitFraction01)
	{
		if (TargetQty <= 0)
		{
			return 0.f;
		}
		const double Done = static_cast<double>(CurrentQty) + FMath::Clamp(UnitFraction01, 0.0, 1.0);
		return static_cast<float>(FMath::Clamp(Done / static_cast<double>(TargetQty), 0.0, 1.0));
	}

	FText DurationLabel(const FTimespan& Duration)
	{
		const int64 Total = FMath::FloorToInt64(Duration.GetTotalSeconds());
		if (Total <= 0)
		{
			return FText::Format(NSLOCTEXT("CountryDrawer", "DurationSeconds", "{0}초"), FText::AsNumber(0));
		}
		const int64 Hours = Total / 3600;
		const int64 Minutes = (Total % 3600) / 60;
		const int64 Seconds = Total % 60;
		if (Hours > 0)
		{
			return Minutes > 0
				? FText::Format(NSLOCTEXT("CountryDrawer", "DurationHoursMinutes", "{0}시간 {1}분"), FText::AsNumber(Hours), FText::AsNumber(Minutes))
				: FText::Format(NSLOCTEXT("CountryDrawer", "DurationHours", "{0}시간"), FText::AsNumber(Hours));
		}
		if (Minutes > 0)
		{
			return Seconds > 0
				? FText::Format(NSLOCTEXT("CountryDrawer", "DurationMinutesSeconds", "{0}분 {1}초"), FText::AsNumber(Minutes), FText::AsNumber(Seconds))
				: FText::Format(NSLOCTEXT("CountryDrawer", "DurationMinutes", "{0}분"), FText::AsNumber(Minutes));
		}
		return FText::Format(NSLOCTEXT("CountryDrawer", "DurationSeconds", "{0}초"), FText::AsNumber(Seconds));
	}

	FText FactoryRemainingLabel(const FTimespan& Remaining)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "FactoryRemaining", "남은 {0}"), DurationLabel(Remaining));
	}

	FText MineFullEtaLabel(const FTimespan& UntilFull)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "MineFullEta", "가득까지 {0}"), DurationLabel(UntilFull));
	}

	FText CompletedStatus() { return NSLOCTEXT("CountryDrawer", "Completed", "완료"); }
	FText FullStatus() { return NSLOCTEXT("CountryDrawer", "Full", "가득 참"); }

	FText BarCountLabel(int64 Current, int64 Target)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "BarCount", "{0} / {1}개"),
			UGlobalUtilFunctions::AbbreviateNumber(Current), UGlobalUtilFunctions::AbbreviateNumber(Target));
	}

	FText EmptyLineDetail() { return NSLOCTEXT("CountryDrawer", "EmptyLineDetail", "제품을 고르면 생산을 시작합니다"); }
	FText EmptyMineDetail() { return NSLOCTEXT("CountryDrawer", "EmptyMineDetail", "캘 자원을 고릅니다"); }

	bool ShowsLockedLine(int32 CurrentSlots, int32 MaxSlots)
	{
		return CurrentSlots < MaxSlots;
	}

	FText LockedLineName(int32 Number)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "LockedLineName", "{0}번 라인"), FText::AsNumber(Number));
	}

	FText LockedLineDetail() { return NSLOCTEXT("CountryDrawer", "LockedLineDetail", "강화의 라인 증설로 열립니다"); }

	bool UpgradeSlotLocked(bool /*bMaxed*/)
	{
		return false;
	}

	FText MaxLevelLabel() { return NSLOCTEXT("CountryDrawer", "MaxLevel", "최대"); }

	FText InstantFinishTitle() { return NSLOCTEXT("CountryDrawer", "InstantFinishTitle", "즉시 완료"); }
	FText CancelLabel() { return NSLOCTEXT("CountryDrawer", "Cancel", "취소"); }

	FText InstantFinishMessage(int32 Diamonds, int64 RemainingQty, bool bMine)
	{
		return bMine
			? FText::Format(NSLOCTEXT("CountryDrawer", "InstantFinishMine", "다이아 {0}개로 남은 {1}개를 지금 바로 캡니다"),
				FText::AsNumber(Diamonds), FText::AsNumber(RemainingQty))
			: FText::Format(NSLOCTEXT("CountryDrawer", "InstantFinishFactory", "다이아 {0}개로 남은 {1}개를 지금 바로 만듭니다"),
				FText::AsNumber(Diamonds), FText::AsNumber(RemainingQty));
	}

	FText ProductionLimitBand(const FText& ResourceName, int64 Have, int32 MaxQty)
	{
		const FString Subject = KoreanJosa::Attach(ResourceName.ToString(), TEXT("가"), TEXT("이"));
		return FText::Format(NSLOCTEXT("CountryDrawer", "ProductionLimitBand", "{0} {1}개뿐이라 최대 {2}개까지 만들 수 있습니다"),
			FText::FromString(Subject), FText::AsNumber(Have), FText::AsNumber(MaxQty));
	}

	FText MineTitle(bool bAllOil, int32 LineCount)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "MineTitle", "{0} {1}곳"),
			MineTabLabel(bAllOil), FText::AsNumber(LineCount));
	}

	FText MineSummary(const TArray<FText>& ResourceNames, bool bAutoClaim, int64 Capacity)
	{
		TArray<FText> Parts;
		Parts.Add(JoinWithDot(ResourceNames));
		Parts.Add(bAutoClaim
			? NSLOCTEXT("CountryDrawer", "MineAutoClaim", "가득 차면 자동으로 받습니다")
			: FText::Format(NSLOCTEXT("CountryDrawer", "MineCapacity", "{0}개씩 보관"), FText::AsNumber(Capacity)));
		return JoinWithDot(Parts);
	}

	FText PortTitle(const FText& CountryName)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "PortTitle", "{0} 무역항"), CountryName);
	}

	FText PortSummary(float PriceMultiplier, int32 OrderCount)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 2;
		TArray<FText> Parts;
		Parts.Add(FText::Format(NSLOCTEXT("CountryDrawer", "PortPrice", "시세 ×{0}"),
			FText::AsNumber(PriceMultiplier, &Options)));
		if (OrderCount > 0)
		{
			Parts.Add(NSLOCTEXT("CountryDrawer", "PortOrdersFirst", "주문을 먼저 채웁니다"));
		}
		return JoinWithDot(Parts);
	}

	FText ClaimFactoryLabel() { return NSLOCTEXT("CountryDrawer", "ClaimFactory", "모두 받기"); }
	FText ClaimMineLabel() { return NSLOCTEXT("CountryDrawer", "ClaimMine", "모두 받기"); }
	FText SellAllLabel() { return NSLOCTEXT("CountryDrawer", "SellAll", "모두 판매"); }
	FText EmptyLineName() { return NSLOCTEXT("CountryDrawer", "EmptyLine", "빈 라인"); }
	FText StartLineLabel() { return NSLOCTEXT("CountryDrawer", "StartLine", "생산 시작"); }
	FText StartMineLabel() { return NSLOCTEXT("CountryDrawer", "StartMine", "자원 선택"); }

	EResourceType NextMineResource(const TArray<EResourceType>& Pool, const TArray<EResourceType>& Used)
	{
		for (const EResourceType Resource : Pool)
		{
			if (Resource != EResourceType::None && !Used.Contains(Resource))
			{
				return Resource;
			}
		}
		return EResourceType::None;
	}

	FText OrderName(const FText& TierLabel, const FText& ProductName)
	{
		return ProductName.IsEmpty()
			? FText::Format(NSLOCTEXT("CountryDrawer", "OrderNameNoProduct", "{0} 주문"), TierLabel)
			: FText::Format(NSLOCTEXT("CountryDrawer", "OrderName", "{0} 주문 · {1}"), TierLabel, ProductName);
	}

	FText OrderDetail(int64 Quantity, float RewardMultiplier)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 2;
		return FText::Format(NSLOCTEXT("CountryDrawer", "OrderDetail", "{0}개 납품 · 보상 ×{1}"),
			FText::AsNumber(Quantity), FText::AsNumber(RewardMultiplier, &Options));
	}

	void StyleChipCount(UCostActionButtonWidget* Button, bool bIncoming)
	{
		UResourceWidget* Chip = Button ? Cast<UResourceWidget>(Button->GetWidgetFromName(TEXT("CostWidget"))) : nullptr;
		if (Chip)
		{
			Chip->SetAmountStyleOverride(FLinearColor(FColor(0x1B, 0x23, 0x30)), bIncoming ? FString(TEXT("+")) : FString());
		}
	}

	FText OrderStatus(bool bDeliverable)
	{
		return bDeliverable
			? NSLOCTEXT("CountryDrawer", "OrderReady", "납품 가능")
			: NSLOCTEXT("CountryDrawer", "OrderShort", "재고 부족");
	}

	FText GoodsQuantity(int64 Quantity)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "GoodsQty", "{0}개"), FText::AsNumber(Quantity));
	}

	FText NoQuickStartOrder()
	{
		return NSLOCTEXT("CountryDrawer", "NoQuickStart", "제품 목록에서 주문과 재료를 확인하세요");
	}

	FText QuickStartDetail()
	{
		return NSLOCTEXT("CountryDrawer", "QuickStartDetail", "제품과 수량을 선택하세요");
	}

	FText QuickStartQuantity(int32 Quantity)
	{
		return FText::Format(NSLOCTEXT("CountryDrawer", "QuickStartQuantity", "{0}개 생산 가능"), FText::AsNumber(Quantity));
	}

	int32 PickQuickStart(const TArray<FQuickStartCandidate>& Candidates, ECompanyType LastCompany, int32 LastProjectIndex)
	{
		int32 BestLast = INDEX_NONE;
		int32 BestAny = INDEX_NONE;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FQuickStartCandidate& Candidate = Candidates[Index];
			if (Candidate.MaxProducible <= 0 || Candidate.Company == ECompanyType::None)
			{
				continue;
			}
			if (BestAny == INDEX_NONE || IsBetterValue(Candidate, Candidates[BestAny]))
			{
				BestAny = Index;
			}
			const bool bSameProduct = LastCompany != ECompanyType::None
				&& Candidate.Company == LastCompany && Candidate.ProjectIndex == LastProjectIndex;
			if (bSameProduct && (BestLast == INDEX_NONE || IsBetterValue(Candidate, Candidates[BestLast])))
			{
				BestLast = Index;
			}
		}
		return BestLast != INDEX_NONE ? BestLast : BestAny;
	}
}
