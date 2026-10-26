#pragma once

#include<memory>
#include<vector>
#include<optional>
#include<DirectXMath.h>
#include"IScene.h"
#include"CharacterUpdateContext.h"
#include"Engine/FrameContext.h"
#include"CharacterTypes.h"

struct ChunkUpdateResult;

class HealthManager;

namespace Game::Timer
{
	class GameTimeSystem;
}

namespace World::Runtime
{
	class WorldItemDropManager;
	class WorldItemDropVisualSystem;
}

namespace Game::World
{
	class WorldObjectSystem;
	class WorldObjectDestroySystem;
}

namespace Game::UI
{
	class HpBarController;
}

namespace HP::Render
{
	class HPBarRenderer;
}

namespace Game::Character
{
	class CharacterSystem;
	struct CharacterEngineServices;
}

namespace Combat
{
	class CombatSystem;

	namespace DamageData
	{
		class DamageManager;
	}
}

namespace Engine
{
	class Engine;
}

namespace Item::Runtime
{
	class ItemSystem;
}

namespace Item::Master
{
	class ItemMasterIconProvider;
}

namespace Game::Character::Presentation
{
	class CharacterRenderBridge;
}

namespace Game
{
	namespace Init
	{
		class EntityInitializer;
	}

	namespace Object
	{
		class ObjectAttributeManager;
	}

	namespace UI
	{
		class HpBarController;
	}

	namespace World
	{
		namespace Drop
		{
			class WorldItemDropSystem;
		}

		namespace Storage
		{
			class StorageSystem;
		}
	}

	namespace Build
	{
		class BuildingDatabase;
		class BuildSystem;
	}

	namespace Agriculture
	{
		class AgricultureSystem;
	}
}


namespace Game::Scene
{
	class GameplayScene : public Engine::Scene::IScene
	{
	public:
		GameplayScene(Engine::Engine& engine);
		~GameplayScene();
		//シーンの初期化、更新、描画、終了処理の純粋仮想関数
		//ここで一気にファイルから各種データ初期化
		void Initialize() override;
		//シーンの更新処理
		void Update(Engine::FrameContext& frameContext) override;
		//シーンの描画処理
		void Draw() override;
		//シーンの終了処理
		void Finalize() override;


	private:

		void InitializeServices();

		void InitHpBar();

		Engine::FrameContext BuildFrameContext(float deltaTime);

		/**
		* @brief 全キャラクター（プレイヤー・NPC）の更新処理を行う
		*
		* @param frameContext     エンジンから渡されるフレーム情報（時間・ビューポート・ビュープロジェクション）
		* @param allowPlayerInput プレイヤーの入力（クリック移動など）を受け付けるかどうか。住民をカメラで追従している間や追従を解除したフレームでは false を渡し、そのクリックでプレイヤーが移動しないようにする。
		*/
		void UpdateCharacter(const Engine::FrameContext& frameContext, bool allowPlayerInput);

		//チャンクの更新処理
		ChunkUpdateResult UpdateChunk(DirectX::XMFLOAT3 playerPos);

		//ディスプレイスメントマッピングの更新処理
		void UpdateDisplacementMapping();


		//HPデータのセット処理
		void SetHealthData(const ChunkUpdateResult& chunkUpdateResult);

		//攻撃処理の更新
		void UpdateAttack(const Engine::FrameContext& frameContext);

		//HP0になったオブジェクトの削除処理
		void RemoveDeadObjects();

		//モデルパスとオブジェクトキーの対応表の初期化
		void InitModelPathToObjectKey();

		/**
		* @brief 住民パネルでターゲットを切り替えたときに追従するカメラ
		* この関数の返り値でプレイヤーのマウスの入出力ブロックするかどうか決定する
		* 
		* @return true プレイヤーのマウス入力あり
		* @return false プレイヤーのマウス入力なし
		*/
		bool UpdateFollowTarget();

	private: //初期化関連
		void InitializeUI();

		void UpdateCameraTarget(const DirectX::XMFLOAT3& playerPos);
	private:

		std::unique_ptr<Item::Runtime::ItemSystem> itemSystem;
		std::unique_ptr<Combat::CombatSystem> combatSystem_;
		std::unique_ptr<Character::CharacterSystem> characterSystem_;
		std::unique_ptr<HP::Render::HPBarRenderer> hPBarRenderer_;
		std::unique_ptr<UI::HpBarController> hpBarController_;
		std::unique_ptr<HealthManager> healthManager_;
		std::unique_ptr<Init::EntityInitializer> entityInitializer_;
		std::unique_ptr<::Game::World::Drop::WorldItemDropSystem> worldItemDropSystem_;
		//std::unique_ptr<::World::Runtime::WorldItemDropManager> worldItemDropManager_;
		std::unique_ptr<::World::Runtime::WorldItemDropVisualSystem> worldItemDropVisualSystem_;
		std::unique_ptr<Object::ObjectAttributeManager> objectAttributeManager_;
		std::unique_ptr<World::WorldObjectSystem> worldObjectSystem_;
		std::unique_ptr<World::WorldObjectDestroySystem> worldObjectDestroySystem_;
		std::unique_ptr<Item::Master::ItemMasterIconProvider> itemMasterIconProvider_;
		std::unique_ptr<World::Storage::StorageSystem> storageSystem_;
		std::unique_ptr<Build::BuildingDatabase> buildingDatabase_;
		std::unique_ptr<Build::BuildSystem> buildSystem_;
		std::unique_ptr<Timer::GameTimeSystem> gameTimeSystem_;
		std::unique_ptr<Agriculture::AgricultureSystem> agricultureSystem_;

		Engine::Engine* engine_ = nullptr;

	private:
		//追従対象
		Character::CharacterId followTargetId_;
		//プレイヤーのキャラクターID
		Character::CharacterId followTargetPlayerId_;

		Character::CharacterEngineServices characterEngineServices_;

		bool fieldDetailInfo_ = false;
		bool temp = false;

	};
}
