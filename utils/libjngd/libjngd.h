/*
 * libjngd.h
 * 
 * Copyright 2018 Fabio Meneghetti <fabiomene97@gmail.com>
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

#ifndef LIBJNGD_H
#define LIBJNGD_H 1

// Parametri di jngd
// Di default il socket è leggibile e scrivibile solo per root e il gruppo input
#define SOCKET_FILE  "test.sock" /*"/var/run/jngd.socket"*/
#define SOCKET_GROUP "input"

// Le funzioni ritornano le costanti E* da errno.h

// Le varie azioni e sotto-azioni
// Nella descrizione la struttura del pacchetto
// I campi sono nella forma "n_bytes nome_campo"
// I campi lunghezza (il cui nome inizia con len) di campi variabili (n_bytes viene sostituito con len)
// comprendono i terminatori delle stringhe dove non è specificato

// Il modello è client-server, dove il server risponde alle richieste del client
// Il pacchetto di richiesta ha struttura
//   1B   Azione
//   var  Payload dell'azione
// mentre quello di risposta
//   1B   Stato (jng_error_e). Se questo campo è diverso da JNGD_SUCCESS il payload può non essere valido
//   var  Payload della risposta
// Nelle descrizioni qui sotto il primo byte viene omesso
// Il pacchetto di richiesta è indicato con [req], quello di risposta con [resp],
// se una o entrambe vengono omesse significa che il payload è vuoto

// Le azioni (o sotto-azioni) che richiedono l'appartenza al gruppo SOCKET_GROUP sono indicate
// con [sg] dopo il nome della costante

typedef enum {
    // Avvia un driver, eseguendo il file specificato con 'exec=' nella definizione del driver
    // Al processo avviato viene aggiunta la variabile d'ambiente JNG_DRIVER contentente il nome del driver
    // [req]
    //     1    len driver
    //     1    len argomenti
    //     len  driver
    //     len  argomenti (stringa concatenata di tutti gli argomenti terminati da 0)
    JNGD_ACTION_DRV_LAUNCH = 0x00, // [sg]


    // Ottiene i driver installati
    // [resp]
    //     2    len lista (totale byte)
    //     len  lista dei driver, come stringhe terminate concatenate
    JNGD_ACTION_DRV_LIST,
    
    
    /// Le opzioni sono nella forma:
    ///     "opzione"             opzioni globali
    ///     "driver"."opzione"    opzioni di un driver. Se l'opzione è globale sovrascrive
    ///                           quell'opzione per quello specifico driver.
    ///                           Nota: non tutte le opzioni globali sono utili ai driver 
    
    // Aggiorna le definizioni di un driver (es. dopo l'installazione di un driver)
    JNGD_ACTION_DRVOPT_UPDATE, // [sg]


    // Ottieni la lista di impostazioni globali o di un driver
    // [req]
    //     1    len driver. Se il driver è nullo (len 0) ritorna il numero di opzioni globali
    //     len  driver
    // [resp]
    //     2    len elementi
    //     len  elementi
    //              1    len nome
    //              1    tipo (JNGD_DRVOPT_TYPE_*)
    //              1    len valore predefinito
    //              2    len descrizione
    //              len  nome
    //              len  valore predefinito
    //              len  descrizione
    JNGD_ACTION_DRVOPT_LIST,


    // Ottieni il valore effettivo di un'opzione.
    // L'ordine di risoluzione è
    //     driver.opzione > driver.globale > driver.predefinita > globale > predefinita globale
    // Le opzioni predefinite sono considerate solo durante la risoluzione di un opzione
    // [req]
    //     1    len opzione
    //     len  opzione
    // [resp]
    //     1    len valore
    //     len  valore
    JNGD_ACTION_DRVOPT_GET,
    
    
    // Imposta il valore di un'opzione
    // [req]
    //     1    len opzione
    //     1    len valore
    //     len  opzione
    //     len  valore (come stringa terminata)
    JNGD_ACTION_DRVOPT_SET // [sg]
} jngd_action_e;


//
// Per JNGD_ACTION_DRVOPT_LIST

// Tipi opzione
typedef enum {
    JNGD_DRVOPT_TYPE_INT = 0
    JNGD_DRVOPT_TYPE_DOUBLE
    JNGD_DRVOPT_TYPE_STRING
    JNGD_DRVOPT_TYPE_EXEC
} jngd_option_type_e;

// Opzioni
typedef struct {
    char*              name; // Nome opzione
    jngd_option_type_e type; // JNGD_DRVOPT_TYPE_*
    char*              def;  // Valore di default
    char*              description;
} jngd_option_t;

//


#ifdef JNGD_SERVER
    // Libreria server (statica)
    #define EXTERN_DECL 
#else
    // Libreria client (dinamica)
    
    // Avvia un driver. La lista di argomenti è terminata con NULL
    extern int jngd_driver_launch(const char* driver, const char* argv[]);
    
    // Ritorna una lista terminata con NULL nella variabile puntata da list
    // La lista viene allocata dinamicamente e deve essere rilasciata con free()
    extern int jngd_driver_list(char*** list);
    
    extern int jngd_drvoption_update();
    
    
#endif

// LIBJNGD_H
#endif

