// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/WorldMapManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ProductRecipeTable.h"

// ─────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────
void UWorldMapManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 기본 채광소/라인 배열 채움 (저장 로드가 덮어쓰기 전에 선배치)
	InitializeDefaultFacilities();

	// Core Ticker 등록 (GameInstanceSubsystem은 Tick 가상함수가 없음)
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UWorldMapManager::TickInternal),
		WorldMapConstants::TickIntervalSec);

	UE_LOG(LogTemp, Log, TEXT("[WorldMapManager] Initialized. Mines=%d Lines=%d"), Mines.Num(), Lines.Num());
}

void UWorldMapManager::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Super::Deinitialize();
}

// ─────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────
bool UWorldMapManager::TickInternal(float DeltaTime)
{
	// 폰/모바일 서스펜드 대비 과도한 DeltaTime은 clamp (오프라인은 별도 경로)
	const float ClampedDelta = FMath::Min(DeltaTime, 60.0f);

	bool bDirty = false;
	TickMines(ClampedDelta, bDirty);
	TickFactories(ClampedDelta, bDirty);

	// 이산 이벤트(원자재 차감/유닛 완성/광산 적립) 틱에만 지연 저장 — 전 맵 상시 티커라 즉시 저장은 매초급 히칭이었음
	if (bDirty)
	{
		if (USaveLoadManager* SaveMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr)
		{
			SaveMgr->RequestDeferredSave();
		}
	}

	return true; // keep alive
}

void UWorldMapManager::TickMines(float DeltaTime, bool& bOutDirty)
{
	const FDateTime Now = FDateTime::UtcNow();

	for (FMiningFacility& Mine : Mines)
	{
		if (!Mine.bConstructed)
		{
			continue;
		}

		// 저장한도 도달 시 채취 중단 (알림 1회)
		if (Mine.IsFull())
		{
			if (!Mine.bFullNotified)
			{
				Mine.bFullNotified = true;
				OnMiningStorageFull.Broadcast(Mine.CountryType, Mine.MaterialType);
			}
			continue;
		}

		// 부스트 적용 (IsBoostActive면 x3)
		const float EffectiveRate = Mine.AutoMineRatePerMin
			* (Mine.IsBoostActive(Now) ? WorldMapConstants::MiningBoostMultiplier : 1.0f);

		if (EffectiveRate <= 0.0f)
		{
			continue;
		}

		// 분수초 누적기 방식: 누적시간 × rate/60 >= 1 유닛일 때 실제 지급
		Mine.AccumulatorSec += DeltaTime;
		const float SecondsPerUnit = 60.0f / EffectiveRate;
		const int32 UnitsToAdd = FMath::FloorToInt(Mine.AccumulatorSec / SecondsPerUnit);

		if (UnitsToAdd >= 1)
		{
			Mine.AccumulatorSec -= UnitsToAdd * SecondsPerUnit;

			const int64 Capacity = Mine.VaultCapacity - Mine.CurrentStorage;
			const int64 Added = FMath::Min<int64>(UnitsToAdd, Capacity);
			if (Added > 0)
			{
				Mine.CurrentStorage += Added;
				bOutDirty = true;

				if (Mine.IsFull() && !Mine.bFullNotified)
				{
					Mine.bFullNotified = true;
					OnMiningStorageFull.Broadcast(Mine.CountryType, Mine.MaterialType);
				}
			}
		}

		// 수집 후 여유 생겼을 때 플래그 리셋
		if (!Mine.IsFull() && Mine.bFullNotified)
		{
			Mine.bFullNotified = false;
		}
	}
}

void UWorldMapManager::TickFactories(float DeltaTime, bool& bOutDirty)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return;
	}

	const FDateTime Now = FDateTime::UtcNow();

	for (FFactoryLine& Line : Lines)
	{
		if (!Line.bUnlocked || Line.IsIdle())
		{
			continue;
		}

		bool bRecipeOk = false;
		const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Line.AssignedCompanyType, Line.AssignedProjectIndex, bRecipeOk);
		if (!bRecipeOk)
		{
			continue;
		}

		// 새 유닛 착수: Progress==0 시점에 원자재/에너지 선 체크 후 한번에 차감
		if (Line.Progress <= 0.0f)
		{
			// 원자재/에너지 양쪽 모두 선체크 (둘 중 하나라도 부족하면 차감 없이 pause)
			const bool bHasMats = HasMaterials(Recipe, 1);
			const bool bHasEnergy = (Recipe.EnergyCost <= 0) || (EnergyCount >= Recipe.EnergyCost);

			if (!bHasMats || !bHasEnergy)
			{
				Line.bPaused = true;
				continue;  // paused 상태로 진입, 다음 틱 재시도
			}

			// 둘 다 확인됐을 때만 실제 차감 (원자재만 빠지고 에너지가 없어 억울한 상황 방지)
			TryConsumeRecipe(Recipe, 1, false);
			if (Recipe.EnergyCost > 0)
			{
				ConsumeEnergy(Recipe.EnergyCost, false);
			}

			Line.bPaused = false;
			bOutDirty = true;
		}

		// Paused 상태이면 Progress 진행 중단 (자원 부족 등으로 일시정지된 라인)
		if (Line.bPaused)
		{
			continue;
		}

		const float SpeedMul = Line.IsBoostActive(Now) ? WorldMapConstants::FactoryBoostMultiplier : 1.0f;
		const float ProdTime = FMath::Max(0.01f, Recipe.ProductionTimeSec);

		// Progress 진행 자체는 dirty 아님 — 매초 바뀌는 값이라 저장 가치 없음(레벨 전환/30초 주기 저장이 커버), 이산 이벤트(차감/완성)만 dirty
		Line.Progress += (DeltaTime * SpeedMul) / ProdTime;

		if (Line.Progress >= 1.0f)
		{
			Line.Progress = 0.0f;
			Line.RemainingQuantity = FMath::Max(0, Line.RemainingQuantity - 1);
			bOutDirty = true;

			const FIntPoint Key = MakeProductKey(Line.AssignedCompanyType, Line.AssignedProjectIndex);
			AddProduct(Key, 1, Line.StampedGrade, false);
			OnFactoryUnitCompleted.Broadcast(Line.CountryType, Line.LineIndex, Key);

			if (Line.RemainingQuantity <= 0)
			{
				const ECountryType CachedCountry = Line.CountryType;
				Line.AssignedCompanyType = ECompanyType::None;
				Line.AssignedProjectIndex = INDEX_NONE;
				Line.AssignedOrderId = INDEX_NONE;
				Line.RemainingQuantity = 0;
				Line.Progress = 0.0f;
				Line.bPaused = false;

				// 자동 배정 활성화된 나라면 큐에서 다음 주문 끌어옴
				TryAutoAssignForCountry(CachedCountry);
			}
		}
	}
}

// ─────────────────────────────────────────────
// 원자재 인벤토리
// ─────────────────────────────────────────────
void UWorldMapManager::AddMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave)
{
	if (Mat == ERawMaterialType::None || Amount <= 0)
	{
		return;
	}
	int64& Cur = RawMaterialInventory.FindOrAdd(Mat);
	Cur += Amount;
	OnMaterialChanged.Broadcast(Mat, Cur);
	if (bShouldSave)
	{
		SaveGameData();
	}
}

bool UWorldMapManager::ConsumeMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave)
{
	if (Mat == ERawMaterialType::None || Amount <= 0)
	{
		return false;
	}
	int64* Cur = RawMaterialInventory.Find(Mat);
	if (!Cur || *Cur < Amount)
	{
		return false;
	}
	*Cur -= Amount;
	OnMaterialChanged.Broadcast(Mat, *Cur);
	if (bShouldSave)
	{
		SaveGameData();
	}
	return true;
}

bool UWorldMapManager::HasMaterials(const FProductRecipeTable& Recipe, int32 Units) const
{
	if (Units <= 0)
	{
		return true;
	}

	// 10종 원자재에 대해 Required = GetMaterialAmount * Units 보유 여부 체크
	static const ERawMaterialType AllMats[] = {
		ERawMaterialType::IronOre, ERawMaterialType::Copper, ERawMaterialType::Silicon,
		ERawMaterialType::Lithium, ERawMaterialType::Oil, ERawMaterialType::RareEarth,
		ERawMaterialType::Aluminum, ERawMaterialType::Wood, ERawMaterialType::Gold,
		ERawMaterialType::DiamondOre
	};

	for (ERawMaterialType M : AllMats)
	{
		const int32 PerUnit = Recipe.GetMaterialAmount(M);
		if (PerUnit <= 0) continue;
		const int64 Need = static_cast<int64>(PerUnit) * Units;
		if (GetMaterialAmount(M) < Need)
		{
			return false;
		}
	}
	return true;
}

bool UWorldMapManager::TryConsumeRecipe(const FProductRecipeTable& Recipe, int32 Units, bool bShouldSave)
{
	if (Units <= 0)
	{
		return true;
	}
	if (!HasMaterials(Recipe, Units))
	{
		return false;
	}

	static const ERawMaterialType AllMats[] = {
		ERawMaterialType::IronOre, ERawMaterialType::Copper, ERawMaterialType::Silicon,
		ERawMaterialType::Lithium, ERawMaterialType::Oil, ERawMaterialType::RareEarth,
		ERawMaterialType::Aluminum, ERawMaterialType::Wood, ERawMaterialType::Gold,
		ERawMaterialType::DiamondOre
	};

	for (ERawMaterialType M : AllMats)
	{
		const int32 PerUnit = Recipe.GetMaterialAmount(M);
		if (PerUnit <= 0) continue;
		const int64 Need = static_cast<int64>(PerUnit) * Units;
		ConsumeMaterial(M, Need, false);
	}

	if (bShouldSave)
	{
		SaveGameData();
	}
	return true;
}

int64 UWorldMapManager::GetMaterialAmount(ERawMaterialType Mat) const
{
	const int64* Found = RawMaterialInventory.Find(Mat);
	return Found ? *Found : 0;
}

// ─────────────────────────────────────────────
// 에너지 / 정제유
// ─────────────────────────────────────────────
void UWorldMapManager::AddEnergy(int64 Amount, bool bShouldSave)
{
	if (Amount <= 0) return;
	EnergyCount += Amount;
	OnEnergyChanged.Broadcast(EnergyCount);
	if (bShouldSave) SaveGameData();
}

bool UWorldMapManager::ConsumeEnergy(int64 Amount, bool bShouldSave)
{
	if (Amount <= 0) return true;
	if (EnergyCount < Amount) return false;
	EnergyCount -= Amount;
	OnEnergyChanged.Broadcast(EnergyCount);
	if (bShouldSave) SaveGameData();
	return true;
}

void UWorldMapManager::AddRefinedOil(int64 Amount, bool bShouldSave)
{
	if (Amount <= 0) return;
	RefinedOilCount += Amount;
	OnRefinedOilChanged.Broadcast(RefinedOilCount);
	if (bShouldSave) SaveGameData();
}

// ─────────────────────────────────────────────
// 채광소
// ─────────────────────────────────────────────
static void RecalcMineLevelStats(FMiningFacility& Mine)
{
	// Level에 비례하는 간단한 곡선: rate = 2.0 * (1 + 0.15*(Lv-1)), cap = 100 * (1 + Lv)
	Mine.AutoMineRatePerMin = 2.0f * (1.0f + 0.15f * (Mine.Level - 1));
	Mine.VaultCapacity = 100 * (1 + Mine.Level);
}

bool UWorldMapManager::StartMiningBoost(ECountryType Country, ERawMaterialType Mat)
{
	FMiningFacility* Mine = FindMineMutable(Country, Mat);
	if (!Mine || !Mine->bConstructed) return false;

	const FDateTime Now = FDateTime::UtcNow();
	if (!Mine->IsBoostReady(Now)) return false;

	Mine->BoostEndTime = Now + FTimespan::FromSeconds(WorldMapConstants::MiningBoostDurationSec);
	Mine->BoostCooldownEnd = Now + FTimespan::FromSeconds(WorldMapConstants::MiningBoostCooldownSec);
	SaveGameData();
	return true;
}

bool UWorldMapManager::UpgradeMine(ECountryType Country, ERawMaterialType Mat)
{
	FMiningFacility* Mine = FindMineMutable(Country, Mat);
	if (!Mine || !Mine->bConstructed) return false;
	Mine->Level += 1;
	RecalcMineLevelStats(*Mine);
	SaveGameData();
	return true;
}

FMiningFacility UWorldMapManager::GetMine(ECountryType Country, ERawMaterialType Mat) const
{
	for (const FMiningFacility& M : Mines)
	{
		if (M.CountryType == Country && M.MaterialType == Mat) return M;
	}
	return FMiningFacility();
}

TArray<FMiningFacility> UWorldMapManager::GetMinesInCountry(ECountryType Country) const
{
	TArray<FMiningFacility> Out;
	for (const FMiningFacility& M : Mines)
	{
		if (M.CountryType == Country) Out.Add(M);
	}
	return Out;
}

int64 UWorldMapManager::CollectMine(ECountryType Country, ERawMaterialType Mat)
{
	FMiningFacility* Mine = FindMineMutable(Country, Mat);
	if (!Mine || Mine->CurrentStorage <= 0) return 0;

	const int64 Amount = Mine->CurrentStorage;
	Mine->CurrentStorage = 0;
	Mine->bFullNotified = false;

	AddMaterial(Mine->MaterialType, Amount, false);
	SaveGameData();
	return Amount;
}

bool UWorldMapManager::ConstructMine(ECountryType Country, ERawMaterialType Mat)
{
	FMiningFacility* Mine = FindMineMutable(Country, Mat);
	if (!Mine || Mine->bConstructed) return false;

	Mine->bConstructed = true;
	Mine->Level = 1;
	RecalcMineLevelStats(*Mine);
	Mine->CurrentStorage = 0;
	Mine->AccumulatorSec = 0.0f;
	SaveGameData();
	return true;
}

// ─────────────────────────────────────────────
// 공장
// ─────────────────────────────────────────────
bool UWorldMapManager::AssignLineAssignment(ECountryType Country, int32 LineIndex, ECompanyType CompanyType, int32 ProjectIndex, int32 OrderId, int32 Quantity, EQualityGrade Grade)
{
	FFactoryLine* Line = FindLineMutable(Country, LineIndex);
	if (!Line || !Line->bUnlocked) return false;
	if (Quantity <= 0) return false;

	Line->AssignedCompanyType = CompanyType;
	Line->AssignedProjectIndex = ProjectIndex;
	Line->AssignedOrderId = OrderId;
	Line->RemainingQuantity = Quantity;
	Line->StampedGrade = Grade;
	Line->Progress = 0.0f;
	Line->bPaused = false;
	SaveGameData();
	return true;
}

bool UWorldMapManager::ClearLine(ECountryType Country, int32 LineIndex)
{
	FFactoryLine* Line = FindLineMutable(Country, LineIndex);
	if (!Line) return false;

	Line->AssignedCompanyType = ECompanyType::None;
	Line->AssignedProjectIndex = INDEX_NONE;
	Line->AssignedOrderId = INDEX_NONE;
	Line->RemainingQuantity = 0;
	Line->Progress = 0.0f;
	Line->bPaused = false;
	SaveGameData();
	return true;
}

bool UWorldMapManager::StartFactoryBoost(ECountryType Country, int32 LineIndex)
{
	FFactoryLine* Line = FindLineMutable(Country, LineIndex);
	if (!Line || !Line->bUnlocked) return false;

	const FDateTime Now = FDateTime::UtcNow();
	if (!Line->IsBoostReady(Now)) return false;

	Line->BoostEndTime = Now + FTimespan::FromSeconds(WorldMapConstants::FactoryBoostDurationSec);
	Line->BoostCooldownEnd = Now + FTimespan::FromSeconds(WorldMapConstants::FactoryBoostCooldownSec);
	SaveGameData();
	return true;
}

FFactoryLine UWorldMapManager::GetLine(ECountryType Country, int32 LineIndex) const
{
	for (const FFactoryLine& L : Lines)
	{
		if (L.CountryType == Country && L.LineIndex == LineIndex) return L;
	}
	return FFactoryLine();
}

TArray<FFactoryLine> UWorldMapManager::GetFactoryLines(ECountryType Country) const
{
	TArray<FFactoryLine> Out;
	for (const FFactoryLine& L : Lines)
	{
		if (L.CountryType == Country) Out.Add(L);
	}
	return Out;
}

void UWorldMapManager::SetAutoAssign(ECountryType Country, bool bEnable)
{
	AutoAssignMap.FindOrAdd(Country) = bEnable;
	SaveGameData();
}

bool UWorldMapManager::IsAutoAssignEnabled(ECountryType Country) const
{
	const bool* Found = AutoAssignMap.Find(Country);
	return Found ? *Found : false;
}

// ─────────────────────────────────────────────
// 생산 주문서 큐
// ─────────────────────────────────────────────
void UWorldMapManager::EnqueueProductionOrder(const FProductionOrder& Order)
{
	PendingProductionQueue.Add(Order);
	SaveGameData();
}

bool UWorldMapManager::TryAutoAssignForCountry(ECountryType Country)
{
	// 재진입 방지: TickFactories → ClearLine → TryAutoAssign → AssignLineAssignment → SaveGameData 등
	if (bReentryGuard_AutoAssign) return false;
	if (!IsAutoAssignEnabled(Country)) return false;
	if (PendingProductionQueue.Num() == 0) return false;

	bReentryGuard_AutoAssign = true;

	bool bAssigned = false;

	// 해당 나라에서 idle한 첫 라인 검색
	FFactoryLine* IdleLine = nullptr;
	for (FFactoryLine& L : Lines)
	{
		if (L.CountryType == Country && L.bUnlocked && L.IsIdle())
		{
			IdleLine = &L;
			break;
		}
	}

	if (IdleLine)
	{
		// 큐 선두의 주문을 그대로 배정 (MVP: 나라별 필터링 없이 FIFO)
		const FProductionOrder Order = PendingProductionQueue[0];
		PendingProductionQueue.RemoveAt(0);

		IdleLine->AssignedCompanyType = Order.CompanyType;
		IdleLine->AssignedProjectIndex = Order.ProjectIndex;
		IdleLine->AssignedOrderId = Order.OrderID;
		IdleLine->RemainingQuantity = Order.RemainingQuantity > 0 ? Order.RemainingQuantity : Order.Quantity;
		IdleLine->StampedGrade = Order.Grade;
		IdleLine->Progress = 0.0f;
		IdleLine->bPaused = false;

		bAssigned = true;
		// 틱(TickFactories 완주) 경로에서도 진입 — 즉시 저장은 유저 입력 없는 프레임 히칭이라 지연 저장
		if (USaveLoadManager* SaveMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr)
		{
			SaveMgr->RequestDeferredSave();
		}
	}

	bReentryGuard_AutoAssign = false;
	return bAssigned;
}

// ─────────────────────────────────────────────
// 완성품 인벤토리
// ─────────────────────────────────────────────
void UWorldMapManager::AddProduct(FIntPoint ProductKey, int64 Qty, EQualityGrade Grade, bool bShouldSave)
{
	if (Qty <= 0) return;

	int64& Cur = ProductInventory.FindOrAdd(ProductKey);
	Cur += Qty;

	// 등급은 기존값보다 높을 때만 갱신 (섞이면 상위 등급 유지)
	EQualityGrade& CurGrade = ProductGrades.FindOrAdd(ProductKey, EQualityGrade::F);
	if (static_cast<uint8>(Grade) > static_cast<uint8>(CurGrade))
	{
		CurGrade = Grade;
	}

	OnProductAdded.Broadcast(ProductKey, Cur);

	if (bShouldSave)
	{
		SaveGameData();
	}
}

bool UWorldMapManager::ConsumeProduct(FIntPoint ProductKey, int64 Qty, bool bShouldSave)
{
	if (Qty <= 0) return false;
	int64* Cur = ProductInventory.Find(ProductKey);
	if (!Cur || *Cur < Qty) return false;

	*Cur -= Qty;
	const int64 NewAmt = *Cur;

	if (NewAmt <= 0)
	{
		ProductInventory.Remove(ProductKey);
		ProductGrades.Remove(ProductKey);
	}

	OnProductConsumed.Broadcast(ProductKey, NewAmt);

	if (bShouldSave)
	{
		SaveGameData();
	}
	return true;
}

int64 UWorldMapManager::GetProductAmount(FIntPoint ProductKey) const
{
	const int64* Found = ProductInventory.Find(ProductKey);
	return Found ? *Found : 0;
}

EQualityGrade UWorldMapManager::GetProductGrade(FIntPoint ProductKey) const
{
	const EQualityGrade* Found = ProductGrades.Find(ProductKey);
	return Found ? *Found : EQualityGrade::F;
}

TArray<FIntPoint> UWorldMapManager::GetAllProductKeys() const
{
	TArray<FIntPoint> Keys;
	ProductInventory.GetKeys(Keys);
	return Keys;
}

int64 UWorldMapManager::GetTotalWarehouseValue() const
{
	// MVP: ProjectIndex 기반 BasePrice 곡선만으로 추정
	// (TradePort::GetBasePrice와 동일 공식 — 추후 함수 통합 가능)
	int64 Total = 0;
	for (const TPair<FIntPoint, int64>& Pair : ProductInventory)
	{
		const int64 Idx = FMath::Max<int64>(1, static_cast<int64>(Pair.Key.Y));
		const int64 Base = FMath::Max<int64>(100, 100 + 50 * Idx * Idx);

		// 등급 배율 적용 (S=2.0, A=1.5, ..., F=0.5)
		const EQualityGrade* GradePtr = ProductGrades.Find(Pair.Key);
		const float GradeMul = GradePtr ? GetQualityGradeRevenueMultiplier(*GradePtr) : 1.0f;

		Total += FMath::RoundToInt64(Base * Pair.Value * GradeMul);
	}
	return Total;
}

void UWorldMapManager::DebugSeedRandomProducts(int32 NumSlots, int64 MinQty, int64 MaxQty)
{
	// 제조업 3종(Electronics/Semiconductor/Automobile) x 프로젝트 1~100 풀에서 랜덤 슬롯.
	// Game/Finance/IT 는 무형 산업이라 판매 가능한 물품 없음 → 제외.
	const ECompanyType Manufacturing[] = {
		ECompanyType::Electronics,
		ECompanyType::Semiconductor,
		ECompanyType::Automobile,
	};
	const int32 IndustryCount = UE_ARRAY_COUNT(Manufacturing);
	const int32 ProjectMin = 1;
	const int32 ProjectMax = 100;
	const int32 GradeMin = static_cast<int32>(EQualityGrade::F);
	const int32 GradeMax = static_cast<int32>(EQualityGrade::S);

	for (int32 i = 0; i < NumSlots; ++i)
	{
		const ECompanyType Industry = Manufacturing[FMath::RandRange(0, IndustryCount - 1)];
		const int32 ProjectIdx = FMath::RandRange(ProjectMin, ProjectMax);
		const int64 Qty = FMath::RandRange(MinQty, MaxQty);
		const EQualityGrade Grade = static_cast<EQualityGrade>(FMath::RandRange(GradeMin, GradeMax));

		AddProduct(FIntPoint(static_cast<int32>(Industry), ProjectIdx), Qty, Grade, /*bShouldSave=*/false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[WorldMapManager] DebugSeedRandomProducts: %d slots seeded (3 manufacturing, project 1~100, qty %lld~%lld)"),
		NumSlots, MinQty, MaxQty);
}

ECountryType UWorldMapManager::GetPrimaryCountryForMaterial(ERawMaterialType Mat) const
{
	// 기획서 기준 주력 산출국 매핑 (보조 산출국은 제외)
	switch (Mat)
	{
	case ERawMaterialType::IronOre:    return ECountryType::Australia;    // 호주
	case ERawMaterialType::Copper:     return ECountryType::Canada;       // 캐나다
	case ERawMaterialType::Silicon:    return ECountryType::Brazil;       // 브라질
	case ERawMaterialType::Lithium:    return ECountryType::Australia;    // 호주
	case ERawMaterialType::Oil:        return ECountryType::Saudi;        // 사우디 독점
	case ERawMaterialType::RareEarth:  return ECountryType::China;        // 중국 독점
	case ERawMaterialType::Aluminum:   return ECountryType::Australia;    // 호주 (캐나다 보조)
	case ERawMaterialType::Wood:       return ECountryType::Canada;       // 캐나다
	case ERawMaterialType::Gold:       return ECountryType::SouthAfrica;  // 남아공 독점
	case ERawMaterialType::DiamondOre: return ECountryType::SouthAfrica;  // 남아공 독점
	default:                           return ECountryType::None;
	}
}

// ─────────────────────────────────────────────
// SaveLoad 연동
// ─────────────────────────────────────────────
void UWorldMapManager::SerializeForSave(
	TMap<ERawMaterialType, int64>& OutMats,
	int64& OutEnergy,
	int64& OutOil,
	TArray<FMiningFacility>& OutMines,
	TArray<FFactoryLine>& OutLines,
	TMap<ECountryType, bool>& OutAutoAssign,
	TArray<FProductionOrder>& OutPending,
	TMap<FIntPoint, int64>& OutProducts,
	TMap<FIntPoint, uint8>& OutGrades,
	FDateTime& OutLastSaveUtc) const
{
	OutMats = RawMaterialInventory;
	OutEnergy = EnergyCount;
	OutOil = RefinedOilCount;
	OutMines = Mines;
	OutLines = Lines;
	OutAutoAssign = AutoAssignMap;
	OutPending = PendingProductionQueue;
	OutProducts = ProductInventory;

	OutGrades.Empty(ProductGrades.Num());
	for (const TPair<FIntPoint, EQualityGrade>& Pair : ProductGrades)
	{
		OutGrades.Add(Pair.Key, static_cast<uint8>(Pair.Value));
	}

	OutLastSaveUtc = FDateTime::UtcNow();
}

void UWorldMapManager::LoadFromSave(
	const TMap<ERawMaterialType, int64>& Mats,
	int64 Energy,
	int64 Oil,
	const TArray<FMiningFacility>& InMines,
	const TArray<FFactoryLine>& InLines,
	const TMap<ECountryType, bool>& InAutoAssign,
	const TArray<FProductionOrder>& InPending,
	const TMap<FIntPoint, int64>& InProducts,
	const TMap<FIntPoint, uint8>& InGrades,
	FDateTime LastSaveUtc)
{
	RawMaterialInventory = Mats;
	EnergyCount = Energy;
	RefinedOilCount = Oil;

	// 저장된 데이터가 있으면 덮어쓰고, 없으면 Initialize에서 채운 기본값 유지
	if (InMines.Num() > 0) Mines = InMines;
	if (InLines.Num() > 0) Lines = InLines;

	AutoAssignMap = InAutoAssign;
	PendingProductionQueue = InPending;
	ProductInventory = InProducts;

	ProductGrades.Empty(InGrades.Num());
	for (const TPair<FIntPoint, uint8>& Pair : InGrades)
	{
		ProductGrades.Add(Pair.Key, static_cast<EQualityGrade>(Pair.Value));
	}

	// 오프라인 시간만큼 catchup (LastSaveUtc 이후 경과 초)
	if (LastSaveUtc.GetTicks() > 0)
	{
		const FTimespan Elapsed = FDateTime::UtcNow() - LastSaveUtc;
		const float ElapsedSec = static_cast<float>(Elapsed.GetTotalSeconds());
		if (ElapsedSec > 0.0f)
		{
			ApplyOfflineCatchup(ElapsedSec);
		}
	}
}

void UWorldMapManager::ApplyOfflineCatchup(float ElapsedSec)
{
	const float Clamped = FMath::Min(ElapsedSec, WorldMapConstants::OfflineCatchupMaxSec);
	if (Clamped <= 0.0f) return;

	bool bDirty = false;
	TickMines(Clamped, bDirty);
	TickFactories(Clamped, bDirty);
	if (bDirty) SaveGameData();
}

// ─────────────────────────────────────────────
// 헬퍼
// ─────────────────────────────────────────────
FMiningFacility* UWorldMapManager::FindMineMutable(ECountryType Country, ERawMaterialType Mat)
{
	for (FMiningFacility& M : Mines)
	{
		if (M.CountryType == Country && M.MaterialType == Mat) return &M;
	}
	return nullptr;
}

FFactoryLine* UWorldMapManager::FindLineMutable(ECountryType Country, int32 LineIndex)
{
	for (FFactoryLine& L : Lines)
	{
		if (L.CountryType == Country && L.LineIndex == LineIndex) return &L;
	}
	return nullptr;
}

void UWorldMapManager::InitializeDefaultFacilities()
{
	Mines.Reset();
	Lines.Reset();

	// 채광소: 나라 × 원자재 조합 (GDD_FACTORY 채광소 배치 기준)
	struct FMineSpec { ECountryType Country; ERawMaterialType Mat; };
	const FMineSpec MineSpecs[] = {
		// 호주
		{ ECountryType::Australia,  ERawMaterialType::IronOre  },
		{ ECountryType::Australia,  ERawMaterialType::Lithium  },
		{ ECountryType::Australia,  ERawMaterialType::Aluminum },
		// 캐나다
		{ ECountryType::Canada,     ERawMaterialType::Wood     },
		{ ECountryType::Canada,     ERawMaterialType::Copper   },
		{ ECountryType::Canada,     ERawMaterialType::Aluminum },
		// 브라질
		{ ECountryType::Brazil,     ERawMaterialType::IronOre  },
		{ ECountryType::Brazil,     ERawMaterialType::Silicon  },
		{ ECountryType::Brazil,     ERawMaterialType::Wood     },
		// 남아공
		{ ECountryType::SouthAfrica,ERawMaterialType::Gold       },
		{ ECountryType::SouthAfrica,ERawMaterialType::DiamondOre },
		{ ECountryType::SouthAfrica,ERawMaterialType::IronOre    },
		// 사우디 (석유 독점)
		{ ECountryType::Saudi,      ERawMaterialType::Oil      },
		// 중국
		{ ECountryType::China,      ERawMaterialType::RareEarth },
		{ ECountryType::China,      ERawMaterialType::IronOre   },
		{ ECountryType::China,      ERawMaterialType::Copper    },
	};

	for (const FMineSpec& Spec : MineSpecs)
	{
		FMiningFacility Mine;
		Mine.CountryType = Spec.Country;
		Mine.MaterialType = Spec.Mat;
		Mine.Level = 1;
		Mine.bConstructed = false; // 플레이어가 명시적으로 건설
		Mine.AutoMineRatePerMin = 2.0f;
		Mine.VaultCapacity = 200;
		Mine.CurrentStorage = 0;
		Mines.Add(Mine);
	}

	// 공장 라인: Korea/China/Japan/Germany/USA 각 5개 라인. 인덱스 0만 초기 해금.
	const ECountryType FactoryCountries[] = {
		ECountryType::Korea, ECountryType::China, ECountryType::Japan,
		ECountryType::Germany, ECountryType::USA
	};

	for (ECountryType C : FactoryCountries)
	{
		for (int32 Idx = 0; Idx < 5; ++Idx)
		{
			FFactoryLine Line;
			Line.CountryType = C;
			Line.LineIndex = Idx;
			Line.bUnlocked = (Idx == 0);
			Lines.Add(Line);
		}
	}
}

void UWorldMapManager::SaveGameData()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr)
	{
		SaveMgr->SaveGameData();
	}
}
