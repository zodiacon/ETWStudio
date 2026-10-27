#pragma once

#include <TraceSession.h>
#include <optional>

// a session definition that can be saved to / loaded from a .etwsession file
struct SessionPreset {
	struct Provider {
		GUID Guid;
		std::wstring Name;
		ProviderConfig Config;
		std::vector<USHORT> EventIds;	// empty: all events
	};
	struct Kernel {
		KernelEventTypes Type;
		bool Stacks{ false };
	};

	std::wstring Name;
	std::wstring OutputFile;
	BufferSettings Buffers;
	std::vector<Provider> Providers;
	std::vector<Kernel> KernelTypes;

	static SessionPreset FromSession(TraceSession const& session);
	bool Save(PCWSTR path) const;
	// nullopt: unreadable or not a session file
	static std::optional<SessionPreset> Load(PCWSTR path);
};
