/*
 * user-elevated.c
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

#include <pwd.h>
#include <grp.h>

#include "jngd.h"

#ifndef GET_PEERCRED
#define GET_PEERCRED 0x11
#endif

// Controlla se l'utente connesso al socket appartiene al gruppo ELEVATED_GROUP
int isUserElevated(int un_sock){
    // Per rendere getgrnam e getpwuid thread-safe
    char pw_buffer[8192];
    char gr_buffer[8192];
    
    struct passwd pw_sto;
    struct group  gr_sto;
    
    // Ottieni le credenziali del client
    struct {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    } cred;
    
    socklen_t credlen = sizeof(cred);
    
    if(getsockopt(un_sock, SOL_SOCKET, GET_PEERCRED, &cred, &credlen) < 0){
        return -1;
    }
    
    if(cred.uid == 0) return 1;
    
    // Ottieni il GID di ELEVATED_GROUP
    struct group* egrp;
    getgrnam_r(ELEVATED_GROUP, &gr_sto, gr_buffer, 8192, &egrp);
    if(!egrp) return -1;
    
    // Ottieni il nome di cred.uid
    struct passwd* pw;
    getpwuid_r(cred.uid, &pw_sto, pw_buffer, 8192, &pw);
    if(!pw) return -1;
    
    // Ottieni tutti i gid di cred.uid
    gid_t groups[1024];
    int max = 1024;
    
    if(getgrouplist(pw->pw_name, cred.gid, groups, &max) < 0) return -1;
    
    // Controlla l'appartenenza al gruppo
    int i;
    for(i = 0;i < max;i++){
        if(groups[i] == egrp->gr_gid) return 1;
    }
    
    // Non è privilegiato
    return 0;
}

