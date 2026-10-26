#include"NpcController.h"

#include"Engine/FrameContext.h"
#include"CharacterUpdateContext.h"
#include"CharacterBase.h"
#include"JobSystem.h"
#include"JobType.h"
#include"JobUtility.h"
#include"FreeTimeBehaviorSystem.h"
#include"GameTimeSystem.h"

namespace Game::Character::Controller
{
	NpcController::NpcController()
	{

	}

	void NpcController::Think(Game::Character::CharacterBase& ch, const Engine::Components::FbxInstanceHandle& fbxInstanceHandle,
		const CharacterUpdateContext& updateContext)
	{
		auto& freeTimeSystem = ch.GetFreeTimeBehaviorSystem();
		auto& stamina = ch.GetCharacterNeeds().staminaNeed;
		if (freeTimeSystem.HasActiveBehavior() && !freeTimeSystem.CanCurrentBehaviorInterruptByJob()) {
			ch.SetWorking(false);
			//割込み不可能な自由時間の行動の更新
			freeTimeSystem.UpdateCharacterBehavior(ch, updateContext);
			return;
		}

		//スタミナが少ない場合は休憩行動を優先する
		if (stamina.ShouldRest()) {
			jobSuspended_ = true;

			ch.SetWorking(false);
			freeTimeSystem.UpdateCharacterBehavior(ch, updateContext);
			return;
		}

		//仕事中断状態であれば仕事を再開するために自由時間行動を終了
		if (jobSuspended_) {
			freeTimeSystem.StopCurrentBehavior(ch, updateContext);

		}
		//NPCの職業に応じた処理
		auto jobUpdateResult = ch.GetJobSystem().UpdateCharacterJob(ch, updateContext, jobSuspended_);

		jobSuspended_ = false;

		if (jobUpdateResult == Job::JobUpdateResult::NoWork) {
			ch.SetWorking(false);
			freeTimeSystem.UpdateCharacterBehavior(ch, updateContext);
		}
		else {
			ch.SetWorking(true);
			//仕事が見つかったら自由時間行動を終了
			freeTimeSystem.StopCurrentBehavior(ch, updateContext);

			//ゲーム内での経過時間を取得する
			const double elapsedGameMinutes =updateContext.gameServices.gameTimeSystem.GetElapsedGameMinutes();
			//ゲーム内での経過時間に応じてスタミナを消費する
			ch.GetCharacterNeeds().staminaNeed.ConsumeStaminaOverTime(elapsedGameMinutes);
		}

	}

	void NpcController::OnAttach(Game::Character::CharacterBase& ch)
	{

	}

	void NpcController::OnDetach(Game::Character::CharacterBase& ch)
	{

	}

}

