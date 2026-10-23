// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Enum/WidgetType.h"
#include "UI/Panel/TitleSkylineLoadingWidget.h"
#include "LoadingPreviewGameMode.generated.h"

class ULoadingWidget;

USTRUCT()
struct FLoadingPreviewEntry
{
	GENERATED_BODY()

	// 화면 왼쪽 위 안내 줄에 찍히는 이름 (엔진 디버그 글꼴이라 영문)
	UPROPERTY(EditAnywhere, Category = "Loading Preview")
	FString Label;

	UPROPERTY(EditAnywhere, Category = "Loading Preview")
	EWidgetType WidgetType = EWidgetType::Loading;

	// 스카이라인 변형(UTitleSkylineLoadingWidget)일 때만 쓰인다
	UPROPERTY(EditAnywhere, Category = "Loading Preview")
	ETitleLogoVariant LogoVariant = ETitleLogoVariant::Final;
};

/**
 * 첫 부팅 로딩 화면 비교용 GameMode — LoadingMap_Preview 전용.
 * 실제 부팅(LoadingMap → ALoadingGameMode → DT_WidgetClass Loading 행)은 건드리지 않는다.
 * 고른 로딩 위젯의 전환 대상을 이 맵으로 바꿔, 로딩이 끝나면 같은 화면이 처음부터 다시 돈다.
 * 숫자키 1~N = 종류 선택, ←/→ = 이전/다음, R·Space = 다시 재생.
 * 콘솔: CGR.LoadingPreview.Select <번호>, CGR.LoadingPreview.Replay
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ALoadingPreviewGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALoadingPreviewGameMode();

	// 0부터 센다. 범위를 벗어나면 양 끝으로 맞춘다.
	void SelectEntry(int32 Index);
	void StepEntry(int32 Delta);
	void Replay();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Loading Preview")
	TArray<FLoadingPreviewEntry> Entries;

	// 한 바퀴 길이(초). 로딩 위젯의 최소 표시 시간을 이 값으로 늘려 연출 뒤 화면을 볼 시간을 준다.
	UPROPERTY(EditAnywhere, Category = "Loading Preview", meta = (ClampMin = "1"))
	float HoldSeconds = 6.f;

private:
	void ShowSelected();
	void BindPreviewKeys();
	void ShowHint() const;

	UPROPERTY()
	TObjectPtr<ULoadingWidget> Shown;
};
