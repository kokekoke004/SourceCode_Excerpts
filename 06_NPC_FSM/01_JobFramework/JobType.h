#pragma once

namespace Game::Character::Job
{
	enum class JobType
	{
		None,
		Woodcutter,
        Miner,
		Builder,
		Farmer,
        Gatherer,
        HaulerJob,
	};

    enum class JobWorkPhase
    {
		SelectingTarget,
		PathFinding,
        Idle,
        MovingToTarget,
        Working,
		Attcking,
		PickingUp,
		SelectingDepositTarget, //—a‚¯‚éêŠ‚ğ‘I‘ğ
        Depositing, //ƒAƒCƒeƒ€‚ğ‚µ‚Ü‚¤

        //Ìæ
        //–Ø‚ğØ‚é
        Chopping,
		//zÎ‚ğÌŒ@‚·‚é
        Mining,
		//ÌW‚·‚é
        Gathering,

        //Œš’z‘Ş‚Ì‰^”À
        SelectingSourceStorage,  //—\–ñ‚µ‚½‘Ş‚ª’u‚¢‚Ä‚ ‚é‘qŒÉ‚ğˆÚ“®æ‚Æ‚µ‚Äİ’è‚·‚é
        WithdrawingReservedItem, //ƒAƒCƒeƒ€‚ğæ‚èo‚·
        SelectingBuildingTarget, //Œš’z•¨‚ğ‘I‘ğ‚·‚é
        SupplyingBuilding,        //Œš’z‘Ş‚ğŒš’z•¨‚É‹Ÿ‹‹‚·‚é

        //Œš’z
        Building,

        //”_‹Æ
        Seeding,             //í‚Ü‚«
        Farming,             //¬’·ƒoƒt‚ğ‚©‚¯‚é
        Harvesting,          //ûŠn

    };

    struct JobState
    {
        //Œ»İFSM‚Å‘I‘ğ’†‚ÌE‹Æ
        JobType type = JobType::None;
        //Œ»İ‰Šú‰»Ï‚İ‚ÌE‹Æ
        JobType activeType = JobType::None;

        int level = 1;
        JobWorkPhase phase = JobWorkPhase::Idle;
    };
}