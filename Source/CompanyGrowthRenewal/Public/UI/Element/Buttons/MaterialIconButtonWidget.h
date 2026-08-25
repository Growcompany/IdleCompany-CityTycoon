// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MaterialIconButtonWidget.generated.h"

class UButton;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMaterialIconButtonClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMaterialIconButtonHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMaterialIconButtonUnhovered);

/**
 * Material 기반 아이콘 버튼 위젯
 * - Material에 통합된 아이콘으로 테두리/그라데이션과 자연스럽게 연결
 * - Dynamic Material Instance로 런타임에 아이콘 텍스처 변경 가능
 * - Widget Blueprint에서 MainButton 이름의 Button 위젯과 바인딩
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMaterialIconButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 버튼 클릭 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Material Icon Button|Events")
	FOnMaterialIconButtonClicked OnClicked;

	// 버튼 호버 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Material Icon Button|Events")
	FOnMaterialIconButtonHovered OnHovered;

	// 버튼 언호버 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Material Icon Button|Events")
	FOnMaterialIconButtonUnhovered OnUnhovered;

	// 아이콘 텍스처 (에디터/스폰 시 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Icon Button", meta = (ExposeOnSpawn = "true"))
	UTexture2D* IconTexture;

	// Material의 텍스처 파라미터 이름 (기본값: "ButtonImage")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Icon Button|Advanced")
	FName TextureParameterName = FName("ButtonImage");

	// 버튼 이미지 크기 (0,0이면 기본 크기 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Icon Button", meta = (ExposeOnSpawn = "true"))
	FVector2D ImageSize = FVector2D::ZeroVector;

	// 런타임에 이미지 크기 변경
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button")
	void SetImageSize(FVector2D NewSize);

	// 런타임에 아이콘 변경
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button")
	void SetIcon(UTexture2D* NewTexture);

	// 아이콘 비동기 로드 (TSoftObjectPtr용)
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button")
	void SetIconFromSoft(TSoftObjectPtr<UTexture2D> SoftIcon);

	// 버튼 활성화/비활성화
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button")
	void SetButtonEnabled(bool bEnabled);

	// 버튼 활성화 상태 확인
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button")
	bool IsButtonEnabled() const;

	// Dynamic Material 직접 접근 (고급 사용자용)
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button|Advanced")
	UMaterialInstanceDynamic* GetDynamicMaterial() const { return DynamicMat; }

	// Material의 스칼라 파라미터 설정
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button|Advanced")
	void SetScalarParameter(FName ParameterName, float Value);

	// Material의 벡터 파라미터 설정
	UFUNCTION(BlueprintCallable, Category = "Material Icon Button|Advanced")
	void SetVectorParameter(FName ParameterName, FLinearColor Value);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Widget Blueprint에서 "MainButton"이라는 이름의 Button과 자동 바인딩
	UPROPERTY(meta = (BindWidget))
	UButton* MainButton;

private:
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMat;

	// 원본 Material 캐싱 (재생성 방지)
	UPROPERTY()
	UMaterialInterface* CachedBaseMaterial;

	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleButtonUnhovered();

	// Dynamic Material 설정
	void SetupDynamicMaterial();

	// 버튼 스타일에 Dynamic Material 적용
	void ApplyDynamicMaterialToButton();
};
