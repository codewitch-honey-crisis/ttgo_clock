// uncomment the following for 14 segment display
// #define SEG14
#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
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
static lcd_t lcd;
static m5core2_power power;
using touch_t = arduino::ft6336<280,320>;
static touch_t touch(Wire1);
#endif
#ifdef M5STACK_TOUGH
#include <tft_io.hpp>
#include <ili9341.hpp>
#include <chsc6540.hpp>
#include <m5tough_power.hpp>
#define LCD_SPI_HOST VSPI
#define LCD_PIN_NUM_MOSI 23
#define LCD_PIN_NUM_MISO 38
#define LCD_PIN_NUM_CLK 18
#define LCD_PIN_NUM_CS 5
#define LCD_PIN_NUM_DC 15
using tft_bus_t = arduino::tft_spi_ex<LCD_SPI_HOST,LCD_PIN_NUM_CS,LCD_PIN_NUM_MOSI,LCD_PIN_NUM_MISO,LCD_PIN_NUM_CLK,0,false>;
using lcd_t = arduino::ili9342c<LCD_PIN_NUM_DC,-1,-1,tft_bus_t,1>;
static lcd_t lcd;
static m5tough_power power;
using touch_t = arduino::chsc6540<280,320,39>;
static touch_t touch(Wire1);
#endif

#ifdef M5STACK_S3_ATOM
#include <tft_io.hpp>
#include <st7789.hpp>
#include <button.hpp>
#define LCD_SPI_HOST SPI3_HOST
#define LCD_PIN_NUM_MOSI 21
#define LCD_PIN_NUM_CLK 17
#define LCD_PIN_NUM_CS 15
#define LCD_PIN_NUM_DC 33
#define LCD_PIN_NUM_RST 34
#define LCD_PIN_NUM_BCKL 16
using tft_bus_t = arduino::tft_spi_ex<LCD_SPI_HOST,LCD_PIN_NUM_CS,LCD_PIN_NUM_MOSI,-1,LCD_PIN_NUM_CLK,0,false>;
using lcd_t = arduino::st7789<128,128,LCD_PIN_NUM_DC,LCD_PIN_NUM_RST,LCD_PIN_NUM_BCKL,tft_bus_t,0>;
static lcd_t lcd;
#define BUTTON 41
using button_t = arduino::int_button<BUTTON,10,true>;
static button_t screen_button;
#endif

#ifdef LILYGO_T5_4_7
#include <lilygot54in7.hpp>
#include <button.hpp>
using lcd_t = arduino::lilygot54in7;
arduino::lilygot54in7 lcd;
#define BUTTON_A 39
#define BUTTON_B 34
#define BUTTON_C 35
#define BUTTON_D 0
using button_a_t = arduino::int_button<BUTTON_A,10,true>;
using button_b_t = arduino::int_button<BUTTON_B,10,true>;
using button_c_t = arduino::int_button<BUTTON_C,10,true>;
using button_d_t = arduino::int_button<BUTTON_D,10,true>;
static button_a_t button_a;
static button_b_t button_b;
static button_c_t button_c;
static button_d_t button_d;
#endif

// set these to assign an SSID and pass for WiFi
constexpr static const char* ssid = nullptr;
constexpr static const char* pass = nullptr;
// NTP server
constexpr static const char* ntp_server = "pool.ntp.org";
// synchronize with NTP every 60 seconds
constexpr static const int clock_sync_seconds = 60;

using namespace arduino;
using namespace gfx;
using color_t = color<typename lcd_t::pixel_type>;

#ifdef SEG14
// from https://www.keshikan.net/fonts-e.html
#define DSEG14CLASSIC_REGULAR_IMPLEMENTATION
#include <assets/DSEG14Classic_Regular.hpp>
static const open_font& text_font = DSEG14Classic_Regular;
#else
#define DSEG7CLASSIC_REGULAR_IMPLEMENTATION
#include <assets/DSEG7Classic_Regular.hpp>
static const open_font& text_font = DSEG7Classic_Regular;
#endif
#ifndef E_PAPER
constexpr static const auto back_color = color_t::dark_gray;
constexpr static const auto ghost_color = color_t::black.blend(color_t::white,0.42f);
#else
constexpr static const auto back_color = color_t::white;
constexpr static const auto ghost_color = color_t::black.blend(color_t::white,0.21f);
#endif
constexpr static const auto text_color = color_t::black;

// static const auto back_color = color_t::black;
// static const auto ghost_color = color_t::black;
// static const auto text_color = color_t::red;

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
static int am_pm = false;
static char city[128];
static open_text_info oti;
static bool got_time = false;
static bool refresh = false;
static time_t current_time;
static IPAddress ntp_ip;
#ifdef ESP32
static File file;
#endif
rect16 text_bounds;

void calculate_positioning() {
    refresh = true;
    lcd.fill(lcd.bounds(),back_color);
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
    #if SEG14
    const char* str = am_pm?"\x7E\x7E:\x7E\x7E.":"\x7E\x7E:\x7E\x7E";
    #else
    const char* str = am_pm?"88:88.":"88:88";
    #endif
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
#if defined(TTGO_T1) || defined(M5STACK_S3_ATOM) || defined(LILYGO_T5_4_7)
void on_pressed_changed(bool pressed, void* state) {
  if(pressed) {
    am_pm = !am_pm;
    calculate_positioning();
    file = SPIFFS.open("/settings","wb",true);
    file.seek(0);
    file.write((uint8_t*)&am_pm,sizeof(am_pm));
    file.close();
    Serial.print("Wrote settings: ");
    if(am_pm) {
      Serial.println("am/pm");
    } else {
      Serial.println("24-hr");
    }
  }
}
#endif
void setup()
{
    Serial.begin(115200);
#ifdef TTGO_T1
  ttgo_initialize();
  button_a_raw.on_pressed_changed(on_pressed_changed);
  button_b_raw.on_pressed_changed(on_pressed_changed);  
#endif
#if defined(M5STACK_CORE2) || defined(M5STACK_TOUGH)
    power.initialize();
    touch.initialize();
    touch.rotation(1);
#endif
#if M5STACK_S3_ATOM
    screen_button.initialize();
    screen_button.on_pressed_changed(on_pressed_changed);
#endif
#ifdef LILYGO_T5_4_7
  button_a.on_pressed_changed(on_pressed_changed);
  button_b.on_pressed_changed(on_pressed_changed);  
  button_c.on_pressed_changed(on_pressed_changed);
  button_d.on_pressed_changed(on_pressed_changed); 
#endif
    lcd.initialize();
#ifdef TTGO_T1
    lcd.rotation(3);
#endif
#ifdef LILYGO_T5_4_7
  lcd.rotation(1); 
#endif
    // must do this in 24hr mode
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
    lcd.fill(lcd.bounds(),back_color);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // get_build_tm(&tim);
    tim.tm_hour = 12;
    tim.tm_min = 0;
    tim.tm_sec = 0;
    SPIFFS.begin(true);
    if(SPIFFS.exists("/settings")) {
      Serial.println("Found settings");
      file = SPIFFS.open("/settings","rb");
      if(sizeof(am_pm)==file.read((uint8_t*)&am_pm,sizeof(am_pm))) {
        if(am_pm) {
          Serial.println("Read settings switched to am/pm");
          calculate_positioning();
        } else {
          Serial.println("Read settings switched to 24-hr");
        }
      }
      file.close();
    }
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
          got_time = false;
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
  static bool dot = true;
  
  // once every second...
  if (!ts_sec || millis() > ts_sec + 1000) {
#ifdef E_PAPER
      static bool first = true;
      if(0==current_time%60 || first) {
        refresh=true;
        first = false;
      }
      
#else
      refresh = true;
#endif
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
            if(*timbuf=='0') {
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
            if(*timbuf=='0') {
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
      fb.fill(fb.bounds(),back_color);
      auto px = ghost_color;
      if(am_pm) {
#if SEG14
        oti.text = "\x7E\x7E:\x7E\x7E.";
#else
        oti.text = "88:88.";
#endif
      } else {
#if SEG14
        oti.text = "\x7E\x7E:\x7E\x7E";
#else
        oti.text = "88:88";
#endif
      }
      draw::text(fb,fb.bounds(),oti,px);
      oti.text = timbuf;
      px = text_color;
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
  #if defined(M5STACK_CORE2) || defined(M5STACK_TOUGH)
  touch.update();
  uint16_t x,y;
  if(touch.xy(&x,&y)) {
    am_pm = !am_pm;
    calculate_positioning();
    file = SPIFFS.open("/settings","wb",true);
    file.seek(0);
    file.write((uint8_t*)&am_pm,sizeof(am_pm));
    file.close();
    Serial.print("Wrote settings: ");
    if(am_pm) {
      Serial.println("am/pm");
    } else {
      Serial.println("24-hr");
    }
  }
  #endif
  #ifdef M5STACK_S3_ATOM
    screen_button.update();
  #endif
  #ifdef LILYGO_T5_4_7
    button_a.update();
    button_b.update();
    button_c.update();
    button_d.update();
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
