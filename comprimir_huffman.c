/*
 * Tarea - Compresor de Huffman (programa 1 de 2)
 *
 * Compilar: gcc -O2 -Wall comprimir_huffman.c -o comprimir -lm   (no usa -lm, es opcional)
 * Uso:      ./comprimir origen.txt origen.hfm
 *
 * FORMATO DEL ARCHIVO COMPRIMIDO  "HFM2"   (enteros en big-endian)
 *
 *   bytes 0-3     marca "HFM2"
 *   bytes 4-11    tamano del archivo original (uint64)
 *   bytes 12-13   cantidad de simbolos distintos, N (uint16, de 0 a 256)
 *   siguientes    N registros de 5 bytes, ordenados por simbolo:
 *                   1 byte  = simbolo
 *                   4 bytes = frecuencia (uint32)
 *   resto         bits codificados, EMPAQUETADOS DEL BIT MENOS SIGNIFICATIVO
 *                 AL MAS SIGNIFICATIVO de cada byte; los bits sobrantes del
 *                 ultimo byte quedan en cero. Si N <= 1 no hay bits.
 *
 * Construccion del arbol (la debe repetir igual el descompresor):
 *   - cola de prioridad = monticulo minimo con llave (frecuencia, indice)
 *   - hojas: indice = valor del simbolo (0..255)
 *   - nodos internos: indices 256, 257, ... en el orden en que se crean
 *   - se extraen los dos menores: el PRIMERO es el hijo izquierdo (bit 0)
 *     y el SEGUNDO el derecho (bit 1)
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

/* ------------------ cola de prioridad (monticulo minimo) ------------------ */
static int es_menor(int a, int b) {
    if (arbol[a].peso != arbol[b].peso) return arbol[a].peso < arbol[b].peso;
    return a < b;                               /* desempate por indice */
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

/* --------------------------- arbol de Huffman --------------------------- */
/* devuelve la raiz, o -1 si no hay ningun simbolo */
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

/* tabla[s] guarda el codigo de s como cadena de '0' y '1' */
static char tabla[NUM_SIMBOLOS][NUM_SIMBOLOS + 1];

static void recorrer(int nodo, char *camino, int prof) {
    if (arbol[nodo].izq < 0) {                  /* hoja */
        camino[prof] = '\0';
        strcpy(tabla[nodo], camino);
        return;
    }
    camino[prof] = '0';
    recorrer(arbol[nodo].izq, camino, prof + 1);
    camino[prof] = '1';
    recorrer(arbol[nodo].der, camino, prof + 1);
}

/* ---------------------------- entrada / salida --------------------------- */
static void escribir_be(FILE *f, uint64_t valor, int bytes) {
    for (int i = bytes - 1; i >= 0; i--)
        fputc((int)((valor >> (8 * i)) & 0xFF), f);
}

static unsigned char *leer_archivo(const char *ruta, uint64_t *tam) {
    FILE *f = fopen(ruta, "rb");
    if (!f) { perror(ruta); exit(1); }
    fseek(f, 0, SEEK_END);
    long largo = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(largo > 0 ? (size_t)largo : 1);
    if (!buf) { fprintf(stderr, "Sin memoria.\n"); exit(1); }
    if (largo > 0 && fread(buf, 1, (size_t)largo, f) != (size_t)largo) {
        fprintf(stderr, "Error al leer %s.\n", ruta);
        exit(1);
    }
    fclose(f);
    *tam = (uint64_t)largo;
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s origen destino.hfm\n", argv[0]);
        return 1;
    }

    uint64_t tam;
    unsigned char *datos = leer_archivo(argv[1], &tam);

    /* 1) frecuencias */
    uint64_t frec[NUM_SIMBOLOS] = {0};
    for (uint64_t i = 0; i < tam; i++) frec[datos[i]]++;

    int distintos = 0;
    for (int s = 0; s < NUM_SIMBOLOS; s++) {
        if (frec[s] > 0xFFFFFFFFULL) {
            fprintf(stderr, "Archivo demasiado grande para este formato.\n");
            return 1;
        }
        if (frec[s]) distintos++;
    }

    /* 2) cabecera */
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 1; }
    fwrite("HFM2", 1, 4, out);
    escribir_be(out, tam, 8);
    escribir_be(out, (uint64_t)distintos, 2);
    for (int s = 0; s < NUM_SIMBOLOS; s++) {
        if (frec[s]) {
            fputc(s, out);
            escribir_be(out, frec[s], 4);
        }
    }

    /* 3) arbol, codigos y bits (solo si hay al menos 2 simbolos distintos) */
    if (distintos > 1) {
        int raiz = construir_arbol(frec);
        char camino[NUM_SIMBOLOS + 1];
        recorrer(raiz, camino, 0);

        uint64_t total_bits = 0;
        for (int s = 0; s < NUM_SIMBOLOS; s++)
            total_bits += frec[s] * (uint64_t)strlen(tabla[s]);

        size_t nbytes = (size_t)((total_bits + 7) / 8);
        unsigned char *bits = calloc(nbytes ? nbytes : 1, 1);
        if (!bits) { fprintf(stderr, "Sin memoria.\n"); return 1; }

        uint64_t pos = 0;
        for (uint64_t i = 0; i < tam; i++) {
            for (const char *p = tabla[datos[i]]; *p; p++, pos++)
                if (*p == '1') bits[pos >> 3] |= (unsigned char)(1u << (pos & 7));
        }
        fwrite(bits, 1, nbytes, out);
        free(bits);
    }

    long salida = ftell(out);
    fclose(out);
    free(datos);

    printf("%s: %llu bytes  ->  %s: %ld bytes", argv[1],
           (unsigned long long)tam, argv[2], salida);
    if (tam > 0) printf("  (%.1f%% del original)", 100.0 * salida / (double)tam);
    printf("\n");
    return 0;
}
