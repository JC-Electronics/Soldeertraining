
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <nvs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "http.h"
#include "driver/i2s.h"

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/tcp.h"

#include "ui.h"
#include "spiram_fifo.h"
#include "audio_renderer.h"
#include "web_radio.h"
#include "playerconfig.h"
#include "app_main.h"
#ifdef CONFIG_BT_SPEAKER_MODE
#include "bt_speaker.h"
#endif

#include "bt_config.h"
#include "driver/gpio.h"
#include "xi2c.h"
#include "fonts.h"
#include "ssd1306.h"
#include "nvs_flash.h"

#include "driver/touch_pad.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

static const char* TAG = "Touch pad";
#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (99)

static bool s_pad_activated[TOUCH_PAD_MAX];
static uint32_t s_pad_init_val[TOUCH_PAD_MAX];

#define I2C_EXAMPLE_MASTER_SCL_IO 32		  	/*!< gpio number for I2C master clock */
#define I2C_EXAMPLE_MASTER_SDA_IO 33			/*!< gpio number for I2C master data  */
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_1   		/*!< I2C port number for master dev	  */
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer 	  */
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer    */
#define I2C_EXAMPLE_MASTER_FREQ_HZ    400000    /*!< I2C master clock frequency 	  */

const static char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

const static char http_t[] = "<html><head><title>ESP32 PCM5122 webradio</title></head><body><h1>ESP32 PCM5122 webradio</h1><h2>Station list</h2><ul>";
const static char http_e[] = "</ul><a href=\"P\">prev</a>&nbsp;<a href=\"N\">next</a></body></html>";

/* */

#define NVSNAME "STATION"
#define MAXURLLEN 128
#define MAXSTATION 10

// Default station when NVRAM is empty.
static const char *preset_url = "http://icecast.omroep.nl/radio2-bb-mp3"; // preset station URL

static uint8_t stno; // current station index no 
static uint8_t stno_max; // number of stations registered
static char sturl[MAXURLLEN]; // current station URL

static const char *key_i = "i";
static const char *key_n = "n";

// Write command for PCM5122 DAC (Writes 2 bytes)
void PCM5122_WRITECOMMAND(uint8_t reg, uint8_t command)
{
  int ret;
   ret = X_WrByte(I2C_NUM_1,0x4C,reg,command); 
        if (ret == ESP_FAIL) {
            printf("I2C Fail\n");
        }
}

// Write command for PCM5122 DAC (writes 1 byte)
void PCM5122_WRITECOMMAND1(uint8_t reg)
{
   int ret;
   ret = X_WrByte1(I2C_NUM_1,0x4C,reg); 
        if (ret == ESP_FAIL) {
            printf("I2C Fail\n");
        }
}

// PCM5122 DAC Initialization
void(PCM5122_init(void)) {
#define TAG "DAC"
	ESP_LOGI(TAG,"Initializing PCM5122 DAC");
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x81,0x11);	// Select Page 0, Register 01. Set 'Reset Module' & Set 'Reset Mode Registers'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x81,0x00);	// Select Page 0, Register 01. Reset 'Reset Module' & Reset 'Reset Mode Registers'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x10);	// Select Page 0, Register 02. Set 'Standby mode'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x11);	// Select Page 0, Register 02. Set 'Standby mode' & Set 'Powerdown Request'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBD,0x3E);	// Select Page 0, Register 61. Set Attenuation
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBE,0x3E);	// Select Page 0, Register 62. Set Attenuation
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x10);	// Select Page 0, Register 02. Set 'Standby mode'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xA5,0x08);	// Select Page 0, Register 37. Set 'Ignore SCK Halt Detection'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x8D,0x10);	// Select Page 0, Register 13. Set 'The PLL Reference Clock is BCK'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x00);	// Select Page 0, Register 02. Reset 'Standby mode' & Reset 'Powerdown Request'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x10);	// Select Page 0, Register 02. Set 'Standby mode'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x11);	// Select Page 0, Register 02. Set 'Standby mode' & Set 'Powerdown Request'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x16);	// Select Page 0, Register 02. Reset 'Standby mode'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0x82,0x00);	// Select Page 0, Register 02. Reset 'Standby mode' & Reset 'Powerdown Request'
//	PCMVOLUME(I2C_NUM_1,0x4C,0x80,0x80);		// Set Attenuation to -70dB

	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBD,0x80);	// Select Page 0, Register 02. Reset 'Standby mode'
	PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBE,0x80);	// Select Page 0, Register 02. Reset 'Standby mode' & Reset 'Powerdown Request'
    
    
    
}

// Default volume level
static int VolLevel = 0x80;           // Set Attenuation to -70dB (0xBC)


  /*
    Handle an interrupt triggered when a pad is touched.
    Recognize what pad has been touched and save it in a table.
   */
  static void tp_example_rtc_intr(void * arg)
  {
	  uint32_t pad_intr = touch_pad_get_status();
	  touch_pad_clear_status(); //clear interrupt

	  if ((pad_intr >> 0) & 0x01) { s_pad_activated[0] = true; }	// Touchbutton "DOWN"
      if ((pad_intr >> 3) & 0x01) { s_pad_activated[3] = true; }	// Touchbutton "UP"
	  if ((pad_intr >> 4) & 0x01) { s_pad_activated[4] = true; }	// Touchbutton "CENTER"
      if ((pad_intr >> 5) & 0x01) { s_pad_activated[5] = true; }	// Touchbutton "LEFT"
	  if ((pad_intr >> 6) & 0x01) { s_pad_activated[6] = true; }	// Touchbutton "RIGHT"


  }

  /*
   * Before reading touch pad, we need to initialize the RTC IO.
   */
  static void tp_example_touch_pad_init()
  {
	  touch_pad_config(0, TOUCH_THRESH_NO_USE);
	  touch_pad_config(3, TOUCH_THRESH_NO_USE);
	  touch_pad_config(4, TOUCH_THRESH_NO_USE);
	  touch_pad_config(5, TOUCH_THRESH_NO_USE);
	  touch_pad_config(6, TOUCH_THRESH_NO_USE);


  }

  /*
    Read values sensed at all available touch pads.
    Use 2 / 3 of read value as the threshold
    to trigger interrupt when the pad is touched.
    Note: this routine demonstrates a simple way
    to configure activation threshold for the touch pads.
    Do not touch any pads when this routine
    is running (on application start).
   */
  static void tp_example_set_thresholds(void)
  {
  	  uint16_t touch_value;
  	  vTaskDelay(500/portTICK_PERIOD_MS);



	    touch_pad_read_filtered(TOUCH_PAD_NUM0, &touch_value);
      s_pad_init_val[TOUCH_PAD_NUM0] = touch_value;
      ESP_LOGI(TAG, "test init touch val: %d\n", touch_value);
      ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD_NUM0, touch_value * 2 / 3));

		touch_pad_read_filtered(TOUCH_PAD_NUM3, &touch_value);
      s_pad_init_val[TOUCH_PAD_NUM3] = touch_value;
      ESP_LOGI(TAG, "test init touch val: %d\n", touch_value);
      ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD_NUM3, touch_value * 2 / 3));

		touch_pad_read_filtered(TOUCH_PAD_NUM4, &touch_value);
      s_pad_init_val[TOUCH_PAD_NUM4] = touch_value;
      ESP_LOGI(TAG, "test init touch val: %d\n", touch_value);
      ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD_NUM4, touch_value * 2 / 3));

		touch_pad_read_filtered(TOUCH_PAD_NUM5, &touch_value);
      s_pad_init_val[TOUCH_PAD_NUM5] = touch_value;
      ESP_LOGI(TAG, "test init touch val: %d\n", touch_value);
      ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD_NUM5, touch_value * 2 / 3));

		touch_pad_read_filtered(TOUCH_PAD_NUM6, &touch_value);
      s_pad_init_val[TOUCH_PAD_NUM6] = touch_value;
      ESP_LOGI(TAG, "test init touch val: %d\n", touch_value);
      ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD_NUM6, touch_value * 2 / 3));

  }

  /*
    Check if any of touch pads has been activated
    by reading a table updated by rtc_intr()
    If so, then print it out on a serial monitor.
    Clear related entry in the table afterwards

    In interrupt mode, the table is updated in touch ISR.

    In filter mode, we will compare the current filtered value with the initial one.
    If the current filtered value is less than 99% of the initial value, we can
    regard it as a 'touched' event.
    When calling touch_pad_init, a timer will be started to run the filter.
    This mode is designed for the situation that the pad is covered
    by a 2-or-3-mm-thick medium, usually glass or plastic.
    The difference caused by a 'touch' action could be very small, but we can still use
    filter mode to detect a 'touch' event.
   */
  static bool Muted;
  static bool WasMuted;

  static void tp_example_read_task(void *pvParameter)
  {

	  // DOWN   pad: T0
	  // UP     pad: T3
	  // CENTER pad: T4
	  // RIGHT  pad: T5
	  // LEFT   pad: T6

	  char str[15];
	  bool ProcessVolume = 0;
	  sprintf(str, "Volume: -%d dB          ", ((VolLevel-48)/2));

	  static int show_message;
	  while (1) {
		  //interrupt mode, enable touch interrupt
		  touch_pad_intr_enable();

		  if (s_pad_activated[4] == true)
		  {
			  ESP_LOGI(TAG, "T%d activated!", 4);
			  vTaskDelay(200 / portTICK_PERIOD_MS);
			  s_pad_activated[4] = false;
			  Muted =! Muted;
		  }

		  if (s_pad_activated[5] == true) { ESP_LOGI(TAG, "T%d activated!", 5); vTaskDelay(200 / portTICK_PERIOD_MS); s_pad_activated[5] = false;}	// Not yet assigned
		  if (s_pad_activated[6] == true) { ESP_LOGI(TAG, "T%d activated!", 6); vTaskDelay(200 / portTICK_PERIOD_MS); s_pad_activated[6] = false;}	// Not yet assigned

		  if (s_pad_activated[3] == true) {
			  ESP_LOGI(TAG, "T%d activated!", 3);
			  ESP_LOGI(TAG, "Volume UP touched");
			  // Wait a while for the pad being released
			  vTaskDelay(200 / portTICK_PERIOD_MS);
			  // Clear information on pad activation
			  s_pad_activated[3] = false;
			  // Reset the counter triggering a message
			  // that application is running
			  show_message = 1;
			  if(VolLevel<=49 || Muted)
			  {
				  ESP_LOGI(TAG,"Max. Volume reached (%d) or muted.",VolLevel);
			  }
			  else
			  {
				  ProcessVolume=1;
				  VolLevel=VolLevel-2;

				  ESP_LOGI(TAG, "Volumelevel: %d", VolLevel);
			  }
		  }

		  if (s_pad_activated[0] == true) {
			  ESP_LOGI(TAG, "T%d activated!", 0);
			  ESP_LOGI(TAG, "Volume DOWN touched");
			  // Wait a while for the pad being released
			  vTaskDelay(200 / portTICK_PERIOD_MS);
			  // Clear information on pad activation
			  s_pad_activated[0] = false;
			  // Reset the counter triggering a message
			  // that application is running
			  show_message = 1;

			  if(VolLevel>=254 || Muted)
			  {
				  ESP_LOGI(TAG,"Min. Volume reached (%d) or muted.",VolLevel);
			  }
			  else
			  {
				  ProcessVolume=1;
				  VolLevel=VolLevel+2;
				  ESP_LOGI(TAG, "Volumelevel: %d", VolLevel);
			  }
		  }

if(Muted)
		  {
			  //int ret;
              PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBD,0xff);	// Select Page 0, Register BD.
              PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBE,0xff);	// Select Page 0, Register BE.
			  //ret = PCMVOLUME(I2C_NUM_1,0x4C,0xff,0xff);
			  //if (ret == ESP_FAIL) {
			//	  printf("I2C Fail\n");
			 // }
			  WasMuted = 1;
			  vTaskDelay(100 / portTICK_PERIOD_MS);
		  }
		  else
		  {

		  if(ProcessVolume == 1 || WasMuted == 1)
		  {
		//	  int ret;
			  //ret = PCMVOLUME(I2C_NUM_1,0x4C,VolLevel,VolLevel);
              
              PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBD,VolLevel);	// Select Page 0, Register BD.
              PCMCONTROL(I2C_NUM_1,0x4C,0x80,0xBE,VolLevel);	// Select Page 0, Register BE.
              
		//	  if (ret == ESP_FAIL) {
		//		  printf("I2C Fail\n");
		//	  }
			  vTaskDelay(100 / portTICK_PERIOD_MS);
			  ProcessVolume = 0;
			  WasMuted = 0;
		  }
		  }
		  vTaskDelay(10 / portTICK_PERIOD_MS);

		  // If no pad is touched, every couple of seconds, show a message
		  // that application is running
		  if (show_message++ % 500 == 0) {
			  ESP_LOGI(TAG, "Waiting for any pad being touched...");
		  }
	  }
  }

char *init_url(int d) {
  nvs_handle h;
  char index[2] = { '0', '\0' };
  size_t length = MAXURLLEN;
  esp_err_t e;

  nvs_open(NVSNAME, NVS_READWRITE, &h);
#if 0
  nvs_erase_all(h);
#endif

  if (nvs_get_u8(h, key_n, &stno_max) != ESP_OK) {
    stno = 0;
    stno_max = 1;
    nvs_set_u8(h, key_i, stno);
    nvs_set_u8(h, key_n, stno_max); 
    nvs_set_str(h, index, preset_url);
  }

  nvs_get_u8(h, key_i, &stno);

  while (1) {
    if (stno + d >= 0)
      stno = (stno + d) % stno_max;
    else
      stno = (stno + d + stno_max) % stno_max;
    index[0] = '0' + stno;
    e = nvs_get_str(h, index, sturl, &length);
    if (e == ESP_OK) break;
    if (abs(d) > 1) d = d / abs(d);
  }

  if (d != 0) nvs_set_u8(h, key_i, stno);

  nvs_commit(h);
  nvs_close(h);

  printf("init_url(%d) stno=%d, stno_max=%d, sturl=%s\n", d, stno, stno_max, sturl);

  return sturl;
}

char *set_url(int d, char *url) {
  nvs_handle h;
  char index[2] = { '0', '\0' };
  size_t length = MAXURLLEN;

  printf("set_url(%d, %s) stno_max=%d\n", d, url, stno_max);
  
  if (strlen(url) >= MAXURLLEN) return NULL;

  if (d > stno_max || d < 0) d = stno_max;
  if (d == stno_max) stno_max++;
  if (stno_max > MAXSTATION) return NULL; // error

  nvs_open(NVSNAME, NVS_READWRITE, &h);

  stno = d;
  index[0] = '0' + stno;
  nvs_set_u8(h, key_n, stno_max);
  nvs_set_str(h, index, url);
  nvs_commit(h);
  nvs_get_str(h, index, sturl, &length);

  nvs_commit(h);
  nvs_close(h);

  return sturl;
}

char *get_url() {
  return sturl;
}

char *get_nvurl(int n, char *buf, size_t length) {
  nvs_handle h;
  char index[2] = { '0', '\0' };
  // length = MAXURLLEN;

  n %= stno_max;

  nvs_open(NVSNAME, NVS_READWRITE, &h);
  index[0] += n;
  if (nvs_get_str(h, index, buf, &length) != OK) {
    buf[0] = '\0';
  }
  nvs_close(h);

  return buf;
}

void erase_nvurl(int n) {
  nvs_handle h;
  char index[2] = { '0', '\0' };
  
  n %= stno_max;
  nvs_open(NVSNAME, NVS_READWRITE, &h);
  index[0] += n;
  nvs_erase_key(h, index);

  stno_max--;
  nvs_set_u8(h, key_n, stno_max);

  for (;n < stno_max; n++) {
    char buf[MAXURLLEN];
    size_t length = MAXURLLEN;

    index[0] = '0' + n + 1;
    nvs_get_str(h, index, buf, &length);
    index[0]--;
    nvs_set_str(h, index, buf);
  }

  nvs_commit(h);
  nvs_close(h);
}

/* */

xSemaphoreHandle print_mux;

static char *surl = NULL;
static char ip[16];
static int x = 0;
static int l = 0;

#ifdef CONFIG_SSD1306_6432
#define XOFFSET 31
#define YOFFSET 32
#define WIDTH 64
#define HEIGHT 32
#else
#define WIDTH 128
#define HEIGHT 64
#define XOFFSET 0
#define YOFFSET 0
#endif

void oled_scroll(void) {
	// Show volume level
	if(Muted)
	{
		SSD1306_GotoXY(2, 4);
		SSD1306_Puts("Volume: Mute       ", &Font_7x10, SSD1306_COLOR_WHITE);
		  SSD1306_UpdateScreen();
	}
	else
	{


	  char str[15];
	  sprintf(str, "Volume: -%d dB               ", ((VolLevel-48)/2));
	SSD1306_GotoXY(2, 4);
	SSD1306_Puts(str, &Font_7x10, SSD1306_COLOR_WHITE);
	  SSD1306_UpdateScreen();
	}

  if (surl == NULL) return;
  while (l) {
    vTaskDelay(20/portTICK_RATE_MS);
  }
  int w = strlen(surl) * 7;
  if (w <= WIDTH) return;




  SSD1306_GotoXY(2 - x, 36);
  SSD1306_Puts(surl, &Font_7x10, SSD1306_COLOR_WHITE);


  x++;
  if (x > w) x = -WIDTH;
  SSD1306_UpdateScreen();
}

void i2c_test(int mode)
{
    char *url = get_url(); // play_url();
    x = 0;
    surl = url;

    SSD1306_Fill(SSD1306_COLOR_BLACK); // clear screen
#ifdef CONFIG_SSD1306_6432
    SSD1306_GotoXY(XOFFSET + 2, YOFFSET); // 31, 32);
    SSD1306_Puts("ESP32PICO", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(XOFFSET - x, YOFFSET + 10);
    SSD1306_Puts(surl, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(XOFFSET - x, YOFFSET + 20);
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    strcpy(ip, ip4addr_ntoa(&ip_info.ip));
    SSD1306_Puts(ip + 3, &Font_7x10, SSD1306_COLOR_WHITE);
#else
 //   SSD1306_GotoXY(40, 4);
//    SSD1306_Puts("ESP32", &Font_11x18, SSD1306_COLOR_WHITE);
    
    SSD1306_GotoXY(2, 20);
#ifdef CONFIG_BT_SPEAKER_MODE /////bluetooth speaker mode/////
    SSD1306_Puts("PCM5122 BT speaker", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 30);
    SSD1306_Puts("Device name is", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 39);
    SSD1306_Puts(dev_name, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(16, 53);
    SSD1306_Puts("Yeah! Speaker!", &Font_7x10, SSD1306_COLOR_WHITE);
#else ////////for webradio mode display////////////////
    SSD1306_Puts("PCM5122 webradio", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 30);
    if (mode) {
      SSD1306_Puts("web server is up.", &Font_7x10, SSD1306_COLOR_WHITE);
    } else {
      //SSD1306_Puts(url, &Font_7x10, SSD1306_COLOR_WHITE);
      if (strlen(url) > 18)  {
	SSD1306_GotoXY(2, 39);
	//SSD1306_Puts(url + 18, &Font_7x10, SSD1306_COLOR_WHITE);
      }
      SSD1306_GotoXY(16, 53);
    }

    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    SSD1306_GotoXY(2, 53);
    SSD1306_Puts("IP:", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_Puts(ip4addr_ntoa(&ip_info.ip), &Font_7x10, SSD1306_COLOR_WHITE);
#endif
#endif
    /* Update screen, send changes to LCD */
    SSD1306_UpdateScreen();

}

/**
 * @brief i2c master initialization
 */
static void i2c_example_master_init()
{
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                       I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
}

#define WIFI_LIST_NUM   10

#define TAG "main"


//Priorities of the reader and the decoder thread. bigger number = higher prio
#define PRIO_READER configMAX_PRIORITIES -3
#define PRIO_MQTT configMAX_PRIORITIES - 3
#define PRIO_CONNECT configMAX_PRIORITIES -1



/* event handler for pre-defined wifi events */
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    EventGroupHandle_t wifi_event_group = ctx;

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;

    default:
        break;
    }

    return ESP_OK;
}

static void initialise_wifi(EventGroupHandle_t wifi_event_group)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, wifi_event_group) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_FLASH) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


static void set_wifi_credentials()
{
    wifi_config_t current_config;
    esp_wifi_get_config(WIFI_IF_STA, &current_config);

    // no changes? return and save a bit of startup time
    if(strcmp( (const char *) current_config.sta.ssid, WIFI_AP_NAME) == 0 &&
       strcmp( (const char *) current_config.sta.password, WIFI_AP_PASS) == 0)
    {
        ESP_LOGI(TAG, "keeping wifi config: %s", WIFI_AP_NAME);
        return;
    }

    // wifi config has changed, update
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_AP_NAME,
            .password = WIFI_AP_PASS,
            .bssid_set = 0,
        },
    };

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    ESP_LOGI(TAG, "connecting");
    esp_wifi_connect();
}

static void init_hardware()
{
    nvs_flash_init();

    //Initialize the SPI RAM chip communications and see if it actually retains some bytes. If it
    //doesn't, warn user.
    if (!spiRamFifoInit()) {
        printf("\n\nSPI RAM chip fail!\n");
        while(1);
    }

    ESP_LOGI(TAG, "hardware initialized");
}

static void start_wifi()
{
    ESP_LOGI(TAG, "starting network");

    /* FreeRTOS event group to signal when we are connected & ready to make a request */
    EventGroupHandle_t wifi_event_group = xEventGroupCreate();

    /* init wifi */
    ui_queue_event(UI_CONNECTING);
    initialise_wifi(wifi_event_group);
    set_wifi_credentials();

    /* Wait for the callback to set the CONNECTED_BIT in the event group. */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    ui_queue_event(UI_CONNECTED);
}

static void http_server(void *pvParameters);

static renderer_config_t *create_renderer_config()
{
    renderer_config_t *renderer_config = calloc(1, sizeof(renderer_config_t));

    renderer_config->bit_depth = I2S_BITS_PER_SAMPLE_16BIT;
    renderer_config->i2s_num = I2S_NUM_0;
    renderer_config->sample_rate = 44100;
    renderer_config->sample_rate_modifier = 1.0;
    renderer_config->output_mode = AUDIO_OUTPUT_MODE;

    if(renderer_config->output_mode == I2S_MERUS) {
        renderer_config->bit_depth = I2S_BITS_PER_SAMPLE_32BIT;
    }

    if(renderer_config->output_mode == DAC_BUILT_IN) {
        renderer_config->bit_depth = I2S_BITS_PER_SAMPLE_16BIT;
    }

    return renderer_config;
}

web_radio_t *radio_config = NULL;

static void start_web_radio()
{
    printf("start_web_radio\n");

    init_url(0); // init_station(0);

    if (radio_config == NULL) {
      // init web radio
      radio_config = calloc(1, sizeof(web_radio_t));

      // init player config
      radio_config->player_config = calloc(1, sizeof(player_t));
      radio_config->player_config->command = CMD_NONE;
      radio_config->player_config->decoder_status = UNINITIALIZED;
      radio_config->player_config->decoder_command = CMD_NONE;
      radio_config->player_config->buffer_pref = BUF_PREF_SAFE;
      radio_config->player_config->media_stream = calloc(1, sizeof(media_stream_t));

      // init renderer
      renderer_init(create_renderer_config());
    }

    radio_config->url = get_url(); // play_url(); /* PLAY_URL; */

    xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);
    if (gpio_get_level(GPIO_NUM_0) == 0) {
      while (1) 
        vTaskDelay(200/portTICK_RATE_MS);
    }

    // start radio
    web_radio_init(radio_config);
    web_radio_start(radio_config);
}

/*
   web interface
 */

static void
http_server_netconn_serve(struct netconn *conn)
{
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;

  int np = 0;
  extern void software_reset();

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);

    /* Is this an HTTP GET command? (only check the first 5 chars, since
    there are other formats for GET, and we're keeping it very simple )*/
    if (buflen>=5 &&
        buf[0]=='G' &&
        buf[1]=='E' &&
        buf[2]=='T' &&
        buf[3]==' ' &&
        buf[4]=='/' ) {
      printf("%c\n", buf[5]);
      /* Send the HTML header
       * subtract 1 from the size, since we dont send the \0 in the string
       * NETCONN_NOCOPY: our data is const static, so no need to copy it
       */
      if (buflen > 5) {
        switch (buf[5]) {
        case 'N':
          np = 1; break;
        case 'P':
          np = -1; break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          {
            int i = buf[5] - '0';
            if (i > stno_max) i = stno_max;
            if (buf[6] == '+') {
              if (strncmp(buf + 7, "http://", 7) == 0) {
                np = i - stno;
                if (i == stno_max) stno_max++;
                char *p = strchr(buf + 7, ' ');
                *p = '\0';
                set_url(i, buf + 7);
              }
            } else if (buf[6] == '-') {
              erase_nvurl(i);
              np = -1;
            } else {
              np = i - stno;
            }
          }
        default:
          break;
        }
      }

      netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
      if (np != 0) init_url(np);
      /* Send our HTML page */
      netconn_write(conn, http_t, sizeof(http_t)-1, NETCONN_NOCOPY);
      for (int i = 0; i < stno_max; i++) {
        char buf[MAXURLLEN];
        int length = MAXURLLEN;
        sprintf(buf, "<li><a href=\"/%d\">", i);
        netconn_write(conn, buf, strlen(buf), NETCONN_NOCOPY);
        get_nvurl(i, buf, length);
        if (i == stno) netconn_write(conn, "<b>", 3, NETCONN_NOCOPY);
        netconn_write(conn, buf, strlen(buf), NETCONN_NOCOPY);
        if (i == stno) netconn_write(conn, "</b> - now playing", 18, NETCONN_NOCOPY);
        netconn_write(conn, "</a></li>", 9, NETCONN_NOCOPY);
      }
      netconn_write(conn, http_e, sizeof(http_e)-1, NETCONN_NOCOPY);
    }
  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);

  if (np != 0) {
    netconn_delete(conn);
    vTaskDelay(3000/portTICK_RATE_MS);
    software_reset();
  }
}

static void http_server(void *pvParameters)
{
  struct netconn *conn, *newconn;
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);
  do {
     err = netconn_accept(conn, &newconn);
     if (err == ERR_OK) {
       http_server_netconn_serve(newconn);
       netconn_delete(newconn);
     }
   } while(err == ERR_OK);
   netconn_close(conn);
   netconn_delete(conn);
}


/**
 * entry point
 */
void app_main()
{
    print_mux = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "starting app_main()");
    ESP_LOGI(TAG, "RAM left: %u", esp_get_free_heap_size());

    init_hardware();

#ifdef CONFIG_BT_SPEAKER_MODE
    bt_speaker_start(create_renderer_config());
      i2c_example_master_init();
    PCM5122_init();     // DAC
    SSD1306_Init();		// OLED
      i2c_test(1);
 //   ChangeVolume(0,0);
	i2c_test(0);

//xTaskCreate(&ChangeVolume, "ChangeVolume", 2048, NULL, 5, NULL);
// Initialize touch pad peripheral, it will start a timer to run a filter
ESP_LOGI(TAG, "Initializing touch pad");
touch_pad_init();

// Initialize and start a software filter to detect slight change of capacitance.
touch_pad_filter_start(10);
// Set measuring time and sleep time
// In this case, measurement will sustain 0xffff / 8MHz = 8.19ms
// Meanwhile, sleep time between two measurement will be 0x1000 / 150kHz = 27.3 ms
touch_pad_set_meas_time(0x1000, 0xffff);

//set reference voltage for charging/discharging
// In this case, the high reference voltage will be 2.4V - 1.5V = 0.9V
// The low reference voltage will be 0.8V, so that the procedure of charging
// and discharging would be very fast.
touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_1V5);
// Init touch pad IO
tp_example_touch_pad_init();
// Set threshhold
tp_example_set_thresholds();
// Register touch interrupt ISR
touch_pad_isr_register(tp_example_rtc_intr, NULL);

// Start a task to show what pads have been touched
xTaskCreate(&tp_example_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
    
#else
    start_wifi();
    i2c_example_master_init();
      PCM5122_init();     // DAC
    SSD1306_Init();			// OLED
    i2c_test(1);

    start_web_radio();

    i2c_test(0);
    
 


//xTaskCreate(&ChangeVolume, "ChangeVolume", 2048, NULL, 5, NULL);
// Initialize touch pad peripheral, it will start a timer to run a filter
ESP_LOGI(TAG, "Initializing touch pad");
touch_pad_init();

// Initialize and start a software filter to detect slight change of capacitance.
touch_pad_filter_start(10);
// Set measuring time and sleep time
// In this case, measurement will sustain 0xffff / 8MHz = 8.19ms
// Meanwhile, sleep time between two measurement will be 0x1000 / 150kHz = 27.3 ms
touch_pad_set_meas_time(0x1000, 0xffff);

//set reference voltage for charging/discharging
// In this case, the high reference voltage will be 2.4V - 1.5V = 0.9V
// The low reference voltage will be 0.8V, so that the procedure of charging
// and discharging would be very fast.
touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_1V5);
// Init touch pad IO
tp_example_touch_pad_init();
// Set threshhold
tp_example_set_thresholds();
// Register touch interrupt ISR
touch_pad_isr_register(tp_example_rtc_intr, NULL);

// Start a task to show what pads have been touched
xTaskCreate(&tp_example_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
#endif
    ESP_LOGI(TAG, "RAM left %d", esp_get_free_heap_size());

    // ESP_LOGI(TAG, "app_main stack: %d\n", uxTaskGetStackHighWaterMark(NULL));
    while (1) {
 //     vTaskDelay(40/portTICK_RATE_MS);
//#ifdef CONFIG_SSD1306_6432
      oled_scroll();
//#endif
    }


#ifndef CONFIG_BT_SPEAKER_MODE // Y.H.Cha : Add this to run in Web radio mode only
xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);
#endif

}
