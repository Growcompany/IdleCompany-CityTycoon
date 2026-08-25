// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Trade/CountrySellRowWidget.h"
#include "Manager/CountryMarketManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/TradePort.h"
#include "Table/CountryInfoTable.h"
#include "Table/CompanyInfoTable.h"
#include "Table/CountryDemandTable.h"
#include "Data/WorldMapTypes.h"
#include "Global/GlobalUtilFunctions.h"
#include "CommonButtonBase.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"

void UCountrySellRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 프리뷰: 매니저 없이 비율만으로 색상/채움 미리보기
	if (!IsDesignTime())
	{
		return;
	}
	ApplyRatioVisuals(PreviewRatio);
	if (RatioText)
	{
		const int32 PreviewCapacity = 1000;
		const int32 PreviewCurrent = FMath::RoundToInt(PreviewRatio * PreviewCapacity);
		RatioText->SetText(FText::Format(
			NSLOCTEXT("CountrySellRow", "DemandFraction", "{0} / {1}"),
			UGlobalUtilFunctions::AbbreviateNumber(PreviewCurrent, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor),
			UGlobalUtilFunctions::AbbreviateNumber(PreviewCapacity, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor)));
	}
}

void UCountrySellRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RowButton)
	{
		RowButton->OnClicked().AddUObject(this, &UCountrySellRowWidget::HandleButtonClicked);
	}
	if (SelectedBorder)
	{
		SelectedBorder->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		MarketMgr = GI->GetSubsystem<UCountryMarketManager>();
		if (MarketMgr)
		{
			MarketMgr->OnDemandChanged.AddDynamic(this, &UCountrySellRowWidget::HandleDemandChanged);
		}
	}

	// SetData 가 NativeConstruct 전에 호출됐다면 초기 상태 반영
	// Industry None 케이스 (카드 선택 없음) 도 평균 모드 적용 위해 SetData 재호출
	if (Country != ECountryType::None)
	{
		SetData(Country, Industry);  // Industry None 이면 평균, 있으면 셀 lookup
	}
}

void UCountrySellRowWidget::NativeDestruct()
{
	if (RowButton)
	{
		RowButton->OnClicked().RemoveAll(this);
	}
	if (MarketMgr)
	{
		MarketMgr->OnDemandChanged.RemoveDynamic(this, &UCountrySellRowWidget::HandleDemandChanged);
		MarketMgr = nullptr;
	}

	Super::NativeDestruct();
}

void UCountrySellRowWidget::SetData(ECountryType InCountry, ECompanyType InIndustry)
{
	Country = InCountry;
	Industry = InIndustry;

	// CountryInfo lookup — 무역 허브 캐시
	bIsTradeHub = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bOk = false;
			const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bOk);
			if (bOk)
			{
				bIsTradeHub = Info.bIsTradeHub;
			}
		}
	}

	ApplyStaticTexts();

	// Industry 미지정 (카드 선택 안 됨) — 이 국가의 모든 산업 평균 Ratio 표시.
	// 11국 상대 비교 (어디가 만수요/포화) 한눈에. EfficiencyText 는 산업 의존이라 "-" 유지.
	if (Industry == ECompanyType::None)
	{
		float TotalRatio = 0.0f;
		int32 Count = 0;
		if (MarketMgr)
		{
			const TArray<FCountryMarketState> AllStates = MarketMgr->GetAllStates();
			for (const FCountryMarketState& S : AllStates)
			{
				if (S.Country == Country)
				{
					TotalRatio += S.GetRatio();
					++Count;
				}
			}
		}
		const float AvgRatio = Count > 0 ? TotalRatio / Count : 1.0f;
		ApplyRatioVisuals(AvgRatio);
		// EfficiencyText 는 ApplyStaticTexts 에서 "-" 처리됨
		return;
	}

	RefreshFromManager();
}

void UCountrySellRowWidget::SetSelected(bool bInSelected)
{
	if (SelectedBorder)
	{
		SelectedBorder->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UCountrySellRowWidget::SetEfficiencyDelta(float DeltaPercent)
{
	if (!EfficiencyText)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Row] SetEfficiencyDelta NULLPTR — Country=%d (WBP EfficiencyText BindWidget 매칭 실패)"), (int32)Country);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[Row] SetEfficiencyDelta OK — Country=%d Delta=%.2f"), (int32)Country, DeltaPercent);

	const FString Sign = DeltaPercent >= 0.0f ? TEXT("+") : TEXT("");
	EfficiencyText->SetText(FText::FromString(FString::Printf(TEXT("%s%.0f%%"), *Sign, DeltaPercent)));

	// 양수 = 청록, 음수 = 붉은빛 (게이지 색상과 일관성)
	const FLinearColor Color = DeltaPercent >= 0.0f
		? FLinearColor(0.31f, 0.78f, 0.78f)
		: FLinearColor(0.86f, 0.31f, 0.31f);
	EfficiencyText->SetColorAndOpacity(FSlateColor(Color));
}

void UCountrySellRowWidget::RefreshFromManager()
{
	// ⚠ 로그는 Verbose — 이 위젯이 국가 상세 정보 탭(기본 탭)에 3장 상주하게 되면서
	//    매니저 1초 틱마다 Warning 이 쏟아지던 것을 격하 (Industry None 은 평균 모드라 정상 경로다).
	if (!MarketMgr || Country == ECountryType::None || Industry == ECompanyType::None)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Row] RefreshFromManager EARLY RETURN - Mgr=%d, Country=%d, Industry=%d"),
			MarketMgr ? 1 : 0, (int32)Country, (int32)Industry);
		return;
	}

	bool bFound = false;
	const FCountryMarketState State = MarketMgr->GetMarketState(Country, Industry, bFound);
	UE_LOG(LogTemp, Verbose, TEXT("[Row] RefreshFromManager Country=%d Industry=%d bFound=%d Ratio=%.2f"),
		(int32)Country, (int32)Industry, bFound ? 1 : 0, bFound ? State.GetRatio() : -1.0f);
	if (!bFound)
	{
		// 미정의 셀 — 만수요로 가정 (DemandMul 1.2× 적용 케이스)
		ApplyRatioVisuals(1.0f);
		return;
	}
	ApplyState(State);
}

void UCountrySellRowWidget::HandleButtonClicked()
{
	OnRowClicked.Broadcast(this);
}

void UCountrySellRowWidget::HandleDemandChanged(ECountryType InCountry, ECompanyType InIndustry, float NewRatio)
{
	// 다른 셀의 변경은 무시 — 33셀 fan-out에서 자기 셀만 필터
	if (InCountry != Country || InIndustry != Industry)
	{
		return;
	}
	RefreshFromManager();
}

void UCountrySellRowWidget::ApplyState(const FCountryMarketState& State)
{
	const float Ratio = State.GetRatio();
	ApplyRatioVisuals(Ratio);

	// RatioText 와 CapacityText 둘 다 같은 "Current / Capacity" 표시 — WBP 에서 어느 슬롯을 hero 로 쓸지 자유 선택.
	// ApplyRatioVisuals 가 RatioText 를 "-" 로 초기화하므로 여기서 실제 값으로 덮어쓰기.
	const FText Fraction = FText::Format(
		NSLOCTEXT("CountrySellRow", "DemandFraction", "{0} / {1}"),
		UGlobalUtilFunctions::AbbreviateNumber(State.Current, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor),
		UGlobalUtilFunctions::AbbreviateNumber(State.Capacity, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	if (RatioText)
	{
		RatioText->SetText(Fraction);
	}
	if (CapacityText)
	{
		CapacityText->SetText(Fraction);
	}
	if (RecoveryText)
	{
		RecoveryText->SetText(FText::FromString(FString::Printf(TEXT("+%s/min"),
			*UGlobalUtilFunctions::AbbreviateNumberFloat(State.RecoveryPerMin, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));
	}
}

void UCountrySellRowWidget::ApplyRatioVisuals(float Ratio)
{
	const float Clamped = FMath::Clamp(Ratio, 0.0f, 1.0f);
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Clamped);
		ProgressBar->SetFillColorAndOpacity(GetGaugeColor(Clamped));
	}
	// RatioText 는 ApplyState 에서 실제 Current/Capacity 로 덮어씀.
	// 여기서는 Current/Capacity 정보가 없는 컨텍스트 (bFound=false 셀 없음, Industry=None 평균 모드)
	// 라 "-" 로 명시 — placeholder 잔존 방지 + Loud failure.
	if (RatioText)
	{
		RatioText->SetText(FText::FromString(TEXT("-")));
	}

	// 배율은 수요 의존이라 여기서 같이 갱신 — 이 함수가 OnDemandChanged 경로에 있어 자동으로 살아 있다.
	// 색은 linear. 증가 그린 / 감소 레드, 블루는 기능색 전용이라 쓰지 않는다.
	if (SellMulText)
	{
		const float Mul = GetSellMultiplier();
		SellMulText->SetText(FText::FromString(FString::Printf(TEXT("%.2f×"), Mul)));
		SellMulText->SetColorAndOpacity(FSlateColor(Mul >= 1.0f
			? FLinearColor(0.0130f, 0.1981f, 0.0743f)    // #1E7B4D
			: FLinearColor(0.5271f, 0.0409f, 0.0241f))); // #C0392B
	}
}

void UCountrySellRowWidget::ApplyStaticTexts()
{
	// 산업 메타 DT lookup — DisplayName/AccentColor 단일 진실 원천
	FCompanyInfoTable CompanyInfo;
	bool bCompanyOk = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			CompanyInfo = TableMgr->GetCompanyInfo(Industry, bCompanyOk);
		}
	}

	// 산업 이름 — DT 단일 진실. 폴백 없음(빈 값 + 경고로 드러냄): 구 enum 폴백은 DT 누락을 가리는 데다
	// UMETA 가 에디터 전용이라 패키징 빌드에서 "Game"/"Finance" 로 나왔다.
	if (IndustryNameText)
	{
		const FText Name = bCompanyOk ? CompanyInfo.DisplayName : FText::GetEmpty();
		if (Name.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[CountrySellRow] DT_CompanyInfo DisplayName 없음 (Industry=%d)"), static_cast<int32>(Industry));
		}
		IndustryNameText->SetText(Name);
	}

	// 국가 정보 DT lookup — FlagIcon + DisplayName 단일 진실 원천
	FCountryInfoTable CountryInfo;
	bool bCountryOk = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			CountryInfo = TableMgr->GetCountryInfo(Country, bCountryOk);
		}
	}

	// 국기 아이콘 — DT_CountryInfo.FlagIcon 텍스처 로드 (11국 라디오에서 시각 구분의 핵심)
	if (IconImage)
	{
		if (bCountryOk && !CountryInfo.FlagIcon.IsNull())
		{
			if (UTexture2D* Tex = CountryInfo.FlagIcon.LoadSynchronous())
			{
				IconImage->SetBrushFromTexture(Tex);
				IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				IconImage->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// 국가 이름 — DT 단일 진실 (CountryInfoTable.DisplayName), 위에서 lookup 한 결과 재사용. 폴백 없음(산업명과 같은 이유).
	if (CountryNameText)
	{
		const FText Name = bCountryOk ? CountryInfo.DisplayName : FText::GetEmpty();
		if (Name.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[CountrySellRow] DT_CountryInfo DisplayName 없음 (Country=%d)"), static_cast<int32>(Country));
		}
		CountryNameText->SetText(Name);
	}

	// 무역 허브 배지
	if (HubBadge)
	{
		HubBadge->SetVisibility(bIsTradeHub ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 판매 컨텍스트 라벨 — (Country, Industry) 셀 단위 DT 조회. Industry None 이면 숨김.
	if (MarketRoleText)
	{
		if (Industry == ECompanyType::None)
		{
			MarketRoleText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bRoleOk = false;
				const FText Role = TableMgr->GetMarketRole(Country, Industry, bRoleOk);
				if (bRoleOk && !Role.IsEmpty())
				{
					MarketRoleText->SetText(Role);
					MarketRoleText->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
				else
				{
					MarketRoleText->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
		}
	}

	// 기준가 — DT_CountryDemand.PriceMul (산업별 정적 단가 레이어). 셀 미정의면 숨김.
	if (PriceMulText)
	{
		bool bDemandOk = false;
		FCountryDemandTable DemandRow;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				DemandRow = TableMgr->GetCountryDemand(Country, Industry, bDemandOk);
			}
		}
		if (bDemandOk)
		{
			PriceMulText->SetText(FText::FromString(FString::Printf(TEXT("%.2f×"), DemandRow.PriceMul)));
			PriceMulText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			PriceMulText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 산업 시그니처색 스와치 — 위에서 이미 lookup 한 CompanyInfo 재사용
	if (IndustryAccentImage)
	{
		IndustryAccentImage->SetColorAndOpacity(bCompanyOk ? CompanyInfo.AccentColor : FLinearColor::White);
	}

	// EfficiencyText 초기화 — Item 미선택 / TotalMoney 0 케이스에서 placeholder 잔존 방지.
	// RefreshAllEfficiencies 가 실제 값으로 덮어쓸 때까지 "-" 회색.
	if (EfficiencyText)
	{
		EfficiencyText->SetText(FText::FromString(TEXT("-")));
		EfficiencyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
	}
}

float UCountrySellRowWidget::GetSellMultiplier() const
{
	if (Country == ECountryType::None || Industry == ECompanyType::None) return 1.0f;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return 1.0f;

	float Mul = 1.0f;

	// 허브 가격 배율은 허브에만 — ComputeSell 의 bIsHub 분기와 같은 조건
	if (UTradePort* Port = GI->GetSubsystem<UTradePort>())
	{
		Mul *= Port->GetMoneyBias(Country);
		if (bIsTradeHub)
		{
			Mul *= Port->GetPortPriceMultiplier(Country);
		}
	}

	if (UCountryMarketManager* Mgr = GI->GetSubsystem<UCountryMarketManager>())
	{
		Mul *= Mgr->GetDemandMul(Country, Industry);
	}

	if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
	{
		bool bOk = false;
		const FCountryDemandTable Row = TableMgr->GetCountryDemand(Country, Industry, bOk);
		if (bOk)
		{
			Mul *= Row.PriceMul;
		}
	}

	return Mul;
}

FLinearColor UCountrySellRowWidget::GetGaugeColor(float Ratio)
{
	// 외부 호출자(CountryNameWidget 등)가 비클램프 값 넘길 수 있어 진입 시 강제 클램프
	const float Clamped = FMath::Clamp(Ratio, 0.0f, 1.0f);

	// 0.0 (포화 — 붉은빛) → 0.5 (보통 — 노랑) → 1.0 (만수요 — 청록)
	static const FLinearColor LowColor(0.86f, 0.31f, 0.31f);
	static const FLinearColor MidColor(0.92f, 0.86f, 0.39f);
	static const FLinearColor HighColor(0.31f, 0.78f, 0.78f);

	if (Clamped < 0.5f)
	{
		return FMath::Lerp(LowColor, MidColor, Clamped * 2.0f);
	}
	return FMath::Lerp(MidColor, HighColor, (Clamped - 0.5f) * 2.0f);
}
