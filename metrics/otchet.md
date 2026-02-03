# Тесты 1 тыс. клиентов по 10 пакетов случаных до 1 кб
## Обработка и статистика в одном потоке 

### Логирование в std::cout

[SERVER_OLD] 
Latency  Avg=99.25 us, Max=22908.28 us

[CLIENT_TEST]
Latency  Avg=31968.49 us, Max=886266.81 us


### Убрать логирование std::cout
* Прирост производительности примерно в 4 раза 
* Снижение средней зарежки на клиенте в 4 раза
* Снижение максмальной в  4 раза

[SERVER_OLD]
Latency  Avg=108.52 us, Max=15664.40 us

[CLIENT_TEST]
Latency  Avg=8443.54 us, Max=183921.78 us

## *Вывод - однопоточная обработка статистики и подключений сильно тормозит выполнение*

## Обработка клиентов и статистика в разных потоках

### Перед добавлением логера
[SERVER]
Latency  Avg=26.73 us, Max=13308.95 us

[CLIENT_TEST]
Latency  Avg=6761.85 us, Max=39952.05 us


### После добавления логера (logger.cpp) - видно подъем задержки в 2 раза
[SERVER]
Latency  Avg=47.15 us, Max=21840.49 us

[CLIENT_TEST]
Latency  Avg=17712.28 us, Max=74975.63 us


# Тесты 10 тыс. клиентов по 1 пакету до 1кб 

## Без логирования в стд single thread
[SERVER_OLD]
Latency  Avg=25.77 us, Max=6703.67 us

[CLIENT_TEST] 
Latency  Avg=4674.21 us, Max=24084.81 us

## Без логирования в файл multi-threaded

[SERVER]
Latency Avg=16.11 us, Max=3600.58 us

[CLIENT_TEST]
Latency Avg=4530.21 us, Max=25699.21 us

## Снижение обработки подключения сервером, засчет мульти-потока, уменьшение пика +- 2 раза на сервере

# multi threaded сервер

## Увеличение буфера события для обработки до 512 с 128
*до*

[SERVER]
Latency Avg=16.11 us, Max=3600.58 us

[CLIENT_TEST]
Latency Avg=4530.21 us, Max=25699.21 us

*после* 

[SERVER]
Latency (Processing/RTT): Avg=16.25 us, Max=6527.35 us

[CLIENT_TEST]
Latency (Processing/RTT): Avg=4859.51 us, Max=24147.03 us

## resize vector 10010

[SERVER]
Latency (Processing/RTT): Avg=20.26 us, Max=2938.42 us

[CLIENT_TEST]

Latency (Processing/RTT): Avg=2998.12 us, Max=29100.13 us

