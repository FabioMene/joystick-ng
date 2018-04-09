/*
 * queue.h
 * 
 * Copyright 2016-2017 Fabio Meneghetti <fabiomene97@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

// Gestione di code dinamiche, versione kernel

// Tutte le funzioni che ritornano un int ritornano
//   0 se è andata a buon fine
//  -1 in caso di problemi di memoria o di posizione (es pos < 0) e coda piena/vuota

typedef struct {
    unsigned char* buffer;
    unsigned int   buflen;
    
    unsigned int   element_len;
    unsigned int   element_limit;
} jng_queue_t;

// Inizializza una coda, con 0 elementi
// q:  coda da inizializzare
// es: dimensione di un singolo elemento
// ql: limite numero elementi (0 = senza limiti)
void jng_queue_init(jng_queue_t* q, int es, int ql);


// Aggiunge un elemento in fondo alla coda (l'elemento viene copiato)
// el: elemento da aggiungere
int jng_queue_add(jng_queue_t* q, void* el);


// Ottiene la lunghezza della lista
unsigned int jng_queue_len(jng_queue_t* q);


// Inserisce un elemento in posizione pos
// int jng_queue_insert(jng_queue_t* q, void* el, int pos);


// Cancella e ritorna il primo elemento
int jng_queue_pop(jng_queue_t* q, void* dest);


// Ottiene l'elemento pos-esimo
int jng_queue_get(jng_queue_t* q, void* dest, int pos);


// Cancella l'elemento pos-esimo
int jng_queue_del(jng_queue_t* q, int pos);


// Cancella con callback. Il callback ha prototipo
//    int (*cb)(void* el, void* arg)
// dove el è l'elemento da analizzare e arg viene passato dalla chiamata a jng_queue_delcb
// se il callback ritorna 1 l'elemento viene cancellato
// la funzione ritorna il numero di elementi cancellati
int jng_queue_delcb(jng_queue_t* q, int(*cb)(void*, void*), void* arg);


// Cancella tutti gli elementi della coda
void jng_queue_delall(jng_queue_t* q);

