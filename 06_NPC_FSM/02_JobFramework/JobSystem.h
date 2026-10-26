#pragma once
#include<memory>
#include<unordered_map>

namespace Game::Character
{
	class CharacterBase;
	struct CharacterUpdateContext;
}

namespace Game::Character::Job
{
	class JobBehaviorBase;
	enum class JobType;

	enum class JobUpdateResult;

	class JobSystem
	{
	public:
		JobSystem();
		~JobSystem();

		//職業のセットをする
		//jobType：職業の種類
		//behavior：その職業の振る舞いを定義したクラスのインスタンス
		void RegisterJobBehavior(JobType jobType, std::unique_ptr<JobBehaviorBase> behavior);

		/**
		* @brief 仕事の更新処理
		* @param character キャラクター
		* @param updateContext 更新コンテキスト
		* @param isResuming 仕事を再開するかどうか
		*/
		JobUpdateResult UpdateCharacterJob(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, bool isResuming);

	private:
		std::unordered_map<JobType, std::unique_ptr<JobBehaviorBase>> jobBehaviors_;
	};
}