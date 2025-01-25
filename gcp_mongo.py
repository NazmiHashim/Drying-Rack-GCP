import json
import paho.mqtt.client as mqtt
from datetime import datetime, timezone
import signal
import sys
import pytz
from pymongo.mongo_client import MongoClient
from pymongo.server_api import ServerApi

# MongoDB Atlas configuration
uri = "mongodb+srv://nazmih79:z8S2buO3iVqSbRyg@dry-rack.oeewn.mongodb.net/?retryWrites=true&w=majority"


# Create a new client and connect to the server
mongo_client = MongoClient(uri, server_api=ServerApi('1'))

# Send a ping to confirm a successful connection
try:
    mongo_client.admin.command('ping')
    print("Pinged your deployment. You successfully connected to MongoDB!")
except Exception as e:
    print(e)

# MongoDB configuration
db = mongo_client["dry-rack"]
collection = db["sensors_data"]

# MQTT configuration
mqtt_broker_address = "34.134.71.68"  # Replace with your GCP broker IP
mqtt_topic = "dry-rack"

def data_preprocess(message):
  return json.loads(message)

def on_message(client, userdata, message):
  payload = message.payload.decode('utf-8')
  print(f'Received message: {payload}')
  # Convert MQTT timestamp to datetime
  timestamp = datetime.utcnow()
  # Convert to Malaysia time
  malaysia_tz = pytz.timezone('Asia/Kuala_Lumpur')
  timestamp_malaysia = timestamp.replace(tzinfo=pytz.UTC).astimezone(malaysia_tz)
  
  # Format the datetime object
  datetime_obj = timestamp_malaysia.strftime("%d-%m-%Y %H:%M:%S")
  # Process the payload and insert into MongoDB with proper timestamp
  document = {
  'timestamp': datetime_obj,
  'data': data_preprocess(payload)
  }
  collection.insert_one(document)
  print(f"Inserted document {document}")
  print('Data ingested into MongoDB')
  
  client = mqtt.Client()
  client.on_message = on_message
  
  # Connect to MQTT broker
  client.connect(mqtt_broker_address, 1883, 60)
  
  # Subscribe to MQTT topic
  client.subscribe(mqtt_topic)
  
  # Start the MQTT loop
  client.loop_forever()
