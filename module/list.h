/*
 * list.h
 * 
 * Copyright 2016-2019 Fabio Meneghetti <fabiomene97@gmail.com>
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

// Gestione di liste dinamiche, versione kernel

// Tutte le operazioni sono atomiche

// Tutte le funzioni che ritornano un int ritornano
//   0 se è andata a buon fine
//  -1 in caso di problemi di memoria o di posizione (es pos < 0) e lista piena/vuota

typedef struct {
    spinlock_t access_spinlock;
    
    //
    unsigned char* buffer;
    // Numero di elementi nel buffer (dimensione buffer = buflen * element_len)
    unsigned int buflen;
    
    unsigned int element_len;
    unsigned int element_limit;
} jng_list_t;

// Inizializza una lista, con 0 elementi
// q:  lista da inizializzare
// es: dimensione di un singolo elemento
// ql: limite numero elementi (0 = senza limiti)
void jng_list_init(jng_list_t* l, unsigned int es, unsigned int ql);


// Aggiunge un elemento in fondo alla lista (l'elemento viene copiato)
// el: elemento da aggiungere
int jng_list_append(jng_list_t* l, void* el);


// Ottiene la lunghezza della lista
unsigned int jng_list_len(jng_list_t* l);


// Inserisce un elemento in posizione pos
// int jng_list_insert(jng_list_t* l, void* el, int pos);


// Cancella e ritorna il primo elemento
int jng_list_pop(jng_list_t* l, void* dest);


// Ottiene l'elemento pos-esimo
int jng_list_get(jng_list_t* l, void* dest, unsigned int pos);


// Cancella l'elemento pos-esimo
int jng_list_del(jng_list_t* l, unsigned int pos);


// Itera tutti gli elementi della lista. Il callback ha prototipo
//    void (*cb)(void* el, void* arg)
// el è l'elemento della lista, arg viene passato dalla chiamata a jng_list_iter
void jng_list_iter(jng_list_t* l, void(*cb)(void*, void*), void* arg);


// Cancella con callback. Il callback ha prototipo
//    int (*cb)(void* el, void* arg)
// dove el è l'elemento da analizzare e arg viene passato dalla chiamata a jng_list_delcb
// se il callback ritorna 1 l'elemento viene cancellato
// la funzione ritorna il numero di elementi cancellati
int jng_list_delcb(jng_list_t* l, int(*cb)(void*, void*), void* arg);


// Cancella tutti gli elementi della lista
void jng_list_delall(jng_list_t* l);

