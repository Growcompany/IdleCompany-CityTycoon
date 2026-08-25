// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficManager.generated.h"

class USplineComponent;
class UStaticMesh;
class UMaterialInterface;
class UInstancedStaticMeshComponent;
class ATimeCycleManager;

// 레벨의 모든 ATrafficRoute(스플라인)를 모아 ~50대의 인스턴싱 차량을 주행시키는 매니저.
// - 렌더: 모델당 ISM 1개 + 라이트카드 ISM 2개(앞/뒤) → 대수와 무관하게 드로우콜 ≈ 모델수+2.
// - 주행: 룩어헤드 조향 + 각속도 제한(보트 무버 로직 이식). 매니저 1틱이 전 차량 인스턴스 갱신.
// - 게이팅: 플레이어가 지은 빌딩(EntityManager, 업종 배정됨) 반경 내에서만 차량 표시(빈 구간은 0스케일 숨김).
//   빌딩 목록은 주기적으로 갱신 → 새로 지은 빌딩 주변에도 자동으로 트래픽 등장.
// - 밤 라이트: 라이트 ISM 의 머티리얼(M_CarLight)이 MPC Night_Intensity 로 밤에만 발광(동적 광원 0).
UCLASS()
class COMPANYGROWTHRENEWAL_API ATrafficManager : public AActor
{
	GENERATED_BODY()

public:
	ATrafficManager();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 총 차량 수(빌딩 근처에 있는 차만 실제로 보임).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0"))
	int32 TotalCars = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float MinSpeed = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float MaxSpeed = 1600.f;

	// 룩어헤드 거리(cm) — 클수록 코너를 미리 보고 완만하게 선회.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float LookAheadDistance = 1200.f;

	// 최대 선회 각속도(deg/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "1.0"))
	float MaxYawRate = 120.f;

	// 트래픽 시뮬 갱신 주기(초). 0=매 프레임. 기본 30Hz — 차속 1000~1600cm/s라 33ms 스텝(33~53cm)은 도시 줌에서 안 보이고, 매 프레임 정렬+인스턴스 업로드 비용을 절감(저사양).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float SimUpdateInterval = 0.0333f;

	// 차 메시 forward축(+Y) 정렬 보정. 차가 뒤로 가면 +90.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic")
	FRotator MeshYawOffset = FRotator(0.f, -90.f, 0.f);

	// 차 메시 바닥 정렬 후 미세 Z 보정(cm). 스플라인/그래프 노드를 도로 위에 그리므로 기본 0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic")
	float VehicleZOffset = 0.f;

	// 지은 빌딩 반경 내에서만 차량 표시(cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Building", meta = (ClampMin = "0.0"))
	float BuildingActivationRadius = 7000.f;

	// 빌딩 목록 갱신 주기(초) — 새로 지은 빌딩 반영.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Building", meta = (ClampMin = "0.1"))
	float BuildingRefreshInterval = 3.f;

	// 장식 스카이라인 빌딩(BP_MB*)도 트래픽 앵커로 포함 → 보이는 도시 전체에 baseline 트래픽.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Building")
	bool bIncludeCityBuildings = true;

	// 장식 빌딩 식별 클래스명 접두사(BP_MB001_... 등). BP_MB 파생은 AActor 라 타입캐스팅 불가→이름매칭.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Building")
	FString CityBuildingClassPrefix = TEXT("BP_MB");

	// [디버그] 켜면 빌딩 게이팅 무시(모든 차 항상 표시) — 게이팅 vs 렌더링 문제 분리용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugIgnoreBuildingGating = false;

	// [디버그] 켜면 가시 차량 수/빌딩 수를 주기적으로 로그.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugLog = false;

	// [디버그] 켜면 도로 그래프를 PIE에 오버레이(노드=점, 엣지=선) — 그래프가 도로에 맞는지 시각 확인.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugDrawGraph = false;

	// [디버그] 켜면 TotalCars 무시하고 '모든 엣지에 양방향 1대씩' 배치 → 전 도로 커버 테스트(빈 도로 식별).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugFillAllEdges = false;

	// 라이트 글로우 카드 크기(차당 헤드2+테일2). 카드는 항상 카메라를 향함(빌보드)이라, 퍼지는 라디얼 글로우가 어느 각도서든 동그랗게 보임.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "0.01"))
	float LightCardScale = 0.6f;

	// 멀리서도 라이트가 안 사라지게: 카메라 거리에 비례해 카드를 키워 '화면상 일정 크기' 유지. 이 거리(cm)에서 원본 LightCardScale.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "100.0"))
	float LightRefDistance = 10000.f;

	// 거리 스케일 상한. 라이트 월드크기 = LightCardScale × clamp(카메라거리/LightRefDistance, LightMinFactor, 이 값). 15→3: 15는 ~9m additive 쿼드 ×200장 = 모바일 fill 주범이었음.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "1.0"))
	float LightMaxGrow = 3.f;

	// 거리 스케일 하한 — 멀어도 카드가 서브픽셀로 안 작아지게(쿼드 커버리지 팝핑=원거리 차 라이트 깜빡임 원인). 가로등 LampMinFactor와 동일 원리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "1.0"))
	float LightMinFactor = 1.5f;

	// 라이트 카드를 카메라 쪽으로 미는 거리(cm) — 빌보드 절반이 차체 폴리곤에 파묻혀 z-테스트로 잘리며 깜빡이는 것 방지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "0.0"))
	float LightCardCamPush = 25.f;

	// 카메라가 이 거리(cm) 이상 움직인 프레임에만 라이트 빌보드 재조준(시뮬 30Hz와 독립, 정지 시 비용 0). 가로등 LampMoveThreshold와 동일 원리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "0.0"))
	float LightMoveThreshold = 1.f;

	// 차 모델 메시(생성자에서 7종 기본 로드, 에디터 override 가능).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	TArray<UStaticMesh*> CarModels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	UStaticMesh* LightCardMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	UMaterialInterface* HeadLightMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	UMaterialInterface* TailLightMaterial;

	// --- 도로 그래프 모드 (Python 툴이 채우면 자동 사용; 비면 ATrafficRoute 스플라인 모드) ---
	// 교차로=노드(월드좌표). 도로구간=엣지. 차가 엣지를 달리다 교차로에서 다음 엣지를 랜덤 선택(U턴 금지).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph")
	TArray<FVector> GraphNodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph")
	TArray<FIntPoint> GraphEdges;

	// 우측통행 안쪽 차선 오프셋(cm) — 중앙선에서 진행방향 오른쪽 첫 차선까지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph", meta = (ClampMin = "0.0"))
	float LaneOffset = 150.f;

	// 같은 방향 두 차선 간격(cm). 4차선 = 안쪽(LaneOffset) + 바깥쪽(LaneOffset+LaneWidth).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph", meta = (ClampMin = "0.0"))
	float LaneWidth = 300.f;

	// 같은 엣지·차선 앞차와 유지할 최소 간격(cm) — 차 겹침 방지(car-following). 차 길이+여유.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph", meta = (ClampMin = "0.0"))
	float MinCarGap = 650.f;

	UPROPERTY(VisibleAnywhere, Category = "Traffic")
	USceneComponent* Root;

private:
	// 모델당 1개 ISM (같은 모델 차량은 한 ISM 의 인스턴스로).
	UPROPERTY()
	TArray<UInstancedStaticMeshComponent*> BodyISM;

	UPROPERTY()
	UInstancedStaticMeshComponent* FrontLightISM;

	UPROPERTY()
	UInstancedStaticMeshComponent* RearLightISM;

	UPROPERTY()
	TArray<USplineComponent*> Routes;

	TArray<bool> RouteReverse;

	// 모델별 바닥 보정값(-BoundsMin.Z) — 스플라인 위에 바퀴가 닿도록 들어올림.
	TArray<float> ModelGroundLift;

	// 모델별 라이트 로컬좌표 — 차 메시 "Color Bloom" 발광면에서 추출(헤드=앞 흰색, 테일=뒤 빨강, 보통 각 2개).
	TArray<TArray<FVector>> ModelHeadlights;
	TArray<TArray<FVector>> ModelTaillights;

	struct FTrafficCar
	{
		int32 RouteIndex = 0;
		int32 ModelIndex = 0;
		int32 BodyInstance = 0;
		TArray<int32> HeadInst;   // 헤드라이트 인스턴스(FrontLightISM)
		TArray<int32> TailInst;   // 테일라이트 인스턴스(RearLightISM)
		float Distance = 0.f;
		float Speed = 0.f;
		float Yaw = 0.f;
		float RouteLength = 0.f;
		bool bReverse = false;
		bool bVisible = false;
	};
	TArray<FTrafficCar> Cars;

	TArray<FVector> BuildingLocations;
	// [Perf] 정적 BP_MB 스카이라인 위치는 안 변함 → 1회만 수집해 캐시(매 3s 전체 액터 스캔 회피).
	TArray<FVector> StaticCityBuildingLocations;
	bool bCityCacheBuilt = false;
	float RefreshAccum = 0.f;
	float SimAccum = 0.f;   // 시뮬 스로틀 누적자(SimUpdateInterval 도달 시 1스텝).
	FVector CachedCameraLoc = FVector::ZeroVector;   // 라이트 빌보드용 — 매 틱 플레이어 카메라 위치 캐시.

	// 라이트 카드 매프레임 재조준 캐시 — 시뮬 스텝(30Hz)이 위치/기본스케일을 쓰고, 매 프레임 패스는 회전·거리스케일만 재계산.
	TArray<FVector> FrontCardPos, RearCardPos;   // ISM 인스턴스 인덱스로 정렬된 카드 월드 위치(카메라 push 적용 전)
	TArray<float> FrontCardBase, RearCardBase;   // 기본 스케일(숨김 카드 = 0 → 매프레임 패스가 스킵)
	FVector LastLightCamLoc = FVector(TNumericLimits<float>::Max());
	void RefreshLightBillboards();

	// day/night 전환 시 라이트 카드 ISM(헤드/테일)만 가시성 토글 — 낮엔 숨겨 fill 절감. 차체(BodyISM)는 토글 안 함.
	UFUNCTION()
	void HandleSunRise();
	UFUNCTION()
	void HandleSunSet();
	void ApplyNightVisibility(bool bNight);

	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	bool bGlowVisible = true; // 시계 못 찾으면 표시 유지(밤 라이트 회귀 방지)

	void BuildTraffic();
	void RefreshBuildingLocations();
	bool IsNearBuilding(const FVector& WorldLoc, float Radius) const;
	void UpdateCar(FTrafficCar& Car, float DeltaTime);

	// --- 공유 헬퍼 (스플라인/그래프 모드 공통) ---
	void CreateInstanceComponents();
	void ApplyCarInstances(int32 ModelIdx, int32 BodyInst, const TArray<int32>& HeadInsts, const TArray<int32>& TailInsts, const FVector& WorldLoc, float WorldYaw, bool bVisible, bool bWasVisible);

	// --- 그래프 모드 ---
	bool bGraphMode = false;
	TArray<TArray<int32>> Adjacency;   // 노드 → 이웃 노드 인덱스
	struct FGraphCar
	{
		int32 FromNode = 0;
		int32 ToNode = 0;
		int32 NextNode = 0;     // 다음다음 노드(미리 선택 → 교차로 전 회전 예측)
		int32 ModelIndex = 0;
		int32 BodyInstance = 0;
		TArray<int32> HeadInst;   // 헤드라이트 인스턴스(FrontLightISM)
		TArray<int32> TailInst;   // 테일라이트 인스턴스(RearLightISM)
		float Dist = 0.f;       // 현재 엣지 진행 거리
		float EdgeLen = 1.f;
		float Speed = 0.f;
		float Yaw = 0.f;
		float LaneOff = 0.f;    // 이 차의 차선 오프셋(안쪽/바깥쪽)
		uint32 Rng = 0;         // 차별 결정적 회전 RNG
		float SpacingGap = 1e9f; // 같은 엣지·차선 앞차와의 간격(car-following) — 매 틱 계산
		bool bVisible = false;   // 직전 스텝 가시 여부 — 숨김→숨김이면 인스턴스 쓰기 스킵.
	};
	TArray<FGraphCar> GCars;
	void BuildGraphTraffic();
	void UpdateGraphCar(FGraphCar& Car, float DeltaTime);
	int32 PickNextNode(int32 FromNode, int32 AtNode, uint32& Rng) const;
};
