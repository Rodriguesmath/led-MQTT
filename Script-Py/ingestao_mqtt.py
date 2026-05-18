import paho.mqtt.client as mqtt
import sqlite3
import ssl
from datetime import datetime
from dotenv import load_dotenv
import os

load_dotenv()

# ==========================================
# 1. Configurações Locais (BeagleBone/Mosquitto)
# ==========================================
LOCAL_BROKER = "192.168.1.112"
LOCAL_PORT = 1883
LOCAL_TOPIC = "ifpb/projeto/led"
DB_NAME = "telemetria_projeto.db"

# ==========================================
# 2. Configurações da Nuvem (Adafruit IO)
# ==========================================
AIO_USERNAME = os.getenv("IO_USERNAME")
AIO_KEY      = os.getenv("AIO_KEY")
AIO_BROKER   = "io.adafruit.com"
AIO_PORT     = 8883  # Porta segura para TLS/SSL
AIO_FEED     = f"{AIO_USERNAME}/feeds/estado-led"

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
    print(f"[*] Conectado ao Adafruit IO via TLS/SSL. Código: {rc}")

def on_local_message(client, userdata, msg):
    payload_str = msg.payload.decode('utf-8')
    agora = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    print(f"\n[->] Novo evento local: {payload_str}")
    
    # AÇÃO 1: Salvar no SQLite Local (Persistência Offline)
    try:
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        cursor.execute('INSERT INTO eventos_led (estado, data_hora) VALUES (?, ?)', (payload_str, agora))
        conn.commit()
        conn.close()
        print(f"  [+] Salvo no SQLite local.")
    except sqlite3.Error as e:
        print(f"  [!] Erro no SQLite: {e}")

    # AÇÃO 2: Replicar para a Nuvem (Adafruit IO)
    # Adafruit IO gera gráficos melhores com números. Vamos padronizar:
    estado_numerico = 1 if payload_str.upper() in ["LIGADO", "HIGH", "1"] else 0
    
    try:
        cloud_client.publish(AIO_FEED, estado_numerico)
        print(f"  [+] Replicado para a Nuvem (Adafruit IO): Valor {estado_numerico}")
    except Exception as e:
        print(f"  [!] Falha ao enviar para a nuvem. (Internet caiu?): {e}")

# ==========================================
# Inicialização dos Clientes
# ==========================================
if __name__ == "__main__":
    setup_database()

    # --- Configura o Cliente da NUVEM (Publicador) ---
    cloud_client = mqtt.Client(client_id="bbb_gateway_cloud")
    cloud_client.username_pw_set(AIO_USERNAME, AIO_KEY)
    
    # Configuração TLS/SSL para segurança exigida pelo Adafruit IO na porta 8883
    context = ssl.create_default_context()
    cloud_client.tls_set_context(context)
    
    cloud_client.on_connect = on_cloud_connect
    
    print("[*] Conectando à Nuvem...")
    cloud_client.connect(AIO_BROKER, AIO_PORT, 60)
    
    # Inicia a thread de rede do cliente em nuvem (roda em background)
    cloud_client.loop_start() 

    # --- Configura o Cliente LOCAL (Subscritor) ---
    local_client = mqtt.Client(client_id="bbb_gateway_local")
    local_client.on_connect = on_local_connect
    local_client.on_message = on_local_message

    print("[*] Conectando ao broker local...")
    local_client.connect(LOCAL_BROKER, LOCAL_PORT, 60)
    
    # O loop principal roda no cliente local, travando o script aqui para ficar ouvindo
    try:
        local_client.loop_forever()
    except KeyboardInterrupt:
        print("\n[*] Encerrando Gateway...")
        cloud_client.loop_stop()
        cloud_client.disconnect()
        local_client.disconnect()