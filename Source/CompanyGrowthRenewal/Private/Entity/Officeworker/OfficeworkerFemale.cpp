// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Officeworker/OfficeworkerFemale.h"
#include "Manager/TableManagerSubsystem.h"

AOfficeworkerFemale::AOfficeworkerFemale() : Super()
{
}

void AOfficeworkerFemale::PostInitializeComponents()
{
    Super::PostInitializeComponents();
}

void AOfficeworkerFemale::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
}

void AOfficeworkerFemale::LoadAssetsAsync()
{
    // ABP 전환 후 비동기 로드할 에셋이 없으므로 바로 OnAssetsLoaded 호출
    OnAssetsLoaded();
}

void AOfficeworkerFemale::OnAssetsLoaded()
{
    //.ToSoftObjectPath().IsValid() 로 경로가 유효한 에셋만 체크해 Load

    // 1. Face 메시 설정
    UE_LOG(LogTemp, Warning, TEXT("=== OnAssetsLoaded ==="));

    if (BaseFaceMesh.ToSoftObjectPath().IsValid())
    {
        USkeletalMesh* LoadedMesh = BaseFaceMesh.LoadSynchronous();
        if (LoadedMesh)
        {
            Face->SetSkeletalMesh(LoadedMesh);
        }
    }


    // 애니메이션 설정 (ABP 사용)
    if (!AnimBlueprintClass.IsNull())
    {
        UClass* ABPClass = AnimBlueprintClass.LoadSynchronous();
        if (ABPClass)
        {
            Face->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            Face->SetAnimInstanceClass(ABPClass);
            UE_LOG(LogTemp, Log, TEXT("[OfficeworkerFemale] Using Animation Blueprint: %s"), *ABPClass->GetName());
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
    HairBack->SetRelativeLocationAndRotation(FVector(0.f, -8.676102f, 3.653825f), FRotator(0.f, 0.f, -90.0f));
    HairFringe->SetRelativeLocationAndRotation(FVector(0.f, -8.676102f, 3.653825f), FRotator(0.f, 0.f, -90.0f));
    HairSides->SetRelativeLocationAndRotation(FVector(0.f, -8.676102f, 2.405820f), FRotator(0.f, 0.f, -90.0f));
}

void AOfficeworkerFemale::SetHairCombination(int32 CombinationType, int32 RandomSeed)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SetHairCombination Called: Type=%d, Seed=%d ==="), CombinationType, RandomSeed);
    UTableManagerSubsystem* TableManager =
        GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableManager) return;

    // 재현 가능한 랜덤 파트 선택
    FRandomStream HairRandom(RandomSeed);

    switch (CombinationType)
    {
    case 1: // 단발 - hair_base02만
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Female);
        break;

    case 2: // 단발 + 앞머리 - hair_base02 + fringe
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Fringe, { "hair_fringe01", "hair_fringe02", "hair_fringe04" }, HairRandom, EEmployeeGender::Female);
        break;

    case 3: // 포니테일 - ponytail
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Back, { "hair_ponytail_long01", "hair_ponytail01", "hair_ponytail02" }, HairRandom, EEmployeeGender::Female);
        break;

    case 4: // 포니테일 + 옆머리
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Back, { "hair_ponytail_long01", "hair_ponytail01", "hair_ponytail02" }, HairRandom, EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Sides, { "hair_sides01", "hair_sides02", "hair_sides03", "hair_sides04",
"hair_sides05" }, HairRandom, EEmployeeGender::Female);
        break;
    case 5: // 풀 웨이브 - base + sides + back
        SetRandomHairPart(EHairPartType::Base, { "hair_base01", "hair_base02" }, HairRandom, EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Back, { "hair_back01", "hair_back02", "hair_back03", "hair_back04" }, HairRandom, EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Sides, { "hair_sides01", "hair_sides03", "hair_sides04", "hair_sides05" }, HairRandom, EEmployeeGender::Female);
        break;

    case 6: // 풀 웨이브 + 앞머리
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Fringe, { "hair_fringe01", "hair_fringe02", "hair_fringe04" },
            HairRandom, EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Back, { "hair_back01", "hair_back02", "hair_back03", "hair_back04" }, HairRandom, EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Sides, { "hair_sides01", "hair_sides03", "hair_sides04", "hair_sides05" }, HairRandom, EEmployeeGender::Female);
        break;

    case 7: // 특정 스타일 - base + sides02 + back01
        SetRandomHairPart(EHairPartType::Base, { "hair_base01", "hair_base02" }, HairRandom, EEmployeeGender::Female);
        SetHairPart(EHairPartType::Back, "hair_back01", EEmployeeGender::Female);
        SetHairPart(EHairPartType::Sides, "hair_sides02", EEmployeeGender::Female);
        break;

    case 8: // 특정 스타일 + 앞머리
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Female);
        SetRandomHairPart(EHairPartType::Fringe, { "hair_fringe01", "hair_fringe02", "hair_fringe04" },
            HairRandom, EEmployeeGender::Female);
        SetHairPart(EHairPartType::Back, "hair_back01", EEmployeeGender::Female);
        SetHairPart(EHairPartType::Sides, "hair_sides02", EEmployeeGender::Female);
        break;

    default:
        // 기본값: 단발
        SetHairPart(EHairPartType::Base, "hair_base02", EEmployeeGender::Female);
        break;
    }
}
