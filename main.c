// minish.c -- Jacques.Madelaine@info.unicaen.fr --
// un mini shell
#define _GNU_SOURCE
// GNU_SOURCE pour avoir getline qui permet de borner la lecture de ligne
// pas comme scanf
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>

int nbMaxPid = 4;
int listePid[4];
int nbPid = 0;

char *listeCmds[10];
int indiceDebut = 0;
int indiceFin = 0;
int nbElements = 0;

int pidATuer;

typedef enum {
    false,
    true
}
bool;

// supprime les blancs qui traînent en queue et retourne la nelle longueur
int
nettoie(char *ligne) {
    int lg = strlen(ligne);
    while (isspace(ligne[lg - 1]))
        lg--;
    ligne[lg] = '\0';
    return (lg);
}

// Fait une tableau d'arguments avec les mots de la ligne
void
mkargs(char *args[], int MAXARGS, char *ligne) {
    static char IFS[] = " \t";
    int i = 0;
    args[i++] = strtok(ligne, IFS);
    while ((args[i++] = strtok(0, IFS)))
        if (i > MAXARGS) {
            fprintf(stderr, "plus de %d arguments, le reste n'est pas pris en compte\n", MAXARGS);
            return;
        }
}

#include <setjmp.h>

jmp_buf env;             // le "jump buffer" du début de lecture

// Fonction attachée à SIGINT et SIGQUIT
void
nvligne(int sig) {
    printf("Interruption %d\n", sig);
    longjmp(env, 1);       // pour forcer la reprise de la lecture
}

void
echanger(int *A, int *B) {
    int AIDE = *A;
    *A = *B;
    *B = AIDE;
}

int
trouver_indice(int pid) {
    for (int i = 0; i < nbPid; i++) {
        if (listePid[i] == pid) {
            return i;
        }
    }
    return -1;
}


void
ajoutCmd(char *ligne) {
    //printf("Ajout Commande : %s \n",ligne);
    listeCmds[nbElements] = ligne;
    nbElements++;
    indiceFin++;

    if (indiceDebut == sizeof(listeCmds)) {
        indiceDebut = 0;
    } else if (indiceFin == sizeof(listeCmds)) {
        indiceFin = 0;
    }
}

void
supprimerCmd() {
    //printf("Supprimer Commande \n");
    nbElements--;
    indiceDebut++;

    if (indiceDebut == sizeof(listeCmds)) {
        indiceDebut = 0;
    } else if (indiceFin == sizeof(listeCmds)) {
        indiceFin = 0;
    }
}

void
gestionAlarme(int numSig) {
    printf("> La commande a dépassé 10 secondes de chargement. Arret du processus %d. \n", pidATuer);
    kill(pidATuer, SIGKILL);
}

void
lancerCmd(char *args[], int size, char *ligne) {
    //printf("Lancer commande : %s \n",ligne);
    signal(SIGALRM, gestionAlarme);
    int ppid;
    switch (ppid = fork()) {
        case -1:
            perror("fork");
        case 0:
            switch (ppid = fork()) {
                case -1:
                    perror("fork");
                case 0:
                    signal(SIGQUIT, SIG_DFL);
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTERM, SIG_DFL);

                    // la commande en arrière plan ne doit pas lire l'entrée standard
                    int fd = open("/dev/null", O_RDONLY);
                    close(0);
                    dup(fd);

                    sleep(5);
                    mkargs(args, size, ligne);
                    execvp(args[0], args);
                    perror(args[0]);
                default:
                    pidATuer = ppid;
                    //printf("Alarm 10 sec %d \n", pidATuer);
                    alarm(10);
                    wait(NULL);
                    exit(10);
            }
    }
}


void
hand(int sig) {
    int pid = wait(NULL);
    //printf("Handler %d %d \n", sig, pid);
    if (pid != -1) {
        int indice = trouver_indice(pid);
        echanger(&listePid[indice], &listePid[nbPid] - 1);
        nbPid--;
    }
}

int
main() {
    char *ps1 = getenv("PS1"); // Le prompt du Bourne (Again) SHell
    char *ligne = 0;
    size_t MAXLIGNE = 0;
    char *args[256];

    signal(SIGTERM, SIG_IGN);
    if (ps1 == 0)
        ps1 = "$ ";
    setjmp(env);           // C'est ici que l'on revient après une interruption
    signal(SIGINT, nvligne);
    signal(SIGQUIT, nvligne);
    signal(SIGCHLD, hand);
    while (1) {
        int lgligne;
        int pid;
        printf("%s", ps1);
        if (getline(&ligne, &MAXLIGNE, stdin) == -1)
            break;             // C'est fini
        if ((lgligne = nettoie(ligne)) == 0) {
            printf("Nombre de processus en cours d'execution : %d \n", nbPid);
            continue;          // juste une ligne vide, on continue
        }

        printf("Nombre de processus en cours d'execution : %d \n",
               nbPid + 1); // +1 prise en compte de la commande prochaine

        ajoutCmd(ligne);

        printf("\n %s", listeCmds[0]);
        printf("\n %s", listeCmds[1]);
        printf("\n %s", listeCmds[2]);
        printf("\n");

        if (nbPid < nbMaxPid) {
            switch (pid = fork()) {
                case -1:
                    perror("fork");
                case 0: // Fils
                    if (nbElements != 0) {
                        lancerCmd(args, sizeof(args), listeCmds[indiceDebut]);
                        supprimerCmd();
                    }
                default: // Père
                    listePid[nbPid] = pid;
                    nbPid++;
            }
        } else {
            printf("Trop de processus en cours \n");
        }
    }
    return 0;
}
