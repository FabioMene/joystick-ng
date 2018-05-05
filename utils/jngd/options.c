/*
 * options.c
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

// La cartella /etc/jngd contiene due sottocartelle
//   defs      contiene le definizioni dei driver (nome, percorso, tipo e descrizioni impostazioni)
//   settings  contiene le effettive impostazioni
// Questi file hanno i newline unix (\n)
// Scheletro per creare i file in defs (nome file senza estensione = nome driver)
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
//   # Nota: i valori stringa non possono contenere '|'. Gli spazi prima e dopo la stringa vengono rimossi
//   #       gli altri campi possono avere spazi intorno, ma non il carattere |
//   # I campi nome, tipo e valore predefinito hanno un limite di 256 caratteri, la descrizione di 512
//   # La lunghezza massima della riga è dunque 1284 caratteri (compreso newline)
//   esempio  = int    | 123 | Un intero
//   esempio2 = string |valore stringa|    Descrizione con whitespace
//   esempio3 = double | 1.23456789 | # # cancelletti letterali # #
// File in settings
//   I file in settings contengono le opzioni modificate con jngctl
//   Il formato è
//     nome_opzione=valore
//   Il tipo è ricavato dal file in defs
// Il driver è responsabile di ricaricare autonomamente le opzioni se vengono modificate
// Generalmente i driver daemon (per esempio server bluetooth in background, non lanciato
// per un dispositivo in particolare ma per attenderne la connessione) dovrebbero ricaricare
// periodicamente la configurazione, mentre driver "one-shot" (per esempio avviati tramite udev
// per gestire un dispositivo appena collegato) possono ignorare le modifiche e applicarle dal
// collegamento successivo

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>

#include "jngd.h"


// Mutex di accesso
static pthread_mutex_t options_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() pthread_mutex_lock(&options_mutex);
#define UNLOCK() pthread_mutex_unlock(&options_mutex);
#define UNLOCK_RET(n) do{pthread_mutex_unlock(&options_mutex); return (n);}while(0)


// L'array associativo di driver e opzioni
static internal_option_t* global_options = NULL;
static int                global_options_num = 0;

typedef struct {
    char               driver[256];
    internal_option_t* options; // Allocato con malloc
    int                options_num;
} internal_driver_options_t;

static internal_driver_options_t* driver_options = NULL;
static int                        driver_options_num = 0;


// Parser di opzioni
static int _parse_file(char*, int(*)(internal_option_t**, int*, unsigned char*, int), internal_option_t**, int*, int);
static int _parse_def_line(internal_option_t**, int*, unsigned char*, int);
static int _parse_setting_line(internal_option_t**, int*, unsigned char*, int);

// Salva le opzioni
static int _update_file(char*, internal_option_t*, int);

// Carica le definizioni e le impostazioni
int drvoption_load(){
    LOCK();
    int i;
    
    // Libera la memoria dei vecchi driver
    if(global_options){
        free(global_options);
        global_options = NULL;
        global_options_num = 0;
    }
    for(i = 0;i < driver_options_num;i++){
        free(driver_options[i].options);
    }
    if(driver_options){
        free(driver_options);
        driver_options = NULL;
        driver_options_num = 0;
    }
    
    // Carica le nuove definizioni/impostazioni
    
    // Globali
    if(_parse_file(JNGD_DATA_PATH "/" JNGD_GLOBAL_FILE, _parse_def_line, &global_options, &global_options_num, 1) < 0){
        printe("Impossibile caricare le definizioni globali");
        if(global_options) free(global_options);
        global_options_num = 0;
        UNLOCK_RET(-1);
    }
    
    _parse_file(JNGD_DATA_PATH "/" JNGD_GLOBAL_OPT, _parse_setting_line, &global_options, &global_options_num, 1);
    if(!global_options){
        printe("Impossibile caricare le impostazioni globali");
        global_options_num = 0;
        UNLOCK_RET(-1);
    }
    
    // Driver
    DIR* dir = opendir(JNGD_DATA_PATH "/defs/");
    if(!dir){
        printe("Impossibile aprire la directory " JNGD_DATA_PATH "/defs/\n");
        UNLOCK_RET(-1);
    }
    
    internal_option_t* opts;
    int                optn;
    
    struct dirent* de;
    while((de = readdir(dir)) != NULL){
        if(strncmp(de->d_name, ".", 1)){
            // Elimina l'estensione
            char* dot = strstr(de->d_name, ".");
            if(dot) *dot = 0;
            
            char path[1024];
            
            opts = NULL;
            optn = 0;
            
            // Carica le definizioni
            strcpy(path, JNGD_DATA_PATH);
            strcat(path, "/defs/");
            strcat(path, de->d_name);
            if(_parse_file(path, _parse_def_line, &opts, &optn, 0) < 0){
                printw("Impossibile caricare le definizioni del driver %s", de->d_name);
                if(opts) free(opts);
                continue;
            }
            
            // Carica le impostazioni
            strcpy(path, JNGD_DATA_PATH);
            strcat(path, "/settings/");
            strcat(path, de->d_name);
            _parse_file(path, _parse_setting_line, &opts, &optn, 0);
            if(!opts){
                printw("Impossibile caricare le impostazioni del driver %s", de->d_name);
                continue;
            }
            
            // Crea una spazio in driver_options
            driver_options = realloc(driver_options, sizeof(internal_driver_options_t) * (driver_options_num + 1));
            if(!driver_options){
                printe("Impossibile allocare memoria per le opzioni driver");
                free(opts);
                closedir(dir);
                UNLOCK_RET(-1);
            }
            
            strncpy(driver_options[driver_options_num].driver, de->d_name, 255);
            driver_options[driver_options_num].options     = opts;
            driver_options[driver_options_num].options_num = optn;
            
            driver_options_num++;
        }
    }
    
    closedir(dir);
    
    UNLOCK();
    return 0;
}


int drvoption_read_exec(char* driver, char* dst){
    LOCK();
    int i, j;
    
    for(i = 0;i < driver_options_num;i++){
        if(strcmp(driver_options[i].driver, driver) == 0) break;
    }
    if(i == driver_options_num) UNLOCK_RET(-1); // Driver non trovato
    
    for(j = 0;j < driver_options[i].options_num;j++){
        if(driver_options[i].options[j].type == JNGD_DRVOPT_TYPE_EXEC){
            strcpy(dst, driver_options[i].options[j].path);
            UNLOCK_RET(0);
        }
    }
    
    // Il driver non ha exec
    UNLOCK();
    return -1;
}


int drvoption_dump_list(char* driver, internal_option_t** dst, int* dstnum){
    LOCK();
    int i;
    
    internal_option_t* opts = NULL;
    int                optn = 0;
    
    if(driver == NULL){
        opts = global_options;
        optn = global_options_num;
    } else for(i = 0;i < driver_options_num;i++){
        if(strcmp(driver_options[i].driver, driver) == 0){
            opts = driver_options[i].options;
            optn = driver_options[i].options_num;
            break;
        }
    }
    if(opts == NULL) UNLOCK_RET(-1); // Driver non trovato
    
    internal_option_t* out = malloc(sizeof(internal_option_t) * optn);
    if(!out) UNLOCK_RET(-2);
    
    memcpy(out, opts, sizeof(internal_option_t) * optn);
    
    UNLOCK();
    
    *dst    = out;
    *dstnum = optn;
    
    return 0;
}


int drvoption_resolve_option(char* option, char* dst){
    // Cerca il . in option, per dividere tra driver e option
    char option_saved[1024];
    strcpy(option_saved, option);
    
    char* driver = "";
    char* oname  = option_saved;
    
    char* dot = strstr(option_saved, ".");
    if(dot){
        *dot = 0;
        driver = option_saved;
        oname  = dot + 1;
    }
    
    int i, j;
    
    LOCK();
    
    if(strcmp(driver, "")) for(i = 0;i < driver_options_num;i++){ // Cerca nei driver
        if(strcmp(driver_options[i].driver, driver) == 0){
            // Cerca nelle opzioni del driver
            for(j = 0;j < driver_options[i].options_num;j++){
                // Solo se l'opzione non è EXEC
                if(driver_options[i].options[j].type != JNGD_DRVOPT_TYPE_EXEC &&
                   strcmp(driver_options[i].options[j].name, oname) == 0){
                    // Questo è il risultato cercato, return
                    strcpy(dst, driver_options[i].options[j].value);
                    UNLOCK_RET(0);
                }
            }
            break;
        }
    }
    
    // Cerca tra le globali
    for(i = 0;i < global_options_num;i++){
        if(strcmp(global_options[i].name, oname) == 0){
            strcpy(dst, global_options[i].value);
            UNLOCK_RET(0);
        }
    }
    
    UNLOCK();
    return -1; // Opzione non trovata
}


int drvoption_update_option(char* option, char* src){
    LOCK();
    
    // Percorso file
    char path[1024];
    strcpy(path, JNGD_DATA_PATH);
    
    // Dividi driver e opzione
    char option_saved[1024];
    strcpy(option_saved, option);
    
    char* driver = "";
    char* oname  = option_saved;
    
    char* dot = strstr(option_saved, ".");
    if(dot){ // Formato driver.opzione
        *dot = 0;
        driver = option_saved;
        oname  = dot + 1;
        
        // Aggiorna path
        strcat(path, "/settings/");
        strcat(path, driver);
    } else {
        // Aggiorna path (globali)
        strcat(path, "/");
        strcat(path, JNGD_GLOBAL_OPT);
    }
    
    int i, j;
    
    internal_option_t* global_equivalent = NULL; // Se != NULL è la variabile globale chiamata oname
    
    // Controlla tra le variabili globali se oname è presente
    for(i = 0;i < global_options_num;i++){
        if(strcmp(global_options[i].name, oname) == 0){
            // Se l'opzione da modificare è globale allora modifica e basta, altrimenti setta solo global_equivalent
            if(strcmp(driver, "") == 0){
                strcpy(global_options[i].value, src);
                
                UNLOCK_RET(_update_file(path, global_options, global_options_num));
            } else {
                global_equivalent = global_options + i;
                break;
            }
        }
    }
    
    // Se arriviamo qui bisogna cercare il driver e l'opzione nel driver
    for(i = 0;i < driver_options_num;i++){
        if(strcmp(driver_options[i].driver, driver) == 0) break;
    }
    if(i == driver_options_num) UNLOCK_RET(-1); // Driver non trovato
    
    for(j = 0;j < driver_options[i].options_num;j++){
        if(driver_options[i].options[j].type != JNGD_DRVOPT_TYPE_EXEC && strcmp(driver_options[i].options[j].name, oname) == 0){
            // Opzione trovata, sovrascrivi e salva
            strcpy(driver_options[i].options[j].value, src);
            UNLOCK_RET(_update_file(path, driver_options[i].options, driver_options[i].options_num));
        }
    }
    
    // Se arriviamo qui il driver esiste ma non ha l'opzione oname
    // Se però oname è un opzione globale aggiunge un opzione .is_override = 1 al driver
    
    if(!global_equivalent) UNLOCK_RET(-1); // Opzione non trovata e non globale
    
    internal_option_t opt = {
        .type = global_equivalent->type,
        .is_override = 1
    };
    strcpy(opt.name, oname);
    strcpy(opt.value, src);
    
    driver_options[i].options = realloc(driver_options[i].options, sizeof(internal_option_t) * (driver_options[i].options_num + 1));
    if(!driver_options[i].options) UNLOCK_RET(-1);
    
    memcpy(driver_options[i].options + driver_options[i].options_num, &opt, sizeof(internal_option_t));
    
    driver_options[i].options_num++;
    
    // Salva le modifiche
    int ret = _update_file(path, driver_options[i].options, driver_options[i].options_num);
    
    UNLOCK();
    return ret;
}


// Per il caricamento e il salvataggio su file
// Queste funzioni lavorano solo con il mutex bloccato

#define rsscanf(n, line, fmt...) do{if(sscanf((char*)line, fmt) != n){return 1;}}while(0)

static void _strip_whitespace(char*);

static int _parse_file(char* path, int(*cb)(internal_option_t**, int*, unsigned char*, int), internal_option_t** dst, int* dstnum, int is_global){
    unsigned char* buffer = NULL;
    int            len = 0;
    int            res, i, n;
    int            fd = open(path, O_RDONLY);
    if(fd < 0) return -1;
    
    do {
        len += 256;
        buffer = realloc(buffer, len);
        if(buffer == NULL){
            close(fd);
            return -1;
        }
        res = read(fd, buffer + len - 256, 256);
        if(res < 0){
            close(fd);
            return -1;
        }
    } while(res == 256);
    close(fd);
    
    len -= 255 - res;
    
    buffer = realloc(buffer, len);
    if(buffer == NULL) return -1;
    
    buffer[len - 1] = 0;
    
    // Separa e analizza le linee
    unsigned char* line = buffer;
    
    int dirty = 0;
    
    for(i = 0;i < len;i++){
        if(buffer[i] == '\n'){
            buffer[i] = 0;
            
            // Togli whitespace all'inizio
            n = 0;
            sscanf((char*)line, "%*[ ]%n", &n);
            if(n) memmove(line, line + n, strlen((char*)(line + n)));
            
            dirty |= cb(dst, dstnum, line, is_global);
            
            line = buffer + i + 1;
        }
    }
    dirty |= cb(dst, dstnum, line, is_global);
    free(buffer);
    
    return dirty;
}


static int _parse_def_line(internal_option_t** optdata, int* optnum, unsigned char* line, int is_global){
    if(strlen((char*)line) == 0 || line[0] == '#') return 0;
    
    internal_option_t opt = {
        .is_override = 0
    };
    
    rsscanf(1, line, "%255[^ =]", opt.name);
    
    if(strcmp(opt.name, "exec") == 0){
        // Il campo exec è particolare
        if(is_global){
            return 1;
        } else {
            opt.type = JNGD_DRVOPT_TYPE_EXEC;
            rsscanf(1, line, "%*255[^ =]%*[ =]%255s", opt.path);
        }
    } else {
        // Ottieni il tipo
        char type[256];
        rsscanf(1, line, "%*255[^ =]%*[ =]%255s", type);
        if     (strcmp(type, "int")    == 0) opt.type = JNGD_DRVOPT_TYPE_INT;
        else if(strcmp(type, "double") == 0) opt.type = JNGD_DRVOPT_TYPE_DOUBLE;
        else if(strcmp(type, "string") == 0) opt.type = JNGD_DRVOPT_TYPE_STRING;
        else return 1;
        
        rsscanf(2, line, "%*255[^ =]%*[ =]%*255[^ |]%*[ ]|%[^|]%*[ |]%511[^|]", opt.def_value, opt.description);
        
        _strip_whitespace(opt.def_value);
        
        strcpy(opt.value, opt.def_value);
    }
    
    // Cerca duplicati
    int i;
    for(i = 0;i < *optnum;i++){
        if((*optdata)[i].type == JNGD_DRVOPT_TYPE_EXEC && opt.type == JNGD_DRVOPT_TYPE_EXEC){
            memcpy((*optdata) + i, &opt, sizeof(internal_option_t));
            return 0;
        } else if(strcmp((*optdata)[i].name, opt.name) == 0){
            memcpy((*optdata) + i, &opt, sizeof(internal_option_t));
            return 0;
        }
    }
    
    // Crea l'opzione
    *optdata = realloc(*optdata, sizeof(internal_option_t) * (*optnum + 1));
    if(!(*optdata)) return -1;
    
    memcpy((*optdata) + *optnum, &opt, sizeof(internal_option_t));
    
    (*optnum)++;
    
    return 0;
}


static int _parse_setting_line(internal_option_t** optdata, int* optnum, unsigned char* line, int is_global){
    if(strlen((char*)line) == 0) return 0;
    
    char name[256];
    char value[256];
    
    rsscanf(2, line, "%255[^=]=%255[^|]", name, value);
    
    int i;
    for(i = 0;i < (*optnum);i++){
        if((*optdata)[i].type != JNGD_DRVOPT_TYPE_EXEC && strcmp((*optdata)[i].name, name) == 0){
            break;
        }
    }
    
    // Questa variabile contiene la posizione dell'opzione da sovrascrivere
    internal_option_t* overwrite;
    
    if(i == (*optnum)){
        // L'opzione non esiste, forse è un override
        if(!is_global){
            for(i = 0;i < global_options_num;i++){
                if(strcmp(global_options[i].name, name) == 0){ // È un override
                    internal_option_t opt = {
                        .type = global_options[i].type,
                        .is_override = 1
                    };
                    strcpy(opt.name, name);
                    
                    
                    *optdata = realloc(*optdata, sizeof(internal_option_t) * (*optnum + 1));
                    if(!(*optdata)) return -1;
                    
                    memcpy((*optdata) + *optnum, &opt, sizeof(internal_option_t));
                    
                    overwrite = (*optdata) + *optnum;
                    
                    (*optnum)++;
                    break;
                }
            }
            if(i == global_options_num) return 1; // L'opzione non esiste
        } else {
            return 1; // L'opzione non esiste
        }
    } else {
        overwrite = (*optdata) + i;
    }
    
    union {
        int i;
        double d;
    } sink;
    
    // Test valore
    switch((*optdata)[i].type){
        case JNGD_DRVOPT_TYPE_INT:
            rsscanf(1, value, "%i", &sink.i);
            break;
        
        case JNGD_DRVOPT_TYPE_DOUBLE:
            rsscanf(1, value, "%lf", &sink.d);
            break;
        
        default:
            // Nessun test necessario per il tipo stringa
            break;
    }
    
    memcpy(overwrite->value, value, 256);
    _strip_whitespace(overwrite->value);
    
    return 0;
}


static int _update_file(char* path, internal_option_t* options, int optnum){
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if(fd < 0) return -2;
    
    int i;
    for(i = 0;i < optnum;i++){
        if(options[i].type != JNGD_DRVOPT_TYPE_EXEC){
            write(fd, options[i].name, strlen(options[i].name));
            write(fd, "=", 1);
            write(fd, options[i].value, strlen(options[i].value));
            write(fd, "\n", 1);
        }
    }
    
    close(fd);
    return 0;
}


static void _strip_whitespace(char* ptr){
    int i;
    
    int len = strlen(ptr);
    
    int start = -1;
    int end   = -1;
    
    for(i = 0;i < len;i++){
        if(ptr[i] != ' '){
            start = i;
            break;
        }
    }
    
    for(i = len - 1;i >= start;i--){
        if(ptr[i] != ' '){
            end = i + 1;
            break;
        }
    }
    
    if(start == -1 || end == -1){
        // Stringa vuota
        ptr[0] = 0;
    }
    
    if(start == 0){
        ptr[end] = 0;
        return;
    }
    
    memmove(ptr, ptr + start, end - start);
    ptr[end - start] = 0;
}

