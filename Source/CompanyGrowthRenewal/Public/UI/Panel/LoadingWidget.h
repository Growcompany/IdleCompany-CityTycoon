// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingWidget.generated.h"

class UProgressBar;
class UCommonTextBlock;
class UImage;

/**
 * 첫 부팅 로딩 화면.
 * 전용 LoadingMap 위에서 ALoadingGameMode 가 생성/표시한다.
 * MainMap 패키지를 비동기 로드하면서 진행률(실제+시간 floor 하이브리드)을
 * 단계 상태 텍스트("도시 깨우는 중… 42%") + 화면 최하단 헤어라인 진행선으로 표시하고,
 * 완료 시 페이드아웃 후 OpenLevel(MainMap) 로 전환한다. (self-clocking — NativeTick 구동)
 * 리소스 추가 다운로드용 Mode B UI(DownloadBox)는 평소 Collapsed — 원격 다운로드 도입 시 사용.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 부팅 시퀀스 시작 (NativeConstruct 에서 자동 호출, 중복 호출 안전)
	void BeginBootSequence();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ---- BindWidget ----
	// 진행 상태 텍스트 — 단계 메시지 + 퍼센트 ("도시 깨우는 중… 42%")
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> StatusText;

	// 화면 최하단 풀폭 헤어라인 진행선 (~4px)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HairlineBar;

	// 풀스크린 배경 (아트 준비 후 ResourceObject 스왑) — 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	// 앰비언트 구름 2장 (배경 위·로고 뒤 z-order, C++ 이 우→좌 드리프트) — 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CloudA;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CloudB;

	// ---- Mode B: 리소스 추가 다운로드 UI (평소 Collapsed) ----
	// 원격 다운로드 도입 시 "데이터 다운로드중 (32MB/123MB)" 로 사용.
	// 로컬 패키지 로드에 가짜 MB 를 표기하지 말 것 — 실제 다운로드가 생길 때만 노출.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DownloadBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DownloadBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DownloadText;

	// ---- 튜너블 (WBP CDO 에서 조정) ----
	// 전환 대상 레벨 패키지 경로 (데이터 주도)
	UPROPERTY(EditAnywhere, Category = "Loading")
	FString TargetLevelPackagePath = TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity");

	// 최소 표시 시간(초) — 바가 이 시간에 걸쳐 차도록 페이스 고정 (깜빡임 방지)
	UPROPERTY(EditAnywhere, Category = "Loading")
	float MinDisplayTime = 2.0f;

	// 표시 퍼센트 보간 속도(초당, 0~1 스케일) — 실제 % 점프를 부드럽게
	UPROPERTY(EditAnywhere, Category = "Loading")
	float SmoothSpeed = 2.0f;

	// 페이드아웃 시간(초). 0 이면 즉시 전환.
	UPROPERTY(EditAnywhere, Category = "Loading")
	float FadeOutDuration = 0.4f;

	// 비동기 로드 콜백 미발화/실패 대비 강제 전환 타임아웃(초) — 영구 hang 방지
	UPROPERTY(EditAnywhere, Category = "Loading")
	float LoadTimeout = 30.0f;

private:
	void StartTargetLevelLoad();
	void BeginTransition();
	void UpdateProgressUI();

	// ---- 앰비언트 새 (하늘 실루엣 — 에셋 없이 NativePaint 라인 드로잉) ----
	// 로고(좌상단)와 안 겹치게 x 0.30 이하로는 안 들어옴. 우→좌 드리프트 + 날갯짓.
	struct FAmbientBird
	{
		float PosX = 0.f;        // 화면폭 비율
		float BaseY = 0.f;       // 화면높이 비율
		float SpeedX = 0.f;      // 비율/초 (음수 = 우→좌)
		float WingSpan = 10.f;   // 로컬 px (작을수록 먼 새)
		float FlapPhase = 0.f;
		float FlapSpeed = 7.f;
		float DriftPhase = 0.f;
	};
	TArray<FAmbientBird> Birds;
	void ResetBird(FAmbientBird& Bird, bool bInitialScatter);
	void TickAmbientBirds(float InDeltaTime);

	float CloudTime = 0.0f;
	void TickClouds(float InDeltaTime);

	// ---- 바람 스트릭 (앰비언트 — 각진 사인 궤적의 흰 획이 이따금 슥 지나감) ----
	struct FWindStreak
	{
		float HeadX = 0.f;       // 진행 선두 (로컬 px, 우→좌)
		float BaseY = 0.f;
		float Speed = 420.f;     // px/s
		float Length = 260.f;
		float WavePhase = 0.f;
		float WaveAmp = 8.f;
		float Cooldown = 2.f;    // 비활성 시 다음 발사 대기(초)
		bool bActive = false;
	};
	TArray<FWindStreak> Streaks;
	void TickWindStreaks(float InDeltaTime);

	FName TargetPackageFName;   // GetAsyncLoadPercentage / OpenLevel 용 (풀 패키지 경로)

	float ElapsedTime = 0.0f;
	float DisplayedPercent = 0.0f;
	float FadeAlpha = 1.0f;

	bool bBootStarted = false;
	bool bTargetLoaded = false;
	bool bFading = false;
	bool bTransitionStarted = false;
};
