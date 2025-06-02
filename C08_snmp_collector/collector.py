import os
import socket
import psutil
from pymongo import MongoClient
import time

def collect_metrics():
    hostname = socket.gethostname()
    os_name = os.name
    cpu_usage = psutil.cpu_percent(interval=1)
    ram_usage = psutil.virtual_memory().percent

    return {
        "hostname": hostname,
        "os_name": os_name,
        "cpu_usage": cpu_usage,
        "ram_usage": ram_usage,
        "timestamp": time.time()
    }

if __name__ == "__main__":
    client = MongoClient("mongodb://c07_mongo:27017/")
    db = client["snmp_db"]
    collection = db["metrics"]

    while True:
        metrics = collect_metrics()
        collection.insert_one(metrics)
        print(f"Inserted: {metrics}")
        time.sleep(10)  # every 10 seconds
