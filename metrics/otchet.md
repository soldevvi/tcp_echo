# Обработка и статистика в одном потоке 

## Логирование в std::cout

--- [SERVER_OLD] REPORT ---
Clients: Active=0
Clients: Sent=10000, Recv=10000, Errors=0
Latency (Processing/RTT): Avg=99.25 us, Max=22908.28 us
Conn Duration: Avg=0.00 s, Max=0.00 s
----------------------------

--- [CLIENT_TEST] REPORT ---
Clients: Active=1000
Clients: Sent=10000, Recv=10000, Errors=0
Latency (Processing/RTT): Avg=31968.49 us, Max=886266.81 us
----------------------------

## Убрать логирование std::cout
* Прирост производительности примерно в 4 раза 
* Снижение средней зарежки на клиенте в 4 раза
* Снижение максмальной в  4 раза

--- [SERVER_OLD] REPORT ---
Clients: Active=0
Clients: Sent=10000, Recv=10000, Errors=0
Latency (Processing/RTT): Avg=108.52 us, Max=15664.40 us
Conn Duration: Avg=0.00 s, Max=0.00 s
----------------------------

--- [CLIENT_TEST] REPORT ---
Clients: Active=1000
Clients: Sent=10000, Recv=10000, Errors=0
Latency (Processing/RTT): Avg=8443.54 us, Max=183921.78 us
Conn Duration: Avg=0.00 s, Max=0.00 s
----------------------------


# Вывод - однопоточная обработка статистики и подключений сильно тормозит выполнение

# Обработка клиентов и статистика в разных потоках

## Перед добавлением логера
--- [SERVER] REPORT ---
Clients: Active=0
Clients: Sent=10000, Recv=10000, Errors=0
Latency (Processing/RTT): Avg=26.73 us, Max=13308.95 us
Conn Duration: Avg=0.07 s, Max=0.15 s
----------------------------

--- [CLIENT_TEST] REPORT ---
Clients: Active=1000
Clients: Sent=10000, Recv=10000, Errors=0
Latency (Processing/RTT): Avg=6761.85 us, Max=39952.05 us
Conn Duration: Avg=0.00 s, Max=0.00 s
----------------------------

## После добавления логера (logger.cpp) - видно подъем задержки в 2 раза
--- [SERVER] REPORT ---
Clients: Active=0
Clients: Sent=20000, Recv=20000, Errors=0
Latency (Processing/RTT): Avg=47.15 us, Max=21840.49 us
Conn Duration: Avg=0.19 s, Max=0.33 s
----------------------------

--- [CLIENT_TEST] REPORT ---
Clients: Active=1000
Clients: Sent=10000, Recv=10000, Errors=0
Latency (Processing/RTT): Avg=17712.28 us, Max=74975.63 us
----------------------------