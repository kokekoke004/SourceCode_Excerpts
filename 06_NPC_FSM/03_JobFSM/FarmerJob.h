#pragma once
#include"JobBehaviorBase.h"
#include"BuildProgressManager.h"
#include"TransformSystem.h"
#include"CropGrowthComponentUtility.h"


namespace Game::Agriculture
{
	enum class CropGrowthStage;
}

namespace Game::Character::Job
{
	class FarmerJob : public JobBehaviorBase
	{
	public:
		FarmerJob();
		~FarmerJob();

		JobUpdateResult Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext) override;
		void Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext) override;
		
		/**
		* @brief 仕事を再開する際の処理を行う
		* @param character キャラクター
		* @param updateContext 更新コンテキスト
		*/
		void Resume(CharacterBase& character, const CharacterUpdateContext& updateContext) override;

	private:
		/**
		* @brief 畑の作業で優先すべきものを選択する
		* @param 畑の作物の成長段階
		* @return 優先値
		*/
		int CalculatePriority(Agriculture::CropGrowthStage stage) const;


		void ReleaseCurrentWork(CharacterBase& character, Agriculture::AgricultureSystem& agricultureSystem);

	private:

		enum class FarmerPurpose
		{
			None,
			//種まき
			Seeding,
			//畑作業
			Farming,
			//農作物の収穫
			Harvesting,
		};



		struct FarmerWorkState {
			//バフをかける作業の経過時間
			float workElapsedTime = 0.0f;
			//バフをかける作業の必要時間
			float workDuration = 3.0f;

			FarmerPurpose farmerPurpose = FarmerPurpose::None;


			bool IsWorkFinished()
			{
				return workDuration <= workElapsedTime;
			}

			void Reset()
			{
				workElapsedTime = 0.0f;
				farmerPurpose = FarmerPurpose::None;
			}
		};
	private:

		//ターゲットとしたものを保持する変数
		std::optional<JobTarget> suspendedTarget_;

		FarmerWorkState farmerWorkState_;

	};
}