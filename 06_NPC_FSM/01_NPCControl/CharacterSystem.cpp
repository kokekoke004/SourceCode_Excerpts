#include"CharacterSystem.h"
#include "CharacterBase.h"
#include "ICharacterController.h"
#include "Engine/FrameContext.h"
#include"CharacterUpdateContext.h"
#include"Engine/IModelRenderer.h"
#include"Engine/IModelLoader.h"
#include"CharacterSpawner.h"
#include"CharacterTypes.h"
#include"PlayerController.h"
#include"NpcController.h"
#include"TransformSystem.h"
#include"GroundingSystem.h"



namespace Game::Character
{
    CharacterSystem::CharacterSystem(CharacterService characterService)
		:characterService_(characterService)
    {
        CharacterSpownerServices spownerServices{
            characterService_.modelRender,
            characterService_.modelLoader
		};

        //キャラクタースポナー初期化
		characterSpawner_ = std::make_unique<CharacterSpawner>(spownerServices);

    }

	CharacterSystem::~CharacterSystem() = default;

    CharacterId CharacterSystem::SpawnPlayer(const std::wstring& playerName, const std::wstring& modelPath,float x, float y, float z)
    {
        CharacterSpawnInfo info{ playerName, playerName, L"PlayerArchetype" };

        CharacterVisualInfo visual{
            modelPath,
            EntityType::PLAYER,
            true,  false,
            x, y, z,
            AnimationType::SKINMESH ,
        };

        auto controller = std::make_unique<Character::Controller::PlayerController>();
        return Spawn(info, visual, std::move(controller));
    }

    CharacterId CharacterSystem::SpawnNpc(const std::wstring& npcName, const std::wstring& modelPath, float x, float y, float z, Job::JobType jobType)
    {
        CharacterSpawnInfo info{ npcName, npcName, L"NPCArchetype" };
        CharacterVisualInfo visual{
            modelPath,
            EntityType::NPC ,
            true,  false,
            x, y, z,
            AnimationType::SKINMESH,
        };

        auto controller = std::make_unique<Character::Controller::NpcController>();
        return Spawn(info, visual, std::move(controller), jobType);
    }

    CharacterId CharacterSystem::Spawn(const CharacterSpawnInfo& info,
        const CharacterVisualInfo& visual,
        std::unique_ptr<ICharacterController> controller,
        Job::JobType jobType)
    {
        const CharacterId id = nextId_++;

        //キャラクターの初期化
        CharacterSlot slot{};
        slot.displayName = info.displayName;
        slot.debugName = info.debugName;
        slot.archetypeKey = info.archetypeKey;
        slot.life = CharacterSlot::LifeState::Alive;
        slot.updateEnabled = true;
        slot.despawnTimer = 0.0f;
        slot.entityType = visual.entityType;

        //キャラクター描画ブリッジに通知
		//ここでキャラクター描画用のモデルレンダラーに登録する
		//characterRenderBridge_.OnSpawn(id, visual);
        auto insHandle = characterSpawner_->SpawnCharacter(id, visual);

		//キャラクターの位置をGroundingStateに登録(接地判定あり)
		Engine::Movement::GroundingState initialGroundingState{};
		characterService_.groundingSystem.Register(insHandle.tid, initialGroundingState);
        
		slot.fbxInstanceHandle = insHandle;

        slot.ch = std::make_unique<CharacterBase>(id,insHandle);
        //プレイヤーかNPCのコントローラー登録
        slot.ch->SetController(std::move(controller));

        registry_.InsertUnique(id, std::move(slot));

        //職業セット
        SetCharacterJob(id, jobType);


        return id;
    }

    void CharacterSystem::RequestDespawn(CharacterId id, float delaySec)
    {
		//キーからスロット取得
        auto* slot = registry_.GetDataByKey(id);
        if (!slot) return;
		// すでに死んでいるなら無視
        if (slot->life == CharacterSlot::LifeState::Dead) return;
		// 死亡状態に変更してタイマーセット
        slot->life = CharacterSlot::LifeState::Dying;
        slot->despawnTimer = delaySec;

		characterService_.groundingSystem.Unregister(slot->fbxInstanceHandle.tid); //接地判定から外す
        if (delaySec <= 0.0f) {
			//更新処理で即時削除するために予約
            pendingErase_.push_back(id);
        }
    }

    bool CharacterSystem::SetController(CharacterId id, std::unique_ptr<ICharacterController> controller)
    {
		//キーからスロット取得すでに存在しているキャラクタークラスをプレーヤーからNPCに切り替えたり、その逆を行う
        auto* slot = registry_.GetDataByKey(id);
        if (!slot || !slot->ch) return false;
		//コントローラー差し替え
        slot->ch->SetController(std::move(controller));
        return true;
    }

    CharacterSlot* CharacterSystem::GetSlot(CharacterId id)
    {
        return registry_.GetDataByKey(id);
    }

    const CharacterSlot* CharacterSystem::GetSlot(CharacterId id) const
    {
        return registry_.GetDataByKey(id);
    }

    CharacterBase* CharacterSystem::Get(CharacterId id)
    {
        auto* slot = registry_.GetDataByKey(id);
        return (slot && slot->ch) ? slot->ch.get() : nullptr;
    }

    const CharacterBase* CharacterSystem::Get(CharacterId id) const
    {
        auto* slot = registry_.GetDataByKey(id);
        return (slot && slot->ch) ? slot->ch.get() : nullptr;
    }

    void CharacterSystem::UpdateAll(const CharacterUpdateContext& updateContext)
    {
        //ループ中にEraseしない（swap&popで配列が壊れるから）
        std::vector<CharacterId> eraseNow;
        eraseNow.reserve(16);

        //配列管理として密な配列で回す
        for (auto& entry : registry_.GetAllData())
        {
            const CharacterId id = entry.key;
            auto& slot = entry.value;

			// キャラクター本体が無ければスキップ
            if (!slot.ch) continue;

            if (slot.life == CharacterSlot::LifeState::Dying)
            {
				// デスポーンタイマーを進める
                //まだこれを使う処理書いてない（仮）
                slot.despawnTimer -= updateContext.frame.time.deltaTime;
                if (slot.despawnTimer <= 0.0f)
                {
                    eraseNow.push_back(id);
                    continue;
                }
            }

            if (slot.life == CharacterSlot::LifeState::Alive && slot.updateEnabled)
            {
                slot.ch->Update(updateContext);
            }
        }

        // 破棄予約をまとめて削除
        if (!pendingErase_.empty())
        {
			//削除リストをまとめてeraseNowに追加
            eraseNow.insert(eraseNow.end(), pendingErase_.begin(), pendingErase_.end());
			//持ち越さないようにここでクリア
            pendingErase_.clear();
        }

        if (!eraseNow.empty())
        {
            std::sort(eraseNow.begin(), eraseNow.end());
			//重複削除
            eraseNow.erase(std::unique(eraseNow.begin(), eraseNow.end()), eraseNow.end());

            for (CharacterId id : eraseNow)
            {
                //削除処理実行
                registry_.EraseByKey(id);
            }
        }
    }

    bool CharacterSystem::TryGetLocalPos(CharacterId id, DirectX::XMFLOAT3& outPos) const
    {
		//キーからスロット取得
        const auto* slot = registry_.GetDataByKey(id);
        if (!slot) return false;
		// すでに死んでいるなら座標は取得できない
        if (slot->life == CharacterSlot::LifeState::Dead) return false;

        const auto tid = slot->fbxInstanceHandle.tid;

        const auto* tr = characterService_.transformSystem.GetLocal(tid);
        if (!tr) return false;

        outPos = tr->pos.xyz;
        return true;
    }

    const std::optional<SelectionResult>& CharacterSystem::GetLastSelection(CharacterId id)
    {
        //キーからスロット取得
        const auto* slot = registry_.GetDataByKey(id);
		if (!slot) return std::nullopt;

        return slot->ch->GetLastSelection();
    }

    void CharacterSystem::ClearLastSelection(CharacterId id)
    {
        //キーからスロット取得
        const auto* slot = registry_.GetDataByKey(id);
        if (!slot) return;

		slot->ch->ClearLastSelection();
    }

    Item::Runtime::Inventory* CharacterSystem::GetInventory(CharacterId id)
    {
        //キーからスロット取得
        auto* slot = registry_.GetDataByKey(id);
        if (!slot || !slot->ch) return nullptr;

        return &slot->ch->GetInventory();
    }

    const Item::Runtime::Inventory* CharacterSystem::GetInventory(CharacterId id) const
    {
        //キーからスロット取得
        auto* slot = registry_.GetDataByKey(id);
        if (!slot || !slot->ch) return nullptr;

        return &slot->ch->GetInventory();
    }

    bool CharacterSystem::SetCharacterJob(CharacterId id, Job::JobType jobType)
    {
        auto* ch = Get(id);
        if (!ch) {
            return false;
        }

		//キャラクタークラスの職業をセットする
        ch->SetCharacterJob(jobType);
        return true;
    }

    const Job::JobState& CharacterSystem::GetJobState(CharacterId id) const
    {
        auto* ch = Get(id);
        if (!ch) {
            return {};
        }
        return ch->GetJobState();
    }

    CharacterBase* CharacterSystem::GetByTransformId(Engine::Components::TransformId tid)
    {
        for (auto& entry : registry_.GetAllData())
        {
            auto& slot = entry.value;

            if (!slot.ch) {
                continue;
            }

            if (slot.fbxInstanceHandle.tid == tid) {
                return slot.ch.get();
            }
        }

        return nullptr;
    }

    const CharacterBase* CharacterSystem::GetByTransformId(Engine::Components::TransformId tid) const
    {
        for (auto& entry : registry_.GetAllData())
        {
            auto& slot = entry.value;

            if (!slot.ch) {
                continue;
            }

            if (slot.fbxInstanceHandle.tid == tid) {
                return slot.ch.get();
            }
        }

        return nullptr;
    }

}
