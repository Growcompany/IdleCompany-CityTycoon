#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CGDevSettings.generated.h"

/** 게임 시작 모드 (개발 편의 — Project Settings > Game > CG Dev 에서 선택) */
UENUM(BlueprintType)
enum class ECGStartMode : uint8
{
	// 신규 게임 시 오프닝 미션 체인부터 (정식 동작)
	FullTutorial UMETA(DisplayName = "처음부터 튜토리얼"),
	// 오프닝 체인 진입 차단 — 미션 없이 바로 일반 플레이
	SkipTutorial UMETA(DisplayName = "튜토리얼 스킵"),
	// 스킵 + 자원 듬뿍(Rich) + 해금 (개발 샌드박스)
	Sandbox      UMETA(DisplayName = "무한모드 (스킵+자원+해금)"),
	// 스킵 + "한동안 플레이한 세이브" 재현 (부지/빌딩/직원/이력/생산). 매 실행마다 재시드.
	MidGamePreset UMETA(DisplayName = "진행 상태 프리셋 (중반부터)")
};

/**
 * 개발 시작 모드 설정 (UDeveloperSettings).
 * Project Settings > Game > "CG Dev" 섹션에 드롭다운으로 노출되고 DefaultGame.ini 에 저장된다.
 * 런타임에서 GetEffectiveStartMode() 로 읽는다 — **Shipping 빌드에선 항상 FullTutorial 강제**
 * (dev 설정 무력화 → 실제 플레이어 세이브/진행 보호).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "CG Dev (개발 시작 모드)"))
class COMPANYGROWTHRENEWAL_API UCGDevSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "시작 모드")
	ECGStartMode StartMode = ECGStartMode::FullTutorial;

	// 임의 미션부터 시작 (개발용 — 매번 M1부터 안 가도 됨). 비우면 StartMode 기본 동작.
	// 드롭다운은 DT_Mission RowName 을 읽어 채운다 (GetMissionIDOptions). StartMode 가 FullTutorial 이 아닐 때만 적용.
	UPROPERTY(EditAnywhere, config, Category = "시작 모드",
		meta = (DisplayName = "시작 미션 (Mn 점프 — 비우면 미사용)", GetOptions = "GetMissionIDOptions"))
	FName DevStartMissionID = NAME_None;

	// 위 미션의 선행 월드상태(자원 듬뿍 + 빌딩 존재 체크)를 자동 시드할지.
	UPROPERTY(EditAnywhere, config, Category = "시작 모드",
		meta = (DisplayName = "선행 상태 자동 시드"))
	bool bSeedPrereqsForStartMission = true;

	// 무한모드일 때 적용할 DT_TestResourceScenarios Row (자원 듬뿍). 비우면 자원 미적용.
	// 이 시나리오의 MarketCap 값이 국가/콘텐츠 시총 게이트 개방을 담당한다 (= "전체 해금" 핵심).
	// GetResourceScenarioOptions 가 DT 행 이름을 읽어 드롭다운으로 채운다 — 직접 타이핑 대신 선택.
	UPROPERTY(EditAnywhere, config, Category = "무한모드",
		meta = (DisplayName = "자원 시나리오 (선택)",
			GetOptions = "GetResourceScenarioOptions",
			ToolTip = "Rich=자원 만렙 · Default=적당 · Poor=빈곤 · Empty=0. 비우면 자원 미적용. (DT_TestResourceScenarios 행)"))
	FName SandboxResourceScenario = TEXT("Rich");

	// 무한모드일 때 강제할 HQ 레벨 (HQ 게이트 콘텐츠 해금용)
	UPROPERTY(EditAnywhere, config, Category = "무한모드", meta = (ClampMin = "1",
		DisplayName = "HQ 레벨 강제",
		ToolTip = "무한모드 진입 시 본사(HQ) 레벨을 이 값으로 강제 — HQ 레벨 게이트 콘텐츠 해금용."))
	int32 SandboxHQLevel = 20;

	// 진행 상태 프리셋 행 (DT_DevProgressPreset). MidGamePreset 모드일 때만 적용.
	// GetProgressPresetOptions 가 DT 행 이름을 읽어 드롭다운으로 채운다 — 행을 늘려도 코드 변경 0.
	UPROPERTY(EditAnywhere, config, Category = "진행 상태 프리셋",
		meta = (DisplayName = "프리셋 (선택)",
			GetOptions = "GetProgressPresetOptions",
			ToolTip = "Early=초반 · Mid=중반 · Late=후반(대기업). 비우면 프리셋 미적용. (DT_DevProgressPreset 행)"))
	FName ProgressPresetRow = TEXT("Mid");

	// 부스/시연 빌드 — 관람객이 교대해도 앞사람 진행이 남지 않도록 매 실행을 신규 게임으로 만든다.
	// StartMode 와 직교(둘 다 적용됨) — 와이프 후 어느 지점에서 시작할지는 StartMode 가 정한다.
	UPROPERTY(EditAnywhere, config, Category = "부스 시연",
		meta = (DisplayName = "매 실행 세이브 초기화",
			ToolTip = "앱을 껐다 켤 때마다 세이브와 직원 초상화를 지우고 처음부터 시작한다. Shipping 빌드에선 무시된다."))
	bool bWipeSaveOnLaunch = false;

	// 실효 시작 모드 — Shipping 은 항상 FullTutorial. 그 외 빌드만 설정값을 따른다.
	static ECGStartMode GetEffectiveStartMode();

	// 실효 와이프 여부 — Shipping 은 항상 false. 켠 채로 출시해도 플레이어 세이브가 날아가지 않게 하는 안전장치.
	static bool ShouldWipeSaveOnLaunch();

	// DevStartMissionID 드롭다운 소스 — DT_Mission RowName 목록 (에디터 전용, GetOptions meta 경유).
	// 첫 항목은 빈 문자열("미사용"). 미션이 추가되면 CSV 리임포트만으로 드롭다운에 반영(코드 변경 0).
	UFUNCTION()
	TArray<FString> GetMissionIDOptions() const;

	// SandboxResourceScenario 드롭다운 소스 — DT_TestResourceScenarios RowName 목록 (에디터 전용, GetOptions meta 경유).
	// 첫 항목은 빈 문자열("미적용"). 시나리오가 추가되면 CSV 리임포트만으로 드롭다운에 반영(코드 변경 0).
	UFUNCTION()
	TArray<FString> GetResourceScenarioOptions() const;

	// ProgressPresetRow 드롭다운 소스 — DT_DevProgressPreset RowName 목록 (에디터 전용, GetOptions meta 경유).
	UFUNCTION()
	TArray<FString> GetProgressPresetOptions() const;

	// Project Settings 의 "Game" 카테고리 아래로 묶는다 (기본값은 Plugins)
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
