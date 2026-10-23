#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/ProjectReportData.h"
#include "ProjectReportWidget.generated.h"

class UCommonTextBlock;
class UDisciplineBarWidget;
class UIconCardWidget;
class UConfirmCancelWidget;
class UBorder;
class UImage;
class UOverlay;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCloseReportDelegate);

/**
 * 프로젝트 결산서 위젯
 * ConfirmCancelWidget을 프레임으로 사용하고 ContentSlot에 결산 데이터 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProjectReportWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * 결산 데이터 설정
	 * @param InData 결산서 데이터
	 */
	UFUNCTION(BlueprintCallable, Category = "Project Report")
	void SetReportData(const FProjectReportData& InData);

	UPROPERTY(BlueprintAssignable, Category = "Project Report|Events")
	FOnCloseReportDelegate OnReportClosed;

	// 튜토리얼이 운영 결산 창을 닫게 할 때 짚는 버튼 — 결산은 [닫기]만 두는 경우가 있어 보이는 쪽을 고른다
	UWidget* GetCloseTargetWidget() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 공통 다이얼로그 프레임 (ConfirmCancelWidget 인스턴스)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UConfirmCancelWidget* ConfirmCancelWidget;

	// 프로젝트 이름
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ProjectName;

	// 품질 등급 텍스트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_QualityGrade;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* LetterPlate;

	// 등급 스탬프 3층 — SDF 머티리얼. RoundedBox 는 단색뿐이라 그라데이션/글린트가 안 나온다.
	// StampBG=세로 그라데이션 채움 / StampLine=키라인 / StampGlint=흐르는 광택
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampBG;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampLine;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampGlint;

	// 등장 연출은 이 Overlay 하나에만 건다 — 7층(BG/Line/Glint/Shade/Sheen/Shadow/StampRow)에 따로 걸면 스탬프가 분해된다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UOverlay* GradeStampStack;

	// 결산서의 수익 배율 텍스트 (WBP 바인딩 이름 유지)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_QualityScore;

	// 프로젝트 이미지 카드
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconCardWidget* UI_ProjectImageCard;

	// 분야별 성과 — 고정 축 6칸(EProductionDiscipline 슬롯 = 칸 인덱스). 비활성 분야는 빈 트랙.
	// 쌍둥이 출시확인 패널(ULaunchConfirmWidget)과 같은 축·같은 부품이라 두 화면의 실루엣이 이어진다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar6;

	// 기본 수익 (초당)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_RevenuePerSecond;

	// 총 수익 (실제 벌어들인 금액)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_TotalRevenue;

	// 획득 시가총액 (프로젝트 완료 보상)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_MarketCapGained;

	// 총 운영 시간
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_OperationTime;

private:
	// 남아 있는 스탬프 SDF 층에 등급색 주입
	void ApplyGradeStampColor(const FLinearColor& GradeColor);

	// SDF 는 Wpx/Hpx 가 실제 위젯 크기와 같아야 코너가 정합. 위젯을 고정하는 대신 크기를 주입한다
	// (UResourceWidget::UpdateChipMaterialSize 와 같은 패턴 — 크기 변화 시에만)
	void UpdateStampMaterialSize();
	FVector2D LastStampMatSize = FVector2D::ZeroVector;

	// 등급 스탬프 등장 팝 (UAlertMarkWidget 과 같은 EaseOutQuad 패턴)
	static constexpr float StampRevealDelay = 0.05f;
	static constexpr float StampRevealDuration = 0.22f;
	static constexpr float StampRevealStartScale = 0.90f;
	float StampRevealElapsed = -1.f;   // 음수 = 유휴, 틱에서 조기 반환

	void TickStampReveal(float InDeltaTime);

	void OnDialogCancelled();

	FProjectReportData ReportData;
};
