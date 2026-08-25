// Fill out your copyright notice in the Description page of Project Settings.


#include "CGGameModeBase.h"
#include "Player/MainMapPlayerController.h"
#include "UI/HUD/MainMapHUD.h"
#include "Player/PlayerCamera.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Entity/Factory/BrickFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Core/CGGameInstance.h"
#include "UI/HUD/VisitModeOverlayWidget.h"
#include "Enum/WidgetType.h"
#include "Enum/CompanyTitle.h"
#include "AsyncLoadingScreenLibrary.h"

ACGGameModeBase::ACGGameModeBase()
{
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
                UVisitModeOverlayWidget* Overlay = CreateWidget<UVisitModeOverlayWidget>(GetWorld(), OverlayClass);
                if (Overlay)
                {
                    const FCitySnapshot& Snapshot = GI->GetVisitCitySnapshot();
                    Overlay->SetVisitInfo(GI->GetVisitTargetDisplayName(), Snapshot.HQLevel);
                    Overlay->AddToViewport(100);

                    UE_LOG(LogTemp, Log, TEXT("[CGGameMode] 방문 모드 오버레이 표시: %s (HQ Lv.%d)"),
                        *GI->GetVisitTargetDisplayName(), Snapshot.HQLevel);
                }
            }
        }
    }
    else
    {
        // 정상 모드: 기존 MainMap UI 표시
        if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->ShowMainMapUI();
        }

        // 미션 체인 시작 (신규 게임 = 오프닝 [건설] 유도부터) — LoadGameAfterLevelStart 이후라 세이브 판정 완료 상태
        if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
        {
            // 미션 트래커/가이드 빌드는 UIManager::OnLevelLayerReady 신호가 구동 — 여기서 직접 호출하지 않는다.
            // 오프닝 2 — 벽돌 노가다 미션 중엔 변두리 Brick Factory 근접 시작 (B안 비트 1: 변두리 + Factory 출발).
            // 줌은 FocusOnActor가 바운딩 박스로 자동 산출, 로딩 화면 뒤에서 전환돼 체감상 즉시 근접.
            // 첫 건물 포커스는 기존 CompleteBuildingPlacement의 줌인이 담당.
            FMissionTable ActiveMission;
            if (MissionMgr->GetActiveMission(ActiveMission) && ActiveMission.ConditionType == EMissionConditionType::CollectBricks)
            {
                ABrickFactory* Factory = Cast<ABrickFactory>(
                    UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass()));
                APlayerController* PC = GetWorld()->GetFirstPlayerController();
                APlayerCamera* Camera = PC ? Cast<APlayerCamera>(PC->GetPawn()) : nullptr;
                if (Factory && Camera)
                {
                    // 로딩 화면 뒤(뷰포트 0x0 가능)라 가림 판정이 무의미 — 고스트 제외
                    Camera->FocusOnActor(Factory, 0.5f, false);
                }
            }
        }
    }

    // 로딩 화면 종료
    UAsyncLoadingScreenLibrary::StopLoadingScreen();
}

