import random
import time
import sys

print("ELPMS Heart Rate Sensor Started", flush=True)

try:
    while True:
        bpm = random.randint(60, 100)

        print(bpm, flush=True)

        time.sleep(1)

except KeyboardInterrupt:
    print("\nSensor stopped.", flush=True)
