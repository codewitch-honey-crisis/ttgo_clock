#pragma once
#ifndef ESP32
#error "This library only supports the ESP32 MCU."
#endif
#include <Arduino.h>
namespace arduino {
typedef void(*ntp_time_callback)(time_t, void*);
class ntp_time final {
    bool m_requesting;
    time_t m_request_result;
    byte m_packet_buffer[48];
    ntp_time_callback m_callback;
    void* m_callback_state;
   public:
    inline ntp_time() : m_requesting(false),m_request_result(0) {}
    void begin_request(IPAddress address, ntp_time_callback callback = nullptr, void* callback_state = nullptr);
    inline bool request_received() const { return m_request_result!=0;}
    inline time_t request_result() const { return m_request_result; }
    inline bool requesting() const { return m_requesting; }
    void update();
};
}  // namespace arduino