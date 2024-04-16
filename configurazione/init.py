percorso_file_configurazione = '/home/episciotta/Documenti/SVILUPPO/repo_sviluppo_ctz/dati_configurazione/configurazione.ini'

import configparser

config = configparser.ConfigParser()
config.read(percorso_file_configurazione)

db_host = config['sql_database_meteo_mqtt']['host']
db_user = config['sql_database_meteo_mqtt']['user']
db_password = config['sql_database_meteo_mqtt']['password']
db_database = config['sql_database_meteo_mqtt']['database']
db_table = config['sql_database_meteo_mqtt']['db_table']

mqtt_user=config['mqtt_meteo_local']['mqtt_user']
mqtt_password=config['mqtt_meteo_local']['mqtt_password']
mqtt_host=config['mqtt_meteo_local']['mqtt_host']
mqtt_port=config['mqtt_meteo_local']['mqtt_port']