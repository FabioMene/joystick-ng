/*
 * libjngdsett.h
 * 
 * Copyright 2017 Fabio Meneghetti <fabiomene97@gmail.com>
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

// libjngdsett
// Libreria per la gestione delle impostazioni dei driver di joystick-ng
// Questa libreria è pensata per essere usata solo da superuser (tramite jngdctl e i driver)
// La cartella /etc/jngd contiene due sottocartelle
//   defs      contiene le definizioni dei driver (nome, percorso, tipo e descrizioni impostazioni)
//   settings  contiene le effettive impostazioni
// Questi file hanno i newline unix (\n)
// Scheletro per creare i file in defs (nome file = nome driver)
//   # Le righe vuote o che iniziano con # sono ignorate. Se appare # in una
//   # riga, ma non all'inizio, viene considerato letterale
//   # Le definizioni duplicate sovrascrivono quelle vecchie
//   # Il campo exec è obbligatorio, indica la posizione del file da eseguire (max 512 bytes)
//   exec=/percorso/eseguibile
//   # Tutte le impostazioni sono strutturate come
//   # nome_opzione=tipo|valore_predefinito|descrizione
//   # nome_opzione non può essere exec
//   # I tipi supportati sono:
//   #     int     intero. Viene indicato in decimale, esadecimale (con 0x anteposto)
//   #             e ottale (con 0 anteposto)
//   #     double  valore in virgola mobile (punto come separatore decimale)
//   #     string  stringa di caratteri
//   # Il valore predefinito indica il valore da usare in mancanza di settings/nome_opzione
//   # Nota: i valori stringa non possono essere circondati da whitespace, ne contenere '|'
//   #       gli altri campi possono avere spazi intorno, ma non il carattere |
//   # I campi nome, tipo e valore predefinito hanno un limite di 256 caratteri, la descrizione di 512
//   # La lunghezza massima della riga è dunque 1284 caratteri (compreso newline)
//   esempio  = int    | 123 | Un intero
//   esempio2 = string |valore stringa|    Descrizione con whitespace
//   esempio3 = double | 1.23456789 | # # cancelletti letterali # #
// File in settings
//   I file in settings contengono le opzioni modificate con jngdctl
//   Il formato è
//     nome_opzione=valore
//   Il tipo è ricavato del file in defs
// Il driver è responsabile di ricaricare autonomamente le opzioni se vengono modificate
// Generalmente i driver daemon (per esempio server bluetooth in background, non lanciato
// per un dispositivo in particolare ma per attenderne la connessione) dovrebbero ricaricare
// periodicamente la configurazione, mentre driver "one-shot" (per esempio avviati tramite udev
// per gestire un dispositivo appena collegato) possono ignorare le modifiche e applicarle dal
// collegamento successivo


#ifndef LIBJNGDSETT
#define LIBJNGDSETT 1


// Carica la configurazione
// La variabile name contiene il nome del driver. Se è NULL
// viene dedotta dalla variabile d'ambiente JNG_DRIVER
// In caso di errore ritorna -1
// Se fallisce l'analisi delle definizioni e/o delle impostazioni ritorna 1
// altrimenti ritorna 0
int jngdsett_load(char* name);

// Questa struttura viene usata internamente dalla libreria e da jngdctl
typedef struct {
    char name[256];
    
    #define JNGDSETT_TYPE_INT    0
    #define JNGDSETT_TYPE_DOUBLE 1
    #define JNGDSETT_TYPE_STRING 2
    #define JNGDSETT_TYPE_EXEC   3
    int  type;
    
    char value[256];
    
    char description[256];
} jngdsett_opt_t;

// Tutte le funzioni seguenti ritonano -1 in caso di errore, altrimenti 0

// Legge un'opzione
// Il tipo di valore è determinato dalla definizione
int jngdsett_read(char* opt, void* dest);

// Scrive un'opzione
// Il valore viene ricavato dalla stringa passata
int jngdsett_write(char* opt, const char* val);

// Ottiene il puntatore ai dati delle opzioni. Usato solo per ottenere informazioni
jngdsett_opt_t* jngdsett_optdata(int* num);

#endif

