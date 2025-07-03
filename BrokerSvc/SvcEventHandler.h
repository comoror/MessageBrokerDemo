#pragma once

void OnPowerEvent(IN DWORD dwEventType,
    IN LPVOID lpEventData,
    IN LPVOID lpContext
);

void OnSessionChange(IN DWORD dwEventType,
    IN LPVOID lpEventData,
    IN LPVOID lpContext
);

void OnDeviceEvent(IN DWORD dwEventType,
    IN LPVOID lpEventData,
    IN LPVOID lpContext
);

