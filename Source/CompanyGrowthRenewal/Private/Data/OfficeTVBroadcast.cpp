#include "Data/OfficeTVBroadcast.h"

namespace OfficeTVProgram
{
	float GetDuration(EOfficeTVProgram Program)
	{
		switch (Program)
		{
		case EOfficeTVProgram::Development:
		case EOfficeTVProgram::DevRace:
		case EOfficeTVProgram::DevForecast:
		case EOfficeTVProgram::EmployeeHighlight:
		case EOfficeTVProgram::Crew:
		case EOfficeTVProgram::WorkFeed:
		case EOfficeTVProgram::Mentoring:
		case EOfficeTVProgram::StaffGap:
			return 4.f;
		case EOfficeTVProgram::CriticalFlash:
		case EOfficeTVProgram::Fever:
		case EOfficeTVProgram::IssueResult:
		case EOfficeTVProgram::Promotion:
			return 3.f;
		case EOfficeTVProgram::NewHire:
		case EOfficeTVProgram::FirstDiscovery:
			return 4.f;
		case EOfficeTVProgram::LaunchCelebration:
		case EOfficeTVProgram::TierUp:
			return 10.f;
		case EOfficeTVProgram::Credits:
		case EOfficeTVProgram::Advertisement:
			return 8.f;
		default:
			return 6.f;
		}
	}

	bool IsLiveReadout(EOfficeTVProgram Program)
	{
		switch (Program)
		{
		case EOfficeTVProgram::Development:
		case EOfficeTVProgram::DevRace:
		case EOfficeTVProgram::DevForecast:
		case EOfficeTVProgram::LaunchCountdown:
		case EOfficeTVProgram::RevenueTicker:
		case EOfficeTVProgram::Operations:
		case EOfficeTVProgram::ProductionLine:
		case EOfficeTVProgram::HotTime:
			return true;
		default:
			return false;
		}
	}

	float GetInterruptLifetime(EOfficeTVProgram Program)
	{
		switch (Program)
		{
		case EOfficeTVProgram::CriticalFlash:
		case EOfficeTVProgram::Fever:
			return 6.f;
		case EOfficeTVProgram::IssueResult:
			return 20.f;
		case EOfficeTVProgram::Promotion:
			return 30.f;
		case EOfficeTVProgram::NewHire:
			return 60.f;
		case EOfficeTVProgram::Reactions:
			// Reactions that arrive during the launch sequence still air after it, back to back.
			return 60.f;
		default:
			return 0.f;
		}
	}

	float GetInterruptGap(EOfficeTVProgram Program)
	{
		// Reactions arrive every 8-12 seconds after launch; a gap would leave most of them unseen.
		return Program == EOfficeTVProgram::Reactions ? 0.f : 8.f;
	}
}
