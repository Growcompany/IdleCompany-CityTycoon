// 검증 — 패치된 애니를 실제 컴포넌트가 재생했을 때의 본 간격. 패치의 자기보고(ratio)가 아닌 독립 측정.
// 서기: Head-Hips≈78 / Hips.z≈54.  앉기: Hips.z 가 낮아지되 Head-Hips 는 여전히 사람 크기.
// BlendSpace(BS_StickLocomotion)는 시퀀스 패치에서 제외됐으므로 샘플 재생으로 따로 확인.
#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimInstance.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyPoseVerify2, "CGR.Employee.Funny.PoseVerify",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

static void MeasureC(const TCHAR* Tag, USkeletalMeshComponent* C, float& OutHH, float& OutHipsZ)
{
	for (int32 i = 0; i < 5; ++i) { C->TickAnimation(1.f / 60.f, false); C->RefreshBoneTransforms(); }
	const float Hips = C->GetSocketTransform(TEXT("Hips"), RTS_Component).GetTranslation().Z;
	const float Head = C->GetSocketTransform(TEXT("Head"), RTS_Component).GetTranslation().Z;
	const float Foot = C->GetSocketTransform(TEXT("LeftFoot"), RTS_Component).GetTranslation().Z;
	OutHH = Head - Hips; OutHipsZ = Hips;
	UE_LOG(LogTemp, Display, TEXT("[V2] %-26s Head-Hips=%7.2f  Hips.z=%7.2f  Hips-Foot=%7.2f"), Tag, Head - Hips, Hips, Hips - Foot);
}

bool FCGRFunnyPoseVerify2::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"));
	UClass* Abp = LoadObject<UClass>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/ABP_FunnyWorker.ABP_FunnyWorker_C"));
	if (!World || !Mesh || !Abp) { AddError(TEXT("로드 실패")); return false; }
	AActor* Holder = World->SpawnActor<AActor>();
	auto Make = [&]() { auto* C = NewObject<USkeletalMeshComponent>(Holder); C->SetSkeletalMesh(Mesh); C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones; return C; };
	float HH, Hz;

	// 서기 3종 + 앉기 2종 — 시퀀스 직접 재생
	for (const TCHAR* N : { TEXT("AS_Idle"), TEXT("AS_Walk"), TEXT("AS_Typing"), TEXT("AS_Sitting"), TEXT("AS_Stand_To_Sit") })
	{
		UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/%s.%s"), N, N));
		if (!S) { AddError(FString::Printf(TEXT("%s 로드 실패"), N)); continue; }
		auto* C = Make(); C->SetAnimationMode(EAnimationMode::AnimationSingleNode); C->RegisterComponentWithWorld(World); C->PlayAnimation(S, true);
		MeasureC(N, C, HH, Hz);
		TestTrue(FString::Printf(TEXT("%s Head-Hips 사람 크기"), N), HH > 60.f && HH < 95.f);
		C->DestroyComponent();
	}
	// 블렌드스페이스 — 시퀀스 패치에서 제외됐던 것. 샘플이 패치된 Run 이므로 정상이어야 한다.
	{
		UBlendSpace* BS = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/Moving/BS_StickLocomotion.BS_StickLocomotion"));
		if (BS) { auto* C = Make(); C->SetAnimationMode(EAnimationMode::AnimationSingleNode); C->RegisterComponentWithWorld(World); C->PlayAnimation(BS, true); MeasureC(TEXT("BS_StickLocomotion"), C, HH, Hz); TestTrue(TEXT("BlendSpace Head-Hips 사람 크기"), HH > 60.f && HH < 95.f); C->DestroyComponent(); }
		else { AddError(TEXT("BS_StickLocomotion 로드 실패")); }
	}
	// ABP — 실제 게임 경로 (기본 상태 = Sitting)
	{
		auto* C = Make(); C->SetAnimationMode(EAnimationMode::AnimationBlueprint); C->SetAnimInstanceClass(Abp); C->RegisterComponentWithWorld(World);
		MeasureC(TEXT("ABP_FunnyWorker"), C, HH, Hz);
		TestTrue(TEXT("ABP Head-Hips 사람 크기"), HH > 60.f && HH < 95.f);
		C->DestroyComponent();
	}
	Holder->Destroy();
	return true;
}
#endif

// 팔 각도 대조 — 절대 손높이는 비율(팔 길이)에 묶이므로, 스틱 원본과 Funny 를 같은 애니로 재생해 상완/전체 팔의 수직 기준 각도를 비교.
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyArmAngle, "CGR.Employee.Funny.ArmAngle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

static void ArmAngles(const TCHAR* Tag, USkeletalMeshComponent* C, int32 Ticks, float& OutUpper, float& OutFull)
{
	for (int32 i = 0; i < Ticks; ++i) { C->TickAnimation(1.f / 60.f, false); C->RefreshBoneTransforms(); }
	auto P = [&](const TCHAR* B) { return C->GetSocketTransform(B, RTS_Component).GetTranslation(); };
	const FVector Arm = P(TEXT("LeftArm")), Fore = P(TEXT("LeftForeArm")), Hand = P(TEXT("LeftHand")), RArm = P(TEXT("RightArm")), RHand = P(TEXT("RightHand"));
	auto Deg = [](const FVector& V) { return FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(V.GetSafeNormal(), FVector(0, 0, -1)))); };
	OutUpper = Deg(Fore - Arm); OutFull = Deg(Hand - Arm);
	UE_LOG(LogTemp, Display, TEXT("[Angle] %-22s L upper=%5.1f° full=%5.1f° | R full=%5.1f° | armLen=%.1f  (0°=수직 아래, 90°=수평)"), Tag, OutUpper, OutFull, Deg(RHand - RArm), (Hand - Arm).Size());
}

bool FCGRFunnyArmAngle::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	struct FCase { const TCHAR* Mesh; const TCHAR* Anim; const TCHAR* Tag; };
	const FCase Cases[] = {
		{ TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/SK_StickmanCG.SK_StickmanCG"), nullptr, TEXT("Stick RefPose") },
		{ TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/SK_StickmanCG.SK_StickmanCG"), TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/Anims/AS_Idle.AS_Idle"), TEXT("Stick AS_Idle") },
		{ TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/SK_StickmanCG.SK_StickmanCG"), TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/Anims/AS_Walk.AS_Walk"), TEXT("Stick AS_Walk") },
		{ TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/SK_StickmanCG.SK_StickmanCG"), TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/Anims/AS_Typing.AS_Typing"), TEXT("Stick AS_Typing") },
		{ TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"), nullptr, TEXT("Funny RefPose") },
		{ TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"), TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/AS_Idle.AS_Idle"), TEXT("Funny AS_Idle") },
		{ TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"), TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/AS_Walk.AS_Walk"), TEXT("Funny AS_Walk") },
		{ TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"), TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/AS_Typing.AS_Typing"), TEXT("Funny AS_Typing") },
	};
	AActor* Holder = World->SpawnActor<AActor>();
	TMap<FString, float> Full;
	for (const FCase& K : Cases)
	{
		USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, K.Mesh); UAnimSequence* A = K.Anim ? LoadObject<UAnimSequence>(nullptr, K.Anim) : nullptr;
		if (!M || (K.Anim && !A)) { AddError(FString::Printf(TEXT("%s 로드 실패"), K.Tag)); continue; }
		auto* C = NewObject<USkeletalMeshComponent>(Holder); C->SetSkeletalMesh(M); C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones; C->SetAnimationMode(EAnimationMode::AnimationSingleNode); C->RegisterComponentWithWorld(World);
		if (A) { C->PlayAnimation(A, true); }
		float U, F; ArmAngles(K.Tag, C, 5, U, F); Full.Add(K.Tag, F); C->DestroyComponent();
	}
	for (const TCHAR* N : { TEXT("AS_Idle"), TEXT("AS_Walk"), TEXT("AS_Typing") })
	{
		const float S = Full.FindRef(FString::Printf(TEXT("Stick %s"), N)), F = Full.FindRef(FString::Printf(TEXT("Funny %s"), N));
		UE_LOG(LogTemp, Display, TEXT("[Angle] ★ %-10s stick=%5.1f° funny=%5.1f° diff=%5.1f°"), N, S, F, F - S);
		TestTrue(FString::Printf(TEXT("%s 팔 각도 스틱과 ±12° 이내"), N), FMath::Abs(F - S) < 12.f);
	}
	Holder->Destroy();
	return true;
}
#endif
