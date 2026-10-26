#pragma once

#include"ICharacter.h"
#include<memory>
#include<optional>
#include<unordered_map>
#include<string>
#include<DirectXMath.h>
#include"TransformSystem.h"
#include"JobType.h"
#include"JobSystem.h"
#include"CharacterTypes.h"
#include"FreeTimeBehaviorUtility.h"
#include"StaminaNeed.h"
#include"HungerNeed.h"

namespace Engine 
{
	struct FrameContext;
}

namespace Item::Runtime
{
	class Inventory;
}

namespace Game::Character
{
	namespace Job
	{
		class JobActionComponent;
		class jobSystem;
	}

	namespace FreeTime
	{
		class FreeTimeBehaviorSystem;
	}
}




namespace Game::Character
{
	struct CharacterUpdateContext;

	class ICharacterController;


	//キャラクターの欲求
	struct CharacterNeeds
	{
		Need::HungerNeed hungerNeed;
		//スタミナの欲求
		Need::StaminaNeed staminaNeed;
	};

	struct SelectionResult
	{
		bool hit = false;
		bool isObstacle = false;
		bool isCharacter = false;
		std::wstring modelName;
		std::wstring drawName;

		DirectX::XMFLOAT3 obstaclePos = { 0,0,0 }; //障害物の位置（攻撃リクエストに必要）
	};

	struct CombatTargetInfo
	{
		std::optional<Engine::Components::TransformId> currentTid = std::nullopt;
		std::optional<Engine::Components::TransformId> previousTid = std::nullopt;

		std::wstring currentModelName = L"";
		std::wstring currentDrawName = L"";

		std::wstring previousModelName = L"";
		std::wstring previousDrawName = L"";

		CombatTargetInfo() = default;

		CombatTargetInfo(
			std::optional<Engine::Components::TransformId> currentTid,
			std::optional<Engine::Components::TransformId> previousTid,
			std::wstring currentModelName,
			std::wstring currentDrawName,
			std::wstring previousModelName,
			std::wstring previousDrawName)
			: currentTid(currentTid),
			previousTid(previousTid),
			currentModelName(std::move(currentModelName)),
			currentDrawName(std::move(currentDrawName)),
			previousModelName(std::move(previousModelName)),
			previousDrawName(std::move(previousDrawName))
		{
		}

	};

	struct CombatFlags
	{
		bool isInCombat = false;
		bool justEnteredCombat = false;
		bool justExitedCombat = false;

		CombatFlags() = default;

		CombatFlags(bool isIncombat, bool justEnteredCombat, bool justExitedCombat)
			:isInCombat(isIncombat), justEnteredCombat(justEnteredCombat), justExitedCombat(justExitedCombat)
		{

		}
	};

	//キャラクターの戦闘状態を管理する構造体
	struct CharacterCombatState
	{
		CombatTargetInfo targetInfo; //ターゲットの情報を管理する構造体
		CombatFlags combatFlags; //戦闘状態を管理する構造体

		CharacterCombatState() = default;
	};

	//NPCやプレイヤーの共通の処理をここで書く
	class CharacterBase :public ICharacter
	{
	public:
		CharacterBase(CharacterId characterId, Engine::Components::FbxInstanceHandle fbxInstanceHandle);
		~CharacterBase();
		void SetController(std::unique_ptr<ICharacterController> c);

		//職業をセットする
		void SetCharacterJob(Job::JobType jobType);

		void Update(const CharacterUpdateContext& updateContext) override;

		//最後の選択結果を保持する
		void SetLastSelection(const SelectionResult& result)
		{
			lastSelection_ = result;
			//printf("SetLastSelection this=%p has=%d\n", this, (int)lastSelection_.has_value());
		}

		//最後に選択した値を取得する
		const std::optional<SelectionResult>& GetLastSelection()
		{
			//printf("GetLastSelection this=%p has=%d\n", this, (int)lastSelection_.has_value());
			return lastSelection_;
		}

		//最後の選択結果をクリアする
		void ClearLastSelection()
		{
			lastSelection_.reset();
		}

		//ここでキャラクターごとの戦闘状態を管理する
		void SetCharacterCombatState(const CharacterCombatState& characterCombatState);

		CharacterCombatState& GetTargetInfo()
		{
			return characterCombatState_;
		}

		const CharacterCombatState& GetTargetInfo() const
		{
			return characterCombatState_;
		}

		const CharacterCombatState& GetCharacterCombatState() const 
		{
			return characterCombatState_;
		}

		CharacterCombatState& GetCharacterCombatState()
		{
			return characterCombatState_;
		}

		Item::Runtime::Inventory& GetInventory() 
		{
			return *inventory_;
		}
		
		const Item::Runtime::Inventory& GetInventory() const 
		{
			return *inventory_;
		}

		Job::JobState& GetJobState() 
		{
			return jobState_;
		}

		const Job::JobState& GetJobState() const 
		{
			return jobState_; 
		}

		FreeTime::FreeTimeState& GetFreeTimeState()
		{
			return freeTimeState_;
		}

		const FreeTime::FreeTimeState& GetFreeTimeState() const 
		{
			return freeTimeState_;
		}


		Job::JobSystem& GetJobSystem()
		{
			return *jobSystem_;
		}

		const Job::JobSystem& GetJobSystem() const
		{
			return *jobSystem_;
		}

		FreeTime::FreeTimeBehaviorSystem& GetFreeTimeBehaviorSystem()
		{
			return *freeTimeBehaviorSystem_;
		}

		const FreeTime::FreeTimeBehaviorSystem& GetFreeTimeBehaviorSystem() const
		{
			return *freeTimeBehaviorSystem_;
		}


		const Engine::Components::FbxInstanceHandle& GetFbxInstanceHandle() const
		{
			return fbxInstanceHandle_;
		}

		Job::JobActionComponent& GetJobActionComponent() 
		{
			return *jobActionComponent_;
		}

		const CharacterId& GetCharacterId() const
		{
			return characterId_;
		}

		CharacterNeeds& GetCharacterNeeds()
		{
			return characterNeeds_;
		}

		const CharacterNeeds& GetCharacterNeeds() const 
		{
			return characterNeeds_;
		}

		/*
		* @brief キャラクターが仕事中かどうかを返す
		*
		* @return true 仕事中
		* @return false 仕事中ではない
		*/
		bool IsWorking() const
		{
			return isWorking_;
		}

		/*
		* @brief キャラクターが仕事中かどうかを返す
		*
		* @param isWorking true 仕事中 / false 仕事中ではないかをセットする
		*/
		void SetWorking(bool isWorking)
		{
			isWorking_ = isWorking;
		}

	private:
		/*
		* @brief キャラクターの欲求更新
		* 
		*/
		void UpdateNeeds(const CharacterUpdateContext& updateContext);

		/*
		* @brief キャラクターの空腹欲求更新
		*
		*/
		void UpdateHunger(const CharacterUpdateContext& updateContext);


	private:
		//変数群

		//ターゲットの名前を管理する構造体
		CharacterCombatState characterCombatState_;

		//最後の選択結果を保存するためのオプション
		std::optional<SelectionResult> lastSelection_;

		//職業の状態を管理する構造体
		Job::JobState jobState_;
		//自由時間の状態を管理する構造体
		FreeTime::FreeTimeState freeTimeState_;

		double minutesPerHour_ = 60.0;
		double hoursPerDay_ = 24.0;
		//1日の分総数
		double minutesPerDay_ = minutesPerHour_ * hoursPerDay_;

		//キャラクターが固有でもつキャラクターID
		CharacterId characterId_;

		//仕事してるかどうかのフラグ
		bool isWorking_ = false;

	private:

		//職業の行動を管理するコンポーネント（キャラクターの職業に応じた行動を管理するためのクラス）
		std::unique_ptr<Job::JobSystem> jobSystem_;

		//自由時間にとる行動を管理するコンポーネント
		std::unique_ptr<FreeTime::FreeTimeBehaviorSystem> freeTimeBehaviorSystem_;

		std::unique_ptr<ICharacterController> controller_;
		//担当するキャラクターのTidを管理する
		Engine::Components::FbxInstanceHandle fbxInstanceHandle_;

		std::unique_ptr < Item::Runtime::Inventory > inventory_;

		//職業の行動を管理するコンポーネント
		std::unique_ptr< Job::JobActionComponent> jobActionComponent_;

		//キャラクターの欲求値
		CharacterNeeds characterNeeds_;

	};

}