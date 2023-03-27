#pragma once

struct SecurityHelper abstract final {
	static bool EnablePrivilege(PCWSTR privilege);
};

