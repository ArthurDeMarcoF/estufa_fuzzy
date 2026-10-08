#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <stdlib.h>
#include <string.h>

#include "fuzzy_controller.h"

// Pinos do diagnostico disponibilizado pelo professor.
#define PINO_DHT 5
#define PINO_FIM_CURSO 4
#define PINO_LAMPADA 9
#define PINO_COOLER 10
#define DHTTYPE DHT11

// Barreiras independentes de seguranca. Ambas permanecem em zero ate a
// validacao presencial do circuito de potencia e do fim de curso.
#define HABILITAR_SAIDAS_FISICAS 0
#define FIM_CURSO_VALIDADO 0

// Estes niveis tambem precisam ser conferidos antes de habilitar o hardware.
#define NIVEL_FIM_CURSO_SEGURO LOW
#define NIVEL_LAMPADA_LIGADA HIGH
#define NIVEL_LAMPADA_DESLIGADA LOW

const float SETPOINT_PADRAO = 25.0f;
const float SETPOINT_MINIMO = 15.0f;
const float SETPOINT_MAXIMO = 35.0f;
const float TEMPERATURA_MINIMA = 0.0f;
const float TEMPERATURA_LIMITE = 40.0f;
const float COOLER_MINIMO_PERCENTUAL = 20.0f;
const unsigned long INTERVALO_SENSOR_MS = 2500UL;
const unsigned long JANELA_LAMPADA_MS = 10000UL;

DHT dht(PINO_DHT, DHTTYPE);

float setpoint = SETPOINT_PADRAO;
float temperaturaAnterior = SETPOINT_PADRAO;
float ultimaTemperatura = 0.0f;
float ultimaUmidade = 0.0f;
float ultimoErro = 0.0f;
float ultimaTendencia = 0.0f;
bool sensorTemHistorico = false;
bool ultimoCalculoValido = false;
bool resultadoOrigemSensor = false;
bool execucaoFisica = false;
unsigned long ultimaLeitura = 0;
unsigned long inicioJanela = 0;
char linha[64];
uint8_t posicaoLinha = 0;
Fuzzy::Resultado ultimoResultado;

void desligarSaidasFisicas() {
  digitalWrite(PINO_LAMPADA, NIVEL_LAMPADA_DESLIGADA);
  analogWrite(PINO_COOLER, 0);
}

void entrarModoSeguro(const __FlashStringHelper *motivo) {
  execucaoFisica = false;
  desligarSaidasFisicas();
  if (motivo != NULL) {
    Serial.print(F("MODO SEGURO: "));
    Serial.println(motivo);
  }
}

void imprimirNomeErro(uint8_t indice) {
  switch (indice) {
    case Fuzzy::ERRO_MUITO_QUENTE: Serial.print(F("muito quente")); break;
    case Fuzzy::ERRO_QUENTE: Serial.print(F("quente")); break;
    case Fuzzy::ERRO_IDEAL: Serial.print(F("ideal")); break;
    case Fuzzy::ERRO_FRIO: Serial.print(F("frio")); break;
    default: Serial.print(F("muito frio")); break;
  }
}

void imprimirNomeTendencia(uint8_t indice) {
  switch (indice) {
    case Fuzzy::TENDENCIA_ESFRIANDO: Serial.print(F("esfriando")); break;
    case Fuzzy::TENDENCIA_ESTAVEL: Serial.print(F("estavel")); break;
    default: Serial.print(F("aquecendo")); break;
  }
}

void imprimirNomeSaida(uint8_t indice) {
  switch (indice) {
    case Fuzzy::SAIDA_DESLIGADO: Serial.print(F("desligado")); break;
    case Fuzzy::SAIDA_FRACO: Serial.print(F("fraco")); break;
    case Fuzzy::SAIDA_MEDIO: Serial.print(F("medio")); break;
    default: Serial.print(F("forte")); break;
  }
}

void imprimirResumo(float temperatura, float anterior,
                    const Fuzzy::Resultado &resultado) {
  Serial.print(F("T="));
  Serial.print(temperatura, 1);
  Serial.print(F(" C | anterior="));
  Serial.print(anterior, 1);
  Serial.print(F(" C | SP="));
  Serial.print(setpoint, 1);
  Serial.print(F(" C | erro="));
  Serial.print(setpoint - temperatura, 2);
  Serial.print(F(" C | dT="));
  Serial.print(temperatura - anterior, 2);
  Serial.print(F(" C/amostra | aquecer="));
  Serial.print(resultado.aquecimento, 1);
  Serial.print(F("% | resfriar="));
  Serial.print(resultado.resfriamento, 1);
  Serial.println(F("%"));
}

void imprimirVetor(const __FlashStringHelper *titulo, const float *valores,
                   uint8_t quantidade, bool vetorErro) {
  Serial.println(titulo);
  for (uint8_t i = 0; i < quantidade; ++i) {
    Serial.print(F("  "));
    if (vetorErro) imprimirNomeErro(i); else imprimirNomeTendencia(i);
    Serial.print(F(" = "));
    Serial.println(valores[i], 3);
  }
}

void imprimirAgregacao(const __FlashStringHelper *titulo,
                       const float valores[Fuzzy::QUANTIDADE_SAIDAS]) {
  Serial.println(titulo);
  for (uint8_t i = 0; i < Fuzzy::QUANTIDADE_SAIDAS; ++i) {
    Serial.print(F("  "));
    imprimirNomeSaida(i);
    Serial.print(F(" = "));
    Serial.println(valores[i], 3);
  }
}

void imprimirDetalhes(float temperatura, float anterior,
                      const Fuzzy::Resultado &resultado) {
  Serial.println(F("--- DIAGNOSTICO FUZZY ---"));
  imprimirResumo(temperatura, anterior, resultado);
  imprimirVetor(F("Pertinencias do erro:"), resultado.pertinenciaErro,
                Fuzzy::QUANTIDADE_ERROS, true);
  imprimirVetor(F("Pertinencias da tendencia:"),
                resultado.pertinenciaTendencia,
                Fuzzy::QUANTIDADE_TENDENCIAS, false);
  Serial.println(F("Regras ativadas (operador E = minimo):"));
  bool encontrou = false;
  for (uint8_t e = 0; e < Fuzzy::QUANTIDADE_ERROS; ++e) {
    for (uint8_t t = 0; t < Fuzzy::QUANTIDADE_TENDENCIAS; ++t) {
      const uint8_t indice = e * Fuzzy::QUANTIDADE_TENDENCIAS + t;
      if (resultado.regras[indice] <= 0.001f) continue;
      encontrou = true;
      const Fuzzy::Consequente consequente = Fuzzy::regra(e, t);
      Serial.print(F("  R"));
      Serial.print(indice + 1);
      Serial.print(F(": "));
      imprimirNomeErro(e);
      Serial.print(F(" E "));
      imprimirNomeTendencia(t);
      Serial.print(F(" -> A="));
      imprimirNomeSaida(consequente.aquecimento);
      Serial.print(F(", R="));
      imprimirNomeSaida(consequente.resfriamento);
      Serial.print(F(" | alfa="));
      Serial.println(resultado.regras[indice], 3);
    }
  }
  if (!encontrou) Serial.println(F("  nenhuma"));
  imprimirAgregacao(F("Agregacao do aquecimento (operador OU = maximo):"),
                    resultado.agregadoAquecimento);
  imprimirAgregacao(F("Agregacao do resfriamento (operador OU = maximo):"),
                    resultado.agregadoResfriamento);
  Serial.print(F("Centroide bruto: aquecer="));
  Serial.print(resultado.aquecimentoBruto, 2);
  Serial.print(F("% | resfriar="));
  Serial.print(resultado.resfriamentoBruto, 2);
  Serial.println(F("%"));
  Serial.print(F("Saida apos limiar/arbitragem: aquecer="));
  Serial.print(resultado.aquecimento, 2);
  Serial.print(F("% | resfriar="));
  Serial.print(resultado.resfriamento, 2);
  Serial.println(F("%"));
  if (resultado.conflitoResolvido) {
    Serial.println(F("Observacao: a arbitragem eliminou comando simultaneo."));
  }
  if (resultado.semRegras) {
    Serial.println(F("ALERTA: nenhuma area de saida foi produzida pelas regras."));
  }
  Serial.println(F("--- FIM DO DIAGNOSTICO ---"));
}

bool calcular(float temperatura, float anterior, bool origemSensor,
              bool detalhado) {
  ultimoErro = setpoint - temperatura;
  ultimaTendencia = temperatura - anterior;
  if (!Fuzzy::calcular(ultimoErro, ultimaTendencia, ultimoResultado)) {
    ultimoCalculoValido = false;
    resultadoOrigemSensor = false;
    Serial.println(F("ERRO: entradas invalidas para o controlador fuzzy."));
    return false;
  }
  ultimaTemperatura = temperatura;
  ultimoCalculoValido = true;
  resultadoOrigemSensor = origemSensor;
  if (detalhado) imprimirDetalhes(temperatura, anterior, ultimoResultado);
  else imprimirResumo(temperatura, anterior, ultimoResultado);
  return true;
}

void simular(float atual, float anterior, bool detalhado) {
  entrarModoSeguro(NULL);
  Serial.println(F("[SIMULACAO - saidas fisicas bloqueadas]"));
  calcular(atual, anterior, false, detalhado);
}

void testarCenarios() {
  entrarModoSeguro(NULL);
  Serial.println(F("--- 8 CENARIOS SIMULADOS (nenhum pino sera acionado) ---"));
  simular(15.0f, 16.0f, false);   // esfriando
  simular(20.0f, 20.0f, false);   // estavel
  simular(23.0f, 22.0f, false);   // aquecendo
  simular(24.5f, 24.5f, false);   // estavel
  simular(25.0f, 25.0f, false);   // estavel
  simular(26.0f, 27.0f, false);   // esfriando
  simular(30.0f, 29.0f, false);   // aquecendo
  simular(35.0f, 34.0f, false);   // aquecendo
  Serial.println(F("--- FIM DOS CENARIOS; use 'f' para detalhar o ultimo ---"));
}

void aplicarAtuadores(unsigned long agora) {
#if HABILITAR_SAIDAS_FISICAS && FIM_CURSO_VALIDADO
  if (!execucaoFisica) {
    desligarSaidasFisicas();
    return;
  }
  if (!ultimoCalculoValido || !resultadoOrigemSensor) {
    entrarModoSeguro(F("nao ha calculo valido originado do sensor"));
    return;
  }
  if (digitalRead(PINO_FIM_CURSO) != NIVEL_FIM_CURSO_SEGURO) {
    entrarModoSeguro(F("fim de curso fora do estado seguro"));
    return;
  }

  if (agora - inicioJanela >= JANELA_LAMPADA_MS) inicioJanela = agora;
  const unsigned long tempoLigado = (unsigned long)(
      JANELA_LAMPADA_MS * ultimoResultado.aquecimento / 100.0f);
  const bool lampadaLigada = (agora - inicioJanela) < tempoLigado;
  digitalWrite(PINO_LAMPADA,
               lampadaLigada ? NIVEL_LAMPADA_LIGADA
                              : NIVEL_LAMPADA_DESLIGADA);

  float percentualCooler = ultimoResultado.resfriamento;
  if (percentualCooler < COOLER_MINIMO_PERCENTUAL) percentualCooler = 0.0f;
  const uint8_t pwmCooler = (uint8_t)(255.0f * percentualCooler / 100.0f);
  analogWrite(PINO_COOLER, pwmCooler);
#else
  (void)agora;
  if (execucaoFisica) execucaoFisica = false;
  desligarSaidasFisicas();
#endif
}

void lerSensor() {
  const float temperatura = dht.readTemperature();
  const float umidade = dht.readHumidity();
  if (isnan(temperatura) || isnan(umidade)) {
    ultimoCalculoValido = false;
    resultadoOrigemSensor = false;
    sensorTemHistorico = false;
    entrarModoSeguro(F("falha de leitura do DHT11"));
    return;
  }
  if (temperatura < TEMPERATURA_MINIMA || temperatura >= TEMPERATURA_LIMITE) {
    ultimoCalculoValido = false;
    resultadoOrigemSensor = false;
    sensorTemHistorico = false;
    entrarModoSeguro(F("temperatura fora do limite configurado"));
    return;
  }

  const float anterior = sensorTemHistorico ? temperaturaAnterior : temperatura;
  ultimaUmidade = umidade;
  Serial.print(F("Umidade="));
  Serial.print(umidade, 1);
  Serial.print(F("% | "));
  if (calcular(temperatura, anterior, true, false)) {
    temperaturaAnterior = temperatura;
    sensorTemHistorico = true;
  } else {
    entrarModoSeguro(F("falha no calculo fuzzy"));
  }
}

void imprimirEstado() {
  Serial.println(F("--- ESTADO ---"));
  Serial.print(F("Setpoint: "));
  Serial.print(setpoint, 1);
  Serial.println(F(" C"));
  Serial.print(F("Modo fisico: "));
  Serial.println(execucaoFisica ? F("ATIVO") : F("BLOQUEADO/SEGURO"));
  Serial.print(F("Chave de compilacao das saidas: "));
  Serial.println(HABILITAR_SAIDAS_FISICAS ? F("habilitada") : F("desabilitada"));
  Serial.print(F("Fim de curso validado: "));
  Serial.println(FIM_CURSO_VALIDADO ? F("sim") : F("nao"));
  Serial.print(F("Nivel atual do fim de curso: "));
  Serial.println(digitalRead(PINO_FIM_CURSO) == HIGH ? F("HIGH") : F("LOW"));
  Serial.print(F("Historico valido do sensor: "));
  Serial.println(sensorTemHistorico ? F("sim") : F("nao"));
  if (ultimoCalculoValido) {
    Serial.print(F("Origem do ultimo calculo: "));
    Serial.println(resultadoOrigemSensor ? F("sensor") : F("simulacao"));
    Serial.print(F("Ultima umidade do sensor: "));
    Serial.print(ultimaUmidade, 1);
    Serial.println(F("%"));
    imprimirResumo(ultimaTemperatura, ultimaTemperatura - ultimaTendencia,
                   ultimoResultado);
  } else {
    Serial.println(F("Ainda nao ha calculo valido."));
  }
  Serial.println(F("Saidas fisicas permanecem LOW/PWM 0 no modo seguro."));
  Serial.println(F("--- FIM DO ESTADO ---"));
}

void imprimirAjuda() {
  Serial.println(F("Comandos:"));
  Serial.println(F("  h             ajuda"));
  Serial.println(F("  s             estado atual"));
  Serial.println(F("  x             executar 8 cenarios simulados"));
  Serial.println(F("  f             detalhar o ultimo calculo fuzzy"));
  Serial.println(F("  t 23 22       simular T atual=23 C e anterior=22 C"));
  Serial.println(F("  p 25.5        alterar setpoint (15 a 35 C)"));
  Serial.println(F("  r             retornar ao modo seguro"));
  Serial.println(F("  a             solicitar modo fisico (bloqueado de fabrica)"));
}

bool lerDoisFloats(char *texto, float &primeiro, float &segundo) {
  char *token = strtok(texto, " ,;");
  if (token == NULL) return false;
  char *fim = NULL;
  primeiro = (float)strtod(token, &fim);
  if (fim == token || *fim != '\0') return false;
  token = strtok(NULL, " ,;");
  if (token == NULL) return false;
  segundo = (float)strtod(token, &fim);
  return fim != token && *fim == '\0';
}

void alterarSetpoint(char *argumento) {
  while (*argumento == ' ') ++argumento;
  char *fim = NULL;
  const float novoSetpoint = (float)strtod(argumento, &fim);
  while (fim != NULL && *fim == ' ') ++fim;
  if (fim == argumento || fim == NULL || *fim != '\0' ||
      isnan(novoSetpoint) || novoSetpoint < SETPOINT_MINIMO ||
      novoSetpoint > SETPOINT_MAXIMO) {
    Serial.println(F("Uso: p 25.5 (valor permitido: 15 a 35 C)"));
    return;
  }
  entrarModoSeguro(NULL);
  setpoint = novoSetpoint;
  ultimoCalculoValido = false;
  resultadoOrigemSensor = false;
  Serial.print(F("Setpoint alterado para "));
  Serial.print(setpoint, 1);
  Serial.println(F(" C; modo fisico continua bloqueado."));
}

void solicitarModoFisico() {
#if HABILITAR_SAIDAS_FISICAS && FIM_CURSO_VALIDADO
  if (!ultimoCalculoValido || !resultadoOrigemSensor || !sensorTemHistorico) {
    Serial.println(F("BLOQUEADO: aguarde uma leitura valida do sensor."));
    return;
  }
  if (digitalRead(PINO_FIM_CURSO) != NIVEL_FIM_CURSO_SEGURO) {
    Serial.println(F("BLOQUEADO: fim de curso fora do estado seguro."));
    return;
  }
  execucaoFisica = true;
  inicioJanela = millis();
  Serial.println(F("MODO FISICO ATIVO: supervisione a estufa."));
#else
  Serial.println(F("BLOQUEADO: hardware e/ou fim de curso nao validados."));
  Serial.println(F("Nao altere as chaves sem validar o circuito em bancada."));
#endif
}

void comandoSerial(char *texto) {
  if (strcmp(texto, "h") == 0) {
    imprimirAjuda();
  } else if (strcmp(texto, "s") == 0) {
    imprimirEstado();
  } else if (strcmp(texto, "x") == 0) {
    testarCenarios();
  } else if (strcmp(texto, "f") == 0) {
    if (ultimoCalculoValido) {
      imprimirDetalhes(ultimaTemperatura,
                       ultimaTemperatura - ultimaTendencia, ultimoResultado);
    } else {
      Serial.println(F("Ainda nao ha calculo valido; use x ou t."));
    }
  } else if (texto[0] == 't' && texto[1] == ' ') {
    float atual = 0.0f;
    float anterior = 0.0f;
    if (lerDoisFloats(texto + 2, atual, anterior)) {
      simular(atual, anterior, true);
    } else {
      Serial.println(F("Uso: t 23 22"));
    }
  } else if (texto[0] == 'p' && (texto[1] == ' ' || texto[1] == '\0')) {
    alterarSetpoint(texto + 1);
  } else if (strcmp(texto, "r") == 0) {
    entrarModoSeguro(F("solicitado pelo operador"));
  } else if (strcmp(texto, "a") == 0) {
    solicitarModoFisico();
  } else if (texto[0] != '\0') {
    Serial.println(F("Comando desconhecido. Digite h."));
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(PINO_LAMPADA, OUTPUT);
  pinMode(PINO_COOLER, OUTPUT);
  pinMode(PINO_FIM_CURSO, INPUT_PULLUP);
  desligarSaidasFisicas();
  Fuzzy::limpar(ultimoResultado);
  dht.begin();
  inicioJanela = millis();
  ultimaLeitura = millis();

  Serial.println(F("ESTUFA FUZZY - Arduino Uno"));
  Serial.println(F("Inicio seguro: lampada LOW e cooler PWM 0."));
  Serial.println(F("Controle Mamdani disponivel para simulacao."));
  Serial.println(F("Digite h para ajuda ou x para os cenarios."));
}

void loop() {
  while (Serial.available() > 0) {
    const char caractere = (char)Serial.read();
    if (caractere == '\r' || caractere == '\n') {
      if (posicaoLinha > 0) {
        linha[posicaoLinha] = '\0';
        comandoSerial(linha);
        posicaoLinha = 0;
      }
    } else if (posicaoLinha < sizeof(linha) - 1) {
      linha[posicaoLinha++] = caractere;
    } else {
      posicaoLinha = 0;
      Serial.println(F("Linha muito longa; comando descartado."));
    }
  }

  const unsigned long agora = millis();
  if (agora - ultimaLeitura >= INTERVALO_SENSOR_MS) {
    ultimaLeitura = agora;
    lerSensor();
  }
  aplicarAtuadores(agora);
}
