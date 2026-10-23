// Funny 직원 재리타겟 후처리 도구 3종 (테스트가 아니라 에디터에서 돌리는 일회성 수정 작업).
// 실행 순서: CGR.Tools.FunnyArmAlignRetarget 하나가 전부 수행한다(재리타겟 + Hips ÷100 + rig 스케일 복원).
//            FunnyHipsFix / FunnyBoneScaleFix 는 그 결과를 다시 돌려도 안전한 멱등 점검용 — 단독 복구에도 쓴다.
//
// 수정 — 리타겟 출력 32종의 Hips 트랙 translation 을 ÷100. 근거: 다른 41본은 refpose 와 일치, Hips 만 ~100배.
// 리타게터는 Root(스켈레톤 refpose 스케일 1) 기준으로 로컬화하지만 메시는 바인드에서 Root 에 x100 을 건다.
// 멱등: 비율 >20 인 트랙만 손댄다. Root 트랙은 loc 0/scale 1 이어야 하며 그 불변을 단언한다.
// FileHelpers.h = UnrealEd 메서드, Build.cs 가 bBuildEditor 일 때만 링크 — include 자체를 가드 안에 둔다
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "ReferenceSkeleton.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyHipsFix, "CGR.Tools.FunnyHipsFix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyHipsFix::RunTest(const FString& Parameters)
{
	static const FString AnimRoot = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims");
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"));
	if (!Mesh) { AddError(TEXT("메시 로드 실패")); return false; }
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const float RefHipsLen = Ref.GetRefBonePose()[Ref.FindBoneIndex(TEXT("Hips"))].GetTranslation().Size();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(*AnimRoot, Assets, true);
	Assets.RemoveAll([](const FAssetData& A) { return A.AssetClassPath.GetAssetName() != TEXT("AnimSequence"); });

	int32 Patched = 0, Skipped = 0, RootBad = 0;
	TArray<UPackage*> ToSave;
	for (const FAssetData& A : Assets)
	{
		UAnimSequence* Seq = Cast<UAnimSequence>(A.GetAsset());
		if (!Seq) { continue; }
		const IAnimationDataModel* M = Seq->GetDataModel();
		if (!M->IsValidBoneTrackName(TEXT("Hips"))) { ++Skipped; continue; }

		// Root 불변 확인 — 여기에 스케일/이동이 있으면 이 수정의 전제가 깨진다
		if (M->IsValidBoneTrackName(TEXT("Root")))
		{
			const FTransform R0 = M->GetBoneTrackTransform(TEXT("Root"), FFrameNumber(0));
			if (!R0.GetTranslation().IsNearlyZero(0.01f) || !R0.GetScale3D().Equals(FVector::OneVector, 0.001f)) { ++RootBad; }
		}

		const float T0 = M->GetBoneTrackTransform(TEXT("Hips"), FFrameNumber(0)).GetTranslation().Size();
		const float Ratio = RefHipsLen > 0.001f ? T0 / RefHipsLen : 0.f;
		if (Ratio < 20.f) { ++Skipped; UE_LOG(LogTemp, Display, TEXT("[HipsFix] %-24s ratio=%.1f 이미 정상 — 건너뜀"), *Seq->GetName(), Ratio); continue; }

		// 키 전체 읽어 translation 만 ÷100
		TArray<FVector3f> Pos; TArray<FQuat4f> Rot; TArray<FVector3f> Scl;
		const int32 N = M->GetNumberOfKeys();
		Pos.Reserve(N); Rot.Reserve(N); Scl.Reserve(N);
		for (int32 k = 0; k < N; ++k)
		{
			const FTransform T = M->GetBoneTrackTransform(TEXT("Hips"), FFrameNumber(k));
			Pos.Add(FVector3f(T.GetTranslation() * 0.01f));
			Rot.Add(FQuat4f(T.GetRotation()));
			Scl.Add(FVector3f(T.GetScale3D()));
		}
		IAnimationDataController& Ctl = Seq->GetController();
		Ctl.OpenBracket(FText::FromString(TEXT("Funny Hips /100")), false);
		Ctl.SetBoneTrackKeys(TEXT("Hips"), Pos, Rot, Scl, false);
		Ctl.CloseBracket(false);

		const float After = Seq->GetDataModel()->GetBoneTrackTransform(TEXT("Hips"), FFrameNumber(0)).GetTranslation().Size();
		UE_LOG(LogTemp, Display, TEXT("[HipsFix] %-24s ratio %.1f → %.2f  (keys=%d)"), *Seq->GetName(), Ratio, After / RefHipsLen, N);
		ToSave.Add(Seq->GetOutermost());
		++Patched;
	}
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	UE_LOG(LogTemp, Display, TEXT("[HipsFix] ★ 패치=%d  건너뜀=%d  Root불변위반=%d  (대상 %d)"), Patched, Skipped, RootBad, Assets.Num());
	TestEqual(TEXT("Root 트랙은 항등이어야 함"), RootBad, 0);
	TestTrue(TEXT("최소 1개 패치됨"), Patched > 0);
	return true;
}
#endif

// 수정 2 — refpose 스케일이 1 이 아닌 본(들)의 트랙 스케일을 메시 refpose 값으로. 리타게터가 스케일을 버려 1 로 쓰면
// 그 본 아래 골격 전체가 1/100 로 무너진다(Root/Hips 는 무죄, 실측으로 찾은 중간 본). 본 이름 고정 없음.
// 멱등: 트랙 스케일이 이미 refpose 값이면 건너뜀. 비단위 본이 하나도 없으면 전제 붕괴 → 실패.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyBoneScaleFix, "CGR.Tools.FunnyBoneScaleFix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyBoneScaleFix::RunTest(const FString& Parameters)
{
	static const FString AnimRoot = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims");
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"));
	if (!Mesh) { AddError(TEXT("메시 로드 실패")); return false; }
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	TArray<TPair<FName, FVector>> NonUnit;
	for (int32 b = 0; b < Ref.GetNum(); ++b)
	{
		const FVector S = Ref.GetRefBonePose()[b].GetScale3D();
		if (!S.Equals(FVector::OneVector, 0.001f)) { NonUnit.Emplace(Ref.GetBoneName(b), S); UE_LOG(LogTemp, Display, TEXT("[ScaleFix] 비단위 본 %s refScale=%s"), *Ref.GetBoneName(b).ToString(), *S.ToString()); }
	}
	if (NonUnit.IsEmpty()) { AddError(TEXT("refpose 에 비단위 스케일 본이 없음 — 이 수정의 전제가 틀림")); return false; }

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets; AR.GetAssetsByPath(*AnimRoot, Assets, true);
	Assets.RemoveAll([](const FAssetData& A) { return A.AssetClassPath.GetAssetName() != TEXT("AnimSequence"); });

	int32 Patched = 0, Skipped = 0, NoTrack = 0; TArray<UPackage*> ToSave;
	for (const FAssetData& A : Assets)
	{
		UAnimSequence* Seq = Cast<UAnimSequence>(A.GetAsset()); if (!Seq) { continue; }
		bool bDirty = false;
		for (const auto& [BN, RefScale] : NonUnit)
		{
			const IAnimationDataModel* M = Seq->GetDataModel();
			if (!M->IsValidBoneTrackName(BN)) { ++NoTrack; continue; }
			const FVector S0 = M->GetBoneTrackTransform(BN, FFrameNumber(0)).GetScale3D();
			if (S0.Equals(RefScale, 0.01f)) { ++Skipped; continue; }
			TArray<FVector3f> Pos; TArray<FQuat4f> Rot; TArray<FVector3f> Scl;
			const int32 N = M->GetNumberOfKeys();
			for (int32 k = 0; k < N; ++k)
			{
				const FTransform T = M->GetBoneTrackTransform(BN, FFrameNumber(k));
				Pos.Add(FVector3f(T.GetTranslation())); Rot.Add(FQuat4f(T.GetRotation())); Scl.Add(FVector3f(RefScale));
			}
			IAnimationDataController& Ctl = Seq->GetController();
			Ctl.OpenBracket(FText::FromString(TEXT("Funny bone scale = mesh refpose")), false);
			Ctl.SetBoneTrackKeys(BN, Pos, Rot, Scl, false);
			Ctl.CloseBracket(false);
			UE_LOG(LogTemp, Display, TEXT("[ScaleFix] %-24s %s scale %.3f → %.1f (keys=%d)"), *Seq->GetName(), *BN.ToString(), S0.X, RefScale.X, N);
			bDirty = true;
		}
		if (bDirty) { ToSave.Add(Seq->GetOutermost()); ++Patched; }
	}
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	UE_LOG(LogTemp, Display, TEXT("[ScaleFix] ★ 패치=%d 건너뜀=%d 트랙없음=%d (대상 %d, 비단위본 %d)"), Patched, Skipped, NoTrack, Assets.Num(), NonUnit.Num());
	TestTrue(TEXT("최소 1개 패치됨"), Patched > 0);
	return true;
}
#endif

// 수정 3 — 팔 T-포즈. 스틱 refpose(A-포즈)와 Funny refpose(T-포즈)가 달라 회전 델타가 팔을 공중에 남긴다.
// 타겟 리타겟 포즈를 만들어 팔 체인을 소스 방향에 자동 정렬 → 임시 폴더로 재리타겟 → 트랙을 기존 에셋에 복사(경로 유지).
// 복사 시 Hips ÷100 + 비단위 본(rig) 스케일=refpose 를 같이 적용한다(리타게터가 매번 되돌리는 두 가지).
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "RetargetEditor/IKRetargeterController.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargeterPoseGenerator.h"
#include "Retargeter/IKRetargeter.h"
#include "Animation/BlendSpace.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyArmAlignRetarget, "CGR.Tools.FunnyArmAlignRetarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyArmAlignRetarget::RunTest(const FString& Parameters)
{
	static const FString StickRoot = TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/Anims");
	static const FString FunnyRoot = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims");
	static const FString TempRoot = TEXT("/Game/_RT2");
	UIKRetargeter* RTG = LoadObject<UIKRetargeter>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Rigs/RTG_Stick_To_Funny.RTG_Stick_To_Funny"));
	USkeletalMesh* Stick = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/SK_StickmanCG.SK_StickmanCG"));
	USkeletalMesh* Funny = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Body/SK_Body_White_001.SK_Body_White_001"));
	if (!RTG || !Stick || !Funny) { AddError(TEXT("로드 실패")); return false; }

	// ① 타겟 리타겟 포즈: 팔 체인만 소스에 정렬 (다리/척추는 이미 검증됨 — 건드리지 않음)
	UIKRetargeterController* Ctl = UIKRetargeterController::GetController(RTG);
	const FName PoseName = TEXT("ArmsAlignedToStick");
	if (RTG->GetCurrentRetargetPoseName(ERetargetSourceOrTarget::Target) != PoseName)
	{
		Ctl->CreateRetargetPose(PoseName, ERetargetSourceOrTarget::Target);
		Ctl->SetCurrentRetargetPose(PoseName, ERetargetSourceOrTarget::Target);
	}
	Ctl->ResetRetargetPose(PoseName, TArray<FName>(), ERetargetSourceOrTarget::Target);
	const TArray<FName> ArmBones = { TEXT("LeftShoulder"), TEXT("LeftArm"), TEXT("LeftForeArm"), TEXT("LeftHand"), TEXT("RightShoulder"), TEXT("RightArm"), TEXT("RightForeArm"), TEXT("RightHand") };
	Ctl->AutoAlignBones(ArmBones, ERetargetAutoAlignMethod::ChainToChain, ERetargetSourceOrTarget::Target);
	if (const FIKRetargetPose* P = RTG->GetCurrentRetargetPose(ERetargetSourceOrTarget::Target))
	{
		FString D; for (const auto& KV : P->GetAllDeltaRotations()) { D += FString::Printf(TEXT("%s(%.1f°) "), *KV.Key.ToString(), FMath::RadiansToDegrees(KV.Value.GetAngle())); }
		UE_LOG(LogTemp, Display, TEXT("[ArmFix] 타겟 포즈 '%s' 델타: %s"), *PoseName.ToString(), *D);
		TestTrue(TEXT("팔 정렬 델타가 생성됨"), P->GetAllDeltaRotations().Num() > 0);
	}
	UEditorLoadingAndSavingUtils::SavePackages({ RTG->GetOutermost() }, false);

	// ② 스틱 원본 → 임시 폴더 재리타겟 (루트 세트 / Moving 세트 각각, 하위폴더 구조 보존)
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TMap<FName, UAnimSequence*> Existing; // 이름 → 기존 Funny 시퀀스
	{
		TArray<FAssetData> F; AR.GetAssetsByPath(*FunnyRoot, F, true);
		for (const FAssetData& A : F) { if (UAnimSequence* S = Cast<UAnimSequence>(A.GetAsset())) { Existing.Add(S->GetFName(), S); } }
	}
	int32 Copied = 0, Missing = 0, KeyMismatch = 0; TArray<UPackage*> ToSave;
	const FReferenceSkeleton& Ref = Funny->GetRefSkeleton();
	for (const FString& Sub : { FString(TEXT("")), FString(TEXT("/Moving")) })
	{
		TArray<FAssetData> Src; AR.GetAssetsByPath(*(StickRoot + Sub), Src, false);
		FIKRetargetBatchOperationContext Cx;
		for (const FAssetData& A : Src)
		{
			const FName C = A.AssetClassPath.GetAssetName();
			if (C == TEXT("AnimSequence") || C == TEXT("BlendSpace")) { Cx.AssetsToRetarget.Add(A.GetAsset()); }
		}
		Cx.SourceMesh = Stick; Cx.TargetMesh = Funny; Cx.IKRetargetAsset = RTG;
		Cx.NameRule.FolderPath = TempRoot + Sub; Cx.bUseSourcePath = false; Cx.bOverwriteExistingFiles = true; Cx.bIncludeReferencedAssets = false;
		UIKRetargetBatchOperation* Op = NewObject<UIKRetargetBatchOperation>();
		Op->RunRetarget(Cx);
		UE_LOG(LogTemp, Display, TEXT("[ArmFix] 재리타겟 %s: 입력 %d"), *(StickRoot + Sub), Cx.AssetsToRetarget.Num());

		// ③ 임시 출력 트랙 → 기존 에셋 (Hips ÷100, rig 스케일 refpose)
		AR.ScanPathsSynchronous({ TempRoot + Sub }, true);
		TArray<FAssetData> Out; AR.GetAssetsByPath(*(TempRoot + Sub), Out, false);
		for (const FAssetData& A : Out)
		{
			UAnimSequence* T = Cast<UAnimSequence>(A.GetAsset()); if (!T) { continue; }
			UAnimSequence** Dst = Existing.Find(T->GetFName());
			if (!Dst) { ++Missing; UE_LOG(LogTemp, Warning, TEXT("[ArmFix] 기존 에셋 없음: %s"), *T->GetName()); continue; }
			const IAnimationDataModel* SM = T->GetDataModel();
			IAnimationDataController& DC = (*Dst)->GetController();
			const int32 N = SM->GetNumberOfKeys();
			if ((*Dst)->GetDataModel()->GetNumberOfKeys() != N) { ++KeyMismatch; UE_LOG(LogTemp, Warning, TEXT("[ArmFix] 키 수 불일치 %s: %d vs %d"), *T->GetName(), (*Dst)->GetDataModel()->GetNumberOfKeys(), N); continue; }
			DC.OpenBracket(FText::FromString(TEXT("Funny arm-aligned retarget")), false);
			TArray<FName> Tracks; SM->GetBoneTrackNames(Tracks);
			for (const FName& B : Tracks)
			{
				const int32 RefIdx = Ref.FindBoneIndex(B);
				const FVector RefScale = RefIdx != INDEX_NONE ? Ref.GetRefBonePose()[RefIdx].GetScale3D() : FVector::OneVector;
				const bool bHips = B == TEXT("Hips"), bNonUnit = !RefScale.Equals(FVector::OneVector, 0.001f);
				TArray<FVector3f> Pos; TArray<FQuat4f> Rot; TArray<FVector3f> Scl;
				for (int32 k = 0; k < N; ++k)
				{
					const FTransform X = SM->GetBoneTrackTransform(B, FFrameNumber(k));
					Pos.Add(FVector3f(bHips ? X.GetTranslation() * 0.01f : X.GetTranslation())); Rot.Add(FQuat4f(X.GetRotation())); Scl.Add(FVector3f(bNonUnit ? RefScale : X.GetScale3D()));
				}
				if (!(*Dst)->GetDataModel()->IsValidBoneTrackName(B)) { DC.AddBoneCurve(B, false); }
				DC.SetBoneTrackKeys(B, Pos, Rot, Scl, false);
			}
			DC.CloseBracket(false);
			ToSave.Add((*Dst)->GetOutermost()); ++Copied;
		}
	}
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	UE_LOG(LogTemp, Display, TEXT("[ArmFix] ★ 복사=%d 기존없음=%d 키불일치=%d (기존 %d)  — 임시 %s 는 디스크에서 삭제할 것"), Copied, Missing, KeyMismatch, Existing.Num(), *TempRoot);
	TestEqual(TEXT("31개 전부 복사"), Copied, 31);
	TestEqual(TEXT("키 불일치 없음"), KeyMismatch, 0);
	return true;
}
#endif

// 도구 4 — 리타겟 소스 메시의 스켈레톤 재연결. 에디터 rename_asset 으로 폴더를 옮길 때 메시가 옛 스켈레톤 경로(리디렉터 없음)를
// 물고 남으면 애니와 스켈레톤이 달라 refpose 로 굳는다. Python 은 SkeletalMesh.Skeleton 세터가 없어 C++ 로만 고친다.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Animation/Skeleton.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyRetargetSourceFix, "CGR.Tools.FunnyRetargetSourceFix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyRetargetSourceFix::RunTest(const FString& Parameters)
{
	static const TCHAR* MeshPath = TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/SK_StickmanCG.SK_StickmanCG");
	static const TCHAR* SkelPath = TEXT("/Game/CompanyGrowth/Characters/_RetargetSource/StickmanCG/SK_StickmanCG_Skeleton.SK_StickmanCG_Skeleton");
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
	USkeleton* Skel = LoadObject<USkeleton>(nullptr, SkelPath);
	if (!Mesh || !Skel) { AddError(FString::Printf(TEXT("로드 실패 mesh=%d skel=%d"), !!Mesh, !!Skel)); return false; }
	UE_LOG(LogTemp, Display, TEXT("[SrcFix] before: mesh.skeleton=%s"), Mesh->GetSkeleton() ? *Mesh->GetSkeleton()->GetPathName() : TEXT("NULL"));
	if (Mesh->GetSkeleton() != Skel)
	{
		Mesh->Modify();
		Mesh->SetSkeleton(Skel);
		Mesh->MarkPackageDirty();
		UEditorLoadingAndSavingUtils::SavePackages({ Mesh->GetOutermost() }, false);
		UE_LOG(LogTemp, Display, TEXT("[SrcFix] ★ skeleton relinked and saved"));
	}
	else { UE_LOG(LogTemp, Display, TEXT("[SrcFix] already linked — nothing to do")); }
	TestEqual(TEXT("스켈레톤 연결"), Mesh->GetSkeleton(), Skel);
	return true;
}
#endif
