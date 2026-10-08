#include <stdio.h>
#include <stdlib.h>

#include "../src/fuzzy_controller.h"

static const char *NOMES_ERRO[Fuzzy::QUANTIDADE_ERROS] = {
    "muito quente", "quente", "ideal", "frio", "muito frio"};
static const char *NOMES_TENDENCIA[Fuzzy::QUANTIDADE_TENDENCIAS] = {
    "esfriando", "estavel", "aquecendo"};
static const char *NOMES_SAIDA[Fuzzy::QUANTIDADE_SAIDAS] = {
    "desligado", "fraco", "medio", "forte"};

static Fuzzy::Resultado executar(float atual, float anterior, float setpoint,
                                 bool detalhar) {
  Fuzzy::Resultado resultado;
  const float erro = setpoint - atual;
  const float tendencia = atual - anterior;
  if (!Fuzzy::calcular(erro, tendencia, resultado)) {
    fprintf(stderr, "Entradas invalidas.\n");
    exit(2);
  }

  printf("%.1f;%.1f;%.1f;%.1f;%.2f;%.2f\n", atual, anterior, erro,
         tendencia, resultado.aquecimento, resultado.resfriamento);
  if (!detalhar) return resultado;

  printf("\nPertinencias do erro:\n");
  for (int i = 0; i < Fuzzy::QUANTIDADE_ERROS; ++i) {
    printf("  %-13s = %.3f\n", NOMES_ERRO[i], resultado.pertinenciaErro[i]);
  }
  printf("Pertinencias da tendencia:\n");
  for (int i = 0; i < Fuzzy::QUANTIDADE_TENDENCIAS; ++i) {
    printf("  %-10s = %.3f\n", NOMES_TENDENCIA[i],
           resultado.pertinenciaTendencia[i]);
  }
  printf("Regras ativadas:\n");
  for (int e = 0; e < Fuzzy::QUANTIDADE_ERROS; ++e) {
    for (int t = 0; t < Fuzzy::QUANTIDADE_TENDENCIAS; ++t) {
      const int indice = e * Fuzzy::QUANTIDADE_TENDENCIAS + t;
      if (resultado.regras[indice] <= 0.001f) continue;
      const Fuzzy::Consequente consequente = Fuzzy::regra(e, t);
      printf("  R%02d: %s E %s -> A=%s, R=%s (alfa=%.3f)\n", indice + 1,
             NOMES_ERRO[e], NOMES_TENDENCIA[t],
             NOMES_SAIDA[consequente.aquecimento],
             NOMES_SAIDA[consequente.resfriamento],
             resultado.regras[indice]);
    }
  }
  printf("Centroides brutos: aquecer=%.2f%%; resfriar=%.2f%%\n",
         resultado.aquecimentoBruto, resultado.resfriamentoBruto);
  return resultado;
}

int main(int argc, char **argv) {
  float setpoint = 25.0f;
  printf("temperatura;anterior;erro;tendencia;aquecer_pct;resfriar_pct\n");
  if (argc == 1) {
    executar(15.0f, 16.0f, setpoint, false);
    executar(20.0f, 20.0f, setpoint, false);
    executar(23.0f, 22.0f, setpoint, false);
    executar(24.5f, 24.5f, setpoint, false);
    executar(25.0f, 25.0f, setpoint, false);
    executar(26.0f, 27.0f, setpoint, false);
    executar(30.0f, 29.0f, setpoint, false);
    executar(35.0f, 34.0f, setpoint, false);
    return 0;
  }
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "Uso: simulador_estufa temperatura_atual anterior [setpoint]\n");
    return 1;
  }
  const float atual = (float)atof(argv[1]);
  const float anterior = (float)atof(argv[2]);
  if (argc == 4) setpoint = (float)atof(argv[3]);
  executar(atual, anterior, setpoint, true);
  return 0;
}
