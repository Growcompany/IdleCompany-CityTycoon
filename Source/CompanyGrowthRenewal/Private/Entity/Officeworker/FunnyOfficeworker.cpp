// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"  // ResolveFaceExpression 이 읽는 행동/피로 상태
#include "Table/FunnyWorkerCosmeticTable.h"
#include "Table/FunnyPartPoolTable.h"
#include "Table/FunnyWardrobeTable.h"
#include "Table/FunnyGarmentStyleTable.h"
#include "Table/FunnyExpressionTable.h"
#include "Entity/Officeworker/FunnyPartPicker.h"
#include "Entity/Officeworker/EmployeeReactionComponent.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"  // 모션 줄이기면 깜빡임을 끈다
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Utils/EmployeeEquipmentAppearanceResolver.h"
#include "Utils/EmployeeEquipmentVisuals.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
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
// 캐릭터 외곽선 = 뒤집은 껍질 Overlay Material(풀스크린 패스·CustomDepth 없음). 구 후처리 스텐실 외곽선은 2026-10-10 폐기.
// 오버레이 슬롯은 하나뿐이라 희귀도 림이 없을 때만 껍질 외곽선이 들어간다.
static UMaterialInterface* LoadHullOutlineMaterial(EFunnyPartSlot PartSlot = EFunnyPartSlot::Body)
{
	const bool bDetailPart = PartSlot == EFunnyPartSlot::Eyebrow || PartSlot == EFunnyPartSlot::Glasses
		|| PartSlot == EFunnyPartSlot::Mustache || PartSlot == EFunnyPartSlot::Glove;
	const bool bAccessoryPart = PartSlot == EFunnyPartSlot::Hair || PartSlot == EFunnyPartSlot::Headwear
		|| PartSlot == EFunnyPartSlot::Back;
	TSoftObjectPtr<UMaterialInterface> Path(FSoftObjectPath(
		bDetailPart
			? TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/MI_CharOutlineDetail.MI_CharOutlineDetail")
			: bAccessoryPart
				? TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/MI_CharOutlineAccessory.MI_CharOutlineAccessory")
				: TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_CharOutlineHull.M_CharOutlineHull")));
	return Path.LoadSynchronous();
}

namespace FunnyBody
{
	const TCHAR* const MaterialPath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/MI_FunnyBody.MI_FunnyBody");
}

// 얼굴 판 = 에디터 스크립트(bake_face_sheet)가 만드는 에셋이라 아직 없을 수 있다 — 없으면 얼굴 슬롯을 숨기고 한 번 크게 알린다.
// 판 테두리가 검은 선으로 보여서 이 슬롯에는 외곽선·희귀도 림 오버레이를 걸지 않는다.
namespace FunnyFaceSheet
{
	const TCHAR* const MeshPath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Face/SK_FaceSheet.SK_FaceSheet");
	const TCHAR* const MaterialPath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Face/M_FunnyFaceSheet.M_FunnyFaceSheet");
	const TCHAR* const ExpressionTablePath = TEXT("/Game/CompanyGrowth/Table/Employee/DT_FunnyExpression.DT_FunnyExpression");
	const TCHAR* const StyleTablePath = TEXT("/Game/CompanyGrowth/Table/Employee/DT_FunnyFaceStyle.DT_FunnyFaceStyle");
	constexpr int32 ClosedEyesCell = 8;   // 눈 아틀라스 칸 8 = 감은 눈(깜빡임·잠 공용)
	constexpr float BlinkDuration = 0.11f;
	constexpr float BlinkMinInterval = 2.2f;
	constexpr float BlinkMaxInterval = 4.8f;

	static USkeletalMesh* LoadMesh()
	{
		USkeletalMesh* Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath)).LoadSynchronous();
		static bool bWarnedMissing = false;
		if (!Mesh && !bWarnedMissing)
		{
			bWarnedMissing = true;
			UE_LOG(LogTemp, Warning, TEXT("[FunnyOfficeworker] 얼굴 판 메시 없음 %s — Tools/EmployeeFace bake_face_sheet() 실행 필요(얼굴 슬롯 숨김)"), MeshPath);
		}
		return Mesh;
	}

	static bool IsFaceSlot(EFunnyPartSlot PartSlot) { return PartSlot == EFunnyPartSlot::Eyebrow; }
}

AFunnyOfficeworker::AFunnyOfficeworker()
{
	// === CDO 프리뷰용: 생성자에서 셋업 → BP_FunnyOfficeworker 뷰포트에서 보고 드래그 튜닝 가능 ===
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMeshFinder(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001"));
	if (BodyMeshFinder.Succeeded() && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(BodyMeshFinder.Object);
	}

	// ★ Task 2 실측 확정(2026-08-25): Funny 팩은 UE 표준 스케일로 임포트돼 보정이 필요 없다(기본 1.0).
	// 스틱의 0.09 는 구 스틱 메시가 FBX 원본 단위로 들어와서 눌렀던 값이며 Funny 엔 해당 없음 — 넣으면 10배 작아진다.
	// 회전/Z오프셋도 손대지 않는다: 부모 AOfficeworker 가 이미 SetRelativeLocationAndRotation(FVector(0,0,-92.04), FRotator(0,-90,0)) 을 걸었고
	// BP 에 Funny 바디를 꽂아 대조한 결과 방향·발높이·키가 스틱과 일치했다.

	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPFinder(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/ABP_FunnyWorker"));
	if (AnimBPFinder.Succeeded() && GetMesh())
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(AnimBPFinder.Class);
	}

	// 캐릭터 전용 2톤 툰(Unlit, 고정 광원) — 파라미터 계약 TintColor/Metallic/Emissive 는 팩 머티리얼과 동일.
	// 툰 에셋이 아직 없으면 팩 머티리얼로 폴백(도구 CGR.Tools.FunnyToonMaterialBuild 가 생성).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FunnyToonFinder(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_FunnyToon"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FunnyMatFinder(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_Material"));
	FunnyBaseMat = FunnyToonFinder.Succeeded() ? FunnyToonFinder.Object : (FunnyMatFinder.Succeeded() ? FunnyMatFinder.Object : nullptr);
	// 의상 존 MIC — 없으면 존 없이 단색으로 떨어진다(도구 CGR.Tools.FunnyToonMaterialBuild 가 생성).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FunnyGarmentFinder(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/MI_FunnyGarment"));
	FunnyGarmentMat = FunnyGarmentFinder.Succeeded() ? FunnyGarmentFinder.Object.Get() : FunnyBaseMat;

	// 코스메틱 슬롯 — 메시는 런타임 DT 픽이라 생성자에서 지정하지 않는다.
	// GetMesh() 에 소켓 없이 부착(identity → 바디 월드 상속) + PostInitializeComponents 에서 LeaderPose.
	auto MakeSlot = [this](const TCHAR* Name) -> USkeletalMeshComponent*
	{
		USkeletalMeshComponent* C = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		C->SetupAttachment(GetMesh());
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		return C;
	};

	OuterwearComponent = MakeSlot(TEXT("Outerwear"));
	PantsComponent     = MakeSlot(TEXT("Pants"));
	ShoeComponent      = MakeSlot(TEXT("Shoe"));
	EyebrowComponent   = MakeSlot(TEXT("Eyebrow"));
	HairComponent      = MakeSlot(TEXT("CosmeticHair"));  // 부모 Hair* 예약명 회피
	GlassesComponent   = MakeSlot(TEXT("Glasses"));
	MustacheComponent  = MakeSlot(TEXT("Mustache"));
	GloveComponent     = MakeSlot(TEXT("Glove"));
	HeadwearComponent  = MakeSlot(TEXT("Headwear"));
	BackComponent      = MakeSlot(TEXT("Back"));
	HeadwearComponent->SetVisibility(false);
	BackComponent->SetVisibility(false);
	GlassesComponent->SetVisibility(false);

	ReactionComponent = CreateDefaultSubobject<UEmployeeReactionComponent>(TEXT("EmployeeReaction"));

	// 초상화 캡쳐 카메라를 캡슐(루트)로 재부착 — 부모는 Face(=GetMesh())에 붙이지만 스틱 Face는 ~0.09배
	// 스케일이라 카메라 상대 오프셋이 11× 축소돼 버린다. 루트 기준이면 BP 뷰포트 튜닝이 액터 공간(언스케일).
	if (FaceCaptureCamera && GetCapsuleComponent())
	{
		FaceCaptureCamera->SetupAttachment(GetCapsuleComponent());
	}

	// 초상화 기본 포즈 = 걷기 첫 프레임 (레퍼런스 T포즈보다 자연스러운 스탠스). BP 에서 교체 가능.
	PortraitPoseAnim = TSoftObjectPtr<UAnimSequence>(
		FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/AS_Walk.AS_Walk")));
}

void AFunnyOfficeworker::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body) { return; }

	// BP 가 옛 컴포넌트 레이아웃으로 직렬화돼 있으면 stale None 이 생성자의 멤버 할당을 덮어써
	// 포인터만 null 이 되고(컴포넌트 객체는 존재) LeaderPose 가 안 걸린다(2026-06-12). 이름으로 재연결해 자가복구.
	if (!OuterwearComponent || !PantsComponent || !ShoeComponent || !EyebrowComponent
		|| !HairComponent || !GlassesComponent || !MustacheComponent || !GloveComponent
		|| !HeadwearComponent || !BackComponent)
	{
		TArray<USkeletalMeshComponent*> SkelComps;
		GetComponents(SkelComps);
		for (USkeletalMeshComponent* C : SkelComps)
		{
			const FString CN = C->GetName();
			if (CN == TEXT("Outerwear")) { OuterwearComponent = C; }
			else if (CN == TEXT("Pants")) { PantsComponent = C; }
			else if (CN == TEXT("Shoe")) { ShoeComponent = C; }
			else if (CN == TEXT("Eyebrow")) { EyebrowComponent = C; }
			else if (CN == TEXT("CosmeticHair")) { HairComponent = C; }
			else if (CN == TEXT("Glasses")) { GlassesComponent = C; }
			else if (CN == TEXT("Mustache")) { MustacheComponent = C; }
			else if (CN == TEXT("Glove")) { GloveComponent = C; }
			else if (CN == TEXT("Headwear")) { HeadwearComponent = C; }
			else if (CN == TEXT("Back")) { BackComponent = C; }
		}
		UE_LOG(LogTemp, Warning, TEXT("[FunnyOfficeworker] 코스메틱 멤버 포인터가 null 이라 이름으로 재연결함 — BP_FunnyOfficeworker 재컴파일+저장 권장(stale 컴포넌트 직렬화)."));
	}

	// 리지드 스킨 코스메틱 → 바디 메시 포즈 추종 (파츠가 Funny 스켈레톤을 공유).
	// PostRegisterAllComponents 로 옮기지 말 것 — 재등록마다 여러 번 호출돼 바인딩이 꼬인다(2026-06-11 회귀).
	for (EFunnyPartSlot PartSlot : { EFunnyPartSlot::Outerwear, EFunnyPartSlot::Pants, EFunnyPartSlot::Shoe,
	                                 EFunnyPartSlot::Hair, EFunnyPartSlot::Eyebrow, EFunnyPartSlot::Glasses,
	                                 EFunnyPartSlot::Mustache, EFunnyPartSlot::Glove, EFunnyPartSlot::Headwear, EFunnyPartSlot::Back })
	{
		if (USkeletalMeshComponent* Comp = GetSlotComponent(PartSlot))
		{
			Comp->SetLeaderPoseComponent(Body);
			Comp->bUseBoundsFromLeaderPoseComponent = true;
			Comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		}
	}
}

USkeletalMeshComponent* AFunnyOfficeworker::GetSlotComponent(EFunnyPartSlot PartSlot) const
{
	switch (PartSlot)
	{
	case EFunnyPartSlot::Body:      return GetMesh();
	case EFunnyPartSlot::Outerwear: return OuterwearComponent;
	case EFunnyPartSlot::Pants:     return PantsComponent;
	case EFunnyPartSlot::Shoe:      return ShoeComponent;
	case EFunnyPartSlot::Hair:      return HairComponent;
	case EFunnyPartSlot::Eyebrow:   return EyebrowComponent;
	case EFunnyPartSlot::Glasses:   return GlassesComponent;
	case EFunnyPartSlot::Mustache:  return MustacheComponent;
	case EFunnyPartSlot::Glove:     return GloveComponent;
	case EFunnyPartSlot::Headwear:  return HeadwearComponent;
	case EFunnyPartSlot::Back:      return BackComponent;
	default:                        return nullptr;
	}
}

void AFunnyOfficeworker::BeginPlay()
{
	Super::BeginPlay();

	// 외형 적용(ApplyAppearance)이 먼저 돌았으면 거기서 이미 걸었다 — 비어 있는 슬롯만 채운다.
	for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(EFunnyPartSlot::Max); ++SlotIdx)
	{
		const EFunnyPartSlot PartSlot = static_cast<EFunnyPartSlot>(SlotIdx);
		USkeletalMeshComponent* C = GetSlotComponent(PartSlot);
		if (C && !C->GetOverlayMaterial() && !FunnyFaceSheet::IsFaceSlot(PartSlot))
		{
			C->SetOverlayMaterial(LoadHullOutlineMaterial(PartSlot));
		}
	}

#if WITH_EDITOR
	// 에디터 빌드는 덜 찬 셰이더를 처음 그릴 때에야 컴파일한다 — 빈 사무실의 첫 증명사진이 그 시간(약 2초)을 기다리지 않게 입장 때 미리 넣는다.
	if (bIsPortraitMode && GetWorld())
	{
		UMaterialInterface* const RigMaterials[] = { FunnyBaseMat, GetBodyMaterial(), GetFaceSheetMaterial(), FunnyGarmentMat,
			LoadHullOutlineMaterial(), LoadHullOutlineMaterial(EFunnyPartSlot::Glasses),
			LoadHullOutlineMaterial(EFunnyPartSlot::Hair) };
		for (UMaterialInterface* RigMaterial : RigMaterials)
		{
			FMaterialResource* Resource = RigMaterial ? RigMaterial->GetMaterialResource(GetWorld()->GetFeatureLevel()) : nullptr;
			if (Resource && !Resource->IsGameThreadShaderMapComplete())
			{
				Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);
			}
		}
	}
#endif

	// 자동화 테스트가 스폰하는 월드엔 GameInstance 가 없다 — EndPlay 와 같은 가드가 여기도 필요하다
	if (!GetGameInstance()) { return; }
	if (UEmployeeEquipmentManagerSubsystem* Equipment = GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>())
	{
		Equipment->OnEquipmentChanged.AddUObject(this, &AFunnyOfficeworker::HandleEquipmentChanged);
	}
}

void AFunnyOfficeworker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetGameInstance())
	{
		if (UEmployeeEquipmentManagerSubsystem* Equipment = GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>())
		{
			Equipment->OnEquipmentChanged.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FString AFunnyOfficeworker::GetEquipmentAppearanceKey() const
{
	const UEmployeeEquipmentManagerSubsystem* Equipment = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	return Equipment && Tables ? FEmployeeEquipmentAppearanceResolver::BuildKey(
		Equipment->GetState(), Tables->GetEmployeeEquipmentDefinitions(), Tables->GetEmployeeEquipmentAppearances(), GetEmployeeID()) : FString();
}

void AFunnyOfficeworker::HandleEquipmentChanged()
{
	// 촬영 중인 정지 리그는 요청 스냅샷을 유지한다. 새 초상화는 매니저의 큐에서 촬영한다.
	if (!bHasCosmeticContext || bIsPortraitMode || AppliedEquipmentKey == GetEquipmentAppearanceKey()) { return; }
	ApplyWorkerCosmetics(AppliedDepartment, AppliedRank, AppliedTier, GetEmployeeID(), AppliedGender, bAppliedGachaReveal);
}

bool AFunnyOfficeworker::PreviewEquipmentAppearance(EEmployeeEquipmentSlot EquipmentSlot,
	EEquipmentAppearanceMode Mode, FName AppearanceId)
{
	const UTableManagerSubsystem* Tables = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!bHasCosmeticContext || !Tables) { return false; }
	if (Mode == EEquipmentAppearanceMode::Override)
	{
		const FEmployeeEquipmentAppearanceRow* Appearance = Tables->FindEmployeeEquipmentAppearance(AppearanceId);
		if (!Appearance || Appearance->EquipmentSlot != EquipmentSlot) { return false; }
	}
	if (Mode == EEquipmentAppearanceMode::Hidden && EquipmentSlot != EEmployeeEquipmentSlot::Head
		&& EquipmentSlot != EEmployeeEquipmentSlot::Eyewear && EquipmentSlot != EEmployeeEquipmentSlot::Back) { return false; }
	bEquipmentPreview = true;
	PreviewEquipmentSlot = EquipmentSlot;
	PreviewEquipmentMode = Mode;
	PreviewAppearanceId = AppearanceId;
	ApplyWorkerCosmetics(AppliedDepartment, AppliedRank, AppliedTier, GetEmployeeID(), AppliedGender, bAppliedGachaReveal);
	return true;
}

void AFunnyOfficeworker::ClearEquipmentAppearancePreview()
{
	if (!bEquipmentPreview) { return; }
	bEquipmentPreview = false;
	if (bHasCosmeticContext)
	{
		ApplyWorkerCosmetics(AppliedDepartment, AppliedRank, AppliedTier, GetEmployeeID(), AppliedGender, bAppliedGachaReveal);
	}
}

void AFunnyOfficeworker::ApplyEquipmentAppearance()
{
	if (HeadwearComponent) { HeadwearComponent->SetVisibility(false); }
	if (BackComponent) { BackComponent->SetVisibility(false); }
	bBackAppearanceVisible = false;
	AppliedEquipmentKey = GetEquipmentAppearanceKey();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables || !GetMesh() || !GetMesh()->GetSkeletalMeshAsset()) { return; }
	TArray<FEquipmentAppearanceInput> Inputs;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		FEquipmentAppearanceInput& Input = Inputs.AddDefaulted_GetRef();
		Input.EquipmentSlot = static_cast<EEmployeeEquipmentSlot>(Index);
		const FEquipmentAppearanceSelection Selection = Equipment->GetAppearanceSelection(GetEmployeeID(), Input.EquipmentSlot);
		Input.Mode = Selection.Mode;
		Input.Override = Tables->FindEmployeeEquipmentAppearance(Selection.AppearanceId);
		Input.bOverrideUnlocked = Equipment->GetState().UnlockedAppearanceIds.Contains(Selection.AppearanceId);
		if (const FEmployeeEquipmentInstance* Instance = Equipment->GetEquippedInstance(GetEmployeeID(), Input.EquipmentSlot))
		{
			if (const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Instance->DefinitionId))
			{
				Input.Equipped = Tables->FindEmployeeEquipmentAppearance(Definition->AppearanceId);
			}
		}
		if (bEquipmentPreview && Input.EquipmentSlot == PreviewEquipmentSlot)
		{
			Input.Mode = PreviewEquipmentMode;
			Input.Override = Tables->FindEmployeeEquipmentAppearance(PreviewAppearanceId);
			Input.bOverrideUnlocked = true;
		}
	}
	FResolvedEquipmentAppearance Appearance;
	FString Error;
	if (!FEmployeeEquipmentAppearanceResolver::Resolve(Inputs, Appearance, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipmentAppearance] Employee %d: %s"), GetEmployeeID(), *Error);
		return;
	}
	// 모든 메시를 먼저 검증해 의상의 일부만 바뀌는 결과를 방지한다.
	TArray<USkeletalMesh*> Meshes;
	TArray<UMaterialInterface*> Materials;
	TArray<const FEmployeeEquipmentGradeRow*> VisualStyles;
	for (const FEquipmentPartOverride& Part : Appearance.Parts)
	{
		const FFunnyPartPoolTable* PartRow = Tables->FindFunnyPartRow(Part.PartRowId);
		USkeletalMesh* PartMesh = PartRow && PartRow->PartSlot == Part.PartSlot ? PartRow->Mesh.LoadSynchronous() : nullptr;
		if (!PartMesh || !GetSlotComponent(Part.PartSlot) || PartMesh->GetSkeleton() != GetMesh()->GetSkeletalMeshAsset()->GetSkeleton())
		{
			UE_LOG(LogTemp, Warning, TEXT("[EquipmentAppearance] Invalid mesh/skeleton: %s"), *Part.PartRowId.ToString());
			return;
		}
		Meshes.Add(PartMesh);
		const EEmployeeEquipmentGrade* VisualGrade = Appearance.VisualGrades.Find(Part.PartSlot);
		const FEmployeeEquipmentGradeRow* Style = VisualGrade ? Tables->FindEmployeeEquipmentGrade(*VisualGrade) : nullptr;
		UMaterialInterface* Material = Style && !Style->MeshMaterial.IsNull() ? Style->MeshMaterial.LoadSynchronous() : FunnyBaseMat;
		if (!Style || !Material)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EquipmentAppearance] Invalid visual style/material: %s"), *Part.PartRowId.ToString());
			return;
		}
		Materials.Add(Material);
		VisualStyles.Add(Style);
	}
	for (int32 Index = 0; Index < Appearance.Parts.Num(); ++Index)
	{
		const FEquipmentPartOverride& Part = Appearance.Parts[Index];
		USkeletalMeshComponent* Comp = GetSlotComponent(Part.PartSlot);
		Comp->SetSkeletalMeshAsset(Meshes[Index]);
		Comp->SetLeaderPoseComponent(GetMesh());
		Comp->bUseBoundsFromLeaderPoseComponent = true;
		Comp->SetVisibility(true);
		const int32 MaterialIndex = static_cast<int32>(Part.PartSlot);
		if (!SlotMIDs.IsValidIndex(MaterialIndex)) { SlotMIDs.SetNumZeroed(static_cast<int32>(EFunnyPartSlot::Max)); }
		if (!SlotMIDs[MaterialIndex] || SlotMIDs[MaterialIndex]->Parent != Materials[Index])
		{
			SlotMIDs[MaterialIndex] = Comp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Materials[Index]);
		}
		if (UMaterialInstanceDynamic* MID = SlotMIDs[MaterialIndex])
		{
			Comp->SetMaterial(0, MID);
			MID->SetVectorParameterValue(TEXT("TintColor"), Part.bOverrideTint ? Part.Tint : FLinearColor::White);
			MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			MID->SetScalarParameterValue(TEXT("Emissive"), 0.f);
			MID->SetScalarParameterValue(TEXT("TexBlend"), 1.f);
			MID->SetScalarParameterValue(TEXT("TexGain"), 1.f);
			FEmployeeEquipmentVisuals::ApplyParameters(MID, *VisualStyles[Index], VisualStyles[Index]->WorldStrength, !bIsPortraitMode);
		}
	}
	for (EFunnyPartSlot HiddenPart : Appearance.HiddenParts)
	{
		if (USkeletalMeshComponent* Comp = GetSlotComponent(HiddenPart)) { Comp->SetVisibility(false); }
	}
	bBackAppearanceVisible = BackComponent && BackComponent->IsVisible();
	UpdateBackVisibility();
	bWearingGlasses = GlassesComponent && GlassesComponent->IsVisible();
}

void AFunnyOfficeworker::UpdateBackVisibility()
{
	if (BackComponent)
	{
		const bool bShouldShowBack = bBackAppearanceVisible && (bIsPortraitMode || !IsSeated());
		if (BackComponent->IsVisible() != bShouldShowBack)
		{
			BackComponent->SetVisibility(bShouldShowBack);
		}
	}
}

void AFunnyOfficeworker::LoadAssetsAsync()
{
	// 단일 메시라 비동기 불필요 — 동기 로드 후 즉시 완료 통지 (부모 BeginPlay 에서 호출됨)
	OnAssetsLoaded();
}

void AFunnyOfficeworker::OnAssetsLoaded()
{
	// 파츠/틴트는 ApplyWorkerCosmetics(GameMode 스폰 후)가 권위 적용한다. 여기선 초기 표정만.
	SetFaceExpression(EWorkerFaceExpression::Neutral);
}

void AFunnyOfficeworker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 액터 전체(캡슐+메시+페이스플레인+안경) 균일 축소. CharacterMovement 가 매 틱 캡슐 바닥을 바닥에 재정렬하므로 발은 항상 지면.
	SetActorScale3D(FVector(BodyScale));

	// 에디터 프리뷰 정합: BP 뷰포트에서도 코스메틱이 몸에 붙어 보이게(스폰 시엔 ApplyWorkerCosmetics 가 권위 재적용).
	// 본 소켓 트랜스폼은 스켈레탈 메시 에셋만 있으면 rest 포즈로 유효하므로 OnConstruction 에서도 동작.
	FitAllCosmetics();

#if WITH_EDITOR
	// 에디터 뷰포트 프리뷰: 게임 월드(스폰)가 아닐 때만 실제 파이프라인을 돌려 파츠/틴트를 즉시 반영.
	// (런타임 스폰 월드에선 IsGameWorld()==true → 스킵, GameMode 가 실제 직원 데이터로 권위 적용.)
	if (bEditorPreview && GetWorld() && !GetWorld()->IsGameWorld())
	{
		ApplyWorkerCosmetics(PreviewDepartment, PreviewRank, PreviewTier, PreviewEmployeeID, GetCharacterGender());
		SetFaceExpression(PreviewExpression);
	}
#endif
}

void AFunnyOfficeworker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateBackVisibility();

	// 행동 상태에서 직접 표정을 뽑는다 — 변화 시에만 칸을 바꾼다(같은 표정이면 no-op).
	// 버블(1초 주기)이 아니라 상태를 읽으므로 졸음/폭주 같은 짧은 순간도 놓치지 않는다.
	const EWorkerFaceExpression Desired = GetEffectiveFaceExpression();
	if (Desired != LastFaceExpression)
	{
		SetFaceExpression(Desired);
	}
	TickFaceBlink();
}

EWorkerFaceExpression AFunnyOfficeworker::ResolveFaceExpression() const
{
	const UEmployeeBehaviorComponent* Behavior = BehaviorComponent;
	if (!Behavior) { return EWorkerFaceExpression::Neutral; }

	// 1) 피로 다운 구간이 최우선 — 플레이어가 손대야 하는 순간이라 얼굴이 가장 크게 말해야 한다.
	//    SeatedPoseIndex 1 = 딴짓 사유(진입 때 정한 자세가 그대로 남는다) — 조는 얼굴 대신 태평한 얼굴.
	switch (Behavior->GetFatigueSlackPhase())
	{
	case EFatigueSlackPhase::Telegraph:
		return Behavior->SeatedPoseIndex == 1 ? EWorkerFaceExpression::Happy : EWorkerFaceExpression::Drowsy;
	case EFatigueSlackPhase::Slumping:
		return Behavior->SeatedPoseIndex == 1 ? EWorkerFaceExpression::Happy : EWorkerFaceExpression::Asleep;
	case EFatigueSlackPhase::Bolting:
		return EWorkerFaceExpression::Sneaky;   // 자리 박차고 몰래 배회
	default:
		break;
	}

	// 춤(신입 환영·뽑기 무대)은 축하 순간이다.
	if (Behavior->EmployeeState == EEmployeeState::Dance) { return EWorkerFaceExpression::Happy; }

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

	// 개발 중 기본도 웃는 기본 얼굴 — 집중 얼굴은 몰입 단계 연출이 따로 쓴다.
	return EWorkerFaceExpression::Neutral;
}

EWorkerFaceExpression AFunnyOfficeworker::GetEffectiveFaceExpression() const
{
	if (DebugFaceOverride != EWorkerFaceExpression::Max) { return DebugFaceOverride; }
	const UWorld* World = GetWorld();
	if (FaceReactionExpression != EWorkerFaceExpression::Max && World && World->GetTimeSeconds() < FaceReactionEndTime)
	{
		return FaceReactionExpression;
	}
	return ResolveFaceExpression();
}

void AFunnyOfficeworker::PlayFaceReaction(EWorkerFaceExpression Expression, float Duration)
{
	const UWorld* World = GetWorld();
	if (!World || Expression == EWorkerFaceExpression::Max || Duration <= 0.f) { return; }
	FaceReactionExpression = Expression;
	FaceReactionEndTime = World->GetTimeSeconds() + Duration;
	SetFaceExpression(GetEffectiveFaceExpression());
}

void AFunnyOfficeworker::SetDebugFaceOverride(EWorkerFaceExpression Expression)
{
	DebugFaceOverride = Expression;
	SetFaceExpression(GetEffectiveFaceExpression());
}

bool AFunnyOfficeworker::IsDozeVisualActive() const
{
	return ReactionComponent && ReactionComponent->IsDozeVisualActive();
}

void AFunnyOfficeworker::SetFaceExpression(EWorkerFaceExpression Expression)
{
	UMaterialInstanceDynamic* FaceMID = GetFaceMID();
	if (!FaceMID) { return; }   // 얼굴 판이 아직 안 걸렸다 — ApplyFaceSheet 뒤 Tick 이 다시 맞춘다

	LastFaceExpression = Expression;
	if (bFaceBlinking)
	{
		bFaceBlinking = false;
		FaceMID->SetScalarParameterValue(TEXT("EyesCell"), static_cast<float>(FaceEyesCell));
	}

	const FFunnyExpressionTable* Row = FindFaceExpressionRow(Expression);
	if (!Row)
	{
		bFaceAllowsBlink = false;
		return;
	}
	FaceEyesCell = Row->EyesCell;
	bFaceAllowsBlink = Row->bAllowBlink;
	FaceMID->SetScalarParameterValue(TEXT("EyesCell"), static_cast<float>(Row->EyesCell));
	FaceMID->SetScalarParameterValue(TEXT("MouthCell"), static_cast<float>(Row->MouthCell));
	FaceMID->SetScalarParameterValue(TEXT("BlushOn"), Row->bBlush ? 1.f : 0.f);
}

const FFunnyExpressionTable* AFunnyOfficeworker::FindFaceExpressionRow(EWorkerFaceExpression Expression) const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UTableManagerSubsystem* Tables = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			return Tables->GetFunnyExpressionFace(Expression);
		}
	}

	// 에디터 프리뷰(게임 인스턴스 없음): DT 에셋 직접 로드
	const UDataTable* DT = TSoftObjectPtr<UDataTable>(FSoftObjectPath(FunnyFaceSheet::ExpressionTablePath)).LoadSynchronous();
	if (!DT) { return nullptr; }
	TArray<FFunnyExpressionTable*> Rows;
	DT->GetAllRows<FFunnyExpressionTable>(TEXT("FunnyFacePreview"), Rows);
	for (const FFunnyExpressionTable* Row : Rows)
	{
		if (Row && Row->Expression == Expression) { return Row; }
	}
	return nullptr;
}

UMaterialInterface* AFunnyOfficeworker::GetFaceSheetMaterial()
{
	if (!FunnyFaceBaseMat)
	{
		FunnyFaceBaseMat = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FunnyFaceSheet::MaterialPath)).LoadSynchronous();
		static bool bWarnedMissing = false;
		if (!FunnyFaceBaseMat && !bWarnedMissing)
		{
			bWarnedMissing = true;
			UE_LOG(LogTemp, Warning, TEXT("[FunnyOfficeworker] 얼굴 판 재질 없음 %s — Tools/EmployeeFace 재질 생성 필요(얼굴 슬롯 숨김)"),
				FunnyFaceSheet::MaterialPath);
		}
	}
	return FunnyFaceBaseMat;
}

UMaterialInterface* AFunnyOfficeworker::GetBodyMaterial()
{
	if (!FunnyBodyMat)
	{
		FunnyBodyMat = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FunnyBody::MaterialPath)).LoadSynchronous();
		static bool bWarnedMissing = false;
		if (!FunnyBodyMat && !bWarnedMissing)
		{
			bWarnedMissing = true;
			UE_LOG(LogTemp, Warning, TEXT("[FunnyOfficeworker] 몸 이너 셔츠 재질 없음 %s — 기본 몸 재질로 표시"), FunnyBody::MaterialPath);
		}
	}
	return FunnyBodyMat ? FunnyBodyMat : FunnyBaseMat;
}

UMaterialInterface* AFunnyOfficeworker::GetSlotMaterial(EFunnyPartSlot PartSlot)
{
	if (PartSlot == EFunnyPartSlot::Body) { return GetBodyMaterial(); }
	if (PartSlot == EFunnyPartSlot::Outerwear) { return FunnyGarmentMat; }
	return FunnyBaseMat;
}

UMaterialInstanceDynamic* AFunnyOfficeworker::GetFaceMID() const
{
	const int32 FaceIdx = static_cast<int32>(EFunnyPartSlot::Eyebrow);
	UMaterialInstanceDynamic* FaceMID = SlotMIDs.IsValidIndex(FaceIdx) ? SlotMIDs[FaceIdx] : nullptr;
	return (FaceMID && FunnyFaceBaseMat && FaceMID->Parent == FunnyFaceBaseMat) ? FaceMID : nullptr;
}

bool AFunnyOfficeworker::IsFaceSheetReady() const
{
	return bFaceSheetReady && EyebrowComponent && EyebrowComponent->IsVisible() && GetFaceMID();
}

bool AFunnyOfficeworker::ResolveFaceStyle(UGameInstance* Instance, int32 InEmployeeID, FFunnyFaceStyleTable& OutStyle)
{
	const TArray<FFunnyFaceStyleTable>* Styles = nullptr;
	TArray<FFunnyFaceStyleTable> PreviewStyles;
	if (Instance)
	{
		if (const UTableManagerSubsystem* Tables = Instance->GetSubsystem<UTableManagerSubsystem>())
		{
			Styles = &Tables->GetFunnyFaceStyles();
		}
	}
#if WITH_EDITOR
	if (!Instance)
	{
		const UDataTable* DataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(FunnyFaceSheet::StyleTablePath)).LoadSynchronous();
		if (UTableManagerSubsystem::BuildFunnyFaceStyles(DataTable, PreviewStyles)) { Styles = &PreviewStyles; }
	}
#endif
	const int32 Pick = Styles ? FFunnyPartPicker::PickFaceStyle(InEmployeeID, Styles->Num()) : INDEX_NONE;
	if (Styles && Styles->IsValidIndex(Pick))
	{
		OutStyle = (*Styles)[Pick];
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("[FaceStyle] No valid face style for employee %d"), InEmployeeID);
	return false;
}

void AFunnyOfficeworker::ApplyFaceSheet(int32 InEmployeeID)
{
	bFaceSheetReady = false;
	USkeletalMeshComponent* FaceComp = EyebrowComponent;
	if (!FaceComp) { return; }

	FaceComp->SetOverlayMaterial(nullptr);
	FFunnyFaceStyleTable FaceStyle;
	if (!ResolveFaceStyle(GetGameInstance(), InEmployeeID, FaceStyle))
	{
		FaceComp->SetVisibility(false);
		return;
	}
	UTexture2D* Eyes = FaceStyle.EyesAtlas.LoadSynchronous();
	UTexture2D* Mouth = FaceStyle.MouthAtlas.LoadSynchronous();
	if (!Eyes || !Mouth)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FaceStyle] Missing atlas for employee %d (style %d): %s / %s"), InEmployeeID,
			FaceStyle.StyleIndex, *FaceStyle.EyesAtlas.ToSoftObjectPath().ToString(), *FaceStyle.MouthAtlas.ToSoftObjectPath().ToString());
		FaceComp->SetVisibility(false);
		return;
	}
	USkeletalMesh* SheetMesh = FunnyFaceSheet::LoadMesh();
	UMaterialInterface* SheetMat = GetFaceSheetMaterial();
	if (!SheetMesh || !SheetMat)
	{
		FaceComp->SetVisibility(false);
		return;
	}

	if (FaceComp->GetSkeletalMeshAsset() != SheetMesh)
	{
		FaceComp->SetSkeletalMeshAsset(SheetMesh);
		FaceComp->SetLeaderPoseComponent(GetMesh());   // 메시 교체가 LeaderPose 바인딩을 날린다
		FaceComp->bUseBoundsFromLeaderPoseComponent = true;
	}

	const int32 FaceIdx = static_cast<int32>(EFunnyPartSlot::Eyebrow);
	if (!SlotMIDs.IsValidIndex(FaceIdx)) { SlotMIDs.SetNumZeroed(static_cast<int32>(EFunnyPartSlot::Max)); }
	if (!SlotMIDs[FaceIdx] || SlotMIDs[FaceIdx]->Parent != SheetMat)
	{
		SlotMIDs[FaceIdx] = FaceComp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, SheetMat);
	}
	UMaterialInstanceDynamic* FaceMID = SlotMIDs[FaceIdx];
	if (!FaceMID)
	{
		FaceComp->SetVisibility(false);
		return;
	}
	FaceComp->SetMaterial(0, FaceMID);
	FaceMID->SetTextureParameterValue(TEXT("EyesAtlas"), Eyes);
	FaceMID->SetTextureParameterValue(TEXT("MouthAtlas"), Mouth);
	FaceComp->SetVisibility(true);

	const FFunnyFaceVariation Variation = FFunnyPartPicker::PickFaceVariation(InEmployeeID);
	FaceMID->SetScalarParameterValue(TEXT("EyeScale"), Variation.EyeScale);
	FaceMID->SetScalarParameterValue(TEXT("EyeOffsetY"), Variation.EyeOffsetY);
	FaceMID->SetScalarParameterValue(TEXT("MouthOffsetY"), Variation.MouthOffsetY);

	FaceBlinkStream.Initialize(FFunnyPartPicker::FaceBlinkSeed(InEmployeeID));
	NextFaceBlinkTime = -1.0;
	bFaceBlinking = false;
	LastFaceExpression = EWorkerFaceExpression::Max;   // 새 MID 일 수 있어 다음 SetFaceExpression 이 칸을 다시 쓴다
	bFaceSheetReady = true;
}

void AFunnyOfficeworker::TickFaceBlink()
{
	UMaterialInstanceDynamic* FaceMID = GetFaceMID();
	const UWorld* World = GetWorld();
	if (!FaceMID || !World || bIsPortraitMode) { return; }   // 증명사진은 정지 이미지라 감은 눈이 찍히면 안 된다

	const double Now = World->GetTimeSeconds();
	if (bFaceBlinking)
	{
		if (Now < FaceBlinkEndTime) { return; }
		bFaceBlinking = false;
		FaceMID->SetScalarParameterValue(TEXT("EyesCell"), static_cast<float>(FaceEyesCell));
		NextFaceBlinkTime = Now + FaceBlinkStream.FRandRange(FunnyFaceSheet::BlinkMinInterval, FunnyFaceSheet::BlinkMaxInterval);
		return;
	}
	if (NextFaceBlinkTime < 0.0)
	{
		// 첫 간격도 시드로 흩어 입장 직후 전원이 같은 프레임에 감지 않게 한다.
		NextFaceBlinkTime = Now + FaceBlinkStream.FRandRange(0.f, FunnyFaceSheet::BlinkMaxInterval);
		return;
	}
	if (!bFaceAllowsBlink || Now < NextFaceBlinkTime) { return; }
	if (USettingsManagerSubsystem::IsReduceMotion(this))
	{
		NextFaceBlinkTime = Now + FunnyFaceSheet::BlinkMaxInterval;
		return;
	}
	bFaceBlinking = true;
	FaceBlinkEndTime = Now + FunnyFaceSheet::BlinkDuration;
	FaceMID->SetScalarParameterValue(TEXT("EyesCell"), static_cast<float>(FunnyFaceSheet::ClosedEyesCell));
}

FName AFunnyOfficeworker::MakeCosmeticRowName(EEmployeeDepartment Department, EEmployeeRank Rank, EGachaTier Tier)
{
	return FName(*FString::Printf(TEXT("D%d_R%d_T%d"),
		static_cast<int32>(Department), static_cast<int32>(Rank), static_cast<int32>(Tier)));
}

bool AFunnyOfficeworker::ResolveCosmeticRow(EEmployeeDepartment Department, EEmployeeRank Rank,
	EGachaTier Tier, FFunnyWorkerCosmeticTable& OutRow) const
{
	const FName RowName = MakeCosmeticRowName(Department, Rank, Tier);

	// 1) 런타임: TableManagerSubsystem 캐시 맵 (GameInstance 존재 시)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bFound = false;
			OutRow = TableMgr->GetFunnyWorkerCosmeticData(RowName, bFound);
			if (bFound) { return true; }
		}
	}

	// 2) 에디터 프리뷰(게임 인스턴스 없음): DT 에셋 직접 로드 → FindRow
	static const TCHAR* DTPath = TEXT("/Game/CompanyGrowth/Table/Employee/DT_FunnyWorkerCosmetic.DT_FunnyWorkerCosmetic");
	if (UDataTable* DT = TSoftObjectPtr<UDataTable>(FSoftObjectPath(DTPath)).LoadSynchronous())
	{
		if (const FFunnyWorkerCosmeticTable* Found = DT->FindRow<FFunnyWorkerCosmeticTable>(RowName, TEXT("StickEditorPreview"), false))
		{
			OutRow = *Found;
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[FunnyOfficeworker] 프리뷰 코스메틱 행 '%s' 없음 (DT_FunnyWorkerCosmetic)"), *RowName.ToString());
	}
	return false;
}

#if WITH_EDITOR
void AFunnyOfficeworker::PreviewNextEmployee()
{
	PreviewEmployeeID++;
	RerunConstructionScripts();  // OnConstruction 재실행 → 새 ID 의 개인 변주(스킨/헤어/옷 밝기) 즉시 반영
}
#endif

void AFunnyOfficeworker::FitAllCosmetics()
{
	// 코스메틱 6종(넥타이/클로그L·R/벨트/카라/헤어/안경) 전부 리지드 스킨(LeaderPose) → 수동 트랜스폼 불필요(no-op).
}

TSoftObjectPtr<USkeletalMesh> AFunnyOfficeworker::ResolveSlotMesh(const UTableManagerSubsystem* Tables,
	EFunnyPartSlot PartSlot, int32 InEmployeeID, EEmployeeGender InGender, bool* bOutHasSelection)
{
	const TArray<FFunnyPartPoolTable> Pool = Tables
		? Tables->GetFunnyPartPool(PartSlot, InGender) : TArray<FFunnyPartPoolTable>();
	TArray<int32> Weights;
	Weights.Reserve(Pool.Num());
	for (const FFunnyPartPoolTable& Row : Pool) { Weights.Add(Row.Weight); }
	const int32 Pick = FFunnyPartPicker::PickWeightedIndex(InEmployeeID, PartSlot, Weights);
	if (bOutHasSelection) *bOutHasSelection = Pool.IsValidIndex(Pick);
	return Pool.IsValidIndex(Pick) ? Pool[Pick].Mesh : TSoftObjectPtr<USkeletalMesh>();
}

void AFunnyOfficeworker::GatherAppearanceAssets(UGameInstance* Instance, int32 InEmployeeID,
	EEmployeeGender InGender, TArray<FSoftObjectPath>& OutPaths)
{
	const UTableManagerSubsystem* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Tables) return;
	const auto AddPath = [&OutPaths](const FSoftObjectPath& Path)
	{
		if (!Path.IsNull()) OutPaths.AddUnique(Path);
	};
	// 직급/표정 전환 때 쓰일 선택 파츠까지 준비하되 풀 전체를 메모리에 올리지는 않는다.
	for (int32 Index = 0; Index < static_cast<int32>(EFunnyPartSlot::Max); ++Index)
	{
		const EFunnyPartSlot PartSlot = static_cast<EFunnyPartSlot>(Index);
		if (PartSlot != EFunnyPartSlot::Body && !FunnyFaceSheet::IsFaceSlot(PartSlot))
			AddPath(ResolveSlotMesh(Tables, PartSlot, InEmployeeID, InGender).ToSoftObjectPath());
	}
	AddPath(FSoftObjectPath(FunnyBody::MaterialPath));
	// 표정은 칸만 바꾸므로 얼굴 판 메시·재질·이 직원 그림 한 벌이면 전 표정이 준비된다.
	AddPath(FSoftObjectPath(FunnyFaceSheet::MeshPath));
	AddPath(FSoftObjectPath(FunnyFaceSheet::MaterialPath));
	FFunnyFaceStyleTable FaceStyle;
	if (ResolveFaceStyle(Instance, InEmployeeID, FaceStyle))
	{
		AddPath(FaceStyle.EyesAtlas.ToSoftObjectPath());
		AddPath(FaceStyle.MouthAtlas.ToSoftObjectPath());
	}
	const UEmployeeEquipmentManagerSubsystem* Equipment = Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
	if (!Equipment) return;
	const auto AddAppearance = [Tables, &AddPath](const FEmployeeEquipmentAppearanceRow* Appearance)
	{
		if (!Appearance) return;
		for (const FEquipmentPartOverride& Part : Appearance->PartOverrides)
			if (const FFunnyPartPoolTable* PartRow = Tables->FindFunnyPartRow(Part.PartRowId))
				AddPath(PartRow->Mesh.ToSoftObjectPath());
		if (const FEmployeeEquipmentGradeRow* Grade = Tables->FindEmployeeEquipmentGrade(Appearance->VisualGrade))
			AddPath(Grade->MeshMaterial.ToSoftObjectPath());
	};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const EEmployeeEquipmentSlot EquipmentSlot = static_cast<EEmployeeEquipmentSlot>(Index);
		const FEquipmentAppearanceSelection Selection = Equipment->GetAppearanceSelection(InEmployeeID, EquipmentSlot);
		AddAppearance(Tables->FindEmployeeEquipmentAppearance(Selection.AppearanceId));
		if (const FEmployeeEquipmentInstance* Equipped = Equipment->GetEquippedInstance(InEmployeeID, EquipmentSlot))
			if (const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Equipped->DefinitionId))
				AddAppearance(Tables->FindEmployeeEquipmentAppearance(Definition->AppearanceId));
	}
}

bool AFunnyOfficeworker::AreAppearanceAssetsReady(const TArray<FSoftObjectPath>& Paths)
{
	for (const FSoftObjectPath& Path : Paths)
		if (const USkeletalMesh* Mesh = Cast<USkeletalMesh>(Path.ResolveObject()))
			if (Mesh->IsCompiling()) return false;
	return true;
}

// 슬롯 하나를 DT 풀에서 뽑아 메시+틴트를 적용한다. 풀이 비면 컴포넌트를 숨긴다.
void AFunnyOfficeworker::ApplySlot(EFunnyPartSlot PartSlot, int32 InEmployeeID,
	EEmployeeGender InGender, const FLinearColor& Tint, float Metal, float Emissive)
{
	USkeletalMeshComponent* Comp = GetSlotComponent(PartSlot);
	if (!Comp) { return; }

	UTableManagerSubsystem* Tables = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// DT 미등록/서브시스템 부재라도 바디는 생성자 메시로 계속 보여야 한다 — 파츠만 숨기고 빠진다.
	bool bHasSelection = false;
	const TSoftObjectPtr<USkeletalMesh> PartMesh = ResolveSlotMesh(Tables, PartSlot, InEmployeeID, InGender, &bHasSelection);
	if (!bHasSelection && PartSlot != EFunnyPartSlot::Body)
	{
		Comp->SetVisibility(false);
		return;
	}

	// 바디는 생성자 메시를 유지한다(스켈레톤/애니 소유자라 교체 시 LeaderPose 가 끊긴다).
	if (PartSlot != EFunnyPartSlot::Body)
	{
		if (USkeletalMesh* PickedMesh = PartMesh.LoadSynchronous())
		{
			if (Comp->GetSkeletalMeshAsset() != PickedMesh)
			{
				Comp->SetSkeletalMeshAsset(PickedMesh);
				Comp->SetLeaderPoseComponent(GetMesh());   // 메시 교체가 LeaderPose 바인딩을 날린다
				Comp->bUseBoundsFromLeaderPoseComponent = true;
			}
			Comp->SetVisibility(true);
		}
		else
		{
			Comp->SetVisibility(false);
			return;
		}
	}

	const int32 Idx = static_cast<int32>(PartSlot);
	if (!SlotMIDs.IsValidIndex(Idx)) { SlotMIDs.SetNumZeroed(static_cast<int32>(EFunnyPartSlot::Max)); }
	UMaterialInterface* ExpectedMat = GetSlotMaterial(PartSlot);
	if (ExpectedMat && (!SlotMIDs[Idx] || SlotMIDs[Idx]->Parent != ExpectedMat))
	{
		SlotMIDs[Idx] = Comp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ExpectedMat);
	}
	if (SlotMIDs[Idx])
	{
		Comp->SetMaterial(0, SlotMIDs[Idx]);
		// M_FunnyToon 파라미터 계약 (팩 M_Material 폴백 시 무시됨)
		SlotMIDs[Idx]->SetVectorParameterValue(TEXT("TintColor"), Tint);
		SlotMIDs[Idx]->SetScalarParameterValue(TEXT("Metallic"), Metal);
		SlotMIDs[Idx]->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		// 헤어/수염은 아틀라스 없이 틴트만 사용한다.
		SlotMIDs[Idx]->SetScalarParameterValue(TEXT("TexBlend"),
			(PartSlot == EFunnyPartSlot::Hair || PartSlot == EFunnyPartSlot::Mustache) ? 0.f : 1.f);
		// "White" 바디 팔레트 셀의 linear ≈0.185를 보정해 피부 틴트와 맞춘다.
		SlotMIDs[Idx]->SetScalarParameterValue(TEXT("TexGain"), PartSlot == EFunnyPartSlot::Body ? 5.4f : 1.f);
		if (PartSlot == EFunnyPartSlot::Body && Tables)
		{
			SlotMIDs[Idx]->SetVectorParameterValue(TEXT("InnerShirtColor"),
				Tables->PickWardrobeColor(EFunnyWardrobeCategory::Accent, InEmployeeID));
		}

		if (PartSlot == EFunnyPartSlot::Outerwear)
		{
			const FFunnyGarmentStyleTable* Style = Tables && Comp->GetSkeletalMeshAsset()
				? Tables->FindGarmentStyle(Comp->GetSkeletalMeshAsset()->GetFName()) : nullptr;
			// 스타일은 AllowedModes 안에서만 고른다 — 민소매에 라글란을 주면 소매가 없어 아무것도 안 보인다.
			int32 Mode = 0;
			if (Style)
			{
				TArray<int32> Allowed;
				for (int32 Bit = 0; Bit < static_cast<int32>(EFunnyGarmentMode::Max); ++Bit)
				{
					if (Style->AllowedModes & (1 << Bit)) { Allowed.Add(Bit); }
				}
				if (Allowed.Num() > 0)
				{
					Mode = Allowed[FFunnyPartPicker::Hash(InEmployeeID, 0x6C078965u) % static_cast<uint32>(Allowed.Num())];
				}
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentZoneMin"), Style->ZoneMin);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentZoneMax"), Style->ZoneMax);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentXMax"), Style->XMax);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentCollarTop"), Style->CollarTop);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentCollarNotch"), Style->CollarNotch);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentPlacketWidth"), Style->PlacketWidth);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentSleeveSplit"), Style->SleeveSplit);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentPipingWidth"), Style->PipingWidth);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentBandCount"), Style->BandCount);
				SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentCuffStart"), Style->CuffStart);
			}
			SlotMIDs[Idx]->SetScalarParameterValue(TEXT("GarmentMode"), static_cast<float>(Mode));
			SlotMIDs[Idx]->SetVectorParameterValue(TEXT("GarmentAccent"), Tables
				? Tables->PickWardrobeColor(EFunnyWardrobeCategory::Accent, InEmployeeID) : FLinearColor(0.93f, 0.94f, 0.96f));
		}
	}
}

void AFunnyOfficeworker::ApplyWorkerCosmetics(EEmployeeDepartment Department, EEmployeeRank Rank,
	EGachaTier Tier, int32 InEmployeeID, EEmployeeGender InGender, bool bIsGachaReveal)
{
	SetEmployeeID(InEmployeeID);
	bHasCosmeticContext = true;
	AppliedDepartment = Department;
	AppliedRank = Rank;
	AppliedTier = Tier;
	AppliedGender = InGender;
	bAppliedGachaReveal = bIsGachaReveal;
	ApplyOfficeLightingChannels(bIsGachaReveal);
	// 숨겨질 슬롯도 기본 부모로 복원한다. 빈 풀/직급 안경 게이트의 조기 반환 후 FX가 남지 않는다.
	for (int32 MaterialIndex = 0; MaterialIndex < SlotMIDs.Num(); ++MaterialIndex)
	{
		const EFunnyPartSlot PartSlot = static_cast<EFunnyPartSlot>(MaterialIndex);
		if (FunnyFaceSheet::IsFaceSlot(PartSlot)) { continue; }   // ApplyFaceSheet 가 얼굴 판 부모를 직접 맞춘다
		UMaterialInterface* ExpectedMat = GetSlotMaterial(PartSlot);
		if (ExpectedMat && SlotMIDs[MaterialIndex] && SlotMIDs[MaterialIndex]->Parent != ExpectedMat)
		{
			if (USkeletalMeshComponent* Comp = GetSlotComponent(PartSlot))
			{
				SlotMIDs[MaterialIndex] = Comp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ExpectedMat);
			}
		}
	}

	FFunnyWorkerCosmeticTable Row;
	ResolveCosmeticRow(Department, Rank, Tier, Row);

	// 스킨톤은 개인 특성이라 부서 DT 와 별개. TexGain 으로 바디 셀을 1 로 맞추므로 이 값이 곧 피부색(linear —
	// sRGB #FFD9BE/#F8C8A4/#E3A97F/#C4865A/#8D5A3B 를 변환한 값. sRGB 분수를 그대로 넣으면 물이 빠진다).
	// 밝은 두 톤을 2배 가중(7칸)해 어두운 계열이 전체를 지배하지 않게 한다.
	static const FLinearColor SkinPalette[] = {
		FLinearColor(1.00f, 0.69f, 0.51f), FLinearColor(1.00f, 0.69f, 0.51f),
		FLinearColor(0.94f, 0.58f, 0.37f), FLinearColor(0.94f, 0.58f, 0.37f),
		FLinearColor(0.77f, 0.40f, 0.21f), FLinearColor(0.55f, 0.24f, 0.10f),
		FLinearColor(0.27f, 0.10f, 0.045f),
	};
	static const FLinearColor HairPalette[] = {
		FLinearColor(0.08f, 0.07f, 0.07f), FLinearColor(0.26f, 0.17f, 0.11f),
		FLinearColor(0.45f, 0.31f, 0.18f), FLinearColor(0.72f, 0.60f, 0.38f),
		FLinearColor(0.55f, 0.55f, 0.58f),
	};

	const FLinearColor SkinTone =
		SkinPalette[FFunnyPartPicker::Hash(InEmployeeID, 0x85EBCA6Bu) % UE_ARRAY_COUNT(SkinPalette)];
	const FLinearColor HairColor =
		HairPalette[FFunnyPartPicker::Hash(InEmployeeID, 0xC2B2AE35u) % UE_ARRAY_COUNT(HairPalette)];
	// 평상복은 부서가 아니라 개인으로 뽑는다 — 부서 틴트를 쓰면 같은 부서가 전부 같은 옷을 입는다.
	// 구 명도 곱(0.88~1.12)은 색상·채도를 못 바꿔 "같은 색의 어두운 버전"밖에 못 만들었다.
	const UTableManagerSubsystem* WardrobeTables = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const FLinearColor TopColor = WardrobeTables
		? WardrobeTables->PickWardrobeColor(EFunnyWardrobeCategory::Top, InEmployeeID) : Row.OuterwearTint;
	const FLinearColor BottomColor = WardrobeTables
		? WardrobeTables->PickWardrobeColor(EFunnyWardrobeCategory::Bottom, InEmployeeID) : Row.PantsTint;

	// 등급 — 팩 바디는 슬롯이 1개라 손만 못 칠한다. 바디 전체를 골드로 물들인다.
	const FLinearColor BodyTint = FMath::Lerp(SkinTone, Row.BodyGoldTint, Row.GoldBlend);

	ApplySlot(EFunnyPartSlot::Body,      InEmployeeID, InGender, BodyTint,                     Row.GoldMetallic, Row.GoldEmissive);
	ApplySlot(EFunnyPartSlot::Outerwear, InEmployeeID, InGender, TopColor,    0.f, 0.f);
	ApplySlot(EFunnyPartSlot::Pants,     InEmployeeID, InGender, BottomColor, 0.f, 0.f);
	ApplySlot(EFunnyPartSlot::Shoe,      InEmployeeID, InGender, Row.ShoeTint,                 0.f, 0.f);
	ApplySlot(EFunnyPartSlot::Hair,      InEmployeeID, InGender, HairColor,                    0.f, 0.f);
	ApplyFaceSheet(InEmployeeID);
	// 수염 — 남성만, 개인 시드 35%. 색은 헤어색. 여성/미당첨은 숨긴다.
	const bool bMustache = InGender == EEmployeeGender::Male
		&& (FFunnyPartPicker::Hash(InEmployeeID, 0x3C6EF372u) % 100u) < 35u;
	if (bMustache) { ApplySlot(EFunnyPartSlot::Mustache, InEmployeeID, InGender, HairColor, 0.f, 0.f); }
	else if (MustacheComponent) { MustacheComponent->SetVisibility(false); }
	// 장갑 — 풀 전량 Weight 0(스킨 예약)이라 지금은 항상 숨김. 행을 켜면 자동으로 뽑힌다.
	ApplySlot(EFunnyPartSlot::Glove, InEmployeeID, InGender, FLinearColor::White, 0.f, 0.f);

	// 안경 — 등급 무관 랜덤이 아니라 직급 임계값(DT). 기존 GlassesChance 게이트는 폐지.
	bWearingGlasses = Row.bWearGlasses;
	if (bWearingGlasses)
	{
		ApplySlot(EFunnyPartSlot::Glasses, InEmployeeID, InGender, FLinearColor::White, 0.f, 0.f);
	}
	else if (GlassesComponent)
	{
		GlassesComponent->SetVisibility(false);
	}

	ApplyEquipmentAppearance();
	SetFaceExpression(GetEffectiveFaceExpression());

	// 몸 희귀도 Overlay 는 Premium 가챠 공개 연출에서만. 선택 중이면 선택 글로우가 우선한다.
	// 희귀도 림은 전 슬롯에 적용하고, 일반 외곽선은 파츠별 재질을 사용한다.
	if (!IsSelected())
	{
		UMaterialInterface* Rim = RimOverlayMaterial ? RimOverlayMaterial : SelectionOverlayMaterial;
		const bool bShowRarityOverlay = ShouldShowRarityOverlay(Tier, bIsGachaReveal);
		for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(EFunnyPartSlot::Max); ++SlotIdx)
		{
			const EFunnyPartSlot PartSlot = static_cast<EFunnyPartSlot>(SlotIdx);
			if (FunnyFaceSheet::IsFaceSlot(PartSlot)) { continue; }
			if (USkeletalMeshComponent* C = GetSlotComponent(PartSlot))
			{
				C->SetOverlayMaterial((bShowRarityOverlay && Rim) ? Rim : LoadHullOutlineMaterial(PartSlot));
			}
		}
	}
}

bool AFunnyOfficeworker::ShouldShowRarityOverlay(EGachaTier Tier, bool bIsGachaReveal)
{
	return bIsGachaReveal && Tier == EGachaTier::Premium;
}

EGachaTier AFunnyOfficeworker::ResolveGachaTier(const FEmployeeInstance& Employee)
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

void AFunnyOfficeworker::ApplyCosmeticsForPortrait(EEmployeeDepartment Department, EEmployeeRank Rank,
	EGachaTier Tier, int32 InEmployeeID, EEmployeeGender InGender)
{
	ApplyWorkerCosmetics(Department, Rank, Tier, InEmployeeID, InGender);

	// 캡쳐는 정적 헤드샷 — 상태 시뮬레이션이 없으므로 표정을 중립으로 고정(결정적).
	SetFaceExpression(EWorkerFaceExpression::Neutral);
	// PNG는 정지 이미지이므로 장비 광택의 위상도 같은 프레임으로 고정한다.
	for (UMaterialInstanceDynamic* MID : SlotMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(TEXT("EquipmentFXAnimate"), 0.f);
			MID->SetScalarParameterValue(TEXT("EquipmentFXPhase"), 0.f);
		}
	}

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

void AFunnyOfficeworker::SetupFaceCapture()
{
	Super::SetupFaceCapture();
	if (!bIsPortraitMode || !FaceCaptureCamera || !FaceCaptureCamera->TextureTarget)
	{
		return;
	}

	// 수동/자동 프레이밍이 끝난 카메라 기준으로 툰 광원을 고정해 맵 방향에 영향을 받지 않는다.
	const FVector LightDirection = FaceCaptureCamera->GetComponentQuat().RotateVector(
		PortraitLightDirection.GetSafeNormal(SMALL_NUMBER, FVector(-1.f, 0.f, 0.f)));
	for (UMaterialInstanceDynamic* MID : SlotMIDs)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("LightDir"), FLinearColor(LightDirection.X, LightDirection.Y, LightDirection.Z, 0.f));
			MID->SetVectorParameterValue(TEXT("ShadowTint"), PortraitShadowTint);
			MID->SetScalarParameterValue(TEXT("ShadowThreshold"), PortraitShadowThreshold);
			MID->SetScalarParameterValue(TEXT("ShadowSoftness"), FMath::Max(PortraitShadowSoftness, 0.01f));
			MID->SetScalarParameterValue(TEXT("Brightness"), PortraitBrightness);
		}
	}
}

void AFunnyOfficeworker::PlayWalkLoopForCapture()
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
