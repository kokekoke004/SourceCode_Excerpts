#include"CharacterBase.h"
#include"ICharacterController.h"

#include"Engine/FrameContext.h"
#include"CharacterUpdateContext.h"
#include"TransformSystem.h"
#include"ItemInventory.h"
#include"JobActionComponent.h"
#include"GameTimeSystem.h"
#include"WoodcutterJob.h"
#include"MinerJob.h"
#include"Gatherer.h"
#include"HaulerJob.h"
#include"BuilderJob.h"
#include"FarmerJob.h"
#include"FreeTimeBehaviorSystem.h"
#include"ItemSystem.h"
#include"WorldItemDropManager.h"
#include"WanderBehavior.h"
#include"EatBehavior.h"
#include"FreeTimeBehaviorUtility.h"

namespace Game::Character
{
	CharacterBase::CharacterBase(CharacterId characterId, Engine::Components::FbxInstanceHandle fih)
        :characterId_(characterId), fbxInstanceHandle_(fih)
	{
		inventory_.reset(new Item::Runtime::Inventory());
        jobActionComponent_.reset(new Job::JobActionComponent());
        jobSystem_.reset(new Job::JobSystem());
        //職業をここですべてセットしてしまう
        {
            jobSystem_->RegisterJobBehavior(Job::JobType::Woodcutter, std::make_unique<Job::WoodcutterJob>());
            jobSystem_->RegisterJobBehavior(Job::JobType::Miner, std::make_unique<Job::MinerJob>());
            jobSystem_->RegisterJobBehavior(Job::JobType::Gatherer, std::make_unique<Job::Gatherer>());
            jobSystem_->RegisterJobBehavior(Job::JobType::HaulerJob, std::make_unique<Job::HaulerJob>());
            jobSystem_->RegisterJobBehavior(Job::JobType::Builder, std::make_unique<Job::BuilderJob>());
            jobSystem_->RegisterJobBehavior(Job::JobType::Farmer , std::make_unique<Job::FarmerJob>());

        }

        freeTimeBehaviorSystem_.reset(new FreeTime::FreeTimeBehaviorSystem());
        //自由時間（仕事がないときの行動）
        {
            freeTimeBehaviorSystem_->RegisterBehavior(FreeTime::FreeTimeBehaviorType::Wander, std::make_unique<FreeTime::WanderBehavior>());
            freeTimeBehaviorSystem_->RegisterBehavior(FreeTime::FreeTimeBehaviorType::Eat, std::make_unique<FreeTime::EatBehavior>());

        }

	}

    CharacterBase::~CharacterBase() = default;

    void CharacterBase::SetCharacterCombatState(const CharacterCombatState& characterCombatState)
    {
		characterCombatState_ = characterCombatState;
    }

    void CharacterBase::SetController(std::unique_ptr<ICharacterController> c)
    {
        //既存Controllerを外す（後始末）
        if (controller_)
        {
            //インタフェース名が OnDetach の場合
            controller_->OnDetach(*this);

        }

        //差し替え
        controller_ = std::move(c);

        //新Controllerを付ける（初期化）
        if (controller_)
        {
            controller_->OnAttach(*this);
        }
    }

    void CharacterBase::SetCharacterJob(Job::JobType jobType)
    {
		jobState_.type = jobType;
		jobState_.phase = Job::JobWorkPhase::Idle;
    }

	void CharacterBase::Update(const CharacterUpdateContext& updateContext)
	{
        //欲求更新
        UpdateNeeds(updateContext);

        //意思決定（入力→意図）
        if (controller_) {
            controller_->Think(*this, fbxInstanceHandle_, updateContext);
        }
	}

    void CharacterBase::UpdateNeeds(const CharacterUpdateContext& updateContext)
    {
        UpdateHunger(updateContext);
    }

    void CharacterBase::UpdateHunger(const CharacterUpdateContext& updateContext)
    {
        auto elapsedGameMinutes = updateContext.gameServices.gameTimeSystem.GetElapsedGameMinutes();
        GetCharacterNeeds().hungerNeed.ConsumeHungerOverTime(elapsedGameMinutes);
    }
}