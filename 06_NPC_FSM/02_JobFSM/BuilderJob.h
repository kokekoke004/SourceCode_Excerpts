#pragma once
#include"JobBehaviorBase.h"
#include"BuildProgressManager.h"
#include"JobTarget.h"

namespace Game::Character::Job
{
	class BuilderJob : public JobBehaviorBase
	{
	public:
		BuilderJob();
		~BuilderJob();

		JobUpdateResult Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext) override;
		void Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext) override;
		/**
		* @brief 仕事を再開する際の処理を行う
		* @param character キャラクター
		* @param updateContext 更新コンテキスト
		*/
		void Resume(CharacterBase& character, const CharacterUpdateContext& updateContext) override;


	private:
		//選択中の建築物の進捗状況データ
		Build::BuildProgressData selectBuildProgressData_;
		//建築タイマー
		float constructionTimer_ = 0.0f;

		//ターゲットとしたものを保持する変数
		std::optional<JobTarget> suspendedTarget_;

	private:

		//最も近くにある建築物のオブジェクトを返却する
		std::optional <Game::Build::BuildProgressData> FindNearestBuildingAwaitingConstruction(Game::Character::CharacterBase& character,
			const Game::Character::CharacterUpdateContext& updateContext);
	};
}