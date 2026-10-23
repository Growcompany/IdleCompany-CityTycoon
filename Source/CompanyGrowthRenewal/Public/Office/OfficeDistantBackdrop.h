#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OfficeDistantBackdrop.generated.h"

class UPrimitiveComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UTextureRenderTarget2D;
class ATimeCycleSky;
class AActor;

UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeDistantBackdrop : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 FaceCount = 3;
	static constexpr int32 MaxSourceFaceResolution = 1024;
	static constexpr int32 SourceDisplayResolution = 512;
	// 캡처 해상도. 기본 1024 로 찍어 512 표시로 부드럽게 줄인다 — 512 로 찍으면 소프트 필터를 거쳐도 도트가 남는다(2026-10-10 사용자 지적). (cg.Office.CityPhotoResolution)
	static int32 GetSourceFaceResolution();
	// false 면 캡처 타깃을 RGBA8 로 잡는다 — GLES 렌더타깃 경로 A/B 용 (cg.Office.CityPhotoHDR).
	static bool UseHDRCapture();
	static constexpr float FaceDistanceCm = 100000.f;
	static FRotator GetFaceRotation(int32 FaceIndex);
	static void BuildFaceGeometry(int32 FaceIndex, TArray<FVector>& Vertices, TArray<FVector2D>& UVs, float DistanceCm = FaceDistanceCm);
	bool CaptureSource(AActor* SourceOwner, ATimeCycleSky* Sky,
		const TArray<UPrimitiveComponent*>& Sources, const FTransform& CaptureFrame);
	bool ApplyCapture(AActor* DisplayOwner, UMaterialInterface* Material,
		const UOfficeDistantBackdrop* SourceCapture, const FTransform& DestinationFrame, float NightBrightness);
	bool HasCapture() const;
	void SetActive(bool bNewActive);
	void Reset();
	bool IsActive() const { return bActive; }
	int32 GetCaptureCount() const { return CaptureCount; }
	float GetCaptureMilliseconds() const { return CaptureMilliseconds; }
	FVector GetCaptureOrigin() const { return CaptureOrigin; }
	FTransform GetCaptureFrame() const { return CaptureFrameTransform; }
	TArray<UTextureRenderTarget2D*> GetRenderTargets() const;

private:
	friend struct FOfficeBackdropTransferTestAccess;
	bool SoftenSourceCapture(AActor* SourceOwner);
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Display;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextureRenderTarget2D>> RenderTargets;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> DisplayMaterials;
	FVector CaptureOrigin = FVector::ZeroVector;
	FTransform CaptureFrameTransform = FTransform::Identity;
	int32 CaptureCount = 0;
	float CaptureMilliseconds = 0.f;
	bool bActive = false;
};
