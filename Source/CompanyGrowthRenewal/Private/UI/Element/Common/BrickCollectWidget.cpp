// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Common/BrickCollectWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/UISoundTags.h"
#include "Utils/FWidgetAnimationUtils.h"

void UBrickCollectWidget::InitCollect(const FVector2D& InStartPos, const FVector2D& InTargetPos, UCanvasPanelSlot* InSlot, int32 InCollectAmount, float InBurstDelayExtra, bool bInIsLastOfBatch, int32 InBatchTotal, float InLandSoundVolume)
{
    Pos = InStartPos;
    TargetPos = InTargetPos;
    CanvasSlot = InSlot;
    CollectAmount = InCollectAmount;
    BurstDelayExtra = InBurstDelayExtra;
    bIsLastOfBatch = bInIsLastOfBatch;
    BatchTotal = InBatchTotal;
    LandSoundVolume = InLandSoundVolume;

    // 수직(-Y) 기준 콘 내 랜덤 방향으로 분출
    const float ConeRad = FMath::DegreesToRadians(FMath::FRandRange(-BurstConeHalfAngleDeg, BurstConeHalfAngleDeg));
    const float Speed = FMath::FRandRange(BurstSpeedMin, BurstSpeedMax);
    Velocity = FVector2D(FMath::Sin(ConeRad), -FMath::Cos(ConeRad)) * Speed;

    CurrentAngle = FMath::FRandRange(-InitialAngleJitterDeg, InitialAngleJitterDeg);
    SpinSpeed = FMath::FRandRange(-SpinSpeedMaxDeg, SpinSpeedMaxDeg);

    Phase = ECollectPhase::Burst;
    ElapsedTime = 0.f;

    // WBP Pivot 설정 의존 제거 — 스케일/회전 중심 고정
    SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    SetRenderTransformAngle(CurrentAngle);
    SetRenderScale(FVector2D(SpawnStartScale, SpawnStartScale));
}

void UBrickCollectWidget::NativeTick(const FGeometry& MyGeometry, float DeltaSeconds)
{
    Super::NativeTick(MyGeometry, DeltaSeconds);

    if (!CanvasSlot)
        return;

    ElapsedTime += DeltaSeconds;

    CurrentAngle += SpinSpeed * DeltaSeconds;
    SetRenderTransformAngle(CurrentAngle);

    if (Phase == ECollectPhase::Burst)
    {
        // 스크린 Y-down 이라 +Gravity = 낙하
        Velocity.Y += Gravity * DeltaSeconds;
        Pos += Velocity * DeltaSeconds;
        CanvasSlot->SetPosition(Pos);

        const float PopAlpha = FMath::Clamp(ElapsedTime / SpawnPopDuration, 0.f, 1.f);
        const float PopScale = FMath::Lerp(SpawnStartScale, 1.f, FWidgetAnimationUtils::EaseOutBack(PopAlpha));
        SetRenderScale(FVector2D(PopScale, PopScale));

        if (ElapsedTime >= BurstDuration + BurstDelayExtra)
        {
            BeginHoming();
        }
        return;
    }

    const float RawAlpha = FMath::Clamp(ElapsedTime / HomingDuration, 0.f, 1.f);
    const float Eased = FMath::Pow(RawAlpha, HomingEaseExp);

    const FVector2D A = FMath::Lerp(HomingStart, HomingControl, Eased);
    const FVector2D B = FMath::Lerp(HomingControl, TargetPos, Eased);
    Pos = FMath::Lerp(A, B, Eased);
    CanvasSlot->SetPosition(Pos);

    // 축소는 선형(RawAlpha) — Eased(pow3)를 타면 막판에 몰려 "갑자기 쪼그라드는" 느낌
    const float FlyScale = FMath::Lerp(1.f, ArriveScale, RawAlpha);
    SetRenderScale(FVector2D(FlyScale, FlyScale));

    if (RawAlpha >= 1.f)
    {
        FinishCollect();
    }
}

void UBrickCollectWidget::BeginHoming()
{
    Phase = ECollectPhase::Homing;
    ElapsedTime = 0.f;
    HomingStart = Pos;

    FVector2D InertiaDir = Velocity.GetSafeNormal();
    if (InertiaDir.IsNearlyZero())
    {
        InertiaDir = FVector2D(0.f, -1.f);
    }
    HomingControl = HomingStart + InertiaDir * HomingCurveLead;

    SetRenderScale(FVector2D(1.f, 1.f));
}

void UBrickCollectWidget::FinishCollect()
{
    // 벽돌 N개 연출이면 착지가 N번이라 즉시 저장 금지, 지연 저장 1회로
    if (UResourceItemManager* RMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
    {
        RMgr->StoreResource(EResourceType::Brick, CollectAmount, /*bShouldSave=*/false);
    }
    if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveMgr->RequestDeferredSave();
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
        {
            if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
            {
                if (UResourceWidget* BrickCounter = InGame->GetResourceWidget(EResourceType::Brick))
                {
                    BrickCounter->PlayBump();
                }

                // "+N" 팝업은 배치 합계로 마지막 도착 시 1회 (아이콘별 스팸 금지)
                if (bIsLastOfBatch)
                {
                    InGame->SpawnGainPopup(EResourceType::Brick, BatchTotal);
                }
            }
        }
        // 착지음도 배치당 1발 — 아이콘마다 울리면 생산틱 1회에 최대 7발이 겹친다.
        // 볼륨 = 배치 간 감쇠(x0.7, 하한 0.35, 0.8s 리셋 — InGameLayerWidget 공유 상태), 피치 ±6% 랜덤으로 기계적 반복감 제거
        if (bIsLastOfBatch)
        {
            if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
            {
                SM->PlayUISoundWithParams(CGUISoundTags::RewardBrick, LandSoundVolume, FMath::FRandRange(0.94f, 1.06f));
            }
        }
    }

    CanvasSlot = nullptr;
    RemoveFromParent();
}
