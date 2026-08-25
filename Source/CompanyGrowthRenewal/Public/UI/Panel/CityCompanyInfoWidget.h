#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "CityCompanyInfoWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UConfirmCancelWidget;
class UResourceWidget;

/**
 * 스카이라인 입주 회사 인수 확인 모달 — 견적 카드(v10).
 * ConfirmCancelWidget 을 외부 프레임으로 사용하고 ContentSlot 에 히어로 + 견적 카드를 표시.
 * UCityAcquisitionManager::OnCompanyClicked(NotAcquired 분기) 에서 PushPromptClass 로 열림.
 *
 * 표시 규약: 확률/기댓값 % 를 걷어내고 비용·회수·구간·소요시간을 전부 "돈과 시간" 으로 말한다.
 * 색 신호는 두 개뿐 — 카드 윗변 등급 라인(딜 성격) / 레드(카드 안=손해 가능, 카드 밖=자금 부족).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCityCompanyInfoWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 표시할 회사 키(FCityCompanyData.BuildingKey) 지정 — Push 직후 호출
	void ConfigureForCompany(int32 InKey);

	// M19 미션 가이드 — [인수] 확인 버튼 하이라이트 타겟 (프레임의 ConfirmButton 에 위임)
	UWidget* GetAcquireButtonWidget() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 공통 다이얼로그 프레임 (타이틀 / 확인·취소 버튼 / ContentSlot)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UConfirmCancelWidget* ConfirmCancelWidget;

	// 회사 로고 — DT CompanyIcon → 없으면 산업 글리프 → 그것도 없으면 로고 칩째 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* Image_CompanyIcon;

	// 회사 표시명 (GetDisplayName — 한/영 컬처 자동 선택)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_CompanyName;

	// 히어로 밑 업종 칩 (DT_CompanyInfo.DisplayName)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Chip_Industry;

	// 인수가 칩 (UIE_Resource_Light) — Money, AmountRoundMode=Ceil.
	// SetCanAfford 는 호출하지 않는다: 카드 안 레드는 "손해 가능" 전용이라 "못 산다" 까지 같은 색이면 둘 다 뜻을 잃는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* Resource_Cost;

	// 카드 윗변 등급색 라인 — 딜 등급의 유일한 색 신호(금액에는 칠하지 않는다)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* Image_GradeLine;

	// "예상 회수" / 리빌 후 "실제 회수"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ReturnLabel;

	// 회수 금액 숫자부 (단위는 Text_ReturnUnit 이 작게 조판)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ReturnValue;

	// 회수 금액 단위부 ("억" 등, 없으면 Collapsed)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ReturnUnit;

	// "최소 A" — 최소 회수가 인수 비용보다 작으면(손해 가능) 이 블록만 레드
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_MinPart;

	// " · 최대 B" — 이 딜의 가장 좋은 결과라 항상 잉크. 레드가 물들면 뜻이 뒤집힌다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_MaxPart;

	// "약 N에 걸쳐 들어옵니다"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_EstTime;

	// 카드 밖 보유 자금 — 부족하면 "보유 N ― M 부족" + 레드
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_Owned;

private:
	// Confirm 클릭 디스패처 — bAwaitingResultConfirm 상태에 따라 "인수 시도" 또는 "확인→관리 패널 전환" 중 하나로 분기
	void HandleAcquireConfirmed();
	// 결과 리빌 완료 → Confirm 버튼을 "확인"으로 전환+재활성(Cancel 은 계속 비활성 유지). 리빌을 못 켠 경우에도 이 경로로 합류한다.
	void HandleRevealFinished();
	void HandleCancelled();

	// 금액을 숫자부/단위부 두 블록에 나눠 쓴다 (리빌 카운트업이 매 프레임 호출)
	void ApplyReturnAmount(int64 Amount);
	// 카운트업 착지 — 라벨/색 확정 + 펀치 + 착지음
	void HandleRevealLanded();

	int32 CompanyKey = 0;

	// 리빌 완료 후 Confirm 이 "확인" 모드로 전환됐는가 — true 면 다음 Confirm 클릭은 관리 패널行
	bool bAwaitingResultConfirm = false;

	// "확인" 클릭으로 DeactivateWidget() 한 뒤 NativeOnDeactivated 가 관리 패널을 열어야 하는가
	bool bPendingManagePanelOpen = false;

	// ===== 결과 리빌 (예상 회수 → 실제 회수 카운트업) =====
	// NativeTick 은 이 구간에서만 일한다(데이터 폴링 아님) — 애니가 끝나면 더 이상 상태를 만지지 않는다.
	bool bRevealRunning = false;
	int64 RevealFromAmount = 0;
	int64 RevealToAmount = 0;
	int32 RevealPct = 0;

	// 알파(0→1)만 애니로 굴리고 실값은 int64 를 double 보간 — float 정밀도로는 거액 자릿수가 깨진다 (UResourceWidget 과 동일 패턴)
	FNumberCountUpAnimation RevealCountUp;
	FScalePunchAnimation RevealPunch;

	// 펀치 대상 = 코인+숫자+단위를 묶은 행. 개별 텍스트를 각자 스케일하면 둘 사이가 벌어진다.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> RevealPunchTarget;
};
