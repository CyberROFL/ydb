# Рецепты кода на {{ ydb-short-name }} SDK

В данном разделе содержатся рецепты кода на разных языках программирования для решения различных задач, часто встречающихся на практике, с использованием {{ ydb-short-name }} SDK.

Содержание:

- [Инициализация драйвера](init.md)
- [Аутентификация](auth.md)

  - [С помощью токена](auth-access-token.md)
  - [Анонимная](auth-anonymous.md)
  - [Файл сервисного аккаунта](auth-service-account.md)
  - [Сервис метаданных](auth-metadata.md)
  - [С помощью переменных окружения](auth-env.md)
  - [С помощью логина и пароля](auth-static.md)

- [Балансировка](balancing.md)
  - [Равномерный случайный выбор](balancing-random-choice.md)
  - [Предпочитать ближайший дата-центр](balancing-prefer-local.md)
  - [Предпочитать зону доступности](balancing-prefer-location.md)

- [Выполнение повторных запросов](retry.md)
- [Установить размер пула сессий](session-pool-limit.md)
- [Вставка данных](upsert.md)
- [Пакетная вставка данных](bulk-upsert.md)
- [Установка режима выполнения транзакции](tx-control.md)
- [Настройка времени жизни строк (TTL) таблиц](ttl.md)
- [Векторный поиск](vector-search.md)
- Координация

  - [Распределённая блокировка](distributed-lock.md)
  - [Обнаружение сервисов](service-discovery.md)
  - [Публикация конфигурации](config-publication.md)
  - [Выбор лидера](leader-election.md)

- [Диагностика проблем](debug.md)

  - [Включить логирование](debug-logs.md)
  - [Подключить метрики в Prometheus](debug-prometheus.md)
  - [Подключить трассировку в Jaeger](debug-jaeger.md)

Смотрите также:

- [{#T}](../../dev/index.md)
- [{#T}](../../dev/example-app/index.md)
- [{#T}](../../reference/ydb-sdk/index.md)
