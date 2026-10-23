#pragma once

#include "CoreMinimal.h"
#include "WidgetType.generated.h"

UENUM(BlueprintType)
enum class EWidgetType : uint8
{
	None UMETA(DisplayName = "None"),
	UIBase UMETA(DisplayName = "UIBase"),
	Splash UMETA(DisplayName = "SplashWidget"),
	MainMenu UMETA(DisplayName = "MainMenuWidget"),
	InGameMain UMETA(DisplayName = "InGameMainWidget"),
	BrickCollection UMETA(DisplayName = "BrickCollection"),
	ResourceElement UMETA(DisplayName = "ResourceElementWidget"),
	ButtonElement UMETA(DisplayName = "ButtonElementWidget"),
	ConfirmCancel UMETA(DisplayName = "ConfirmCancelWidget"),
	PauseMenu UMETA(DisplayName = "PauseMenuWidget"),
	ResultMenu UMETA(DisplayName = "ResultMenuWidget"),
	BuildOpen UMETA(DisplayName = "BuildOpenWidget"),
	MenuPanel UMETA(DisplayName = "MenuPanelWidget"),
	BuildPanel UMETA(DisplayName = "BuildPanelWidget"),
	BuildEntityCard UMETA(DisplayName = "BuildEntityCardWidget"),
	CardInfo UMETA(DisplayName = "CardInfoWidget"),
	ConstructionResource UMETA(DisplayName = "ConstructionResourceWidget"),
	BuildPlacement UMETA(DisplayName = "BuildPlacementWidget"),
	BuildPlacementPanel UMETA(DisplayName = "BuildPlacementPanelWidget"),
	BuildingManage UMETA(DisplayName = "BuildManageWidget"),
	HQManagePanel UMETA(DisplayName = "HQManagePanelWidget"),
	FactoryPanel UMETA(DisplayName = "FactoryPanelWidget"),
	BuildingSkinCard UMETA(DisplayName = "BuildingSkinCardWidget"),
	BuildingLightCard UMETA(DisplayName = "BuildingLightCardWidget"),
	IconButton UMETA(DisplayName = "IconButtonWidget"),
	// OfficeMap
	OfficeLayer UMETA(DisplayName = "OfficeLayerWidget"),
	OfficeMain UMETA(DisplayName = "OfficeMainWidget"),
	OfficeWorkstationPanel UMETA(DisplayName = "OfficeWorkstationPanelWidget"),
	// 책상 클릭 시 우측 도킹 패널 (직원 배치/강화 + 책상 강화)
	WorkstationInfo UMETA(DisplayName = "WorkstationInfoWidget"),
	OfficeWorkstationCard UMETA(DisplayName = "OfficeWorkstationCardWidget"),
	OfficeDecorationPanel UMETA(DisplayName = "OfficeDecorationPanelWidget"),
	OfficeDecorationCard UMETA(DisplayName = "OfficeDecorationCardWidget"),
	OfficeWallSelection UMETA(DisplayName = "OfficeWallSelectionWidget"),
	OfficeUpgradePanel UMETA(DisplayName = "OfficeUpgradePanelWidget"),
	OfficeRecruitmentCard UMETA(DisplayName = "OfficeRecruitmentCardWidget"),
	OfficeCollectionPanel UMETA(DisplayName = "OfficeCollectionPanelWidget"),
	OfficeProjectCard UMETA(DisplayName = "OfficeProjectCardWidget"),
	// UI Elements
	StatRow UMETA(DisplayName = "StatRowWidget"),
	HQBuildingSlot UMETA(DisplayName = "HQBuildingSlotWidget"),
	ProjectInfoBar UMETA(DisplayName = "ProjectInfoBarWidget"),
	OfficeEmployeeListCard UMETA(DisplayName = "OfficeEmployeeListCardWidget"),
	AlertMark UMETA(DisplayName = "AlertMarkWidget"),
	// Niagara Text Effect (색상별)
	NiagaraTextEffect_Green UMETA(DisplayName = "NiagaraTextEffectWidget_Green"),
	NiagaraTextEffect_Blue UMETA(DisplayName = "NiagaraTextEffectWidget_Blue"),
	NiagaraTextEffect_Red UMETA(DisplayName = "NiagaraTextEffectWidget_Red"),
	NiagaraTextEffect_Yellow UMETA(DisplayName = "NiagaraTextEffectWidget_Yellow"),
	NiagaraTextEffect_Purple UMETA(DisplayName = "NiagaraTextEffectWidget_Purple"),
	// Notification
	NotificationContainer UMETA(DisplayName = "NotificationContainerWidget"),
	NotificationElement UMETA(DisplayName = "NotificationElementWidget"),
	// etc, loading
	Loading UMETA(DisplayName = "LoadingWidget"),
	// 첫 부팅 로딩 화면의 스카이라인 변형 — 가운데 큰 로고 등장 연출 (LoadingMap_Preview 에서 비교)
	TitleSkylineLoading UMETA(DisplayName = "TitleSkylineLoadingWidget"),
	// Effects
	BlindMaskSweep UMETA(DisplayName = "BlindMaskSweepWidget"),
	TextGlitch UMETA(DisplayName = "TextGlitchWidget"),
	// Project Lifecycle Modals
	LaunchConfirm UMETA(DisplayName = "LaunchConfirmWidget"),
	ProjectReport UMETA(DisplayName = "ProjectReportWidget"),
	BoostGamble UMETA(DisplayName = "BoostGambleWidget"),
	CodexPanel UMETA(DisplayName = "CodexPanelWidget"),
	TeamPipItem UMETA(DisplayName = "TeamPipItemWidget"),
	// 기획 보드 타일 (2026-08-22 신설)
	PitchTile UMETA(DisplayName = "PitchTileWidget"),
	// 기획 보드 좌측 세로 티어 레일 탭 (2026-08-28 신설 — 상단 페이저/미터 대체)
	PitchTierTab UMETA(DisplayName = "PitchTierTabWidget"),
	// 착수 창구 = 현재 티어 전부를 보여주는 기획 보드 (단일 경로)
	PitchBoard UMETA(DisplayName = "PitchBoardWidget"),
	// 포트폴리오 패널 재사용 컴포넌트 (로드맵 노드 / 조합 셀 — CodexPanel 이 CreateWidget + Configure)
	RoadmapNode UMETA(DisplayName = "RoadmapNodeWidget"),
	PortfolioCell UMETA(DisplayName = "PortfolioCellWidget"),
	// RecruitmentMap
	RecruitmentResultPanel UMETA(DisplayName = "RecruitmentResultPanelWidget"),
	// Bubble
	BubbleElement UMETA(DisplayName = "BubbleElementWidget"),
	// ScoreOrb
	ScoreOrbContainer UMETA(DisplayName = "ScoreOrbContainerWidget"),
	// WorldMap
	LegacyWorldMapLayer UMETA(DisplayName = "LegacyWorldMapLayerWidget"),
	WorldMapBottom UMETA(DisplayName = "WorldMapBottomWidget"),
	CountryName UMETA(DisplayName = "CountryNameWidget"),
	// WorldMap Production System
	WorldResourcePanel UMETA(DisplayName = "WorldResourcePanelWidget"),
	WorldProductsPanel UMETA(DisplayName = "WorldProductsPanelWidget"),
	TradeOrderBoard UMETA(DisplayName = "TradeOrderBoardWidget"),
	TradeOrderCard UMETA(DisplayName = "TradeOrderCardWidget"),
	WorldFactoryLine UMETA(DisplayName = "WorldFactoryLineWidget"),
	MineLine UMETA(DisplayName = "MineLineWidget"),
	MinePicker UMETA(DisplayName = "MinePickerWidget"),
	MineResourceCard UMETA(DisplayName = "MineResourceCardWidget"),
	CountryDetail UMETA(DisplayName = "CountryDetailWidget"),
	// 국가 상세 정보 탭의 산업별 수요 카드 — UCountrySellRowWidget 의 카드형 변형 (UIE_CountryDemandCard)
	CountryDemandCard UMETA(DisplayName = "CountryDemandCardWidget"),
	CountryRouterCard UMETA(DisplayName = "CountryRouterCardWidget"),
	CountryRouterPanel UMETA(DisplayName = "CountryRouterPanelWidget"),
	ProductListRow UMETA(DisplayName = "ProductListRowWidget"),
	ProductionStartPopup UMETA(DisplayName = "ProductionStartPopupWidget"),
	ProductionCard UMETA(DisplayName = "ProductionCardWidget"),
	// 제작 시작 팝업 A안 — 좌측 주문서 레일 행 + 우측 재료 요구 행 (팝업이 CreateWidget)
	ProductionOrderRow UMETA(DisplayName = "ProductionOrderRowWidget"),
	MaterialReqRow UMETA(DisplayName = "MaterialReqRowWidget"),
	ItemCard UMETA(DisplayName = "ItemCardWidget"),
	ItemTooltip UMETA(DisplayName = "ItemTooltipWidget"),
	// PlayFab/Chat
	LoginPanel UMETA(DisplayName = "LoginPanelWidget"),
	ChatPanel UMETA(DisplayName = "ChatPanelWidget"),
	ChatMessage UMETA(DisplayName = "ChatMessageWidget"),
	// Profile
	ProfileImagePanel UMETA(DisplayName = "ProfileImagePanelWidget"),
	ProfileAvatarTile UMETA(DisplayName = "ProfileAvatarTileWidget"),
	// Ranking & Visit
	RankingPanel UMETA(DisplayName = "RankingPanelWidget"),
	RankingEntryCard UMETA(DisplayName = "RankingEntryCardWidget"),
	RankingPlayerDetail UMETA(DisplayName = "RankingPlayerDetailWidget"),
	VisitModeOverlay UMETA(DisplayName = "VisitModeOverlayWidget"),
	// HQ LevelUp Celebration
	HQLevelUpCelebration UMETA(DisplayName = "HQLevelUpCelebrationWidget"),
	// Shop
	ShopPanel UMETA(DisplayName = "ShopPanelWidget"),
	ShopItemCard UMETA(DisplayName = "ShopItemCardWidget"),
	// 기획 보드 기획안 카드
	OfficeRequestCard UMETA(DisplayName = "OfficeRequestCardWidget"),
	// Event 3-choice modal (뱀서라이크 카드형)
	EventChoicePanel UMETA(DisplayName = "EventChoicePanelWidget"),
	EventChoiceCard UMETA(DisplayName = "EventChoiceCardWidget"),
	// 직원 머리 위 상태 버블
	WorkerBubble UMETA(DisplayName = "WorkerBubbleWidget"),
	// 직원 머리 위 상시 피로 게이지 바 (무드 버블과 별개의 전용 WidgetComponent)
	WorkerFatigueBar UMETA(DisplayName = "WorkerFatigueBarWidget"),
	// 건물 강화 슬롯 (BuildingManagePanel에서 DT 기반 동적 생성)
	UpgradeSlot UMETA(DisplayName = "UpgradeSlotWidget"),
	// WorldMap 물품거래소 (좌측 인벤 카드 + 우측 11국 게이지)
	ProductSellModal UMETA(DisplayName = "ProductSellModalWidget"),
	CountrySellRow UMETA(DisplayName = "CountrySellRowWidget"),
	// ItemCard 변형 — 카드 본체(UIE_ItemCard) 공유, 수량 라벨 위치만 다름
	ItemCardQtyInside UMETA(DisplayName = "ItemCardQtyInsideWidget"),
	ItemCardQtyBelow UMETA(DisplayName = "ItemCardQtyBelowWidget"),
	ItemCardQtyBelowLarge UMETA(DisplayName = "ItemCardQtyBelowLargeWidget"),   // 216 대형(본체 UIE_ItemCard_L) — 보상 리빌
	// 계정 보유 특성 카드 (빌딩관리 특성 탭)
	TraitCard UMETA(DisplayName = "TraitCardSlotWidget"),
	// 자원 획득 +N 플로팅 숫자 (Money 수거 / Brick 도착 시 카운터 옆 팝업)
	FloatingNumber = 116 UMETA(DisplayName = "FloatingNumberWidget"),
	// 미션 시스템 (트래커 카드 + 가이드 펄스 링)
	MissionTracker UMETA(DisplayName = "MissionTrackerWidget"),
	MissionGuideOverlay UMETA(DisplayName = "MissionGuideOverlayWidget"),
	// 코치마크 말풍선 (MissionGuideOverlay 가 소유 ― 꼬리 4방향은 한 장을 회전)
	GuideTooltip UMETA(DisplayName = "GuideTooltipWidget"),
	// 패널 최초 진입 코치마크 오버레이 (미션과 무관 ― 세이브 플래그로 패널당 1회)
	PanelIntroOverlay UMETA(DisplayName = "PanelIntroOverlayWidget"),
	// 제스처 힌트 (손 글리프 + 홀드 링 + 라벨 — 미션 오버레이·오피스 캐치 링이 공유)
	GestureHint UMETA(DisplayName = "GestureHintWidget"),
	// 직원 가챠 확률표 모달
	EmployeeGachaProbabilityPanel UMETA(DisplayName = "EmployeeGachaProbabilityWidget"),
	// 직원 가챠 2D 뽑기 연출 오버레이 (SceneCapture 댄스)
	EmployeeGachaPresentation UMETA(DisplayName = "EmployeeGachaPresentationWidget"),
	// 직원 가챠 멀티 리빌 사이드 사원증 (UIE_EmployeeIdCardMini)
	EmployeeIdCardMini UMETA(DisplayName = "EmployeeIdCardMiniWidget"),
	// 건물 특성 가챠 (MainMap 허브)
	BuildingTraitGachaPanel UMETA(DisplayName = "BuildingTraitGachaPanelWidget"),
	BuildingTraitGachaPresentation UMETA(DisplayName = "TraitGachaPresentationWidget"),
	TraitGachaProbabilityPanel UMETA(DisplayName = "TraitGachaProbabilityWidget"),
	// 건물 스킨 가챠 연출 (특성 패널 재사용, 결과 카드만 스킨 카드)
	BuildingSkinGachaPresentation UMETA(DisplayName = "SkinGachaPresentationWidget"),
	// 특성/스킨 공용 단일 연출 위젯 (NamedSlot 카드 주입). 위 둘은 레거시(통합으로 대체).
	GachaRevealPresentation UMETA(DisplayName = "GachaRevealPresentationWidget"),
	// 뽑기 공개 카드 1장 ― 공개·결과 화면이 뽑은 수만큼 런타임 생성
	GachaRevealCard UMETA(DisplayName = "GachaRevealCardWidget"),
	// 미션 마일스톤(튜토리얼 완료) 보상 리빌 오버레이
	RewardReveal UMETA(DisplayName = "RewardRevealPresentationWidget"),
	// 타이틀 화면 로고 (UIE_TitleLogo — 메인메뉴 자식 BindWidget + 단독 Push 둘 다)
	TitleLogo UMETA(DisplayName = "TitleLogoWidget"),

	// 인접-미소유 부지 위 가격 배지 (UIE_PlotPriceBadge — 잠금+가격, 클릭 시 TryPurchase). InGameLayer 가 풀로 생성.
	PlotPriceBadge UMETA(DisplayName = "PlotPriceBadgeWidget"),

	// 스카이라인 입주 회사 인수 확인 모달 (UCityCompanyInfoWidget — NotAcquired 클릭 시 PushPromptClass)
	CityCompanyInfo UMETA(DisplayName = "CityCompanyInfoWidget"),

	// 스카이라인 입주 회사 관리 패널 (UCityCompanyManageWidget — Milking/Depleted 클릭 시 PushBottomClass)
	CityCompanyManage UMETA(DisplayName = "CityCompanyManageWidget"),

	// 설정창 (MenuPanel → PushPromptClass 중앙 모달. 사운드/그래픽/계정 3탭)
	SettingsPanel UMETA(DisplayName = "SettingsPanelWidget"),

	// 풀스크린 직원창 (좌 로스터 / 중 히어로 / 우 능력치+잠재)
	// 로스터 카드(UIE_EmployeeRosterCard)는 ListView EntryWidgetClass 전용이라 등록 불필요 — 호스트 WBP 가 엔트리 클래스를 직접 들고 있어 GetWidgetClass 조회 경로를 안 탄다
	EmployeeWindow UMETA(DisplayName = "EmployeeWindowWidget"),
	EmployeeLevelUpCard UMETA(DisplayName = "EmployeeLevelUpCardWidget"),

	// 직원 강화 스타포스 모달 (직원창 [강화] → PushPromptClass. Money 비용 + 성공/유지/하락 확률 공개)
	EnhanceStarforceModal UMETA(DisplayName = "EnhanceStarforceModalWidget"),

	// 잠재 재설정 기본 추첨 확률표 (직원창 잠재 패널 [확률] → 뷰포트 오버레이)
	PotentialOddsPanel UMETA(DisplayName = "PotentialOddsWidget"),
	PotentialOddsRow UMETA(DisplayName = "PotentialOddsRowWidget"),

	// 특성 탭 세트 보너스 요약 칩 (BuildingManagePanel SetBonusBand 가 런타임 생성)
	TraitSetChip UMETA(DisplayName = "TraitSetChipWidget"),

	// Office 우측 하단 이벤트 레일 (UOfficeLayerWidget 이 InGameCanvas 에 런타임 생성 — 상태 토스트 + 개발 이벤트 카드 피드)
	OfficeEventRail UMETA(DisplayName = "OfficeEventRailWidget"),

	// 이벤트 레일 전용 컴팩트 상태 토스트 (UOfficeEventRailWidget::AddStatusToast — 다크 슬레이트 칩, 전역 알림 UIE_Notification 과 분리)
	OfficeStatusToast UMETA(DisplayName = "OfficeStatusToastWidget"),

	// 오프라인 정산 모달 + 그 빌딩별 행 (복귀 시 1회 — SaveLoadManager::ConsumePendingOfflineReport 경유)
	OfflineReportModal UMETA(DisplayName = "OfflineReportModalWidget"),
	OfflineGainRow UMETA(DisplayName = "OfflineGainRowWidget"),

	// 자원 전용 미션 보상용 센터 밴드 토스트 (RewardReveal 과 C++ 베이스 공유, 딤/버튼 없이 자동 소멸)
	RewardToast UMETA(DisplayName = "RewardToastWidget"),

	// 미션판 트래커 (체인 종료 후 좌측 상시 리스트 — HUD, AddToViewport)
	GoalTracker UMETA(DisplayName = "GoalTrackerWidget"),
	// 미션판 행 (트래커가 CreateWidget + Configure)
	GoalTrackerRow UMETA(DisplayName = "GoalTrackerRowWidget"),

	// 건물 위 금고 채움 게이지 (InGameLayerWidget 이 풀로 관리 — 길이=금고, 색=감쇠)
	VaultGauge UMETA(DisplayName = "VaultGaugeWidget"),

	// HUD 상단 실시간 수익 칩 (Money 칩 우측 인접 — 유량 상시 노출)
	RevenueRateChip UMETA(DisplayName = "RevenueRateChipWidget"),

	FundsToast UMETA(DisplayName = "FundsToastWidget"),

	EmployeeEquipmentWorkspace UMETA(DisplayName = "EmployeeEquipmentWorkspaceWidget"),
	EmployeeEquipmentPanel UMETA(DisplayName = "EmployeeEquipmentPanelWidget"),
	EmployeeEquipmentCard UMETA(DisplayName = "EmployeeEquipmentCardWidget"),
	EmployeeEquipmentDialog UMETA(DisplayName = "EmployeeEquipmentDialogWidget"),
	// 선택 장비 상세 — 드로어 왼쪽에 붙는 독립 창 (UIE_EquipmentDetailWindow)
	EquipmentDetailWindow UMETA(DisplayName = "EquipmentDetailWindowWidget"),
	// 상세 창이 행 수만큼 찍어내는 부품 — 능력치 변화 한 줄과 고유효과 칩
	EquipmentDeltaRow UMETA(DisplayName = "EquipmentDeltaRowWidget"),
	EquipmentUniqueChip UMETA(DisplayName = "EquipmentUniqueChipWidget"),
	PotentialResetWorkspace UMETA(DisplayName = "PotentialResetWorkspaceWidget"),

	// ===== 라이브옵스 1군 (2026-09-13 메뉴 시트 · 출석 · 미션 · 우편함 · 소식) =====
	AttendancePanel UMETA(DisplayName = "AttendancePanelWidget"),
	AttendanceDayTile UMETA(DisplayName = "AttendanceDayTileWidget"),
	QuestPanel UMETA(DisplayName = "QuestPanelWidget"),
	QuestRow UMETA(DisplayName = "QuestRowWidget"),
	MailboxPanel UMETA(DisplayName = "MailboxPanelWidget"),
	MailRow UMETA(DisplayName = "MailRowWidget"),
	NewsPanel UMETA(DisplayName = "NewsPanelWidget"),
	EventBanner UMETA(DisplayName = "EventBannerWidget"),
	NoticeRow UMETA(DisplayName = "NoticeRowWidget"),
	IndustryRailTab UMETA(DisplayName = "IndustryRailTabWidget"),

	// 도움말 "?" 의 표 창과 그 행 (설계 R2 §6.2)
	HelpTablePanel UMETA(DisplayName = "HelpTablePanelWidget"),
	HelpTableRow UMETA(DisplayName = "HelpTableRowWidget"),

	// 패키지 카드의 구성품 한 줄
	ShopBundleRow UMETA(DisplayName = "ShopBundleRowWidget"),

	// 패키지 전용 카드 — 상단 아트 밴드 + 구성품 목록 3단
	ShopPackageCard UMETA(DisplayName = "ShopPackageCardWidget"),

	// 사이드 컨텐츠 — 출장 모달과 그 편성 레일의 직원 칩
	BusinessTripPanel UMETA(DisplayName = "BusinessTripPanelWidget"),
	BusinessTripCrewChip UMETA(DisplayName = "BusinessTripCrewChipWidget"),

	// WorldMap 전략 지도 테이블 (2026-09-24). 맨 끝에 붙인다 ― 중간에 끼우면 아래쪽 암묵값이 밀려
	// FloatingNumber = 116 과 값이 겹친다. 구 WorldMapLayer(현 LegacyWorldMapLayer) 는 구 경로가 살아 있어 그대로 둔다.
	WorldMapPanel = 180 UMETA(DisplayName = "WorldMapPanelWidget"),

	// 사이드 컨텐츠 공통 축 — 메뉴 "도전" 그룹 타일 3개의 목적지 (2026-09-25). 맨 끝 규칙 동일.
	// 스카이라인은 HUD 를 덮는 전용 화면(메뉴 스택), 나머지 둘은 라이브옵스 셸 모달(프롬프트 스택).
	SkylineScreen UMETA(DisplayName = "SkylineScreenWidget"),
	FusionProjectPanel UMETA(DisplayName = "FusionProjectPanelWidget"),

	// 기술 연구소는 건물 축이라 **그 건물 사무실(오피스 도크)에서만** 연다 (2026-09-25 사용자 확정)
	TrainingCenterPanel UMETA(DisplayName = "TrainingCenterPanelWidget"),

	// 스카이라인이 런타임에 CreateWidget 하는 자식 3종. 화면 자체는 위 SkylineScreen 이다 — 중복 추가 금지.
	// 출전 회사 선택은 화면 안 오버레이라 WBP 에 박히고, 그 안의 행만 동적이라 여기 등록한다.
	SkylineFloorEntry UMETA(DisplayName = "SkylineFloorEntryWidget"),
	SkylineCrewRow UMETA(DisplayName = "SkylineCrewRowWidget"),
	SkylineCompanyRow UMETA(DisplayName = "SkylineCompanyRowWidget"),

	// 기술 연구소 성과 미리보기·실제 지급 결과 칩
	TrainingRewardChip UMETA(DisplayName = "TrainingRewardChipWidget"),
	FusionCard UMETA(DisplayName = "FusionCardWidget"),
	FusionSlot UMETA(DisplayName = "FusionSlotWidget"),
	FusionBuildingRow UMETA(DisplayName = "FusionBuildingRowWidget"),

	PanelHeader UMETA(DisplayName = "PanelHeaderWidget"),
	ModalFrame UMETA(DisplayName = "ModalFrameWidget"),
	HelpAxisTab UMETA(DisplayName = "HelpAxisTabWidget"),

	// 기술 연구소 좌측 레일 한 칸 — 현재 Lv 의 기술 목록을 패널이 런타임에 채운다.
	// 구 사다리 칸(UIE_CourseStep)을 되살려 쓴다 — 같은 역할이라 부품을 새로 만들지 않았다.
	CourseStep UMETA(DisplayName = "CourseStepWidget"),

	// 출장 카드의 드랍 칩. 클래스는 연구소 칩(UTrainingRewardChipWidget)을 그대로 쓰고
	// WBP 만 카드 840x208 에 맞춘 변형이다 — 연구소 칩의 전역 규격은 건드리지 않는다.
	TripRewardChip UMETA(DisplayName = "TripRewardChipWidget"),

	// 공장 홀드 2배속의 UI 층 — 공장에 붙는 월드 오로라 위에 겹치는 추진 기둥 + 배수 표기.
	// 줌을 타지 않아야 해서 월드가 아니라 위젯이다.
	FactoryBoost UMETA(DisplayName = "FactoryBoostWidget"),
	DevelopmentStart UMETA(DisplayName = "DevelopmentStartWidget"),
	CityOpeningOverlay UMETA(DisplayName = "CityOpeningOverlayWidget"),
	OfficeRecruitmentModal UMETA(DisplayName = "OfficeRecruitmentModalWidget"),
	EquipmentGachaPanel UMETA(DisplayName = "EquipmentGachaPanelWidget"),

	// 본사 운영 현황 업종 카드 / 최고 레벨 해금 기록 칸 — 패널이 DT 행 수만큼 런타임 생성
	HQIndustryCard UMETA(DisplayName = "HQIndustryCardWidget"),
	HQLevelStep UMETA(DisplayName = "HQLevelStepWidget"),
	WorkstationPromotion UMETA(DisplayName = "WorkstationPromotionWidget"),
	// 본사 레벨업 보상 카드의 보상 칸 — 다음 레벨 보상 수만큼 런타임 생성
	HQRewardSlot UMETA(DisplayName = "HQRewardSlotWidget"),
	// 잠재능력 등급이 오른 재설정 직후 화면 전체에 한 번 뜨는 연출
	PotentialTierUpOverlay UMETA(DisplayName = "PotentialTierUpOverlayWidget"),
	OfficeTVScreen UMETA(DisplayName = "OfficeTVScreenWidget"),
	// 사무실 상단 바 분야 칸이 목표에 닿을 때 칸 아래에서 빠져나오는 「목표 달성」 서랍 탭
	StageGoalTab UMETA(DisplayName = "StageGoalTabWidget"),
	// 선택한 미소유 부지의 조건·가격·인수 액션을 하단 카드에 표시한다.
	PlotPurchasePanel UMETA(DisplayName = "PlotPurchasePanelWidget"),

	// 사무실 직원 패널 (좌 사원증 레일 + 우 840 패널, 3D 사무실 배경 — BottomStack push. 직원 탭·도크 「직원」·책상 패널 진입)
	OfficeEmployeePanel UMETA(DisplayName = "OfficeEmployeePanelWidget"),
	// 사무실 직원 패널 성장 탭 능력치 행 (패널이 6개를 런타임 생성)
	OfficeEmployeeStatRow UMETA(DisplayName = "OfficeEmployeeStatRowWidget"),
	// 사무실 직원 패널 ★ 강화 카드 (EnhanceStarforceModalWidget 의 600 카드 WBP — CardHost 자식, 비활성)
	OfficeEnhanceCard UMETA(DisplayName = "OfficeEnhanceCard"),
	// 사무실 직원 패널 왼쪽 레일 사원증 카드 (패널이 사무실/대기 두 묶음을 런타임 생성)
	EmployeeBadgeCard UMETA(DisplayName = "EmployeeBadgeCardWidget"),
	// 사무실 직원 패널 잠재 탭 전용 워크스페이스 (PotentialResetWorkspaceWidget 의 한 화면·스크롤 없는 변형 WBP)
	OfficePotentialWorkspace UMETA(DisplayName = "OfficePotentialWorkspace"),

	// 월드맵 이름표 한 칸·지도 말풍선 버튼 (해외 생산 2차)
	MapIndustryTag UMETA(DisplayName = "MapIndustryTagWidget"),
	MapActionBubble UMETA(DisplayName = "MapActionBubbleWidget"),

	// 국가 서랍 한 장의 한 줄 행 — 라인/강화는 기존 클래스의 WBP 변형, 정보 행은 UDrawerInfoRowWidget
	WorldFactoryLineRow UMETA(DisplayName = "WorldFactoryLineRowWidget"),
	MineLineRow UMETA(DisplayName = "MineLineRowWidget"),
	UpgradeRow UMETA(DisplayName = "UpgradeRowWidget"),
	DrawerInfoRow UMETA(DisplayName = "DrawerInfoRowWidget"),
	// 뽑기 공개 상자 1개 ― 공개 화면이 만들어 착지·테이프 가르기·뚜껑 열림을 그린다
	GachaRevealBox UMETA(DisplayName = "GachaRevealBoxWidget"),
	GachaOddsCell UMETA(DisplayName = "GachaOddsCellWidget"),
	// 제조업 제품 선택 전용 변형 ― 같은 C++(UPitchTileWidget/UPitchTierTabWidget), 다른 WBP
	PitchTileProduct UMETA(DisplayName = "PitchTileProductWidget"),
	PitchTierTabCompact UMETA(DisplayName = "PitchTierTabCompactWidget"),
	// 국가 서랍 무역항 「보관 중인 완제품」 4열 격자의 한 칸
	DrawerGoodsTile UMETA(DisplayName = "DrawerGoodsTileWidget"),
	// 국가 서랍 공장·광산 라인의 다이아 즉시 완료 확인 창
	InstantFinishConfirm UMETA(DisplayName = "InstantFinishConfirmWidget")

};

// FloatingNumber = 116 · WorldMapPanel = 180 은 값이 고정돼 있다 ― 그 앞에 값을 끼우면 암묵값이 고정값과 겹쳐
// DT_WidgetClass 행이 남의 이름으로 저장된다(2026-10-17 실측: 월드맵 패널 행이 BusinessTripCrewChip 으로 바뀜). 새 값은 맨 끝에.
static_assert(static_cast<uint8>(EWidgetType::TraitCard) < static_cast<uint8>(EWidgetType::FloatingNumber),
	"EWidgetType: FloatingNumber(=116) 앞 암묵값이 겹친다 ― 새 값은 enum 맨 끝에 붙일 것");
static_assert(static_cast<uint8>(EWidgetType::BusinessTripCrewChip) < static_cast<uint8>(EWidgetType::WorldMapPanel),
	"EWidgetType: WorldMapPanel(=180) 앞 암묵값이 겹친다 ― 새 값은 enum 맨 끝에 붙일 것");
