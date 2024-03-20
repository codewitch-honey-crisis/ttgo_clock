#include <Arduino.h>
#include <WiFi.h>
#include <gfx.hpp>
#include <ntp_time.hpp>
#include <ip_loc.hpp>

#ifdef TTGO_T1
#include <ttgo.hpp>
#endif
#ifdef M5STACK_CORE2
#include <tft_io.hpp>
#include <ili9341.hpp>
#include <ft6336.hpp>
#include <m5core2_power.hpp>
#define LCD_SPI_HOST VSPI
#define LCD_PIN_NUM_MOSI 23
#define LCD_PIN_NUM_CLK 18
#define LCD_PIN_NUM_CS 5
#define LCD_PIN_NUM_DC 15
using tft_bus_t = arduino::tft_spi_ex<LCD_SPI_HOST,LCD_PIN_NUM_CS,LCD_PIN_NUM_MOSI,-1,LCD_PIN_NUM_CLK,0,false>;
using lcd_t = arduino::ili9342c<LCD_PIN_NUM_DC,-1,-1,tft_bus_t,1>;
lcd_t lcd;
static m5core2_power power;
using touch_t = arduino::ft6336<280,320>;
touch_t touch(Wire1);
#endif

// set these to assign an SSID and pass for WiFi
constexpr static const char* ssid = nullptr;
constexpr static const char* pass = nullptr;

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
static uint8_t* lcd_buffer;
static int connect_state = 0;
static char timbuf[16];
static tm tim;
static ntp_time ntp;
static float latitude;
static float longitude;
static long utc_offset;
static char region[128];
static bool am_pm = false;
static char city[128];
static open_text_info oti;
static bool got_time = false;
static bool refresh = false;
static time_t current_time;
static IPAddress ntp_ip;
rect16 text_bounds;

void calculate_positioning() {
    refresh = true;
    lcd.fill(lcd.bounds(),color_t::dark_gray);
    float scl = text_font.scale(lcd.dimensions().height - 2);
    ssize16 dig_size = text_font.measure_text(ssize16::max(), spoint16::zero(), "0", scl);
    ssize16 am_pm_size = {0,0};
    int16_t w = (dig_size.width + 1) * 6;
    if(am_pm) {
      am_pm_size = text_font.measure_text(ssize16::max(), spoint16::zero(), ".", scl);
      w+=am_pm_size.width;
    }
    float mult = (float)(lcd.dimensions().width - 2) / (float)w;
    if (mult > 1.0f) mult = 1.0f;
    int16_t lh = (lcd.dimensions().height - 2) * mult;
    const char* str = am_pm?"\x7E\x7E:\x7E\x7E.":"\x7E\x7E:\x7E\x7E";
    oti=open_text_info(str,text_font,text_font.scale(lh));
    text_bounds = (rect16)text_font.measure_text(
      ssize16::max(),
      oti.offset,
      oti.text,
      oti.scale,
      oti.scaled_tab_width,
      oti.encoding,
      oti.cache).bounds();
    // set to the screen's width
    text_bounds.x2=text_bounds.x1+lcd.dimensions().width-1;
    text_bounds=text_bounds.center(lcd.bounds());
}
#ifdef TTGO_T1
void on_pressed_changed(bool pressed, void* state) {
  if(pressed) {
    am_pm = !am_pm;
    calculate_positioning();
  }
}
#endif
void setup()
{
    Serial.begin(115200);
#ifdef M5STACK_CORE2
    power.initialize();
    touch.initialize();
    touch.rotation(1);
#endif
#ifdef TTGO_T1
  ttgo_initialize();
  button_a_raw.on_pressed_changed(on_pressed_changed);
  button_b_raw.on_pressed_changed(on_pressed_changed);  
#endif
    lcd.initialize();
#ifdef TTGO_T1
    lcd.rotation(3);
#endif
    calculate_positioning();
    size_t sz = fb_type::sizeof_buffer(text_bounds.dimensions());
#ifdef BOARD_HAS_PSRAM
    lcd_buffer = (uint8_t*)ps_malloc(sz);
#else
    lcd_buffer = (uint8_t*)malloc(sz);
#endif
    if(lcd_buffer==nullptr) {
      Serial.println("Out of memory allocating LCD buffer");
      while(1);
    }
    lcd.fill(lcd.bounds(),color_t::dark_gray);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // get_build_tm(&tim);
    tim.tm_hour = 12;
    tim.tm_min = 0;
    tim.tm_sec = 0;    
}

void loop()
{
  static uint32_t ntp_ts = 0;
  switch(connect_state) {
    case 0: // DISCONNECTED
        Serial.println("WiFi Connecting");
        if(ssid==NULL) {
          WiFi.begin();
        } else {
          WiFi.begin(ssid, pass);
        }
        connect_state = 1;
        break;
    case 1: // CONNECTION ESTABLISHED
      if(WiFi.status()==WL_CONNECTED) {
        got_time = false;
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
    case 2: // CONNECTED
      if (WiFi.status() != WL_CONNECTED) {
        connect_state = 0;
      } else {
        if(!ntp_ts || millis() > ntp_ts + (clock_sync_seconds*got_time*1000)+((!got_time)*250)) {
          ntp_ts = millis();
          Serial.println("Sending NTP request");
          ntp.begin_request(ntp_ip,[] (time_t result, void* state) {
            Serial.println("NTP response received");
            current_time = utc_offset + result;
            got_time = true;
          });
        }
        ntp.update();
      }
      break;
  }
  static uint32_t ts_sec = 0;
  static bool dot = false;
  // once every second...
  if (!ts_sec || millis() > ts_sec + 1000) {
      refresh = true;
      ts_sec = millis();
      if(connect_state==2) { // is connected?
        ++current_time;
      } else {
        current_time = 12*60*60;
      }
      tim = *localtime(&current_time);
      if (dot) {
          if(am_pm) {
            if(tim.tm_hour>=12) {
              strftime(timbuf, sizeof(timbuf), "%I:%M.", &tim);
            } else {
              strftime(timbuf, sizeof(timbuf), "%I:%M", &tim);
            }
            if(tim.tm_hour%12<10) {
              *timbuf='!';
            }
          } else {
            strftime(timbuf, sizeof(timbuf), "%H:%M", &tim);
          }
      } else {
          if(am_pm) {
            if(tim.tm_hour>=12) {
              strftime(timbuf, sizeof(timbuf), "%I %M.", &tim);
            } else {
              strftime(timbuf, sizeof(timbuf), "%I %M", &tim);
            }
            if(tim.tm_hour%12<10) {
              *timbuf='!';
            }
          } else {
            strftime(timbuf, sizeof(timbuf), "%H %M", &tim);
          }
      }
      dot = !dot;
    }
    if(refresh) {
      refresh = false;
      fb_type fb(text_bounds.dimensions(),lcd_buffer);
      fb.fill(fb.bounds(),color_t::dark_gray);
      typename lcd_t::pixel_type px = color_t::black.blend(color_t::white,0.42f);
      if(am_pm) {
        oti.text = "\x7E\x7E:\x7E\x7E.";
      } else {
        oti.text = "\x7E\x7E:\x7E\x7E";
      }
      draw::text(fb,fb.bounds(),oti,px);
      oti.text = timbuf;
      px = color_t::black;
      draw::text(fb,fb.bounds(),oti,px);
  #ifdef BOARD_HAS_PSRAM
      draw::bitmap(lcd,text_bounds,fb,fb.bounds());
  #else
      draw::wait_all_async(lcd);
      draw::bitmap_async(lcd,text_bounds,fb,fb.bounds());
  #endif
  }
  #ifdef TTGO_T1
  dimmer.wake();
  ttgo_update();
  #endif
  #ifdef M5STACK_CORE2
  touch.update();

  uint16_t x,y;
  if(touch.xy(&x,&y)) {
    am_pm = !am_pm;
    calculate_positioning();
  }

  #endif
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
