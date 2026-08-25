// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/EmployeeManager.h"
#include "Core/CGGameInstance.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EmployeePotentialData.h"

#include "Manager/TableManagerSubsystem.h"
#include "Table/EmployeeNameTable.h"
#include "Manager/SpawnManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Data/EntitySaveData.h"
#include "Table/BuildingData.h"
#include "Manager/ProjectOperationManager.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "GameMode/OfficeGameMode.h"

#include "Entity/Officeworker/ModularOfficeworker.h"
#include "Entity/Officeworker/OfficeworkerMale.h"
#include "Entity/Officeworker/OfficeworkerFemale.h"
#include "Entity/Officeworker/StickOfficeworker.h"
#include "Components/SceneCaptureComponent2D.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameMode/OfficeGameMode.h"
#include "Kismet/GameplayStatics.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Enum/NotificationType.h"

void UEmployeeManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UEmployeeManager::HandleWorldCleanup);
}

void UEmployeeManager::Deinitialize()
{
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    WorldCleanupHandle.Reset();

    Super::Deinitialize();
}

void UEmployeeManager::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
    // 우리 게임 인스턴스의 월드만 — 에디터 프리뷰/기타 월드 정리에 반응하면 진행 중인 PIE 캡처를 죽인다.
    // 정리 시점에 OwningGameInstance 가 이미 끊겼을 수 있어 현재 월드 동일성도 함께 본다.
    if (!World || (World != GetWorld() && World->GetGameInstance() != GetGameInstance()))
    {
        return;
    }

    // 촬영 중 오피스를 떠나면 캡처 완료 델리게이트가 영영 안 온다 → bPortraitCapturing 이 세션 내내 true 로 남아
    // 이후 모든 채용이 차단된다(위젯 파괴 뱅킹까지 같은 게이트에 막혀 결과 유실). 월드가 사라지는 지점에서 끊는다.
    // 큐 폐기가 안전한 이유 = 초상화 없는 직원은 오피스 재입장 backfill 이 다시 찍는다(데이터는 이미 적립됨).
    if (bPortraitCapturing || bInPortraitCompletion || PendingPortraitQueue.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Portrait] 레벨 전환 — 파이프라인 리셋 (진행중=%d, 대기 큐 %d건 폐기, backfill 로 복구)"),
            bPortraitCapturing ? 1 : 0, PendingPortraitQueue.Num());
    }
    bPortraitCapturing = false;
    bInPortraitCompletion = false;
    PendingPortraitQueue.Empty();
}

UTableManagerSubsystem* UEmployeeManager::GetTableManager() const
{
    if (!TableManager)
    {
        TableManager = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    }
    return TableManager;
}

void UEmployeeManager::EnsureDataTablesLoaded()
{
    if (!LastNameDataTable || !FirstNameDataTable)
    {
        UTableManagerSubsystem* TableMgr = GetTableManager();
        if (TableManager)
        {
            LastNameDataTable = TableMgr->GetLastNameDataTable();
            FirstNameDataTable = TableMgr->GetFirstNameDataTable();

            if (LastNameDataTable && FirstNameDataTable)
            {
                UE_LOG(LogTemp, Warning, TEXT("Employee name data tables loaded successfully (delayed)"));
            }
        }
    }
}

bool UEmployeeManager::HireEmployee(EEmployeeDepartment Department, int32 EnhancementLevel)
{
    // 캡처 진행 중이면 거부
    if (bPortraitCapturing)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot hire: Portrait capture in progress"));
        if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIMgr->ShowNotification(NSLOCTEXT("Notification", "ProcessingInProgress", "처리 중입니다. 잠시 후 다시 시도해주세요"), 3.0f, ENotificationType::Warning);
        }
        return false;
    }

    UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
    USpawnManager* SpawnManager = GetWorld()->GetSubsystem<USpawnManager>();

    // 1. Employee 인스턴스 생성
    FEmployeeInstance NewInstance = CreateEmployeeInstance(Department);

    // 강화 레벨 적용
    NewInstance.EnhancementLevel = EnhancementLevel;

    EmployeeList.Add(NewInstance);

    // 2. 외모 데이터 생성
    EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(NewInstance.EnhancementLevel);
    FCharacterAppearance RandomAppearance = GenerateRandomAppearance(Rank, NewInstance.Gender);

    // 3. HairRandomSeed 생성 (GenerateRandomAppearance에서는 0으로 초기화됨)
    RandomAppearance.HairRandomSeed = FMath::Rand();

    FEmployeeAppearanceData AppearanceData;
    AppearanceData.Appearance = RandomAppearance;
    AppearanceData.CurrentRank = Rank;

    EmployeeAppearances.Add(NewInstance.EmployeeID, AppearanceData);

    // 4. Portrait 촬영 (FCharacterAppearance에서 정보 추출)
    FString EmployeeIDStr = FString::FromInt(NewInstance.EmployeeID);
    CaptureEmployeePortrait(
        EmployeeIDStr,
        RandomAppearance,
        Rank,
        NewInstance.Gender,
        RandomAppearance.HairCombinationType,
        RandomAppearance.HairRandomSeed,
        Department,
        AStickOfficeworker::ResolveGachaTier(NewInstance)
    );
    UE_LOG(LogTemp, Warning, TEXT("Employee hired: Department=%d, EmployeeID=%d"),
        static_cast<int32>(Department), NewInstance.EmployeeID);

    // Employee 고용 시 즉시 저장
    if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveLoadManager->SaveGameData();
        UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Game data saved after hiring employee"));
    }

    return true;
}

FText UEmployeeManager::GetCapacityFullMessage()
{
    return NSLOCTEXT("Employee", "BuildingCapacityFull", "인원이 가득 찼습니다. 빌드업으로 층을 올리면 더 뽑을 수 있습니다");
}

int32 UEmployeeManager::GetEmployeeCountInBuilding(int32 BuildingIndex) const
{
    int32 Count = 0;
    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.AssignedBuildingIndex == BuildingIndex)
        {
            ++Count;
        }
    }
    return Count;
}

int32 UEmployeeManager::GetBuildingEmployeeCapacity(int32 BuildingIndex, bool bLogIfZero) const
{
    return GetBuildingEmployeeCapacityAtFloors(BuildingIndex, INDEX_NONE, bLogIfZero);
}

int32 UEmployeeManager::GetBuildingEmployeeCapacityAtFloors(int32 BuildingIndex, int32 AddedFloors, bool bLogIfZero) const
{
    // 0 반환은 곧 "더 못 뽑는다" 로 읽혀 채용이 막힌다 — 조기 반환 전부가 원인을 남겨야 조용한 차단이 안 생긴다
    if (BuildingIndex == INDEX_NONE)
    {
        if (bLogIfZero)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] GetBuildingEmployeeCapacity: BuildingIndex is INDEX_NONE (호출자가 대상 빌딩을 모름)"));
        }
        return 0;
    }

    USaveLoadManager* SaveMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
    UTableManagerSubsystem* TableMgr = GetTableManager();
    if (!SaveData || !TableMgr)
    {
        if (bLogIfZero)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] GetBuildingEmployeeCapacity: Building %d 조회 불가 (SaveData=%d, TableManager=%d)"),
                BuildingIndex, SaveData != nullptr, TableMgr != nullptr);
        }
        return 0;
    }

    for (const FBuildingEntitySaveData& BuildingSave : SaveData->GameData.Buildings)
    {
        if (BuildingSave.BuildingIndex != BuildingIndex)
        {
            continue;
        }
        bool bFound = false;
        const FBuildingData Row = TableMgr->GetBuildingData(BuildingSave.InteractableName, bFound);
        if (!bFound)
        {
            if (bLogIfZero)
            {
                UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] GetBuildingEmployeeCapacity: Building %d 의 DT 행 '%s' 없음"),
                    BuildingIndex, *BuildingSave.InteractableName.ToString());
            }
            return 0;
        }

        const int32 Floors = (AddedFloors >= 0) ? AddedFloors : BuildingSave.BuildingData.Body_Module_Copies;
        return Row.GetEmployeeCapacity(Floors);
    }

    if (bLogIfZero)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] GetBuildingEmployeeCapacity: Building %d not found in save data"), BuildingIndex);
    }
    return 0;
}

bool UEmployeeManager::CanHireIntoBuilding(int32 BuildingIndex) const
{
    // 상한은 책상/자리가 아니라 인원 — 책상은 무제한 배치라 세지 않는다(층수만 상한을 올린다)
    return IsUnderCapacity(GetEmployeeCountInBuilding(BuildingIndex), GetBuildingEmployeeCapacity(BuildingIndex));
}

bool UEmployeeManager::HireEmployeeFromCard(const FEmployeeInstance& CardData, int32 BuildingIndex, bool bShouldSave)
{
    // 캡처 진행 중이면 거부 (대기 큐 잔량 포함 — 배치 뽑기의 나머지 초상화가 아직 남아 있을 수 있음)
    if (IsPortraitCapturing())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot hire from card: Portrait capture in progress"));
        if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIMgr->ShowNotification(NSLOCTEXT("Notification", "ProcessingInProgress", "처리 중입니다. 잠시 후 다시 시도해주세요"), 3.0f, ENotificationType::Warning);
        }
        return false;
    }

    // 인원 상한 게이트 최후 방어선 — 채용 진입 사전차단(OfficeRecruitmentPanelWidget)을 뚫고 온 경로까지 막는다
    if (!CanHireIntoBuilding(BuildingIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] Hire blocked: building %d at employee capacity (%d/%d)"),
            BuildingIndex, GetEmployeeCountInBuilding(BuildingIndex), GetBuildingEmployeeCapacity(BuildingIndex));
        if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIMgr->ShowNotification(GetCapacityFullMessage(), 3.0f, ENotificationType::Failed);
        }
        return false;
    }

    // 카드 데이터 복사 (카드 생성 시 할당된 ID를 그대로 사용)
    FEmployeeInstance NewEmployee = CardData;

    // 소속 건물은 고정, 좌석은 미점유(미배치 풀) — 좌석 배정은 책상 패널이 담당 (pull-to-pool)
    NewEmployee.AssignedBuildingIndex = BuildingIndex;
    NewEmployee.bIsAssigned = false;
    NewEmployee.HiredDate = FDateTime::Now();

    // EmployeeList에 추가
    EmployeeList.Add(NewEmployee);

    // 외모 데이터 저장 (카드의 Appearance 사용)
    EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(NewEmployee.EnhancementLevel);
    FEmployeeAppearanceData AppearanceData;
    AppearanceData.Appearance = CardData.Appearance;
    AppearanceData.CurrentRank = Rank;
    EmployeeAppearances.Add(NewEmployee.EmployeeID, AppearanceData);

    // 초상화는 뽑기 시점(RecruitmentManager::ExecuteGachaPull)에 이미 촬영됨 → 여기선 중복 촬영 안 함.
    // (촬영 누락 시 안전망: 오피스 입장 StartPortraitCapture 가 PNG 없는 직원을 backfill.)

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Hired from card: ID=%d, Name=%s, BuildingIndex=%d, Dept=%d"),
        NewEmployee.EmployeeID, *NewEmployee.EmployeeName, BuildingIndex, static_cast<int32>(NewEmployee.Department));

    // 저장 — 배치 채용은 마지막 1건에서만 저장하도록 호출자가 끈다
    if (bShouldSave)
    {
        if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        {
            SaveLoadManager->SaveGameData();
            UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Game data saved after hiring from card"));
        }
    }

    // 기대 수익 갱신 — 직원 배치 변화 즉시 반영
    if (BuildingIndex != INDEX_NONE)
    {
        if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
        {
            OpMgr->InvalidateStatBonusCache(BuildingIndex);
        }
    }

    // 벤치 적립 완료 → 로스터 변경 알림 (책상 패널이 가려진 채로도 RefreshRosterPicker 됨 — 가챠 채용 즉시 반영)
    OnEmployeeRosterChanged.Broadcast();

    return true;
}

bool UEmployeeManager::FireEmployee(int32 EmployeeInstanceID)
{
    for (int32 i = 0; i < EmployeeList.Num(); i++)
    {
        if (EmployeeList[i].EmployeeID == EmployeeInstanceID)
        {
            // 해고 전에 소속 빌딩 기록 — 제거 후 접근 불가하므로 복사
            const int32 FiredBuildingIndex = EmployeeList[i].AssignedBuildingIndex;

            UE_LOG(LogTemp, Warning, TEXT("Employee Fired: (ID: %d)"), EmployeeInstanceID);

            // 좌석/카운터/워커 액터 정리 — 목록에서 지우기 전에 해야 책상을 역참조할 수 있다
            if (UWorld* World = GetGameInstance()->GetWorld())
            {
                if (UOfficeManager* OfficeMgr = World->GetSubsystem<UOfficeManager>())
                {
                    if (AWorkstationActorBase* Seated = OfficeMgr->FindWorkstationByEmployeeID(EmployeeInstanceID))
                    {
                        OfficeMgr->UnseatEmployee(Seated, EmployeeInstanceID);
                    }
                }

                // UnseatEmployee 는 책상이 있을 때만 도는 경로라, 벤치 직원의 잔존 액터는 여기서 직접 거둔다
                if (AOfficeGameMode* OfficeGM = World->GetAuthGameMode<AOfficeGameMode>())
                {
                    OfficeGM->DespawnWorkerByID(EmployeeInstanceID);
                }
            }

            // 목록에서 제거
            EmployeeList.RemoveAt(i);

            // Employee 해고 시 즉시 저장
            if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
            {
                SaveLoadManager->SaveGameData();
                UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Game data saved after firing employee"));
            }

            // 기대 수익 갱신 — 이전 소속 빌딩 수익보너스 평균 바뀜
            if (FiredBuildingIndex != INDEX_NONE)
            {
                if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
                {
                    OpMgr->InvalidateStatBonusCache(FiredBuildingIndex);
                }
            }

            // 벤치 적립 경로와 같은 규약 — 직원창 로스터/책상 도크가 가려진 채로도 스스로 갱신
            OnEmployeeRosterChanged.Broadcast();

            return true;
        }
    }
    return false;
}

FEmployeeInstance UEmployeeManager::CreateEmployeeInstance(EEmployeeDepartment Department)
{
    FEmployeeInstance Instance;
    Instance.EmployeeID = NextInstanceID++;
    Instance.Gender = GenerateRandomGender();
    Instance.SpawnRarity = ELootBoxRarity::Common;  // 비가챠(스타터 등) 직원은 Common 밴드 핸들
    Instance.EmployeeName = GenerateRandomName(Instance.SpawnRarity);
    Instance.Department = Department;
	// 전달된 부서가 생산직능이면 그 직능 집중 프로필 시드 → 부서 재파생(동일). 운영/None이면 프로필 없음(부서 유지).
	const EProductionDiscipline SeedDisc = DepartmentToDiscipline(Department);
	if (SeedDisc != EProductionDiscipline::Count)
	{
		Instance.DisciplinePoints = MakeDisciplineProfile(SeedDisc, Instance.SpawnRarity);
	}
	SyncDerivedDepartment(Instance);

	// 초기 스탯 — 총량 30 랜덤 분배(최소 3). 등급은 관여하지 않는다(뾰족함은 직능 총량 전담).
	Instance.Stats = FEmployeeStats();
	ApplyStatProfile(Instance.Stats, MakeStatProfile());
	Instance.InvestedDisciplinePoints.Init(0, static_cast<int32>(EProductionDiscipline::Count));
    Instance.Level = 1;
    Instance.Experience = 0.0f;
    Instance.EnhancementLevel = 0;  // 0 = 인턴부터 시작
    Instance.HiredDate = FDateTime::Now();

    return Instance;
}

bool UEmployeeManager::AssignEmployeeToBuilding(int32 EmployeeID, int32 BuildingIndex)
{
    for (FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.EmployeeID == EmployeeID)
        {
            const int32 OldBuildingIndex = Employee.AssignedBuildingIndex;

            // 인원 상한 우회 경로를 남기지 않는다. 대상 건물 인원이 실제로 늘어나는 경우만 검사 —
            // 해제(INDEX_NONE)와 같은 건물 내 재배치는 인원이 그대로라 검사하면 오히려 정상 동작을 막는다.
            if (BuildingIndex != INDEX_NONE && OldBuildingIndex != BuildingIndex && !CanHireIntoBuilding(BuildingIndex))
            {
                UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] Assign blocked: building %d at employee capacity (%d/%d), Employee=%d"),
                    BuildingIndex, GetEmployeeCountInBuilding(BuildingIndex), GetBuildingEmployeeCapacity(BuildingIndex), EmployeeID);
                return false;
            }

            Employee.AssignedBuildingIndex = BuildingIndex;
            Employee.bIsAssigned = (BuildingIndex != INDEX_NONE);

            UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Employee %d assigned to building index %d"), EmployeeID, BuildingIndex);

            // 저장
            if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
            {
                SaveLoadManager->SaveGameData();
            }

            // 기대 수익 갱신 — 이동 시 이전/새 빌딩 둘 다 영향
            if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
            {
                if (OldBuildingIndex != INDEX_NONE)
                {
                    OpMgr->InvalidateStatBonusCache(OldBuildingIndex);
                }
                if (BuildingIndex != INDEX_NONE && BuildingIndex != OldBuildingIndex)
                {
                    OpMgr->InvalidateStatBonusCache(BuildingIndex);
                }
            }

            return true;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] Employee %d not found for assignment"), EmployeeID);
    return false;
}

bool UEmployeeManager::UnassignEmployee(int32 EmployeeID)
{
    for (FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.EmployeeID == EmployeeID)
        {
            const int32 OldBuildingIndex = Employee.AssignedBuildingIndex;

            Employee.AssignedBuildingIndex = INDEX_NONE;
            Employee.bIsAssigned = false;

            UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Employee %d unassigned (fired)"), EmployeeID);

            // 저장
            if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
            {
                SaveLoadManager->SaveGameData();
            }

            // 기대 수익 갱신 — 이전 소속 빌딩 수익보너스 평균 바뀜
            if (OldBuildingIndex != INDEX_NONE)
            {
                if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
                {
                    OpMgr->InvalidateStatBonusCache(OldBuildingIndex);
                }
            }

            return true;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] Employee %d not found for unassignment"), EmployeeID);
    return false;
}

TArray<FEmployeeInstance> UEmployeeManager::GetUnassignedEmployees() const
{
    TArray<FEmployeeInstance> UnassignedList;

    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (!Employee.bIsAssigned)
        {
            UnassignedList.Add(Employee);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Found %d unassigned employees"), UnassignedList.Num());
    return UnassignedList;
}

TArray<FEmployeeInstance> UEmployeeManager::GetUnassignedEmployeesInBuilding(int32 BuildingIndex) const
{
    TArray<FEmployeeInstance> List;
    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (!Employee.bIsAssigned && Employee.AssignedBuildingIndex == BuildingIndex)
        {
            List.Add(Employee);
        }
    }
    return List;
}

TArray<FEmployeeInstance> UEmployeeManager::GetEmployeesInBuilding(int32 BuildingIndex) const
{
    TArray<FEmployeeInstance> BuildingEmployees;

    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.bIsAssigned && Employee.AssignedBuildingIndex == BuildingIndex)
        {
            BuildingEmployees.Add(Employee);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Found %d employees in building index %d"), BuildingEmployees.Num(), BuildingIndex);
    return BuildingEmployees;
}

TArray<AOfficeworker*> UEmployeeManager::GetSpawnedWorkersInBuilding(int32 BuildingIndex) const
{
    TArray<AOfficeworker*> Result;

    UWorld* World = GetWorld();
    if (!World || BuildingIndex < 0) return Result;

    // 건물 소속 직원 EmployeeID 집합 구축
    TSet<int32> TargetIDs;
    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.bIsAssigned && Employee.AssignedBuildingIndex == BuildingIndex)
        {
            TargetIDs.Add(Employee.EmployeeID);
        }
    }
    if (TargetIDs.Num() == 0) return Result;

    // 월드의 Officeworker 중 해당 ID만 수집
    for (TActorIterator<AOfficeworker> It(World); It; ++It)
    {
        AOfficeworker* Worker = *It;
        if (Worker && TargetIDs.Contains(Worker->GetEmployeeID()))
        {
            Result.Add(Worker);
        }
    }

    return Result;
}

FEmployeeInstance* UEmployeeManager::GetEmployeeData(int32 EmployeeID)
{
    return FindEmployee(EmployeeID);
}

FEmployeeInstance* UEmployeeManager::FindEmployee(int32 InstanceID)
{
    for (FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.EmployeeID == InstanceID)
        {
            return &Employee;
        }
    }
    return nullptr;
}

void UEmployeeManager::NotifyEmployeeDown(int32 EmployeeID, EWorkerDownReason Reason, float DisplayDuration)
{
    const FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee)
    {
        return;
    }

    // "무엇이 벌어졌는가 · 무엇을 누르면 되는가" 2부 구성.
    // 레일 폭 480 + 토스트 ClipToBounds 라 한 줄이 잘리지 않는 24~25자 안에서 유지할 것.
    FString Msg;
    switch (Reason)
    {
    case EWorkerDownReason::Lazy:
        Msg = FString::Printf(TEXT("%s 직원이 딴짓 중입니다 · 눌러서 깨우기"), *Employee->EmployeeName);
        break;
    case EWorkerDownReason::Bolted:
        Msg = FString::Printf(TEXT("%s 직원이 뛰쳐나갔습니다 · 눌러서 데려오기"), *Employee->EmployeeName);
        break;
    default:
        Msg = FString::Printf(TEXT("%s 직원이 졸고 있습니다 · 눌러서 깨우기"), *Employee->EmployeeName);
        break;
    }

    OnEmployeeStatusToast.Broadcast(EmployeeID, FText::FromString(Msg), DisplayDuration);
}

void UEmployeeManager::NotifyEmployeeRecovered(int32 EmployeeID)
{
    // 직원 조회 없이 그대로 흘린다 — 워커가 이미 사라진 뒤에도 레일의 알림은 걷어야 한다
    OnEmployeeStatusToastCleared.Broadcast(EmployeeID);
}

void UEmployeeManager::SelectEmployee(int32 EmployeeID)
{
    int32 OldID = SelectedEmployeeID;

    if (SelectedEmployeeID == EmployeeID)
    {
        // 같은 직원 클릭 시 선택 해제
        DeselectEmployee();
        return;
    }

    // OfficeGameMode에서 3D Actor 찾기
    AOfficeGameMode* OfficeGM = Cast<AOfficeGameMode>(UGameplayStatics::GetGameMode(GetGameInstance()->GetWorld()));

    // 이전 선택 직원 Overlay 해제
    if (OldID != -1 && OfficeGM)
    {
        if (AOfficeworker* OldActor = OfficeGM->FindEmployeeActorByID(OldID))
        {
            OldActor->SetSelected(false);
        }
    }

    // 새 직원 선택 및 Overlay 적용
    SelectedEmployeeID = EmployeeID;
    if (OfficeGM)
    {
        if (AOfficeworker* NewActor = OfficeGM->FindEmployeeActorByID(EmployeeID))
        {
            NewActor->SetSelected(true);
        }
    }

    // 이벤트 발생
    OnEmployeeSelectionChanged.Broadcast(OldID, EmployeeID);

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Employee %d selected (previous: %d)"), EmployeeID, OldID);
}

void UEmployeeManager::DeselectEmployee()
{
    int32 OldID = SelectedEmployeeID;

    // 이전 선택 직원 Overlay 해제
    if (OldID != -1)
    {
        AOfficeGameMode* OfficeGM = Cast<AOfficeGameMode>(UGameplayStatics::GetGameMode(GetGameInstance()->GetWorld()));
        if (OfficeGM)
        {
            if (AOfficeworker* OldActor = OfficeGM->FindEmployeeActorByID(OldID))
            {
                OldActor->SetSelected(false);
            }
        }
    }

    SelectedEmployeeID = -1;

    // 이벤트 발생
    OnEmployeeSelectionChanged.Broadcast(OldID, -1);

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Employee deselected (previous: %d)"), OldID);
}

FEmployeeInstance* UEmployeeManager::GetSelectedEmployee()
{
    if (SelectedEmployeeID == -1) return nullptr;

    return FindEmployee(SelectedEmployeeID);
}


FString UEmployeeManager::GetRankDisplayName(EEmployeeRank Rank)
{
    switch (Rank)
    {
    case EEmployeeRank::Intern: return TEXT("인턴");
    case EEmployeeRank::Assistant: return TEXT("사원");
    case EEmployeeRank::Associate: return TEXT("주임");
    case EEmployeeRank::SeniorAssociate: return TEXT("대리");
    case EEmployeeRank::Manager: return TEXT("과장");
    case EEmployeeRank::SeniorManager: return TEXT("차장");
    case EEmployeeRank::Director: return TEXT("부장");
    case EEmployeeRank::ManagingDirector: return TEXT("이사");
    case EEmployeeRank::ExecutiveDirector: return TEXT("상무");
    case EEmployeeRank::VP: return TEXT("전무");
    case EEmployeeRank::VicePresident: return TEXT("부사장");
    case EEmployeeRank::President: return TEXT("사장");
    case EEmployeeRank::Chairman: return TEXT("회장");
    default: return TEXT("인턴");
    }
}

FString UEmployeeManager::GetEmployeeFullName(int32 EmployeeID)
{
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee) return TEXT("Unknown");

    EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel);
    FString RankName = GetRankDisplayName(Rank);

    return FString::Printf(TEXT("%s %s"), *Employee->EmployeeName, *RankName);
}

int32 UEmployeeManager::GetMaxExperienceForLevel(int32 Level) const
{
    // 레벨에 따른 필요 경험치 공식
    // 예: Level 1 = 100, Level 2 = 150, Level 3 = 225...
    // 공식: BaseExp * (Level * 1.5)
    int32 BaseExp = 100;
    return FMath::FloorToInt(BaseExp * Level * 1.5f);
}

// 강화 기본 확률 (레벨별) - 게이지 100%일 때 기준
float UEmployeeManager::GetBaseEnhanceChance(int32 CurrentLevel)
{
    // 완만한 선형 감쇠 (밸런스 노브) — 구 곡선(레벨당 -10%p)이 너무 가팔라 교체 (2026-07-21)
    //   ★0: 95% → 레벨당 -5.5%p → ★14: 18% 바닥. 예: ★5 67.5% / ★9 45.5% / ★12 29%
    if (CurrentLevel >= MaxEnhancementLevel)
    {
        return 0.0f;
    }
    return FMath::Max(0.18f, 0.95f - 0.055f * CurrentLevel);
}

int32 UEmployeeManager::GetEmployeeEnhancementLevel(int32 EmployeeID)
{
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    return Employee ? Employee->EnhancementLevel : 0;
}

int64 UEmployeeManager::GetEnhanceCost(int32 EnhancementLevel)
{
    const int32 E = FMath::Clamp(EnhancementLevel, 0, MaxEnhancementLevel - 1);
    return static_cast<int64>(EnhanceCostBase * FMath::Pow(EnhanceCostGrowth, static_cast<float>(E)));
}

void UEmployeeManager::GetEnhanceOdds(int32 EnhancementLevel, float& OutSuccess, float& OutMaintain, float& OutDowngrade) const
{
    OutSuccess = const_cast<UEmployeeManager*>(this)->GetBaseEnhanceChance(EnhancementLevel);
    const float Fail = 1.0f - OutSuccess;
    // ★0~5 실패=유지, ★6+ 실패=하락 (파괴 없음)
    if (EnhancementLevel >= 6) { OutMaintain = 0.0f; OutDowngrade = Fail; }
    else                       { OutMaintain = Fail; OutDowngrade = 0.0f; }
}

bool UEmployeeManager::EnhanceEmployee(int32 TargetEmployeeID)
{
    FEmployeeInstance* TargetEmployee = FindEmployee(TargetEmployeeID);
    if (!TargetEmployee)
    {
        return false;
    }

    if (TargetEmployee->EnhancementLevel >= MaxEnhancementLevel)
    {
        if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIMgr->ShowNotification(NSLOCTEXT("Notification", "MaxStarReached", "이미 최대 강화입니다"), 3.0f, ENotificationType::Warning);
        }
        return false;
    }

    UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
    const int64 Cost = GetEnhanceCost(TargetEmployee->EnhancementLevel);
    if (!ResourceMgr || !ResourceMgr->HasResource(EResourceType::Money, Cost))
    {
        if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoneyEnhance", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
        }
        return false;
    }

    // bShouldSave=false + 지연 저장 — 연타 경로 (리롤/직능 리셋과 동일)
    ResourceMgr->SpendResource(EResourceType::Money, Cost, /*bShouldSave=*/false);

    const float SuccessChance = GetBaseEnhanceChance(TargetEmployee->EnhancementLevel);
    const bool bSuccess = FMath::FRand() <= SuccessChance;

    EEnhanceResult Result = EEnhanceResult::Maintain;
    if (bSuccess)
    {
        TargetEmployee->EnhancementLevel++;
        Result = EEnhanceResult::Success;
    }
    else if (TargetEmployee->EnhancementLevel >= 6)   // ★6+ 실패 = 하락 (파괴 없음)
    {
        TargetEmployee->EnhancementLevel--;
        Result = EEnhanceResult::Downgrade;
    }

    if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveLoadManager->RequestDeferredSave();
    }

    // 강화 레벨이 실제로 바뀐 경우(성공/하락)만 무효화 — 유지(Maintain)는 스탯 불변이라 불필요
    if (Result != EEnhanceResult::Maintain && TargetEmployee->AssignedBuildingIndex != INDEX_NONE)
    {
        if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
        {
            OpMgr->InvalidateStatBonusCache(TargetEmployee->AssignedBuildingIndex);
        }
    }

    OnEmployeeEnhanced.Broadcast(TargetEmployeeID, Result);
    OnEmployeeStatsChanged.Broadcast(TargetEmployeeID);   // 유효스탯(★×2)·별 표시 갱신
    return bSuccess;
}

FString UEmployeeManager::GenerateRandomName(ELootBoxRarity Rarity)
{
    // 현재는 한국어 고정 (다국어는 통짜 말장난이 번역 불가라 별도 설계 필요)
    const FString Language = TEXT("Korean");

    const FEmployeeHandleData* Row = PickNameRow(Language, Rarity);
    if (!Row)
    {
        return TEXT("Unknown");
    }

    if (Row->bIsFullName)
    {
        return ComposeEmployeeName(FString(), Row->Handle, true, Language);
    }

    const FString Surname = PickSurname(Language, Row->Handle);
    return ComposeEmployeeName(Surname, Row->Handle, false, Language);
}

EEmployeeGender UEmployeeManager::GenerateRandomGender()
{
    return FMath::RandBool() ? EEmployeeGender::Male : EEmployeeGender::Female;
}

const FEmployeeHandleData* UEmployeeManager::PickNameRow(const FString& Language, ELootBoxRarity Rarity)
{
    EnsureDataTablesLoaded();

    if (!FirstNameDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] FirstNameDataTable 없음"));
        return nullptr;
    }

    TArray<FEmployeeHandleData*> AllRows;
    FirstNameDataTable->GetAllRows<FEmployeeHandleData>(TEXT(""), AllRows);

    TArray<FEmployeeHandleData*> FullNames;
    TArray<FEmployeeHandleData*> Handles;
    for (FEmployeeHandleData* Row : AllRows)
    {
        if (!Row || Row->Language != Language)
        {
            continue;
        }

        if (Row->bIsFullName)
        {
            if (Row->Rarity == Rarity)
            {
                FullNames.Add(Row);
            }
        }
        else
        {
            Handles.Add(Row);
        }
    }

    // 등급 전용 통짜가 있으면 그것만 — 없으면 별명 핸들 공통 풀로 내려간다
    const TArray<FEmployeeHandleData*>& Pool = (FullNames.Num() > 0) ? FullNames : Handles;
    if (Pool.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] 이름 풀이 비어 있음 (Language %s, Rarity %d)"), *Language, (int32)Rarity);
        return nullptr;
    }

    return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

FString UEmployeeManager::PickSurname(const FString& Language, const FString& Handle)
{
    EnsureDataTablesLoaded();

    if (!LastNameDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] LastNameDataTable 없음"));
        return FString();
    }

    TArray<FLastNameData*> AllRows;
    LastNameDataTable->GetAllRows<FLastNameData>(TEXT(""), AllRows);

    TArray<FString> Candidates;
    for (FLastNameData* Row : AllRows)
    {
        if (Row && Row->Language == Language)
        {
            Candidates.Add(Row->LastName);
        }
    }

    if (Candidates.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] 성씨 풀이 비어 있음 (Language %s)"), *Language);
        return FString();
    }

    const TArray<FString> Filtered = FilterSurnamesForHandle(Candidates, Handle);
    return Filtered[FMath::RandRange(0, Filtered.Num() - 1)];
}

FCharacterAppearance UEmployeeManager::GenerateRandomAppearance(EEmployeeRank Rank, EEmployeeGender Gender)
{
    FCharacterAppearance Appearance;
    UTableManagerSubsystem* TableMgr = GetTableManager();

    if (!TableMgr) return Appearance;

    // 1. 눈썹 랜덤
    int32 EyebrowVariant = 0;
    if (Gender == EEmployeeGender::Male)
    {
        EyebrowVariant = FMath::RandRange(1, 3);
    }
    else
    {
        EyebrowVariant = FMath::RandRange(1, 2);
    }
    Appearance.EyebrowsPartName = FString::Printf(TEXT("brows%d"), EyebrowVariant);

    // 2. 머리 조합
    if (Gender == EEmployeeGender::Male)
    {
        Appearance.HairCombinationType = FMath::RandRange(1, 7); // 남성: 1-7
    }
    else
    {
        Appearance.HairCombinationType = FMath::RandRange(1, 8); // 여성: 1-8
    } // 머리 각 타입안에서 정해지는 랜덤시드 생성

    // 3. 눈 색깔 TableManager에서 가져오기
    if (FEyeColorTable* EyeData = TableMgr->GetRandomEyeColor())
    {
        Appearance.EyeColors = EyeData->ColorSet;
    }

    // 4. 피부 색깔 TableManager에서 가져오기
    if (FSkinColorTable* SkinData = TableMgr->GetRandomSkinColor())
    {
        Appearance.SkinColors = SkinData->ColorSet;
    }

    // 머리색 랜덤 설정
    if (FHairColorTable* HairColorData = TableMgr->GetRandomHairColor())
    {
        Appearance.HairColors = HairColorData->ColorSet;
    }

    // 5. 표정
    Appearance.FacialExpression = GenerateRandomFacialExpression();

    return Appearance;
}

FCharacterAppearance UEmployeeManager::GetEmployeeAppearance(int32 EmployeeID)
{
    if (FEmployeeAppearanceData* AppearanceData = EmployeeAppearances.Find(EmployeeID))
    {
        return AppearanceData->Appearance; // FCharacterAppearance 반환
    }


    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (Employee)
    {
        EEmployeeRank CurrentRank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel);
        FCharacterAppearance NewAppearance = GenerateRandomAppearance(CurrentRank, Employee->Gender);

        FEmployeeAppearanceData NewData;
        NewData.Appearance = NewAppearance;
        NewData.CurrentRank = CurrentRank;
        EmployeeAppearances.Add(EmployeeID, NewData);

        return NewAppearance;
    }

    return FCharacterAppearance();
}


FMorphTargetSet UEmployeeManager::GenerateRandomFacialExpression()
{
    FMorphTargetSet Expression;

    // 눈 표정 패턴 중 랜덤 선택
    int32 EyePattern = FMath::RandRange(1, 4);

    switch (EyePattern)
    {
    case 1: // 기본 눈 깜빡임
        Expression.MorphValues.Add("left eye blink 01", FMath::RandRange(0, 4) * 0.1f);
        Expression.MorphValues.Add("right eye blink 01", Expression.MorphValues["left eye blink 01"]);
        break;

    case 2: // 눈 크게 뜨기 + 깜빡임
        Expression.MorphValues.Add("left eye widen 01", FMath::FRandRange(0.0f, 1.0f));
        Expression.MorphValues.Add("right eye widen 01", Expression.MorphValues["left eye widen 01"]);
        Expression.MorphValues.Add("left eye blink 01", FMath::RandRange(0, 5) * 0.1f);
        Expression.MorphValues.Add("right eye blink 01", Expression.MorphValues["left eye blink 01"]);
        break;

    case 3: // 화난 눈 + 깜빡임
        Expression.MorphValues.Add("left eye angry", FMath::FRandRange(0.0f, 1.0f));
        Expression.MorphValues.Add("right eye angry", Expression.MorphValues["left eye angry"]);
        Expression.MorphValues.Add("left eye blink 01", FMath::RandRange(0, 3) * 0.1f);
        Expression.MorphValues.Add("right eye blink 01", Expression.MorphValues["left eye blink 01"]);
        break;

    case 4: // 처진 눈
        Expression.MorphValues.Add("left eye droopy", FMath::FRandRange(0.0f, 1.0f));
        Expression.MorphValues.Add("right eye droopy", Expression.MorphValues["left eye droopy"]);
        break;
    }

    // 독립적인 아래 눈꺼풀
    Expression.MorphValues.Add("left eye lower lid up", FMath::FRandRange(0.0f, 1.0f));
    Expression.MorphValues.Add("right eye lower lid up", Expression.MorphValues["left eye lower lid up"]);

    return Expression;
}

void UEmployeeManager::UpdateOfficeworkerAppearance(int32 EmployeeID)
{
    FCharacterAppearance Appearance = GetEmployeeAppearance(EmployeeID);

    // 직원 wjdqh
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee) return;

    EEmployeeRank CurrentRank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel);

    UWorld* World = GetWorld();
    if (!World) return;

    // Male과 Female 액터 각각 찾기
    AOfficeworkerMale* MaleActor = nullptr;
    AOfficeworkerFemale* FemaleActor = nullptr;

    for (TActorIterator<AOfficeworkerMale> ActorItr(World); ActorItr; ++ActorItr)
    {
        MaleActor = *ActorItr;
        break;
    }

    for (TActorIterator<AOfficeworkerFemale> ActorItr(World); ActorItr; ++ActorItr)
    {
        FemaleActor = *ActorItr;
        break;
    }

    if (!MaleActor || !FemaleActor) return;

    // 위치로 표시/숨김
    FVector CapturePosition = FVector(1000, 0, 0);
    FVector HiddenPosition = FVector(0, 0, -10000);

    if (Employee->Gender == EEmployeeGender::Male)
    {
        MaleActor->SetActorLocation(CapturePosition);
        FemaleActor->SetActorLocation(HiddenPosition);
        MaleActor->SetCharacterAppearance(Appearance, CurrentRank, Employee->Gender, EmployeeID);
    }
    else
    {
        MaleActor->SetActorLocation(HiddenPosition);
        FemaleActor->SetActorLocation(CapturePosition);
        FemaleActor->SetCharacterAppearance(Appearance, CurrentRank, Employee->Gender, EmployeeID);
    }
}

void UEmployeeManager::TestClothingRank(int32 EnhancementLevel)
{
    // 선택된 직원의 직급 변경
    int32 EmployeeID = GetSelectedEmployeeID();
    if (EmployeeID == -1)
    {
        UE_LOG(LogTemp, Warning, TEXT("No employee selected"));
        return;
    }

    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee)
    {
        UE_LOG(LogTemp, Warning, TEXT("Selected employee not found: %d"), EmployeeID);
        return;
    }

    // 강화 레벨 설정
    Employee->EnhancementLevel = EnhancementLevel;

    // 자동으로 랭크 계산됨
    EEmployeeRank NewRank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel);

    UE_LOG(LogTemp, Warning, TEXT("Employee %d (%s) enhancement level set to %d (Rank: %s)"),
        EmployeeID, *Employee->EmployeeName, Employee->EnhancementLevel, *GetRankDisplayName(NewRank));

    // 외모 업데이트 (의상 적용)
    UpdateOfficeworkerAppearance(EmployeeID);
}

void UEmployeeManager::ListEmployees()
{
    for (const auto& Employee : EmployeeList)
    {
        UE_LOG(LogTemp, Warning, TEXT("Employee ID: %d, Name: %s"), Employee.EmployeeID,
            *Employee.EmployeeName);
    }
}

void UEmployeeManager::CaptureEmployeePortrait(const FString& EmployeeID,
    const FCharacterAppearance& Appearance,
    EEmployeeRank Rank,
    EEmployeeGender Gender,
    int32 HairCombinationType,
    int32 RandomSeed,
    EEmployeeDepartment Department,
    EGachaTier Tier)
{
    FPortraitCaptureRequest Request;
    Request.EmployeeID = EmployeeID;
    Request.Appearance = Appearance;
    Request.Rank = Rank;
    Request.Gender = Gender;
    Request.HairCombinationType = HairCombinationType;
    Request.RandomSeed = RandomSeed;
    Request.Department = Department;
    Request.Tier = Tier;

    // 완료 브로드캐스트 구간의 재진입 요청도 적재만 — 실행 중인 워커 델리게이트 재바인딩 UB 방지(시작은 다음 틱 Dequeue)
    if (bPortraitCapturing || bInPortraitCompletion)
    {
        PendingPortraitQueue.Add(Request);
        return;
    }

    StartPortraitCapture(Request);
}

void UEmployeeManager::DequeueNextPortraitCapture()
{
    // 실패는 동기 반환이라 재귀 대신 루프로 다음 요청을 이어받는다(큐가 잠들면 채용 전체가 막힘)
    while (!bPortraitCapturing && PendingPortraitQueue.Num() > 0)
    {
        const FPortraitCaptureRequest NextRequest = PendingPortraitQueue[0];
        PendingPortraitQueue.RemoveAt(0);
        StartPortraitCapture(NextRequest);
    }
}

void UEmployeeManager::StartPortraitCapture(const FPortraitCaptureRequest& Request)
{
    const FString& EmployeeID = Request.EmployeeID;
    const EEmployeeGender Gender = Request.Gender;

    UE_LOG(LogTemp, Warning, TEXT("[Portrait] StartPortraitCapture EmployeeID: '%s' (대기 큐 %d)"), *EmployeeID, PendingPortraitQueue.Num());

    bPortraitCapturing = true;

    UWorld* World = GetWorld();
    if (!World)
    {
        bPortraitCapturing = false;
        return;
    }

    // 캐싱된 Worker가 있으면 재사용 (레벨 전환 시 무효화될 수 있으므로 IsValid 체크)
    AOfficeworker* Worker = nullptr;

    // 1순위: 스틱맨 캡쳐 워커 (유니섹스 — 성별 분기 없이 1개). bIsPortraitMode 로 레벨에 배치.
    if (!IsValid(StickPortraitWorker))
    {
        StickPortraitWorker = nullptr;
        for (TActorIterator<AStickOfficeworker> ActorItr(World); ActorItr; ++ActorItr)
        {
            if ((*ActorItr)->bIsPortraitMode)
            {
                StickPortraitWorker = *ActorItr;
                UE_LOG(LogTemp, Log, TEXT("[Portrait] Stick portrait worker found: %s"), *StickPortraitWorker->GetName());
                break;
            }
        }
    }

    if (StickPortraitWorker)
    {
        Worker = StickPortraitWorker;
    }
    else if (Gender == EEmployeeGender::Female)
    {
        // 폴백: 구 모듈러 여성 캡쳐 워커 (스틱 미배치 시 — strangler-fig)
        if (!IsValid(FemalePortraitWorker))
        {
            FemalePortraitWorker = nullptr;
            for (TActorIterator<AOfficeworkerFemale> ActorItr(World); ActorItr; ++ActorItr)
            {
                if ((*ActorItr)->bIsPortraitMode)
                {
                    FemalePortraitWorker = *ActorItr;
                    UE_LOG(LogTemp, Log, TEXT("[Portrait] Female portrait worker found: %s"), *FemalePortraitWorker->GetName());
                    break;
                }
            }
        }
        Worker = FemalePortraitWorker;
    }
    else
    {
        // 폴백: 구 모듈러 남성 캡쳐 워커 (스틱 미배치 시 — strangler-fig)
        if (!IsValid(MalePortraitWorker))
        {
            MalePortraitWorker = nullptr;
            for (TActorIterator<AOfficeworkerMale> ActorItr(World); ActorItr; ++ActorItr)
            {
                if ((*ActorItr)->bIsPortraitMode)
                {
                    MalePortraitWorker = *ActorItr;
                    UE_LOG(LogTemp, Log, TEXT("[Portrait] Male portrait worker found: %s"), *MalePortraitWorker->GetName());
                    break;
                }
            }
        }
        Worker = MalePortraitWorker;
    }

    if (!Worker)
    {
        UE_LOG(LogTemp, Error, TEXT("[Portrait] No portrait worker found for gender: %d"), (int32)Gender);
        // 동기 실패 — 플래그만 내리면 DequeueNextPortraitCapture 루프가 다음 요청을 이어받는다
        bPortraitCapturing = false;
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Portrait] Using worker: %s for EmployeeID: %s"), *Worker->GetName(), *EmployeeID);

    // 캡쳐 카메라/타깃 미설정이면 캡쳐가 조용히 실패해 bPortraitCapturing 이 영영 true → 채용 흐름 행.
    // 미리 막고 완료 통지로 흐름을 풀어준다(초상화는 없지만 게임은 진행). 새 스틱 BP 의 TextureTarget 미배선 대비.
    if (!Worker->FaceCaptureCamera || !Worker->FaceCaptureCamera->TextureTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("[Portrait] FaceCaptureCamera/TextureTarget 미설정 — 캡쳐 스킵: %s"), *Worker->GetName());
        // 동기 실패 — 플래그만 내리면 DequeueNextPortraitCapture 루프가 다음 요청을 이어받는다
        bPortraitCapturing = false;
        OnEmployeeHireCompleted.Broadcast(EmployeeID);
        return;
    }

    // 캡처 완료 델리게이트
    Worker->OnPortraitCaptured.BindLambda([this](const FString& CompletedID)
    {
        bInPortraitCompletion = true;
        bPortraitCapturing = false;
        UE_LOG(LogTemp, Log, TEXT("Portrait capture completed: %s"), *CompletedID);

        // 고용 완료 이벤트 브로드캐스트 (UI 쿨타임 종료용)
        OnEmployeeHireCompleted.Broadcast(CompletedID);

        bInPortraitCompletion = false;

        // 실행 중인 단일캐스트 델리게이트를 같은 콜스택에서 재바인딩하면 UB — 한 틱 미뤄 콜스택을 벗어난다
        if (UWorld* CurWorld = GetWorld())
        {
            CurWorld->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateUObject(this, &UEmployeeManager::DequeueNextPortraitCapture));
        }
        else if (PendingPortraitQueue.Num() > 0)
        {
            // 월드 없음(레벨 전환 중) = 다음 틱 예약 불가 → 큐를 비워 채용이 영구 차단되는 것만 막는다.
            // 초상화는 오피스 입장 backfill 이 다시 찍는다.
            UE_LOG(LogTemp, Error, TEXT("[Portrait] 월드 없음 — 대기 큐 %d건 폐기(backfill 로 복구)"), PendingPortraitQueue.Num());
            PendingPortraitQueue.Empty();
        }
    });

    // 메시 로드 완료 델리게이트
    Worker->OnMeshLoadCompleted.BindLambda([this, Worker]()
    {
        UE_LOG(LogTemp, Log, TEXT("Mesh loaded, setting up capture..."));
        Worker->SetupFaceCapture();

        // Material 적용을 위해 반복 체크
        TWeakObjectPtr<AOfficeworker> WeakWorker(Worker);

        TSharedPtr<FTimerHandle> MaterialCheckTimerPtr = MakeShared<FTimerHandle>();
        TSharedPtr<int32> MaterialCheckCountPtr = MakeShared<int32>(0);
        const int32 MaxMaterialChecks = 15;  // 최대 1.5초

        GetWorld()->GetTimerManager().SetTimer(*MaterialCheckTimerPtr,
            [WeakWorker, MaterialCheckTimerPtr, MaterialCheckCountPtr, MaxMaterialChecks, this]()
            {
                if (!WeakWorker.IsValid()) return;

                (*MaterialCheckCountPtr)++;
                AOfficeworker* W = WeakWorker.Get();

                // 머티리얼이 준비되었는지 확인
                bool bMaterialsReady = true;
                if (W->GetMesh() && W->GetMesh()->GetNumMaterials() > 0)
                {
                    UMaterialInterface* Mat = W->GetMesh()->GetMaterial(0);
                    if (!Mat)
                    {
                        bMaterialsReady = false;
                        UE_LOG(LogTemp, Warning, TEXT("Material check %d: Materials not ready"), *MaterialCheckCountPtr);
                    }
                }
                else
                {
                    bMaterialsReady = false;
                    UE_LOG(LogTemp, Warning, TEXT("Material check %d: No materials found"), *MaterialCheckCountPtr);
                }

                if (bMaterialsReady || *MaterialCheckCountPtr >= MaxMaterialChecks)
                {
                    if (*MaterialCheckCountPtr >= MaxMaterialChecks)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[Warning] Material check timeout after %d checks, capturing anyway"), *MaterialCheckCountPtr);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Log, TEXT("Materials ready after %d checks (%.2fs)"),
                            *MaterialCheckCountPtr, *MaterialCheckCountPtr * 0.1f);
                    }

                    GetWorld()->GetTimerManager().ClearTimer(*MaterialCheckTimerPtr);
                    UE_LOG(LogTemp, Warning, TEXT("[Portrait] About to capture portrait (Worker has EmployeeID)"));
                    W->CaptureAndSaveFacePortrait();
                }
            },
            0.1f, true);  // 0.1초마다 체크
    });

    // 외형 적용: 스틱 = 코스메틱 단일 진입점(메시 이미 로드 → OnMeshLoadCompleted 동기 발화),
    //            구 모듈러 = 비동기 헤어/모프 후 로드 완료 시 발화. 두 경로 모두 위 바인딩을 트리거.
    int32 EmployeeIDInt = FCString::Atoi(*EmployeeID);
    if (AStickOfficeworker* StickWorker = Cast<AStickOfficeworker>(Worker))
    {
        StickWorker->ApplyCosmeticsForPortrait(Request.Department, Request.Rank, Request.Tier, EmployeeIDInt);
    }
    else if (AModularOfficeworker* ModularWorker = Cast<AModularOfficeworker>(Worker))
    {
        ModularWorker->ApplyAppearanceAndNotify(Request.Appearance, Request.Rank, Gender,
            Request.HairCombinationType, Request.RandomSeed, EmployeeIDInt);
    }
    else
    {
        // 외형 미구현 베이스 타입 — 초상화 플로우가 조용히 멈추지 않게 loud 처리
        UE_LOG(LogTemp, Error, TEXT("[Portrait] 외형 경로 없는 워커 타입: %s"), *Worker->GetClass()->GetName());
        Worker->OnMeshLoadCompleted.ExecuteIfBound();
    }
}

// ========== 건물별 직원 관리 (저장/로드용) ==========

TArray<FEmployeeInstance> UEmployeeManager::GetEmployeesByBuilding(int32 BuildingIndex) const
{
    TArray<FEmployeeInstance> Result;

    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.AssignedBuildingIndex == BuildingIndex)
        {
            Result.Add(Employee);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] GetEmployeesByBuilding(%d): Found %d employees"), BuildingIndex, Result.Num());
    return Result;
}

TMap<int32, FCharacterAppearance> UEmployeeManager::GetAppearancesByBuilding(int32 BuildingIndex) const
{
    TMap<int32, FCharacterAppearance> Result;

    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.AssignedBuildingIndex == BuildingIndex)
        {
            if (const FEmployeeAppearanceData* AppData = EmployeeAppearances.Find(Employee.EmployeeID))
            {
                Result.Add(Employee.EmployeeID, AppData->Appearance);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] GetAppearancesByBuilding(%d): Found %d appearances"), BuildingIndex, Result.Num());
    return Result;
}

void UEmployeeManager::SetEmployeesForBuilding(int32 BuildingIndex, const TArray<FEmployeeInstance>& Employees)
{
    // 기존 해당 건물 직원 제거
    EmployeeList.RemoveAll([BuildingIndex](const FEmployeeInstance& Emp)
    {
        return Emp.AssignedBuildingIndex == BuildingIndex;
    });

    // 새 직원 추가 (ID 중복 체크)
    int32 SkippedCount = 0;
    for (const FEmployeeInstance& Employee : Employees)
    {
        // 다른 건물에 이미 같은 ID의 직원이 있으면 스킵
        bool bDuplicate = EmployeeList.ContainsByPredicate([&](const FEmployeeInstance& Existing)
        {
            return Existing.EmployeeID == Employee.EmployeeID;
        });

        if (bDuplicate)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] SetEmployeesForBuilding(%d): Skipped duplicate EmployeeID %d"),
                BuildingIndex, Employee.EmployeeID);
            SkippedCount++;
            continue;
        }

        FEmployeeInstance NewEmployee = Employee;
        NewEmployee.AssignedBuildingIndex = BuildingIndex;
        // bIsAssigned 는 역직렬화된 값을 그대로 신뢰 — 벤치(false)/착석(true) 구분 보존 (pull-to-pool)
        EmployeeList.Add(NewEmployee);
    }

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] SetEmployeesForBuilding(%d): Set %d employees (skipped %d duplicates)"),
        BuildingIndex, Employees.Num() - SkippedCount, SkippedCount);
}

void UEmployeeManager::SetAppearancesForBuilding(int32 BuildingIndex, const TMap<int32, FCharacterAppearance>& Appearances)
{
    for (const auto& Pair : Appearances)
    {
        FEmployeeAppearanceData AppData;
        AppData.Appearance = Pair.Value;

        // 직원의 현재 랭크 설정
        if (FEmployeeInstance* Employee = FindEmployee(Pair.Key))
        {
            AppData.CurrentRank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel);
        }

        EmployeeAppearances.Add(Pair.Key, AppData);
    }

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] SetAppearancesForBuilding(%d): Set %d appearances"), BuildingIndex, Appearances.Num());
}

void UEmployeeManager::ClearEmployeesForBuilding(int32 BuildingIndex)
{
    // 해당 건물 직원들의 외모 데이터 제거
    TArray<int32> EmployeeIDsToRemove;
    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        if (Employee.AssignedBuildingIndex == BuildingIndex)
        {
            EmployeeIDsToRemove.Add(Employee.EmployeeID);
        }
    }

    for (int32 ID : EmployeeIDsToRemove)
    {
        EmployeeAppearances.Remove(ID);
    }

    // 해당 건물 직원 제거
    int32 RemovedCount = EmployeeList.RemoveAll([BuildingIndex](const FEmployeeInstance& Emp)
    {
        return Emp.AssignedBuildingIndex == BuildingIndex;
    });

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] ClearEmployeesForBuilding(%d): Removed %d employees"), BuildingIndex, RemovedCount);
}

// ========== 경험치 시스템 ==========

float UEmployeeManager::GetExpGainMultiplier(const FEmployeeInstance& Employee) const
{
    // 유효스탯(저장값 + ★ 파생)을 써야 강화 이득이 스탯 경로 단일로 유지된다
    const int32 EffectiveExpGain = Employee.Stats.ExpGain + UEmployeeTypeHelper::GetEnhanceStatBonus(Employee.EnhancementLevel);
    return 1.0f + (EffectiveExpGain * EmployeeStatTuning::ExpGainMultiplierPerPoint);
}

bool UEmployeeManager::AddExperience(int32 EmployeeID, float Amount)
{
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] AddExperience - Employee %d not found"), EmployeeID);
        return false;
    }

    if (Amount <= 0.0f)
    {
        return false;
    }

    const float ExpMultiplier = GetExpGainMultiplier(*Employee);
    float FinalAmount = Amount * ExpMultiplier;

    Employee->Experience += FinalAmount;

    // 경험치 획득 이벤트 브로드캐스트
    OnExperienceGained.Broadcast(EmployeeID, FinalAmount);

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Employee %d gained %.1f EXP (%.1f base x %.2f multiplier). Total: %.1f"),
        EmployeeID, FinalAmount, Amount, ExpMultiplier, Employee->Experience);

    // 레벨업 처리 (다중 레벨업 허용, MaxExp<=0 가드로 무한루프 방지)
    bool bLeveledUp = false;
    while (true)
    {
        const int32 MaxExp = GetMaxExperienceForLevel(Employee->Level);
        if (MaxExp <= 0 || Employee->Experience < MaxExp)
        {
            break;
        }

        Employee->Experience -= MaxExp;  // 초과분 이월
        Employee->Level++;

        Employee->AvailableSkillPoints += SkillPointsPerLevel;   // 6직능 투자용 SP 적립

        bLeveledUp = true;

        UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Employee %d leveled up to Lv.%d"), EmployeeID, Employee->Level);
    }

    if (bLeveledUp)
    {
        const int32 FinalLevel = Employee->Level;
        // 지연 저장 — XP 분배로 여러 직원이 연쇄 레벨업하는 경로, 즉시 저장이면 레벨업마다 히칭
        if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        {
            SaveLoadManager->RequestDeferredSave();
        }
        OnEmployeeLevelUp.Broadcast(EmployeeID, FinalLevel);
        OnEmployeeStatsChanged.Broadcast(EmployeeID);
        PlayLevelUpEffects(EmployeeID, FinalLevel);
    }

    return true;
}

float UEmployeeManager::DistributeExperienceToBuilding(int32 BuildingIndex, float BaseAmount, EQualityGrade QualityGrade)
{
    // 품질 등급에 따른 배율 적용
    float QualityMultiplier = GetQualityMultiplier(QualityGrade);
    float AdjustedAmount = BaseAmount * QualityMultiplier;

    // 빌딩 강화 EmployeeGrowth 배율 적용 (Phase 4) — 빌딩에 배치된 직원들 ExpGain 증가
    {
        float GrowthMult = 1.0f;
        if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
        {
            if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
            {
                if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
                {
                    for (const FBuildingEntitySaveData& B : SaveData->GameData.Buildings)
                    {
                        if (B.BuildingIndex == BuildingIndex)
                        {
                            const int32 Lv = B.BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::EmployeeGrowth);
                            GrowthMult = UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType::EmployeeGrowth, Lv);
                            break;
                        }
                    }
                }
            }
        }
        AdjustedAmount *= GrowthMult;
    }

    // 해당 빌딩의 모든 직원 가져오기
    TArray<FEmployeeInstance> BuildingEmployees = GetEmployeesInBuilding(BuildingIndex);

    if (BuildingEmployees.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeManager] DistributeExperienceToBuilding - No employees in building %d"), BuildingIndex);
        return 0.0f;
    }

    int32 DistributedCount = 0;
    for (const FEmployeeInstance& Employee : BuildingEmployees)
    {
        // 큐브 ExpGain 줄 — 직원별 XP 배율
        const float ExpMult = UEmployeePotentialHelper::AggregateModifiers(Employee.PotentialAbility).ExpMult;
        if (AddExperience(Employee.EmployeeID, AdjustedAmount * ExpMult))
        {
            DistributedCount++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[EmployeeManager] Distributed %.1f EXP (%.1f base x %.2f quality) to %d employees in building %d"),
        AdjustedAmount, BaseAmount, QualityMultiplier, DistributedCount, BuildingIndex);

    return AdjustedAmount;
}

float UEmployeeManager::GetQualityMultiplier(EQualityGrade Grade) const
{
    // 품질 등급별 경험치 배율
    // F: 0.3x, D: 0.5x, C: 0.8x, B: 1.0x, A: 1.3x, S: 1.5x
    switch (Grade)
    {
    case EQualityGrade::F: return 0.3f;
    case EQualityGrade::D: return 0.5f;
    case EQualityGrade::C: return 0.8f;
    case EQualityGrade::B: return 1.0f;
    case EQualityGrade::A: return 1.3f;
    case EQualityGrade::S: return 1.5f;
    default: return 1.0f;
    }
}

// ========== 레벨업 이펙트 ==========

void UEmployeeManager::PlayLevelUpEffects(int32 EmployeeID, int32 NewLevel)
{
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee) return;

    // 워커 위치에 Niagara 스폰 (미스폰 시 스킵)
    const TArray<AOfficeworker*> Workers = GetSpawnedWorkersInBuilding(Employee->AssignedBuildingIndex);
    for (AOfficeworker* Worker : Workers)
    {
        if (Worker && Worker->GetEmployeeID() == EmployeeID)
        {
            if (UNiagaraSystem* VFX = LevelUpVFX.LoadSynchronous())
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                    GetWorld(),
                    VFX,
                    Worker->GetActorLocation(),
                    FRotator::ZeroRotator,
                    FVector(1.0f),
                    true
                );
            }
            break;
        }
    }

    // 알림 문구
    if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        const FText Msg = FText::FromString(
            FString::Printf(TEXT("레벨 업! %s Lv.%d"), *Employee->EmployeeName, NewLevel));
        UIMgr->ShowNotification(Msg, 2.5f, ENotificationType::Success);
    }
}

bool UEmployeeManager::InvestDisciplinePoint(int32 EmployeeID, EProductionDiscipline Discipline)
{
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee || Employee->AvailableSkillPoints <= 0)
    {
        return false;
    }

    const int32 Idx = static_cast<int32>(Discipline);
    const int32 N = static_cast<int32>(EProductionDiscipline::Count);
    if (Idx < 0 || Idx >= N)
    {
        return false;
    }

    // 배열 크기 보정(방금 생성 직원의 미시딩 대비)
    if (Employee->DisciplinePoints.Num() != N)          { Employee->DisciplinePoints.SetNumZeroed(N); }
    if (Employee->InvestedDisciplinePoints.Num() != N)  { Employee->InvestedDisciplinePoints.SetNumZeroed(N); }

    Employee->DisciplinePoints[Idx]         += 1;
    Employee->InvestedDisciplinePoints[Idx] += 1;
    Employee->AvailableSkillPoints          -= 1;
    SyncDerivedDepartment(*Employee);

    if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveLoadManager->RequestDeferredSave();
    }

    // G9 걸음 전이는 이 신호가 아니라 InvestedDisciplinePoints 파생값이 판정한다 (세이브를 타 재접속에도 유지)
    OnEmployeeDisciplineChanged.Broadcast(EmployeeID);
    return true;
}

bool UEmployeeManager::ResetDisciplinePoints(int32 EmployeeID)
{
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee)
    {
        return false;
    }

    const int32 N = static_cast<int32>(EProductionDiscipline::Count);
    if (Employee->InvestedDisciplinePoints.Num() != N)
    {
        return false;   // 투자 이력 없음
    }

    int32 TotalInvested = 0;
    for (int32 v : Employee->InvestedDisciplinePoints) { TotalInvested += v; }
    if (TotalInvested <= 0)
    {
        return false;   // 되돌릴 것 없음 → 다이아 차감 안 함
    }

    UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
    if (!ResourceMgr || !ResourceMgr->HasResource(EResourceType::Diamond, DisciplineResetDiamondCost))
    {
        return false;
    }

    // bShouldSave=false + 지연 저장 (리롤과 동일 — 클릭당 전체 세이브 방지)
    ResourceMgr->SpendResource(EResourceType::Diamond, DisciplineResetDiamondCost, /*bShouldSave=*/false);

    for (int32 i = 0; i < N; ++i)
    {
        Employee->DisciplinePoints[i]        -= Employee->InvestedDisciplinePoints[i];   // 이너트 복원
        Employee->AvailableSkillPoints       += Employee->InvestedDisciplinePoints[i];   // SP 반환
        Employee->InvestedDisciplinePoints[i] = 0;
    }
    SyncDerivedDepartment(*Employee);

    if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveLoadManager->RequestDeferredSave();
    }
    OnEmployeeDisciplineChanged.Broadcast(EmployeeID);
    return true;
}

bool UEmployeeManager::RerollEmployeePotential(int32 EmployeeID, EItemType CubeType)
{
    FEmployeeInstance* Employee = FindEmployee(EmployeeID);
    if (!Employee)
    {
        return false;
    }

    // 명함 3종 외 타입은 리롤 불가 (GetCubeCeiling 폴백값 Common과 구분해 여기서 명시적으로 걸러낸다)
    if (CubeType != EItemType::BusinessCardPaper && CubeType != EItemType::BusinessCardGold && CubeType != EItemType::BusinessCardBlack)
    {
        return false;
    }

    UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
    if (!ItemMgr || !ItemMgr->HasItem(CubeType, 1))
    {
        return false;
    }

    const int32 Slots = UEmployeePotentialHelper::GetPotentialSlotsForRank(
        UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel));
    const ELootBoxRarity Ceiling = UEmployeePotentialHelper::GetCubeCeiling(CubeType);

    // bShouldSave=false + 지연 저장 — 리롤은 연타 경로, 기본값이면 클릭당 전체 세이브 2회
    ItemMgr->SpendItem(CubeType, 1, /*bShouldSave=*/false);
    UEmployeePotentialHelper::ResetPotentialAbility(Employee->PotentialAbility, Slots, Ceiling);

    if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveLoadManager->RequestDeferredSave();
    }

    // 잠재 수익줄이 운영 요율(StatBonus)에 들어가므로 리롤은 캐시를 무효화해야 한다 — 안 하면 다음 배치까지 옛 값이 산다.
    if (Employee->AssignedBuildingIndex != INDEX_NONE)
    {
        if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
        {
            OpMgr->InvalidateStatBonusCache(Employee->AssignedBuildingIndex);
        }
    }

    OnEmployeeStatsChanged.Broadcast(EmployeeID);
    return true;
}

// ========== 경험치 디버깅 명령어 ==========

void UEmployeeManager::TestAddExp(int32 Amount, int32 EmployeeID)
{
    // EmployeeID가 -1이면 선택된 직원 사용
    int32 TargetID = (EmployeeID == -1) ? GetSelectedEmployeeID() : EmployeeID;

    if (TargetID == -1)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TestAddExp] No employee selected. Use: TestAddExp [Amount] [EmployeeID]"));
        return;
    }

    FEmployeeInstance* Employee = FindEmployee(TargetID);
    if (!Employee)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TestAddExp] Employee %d not found"), TargetID);
        return;
    }

    float OldExp = Employee->Experience;
    int32 MaxExp = GetMaxExperienceForLevel(Employee->Level);

    AddExperience(TargetID, static_cast<float>(Amount));

    UE_LOG(LogTemp, Warning, TEXT("[TestAddExp] %s (ID:%d): %.1f -> %.1f EXP (Max: %d, Level: %d)"),
        *Employee->EmployeeName, TargetID, OldExp, Employee->Experience, MaxExp, Employee->Level);

    // 레벨업 가능 여부 체크
    if (Employee->Experience >= MaxExp)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TestAddExp] >>> LEVEL UP AVAILABLE! Current: Lv.%d, Exp: %.1f/%d"),
            Employee->Level, Employee->Experience, MaxExp);
    }
}

void UEmployeeManager::TestDistributeExp(int32 Amount, int32 QualityGradeValue)
{
    UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
    if (!GI)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TestDistributeExp] GameInstance not found"));
        return;
    }

    int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
    if (BuildingIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TestDistributeExp] No building selected"));
        return;
    }

    // QualityGrade 변환 (0=F, 1=D, 2=C, 3=B, 4=A, 5=S)
    EQualityGrade Grade = static_cast<EQualityGrade>(FMath::Clamp(QualityGradeValue, 0, 5));

    UE_LOG(LogTemp, Warning, TEXT("[TestDistributeExp] Distributing %d EXP (Grade: %s) to building %d"),
        Amount, *QualityGradeToAlphabetString(Grade), BuildingIndex);

    DistributeExperienceToBuilding(BuildingIndex, static_cast<float>(Amount), Grade);
}

void UEmployeeManager::ShowExpStatus()
{
    UE_LOG(LogTemp, Warning, TEXT("========== Employee EXP Status =========="));

    for (const FEmployeeInstance& Employee : EmployeeList)
    {
        int32 MaxExp = GetMaxExperienceForLevel(Employee.Level);
        float Progress = (MaxExp > 0) ? (Employee.Experience / MaxExp * 100.0f) : 0.0f;
        bool bCanLevelUp = Employee.Experience >= MaxExp;

        UE_LOG(LogTemp, Warning, TEXT("  [%d] %s | Lv.%d | EXP: %.1f/%d (%.1f%%) | Building: %d %s"),
            Employee.EmployeeID,
            *Employee.EmployeeName,
            Employee.Level,
            Employee.Experience,
            MaxExp,
            Progress,
            Employee.AssignedBuildingIndex,
            bCanLevelUp ? TEXT(">>> LEVEL UP!") : TEXT(""));
    }

    UE_LOG(LogTemp, Warning, TEXT("========================================="));
}

void UEmployeeManager::SeedEmployeesForBuilding(int32 BuildingIndex, int32 Count,
    int32 LevelMin, int32 LevelMax, int32 EnhanceMin, int32 EnhanceMax, int32 RandomSeed)
{
#if !UE_BUILD_SHIPPING
    // 상한을 넘겨 시드하면 CanHireIntoBuilding 이 거부할 상태를 프리셋이 만들어 낸다 —
    // 플레이로는 도달 불가능한 세이브를 밸런스 하네스가 정상으로 읽으므로 여기서 자른다.
    // 키스톤(상한 0)은 자연히 0명이 된다.
    const int32 Capacity = GetBuildingEmployeeCapacity(BuildingIndex, /*bLogIfZero*/ false);
    const int32 SeedCount = FMath::Clamp(Count, 0, Capacity);
    if (SeedCount < Count)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] 빌딩%d 요청 %d명 → 인원 상한 %d명으로 절삭"),
            BuildingIndex, Count, Capacity);
    }

    // 빌딩마다 다른 로스터가 나오되 실행할 때마다는 같도록 — 프리셋 재현성(R2)의 핵심
    FRandomStream Rng(RandomSeed + BuildingIndex * 977);

    TArray<FEmployeeInstance> Seeded;
    Seeded.Reserve(SeedCount);

    for (int32 i = 0; i < SeedCount; ++i)
    {
        FEmployeeInstance Emp;
        Emp.EmployeeID = AllocateNextInstanceID();
        Emp.AssignedBuildingIndex = BuildingIndex;
        // false = 벤치. 벤치 직원은 워커 액터도 초상화 backfill 도 안 돈다.
        Emp.bIsAssigned = true;
        Emp.Level = Rng.RandRange(FMath::Min(LevelMin, LevelMax), FMath::Max(LevelMin, LevelMax));
        Emp.EnhancementLevel = FMath::Clamp(
            Rng.RandRange(FMath::Min(EnhanceMin, EnhanceMax), FMath::Max(EnhanceMin, EnhanceMax)),
            0, MaxEnhancementLevel);

        // 등급 분포 — 중반 로스터답게 Common 이 많고 상위가 드물게
        const int32 RarityRoll = Rng.RandRange(0, 99);
        const ELootBoxRarity Rarity =
            (RarityRoll < 45) ? ELootBoxRarity::Common :
            (RarityRoll < 70) ? ELootBoxRarity::Unusual :
            (RarityRoll < 88) ? ELootBoxRarity::Rare :
            (RarityRoll < 97) ? ELootBoxRarity::Epic : ELootBoxRarity::Legendary;
        Emp.SpawnRarity = Rarity;

        // 가챠와 같은 규칙을 써야 값 분포가 실제 플레이와 일치한다
        const EProductionDiscipline Primary =
            static_cast<EProductionDiscipline>(Rng.RandRange(0, static_cast<int32>(EProductionDiscipline::Count) - 1));
        Emp.DisciplinePoints = MakeDisciplineProfile(Primary, Rarity);
        Emp.InvestedDisciplinePoints.Init(0, Emp.DisciplinePoints.Num());
        ApplyStatProfile(Emp.Stats, MakeStatProfile());
        Emp.Department = GetDerivedDepartment(Emp);

        Emp.EmployeeName = FString::Printf(TEXT("사원 %d-%d"), BuildingIndex, i + 1);

        Seeded.Add(Emp);
    }

    SetEmployeesForBuilding(BuildingIndex, Seeded);

    // SetEmployeesForBuilding 은 아무것도 브로드캐스트하지 않는다
    OnEmployeeRosterChanged.Broadcast();

    UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] 빌딩%d 직원 %d명 시드"), BuildingIndex, Seeded.Num());
#endif
}