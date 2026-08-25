#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "CityCompanyManageWidget.generated.h"

class UTextBlock;
class UImage;
class UBorder;
class UOverlay;
class UCanvasPanel;
class URadialProgressWidget;
class UButton;
class UButtonWidget;
class UCloseButtonWidget;
class UCityAcquisitionManager;
class UCoinFlyoutContainerWidget;
class UTexture2D;

/**
 * 스카이라인 입주 회사 관리 패널 (우측 도킹).
 * UCityAcquisitionManager::OnCompanyClicked(Milking/Depleted) 에서 PushBottomClass 로 열림.
 * 다크 에디토리얼 크롬 + 라디얼 링(회수율, 상태색 블루→앰버→골드) + 3칩(인수가/누적/잔여)
 * + 초당 드립/ETA/본전 돌파 배지 + 드립 코인 연출(로고→누적 칩→레이어 머니바 착지) + 철거 버튼.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCityCompanyManageWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Push 직후 호출 — 회사 키 세팅 + UI 채우기
	void ConfigureForCompany(int32 InKey);

	// M20 미션 가이드 — [철거] 버튼 하이라이트 타겟
	UWidget* GetDemolishButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	// 회수율 부드러운 채움 — TargetPct 로 DisplayedPct 를 매 프레임 lerp + 누적 칩 펀치 스케일
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 로고 칩 아이콘 (FCityCompanyData::CompanyIcon 로드). 없으면 Collapsed.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* Image_CompanyIcon;

	// 헤더 회사명 타이틀 (plain UTextBlock — WBP에서 폰트 직접 지정)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Text_CompanyName;

	// "초당 +N 회수 중" 보조 라인 (Depleted 면 "회수 완료")
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Text_DripRate;

	// 라디얼 게이지 위젯 (earned / total %)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	URadialProgressWidget* Ring_Remaining;

	// 링 중앙 퍼센트 텍스트 ("60%")
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* Text_Percent;

	// "회수 완료까지 MM:SS" — 잔여÷초당 기대 드립. Depleted 면 Collapsed.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Text_Eta;

	// 본전 돌파 골드 배지 (회수율 >= 100% 에서만 Visible)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* PaidoffBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Text_Paidoff;

	// 인수가 칩 값
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* Text_Cost;

	// 누적 회수 칩 값 (액센트 색으로 표시)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* Text_Earned;

	// 잔여 칩 값
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* Text_Remaining;

	// 드립 코인 시작 앵커 (회사 로고 오버레이 중심)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UOverlay* LogoOverlay;

	// 드립 코인 도착 앵커 (누적 회수 칩 중심)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* Chip2Border;

	// 드립 코인 연출 레이어 — 루트 최상단 풀스트레치 (팝업 위 그려지는 AddToViewport 금지 → 패널 내부 오버레이)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCanvasPanel* EffectCanvas;

	// 철거 버튼 — 캐시아웃이라 언제든 활성. 회수 중이면 확인 모달을 거친다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Button_Demolish;

	// [보석] 남은 회수 즉시 완료 — 공용 부품 UIE_UpgradeBtn1(UCostActionButtonWidget) 재사용.
	// 가격 칩 조판 · afford 자동 체크 · 부족 시 입력 삼킴+셰이크+부족액 토스트 · 전환 펀치가 이미 들어 있어
	// 여기서는 SetCost 한 줄과 표시 토글만 한다. Milking 에서만 노출(고갈이면 살 게 없다).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UCostActionButtonWidget* Button_SkipRecovery;

	// 닫기 X 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCloseButtonWidget* UIE_CloseButton;

	// 배경 딤 버튼 — 패널 밖(딤) 클릭 시 닫기. plain UButton (ProductSellModal/BuildModal 딤 패턴과 동일).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

private:
	void HandleDemolish();

	// 회수 중 철거 — 되돌릴 수 없고 남은 회수를 버리므로 한 번 되묻는다 (Depleted 는 되물을 게 없어 건너뜀)
	void ShowDemolishConfirm();
	void HandleDemolishConfirmed();

	// [보석] 즉시 완료 — 재화를 쓰는 행동이라 한 번 확인받는다
	void HandleSkipRecovery();
	void HandleSkipConfirmed();

	UFUNCTION()
	void HandleCloseClicked();

	// 배경 딤 클릭 → 닫기 (닫기 X 버튼과 동일 동작). UButton::OnClicked(다이나믹 델리게이트)용 UFUNCTION.
	UFUNCTION()
	void HandleBackgroundClicked();

	// 진행도 목표 갱신(불변 인수가/철거버튼/드립레이트는 즉시) — 최초 1회 + 드립 이벤트마다 재호출. 링/숫자는 NativeTick 이 보간.
	void RefreshFromProgress();

	// DisplayedPct 로 링/퍼센트/누적/잔여/ETA/본전 배지/링 상태색을 실제 위젯에 반영(NativeTick + 오픈 스냅에서 호출).
	void ApplyDisplay(float Pct);

	// 매니저 진행도 이벤트 핸들러 — 현재 패널 키와 일치할 때만 갱신. Earned 증가분은 코인 연출로.
	void HandleProgressChanged(int32 ChangedKey);

	// 드립 코인 1개 발사 (로고→누적 칩). 컨테이너/코인 텍스처는 첫 사용 시 lazy 생성·캐시.
	void SpawnDripCoinFx(int64 Delta);

	// 코인 착지 — 누적 칩 펀치 + 레이어 머니바 착지 연출 (누적 델타 일괄 정산)
	void HandleStreamCoinArrived();

	int32 CompanyKey = 0;

	// 이 패널이 카메라 가림 고스트의 주인인지 (등록은 여는 쪽 OpenManagePanel 이 대신 건다).
	// 세션 시작(NativeConstruct)에만 리셋 — 풀 재사용 인스턴스가 이전 회사로 고스트를 켜지 않게 한다
	bool bOwnsFocusTarget = false;

	// 부드러운 회수율 애니메이션 — 데이터(1초 드립)는 Target 만 갱신, NativeTick 이 Displayed 를 lerp 로 추적
	float DisplayedPct = 0.f;
	float TargetPct = 0.f;
	int64 CachedTotal = 0;   // Earned + Remaining (회사당 불변 총 회수액)
	int64 CachedCost = 0;    // 인수가(불변)

	// 드립 표시/연출 상태
	int64 ExpectedPerSec = 0;       // 초당 기대 회수액 (Chunk × 확률)
	bool bDepletedCached = false;
	int64 LastEarnedForFx = 0;      // 코인 연출용 Earned 델타 기준점
	int64 PendingLandingDelta = 0;  // 스폰 스로틀로 코인이 생략돼도 착지 시 일괄 정산되도록 누적

	// 상주 드립 코인 컨테이너 (EffectCanvas 자식, 첫 사용 시 생성)
	UPROPERTY()
	UCoinFlyoutContainerWidget* DripCoinFlyout = nullptr;

	UPROPERTY()
	UTexture2D* CachedCoinTexture = nullptr;

	// 누적 회수 값 펀치 (코인 착지 시)
	FScalePunchAnimation EarnedPunch;
};
