#pragma once

#include "EtwProvider.h"

class Utf8Writer;

// Registered providers' metadata as CSV or JSON. progress (optional) gets the number of items
// (providers, or events for a single provider) written so far; returning false stops (returns false).
// The providers' metadata is loaded on demand, so MOF providers need COM on the calling thread.
namespace ProviderExport {
	using Progress = std::function<bool(size_t)>;

	// one row per provider: name, GUID, source, event count
	bool WriteProviderListCsv(Utf8Writer& writer, std::span<EtwProvider const> providers, Progress const& progress = nullptr);
	// one row per event of the provider, in the given order
	bool WriteEventsCsv(Utf8Writer& writer, EtwProvider const& provider, std::span<EVENT_DESCRIPTOR const> events,
		Progress const& progress = nullptr);
	// one row per event of all the providers (by ID, version), with provider columns first
	bool WriteAllEventsCsv(Utf8Writer& writer, std::span<EtwProvider const> providers, Progress const& progress = nullptr);

	// a JSON array of providers; with events, each one gets its events (by ID, version) and their properties
	bool WriteProvidersJson(Utf8Writer& writer, std::span<EtwProvider const> providers, bool events, Progress const& progress = nullptr);
	// a JSON object of the provider with the given events, in that order
	bool WriteProviderJson(Utf8Writer& writer, EtwProvider const& provider, std::span<EVENT_DESCRIPTOR const> events,
		Progress const& progress = nullptr);
}
