// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "UI/PitchBoardText.h"
#include "PitchTileWidget.generated.h"

class UBorder;
class UCommonTextBlock;
class UImage;
class UOverlay;
class UProgressBar;
class UTexture2D;
class UMaterialInstanceDynamic;
class UWidgetSwitcher;

// 보드가 타일 1칸에 밀어 넣는 주입 묶음.
USTRUCT()
struct FPitchTileData
{
	GENERATED_BODY()

	int32 ProjectIndex = 0;
	FText ProjectName;
	FText Combo;                 // "퍼즐 · 색채"
	TSoftObjectPtr<UTexture2D> Cover;
	FLinearColor GenreColor = FLinearColor::Gray;
	PitchBoardText::ETileVerdict Verdict = PitchBoardText::ETileVerdict::Short;
	ECompanyType CompanyType = ECompanyType::None;
	EQualityGrade ReleaseGrade = EQualityGrade::Max;
	bool bRecommended = false;
	// 출시 표시 전용. 회사별 티어 클리어 판정과 분리하며 커버와 이력 행이 함께 소비한다.
	bool bHasShipped = false;
	bool bManufacturing = false;
	bool bManufacturingResearchInProgress = false;
	bool bManufacturingQuantitySecured = false;
	// 제조 상태 띠 숫자 ― 이 건물의 확보 잔량 합 / 지금 포인트로 확보 가능한 최대 / 최초 개발 진행률(0~1).
	int32 ManufacturingSecuredQuantity = 0;
	int32 ManufacturingMaxReservable = 0;
	float ManufacturingResearchFraction = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPitchTileClicked, int32, ProjectIndex);

/**
 * 기획 보드의 가로형 프로젝트 선택 행 (UIE_PitchTile).
 * 프로젝트 이미지 + 이름/장르·소재 + 출시 이력/추천 리본.
 * 전체 타일이 버튼이라 클릭 = OnTileClicked.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPitchTileWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void Configure(const FPitchTileData& InData);

	// 선택 타일 1개만 플레이트를 액센트색으로 — 보드가 켠다
	void SetSelected(bool bInSelected);

	int32 GetProjectIndex() const { return ProjectIndex; }

	// 제품 그림(흰 배경)을 밝은 무대에 녹이는 키 재질 MID. 재질이 없으면 nullptr ― 호출부가 원본 텍스처로 폴백한다.
	static UMaterialInstanceDynamic* CreateProductCoverMID(UObject* Outer);

	UPROPERTY(BlueprintAssignable, Category = "PitchBoard")
	FOnPitchTileClicked OnTileClicked;

protected:
	virtual void NativeOnClicked() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 타일 플레이트 — 판정 워시·잠금 뮤트를 SetBrushColor 로 겸한다 (선택은 SelectedGlow 가 담당)
	UPROPERTY(meta = (BindWidget))
	UBorder* Plate;

	// 선택 테두리 — C++가 RenderOpacity로 표시 여부를 제어한다.
	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* SelectedGlow;

	UPROPERTY(meta = (BindWidget))
	UImage* CoverImage;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* Text_Combo;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_ReleaseState;

	// 상태 글자의 외곽선은 WBP가 저작하고 색만 출시 이력에 맞춘다.
	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* ReleaseStampFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* ManufacturingStatusFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* ManufacturingStatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* ManufacturingCoverFrame;

	// 제조 전용 변형(UIE_PitchTileProduct)의 폭 가득 상태 띠 ― 0 미개발 / 1 개발 중 / 2 개발 완료 / 3 수량 확보 완료.
	// 이 바인딩이 있으면 제품 타일로 보고 판 색 워시를 끈다(판 표면은 WBP 가 구운 그림 그대로).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ProductBandSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProductBandProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductBandProgressText;

	// 「확보 가능」 라벨 ― 확보 가능 수량이 0 이면 접고 값 칸에 부족 안내를 쓴다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductBandReservableLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductBandReservableText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductBandSecuredText;

	// 출시 기록이 있을 때만 표시하는 등급 한 글자 배지.
	UPROPERTY(meta = (BindWidget))
	UBorder* Badge;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Badge;

	// 추천 표시와 장식. 미저작 WBP에서는 해당 연출만 생략한다.
	UPROPERTY(meta = (BindWidgetOptional))
	UOverlay* RecommendationRoot;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* RecommendShine;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* RecommendSparkle;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* RecommendGlow;

	// 진행표 식별자 — 보드가 세팅할 때만 표시
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* Text_Index;

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor PlateBaseColor = FLinearColor(0.906f, 0.839f, 0.694f, 1.f);

	// 미출시 타일은 출시 기록과 구분되는 차분한 회청빛 크림을 쓴다.
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor PlateUnreleasedColor = FLinearColor::FromSRGBColor(FColor(0xD3, 0xDD, 0xE2));

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor DevelopedStampColor = FLinearColor::FromSRGBColor(FColor(0x38, 0x6D, 0x70));

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor UndevelopedStampColor = FLinearColor::FromSRGBColor(FColor(0x78, 0x88, 0x95));

	// 선택 = 액센트 블루 #3D9BE0 (linear)
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor PlateSelectColor = FLinearColor(0.047f, 0.328f, 0.745f, 1.f);

	// 잠금 뮤트 플레이트 (다크 슬레이트)
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor PlateMutedColor = FLinearColor(0.070f, 0.088f, 0.120f, 1.f);

	// 추천 배지 골드
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor BadgeRecommendColor = FLinearColor(0.76f, 0.40f, 0.04f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Style|Grade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GradeReflectionScale = 0.35f;

	// 판정 워시 3색 — 크림 위에 얹는 낮은 알파 오버레이 (알파 = 블렌드 강도)
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor TileTintPass = FLinearColor(0.047f, 0.185f, 0.076f, 0.16f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor TileTintShort = FLinearColor(0.72f, 0.26f, 0.24f, 0.13f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor TileTintNeutral = FLinearColor(1.f, 1.f, 1.f, 0.045f);

private:
	// 저장된 판정 + 선택으로 플레이트색 재계산 (Configure / SetSelected 공용)
	void ApplyPlateColor();
	FLinearColor ResolvePlateColor() const;
	void ApplyReleaseHistory(const FPitchTileData& InData);
	void ApplyProductBand(const FPitchTileData& InData);
	void ApplyCoverTexture(UTexture2D* CoverTex, bool bProductKey);
	void RefreshRecommendation(bool bRestartIntro);
	void ResetRecommendationVisuals();
	void UpdateRecommendationVisuals(float DeltaTime);

	PitchBoardText::ETileVerdict CurrentVerdict = PitchBoardText::ETileVerdict::Short;
	bool bTileHasShipped = false;
	bool bTileSelected = false;
	bool bRecommended = false;
	bool bRecommendationActive = false;
	bool bRecommendShineReady = false;
	bool bRecommendSparkleReady = false;
	bool bRecommendGlowReady = false;
	float RecommendationIntroElapsed = 0.f;
	float RecommendationCycleElapsed = 0.f;

	int32 ProjectIndex = 0;

	// 커버 모자이크 MID — 카드와 같은 M_UI_CoverMosaic. 타일이 프로젝트를 갈아끼워도 재사용(텍스처 파라미터만 교체).
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* CoverMID = nullptr;

	// 제품 그림의 흰 배경을 밝은 무대에 녹이는 키 재질 MID ― 제품 타일만 쓴다.
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* ProductCoverMID = nullptr;
};
