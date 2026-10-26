#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <optional>

#include "IndexedRegistry.h"
#include "CharacterTypes.h"

#include"JobType.h"

namespace Item::Runtime
{
    class Inventory;
}

namespace Engine 
{ 
    struct FrameContext;

	class IModelRenderer;
	class IModelLoader;

    namespace Components
    {
        struct FbxInstanceHandle;
		class TransformSystem;
	}

    namespace Movement
    {
        class GroundingSystem;
	}
}



namespace Game::Character
{
    class CharacterBase;
    class ICharacterController;

    struct CharacterVisualInfo;
    struct CharacterUpdateContext;
    class CharacterSpawner;

    namespace Job
    {
        enum class JobType;
	}

	// キャラクターシステムで必要なサービス群
    struct CharacterService
    {
        Engine::IModelRenderer& modelRender;
		Engine::IModelLoader& modelLoader;
		Engine::Movement::GroundingSystem& groundingSystem;
		Engine::Components::TransformSystem& transformSystem;
        CharacterService(Engine::IModelRenderer& mR, Engine::IModelLoader& mL, Engine::Movement::GroundingSystem& gS, Engine::Components::TransformSystem& tS)
			: modelRender(mR), modelLoader(mL), groundingSystem(gS), transformSystem(tS)
        {

        }
    };

    struct SelectionResult;

	// キャラクターの情報を持つスロット
    struct CharacterSlot
    {
        //キャラクタークラスの本体
        std::unique_ptr<CharacterBase> ch;
        Engine::Components::FbxInstanceHandle fbxInstanceHandle;
        std::wstring displayName;
        std::wstring debugName;
        std::wstring archetypeKey;

        enum class LifeState : uint8_t { Alive, Dying, Dead };
        EntityType entityType = EntityType::NPC;
        LifeState life = LifeState::Alive;

        bool updateEnabled = true;
        float despawnTimer = 0.0f; //Dying中の残り秒（<=0で削除）
    };

    struct CharacterRecord
    {
        CharacterId id;
        Engine::Components::FbxInstanceHandle insHandle; // 描画インスタンス
    };

    class CharacterSystem
    {
    public:
        CharacterSystem(CharacterService characterService);
        ~CharacterSystem();
        using Registry = IndexedRegistry<CharacterId, CharacterSlot>;

		//生成：プレイヤー
		//playerName：表示名、modelPath：モデルファイルパス、x,y,z：初期位置
        CharacterId SpawnPlayer(const std::wstring& playerName, const std::wstring& modelPath,float x, float y, float z);

		//生成：NPC
		//npcName：表示名、modelPath：モデルファイルパス、x,y,z：初期位置
        CharacterId SpawnNpc(const std::wstring& npcName, const std::wstring& modelPath, float x, float y, float z, Job::JobType jobType = Job::JobType::None);


        //生成：キャラ作成＋Controller付与＋登録
        CharacterId Spawn(const CharacterSpawnInfo& info,
            const CharacterVisualInfo& visual,
            std::unique_ptr<ICharacterController> controller,
            Job::JobType jobType = Job::JobType::None);

        //破棄予約（delaySec>0なら演出待ちなど）
		//必ずUpdateAll内で削除するため、そのための予約をここで行う
        void RequestDespawn(CharacterId id, float delaySec = 0.0f);

        //Controller差し替え（プレイヤー/NPC切替）
        bool SetController(CharacterId id, std::unique_ptr<ICharacterController> controller);

        //取得
        CharacterSlot* GetSlot(CharacterId id);
        const CharacterSlot* GetSlot(CharacterId id) const;

        CharacterBase* GetByTransformId(Engine::Components::TransformId tid);

        const CharacterBase* GetByTransformId(Engine::Components::TransformId tid) const;

        CharacterBase* Get(CharacterId id);
        const CharacterBase* Get(CharacterId id) const;

        //注意：Eraseで配列順序が変わる
        Registry& RegistryRef() { return registry_; }
        const Registry& RegistryRef() const { return registry_; }

        void UpdateAll(const CharacterUpdateContext& updateContext);

        size_t Count() const noexcept { return registry_.Size(); }
        void Reserve(size_t n) { registry_.Reserve(n); }

		//特定キャラクターの座標を取得するための関数
		//座標が取得できたらtrue、取得できなければfalseを返す]
		//座標はoutPosに格納される
        bool TryGetLocalPos(CharacterId id, DirectX::XMFLOAT3& outPos) const;

        //キャラクターが最後に選択したものに関する情報を取得する
        const std::optional<SelectionResult>& GetLastSelection(CharacterId id);

        //最後の選択結果をクリアする
        void ClearLastSelection(CharacterId id);

    public: //キャラクターベースクラスのセッター群
        bool SetCharacterJob(CharacterId id, Job::JobType jobType);


    public: //キャラクターベースクラスのゲッター群

		//指定したキャラクターのインベントリを取得するためのゲッター
        Item::Runtime::Inventory* GetInventory(CharacterId id);
        const Item::Runtime::Inventory* GetInventory(CharacterId id) const;

		//全キャラクターデータのリストを取得するためのゲッター
        const std::vector<Registry::Entry>& GetAllData() const { return registry_.GetAllData(); }

        const Job::JobState& GetJobState(CharacterId id) const;

    private:
		//キャラクター描画ブリッジ（キャラクター描画とエンジンのレンダーモデルクラスを連携するためのクラス）
		std::unique_ptr<CharacterSpawner> characterSpawner_;
        CharacterService characterService_;
    private:


        CharacterId nextId_ = 1;
        Registry registry_;

        //Update中に消すと危険なのでIDを貯めて後で消す
        std::vector<CharacterId> pendingErase_;
    };
}
