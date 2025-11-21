#include "pch.h"
#include "TraceSessionInfo.h"

std::vector<TraceSessionInfo> TraceSessionInfo::EnumTraceSessions() {
	const uint32_t maxNameLen = 256;
	const uint32_t maxPathLen = 512;
	const uint32_t maxSessions = 256;
	ULONG size = sizeof(EVENT_TRACE_PROPERTIES) +
		maxNameLen * sizeof(WCHAR) + maxPathLen * sizeof(WCHAR);
	auto buffer = std::make_unique<uint8_t[]>(maxSessions * size);
	if (!buffer)
		return {};

	PEVENT_TRACE_PROPERTIES psessions[maxSessions];
	memset(buffer.get(), 0, maxSessions * size);

	for (uint32_t i = 0; i < maxSessions; i++) {
		psessions[i] = (PEVENT_TRACE_PROPERTIES)(buffer.get() + i * size);
		psessions[i]->Wnode.BufferSize = size;
		psessions[i]->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
		psessions[i]->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + maxNameLen * sizeof(WCHAR);
	}
	ULONG count;
	auto err = ::QueryAllTraces(psessions, maxSessions, &count);
	if (err != ERROR_SUCCESS)
		return {};

	std::vector<TraceSessionInfo> sessions;
	sessions.reserve(count);
	for(uint32_t i = 0; i < count; i++) {
		TraceSessionInfo si(*psessions[i]);
		si.LoggerName = (PCWSTR)((PBYTE)psessions[i] + psessions[i]->LoggerNameOffset);
		if (psessions[i]->LogFileNameOffset)
			si.LogFileName = (PCWSTR)((PBYTE)psessions[i] + psessions[i]->LogFileNameOffset);
		si.Index = i + 1;
		sessions.push_back(std::move(si));
	}
	return sessions;
}

std::vector<TRACE_ENABLE_INFO> TraceSessionInfo::EnumProviders() const {
	std::vector<TRACE_ENABLE_INFO> providers;
	ULONG size = 0;
	auto err = ::EnumerateTraceGuidsEx(TraceGuidQueryInfo, (PVOID)&Wnode.Guid, sizeof(GUID), nullptr, 0, &size);
	if (err != ERROR_SUCCESS && err != ERROR_INSUFFICIENT_BUFFER)
		return providers;
	auto buffer = std::make_unique<uint8_t[]>(size);
	if (!buffer)
		return providers;
	auto info = (TRACE_GUID_INFO*)buffer.get();
	err = ::EnumerateTraceGuidsEx(TraceGuidQueryInfo, (PVOID)&Wnode.Guid, sizeof(GUID), info, size, &size);
	if (err != ERROR_SUCCESS)
		return providers;

	auto inst = (TRACE_PROVIDER_INSTANCE_INFO*)PBYTE(info + 1);
	for (ULONG i = 0; i < info->InstanceCount; i++) {
		inst = (TRACE_PROVIDER_INSTANCE_INFO*)((PBYTE)inst + inst->NextOffset);
		int zz = 9;
		//providers.push_back(*inst);
	}

	return providers;
}
