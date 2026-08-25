#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/ProjectBoardData.h"
#include "OfficeRequestCardWidget.generated.h"

class UCommonTextBlock;
class UButtonWidget;
class UBorder;
class UImage;
class UMaterialInstanceDynamic;
class UProgressBar;
class UWrapBox;
class UWidget;
class UTexture2D;
class UTeamPipItemWidget;

// 카드 선택 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectCardSelected, int32, SlotIndex);

// 미달 카드의 처방 경로 — 최저 직능 슬롯을 함께 넘겨 받는 쪽이 곧바로 그 직능을 가리킬 수 있게 한다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardViewEmployees, int32, WorstSlot);

/**
 * 기획 보드 기획안 카드 — 커버 웰 + 전폭 판정 밴드 + 접힘 상세(6칸 + 내 팀 줄) + 수익 한 줄
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeRequestCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetSlotData(const FProjectBoardSlot& SlotData, int32 InSlotIndex);

	// 추천 강조 토글 — 보드가 카드 1장에 매 렌더 명시 세팅한다
	void SetRecommended(bool bInRecommended);

	// 이미 개발한 프로젝트의 기록 등급 (빈 문자열 = 미개발) — 밴드 제목이 「재개발 가능」으로 갈린다
	void SetDevelopedGrade(const FString& InGradeLetter);

	// 내 팀 줄 입력 = 직능 슬롯별 보유 점수 합 6칸. 비어 있으면 줄 자체를 접는다
	void SetTeamDisciplinePoints(const TArray<int32>& TotalsBySlot);

	void SetExpanded(bool bInExpanded);
	bool IsExpanded() const { return bExpanded; }

	// 튜토리얼 앵커용 — 지금 실제로 보이는 CTA (통과=AcceptButton / 미달=AcceptButtonSecondary / 대기=ViewEmployeesButton)
	UWidget* GetPrimaryCtaWidget() const;

	UPROPERTY(BlueprintAssignable)
	FOnProjectCardSelected OnCardSelected;

	UPROPERTY(BlueprintAssignable)
	FOnCardViewEmployees OnViewEmployeesRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 장르 · 소재 라인
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_Tagline;

	// ── 판정 밴드 (전폭) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* Band;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Img_BandGlyph;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_BandTitle;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_BandSub;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* BandRightGrade;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_BandGrade;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* BandRightShort;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_BandShortName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_BandShortPct;

	// ── 접힘/펼침 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* MoreButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* DetailSection;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWrapBox* TeamWrap;

	// ── 현재 팀 예상 — 고정 축 6칸 미니 바 (펼침 영역) ──
	// 칸 순서 = EProductionDiscipline(Plan/Dev/Graphics/Sound/Server/QA) 고정. 축이 타일 전환마다 같아야 실루엣으로 비교할 수 있다
	// (정렬순으로 두면 축이 기획안마다 달라져 비교가 성립 안 함). 스타일은 WBP baked, C++는 예상 달성률과 의미색만 주입.
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style|Color")
	FLinearColor OutlookInactiveColor = FLinearColor(0.03f, 0.012f, 0.003f, 0.40f);

	// ── 밴드 색/글리프 ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style|Color")
	FLinearColor BandPassColor = FLinearColor(0.22f, 0.58f, 0.40f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style|Color")
	FLinearColor BandFailColor = FLinearColor(0.72f, 0.26f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style|Color")
	FLinearColor BandWaitColor = FLinearColor(0.10f, 0.14f, 0.20f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Style|Texture")
	TSoftObjectPtr<UTexture2D> GlyphCheck;

	UPROPERTY(EditAnywhere, Category = "Style|Texture")
	TSoftObjectPtr<UTexture2D> GlyphExclaim;

	// 선택 버튼 ("개발 시작")
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* AcceptButton;

	// 미달 카드의 회색 2차 CTA ("그래도 개발") — AcceptButton 과 배타
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* AcceptButtonSecondary;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* ViewEmployeesButton;

	// 프로젝트명 (피치: 웰 아래 대표 타이틀)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ProjectName;

	// 커버 뒤 폴백 면 — C++가 장르 딥색으로 SetBrushColor. 커버가 있으면 CoverImage에 완전히 가려진다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* WellFill;

	// 프로젝트 커버 이미지 — 있으면 모자이크 커버 표시, 없으면 WellFill 장르색 폴백
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* CoverImage;

	// 예상 수익 · 운영 한 줄
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* Text_Economy;

	// ── 추천 강조 (티어1 초보 가이드) ──
	// 보드 전체를 비교해야만 나오는 파생 표시 상태 — 슬롯 데이터가 아니라 보드가 별도 주입한다.

	// 골드 보더 글린트 오버레이 (MI_UI_BorderGlint_Recommend — 부모는 대칭 링 계열 M_UI_ChipLine.
	// 이름의 "BorderGlint"와 달리 M_UI_BorderGlint 계열이 아니다. 스타일은 WBP baked)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Img_RecommendGlint;

	// 커버 웰 상단 "★ 추천" 리본 (WBP baked)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* RecommendRibbon;

private:
	// 커버 모자이크 MID — 카드가 행을 갈아끼워도 재사용(텍스처 파라미터만 교체)
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* CoverMID = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTeamPipItemWidget>> TeamItems;

	int32 SlotIndex = 0;
	FProjectBoardSlot CachedSlotData;
	bool bHasCachedData = false;

	bool bRecommended = false;
	bool bExpanded = false;

	FString DevelopedGrade;
	TArray<int32> TeamPointsBySlot;

	// 「직원 보기」가 가리킬 직능 = 마지막 표현에서 계산된 최저 달성률 슬롯
	int32 CachedWorstSlot = INDEX_NONE;

	void ApplyRecommendedVisual();

	void ApplySlotData();

	// 피치 표현 — 커버 웰 + 판정 밴드 + 6칸 전망 + 수익 한 줄
	void ApplyPitchPresentation(const FProjectBoardSlot& SlotData);

	// 요구 직능(가중치 > 0)만 핍 항목으로 생성
	void RebuildTeamLine(const int32 W[6]);

	void ApplyExpanded();

	void OnAcceptClicked();

	void OnMoreClicked();

	void OnViewEmployeesClicked();
};
