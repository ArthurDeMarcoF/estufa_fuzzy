# Resultados obtidos por simulação

Data da execução: **08/10/2026**. Setpoint: **25,0 °C**. Estes resultados foram
produzidos pelo executável nativo que inclui o mesmo `src/fuzzy_controller.h`
compilado no firmware. Não são medições da estufa física.

## Cenários do enunciado

Comando executado:

```powershell
.\scripts\simular.ps1
```

| Temperatura atual | Anterior | Tendência | Erro | Aquecimento | Resfriamento | Análise |
|---:|---:|---:|---:|---:|---:|---|
| 15,0 °C | 16,0 °C | −1,0 °C/amostra | +10,0 °C | 87,87% | 0,00% | aquecimento forte coerente |
| 20,0 °C | 20,0 °C | 0,0 °C/amostra | +5,0 °C | 62,06% | 0,00% | aquecimento médio/forte |
| 23,0 °C | 22,0 °C | +1,0 °C/amostra | +2,0 °C | 25,00% | 0,00% | aquecimento reduzido porque já aquece |
| 24,5 °C | 24,5 °C | 0,0 °C/amostra | +0,5 °C | 0,00% | 0,00% | zona de estabilidade |
| 25,0 °C | 25,0 °C | 0,0 °C/amostra | 0,0 °C | 0,00% | 0,00% | ponto ideal estável |
| 26,0 °C | 27,0 °C | −1,0 °C/amostra | −1,0 °C | 0,00% | 25,00% | resfriamento fraco/reduzido |
| 30,0 °C | 29,0 °C | +1,0 °C/amostra | −5,0 °C | 0,00% | 86,49% | resfriamento forte |
| 35,0 °C | 34,0 °C | +1,0 °C/amostra | −10,0 °C | 0,00% | 87,87% | resfriamento forte; proteção física ainda bloqueada |

Os oito comportamentos atendem às expectativas qualitativas. A pequena
diferença entre 86,49% e 87,87% ocorre porque, em erro −5 °C, os conjuntos
“quente” e “muito quente” estão sobrepostos e recortam o conjunto forte em
graus diferentes. O centroide forte não precisa resultar em 100%; ele representa
o centro de área do conjunto agregado.

No cenário de 23 °C, o diagnóstico detalhado mostrou:

- pertinência 0,500 em “frio”;
- pertinência 1,000 em “aquecendo”;
- regra R12 (“frio E aquecendo”) ativada em 0,500;
- centroide bruto de aquecimento 25,00%;
- centroide bruto de resfriamento 4,40%, oriundo do conjunto “desligado”,
  corretamente zerado pelo limiar de 8%.

## Testes automatizados

Comando executado:

```powershell
.\scripts\testar.ps1
```

Resultado real: **5 casos de teste, 5 aprovados, 0 falhas**. Foram verificados:

1. valores das funções triangular, trapezoidal e de ombro;
2. presença e ativação das 15 combinações de regras;
3. coerência dos oito cenários exigidos;
4. grade de erro de −15 a +15 e tendência de −2 a +2, incluindo saturação,
   cobertura e ausência de comando simultâneo;
5. erro zero e rejeição de entradas `NaN`.

## Compilação para Arduino Uno

Comando executado:

```powershell
platformio run -e uno
```

Resultado: **sucesso**, sem erros. Medição do PlatformIO:

| Recurso | Uso | Total | Percentual |
|---|---:|---:|---:|
| Flash | 14.798 bytes | 32.256 bytes | 45,9% |
| SRAM | 502 bytes | 2.048 bytes | 24,5% |

## O que ainda não foi medido

Não houve acesso à estufa, portanto não foram medidos temperatura real, tempo de
subida, sobressinal, acomodação, consumo elétrico, partida mínima do cooler nem
comutação do SCR. Esses itens permanecem explicitamente para a etapa presencial.
