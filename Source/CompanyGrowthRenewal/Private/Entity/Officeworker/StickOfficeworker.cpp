// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Officeworker/StickOfficeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"  // ResolveFaceExpression 이 읽는 행동/피로 상태
#include "Table/WorkerCosmeticTable.h"
#include "Manager/TableManagerSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"  // FSkeletalMaterial (UE5.4)
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Data/EmployeeTypes.h"        // FEmployeeInstance / ELootBoxRarity 전이
#include "Enum/LootBoxRarity.h"
#include "Engine/World.h"              // GetWorld()->IsGameWorld() (에디터 프리뷰 게이트)
#include "Engine/DataTable.h"          // 에디터 프리뷰 DT 직접 로드 FindRow

namespace
{
	// 외형 축별 해시 솔트. 전부 같은 Id 의 나머지를 쓰면(구 방식: Id%26, Id%5, Id%6) 축끼리 상관이 생겨
	// 도달 불가능한 조합이 절반쯤 되고, 연속 채용된 직원들이 규칙적으로 닮아 보인다.
	constexpr uint32 SaltFace  = 0x9E3779B1u;
	constexpr uint32 SaltSkin  = 0x85EBCA6Bu;
	constexpr uint32 SaltHair  = 0xC2B2AE35u;
	constexpr uint32 SaltCloth = 0x27D4EB2Fu;
}

AStickOfficeworker::AStickOfficeworker()
{
	// === CDO 프리뷰용: 생성자에서 셋업 → BP_StickOfficeworker 뷰포트에서 보고 드래그 튜닝 가능 ===
	// 바디 스켈레탈 메시 (cgtrader + Mixamo). CDO 한정 ConstructorHelpers — BP 미리보기 + 쿠킹 하드참조.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMeshFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/SK_StickmanCG"));
	if (BodyMeshFinder.Succeeded() && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(BodyMeshFinder.Object);
	}

	// SK_StickmanCG 에셋이 원본 FBX 단위(~11배) 그대로 임포트돼 있어 메시 컴포넌트에서 0.09 보정.
	// BP 직렬화 값에만 두면 한 번 만질 때 날아가는 사고가 나서(2026-06-10) CDO 에 고정.
	if (GetMesh())
	{
		GetMesh()->SetRelativeScale3D(FVector(0.09f));
	}

	// 애니메이션 BP (직원 상태머신 — 네이티브 Mixamo 애니)
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/ABP_StickWorker"));
	if (AnimBPFinder.Succeeded() && GetMesh())
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(AnimBPFinder.Class);
	}

	// 안경 — Head 본 리지드 스킨(SK_Glasses, 스켈레톤 공유) + LeaderPose(PostInitializeComponents). 머리/넥타이와 동일.
	GlassesComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Glasses"));
	GlassesComponent->SetupAttachment(GetMesh());
	GlassesComponent->SetMobility(EComponentMobility::Movable);
	GlassesComponent->SetVisibility(false);
	GlassesComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlassesComponent->SetCastShadow(false);
	GlassesComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// 변주 풀 — 소프트참조(쿠킹 안전, /Game/.../StickmanCG 폴더는 이미 쿠킹 등록됨)
	GlassesVariants.Empty(3);
	GlassesVariants.Add(TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Glasses/SK_Glasses1.SK_Glasses1"))));
	GlassesVariants.Add(TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Glasses/SK_Glasses2.SK_Glasses2"))));
	GlassesVariants.Add(TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Glasses/SK_Glasses3.SK_Glasses3"))));

	// 코스메틱 5종 — 본에 리지드 스킨된 SkeletalMesh(SK_Cos_*, SK_StickmanCG_Skeleton 공유).
	// GetMesh() 에 소켓 없이 부착(identity → 바디 월드 상속) + PostInitializeComponents 에서 LeaderPose 설정.
	// → 위치/회전/스케일이 바디 본을 그대로 추종. 수동 트랜스폼/소켓 수학 불필요.
	NecktieComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Necktie"));
	NecktieComponent->SetupAttachment(GetMesh());
	NecktieComponent->SetMobility(EComponentMobility::Movable);
	NecktieComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NecktieComponent->SetCastShadow(false);
	NecktieComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TieFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Cosmetics/SK_Cos_Tie"));
	if (TieFinder.Succeeded()) { NecktieComponent->SetSkeletalMeshAsset(TieFinder.Object); }

	// 신발 = 바디 발 메시(이미 신발 모양) + M_Shoes 슬롯 ShoeColor 로 처리 → 별도 클로그 메시 폐지(2026-06-11).
	BeltComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CosmeticBelt"));  // 부모 모듈러 "Belt" 와 이름 충돌 회피
	BeltComponent->SetupAttachment(GetMesh());
	BeltComponent->SetMobility(EComponentMobility::Movable);
	BeltComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeltComponent->SetCastShadow(false);
	BeltComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BeltFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Cosmetics/SK_Cos_Belt"));
	if (BeltFinder.Succeeded()) { BeltComponent->SetSkeletalMeshAsset(BeltFinder.Object); }

	CollarComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Collar"));
	CollarComponent->SetupAttachment(GetMesh());
	CollarComponent->SetMobility(EComponentMobility::Movable);
	CollarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollarComponent->SetCastShadow(false);
	CollarComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CollarFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Cosmetics/SK_Cos_Collar"));
	if (CollarFinder.Succeeded()) { CollarComponent->SetSkeletalMeshAsset(CollarFinder.Object); }

	// 머리카락 캡 — Head 본 리지드 스킨(SK_Cos_Hair, 스켈레톤 공유) + LeaderPose. 색은 ApplyWorkerCosmetics 에서 변주.
	HairComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CosmeticHair"));  // 부모 Hair* 예약명 회피
	HairComponent->SetupAttachment(GetMesh());
	HairComponent->SetMobility(EComponentMobility::Movable);
	HairComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HairComponent->SetCastShadow(false);
	HairComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HairFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Cosmetics/SK_Cos_Hair"));
	if (HairFinder.Succeeded()) { HairComponent->SetSkeletalMeshAsset(HairFinder.Object); }

	// 슬롯 머티리얼 베이스 (CDO ConstructorHelpers — 하드참조 쿠킹). 런타임에 슬롯별로 SetMaterial.
	// 색 영역 = M_StickRegion(RegionColor/Metallic), 얼굴 = M_StickFaceSlot(FaceTex/SkinTone).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RegionMatFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/M_StickRegion"));
	if (RegionMatFinder.Succeeded()) { RegionBaseMat = RegionMatFinder.Object; }
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FaceMatFinder(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/M_StickFaceSlot"));
	if (FaceMatFinder.Succeeded()) { FaceBaseMat = FaceMatFinder.Object; }

	// 초상화 캡쳐 카메라를 캡슐(루트)로 재부착 — 부모는 Face(=GetMesh())에 붙이지만 스틱 Face는 ~0.09배
	// 스케일이라 카메라 상대 오프셋이 11× 축소돼 버린다. 루트 기준이면 BP 뷰포트 튜닝이 액터 공간(언스케일).
	if (FaceCaptureCamera && GetCapsuleComponent())
	{
		FaceCaptureCamera->SetupAttachment(GetCapsuleComponent());
	}

	// 초상화 기본 포즈 = 걷기 첫 프레임 (레퍼런스 T포즈보다 자연스러운 스탠스). BP 에서 교체 가능.
	PortraitPoseAnim = TSoftObjectPtr<UAnimSequence>(
		FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Anims/AS_Walk.AS_Walk")));
}

void AStickOfficeworker::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body) { return; }

	// [근본 수정 2026-06-12] BP_StickOfficeworker 가 옛 컴포넌트 레이아웃(클로그 제거 / HairComponent StaticMesh→Skeletal 전환 이전)으로
	// 직렬화돼, 로드 시 stale None 이 생성자의 CreateDefaultSubobject 멤버 할당을 덮어써 4개 코스메틱 멤버 포인터가 null 이 됐다.
	// 컴포넌트 객체 자체(Necktie/CosmeticBelt/Collar/CosmeticHair)는 존재해 화면엔 보이지만, LeaderPose 셋업이 null 에 적용돼 안 따라갔음.
	// → 존재하는 네이티브 컴포넌트를 이름으로 멤버 포인터에 재연결(자가복구). BP 재컴파일로도 고칠 수 있으나 코드 복구가 견고.
	if (!NecktieComponent || !BeltComponent || !CollarComponent || !HairComponent || !GlassesComponent)
	{
		TArray<USkeletalMeshComponent*> SkelComps;
		GetComponents(SkelComps);
		for (USkeletalMeshComponent* C : SkelComps)
		{
			const FString CN = C->GetName();
			if (CN == TEXT("Necktie")) { NecktieComponent = C; }
			else if (CN == TEXT("CosmeticBelt")) { BeltComponent = C; }
			else if (CN == TEXT("Collar")) { CollarComponent = C; }
			else if (CN == TEXT("CosmeticHair")) { HairComponent = C; }
			else if (CN == TEXT("Glasses")) { GlassesComponent = C; }
		}
		UE_LOG(LogTemp, Warning, TEXT("[StickOfficeworker] 코스메틱 멤버 포인터가 null 이라 이름으로 재연결함 — BP_StickOfficeworker 재컴파일+저장 권장(stale 컴포넌트 직렬화)."));
	}

	// 리지드 스킨 코스메틱 → 바디 메시 포즈 추종 (SK_Cos_* 가 SK_StickmanCG_Skeleton 공유). ModularOfficeworker 와 동일 패턴.
	USkeletalMeshComponent* Cosmetics[] = {
		NecktieComponent, BeltComponent, CollarComponent, HairComponent, GlassesComponent };
	for (USkeletalMeshComponent* Comp : Cosmetics)
	{
		if (Comp)
		{
			Comp->SetLeaderPoseComponent(Body);
			Comp->bUseBoundsFromLeaderPoseComponent = true;
			Comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		}
	}
}

void AStickOfficeworker::LoadAssetsAsync()
{
	// 단일 메시라 비동기 불필요 — 동기 로드 후 즉시 완료 통지 (부모 BeginPlay 에서 호출됨)
	OnAssetsLoaded();
}

void AStickOfficeworker::OnAssetsLoaded()
{
	// 슬롯별 베이스 머티리얼 + MID 셋업(멱등) → 얼굴 MID 캐시 + 초기 중립 표정.
	// 색은 ApplyWorkerCosmetics(GameMode 스폰 후)에서 DT 값으로 적용.
	SetupSlotMaterials();
	SetFaceExpression(EWorkerFaceExpression::Neutral);
}

void AStickOfficeworker::SetupSlotMaterials()
{
	USkeletalMeshComponent* Body = GetMesh();
	USkeletalMesh* SkMesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!Body || !SkMesh) return;

	// 생성자 ConstructorHelpers 로 로드한 베이스 머티리얼이 BP stale 직렬화로 null 이 될 수 있다(코스메틱 컴포넌트 null 과 동일 원인).
	// null 이면 슬롯 MID 가 안 만들어져 옷/얼굴 색이 통째 안 칠해지므로 on-demand 로 폴백 로드해 자가복구.
	if (!RegionBaseMat)
	{
		RegionBaseMat = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/M_StickRegion.M_StickRegion"))).LoadSynchronous();
	}
	if (!FaceBaseMat)
	{
		FaceBaseMat = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/M_StickFaceSlot.M_StickFaceSlot"))).LoadSynchronous();
	}

	// SK_StickmanCG 는 머티리얼 슬롯이 2벌(명명 0-5: Skin/Shirt/Pants/Shoes/Hands/Face + Fbx Default 6-11 동일 순서).
	// 실제 메시 섹션은 Fbx Default 슬롯(6-11)을 렌더하므로 논리 부위는 이름이 아니라 인덱스(i%6)로 판정해야 양쪽 다 칠해진다.
	// 부위: 0=Skin 1=Shirt 2=Pants 3=Shoes 4=Hands 5=Face.
	const TArray<FSkeletalMaterial>& Slots = SkMesh->GetMaterials();
	const int32 Num = FMath::Min(Body->GetNumMaterials(), Slots.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		const bool bFace = (i % 6 == 5);
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(i));

		// 레벨 배치 액터는 에디터 프리뷰(OnConstruction)가 만든 MID 를 컴포넌트 OverrideMaterials 에 남기고,
		// 그게 UPROPERTY 라 .umap 에 통째로 직렬화된다. 그 MID 는 만들어진 시점의 파라미터 오버라이드를
		// 그대로 안고 있어, 나중에 머티리얼 기본값이 바뀌어도 옛 값이 계속 이긴다.
		// 실제 사고(2026-07-28): 구 MID 의 FaceTex=Face_2_01 이 아틀라스 기본값을 가려 초상화 얼굴이 통째로 사라짐.
		// RF_WasLoaded = 디스크에서 온 객체. 런타임에 우리가 만든 MID 는 이 플래그가 없으므로,
		// "구 세션이 저장해 둔 것"만 정확히 골라 버릴 수 있다(플래그 변수 불필요, 재실행에도 멱등).
		if (MID && MID->HasAnyFlags(RF_WasLoaded))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[StickOfficeworker] 맵에 직렬화된 구 MID 폐기 — 슬롯 %d (%s)"), i, *MID->GetName());
			MID = nullptr;
		}

		if (!MID)
		{
			UMaterialInterface* Base = bFace ? FaceBaseMat : RegionBaseMat;
			if (!Base)
			{
				// 베이스 미로드(에셋 누락/쿠킹 실패) — 베이스 없는 MID 는 슬롯을 깨뜨리므로 생성하지 않고 스킵
				UE_LOG(LogTemp, Warning, TEXT("[StickOfficeworker] %s 슬롯 베이스 머티리얼 미로드 — 슬롯 %d 스킵"), bFace ? TEXT("Face") : TEXT("Region"), i);
				continue;
			}
			Body->SetMaterial(i, Base);
			MID = Body->CreateAndSetMaterialInstanceDynamic(i);
		}
		if (bFace) { FaceMID = MID; }
	}

}

void AStickOfficeworker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 액터 전체(캡슐+메시+페이스플레인+안경) 균일 축소. CharacterMovement 가 매 틱 캡슐 바닥을 바닥에 재정렬하므로 발은 항상 지면.
	SetActorScale3D(FVector(BodyScale));

	// 에디터 프리뷰 정합: BP 뷰포트에서도 코스메틱이 몸에 붙어 보이게(스폰 시엔 ApplyWorkerCosmetics 가 권위 재적용).
	// 본 소켓 트랜스폼은 스켈레탈 메시 에셋만 있으면 rest 포즈로 유효하므로 OnConstruction 에서도 동작.
	FitAllCosmetics();

#if WITH_EDITOR
	// 에디터 뷰포트 프리뷰: 게임 월드(스폰)가 아닐 때만 실제 파이프라인을 돌려 부서색/직급/희귀도/기분을 즉시 반영.
	// ApplyWorkerCosmetics 내부에서 SetupSlotMaterials 까지 호출되므로 몸 슬롯 머티리얼(색/얼굴)이 뷰포트에 표시된다.
	// (런타임 스폰 월드에선 IsGameWorld()==true → 스킵, GameMode 가 실제 직원 데이터로 권위 적용.)
	if (bEditorPreview && GetWorld() && !GetWorld()->IsGameWorld())
	{
		ApplyWorkerCosmetics(PreviewDepartment, PreviewRank, PreviewTier, PreviewEmployeeID);
		SetFaceExpression(PreviewExpression);
	}
#endif
}

void AStickOfficeworker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 행동 상태에서 직접 표정을 뽑는다 — 변화 시에만 스왑(같은 표정이면 no-op).
	// 버블(1초 주기)이 아니라 상태를 읽으므로 졸음/폭주 같은 짧은 순간도 놓치지 않는다.
	const EWorkerFaceExpression Desired = ResolveFaceExpression();
	if (Desired != LastFaceExpression)
	{
		SetFaceExpression(Desired);
	}
}

uint32 AStickOfficeworker::AppearanceHash(int32 EmployeeID, uint32 Salt)
{
	uint32 H = static_cast<uint32>(EmployeeID) ^ (Salt * 2654435761u);
	H ^= H >> 16; H *= 2246822519u;
	H ^= H >> 13; H *= 3266489917u;
	H ^= H >> 16;
	return H;
}

EWorkerFaceExpression AStickOfficeworker::ResolveFaceExpression() const
{
	const UEmployeeBehaviorComponent* Behavior = BehaviorComponent;
	if (!Behavior) { return EWorkerFaceExpression::Neutral; }

	// 1) 피로 다운 구간이 최우선 — 플레이어가 손대야 하는 순간이라 얼굴이 가장 크게 말해야 한다.
	//    단계(Telegraph/Slumping)는 비공개라, 공개된 착석 여부 + 자세 인덱스로 사유를 읽는다.
	//    (SeatedPoseIndex 2=졸음 / 1=딴짓 — EWorkerDownReason 이 진입 시 정한 값이 그대로 반영된다.)
	if (Behavior->IsAwaitingCatch())
	{
		if (!Behavior->bIsSeated)          { return EWorkerFaceExpression::Surprised; }  // 폭주 — 자리 박차고 배회
		if (Behavior->SeatedPoseIndex == 2){ return EWorkerFaceExpression::Asleep; }     // 꾸벅 / 늘어짐
		if (Behavior->SeatedPoseIndex == 1){ return EWorkerFaceExpression::Happy; }      // 딴짓 — 태평
		return EWorkerFaceExpression::Tired;
	}

	// 2) 버프 — 기존 버블 우선순위와 같은 규칙(종류별로 다른 얼굴)
	if (Behavior->HasActiveBuff())
	{
		if (Behavior->GetTotalCritChanceBonus() > 0.0f)   { return EWorkerFaceExpression::Happy; }
		if (Behavior->GetWorkSpeedMultiplier() < 1.0f)    { return EWorkerFaceExpression::Angry; }
		return EWorkerFaceExpression::Wink;
	}

	// 3) 상시 피로 — 다운까지 안 갔어도 지친 티는 난다 (Tired 밴드부터 graded 로 표시)
	const EFatigueBand Band = Behavior->GetFatigueBand();
	if (Band == EFatigueBand::Slacking || Band == EFatigueBand::Tired) { return EWorkerFaceExpression::Tired; }
	if (Behavior->EmployeeState == EEmployeeState::Sitting)            { return EWorkerFaceExpression::Tired; }

	if (Behavior->CurrentBehaviorMode == EEmployeeBehaviorMode::Stage) { return EWorkerFaceExpression::Focused; }
	return EWorkerFaceExpression::Neutral;
}

void AStickOfficeworker::SetFaceExpression(EWorkerFaceExpression Expression)
{
	if (!FaceMID) return;

	// enum 값이 곧 아틀라스 표정 인덱스 — 매핑 테이블 불필요(순서가 계약).
	const int32 Expr = FMath::Clamp(static_cast<int32>(Expression), 0, FaceExpressionCount - 1);

	// 정체성 = 직원별 고정. 표정만 바뀌므로 같은 직원은 상태가 변해도 같은 얼굴이다.
	const int32 Identity = static_cast<int32>(AppearanceHash(GetEmployeeID(), SaltFace) % FaceIdentityCount);

	// 텍스처 교체가 아니라 UV 오프셋만 — 기분 변화에 동기 로드가 사라진다.
	// 선형 인덱스를 정사각 그리드로 접는다(생성기 팩 순서와 반드시 동일).
	const int32 Index = Identity * FaceExpressionCount + Expr;
	const FLinearColor Cell(
		static_cast<float>(Index % FaceAtlasColumns) / FaceAtlasColumns,
		static_cast<float>(Index / FaceAtlasColumns) / FaceAtlasRows,
		1.0f / FaceAtlasColumns,
		1.0f / FaceAtlasRows);

	// 슬롯에서 매번 다시 가져온다 — 캐시된 FaceMID 는 재구성/에디터 프리뷰를 거치면 화면에 안 뜨는
	// 인스턴스를 가리킬 수 있다(색 루프가 이미 재조회 방식인데 여기만 캐시라 어긋났음).
	USkeletalMeshComponent* Body = GetMesh();
	USkeletalMesh* SkMesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	int32 Applied = 0;
	if (Body && SkMesh)
	{
		const int32 Num = FMath::Min(Body->GetNumMaterials(), SkMesh->GetMaterials().Num());
		for (int32 i = 0; i < Num; ++i)
		{
			if (i % 6 != 5) { continue; }
			if (UMaterialInstanceDynamic* SlotMID = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(i)))
			{
				SlotMID->SetVectorParameterValue(TEXT("FaceCell"), Cell);
				++Applied;
			}
		}
	}
	if (Applied == 0)
	{
		FaceMID->SetVectorParameterValue(TEXT("FaceCell"), Cell);
	}
	LastFaceExpression = Expression;

	// SceneCapture 는 텍스처 스트리밍 계산에 기여하지 않는다. 초상화 리그는 화면 밖(Z≈-4864)이라
	// 메인 뷰에 절대 안 잡혀 아틀라스 밉이 하나도 상주하지 않고, 그러면 샘플이 검정(0)이 되어
	// 얼굴이 통째로 사라진다. 구 방식은 얼굴마다 개별 텍스처를 LoadSynchronous 해서 이 문제가 없었다.
	if (bIsPortraitMode)
	{
		UTexture* FaceTexture = nullptr;
		if (FaceMID->GetTextureParameterValue(FMaterialParameterInfo(TEXT("FaceTex")), FaceTexture))
		{
			if (UTexture2D* Atlas = Cast<UTexture2D>(FaceTexture))
			{
				Atlas->bForceMiplevelsToBeResident = true;
				Atlas->SetForceMipLevelsToBeResident(30.f);
			}
		}
	}

}

FName AStickOfficeworker::MakeCosmeticRowName(EEmployeeDepartment Department, EEmployeeRank Rank, EGachaTier Tier)
{
	return FName(*FString::Printf(TEXT("D%d_R%d_T%d"),
		static_cast<int32>(Department), static_cast<int32>(Rank), static_cast<int32>(Tier)));
}

bool AStickOfficeworker::ResolveCosmeticRow(EEmployeeDepartment Department, EEmployeeRank Rank,
	EGachaTier Tier, FWorkerCosmeticTable& OutRow) const
{
	const FName RowName = MakeCosmeticRowName(Department, Rank, Tier);

	// 1) 런타임: TableManagerSubsystem 캐시 맵 (GameInstance 존재 시)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bFound = false;
			OutRow = TableMgr->GetWorkerCosmeticData(RowName, bFound);
			if (bFound) { return true; }
		}
	}

	// 2) 에디터 프리뷰(게임 인스턴스 없음): DT 에셋 직접 로드 → FindRow
	static const TCHAR* DTPath = TEXT("/Game/CompanyGrowth/Table/Employee/DT_WorkerCosmetic.DT_WorkerCosmetic");
	if (UDataTable* DT = TSoftObjectPtr<UDataTable>(FSoftObjectPath(DTPath)).LoadSynchronous())
	{
		if (const FWorkerCosmeticTable* Found = DT->FindRow<FWorkerCosmeticTable>(RowName, TEXT("StickEditorPreview"), false))
		{
			OutRow = *Found;
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[StickOfficeworker] 프리뷰 코스메틱 행 '%s' 없음 (DT_WorkerCosmetic)"), *RowName.ToString());
	}
	return false;
}

#if WITH_EDITOR
void AStickOfficeworker::PreviewNextEmployee()
{
	PreviewEmployeeID++;
	RerunConstructionScripts();  // OnConstruction 재실행 → 새 ID 의 개인 변주(스킨/헤어/옷 밝기) 즉시 반영
}
#endif

void AStickOfficeworker::FitAllCosmetics()
{
	// 코스메틱 6종(넥타이/클로그L·R/벨트/카라/헤어/안경) 전부 리지드 스킨(LeaderPose) → 수동 트랜스폼 불필요(no-op).
}

void AStickOfficeworker::ApplyWorkerCosmetics(EEmployeeDepartment Department, EEmployeeRank Rank,
	EGachaTier Tier, int32 InEmployeeID, bool bIsGachaReveal)
{
	SetEmployeeID(InEmployeeID);

	// DT lookup — 런타임=서브시스템 캐시, 에디터 프리뷰=DT 직접 로드. 미발견 시 기본 구조체값 + 내부 loud 경고.
	FWorkerCosmeticTable Row;
	ResolveCosmeticRow(Department, Rank, Tier, Row);

	// 개인별 변주 (부서/직급 무관, EmployeeID deterministic) — 같은 DT 키 직원도 달라 보이게.
	// 스킨톤은 개인 특성이라 부서 DT 와 별개 → 코드 팔레트 허용. 옷 밝기/키도 미세 변주.
	const int32 Id = FMath::Abs(InEmployeeID);
	static const FLinearColor SkinPalette[] = {
		FLinearColor(0.96f, 0.82f, 0.70f), FLinearColor(0.90f, 0.72f, 0.58f),
		FLinearColor(0.82f, 0.62f, 0.48f), FLinearColor(0.70f, 0.52f, 0.40f),
		FLinearColor(0.58f, 0.42f, 0.32f),
	};
	const FLinearColor SkinTone = SkinPalette[AppearanceHash(InEmployeeID, SaltSkin) % UE_ARRAY_COUNT(SkinPalette)];
	const float ClothMul = 0.88f + (AppearanceHash(InEmployeeID, SaltCloth) % 25) * 0.01f;   // 0.88~1.12 옷 밝기 변주

	// 슬롯 분리 코스메틱: SetupSlotMaterials(멱등)로 슬롯별 베이스+MID 보장 → 슬롯 이름으로 색 적용.
	// 경계가 메시 토폴로지를 따라가 깔끔(UV 마스크 톱니 폐기). 얼굴 슬롯은 배경 SkinTone 만, 표정은 SetFaceExpression.
	SetupSlotMaterials();
	USkeletalMeshComponent* Body = GetMesh();
	USkeletalMesh* SkMesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (Body && SkMesh)
	{
		const TArray<FSkeletalMaterial>& SlotMats = SkMesh->GetMaterials();
		const int32 Num = FMath::Min(Body->GetNumMaterials(), SlotMats.Num());
		for (int32 i = 0; i < Num; ++i)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(i));
			if (!MID) continue;

			// 논리 부위 = 인덱스 i%6 (0=Skin 1=Shirt 2=Pants 3=Shoes 4=Hands 5=Face). 명명(0-5)·Fbx Default(6-11) 양쪽 동일 적용.
			const int32 Part = i % 6;
			if (Part == 5)
			{
				MID->SetVectorParameterValue(TEXT("SkinTone"), SkinTone);  // 얼굴 배경 = 개인 스킨톤 (표정은 SetFaceExpression)
				continue;
			}
			FLinearColor RegionColor = SkinTone;   // Part 0(Skin/머리·목) = 개인 스킨톤
			float Metal = 0.f;
			if (Part == 1)      { RegionColor = Row.TieColor * ClothMul; }    // 상의 — 부서색 + 개인 밝기
			else if (Part == 2) { RegionColor = Row.PantsColor * ClothMul; }  // 하의 + 개인 밝기
			else if (Part == 3) { RegionColor = Row.ShoeColor; }             // 신발 — 직급 단계
			else if (Part == 4)                                              // 손 — 희귀도: 개인 스킨→골드
			{
				RegionColor = FMath::Lerp(SkinTone, Row.GoldHandColor, Row.HandMetalGold);
				Metal = Row.HandMetalGold;
			}
			MID->SetVectorParameterValue(TEXT("RegionColor"), RegionColor);
			MID->SetScalarParameterValue(TEXT("Metallic"), Metal);
		}
	}

	// 신발은 바디 "Shoes" 슬롯(M_Shoes)이 위 색 루프에서 ShoeColor 로 칠해져 처리됨 — 별도 클로그 메시/숨김 폐지(2026-06-11).

	// 색 적용 후 표정 재적용 (FaceMID 는 SetupSlotMaterials 에서 캐시됨)
	SetFaceExpression(ResolveFaceExpression());

	// 안경 토글 (직급 임계값)
	// 안경: 등급 무관 완전 랜덤 (EmployeeID 결정적). 착용 여부·스타일 모두 ID 해시 → DT bWearGlasses/GlassesMesh 미사용.
	// 같은 직원은 항상 같은 결과(스킨/머리색과 동일), 전체는 GlassesChance 비율로 3종이 골고루 섞임.
	const uint32 UId = (uint32)Id;
	const int32 NumGlasses = GlassesVariants.Num();
	const uint32 WearRoll = (UId * 2246822519u) % 1000u;                 // 착용 게이트(스타일과 독립 해시)
	bWearingGlasses = (NumGlasses > 0) && ((float)WearRoll / 1000.f < GlassesChance);
	if (GlassesComponent)
	{
		if (bWearingGlasses)
		{
			USkeletalMesh* Glasses = GlassesVariants[(int32)((UId * 2654435761u) % (uint32)NumGlasses)].LoadSynchronous();
			if (!Glasses) { Glasses = DefaultGlassesMesh.LoadSynchronous(); }
			if (Glasses)
			{
				GlassesComponent->SetSkeletalMeshAsset(Glasses);
				GlassesComponent->SetLeaderPoseComponent(GetMesh());   // 메시 교체 후 재설정 (바디 포즈 추종)
				GlassesComponent->bUseBoundsFromLeaderPoseComponent = true;
			}
		}
		GlassesComponent->SetVisibility(bWearingGlasses && GlassesComponent->GetSkeletalMeshAsset() != nullptr);
	}

	// 넥타이 = 실크 텍스처(M_Cos_Tie) × 부서색(TieColor 파라미터). 실크가 이미 음영을 줘 셔츠보다 자연히 진함.
	if (NecktieComponent && NecktieComponent->GetSkeletalMeshAsset())
	{
		UMaterialInstanceDynamic* TieMID = Cast<UMaterialInstanceDynamic>(NecktieComponent->GetMaterial(0));
		if (!TieMID)
		{
			UMaterialInterface* TieBase = TSoftObjectPtr<UMaterialInterface>(
				FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Cosmetics/M_Cos_Tie.M_Cos_Tie"))).LoadSynchronous();
			if (!TieBase) { TieBase = NecktieComponent->GetMaterial(0); }  // 폴백: 메시 베이크 머티리얼
			if (TieBase) { TieMID = NecktieComponent->CreateDynamicMaterialInstance(0, TieBase); }
		}
		if (TieMID)
		{
			TieMID->SetVectorParameterValue(TEXT("TieColor"), Row.TieColor);  // 실크 × 부서색
		}
	}

	// 헤어 = 개인 변주(EmployeeID 결정적) 색만. 메시는 생성자에서 SK_Cos_Hair 고정(리지드 스킨).
	// 스타일 분기(단발 등)는 추후 SK_Cos_HairLong 추가 + Id/8 분기로 확장 (메시 교체는 SetSkeletalMeshAsset).
	if (HairComponent && HairComponent->GetSkeletalMeshAsset())
	{
		// 색: 자연 모발색 6팔레트 (흑/다크브라운/브라운/적갈/다크블론드/그레이)
		static const FLinearColor HairPalette[] = {
			FLinearColor(0.03f, 0.025f, 0.020f), FLinearColor(0.09f, 0.060f, 0.040f),
			FLinearColor(0.17f, 0.100f, 0.055f), FLinearColor(0.16f, 0.070f, 0.040f),
			FLinearColor(0.34f, 0.230f, 0.120f), FLinearColor(0.30f, 0.300f, 0.320f),
		};
		const FLinearColor HairColor = HairPalette[AppearanceHash(InEmployeeID, SaltHair) % UE_ARRAY_COUNT(HairPalette)];
		UMaterialInstanceDynamic* HairMID = Cast<UMaterialInstanceDynamic>(HairComponent->GetMaterial(0));
		if (!HairMID)
		{
			UMaterialInterface* HairBaseMat = TSoftObjectPtr<UMaterialInterface>(
				FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/Cosmetics/M_Cos_Hair.M_Cos_Hair"))).LoadSynchronous();
			if (!HairBaseMat) { HairBaseMat = HairComponent->GetMaterial(0); }  // 폴백: 메시 베이크 머티리얼
			if (HairBaseMat) { HairMID = HairComponent->CreateDynamicMaterialInstance(0, HairBaseMat); }
		}
		if (HairMID) { HairMID->SetVectorParameterValue(TEXT("HairColor"), HairColor); }
	}

	// 희귀도 골드 림은 가챠 공개 연출에서만 사용한다. 선택 중일 때는 선택 글로우를 우선한다.
	if (Body && !IsSelected())
	{
		UMaterialInterface* Rim = RimOverlayMaterial ? RimOverlayMaterial : SelectionOverlayMaterial;
		const bool bShowRarityOverlay = ShouldShowRarityOverlay(Tier, bIsGachaReveal);
		Body->SetOverlayMaterial((bShowRarityOverlay && Rim) ? Rim : nullptr);
	}

	// 코스메틱 정합 (스폰 후 권위 적용): 본 앵커 + 스케일 보정으로 몸에 안착 + 본 추적. BP 수동 트랜스폼 불필요.
	FitAllCosmetics();
}

bool AStickOfficeworker::ShouldShowRarityOverlay(EGachaTier Tier, bool bIsGachaReveal)
{
	return bIsGachaReveal && Tier == EGachaTier::Premium;
}

EGachaTier AStickOfficeworker::ResolveGachaTier(const FEmployeeInstance& Employee)
{
	switch (Employee.PotentialAbility.CurrentRarity)
	{
	case ELootBoxRarity::Common:
	case ELootBoxRarity::Unusual:
		return EGachaTier::Normal;
	case ELootBoxRarity::Rare:
		return EGachaTier::Advanced;
	case ELootBoxRarity::Epic:
	case ELootBoxRarity::Legendary:
	case ELootBoxRarity::Mythic:
		return EGachaTier::Premium;
	default:
		return EGachaTier::Normal;
	}
}

void AStickOfficeworker::ApplyCosmeticsForPortrait(EEmployeeDepartment Department, EEmployeeRank Rank,
	EGachaTier Tier, int32 InEmployeeID)
{
	ApplyWorkerCosmetics(Department, Rank, Tier, InEmployeeID);

	// 캡쳐는 정적 헤드샷 — 상태 시뮬레이션이 없으므로 표정을 중립으로 고정(결정적).
	SetFaceExpression(EWorkerFaceExpression::Neutral);

	// 정지 포즈: 부모 bIsPortraitMode 가 굳혀둔 레퍼런스(T)포즈 대신 지정 시퀀스의 한 프레임으로 고정.
	// SingleNode 모드 + SetPosition 으로 재생 없이 정적 평가(이후 SetupFaceCapture 의 RefreshBoneTransforms 가 반영).
	if (UAnimSequence* Pose = PortraitPoseAnim.LoadSynchronous())
	{
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Body->SetAnimation(Pose);
			Body->SetPosition(PortraitPoseTime, false);
			Body->Stop();
		}
	}

	// 부모 캡쳐 파이프라인(SetupFaceCapture → 머티리얼 폴링 → CaptureAndSaveFacePortrait)은
	// OnMeshLoadCompleted 발화로 시작된다. 스틱은 메시가 이미 로드돼 있어 여기서 즉시 발화.
	OnMeshLoadCompleted.ExecuteIfBound();
}

void AStickOfficeworker::PlayWalkLoopForCapture()
{
	UAnimSequence* Walk = PortraitPoseAnim.LoadSynchronous();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Walk || !Body)
	{
		return;
	}

	// ABP(속도 주도 상태머신) 우회 — 무대 워커는 MOVE_None 이라 속도 0 = idle 로 굳으므로 직접 루프
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->SetAnimation(Walk);
	Body->SetPosition(0.f, false);
	Body->Play(true);
}
