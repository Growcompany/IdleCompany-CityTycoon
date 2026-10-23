// Fill out your copyright notice in the Description page of Project Settings.


#include "CGGameModeBase.h"
#include "Player/MainMapPlayerController.h"
#include "UI/HUD/MainMapHUD.h"
#include "Player/PlayerCamera.h"
#include "Manager/TableManagerSubsystem.h"
#include "Entity/Factory/BrickFactory.h"
#include "Entity/Environment/CityPlayableArea.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/EntityManager.h"
#include "Kismet/GameplayStatics.h"
#include "Core/CGGameInstance.h"
#include "UI/HUD/VisitModeOverlayWidget.h"
#include "Enum/WidgetType.h"
#include "Enum/CompanyTitle.h"
#include "MoviePlayer.h"

ACGGameModeBase::ACGGameModeBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    DefaultPawnClass = APlayerCamera::StaticClass();
    // PlayerController 클래스 지정
    PlayerControllerClass = AMainMapPlayerController::StaticClass();
    HUDClass = AMainMapHUD::StaticClass();
}

void ACGGameModeBase::BeginPlay()
{
    Super::BeginPlay();
}

void ACGGameModeBase::StartPlay()
{
    Super::StartPlay();

    // 게임 데이터 로드 (GameMode::StartPlay가 정석적인 위치)
    UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
    if (GameInstance)
    {
        GameInstance->LoadGameAfterLevelStart();
    }

    // 도시 시작 방위는 축에 맞춘다. 폰 yaw 가 곧 카메라 방위이고(SpringArm bUsePawnControlRotation=false),
    // 이 맵엔 PlayerStart 가 없어 엔진이 WorldSettings 트랜스폼으로 스폰한다 — 회전이 딸려올 수 있는 유일한 경로다.
    // ⚠ 아래 시작 포커스보다 먼저여야 한다: 목표 pose 를 폰의 현재 회전 기준으로 계산하므로 순서가 바뀌면 시선이 틀어진다.
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        if (APlayerCamera* Camera = Cast<APlayerCamera>(PC->GetPawn()))
        {
            // 실제로 틀어져 있었는지 로그로 가른다 — 이미 0 이면 이 줄은 보장일 뿐이고,
            // 찍힌다면 WorldSettings 가 회전을 들고 있다는 뜻이라 레벨 쪽을 봐야 한다.
            const FRotator SpawnRotation = Camera->GetActorRotation();
            if (!SpawnRotation.IsNearlyZero())
            {
                UE_LOG(LogTemp, Warning, TEXT("[CGGameMode] 시작 회전 보정: %s → 0,0,0"), *SpawnRotation.ToCompactString());
            }
            Camera->SetActorRotation(FRotator::ZeroRotator);
            if (const ACityPlayableArea* PlayableArea = Cast<ACityPlayableArea>(
                UGameplayStatics::GetActorOfClass(GetWorld(), ACityPlayableArea::StaticClass())))
            {
                PlayableArea->ApplyToCamera(Camera);
            }
        }
    }

    // 방문 모드 vs 정상 모드 UI 분기
    UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
    if (GI && GI->IsVisitMode())
    {
        // 방문 모드: VisitModeOverlay 표시 (일반 UI 숨김)
        UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        if (TableMgr)
        {
            TSubclassOf<UUserWidget> OverlayClass = TableMgr->GetWidgetClass(EWidgetType::VisitModeOverlay);
            if (OverlayClass)
            {
                VisitOverlay = CreateWidget<UVisitModeOverlayWidget>(GetWorld(), OverlayClass);
                if (VisitOverlay)
                {
                    const FCitySnapshot& Snapshot = GI->GetVisitCitySnapshot();
                    VisitOverlay->SetVisitInfo(GI->GetVisitTargetDisplayName(), Snapshot.HQLevel);
                    VisitOverlay->AddToViewport(100);

                    UE_LOG(LogTemp, Log, TEXT("[CGGameMode] 방문 모드 오버레이 표시: %s (HQ Lv.%d)"),
                        *GI->GetVisitTargetDisplayName(), Snapshot.HQLevel);
                }
            }
        }
    }
    else
    {
        // 도착 구도는 기존 포커스가 소유하고, 첫 오프닝은 그 구도로 이어진다.
        const bool bReturnedFromOffice = TryFocusCameraOnReturnedOfficeBuilding();
        if (!bReturnedFromOffice)
        {
            FocusCameraOnCityHomeAnchor();
            TryPrepareCityOpening();
        }

        // MainMapHUD가 UI 초기화를 요청하고, OnLevelLayerReady 뒤에 오프닝/미션 가이드를 표시한다.
    }

    // 로딩 화면 종료
    GetMoviePlayer()->StopMovie();
}

bool ACGGameModeBase::IsVisitPresentationReady() const
{
    return IsValid(VisitOverlay) && VisitOverlay->IsInViewport();
}

void ACGGameModeBase::FocusCameraOnCityHomeAnchor()
{
    ABrickFactory* Factory = Cast<ABrickFactory>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass()));
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APlayerCamera* Camera = PC ? Cast<APlayerCamera>(PC->GetPawn()) : nullptr;
    if (!Factory || !Camera)
    {
        // 공장이 없는 도시 맵이면 레벨 저작 스폰 지점에 남는다 — 조용히 넘기면 원인을 못 찾는다
        UE_LOG(LogTemp, Warning, TEXT("[CGGameMode] 시작 구도 생략 — BrickFactory=%s Camera=%s"),
            Factory ? TEXT("있음") : TEXT("없음"), Camera ? TEXT("있음") : TEXT("없음"));
        return;
    }

    const float FocusDistance = Factory->GetCameraFocusDistanceCm();
    if (FocusDistance > 0.f)
    {
        Camera->SnapFocusOnActor(Factory, 0.5f, FocusDistance);
    }
}

bool ACGGameModeBase::TryFocusCameraOnReturnedOfficeBuilding()
{
    UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
    if (!GameInstance)
    {
        return false;
    }

    // 소비형 — 포커스가 실패해도 인덱스는 비워, 다음 도시 진입이 묵은 건물로 끌려가지 않게 한다
    const int32 BuildingIndex = GameInstance->ConsumePendingMainMapFocusBuilding();
    if (BuildingIndex == INDEX_NONE)
    {
        return false;
    }

    UEntityManager* EntityMgr = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr;
    ABuildingBaseActor* Building = EntityMgr ? EntityMgr->GetBuildingByIndex(BuildingIndex) : nullptr;
    if (!Building)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CGGameMode] 복귀 포커스 대상 건물을 못 찾음 (BuildingIndex: %d)"), BuildingIndex);
        return false;
    }

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    APlayerCamera* Camera = PC ? Cast<APlayerCamera>(PC->GetPawn()) : nullptr;
    if (!Camera)
    {
        return false;
    }

    // 로딩 화면이 걷히는 순간 이미 그 자리여야 한다 — 글라이드면 스폰 지점에서 날아오는 게 보인다
    if (!Camera->SnapFocusOnBuilding(Building))
    {
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[CGGameMode] 오피스 복귀 — 카메라를 건물 %d (%s) 구도로 정착"),
        BuildingIndex, *Building->GetName());
    return true;
}

