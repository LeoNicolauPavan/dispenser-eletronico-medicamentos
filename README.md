
# Dispenser Eletrônico de Medicamentos

**Trabalho de Conclusão de Curso — Engenharia Eletrônica**

**Autor:** Léo Nicolau Pavan  
**Instituição:** Universidade Tecnológica Federal do Paraná (UTFPR) — Campus Campo Mourão  
**Orientador:** Prof. Dr. Andre Luiz Regis Monteiro  
**Ano:** 2026

---

## 1. Sobre o projeto

Este repositório disponibiliza o código-fonte do dispenser eletrônico de medicamentos desenvolvido como Trabalho de Conclusão de Curso do Bacharelado em Engenharia Eletrônica da Universidade Tecnológica Federal do Paraná (UTFPR), Campus Campo Mourão.

O projeto consiste em um sistema embarcado baseado no microcontrolador Arduino Mega 2560, desenvolvido para auxiliar na organização e no acompanhamento dos horários de administração de medicamentos.

O equipamento integra dispositivos de entrada, interface gráfica, controle temporal, armazenamento não volátil e mecanismos de sinalização visual e sonora.

Seu funcionamento é realizado localmente, sem necessidade de conexão com a internet.

## 2. Funcionalidades implementadas

O software embarcado apresenta as seguintes funcionalidades:

- Cadastro de até 20 medicamentos distintos.
- Seleção individual dos sete dias da semana.
- Configuração de até quatro horários diários por medicamento.
- Consulta, edição e exclusão dos medicamentos cadastrados.
- Validação de horários entre 00:00 e 23:59.
- Impedimento de horários duplicados para o mesmo medicamento e dia da semana.
- Armazenamento e recuperação dos registros na memória EEPROM.
- Controle de data e hora por meio de um módulo RTC.
- Ajuste manual e sincronização do relógio.
- Interface gráfica em display TFT de 2,4 polegadas.
- Entrada de informações por teclado matricial 3×4 e teclado PS/2.
- Acionamento de alertas visuais e sonoros nos horários programados.
- Indicação de reabastecimento associada à programação semanal.
- Funcionamento com alimentação por power bank.

## 3. Componentes eletrônicos utilizados

Os principais componentes empregados no desenvolvimento do protótipo são:

- Arduino Mega 2560.
- Display TFT LCD Shield MCUFRIEND de 2,4 polegadas.
- Módulo de relógio de tempo real DS1307.
- Teclado matricial 3×4.
- Teclado com interface PS/2.
- Sete LEDs indicadores dos dias da semana.
- Buzzer para sinalização sonora.
- Sete resistores de 220 ohms.
- Power bank de 10.000 mAh.
- Cabos e conectores elétricos.

A estrutura física do equipamento foi desenvolvida no Autodesk Fusion 360 e fabricada por impressão 3D.

## 4. Desenvolvimento do software embarcado

O programa foi desenvolvido em linguagem C++, utilizando o ambiente Arduino IDE.

Sua implementação está organizada em **31 blocos numerados de 00 a 30**, acompanhados de comentários explicativos sobre as principais funções e procedimentos empregados.

A programação contempla:

- Inicialização e configuração dos periféricos.
- Estruturas de dados e organização dos medicamentos.
- Processamento dos comandos dos teclados.
- Cadastro, consulta, edição e exclusão de medicamentos.
- Validação e ordenação dos horários.
- Armazenamento e recuperação dos registros na EEPROM.
- Sincronização e ajuste do relógio de tempo real.
- Verificação dos horários programados.
- Acionamento dos alertas luminosos e sonoros.
- Desenvolvimento e atualização das interfaces gráficas.
- Gerenciamento de mensagens e estados do sistema.

Os comentários presentes no código facilitam a compreensão da arquitetura do software, da execução das funções e da interação entre os componentes eletrônicos.

## 5. Bibliotecas utilizadas

O desenvolvimento utiliza as seguintes bibliotecas:

- **MCUFRIEND_kbv:** controle do display TFT.
- **Adafruit_GFX:** implementação dos elementos gráficos da interface.
- **Keypad:** leitura do teclado matricial 3×4.
- **PS2Keyboard:** comunicação com o teclado PS/2.
- **Wire:** comunicação pelo barramento I²C.
- **RTClib:** leitura e ajuste das informações do relógio de tempo real.
- **EEPROM:** armazenamento não volátil das informações cadastradas.

Também são utilizadas bibliotecas nativas da linguagem C++ e recursos específicos da plataforma AVR.

### Observação sobre a biblioteca PS2Keyboard

A biblioteca PS2Keyboard utilizada no protótipo possui uma adaptação para o reconhecimento das teclas de função F1, F3 e F5.

Essa adaptação permite utilizar os seguintes atalhos:

- **F1:** cadastro de medicamentos.
- **F3:** ajuste de data e hora.
- **F5:** acesso ao gerenciador de medicamentos.

A adaptação não está incluída neste repositório e deve ser considerada para reproduzir integralmente o funcionamento original do protótipo.

## 6. Acesso ao código-fonte completo

A versão final documentada do software embarcado está disponível no seguinte arquivo:

**[ACESSAR O CÓDIGO-FONTE COMPLETO DO DISPENSER](VersaoDispensadorDispensadorMedicamentoscerto.ino)**

O arquivo contém a programação integral utilizada no desenvolvimento do protótipo, incluindo os 31 blocos explicativos, as funções de controle e as rotinas de interação com os dispositivos eletrônicos.

O código é disponibilizado para consulta técnica e compreensão da implementação apresentada no Trabalho de Conclusão de Curso.

## 7. Finalidade e limitações do equipamento

O dispenser eletrônico de medicamentos foi desenvolvido como recurso auxiliar para organização, gerenciamento e sinalização dos horários de administração de medicamentos.

O equipamento não realiza a liberação automática dos medicamentos, não verifica sua retirada ou ingestão e não substitui as orientações fornecidas por profissionais de saúde.

Os testes realizados concentraram-se na verificação funcional dos componentes eletrônicos e das operações implementadas, não constituindo uma avaliação clínica da contribuição do equipamento para a adesão ao tratamento medicamentoso.

## 8. Informações acadêmicas

Este repositório integra a documentação técnica do Trabalho de Conclusão de Curso desenvolvido no Bacharelado em Engenharia Eletrônica da Universidade Tecnológica Federal do Paraná, Campus Campo Mourão.

**Título:** Proposta de um Dispenser Eletrônico de Medicamentos  
**Autor:** Léo Nicolau Pavan  
**Orientador:** Prof. Dr. Andre Luiz Regis Monteiro  
**Instituição:** UTFPR — Campus Campo Mourão  
**Ano:** 2026

---

**Desenvolvido por Léo Nicolau Pavan — UTFPR, 2026.**

---

**Desenvolvido por Léo Nicolau Pavan — UTFPR, 2026.**
