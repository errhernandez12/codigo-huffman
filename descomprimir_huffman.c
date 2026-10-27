/*
 * Tarea - Descompresor de Huffman (programa 2 de 2)
 *
 * Compilar: gcc -O2 -Wall descomprimir_huffman.c -o descomprimir
 * Uso:      ./descomprimir origen.hfm recuperado.txt
 *
 * Solo necesita conocer el formato "HFM2" (ver comprimir_huffman.c):
 *   "HFM2" | tamano original (8 bytes BE) | N (2 bytes BE) |
 *   N x [simbolo (1 byte), frecuencia (4 bytes BE)] | bits (LSB primero)
 *
 * Con la tabla de frecuencias se reconstruye EXACTAMENTE el mismo arbol que
 * armo el compresor (monticulo minimo, llave = (frecuencia, indice), hojas
 * con indice = simbolo, internos desde 256, primer extraido = hijo izquierdo).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define NUM_SIMBOLOS 256

typedef struct {
    uint64_t peso;
    int izq, der;               /* -1 en las hojas */
} Nodo;

static Nodo arbol[2 * NUM_SIMBOLOS];
static int  monticulo[2 * NUM_SIMBOLOS];
static int  tam_monticulo = 0;

/* ------------- cola de prioridad: igual que en el compresor ------------- */
static int es_menor(int a, int b) {
    if (arbol[a].peso != arbol[b].peso) return arbol[a].peso < arbol[b].peso;
    return a < b;
}

static void intercambiar(int i, int j) {
    int t = monticulo[i]; monticulo[i] = monticulo[j]; monticulo[j] = t;
}

static void subir(int i) {
    while (i > 0) {
        int padre = (i - 1) / 2;
        if (!es_menor(monticulo[i], monticulo[padre])) break;
        intercambiar(i, padre);
        i = padre;
    }
}

static void bajar(int i) {
    for (;;) {
        int izq = 2 * i + 1, der = izq + 1, m = i;
        if (izq < tam_monticulo && es_menor(monticulo[izq], monticulo[m])) m = izq;
        if (der < tam_monticulo && es_menor(monticulo[der], monticulo[m])) m = der;
        if (m == i) break;
        intercambiar(i, m);
        i = m;
    }
}

static void encolar(int nodo) {
    monticulo[tam_monticulo] = nodo;
    subir(tam_monticulo++);
}

static int desencolar(void) {
    int menor = monticulo[0];
    monticulo[0] = monticulo[--tam_monticulo];
    bajar(0);
    return menor;
}

static int construir_arbol(const uint64_t frec[NUM_SIMBOLOS]) {
    int siguiente = NUM_SIMBOLOS;
    tam_monticulo = 0;
    for (int s = 0; s < NUM_SIMBOLOS; s++) {
        arbol[s].peso = frec[s];
        arbol[s].izq = arbol[s].der = -1;
        if (frec[s] > 0) encolar(s);
    }
    if (tam_monticulo == 0) return -1;
    while (tam_monticulo > 1) {
        int a = desencolar();
        int b = desencolar();
        arbol[siguiente].peso = arbol[a].peso + arbol[b].peso;
        arbol[siguiente].izq = a;
        arbol[siguiente].der = b;
        encolar(siguiente++);
    }
    return desencolar();
}

/* ------------------------------ utilidades ------------------------------ */
static void error(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

static uint64_t leer_be(const unsigned char *p, int bytes) {
    uint64_t v = 0;
    for (int i = 0; i < bytes; i++) v = (v << 8) | p[i];
    return v;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s origen.hfm recuperado\n", argv[0]);
        return 1;
    }

    /* cargar todo el archivo comprimido en memoria */
    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 1; }
    fseek(in, 0, SEEK_END);
    long largo = ftell(in);
    fseek(in, 0, SEEK_SET);
    unsigned char *buf = malloc(largo > 0 ? (size_t)largo : 1);
    if (!buf) error("sin memoria");
    if (largo > 0 && fread(buf, 1, (size_t)largo, in) != (size_t)largo)
        error("no se pudo leer el archivo");
    fclose(in);

    /* ---- cabecera ---- */
    if (largo < 14 || memcmp(buf, "HFM2", 4) != 0)
        error("el archivo no tiene formato HFM2");
    uint64_t tam      = leer_be(buf + 4, 8);
    int      distintos = (int)leer_be(buf + 12, 2);
    if (distintos > NUM_SIMBOLOS) error("cabecera danada");
    if (largo < 14 + 5L * distintos) error("cabecera incompleta");

    uint64_t frec[NUM_SIMBOLOS] = {0}, suma = 0;
    const unsigned char *p = buf + 14;
    for (int i = 0; i < distintos; i++, p += 5) {
        frec[p[0]] = leer_be(p + 1, 4);
        suma += frec[p[0]];
    }
    if (suma != tam) error("la tabla de frecuencias no coincide con el tamano");

    const unsigned char *bits = p;                       /* inicio de los datos */
    uint64_t bytes_bits = (uint64_t)(largo - (p - buf));

    /* ---- reconstruir y decodificar ---- */
    unsigned char *salida = malloc(tam > 0 ? (size_t)tam : 1);
    if (!salida) error("sin memoria");

    if (distintos == 1) {                                /* un solo simbolo */
        int simbolo = buf[14];
        memset(salida, simbolo, (size_t)tam);
    } else if (distintos > 1) {
        int raiz = construir_arbol(frec);
        int nodo = raiz;
        uint64_t pos = 0, hechos = 0;
        while (hechos < tam) {
            if ((pos >> 3) >= bytes_bits) error("datos comprimidos incompletos");
            int bit = (bits[pos >> 3] >> (pos & 7)) & 1;
            pos++;
            nodo = bit ? arbol[nodo].der : arbol[nodo].izq;
            if (arbol[nodo].izq < 0) {                   /* hoja: indice = simbolo */
                salida[hechos++] = (unsigned char)nodo;
                nodo = raiz;
            }
        }
    }

    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 1; }
    if (tam > 0) fwrite(salida, 1, (size_t)tam, out);
    fclose(out);

    printf("%s  ->  %s: %llu bytes recuperados\n", argv[1], argv[2],
           (unsigned long long)tam);
    free(buf);
    free(salida);
    return 0;
}
