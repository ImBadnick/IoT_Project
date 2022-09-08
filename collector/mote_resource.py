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

    # When the registry has been done and the MoteResource is created 
    def __init__(self,source_address):
        #Connect to db 
        self.db = Database()
        self.connection = self.db.connect_db()

        # Name of the resources 
        self.actuator_led_resource = "res_leds"
        self.actuator_ac_resource = "res_air_conditioner"
        self.resource = "res_temperature_sensor"

        # Address of resource
        self.address = source_address
        
        # Values observed from resource 
        self.node_id = 0
        self.temperature = 0
        self.timestamp = 0
        self.ac_temperature = 0
        self.ac_status = 0

        # Start observing from the resource 
        self.start_observing()


    # Execute the query to save the data (got from observing the resource) on the db 
    def execute_query(self):
        # Create a new record
        with self.connection.cursor() as cursor:
            sql = "INSERT INTO `coap` (`node_id`, `temperature`, `timestamp`, `ac_temperature`, `ac_status`) VALUES (%s, %s, %s, %s, %s)"
            cursor.execute(sql, (self.node_id, self.temperature, self.timestamp, self.ac_temperature, self.ac_status))
        
        #Commit the query and show the results 
        self.connection.commit()

    # Function called when the resource notifies of a new value 
    def observer(self, response):
        if response.payload is not None:
            # Get the data from the payload in json format 
            node_data = json.loads(response.payload)
            if not node_data["temperature"]:
                print("empty")
                return
            
            # Extract the data from the json file 
            self.node_id = int(node_data["node"])
            self.temperature = int(node_data["temperature"])
            self.timestamp = int(node_data["timestamp"])
            self.ac_temperature = int(node_data["ac_temperature"])
            if int(node_data["ac_status"]): 
                self.ac_status = "ON"
            else: self.ac_status = "OFF"

            # Check if the temperature is equal to the air conditioner temperature to actuate on sensor's leds 
            if self.temperature == self.ac_temperature:
                response = self.client.put(self.actuator_led_resource, "temp_status=equal_to_conditioner")
            elif self.temperature != self.ac_temperature:
                response = self.client.put(self.actuator_led_resource, "temp_status=not_equal_to_conditioner")
            else:
                print("Led API error")

            # Execute the query to store the values in the DB
            self.execute_query()

    # Function used to start the observing procedure
    def start_observing(self):
        logging.getLogger("coapthon.server.coap").setLevel(logging.WARNING)
        logging.getLogger("coapthon.layers.messagelayer").setLevel(logging.WARNING)
        logging.getLogger("coapthon.client.coap").setLevel(logging.WARNING)

        # Instanciate a new coap client 
        self.client = HelperClient(self.address) 
        # Start observing 
        self.client.observe(self.resource, self.observer)

    def get_env_temperature(self):
        return self.temperature
    
    def activate_ac(self):
        response = self.client.put(self.actuator_ac_resource, "ac_status=ON")

    def deactivate_ac(self):
        response = self.client.put(self.actuator_ac_resource, "ac_status=OFF")

    def get_activated_led(self):
        if self.temperature == self.ac_temperature:
            return "The led activated is the GREEN one 'cause the temperature of the env is equal to the temperature of the air conditioner\n"
        elif self.temperature != self.ac_temperature:
            return "The led activated is the RED one 'cause the temperature of the env is not equal to the temperature of the air conditioner\n"

    def change_ac_temperature(self,value):
        response = self.client.put(self.actuator_ac_resource, "ac_temp=" + value)

    
