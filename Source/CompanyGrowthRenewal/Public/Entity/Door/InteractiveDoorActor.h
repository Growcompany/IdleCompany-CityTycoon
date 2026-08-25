// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "InteractiveDoorActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoorStateChanged);

/**
 * 문 액터 C++ 베이스 클래스
 * - BP_Door를 이 클래스로 reparent하면 기존 비주얼 유지 + C++ 제어 가능
 * - OpenDoor()/CloseDoor()로 프로그래밍 방식 제어
 * - BeginPlay에서 HingeComponentName에 해당하는 컴포넌트를 자동 탐색
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AInteractiveDoorActor : public AActor
{
	GENERATED_BODY()

public:
	AInteractiveDoorActor();

	// 문 열기 (Timeline Forward)
	UFUNCTION(BlueprintCallable, Category = "Door")
	void OpenDoor();

	// 문 닫기 (Timeline Reverse)
	UFUNCTION(BlueprintCallable, Category = "Door")
	void CloseDoor();

	// 현재 열림 상태
	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bIsOpen; }

	// 문 열림 완료 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FOnDoorStateChanged OnDoorOpened;

	// 문 닫힘 완료 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FOnDoorStateChanged OnDoorClosed;

	// 수동 상호작용(오버랩+클릭) 허용 여부 — false면 OpenDoor/CloseDoor만 동작
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Config")
	bool bAllowManualInteraction = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── 자동 탐색 설정 ──────────────────────────────
	// BeginPlay에서 이 이름의 컴포넌트를 찾아 HingeComponent에 할당
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Components")
	FName HingeComponentName = TEXT("Chinge");

	// 자동 탐색된 힌지 컴포넌트 (회전 대상)
	UPROPERTY(BlueprintReadOnly, Category = "Door|Components")
	TObjectPtr<USceneComponent> HingeComponent;

	// ── 설정값 ───────────────────────────────────────
	// 문이 열릴 때의 회전 (BP_Door 기본: Yaw 90도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Config")
	FRotator DoorOpenRotation = FRotator(0.f, 90.f, 0.f);

	// 문 열림/닫힘 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Config")
	TObjectPtr<USoundBase> DoorSound;

	// 애니메이션 소요 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Config", meta = (ClampMin = "0.1"))
	float OpenDuration = 1.0f;

private:
	FTimeline DoorTimeline;

	UPROPERTY()
	TObjectPtr<UCurveFloat> DoorCurve;

	bool bIsOpen = false;

	UFUNCTION()
	void OnTimelineUpdate(float Alpha);

	UFUNCTION()
	void OnTimelineFinished();
};
