## CONNESSIONE MQTT A MOSQUITTO SERVER, SUBSCRIBING A TOPIC SPECIFICI E UPLOAD DEI DATI SU DATABASE MYSQL

import paho.mqtt.client as mqtt
import sqlalchemy
from sqlalchemy import insert
from sqlalchemy import table, column
from datetime import datetime
from configurazione.init import * 

def connect_to_mysql():
    global engine
    global connection
    try:
        url = 'mysql+pymysql://'+db_user+':'+db_password+'@'+db_host+'/'+db_database
        engine = sqlalchemy.create_engine(url)
        connection = engine.connect()
    except Exception as error:
        print("Errore di connessione al DB")
        # sys.exit(str(error))
    return

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    client.subscribe("Temperature")
    client.subscribe("Pressure")
    client.subscribe("Humidity")
    client.subscribe("Wind Direction")
    client.subscribe("Wind Speed")

def on_message(client, userdata, msg):
    global nome
    now = datetime.now()
    payload = (msg.payload).decode("utf-8")
    print("topic: ", msg.topic," ", payload," ", now)
    connect_to_mysql()
    
    db_table = table("mqtt_meteo",
                       column("id"),
                       column("value"),
                       column("topic"),
                       column("time"),
                       column("timestamp"),
                      )
    
    with engine.connect() as conn:
            stmt = insert(db_table).values({"value":str(payload),"topic":str(msg.topic), "time":str(now), "timestamp":str(now.timestamp()*1000)})
            result = conn.execute(stmt)
            

client = mqtt.Client()
client.username_pw_set(mqtt_user, mqtt_password)
client.on_connect = on_connect
client.on_message = on_message
client.connect(mqtt_host, int(mqtt_port), 60)
client.loop_forever()