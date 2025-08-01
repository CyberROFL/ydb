
# Передача параметров и данных в формате JSON

YQL позволяет задавать параметры с помощью [DECLARE](../syntax/declare.md) выражений. Сами параметры передаются в restricted JSON формате.

Ниже дано описание формата передачи различных типов данных. Этот формат также используется для ответов из API.

## Bool {#bool}

Логическое значение.

* Тип в JSON — `bool`.
* Пример значения JSON — `true`.

## Числовые типы

### Int, Uint, Float, Double, Decimal, DyNumber {#numbers}

* Тип в JSON — `string`.
* Числа передаются в строковом представлении.
* Пример значения JSON — `"123456"`, `"-42"`, `"0.12345679"`, `"-320.789"`.

## Строковые типы

### String {#string}

Бинарные строки.

* при наличии невалидных utf-8 символов, например, `\xFF`, строка кодируется в base64 и оборачивается в массив с одним элементом
* если нет специальных символов, то передается как есть
* Тип в JSON — `string` или `array`
* Пример значения JSON — `"AB"`, `["qw6="]` (для строки `\xAB\xAC`)

### Utf8 {#utf}

Строковые типы в utf-8. Такие строки представляются в JSON строками с escaping'ом JSON-символов: `\\`, `\"`, `\n`, `\r`, `\t`, `\f`.

* Тип в JSON — `string`.
* Пример значения JSON — `"Escaped characters: \\ \" \f \b \t \r\nNon-escaped characters: / ' < > & []() "`.

### Uuid {#uuid}

Универсальный идентификатор UUID.

* бинарный формат UUID кодируется с помощью base64 и оборачивается в лист
* Тип в JSON — `array`
* Пример значения JSON — `["AIQOVZvi1EGnFkRmVUQAAA=="]` для `550e8400-e29b-41d4-a716-446655440000`

### Yson {#yson}

* Тип в JSON — `object`.
* YSON более богатый язык, чем JSON, поэтому есть необходимость предоставлять дополнительные атрибуты, поэтому задаются поля `$value`, `$type`, `$attributes`.
* Строки, числа и логические значения заменяются на объект с двумя ключами: `$value` и `$type`. Типы могут быть: `boolean`, `int64`, `uint64`, `double`, `string`.
* Каждый байт бинарной строки переводится в юникодный символ с соответствующим номером и кодируется в UTF-8 (такой же механизм использует YT при преобразовании Yson в Json).
* Yson атрибуты добавляются в виде отдельного объекта c ключом `$attributes` и значениями атрибутов.
* если какое имя начинается с `$`, то он экранируется с помощью удваивания `$$`.
* Пример YSON:

```yql
{ "$a" = 2;
  b = {
    c = <attr1=val1;attr2=5>12.5;
    d = [ "el"; # ]
  }
}
```

* Пример значения JSON:

```json
{
  "$$a" : { "$value": "2", "$type": "int64" },
  "b" : {
    "c": {
          "$value" : "12.5",
          "$type" : "double",
          "$attributes" :
            {
              "attr1": { "$value": "b", "$type": "string" },
              "attr2": { "$value": "5", "$type": "int64" }
            }
        },
    "d": [ { "$value": "el", "$type": "string" }, null ]
  }
}
```

### Json {#json}

* Тип в JSON — `object`.
* Пример значения JSON — `{ "a" : 12.5, "c" : 25 }`.

## Дата и время

### Date {#date}

Дата, внутреннее представление — Uint16, количество дней c unix epoch.

* Тип в JSON — `string`.
* Пример значения JSON — `"19509"` для даты `2023-06-01`.

### Datetime {#datetime}

Дата и время, внутреннее представление — Uint32, количество секунд c unix epoch.

* Тип в JSON — `string`.
* Пример значения JSON — `"1686966302"`для даты `"2023-06-17T01:45:02Z"`.

### Timestamp {#timestamp}

Дата и время, внутреннее представление — Uint64, количество микросекунд c unix epoch.

* Тип в JSON — `string`.
* Пример значения JSON — `"1685577600000000"` для `2023-06-01T00:00:00.000000Z`.

### Interval {#interval}

Временной интервал, внутреннее представление — Int64, точность до микросекунд.

* Тип в JSON — `string`.
* Пример значения JSON — `"12345678910"`.

### TzDate, TzDateTime, TzTimestamp {#tzdate}

Временные типы с меткой временной зоны.

* Тип в JSON — `string`.
* Значение представляется как строковое представление времени и временная зона через запятую.
* Пример значения JSON — `"2023-06-29,Europe/Moscow"`, `""2023-06-29T17:14:11,Europe/Moscow""`, `""2023-06-29T17:15:36.645735,Europe/Moscow""` для TzDate, TzDateTime и TzTimestamp соответственно.

### Date32 {#date32}

Дата, внутреннее представление — Int32, количество дней относительно unix epoch.

* Тип в JSON — `string`.
* Пример значения JSON — `"-8722"` для даты `1946-02-14`.

### Datetime64 {#datetime64}

Дата и время, внутреннее представление — Int64, количество секунд относительно unix epoch.

* Тип в JSON — `string`.
* Пример значения JSON — `"-753511371"`для даты `1946-02-14T19:17:09Z`.

### Timestamp64 {#timestamp64}

Дата и время, внутреннее представление — Int64, количество микросекунд c unix epoch.

* Тип в JSON — `string`.
* Пример значения JSON — `"-753511370765432"` для `1946-02-14T19:17:09.234568Z`.

### Interval64 {#interval64}

Временной интервал, внутреннее представление — Int64, точность до микросекунд.

* Тип в JSON — `string`.
* Пример значения JSON — `"9223339708799000000"`.

### TzDate32, TzDateTime64, TzTimestamp64 {#tzdate32}

Временные типы с меткой временной зоны.

* Тип в JSON — `string`.
* Значение представляется как строковое представление времени и временная зона через запятую.
* Пример значения JSON — `"1946-02-14,Europe/Moscow"`, `"1946-02-14T19:17:09,Europe/Moscow"`, `"1946-02-14T19:17:09.234568,Europe/Moscow"` для TzDate32, TzDateTime64 и TzTimestamp64 соответственно.

## PostgreSQL типы

Типы из PostgreSQL, например pgbytea, pgdate и прочие.

* если значение содержит `NULL`, то передается в JSON как `null`
* при наличии невалидных utf-8 символов, например, `\xFF`, строка кодируется в base64 и оборачивается в массив с одним элементом
* если нет специальных символов, то передается как есть
* Тип в JSON — `string` или `array` или `null`
* Пример значения JSON — `null`, `"AB"`, `["qw6="]` (для строки `\xAB\xAC`)

## Контейнеры

В контейнерах все элементы кодируются согласно их типу.

### List {#list}

Список. Упорядоченный набор значений заданного типа, значения передаются как строки.

* Тип в JSON — `array`.
* Пример значения JSON  для `List<Int32>` — `["1","10","100"]`.

### Struct {#struct}

Структура. Неупорядоченный набор значений с заданными именами и типом.

* Тип в JSON — `object` или `array`.
* Может передоваться как объект или как массив, где порядок элементов должен совпадать с соответствующими полями структуры.
* Пример значения JSON `{"a": "-100", "b": "foo"}`, `{"a": "-100", "b": "foo", "c": null}` или `["-100", "foo", null]` для `Struct<a:Int32, b:String, c:Optional<String>>`.

### Tuple {#tuple}

Кортеж. Упорядоченный набор значений заданных типов.

* Тип в JSON — `array`.
* Пример значения JSON для типа Tuple<Int32, String, Float?> — `[-1,"Some string",null]`.

### Dict {#dict}

Словарь. Неупорядоченный набор пар ключ-значение.

* Тип в JSON — `object` или `array`.
* для ключей типа `String` и `Utf8` можно передавать объект, для остальных типов надо передавать массив массивов, состоящих из двух элементов (ключа и значения).
* Пример значения JSON — `[["1","123"],["2","456"]]` для `Dict<Int32, Interval>`, `{ "foo": "123", "bar": "456" }` для `Dict<String, Int32>`.

### Enum {#enum}

Перечисление.

* Тип в JSON — `string`.
* Пример значения JSON — `"b"` для `Enum<a,b>`.

### Variant {#variant}

Кортеж или структура, про которые известно, что заполнен ровно один элемент.

* Тип в JSON — `array`.
* Первый способ передачи — массив из поля структуры, обернутый в массив, и значения. Способ подходит только для варианта над структурами.
* Второй способ передачи — массив из индекса поля структуры/кортежа и само значение.
* Пример значения JSON — `[["foo"], false]`, `[["bar"], "6"]` или `["0", false]`, `["1", "6"]` для `Variant<foo: Int32, bar: Bool>`.

### Optional {#optional}

Означает, что значение может быть `null`.

* Тип в JSON — `array` или `null`.
* Значения обертываются в массив, `null` значение представляется пустым массивом или `null`.
* Пример значения JSON — `[["1"], ["2"], ["3"], []]` или `[["1"], ["2"], ["3"], null]` для `List<Optional<Int32>>`.


## Специальные типы

### Void {#void}

Сингулярный тип данных с единственным возможным значением `null`.

* Тип в JSON — `string`.
* Значение JSON — `"Void"`.
