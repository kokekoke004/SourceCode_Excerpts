#include"FreeTimeBehaviorSystem.h"
#include"FreeTimeBehaviorUtility.h"
#include"CharacterBase.h"

#include"FreeTimeBehaviorBase.h"
#include"GameTimeSystem.h"

namespace Game::Character::FreeTime
{
	void FreeTimeBehaviorSystem::RegisterBehavior(FreeTimeBehaviorType type, std::unique_ptr<FreeTimeBehaviorBase> behavior)
	{
		behaviors_[type] = std::move(behavior);
	}

	bool FreeTimeBehaviorSystem::HasActiveBehavior() const
	{
		return currentBehaviorType_.has_value();
	}

	bool FreeTimeBehaviorSystem::CanCurrentBehaviorInterruptByJob() const
	{
		//現在の行動が未登録なら割込み可
		if (!currentBehaviorType_)
		{
			return true;
		}

		auto it = behaviors_.find(*currentBehaviorType_);

		//見つからない場合は割込み可
		if (it == behaviors_.end() || !it->second)
		{
			return true;
		}

		return it->second->CanInterruptByJob();

	}

	FreeTimeBehaviorType FreeTimeBehaviorSystem::SelectBehavior(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		FreeTimeBehaviorType selectedType = FreeTimeBehaviorType::Wander;

		int bestPriority = -1;

		//行動リストにアクセスして、どの行動をとるかをここで決める
		for (const auto& [type, behavior] : behaviors_)
		{
			if (!behavior) {
				continue;
			}

			//もし、状態遷移を移れない場合はスキップ
			if (!behavior->CanStart(character, updateContext)) {
				continue;
			}


			const int priority = behavior->GetPriority(character, updateContext);

			//優先順位が高ければ優先して行動する
			if (priority > bestPriority) {
				bestPriority = priority;
				selectedType = type;
			}

		}

		return selectedType;
	}

	void FreeTimeBehaviorSystem::UpdateCharacterBehavior(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//ゲーム内での経過時間を取得
		const double elapsedGameMinutes = updateContext.gameServices.gameTimeSystem.GetElapsedGameMinutes();

		if (currentBehaviorType_) {
			auto currentIt = behaviors_.find(*currentBehaviorType_);

			//自由時間の行動クラスが初期化しっかりされていてかつマップの中にしっかり入ってたら
			if(currentIt == behaviors_.end() || !currentIt->second) {
				currentBehaviorType_.reset();
				return;
			}

			//現在のBehaviorを更新
			const auto result = currentIt->second->Update(character, updateContext);


			//ゲーム内での経過時間に応じてスタミナを回復する（家に到着済みであれば回復速度が上がる）
			character.GetCharacterNeeds().staminaNeed.RecoverStaminaOverTime(elapsedGameMinutes);


			//まだ行動中
			if(result == FreeTimeBehaviorUpdateResult::Running)
			{
				return;
			}

			//Completed / Failedの場合は終了処理
			currentIt->second->Exit(character, updateContext);

			currentBehaviorType_.reset();
		}

		//Behaviorがない時だけ新しく選択

		auto selectedBehaviorType = SelectBehavior(character, updateContext);

		//選択されたBehaviorが登録されているか確認
		auto selectedIt = behaviors_.find(selectedBehaviorType);

		if (selectedIt == behaviors_.end() || !selectedIt->second)
		{
			return;
		}

		//新しいBehaviorに切り替える
		currentBehaviorType_ = selectedBehaviorType;

		//アクティブな行動として登録
		character.GetFreeTimeState().activeType = *currentBehaviorType_;
		character.GetFreeTimeState().type = *currentBehaviorType_;

		selectedIt->second->Enter(character);

		//現在の自由時間行動を更新
		auto result = selectedIt->second->Update(character, updateContext);
		//ゲーム内での経過時間に応じてスタミナを回復する（家に到着済みであれば回復速度が上がる）
		character.GetCharacterNeeds().staminaNeed.RecoverStaminaOverTime(elapsedGameMinutes);

		//開始直後に終了した場合
		if(result != FreeTimeBehaviorUpdateResult::Running)
		{
			selectedIt->second->Exit(character, updateContext);
			currentBehaviorType_.reset();
		}

	}

	void FreeTimeBehaviorSystem::StopCurrentBehavior(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{

		if (currentBehaviorType_) {
			//行動に対応する振る舞いが登録されているか確認
			auto it = behaviors_.find(*currentBehaviorType_);

			if (it != behaviors_.end() && it->second)
			{
				it->second->Exit(character, updateContext);
			}

		}


		currentBehaviorType_.reset();

		//キャラクターの暇な時の行動を取得
		auto& state = character.GetFreeTimeState();

		//自由時間行動を未選択状態に戻す
		state.type = FreeTimeBehaviorType::None;
		state.activeType = FreeTimeBehaviorType::None;

	}
}