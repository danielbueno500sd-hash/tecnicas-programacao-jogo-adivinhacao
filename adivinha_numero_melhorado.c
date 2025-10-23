#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#define MAX_JOGADORES 5
#define MAX_NUMERO 100
#define NAME_LEN 50
#define RANKING_FILE "ranking.txt"

struct Jogador {
    char nome[NAME_LEN];
    int pontuacao; // menor é melhor
};

static struct Jogador ranking[MAX_JOGADORES];
static int total_jogadores = 0;

/* ---------- Funções utilitárias de entrada ---------- */

static void trim_newline(char *s) {
    size_t len = strlen(s);
    if (len == 0) return;
    if (s[len - 1] == '\n') s[len - 1] = '\0';
}

/* Lê uma linha do stdin para buffer (inclui \0), retorna 1 em sucesso, 0 em EOF */
static int read_line(char *buffer, size_t size) {
    if (fgets(buffer, (int)size, stdin) == NULL) return 0;
    trim_newline(buffer);
    return 1;
}

/* Lê um inteiro, re-prompta até obter um inteiro dentro de [min, max]. 
   Retorna 1 em sucesso e escreve o valor em *out, ou 0 em caso de EOF. */
static int read_int_range(const char *prompt, int min, int max, int *out) {
    char buf[128];
    char *endptr;
    long val;

    while (1) {
        if (prompt) printf("%s", prompt);
        if (!read_line(buf, sizeof(buf))) return 0; // EOF

        // permite espaços em branco
        char *p = buf;
        while (isspace((unsigned char)*p)) p++;

        if (*p == '\0') {
            printf("Entrada vazia. Tente novamente.\n");
            continue;
        }

        val = strtol(p, &endptr, 10);
        if (endptr == p || *endptr != '\0') {
            printf("Entrada inválida. Digite um número.\n");
            continue;
        }
        if (val < min || val > max) {
            printf("Número fora do intervalo (%d a %d). Tente novamente.\n", min, max);
            continue;
        }
        *out = (int)val;
        return 1;
    }
}

/* ---------- Funções de ranking (persistência e manipulação) ---------- */

static int jogador_index_por_nome(const char *nome) {
    for (int i = 0; i < total_jogadores; ++i) {
        if (strcmp(ranking[i].nome, nome) == 0) return i;
    }
    return -1;
}

static int cmp_ranking(const void *a, const void *b) {
    const struct Jogador *A = a;
    const struct Jogador *B = b;
    if (A->pontuacao != B->pontuacao) return A->pontuacao - B->pontuacao;
    return strcmp(A->nome, B->nome);
}

static void ordenar_ranking() {
    qsort(ranking, (size_t)total_jogadores, sizeof(struct Jogador), cmp_ranking);
}

static void carregar_ranking() {
    FILE *f = fopen(RANKING_FILE, "r");
    if (!f) return; // sem arquivo, começa vazio

    char line[256];
    total_jogadores = 0;
    while (fgets(line, sizeof(line), f) && total_jogadores < MAX_JOGADORES) {
        trim_newline(line);
        char *sep = strchr(line, '|');
        if (!sep) continue;
        *sep = '\0';
        char *nome = line;
        char *pont = sep + 1;
        int tent = atoi(pont);
        // protege o tamanho do nome
        strncpy(ranking[total_jogadores].nome, nome, NAME_LEN - 1);
        ranking[total_jogadores].nome[NAME_LEN - 1] = '\0';
        ranking[total_jogadores].pontuacao = tent;
        total_jogadores++;
    }
    fclose(f);
    ordenar_ranking();
}

static void salvar_ranking() {
    FILE *f = fopen(RANKING_FILE, "w");
    if (!f) {
        fprintf(stderr, "Aviso: não foi possível salvar o ranking em '%s'\n", RANKING_FILE);
        return;
    }
    for (int i = 0; i < total_jogadores; ++i) {
        // formato: nome|pontuacao\n
        fprintf(f, "%s|%d\n", ranking[i].nome, ranking[i].pontuacao);
    }
    fclose(f);
}

/* Adiciona ou atualiza um jogador no ranking seguindo as regras:
   - Se o jogador já existe, atualiza somente se a nova pontuação for melhor (menor).
   - Se houver espaço, adiciona.
   - Se estiver cheio e a nova pontuação for melhor que a pior, substitui a pior.
   - Caso contrário, não adiciona.
*/
static void registrar_pontuacao(const char *nome_jogador, int tentativas) {
    int idx = jogador_index_por_nome(nome_jogador);
    if (idx >= 0) {
        if (tentativas < ranking[idx].pontuacao) {
            ranking[idx].pontuacao = tentativas;
            printf("Pontuação de %s atualizada para %d tentativas.\n", nome_jogador, tentativas);
        } else {
            printf("Pontuação de %s não melhorou (atual: %d tentativas).\n", nome_jogador, ranking[idx].pontuacao);
        }
        ordenar_ranking();
        salvar_ranking();
        return;
    }

    if (total_jogadores < MAX_JOGADORES) {
        strncpy(ranking[total_jogadores].nome, nome_jogador, NAME_LEN - 1);
        ranking[total_jogadores].nome[NAME_LEN - 1] = '\0';
        ranking[total_jogadores].pontuacao = tentativas;
        total_jogadores++;
        ordenar_ranking();
        salvar_ranking();
        printf("Pontuação de %s registrada com %d tentativas.\n", nome_jogador, tentativas);
        return;
    }

    // ranking cheio: verificar se substitui a pior
    ordenar_ranking();
    int pior_idx = total_jogadores - 1;
    if (tentativas < ranking[pior_idx].pontuacao) {
        printf("Ranking cheio. %s substitui %s (melhor: %d < %d).\n",
               nome_jogador, ranking[pior_idx].nome, tentativas, ranking[pior_idx].pontuacao);
        strncpy(ranking[pior_idx].nome, nome_jogador, NAME_LEN - 1);
        ranking[pior_idx].nome[NAME_LEN - 1] = '\0';
        ranking[pior_idx].pontuacao = tentativas;
        ordenar_ranking();
        salvar_ranking();
    } else {
        printf("Ranking cheio e pontuação não é suficiente para entrar (sua: %d, pior: %d).\n",
               tentativas, ranking[pior_idx].pontuacao);
    }
}

/* ---------- Interface do jogo ---------- */

static void mostrar_menu() {
    printf("\n======================================\n");
    printf("           MENU PRINCIPAL\n");
    printf("======================================\n");
    printf("1. Jogar\n");
    printf("2. Mostrar Ranking\n");
    printf("0. Sair\n");
}

/* Exibe ranking ordenado (
