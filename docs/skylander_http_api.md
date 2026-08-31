# Documentación de la API HTTP - Dolphin Skylanders Server

Esta API REST permite consultar el catálogo de Skylanders, ver el estado en tiempo real del Portal de Poder, y colocar o retirar figuras en **Dolphin** mediante peticiones HTTP en red local.

---

## 🌐 Configuración de Conexión

- **Dirección Base**: `http://<IP_DISPOSITIVO>:9090` (o `http://localhost:9090` en PC)
- **Puerto por defecto**: `9090` (configurable en Ajustes › General › Network)
- **Cabeceras CORS**: Habilitadas para todos los orígenes (`Access-Control-Allow-Origin: *`)

---

## 📋 Resumen de Endpoints

| Método | Endpoint | Descripción |
| :--- | :--- | :--- |
| `GET` | `/` | Panel Web visual interactivo del emulador |
| `GET` | `/api/status` | Estado de la emulación (juego activo, FPS, velocidad, etc.) |
| `GET` | `/api/skylanders` | Catálogo completo de Skylanders soportados con ID, variante, elemento y tipo |
| `GET` | `/api/skylanders/status` | Estado actual de las 16 ranuras del Portal de Poder y figuras colocadas |
| `POST` | `/api/skylanders/load` | Coloca una figura en una ranura dada (por nombre, ID o archivo local) |
| `POST` | `/api/skylanders/remove` | Retira la figura de una ranura dada |
| `POST` | `/api/skylanders/clear` | Retira todas las figuras de todas las ranuras simultáneamente |

---

## 🛠️ Detalle de Endpoints y Ejemplos

### 1. Obtener Catálogo de Skylanders
Devuelve la lista completa de figuras compatibles con sus IDs, variantes, juegos y elementos.

- **Método**: `GET`
- **Ruta**: `/api/skylanders`
- **Respuesta JSON**:
```json
[
  { "id": 1, "variant": 0, "name": "Spyro", "game": "Spyro's Adventure", "element": "Magic", "type": "Core" },
  { "id": 2, "variant": 0, "name": "Gill Grunt", "game": "Spyro's Adventure", "element": "Water", "type": "Core" },
  { "id": 3, "variant": 0, "name": "Trigger Happy", "game": "Spyro's Adventure", "element": "Tech", "type": "Core" }
]
```

#### Ejemplo en cURL:
```bash
curl -X GET http://localhost:9090/api/skylanders
```

---

### 2. Obtener Estado del Portal
Devuelve el estado de las 16 ranuras del portal y los datos de las figuras activas (ID, variante, experiencia, nivel calculado, dinero, etc.).

- **Método**: `GET`
- **Ruta**: `/api/skylanders/status`
- **Respuesta JSON**:
```json
{
  "success": true,
  "slots": [
    {
      "slot": 0,
      "occupied": true,
      "id": 1,
      "variant": 0,
      "name": "Spyro",
      "nickname": "Spyro",
      "experience": 33000,
      "level": 10,
      "money": 500
    },
    { "slot": 1, "occupied": false },
    { "slot": 2, "occupied": false }
  ]
}
```
*(Nota: El nivel `level` se calcula automáticamente en base a los puntos de experiencia `experience` de la figura, siguiendo la curva oficial de Skylanders del nivel 1 al 20).*

#### Ejemplo en cURL:
```bash
curl -X GET http://localhost:9090/api/skylanders/status
```

---

### 3. Colocar un Skylander en el Portal
Coloca un personaje en la ranura indicada (por defecto ranura `0`).  
*Si el archivo `.sky` de la figura ya existe en el dispositivo, reutilizará su experiencia y monedas guardadas; si no existe, lo creará automáticamente.*

- **Método**: `POST`
- **Ruta**: `/api/skylanders/load`
- **Cabecera**: `Content-Type: application/json`

#### Parámetros del Cuerpo (JSON):
| Campo | Tipo | Requerido | Descripción |
| :--- | :--- | :--- | :--- |
| `slot` | int | Opcional (def: 0) | Índice de ranura del portal (0 a 15) |
| `name` | string | Opcional | Nombre del Skylander (ej. `"Spyro"`, `"Gill Grunt"`) |
| `id` | int | Opcional | ID numérico del Skylander |
| `variant` | int | Opcional (def: 0) | Variante numérica del Skylander |
| `path` | string | Opcional | Ruta absoluta a un archivo `.sky` local en el dispositivo |

*(Nota: Debe proporcionarse `name`, o `id` [+ `variant`], o `path`).*

#### Ejemplos de Cuerpo JSON:
```json
// Opción A: Por Nombre
{
  "name": "Spyro",
  "slot": 0
}

// Opción B: Por ID y Variante
{
  "id": 1,
  "variant": 0,
  "slot": 0
}

// Opción C: Por Ruta de Archivo Local
{
  "path": "C:/Users/sergi/Documents/Dolphin Emulator/Skylanders/Spyro.sky",
  "slot": 0
}
```

- **Respuesta Exitosa JSON**:
```json
{
  "success": true,
  "slot": 0,
  "portalSlot": 0,
  "name": "Spyro",
  "path": "C:/Users/sergi/Documents/Dolphin Emulator/Skylanders/Spyro.sky"
}
```

#### Ejemplo en JavaScript (Fetch):
```javascript
const res = await fetch('http://localhost:9090/api/skylanders/load', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    name: 'Spyro',
    slot: 0
  })
});
const result = await res.json();
console.log(result);
```

---

### 4. Retirar una Figura de una Ranura
Quita el Skylander colocado en la ranura especificada.

- **Método**: `POST`
- **Ruta**: `/api/skylanders/remove`
- **Cabecera**: `Content-Type: application/json`
- **Cuerpo JSON**:
```json
{
  "slot": 0
}
```

- **Respuesta Exitosa JSON**:
```json
{
  "success": true,
  "slot": 0
}
```

#### Ejemplo en cURL:
```bash
curl -X POST http://localhost:9090/api/skylanders/remove \
  -H "Content-Type: application/json" \
  -d '{"slot": 0}'
```

---

### 5. Retirar Todas las Figuras (Limpiar Portal)
Quita todas las figuras de todas las ranuras simultáneamente.

- **Método**: `POST`
- **Ruta**: `/api/skylanders/clear`
- **Cabecera**: `Content-Type: application/json`
- **Cuerpo JSON**: `{}`

- **Respuesta Exitosa JSON**:
```json
{
  "success": true,
  "message": "All figures removed from portal"
}
```

#### Ejemplo en cURL:
```bash
curl -X POST http://localhost:9090/api/skylanders/clear \
  -H "Content-Type: application/json" \
  -d '{}'
```
