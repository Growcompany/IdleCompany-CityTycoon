// UCommonButtonBase 상속으로 탭/클릭 감지 자동 처리

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Layout/SlateRect.h"
#include "Enum/CountryType.h"
#include "Enum/CompanyType.h"
#include "Enum/CountryMapState.h"
#include "Enum/MedallionAnchor.h"
#include "UI/Element/WorldMap/MapIndustryTagWidget.h"
#include "CountryNameWidget.generated.h"

class UImage;
class USizeBox;
class UTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCommonTextBlock;
class UCountryMarketManager;
class UHorizontalBox;
struct FDioramaStillInfo;

DECLARE_DELEGATE_OneParam(FOnCountryFlagClicked, ECountryType /*CountryType*/);

/**
 * 전략 지도 판 위의 국가 메달리온 버튼 (원반 88px + 링 + 라벨).
 * 시각 규격 = `docs/05_UI/UI_STYLE_CATALOG.md` §14.
 *
 * 잠금 상태 지원: SetLockState(true) 호출 시
 *  - LockImage / LockText 노출 (원반 면은 그대로 ― 틴트하면 그라데이션이 검어진다)
 *  - 클릭은 살려 두고 삼킨 뒤 OnFlagRejected 발화
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCountryNameWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void SetCountryType(ECountryType InType);

	// 잠금 상태 토글 — 어두운 틴트 + 잠금 아이콘 + 비활성화
	UFUNCTION(BlueprintCallable, Category = "Country")
	void SetLockState(bool bInLocked, const FText& InLockReason = FText::GetEmpty());

	// ── 전략 지도 테이블 메달리온 (2026-09-24) ──
	/** 라벨 윗줄 국가명. 출처는 `DT_CountryInfo.DisplayName` ― 코드에 표기 분기를 만들지 말 것. */
	UFUNCTION(BlueprintCallable, Category = "Country|Medallion")
	void SetCountryName(const FText& InName);

	UFUNCTION(BlueprintCallable, Category = "Country|Medallion")
	void SetStatusLine(const FText& InStatus);

	UFUNCTION(BlueprintCallable, Category = "Country|Medallion")
	void SetRingColor(const FLinearColor& InColor);

	/** 포커스한 국가에 바깥 선택 링을 표시한다. 핀마다 별도 Tick은 만들지 않는다. */
	void SetFocusHighlight(bool bHighlight);
	void UpdateFocusHighlight(float DeltaTime);

	/** 그려지지 않아 캐시 Geometry가 낡아도 유효한, 슬롯/DesiredSize 기준 원반+라벨 로컬 경계. */
	bool GetFocusVisualBounds(FSlateRect& OutBounds) const;

	// 방향을 저장하고 곧바로 StatusPlate 의 캔버스 슬롯(앵커·정렬·오프셋)에 옮긴다.
	void SetAnchorSide(EMedallionAnchor InSide);

	// 슬롯에 실린 방향과 저장값이 같은지 보는 검증 입구. 화면 판정은 라벨 사각형 교차가 한다.
	EMedallionAnchor GetAnchorSide() const { return AnchorSide; }

	UFUNCTION(BlueprintCallable, Category = "Country|Medallion")
	void SetAlertVisible(bool bVisible);

	/**
	 * 이름 아래 담당 산업 칸(UIE_MapIndustryTag). 칸이 하나라도 있으면 상태 줄·수요 행을 접고,
	 * 빈 목록이면 이름표 줄을 접고 상태 줄·수요 행으로 돌아간다. 같은 목록이면 다시 만들지 않는다.
	 */
	void SetIndustryTags(const TArray<FMapIndustryTagSpec>& Specs);
	bool IsTagMode() const { return bTagMode; }
	int32 GetTagCount() const { return TagWidgets.Num(); }

	/** 앵커 위로 솟은 그림 높이. 지도 말풍선 줄을 이 위에 얹는다. */
	float GetHeightAboveAnchor() const;

	// 5.8px/m 로 찍은 지도 그림을 판 위에서는 2.9px/m 로 띄운다.
	static constexpr float kDioMiniatureDisplayScale = 0.5f;

	/**
	 * 원반 대신 그 나라 시설 지도 그림을 띄운다. 원반 부품은 접히고, 라벨판은 방향과 무관하게 그림 아래로,
	 * 위젯 desired 크기는 그림 크기가 된다(그림 모서리를 눌러도 버튼이 받는다). 그림·재질·바인딩 중 하나라도 없으면 원반으로 돌아간다.
	 */
	void SetMiniature(UTexture* Texture, const FDioramaStillInfo& Info, UMaterialInterface* RTMaterial);
	void ClearMiniature();
	bool HasMiniature() const { return bHasMiniature; }

	/** 캔버스 슬롯 정렬로 쓸 값. 원반은 중심, 그림은 desired 사각형 안 땅 조각 중심의 비율 위치. */
	FVector2D GetAnchorAlignment() const;

	/**
	 * 링 색. 진출국만 넘겨받은 산업 시그니처 색을 쓰고, 나머지 둘은 중립색이다 ―
	 * 잠긴 국가에 특산을 미리 흘리지 않는다. 산업색은 DT(FCompanyInfoTable::AccentColor)가 출처라
	 * 호출부가 읽어 넘긴다. 이 함수 안에 산업별 분기를 다시 만들지 말 것.
	 */
	static FLinearColor ResolveRingColor(ECountryMapState State, const FLinearColor& SpecialtyAccent);

	/** 채광 전담국 링(브라스). DT 에 "채광" 산업 행이 없어 AccentColor 로 읽어올 출처가 없다 ― 그래서 이 한 색만 코드에 남는다. */
	static FLinearColor MiningRingColor();

	/** 무역 허브 링(청록). 채광 전담국과 같은 이유다 ― 읽어올 산업 행 자체가 없는 국가다. */
	static FLinearColor TradeHubRingColor();

	/**
	 * 영역 덮개 알파. 마스크가 없어도 0 을 돌려주지 않는다 ― 잠김이 밝게 보이는 것을 막는다.
	 * 상태를 링 색과 같은 축(`ECountryMapState`)으로 받는다 ― 한 국가의 상태를 두 표현으로 들고 있으면 갈린다.
	 */
	static float ResolveRegionShadeAlpha(ECountryMapState State, bool bMaskValid);

	FOnCountryFlagClicked OnFlagClicked;

	// 잠긴 핀 탭. 셰이크와 사유 토스트는 층(WorldMapLayer)이 맡는다 - 핀마다 NativeTick 을 새로 달지 않기 위해.
	FOnCountryFlagClicked OnFlagRejected;

protected:
	virtual void NativeOnClicked() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> BGImage;

	// 원반(88px) 안 2글자 국가 코드. 국기 텍스처가 DT 에 없는 국가도 식별되게 한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CodeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LockText;

	// 메달리온 링 / 상태 줄 — 데이터 주입 대상이라 required. 없으면 조용한 null 대신 컴파일에서 터진다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RingImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> StatusText;

	// 라벨 윗줄 국가명(목업 `.tag` 2단의 앞부분). 데이터 주입 대상이라 required.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText;

	/**
	 * 상태 줄을 감싼 라벨 묶음(판 + 글자). LabelSide 가 실제로 움직이는 대상이라 required 다 ―
	 * Optional 로 두면 방향이 조용히 죽고 11개 라벨이 전부 같은 쪽에 쌓인다(그게 직전 증상이었다).
	 * 원반 캔버스의 자식이어야 하며(슬롯이 UCanvasPanelSlot), 판 크기는 글자가 정한다.
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> StatusPlate;

	// 손이 필요한 국가에만 뜨는 금색 도트. 판 위 장식이라 없어도 메달리온은 성립한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AlertDot;

	/** 수요 글리프 3개를 담는 라벨판 내부 가로 행. StatusPlate와 함께 이동한다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> DotRow;

	// 산업별 수요 닷 (3종 fixed). DT_CountryDemand 미정의 셀과 미가용 업종은 Collapsed.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DotSemiconductor;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DotElectronics;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DotAutomobile;

	/** 원반 가장자리와 국가명·상태·수요 행을 담는 라벨판 사이의 간격. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Medallion")
	float LabelGap = 10.0f;

	// 진출국 지도 그림. 원반 캔버스(MedallionRoot)의 자식이어야 한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> MiniatureImage;

	// WBP 루트의 88px 고정 상자. 버튼 히트 영역이 이 크기라 그림 모드에서만 그림 크기로 바꾼다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<USizeBox> MedallionBox;

	// LabelBox 안 NameText 바로 아래 가로 줄. 담당 산업 칸이 런타임에 채워진다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> TagRow;

	// 이름표 칸 사이 간격(시안 `.tags{gap:14px}`).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Medallion")
	float TagGap = 14.0f;

private:
	ECountryType CountryType = ECountryType::None;
	bool bLocked = false;

	// 서수 0 = Bottom 이라 DT 미기입 행도 "아래"로 떨어진다.
	EMedallionAnchor AnchorSide = EMedallionAnchor::Bottom;

	// 마지막으로 주입된 상태색. 링과 코드가 같은 값을 써야 해서 위젯 밖에 한 벌만 들고 있는다.
	FLinearColor RingColor = FLinearColor::White;
	// CommonButton의 임시 자동 선택과 부모가 확정한 포커스 표시를 구분한다.
	bool bFocusHighlightRequested = false;
	float FocusHighlightElapsed = 0.0f;

	UPROPERTY()
	TObjectPtr<UCountryMarketManager> MarketMgr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MiniatureMID;

	UPROPERTY()
	TObjectPtr<UTexture> MiniatureTexture;

	bool bHasMiniature = false;

	bool bTagMode = false;
	TArray<FMapIndustryTagSpec> AppliedTagSpecs;

	UPROPERTY()
	TArray<TObjectPtr<UMapIndustryTagWidget>> TagWidgets;

	// 이름표 모드에서 접었다가 되돌릴 수요 행의 WBP 저작 가시성.
	ESlateVisibility DotRowAuthoredVisibility = ESlateVisibility::SelfHitTestInvisible;
	bool bDotRowVisibilitySaved = false;
	bool bTagRowMissingWarned = false;

	// 이름표 모드에 따라 상태 줄·수요 행·이름표 줄 가시성을 한 곳에서 정한다.
	void ApplyLabelMode();
	// 그림 모드의 표시 크기와 그 안의 땅 조각 중심(둘 다 판 위 px).
	FVector2D MiniatureDisplaySize = FVector2D::ZeroVector;
	FVector2D MiniaturePivot = FVector2D::ZeroVector;

	// 그림 모드로 들어가기 직전의 WBP 저작값. 접은 부품과 옮긴 도트를 그대로 되돌리려고 들고 있는다.
	struct FDiscLayout
	{
		ESlateVisibility BGVisibility = ESlateVisibility::HitTestInvisible;
		ESlateVisibility RingVisibility = ESlateVisibility::HitTestInvisible;
		bool bBoxWidthOverride = false;
		bool bBoxHeightOverride = false;
		float BoxWidth = 0.0f;
		float BoxHeight = 0.0f;
		FVector2D AlertAnchorMin = FVector2D(0.5, 0.5);
		FVector2D AlertAnchorMax = FVector2D(0.5, 0.5);
		FVector2D AlertAlignment = FVector2D(0.5, 0.5);
		FVector2D AlertPosition = FVector2D::ZeroVector;
	};
	FDiscLayout SavedDiscLayout;

	void SaveDiscLayout();
	void ApplyMiniatureLayout();

	// 저장된 방향을 슬롯에 싣는다. 앵커는 원반의 어느 변인지, 정렬은 라벨의 어느 점을 그 변에 댈지를 뜻해
	// 라벨 크기를 몰라도 방향이 정해진다 ― 그래서 글자 길이가 바뀌어도 간격이 흔들리지 않는다.
	void ApplyAnchorSide();

	// 링과 코드 글자에 같은 색을 싣는다. NativeConstruct 에서도 부르는 이유는 CommonTextBlock 의
	// SynchronizeProperties→UpdateFromStyle 이 스타일의 흰색으로 색을 무조건 되덮기 때문이다.
	void ApplyRingColor();

	void RefreshAllDots();
	void RefreshDot(UImage* Dot, ECompanyType Industry);

	UFUNCTION()
	void HandleDemandChanged(ECountryType InCountry, ECompanyType InIndustry, float NewRatio);
};
