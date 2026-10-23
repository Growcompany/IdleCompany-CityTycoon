#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"
#include "OverseasDioramaStage.generated.h"

class FDioramaGeometry;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UAnimSequence;
class UPointLightComponent;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UStaticMesh;
class USkeletalMesh;
class UTextureRenderTarget2D;

// 라이브 카메라 자세 — 시안 공간(미터, Y 위) 기준.
struct COMPANYGROWTHRENEWAL_API FDioramaLiveCamera
{
	FVector TargetM = FVector::ZeroVector;
	double YawDeg = 14.0;
	double PitchDeg = 45.0;
	double DistM = 110.0;
};

/**
 * 해외 시설 디오라마의 격리 촬영 무대. 도시에서 멀리 스폰하고 ShowOnly 촬영으로 이 무대만 RT 에 담는다.
 * 정지 부품은 모듈별 절차 메시(색별 섹션)로 합치고, 움직이는 부품만 따로 컴포넌트를 둔다.
 * RT 알파는 반전(형상=0)이라 UI 는 M_GachaRevealRT 로 보정한다.
 */
UCLASS(NotPlaceable, Transient)
class COMPANYGROWTHRENEWAL_API AOverseasDioramaStage : public AActor
{
	GENERATED_BODY()

public:
	AOverseasDioramaStage();

	static constexpr double StillPixelsPerMeter = 5.8;
	static constexpr double StillHeightScale = 1.6;

	void BuildFor(const FOverseasDioramaSpec& Spec, bool bMiniature);
	const FOverseasDioramaSpec& GetBuiltSpec() const { return BuiltSpec; }
	bool IsBuiltMiniature() const { return bBuiltMiniature; }
	bool HasBuild() const { return bHasBuild; }

	void StartLive(UTextureRenderTarget2D* Target);
	// 매 프레임 촬영이 아직 한 번도 안 돈 첫 프레임(빈 RT)을 메우거나, 월드가 틱하지 않는 탐침에서 쓴다.
	void CaptureLiveFrameNow();
	void StopLive();
	bool IsLive() const { return bLive; }
	void FocusModule(EDioramaModule Module, bool bInstant);
	void PlayIntro();

	// 지도 그림 1회 촬영. 렌더가 없는 실행(-nullrhi)이면 RT 없이 크기·피벗만 채운다.
	bool CaptureStill(UTextureRenderTarget2D*& InOutTarget, FDioramaStillInfo& OutInfo);

	FDioramaLiveCamera GetLiveCameraGoal() const { return Goal; }
	FDioramaLiveCamera GetLiveCameraCurrent() const { return Current; }
	int32 GetRigComponentCount() const { return Rigs.Num(); }
	const TArray<TObjectPtr<UProceduralMeshComponent>>& GetModuleMeshes() const { return ModuleMeshes; }
	UProceduralMeshComponent* GetTileMesh() const { return TileMesh; }
	FBox2D GetTileRectM() const { return TileRectM; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Diorama") TObjectPtr<USceneComponent> StageRoot;
	// 진입 연출에서 솟아오르고, 지도 그림 촬영 동안 높이를 과장하는 대상.
	UPROPERTY(VisibleAnywhere, Category = "Diorama") TObjectPtr<USceneComponent> ContentRoot;
	UPROPERTY(VisibleAnywhere, Category = "Diorama") TObjectPtr<UProceduralMeshComponent> TileMesh;
	UPROPERTY(VisibleAnywhere, Category = "Diorama") TObjectPtr<USceneCaptureComponent2D> LiveCapture;
	UPROPERTY(VisibleAnywhere, Category = "Diorama") TObjectPtr<USceneCaptureComponent2D> StillCapture;

	UPROPERTY() TObjectPtr<UMaterialInterface> ToonMaterial;
	UPROPERTY() TObjectPtr<UStaticMesh> TruckMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<USkeletalMesh> OilPumpMesh;
	UPROPERTY() TObjectPtr<UAnimSequence> OilPumpAnimation;
	UPROPERTY(VisibleAnywhere, Category = "Diorama") TObjectPtr<UPointLightComponent> OilKeyLight;
	UPROPERTY(VisibleAnywhere, Category = "Diorama") TObjectPtr<UPointLightComponent> OilFillLight;

private:
	struct FRigRuntime
	{
		uint8 Kind = 0;
		TWeakObjectPtr<USceneComponent> Component;
		TArray<TWeakObjectPtr<UPrimitiveComponent>> Packets;
		FVector BaseLocation = FVector::ZeroVector;
		FVector PathStart = FVector::ZeroVector;
		FVector PathEnd = FVector::ZeroVector;
		double YawDeg = 0.0;
		float Speed = 0.f;
		float Amplitude = 0.f;
		float Period = 1.f;
		float Phase = 0.f;
		bool bActive = true;
	};

	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> ModuleMeshes;
	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> GeneratedComponents;
	UPROPERTY() TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> ColorMaterials;

	TArray<FRigRuntime> Rigs;
	TArray<FVector2D> ModuleOffsetsM;
	TArray<FBox2D> ModuleRectsM;
	TArray<EDioramaModule> ModuleTypes;
	FOverseasDioramaSpec BuiltSpec;
	FBox2D TileRectM = FBox2D(ForceInit);
	bool bBuiltMiniature = false;
	bool bHasBuild = false;
	bool bLive = false;

	FDioramaLiveCamera Goal;
	FDioramaLiveCamera Current;
	FDioramaLiveCamera IntroFrom;
	float IntroTime = -1.f;
	float AnimTime = 0.f;

	void ClearGenerated();
	UMaterialInstanceDynamic* MaterialForColor(uint32 PackedColor);
	void CommitGeometry(UProceduralMeshComponent* Target, const FDioramaGeometry& Geometry);
	UProceduralMeshComponent* NewMeshComponent(USceneComponent* Parent, const FVector& RelativeLocation);
	void ConfigureCapture(USceneCaptureComponent2D* Capture) const;
	void ApplyLiveCamera(const FDioramaLiveCamera& Camera);
	void TickRigs(float DeltaSeconds);
	void SetPacketsHidden(bool bHide);
	FDioramaLiveCamera ComputeGoal(EDioramaModule Module) const;
};
