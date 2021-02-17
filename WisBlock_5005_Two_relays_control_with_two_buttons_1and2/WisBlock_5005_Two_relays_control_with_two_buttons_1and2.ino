/**
 *
 * To configurate the WisBlock and start work with Arduino IDE use this tutorial: https://docs.rakwireless.com/Knowledge-Hub/Learn/Installation-of-Board-Support-Package-in-Arduino-IDE/
 *
 *
 * Ver. 1.1
 * Click Button 1 one time Relay one(1) -> ON 
 * Click Button 1 one time Relay one(1) -> OFF
 
 * Click Button 2 one time Relay two(2) -> ON
 * Click Button 2 one time Relay two(2) -> OFF
 
 * @note RAK5005-O GPIO mapping to RAK4631 GPIO ports
 * IO1 <-> P0.17 (Arduino GPIO number 17)
 * IO2 <-> P1.02 (Arduino GPIO number 34)
 * IO3 <-> P0.21 (Arduino GPIO number 21)
 * IO4 <-> P0.04 (Arduino GPIO number 4)
 * For more info about RAK5804 WisBlock IO Extension Module: https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5804/Datasheet/#overview
 */
#include <Arduino.h>
#include <LoRaWan-RAK4630.h>   //http://librarymanager/All#SX126x
#include <SPI.h>

// RAK4630 supply two LED
#ifndef LED_BUILTIN
#define LED_BUILTIN 35
#endif

#ifndef LED_BUILTIN2
#define LED_BUILTIN2 36
#endif

bool doOTAA = true;
#define SCHED_MAX_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE 60                      /**< Maximum number of events in the scheduler queue. */
#define LORAWAN_DATERATE DR_0                   /*LoRaMac datarates definition, from DR_0 to DR_5*/
#define LORAWAN_TX_POWER TX_POWER_5                 /*LoRaMac tx power definition, from TX_POWER_0 to TX_POWER_15*/
#define JOINREQ_NBTRIALS 3                      /**< Number of trials for the join request. */
DeviceClass_t gCurrentClass = CLASS_C;                /* class definition*/
lmh_confirm gCurrentConfirm = LMH_CONFIRMED_MSG;          /* confirm/unconfirm packet definition*/
uint8_t gAppPort = LORAWAN_APP_PORT;

#define LORAWAN_APP_PORT 1
#define LORAWAN_APP_PORT 2


/**@brief Structure containing LoRaWan parameters, needed for lmh_init()
 */
static lmh_param_t lora_param_init = {LORAWAN_ADR_ON, LORAWAN_DATERATE, LORAWAN_PUBLIC_NETWORK, JOINREQ_NBTRIALS, LORAWAN_TX_POWER, LORAWAN_DUTYCYCLE_OFF};

// Foward declaration
static void lorawan_has_joined_handler(void);
static void lorawan_rx_handler(lmh_app_data_t *app_data);
static void lorawan_confirm_class_handler(DeviceClass_t Class);


/**@brief Structure containing LoRaWan callback functions, needed for lmh_init()
*/
static lmh_callback_t lora_callbacks = {BoardGetBatteryLevel, BoardGetUniqueId, BoardGetRandomSeed,
                    lorawan_rx_handler, lorawan_has_joined_handler, lorawan_confirm_class_handler};

//OTAA keys
uint8_t nodeDeviceEUI[8] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x33, 0x33};
uint8_t nodeAppEUI[8] = {0x73, 0xe3, 0x11, 0xc4, 0x63, 0x47, 0xca, 0xd0};
uint8_t nodeAppKey[16] = {0xa6, 0xa5, 0x72, 0xbb, 0x99, 0xb4, 0xaf, 0x46, 0x8e, 0x64, 0x90, 0xf6, 0xe9, 0xe0, 0x0e, 0x5b};


// Private defination
#define LORAWAN_APP_DATA_BUFF_SIZE 64                     /**< buffer size of the data to be transmitted. */
#define LORAWAN_APP_INTERVAL 20000                        /**< Defines for user timer, the application data transmission interval. 20s, value in [ms]. */
static uint8_t m_lora_app_data_buffer[LORAWAN_APP_DATA_BUFF_SIZE];        //< Lora user application data buffer.
static lmh_app_data_t m_lora_app_data = {m_lora_app_data_buffer, 0, 0, 0, 0}; //< Lora user application data structure.

TimerEvent_t appTimer;
static uint32_t timers_init(void);
static uint32_t count = 0;
static uint32_t count_fail = 0;

int IO1 = 17;     //IO1 
int IO2 = 34;     //IO2

int hardcode = HIGH;   

int statuss = 0;



void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

 // ++++++++ DEFINE_PINMODE +++++
  pinMode(IO1, OUTPUT);       //1st Relay
  pinMode(IO2, OUTPUT);       //2nd Relay
 // ++++++++ DEFINE_PINMODE +++++
 
  // +++++ SET_START_MODES+++++
  digitalWrite(IO1, LOW);     //1st Relay
  digitalWrite(IO2, LOW);     //2nd Relay
  // +++++ SET_START_MODES+++++
  
  // Initialize LoRa chip.
  lora_rak4630_init();

  // Initialize Serial for debug output
  Serial.begin(115200);
  {
    delay(10);
  }
  Serial.println("=====================================");
  Serial.println("Welcome to RAK4630 LoRaWan!!!");
  Serial.println("Type: OTAA");

#if defined(REGION_AS923)
  Serial.println("Region: AS923");
#elif defined(REGION_AU915)
  Serial.println("Region: AU915");
#elif defined(REGION_CN470)
  Serial.println("Region: CN470");
#elif defined(REGION_CN779)
  Serial.println("Region: CN779");
#elif defined(REGION_EU433)
  Serial.println("Region: EU433");
#elif defined(REGION_IN865)
  Serial.println("Region: IN865");
#elif defined(REGION_EU868)
  Serial.println("Region: EU868");
#elif defined(REGION_KR920)
  Serial.println("Region: KR920");
#elif defined(REGION_US915)
  Serial.println("Region: US915");
#elif defined(REGION_US915_HYBRID)
  Serial.println("Region: US915_HYBRID");
#else
  Serial.println("Please define a region in the compiler options.");
#endif
  Serial.println("=====================================");

  //creat a user timer to send data to server period
  uint32_t err_code;
  err_code = timers_init();
  if (err_code != 0)
  {
    Serial.printf("timers_init failed - %d\n", err_code);
  }

  // Setup the EUIs and Keys
  lmh_setDevEui(nodeDeviceEUI);
  lmh_setAppEui(nodeAppEUI);
  lmh_setAppKey(nodeAppKey);

  // Initialize LoRaWan
  err_code = lmh_init(&lora_callbacks, lora_param_init, doOTAA);
  if (err_code != 0)
  {
    Serial.printf("lmh_init failed - %d\n", err_code);
  }

  // Start Join procedure
  lmh_join();
}

void loop()
{
  // Handle Radio events
  Radio.IrqProcess();
}

/**@brief LoRa function for handling HasJoined event.
 */
void lorawan_has_joined_handler(void)
{
  Serial.println("OTAA Mode, Network Joined!");

  lmh_error_status ret = lmh_class_request(gCurrentClass);
  if (ret == LMH_SUCCESS)
  {
    delay(1000);
    TimerSetValue(&appTimer, LORAWAN_APP_INTERVAL);
    TimerStart(&appTimer);
  }
}

/**@brief Function for handling LoRaWan received data from Gateway
 *
 * @param[in] app_data  Pointer to rx data
 */
void lorawan_rx_handler(lmh_app_data_t *app_data)
{
  Serial.printf("LoRa Packet received on port %d, size:%d, rssi:%d, snr:%d, data:%s\n",
          app_data->port, app_data->buffsize, app_data->rssi, app_data->snr, app_data->buffer);
  Serial.printf("data:%s\n", app_data->buffer);


 switch (app_data->port)
  {
   case 1:
    if (app_data->port == 1){
       digitalWrite(IO1, HIGH);
        }
    if (hardcode==HIGH && statuss==0){
      statuss=1;
      Serial.println(statuss);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(IO1, HIGH);
      Serial.printf("IO1_HIGH");
        }
     else if(hardcode==HIGH && statuss==1){
         statuss=0;
      Serial.println(statuss);
      Serial.print("IO1_LOW");
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
        }
    // IO1 OFF:   
    if(hardcode==HIGH && statuss==0){
    digitalWrite(IO1, LOW);
    hardcode=HIGH;
    }
    // IO1 ON:
    else if (hardcode==HIGH && statuss==1){
    digitalWrite(IO1, HIGH);
    hardcode=HIGH;
    } 
     break; 
    
   case 2:
    if (app_data->port == 2){
       digitalWrite(IO2, LOW);
        }
    if (hardcode==HIGH && statuss==0){
      statuss=1;
      Serial.println(statuss);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(IO2, HIGH);
      Serial.printf("IO2_HIGH");
        }
     else if(hardcode==HIGH && statuss==1){
         statuss=0;
      Serial.println(statuss);
      Serial.print("IO2_LOW");
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
        }
    // IO2 OFF:   
    if(hardcode==HIGH && statuss==0){
    digitalWrite(IO2, LOW);
    hardcode=HIGH;
    }
    // IO2 ON:
    else if (hardcode==HIGH && statuss==1){
    digitalWrite(IO2, HIGH);
    hardcode=HIGH;
    } 
   break;
  default:
    break;
  }
}

void lorawan_confirm_class_handler(DeviceClass_t Class)
{
  Serial.printf("switch to class %c done\n", "ABC"[Class]);
  // Informs the server that switch has occurred ASAP
  m_lora_app_data.buffsize = 0;
  m_lora_app_data.port = gAppPort;
  lmh_send(&m_lora_app_data, gCurrentConfirm);

}

/**@brief Function for handling user timerout event.
 */
void tx_lora_periodic_handler(void)
{
  TimerSetValue(&appTimer, LORAWAN_APP_INTERVAL);
  TimerStart(&appTimer);
  Serial.println("Listenig... for Downlink");
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
uint32_t timers_init(void)
{
  TimerInit(&appTimer, tx_lora_periodic_handler);
  return 0;
}
