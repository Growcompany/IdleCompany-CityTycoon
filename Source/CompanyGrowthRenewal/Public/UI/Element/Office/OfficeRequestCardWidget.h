#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "Data/ProjectBoardData.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "OfficeRequestCardWidget.generated.h"

class UCommonTextBlock;
class UButtonWidget;
class UBorder;
class UImage;
class UProgressBar;
class UMaterialInstanceDynamic;
class UWidget;
class UTexture2D;
class UCommonButtonStyle;
class UStatRowWidget;
class UItemTooltipWidget;

// 카드 선택 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectCardSelected, int32, SlotIndex);

/**
 * 기획 보드 기획안 카드 — 커버 웰 + 상시 6칸 전망 + 수익/운영 스탯행 + 단일 CTA(색으로 판정)
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UOfficeRequestCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetSlotData(const FProjectBoardSlot& SlotData, int32 InSlotIndex, bool bInHasShipped);

	// 추천 강조 토글 — 보드가 카드 1장에 매 렌더 명시 세팅한다
	void SetRecommended(bool bInRecommended);

	void SetReleaseGrade(ECompanyType InCompanyType, EQualityGrade InGrade);

	// 6칸 전망의 값 = 직능 슬롯별 팀 보유 점수 합(6칸). 매 렌더 보드가 주입한다
	void SetTeamDisciplinePoints(const TArray<int32>& TotalsBySlot);

	// 튜토리얼 앵커용 — 지금 실제로 보이는 CTA (착수 가능=AcceptButton / 대기=CTA 없음)
	UWidget* GetPrimaryCtaWidget() const;
	// CTA 가 미달 경고(「그래도 개발」)로 떠 있는가 ― 튜토리얼 문구가 버튼 글자와 같은 말을 하게
	bool IsPrimaryCtaGateRisk() const { return bCtaGateRisk; }

	UPROPERTY(BlueprintAssignable)
	FOnProjectCardSelected OnCardSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 장르 · 소재 라인
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_Tagline;

	// 미달 사유 캡션 (예: "그래픽이 부족합니다") — 미달일 때만 Visible, 그 외 Collapsed.
	// 크림 플레이트 위 레드(#C4463E)라 Style 슬롯 비우고 Font+잉크 직접(§1.2 baked 흰색 실종 방지).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ShortageReason;

	// ── 상시 6칸 전망 (카드 본문) ──
	// 칸 순서 = EProductionDiscipline(기획/개발/그래픽/사운드/서버/QA) 고정 — 축이 기획안마다 같아야 실루엣으로 비교된다.
	// 판정 밴드를 없앤 뒤 카드의 항상 보이는 주 콘텐츠. 스타일은 WBP baked, C++는 팀 점수·예상 달성률·의미색만 주입.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* DevPlate;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* DevBar1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* DevBar2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* DevBar3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* DevBar4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* DevBar5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* DevBar6;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevVal1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevVal2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevVal3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevVal4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevVal5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevVal6;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevName1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevName2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevName3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevName4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevName5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_DevName6;

	// 6칸 색은 미달/통과 2개뿐 — 중간 앰버는 "부족한데 빨갛지 않다"는 회색지대를 만들어 폐기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style|Color")
	FLinearColor OutlookShortColor = FLinearColor(0.72f, 0.26f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style|Color")
	FLinearColor OutlookPassColor = FLinearColor(0.22f, 0.58f, 0.40f, 1.0f);

	// 비요구(가중치 0)·전망 없음 칸의 딤 색 — 축은 유지하되 이 카드가 안 쓰는 분야임을 흐리게 보인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style|Color")
	FLinearColor OutlookInactiveColor = FLinearColor(0.03f, 0.012f, 0.003f, 0.40f);

	// 착수 CTA — 통과=초록「개발 시작」, 미달=회색「그래도 개발」로 라벨+스타일만 갈아끼운다(한 버튼 3상태)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* AcceptButton;

	// AcceptButton 상태 스타일 — 색은 스타일 에셋이 소유, 전환만 코드(WBP Class Defaults 로 덮어쓰기 가능)
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSoftClassPtr<UCommonButtonStyle> AcceptStyle_Pass = TSoftClassPtr<UCommonButtonStyle>(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Gold.CUI_Style2_Btn_Gold_C")));

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSoftClassPtr<UCommonButtonStyle> AcceptStyle_Fail = TSoftClassPtr<UCommonButtonStyle>(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Gray.CUI_Style2_Btn_Gray_C")));

	// ── 수익/운영 스탯행 (UIE_StatRow) — 이름은 WBP DefaultStatName, C++는 값만 주입 ──
	// WBP 기본 Collapsed 라 표시 시점에 Visible 로 올린다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Text_OperationTime;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Text_RevenueRange;

	// 프로젝트명 (피치: 웰 아래 대표 타이틀)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ProjectName;

	// 커버 뒤 폴백 면 — C++가 장르 딥색으로 SetBrushColor. 커버가 있으면 CoverImage에 완전히 가려진다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* WellFill;

	// 프로젝트 커버 이미지 — 있으면 모자이크 커버 표시, 없으면 WellFill 장르색 폴백
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* CoverImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* ReleaseGradeHost;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* ReleaseGradePlate;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ReleaseGrade;

	// ── 추천 강조 (티어1 초보 가이드) ──
	// 보드 전체를 비교해야만 나오는 파생 표시 상태 — 슬롯 데이터가 아니라 보드가 별도 주입한다.

	// 골드 보더 글린트 오버레이 (WBP baked). 추천일 때만 노출
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* Img_RecommendGlint;

	// 커버 웰 상단 "★ 추천" 리본 (WBP baked). 추천일 때만 노출
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* RecommendBadge;

	// 구 난이도 배지 — 현재 미사용. 바인딩돼 있으면 접어 숨긴다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* DiffBadge;

private:
	bool bCtaGateRisk = false;

	// 커버 모자이크 MID — 카드가 행을 갈아끼워도 재사용(텍스처 파라미터만 교체)
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* CoverMID = nullptr;

	int32 SlotIndex = 0;
	FProjectBoardSlot CachedSlotData;
	bool bHasCachedData = false;

	bool bRecommended = false;

	ECompanyType ReleaseCompanyType = ECompanyType::None;
	EQualityGrade ReleaseGrade = EQualityGrade::Max;
	bool bHasShipped = false;

	// 6칸 전망의 값 = 직능 슬롯별 팀 보유 점수 합
	TArray<int32> TeamPointsBySlot;

	// 직능별 예상/목표/달성률 한 벌 — 6칸과 CTA 판정이 전부 이 한 계산에서 읽어 서로 어긋나지 않는다
	struct FDisciplineOutlook
	{
		int32 Weights[6] = { 0, 0, 0, 0, 0, 0 };
		TArray<float> Targets;           // 착수 시 만들어질 직능 목표 점수
		TArray<float> Scores;            // 현재 팀 예상 점수
		TArray<FLinearColor> Colors;     // 슬롯별 통과/미달 칩색
		bool bOutlookUsable = false;
	};

	// 6칸 값 계산 캐시 — 렌더가 % 값 + 툴팁 문자열을 이 한 벌에서 뽑는다
	FDisciplineOutlook CachedOutlook;

	// 6칸 탭 시 뜨는 팝오버 툴팁(UIE_ItemTooltip 재사용) + 렌더가 만들어 둔 직능별 예상/요구 설명
	UPROPERTY(Transient)
	UItemTooltipWidget* ActiveTooltip = nullptr;
	FText DisciplineTooltipDesc;

	void ApplyRecommendedVisual();

	void ApplySlotData();

	// 피치 표현 — 커버 웰 + 상시 6칸 전망 + 수익/운영 스탯행 + 단일 CTA
	void ApplyPitchPresentation(const FProjectBoardSlot& SlotData);

	// 직능 산식 단일 출처 — 목표/예상/달성률/색을 한 번만 계산한다
	void ComputeDisciplineOutlook(const FProjectBoardSlot& SlotData, FDisciplineOutlook& Out) const;

	// 한 칸의 값 텍스트 = 기준 대비 달성률(%). CachedOutlook 을 읽는다.
	FText FormatOutlookCellValue(int32 SlotIdx) const;

	// 6칸 플레이트 탭 → 직능별 예상/요구 팝오버 툴팁(UIE_ItemTooltip)을 누른 지점에 띄운다
	UFUNCTION()
	FEventReply HandleDevPlateClicked(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	// AcceptButton 스타일 스왑 — 통과/미달 스타일 에셋을 로드해 SetStyle
	void ApplyAcceptStyle(const TSoftClassPtr<UCommonButtonStyle>& StylePtr);

	void OnAcceptClicked();
};
