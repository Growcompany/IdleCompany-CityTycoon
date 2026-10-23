// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/WidgetType.h"
#include "UI/Element/Common/CountdownText.h"
#include "MenuPanelWidget.generated.h"

class UButton;
class UPanelHeaderWidget;
class UMenuTileWidget;
class UCommonTextBlock;

/**
 * 메뉴 시트 — 3그룹 9타일 + 설정 필
 * - 타일 잠금 = DT_MenuUnlock (RowName = 타일 이름)
 * - 레드닷 = 출석/미션/우편/라이브옵스/게임시계 5 매니저 델리게이트
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMenuPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	// 튜토리얼 M31 메뉴 소개 — 닫기(헤더 X) + PanelIntro "MenuPanel" 앵커 타일
	UWidget* GetCloseButtonWidget() const;
	UWidget* GetAttendanceTileWidget() const;
	UWidget* GetMailboxTileWidget() const;
	UWidget* GetQuestTileWidget() const;

	// PanelIntro "MenuPanel" — 자동 재생은 ShouldPlay 가 막으므로 튜토리얼이 명시적으로 부른다(본 적 있어도 재생). 재생 중이거나 시작되면 true
	bool PlayPanelIntro();
	bool IsPanelIntroPlaying() const { return PanelIntroOverlay.IsValid(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> AttendanceTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> QuestTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> EventTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> MailboxTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> AchievementTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> CodexTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> NoticeTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> RankingTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> ProfileTile = nullptr;

	// 4번째 그룹 "도전" — 사이드 컨텐츠 공통 축 2칸 (스카이라인 · 콜라보 개발). 사내 활동 허브 타일은 2026-09-25 폐기.
	// 표시명은 DT/WBP 문자열이고 식별자(Fusion*)는 그대로 둔다 — 코드 키와 표시명은 갈라도 된다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> SkylineTile = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UMenuTileWidget> FusionTile = nullptr;


	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CountdownText = nullptr;

	// 하단 설정 필의 히트 영역 (BackgroundBtn 과 같은 투명 버튼 패턴)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> SettingsBtn = nullptr;

private:
	UFUNCTION()
	void OnBackgroundClicked();

	UFUNCTION()
	void OnCancelButtonClicked();

	UFUNCTION()
	void OnSettingsButtonClicked();

	void OpenAfterClose(EWidgetType Widget, FName Tab = NAME_None);
	void BindTile(UMenuTileWidget* Tile, EWidgetType Widget, FName Tab);
	void RefreshLocks();
	void RefreshBadges();
	void HandleDayKey(const FString& NewDayKey);
	void ClosePanel();

	// 타일 → (열 위젯, 탭 힌트). 타일 자체는 BindWidget 이 잡고 있어 GC 대상이 아니다
	TArray<TPair<UMenuTileWidget*, TPair<EWidgetType, FName>>> TileRoutes;

	// 닫기 애니메이션 후 열 위젯 (None 이면 없음)
	EWidgetType PendingOpenWidget = EWidgetType::None;
	FName PendingTab = NAME_None;

	FCountdownTicker Countdown;

	void HandlePanelIntroFinished();
	TWeakObjectPtr<class UPanelIntroOverlayWidget> PanelIntroOverlay;
};
