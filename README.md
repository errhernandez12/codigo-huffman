# Código Huffman

Compresor y descompresor de archivos con el algoritmo de Huffman, escritos en C.
Son dos programas independientes: el descompresor solo necesita conocer el
formato del encabezado del archivo comprimido.

**Autor:** Ernesto Michel Rodríguez Hernández

## Archivos

| Archivo | Descripción |
|---|---|
| `comprimir_huffman.c` | Lee un archivo, construye el árbol de Huffman y escribe el archivo comprimido `.hfm` |
| `descomprimir_huffman.c` | Lee un `.hfm`, reconstruye el árbol y recupera el archivo original |
| `origen.txt` | Texto de prueba |

## Compilación

```bash
gcc -O2 -Wall comprimir_huffman.c -o comprimir
gcc -O2 -Wall descomprimir_huffman.c -o descomprimir
```

## Uso

```bash
./comprimir origen.txt origen.hfm
./descomprimir origen.hfm recuperado.txt

# verificar que el archivo recuperado es idéntico al original
cmp origen.txt recuperado.txt && echo "iguales"
```

Con `origen.txt` el archivo pasa de 2757 a 1695 bytes (61.5 % del original).

## Cómo funciona

1. **Frecuencias**: se cuenta cuántas veces aparece cada byte (0–255).
2. **Árbol**: con una cola de prioridad (montículo mínimo) se toman repetidamente los dos nodos de menor frecuencia y se unen en un nodo nuevo, hasta que queda uno solo, la raíz.
3. **Códigos**: bajar por la rama izquierda escribe `0` y por la derecha `1`. Como los símbolos están en las hojas, ningún código es prefijo de otro.
4. **Compresión**: cada byte del original se sustituye por su código y los bits se empaquetan en bytes.
5. **Descompresión**: se reconstruye el mismo árbol a partir de la tabla de frecuencias del encabezado y se recorre bit por bit hasta llegar a una hoja, se escribe el símbolo y se vuelve a la raíz.

Para que ambos programas obtengan el mismo árbol, el desempate es determinista:
se ordena por (frecuencia, índice), las hojas tienen índice igual al valor del
símbolo y los nodos internos usan 256, 257, … en orden de creación. El primer
nodo extraído es el hijo izquierdo.

## Formato del archivo comprimido (`HFM2`)

Los enteros se guardan en big-endian.

| Posición | Tamaño | Campo |
|---|---|---|
| 0 | 4 bytes | Marca `HFM2` |
| 4 | 8 bytes | Tamaño del archivo original (`uint64`) |
| 12 | 2 bytes | Número de símbolos distintos `N` (`uint16`, 0 a 256) |
| 14 | 5·N bytes | Tabla de frecuencias ordenada por símbolo: 1 byte de símbolo + 4 bytes de frecuencia (`uint32`) |
| 14 + 5·N | resto | Bits codificados, del bit menos significativo al más significativo de cada byte; los bits sobrantes del último byte son ceros. No hay datos si `N ≤ 1` |

## Casos especiales

- **Archivo vacío**: `N = 0`, solo se guarda el encabezado.
- **Un solo símbolo repetido**: `N = 1`, no hay bits; el descompresor repite el símbolo tantas veces como indica el tamaño original.
- El descompresor valida la marca, que la suma de frecuencias coincida con el tamaño original y que los datos no estén incompletos.
- Límite: cada frecuencia debe caber en 32 bits (archivos de menos de 4 GiB por símbolo).
