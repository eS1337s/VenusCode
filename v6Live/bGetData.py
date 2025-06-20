import paho.mqtt.client as paho
import time
import json

class Broker:
    def __init__(self, host, password, client_id, topicSubList, messageCallback, userdata=None):
        self.client = paho.Client(client_id=client_id, userdata=userdata)
        self.client.username_pw_set(client_id, password)
        self.client.on_message = messageCallback

        if self.client.connect(host=host) != 0:
            print("Error connecting to broker")
            raise RuntimeError("Error connecting to broker")

        for topic in topicSubList:
            if self.client.subscribe(topic)[0] != paho.MQTT_ERR_SUCCESS:
                print(f"Error subscribing to topic {topic}")
        print("Connected to broker.")
        self.client.loop_forever()

# This is the callback function for messages
def message_callback(client, userdata, message):
    keys = ['x', 'y', 'rock', 'colour', 'size', 'cliff', 'mountain']
    queue = userdata

    try:
        payload = message.payload.decode('utf-8').strip()

        # Debug print to inspect raw payload
        print(f"Raw payload received: {repr(payload)}")

        # Attempt to parse JSON
        values = json.loads(payload)

        # Ensure the payload structure is compatible
        if not isinstance(values, list) or len(values) != len(keys):
            print(f"Warning: Unexpected payload format: {values}")
            return

        dataToSend = dict(zip(keys, values))

        print(f"B callback: Received payload: {dataToSend}")
        if not queue.full():
            queue.put(("bData", dataToSend))
        else:
            print("Queue is full in B")

    except UnicodeDecodeError as e:
        print(f"Unicode decode error: {e}")
        print(f"Raw payload bytes: {message.payload}")

    except json.JSONDecodeError as e:
        print(f"JSON decode error: {e}")
        print(f"Payload that caused error: {repr(payload)}")

    except Exception as e:
        print(f"Unexpected error in message_callback: {e}")

def getData(queue):
    broker = Broker(
        host="mqtt.ics.ele.tue.nl",
        client_id="robot_2_1",
        password="PhrouDrij",
        topicSubList=["/pynqbridge/2"],
        messageCallback=message_callback,
        userdata=(queue)  
    )

