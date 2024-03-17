#include <Arduino.h>
#include <gfx.hpp>
#include <ttgo.hpp>
#include <config.h>
#include <button.hpp>
#include <WiFi.h>
#include <ntp_time.hpp>
#include <ip_loc.hpp>

constexpr static const char* ssid = NULL;
constexpr static const char* pass = NULL;

#define DSEG14CLASSIC_REGULAR_IMPLEMENTATION
#include <assets/DSEG14Classic_Regular.hpp>
static const gfx::open_font& text_font = DSEG14Classic_Regular;
// NTP server
constexpr static const char* ntp_server = "pool.ntp.org";

// synchronize with NTP every 60 seconds
constexpr static const int clock_sync_seconds = 60;


using namespace arduino;
using namespace gfx;
using color_t = color<lcd_t::pixel_type>;
using fb_type = bitmap<lcd_t::pixel_type>;
constexpr static const size_t lcd_buffer_size = fb_type::sizeof_buffer({lcd_t::base_width,lcd_t::base_height});
static uint8_t lcd_buffer[lcd_buffer_size];
fb_type fb({240,135},lcd_buffer);
static int connect_state = 0;
char timbuf[16];
tm tim;
ntp_time ntp;
float latitude;
float longitude;
long utc_offset;
char region[128];
char city[128];
open_text_info oti;
static bool got_time = false;
static time_t current_time;
static IPAddress ntp_ip;
rect16 text_bounds;
void setup()
{
    COMMAND.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // get_build_tm(&tim);
    tim.tm_hour = 12;
    tim.tm_min = 0;
    tim.tm_sec = 0;
    lcd.initialize();
    lcd.rotation(3);
    float scl = text_font.scale(lcd.dimensions().height - 2);
    ssize16 dig_size = text_font.measure_text(ssize16::max(), spoint16::zero(), "0", scl);
    int16_t w = (dig_size.width + 1) * 6;
    float mult = (float)(lcd.dimensions().width - 2) / (float)w;
    if (mult > 1.0f) mult = 1.0f;
    int16_t lh = (lcd.dimensions().height - 2) * mult;
    oti=open_text_info("\x7E\x7E:\x7E\x7E",text_font,text_font.scale(lh));
    text_bounds = (rect16)text_font.measure_text(ssize16::max(),oti.offset,oti.text,oti.scale,oti.scaled_tab_width,oti.encoding,oti.cache).bounds();
    text_bounds=text_bounds.center(fb.bounds());
      
}

void loop()
{
  static uint32_t ntp_ts = 0;
  switch(connect_state) {
    case 0:
        Serial.println("WiFi Connecting");
        if(ssid==NULL) {
          WiFi.begin();
        } else {
          WiFi.begin(ssid, pass);
        }
        connect_state = 1;
        break;
    case 1:
      if(WiFi.status()==WL_CONNECTED) {
        Serial.println("WiFi Connected");
        ntp_ip = false;
        connect_state = 2;
        WiFi.hostByName(ntp_server, ntp_ip);
        Serial.print("NTP IP: ");
        Serial.println(ntp_ip.toString());
        ip_loc::fetch(&latitude, &longitude, &utc_offset, region, 128, city, 128);
        Serial.print("City: ");
        Serial.println(city);
      }
      break;
    case 2:
      if (WiFi.status() == WL_CONNECTED) {
        connect_state = 3;
      }
      break;
    case 3:
      if (WiFi.status() != WL_CONNECTED) {
        connect_state = 0;
      } else {
        if(!ntp_ts || millis() > ntp_ts + 30*1000) {
          ntp_ts = millis();
          Serial.println("Sending NTP request");
          ntp.begin_request(ntp_ip,[] (time_t result, void* state) {
            Serial.println("NTP response received");
            current_time = utc_offset + result;
            got_time = true;
          });
        }
        
        ntp.update();
        
        if(got_time) {
          tim = *localtime(&current_time);
          connect_state = 3;
        }
      }

      break;
  }
  static uint32_t ts_sec = 0;
  static bool dot = false;
  if (!ts_sec || millis() > ts_sec + 1000) {
      ts_sec = millis();
      if (dot) {
          strftime(timbuf, sizeof(timbuf), "%H:%M", &tim);
      } else {
          strftime(timbuf, sizeof(timbuf), "%H %M", &tim);
      }
      dot = !dot;
      if(connect_state==3) {
        ++tim.tm_sec;
        if (tim.tm_sec == 60) {
            tim.tm_sec = 0;
            if (tim.tm_min == 59) {
                tim.tm_min = 0;
                if (tim.tm_hour == 23) {
                    tim.tm_hour = 0;
                } else {
                    ++tim.tm_hour;
                }
            } else {
                ++tim.tm_min;
            }
        }
      } else {
        tim.tm_hour = 12;
        tim.tm_min = 0;
        tim.tm_sec = 0;
      }
      fb.fill(fb.bounds(),color_t::dark_gray);
      typename lcd_t::pixel_type px = color_t::black.blend(color_t::white,0.42f);
      oti.text = "\x7E\x7E:\x7E\x7E";
      draw::text(fb,text_bounds,oti,px);
      oti.text = timbuf;
      px = color_t::black;
      draw::text(fb,text_bounds,oti,px);
      draw::wait_all_async(lcd);
      draw::bitmap_async(lcd,lcd.bounds(),fb,fb.bounds());
  }
  dimmer.wake();
  dimmer.update();
}

#if 0
static void get_build_tm(tm* out_tm) {
    char s_month[5];
    int month, day, year;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(__DATE__, "%s %d %d", s_month, &day, &year);
    month = (strstr(month_names, s_month) - month_names) / 3;

    out_tm->tm_mon = month;
    out_tm->tm_mday = day;
    out_tm->tm_year = year - 1900;
    out_tm->tm_isdst = -1;
    int d = day;        // Day     1-31
    int m = month + 1;  // Month   1-12`
    int y = year;
    // from https://stackoverflow.com/questions/6054016/c-program-to-find-day-of-week-given-date
    int weekday = (d += m < 3 ? y-- : y - 2, 23 * m / 9 + d + 4 + y / 4 - y / 100 + y / 400) % 7;
    out_tm->tm_wday = weekday;
    int hr, min, sec;
    sscanf(__TIME__, "%2d:%2d:%2d", &hr, &min, &sec);
    out_tm->tm_hour = hr;
    out_tm->tm_min = min;
    out_tm->tm_sec = sec;
}
#endif
