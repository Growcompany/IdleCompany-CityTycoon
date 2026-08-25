// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ConfirmCancelWidget.generated.h"

class UCommonTextBlock;
class UButtonWidget;
class UNamedSlot;
class UCommonButtonStyle;

DECLARE_MULTICAST_DELEGATE(FOnConfirmDelegate);
DECLARE_MULTICAST_DELEGATE(FOnCancelDelegate);

/**
 * 확인/취소 대화상자 위젯
 * - 공통 프레임(배경, 제목, 버튼)을 제공하고 ContentSlot으로 내용 교체 가능
 * - 단독 사용: MessageText에 텍스트 표시 (기존 호환)
 * - 컴포넌트 사용: ContentSlot에 커스텀 UI 삽입 (LaunchConfirm, ProjectReport 등)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UConfirmCancelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
    FOnConfirmDelegate OnConfirm;
    FOnCancelDelegate OnCancel;

    // 메시지 텍스트 설정 (단독 사용 시 MessageText가 있을 때만 동작)
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void SetMessage(const FText& Message);

    // 제목 텍스트 설정
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void SetTitle(const FText& Title);

    /**
     * 제목/버튼 라벨을 WBP Default* 값으로 되돌린다.
     * Default* 는 NativePreConstruct 에서만 적용되는데 프롬프트 스택 풀 재사용 시 PreConstruct 는 다시 돌지 않아,
     * 상황별로 라벨을 갈아끼운 소비자는 다음 오픈에 그 라벨이 그대로 남는다. 재사용 진입점에서 이걸 먼저 호출할 것.
     */
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void RestoreDefaultTexts();

    // 확인만 있는 모드 설정 (Alert 모드)
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void SetConfirmOnly(bool bConfirmOnly);

    // 취소(닫기)만 있는 모드 설정 (ConfirmButton 숨김)
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void SetCancelOnly(bool bCancelOnly);

    // 확인 버튼 텍스트 설정
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void SetConfirmButtonText(const FText& Text);

    // 취소 버튼 텍스트 설정
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void SetCancelButtonText(const FText& Text);

    // MessageText 숨기기 (ContentSlot에 커스텀 UI를 넣을 때 호출)
    UFUNCTION(BlueprintCallable, Category = "Dialog")
    void HideMessageText();

    // M10 LaunchConfirm [출시] 확인 버튼 — 미션 가이드 하이라이트 타겟
    UWidget* GetConfirmButtonWidget() const;

    // 리빌 등 진행 중 중복 입력 차단용 — Confirm 과 대칭으로 취소도 잠글 수 있어야 함
    UWidget* GetCancelButtonWidget() const;

    // 에디터에서 설정 가능한 기본 제목 (NativePreConstruct에서 적용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
    FText DefaultTitle = NSLOCTEXT("ConfirmCancel", "DefaultTitle", "제목");

    // 에디터에서 설정 가능한 확인 버튼 텍스트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
    FText DefaultConfirmText = NSLOCTEXT("ConfirmCancel", "Confirm", "확인");

    // 에디터에서 설정 가능한 취소 버튼 텍스트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
    FText DefaultCancelText = NSLOCTEXT("ConfirmCancel", "Cancel", "취소");

    // 컴포넌트로 사용 시 false로 설정하면 버튼 클릭 시 자동 RemoveFromParent 안 함
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
    bool bAutoRemove = true;

    // 확인 버튼 CommonStyle 오버라이드 (설정 시 기본 스타일 대체)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Style")
    TSubclassOf<UCommonButtonStyle> ConfirmButtonStyle;

    // 취소 버튼 CommonStyle 오버라이드 (설정 시 기본 스타일 대체)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Style")
    TSubclassOf<UCommonButtonStyle> CancelButtonStyle;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UCommonTextBlock* TitleText;

    // 단독 사용 시 텍스트 표시 (ContentSlot 안에 배치하면 단독/컴포넌트 둘 다 지원)
    UPROPERTY(meta = (BindWidgetOptional))
    UCommonTextBlock* MessageText;

    // 커스텀 콘텐츠 삽입용 슬롯 (WBP에서 NamedSlot 위젯으로 배치)
    UPROPERTY(meta = (BindWidgetOptional))
    UNamedSlot* ContentSlot;

    UPROPERTY(meta = (BindWidget))
    UButtonWidget* ConfirmButton;

    UPROPERTY(meta = (BindWidget))
    UButtonWidget* CancelButton;

    UFUNCTION()
    void OnConfirmButtonClicked();

    UFUNCTION()
    void OnCancelButtonClicked();
};
