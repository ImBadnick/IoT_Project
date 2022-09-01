from coapthon.server.coap import CoAP
from coapthon.resources.resource import Resource
from coapthon.client.helperclient import HelperClient
from coapthon.messages.request import Request
from coapthon.messages.response import Response
from coapthon import defines
import json
import time
import server
import threading
from database import Database
import tabulate
import logging


class MoteResource:

    def __init__(self,source_address):
        self.db = Database()
        self.connection = self.db.connect_db()

        self.actuator_led_resource = "air_conditioning/res_leds"
        self.actuator_ac_resource = "air_conditioning/res_air_conditioner"
        self.resource = "air_conditioning/res_temperature"

        self.address = source_address
        
        self.node_id = 0
        self.temperature = 0
        self.timestamp = 0
        
        self.start_observing()

    def execute_query(self):
        with self.connection.cursor() as cursor:
            # Create a new record
            sql = "INSERT INTO `coap` (`node_id`, `temperature`, `timestamp`) VALUES (%s, %s, %s)"
            cursor.execute(sql, (self.node_id, self.temperature, self.timestamp))
        
        self.connection.commit()
        self.show_log()

    def observer(self, response):
        print("callback called")
        if response.payload is None:
            print("response is none")
        if response.payload is not None:
            print("response:")
            print(response.payload)
            node_data = json.loads(response.payload)
            if not node_data["temperature"]:
                print("empty")
                return
            
            self.node_id = int(node_data["node"])
            self.temperature = int(node_data["temperature"])
            self.timestamp = int(node_data["timestamp"])

            ac_temperature = int(node_data["ac_temperature"])

            if self.temperature == ac_temperature:
                response = self.client.post(self.actuator_resource, "temp_status=equal_to_conditioner")
            elif self.temperature != ac_temperature:
                response = self.client.post(self.actuator_resource, "temp_status=not_equal_to_conditioner")
            else:
                print("Led API error")
            self.execute_query()

    def show_log(self):

        with self.connection.cursor() as cursor:
            sql = "SELECT * FROM `coap`"
            cursor.execute(sql)
            results = cursor.fetchall()
            header = results[0].keys()
            rows = [x.values() for x in results]
            print(tabulate.tabulate(rows, header, tablefmt='grid'))

    def start_observing(self):
        logging.getLogger("coapthon.server.coap").setLevel(logging.WARNING)
        logging.getLogger("coapthon.layers.messagelayer").setLevel(logging.WARNING)
        logging.getLogger("coapthon.client.coap").setLevel(logging.WARNING)
        self.client = HelperClient(self.address)
        self.client.observe(self.resource, self.observer)
