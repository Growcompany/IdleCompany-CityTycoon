#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Table/GachaBoxVisualTable.h"
#include "GachaRevealBoxWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class USizeBox;
class UTableManagerSubsystem;
class UTexture2D;

// 시안 #mkL·#mkR·#mkT 와 상자 기준점(.bpos 원점 = 던지기·눌림·흔들림의 축)
enum class EGachaSeam : uint8 { Left, Right, Top, Origin };

namespace GachaRevealBoxLayout
{
	// 날개 프레임 = 날개 자기 시간 t = i/10 표본(시안 bake/bake_box.py), 0 = 닫힘
	constexpr int32 FlapFrameCount = 11;
	int32 FlapFrameIndex(float Alpha);
	FVector2D SeamRatio(const FGachaBoxVisualRow& Row, EGachaSeam Which);
}

// 뽑기 공개 상자 1개(WBP UIE_GachaRevealBox). 시안 상자를 구운 층 그림을 겹쳐 그린다.
// desired 크기 = 행의 LayerSize ― 자동 크기 캔버스 슬롯에 정렬 = GetSeamPoint(Origin)/LayerSize 로 놓으면 위치가 상자 기준점이 되고,
// 렌더 변형 축(위젯 자신의 피벗)도 SetBoxType 이 기준점으로 맞춘다
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API UGachaRevealBoxWidget : public UCommonUserWidget
{
	GENERATED_BODY()

	friend struct FGachaRevealBoxTestAccessor;

public:
	// 그 종류의 텍스처를 동기 로드하고 닫힌 채 쉬는 상태로 돌린다. 행이 없으면 TableManager 가 Warning 을 남기고 상자는 빈 채로 남는다
	void SetBoxType(EGachaBoxType InBoxType);
	// 0 = 닫힘(닫힌 그림 한 장), 0 초과 = 층 그림 + 앞·뒤 날개 프레임 round(Alpha*10)
	void SetLid(float Alpha);
	// Color = linear. 0 초과면 닫힌 동안에도 층 그림으로 바뀐다(빛은 날개 밑에 깔린다)
	// SetBoxType 뒤에 부를 것 ― SetBoxType 이 빛 세기를 0 으로 되돌린다
	void SetInnerGlow(FLinearColor Color, float Opacity);
	// 위젯 로컬 좌표 ― 화면 좌표는 GetCachedGeometry().LocalToAbsolute 로(렌더 변형 포함)
	FVector2D GetSeamPoint(EGachaSeam Which) const;

	bool HasBoxVisual() const { return bHasVisual; }
	EGachaBoxType GetBoxType() const { return BoxType; }
	FVector2D GetLayerSize() const { return LayerSize; }
	int32 GetFlapFrame() const { return FlapFrame; }

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> LayerBox;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ClosedImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BodyImage;
	// 브러시 = WBP 의 M_UI_GachaBoxGlow(더하기) ― 런타임에 상자 종류의 빛 그림·색·세기만 넣는다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GlowImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> FlapBackImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> FlapFrontImage;

private:
	const UTableManagerSubsystem* ResolveTables() const;
	void ClearVisual();
	void ApplyFlapFrames();
	void RefreshLayers();
	void ApplyGlowParameters();

	EGachaBoxType BoxType = EGachaBoxType::EquipmentNormal;
	bool bHasVisual = false;
	FVector2D LayerSize = FVector2D::ZeroVector;
	FVector2D SeamRatios[4] = {};

	float LidAlpha = 0.f;
	int32 FlapFrame = 0;
	FLinearColor GlowColor = FLinearColor::White;
	float GlowOpacity = 0.f;

	// 프레임을 바꿀 때마다 로드하지 않도록 상자 한 종류분을 쥐고 있는다(공개 화면이 떠 있는 동안만)
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ClosedTexture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BodyTexture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GlowTexture;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> FlapFrontTextures;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> FlapBackTextures;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	// 테스트만 넣는다 ― 게임에서는 늘 GetGameInstance 의 TableManager
	TWeakObjectPtr<const UTableManagerSubsystem> TablesOverride;
};
