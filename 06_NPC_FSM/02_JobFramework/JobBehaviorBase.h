#pragma once

#include"JobType.h"
#include"CharacterUpdateContext.h"
#include "CharacterBase.h"

#include"JobActionComponent.h"
#include"JobUtility.h"

//namespace Game::Character
//{
//	class CharacterBase;
//}


namespace Game::Character::Job
{
	class JobBehaviorBase
	{
	public:
		virtual ~JobBehaviorBase() = default;

		virtual void Enter(Game::Character::CharacterBase& character)
		{
			auto& jobState = character.GetJobState();
			character.GetJobState().phase = JobWorkPhase::Idle;
			//ターゲット情報も初期化
			character.GetJobActionComponent().ClearTarget();
		}

		virtual JobUpdateResult Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext) = 0;
		virtual void Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext) = 0;

		/**
		* @brief 仕事を再開する際の処理を行う
		* @param character キャラクター
		* @param updateContext 更新コンテキスト
		*/
		virtual void Resume(CharacterBase& character, const CharacterUpdateContext& updateContext) = 0;


	};
}