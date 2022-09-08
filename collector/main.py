from server import *
from coapthon.server.coap import CoAP
import threading
from mqtt_collector import MQTTClient
import logging
import time
import os
import keyboard

# Unspecified IPv6 address
ip = "::"
port = 5683


class CoAPServer(CoAP):
    def __init__(self, host, port):
        CoAP.__init__(self, (host, port), False)
        self.adv_resource = AdvancedResource()
        self.add_resource("registry", self.adv_resource)

    def get_env_temperature(self):
        mote_resource = self.adv_resource.get_mote_resource()
        if(mote_resource != None):
            return mote_resource.get_env_temperature()
        else:
            return 0

    def activate_ac(self):
        mote_resource = self.adv_resource.get_mote_resource()
        if(mote_resource != None):
            mote_resource.activate_ac()
            return "Operation done"
        else:
            return "Error in activate ac operation"

    def deactivate_ac(self):
        mote_resource = self.adv_resource.get_mote_resource()
        if(mote_resource != None):
            mote_resource.deactivate_ac()
            return "Operation done"
        else:
            return "Error in deactivate ac operation"

    def get_activated_led(self):
        mote_resource = self.adv_resource.get_mote_resource()
        if(mote_resource != None):
            return (mote_resource.get_activated_led())
        else:
            return "Error in retriving the current activated led "

    def change_ac_temperature(self, value):
        mote_resource = self.adv_resource.get_mote_resource()
        if(mote_resource != None):
            mote_resource.change_ac_temperature(value)
            return "Operation done"
        else:
            return "Error in temperature change operation"


def test():
    logging.getLogger("coapthon.server.coap").setLevel(logging.WARNING)
    logging.getLogger("coapthon.layers.messagelayer").setLevel(logging.WARNING)
    logging.getLogger("coapthon.client.coap").setLevel(logging.WARNING)

    mqtt_client = MQTTClient()
    mqtt_thread = threading.Thread(target=mqtt_client.mqtt_client, args=(), kwargs={})
    mqtt_thread.deamon = True
    mqtt_thread.start()

    coap_server = CoAPServer(ip, port)
    coap_thread = threading.Thread(target=coap_server.listen, args=([100]), kwargs={})
    coap_thread.deamon = True
    coap_thread.start()
    

    while(1):
        print("------------------ COMMANDS AVAILABLE ------------------\n")
        print("Energy Management:")
        print("1. Get the energy consumed right now")
        print("2. Get which led is activate right now and the reason why")
        print("")
        print("Air Conditioning Management")
        print("5. Get the enviroment temperature right now")
        print("6. Activate the air conditioner")
        print("7. Deactivate the air conditioner")
        print("8. Get which led is activate right now and the reason why")
        print("9. Change the air conditioner temperature")
        print("")
        print("0. EXIT")
        print("")
        print("--------------------------------------------------------\n")


        
        try:
            # CHECK THE OPTION 
            text = int(input("Insert your choice: "))
            print("")
            if(text == 1): 
                print("The energy_consumption in this moment is: " + str(mqtt_client.get_energy_consumption()) + " Watt\n")
            elif(text == 2):
                print(mqtt_client.get_activated_led())
            elif(text == 3):     
                print("")
            elif(text == 4):
                print("")
            elif(text == 5):
                print("The temperature in this moment is: " + str(coap_server.get_env_temperature()) + "°C\n")
            elif(text == 6):
                print(coap_server.activate_ac())
            elif(text == 7):
                print(coap_server.deactivate_ac())
            elif(text == 8):
                print(coap_server.get_activated_led())
            elif(text == 9):
                value = input("Insert ac value: ")
                print(coap_server.change_ac_temperature(value))
            elif(text == 0):
                break

            # ASK IF THE USER WANTS TO CONTINUE OR STOP 
            text_2 = ""
            
            while((text_2.lower() != "yes") and (text_2.lower() != "no")):
                text_2 = input("Do you want to continue? [YES/NO]: ")

            if (text_2.lower() == "yes"):
                continue
            elif (text_2.lower() == "no"):
                break
            
            print("")

        except KeyboardInterrupt:
            break

    print("\n")

    quit_text = ""

    while(quit_text.lower() != "quit"):
        try:
            print("The collector is still working in background, if you want to close it, write 'quit' and press enter")
            quit_text = input()
        except KeyboardInterrupt:
            break
        
    mqtt_client.stop_loop()
    coap_server.close()
    
    print("\n\nSHUTDOWN")

    os._exit(0)


if __name__ == '__main__':
    test()
