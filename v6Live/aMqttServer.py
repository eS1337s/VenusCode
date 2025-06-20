import paho.mqtt.client as mqtt
import time, json, random

broker = "broker.hivemq.com"
port = 1883
topic = "pynqbridge/1"

def main():
    client = mqtt.Client()

    client.connect(broker, port, 50)
    client.loop_start()

    try:
        while True:
            x = random.randint(-2,2)
            y = random.randint(-2,2)
            data = [x,y, bool(random.getrandbits(1)), False, random.randint(3,9), False, False ]

            payload = json.dumps(data)
            client.publish(topic, payload)
            print(f"Payload sent:{payload}")
            time.sleep(2)
    except KeyboardInterrupt:
        print(f"Stopped sending...")
    finally:
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()
