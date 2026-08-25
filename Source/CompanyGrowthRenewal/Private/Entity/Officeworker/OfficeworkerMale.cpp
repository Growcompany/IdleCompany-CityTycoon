// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Officeworker/OfficeworkerMale.h"
#include "Manager/TableManagerSubsystem.h"


AOfficeworkerMale::AOfficeworkerMale() : Super()
{
}

void AOfficeworkerMale::PostInitializeComponents()
{
    Super::PostInitializeComponents();
}

void AOfficeworkerMale::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void AOfficeworkerMale::LoadAssetsAsync()
{
    // ABP 전환 후 비동기 로드할 에셋이 없으므로 바로 OnAssetsLoaded 호출
    OnAssetsLoaded();
}

void AOfficeworkerMale::OnAssetsLoaded()
{
    // Face 메시 설정
    if (BaseFaceMesh.ToSoftObjectPath().IsValid())
    {
        Face->SetSkeletalMesh(BaseFaceMesh.LoadSynchronous());
    }

    // 애니메이션 설정 (ABP 사용)
    if (!AnimBlueprintClass.IsNull())
    {
        UClass* ABPClass = AnimBlueprintClass.LoadSynchronous();
        if (ABPClass)
        {
            Face->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            Face->SetAnimInstanceClass(ABPClass);
            UE_LOG(LogTemp, Log, TEXT("[OfficeworkerMale] Using Animation Blueprint: %s"), *ABPClass->GetName());
        }
    }

    // Hair - SetCharacterAppearance()에서 SetHairCombination()을 통해 설정되므로 여기서는 설정하지 않음
    // GPU Skin Cache 충돌 방지를 위해 hair는 한 번만 설정되어야 함

    // Face
    if (DefaultMouthMesh.ToSoftObjectPath().IsValid()) Mouth->SetSkeletalMesh(DefaultMouthMesh.LoadSynchronous());
    // Eyebrows는 SetCharacterAppearance()에서 SetEyebrowsPart()를 통해 설정되므로 주석 처리
    if (DefaultEarsMesh.ToSoftObjectPath().IsValid()) Ears->SetSkeletalMesh(DefaultEarsMesh.LoadSynchronous());

    // Accessories - SetClothingByRank()에서 설정되므로 주석 처리
    if (DefaultBeltMesh.ToSoftObjectPath().IsValid()) Belt->SetSkeletalMesh(DefaultBeltMesh.LoadSynchronous());

    // Clothes - Top과 Shoes는 SetClothingByRank()에서 설정되므로 주석 처리
    if (DefaultTrousersMesh.ToSoftObjectPath().IsValid()) Trousers->SetSkeletalMesh(DefaultTrousersMesh.LoadSynchronous());
    if (DefaultHandsMesh.ToSoftObjectPath().IsValid()) Hands->SetSkeletalMesh(DefaultHandsMesh.LoadSynchronous());

    // Hair 파츠 소켓 위치 설정
    HairBack->AttachToComponent(Face, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        HairAttachSocketName);
    HairFringe->AttachToComponent(Face, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        HairAttachSocketName);
    HairSides->AttachToComponent(Face, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        HairAttachSocketName);

    // 각 파츠별 Transform 설정
    HairBack->SetRelativeLocationAndRotation(FVector(0.000194f, -5.37728f, 2.707909f), FRotator(0.f, 0.f, -90.0f));
    HairFringe->SetRelativeLocationAndRotation(FVector(-0.040815f, -5.964564f, 3.028383f), FRotator(0.f, 0.f, -90.0f));
    HairSides->SetRelativeLocationAndRotation(FVector(-0.040815f, -5.964564f, 3.028383f), FRotator(0.f, 0.f, -90.0f));
}

void AOfficeworkerMale::SetHairCombination(int32 CombinationType, int32 RandomSeed)
{
    UTableManagerSubsystem* TableManager =
        GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableManager) return;

    // 재현 가능한 랜덤 파트 선택
    FRandomStream HairRandom(RandomSeed);

    switch (CombinationType)
    {
    case 1: // SK_hair_base01만
        SetHairPart(EHairPartType::Base, "hair_base01", EEmployeeGender::Male);
        break;

    case 2: // SK_hair_base02만
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Male);
        break;

    case 3: // SK_hair_base02 + SK_hair_fringe01,02,04
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Male);
        SetRandomHairPart(EHairPartType::Fringe, { "hair_fringe01", "hair_fringe02", "hair_fringe04" }, HairRandom, EEmployeeGender::Male);
        break;

    case 4: // SK_hair_base02 + SK_hair_fringe01,02,04 + SK_hair_back02~03
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Male);
        SetRandomHairPart(EHairPartType::Fringe, { "hair_fringe01", "hair_fringe02", "hair_fringe04" }, HairRandom, EEmployeeGender::Male);
        SetRandomHairPart(EHairPartType::Back, { "hair_back02", "hair_back03" }, HairRandom, EEmployeeGender::Male);
        break;

    case 5: // SK_hair_base01 + SK_hair_back02~03
        SetHairPart(EHairPartType::Base, "hair_base01", EEmployeeGender::Male);
        ClearHairPart(EHairPartType::Fringe);
        SetRandomHairPart(EHairPartType::Back, { "hair_back02", "hair_back03" }, HairRandom, EEmployeeGender::Male);
        break;

    case 6: // SK_hair_base02 + SK_hair_back02~03
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Male);
        SetRandomHairPart(EHairPartType::Back, { "hair_back02", "hair_back03" }, HairRandom, EEmployeeGender::Male);
        break;

    case 7: // 풀 세트
        SetRandomHairPart(EHairPartType::Base, { "hair_base01", "hair_base02" }, HairRandom, EEmployeeGender::Male);
        SetRandomHairPart(EHairPartType::Fringe, { "hair_fringe01", "hair_fringe02", "hair_fringe04" }, HairRandom, EEmployeeGender::Male);
        SetHairPart(EHairPartType::Back, "hair_back03", EEmployeeGender::Male);
        SetRandomHairPart(EHairPartType::Sides, { "hair_side01", "hair_side03", "hair_side05" }, HairRandom, EEmployeeGender::Male);
        break;

    default:
        // 기본값: 1번과 동일
        SetHairPart(EHairPartType::Base, "hair_base01", EEmployeeGender::Male);
        break;
    }
}
