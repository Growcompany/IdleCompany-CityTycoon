// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Common/BrickCollectWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Panel/InGameLayerWidget.h"
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
    BurstStartPos = Pos;
    BurstStartVelocity = Velocity;

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

    ElapsedTime += FMath::Max(0.f, DeltaSeconds);

    CurrentAngle += SpinSpeed * DeltaSeconds;
    SetRenderTransformAngle(CurrentAngle);

    if (Phase == ECollectPhase::Burst)
    {
        const float BurstEndTime = FMath::Max(0.01f, BurstDuration + FMath::Max(0.f, BurstDelayExtra));
        const float BurstAge = FMath::Min(ElapsedTime, BurstEndTime);
        // 스크린 Y-down 이라 +Gravity = 낙하. 시작 상태와 경과 시간으로 계산해 프레임 간격에 흔들리지 않는다.
        Pos = BurstStartPos + BurstStartVelocity * BurstAge
            + FVector2D(0.f, 0.5f * Gravity * BurstAge * BurstAge);
        Velocity = BurstStartVelocity + FVector2D(0.f, Gravity * BurstAge);
        CanvasSlot->SetPosition(Pos);

        const float PopAlpha = FMath::Clamp(BurstAge / FMath::Max(SpawnPopDuration, 0.01f), 0.f, 1.f);
        const float PopScale = FMath::Lerp(SpawnStartScale, 1.f, FWidgetAnimationUtils::EaseOutBack(PopAlpha));
        SetRenderScale(FVector2D(PopScale, PopScale));

        if (ElapsedTime < BurstEndTime)
        {
            return;
        }

        const float OverflowTime = ElapsedTime - BurstEndTime;
        BeginHoming();
        ElapsedTime = OverflowTime;
    }

    const float RawAlpha = FMath::Clamp(ElapsedTime / FMath::Max(HomingDuration, 0.01f), 0.f, 1.f);
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
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>())
        {
            RMgr->CollectFactoryBricks(CollectAmount);
        }
        if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
        {
            if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
            {
                // "+N" 팝업은 배치 합계로 마지막 도착 시 1회 (아이콘별 스팸 금지)
                if (bIsLastOfBatch)
                {
                    InGame->PlayBrickCollectionFeedback(BatchTotal, LandSoundVolume);
                }
            }
        }
    }

    CanvasSlot = nullptr;
    RemoveFromParent();
}
