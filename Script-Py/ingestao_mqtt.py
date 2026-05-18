import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion
import sqlite3
import ssl
from datetime import datetime
from dotenv import load_dotenv
import os

load_dotenv()

# ==========================================
# 1. Configurações Locais (BeagleBone/Mosquitto)
# ==========================================
LOCAL_BROKER = "192.168.1.106"
LOCAL_PORT = 1883
LOCAL_TOPIC = "ifpb/projeto/led"
DB_NAME = "telemetria_projeto.db"

# ==========================================
# 2. Configurações da Nuvem (Adafruit IO)
# ==========================================
AIO_USERNAME = os.getenv("IO_USERNAME")
AIO_KEY      = os.getenv("AIO_KEY")
AIO_BROKER   = "io.adafruit.com"
AIO_PORT     = 443  # Usando porta 443 para passar pelo firewall via WebSockets

# ATENÇÃO: Verifique se a KEY do seu feed é realmente 'led'. Se não for, altere aqui!
FEED_KEY     = "led_status" 
AIO_FEED     = f"{AIO_USERNAME}/feeds/{FEED_KEY}"

# ==========================================
# Banco de Dados SQLite
# ==========================================
def setup_database():
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS eventos_led (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            estado TEXT NOT NULL,
            data_hora TEXT NOT NULL
        )
    ''')
    conn.commit()
    conn.close()

# ==========================================
# Callbacks MQTT
# ==========================================
def on_local_connect(client, userdata, flags, rc):
    print(f"[*] Conectado ao Broker Local (Mosquitto). Código: {rc}")
    client.subscribe(LOCAL_TOPIC)

def on_cloud_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[*] Conectado ao Adafruit IO via WebSockets/TLS com sucesso!")
    else:
        print(f"[!] Falha na conexão com Adafruit IO. Código de recusa: {rc}")

def on_local_message(client, userdata, msg):
    # O .strip() limpa espaços/quebras de linha vindos do sensor
    payload_str = msg.payload.decode('utf-8').strip()
    agora = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    print(f"\n[->] Novo evento local: '{payload_str}'")
    
    # AÇÃO 1: Salvar no SQLite Local (Persistência Offline)
    try:
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        cursor.execute('INSERT INTO eventos_led (estado, data_hora) VALUES (?, ?)', (payload_str, agora))
        conn.commit()
        conn.close()
        print("  [+] Salvo no SQLite local.")
    except sqlite3.Error as e:
        print(f"  [!] Erro no SQLite: {e}")

    # AÇÃO 2: Replicar para a Nuvem (Adafruit IO)
    estado_numerico = 1 if payload_str.upper() in ["ON", "LIGADO", "HIGH", "1"] else 0
    
    try:
        cloud_client.publish(AIO_FEED, estado_numerico)
        print(f"  [+] Replicado para a Nuvem (Adafruit IO): Valor {estado_numerico}")
    except Exception as e:
        print(f"  [!] Falha ao enviar para a nuvem: {e}")

# ==========================================
# Inicialização dos Clientes
# ==========================================
if __name__ == "__main__":
    setup_database()

    # --- Configura o Cliente da NUVEM (Publicador) ---
    # Adicionado API Version 1 e transport="websockets" para burlar o firewall
    cloud_client = mqtt.Client(CallbackAPIVersion.VERSION1, client_id="bbb_gateway_cloud", transport="websockets")
    cloud_client.username_pw_set(AIO_USERNAME, AIO_KEY)
    
    # Configuração TLS/SSL (Obrigatório para WebSockets na porta 443)
    context = ssl.create_default_context()
    cloud_client.tls_set_context(context)
    
    cloud_client.on_connect = on_cloud_connect
    
    print("[*] Conectando à Nuvem via WebSockets (Porta 443)...")
    try:
        cloud_client.connect(AIO_BROKER, AIO_PORT, 60)
        cloud_client.loop_start() 
    except Exception as e:
        print(f"[!] Erro crítico ao conectar na nuvem: {e}")

    # --- Configura o Cliente LOCAL (Subscritor) ---
    local_client = mqtt.Client(CallbackAPIVersion.VERSION1, client_id="bbb_gateway_local")
    local_client.on_connect = on_local_connect
    local_client.on_message = on_local_message

    print("[*] Conectando ao broker local...")
    try:
        local_client.connect(LOCAL_BROKER, LOCAL_PORT, 60)
        
        # O loop principal roda no cliente local, travando o script aqui para escutar eventos
        local_client.loop_forever()
    except KeyboardInterrupt:
        print("\n[*] Encerrando Gateway...")
        cloud_client.loop_stop()
        cloud_client.disconnect()
        local_client.disconnect()
    except Exception as e:
        print(f"[!] Erro no cliente local: {e}")
