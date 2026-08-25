// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BoostGambleWidget.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UButtonWidget;
class UImage;
class UOfficeStageProgressManager;
struct FBoostGambleRow;

// 카드 자체 연출 상태머신 (C++ 보간 — 토스트와 같은 언어, WBP 위젯애니 미사용).
enum class EBoostCardAnim : uint8
{
	Intro,     // 등장: 아래에서 슬라이드업 + 페이드인
	Countdown, // 대기: 일반 카운트다운
	Urgent,    // 긴박: 마지막 구간 붉은 펄스
	Result,    // 결과: 성공/실패 플래시
	Dismiss    // 소멸: 축소 + 페이드아웃
};

/**
 * 산업별 개발 이벤트 — 타임드 바이너리 카드 (UI_BoostGamblePanel).
 * 개발 ~40% 지점에 뜸. 5초 카운트다운(바 줄어듦) 안에 [지른다]/[안전] 순간 결단.
 * 미선택(타임아웃)=안전. 자체 구동 — 버튼/타임아웃 시 StageMgr->ResolveBoostGamble 직접 호출.
 * v12(2026-07-23): 중앙 모달 → Office 우측 이벤트 레일(UOfficeEventRailWidget) 엔트리로 전환.
 *   스택에 push 되지 않으므로 DeactivateWidget 이 무효 → 해결 시 OnRailRemoveRequested 로 자기 제거를 요청한다.
 * v13(2026-07-24): 카드 자체 연출 3종을 내장(레일은 넣고 빼기만). 등장(슬라이드업+페이드) / 긴박(마지막 1.5s 붉은 펄스)
 *   / 결과(성공=그린 플래시·실패=레드 흔들림·안전=조용히 소멸) → 소멸 애니 → OnRailRemoveRequested. 전부 NativeTick C++ 보간.
 * 산업색은 상단 소프트 글로우(AccentGlow=Glow_Oval)+칩 도트에만.
 * 리드아웃(성공률·성공/실패 결합문구·비용줄)은 DT_BoostGamble 의 EffectType 을 읽어 타입별로 채움.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBoostGambleWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 해결 완료 → 레일에서 자기 제거 요청. 미바인딩(레일 밖 생성)이면 RemoveFromParent 폴백.
	FSimpleDelegate OnRailRemoveRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== 데이터 주입 대상 (required — DT_BoostGamble 행 기반, 매 오픈 재바인딩) =====

	// 제목 (시나리오 Title)
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* PromptText;

	// 산업 표시명 칩 (DT_CompanyInfo DisplayName, 없으면 Industry 문자열)
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* IndustryChipText;

	// 성공 확률 히어로 숫자 ("60%")
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* SuccessChanceText;

	// 성공 셀 결합 문구 (점수 +N% / 출시 -N초 / 수익 ×n)
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* WinValueText;

	// 실패 셀 결합 문구 (버그·불량·리콜 -N% / 수익 ×n)
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* LoseValueText;

	// 비용 줄 (야근수당 — TimeExtend GoCost>0 일 때만, 그 외 빈 줄). 여력 부족 시 게이트 안내로 재사용.
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* CostText;

	// 산업 색 도트 — 색 = 산업 시그니처색(C++ 주입)
	UPROPERTY(meta = (BindWidget))
	UImage* IndustryDot;

	// 상단 산업 소프트 글로우 — 브러시 Glow_Oval, 색 = 산업 시그니처색(C++ 주입)
	UPROPERTY(meta = (BindWidget))
	UImage* AccentGlow;

	// ===== 오즈 바 (v14 — 성공/실패 비율 세그, 폭=확률. Optional 이라 구 WBP 안 깨짐) =====

	// 성공 세그(초록) — C++ 가 HBox 슬롯 Fill 폭 = SuccessChance 로 구동
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* OddsSuccessSeg;

	// 실패 세그(빨강) — Fill 폭 = 1 - SuccessChance
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* OddsFailSeg;

	// 실패 확률 텍스트("실패 40%") — 실패 세그 안. (성공은 SuccessChanceText 를 "성공 60%" 로 재사용)
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* FailChanceText;

	// 결과 금액 공유 단위 헤더("전체 점수"/"수익") — 성공/실패가 명사를 공유하면 표시, 안 겹치면 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* OutcomeUnitText;

	// ===== 타이머 =====

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* CountdownText;

	// 줄어드는 카운트다운 바 — 긴장의 핵심
	UPROPERTY(meta = (BindWidget))
	UProgressBar* CountdownBar;

	// ===== 버튼 =====

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* GambleButton;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* SafeButton;

	// ===== 결과 플래시 (v13 신규 — 옵션이라 WBP 미갱신이어도 안 깨짐) =====

	// 카드 전체를 덮는 틴트(성공 그린 / 실패 레드). 평시 알파 0 · HitTestInvisible. C++ 가 색/알파 주입.
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* ResultFlash;

	// 결과 라벨("성공!"/"실패…"). 평시 숨김. C++ 가 텍스트/알파 주입.
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* ResultText;

private:
	void OnGambleClicked();
	void OnSafeClicked();
	void Resolve(bool bGamble);

	// 오즈 세그의 HorizontalBox 슬롯 Fill 폭을 확률로 설정 (0 폭 방지 클램프)
	void SetOddsSegWidth(class UWidget* Seg, float Fill);

	// 매니저 결과 델리게이트 수신 — ResolveBoostGamble 내부에서 동기 브로드캐스트되므로 Resolve 도중 채워진다.
	UFUNCTION()
	void HandleBoostGambleResolved(bool bGambled, bool bSuccess);

	// EffectType 별 리드아웃 채움 (공통 + 타입 분기). 매 오픈 호출.
	void FillReadout(const FBoostGambleRow& Row);

	// ---- 연출 상태머신 ----
	void EnterState(EBoostCardAnim NewState);
	void TickIntro(float DeltaTime);
	void TickCountdown(float DeltaTime); // Countdown/Urgent 공용 (감소·바·타임아웃), Urgent 는 펄스 얹음
	void ApplyUrgentPulse();
	void TickResult(float DeltaTime);
	void TickDismiss(float DeltaTime);
	void RequestRemoval();

	// 렌더 헬퍼 (토스트와 동일 언어)
	void SetWidgetAlpha(float Alpha);
	void SetCardTranslation(float X, float Y);
	void SetCardScale(float Scale);
	static float EaseOutQuad(float T) { return 1.0f - (1.0f - T) * (1.0f - T); }
	static float EaseInQuad(float T) { return T * T; }

	float RemainingTime = 5.0f;
	bool bResolved = false;

	// 연출 상태
	EBoostCardAnim AnimState = EBoostCardAnim::Intro;
	float AnimElapsed = 0.0f; // 현재 상태 진입 후 경과 (전환 시 0 리셋)

	// 결과 캡처 — Resolve 중 매니저 동기 브로드캐스트로 채워짐
	bool bAwaitingResult = false;
	bool bResultKnown = false;
	bool bLastGambled = false;
	bool bLastSuccess = false;
	bool bResultWin = false; // Result 진입 시 확정 (플래시 색/텍스트 결정)

	// NativeDestruct 안전정산 경로 표식 — 이 경로의 Resolve 는 정산만, 연출 진입 금지(위젯 소멸 중).
	bool bTearingDown = false;

	// AddDynamic/RemoveDynamic 대상 동일 인스턴스 보장용
	TWeakObjectPtr<UOfficeStageProgressManager> StageMgrWeak;

	// ---- 연출 튜닝 상수 (후속 튜닝 쉽게, 전부 여기서) ----
	static constexpr float CountdownSeconds = 7.0f;
	static constexpr float IntroSeconds = 0.28f;   // 등장
	static constexpr float UrgentThreshold = 1.5f; // 긴박 진입 남은시간
	static constexpr float ResultFlashSeconds = 0.5f; // 결과 플래시 유지
	static constexpr float DismissSeconds = 0.3f;  // 소멸

	static constexpr float IntroRiseY = 20.0f;     // 아래에서 +20 → 0
	static constexpr float UrgentPulseHz = 2.5f;   // 1초 2~3회
	static constexpr float UrgentScaleAmp = 0.015f; // 1.0 ↔ 1.015
	static constexpr float DismissEndScale = 0.86f; // 소멸 축소 도착 스케일
	static constexpr float ShakeHz = 9.0f;         // 실패 흔들림 진동수
	static constexpr float ShakeAmpPx = 6.0f;      // 실패 흔들림 진폭(px)
};
