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
	OfficeRecruitmentPanel UMETA(DisplayName = "OfficeRecruitmentPanelWidget"),
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
	// 착수 창구 = 현재 티어 전부를 보여주는 기획 보드 (단일 경로)
	PitchBoard UMETA(DisplayName = "PitchBoardWidget"),
	// 포트폴리오 패널 재사용 컴포넌트 (로드맵 노드 / 조합 셀 — CodexPanel 이 CreateWidget + Configure)
	RoadmapNode UMETA(DisplayName = "RoadmapNodeWidget"),
	PortfolioCell UMETA(DisplayName = "PortfolioCellWidget"),
	// Bubble
	BubbleElement UMETA(DisplayName = "BubbleElementWidget"),
	// ScoreOrb
	ScoreOrbContainer UMETA(DisplayName = "ScoreOrbContainerWidget"),
	// WorldMap
	WorldMapLayer UMETA(DisplayName = "WorldMapLayerWidget"),
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
	// 글로벌 인벤토리 허브 (BUILDING_TRAIT_SYSTEM v1.1 — 특성/직원강화/티켓/스킨/분해/도감 통합)
	InventoryHub UMETA(DisplayName = "InventoryHubWidget"),
	// 자원 획득 +N 플로팅 숫자 (Money 수거 / Brick 도착 시 카운터 옆 팝업)
	FloatingNumber UMETA(DisplayName = "FloatingNumberWidget"),
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

	// 직원 강화 스타포스 모달 (직원창 [강화] → PushPromptClass. Money 비용 + 성공/유지/하락 확률 공개)
	EnhanceStarforceModal UMETA(DisplayName = "EnhanceStarforceModalWidget"),

	// 잠재 재설정 확률표 (직원창 잠재 패널 [확률] → 뷰포트 오버레이. 명함 3종 × 실효 등급 확률)
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

	// 티어 해금 로드맵 팝오버 (관리 패널 칩 / 오피스 밴드 뱃지 → PushPromptClass)
	TierRoadmap UMETA(DisplayName = "TierRoadmapWidget"),
	// 그 레일의 노드 1개 (로드맵이 CreateWidget + Configure)
	TierNode UMETA(DisplayName = "TierNodeWidget"),

	// 미션판 트래커 (체인 종료 후 좌측 상시 리스트 — HUD, AddToViewport)
	GoalTracker UMETA(DisplayName = "GoalTrackerWidget"),
	// 미션판 행 (트래커가 CreateWidget + Configure)
	GoalTrackerRow UMETA(DisplayName = "GoalTrackerRowWidget"),

	// 건물 위 금고 채움 게이지 (InGameLayerWidget 이 풀로 관리 — 길이=금고, 색=감쇠)
	VaultGauge UMETA(DisplayName = "VaultGaugeWidget"),

	// HUD 상단 실시간 수익 칩 (Money 칩 우측 인접 — 유량 상시 노출)
	RevenueRateChip UMETA(DisplayName = "RevenueRateChipWidget"),

	FundsToast UMETA(DisplayName = "FundsToastWidget"),

	// 출시 직후 풀스크린 보상 리빌 (스탬프 런) — LaunchConfirm 닫힘 후 PromptStack push
	LaunchRewardReveal UMETA(DisplayName = "LaunchRewardRevealWidget"),
	// 운영 중 이벤트 레일 반응 토스트 (비평가/SNS, 자동 만료)
	ReviewReactionToast UMETA(DisplayName = "ReviewReactionToastWidget")

};
