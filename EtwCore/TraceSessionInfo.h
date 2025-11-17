#pragma once

#include <vector>
#include <string>

struct TraceSessionInfo : EVENT_TRACE_PROPERTIES {
	static std::vector<TraceSessionInfo> EnumTraceSessions();

	std::wstring LoggerName;
	std::wstring LogFileName;
	uint32_t Index;
	bool Running{ true };
};

