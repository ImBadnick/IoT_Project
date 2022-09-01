#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "mqtt-client.h"
#include "node-id.h"

#include <string.h>
#include <strings.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "energy-control"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif

/*---------------------------------------------------------------------------*/
/* MQTT broker address. */
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"

static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;

// Default config values
#define DEFAULT_BROKER_PORT         1883
#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)
#define PUBLISH_INTERVAL	          (5 * CLOCK_SECOND)


// We assume that the broker does not require authentication


/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;

#define STATE_INIT    		  0
#define STATE_NET_OK    	  1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_SUBSCRIBED      4
#define STATE_DISCONNECTED    5

/*---------------------------------------------------------------------------*/
PROCESS_NAME(energy_control_process);
PROCESS_NAME(alarm_control);
AUTOSTART_PROCESSES(&energy_control_process);

/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topics.
 * Make sure they are large enough to hold the entire respective string
 */
#define BUFFER_SIZE 64

static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];

// Periodic timer to check the state of the MQTT client
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
static struct etimer periodic_timer;

/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;

static struct mqtt_connection conn;

/*---------------------------------------------------------------------------*/
PROCESS(energy_control_process, "Energy Control process");

static int energy_consumption = 500;
static int energy_variation = 0;
static int add_or_sub = 0;

static bool sound_alarm_on = false;

/*---------------------------------------------------------------------------*/
static void pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk, uint16_t chunk_len) {
  printf("Pub Handler: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len, chunk_len);
  if(strcmp((const char *)topic, "led")==0){
      printf("Received Actuator command\n");
      if(strcmp((const char *)chunk, "low")==0){
          leds_on(LEDS_GREEN);
          printf("Energy consumed is low (<3000kW)\n");
      }else if (strcmp((const char *)chunk, "high")==0){
          leds_on(LEDS_RED);
          printf("Energy consumed is high (>3000kW)\n");
      }else{
          printf("UNKNOWN\n");
      }
      return;
    }
}


/*---------------------------------------------------------------------------*/
static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
    case MQTT_EVENT_CONNECTED: {
        state = STATE_CONNECTED;
        LOG_INFO("Application has a MQTT connection\n");
        break;
    }
    case MQTT_EVENT_DISCONNECTED: {
        state = STATE_DISCONNECTED;
        process_poll(&energy_control_process);
        printf("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));
        break;
    }
    case MQTT_EVENT_PUBLISH: {
        msg_ptr = data;
        pub_handler(msg_ptr->topic, strlen(msg_ptr->topic), msg_ptr->payload_chunk, msg_ptr->payload_length);
        break;
    }
    case MQTT_EVENT_SUBACK: {
        #if MQTT_311
            mqtt_suback_event_t *suback_event = (mqtt_suback_event_t *)data;

            if(suback_event->success) {
              LOG_INFO("Application is subscribed to topic successfully\n");
            } else {
              LOG_ERR("Application failed to subscribe to topic (ret code %x)\n", suback_event->return_code);
            }
        #else
            LOG_INFO("Application is subscribed to topic successfully\n");
        #endif
            break;
    }
    case MQTT_EVENT_UNSUBACK: {
        LOG_INFO("Application is unsubscribed to topic successfully\n");
        break;
    }
    case MQTT_EVENT_PUBACK: {
        LOG_INFO("Publishing complete.\n");
        break;
    }
    default:
        LOG_INFO("Application got a unhandled MQTT event: %i\n", event);
        break;
    }
  }

static bool have_connectivity(void) {
    if(uip_ds6_get_global(ADDR_PREFERRED) == NULL ||  uip_ds6_defrt_choose() == NULL) {
        return false;
    }
    return true;
}

static void alarm_handler(){
    sound_alarm_on = !sound_alarm_on;
    
    if(sound_alarm_on){
      process_start(&alarm_control, NULL);
    }
    else{
      process_exit(&alarm_control)
    }
}

int random_in_range(int a, int b) {
    int v = random_rand() % (b-a);
    return v + a;
}


static void simulate_energy_consumption(){
    add_or_sub = random_in_range(0,2)

    if (add_or_sub == 0) {
      //add energy consumption 
      energy_variation = random_in_range(0,500);
      if (energy_consumption + energy_variation < 3500) {
        energy_consumption += energy_variation;
      }
    }
    else {
      //sub energy consumption 
      energy_variation = random_in_range(0,energy_consumption-50);
      energy_consumption -= energy_variation;
    }

	  LOG_INFO("New energy consumption value: %d\n", energy_consumption);
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(energy_control_process, ev, data)
{

    PROCESS_BEGIN();
    
    mqtt_status_t status;
    char broker_address[CONFIG_IP_ADDR_STR_LEN];

    printf("Energy Controller Process\n");

    // Initialize the ClientID as MAC address
    snprintf(client_id, BUFFER_SIZE, "%02x%02x%02x%02x%02x%02x",
                      linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                      linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                      linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

    // Broker registration					 
    mqtt_register(&conn, &energy_control_process, client_id, mqtt_event, MAX_TCP_SEGMENT_SIZE);
				  
    state=STATE_INIT;
              
    // Initialize periodic timer to check the status 
    etimer_set(&periodic_timer, PUBLISH_INTERVAL);

    /* Main loop */
    while(1) {

        PROCESS_YIELD();

        if((ev == PROCESS_EVENT_TIMER && data == &periodic_timer) || ev == PROCESS_EVENT_POLL || ev == button_hal_press_event){
                    
            if(state==STATE_INIT){
              if(have_connectivity()==true)  
                  state = STATE_NET_OK;
            } 
          
          if(state == STATE_NET_OK){

              memcpy(broker_address, broker_ip, strlen(broker_ip));
              
              mqtt_connect(&conn, broker_address, DEFAULT_BROKER_PORT, (DEFAULT_PUBLISH_INTERVAL * 3) / CLOCK_SECOND, MQTT_CLEAN_SESSION_ON);

              state = STATE_CONNECTING;

              LOG_INFO("Connected to MQTT server \n")
          }
          
          if(state==STATE_CONNECTED){
              // Subscribe to a topic
              strcpy(sub_topic,"led");

              status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);

              printf("Subscribing!\n");

              if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
                  LOG_ERR("Tried to subscribe but command queue was full!\n");
                  PROCESS_EXIT();
              }
              
              state = STATE_SUBSCRIBED;
          }

        if(state == STATE_SUBSCRIBED){
            // Publish something
            sprintf(pub_topic, "%s", "energy-consumption");

            if(ev == button_hal_press_event){
					      button_hal_button_t* btn = (button_hal_button_t*)data;
                if (btn->unique_id == BOARD_BUTTON_HAL_INDEX_KEY_LEFT) {
                    alarm_handler();
                }
            }

            simulate_energy_consumption()
            
            sprintf(app_buffer, APP_BUFFER_SIZE, "{\"node\": %d, \"energy_consumption\": %d, \"timestamp\": %lu , \"manual\": %d}", 
                    node_id, energy_consumption, clock_seconds(), (int) manual_on_off);
                    
            LOG_INFO("message: %s\n", app_buffer);

            mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
          
        } else if ( state == STATE_DISCONNECTED ){
            LOG_ERR("Disconnected form MQTT broker\n");	
            state = STATE_INIT;
        }
        
        etimer_set(&periodic_timer, PUBLISH_INTERVAL);
      }

    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/


PROCESS(alarm_control, "Alarm Control process");

static struct etimer alarm_timer;

PROCESS_THREAD(alarm_control, ev, data){

  etimer_set(&alarm_timer, 5*CLOCK_SECOND);

  while(1){
      PROCESS_YIELD();
      if (ev == PROCESS_EVENT_TIMER){
          printf("SOUND ALERT SIMULATION!")
      }
  }
  
  PROCESS_END();
}
        