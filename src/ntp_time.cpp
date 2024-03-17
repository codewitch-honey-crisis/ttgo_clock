#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#include <ntp_time.hpp>
namespace arduino {
WiFiUDP g_ntp_time_udp;
void ntp_time::begin_request(IPAddress address, 
                            ntp_time_callback callback, 
                            void* callback_state) {
    memset(m_packet_buffer, 0, 48);
    m_packet_buffer[0] = 0b11100011;   // LI, Version, Mode
    m_packet_buffer[1] = 0;     // Stratum, or type of clock
    m_packet_buffer[2] = 6;     // Polling Interval
    m_packet_buffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    m_packet_buffer[12]  = 49;
    m_packet_buffer[13]  = 0x4E;
    m_packet_buffer[14]  = 49;
    m_packet_buffer[15]  = 52;

    //NTP requests are to port 123
    g_ntp_time_udp.beginPacket(address, 123); 
    g_ntp_time_udp.write(m_packet_buffer, 48);
    g_ntp_time_udp.endPacket();
    m_request_result = 0;
    m_requesting = true;

    m_callback_state = callback_state;
    m_callback = callback;
}

void ntp_time::update() {
    m_request_result = 0;
        
    if(m_requesting) {
        // read the packet into the buffer
        // if we got a packet from NTP, read it
        if (0 < g_ntp_time_udp.parsePacket()) {
            g_ntp_time_udp.read(m_packet_buffer, 48); 

            //the timestamp starts at byte 40 of the received packet and is four bytes,
            // or two words, long. First, extract the two words:

            unsigned long hi = word(m_packet_buffer[40], m_packet_buffer[41]);
            unsigned long lo = word(m_packet_buffer[42], m_packet_buffer[43]);
            // combine the four bytes (two words) into a long integer
            // this is NTP time (seconds since Jan 1 1900):
            unsigned long since1900 = hi << 16 | lo;
            // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
            constexpr const unsigned long seventyYears = 2208988800UL;
            // subtract seventy years:
            m_request_result = since1900 - seventyYears;
            m_requesting = false;
            if(m_callback!=nullptr) {
                m_callback(m_request_result,m_callback_state);
            }
        }
    }
}
}

