#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/node-id.h"
#include "random.h"

#include "global_variables.h"

#include "sys/log.h"

#define LOG_MODULE "temperature-sensor"
#define LOG_LEVEL LOG_LEVEL_APP

//API FUNCTION DEFINITIONS
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);


//RESOURCE DEFINITION
EVENT_RESOURCE(temperature_sensor,
	"title=\"Temperature Sensor\";obs;rt=\"temp\"",
	temperature_get_handler,
	NULL,
	NULL,
	NULL,
	temperature_event_handler);

static int default_temperature = 30;
static int temperature = 30;


static void simulate_temperature_values () {
    int variation = 0;

	if(ac_on) { 
	    if (temperature > ac_temperature) { variation = 1; }
	    else if (temperature == ac_temperature) { variation = 0; }  

	    temperature = temperature - variation;

	} else {
        variation = 1;

		if (temperature + variation < default_temperature) { 
			temperature = temperature + variation; 
		}
    }

}


//API FUNCTIONS IMPLEMENTATIONS
static void temperature_event_handler(void) {
	simulate_temperature_values();
    coap_notify_observers(&res_temperature_sensor);
}


static void temperature_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	
	unsigned int accept = -1;
	coap_get_header_accept(request, &accept);

	if (accept == -1) {
    	coap_set_header_content_format(response, APPLICATION_JSON);
		snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"node\": %d, \"temperature\": %d, \"ac_temperature\": %d, \"timestamp\": %lu}", (unsigned int) node_id, (int)temperature, (int) ac_temperature, clock_seconds());
		coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
	}

	else if (accept == APPLICATION_JSON){
		coap_set_header_content_format(response, APPLICATION_JSON);
		snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"node\": %d, \"temperature\": %d, \"timestamp\": %lu}", (unsigned int) node_id, (int)temperature, (int) ac_temperature, clock_seconds());
		coap_set_payload(response, buffer, strlen((char *)buffer));
	}
	else {
		coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
		const char *msg = "Supporting content-type application/json";
   		 coap_set_payload(response, msg, strlen(msg));
	}
}


