#pragma once

#include"FreeTimeBehaviorBase.h"

namespace Game::Character::FreeTime
{
	class WanderBehavior : public FreeTimeBehaviorBase
	{
	public:
		WanderBehavior() = default;
		~WanderBehavior() = default;

		/**
		* @brief 現在の自由時間行動を仕事によって割り込み可能かどうかを返す
		* @return true 仕事による割り込みが可能
		* @return false 仕事による割り込みが不可能
		*/
		bool CanInterruptByJob() const override;

		/**
		* @brief FSMの行動優先順位の設定
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*
		* @return 優先順位
		*/
		int GetPriority(CharacterBase& character, const CharacterUpdateContext& updateContext) const override;

		/**
		* @brief FSMが実行可能であるかどうか
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*
		* @return 可能かどうか(true 可 / false 不可)
		*/
		bool CanStart(CharacterBase& character, const CharacterUpdateContext& updateContext) const override;

		/**
		* @brief FSMの初期化をする
		* @param character 対象のキャラクター
		*/
		void Enter(CharacterBase& character) override;

		/**
		* @brief FSMの更新処理
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		* @return 更新の結果
		*/
		FreeTimeBehaviorUpdateResult Update(CharacterBase& character, const CharacterUpdateContext& updateContext) override;

		/**
		* @brief FSMを出るときの処理
		* @param character 対象のキャラクター
		*/
		void Exit(CharacterBase& character, const CharacterUpdateContext& updateContext) override;

	private:

		//何秒待機してるのか
		float waitElapsedTime_ = 0.0f;
		//何秒待機するのか
		float waitRemainingTime_ = 0.0f;
	};

}
