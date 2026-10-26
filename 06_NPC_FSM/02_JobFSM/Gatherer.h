#pragma once
#include"JobBehaviorBase.h"

//採取職の行動パターン設計
namespace Game::Character::Job
{
	class Gatherer : public JobBehaviorBase
	{
	public:
		Gatherer();

		JobUpdateResult Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext) override;
		void Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext) override;
		/**
		* @brief 仕事を再開する際の処理を行う
		* @param character キャラクター
		* @param updateContext 更新コンテキスト
		*/
		void Resume(CharacterBase& character, const CharacterUpdateContext& updateContext) override;

	private:
		//ターゲットとしたものを保持する変数
		std::optional<JobTarget> suspendedTarget_;

	};
}