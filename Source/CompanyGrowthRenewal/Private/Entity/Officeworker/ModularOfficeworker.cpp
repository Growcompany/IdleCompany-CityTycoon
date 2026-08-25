// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Officeworker/ModularOfficeworker.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Manager/TableManagerSubsystem.h"
#include "TimerManager.h"
#include "Engine/World.h"

AModularOfficeworker::AModularOfficeworker()
{
    // Face = 캐릭터 기본 메시의 별칭. 모듈러 파츠들은 전부 Face 에 부착 + LeaderPose 추종.
    Face = GetMesh();

    // Hair Components
    HairBase = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairBase"));
    HairBase->SetupAttachment(Face);

    HairBack = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairBack"));
    HairBack->SetupAttachment(Face);

    HairFringe = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairFringe"));
    HairFringe->SetupAttachment(Face);

    HairSides = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairSides"));
    HairSides->SetupAttachment(Face);

    // Face Components
    Mouth = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mouth"));
    Mouth->SetupAttachment(Face);

    Ears = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Ears"));
    Ears->SetupAttachment(Face);

    Eyebrows = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Eyebrows"));
    Eyebrows->SetupAttachment(Face);

    // Accessory Components
    Accessory1 = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Accessory1"));
    Accessory1->SetupAttachment(Face);

    Accessory2 = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Accessory2"));
    Accessory2->SetupAttachment(Face);

    Cape = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Cape"));
    Cape->SetupAttachment(Face);

    Tail = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Tail"));
    Tail->SetupAttachment(Face);

    Belt = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Belt"));
    Belt->SetupAttachment(Face);

    // Other Components
    Top = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Top"));
    Top->SetupAttachment(Face);

    Trousers = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Trousers"));
    Trousers->SetupAttachment(Face);

    Hands = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hands"));
    Hands->SetupAttachment(Face);

    Shoes = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Shoes"));
    Shoes->SetupAttachment(Face);

    // 모바일 최적화: 화면 밖 직원은 pose 갱신 스킵 (LeaderPose follower 포함). 기본 메시는 부모가 처리.
    const auto SetMobileTick = [](USkeletalMeshComponent* Comp)
    {
        if (Comp) Comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    };
    SetMobileTick(HairBase); SetMobileTick(HairBack); SetMobileTick(HairFringe); SetMobileTick(HairSides);
    SetMobileTick(Mouth); SetMobileTick(Ears); SetMobileTick(Eyebrows);
    SetMobileTick(Accessory1); SetMobileTick(Accessory2);
    SetMobileTick(Cape); SetMobileTick(Tail); SetMobileTick(Belt);
    SetMobileTick(Top); SetMobileTick(Trousers); SetMobileTick(Hands); SetMobileTick(Shoes);
}

void AModularOfficeworker::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // TableManager에서 기본 메시 로딩
    LoadDefaultMeshesFromTable();

    // LeaderPose 설정
    HairBase->SetLeaderPoseComponent(Face);
    Mouth->SetLeaderPoseComponent(Face);
    Ears->SetLeaderPoseComponent(Face);
    Eyebrows->SetLeaderPoseComponent(Face);
    Accessory1->SetLeaderPoseComponent(Face);
    Accessory2->SetLeaderPoseComponent(Face);
    Cape->SetLeaderPoseComponent(Face);
    Tail->SetLeaderPoseComponent(Face);
    Belt->SetLeaderPoseComponent(Face);
    Top->SetLeaderPoseComponent(Face);
    Trousers->SetLeaderPoseComponent(Face);
    Hands->SetLeaderPoseComponent(Face);
    Shoes->SetLeaderPoseComponent(Face);
}

void AModularOfficeworker::LoadDefaultMeshesFromTable()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        UTableManagerSubsystem* TableManager = GI->GetSubsystem<UTableManagerSubsystem>();
        if (TableManager)
        {
            FCharacterBaseMeshTable* BaseMeshData = TableManager->GetCharacterBaseMesh(GetCharacterGender());
            if (BaseMeshData)
            {
                BaseFaceMesh = BaseMeshData->BaseFaceMesh;
                DefaultMouthMesh = BaseMeshData->MouthMesh;
                DefaultEarsMesh = BaseMeshData->EarsMesh;
                DefaultHandsMesh = BaseMeshData->HandsMesh;
                DefaultHairBaseMesh = BaseMeshData->DefaultHairBaseMesh;
                DefaultHairBackMesh = BaseMeshData->DefaultHairBackMesh;
                DefaultHairSidesMesh = BaseMeshData->DefaultHairSidesMesh;
                DefaultHairFringeMesh = BaseMeshData->DefaultHairFringeMesh;
                DefaultEyebrowsMesh = BaseMeshData->DefaultEyebrowsMesh;
                DefaultTopMesh = BaseMeshData->DefaultTopMesh;
                DefaultTrousersMesh = BaseMeshData->DefaultTrousersMesh;
                DefaultShoesMesh = BaseMeshData->DefaultShoesMesh;
                DefaultAccessoryMesh = BaseMeshData->DefaultAccessoryMesh;
                DefaultBeltMesh = BaseMeshData->DefaultBeltMesh;
                AnimBlueprintClass = BaseMeshData->AnimBlueprintClass;
            }
        }
    }
}

void AModularOfficeworker::ClearDynamicMeshes()
{
    // Hair 파츠 (SetHairCombination에서 변경되는 것들)
    ClearHairPart(EHairPartType::Base);
    ClearHairPart(EHairPartType::Fringe);
    ClearHairPart(EHairPartType::Back);
    ClearHairPart(EHairPartType::Sides);
    ClearHairPart(EHairPartType::Eyebrows);

    // Clothing (SetClothingByRank에서 변경되는 것들)
    if (Top) Top->SetSkeletalMesh(nullptr);
    if (Accessory1) Accessory1->SetSkeletalMesh(nullptr);
    if (Shoes) Shoes->SetSkeletalMesh(nullptr);
}

void AModularOfficeworker::SetCharacterAppearance(const FCharacterAppearance& Appearance, EEmployeeRank Rank, EEmployeeGender Gender, int32 InEmployeeID)
{
    // 먼저 동적 메시 초기화 (이전 캐릭터 데이터 제거)
    ClearDynamicMeshes();

    // EmployeeID 저장 (deterministic 신발 선택용)
    if (InEmployeeID >= 0)
    {
        SetEmployeeID(InEmployeeID);
    }

    SetEyebrowsPart(Appearance.EyebrowsPartName, Gender);
    SetHairCombination(Appearance.HairCombinationType, Appearance.HairRandomSeed);
    SetHairColors(Appearance.HairColors);
    SetEyebrowsColors(Appearance.HairColors.Color1, Appearance.HairColors.ColorLines);
    SetEyeColors(Appearance.EyeColors);
    SetSkinColors(Appearance.SkinColors);
    ApplyMorphTargets(Appearance.FacialExpression);
    SetClothingByRank(Rank, Gender, InEmployeeID);
}

void AModularOfficeworker::SetEyebrowsPart(const FString& PartName, EEmployeeGender Gender)
{
    UE_LOG(LogTemp, Warning, TEXT("SetEyebrowsPart called with: %s"), *PartName);
    SetHairPart(EHairPartType::Eyebrows, PartName, Gender);
}

void AModularOfficeworker::SetHairColors(const FHairColorSet& HairColors)
{
    UE_LOG(LogTemp, Warning, TEXT("SetHairColors called: Color1=%s"),
        *HairColors.Color1.ToString());

    // 모든 머리 파트에 색상 적용
    if (HairBase)
    {
        SetMaterialParameter(HairBase, TEXT("Color 1"), HairColors.Color1);
        SetMaterialParameter(HairBase, TEXT("Color 2"), HairColors.Color2);
        SetMaterialParameter(HairBase, TEXT("Color Lines"), HairColors.ColorLines);
    }

    if (HairBack)
    {
        SetMaterialParameter(HairBack, TEXT("Color 1"), HairColors.Color1);
        SetMaterialParameter(HairBack, TEXT("Color 2"), HairColors.Color2);
        SetMaterialParameter(HairBack, TEXT("Color Lines"), HairColors.ColorLines);
    }

    if (HairFringe)
    {
        SetMaterialParameter(HairFringe, TEXT("Color 1"), HairColors.Color1);
        SetMaterialParameter(HairFringe, TEXT("Color 2"), HairColors.Color2);
        SetMaterialParameter(HairFringe, TEXT("Color Lines"), HairColors.ColorLines);
    }

    if (HairSides)
    {
        SetMaterialParameter(HairSides, TEXT("Color 1"), HairColors.Color1);
        SetMaterialParameter(HairSides, TEXT("Color 2"), HairColors.Color2);
        SetMaterialParameter(HairSides, TEXT("Color Lines"), HairColors.ColorLines);
    }
}

void AModularOfficeworker::SetEyebrowsColors(const FLinearColor& Color1, const FLinearColor& ColorLines)
{
    if (Eyebrows)
    {
        SetMaterialParameter(Eyebrows, TEXT("Color 1"), Color1);
        SetMaterialParameter(Eyebrows, TEXT("Color Lines"), ColorLines);
    }
}

void AModularOfficeworker::SetEyeColors(const FEyeColorSet& Colors)
{
    // 좌우 눈 모두 같은 색상 적용
    SetMaterialParameter(Face, "Color Eye Left 1", Colors.EyeColor1);
    SetMaterialParameter(Face, "Color Eye Left 2", Colors.EyeColor2);
    SetMaterialParameter(Face, "Color Eye Right 1", Colors.EyeColor1);  // 좌눈과 같은 색
    SetMaterialParameter(Face, "Color Eye Right 2", Colors.EyeColor2);  // 좌눈과 같은 색
}

void AModularOfficeworker::SetSkinColors(const FSkinColorSet& Colors)
{
    // 피부 색깔 머티리얼 파라미터 설정
    SetMaterialParameter(Face, "Color Skin", Colors.SkinColor);
    SetMaterialParameter(Face, "Color Makeup", Colors.MakeupColor);
    SetMaterialParameter(Face, "Color Blush", Colors.BlushColor);

    SetMaterialParameter(Ears, "Color Skin", Colors.SkinColor);
    SetMaterialParameter(Ears, "Color Makeup", Colors.MakeupColor);
    SetMaterialParameter(Ears, "Color Blush", Colors.BlushColor);

    SetMaterialParameter(Hands, "Color Skin", Colors.SkinColor);
    SetMaterialParameter(Hands, "Color Makeup", Colors.MakeupColor);
    SetMaterialParameter(Hands, "Color Blush", Colors.BlushColor);
}

void AModularOfficeworker::ApplyMorphTargets(const FMorphTargetSet& MorphSet)
{
    if (!Face) return;

    // 먼저 모든 morph target을 0으로 리셋
    USkeletalMesh* SkeletalMesh = Face->GetSkeletalMeshAsset();
    if (SkeletalMesh)
    {
        for (UMorphTarget* MorphTarget : SkeletalMesh->GetMorphTargets())
        {
            if (MorphTarget)
            {
                Face->SetMorphTarget(MorphTarget->GetFName(), 0.0f);
            }
        }
    }

    for (const auto& MorphPair : MorphSet.MorphValues)
    {
        Face->SetMorphTarget(FName(*MorphPair.Key), MorphPair.Value);
    }
}

void AModularOfficeworker::SetClothingByRank(EEmployeeRank Rank, EEmployeeGender Gender, int32 InEmployeeID)
{
    UTableManagerSubsystem* TableManager = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableManager) return;

    // 테이블에서 랭크와 성별에 맞는 의상 데이터 가져오기
    FRankClothingTable* ClothingData = TableManager->GetClothingForRank(Rank, Gender);
    if (!ClothingData) return;

    // 상의 적용
    // IsValid() 체크 제거: LoadSynchronous()가 메모리에 없으면 자동으로 로드함
    USkeletalMesh* TopLoadedMesh = ClothingData->TopMeshes.LoadSynchronous();
    if (TopLoadedMesh && Top)
    {
        Top->SetSkeletalMesh(TopLoadedMesh);
        UE_LOG(LogTemp, Log, TEXT("Top mesh applied: %s"), *TopLoadedMesh->GetName());
    }

    // 액세서리 적용
    // IsValid() 체크 제거: LoadSynchronous()가 메모리에 없으면 자동으로 로드함
    USkeletalMesh* AccessoryLoadedMesh = ClothingData->AccessoryMeshes.LoadSynchronous();
    if (AccessoryLoadedMesh && Accessory1)
    {
        Accessory1->SetSkeletalMesh(AccessoryLoadedMesh);
        UE_LOG(LogTemp, Log, TEXT("Accessory mesh applied: %s"), *AccessoryLoadedMesh->GetName());
    }

    // 신발은 EmployeeID 기반 deterministic 선택
    if (ClothingData->ShoeMeshes.Num() > 0)
    {
        // EmployeeID가 유효하면 deterministic 선택, 아니면 저장된 EmployeeID 사용
        int32 UsedEmployeeID = (InEmployeeID >= 0) ? InEmployeeID : GetEmployeeID();

        int32 shoeIndex = 0;
        if (UsedEmployeeID >= 0)
        {
            // EmployeeID 기반 deterministic 선택
            shoeIndex = UsedEmployeeID % ClothingData->ShoeMeshes.Num();
            UE_LOG(LogTemp, Log, TEXT("Deterministic shoe selection: EmployeeID=%d, Index=%d/%d"),
                UsedEmployeeID, shoeIndex, ClothingData->ShoeMeshes.Num());
        }
        else
        {
            // 폴백: 랜덤 선택 (EmployeeID가 없는 경우)
            shoeIndex = FMath::RandRange(0, ClothingData->ShoeMeshes.Num() - 1);
            UE_LOG(LogTemp, Warning, TEXT("[Warning] Random shoe selection (no EmployeeID): Index=%d/%d"),
                shoeIndex, ClothingData->ShoeMeshes.Num());
        }

        TSoftObjectPtr<USkeletalMesh> ShoeMesh = ClothingData->ShoeMeshes[shoeIndex];

        // IsValid() 체크 제거: LoadSynchronous()가 메모리에 없으면 자동으로 로드함
        USkeletalMesh* LoadedMesh = ShoeMesh.LoadSynchronous();
        if (LoadedMesh && Shoes)
        {
            Shoes->SetSkeletalMesh(LoadedMesh);
            UE_LOG(LogTemp, Log, TEXT("Shoe mesh applied: %s (Index %d/%d, EmployeeID %d)"),
                *LoadedMesh->GetName(), shoeIndex, ClothingData->ShoeMeshes.Num() - 1, UsedEmployeeID);
        }
        else if (!ShoeMesh.IsNull())
        {
            UE_LOG(LogTemp, Error, TEXT("[Error] Failed to load shoe mesh: Index=%d, Path=%s"),
                shoeIndex, *ShoeMesh.ToSoftObjectPath().ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[Error] Shoe mesh path is null at index %d"), shoeIndex);
        }
    }
}

void AModularOfficeworker::SetMaterialParameter(USkeletalMeshComponent* Component, const FString& ParameterName, const
    FLinearColor& Color)
{
    if (!Component) return;

    // 머티리얼 인스턴스 다이나믹 생성 후 파라미터 설정
    for (int32 i = 0; i < Component->GetNumMaterials(); ++i)
    {
        UMaterialInstanceDynamic* DynamicMaterial = Component->CreateAndSetMaterialInstanceDynamic(i);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetVectorParameterValue(FName(*ParameterName), Color);
        }
    }
}

void AModularOfficeworker::SetHairPart(EHairPartType PartType, const FString& PartName, EEmployeeGender Gender)
{
    UE_LOG(LogTemp, Warning, TEXT("SetHairPart: Type=%d, PartName=%s"), (int32)PartType, *PartName);

    UTableManagerSubsystem* TableManager =
        GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableManager) return;

    // DataTable에서 헤어 정보 가져오기
    FHairPartTable* HairPart = TableManager->GetHairPartByName(PartName, Gender);
    if (!HairPart)
    {
        UE_LOG(LogTemp, Warning, TEXT("Hair part not found: %s"), *PartName);
        return;
    }

    // 해당 타입의 컴포넌트에 메시 적용
    USkeletalMeshComponent* Component = GetHairComponent(PartType);
    if (!Component)
    {
        UE_LOG(LogTemp, Error, TEXT("Component is NULL for PartType: %d"), (int32)PartType);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Component found: %s"), *Component->GetName());

    if (HairPart->HairMesh.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Loading mesh synchronously: %s"), *HairPart->HairMesh.ToString());

		USkeletalMesh* LoadedMesh = HairPart->HairMesh.LoadSynchronous();
        if (LoadedMesh)
        {
            Component->SetSkeletalMesh(LoadedMesh);
            UE_LOG(LogTemp, Warning, TEXT("? Successfully set hair mesh: %s to component %s"), *PartName, *Component->GetName());

            // 설정 후 확인
            if (Component->GetSkeletalMeshAsset())
            {
                UE_LOG(LogTemp, Warning, TEXT("? Verified: Component now has mesh: %s"), *Component->GetSkeletalMeshAsset()->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("? ERROR: Component mesh is still NULL after SetSkeletalMesh!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to load mesh for part: %s"), *PartName);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? HairMesh path is invalid for part: %s"), *PartName);
    }
}

void AModularOfficeworker::ClearHairPart(EHairPartType PartType)
{
    USkeletalMeshComponent* Component = GetHairComponent(PartType);
    if (Component)
    {
        Component->SetSkeletalMesh(nullptr);
    }
}

void AModularOfficeworker::SetRandomHairPart(EHairPartType PartType, const TArray<FString>& PartNames,
    FRandomStream& RandomStream, EEmployeeGender Gender)
{
    if (PartNames.Num() == 0) return;

    int32 RandomIndex = RandomStream.RandRange(0, PartNames.Num() - 1);
    FString SelectedPartName = PartNames[RandomIndex];

    SetHairPart(PartType, SelectedPartName, Gender);
}

USkeletalMeshComponent* AModularOfficeworker::GetHairComponent(EHairPartType PartType)
{
    switch (PartType)
    {
    case EHairPartType::Base:   return HairBase;
    case EHairPartType::Fringe: return HairFringe;
    case EHairPartType::Back:   return HairBack;
    case EHairPartType::Sides:  return HairSides;
    case EHairPartType::Eyebrows: return Eyebrows;
    default: return nullptr;
    }
}

void AModularOfficeworker::SetSelected(bool bSelected)
{
    Super::SetSelected(bSelected);  // 상태(bIsSelected)는 부모가 관리

    if (bSelected)
    {
        if (SelectionOverlayMaterial)
        {
            // 의상
            if (Top) Top->SetOverlayMaterial(SelectionOverlayMaterial);
            if (Trousers) Trousers->SetOverlayMaterial(SelectionOverlayMaterial);

            // 머리카락
            if (HairBase) HairBase->SetOverlayMaterial(SelectionOverlayMaterial);
            if (HairFringe) HairFringe->SetOverlayMaterial(SelectionOverlayMaterial);
            if (HairBack) HairBack->SetOverlayMaterial(SelectionOverlayMaterial);
            if (HairSides) HairSides->SetOverlayMaterial(SelectionOverlayMaterial);

            UE_LOG(LogTemp, Log, TEXT("[Officeworker] Applied overlay for Employee %d"), GetEmployeeID());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Officeworker] SelectionOverlayMaterial is null for Employee %d"), GetEmployeeID());
        }
    }
    else
    {
        // 의상
        if (Top) Top->SetOverlayMaterial(nullptr);
        if (Trousers) Trousers->SetOverlayMaterial(nullptr);

        // 머리카락
        if (HairBase) HairBase->SetOverlayMaterial(nullptr);
        if (HairFringe) HairFringe->SetOverlayMaterial(nullptr);
        if (HairBack) HairBack->SetOverlayMaterial(nullptr);
        if (HairSides) HairSides->SetOverlayMaterial(nullptr);

        UE_LOG(LogTemp, Log, TEXT("[Officeworker] Removed overlay for Employee %d"), GetEmployeeID());
    }
}

void AModularOfficeworker::SetGlowOverlay(bool bEnabled, FLinearColor Color)
{
    // 직원 선택 시와 동일한 SelectionOverlayMaterial 재사용 — 시각적 일관성 + 자산 1개로 통일
    // 단, 플레이어가 직접 선택한 경우(bIsSelected=true)는 buff 종료 시에도 overlay 유지해야 하므로 보호
    if (IsSelected()) return;

    if (!SelectionOverlayMaterial) return;

    UMaterialInterface* TargetMat = bEnabled ? SelectionOverlayMaterial : nullptr;
    if (Top)        Top->SetOverlayMaterial(TargetMat);
    if (Trousers)   Trousers->SetOverlayMaterial(TargetMat);
    if (HairBase)   HairBase->SetOverlayMaterial(TargetMat);
    if (HairFringe) HairFringe->SetOverlayMaterial(TargetMat);
    if (HairBack)   HairBack->SetOverlayMaterial(TargetMat);
    if (HairSides)  HairSides->SetOverlayMaterial(TargetMat);
}

void AModularOfficeworker::ApplyAppearanceAndNotify(const FCharacterAppearance& Appearance,
    EEmployeeRank Rank, EEmployeeGender Gender,
    int32 HairCombinationType, int32 RandomSeed, int32 InEmployeeID)
{
    ClearDynamicMeshes();
    SetCharacterAppearance(Appearance, Rank, Gender, InEmployeeID);
    SetHairCombination(HairCombinationType, RandomSeed);

    // 메시 로드 상태를 반복적으로 확인
    TWeakObjectPtr<AModularOfficeworker> WeakThis(this);

    TSharedPtr<FTimerHandle> CheckTimerPtr = MakeShared<FTimerHandle>();
    TSharedPtr<int32> CheckCountPtr = MakeShared<int32>(0);
    const int32 MaxCheckCount = 30;  // 최대 3초 대기 (0.1초 * 30)

    GetWorld()->GetTimerManager().SetTimer(*CheckTimerPtr,
        [WeakThis, CheckTimerPtr, CheckCountPtr, MaxCheckCount]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            AModularOfficeworker* This = WeakThis.Get();
            (*CheckCountPtr)++;

            // 주요 메시들이 실제로 로드되었는지 확인
            bool bAllMeshesLoaded = true;

            // Face (기본 메시)
            if (!This->Face || !This->Face->GetSkeletalMeshAsset())
            {
                bAllMeshesLoaded = false;
                UE_LOG(LogTemp, Warning, TEXT("Check %d: Face mesh not loaded"), *CheckCountPtr);
            }

            // Hair meshes (설정된 것만 확인)
            if (This->HairBase && This->HairBase->GetSkeletalMeshAsset() == nullptr)
            {
                bAllMeshesLoaded = false;
                UE_LOG(LogTemp, Warning, TEXT("Check %d: HairBase mesh not loaded"), *CheckCountPtr);
            }

            // Clothing
            if (!This->Top || !This->Top->GetSkeletalMeshAsset())
            {
                bAllMeshesLoaded = false;
                UE_LOG(LogTemp, Warning, TEXT("Check %d: Top mesh not loaded"), *CheckCountPtr);
            }

            if (!This->Shoes || !This->Shoes->GetSkeletalMeshAsset())
            {
                bAllMeshesLoaded = false;
                UE_LOG(LogTemp, Warning, TEXT("Check %d: Shoes mesh not loaded"), *CheckCountPtr);
            }

            if (bAllMeshesLoaded)
            {
                UE_LOG(LogTemp, Log, TEXT("All meshes loaded after %d checks (%.1fs)"),
                    *CheckCountPtr, *CheckCountPtr * 0.1f);
                This->GetWorld()->GetTimerManager().ClearTimer(*CheckTimerPtr);
                This->OnMeshLoadCompleted.ExecuteIfBound();
            }
            else if (*CheckCountPtr >= MaxCheckCount)
            {
                UE_LOG(LogTemp, Error, TEXT("[Error] Mesh load timeout after %d checks (%.1fs), proceeding anyway"),
                    *CheckCountPtr, *CheckCountPtr * 0.1f);
                This->GetWorld()->GetTimerManager().ClearTimer(*CheckTimerPtr);
                This->OnMeshLoadCompleted.ExecuteIfBound();
            }
        },
        0.1f, true);  // 0.1초마다 반복 체크
}
