import socket
import json
import MetaTrader5 as mt5

# ----------------------------
# CONFIG
# ----------------------------
HOST = "127.0.0.1"
PORT = 9100

# ----------------------------
# MT5 INITIALIZATION
# ----------------------------
if not mt5.initialize():
    print("❌ MT5 initialize failed")
    mt5.shutdown()
    exit(1)

account = mt5.account_info()
if account is None:
    print("❌ No MT5 account info")
    mt5.shutdown()
    exit(1)

print(f"✅ Connected to MT5 account: {account.login}")

# ----------------------------
# SOCKET SERVER
# ----------------------------
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, PORT))
server.listen(1)

print(f"📡 Python MT5 bridge listening on port {PORT}")

conn, addr = server.accept()
print(f"🔗 Qt server connected from {addr}")

# ----------------------------
# TRADE FUNCTION
# ----------------------------
def execute_trade(symbol, order_type, volume):
    symbol_info = mt5.symbol_info(symbol)
    if symbol_info is None:
        return {"status": "error", "message": "Symbol not found"}

    if not symbol_info.visible:
        mt5.symbol_select(symbol, True)

    tick = mt5.symbol_info_tick(symbol)
    if tick is None:
        return {"status": "error", "message": "No tick data"}

    price = tick.ask if order_type == "BUY" else tick.bid

    request = {
        "action": mt5.TRADE_ACTION_DEAL,
        "symbol": symbol,
        "volume": volume,
        "type": mt5.ORDER_TYPE_BUY if order_type == "BUY" else mt5.ORDER_TYPE_SELL,
        "price": price,
        "deviation": 20,
        "magic": 202501,
        "comment": "Qt Econ Event Trade",
        "type_time": mt5.ORDER_TIME_GTC,
        "type_filling": mt5.ORDER_FILLING_IOC
    }

    result = mt5.order_send(request)

    if result.retcode != mt5.TRADE_RETCODE_DONE:
        return {
            "status": "error",
            "message": f"Trade failed: {result.retcode}"
        }

    return {
        "status": "ok",
        "order": result.order
    }

# ----------------------------
# MAIN LOOP
# ----------------------------
try:
    while True:
        data = conn.recv(1024)
        if not data:
            break

        message = data.decode()
        print("📨 Received:", message)

        try:
            command = json.loads(message)

            # Expected JSON format:
            # {
            #   "symbol": "EURUSD",
            #   "type": "BUY",
            #   "volume": 0.1
            # }

            response = execute_trade(
                command["symbol"],
                command["type"],
                command["volume"]
            )

        except Exception as e:
            response = {
                "status": "error",
                "message": str(e)
            }

        conn.sendall(json.dumps(response).encode())

except KeyboardInterrupt:
    print("⛔ Shutting down")

# ----------------------------
# CLEANUP
# ----------------------------
conn.close()
server.close()
mt5.shutdown()
print("✅ Bridge stopped cleanly")
