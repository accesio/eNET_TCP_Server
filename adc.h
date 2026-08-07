#pragma once

#include <cstdint>

using TAdcConnectionId = std::uint32_t;

constexpr TAdcConnectionId ADC_INVALID_CONNECTION_ID = 0;

TAdcConnectionId AdcRegisterControlConnection(int socket);
bool AdcIsControlConnectionActive(TAdcConnectionId connectionId, int socket);
void AdcControlConnectionClosed(TAdcConnectionId connectionId);

TAdcConnectionId AdcRegisterDataConnection(int socket);
void AdcDataConnectionClosed(TAdcConnectionId connectionId, int socket);

int AdcStartStream(TAdcConnectionId controlConnectionId, TAdcConnectionId dataConnectionId);
int AdcStopStream(TAdcConnectionId controlConnectionId);
void AdcShutdown();
