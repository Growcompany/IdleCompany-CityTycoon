// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/GachaRecruitmentData.h"
#include "Navigation/PathFollowingComponent.h"
#include "RecruitmentGameMode.generated.h"

class AInteractiveDoorActor;
class UCGGameInstance;
class AOfficeworker;
class AOfficeworkerMale;
class AOfficeworkerFemale;
class URecruitmentResultPanelWidget;
struct FAIRequestID;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGachaSequenceFinished);

/**
 * 채용(가챠) 맵 전용 게임 모드
 * - 고정 카메라 Pawn 설정
 * - 문 열림 → 직원 등장 → 결과 표시 시퀀스 관리
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ARecruitmentGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARecruitmentGameMode();

	// 가챠 연출 시작 (UI에서 뽑기 버튼 클릭 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void StartGachaSequence();

	// 가챠 시퀀스 완료 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Recruitment")
	FOnGachaSequenceFinished OnGachaSequenceFinished;

	// 레벨에 배치된 문 참조
	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	TObjectPtr<AInteractiveDoorActor> DoorActor;

	// 현재 가챠 결과
	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	FGachaResultData CurrentGachaResult;

	// 오피스맵으로 복귀
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void ReturnToOffice();

protected:
	virtual void StartPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	void OnWalkingWorkerArrived();
	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

	UFUNCTION()
	void OnGreetingFinished();

	// Portrait 선행 촬영 (연출과 병행)
	void StartPreemptivePortraitCapture();
	void OnPreemptivePortraitDone(const FString& EmployeeID);
	void PerformHireAndReturn();

	FDelegateHandle PortraitCaptureHandle;
	bool bPendingHireConfirm = false;

	// 결과 패널 위젯
	UPROPERTY()
	TObjectPtr<URecruitmentResultPanelWidget> ResultPanelWidget;

	// 결과 UI 표시/정리
	void ShowResultUI();
	void CleanupResultUI();

	// 결과 패널 버튼 콜백
	void OnConfirmExitRequested();

	void FindDoorActor();
	void FindPortraitWorkers();

	// Walking Worker 동적 스폰 (문 뒤 스폰 → 걸어나옴)
	void SpawnWalkingWorker();

	// Portrait/Walking Worker에 외형 적용
	void ApplyAppearanceToWorker(AOfficeworker* Worker);

	// ── Portrait Worker (레벨에 미리 배치, 공개용) ──
	UPROPERTY()
	TObjectPtr<AOfficeworkerMale> PortraitMale;

	UPROPERTY()
	TObjectPtr<AOfficeworkerFemale> PortraitFemale;

	// 성별에 따라 활성화된 Portrait Worker
	UPROPERTY()
	TObjectPtr<AOfficeworker> ActivePortraitWorker;

	// ── Walking Worker (동적 스폰, 문에서 걸어나옴) ──
	UPROPERTY()
	TObjectPtr<AOfficeworker> WalkingWorker;

	// 스틱맨 BP 클래스 1회 해석 캐시 (OfficeGameMode 와 동일 패턴 — 매 스폰 소프트 클래스 재로드 제거).
	UPROPERTY(Transient)
	TSubclassOf<AOfficeworker> ResolvedStickWorkerClass = nullptr;

	UFUNCTION()
	void OnDoorOpened();

	UFUNCTION()
	void OnDoorClosed();
};
