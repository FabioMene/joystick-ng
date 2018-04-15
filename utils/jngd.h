/*
 * jngd.h
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
 
// Questo file contiene definizioni in comune a jngd e jngctl

#ifndef JNGD_H
#define JNGD_H 1

// Di default il socket è leggibile e scrivibile solo per root e il gruppo input
#define SOCKET_FILE  "/var/run/jngd.socket"
#define SOCKET_GROUP "input"

// Azioni disponibili. Di default i campi Dimensione (Dim.) comprendono i terminatori delle stringhe

#define ACTION_DRV_LAUNCH 0x00
// Avvia un driver, eseguendo il file specificato con 'exec=' nel .def del driver
// Al processo avviato viene aggiunta la variabile d'ambiente JNG_DRIVER contentente il nome del driver
// Il pacchetto è
//   1B   Dim. Nome driver
//   1B   Dim. Argomenti
//   var  Nome driver
//   var  Argomenti (stringa concatenata di tutti gli argomenti, terminati da 0)


#define ACTION_DRV_MODSETT 0x01
// Modifica un impostazione di un driver
//   1B   Dim. Nome driver
//   1B   Dim. Nome impostazione
//   1B   Dim. Valore impostazione
//   var  Nome driver
//   var  Nome impostazione
//   var  Valore impostazione

#endif
