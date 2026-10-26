#include"JobSystem.h"

#include"CharacterBase.h"
#include"JobBehaviorBase.h"
#include"JobType.h"
#include"CharacterUpdateContext.h"

namespace Game::Character::Job
{
	JobSystem::JobSystem() = default;
	JobSystem::~JobSystem() = default;

	void JobSystem::RegisterJobBehavior(JobType jobType, std::unique_ptr<JobBehaviorBase> behavior)
	{
		jobBehaviors_[jobType] = std::move(behavior);
	}

	JobUpdateResult JobSystem::UpdateCharacterJob(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, bool isResuming)
	{
		//ƒLƒƒƒ‰ƒNƒ^[‚ÌE‹Æ‚ðŽæ“¾
		JobType jobType = character.GetJobState().type;
		auto& jobState = character.GetJobState();
		//E‹Æ‚É‘Î‰ž‚·‚éU‚é•‘‚¢‚ª“o˜^‚³‚ê‚Ä‚¢‚é‚©Šm”F
		auto it = jobBehaviors_.find(jobType);

		if (jobState.type != jobState.activeType) {
			//‘O‰ñ‘I‘ð‚µ‚½E‹Æ‚ÌI—¹ˆ—
			auto oldIt = jobBehaviors_.find(jobState.activeType);

			if (oldIt != jobBehaviors_.end()) {
				oldIt->second->Exit(character,updateContext);
			}

			//V‚µ‚¢E‹Æ‚ÌŠJŽnˆ—
			auto newIt = jobBehaviors_.find(jobState.type);

			if (newIt != jobBehaviors_.end()) {
				newIt->second->Enter(character);
			}

			//V‚µ‚¢E‹Æ‚ðƒAƒNƒeƒBƒu‚Æ‚µ‚Ä“o˜^
			jobState.activeType = jobState.type;
		}

		if (it == jobBehaviors_.end()){
			return JobUpdateResult::NoWork;
		}

		//“r’†‚Ü‚Å‚µ‚Ä‚¢‚½ŽdŽ–‚ÌÄŠJˆ—
		if (isResuming) {
			it->second->Resume(character, updateContext);
		}
		//’Êí‚ÌŽdŽ–XVˆ—
		return it->second->Update(character, updateContext);

	}
}