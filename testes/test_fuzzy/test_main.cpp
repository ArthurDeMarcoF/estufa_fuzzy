#include <unity.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../../src/fuzzy_controller.h"

void setUp() {}
void tearDown() {}

static Fuzzy::Resultado calcularTemperatura(float atual, float anterior) {
  Fuzzy::Resultado resultado;
  TEST_ASSERT_TRUE(Fuzzy::calcular(25.0f - atual, atual - anterior, resultado));
  return resultado;
}

static void registrarCenario(float atual, float anterior,
                             const Fuzzy::Resultado &resultado) {
  printf("SIM T=%.1f anterior=%.1f dT=%.1f aquecer=%.2f resfriar=%.2f\n",
         atual, anterior, atual - anterior, resultado.aquecimento,
         resultado.resfriamento);
}

void test_funcoes_de_pertinencia() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                           Fuzzy::triangular(2.0f, 0.0f, 2.0f, 4.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f,
                           Fuzzy::triangular(1.0f, 0.0f, 2.0f, 4.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                           Fuzzy::trapezoidal(0.0f, -1.0f, -0.4f, 0.4f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                           Fuzzy::ombroEsquerdo(1.0f, -1.0f, 0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                           Fuzzy::ombroDireito(1.0f, 0.0f, 1.0f));
}

void test_base_possui_as_15_regras_sem_consequentes_conflitantes() {
  const float centrosErro[Fuzzy::QUANTIDADE_ERROS] = {-10.0f, -3.5f, 0.0f,
                                                       3.5f, 10.0f};
  const float centrosTendencia[Fuzzy::QUANTIDADE_TENDENCIAS] = {-1.0f, 0.0f,
                                                                1.0f};
  for (uint8_t e = 0; e < Fuzzy::QUANTIDADE_ERROS; ++e) {
    for (uint8_t t = 0; t < Fuzzy::QUANTIDADE_TENDENCIAS; ++t) {
      const Fuzzy::Consequente consequente = Fuzzy::regra(e, t);
      TEST_ASSERT_LESS_THAN_UINT8(Fuzzy::QUANTIDADE_SAIDAS,
                                  consequente.aquecimento);
      TEST_ASSERT_LESS_THAN_UINT8(Fuzzy::QUANTIDADE_SAIDAS,
                                  consequente.resfriamento);
      TEST_ASSERT_TRUE(consequente.aquecimento == Fuzzy::SAIDA_DESLIGADO ||
                       consequente.resfriamento == Fuzzy::SAIDA_DESLIGADO);

      Fuzzy::Resultado resultado;
      TEST_ASSERT_TRUE(Fuzzy::calcular(centrosErro[e], centrosTendencia[t],
                                       resultado));
      const uint8_t indice = e * Fuzzy::QUANTIDADE_TENDENCIAS + t;
      TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, resultado.regras[indice]);
    }
  }
}

void test_cenarios_qualitativos_do_enunciado() {
  Fuzzy::Resultado resultado = calcularTemperatura(15.0f, 16.0f);
  registrarCenario(15.0f, 16.0f, resultado);
  TEST_ASSERT_GREATER_THAN(75.0f, resultado.aquecimento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.resfriamento);

  resultado = calcularTemperatura(20.0f, 20.0f);
  registrarCenario(20.0f, 20.0f, resultado);
  TEST_ASSERT_GREATER_THAN(40.0f, resultado.aquecimento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.resfriamento);

  resultado = calcularTemperatura(23.0f, 22.0f);
  registrarCenario(23.0f, 22.0f, resultado);
  TEST_ASSERT_GREATER_THAN(10.0f, resultado.aquecimento);
  TEST_ASSERT_LESS_THAN(45.0f, resultado.aquecimento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.resfriamento);

  resultado = calcularTemperatura(24.5f, 24.5f);
  registrarCenario(24.5f, 24.5f, resultado);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.aquecimento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.resfriamento);

  resultado = calcularTemperatura(25.0f, 25.0f);
  registrarCenario(25.0f, 25.0f, resultado);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.aquecimento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.resfriamento);

  resultado = calcularTemperatura(26.0f, 27.0f);
  registrarCenario(26.0f, 27.0f, resultado);
  TEST_ASSERT_GREATER_THAN(10.0f, resultado.resfriamento);
  TEST_ASSERT_LESS_THAN(45.0f, resultado.resfriamento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.aquecimento);

  resultado = calcularTemperatura(30.0f, 29.0f);
  registrarCenario(30.0f, 29.0f, resultado);
  TEST_ASSERT_GREATER_THAN(75.0f, resultado.resfriamento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.aquecimento);

  resultado = calcularTemperatura(35.0f, 34.0f);
  registrarCenario(35.0f, 34.0f, resultado);
  TEST_ASSERT_GREATER_THAN(75.0f, resultado.resfriamento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.aquecimento);
}

void test_transicoes_saturacao_e_ausencia_de_conflito() {
  for (float erro = -15.0f; erro <= 15.0f; erro += 1.0f) {
    for (float tendencia = -2.0f; tendencia <= 2.0f; tendencia += 0.1f) {
      Fuzzy::Resultado resultado;
      TEST_ASSERT_TRUE(Fuzzy::calcular(erro, tendencia, resultado));
      TEST_ASSERT_FALSE(resultado.semRegras);
      TEST_ASSERT_TRUE(resultado.aquecimento >= 0.0f &&
                       resultado.aquecimento <= 100.0f);
      TEST_ASSERT_TRUE(resultado.resfriamento >= 0.0f &&
                       resultado.resfriamento <= 100.0f);
      TEST_ASSERT_TRUE(resultado.aquecimento == 0.0f ||
                       resultado.resfriamento == 0.0f);
    }
  }
}

void test_erro_zero_e_entrada_invalida() {
  Fuzzy::Resultado resultado;
  TEST_ASSERT_TRUE(Fuzzy::calcular(0.0f, 0.0f, resultado));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.aquecimento);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resultado.resfriamento);
  TEST_ASSERT_FALSE(Fuzzy::calcular(NAN, 0.0f, resultado));
  TEST_ASSERT_FALSE(Fuzzy::calcular(0.0f, NAN, resultado));

  float agregadoVazio[Fuzzy::QUANTIDADE_SAIDAS] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool possuiArea = true;
  TEST_ASSERT_FLOAT_WITHIN(
      0.001f, 0.0f, Fuzzy::defuzzificar(agregadoVazio, possuiArea));
  TEST_ASSERT_FALSE(possuiArea);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_funcoes_de_pertinencia);
  RUN_TEST(test_base_possui_as_15_regras_sem_consequentes_conflitantes);
  RUN_TEST(test_cenarios_qualitativos_do_enunciado);
  RUN_TEST(test_transicoes_saturacao_e_ausencia_de_conflito);
  RUN_TEST(test_erro_zero_e_entrada_invalida);
  return UNITY_END();
}
