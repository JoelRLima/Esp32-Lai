import dash
from dash import dcc, html
from dash.dependencies import Input, Output
import plotly.graph_objs as go
import paho.mqtt.client as mqtt
import json
import datetime
from collections import deque

# CONFIGURAÇÕES
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "iniciacao/ricardo/temp"

# ARMAZENAMENTO (Últimos 20 pontos)
X_tempo = deque(maxlen=20)
Y_temp = deque(maxlen=20)
ultimo_valor = 0.0

# LÓGICA MQTT
def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print(f"Conectado ao Broker! Escutando: {TOPIC}")
        client.subscribe(TOPIC)
    else:
        print(f"Erro na conexão: {reason_code}")

def on_message(client, userdata, msg):
    global ultimo_valor
    try:
        # Decodifica: {"temp": 27.12}
        payload = msg.payload.decode()
        dados = json.loads(payload)
        
        temp_recebida = float(dados['temp'])
        hora_atual = datetime.datetime.now().strftime("%H:%M:%S")
        
        # Salva na lista para o gráfico usar depois
        X_tempo.append(hora_atual)
        Y_temp.append(temp_recebida)
        ultimo_valor = temp_recebida
        
        print(f"Grafico recebeu: {temp_recebida}°C")
        
    except Exception as e:
        print("Erro ao processar:", e)

# Configura o cliente MQTT
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

print("Conectando ao Broker...")
client.connect(BROKER, PORT, 60)
client.loop_start() # Roda o MQTT em segundo plano (Thread separada)

# LÓGICA DO DASHBOARD (O Site)
app = dash.Dash(__name__)

app.layout = html.Div(style={'backgroundColor': '#111111', 'color': '#7FDBFF', 'height': '100vh', 'padding': '20px'}, children=[
    
    # (Linha Superior)
    html.Div(style={'display': 'flex', 'justifyContent': 'space-between', 'alignItems': 'center', 'marginBottom': '20px'}, children=[
        
        # Item da Esquerda: O Título
        html.H1("Monitoramento Remoto", style={'margin': '0', 'fontSize': '30px'}),
        
        # Item da Direita: O Valor da Temperatura
        html.Div(id='painel-valor', style={'fontSize': '50px', 'fontWeight': 'bold', 'color': '#00FF00'})
    ]),
    
    # Gráfico
    dcc.Graph(id='grafico-tempo-real'),
    
    dcc.Interval(
        id='intervalo-atualizacao',
        interval=2000, 
        n_intervals=0
    )
])

@app.callback(
    [Output('grafico-tempo-real', 'figure'),
     Output('painel-valor', 'children')],
    [Input('intervalo-atualizacao', 'n_intervals')]
)
def atualizar_tela(n):
    # Cria o desenho da linha
    trace = go.Scatter(
        x=list(X_tempo),
        y=list(Y_temp),
        mode='lines+markers',
        name='Temperatura',
        line=dict(color='#00D2FF', width=3),
        marker=dict(size=8, color='#ff0000')
    )

    layout = go.Layout(
        title='Histórico Recente',
        xaxis=dict(title='Horário', gridcolor='#333'),
        yaxis=dict(title='Temperatura (°C)', gridcolor='#333', range=[15, 40]),
        paper_bgcolor='#111111',
        plot_bgcolor='#111111',
        font=dict(color='#7FDBFF'),
        uirevision='true'
    )
    
    texto_display = f"{ultimo_valor:.2f} °C"
    
    return {'data': [trace], 'layout': layout}, texto_display

if __name__ == '__main__':
    # Roda o servidor do site
    app.run(debug=True, port=8050)