// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/ItemType.h"
#include "Enum/LootBoxRarity.h"
#include "Fonts/SlateFontInfo.h"
#include "PotentialResetWorkspaceWidget.generated.h"

class UBorder;
class UImage;
class UCommonTextBlock;
class UButtonWidget;
class UEmployeeManager;
class UItemInventoryManager;
class UPotentialTierUpOverlayWidget;
struct FEmployeeInstance;

/**
 * 직원창 PotentialHost 안의 잠재능력 작업 영역.
 * 명함 선택·상승 확률·재설정·리롤 FX 를 소유한다.
 * 직원 능력치와 명함 재고 변경을 구독하고, 표시를 중단하면 진행 중인 연출을 취소한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPotentialResetWorkspaceWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Configure(int32 InEmployeeID);
	void Suspend();
	EItemType GetSelectedCard() const { return SelectedCard; }

	DECLARE_DELEGATE_OneParam(FOnRequestOddsTable, int32);
	FOnRequestOddsTable OnRequestOddsTable;
	FSimpleDelegate OnRequestShop;

#if !UE_BUILD_SHIPPING
	// 캡처 하네스 전용 — 지금 표시 중인 직원으로 리롤 연출을 Time 초 시점에 세운다. 데이터·사운드는 건드리지 않는다
	void DebugSeekRerollFx(bool bTierUp, float Time, int32 ForcedTopLineMask = -1);
	void DebugEndRerollFx();
	// 모든 연출 층이 기본 변형·투명으로 돌아왔는지 — 중단 뒤 잔상 검사용
	bool DebugIsRerollFxAtRest(FString& OutDetail) const;
#endif

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget)) UBorder* TierChipBorder;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* TierText;

	UPROPERTY(meta = (BindWidget)) UBorder* PotLine0;
	UPROPERTY(meta = (BindWidget)) UBorder* PotLine1;
	UPROPERTY(meta = (BindWidget)) UBorder* PotLine2;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PotLineName0;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PotLineName1;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PotLineName2;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PotLineValue0;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PotLineValue1;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PotLineValue2;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* PotLineBG0;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* PotLineBG1;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* PotLineBG2;

	// 명함 타일 3장 — 판 테두리가 선택을 말하고, 그림·이름·보유 수·상승 확률을 한 판에 담는다. 판 전체가 터치 영역이다.
	UPROPERTY(meta = (BindWidget)) UBorder* CardTile0;
	UPROPERTY(meta = (BindWidget)) UBorder* CardTile1;
	UPROPERTY(meta = (BindWidget)) UBorder* CardTile2;
	UPROPERTY(meta = (BindWidget)) UImage* CardTileBG0;
	UPROPERTY(meta = (BindWidget)) UImage* CardTileBG1;
	UPROPERTY(meta = (BindWidget)) UImage* CardTileBG2;
	UPROPERTY(meta = (BindWidget)) UImage* CardArt0;
	UPROPERTY(meta = (BindWidget)) UImage* CardArt1;
	UPROPERTY(meta = (BindWidget)) UImage* CardArt2;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardName0;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardName1;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardName2;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardCountText0;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardCountText1;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardCountText2;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardCountUnit0;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardCountUnit1;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CardCountUnit2;
	// 명함별 상승 확률 칩 — 아래 등급 상승 줄이 고른 명함의 확률을 말하는 변형(사무실 직원 패널)은 두지 않는다
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* CardChanceChip0;
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* CardChanceChip1;
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* CardChanceChip2;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* CardChanceArrow0;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* CardChanceArrow1;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* CardChanceArrow2;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* CardChanceText0;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* CardChanceText1;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* CardChanceText2;
	UPROPERTY(meta = (BindWidget)) UWidget* CardCheck0;
	UPROPERTY(meta = (BindWidget)) UWidget* CardCheck1;
	UPROPERTY(meta = (BindWidget)) UWidget* CardCheck2;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* CardTileButton0;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* CardTileButton1;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* CardTileButton2;

	// 등급 상승 — 라벨, 현재→다음 등급 칩, 큰 확률 수치. 오를 수 없으면 칩 자리에 사유 문장이 선다.
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* OddsLabelText;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* OddsValueText;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* OddsSubText;
	UPROPERTY(meta = (BindWidget)) UWidget* OddsGradeRow;
	UPROPERTY(meta = (BindWidget)) UBorder* OddsFromChip;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* OddsFromText;
	UPROPERTY(meta = (BindWidget)) UBorder* OddsToChip;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* OddsToText;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* OddsWellBG;
	// 라벨과 사유 문장을 한 줄에 둘 때의 구분점 — 사유 문장과 함께 보이고 접힌다
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* OddsSubSeparator;

	// 오를 수 있을 때 등급 칩 앞 라벨. 한 줄 변형은 「등급 상승」, 확률을 칩 아래에 따로 세우는 직원창은 「등급 상승 확률」
	UPROPERTY(EditDefaultsOnly, Category = "Potential|Style")
	FText RiseLabelText = NSLOCTEXT("PotentialResetWorkspace", "RiseLabel", "등급 상승 확률");

	// 0장 명함 수량을 빨강 대신 흐린 잉크로 — 0장은 그림 딤과 「구매」 버튼이 이미 말한다
	UPROPERTY(EditDefaultsOnly, Category = "Potential|Style")
	bool bMuteEmptyCardCount = false;

	// 재설정 — 라벨과 소비 명함 칩은 버튼 위 오버레이다. 고른 명함이 0장이면 같은 버튼이 그 명함 구매로 바뀐다.
	UPROPERTY(meta = (BindWidget)) UButtonWidget* ResetButton;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* ResetLabelText;
	UPROPERTY(meta = (BindWidget)) UWidget* ResetCostChip;
	UPROPERTY(meta = (BindWidget)) UImage* ResetCostIcon;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* ResetCostText;
	UPROPERTY(meta = (BindWidget)) UWidget* ResetShopIcon;
	UPROPERTY(meta = (BindWidgetOptional)) UButtonWidget* OddsButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButtonWidget* ShopButton;

	// 리롤 연출 층 — 판 전체 위에 깔린 캔버스(소비량 "-1"). 줄별 층(PotLineGlow/Shine/SparkA/SparkB/Row{i})은 이름 계약으로 모은다.
	// 등급 상승의 큰 연출은 판이 아니라 화면 전체(UPotentialTierUpOverlayWidget)가 맡는다.
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* RerollFxCanvas;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* RerollCostFloat;

private:
	// 줄 하나의 표시값 — 리롤 직전 화면을 들고 있다가 줄이 빠지는 동안 보여 준다
	struct FPotLineView
	{
		FText Name;
		FText Value;
		FLinearColor NameInk = FLinearColor::White;
		FLinearColor ValueInk = FLinearColor::White;
		FLinearColor Grade = FLinearColor::White;
		bool bActive = false;
		bool bLocked = false;
	};

	static constexpr EItemType CardTypes[3] = { EItemType::BusinessCardPaper, EItemType::BusinessCardGold, EItemType::BusinessCardBlack };

	int32 EmployeeID = INDEX_NONE;
	EItemType SelectedCard = EItemType::BusinessCardPaper;
	bool bPresentationActive = false;
	uint32 PresentationGeneration = 0;

	UPROPERTY() UEmployeeManager* EmployeeManager = nullptr;
	UPROPERTY() UItemInventoryManager* ItemManager = nullptr;

	// 리롤 연출 — < 0 = 쉬는 중. 줄 다시 뽑기(A)·고점 줄 강조(B)가 이 시계 하나를 공유한다
	float RerollFxElapsed = -1.f;
	bool bRerollUpgraded = false;
	bool bRerollFxFrozen = false;
	bool bRerollFxSilent = false;
	// 줄이 다시 뽑히기 시작하는 시각 — 등급이 오르면 화면 전체 연출이 걷힐 때까지 미뤄 둔다
	float RerollReleaseTime = 0.f;
	int32 RerollCardIndex = INDEX_NONE;
	int32 RerollLineCount = 0;
	uint8 RerollTopLineMask = 0;
	bool RerollLineSwapped[3] = { true, true, true };
	FPotLineView RerollOldLines[3];
	FLinearColor RerollLineGrade[3];
	// 빛줄기·번짐·반짝이 색 — 판 테두리(RerollLineGrade)보다 진한 등급색
	FLinearColor RerollLineInk[3];

	// 판 규격(모서리·테두리·립 px)을 고르는 상태 — UpdatePlateMaterialSizes 가 실제 크기로 환산해 재질에 넣는다
	bool LinePlateActive[3] = { true, true, true };
	bool TilePlateSelected[3] = { false, false, false };

	// 줄 값의 WBP 숫자 글꼴 — 값이 문구일 때 이름 글꼴로 바꿨다가 숫자로 돌아올 때 쓴다
	FSlateFontInfo LineValueNumberFont;

	// 서버 응답을 오래 기다리면 버튼을 눌린 채로 보인다 — 짧은 응답에서 깜빡이지 않게 지연 뒤에만
	float RerollWaitElapsed = -1.f;

	TWeakObjectPtr<UWidget> LineRows[3];
	// 잠긴 줄 자물쇠(PotLineLock{i}) — 그리는 WBP 만 둔다
	TWeakObjectPtr<UWidget> LineLocks[3];
	TWeakObjectPtr<UImage> LineGlows[3];
	TWeakObjectPtr<UImage> LineShines[3];
	TWeakObjectPtr<UImage> LineSparks[3][2];
	TWeakObjectPtr<UPotentialTierUpOverlayWidget> TierUpOverlay;

	// 리롤은 연타 경로다 — 서버 응답 전 두 번째 요청을 막는다(매니저 pending 이 거절하면 "명함 부족"이 거짓말이 된다)
	bool bRerollRequestInFlight = false;

	FEmployeeInstance* GetEmployee() const;
	void RefreshAll();
	void RefreshTier(const FEmployeeInstance* Employee);
	void RefreshLines(const FEmployeeInstance* Employee);
	void RefreshCards(const FEmployeeInstance* Employee);
	void RefreshOddsAndAction(const FEmployeeInstance* Employee);
	FPotLineView MakeLineView(int32 Index, const FEmployeeInstance* Employee) const;
	void ApplyLineView(int32 Index, const FPotLineView& View);
	void ApplyPlateStyle(int32 Index, const FLinearColor& Grade, bool bActive, float OutlineAlpha = .45f);
	void ApplyTileStyle(int32 Index, bool bSelected);
	void UpdatePlateMaterialSizes();
	void CollectRerollFxLayers();
	void ResetRerollFxVisuals();

	// 리롤 성공 응답 뒤 연출 — 등급 상승 판정에 요청 직전 등급이 필요하다
	void PlayRerollFx(ELootBoxRarity PreRarity, EItemType UsedCard);
	void StartRerollFx(bool bUpgraded, ELootBoxRarity PreRarity, ELootBoxRarity NewRarity, EItemType UsedCard);
	// 시각 Time 의 모습을 그린다 — 실시간 틱과 하네스 시크가 같은 함수를 탄다
	void ApplyRerollFx(float Time);
	void ApplyRerollLines(float Time, float LineStart);
	bool ShowTierUpOverlay(ELootBoxRarity From, ELootBoxRarity To);
	void CloseTierUpOverlay();
	void HandleTierUpRelease();
	// Widget 안의 정규 좌표 Point(0~1) 를 연출 캔버스 좌표로
	bool GetFxLocalPoint(const UWidget* Widget, const FVector2D& Point, FVector2D& OutLocal) const;
	float GetRerollFxEnd() const;

	void HandleCardClicked(int32 CardIndex);
	void HandleResetClicked();
	void HandleOddsClicked();
	void HandleShopClicked();
	void HandleStatsChanged(int32 ChangedEmployeeID);
	void HandleItemChanged(EItemType Type, int32 NewCount, int32 Delta);

	// 경제 잠금/해제 — 해제 시 서버값으로 정정된 명함 재고로 카드·확률·CTA 재계산
	void HandleEconomyReady(bool bReady);
};
