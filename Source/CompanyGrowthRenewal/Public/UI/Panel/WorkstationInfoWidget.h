// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Office/WorkstationEquipmentText.h"
#include "WorkstationInfoWidget.generated.h"

class UImage;
class UButton;
class UButtonWidget;
class UCloseButtonWidget;
class UCommonTextBlock;
class UListView;
class UUpgradeSlot;
class UBorder;
class UDisciplineRadarWidget;
class UEmployeeRosterCardWidget;
class AWorkstationActorBase;
class UEmployeeManager;

/**
 * 책상 도크 (우측 고정, 다크 슬레이트) — 책상 하드웨어 강화 + 이 책상의 착석 관리.
 *
 * 착석/자리 비우기는 도크가 소유하고, 육성(스탯/강화)만 UI_EmployeeWindow(직원창)에 남는다.
 * 구성: 헤더("책상" + 책상이름·Lv + X) / 책상 카드(DeskUpgradeSlot 재사용) + 장비 블록
 *      / 근무자 칩 카드(점유 시만 — 얼굴+이름+★ + [직원창] 링크 + [자리 비우기])
 *      / 대기 직원 목록(빈 좌석일 때만, 탭 = 즉시 착석) / 안내 캡션.
 * 목업 SOT = docs/05_UI/EmployeeWindow_MOCKUP.html 스크린 C (px×2 = 2560 캔버스)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorkstationInfoWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Workstation")
	void SetWorkstationData(AWorkstationActorBase* Workstation);

	UFUNCTION(BlueprintCallable, Category = "Workstation")
	void SetEmployeeData(int32 EmployeeID);

	// 미션 가이드 스텁 — 구 도크의 [스탯 관리]/첫 [+] 타겟은 직원창 이관으로 소멸(M9 재배선은 직능 SDD 작업).
	// MissionManagerSubsystem 호출 계약 유지용(nullptr = 가이드 링 생략).
	UWidget* GetOccupantManageStatsButton() const { return nullptr; }
	UWidget* GetFirstStatPlusButton() const { return nullptr; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// ========== 헤더 ==========

	// "기본 책상 · Lv.2" (DT_WorkstationCard DisplayName + 세팅 레벨)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeaderSubText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* CloseButton;

	// 배경 클릭 캐처 (도크 밖 클릭 = 닫기) — 풀스크린 투명 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// ========== 책상 카드 ==========

	// 컴퓨터 세팅 강화 슬롯 (재사용 UUpgradeSlot — 아이콘/레벨/현재→다음 작업 속도/비용/버튼+VFX)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUpgradeSlot* DeskUpgradeSlot;

	// ========== 근무자 칩 카드 (점유 시만, 빈 책상 = Collapsed) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* WorkerCard;

	// 점유 직원 신원 = 직원창 로스터 카드 재사용. 초상·이름·Lv·★·종합·등급 젬을 한 부품이 담당하므로
	// 개별 위젯(WorkerFaceImage/WorkerNameText/…)은 폐기했다 — 같은 직원이 세 화면에서 갈라질 수 없게.
	// ⚠ 트리 인스턴스 이름도 OccupantCard 여야 한다(에셋명으로 두면 이름 불일치로 silent null).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEmployeeRosterCardWidget* OccupantCard;

	// ===== 근무자 서류 — 특화/연혁만 남는다 (신원은 OccupantCard 소관) =====

	// 특화 = 기존 직능 레이더 재사용(작게). 값/등급색 코드 주입, 직원창 DisciplineCard 와 동일 규칙
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineRadarWidget* OccupantRadar;

	// "그래픽 특화" (argmax 직능, enum DisplayName)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantSpecText;

	// 주력/보조 직능 값 (라벨 "주력"/"보조"는 트리 정적)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantLeadValue0;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantLeadValue1;

	// 연혁: 사번/입사일/근속 (등급은 위 배지)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantNoText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantHiredText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantTenureText;

	// [직원창] → EWidgetType::EmployeeWindow PushPrompt (도크는 아래 유지 → 닫으면 복귀)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EmployeeWindowButton;

	// [자리 비우기] — UnseatEmployee 호출. 확인 모달 없음(되돌리기가 바로 아래 목록 1탭)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* UnseatButton;

	// ========== 장비 블록 (강화 슬롯 아래 인셋 플레이트) ==========

	// 플레이트 셸 = SDF 2층(채움/키라인). 절차식 RoundedBox 는 다크 위에서 코너가 얼룩지고
	// 모바일 다운스케일에서 키라인이 번진다 — UI_STYLE_CATALOG "SDF 채택 기준".
	// Chip 마스터는 Wpx/Hpx 베이크라 높이가 변하는 이 블록에서는 C++ 가 실크기를 주입한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* EquipBlockBG;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* EquipBlockLine;

	// 최대 레벨이면 아이콘+이름+부연 행만 접는다. 요약 줄은 남는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* EquipNextRow;

	// "Lv.5 강화 시 추가" — 변화가 언제·어떤 성격으로 일어나는지. 품목명 위 눈썹 줄
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* EquipNextLead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* EquipNextIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* EquipNextName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* EquipNextDetail;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* EquipSummaryText;

	// 품목별 아이콘 — 디자이너가 Class Defaults 에서 5종 지정(키보드/평면·커브드·세로 모니터/본체).
	// WBP 에 직렬화되는 소프트 참조라 쿠커가 따라간다 — 별도 폴더 등록 불필요.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TMap<EWorkstationEquipItem, TSoftObjectPtr<UTexture2D>> EquipIcons;

	// ========== 벤치 섹션 (빈 좌석일 때만) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* BenchSection;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* BenchHeadText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UListView* BenchListView;

	// 대기 0명 안내 + [채용하기]
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* BenchEmptyBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* RecruitButton;

	// 안내 캡션 — 빈 좌석 유무로 문구가 갈린다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* NoteText;

private:
	UPROPERTY()
	AWorkstationActorBase* CurrentWorkstation;

	// 착석 직원 ID (-1이면 빈 책상)
	int32 CurrentEmployeeID = -1;

	// WBP 가 저작한 강화 버튼 라벨. 최대 레벨 「변경」 모드에서 바꿔 쓰고 되돌릴 때 필요하다.
	FText DefaultUpgradeButtonText;

	UPROPERTY()
	UEmployeeManager* EmployeeManager;

	void UpdateUI();

	// 책상 실점유 ↔ CurrentEmployeeID 양방향 동기화 (직원창 자리비우기/해고/자동착석이 가려진 채로 바꿔도 칩 일치)
	void SyncOccupancyFromWorkstation();

	// 헤더 서브 + DeskUpgradeSlot 갱신 (직원 유무와 무관, 항상 표시)
	void UpdateDeskSection();

	// 근무자 칩 채우기 (빈 책상이면 카드째 Collapsed)
	void UpdateWorkerCard();

	// 강화 슬롯 아래 장비 블록 갱신 (현재 목록 + 다음 단계 변화)
	void UpdateEquipBlock();

	// 장비 플레이트 SDF 층에 실제 픽셀 크기 주입 (장비 행이 접히면 높이가 바뀐다)
	void UpdateEquipPlateMaterialSize();

	FVector2D LastEquipPlateSize = FVector2D::ZeroVector;

	// 벤치 섹션 표시/갱신. 빈 좌석 없으면 섹션째 Collapsed
	void UpdateBenchSection();

	// 리스트 엔트리 생성 시 클릭 델리게이트 연결 (재활용 엔트리 대비 매번 재바인딩)
	void OnBenchEntryGenerated(UUserWidget& EntryWidget);

	UFUNCTION()
	void OnBenchCardClicked(int32 EmployeeID);

	void OnUnseatClicked();
	void OnRecruitClicked();

	void FocusCameraOnDesk();

	// 점유 직원 워커에 흰색 선택 오버레이 on/off (CurrentEmployeeID 기준, 워커는 OfficeGameMode 에서 조회)
	void SetOccupantHighlight(bool bHighlighted);

	// ===== 핸들러 =====

	// UUpgradeSlot 의 강화 버튼 클릭 → 책상 세팅 레벨 업그레이드
	void OnDeskUpgradeClicked();

	// [직원창] → EmployeeWindow 푸시 (OfficeMainWidget::OnEmployeeButtonClicked 와 동일 경로)
	void OnEmployeeWindowClicked();

	// EmployeeManager OnEmployeeStatsChanged 구독 핸들러 (키 필터 — 점유 직원이면 칩 부분 갱신)
	void HandleEmployeeStatsChanged(int32 EmployeeID);

	// EmployeeManager OnEmployeeRosterChanged 구독 핸들러 (착석/해제 변동 → 점유 재동기화, 패널 가려져도 동작)
	void HandleRosterChanged();

	UFUNCTION()
	void OnCloseDelegate();

	// 배경 버튼 클릭 = 패널 닫기 (CloseButton 과 동일)
	UFUNCTION()
	void OnBackgroundClicked();
};
