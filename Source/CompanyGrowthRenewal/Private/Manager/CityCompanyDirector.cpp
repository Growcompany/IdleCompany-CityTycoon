#include "Manager/CityCompanyDirector.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BuildingLightData.h"
#include "Table/BuildingSkinData.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Utils/MobileFacadeMaterialTuning.h"
#include "EngineUtils.h" // TActorIterator
#include "Engine/GameInstance.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Engine/DataTable.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#endif

// 스카이라인 창문 발광색 다양화 — 키 해시로 DT_BuildingLight 색 결정(결정적, 재시작 불변) 후
// BP_MB 머티리얼의 "Window_Emissive_Color" 파라미터에 적용. (Static mobility 무관 — 머티리얼 파라미터라 OK)
static void ApplyWindowLightToBuilding(AActor* B, int32 Key, int32 ExplicitLightID, UTableManagerSubsystem* TableMgr)
{
	if (!B || !TableMgr) { return; }
	TArray<UStaticMeshComponent*> Comps;
	B->GetComponents<UStaticMeshComponent>(Comps);
	for (UStaticMeshComponent* C : Comps)
	{
		MobileFacadeMaterialTuning::ApplyToSlotZero(C);
	}

	// DT_CityCompany.LightID 로 회사별 지정: >0 이면 그 DT_BuildingLight 행 명시 사용,
	// 0 이면 자동(키 해시) — 테이블에서 한 칸 바꿔 색 토글 가능, 0은 자동 팔레트로 폴백.
	int32 Lid = ExplicitLightID;
	if (Lid <= 0)
	{
		// 자동 팔레트 — 따뜻한 골든만: 순백(102)/냉백(104)/파스텔은 야경을 하얗게 떠 보이게 해 제외.
		// 노랑(100)·앰버(103) 위주 + 웜화이트(101) 소수로만 변주.
		static const int32 Pool[] = { 100, 100, 100, 103, 103, 103, 101, 100, 103, 101, 100, 103, 100, 101 };
		const uint32 H = (uint32)Key * 2654435761u;
		Lid = Pool[H % UE_ARRAY_COUNT(Pool)];
	}

	bool bOk = false;
	const FBuildingLightData L = TableMgr->GetBuildingLightData(Lid, bOk);
	if (!bOk) { return; }
	const FLinearColor FinalEmissive = L.EmissiveColor * L.EmissiveIntensity;

	for (UStaticMeshComponent* C : Comps)
	{
		const int32 NumMat = C->GetNumMaterials();
		for (int32 i = 0; i < NumMat; ++i)
		{
			UMaterialInterface* Cur = C->GetMaterial(i);
			if (!Cur) { continue; }
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Cur);
			if (!MID) { MID = C->CreateAndSetMaterialInstanceDynamicFromMaterial(i, Cur); }
			if (MID) { MID->SetVectorParameterValue(TEXT("Window_Emissive_Color"), FinalEmissive); }
		}
	}
}

// 스카이라인 스킨: DT_CityCompany.SkinID → DT_BuildingSkin.SkinMaterial 을 ISM 슬롯0에 적용(슬롯1=Roof 보존).
static void ApplySkinToBuilding(AActor* B, int32 SkinID, UTableManagerSubsystem* TableMgr)
{
	if (!B || !TableMgr || SkinID <= 0) { return; }
	bool bOk = false;
	const FBuildingSkinData Sk = TableMgr->GetBuildingSkinData(SkinID, bOk);
	if (!bOk) { return; }
	UMaterialInterface* Mat = Sk.SkinMaterial.LoadSynchronous();
	if (!Mat) { return; }
	TArray<UStaticMeshComponent*> Comps;
	B->GetComponents<UStaticMeshComponent>(Comps);
	for (UStaticMeshComponent* C : Comps)
	{
		C->SetMaterial(0, Mat); // 슬롯0 = 바디 머티리얼
	}
}

// 액터 Tag "CityKey:N" → N. 없으면 -1. (서쪽 복제로 클래스명 중복 → Tag가 정본 식별자)
static int32 CityKeyFromTags(const AActor* B)
{
	if (!B) { return -1; }
	for (const FName& Tg : B->Tags)
	{
		const FString S = Tg.ToString();
		if (S.StartsWith(TEXT("CityKey:")))
		{
			return FCString::Atoi(*S.Mid(8)); // "CityKey:" 길이 8
		}
	}
	return -1;
}

void UCityCompanyDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	UWorld* DirectorWorld = GetWorld();
	if (DirectorWorld && DirectorWorld->WorldType == EWorldType::Editor && !DirectorWorld->IsGameWorld())
	{
		FEditorDelegates::PreSaveWorldWithContext.AddUObject(this, &UCityCompanyDirector::HandlePreSaveWorld);
		FEditorDelegates::PostSaveWorldWithContext.AddUObject(this, &UCityCompanyDirector::HandlePostSaveWorld);
		FEditorDelegates::PreBeginPIE.AddUObject(this, &UCityCompanyDirector::HandlePreBeginPIE);
		FEditorDelegates::EndPIE.AddUObject(this, &UCityCompanyDirector::HandleEndPIE);
		EditorPreviewTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UCityCompanyDirector::TickEditorPreview),
			0.25f);
		TickEditorPreview(0.0f);
	}
#endif
}

void UCityCompanyDirector::Deinitialize()
{
#if WITH_EDITOR
	RestoreMobileFacadePreview();
	if (EditorPreviewTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(EditorPreviewTickerHandle);
		EditorPreviewTickerHandle.Reset();
	}
	FEditorDelegates::PreSaveWorldWithContext.RemoveAll(this);
	FEditorDelegates::PostSaveWorldWithContext.RemoveAll(this);
	FEditorDelegates::PreBeginPIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
#endif

	Super::Deinitialize();
}

bool UCityCompanyDirector::EnableMobileFacadePreview()
{
#if WITH_EDITOR
	bMobileFacadePreviewRequested = true;
	EditorPreviewRetryRemainingSeconds = 0.0f;
	const bool bApplied = bMobileFacadePreviewEnabled || ApplyMobileFacadePreview();
	if (!bApplied)
	{
		EditorPreviewRetryRemainingSeconds = EditorPreviewRetryBackoffSeconds;
	}
	return bApplied;
#else
	return false;
#endif
}

bool UCityCompanyDirector::RefreshMobileFacadePreview()
{
#if WITH_EDITOR
	bMobileFacadePreviewRequested = true;
	EditorPreviewRetryRemainingSeconds = 0.0f;
	RestoreMobileFacadePreview();
	const bool bApplied = ApplyMobileFacadePreview();
	if (!bApplied)
	{
		EditorPreviewRetryRemainingSeconds = EditorPreviewRetryBackoffSeconds;
	}
	return bApplied;
#else
	return false;
#endif
}

void UCityCompanyDirector::DisableMobileFacadePreview()
{
#if WITH_EDITOR
	bMobileFacadePreviewRequested = false;
	EditorPreviewRetryRemainingSeconds = 0.0f;
	RestoreMobileFacadePreview();
#endif
}

bool UCityCompanyDirector::IsMobileFacadePreviewEnabled() const
{
#if WITH_EDITORONLY_DATA
	return bMobileFacadePreviewEnabled;
#else
	return false;
#endif
}

int32 UCityCompanyDirector::GetMobileFacadePreviewActorCount() const
{
#if WITH_EDITORONLY_DATA
	return MobileFacadePreviewActorCount;
#else
	return 0;
#endif
}

#if WITH_EDITOR
bool UCityCompanyDirector::IsEligibleEditorPreviewWorld() const
{
	const UWorld* DirectorWorld = GetWorld();
	return DirectorWorld
		&& DirectorWorld->WorldType == EWorldType::Editor
		&& !DirectorWorld->IsGameWorld()
		&& DirectorWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1
		&& DirectorWorld->GetPackage()->GetName()
			== TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity");
}

bool UCityCompanyDirector::ApplyMobileFacadePreview()
{
	if (bMobileFacadePreviewEnabled)
	{
		return true;
	}
	if (!IsEligibleEditorPreviewWorld() || bSuspendedForSave || bSuspendedForPIE)
	{
		return false;
	}

	const TSoftObjectPtr<UDataTable> CityTablePath(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Table/CityCompany/DT_CityCompany.DT_CityCompany")));
	const TSoftObjectPtr<UDataTable> SkinTablePath(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Table/Building/DT_BuildingSkin.DT_BuildingSkin")));
	UDataTable* CityTable = CityTablePath.LoadSynchronous();
	UDataTable* SkinTable = SkinTablePath.LoadSynchronous();
	if (!CityTable || !SkinTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityDirector] editor preview DataTable load failed"));
		return false;
	}

	TMap<int32, FCityCompanyData> CityDataByKey;
	TArray<FCityCompanyData*> CityRows;
	CityTable->GetAllRows(TEXT("CityCompanyEditorPreview"), CityRows);
	for (const FCityCompanyData* Row : CityRows)
	{
		if (Row && Row->BuildingKey > 0)
		{
			CityDataByKey.Add(Row->BuildingKey, *Row);
		}
	}

	TMap<int32, FBuildingSkinData> SkinDataByID;
	TArray<FBuildingSkinData*> SkinRows;
	SkinTable->GetAllRows(TEXT("CityCompanyEditorPreview"), SkinRows);
	for (const FBuildingSkinData* Row : SkinRows)
	{
		if (Row && Row->SkinID > 0)
		{
			SkinDataByID.Add(Row->SkinID, *Row);
		}
	}

	UWorld* DirectorWorld = GetWorld();
	TSet<int32> AppliedKeys;
	bool bApplyFailed = false;
	for (TActorIterator<AActor> It(DirectorWorld); It; ++It)
	{
		AActor* BuildingActor = *It;
		if (!BuildingActor || !BuildingActor->GetClass())
		{
			continue;
		}

		const FString ClassName = BuildingActor->GetClass()->GetName();
		if (!ClassName.StartsWith(CityBuildingClassPrefix))
		{
			continue;
		}

		int32 BuildingKey = CityKeyFromTags(BuildingActor);
		if (BuildingKey < 0)
		{
			BuildingKey = ParseBuildingKey(ClassName);
		}
		const FCityCompanyData* CityData = CityDataByKey.Find(BuildingKey);
		if (!CityData || AppliedKeys.Contains(BuildingKey))
		{
			bApplyFailed = true;
			break;
		}

		const FBuildingSkinData* SkinData = SkinDataByID.Find(CityData->SkinID);
		UMaterialInterface* SkinMaterial = SkinData ? SkinData->SkinMaterial.LoadSynchronous() : nullptr;
		if (!SkinMaterial)
		{
			bApplyFailed = true;
			break;
		}

		TArray<UStaticMeshComponent*> MeshComponents;
		BuildingActor->GetComponents<UStaticMeshComponent>(MeshComponents);
		if (MeshComponents.IsEmpty())
		{
			bApplyFailed = true;
			break;
		}

		for (UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			FCityFacadeMaterialSnapshot& Snapshot = EditorMaterialSnapshots.AddDefaulted_GetRef();
			Snapshot.Component = MeshComponent;
			Snapshot.OverrideMaterials = MeshComponent->OverrideMaterials;

			UMaterialInstanceDynamic* PreviewMID = UMaterialInstanceDynamic::Create(
				SkinMaterial,
				GetTransientPackage());
			if (!PreviewMID
				|| !MobileFacadeMaterialTuning::ApplyToDynamicMaterial(PreviewMID, SkinMaterial))
			{
				bApplyFailed = true;
				break;
			}
			PreviewMID->SetFlags(RF_Transient);
			MeshComponent->SetMaterial(0, PreviewMID);
		}

		if (bApplyFailed)
		{
			break;
		}
		AppliedKeys.Add(BuildingKey);
	}

	if (bApplyFailed || AppliedKeys.Num() != CityDataByKey.Num())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[CityDirector] editor preview mapping incomplete: applied=%d expected=%d"),
			AppliedKeys.Num(),
			CityDataByKey.Num());
		RestoreMobileFacadePreview();
		return false;
	}

	MobileFacadePreviewActorCount = AppliedKeys.Num();
	bMobileFacadePreviewEnabled = true;
	EditorPreviewRetryRemainingSeconds = 0.0f;
	UE_LOG(LogTemp, Log, TEXT("[CityDirector] transient mobile facade preview enabled: %d"), MobileFacadePreviewActorCount);
	return true;
}

void UCityCompanyDirector::RestoreMobileFacadePreview()
{
	for (const FCityFacadeMaterialSnapshot& Snapshot : EditorMaterialSnapshots)
	{
		if (!Snapshot.Component.IsValid())
		{
			continue;
		}
		UMeshComponent* MeshComponent = Snapshot.Component.Get();

		MeshComponent->EmptyOverrideMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < Snapshot.OverrideMaterials.Num(); ++MaterialIndex)
		{
			MeshComponent->SetMaterial(MaterialIndex, Snapshot.OverrideMaterials[MaterialIndex].Get());
		}
	}
	EditorMaterialSnapshots.Reset();
	MobileFacadePreviewActorCount = 0;
	bMobileFacadePreviewEnabled = false;
}

bool UCityCompanyDirector::TickEditorPreview(float DeltaTime)
{
	if (bSuspendedForSave || bSuspendedForPIE)
	{
		return true;
	}

	if (bMobileFacadePreviewRequested && IsEligibleEditorPreviewWorld())
	{
		if (!bMobileFacadePreviewEnabled)
		{
			EditorPreviewRetryRemainingSeconds = FMath::Max(
				0.0f,
				EditorPreviewRetryRemainingSeconds - DeltaTime);
			if (EditorPreviewRetryRemainingSeconds <= 0.0f
				&& !ApplyMobileFacadePreview())
			{
				EditorPreviewRetryRemainingSeconds = EditorPreviewRetryBackoffSeconds;
			}
		}
	}
	else
	{
		EditorPreviewRetryRemainingSeconds = 0.0f;
		if (bMobileFacadePreviewEnabled)
		{
			RestoreMobileFacadePreview();
		}
	}
	return true;
}

void UCityCompanyDirector::HandlePreSaveWorld(UWorld* SavedWorld, FObjectPreSaveContext)
{
	if (SavedWorld == GetWorld())
	{
		bSuspendedForSave = true;
		RestoreMobileFacadePreview();
	}
}

void UCityCompanyDirector::HandlePostSaveWorld(UWorld* SavedWorld, FObjectPostSaveContext)
{
	if (SavedWorld == GetWorld())
	{
		bSuspendedForSave = false;
		if (bMobileFacadePreviewRequested && !bSuspendedForPIE)
		{
			EditorPreviewRetryRemainingSeconds = 0.0f;
			if (!ApplyMobileFacadePreview())
			{
				EditorPreviewRetryRemainingSeconds = EditorPreviewRetryBackoffSeconds;
			}
		}
	}
}

void UCityCompanyDirector::HandlePreBeginPIE(bool)
{
	bSuspendedForPIE = true;
	RestoreMobileFacadePreview();
}

void UCityCompanyDirector::HandleEndPIE(bool)
{
	bSuspendedForPIE = false;
	if (bMobileFacadePreviewRequested && !bSuspendedForSave)
	{
		EditorPreviewRetryRemainingSeconds = 0.0f;
		if (!ApplyMobileFacadePreview())
		{
			EditorPreviewRetryRemainingSeconds = EditorPreviewRetryBackoffSeconds;
		}
	}
}
#endif

int32 UCityCompanyDirector::ParseBuildingKey(const FString& ClassName)
{
	// "BP_MB" = 5글자. 그 뒤 선행 숫자만 파싱: "036_EonSpire_C" -> 36
	if (!ClassName.StartsWith(TEXT("BP_MB")))
	{
		return 0;
	}
	return FCString::Atoi(*ClassName.Mid(5));
}

void UCityCompanyDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BuildSkylineMapping();
}

void UCityCompanyDirector::BuildSkylineMapping()
{
	SkylineEntries.Reset();

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr =
		W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityDirector] TableManagerSubsystem 없음"));
		return;
	}

	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->GetClass())
		{
			continue;
		}
		const FString ClassName = A->GetClass()->GetName();
		if (!ClassName.StartsWith(CityBuildingClassPrefix))
		{
			continue;
		}

		int32 Key = CityKeyFromTags(A);   // 베이크가 심은 Tag 우선(복제 구분)
		if (Key < 0)
		{
			Key = ParseBuildingKey(ClassName); // 태그 없으면 클래스명 폴백
		}
		if (Key <= 0)
		{
			continue;
		}

		FCityCompanyData Data;
		if (!TableMgr->GetCityCompanyData(Key, Data))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CityDirector] DT_CityCompany 미매핑 key=%d (%s)"), Key, *ClassName);
			continue;
		}

		FVector Origin, Extent;
		A->GetActorBounds(false, Origin, Extent);

		// 스킨(머티리얼 슬롯0) 적용 (DT_CityCompany.SkinID)
		ApplySkinToBuilding(A, Data.SkinID, TableMgr);
		// 창문 발광색 적용 (DT_CityCompany.LightID: >0 명시 / 0 자동 해시 팔레트)
		ApplyWindowLightToBuilding(A, Key, Data.LightID, TableMgr);

		FCitySkylineEntry E;
		E.BuildingKey = Key;
		E.Building = A;
		E.Data = Data;
		E.Height = Extent.Z * 2.f;
		E.AnchorWorld = FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z + 200.f);
		SkylineEntries.Add(MoveTemp(E));
	}

	// 가장 높은 N개를 flagship(사인형 변형)으로
	SkylineEntries.Sort([](const FCitySkylineEntry& LHS, const FCitySkylineEntry& RHS)
	{
		return LHS.Height > RHS.Height;
	});
	const int32 N = FMath::Min(FlagshipCount, SkylineEntries.Num());
	for (int32 i = 0; i < N; ++i)
	{
		SkylineEntries[i].bFlagship = true;
	}

	UE_LOG(LogTemp, Log, TEXT("[CityDirector] skyline entries mapped: %d (flagship %d)"), SkylineEntries.Num(), N);
}
