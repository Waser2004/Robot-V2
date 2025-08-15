import asyncio

from modules.mqtt_client import MQTTClient

async def main():
    mqtt = MQTTClient()
    await mqtt.connect()

    # subscribe to checkup
    await mqtt.sub("arduino/out/checkup", mqtt.pub("computer/out/checkup", "{}"))

    mqtt.loop()

    # close connection on exit
    try:
        await asyncio.Event().wait()
    except (KeyboardInterrupt, asyncio.CancelledError):
        pass
    finally:
        mqtt.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass