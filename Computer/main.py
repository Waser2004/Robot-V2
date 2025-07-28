import asyncio

from modules.mqtt_client import MQTTClient

async def main():
    mqtt = MQTTClient()
    await mqtt.connect()

    await mqtt.sub('arduino/out', lambda topic, message: print(f"Received on {topic}: {message}"))
    await mqtt.pub('arduino/in', '{"type": "message", "message": "Hello, World!"}')

    mqtt.loop()

if __name__ == "__main__":
    asyncio.run(main())