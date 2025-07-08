# psis-db-cli

Cliente C++ para conectarse a un servidor de base de datos personalizado (PlusDB) mediante sockets TCP. Usa un conector llamado `DBConnector`, basado en `AsyncClient`, para enviar comandos y recibir respuestas en formato JSON.

---

## 📁 Estructura del Proyecto

```

psis-db-cli/
│
├── include/               # Headers públicos
│   ├── connector.h        # Interfaz del conector DBConnector
│   └── client.hpp         # Cliente TCP asíncrono
│
└── README.md              # Este archivo

````

---

## 🧱 Requisitos

- C++17
- `g++` o `clang++`
- Windows o Linux
- Servidor PlusDB corriendo en segundo plano (por defecto en `127.0.0.1:65535`)

---

## 🔨 Compilación

### 1. Compilar el cliente CLI

```bash
g++ client.cpp -std=c++17 -lws2_32 -o client
```

---

## ▶️ Uso

Ejecuta el cliente:

```bash
./client.exe
```

Comandos válidos (en el modo interactivo):

* `CREATE table 0 col1:INT col2:TEXT`
* `INSERT table col1:10:INT col2:"hello world":TEXT`
* `GET table 10`
* `UPDATE table 10 col2:"updated text":TEXT`
* `DELETE table 10`


## 📚 Documentación del Conector (`DBConnector`)

### Estructuras:

```cpp
using Record = std::unordered_map<std::string, std::string>;

struct DBResponse {
  bool success;                 // Indica si la operación fue exitosa
  std::string detail;           // Mensaje del servidor
  std::vector<Record> data;     // Lista de registros (clave-valor)
};
```

### Métodos disponibles:

```cpp
DBConnector(std::string host, std::string port);
~DBConnector();
void connect();                // Establece conexión TCP con el servidor
void disconnect();             // Cierra conexión

DBResponse query(std::string qry);    // Enviar comando de solo lectura
DBResponse cmd(std::string cmd);      // Enviar comando de modificación
DBResponse get(std::string table, int64_t key);
DBResponse create(std::string table, int keyCol,
                  std::vector<std::tuple<std::string, std::string>> values);
DBResponse insert(std::string table,
                  std::vector<std::tuple<std::string, std::string, std::string>> values);
DBResponse update(std::string table, int64_t key,
                  std::vector<std::tuple<std::string, std::string, std::string>> values);
DBResponse remove(std::string table, int64_t key);
```

### Tipos soportados en columnas

* `"INT"`
* `"TEXT"`
* `"DOUBLE"`
* `"DATE"`

> ⚠️ El tipo debe especificarse correctamente en `insert` y `update`, y también en `create`.

---

### Ejemplo de uso:

```cpp
DBConnector db;
db.connect();

auto r1 = db.create("usuarios", 0, {{"id", "INT"}, {"nombre", "TEXT"}});
auto r2 = db.insert("usuarios", {{"id", "1", "INT"}, {"nombre", "Alice", "TEXT"}});
auto r3 = db.get("usuarios", 1);
db.disconnect();
```

---

## 📌 Notas

* El conector usa parseo JSON manual (sin dependencia externa).
* Si usas Windows, asegúrate de tener `ws2_32.lib` disponible.
* El protocolo espera respuestas tipo:

```json
{
  "success": true,
  "detail": "OK",
  "data": [ { "col1": "10", "col2": "hello" } ]
}
```

---

## 🛠 TODO

* Mejorar parser JSON (¿usar una lib como nlohmann/json?).
* Validar respuesta incompleta o malformada.
* Agregar pruebas automatizadas.

---