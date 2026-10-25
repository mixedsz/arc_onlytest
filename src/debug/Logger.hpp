#pragma once
#include <string>

// In-memory / console sinks only — no disk logging.
void Logger_Init();
void Logger_SinkInfo(const std::string& msg);
void Logger_SinkError(const std::string& msg);
void Logger_TransportError(const char* status);
