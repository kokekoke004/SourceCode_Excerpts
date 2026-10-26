#pragma once

#include"CharacterBase.h"
#include"ICharacterController.h"


namespace Game::Character::Job
{
	enum class JobType;
}


namespace Game::Character::Controller 
{
	class NpcController : public ICharacterController
	{
	public:
		NpcController();

		//キャラクターの思考処理
		void Think(Game::Character::CharacterBase& ch, const Engine::Components::FbxInstanceHandle& fbxInstanceHandle,
			const CharacterUpdateContext& updateContext) override;

		//キャラクターの操作権を取得したときの処理
		void OnAttach(Game::Character::CharacterBase& ch) override;
		//キャラクターの操作権を放棄したときの処理
		void OnDetach(Game::Character::CharacterBase& ch) override;

	private:
		//仕事を中断しているか
		bool jobSuspended_ = false;


	};
}