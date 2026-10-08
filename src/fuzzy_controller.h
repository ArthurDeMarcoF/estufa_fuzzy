#pragma once

#include <math.h>
#include <stdint.h>

// Controlador Mamdani independente do hardware.
//
// Entradas:
//   erro      = setpoint - temperaturaAtual [graus Celsius]
//   tendencia = temperaturaAtual - temperaturaAnterior [graus Celsius/amostra]
//
// A tendencia NAO e uma taxa por segundo. O periodo de amostragem e controlado
// em main.cpp. Os parametros abaixo ficam concentrados para facilitar o ajuste
// depois dos ensaios com a estufa real.
namespace Fuzzy {

namespace Parametros {
  // Universo e conjuntos do erro (MQ=quente demais; MF=frio demais).
  const float ERRO_MQ_INICIO = -8.0f;
  const float ERRO_MQ_FIM = -4.0f;
  const float ERRO_Q_A = -8.0f;
  const float ERRO_Q_B = -3.5f;
  const float ERRO_Q_C = -0.5f;
  const float ERRO_I_A = -1.0f;
  const float ERRO_I_B = -0.4f;
  const float ERRO_I_C = 0.4f;
  const float ERRO_I_D = 1.0f;
  const float ERRO_F_A = 0.5f;
  const float ERRO_F_B = 3.5f;
  const float ERRO_F_C = 8.0f;
  const float ERRO_MF_INICIO = 4.0f;
  const float ERRO_MF_FIM = 8.0f;

  // Universo da tendencia, em graus Celsius por amostra.
  const float TEND_ESFRIA_INICIO = -0.60f;
  const float TEND_ESFRIA_FIM = -0.15f;
  const float TEND_ESTAVEL_A = -0.30f;
  const float TEND_ESTAVEL_B = -0.08f;
  const float TEND_ESTAVEL_C = 0.08f;
  const float TEND_ESTAVEL_D = 0.30f;
  const float TEND_AQUECE_INICIO = 0.15f;
  const float TEND_AQUECE_FIM = 0.60f;

  // Conjuntos das duas saidas, ambas no universo de 0 a 100%.
  const float SAIDA_DESLIGADO_FIM = 12.0f;
  const float SAIDA_FRACO_A = 5.0f;
  const float SAIDA_FRACO_B = 25.0f;
  const float SAIDA_FRACO_C = 45.0f;
  const float SAIDA_MEDIO_A = 30.0f;
  const float SAIDA_MEDIO_B = 55.0f;
  const float SAIDA_MEDIO_C = 80.0f;
  const float SAIDA_FORTE_INICIO = 65.0f;
  const float SAIDA_FORTE_FIM = 90.0f;

  const float LIMIAR_ATUACAO = 8.0f;
  const float MARGEM_CONFLITO = 5.0f;
}

enum ConjuntoErro {
  ERRO_MUITO_QUENTE = 0,
  ERRO_QUENTE,
  ERRO_IDEAL,
  ERRO_FRIO,
  ERRO_MUITO_FRIO,
  QUANTIDADE_ERROS
};

enum ConjuntoTendencia {
  TENDENCIA_ESFRIANDO = 0,
  TENDENCIA_ESTAVEL,
  TENDENCIA_AQUECENDO,
  QUANTIDADE_TENDENCIAS
};

enum ConjuntoSaida {
  SAIDA_DESLIGADO = 0,
  SAIDA_FRACO,
  SAIDA_MEDIO,
  SAIDA_FORTE,
  QUANTIDADE_SAIDAS
};

struct Consequente {
  uint8_t aquecimento;
  uint8_t resfriamento;
};

struct Resultado {
  float pertinenciaErro[QUANTIDADE_ERROS];
  float pertinenciaTendencia[QUANTIDADE_TENDENCIAS];
  float regras[QUANTIDADE_ERROS * QUANTIDADE_TENDENCIAS];
  float agregadoAquecimento[QUANTIDADE_SAIDAS];
  float agregadoResfriamento[QUANTIDADE_SAIDAS];
  float aquecimentoBruto;
  float resfriamentoBruto;
  float aquecimento;
  float resfriamento;
  bool conflitoResolvido;
  bool semRegras;
};

inline float limitar(float x, float minimo, float maximo) {
  return x < minimo ? minimo : (x > maximo ? maximo : x);
}

inline float minimo(float a, float b) { return a < b ? a : b; }
inline float maximo(float a, float b) { return a > b ? a : b; }

inline float triangular(float x, float a, float b, float c) {
  if (x <= a || x >= c) return 0.0f;
  if (x == b) return 1.0f;
  return x < b ? (x - a) / (b - a) : (c - x) / (c - b);
}

inline float trapezoidal(float x, float a, float b, float c, float d) {
  if (x <= a || x >= d) return 0.0f;
  if (x >= b && x <= c) return 1.0f;
  return x < b ? (x - a) / (b - a) : (d - x) / (d - c);
}

inline float ombroEsquerdo(float x, float a, float b) {
  if (x <= a) return 1.0f;
  if (x >= b) return 0.0f;
  return (b - x) / (b - a);
}

inline float ombroDireito(float x, float a, float b) {
  if (x <= a) return 0.0f;
  if (x >= b) return 1.0f;
  return (x - a) / (b - a);
}

inline void limpar(Resultado &resultado) {
  for (uint8_t i = 0; i < QUANTIDADE_ERROS; ++i) {
    resultado.pertinenciaErro[i] = 0.0f;
  }
  for (uint8_t i = 0; i < QUANTIDADE_TENDENCIAS; ++i) {
    resultado.pertinenciaTendencia[i] = 0.0f;
  }
  for (uint8_t i = 0; i < QUANTIDADE_ERROS * QUANTIDADE_TENDENCIAS; ++i) {
    resultado.regras[i] = 0.0f;
  }
  for (uint8_t i = 0; i < QUANTIDADE_SAIDAS; ++i) {
    resultado.agregadoAquecimento[i] = 0.0f;
    resultado.agregadoResfriamento[i] = 0.0f;
  }
  resultado.aquecimentoBruto = 0.0f;
  resultado.resfriamentoBruto = 0.0f;
  resultado.aquecimento = 0.0f;
  resultado.resfriamento = 0.0f;
  resultado.conflitoResolvido = false;
  resultado.semRegras = false;
}

inline float pertinenciaSaida(uint8_t conjunto, float x) {
  switch (conjunto) {
    case SAIDA_DESLIGADO:
      return ombroEsquerdo(x, 0.0f, Parametros::SAIDA_DESLIGADO_FIM);
    case SAIDA_FRACO:
      return triangular(x, Parametros::SAIDA_FRACO_A,
                        Parametros::SAIDA_FRACO_B, Parametros::SAIDA_FRACO_C);
    case SAIDA_MEDIO:
      return triangular(x, Parametros::SAIDA_MEDIO_A,
                        Parametros::SAIDA_MEDIO_B, Parametros::SAIDA_MEDIO_C);
    case SAIDA_FORTE:
      return ombroDireito(x, Parametros::SAIDA_FORTE_INICIO,
                          Parametros::SAIDA_FORTE_FIM);
    default:
      return 0.0f;
  }
}

inline float defuzzificar(const float agregado[QUANTIDADE_SAIDAS],
                          bool &possuiArea) {
  float numerador = 0.0f;
  float denominador = 0.0f;

  // Centroide discreto com passo de 1%. Para cada ponto, recorta-se o
  // consequente pelo grau da regra (min) e agregam-se as regras por max.
  for (uint8_t x = 0; x <= 100; ++x) {
    float pertinenciaAgregada = 0.0f;
    for (uint8_t conjunto = 0; conjunto < QUANTIDADE_SAIDAS; ++conjunto) {
      const float recorte = minimo(agregado[conjunto],
                                   pertinenciaSaida(conjunto, (float)x));
      pertinenciaAgregada = maximo(pertinenciaAgregada, recorte);
    }
    numerador += (float)x * pertinenciaAgregada;
    denominador += pertinenciaAgregada;
  }

  possuiArea = denominador > 0.0f;
  return possuiArea ? limitar(numerador / denominador, 0.0f, 100.0f) : 0.0f;
}

// Base completa: linhas = erro [muito quente ... muito frio];
// colunas = tendencia [esfriando, estavel, aquecendo].
inline Consequente regra(uint8_t erro, uint8_t tendencia) {
  static const Consequente REGRAS[QUANTIDADE_ERROS][QUANTIDADE_TENDENCIAS] = {
    { {SAIDA_DESLIGADO, SAIDA_MEDIO}, {SAIDA_DESLIGADO, SAIDA_FORTE},
      {SAIDA_DESLIGADO, SAIDA_FORTE} },
    { {SAIDA_DESLIGADO, SAIDA_FRACO}, {SAIDA_DESLIGADO, SAIDA_MEDIO},
      {SAIDA_DESLIGADO, SAIDA_FORTE} },
    { {SAIDA_FRACO, SAIDA_DESLIGADO}, {SAIDA_DESLIGADO, SAIDA_DESLIGADO},
      {SAIDA_DESLIGADO, SAIDA_FRACO} },
    { {SAIDA_FORTE, SAIDA_DESLIGADO}, {SAIDA_MEDIO, SAIDA_DESLIGADO},
      {SAIDA_FRACO, SAIDA_DESLIGADO} },
    { {SAIDA_FORTE, SAIDA_DESLIGADO}, {SAIDA_FORTE, SAIDA_DESLIGADO},
      {SAIDA_MEDIO, SAIDA_DESLIGADO} }
  };
  return REGRAS[erro][tendencia];
}

// Retorna false somente para entradas NaN. Valores finitos fora dos intervalos
// sao aceitos e saturam nos conjuntos de ombro.
inline bool calcular(float erro, float tendencia, Resultado &resultado) {
  limpar(resultado);
  if (isnan(erro) || isnan(tendencia)) return false;

  resultado.pertinenciaErro[ERRO_MUITO_QUENTE] =
      ombroEsquerdo(erro, Parametros::ERRO_MQ_INICIO, Parametros::ERRO_MQ_FIM);
  resultado.pertinenciaErro[ERRO_QUENTE] =
      triangular(erro, Parametros::ERRO_Q_A, Parametros::ERRO_Q_B,
                 Parametros::ERRO_Q_C);
  resultado.pertinenciaErro[ERRO_IDEAL] =
      trapezoidal(erro, Parametros::ERRO_I_A, Parametros::ERRO_I_B,
                  Parametros::ERRO_I_C, Parametros::ERRO_I_D);
  resultado.pertinenciaErro[ERRO_FRIO] =
      triangular(erro, Parametros::ERRO_F_A, Parametros::ERRO_F_B,
                 Parametros::ERRO_F_C);
  resultado.pertinenciaErro[ERRO_MUITO_FRIO] =
      ombroDireito(erro, Parametros::ERRO_MF_INICIO, Parametros::ERRO_MF_FIM);

  resultado.pertinenciaTendencia[TENDENCIA_ESFRIANDO] =
      ombroEsquerdo(tendencia, Parametros::TEND_ESFRIA_INICIO,
                    Parametros::TEND_ESFRIA_FIM);
  resultado.pertinenciaTendencia[TENDENCIA_ESTAVEL] =
      trapezoidal(tendencia, Parametros::TEND_ESTAVEL_A,
                  Parametros::TEND_ESTAVEL_B, Parametros::TEND_ESTAVEL_C,
                  Parametros::TEND_ESTAVEL_D);
  resultado.pertinenciaTendencia[TENDENCIA_AQUECENDO] =
      ombroDireito(tendencia, Parametros::TEND_AQUECE_INICIO,
                   Parametros::TEND_AQUECE_FIM);

  float maiorRegra = 0.0f;
  for (uint8_t e = 0; e < QUANTIDADE_ERROS; ++e) {
    for (uint8_t t = 0; t < QUANTIDADE_TENDENCIAS; ++t) {
      const uint8_t indice = e * QUANTIDADE_TENDENCIAS + t;
      const float ativacao = minimo(resultado.pertinenciaErro[e],
                                    resultado.pertinenciaTendencia[t]);
      resultado.regras[indice] = ativacao;
      maiorRegra = maximo(maiorRegra, ativacao);

      const Consequente consequente = regra(e, t);
      resultado.agregadoAquecimento[consequente.aquecimento] = maximo(
          resultado.agregadoAquecimento[consequente.aquecimento], ativacao);
      resultado.agregadoResfriamento[consequente.resfriamento] = maximo(
          resultado.agregadoResfriamento[consequente.resfriamento], ativacao);
    }
  }

  resultado.semRegras = maiorRegra <= 0.0f;
  bool areaAquecimento = false;
  bool areaResfriamento = false;
  resultado.aquecimentoBruto =
      defuzzificar(resultado.agregadoAquecimento, areaAquecimento);
  resultado.resfriamentoBruto =
      defuzzificar(resultado.agregadoResfriamento, areaResfriamento);
  if (!areaAquecimento || !areaResfriamento) resultado.semRegras = true;

  resultado.aquecimento = resultado.aquecimentoBruto;
  resultado.resfriamento = resultado.resfriamentoBruto;
  if (resultado.aquecimento < Parametros::LIMIAR_ATUACAO) {
    resultado.aquecimento = 0.0f;
  }
  if (resultado.resfriamento < Parametros::LIMIAR_ATUACAO) {
    resultado.resfriamento = 0.0f;
  }

  // A fuzzificacao pode ativar conjuntos vizinhos. Esta arbitragem e uma
  // camada final de seguranca: nunca devolve os dois atuadores ligados.
  if (resultado.aquecimento > 0.0f && resultado.resfriamento > 0.0f) {
    resultado.conflitoResolvido = true;
    const float diferenca = fabsf(resultado.aquecimento - resultado.resfriamento);
    if (diferenca <= Parametros::MARGEM_CONFLITO) {
      resultado.aquecimento = 0.0f;
      resultado.resfriamento = 0.0f;
    } else if (resultado.aquecimento > resultado.resfriamento) {
      resultado.resfriamento = 0.0f;
    } else {
      resultado.aquecimento = 0.0f;
    }
  }

  return true;
}

}  // namespace Fuzzy
