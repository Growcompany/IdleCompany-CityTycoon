#include "Manager/RankingManagerSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/EntityManager.h"
#include "Manager/EmployeeManager.h"
#include "Core/CGGameInstance.h"
#include "Data/GameSaveData.h"
#include "Data/EntitySaveData.h"
#include "Data/BuildingSaveData.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

const FString URankingManagerSubsystem::StatName_Weekly = TEXT("WeeklyRevenue");
const FString URankingManagerSubsystem::StatName_AllTime = TEXT("MarketCap");

void URankingManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PlayFabMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>();

	BindPlayFabDelegates();

	UE_LOG(LogTemp, Log, TEXT("[RankingManager] 초기화 완료"));
}

void URankingManagerSubsystem::Deinitialize()
{
	FlushWeeklyRevenue(true);   // 배치 대기분 유실 방지
	UnbindPlayFabDelegates();

	Super::Deinitialize();
}

// ===== PlayFab 델리게이트 바인딩 =====

void URankingManagerSubsystem::BindPlayFabDelegates()
{
	if (PlayFabMgr)
	{
		PlayFabMgr->OnLeaderboardResult.AddUObject(this, &URankingManagerSubsystem::HandleLeaderboardResult);
		PlayFabMgr->OnOtherPlayerDataLoaded.AddUObject(this, &URankingManagerSubsystem::HandleOtherPlayerData);
		PlayFabMgr->OnError.AddUObject(this, &URankingManagerSubsystem::HandlePlayFabError);
	}
}

void URankingManagerSubsystem::UnbindPlayFabDelegates()
{
	if (PlayFabMgr)
	{
		PlayFabMgr->OnLeaderboardResult.RemoveAll(this);
		PlayFabMgr->OnOtherPlayerDataLoaded.RemoveAll(this);
		PlayFabMgr->OnError.RemoveAll(this);
	}
}

// ===== 시총/주간매출 업로드 =====

// 시총 현재 값을 MarketCap 리더보드에 업로드 (Last 모드)
// SaveLoadManager::OnResourceChangedHandler 에서 MarketCap 변경 시마다 호출
void URankingManagerSubsystem::UploadMarketCap(int64 MarketCapValue)
{
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn()) return;

	PlayFabMgr->UpdateLeaderboardScore(StatName_AllTime, MarketCapValue);
	UE_LOG(LogTemp, Log, TEXT("[RankingManager] 시총 업로드 (%lld)"), MarketCapValue);
}

// 주간매출 증가분을 WeeklyRevenue 리더보드에 업로드 (Sum 모드, Weekly Reset)
// Money 자원이 증가할 때만 양수 Delta 호출 — 지출(감소) 시엔 호출 안 함
void URankingManagerSubsystem::UploadWeeklyRevenueDelta(int64 Delta)
{
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn()) return;
	if (Delta <= 0) return;

	// 즉시 전송 금지 — 방치 수익은 Money 를 초당 여러 번 올려서 그대로 쏘면 HTTP 가 초당 N건이 된다.
	// Sum 모드 리더보드라 누산 후 한 번에 보내도 최종값이 같다.
	PendingWeeklyDelta += Delta;
	FlushWeeklyRevenue(false);
}

void URankingManagerSubsystem::FlushWeeklyRevenue(bool bForce)
{
	if (PendingWeeklyDelta <= 0) return;
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn()) return;

	const double Now = FPlatformTime::Seconds();
	if (!bForce && (Now - LastWeeklyFlushTime) < UploadThrottleSeconds) return;
	LastWeeklyFlushTime = Now;

	PlayFabMgr->UpdateLeaderboardScore(StatName_Weekly, PendingWeeklyDelta);
	UE_LOG(LogTemp, Verbose, TEXT("[RankingManager] 주간매출 +%lld 업로드(배치)"), PendingWeeklyDelta);
	PendingWeeklyDelta = 0;
}

// 레거시 호환 — 기존 호출부가 있을 경우 프로필 + 시총 현재값 업로드로 대체
void URankingManagerSubsystem::UploadRevenueScore()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn() || !SaveMgr) return;

	const double Now = FPlatformTime::Seconds();
	if (Now - LastScoreUploadTime < UploadThrottleSeconds) return;
	LastScoreUploadTime = Now;

	if (UResourceItemManager* RMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		UploadMarketCap(RMgr->GetResourceAmount(EResourceType::MarketCap));
	}
	UploadRankingProfile();
}

void URankingManagerSubsystem::ForceUploadRevenueScore()
{
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn()) return;

	LastScoreUploadTime = FPlatformTime::Seconds();

	if (UResourceItemManager* RMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		UploadMarketCap(RMgr->GetResourceAmount(EResourceType::MarketCap));
	}
	UploadRankingProfile();
}

// ===== 리더보드 조회 =====

void URankingManagerSubsystem::FetchLeaderboard(int32 TabIndex, int32 Count)
{
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn())
	{
		return;
	}

	PendingTabIndex = TabIndex;
	// Tab 0 = 시총, Tab 1 = 주간매출
	const FString& StatName = (TabIndex == 0) ? StatName_AllTime : StatName_Weekly;
	PlayFabMgr->GetLeaderboard(StatName, Count);
}

void URankingManagerSubsystem::FetchLeaderboardAroundPlayer(int32 TabIndex, int32 Count)
{
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn())
	{
		return;
	}

	PendingTabIndex = TabIndex;
	// Tab 0 = 시총, Tab 1 = 주간매출
	const FString& StatName = (TabIndex == 0) ? StatName_AllTime : StatName_Weekly;
	PlayFabMgr->GetLeaderboardAroundPlayer(StatName, Count);
}

// ===== 도시 스냅샷 =====

void URankingManagerSubsystem::UploadCitySnapshot()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn() || !SaveMgr)
	{
		return;
	}

	// 쓰로틀 체크
	const double Now = FPlatformTime::Seconds();
	if (Now - LastSnapshotUploadTime < UploadThrottleSeconds)
	{
		return;
	}
	LastSnapshotUploadTime = Now;

	FCitySnapshot Snapshot = BuildCitySnapshot();

	FString JsonStr;
	if (FJsonObjectConverter::UStructToJsonObjectString(Snapshot, JsonStr))
	{
		PlayFabMgr->SavePlayerData(TEXT("CitySnapshot"), JsonStr);
		UE_LOG(LogTemp, Log, TEXT("[RankingManager] 도시 스냅샷 업로드 (빌딩 %d개, %d bytes)"),
			Snapshot.Buildings.Num(), JsonStr.Len());
	}
}

void URankingManagerSubsystem::FetchCitySnapshot(const FString& TargetPlayFabId)
{
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn())
	{
		return;
	}

	TArray<FString> Keys;
	Keys.Add(TEXT("CitySnapshot"));
	PlayFabMgr->GetOtherPlayerData(TargetPlayFabId, Keys);
}

FCitySnapshot URankingManagerSubsystem::BuildCitySnapshot() const
{
	FCitySnapshot Snapshot;

	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr)
	{
		return Snapshot;
	}

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData)
	{
		return Snapshot;
	}

	const FGameSaveData& GD = SaveData->GameData;
	Snapshot.HQLevel = GD.HQLevel;
	Snapshot.CompanyTitle = GD.CompanyTitle;
	Snapshot.TotalRevenueEarned = GD.TotalRevenueEarned;

	// 직원 수 합산
	int32 TotalEmp = 0;
	for (const auto& Pair : GD.OfficeDataMap)
	{
		TotalEmp += Pair.Value.EmployeeList.Num();
	}
	Snapshot.TotalEmployees = TotalEmp;

	// 빌딩 데이터 추출
	for (const FBuildingEntitySaveData& BuildingEntity : GD.Buildings)
	{
		FCitySnapshotBuilding SnapBuilding;
		SnapBuilding.InteractableName = BuildingEntity.InteractableName;
		SnapBuilding.Location = BuildingEntity.Location;
		SnapBuilding.Rotation = BuildingEntity.Rotation;
		SnapBuilding.Scale = BuildingEntity.Scale;
		SnapBuilding.Body_Module_Copies = BuildingEntity.BuildingData.Body_Module_Copies;
		SnapBuilding.AppliedSkinID = BuildingEntity.BuildingData.AppliedSkinID;
		SnapBuilding.CompanyType = BuildingEntity.BuildingData.CompanyType;
		Snapshot.Buildings.Add(MoveTemp(SnapBuilding));
	}

	return Snapshot;
}

// ===== 방문 모드 =====

void URankingManagerSubsystem::EnterVisitMode(const FString& TargetPlayFabId, const FString& TargetDisplayName)
{
	PendingVisitPlayFabId = TargetPlayFabId;
	PendingVisitDisplayName = TargetDisplayName;

	// 스냅샷 다운로드 요청 → HandleOtherPlayerData에서 맵 전환 처리
	FetchCitySnapshot(TargetPlayFabId);

	UE_LOG(LogTemp, Log, TEXT("[RankingManager] 방문 모드 진입 요청 (ID: %s, Name: %s)"),
		*TargetPlayFabId, *TargetDisplayName);
}

void URankingManagerSubsystem::ExitVisitMode()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	GI->SetVisitMode(false);
	GI->ClearVisitData();

	// MainMap 재진입
	GI->TransitionToLevel(TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"));

	UE_LOG(LogTemp, Log, TEXT("[RankingManager] 방문 모드 퇴출 → 정상 MainMap 복귀"));
}

// ===== 프로필 =====

void URankingManagerSubsystem::UploadRankingProfile()
{
	if (!PlayFabMgr || !PlayFabMgr->IsLoggedIn())
	{
		return;
	}

	FString ProfileJson = SerializeRankingProfile();
	PlayFabMgr->SavePlayerData(TEXT("RankingProfile"), ProfileJson);
}

FString URankingManagerSubsystem::SerializeRankingProfile() const
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr)
	{
		return TEXT("{}");
	}

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData)
	{
		return TEXT("{}");
	}

	const FGameSaveData& GD = SaveData->GameData;

	// 빌딩 통계 계산
	int32 BuildingCount = GD.Buildings.Num();
	int32 MaxTier = 0;
	if (USaveLoadManager* TierSaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		MaxTier = TierSaveMgr->GetMaxBuildingTier();
	}

	// 직원 수 합산
	int32 EmpCount = 0;
	for (const auto& Pair : GD.OfficeDataMap)
	{
		EmpCount += Pair.Value.EmployeeList.Num();
	}

	// 디스플레이 이름
	FString DisplayName = PlayFabMgr->GetUserInfo().DisplayName;

	// 컴팩트 JSON
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("dn"), DisplayName);
	Obj->SetNumberField(TEXT("hq"), GD.HQLevel);
	Obj->SetNumberField(TEXT("ct"), static_cast<int32>(GD.CompanyTitle));
	Obj->SetNumberField(TEXT("bc"), BuildingCount);
	Obj->SetNumberField(TEXT("mt"), MaxTier);
	Obj->SetNumberField(TEXT("ec"), EmpCount);
	Obj->SetNumberField(TEXT("rev"), static_cast<double>(GD.TotalRevenueEarned));
	Obj->SetNumberField(TEXT("pi"), GD.ProfileImageID);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Obj, Writer);
	return Output;
}

FRankingEntry URankingManagerSubsystem::ParseRankingProfile(
	const FString& JsonStr, const FPlayFabLeaderboardEntry& LeaderboardEntry) const
{
	FRankingEntry Entry;
	Entry.PlayFabId = LeaderboardEntry.EntityId;
	Entry.DisplayName = LeaderboardEntry.DisplayName;
	Entry.Rank = LeaderboardEntry.Rank;
	Entry.TotalRevenue = LeaderboardEntry.Score;

	if (JsonStr.IsEmpty())
	{
		// 프로필 없음 — 리더보드 기본 정보만
		return Entry;
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
	{
		Entry.DisplayName = JsonObj->GetStringField(TEXT("dn"));
		Entry.HQLevel = static_cast<int32>(JsonObj->GetNumberField(TEXT("hq")));
		Entry.CompanyTitle = static_cast<ECompanyTitle>(static_cast<int32>(JsonObj->GetNumberField(TEXT("ct"))));
		Entry.BuildingCount = static_cast<int32>(JsonObj->GetNumberField(TEXT("bc")));
		// mt 는 구 프로필(빌딩 레벨 시절 mbl)에 없는 유일한 필드라 TryGet — 없으면 0, 경고 로그 없이.
		double MtValue = 0.0;
		JsonObj->TryGetNumberField(TEXT("mt"), MtValue);
		Entry.MaxTier = static_cast<int32>(MtValue);
		Entry.EmployeeCount = static_cast<int32>(JsonObj->GetNumberField(TEXT("ec")));
	}

	return Entry;
}

// ===== PlayFab 콜백 핸들러 =====

void URankingManagerSubsystem::HandleLeaderboardResult(
	const FString& StatName, const TArray<FPlayFabLeaderboardEntry>& Entries)
{
	// 기본 정보로 캐시 갱신 (프로필은 별도 조회 필요하지만,
	// 우선 리더보드 기본 정보만으로 표시)
	CachedLeaderboard.Reset();
	CachedLeaderboard.Reserve(Entries.Num());

	for (const FPlayFabLeaderboardEntry& PFEntry : Entries)
	{
		FRankingEntry RankEntry;
		RankEntry.PlayFabId = PFEntry.EntityId;
		RankEntry.DisplayName = PFEntry.DisplayName;
		RankEntry.Rank = PFEntry.Rank;
		RankEntry.TotalRevenue = PFEntry.Score;

		// 레벨 탭이면 Score가 HQ Level
		if (PendingTabIndex == 1)
		{
			RankEntry.HQLevel = static_cast<int32>(PFEntry.Score);
		}

		CachedLeaderboard.Add(MoveTemp(RankEntry));
	}

	UE_LOG(LogTemp, Log, TEXT("[RankingManager] 리더보드 로드 완료 (Tab=%d, %d명)"),
		PendingTabIndex, CachedLeaderboard.Num());
	OnLeaderboardLoaded.Broadcast(PendingTabIndex, CachedLeaderboard);

	// 각 엔트리의 RankingProfile 가져오기 (HQLevel, 빌딩수 등 보강)
	if (PlayFabMgr)
	{
		TArray<FString> Keys;
		Keys.Add(TEXT("RankingProfile"));

		for (const FRankingEntry& RankEntry : CachedLeaderboard)
		{
			if (!RankEntry.PlayFabId.IsEmpty())
			{
				PlayFabMgr->GetOtherPlayerData(RankEntry.PlayFabId, Keys);
			}
		}
	}
}

void URankingManagerSubsystem::HandleOtherPlayerData(
	const FString& PlayFabId, const TMap<FString, FString>& Data)
{
	// 도시 스냅샷 응답인지 확인
	if (const FString* SnapshotJson = Data.Find(TEXT("CitySnapshot")))
	{
		FCitySnapshot Snapshot;
		if (FJsonObjectConverter::JsonObjectStringToUStruct(*SnapshotJson, &Snapshot))
		{
			UE_LOG(LogTemp, Log, TEXT("[RankingManager] 도시 스냅샷 수신 (ID: %s, 빌딩: %d)"),
				*PlayFabId, Snapshot.Buildings.Num());

			// 방문 대기 중이었다면 맵 전환
			if (PlayFabId == PendingVisitPlayFabId)
			{
				UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
				if (GI)
				{
					GI->SetVisitMode(true);
					GI->SetVisitData(Snapshot, PendingVisitPlayFabId, PendingVisitDisplayName);
					GI->TransitionToLevel(TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"));
				}
				PendingVisitPlayFabId.Empty();
				PendingVisitDisplayName.Empty();
			}

			OnCitySnapshotLoaded.Broadcast(PlayFabId, Snapshot);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[RankingManager] 스냅샷 JSON 파싱 실패 (ID: %s)"), *PlayFabId);
			OnRankingError.Broadcast(TEXT("도시 스냅샷 파싱 실패"));
		}
	}

	// RankingProfile 응답 처리 (리더보드 캐시 보강)
	if (const FString* ProfileJson = Data.Find(TEXT("RankingProfile")))
	{
		for (FRankingEntry& Entry : CachedLeaderboard)
		{
			if (Entry.PlayFabId == PlayFabId)
			{
				TSharedPtr<FJsonObject> JsonObj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ProfileJson);
				if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
				{
					Entry.HQLevel = static_cast<int32>(JsonObj->GetNumberField(TEXT("hq")));
					Entry.CompanyTitle = static_cast<ECompanyTitle>(static_cast<int32>(JsonObj->GetNumberField(TEXT("ct"))));
					Entry.BuildingCount = static_cast<int32>(JsonObj->GetNumberField(TEXT("bc")));
					// mt 는 구 프로필(빌딩 레벨 시절 mbl)에 없는 유일한 필드라 TryGet — 없으면 0, 경고 로그 없이.
		double MtValue = 0.0;
		JsonObj->TryGetNumberField(TEXT("mt"), MtValue);
		Entry.MaxTier = static_cast<int32>(MtValue);
					Entry.EmployeeCount = static_cast<int32>(JsonObj->GetNumberField(TEXT("ec")));
					if (JsonObj->HasField(TEXT("pi")))
					{
						Entry.ProfileImageID = static_cast<int32>(JsonObj->GetNumberField(TEXT("pi")));
					}

					UE_LOG(LogTemp, Log, TEXT("[RankingManager] 프로필 보강: %s → HQ Lv.%d"),
						*PlayFabId, Entry.HQLevel);
				}
				break;
			}
		}

		// 프로필 업데이트 후 UI 갱신
		OnLeaderboardLoaded.Broadcast(PendingTabIndex, CachedLeaderboard);
	}
}

void URankingManagerSubsystem::HandlePlayFabError(const FString& ErrorMsg)
{
	UE_LOG(LogTemp, Warning, TEXT("[RankingManager] PlayFab 에러: %s"), *ErrorMsg);
	OnRankingError.Broadcast(ErrorMsg);
}
