import datetime
from flask import Flask, request, jsonify
import dash
from dash import dcc, html
from dash.dependencies import Input, Output
import plotly.graph_objs as go
import pandas as pd

# 1. Configuração do Servidor
# Cria-se o Flask (Backend)
server = Flask(__name__)

# Cria o Dash (Frontend) e conecta ele ao Flask
app = dash.Dash(__name__, server=server)

# Variável global para armazenar o histórico (RAM)
data_storage = {
    'time': [],
    'temp': []
}

# 2. Rota para o ESP32
@server.route("/telemetry", methods=["POST"])
def telemetry():
    req_data = request.json
    
    if req_data and 'temp' in req_data:
        temperatura = req_data['temp']
        agora = datetime.datetime.now().strftime("%H:%M:%S")
        
        # Salva na memória
        data_storage['time'].append(agora)
        data_storage['temp'].append(temperatura)
        
        # Mantém apenas os últimos 20 pontos
        if len(data_storage['time']) > 20:
            data_storage['time'].pop(0)
            data_storage['temp'].pop(0)
            
        print(f"Recebido: {temperatura}°C às {agora}")
        return jsonify({"status": "ok"})
    else:
        return jsonify({"error": "Dados inválidos"}), 400

# 3. Layout do Dashboard
app.layout = html.Div(style={'backgroundColor': '#111111', 'color': '#7FDBFF', 'padding': '20px'}, children=[
    
    html.H1("Monitoramento em Tempo Real - DS18B20", style={'textAlign': 'center'}),
    
    html.Div(id='display-valor-atual', style={'textAlign': 'center', 'fontSize': '30px', 'marginTop': '20px'}),

    # O Gráfico
    dcc.Graph(id='live-graph'),

    # Componente invisível que dispara a atualização a cada 2 segundos (2000ms)
    dcc.Interval(
        id='interval-component',
        interval=2000, 
        n_intervals=0
    )
])

# 4. A Lógica de Atualização (Callback)
@app.callback(
    [Output('live-graph', 'figure'),
     Output('display-valor-atual', 'children')],
    [Input('interval-component', 'n_intervals')]
)
def update_graph_scatter(n):
    # Cria o gráfico com os dados atuais da memória
    trace = go.Scatter(
        x=data_storage['time'],
        y=data_storage['temp'],
        mode='lines+markers',
        name='Temperatura',
        line=dict(color='#00D2FF', width=3),
        marker=dict(size=8)
    )

    layout = go.Layout(
        title='Histórico de Temperatura',
        uirevision='true',
        xaxis=dict(title='Horário', gridcolor='#444'),
        yaxis=dict(title='Temperatura (°C)', range=[15, 40], gridcolor='#444'),
        paper_bgcolor='#111111',
        plot_bgcolor='#111111',
        font=dict(color='#7FDBFF')
    )
    
    # Pega o último valor para mostrar em texto grande
    ultimo_valor = "Aguardando dados..."
    if data_storage['temp']:
        ultimo_valor = f"Temperatura Atual: {data_storage['temp'][-1]}°C"

    return {'data': [trace], 'layout': layout}, ultimo_valor

# 5. Executa o servidor
if __name__ == '__main__':
    # host='0.0.0.0' permite que outros PCs na rede vejam o site
    app.run(host='0.0.0.0', port=5000, debug=False)