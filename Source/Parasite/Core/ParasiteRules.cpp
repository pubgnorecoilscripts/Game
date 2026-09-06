#include "ParasiteRules.h"

namespace Parasite
{
	const char* ToString(EPossessResult Result)
	{
		switch (Result)
		{
		case EPossessResult::Success:			return "POSSESSED";
		case EPossessResult::NoSuchPlayer:		return "NO SUCH PLAYER";
		case EPossessResult::NoSuchHost:		return "NO HOST IN RANGE";
		case EPossessResult::WrongPhase:		return "MATCH NOT RUNNING";
		case EPossessResult::OnCooldown:		return "POSSESSION ON COOLDOWN";
		case EPossessResult::AlreadyPossessing:	return "ALREADY POSSESSING";
		case EPossessResult::OutOfRange:		return "OUT OF RANGE";
		case EPossessResult::HostOccupied:		return "HOST ALREADY TAKEN";
		case EPossessResult::HostIsFriendly:	return "THAT IS ONE OF YOURS";
		case EPossessResult::SelfPossession:	return "YOU ARE ALREADY YOU";
		case EPossessResult::Hijacked:			return "YOU ARE BEING RIDDEN";
		}
		return "UNKNOWN";
	}

	const char* ToString(ETeam Team)
	{
		switch (Team)
		{
		case ETeam::A:	return "TEAM A";
		case ETeam::B:	return "TEAM B";
		default:		return "NO TEAM";
		}
	}

	const char* ToString(EHostType Type)
	{
		switch (Type)
		{
		case EHostType::Prop:		return "OBJECT";
		case EHostType::NPC:		return "NPC";
		case EHostType::Vehicle:	return "VEHICLE";
		case EHostType::Player:		return "ENEMY";
		}
		return "OBJECT";
	}

	ETeam EnemyOf(ETeam Team)
	{
		switch (Team)
		{
		case ETeam::A:	return ETeam::B;
		case ETeam::B:	return ETeam::A;
		default:		return ETeam::None;
		}
	}
}
