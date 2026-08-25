#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VisitModeOverlayWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;

/**
 * 도시 방문 모드 오버레이
 * - 좌상단: 프로필 이미지 + 레벨 + 플레이어 이름
 * - 우상단: 홈(나가기) 버튼
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UVisitModeOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 방문 대상 정보 설정
	void SetVisitInfo(const FString& PlayerName, int32 HQLevel);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 플레이어 이름
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText = nullptr;

	// 플레이어 레벨
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerLevelText = nullptr;

	// 홈(나가기) 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> HomeBtn = nullptr;

	// 프로필 이미지
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ProfileImage = nullptr;

private:
	UFUNCTION()
	void OnHomeBtnClicked();
};
