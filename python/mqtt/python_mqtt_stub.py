import paho.mqtt.client as mqtt
import json 
import base64
import struct

setpoint = 25
magic_number = 0x96089608

def on_connect(client, userdata, flags, rc):  # The callback for when the client connects to the broker
    print("Connected with result code {0}".format(str(rc)))  # Print result of connection attempt
    client.subscribe("00124B000F198B03/upstream")  # Subscribe to the topic “labscim/example”, receive any messages published on it

def on_message(client, userdata, msg):  # The callback for when a PUBLISH message is received from the server.
    print("Message received-> " + msg.topic + " " + str(msg.payload))  # Print a received msg    
    rcvd_value = int(msg.payload.decode("utf-8"))
    print("received: ", rcvd_value)
    device_id = msg.topic.split('/')[0]
    if rcvd_value >= 70:
        tg = '{:s}/control/red'.format(device_id)
        client.publish(tg, "1")
        tg = '{:s}/control/green'.format(device_id)
        client.publish(tg, "0")
    elif rcvd_value > 30 or rcvd_value < 70:
        tg = '{:s}/control/red'.format(device_id)
        client.publish(tg, "0")
        tg = '{:s}/control/green'.format(device_id)
        client.publish(tg, "1")
    else:
        tg = '{:s}/control/red'.format(device_id)
        client.publish(tg, "0")
        tg = '{:s}/control/green'.format(device_id)
        client.publish(tg, "0")    

client = mqtt.Client("labscim_student_client")  # Create instance of client with client ID “labscim_student_client”
client.on_connect = on_connect  # Define callback function for successful connection
client.on_message = on_message  # Define callback function for receipt of a message
client.connect('labscpi.eletrica.eng.br', 1883, 60)
client.loop_forever()  # Start networking daemon