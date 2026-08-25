// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Gacha/GachaCaptureStage.h"
#include "Entity/Officeworker/StickOfficeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Data/EmployeeTypes.h"
#include "Enum/GachaTier.h"
#include "Enum/BubbleType.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AGachaCaptureStage::AGachaCaptureStage()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WorkerAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("WorkerAnchor"));
	WorkerAnchor->SetupAttachment(Root);
	WorkerAnchor->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); // 스폰 직원이 카메라(-X) 정면 보게

	// 키라이트 — 격리 무대라 자체 조명 필요(없으면 검게 나옴)
	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetRelativeLocation(FVector(-300.f, -90.f, 260.f)); // 카메라 쪽(전면) 상단
	KeyLight->SetRelativeRotation(FRotator(-30.f, 16.f, 0.f));    // 캐릭터(원점) 조준
	// KeyLight(상단 스팟)는 기본 OFF — 균일 조명 우선(아래 먼 FillLight 단독). 방향성 스팟은 위/앞을
	// 밝히고 그늘을 만들어 불균일 + 컷아웃 임계값 미달 구멍의 원인이었다. 올리면 상단 입체감(불균일 증가).
	KeyLight->SetIntensity(0.f);                                // ⚠ 입체감 노브 (0=완전 균일)
	KeyLight->SetAttenuationRadius(4000.f);
	KeyLight->SetInnerConeAngle(0.f);
	KeyLight->SetOuterConeAngle(85.f);
	KeyLight->SetCastShadows(false);

	// 균일 조명 = 먼 전면 포인트광(≈평행광). 가까운 광은 거리제곱 감쇠로 상하/원근 밝기 편차(불균일)를
	// 만들지만, 20m 앞에 두면 캐릭터 2m 깊이가 거리 대비 <10% → 전신 고르게 + 컷아웃 전 부위 임계값 위(솔리드).
	// (디렉셔널 라이트는 이 SceneCapture 에 기여 안 함 — 헤드리스 실측 2026-07-13. Units 는 균일과 무관, 밝기 스케일뿐.)
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetIntensityUnits(ELightUnits::Unitless);        // 결정적 스케일 고정(1.3M 검증값 기준)
	FillLight->SetRelativeLocation(FVector(-2000.f, 0.f, 100.f)); // 먼 전면(≈평행광 → 균일)
	FillLight->SetIntensity(1300000.f);                         // ⚠ 밝기 노브 (Unitless·먼 거리라 큰 값)
	FillLight->SetAttenuationRadius(4000.f);
	FillLight->SetCastShadows(false);

	// 캡처 카메라 — 워커 앞(-X)에서 +X 방향(워커) 바라봄.
	// 거리 멀고 조준 약간 낮게 → 전신(발끝까지) + 캐릭터가 프레임 위쪽에 위치(다리 잘림 방지).
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->SetRelativeLocation(FVector(-520.f, 0.f, 5.f));  // 워커 중심(발 -88~머리 +92) 높이로 → 전신
	Capture->SetRelativeRotation(FRotator(0.f, 0.f, 0.f)); // +X 바라봄
	Capture->ProjectionType = ECameraProjectionMode::Perspective;
	Capture->FOVAngle = 40.f;
	// 인버스 오파시티 알파(캐릭터=0, 배경=1) 제공 — M_GachaRevealRT 가 OneMinus(a) 로 컷아웃한다.
	// 구 FinalColorLDR + 루마키(Opacity=luma×MaskBoost)는 "어두우면 배경"이라 캐릭터 위의 검정
	// (눈동자·머리·구두)까지 뚫었다. 알파는 색과 무관하게 실루엣만 보므로 원리적으로 안 뚫린다.
	// ※ pre-tonemap 이라 톤매퍼를 안 타므로 색/밝기 기준이 FinalColorLDR 과 다르다(광량 재조정 필요).
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->SetActive(false);

	// 노출 고정 — SCS_FinalColorLDR 은 톤매퍼+오토노출을 통과하는데, 이 캡처는 노출을 어디에도
	// 고정하지 않아 밝기가 비결정적이었다: (1) 에디터=Histogram 오토노출이 ShowOnlyList 로 대부분
	// 검정인 프레임을 "어두운 씬"으로 오판→노출 급상승→블로우아웃, (2) 모바일=DeviceProfile 의
	// r.EyeAdaptation.MethodOverride=2 가 Manual 강제→자동보정 없이 강한 키라이트 원본이 새하얌.
	// Manual 로 못박아 두 경로 모두 결정적 노출로 통일(모바일 강제 Manual 과 일치).
	// ※ 이 구성(Manual + PhysicalCameraExposure OFF)에선 Bias 가 밝기에 무효(실측) → 밝기는 광량으로만 조정.
	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.f;                                    // 밝은 피사체 밀키 워시(번짐) 제거

	// 에디터 전용 프리뷰 직원 — 뷰포트에서 카메라/라이트 프레이밍 잡는 기준. 실제 직원과 동일 메시/스케일/오프셋/정면.
	PreviewWorker = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewWorker"));
	PreviewWorker->SetupAttachment(WorkerAnchor);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyF(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/SK_StickmanCG"));
	if (BodyF.Succeeded()) { PreviewWorker->SetSkeletalMeshAsset(BodyF.Object); }
	PreviewWorker->SetRelativeLocation(FVector(0.f, 0.f, -88.f));   // ACharacter 캡슐 오프셋(발이 anchor 아래)
	PreviewWorker->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));  // ACharacter 메시 표준 yaw 보정
	PreviewWorker->SetRelativeScale3D(FVector(0.09f));             // 원본 FBX ~11배 보정
	// bHiddenInGame 안 줌 — SceneCapture(게임뷰)가 그걸 숨겨 캡처에 안 잡힘. 런타임 제외는 ShowOnly 로.
	PreviewWorker->SetCastShadow(false);
	PreviewWorker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewWorker->bIsEditorOnly = true;

	// 스폰할 직원 BP 자동 지정 (게임이 쓰는 그 BP) → 수동 설정/BP 제작 불필요
	static ConstructorHelpers::FClassFinder<AStickOfficeworker> WorkerF(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/BP_StickOfficeworker"));
	if (WorkerF.Succeeded()) { WorkerClass = WorkerF.Class; }
}

void AGachaCaptureStage::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터/배치 인스턴스 프리뷰: 자기 컴포넌트(프리뷰워커+배경판)만 캡처 + 매프레임 + 하늘/안개 제거.
	// → BP 뷰포트나 레벨에 배치하면 지정한 RT 에 프리뷰가 실시간으로 그려져 카메라/라이트를 보면서 잡을 수 있음.
	if (Capture)
	{
		Capture->ShowOnlyActors.Empty();
		Capture->ShowOnlyActors.Add(this);
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->ShowFlags.SetFog(false);
		Capture->ShowFlags.SetVolumetricFog(false);
		// TAA는 유지(끄면 머티리얼 디더가 노이즈로 노출), 모션블러만 제거 (2026-07-11 실측)
		Capture->ShowFlags.SetMotionBlur(false);
		Capture->bCaptureEveryFrame = true;
	}
}

namespace
{
	// 공유 무대 단일 슬롯 — 레벨 전환/파괴는 weak 가 흡수
	TWeakObjectPtr<AGachaCaptureStage> GSharedCaptureStage;
}

AGachaCaptureStage* AGachaCaptureStage::AcquireShared(UObject* Holder, UWorld* World, TSubclassOf<AGachaCaptureStage> StageClass)
{
	if (!World || !StageClass)
	{
		return nullptr;
	}

	AGachaCaptureStage* Stage = GSharedCaptureStage.Get();
	if (!Stage || Stage->GetWorld() != World || Stage->IsActorBeingDestroyed())
	{
		// 라이브 맵과 안 겹치게 먼 곳에 스폰 (ShowOnlyList 가 격리하지만 그림자/광원 안전)
		const FTransform FarOrigin(FRotator::ZeroRotator, FVector(0.f, 0.f, 100000.f));
		Stage = World->SpawnActor<AGachaCaptureStage>(StageClass, FarOrigin);
		GSharedCaptureStage = Stage;
	}
	if (Stage)
	{
		Stage->HolderWeak = Holder;
	}
	return Stage;
}

void AGachaCaptureStage::ReleaseShared(const UObject* Holder)
{
	if (HolderWeak.Get() != Holder)
	{
		return; // 다른 소비처가 인계 — 그쪽이 반납
	}
	HolderWeak = nullptr;
	EndReveal();
}

FVector AGachaCaptureStage::GetSingleCamLocation() const
{
	const AGachaCaptureStage* CDO = GetClass() ? GetClass()->GetDefaultObject<AGachaCaptureStage>() : nullptr;
	return (CDO && CDO->Capture) ? CDO->Capture->GetRelativeLocation() : FVector(SingleCamX, 0.f, 5.f);
}

FRotator AGachaCaptureStage::GetSingleCamRotation() const
{
	const AGachaCaptureStage* CDO = GetClass() ? GetClass()->GetDefaultObject<AGachaCaptureStage>() : nullptr;
	return (CDO && CDO->Capture) ? CDO->Capture->GetRelativeRotation() : FRotator::ZeroRotator;
}

void AGachaCaptureStage::ApplyCaptureInvariants()
{
	if (!Capture) { return; }

	// 노출 고정 재확인 — 결정성 불변식이라 코드가 권위(룩 노브 아님). BP 에서 bOverride_AutoExposureMethod
	// 체크가 풀리면 오토노출이 ShowOnlyList 의 대부분-검정 프레임을 "어두운 씬"으로 오판해 노출을 폭주시켜,
	// 광량을 줄여도 그만큼 되올려 흰 화면이 된다(=광량 노브가 무력화). 생성자와 동일 값으로 매번 되돌린다.
	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;

	// 컷아웃 계약(알파 기반) — 소비 머티리얼이 OneMinus(a) 를 전제하므로 소스도 불변식이다.
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;

	// 하늘/대기/안개 제거. 조명은 ON(입체감) — 그늘 순흑화는 필 라이트 + MaskBoost 로 방지.
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	// TAA는 유지(끄면 머티리얼 디더가 노이즈로 노출), 모션블러만 제거 (2026-07-11 실측)
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->bCaptureEveryFrame = true;
	Capture->SetActive(true);
}

UTextureRenderTarget2D* AGachaCaptureStage::BeginReveal(const FEmployeeInstance& Employee, EStageAnimMode AnimMode)
{
	// 멀티가 걸어둔 와이드 RT 는 단발 소스가 아니다 — 인계 시 BP/기존 단발 RT 로 되돌린다.
	if (Capture && WideRT && Capture->TextureTarget == WideRT)
	{
		Capture->TextureTarget = nullptr;
	}
	// BP/배치에서 지정한 RT 가 있으면 그걸 사용(에디터 프리뷰와 동일 RT → 튜닝 일관). 없으면 transient 생성.
	if (Capture && Capture->TextureTarget)
	{
		RT = Capture->TextureTarget;
	}
	else if (!RT)
	{
		RT = UKismetRenderingLibrary::CreateRenderTarget2D(
			this, FMath::Max(RTWidth, 64), FMath::Max(RTHeight, 64),
			ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor(0.015f, 0.02f, 0.03f, 1.f), false);
	}
	if (Capture)
	{
		Capture->TextureTarget = RT;
		// 공유 무대 인계 시 멀티 카메라 잔존 방지 — 기본(단발) 프레이밍으로 복귀.
		// 멀티는 pitch 를 0 으로 눕히므로 회전도 함께 되돌려야 직원창 히어로 뷰가 회귀하지 않는다.
		Capture->SetRelativeLocation(GetSingleCamLocation());
		Capture->SetRelativeRotation(GetSingleCamRotation());
	}

	DestroyCurrentWorkers();
	SlotUVWindows.Reset(); // 단발 = 풀 윈도우, 직전 배치 창이 남아 카드에 새면 안 된다
	SpawnAndDress(Employee, AnimMode, FVector::ZeroVector, 1.f);
	RefreshShowOnly();

	ApplyCaptureInvariants();
	return RT;
}

UTextureRenderTarget2D* AGachaCaptureStage::BeginRevealBatch(
	const TArray<FEmployeeInstance>& EmployeesInSlotOrder, EStageAnimMode AnimMode)
{
	const int32 N = EmployeesInSlotOrder.Num();
	if (N <= 0) { return nullptr; }
	if (N == 1) { return BeginReveal(EmployeesInSlotOrder[0], AnimMode); }

	DestroyCurrentWorkers();
	SlotUVWindows.Reset();

	const int32 RTW = CenterSlotPx + (N - 1) * GetColumnPitchPx();
	const int32 RTH = FMath::Max(RTHeight, 64);
	// 멀티 RT 는 transient — BP 지정 RT(에디터 프리뷰용 512×640)를 덮지 않는다. 폭이 바뀔 때만 재생성.
	if (Capture && Capture->TextureTarget && Capture->TextureTarget != WideRT)
	{
		RT = Capture->TextureTarget; // 단발 복귀용으로 BP RT 보존
	}
	if (!WideRT || WideRT->SizeX != RTW || WideRT->SizeY != RTH)
	{
		WideRT = UKismetRenderingLibrary::CreateRenderTarget2D(this, RTW, RTH,
			ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor(0.015f, 0.02f, 0.03f, 1.f), false);
	}

	const FVector SingleCam = GetSingleCamLocation();
	const FRotator SingleRot = GetSingleCamRotation();
	const float CamX = static_cast<float>(SingleCam.X) * (static_cast<float>(RTW) / static_cast<float>(CenterSlotPx));
	// 단발 카메라가 피사체 평면에서 겨냥하는 높이. 아래 슬롯 기하는 전부 "pitch 0" 을 전제하는데, 후퇴는 거리를
	// N배로 늘리므로 pitch 오프셋(거리 x tan)도 N배가 된다 — BP 가 pitch 를 조금만 줘도 피사체가 프레임 밖으로
	// 밀려난다(실측: pitch −5.6°/7.25배 후퇴 → 150cm 하강, 댄서가 상단에 붙고 머리 잘림).
	// 겨냥 높이를 카메라 Z 로 옮기고 pitch 를 눕히면 세로 프레이밍이 단발과 완전히 동일해지고 전제도 성립한다.
	const float AimZ = static_cast<float>(SingleCam.Z)
		+ FMath::Abs(static_cast<float>(SingleCam.X)) * FMath::Tan(FMath::DegreesToRadians(static_cast<float>(SingleRot.Pitch)));
	if (Capture)
	{
		Capture->TextureTarget = WideRT;
		// 세로 프레이밍 유지: RT 가로비만큼 후퇴 (FOV 불변 — 광각 왜곡 없음). 좌우/높이는 단발과 동일.
		Capture->SetRelativeLocation(FVector(CamX, SingleCam.Y, AimZ));
		Capture->SetRelativeRotation(FRotator::ZeroRotator);
	}

	// 아래 기하는 전부 캡처에 실제로 넣은 값(CamX·AimZ·라이브 FOV·실제 RT 치수)에서만 유도한다 — 권위 혼재 금지.
	// 피사체 평면 cm/px — 가로 FOV 고정 + 후퇴가 폭에 비례라 N 과 무관하게 일정(라이브 BP 리그 ≈0.4758)
	const float HalfFOVRad = FMath::DegreesToRadians((Capture ? Capture->FOVAngle : 40.f) * 0.5f);
	const float HalfWidthCm = FMath::Abs(CamX) * FMath::Tan(HalfFOVRad);
	const float CmPerPx = (2.f * HalfWidthCm) / static_cast<float>(RTW);
	// 엔진 SceneCapture 투영은 YAxisMultiplier=W/H 라 FOV 는 항상 가로 → 세로 반높이는 RT 비로 유도
	const float HalfHeightCm = HalfWidthCm * (static_cast<float>(RTH) / static_cast<float>(RTW));
	// 지면선(발끝 Z=−FeetZ)의 v 좌표. BP 룩 튜닝(FOV/높이/pitch)이 그대로 반영된 라이브 리그에서 재계산한다.
	// pitch 는 위에서 AimZ 로 흡수했으므로 여기서는 순수 수평 카메라 기하가 성립한다.
	const float GroundLineLive = (HalfHeightCm > UE_KINDA_SMALL_NUMBER)
		? (HalfHeightCm + AimZ + FeetZ) / (2.f * HalfHeightCm)
		: GroundLineV;
	// BP 가 리그를 튜닝하면 코드 상수와 갈라지는 게 정상 — 경고가 아니라 기록(상수는 퇴화 폴백 전용)
	UE_LOG(LogTemp, Log, TEXT("[GachaStage] 배치 리그 — GroundLineV %.4f (상수 %.4f) CamX=%.1f AimZ=%.1f pitch=%.2f FOV=%.1f RT=%dx%d"),
		GroundLineLive, GroundLineV, CamX, AimZ, SingleRot.Pitch, Capture ? Capture->FOVAngle : 40.f, RTW, RTH);

	SlotUVWindows.Reserve(N);
	for (int32 SlotIdx = 0; SlotIdx < N; ++SlotIdx)
	{
		const FVector4 UV = GetSlotUVWindow(SlotIdx, N, GroundLineLive);
		SlotUVWindows.Add(UV); // 카드가 소비할 권위 창 = 워커를 실제로 세운 그 창
		const float ColCenterPx = (static_cast<float>(UV.X) + static_cast<float>(UV.Z) * 0.5f) * RTW - RTW * 0.5f;
		const FVector LocalOffset(0.f, ColCenterPx * CmPerPx * ScreenRightToAnchorY,
			SlotIdx == 0 ? 0.f : -SideZDrop);
		SpawnAndDress(EmployeesInSlotOrder[SlotIdx], AnimMode, LocalOffset,
			SlotIdx == 0 ? 1.f : SideActorScale);
	}

	RefreshShowOnly();
	ApplyCaptureInvariants();
	return WideRT;
}

FVector4 AGachaCaptureStage::GetSlotUVWindow(int32 SlotIndex, int32 NumSlots, float InGroundLineV) const
{
	if (NumSlots <= 1) { return FVector4(0.f, 0.f, 1.f, 1.f); }

	const int32 Pitch = GetColumnPitchPx();
	const float RTW = static_cast<float>(CenterSlotPx + (NumSlots - 1) * Pitch);
	// px 열 시작 (좌→우): L2, L1, C, R1, R2 — 존재하는 슬롯만 왼쪽부터 빈틈없이 채운다
	auto ColStartPx = [NumSlots, Pitch](int32 K) -> float
	{
		const int32 LeftSides = (NumSlots - 1) / 2;                  // L 슬롯 수 (5장=2, 4장=1, 3장=1, 2장=0)
		switch (K)
		{
		case 0:  return static_cast<float>(LeftSides * Pitch);                                  // C
		case 1:  return static_cast<float>(LeftSides * Pitch + CenterSlotPx);                   // R1
		case 2:  return static_cast<float>((LeftSides - 1) * Pitch);                            // L1
		case 3:  return static_cast<float>(LeftSides * Pitch + CenterSlotPx + Pitch);           // R2
		default: return static_cast<float>(FMath::Max(LeftSides - 2, 0) * Pitch);               // L2 (맨 왼쪽)
		}
	};

	if (SlotIndex == 0)
	{
		return FVector4(ColStartPx(0) / RTW, 0.f, static_cast<float>(CenterSlotPx) / RTW, 1.f);
	}

	// 사이드 윈도우 = 센터 윈도우를 지면선 기준 SideActorScale 배 축소한 것(액터 축소와 같은 변환) →
	// 카드 안 프레이밍이 센터와 동일(297×371, 종횡비 0.8 = 센터와 일치라 가로 눌림 없음).
	// 열 피치보다 좁은 창을 열 중심에 얹는 구조 — 남는 폭이 이웃 침범 여유가 된다(피치를 키워도 이 창은 불변).
	const float WinW = CenterSlotPx * SideActorScale;
	const float U0 = (ColStartPx(SlotIndex) + Pitch * 0.5f - WinW * 0.5f) / RTW;
	return FVector4(U0, InGroundLineV * (1.f - SideActorScale), WinW / RTW, SideActorScale);
}

FVector4 AGachaCaptureStage::GetLiveSlotUVWindow(int32 SlotIndex) const
{
	return SlotUVWindows.IsValidIndex(SlotIndex) ? SlotUVWindows[SlotIndex] : FVector4(0.f, 0.f, 1.f, 1.f);
}

UTextureRenderTarget2D* AGachaCaptureStage::Rebuild(const FEmployeeInstance& Employee, EStageAnimMode AnimMode)
{
	DestroyCurrentWorkers();
	SlotUVWindows.Reset(); // 단발 재구성 = 풀 윈도우 (직전 배치 창 잔존 금지)
	SpawnAndDress(Employee, AnimMode, FVector::ZeroVector, 1.f);
	RefreshShowOnly();
	return RT;
}

void AGachaCaptureStage::EndReveal()
{
	if (Capture)
	{
		Capture->bCaptureEveryFrame = false;
		Capture->SetActive(false);
	}
	DestroyCurrentWorkers();
	WideRT = nullptr;
	Destroy();
}

AStickOfficeworker* AGachaCaptureStage::SpawnAndDress(const FEmployeeInstance& Employee, EStageAnimMode AnimMode,
	const FVector& LocalOffset, float ActorScale)
{
	UWorld* World = GetWorld();
	UClass* Cls = WorkerClass.Get();
	UE_LOG(LogTemp, Warning, TEXT("[GachaStage] SpawnAndDress: WorkerClass=%s, EmpID=%d, Dept=%d, EnhLv=%d"),
		Cls ? *Cls->GetName() : TEXT("NULL"), Employee.EmployeeID, static_cast<int32>(Employee.Department), Employee.EnhancementLevel);
	if (!World || !Cls) { return nullptr; }

	FTransform Xform = WorkerAnchor ? WorkerAnchor->GetComponentTransform() : GetActorTransform();
	// 사이드 배치는 앵커 로컬축 기준(앵커가 yaw 180 이라 로컬 Y 가 월드 -Y)
	Xform.AddToTranslation(Xform.TransformVectorNoScale(LocalOffset));
	Xform.SetScale3D(FVector(ActorScale));
	AStickOfficeworker* Worker = World->SpawnActorDeferred<AStickOfficeworker>(
		Cls, Xform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Worker) { return nullptr; }

	// BeginPlay 전에 자동 배회/초기화 차단 (격리 무대라 navmesh 없음)
	Worker->SetEmployeeID(Employee.EmployeeID);
	if (Worker->BehaviorComponent) { Worker->BehaviorComponent->bSkipAutoInit = true; }
	Worker->FinishSpawning(Xform);

	// 스폰 트랜스폼의 스케일은 여기서 반드시 되쓴다 — FinishSpawning 안의 ExecuteConstruction 이
	// AStickOfficeworker::OnConstruction 을 돌리고, 거기서 SetActorScale3D(BodyScale) 이 무조건 덮기 때문.
	// (BodyScale CDO=1.0 이라 센터는 우연히 맞고 사이드 0.58 만 원본 크기로 튀어 이웃 열까지 침범했다.)
	Worker->SetActorScale3D(FVector(ActorScale));

	// 코스메틱은 FinishSpawning(PostInitializeComponents=LeaderPose 바인딩) 이후에 적용해야 정합
	const EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee.EnhancementLevel);
	const EGachaTier Tier = AStickOfficeworker::ResolveGachaTier(Employee);
	Worker->ApplyWorkerCosmetics(Employee.Department, Rank, Tier, Employee.EmployeeID,
		/*bIsGachaReveal=*/true);
	// 가챠(댄스)=축하 표정, 라이브 뷰(걷기/아이들)=중립(결정적)
	Worker->SetFaceExpression(AnimMode == EStageAnimMode::Dance
		? EWorkerFaceExpression::Happy : EWorkerFaceExpression::Neutral);

	// 제자리 고정(먼 곳 스폰이라 중력 낙하 방지) + AI off
	Worker->StopRandomRoaming();
	if (UCharacterMovementComponent* Move = Worker->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement(); // MOVE_None → 중력/배회 정지, 제자리 애니만
	}

	if (AnimMode == EStageAnimMode::Walk)
	{
		Worker->PlayWalkLoopForCapture(); // 제자리 걷기(SingleNode 루프) — 카메라 앞 캣워크
	}
	else if (AnimMode == EStageAnimMode::Dance && Worker->BehaviorComponent)
	{
		Worker->BehaviorComponent->StartDance(-1, 999.f); // 랜덤 댄스, 연출 내내 유지
	}
	// Idle = 오버라이드 없음 (ABP 기본 아이들 브리딩)

	CurrentWorkers.Add(Worker);
	UE_LOG(LogTemp, Warning, TEXT("[GachaStage] Worker spawned OK: %s @ %s (캡처 프레임 안에 있어야 보임)"),
		*Worker->GetName(), *Xform.GetLocation().ToString());
	return Worker;
}

void AGachaCaptureStage::RefreshShowOnly()
{
	if (!Capture) { return; }
	Capture->ShowOnlyActors.Empty();
	for (AStickOfficeworker* W : CurrentWorkers) // 런타임 = 진짜 워커만(프리뷰는 제외)
	{
		if (W) { Capture->ShowOnlyActors.Add(W); }
	}
}

void AGachaCaptureStage::DestroyCurrentWorkers()
{
	for (AStickOfficeworker* W : CurrentWorkers)
	{
		if (W) { W->Destroy(); }
	}
	CurrentWorkers.Empty();
}
