// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "CatchRingWidget.generated.h"

class UButton;
class AOfficeworker;

// 캐치 성공(파열 시작) 통지 — 연타 감쇠 사운드는 링이 아니라 OfficeLayer 가 단일 누산기로 처리한다.
// 페이즈를 실어 보낸다 — 수신 측에서 다시 물으면 ApplyTapRelief 로 이미 None 이라 힌트 졸업 키를 못 고른다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCatchRingCaught, EFatigueSlackPhase);

// 농땡이(슬랙 텔레그래프) 직원 머리 위 "지금 톡 쳐!" 펄스 링 — 주황->빨강 맥동(scale+alpha).
// 비주얼은 자기 로컬 공간 NativePaint 드로잉(절대좌표 왕복 없음 = 페인트 공간 버그 무관), 배치는 OfficeLayer 가 캔버스 슬롯으로.
// 클릭 = 투명 루트 버튼 -> 앵커 직원의 BehaviorComponent->ApplyTapRelief(). 무인 런타임 위젯이라 WBP/DT 등록 없이 StaticClass 직접 생성
// (플레이북 '디자이너 비관여 = C++ 자가 트리' 분기, SelectionChevron 선례).
UCLASS()
class COMPANYGROWTHRENEWAL_API UCatchRingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이 링이 대표하는 직원(클릭 시 이 직원을 깨움). 약참조 — 직원 소멸/상태 변화 시 OfficeLayer 가 풀에서 회수.
	void SetWorker(AOfficeworker* InWorker);
	AOfficeworker* GetWorker() const { return Worker.Get(); }

	// 파열 잔상 재생 중 — 앵커 직원이 이미 캐치 해제돼도 OfficeLayer 는 이 링을 회수하지 않고 점유 유지.
	bool IsBurstActive() const;

	FOnCatchRingCaught OnCaught;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UFUNCTION()
	void HandleRingClicked();

	// 펄스 위상 누산기 (NativeTick 갱신, NativePaint 는 읽기만)
	float PulseElapsed = 0.0f;

	// 파열 잔상 상태 (NativeTick 갱신, NativePaint 는 읽기만). bBursting 은 이중 탭 가드도 겸한다.
	bool bBursting = false;
	float BurstElapsed = 0.0f;

	// 자가 트리 루트 안의 투명 클릭 버튼 (전체 영역 히트)
	UPROPERTY()
	UButton* HitButton = nullptr;

	TWeakObjectPtr<AOfficeworker> Worker;
};
