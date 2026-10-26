#pragma once
#include<memory>
#include<optional>

#include"IUIScreen.h"
#include"UI.h"
#include"Uiutillity.h"
#include"UIGamePlayScreenUnderBar.h"
#include"UIBuildUtility.h"
#include"CharacterTypes.h"


namespace Item::Master
{
	class ItemMasterIconProvider;
}

namespace Game::World::Storage
{
	class StorageSystem;
}


namespace Item::Runtime
{
	class Inventory;
}

namespace Game::UI::Parts
{
	class UIResourcePanel;
	
}

namespace Game::Object
{
	class ObjectAttributeManager;
}

namespace Game::Build
{
	class BuildingDatabase;
	struct BuildProgressData;
}


namespace Game::UI::GamePlayScreen::Parts
{
	class UIGamePlayScreenTopBar;
	class UIGamePlayScreenUnderBar;
	class UIGamePlayScreenResidentPanel;
	class UIGamePlayScreenBuildPanel;
	class UIBuildDetailPanel;
	class UIBuildingInfoPanel;
	class UIGamePlayFieldDetailPanel;
}

namespace Game::Character
{
	class CharacterSystem;
}

namespace Game::Time
{
	struct GameDateTime;
}

namespace Game::Timer
{

	class GameSpeedController;
}

namespace Game::Agriculture
{
	class AgricultureSystem;
}

namespace Engine::Components
{
	struct TransformId;
}

namespace Game::UI::GamePlayScreen::Parts
{
	struct SelectResident;
}

namespace Game::UI::Screens
{
	//UIに必要な共通サービスをまとめた構造体
	struct UICommonServices
	{
		UIService uiService;
		UIContext uiContext;
		const UIPipeline uiPipe;
		const ViewportScissor viewportScissor;

		UICommonServices(
			const UIService& uiService_,
			const UIContext& uiContext_,
			const UIPipeline& uiPipe_,
			const ViewportScissor& viewportScissor_)
			: uiService(uiService_)
			, uiContext(uiContext_)
			, uiPipe(uiPipe_)
			, viewportScissor(viewportScissor_)
		{
		}
	};


	class UIGamePlayScreen : public IUIScreen
	{
	public:

		//UI生成で必要な共通サービスを引数にとるコンストラクタ
		UIGamePlayScreen(UIService& uiService,
			UIContext& uiContext,
			const UIPipeline& uiPipe,
			const ViewportScissor& viewportScissor);

		~UIGamePlayScreen();
		void LoadResources() override;
		void SetupLayout() override;
		void Initialize() override;
		void Update() override;
		void Draw() override;
		void Exit() override;
		void DrawUIText() override;
		UIHitResult IsHoveringUI() override;


		//UIの部品の生成UIGamePlayScreenこれを、UIManagerクラスにセットする前に、初期化してインスタンス生成する
		void CreateParts(Item::Master::ItemMasterIconProvider& iconProvider,
			Item::Runtime::Inventory& inventory, Game::Character::CharacterSystem& characterSystem, Game::Object::ObjectAttributeManager& objectAttributeManager, Game::Build::BuildingDatabase& buildingDatabase,
			Game::Timer::GameSpeedController& gameSpeedController, Game::Agriculture::AgricultureSystem& agricultureSystem, Game::World::Storage::StorageSystem& storageSystem);

	public:
		//UIで選択している建築アイコンに対応するオブジェクト名を返す
		Game::UI::GamePlayScreen::Parts::SelectedBuildData GetSelectedBuildData() const;
		//UIで選択している建築アイコンに対応するオブジェクト名をクリアする
		void ClearSelectedBuildData();

		//現在クリックされているアンダーバーボタンの情報を返す
		const std::optional<GamePlayScreen::Parts::UnderBarButtonDef>& GetCurrentClickUnderBarButton() const;
		//最後にクリックしたアンダーバーボタンのタイプを返す（毎フレームリセット）
		const std::optional<GamePlayScreen::Parts::UnderBarButtonType>& GetLastClickedUnderBarButtonType() const;

		bool WasUnderBarButtonDeselected() const;

		//建築情報を受け取る
		void GetBuildingInfo(const std::optional<Game::Build::BuildProgressData>& buildProgressData);
		
		//UIにゲーム内での経過時間反映させるため日付情報を取得
		void SetGametime(Game::Time::GameDateTime gameDateTime);

		/**
		* @brief 詳細を表示する畑をここでセットする
		* @param farmerTid 詳細を表示するターゲットの畑のTransformId
		*/
		void SetTargetField(Engine::Components::TransformId fieldTid);

		/**
		* @brief ここでターゲットをクリアする
		*/
		void ClearTargetField();

		std::optional<Game::Character::CharacterId> ConsumeCameraFollowRequest();


	private:
		//レジデントパネルは、アンダーバーの住人ボタンを押したときに表示するので、個別処理で表示非表示切り替える
		void UpdateUnderBarPanel();
		void DrawResidentPanel();
		void DrawUITextResidentPanel();


	private:
		UICommonServices uiCommonServices_;

		std::vector<IUIScreen*> uiParts_;

	private:
		std::unique_ptr<Game::UI::Parts::UIResourcePanel> resourcePanel_;
		std::unique_ptr<Game::UI::GamePlayScreen::Parts::UIGamePlayScreenTopBar> topBar_;
		std::unique_ptr<Game::UI::GamePlayScreen::Parts::UIGamePlayScreenUnderBar> underBar_;
		std::unique_ptr<Game::UI::GamePlayScreen::Parts::UIGamePlayScreenResidentPanel> residentPanel_;
		std::unique_ptr<Game::UI::GamePlayScreen::Parts::UIGamePlayScreenBuildPanel> buildPanel_;
		std::unique_ptr<Game::UI::GamePlayScreen::Parts::UIBuildDetailPanel> buildDetailPanel_;
		std::unique_ptr<Game::UI::GamePlayScreen::Parts::UIBuildingInfoPanel> uiBuildingInfoPanel_;
		std::unique_ptr<Game::UI::GamePlayScreen::Parts::UIGamePlayFieldDetailPanel> uiGamePlayFieldDetailPanel_;


	};
}