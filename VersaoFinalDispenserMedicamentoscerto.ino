/*
 * ===========================================================================
 * VERSAO FINAL - DISPENSER ELETRONICO DE MEDICAMENTOS
 * ===========================================================================
 * Autor: Leo Nicolau Pavan
 * Projeto: Trabalho de Conclusao de Curso - Engenharia Eletronica
 * Instituicao: UTFPR - Campus Campo Mourao
 * Plataforma: Arduino Mega 2560
 *
 * FINALIDADE
 * Organizar a programacao semanal de medicamentos e sinalizar os horarios
 * cadastrados por meio de display, LEDs e buzzer. Os registros sao mantidos
 * na memoria EEPROM, inclusive quando a alimentacao e interrompida.
 *
 * RECURSOS IMPLEMENTADOS
 * - Cadastro de ate 20 medicamentos e ate 4 horarios por dia/medicamento;
 * - Consulta, edicao, exclusao e confirmacao das alteracoes;
 * - Controle de data e hora, ajuste manual e sincronizacao na compilacao;
 * - Alertas de administracao e indicacao semanal de reabastecimento;
 * - Entrada de dados por teclado matricial 3x4 ou teclado PS/2;
 * - Interface grafica no display TFT de 2,4 polegadas.
 *
 * LEITURA DO PROGRAMA
 * O codigo esta dividido em 31 blocos numerados (00 a 30).
 * Cada bloco apresenta sua finalidade, e as rotinas relevantes possuem
 * comentarios sobre suas entradas, validacoes e efeitos.
 *
 * DEPENDENCIA ESPECIFICA DO PROJETO
 * A biblioteca PS2Keyboard utilizada pelo prototipo foi adaptada para as
 * teclas de funcao. Os codigos esperados por este programa sao:
 * F1 = 1 (cadastro), F3 = 2 (ajuste do relogio) e F5 = 3 (gerenciador).
 * A biblioteca original sem a adaptacao pode interpretar essas teclas
 * de outra forma. Consulte a configuracao utilizada no prototipo.
 * ===========================================================================
 */

#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <Keypad.h>
#include <PS2Keyboard.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <string.h>
#include <avr/pgmspace.h>

// ---------------------------------------------------------------------------
// 00 | CONFIGURACAO DOS PERIFERICOS
// As bibliotecas fornecem drivers para a interface, os teclados, o RTC
// e a memoria EEPROM. Esta secao associa os perifericos aos pinos usados
// no prototipo; a alimentacao eletrica e definida no circuito fisico.
// ---------------------------------------------------------------------------

// Objeto que concentra as operacoes graficas do display TFT.
MCUFRIEND_kbv tft;

// [TECLADO PS/2]
// Sinais DATA e CLOCK nos pinos digitais 22 e 18 do Arduino Mega.
// A versao adaptada da PS2Keyboard identifica F1, F3 e F5.
const uint8_t PS2_DATA_PIN = 22;
const uint8_t PS2_CLOCK_PIN = 18;
PS2Keyboard tecladoPS2;

// [RELOGIO EM TEMPO REAL]
// Comunicacao I2C: SDA no pino 20 e SCL no pino 21 do Arduino Mega.
// A classe abaixo corresponde a configuracao do firmware validado.
RTC_DS3231 rtc;
bool rtcDisponivel = false;

// [TECLADO MATRICIAL 3x4]
// O mapa associa caracteres as linhas 40-43 e as colunas 44-46.
// A biblioteca Keypad interpreta os contatos das teclas pressionadas.
const byte LINHAS = 4;
const byte COLUNAS = 3;
char mapaTeclas[LINHAS][COLUNAS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte pinosLinhas[LINHAS] = {40, 41, 42, 43};
byte pinosColunas[COLUNAS] = {44, 45, 46};
Keypad tecladoMatricial = Keypad(makeKeymap(mapaTeclas), pinosLinhas,
                                 pinosColunas, LINHAS, COLUNAS);

// Atalhos da tela principal: segure por 1,5 s para evitar acionamento acidental.
const unsigned long TEMPO_SEGURAR_ATALHO = 1500UL;
char atalhoMatricialPendente = 0;

// [SAIDAS DE SINALIZACAO]
// Buzzer no pino A8; sete LEDs em A9-A15, de segunda-feira a domingo.
const uint8_t PIN_BUZZER = A8;
const uint8_t PINOS_LEDS[7] = {A9, A10, A11, A12, A13, A14, A15};

// Teclas de funcao recebidas da biblioteca PS2Keyboard adaptada.
// A correspondencia abaixo depende da mesma adaptacao em PS2Keyboard.h.
const uint8_t TECLA_PS2_F1 = 1;
const uint8_t TECLA_PS2_F3 = 2;
const uint8_t TECLA_PS2_F5 = 3;

// [PALETA GRAFICA]
// Valores RGB565 usados nas cores do fundo, textos, caixas e bordas.
const uint16_t PRETO       = 0x0000;
const uint16_t BRANCO      = 0xFFFF;
const uint16_t VERMELHO    = 0xF800;
const uint16_t VERDE       = 0x07E0;
const uint16_t AZUL        = 0x001F;
const uint16_t AZUL_ESCURO = 0x000F;
const uint16_t CIANO       = 0x07FF;
const uint16_t AMARELO     = 0xFFE0;
const uint16_t LARANJA     = 0xFD20;
const uint16_t CINZA       = 0x8410;
const uint16_t CINZA_CLARO = 0xC618;

// ---------------------------------------------------------------------------
// 01 | ESTRUTURAS DE DADOS E EEPROM
// As estruturas agrupam nome, estado ativo e ate quatro horarios para
// cada um dos sete dias. O atributo packed mantem o formato salvo.
// A versao antiga e lida apenas quando e necessario migrar cadastros.
// ---------------------------------------------------------------------------

const uint8_t MAX_MEDICAMENTOS = 20;
const uint8_t MAX_HORARIOS_DIA = 4;
const uint8_t TAM_NOME = 21;
// Identificador persistente: manter este valor para reconhecer os cadastros
// ja salvos. A mudanca exigiria tratar a migracao desses dados.
const uint32_t ASSINATURA_EEPROM = 0x4D4F434FUL; // Assinatura do formato salvo
const uint8_t VERSAO_EEPROM = 2;

struct __attribute__((packed)) HorarioDose {
  uint8_t hora;
  uint8_t minuto;
};

struct __attribute__((packed)) Medicamento {
  char nome[TAM_NOME];
  uint8_t ativo;
  uint8_t qtdHorarios[7];
  HorarioDose horarios[7][MAX_HORARIOS_DIA];
};

struct __attribute__((packed)) DadosPersistentes {
  uint32_t assinatura;
  uint8_t versao;
  uint8_t quantidade;
  uint8_t reabastecimentoPendente;
  Medicamento medicamentos[MAX_MEDICAMENTOS];
};

// Estruturas antigas, usadas somente para migrar automaticamente a V1.
struct __attribute__((packed)) MedicamentoV1 {
  char nome[TAM_NOME];
  uint8_t hora;
  uint8_t minuto;
  uint8_t dias;
  uint8_t ativo;
};

struct __attribute__((packed)) DadosPersistentesV1 {
  uint32_t assinatura;
  uint8_t versao;
  uint8_t quantidade;
  uint8_t reabastecimentoPendente;
  MedicamentoV1 medicamentos[MAX_MEDICAMENTOS];
};

DadosPersistentes dados;

// ---------------------------------------------------------------------------
// 02 | ESTADOS E VARIAVEIS DO SISTEMA
// A enumeracao identifica qual tela recebe cada comando do usuario.
// Campos temporarios representam cadastros e edicoes em andamento; somente
// uma operacao confirmada substitui os dados persistentes.
// ---------------------------------------------------------------------------

enum TelaSistema {
  TELA_PRINCIPAL,
  TELA_CADASTRO_NOME,
  TELA_CADASTRO_DIA,
  TELA_CADASTRO_HORARIO,
  TELA_CADASTRO_ACAO,
  TELA_AJUSTE_RTC,
  TELA_GERENCIADOR,
  TELA_GERENCIADOR_ITEM,
  TELA_EDITAR_MENU,
  TELA_EDITAR_DIAS,
  TELA_EDITAR_DIA_HORARIOS,
  TELA_EDITAR_HORARIO_DESTINO,
  TELA_EDITAR_HORARIO_VALOR,
  TELA_CONFIRMAR_EXCLUSAO,
  TELA_CONFIRMAR_LIMPAR_TUDO
};

TelaSistema telaAtual = TELA_PRINCIPAL;

Medicamento medicamentoEmEdicao;
char nomeEmEdicao[TAM_NOME] = "";
uint8_t tamanhoNome = 0;
char horarioEmEdicao[5] = "";
uint8_t tamanhoHorario = 0;
uint8_t diaEmEdicao = 0;
uint8_t ultimaHoraAdicionada = 0;
uint8_t ultimoMinutoAdicionado = 0;

char dataHoraEmEdicao[13] = "";
uint8_t tamanhoDataHora = 0;

// Gerenciador
const uint8_t ITENS_POR_PAGINA = 3;
uint8_t paginaGerenciador = 0;
int8_t indiceExclusaoPendente = -1;
int8_t indiceMedicamentoSelecionado = -1;

// Edicao segura: trabalha em uma copia e so grava ao confirmar SALVAR.
bool modoEdicaoMedicamento = false;
int8_t indiceMedicamentoEdicao = -1;
uint8_t diaHorarioOriginal = 0;
int8_t indiceHorarioOriginal = -1;   // -1 = novo horario
uint8_t diaHorarioDestino = 0;

// Digitacao multiclique: a mesma tecla alterna letras em ate 3 s.
// Exemplo: tecla 4 -> G -> H -> I.
const unsigned long TEMPO_MULTICLIQUE = 3000UL;
char ultimaTeclaMulti = 0;
uint8_t indiceMulti = 0;
unsigned long instanteUltimaTeclaMulti = 0;

// Mensagens temporarias
char mensagemStatus[48] = "";
unsigned long fimMensagemStatus = 0;

// Atualizacao do display
bool redesenharTela = true;
unsigned long ultimaAtualizacaoTela = 0;
uint8_t ultimoDiaPrincipal = 255;
uint8_t ultimoMesPrincipal = 255;
uint16_t ultimoAnoPrincipal = 0;

// ---------------------------------------------------------------------------
// 03 | CONTROLE DE ALARMES
// Guarda o estado de cada alarme, a dose acionada e seu instante inicial.
// Os avisos de administracao duram ate 60 s; o reabastecimento aguarda
// confirmacao e utiliza um padrao de sinalizacao independente.
// ---------------------------------------------------------------------------

bool alarmeMedicamentoAtivo = false;
unsigned long inicioAlarmeMedicamento = 0;
const unsigned long DURACAO_ALARME_MEDICAMENTO = 60000UL;
uint8_t diaAlarme = 0;
uint8_t horaAlarme = 0;
uint8_t minutoAlarme = 0;
bool alarmeEhUltimaDoseDaSemana = false;
uint32_t ultimaAssinaturaMinuto = 0xFFFFFFFFUL;
bool buzzerLigado = false;

// ---------------------------------------------------------------------------
// 04 | NOMES E ORDEM DOS DIAS
// As tabelas usam a ordem segunda-feira a domingo (indices 0 a 6).
// O indice recebido do RTC e convertido para essa mesma ordem.
// ---------------------------------------------------------------------------

const char* NOMES_DIAS[7] = {
  "SEGUNDA-FEIRA", "TERCA-FEIRA", "QUARTA-FEIRA", "QUINTA-FEIRA",
  "SEXTA-FEIRA", "SABADO", "DOMINGO"
};

const char* NOMES_DIAS_CURTOS[7] = {
  "SEG", "TER", "QUA", "QUI", "SEX", "SAB", "DOM"
};

// ---------------------------------------------------------------------------
// 05 | DECLARACOES DAS FUNCOES
// Os prototipos permitem que cada modulo chame funcoes definidas mais
// adiante, sem alterar a organizacao original da programacao.
// ---------------------------------------------------------------------------

void carregarDados();
void salvarDados();
void inicializarDadosVazios();
void migrarDadosV1();
void ajustarRtcPeloUpload();
uint32_t calcularAssinaturaCompilacao();

void iniciarCadastro();
void iniciarAjusteRtc();
void iniciarGerenciador();
void voltarAoGerenciador();
void cancelarEdicao();

void selecionarMedicamentoGerenciador(uint8_t indice);
void iniciarEdicaoMedicamento(uint8_t indice);
void abrirEdicaoNome();
void abrirEdicaoHorarios();
void cancelarEdicaoMedicamento();
void salvarEdicaoMedicamento();
void removerHorarioMedicamento(Medicamento& med, uint8_t dia, uint8_t indice);
void iniciarNovoHorarioEdicao();
void selecionarHorarioEdicao(uint8_t indice);
void escolherDiaDestinoEdicao(uint8_t dia);
void confirmarHorarioEdicao();

void processarTecladoPS2();
void processarTecladoMatricial();
void tratarTeclaPS2(uint8_t tecla);
void tratarTeclaMatricial(char tecla);

void tratarNomePS2(uint8_t tecla);
void tratarNomeMatricial(char tecla);
void tratarDiaPS2(uint8_t tecla);
void tratarDiaMatricial(char tecla);
void tratarHorarioPS2(uint8_t tecla);
void tratarHorarioMatricial(char tecla);
void tratarAcaoCadastroPS2(uint8_t tecla);
void tratarAcaoCadastroMatricial(char tecla);
void tratarRtcPS2(uint8_t tecla);
void tratarRtcMatricial(char tecla);
void tratarGerenciadorPS2(uint8_t tecla);
void tratarGerenciadorMatricial(char tecla);
void tratarGerenciadorItemPS2(uint8_t tecla);
void tratarGerenciadorItemMatricial(char tecla);
void tratarEditarMenuPS2(uint8_t tecla);
void tratarEditarMenuMatricial(char tecla);
void tratarEditarDiasPS2(uint8_t tecla);
void tratarEditarDiasMatricial(char tecla);
void tratarEditarDiaHorariosPS2(uint8_t tecla);
void tratarEditarDiaHorariosMatricial(char tecla);
void tratarEditarHorarioDestinoPS2(uint8_t tecla);
void tratarEditarHorarioDestinoMatricial(char tecla);
void tratarEditarHorarioValorPS2(uint8_t tecla);
void tratarEditarHorarioValorMatricial(char tecla);
void tratarConfirmacaoExclusaoPS2(uint8_t tecla);
void tratarConfirmacaoExclusaoMatricial(char tecla);
void tratarConfirmacaoLimparTudoPS2(uint8_t tecla);
void tratarConfirmacaoLimparTudoMatricial(char tecla);

void adicionarCaractereNome(char caractere);
void apagarCaractereNome();
void processarMultiClique(char tecla);
void finalizarMultiClique();
const char* grupoLetras(char tecla);

void adicionarDigitoHorario(char digito);
void apagarDigitoHorario();
void adicionarDigitoDataHora(char digito);
void apagarDigitoDataHora();

void confirmarNome();
void selecionarDiaCadastro(uint8_t dia);
void confirmarHorario();
void concluirCadastro();
bool adicionarHorarioMedicamento(Medicamento& med, uint8_t dia,
                                  uint8_t hora, uint8_t minuto);
bool existeHorarioNoMedicamento(const Medicamento& med, uint8_t dia,
                                uint8_t hora, uint8_t minuto);
void ordenarHorariosDoDia(Medicamento& med, uint8_t dia);
uint8_t contarHorariosSemana(const Medicamento& med);
bool medicamentoTemHorarios(const Medicamento& med);

void confirmarAjusteRtc();
void solicitarExclusao(uint8_t indice);
void excluirMedicamento(uint8_t indice);
void solicitarLimparTodos();
void excluirTodosMedicamentos();
uint8_t totalPaginasGerenciador();

bool dataHoraValida(uint16_t ano, uint8_t mes, uint8_t dia,
                    uint8_t hora, uint8_t minuto);
uint8_t diasNoMes(uint16_t ano, uint8_t mes);

void verificarAlarmes(const DateTime& agora);
bool existeMedicamentoAgora(uint8_t dia, uint8_t hora, uint8_t minuto);
bool medicamentoPossuiHorario(const Medicamento& med, uint8_t dia,
                              uint8_t hora, uint8_t minuto);
bool ehUltimoHorarioDaSemana(uint8_t dia, uint8_t hora, uint8_t minuto);
void iniciarAlarmeMedicamento(uint8_t dia, uint8_t hora, uint8_t minuto);
void encerrarAlarmeMedicamento();
void confirmarReabastecimento();

void atualizarBuzzer();
void atualizarLeds(uint8_t diaAtual);
uint8_t indiceDiaSegundaPrimeiro(const DateTime& dataHora);

void atualizarInterface(const DateTime& agora);
void desenharTelaPrincipal(const DateTime& agora);
void atualizarRelogioTelaPrincipal(const DateTime& agora);
void desenharTelaCadastroNome();
void desenharTelaCadastroDia();
void desenharTelaCadastroHorario();
void desenharTelaCadastroAcao();
void desenharTelaAjusteRtc();
void desenharCampoDataHora();
void desenharTelaGerenciador();
void desenharTelaGerenciadorItem();
void desenharTelaEditarMenu();
void desenharTelaEditarDias();
void desenharTelaEditarDiaHorarios();
void desenharTelaEditarHorarioDestino();
void desenharTelaEditarHorarioValor();
void desenharConfirmacaoExclusao();
void desenharConfirmacaoLimparTudo();
void desenharAlarmeMedicamento();
void desenharAlertaReabastecimento();
void desenharCabecalho(const char* titulo, uint16_t cor);
void desenharCabecalho(const __FlashStringHelper* titulo, uint16_t cor);
void imprimirCentralizado(const char* texto, int16_t y, uint8_t tamanho,
                          uint16_t cor);
void imprimirCentralizado(const __FlashStringHelper* texto, int16_t y,
                          uint8_t tamanho, uint16_t cor);
void desenharCaixaPadrao(int16_t x, int16_t y, int16_t largura, int16_t altura,
                         uint16_t fundo, uint16_t borda);
void desenharRodapePadrao(const __FlashStringHelper* linha1,
                          const __FlashStringHelper* linha2);
void desenharRodapeOuMensagem(const __FlashStringHelper* linha1,
                              const __FlashStringHelper* linha2);
void mostrarMensagem(const char* texto, unsigned long duracaoMs = 3000UL);
void mostrarMensagem(const __FlashStringHelper* texto,
                     unsigned long duracaoMs = 3000UL);
void limparMensagemExpirada();
void montarDiasAtivos(const Medicamento& med, char* destino, size_t tamanho);

// ---------------------------------------------------------------------------
// 06 | INICIALIZACAO DO EQUIPAMENTO
// Configura LEDs e buzzer, inicia os teclados, o barramento I2C e o RTC.
// Depois recupera dados da EEPROM e prepara o display TFT.
// ---------------------------------------------------------------------------

// Prepara os recursos fisicos antes de iniciar o sistema.
// Inicializa perifericos, recupera registros e desenha a tela inicial.
void setup() {
  Serial.begin(9600);

  for (uint8_t i = 0; i < 7; i++) {
    pinMode(PINOS_LEDS[i], OUTPUT);
    digitalWrite(PINOS_LEDS[i], LOW);
  }

  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BUZZER, INPUT);
  buzzerLigado = false;

  tecladoPS2.begin(PS2_DATA_PIN, PS2_CLOCK_PIN);
  tecladoMatricial.setHoldTime(TEMPO_SEGURAR_ATALHO);

  Wire.begin();
  rtcDisponivel = rtc.begin();

  if (rtcDisponivel) {
    ajustarRtcPeloUpload();
  } else {
    Serial.println(F("ERRO: RTC I2C nao encontrado no endereco 0x68."));
    Serial.println(F("Confira SDA=20, SCL=21, VCC=5V e GND."));
  }

  carregarDados();

  tft.begin(0x1289);
  tft.setRotation(0);
  tft.setTextWrap(false);
  tft.fillScreen(BRANCO);

  Serial.print(F("Resolucao logica do TFT: "));
  Serial.print(tft.width());
  Serial.print(F(" x "));
  Serial.println(tft.height());

  Serial.print(F("EEPROM usada pelos dados: "));
  Serial.print(sizeof(DadosPersistentes));
  Serial.println(F(" bytes"));

  desenharCabecalho(F("INICIALIZANDO"), AZUL_ESCURO);
  imprimirCentralizado(F("Dispenser de medicamentos"), 145, 1, PRETO);
  delay(1200);

  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 07 | CICLO PRINCIPAL DE EXECUCAO
// A cada passagem, le comandos, verifica horarios, atualiza sinalizadores
// e redesenha a interface quando necessario, sem bloquear o ciclo.
// ---------------------------------------------------------------------------

// Executa continuamente a logica de operacao do dispenser.
// Consulta RTC e teclados, trata alarmes e atualiza os elementos visuais.
void loop() {
  DateTime agora = rtcDisponivel ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);

  processarTecladoPS2();
  processarTecladoMatricial();

  if (ultimaTeclaMulti != 0 &&
      millis() - instanteUltimaTeclaMulti > TEMPO_MULTICLIQUE) {
    finalizarMultiClique();
  }

  limparMensagemExpirada();

  if (rtcDisponivel && agora.isValid()) {
    verificarAlarmes(agora);
    atualizarLeds(indiceDiaSegundaPrimeiro(agora));
  } else {
    for (uint8_t i = 0; i < 7; i++) {
      digitalWrite(PINOS_LEDS[i], LOW);
    }
  }

  if (alarmeMedicamentoAtivo &&
      millis() - inicioAlarmeMedicamento >= DURACAO_ALARME_MEDICAMENTO) {
    encerrarAlarmeMedicamento();
  }

  atualizarBuzzer();
  atualizarInterface(agora);
}

// ---------------------------------------------------------------------------
// 08 | PERSISTENCIA DOS CADASTROS
// Recupera os registros persistentes, valida seus campos e salva edicoes.
// Inclui compatibilidade com registros gravados no formato V1.
// A identificacao da compilacao ocupa os ultimos bytes da EEPROM.
// ---------------------------------------------------------------------------

// Cria o banco persistente com campos zerados.
// Grava assinatura, versao e quantidade inicial na EEPROM.
void inicializarDadosVazios() {
  memset(&dados, 0, sizeof(dados));
  dados.assinatura = ASSINATURA_EEPROM;
  dados.versao = VERSAO_EEPROM;
  dados.quantidade = 0;
  dados.reabastecimentoPendente = 0;
  salvarDados();
}

// Converte cadastros gravados no formato antigo V1.
// Le um registro antigo por vez e distribui seus horarios na estrutura atual.
void migrarDadosV1() {
  // A leitura de um registro por vez reduz o uso temporario de memoria.
  uint8_t quantidadeAntiga = 0;
  uint8_t reabastecimentoAntigo = 0;

  const int enderecoQuantidade = sizeof(uint32_t) + sizeof(uint8_t);
  const int enderecoReabastecimento = enderecoQuantidade + sizeof(uint8_t);
  const int enderecoMedicamentos = enderecoReabastecimento + sizeof(uint8_t);

  EEPROM.get(enderecoQuantidade, quantidadeAntiga);
  EEPROM.get(enderecoReabastecimento, reabastecimentoAntigo);

  memset(&dados, 0, sizeof(dados));
  dados.assinatura = ASSINATURA_EEPROM;
  dados.versao = VERSAO_EEPROM;
  dados.quantidade = quantidadeAntiga <= MAX_MEDICAMENTOS
                     ? quantidadeAntiga : MAX_MEDICAMENTOS;
  dados.reabastecimentoPendente = reabastecimentoAntigo;

  for (uint8_t i = 0; i < dados.quantidade; i++) {
    MedicamentoV1 velho;
    EEPROM.get(enderecoMedicamentos + i * sizeof(MedicamentoV1), velho);

    Medicamento& novo = dados.medicamentos[i];
    memset(&novo, 0, sizeof(novo));
    strncpy(novo.nome, velho.nome, TAM_NOME - 1);
    novo.nome[TAM_NOME - 1] = '\0';
    novo.ativo = velho.ativo;

    if (velho.ativo && velho.hora <= 23 && velho.minuto <= 59) {
      for (uint8_t d = 0; d < 7; d++) {
        if (velho.dias & (1 << d)) {
          adicionarHorarioMedicamento(novo, d, velho.hora, velho.minuto);
        }
      }
    }
  }

  salvarDados();
  Serial.println(F("EEPROM V1 migrada automaticamente para V2."));
}

// Recupera os cadastros e o estado de reabastecimento da EEPROM.
// Confere assinatura e versao, migra V1 ou reinicia dados invalidos.
void carregarDados() {
  uint32_t assinatura = 0;
  uint8_t versao = 0;

  EEPROM.get(0, assinatura);
  EEPROM.get(sizeof(uint32_t), versao);

  if (assinatura == ASSINATURA_EEPROM && versao == VERSAO_EEPROM) {
    EEPROM.get(0, dados);

    if (dados.quantidade > MAX_MEDICAMENTOS) {
      inicializarDadosVazios();
      return;
    }

    for (uint8_t i = 0; i < dados.quantidade; i++) {
      dados.medicamentos[i].nome[TAM_NOME - 1] = '\0';
      for (uint8_t d = 0; d < 7; d++) {
        if (dados.medicamentos[i].qtdHorarios[d] > MAX_HORARIOS_DIA) {
          dados.medicamentos[i].qtdHorarios[d] = 0;
        }
      }
    }
    return;
  }

  if (assinatura == ASSINATURA_EEPROM && versao == 1) {
    migrarDadosV1();
    return;
  }

  inicializarDadosVazios();
}

// Persiste o banco completo na EEPROM interna.
// Usa EEPROM.put para atualizar os bytes da estrutura de dados.
void salvarDados() {
  EEPROM.put(0, dados);
}

// Identifica a compilacao incorporada ao firmware.
// Aplica hash FNV-1a a __DATE__ e __TIME__.
uint32_t calcularAssinaturaCompilacao() {
  const char textoCompilacao[] = __DATE__ " " __TIME__;
  uint32_t hash = 2166136261UL;  // FNV-1a 32 bits

  for (uint8_t i = 0; textoCompilacao[i] != '\0'; i++) {
    hash ^= (uint8_t)textoCompilacao[i];
    hash *= 16777619UL;
  }

  return hash;
}

// Atualiza o RTC quando o firmware muda ou perde a referencia.
// Compara a assinatura na EEPROM e preserva o horario em reinicios comuns.
void ajustarRtcPeloUpload() {
  if (!rtcDisponivel) return;

  // A assinatura fica nos quatro bytes finais, separados dos cadastros.
  const int enderecoAssinatura = EEPROM.length() - sizeof(uint32_t);

  uint32_t assinaturaAtual = calcularAssinaturaCompilacao();
  uint32_t assinaturaSalva = 0;
  EEPROM.get(enderecoAssinatura, assinaturaSalva);

  DateTime dataCompilacao(F(__DATE__), F(__TIME__));
  bool novaCompilacao = (assinaturaSalva != assinaturaAtual);
  bool perdeuAlimentacaoRtc = rtc.lostPower();

  // Ajusta na nova compilacao ou perda de referencia; caso contrario,
  // preserva a contagem mantida pela bateria do modulo RTC.
  if (novaCompilacao || perdeuAlimentacaoRtc) {
    rtc.adjust(dataCompilacao);
    EEPROM.put(enderecoAssinatura, assinaturaAtual);
    delay(20);

    if (novaCompilacao) {
      Serial.print(F("RTC sincronizado pela NOVA COMPILACAO: "));
    } else {
      Serial.print(F("RTC sincronizado porque perdeu alimentacao: "));
    }

    Serial.print(dataCompilacao.day());
    Serial.print('/');
    Serial.print(dataCompilacao.month());
    Serial.print('/');
    Serial.print(dataCompilacao.year());
    Serial.print(' ');
    Serial.print(dataCompilacao.hour());
    Serial.print(':');
    Serial.print(dataCompilacao.minute());
    Serial.print(':');
    Serial.println(dataCompilacao.second());
  } else {
    Serial.println(F("Sem nova compilacao: RTC preservado pela bateria."));
  }

  DateTime leituraRtc = rtc.now();
  Serial.print(F("Leitura atual do RTC: "));

  if (!leituraRtc.isValid()) {
    Serial.println(F("INVALIDA - confira SDA=20, SCL=21, bateria e alimentacao"));
  } else {
    Serial.print(leituraRtc.day());
    Serial.print('/');
    Serial.print(leituraRtc.month());
    Serial.print('/');
    Serial.print(leituraRtc.year());
    Serial.print(' ');
    Serial.print(leituraRtc.hour());
    Serial.print(':');
    Serial.print(leituraRtc.minute());
    Serial.print(':');
    Serial.println(leituraRtc.second());
  }
}

// ---------------------------------------------------------------------------
// 09 | ABERTURA E CANCELAMENTO DE TELAS
// Reinicia campos temporarios e seleciona a tela correspondente.
// Cancelar uma edicao nao grava alteracoes no banco persistente.
// ---------------------------------------------------------------------------

// Abre um cadastro com campos temporarios vazios.
// Antes verifica o limite de medicamentos e define a primeira tela.
void iniciarCadastro() {
  if (dados.quantidade >= MAX_MEDICAMENTOS) {
    mostrarMensagem(F("Memoria cheia: 20 medicamentos"));
    return;
  }

  memset(&medicamentoEmEdicao, 0, sizeof(medicamentoEmEdicao));
  memset(nomeEmEdicao, 0, sizeof(nomeEmEdicao));
  memset(horarioEmEdicao, 0, sizeof(horarioEmEdicao));

  tamanhoNome = 0;
  tamanhoHorario = 0;
  diaEmEdicao = 0;
  ultimaHoraAdicionada = 0;
  ultimoMinutoAdicionado = 0;

  finalizarMultiClique();
  telaAtual = TELA_CADASTRO_NOME;
  redesenharTela = true;
}

// Abre o formulario de correcao do relogio.
// Limpa os digitos anteriores e seleciona a tela de ajuste.
void iniciarAjusteRtc() {
  memset(dataHoraEmEdicao, 0, sizeof(dataHoraEmEdicao));
  tamanhoDataHora = 0;
  finalizarMultiClique();
  telaAtual = TELA_AJUSTE_RTC;
  redesenharTela = true;
}

// Prepara a lista de medicamentos para consulta.
// Reinicia pagina e selecoes antigas antes de abrir o menu.
void iniciarGerenciador() {
  paginaGerenciador = 0;
  indiceExclusaoPendente = -1;
  indiceMedicamentoSelecionado = -1;
  modoEdicaoMedicamento = false;
  indiceMedicamentoEdicao = -1;
  finalizarMultiClique();
  telaAtual = TELA_GERENCIADOR;
  redesenharTela = true;
}

// Retorna a listagem depois de uma acao ou cancelamento.
// Descarta indices temporarios e solicita o redesenho.
void voltarAoGerenciador() {
  indiceExclusaoPendente = -1;
  indiceMedicamentoSelecionado = -1;
  modoEdicaoMedicamento = false;
  indiceMedicamentoEdicao = -1;
  finalizarMultiClique();
  telaAtual = TELA_GERENCIADOR;
  redesenharTela = true;
}

// Cancela o formulario de cadastro ou ajuste corrente.
// Retorna a tela principal sem gravar o conteudo temporario.
void cancelarEdicao() {
  telaAtual = TELA_PRINCIPAL;
  finalizarMultiClique();
  mostrarMensagem(F("Operacao cancelada"));
  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 10 | LEITURA E DISTRIBUICAO DOS COMANDOS
// Converte sinais dos teclados PS/2 e matricial em comandos da tela ativa.
// Na tela principal, os atalhos matriciais exigem pressionamento longo.
// ---------------------------------------------------------------------------

// Consome os caracteres e teclas especiais recebidos pelo PS/2.
// Cada codigo lido e encaminhado para a tela ativa.
void processarTecladoPS2() {
  while (tecladoPS2.available()) {
    int valor = tecladoPS2.read();
    if (valor >= 0) {
      tratarTeclaPS2((uint8_t)valor);
    }
  }
}

// Consulta o estado do teclado 3x4.
// Distingue atalhos longos na tela principal de comandos normais nos menus.
void processarTecladoMatricial() {
  char tecla = tecladoMatricial.getKey();

  // Apenas os atalhos da tela principal requerem 1,5 s de pressionamento.
  if (telaAtual == TELA_PRINCIPAL &&
      !dados.reabastecimentoPendente &&
      !alarmeMedicamentoAtivo) {

    if (atalhoMatricialPendente != 0) {
      KeyState estado = tecladoMatricial.getState();

      if (estado == HOLD) {
        char executar = atalhoMatricialPendente;
        atalhoMatricialPendente = 0;
        tratarTeclaMatricial(executar);
        return;
      }

      if (estado == RELEASED || estado == IDLE) {
        atalhoMatricialPendente = 0;
      }

      return;
    }

    if (tecla == '*' || tecla == '0' || tecla == '#') {
      atalhoMatricialPendente = tecla;
      return;
    }
  }

  if (tecla != NO_KEY) {
    tratarTeclaMatricial(tecla);
  }
}

// Distribui uma tecla PS/2 conforme o estado atual.
// Prioriza confirmacoes de alerta, atalhos e controles de cada tela.
void tratarTeclaPS2(uint8_t tecla) {
  if (dados.reabastecimentoPendente) {
    if (tecla == PS2_ENTER) confirmarReabastecimento();
    return;
  }

  if (alarmeMedicamentoAtivo) {
    if (tecla == PS2_ENTER) encerrarAlarmeMedicamento();
    return;
  }

  if (tecla == TECLA_PS2_F5) {
    if (telaAtual == TELA_PRINCIPAL) {
      iniciarGerenciador();
    } else if (telaAtual == TELA_GERENCIADOR) {
      solicitarLimparTodos();
    }
    return;
  }

  if (tecla == PS2_ESC) {
    if (telaAtual == TELA_CONFIRMAR_EXCLUSAO ||
        telaAtual == TELA_CONFIRMAR_LIMPAR_TUDO) {
      voltarAoGerenciador();
    } else if (modoEdicaoMedicamento) {
      cancelarEdicaoMedicamento();
    } else if (telaAtual == TELA_GERENCIADOR_ITEM) {
      voltarAoGerenciador();
    } else if (telaAtual == TELA_GERENCIADOR) {
      telaAtual = TELA_PRINCIPAL;
      redesenharTela = true;
    } else if (telaAtual != TELA_PRINCIPAL) {
      cancelarEdicao();
    }
    return;
  }

  switch (telaAtual) {
    case TELA_PRINCIPAL:
      if (tecla == TECLA_PS2_F1) iniciarCadastro();
      else if (tecla == TECLA_PS2_F3) iniciarAjusteRtc();
      break;

    case TELA_CADASTRO_NOME:
      tratarNomePS2(tecla);
      break;

    case TELA_CADASTRO_DIA:
      tratarDiaPS2(tecla);
      break;

    case TELA_CADASTRO_HORARIO:
      tratarHorarioPS2(tecla);
      break;

    case TELA_CADASTRO_ACAO:
      tratarAcaoCadastroPS2(tecla);
      break;

    case TELA_AJUSTE_RTC:
      tratarRtcPS2(tecla);
      break;

    case TELA_GERENCIADOR:
      tratarGerenciadorPS2(tecla);
      break;

    case TELA_GERENCIADOR_ITEM:
      tratarGerenciadorItemPS2(tecla);
      break;

    case TELA_EDITAR_MENU:
      tratarEditarMenuPS2(tecla);
      break;

    case TELA_EDITAR_DIAS:
      tratarEditarDiasPS2(tecla);
      break;

    case TELA_EDITAR_DIA_HORARIOS:
      tratarEditarDiaHorariosPS2(tecla);
      break;

    case TELA_EDITAR_HORARIO_DESTINO:
      tratarEditarHorarioDestinoPS2(tecla);
      break;

    case TELA_EDITAR_HORARIO_VALOR:
      tratarEditarHorarioValorPS2(tecla);
      break;

    case TELA_CONFIRMAR_EXCLUSAO:
      tratarConfirmacaoExclusaoPS2(tecla);
      break;

    case TELA_CONFIRMAR_LIMPAR_TUDO:
      tratarConfirmacaoLimparTudoPS2(tecla);
      break;
  }
}

// Distribui teclas do teclado 3x4 para as rotinas ativas.
// Bloqueia a navegacao normal enquanto ha alerta prioritario.
void tratarTeclaMatricial(char tecla) {
  if (dados.reabastecimentoPendente) {
    if (tecla == '*') confirmarReabastecimento();
    return;
  }

  if (alarmeMedicamentoAtivo) {
    if (tecla == '*') encerrarAlarmeMedicamento();
    return;
  }

  switch (telaAtual) {
    case TELA_PRINCIPAL:
      // Atalhos do teclado matricial na tela principal.
      if (tecla == '*') iniciarCadastro();
      else if (tecla == '0') iniciarAjusteRtc();
      else if (tecla == '#') iniciarGerenciador();
      break;

    case TELA_CADASTRO_NOME:
      tratarNomeMatricial(tecla);
      break;

    case TELA_CADASTRO_DIA:
      tratarDiaMatricial(tecla);
      break;

    case TELA_CADASTRO_HORARIO:
      tratarHorarioMatricial(tecla);
      break;

    case TELA_CADASTRO_ACAO:
      tratarAcaoCadastroMatricial(tecla);
      break;

    case TELA_AJUSTE_RTC:
      tratarRtcMatricial(tecla);
      break;

    case TELA_GERENCIADOR:
      tratarGerenciadorMatricial(tecla);
      break;

    case TELA_GERENCIADOR_ITEM:
      tratarGerenciadorItemMatricial(tecla);
      break;

    case TELA_EDITAR_MENU:
      tratarEditarMenuMatricial(tecla);
      break;

    case TELA_EDITAR_DIAS:
      tratarEditarDiasMatricial(tecla);
      break;

    case TELA_EDITAR_DIA_HORARIOS:
      tratarEditarDiaHorariosMatricial(tecla);
      break;

    case TELA_EDITAR_HORARIO_DESTINO:
      tratarEditarHorarioDestinoMatricial(tecla);
      break;

    case TELA_EDITAR_HORARIO_VALOR:
      tratarEditarHorarioValorMatricial(tecla);
      break;

    case TELA_CONFIRMAR_EXCLUSAO:
      tratarConfirmacaoExclusaoMatricial(tecla);
      break;

    case TELA_CONFIRMAR_LIMPAR_TUDO:
      tratarConfirmacaoLimparTudoMatricial(tecla);
      break;
  }
}

// ---------------------------------------------------------------------------
// 11 | ENTRADA E VALIDACAO DO NOME
// No PS/2, os caracteres chegam diretamente; no matricial, cada tecla
// numerica alterna letras por multiclique no intervalo de ate 3 s.
// O nome e validado antes de passar a selecao dos dias.
// ---------------------------------------------------------------------------

// Recebe nome diretamente do teclado PS/2.
// ENTER confirma, BACKSPACE remove e caracteres validos sao anexados.
void tratarNomePS2(uint8_t tecla) {
  finalizarMultiClique();

  if (tecla == PS2_ENTER) {
    confirmarNome();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    apagarCaractereNome();
  } else if (tecla >= 32 && tecla <= 126) {
    adicionarCaractereNome((char)tecla);
  }
}

// Recebe nome pelo teclado de doze teclas.
// Usa multiclique para letras, 0 para espaco e * para confirmar.
void tratarNomeMatricial(char tecla) {
  if (modoEdicaoMedicamento && tecla == '1') {
    finalizarMultiClique();
    tamanhoNome = 0;
    nomeEmEdicao[0] = '\0';
    redesenharTela = true;
  } else if (tecla == '*') {
    finalizarMultiClique();
    confirmarNome();
  } else if (tecla == '#') {
    apagarCaractereNome();
  } else if (tecla == '0') {
    finalizarMultiClique();
    adicionarCaractereNome(' ');
  } else if (tecla >= '2' && tecla <= '9') {
    processarMultiClique(tecla);
  }
}

// Inclui um caractere no texto em edicao.
// Respeita o tamanho do vetor e mantem o terminador nulo.
void adicionarCaractereNome(char caractere) {
  if (tamanhoNome < TAM_NOME - 1) {
    nomeEmEdicao[tamanhoNome++] = caractere;
    nomeEmEdicao[tamanhoNome] = '\0';
    redesenharTela = true;
  }
}

// Exclui o ultimo caractere digitado no nome.
// Encerra o ciclo multiclique antes de modificar o vetor.
void apagarCaractereNome() {
  finalizarMultiClique();
  if (tamanhoNome > 0) {
    tamanhoNome--;
    nomeEmEdicao[tamanhoNome] = '\0';
    redesenharTela = true;
  }
}

// Associa cada tecla numerica a seu grupo de letras.
// Retorna a sequencia de caracteres usada pelo multiclique.
const char* grupoLetras(char tecla) {
  switch (tecla) {
    case '2': return "ABC";
    case '3': return "DEF";
    case '4': return "GHI";
    case '5': return "JKL";
    case '6': return "MNO";
    case '7': return "PQRS";
    case '8': return "TUV";
    case '9': return "WXYZ";
    default:  return "";
  }
}

// Alterna letras ao repetir a mesma tecla em ate 3 segundos.
// Substitui a ultima letra ou inicia uma nova posicao no nome.
void processarMultiClique(char tecla) {
  const char* letras = grupoLetras(tecla);
  uint8_t quantidadeLetras = strlen(letras);
  if (quantidadeLetras == 0) return;

  unsigned long agoraMs = millis();

  if (ultimaTeclaMulti == tecla && tamanhoNome > 0 &&
      agoraMs - instanteUltimaTeclaMulti <= TEMPO_MULTICLIQUE) {
    indiceMulti = (indiceMulti + 1) % quantidadeLetras;
    nomeEmEdicao[tamanhoNome - 1] = letras[indiceMulti];
  } else {
    finalizarMultiClique();
    if (tamanhoNome >= TAM_NOME - 1) return;

    ultimaTeclaMulti = tecla;
    indiceMulti = 0;
    nomeEmEdicao[tamanhoNome++] = letras[0];
    nomeEmEdicao[tamanhoNome] = '\0';
  }

  ultimaTeclaMulti = tecla;
  instanteUltimaTeclaMulti = agoraMs;
  redesenharTela = true;
}

// Encerra a alternancia de letras.
// Zera tecla, indice e instante utilizados pelo multiclique.
void finalizarMultiClique() {
  ultimaTeclaMulti = 0;
  indiceMulti = 0;
  instanteUltimaTeclaMulti = 0;
}

// Valida e guarda o nome informado no cadastro/edicao.
// Remove espacos finais e avanca somente quando o campo nao esta vazio.
void confirmarNome() {
  finalizarMultiClique();

  while (tamanhoNome > 0 && nomeEmEdicao[tamanhoNome - 1] == ' ') {
    nomeEmEdicao[--tamanhoNome] = '\0';
  }

  if (tamanhoNome == 0) {
    mostrarMensagem(F("Digite o nome do medicamento"));
    return;
  }

  strncpy(medicamentoEmEdicao.nome, nomeEmEdicao, TAM_NOME - 1);
  medicamentoEmEdicao.nome[TAM_NOME - 1] = '\0';
  medicamentoEmEdicao.ativo = 1;

  if (modoEdicaoMedicamento) {
    telaAtual = TELA_EDITAR_MENU;
    mostrarMensagem(F("Nome alterado na copia. Salve ao final."));
  } else {
    telaAtual = TELA_CADASTRO_DIA;
  }
  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 12 | ESCOLHA DO DIA NO CADASTRO
// Escolhe o dia da semana (1 a 7) que recebera uma ou mais doses.
// A finalizacao exige pelo menos um horario no cadastro.
// ---------------------------------------------------------------------------

// Seleciona o dia usando o teclado PS/2.
// Permite concluir o cadastro quando ja existem horarios.
void tratarDiaPS2(uint8_t tecla) {
  if (tecla >= '1' && tecla <= '7') {
    selecionarDiaCadastro(tecla - '1');
  } else if ((tecla == '0' || tecla == PS2_ENTER) &&
             medicamentoTemHorarios(medicamentoEmEdicao)) {
    concluirCadastro();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    cancelarEdicao();
  }
}

// Seleciona o dia pelo teclado matricial.
// Distingue numeros de dias dos comandos de concluir/cancelar.
void tratarDiaMatricial(char tecla) {
  if (tecla >= '1' && tecla <= '7') {
    selecionarDiaCadastro(tecla - '1');
  } else if ((tecla == '0' || tecla == '*') &&
             medicamentoTemHorarios(medicamentoEmEdicao)) {
    concluirCadastro();
  } else if (tecla == '#') {
    cancelarEdicao();
  }
}

// Define qual dia da semana recebera um horario.
// Limpa a digitacao anterior e abre a tela de horario.
void selecionarDiaCadastro(uint8_t dia) {
  if (dia > 6) return;
  diaEmEdicao = dia;
  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';
  telaAtual = TELA_CADASTRO_HORARIO;
  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 13 | CADASTRO E ORDENACAO DOS HORARIOS
// Recebe quatro digitos HHMM, valida 00:00 a 23:59, evita duplicatas
// no mesmo dia e organiza os horarios em ordem cronologica.
// ---------------------------------------------------------------------------

// Interpreta os comandos PS/2 para digitar uma dose.
// Recebe numeros e usa ENTER/BACKSPACE para confirmar ou apagar.
void tratarHorarioPS2(uint8_t tecla) {
  if (tecla == PS2_ENTER) {
    confirmarHorario();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    apagarDigitoHorario();
  } else if (tecla >= '0' && tecla <= '9') {
    adicionarDigitoHorario((char)tecla);
  }
}

// Interpreta os comandos matriciais para o horario.
// Usa * para confirmar, # para apagar e numeros para digitar.
void tratarHorarioMatricial(char tecla) {
  if (tecla == '*') {
    confirmarHorario();
  } else if (tecla == '#') {
    apagarDigitoHorario();
  } else if (tecla >= '0' && tecla <= '9') {
    adicionarDigitoHorario(tecla);
  }
}

// Acrescenta um digito do horario HHMM.
// Bloqueia valores impossiveis ainda durante a digitacao.
void adicionarDigitoHorario(char digito) {
  if (digito < '0' || digito > '9' || tamanhoHorario >= 4) return;

  // Horas: primeiro digito limitado a 0, 1 ou 2.
  if (tamanhoHorario == 0 && digito > '2') {
    mostrarMensagem(F("Hora permitida: 00 a 23"));
    return;
  }

  // Horas: com prefixo 2, o segundo digito fica entre 0 e 3.
  if (tamanhoHorario == 1 &&
      horarioEmEdicao[0] == '2' && digito > '3') {
    mostrarMensagem(F("Hora maxima permitida: 23"));
    return;
  }

  // Minutos: primeiro digito limitado a 0 a 5.
  if (tamanhoHorario == 2 && digito > '5') {
    mostrarMensagem(F("Minutos permitidos: 00 a 59"));
    return;
  }

  horarioEmEdicao[tamanhoHorario++] = digito;
  horarioEmEdicao[tamanhoHorario] = '\0';
  redesenharTela = true;
}

// Remove o ultimo digito de HHMM.
// Quando o campo esta vazio, retorna a selecao de dia.
void apagarDigitoHorario() {
  if (tamanhoHorario > 0) {
    tamanhoHorario--;
    horarioEmEdicao[tamanhoHorario] = '\0';
    redesenharTela = true;
  } else {
    telaAtual = TELA_CADASTRO_DIA;
    redesenharTela = true;
  }
}

// Procura um horario repetido em um dia especifico.
// Compara hora e minuto com as doses ja cadastradas.
bool existeHorarioNoMedicamento(const Medicamento& med, uint8_t dia,
                                uint8_t hora, uint8_t minuto) {
  if (dia > 6) return false;
  for (uint8_t h = 0; h < med.qtdHorarios[dia]; h++) {
    if (med.horarios[dia][h].hora == hora &&
        med.horarios[dia][h].minuto == minuto) {
      return true;
    }
  }
  return false;
}

// Organiza as doses de um dia em ordem crescente.
// Aplica insercao ordenada sobre os horarios cadastrados.
void ordenarHorariosDoDia(Medicamento& med, uint8_t dia) {
  if (dia > 6) return;

  for (uint8_t i = 1; i < med.qtdHorarios[dia]; i++) {
    HorarioDose atual = med.horarios[dia][i];
    int8_t j = i - 1;
    uint16_t minAtual = atual.hora * 60U + atual.minuto;

    while (j >= 0) {
      uint16_t minJ = med.horarios[dia][j].hora * 60U +
                      med.horarios[dia][j].minuto;
      if (minJ <= minAtual) break;
      med.horarios[dia][j + 1] = med.horarios[dia][j];
      j--;
    }
    med.horarios[dia][j + 1] = atual;
  }
}

// Inclui uma dose valida na estrutura temporaria.
// Respeita o limite por dia, evita duplicatas e ordena a lista.
bool adicionarHorarioMedicamento(Medicamento& med, uint8_t dia,
                                  uint8_t hora, uint8_t minuto) {
  if (dia > 6 || hora > 23 || minuto > 59) return false;
  if (med.qtdHorarios[dia] >= MAX_HORARIOS_DIA) return false;
  if (existeHorarioNoMedicamento(med, dia, hora, minuto)) return false;

  uint8_t pos = med.qtdHorarios[dia];
  med.horarios[dia][pos].hora = hora;
  med.horarios[dia][pos].minuto = minuto;
  med.qtdHorarios[dia]++;
  ordenarHorariosDoDia(med, dia);
  return true;
}

// Conclui a entrada de HHMM no cadastro.
// Valida os quatro digitos, adiciona a dose e abre as opcoes seguintes.
void confirmarHorario() {
  if (tamanhoHorario != 4) {
    mostrarMensagem(F("Digite o horario com 4 numeros"));
    return;
  }

  uint8_t hora = (horarioEmEdicao[0] - '0') * 10 +
                 (horarioEmEdicao[1] - '0');
  uint8_t minuto = (horarioEmEdicao[2] - '0') * 10 +
                   (horarioEmEdicao[3] - '0');

  if (hora > 23 || minuto > 59) {
    mostrarMensagem(F("Horario invalido"));
    tamanhoHorario = 0;
    horarioEmEdicao[0] = '\0';
    return;
  }

  if (medicamentoEmEdicao.qtdHorarios[diaEmEdicao] >= MAX_HORARIOS_DIA) {
    mostrarMensagem(F("Limite: 4 horarios neste dia"));
    telaAtual = TELA_CADASTRO_ACAO;
    redesenharTela = true;
    return;
  }

  if (existeHorarioNoMedicamento(medicamentoEmEdicao, diaEmEdicao,
                                 hora, minuto)) {
    mostrarMensagem(F("Este horario ja foi adicionado"));
    tamanhoHorario = 0;
    horarioEmEdicao[0] = '\0';
    return;
  }

  if (!adicionarHorarioMedicamento(medicamentoEmEdicao, diaEmEdicao,
                                   hora, minuto)) {
    mostrarMensagem(F("Nao foi possivel adicionar horario"));
    return;
  }

  ultimaHoraAdicionada = hora;
  ultimoMinutoAdicionado = minuto;
  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';

  telaAtual = TELA_CADASTRO_ACAO;
  redesenharTela = true;
}

// Calcula quantas doses foram registradas em uma semana.
// Soma os contadores dos sete dias do medicamento.
uint8_t contarHorariosSemana(const Medicamento& med) {
  uint8_t total = 0;
  for (uint8_t d = 0; d < 7; d++) {
    total += med.qtdHorarios[d];
  }
  return total;
}

// Verifica se o cadastro ja possui ao menos uma dose.
// Consulta a soma semanal para permitir ou impedir o salvamento.
bool medicamentoTemHorarios(const Medicamento& med) {
  return contarHorariosSemana(med) > 0;
}

// ---------------------------------------------------------------------------
// 14 | FINALIZACAO DO CADASTRO
// Permite repetir o horario em outro dia, incluir outra dose no dia
// atual ou salvar de uma vez o medicamento completo na EEPROM.
// ---------------------------------------------------------------------------

// Trata a escolha apos incluir uma dose pelo PS/2.
// Permite repetir no dia, selecionar outro ou concluir o cadastro.
void tratarAcaoCadastroPS2(uint8_t tecla) {
  if (tecla == '1') {
    if (medicamentoEmEdicao.qtdHorarios[diaEmEdicao] >= MAX_HORARIOS_DIA) {
      mostrarMensagem(F("Dia ja possui 4 horarios"));
    } else {
      telaAtual = TELA_CADASTRO_HORARIO;
      redesenharTela = true;
    }
  } else if (tecla == '2') {
    telaAtual = TELA_CADASTRO_DIA;
    redesenharTela = true;
  } else if (tecla == '0' || tecla == PS2_ENTER) {
    concluirCadastro();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    telaAtual = TELA_CADASTRO_DIA;
    redesenharTela = true;
  }
}

// Trata a escolha apos incluir uma dose pelo matricial.
// Encaminha para outro horario, outro dia ou gravacao final.
void tratarAcaoCadastroMatricial(char tecla) {
  if (tecla == '1') {
    if (medicamentoEmEdicao.qtdHorarios[diaEmEdicao] >= MAX_HORARIOS_DIA) {
      mostrarMensagem(F("Dia ja possui 4 horarios"));
    } else {
      telaAtual = TELA_CADASTRO_HORARIO;
      redesenharTela = true;
    }
  } else if (tecla == '2') {
    telaAtual = TELA_CADASTRO_DIA;
    redesenharTela = true;
  } else if (tecla == '0' || tecla == '*') {
    concluirCadastro();
  } else if (tecla == '#') {
    telaAtual = TELA_CADASTRO_DIA;
    redesenharTela = true;
  }
}

// Grava um novo medicamento somente quando o cadastro e valido.
// Confere quantidade e horarios, copia o registro e salva na EEPROM.
void concluirCadastro() {
  if (!medicamentoTemHorarios(medicamentoEmEdicao)) {
    mostrarMensagem(F("Adicione pelo menos um horario"));
    telaAtual = TELA_CADASTRO_DIA;
    redesenharTela = true;
    return;
  }

  if (dados.quantidade >= MAX_MEDICAMENTOS) {
    telaAtual = TELA_PRINCIPAL;
    mostrarMensagem(F("Memoria de medicamentos cheia"));
    return;
  }

  dados.medicamentos[dados.quantidade] = medicamentoEmEdicao;
  dados.medicamentos[dados.quantidade].ativo = 1;
  dados.quantidade++;
  dados.reabastecimentoPendente = 0;
  salvarDados();

  telaAtual = TELA_PRINCIPAL;
  mostrarMensagem(F("Medicamento cadastrado com sucesso"), 4000UL);
  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 15 | AJUSTE MANUAL DE DATA E HORA
// Interpreta 12 digitos no formato DDMMAAAAHHMM, verifica o calendario
// e atualiza o RTC somente depois da confirmacao do usuario.
// ---------------------------------------------------------------------------

// Recebe a data/hora digitada pelo teclado PS/2.
// Encaminha numeros, apagamento e confirmacao do formulario.
void tratarRtcPS2(uint8_t tecla) {
  Serial.print(F("Tecla PS/2 no ajuste RTC - codigo: "));
  Serial.println(tecla);

  if (tecla == PS2_ENTER) {
    confirmarAjusteRtc();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    apagarDigitoDataHora();
  } else if (tecla >= '0' && tecla <= '9') {
    adicionarDigitoDataHora((char)tecla);
  }
}

// Recebe a data/hora digitada pelo teclado 3x4.
// Usa * e # para confirmar ou apagar o campo.
void tratarRtcMatricial(char tecla) {
  if (tecla == '*') {
    confirmarAjusteRtc();
  } else if (tecla == '#') {
    apagarDigitoDataHora();
  } else if (tecla >= '0' && tecla <= '9') {
    adicionarDigitoDataHora(tecla);
  }
}

// Acrescenta um numero no campo DDMMAAAAHHMM.
// Limita a entrada a 12 digitos e atualiza apenas o campo visual.
void adicionarDigitoDataHora(char digito) {
  if (tamanhoDataHora < 12) {
    dataHoraEmEdicao[tamanhoDataHora++] = digito;
    dataHoraEmEdicao[tamanhoDataHora] = '\0';
    desenharCampoDataHora();
  }
}

// Remove o ultimo numero do campo de data/hora.
// Atualiza a representacao na tela apos cada exclusao.
void apagarDigitoDataHora() {
  if (tamanhoDataHora > 0) {
    tamanhoDataHora--;
    dataHoraEmEdicao[tamanhoDataHora] = '\0';
    desenharCampoDataHora();
  }
}

// Valida e aplica a nova data/hora ao modulo RTC.
// Converte 12 digitos para campos numericos e usa rtc.adjust.
void confirmarAjusteRtc() {
  if (!rtcDisponivel) {
    mostrarMensagem(F("RTC I2C nao encontrado"));
    return;
  }

  if (tamanhoDataHora != 12) {
    mostrarMensagem(F("Digite os 12 numeros"));
    return;
  }

  uint8_t dia = (dataHoraEmEdicao[0] - '0') * 10 +
                (dataHoraEmEdicao[1] - '0');
  uint8_t mes = (dataHoraEmEdicao[2] - '0') * 10 +
                (dataHoraEmEdicao[3] - '0');
  uint16_t ano = (dataHoraEmEdicao[4] - '0') * 1000UL +
                 (dataHoraEmEdicao[5] - '0') * 100UL +
                 (dataHoraEmEdicao[6] - '0') * 10UL +
                 (dataHoraEmEdicao[7] - '0');
  uint8_t hora = (dataHoraEmEdicao[8] - '0') * 10 +
                 (dataHoraEmEdicao[9] - '0');
  uint8_t minuto = (dataHoraEmEdicao[10] - '0') * 10 +
                   (dataHoraEmEdicao[11] - '0');

  if (!dataHoraValida(ano, mes, dia, hora, minuto)) {
    mostrarMensagem(F("Data ou hora invalida"));
    tamanhoDataHora = 0;
    dataHoraEmEdicao[0] = '\0';
    return;
  }

  rtc.adjust(DateTime(ano, mes, dia, hora, minuto, 0));
  ultimaAssinaturaMinuto = 0xFFFFFFFFUL;
  telaAtual = TELA_PRINCIPAL;
  mostrarMensagem(F("Data e hora corrigidas"));
  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 16 | GERENCIADOR DE MEDICAMENTOS
// Agrupa consulta paginada, edicao em copia temporaria e exclusao.
// Operacoes destrutivas exigem confirmacao antes de salvar na EEPROM.
// ---------------------------------------------------------------------------

// Determina o numero de paginas da listagem.
// Divide os registros em blocos de tres, com minimo de uma pagina.
uint8_t totalPaginasGerenciador() {
  if (dados.quantidade == 0) return 1;
  return (dados.quantidade + ITENS_POR_PAGINA - 1) / ITENS_POR_PAGINA;
}

// Abre os detalhes de um registro escolhido.
// Verifica se o indice existe antes de trocar de tela.
void selecionarMedicamentoGerenciador(uint8_t indice) {
  if (indice >= dados.quantidade) return;
  indiceMedicamentoSelecionado = indice;
  telaAtual = TELA_GERENCIADOR_ITEM;
  redesenharTela = true;
}

// Prepara uma copia editavel do medicamento escolhido.
// Mantem o original intacto ate o comando de salvar.
void iniciarEdicaoMedicamento(uint8_t indice) {
  if (indice >= dados.quantidade) return;

  indiceMedicamentoEdicao = indice;
  modoEdicaoMedicamento = true;
  medicamentoEmEdicao = dados.medicamentos[indice];

  strncpy(nomeEmEdicao, medicamentoEmEdicao.nome, TAM_NOME - 1);
  nomeEmEdicao[TAM_NOME - 1] = '\0';
  tamanhoNome = strlen(nomeEmEdicao);

  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';
  indiceHorarioOriginal = -1;
  diaHorarioOriginal = 0;
  diaHorarioDestino = 0;
  finalizarMultiClique();

  telaAtual = TELA_EDITAR_MENU;
  redesenharTela = true;
}

// Abre a alteracao do nome do medicamento.
// Recarrega o nome da copia e reaproveita a tela de digitacao.
void abrirEdicaoNome() {
  if (!modoEdicaoMedicamento) return;

  strncpy(nomeEmEdicao, medicamentoEmEdicao.nome, TAM_NOME - 1);
  nomeEmEdicao[TAM_NOME - 1] = '\0';
  tamanhoNome = strlen(nomeEmEdicao);
  finalizarMultiClique();

  telaAtual = TELA_CADASTRO_NOME;
  redesenharTela = true;
}

// Abre a edicao dos dias e das doses.
// Encaminha para a selecao do dia dentro da copia temporaria.
void abrirEdicaoHorarios() {
  if (!modoEdicaoMedicamento) return;
  telaAtual = TELA_EDITAR_DIAS;
  redesenharTela = true;
}

// Descarta as modificacoes de um medicamento.
// Limpa os controles temporarios sem executar salvarDados.
void cancelarEdicaoMedicamento() {
  modoEdicaoMedicamento = false;
  indiceMedicamentoEdicao = -1;
  indiceMedicamentoSelecionado = -1;
  indiceHorarioOriginal = -1;
  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';
  finalizarMultiClique();

  telaAtual = TELA_GERENCIADOR;
  mostrarMensagem(F("Alteracoes canceladas"));
  redesenharTela = true;
}

// Confirma as modificacoes feitas na copia temporaria.
// Confere nome/horarios e so entao substitui o registro e grava.
void salvarEdicaoMedicamento() {
  if (!modoEdicaoMedicamento ||
      indiceMedicamentoEdicao < 0 ||
      indiceMedicamentoEdicao >= dados.quantidade) {
    voltarAoGerenciador();
    return;
  }

  if (strlen(medicamentoEmEdicao.nome) == 0) {
    mostrarMensagem(F("O medicamento precisa ter nome"));
    return;
  }

  if (!medicamentoTemHorarios(medicamentoEmEdicao)) {
    mostrarMensagem(F("Mantenha pelo menos um horario"));
    return;
  }

  dados.medicamentos[(uint8_t)indiceMedicamentoEdicao] = medicamentoEmEdicao;
  dados.medicamentos[(uint8_t)indiceMedicamentoEdicao].ativo = 1;
  salvarDados();

  ultimaAssinaturaMinuto = 0xFFFFFFFFUL;
  modoEdicaoMedicamento = false;
  indiceMedicamentoEdicao = -1;
  indiceMedicamentoSelecionado = -1;
  indiceHorarioOriginal = -1;

  telaAtual = TELA_GERENCIADOR;
  mostrarMensagem(F("Alteracoes salvas na EEPROM"), 4000UL);
  redesenharTela = true;
}

// Exclui uma dose de um dia na estrutura em memoria.
// Desloca as doses seguintes e atualiza o contador.
void removerHorarioMedicamento(Medicamento& med, uint8_t dia, uint8_t indice) {
  if (dia > 6 || indice >= med.qtdHorarios[dia]) return;

  for (uint8_t i = indice; i + 1 < med.qtdHorarios[dia]; i++) {
    med.horarios[dia][i] = med.horarios[dia][i + 1];
  }

  if (med.qtdHorarios[dia] > 0) {
    med.qtdHorarios[dia]--;
    med.horarios[dia][med.qtdHorarios[dia]].hora = 0;
    med.horarios[dia][med.qtdHorarios[dia]].minuto = 0;
  }
}

// Inicia a adicao de uma dose durante a edicao.
// Verifica limite diario e limpa os campos de entrada.
void iniciarNovoHorarioEdicao() {
  if (diaEmEdicao > 6) return;

  if (medicamentoEmEdicao.qtdHorarios[diaEmEdicao] >= MAX_HORARIOS_DIA) {
    mostrarMensagem(F("Este dia ja possui 4 horarios"));
    return;
  }

  indiceHorarioOriginal = -1;
  diaHorarioOriginal = diaEmEdicao;
  diaHorarioDestino = diaEmEdicao;
  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';

  telaAtual = TELA_EDITAR_HORARIO_VALOR;
  redesenharTela = true;
}

// Abre a alteracao de uma dose ja existente.
// Guarda o indice e o dia originais antes da nova escolha.
void selecionarHorarioEdicao(uint8_t indice) {
  if (diaEmEdicao > 6 ||
      indice >= medicamentoEmEdicao.qtdHorarios[diaEmEdicao]) {
    return;
  }

  indiceHorarioOriginal = indice;
  diaHorarioOriginal = diaEmEdicao;
  diaHorarioDestino = diaEmEdicao;
  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';

  telaAtual = TELA_EDITAR_HORARIO_DESTINO;
  redesenharTela = true;
}

// Define para qual dia a dose editada sera atribuida.
// Prepara a digitacao de um novo horario para esse dia.
void escolherDiaDestinoEdicao(uint8_t dia) {
  if (dia > 6) return;
  diaHorarioDestino = dia;
  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';
  telaAtual = TELA_EDITAR_HORARIO_VALOR;
  redesenharTela = true;
}

// Valida a inclusao ou movimentacao de uma dose.
// Aplica alteracoes em copia candidata, evitando perdas se falharem.
void confirmarHorarioEdicao() {
  if (tamanhoHorario != 4) {
    mostrarMensagem(F("Digite o horario com 4 numeros"));
    return;
  }

  uint8_t hora = (horarioEmEdicao[0] - '0') * 10 +
                 (horarioEmEdicao[1] - '0');
  uint8_t minuto = (horarioEmEdicao[2] - '0') * 10 +
                   (horarioEmEdicao[3] - '0');

  if (hora > 23 || minuto > 59 || diaHorarioDestino > 6) {
    mostrarMensagem(F("Horario invalido"));
    return;
  }

  Medicamento candidato = medicamentoEmEdicao;

  if (indiceHorarioOriginal >= 0) {
    if (diaHorarioOriginal > 6 ||
        indiceHorarioOriginal >= candidato.qtdHorarios[diaHorarioOriginal]) {
      mostrarMensagem(F("Horario original nao encontrado"));
      return;
    }
    removerHorarioMedicamento(candidato, diaHorarioOriginal,
                              (uint8_t)indiceHorarioOriginal);
  }

  if (candidato.qtdHorarios[diaHorarioDestino] >= MAX_HORARIOS_DIA) {
    mostrarMensagem(F("Destino ja possui 4 horarios"));
    return;
  }

  if (existeHorarioNoMedicamento(candidato, diaHorarioDestino, hora, minuto)) {
    mostrarMensagem(F("Este horario ja existe no dia"));
    return;
  }

  if (!adicionarHorarioMedicamento(candidato, diaHorarioDestino, hora, minuto)) {
    mostrarMensagem(F("Nao foi possivel alterar horario"));
    return;
  }

  medicamentoEmEdicao = candidato;
  diaEmEdicao = diaHorarioDestino;
  indiceHorarioOriginal = -1;
  tamanhoHorario = 0;
  horarioEmEdicao[0] = '\0';

  telaAtual = TELA_EDITAR_DIA_HORARIOS;
  mostrarMensagem(F("Horario alterado na copia"));
  redesenharTela = true;
}

// Navega pela lista com o teclado PS/2.
// Seleciona item ou muda de pagina conforme o comando.
void tratarGerenciadorPS2(uint8_t tecla) {
  if (tecla == TECLA_PS2_F1) {
    iniciarCadastro();
  } else if (tecla >= '1' && tecla <= '3') {
    uint8_t indice = paginaGerenciador * ITENS_POR_PAGINA + (tecla - '1');
    if (indice < dados.quantidade) selecionarMedicamentoGerenciador(indice);
  } else if (tecla == '0') {
    paginaGerenciador = (paginaGerenciador + 1) % totalPaginasGerenciador();
    redesenharTela = true;
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    uint8_t total = totalPaginasGerenciador();
    paginaGerenciador = (paginaGerenciador == 0) ? total - 1
                                                  : paginaGerenciador - 1;
    redesenharTela = true;
  } else if (tecla == PS2_ENTER) {
    telaAtual = TELA_PRINCIPAL;
    redesenharTela = true;
  }
}

// Navega pela lista com o teclado 3x4.
// Seleciona um dos tres itens ou muda de pagina.
void tratarGerenciadorMatricial(char tecla) {
  if (tecla >= '1' && tecla <= '3') {
    uint8_t indice = paginaGerenciador * ITENS_POR_PAGINA + (tecla - '1');
    if (indice < dados.quantidade) selecionarMedicamentoGerenciador(indice);
  } else if (tecla == '0') {
    paginaGerenciador = (paginaGerenciador + 1) % totalPaginasGerenciador();
    redesenharTela = true;
  } else if (tecla == '#') {
    uint8_t total = totalPaginasGerenciador();
    paginaGerenciador = (paginaGerenciador == 0) ? total - 1
                                                  : paginaGerenciador - 1;
    redesenharTela = true;
  } else if (tecla == '*') {
    telaAtual = TELA_PRINCIPAL;
    redesenharTela = true;
  }
}

// Escolhe entre editar e excluir o item selecionado.
// Utiliza os comandos PS/2 para navegar ou voltar.
void tratarGerenciadorItemPS2(uint8_t tecla) {
  if (indiceMedicamentoSelecionado < 0 ||
      indiceMedicamentoSelecionado >= dados.quantidade) {
    voltarAoGerenciador();
    return;
  }

  if (tecla == '1') {
    iniciarEdicaoMedicamento((uint8_t)indiceMedicamentoSelecionado);
  } else if (tecla == '2') {
    solicitarExclusao((uint8_t)indiceMedicamentoSelecionado);
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE ||
             tecla == PS2_ENTER) {
    voltarAoGerenciador();
  }
}

// Escolhe entre editar e excluir pelo matricial.
// Valida o indice do medicamento antes da acao.
void tratarGerenciadorItemMatricial(char tecla) {
  if (indiceMedicamentoSelecionado < 0 ||
      indiceMedicamentoSelecionado >= dados.quantidade) {
    voltarAoGerenciador();
    return;
  }

  if (tecla == '1') {
    iniciarEdicaoMedicamento((uint8_t)indiceMedicamentoSelecionado);
  } else if (tecla == '2') {
    solicitarExclusao((uint8_t)indiceMedicamentoSelecionado);
  } else if (tecla == '#' || tecla == '*') {
    voltarAoGerenciador();
  }
}

// Controla o menu de alteracoes pelo PS/2.
// Abre nome, horarios, salvar ou cancelar.
void tratarEditarMenuPS2(uint8_t tecla) {
  if (tecla == '1') {
    abrirEdicaoNome();
  } else if (tecla == '2') {
    abrirEdicaoHorarios();
  } else if (tecla == '0' || tecla == PS2_ENTER) {
    salvarEdicaoMedicamento();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    cancelarEdicaoMedicamento();
  }
}

// Controla o menu de alteracoes pelo teclado matricial.
// Distingue entrada em campos do salvamento final.
void tratarEditarMenuMatricial(char tecla) {
  if (tecla == '1') {
    abrirEdicaoNome();
  } else if (tecla == '2') {
    abrirEdicaoHorarios();
  } else if (tecla == '0' || tecla == '*') {
    salvarEdicaoMedicamento();
  } else if (tecla == '#') {
    cancelarEdicaoMedicamento();
  }
}

// Seleciona o dia a editar pelo PS/2.
// Entra na lista diaria ou volta ao menu de edicao.
void tratarEditarDiasPS2(uint8_t tecla) {
  if (tecla >= '1' && tecla <= '7') {
    diaEmEdicao = tecla - '1';
    telaAtual = TELA_EDITAR_DIA_HORARIOS;
    redesenharTela = true;
  } else if (tecla == '0' || tecla == PS2_ENTER ||
             tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    telaAtual = TELA_EDITAR_MENU;
    redesenharTela = true;
  }
}

// Seleciona o dia a editar pelo matricial.
// Interpreta 1 a 7 ou os comandos de retorno.
void tratarEditarDiasMatricial(char tecla) {
  if (tecla >= '1' && tecla <= '7') {
    diaEmEdicao = tecla - '1';
    telaAtual = TELA_EDITAR_DIA_HORARIOS;
    redesenharTela = true;
  } else if (tecla == '0' || tecla == '*' || tecla == '#') {
    telaAtual = TELA_EDITAR_MENU;
    redesenharTela = true;
  }
}

// Seleciona uma dose para editar ou cria outra.
// Usa as opcoes 1 a 4 e 5 no teclado PS/2.
void tratarEditarDiaHorariosPS2(uint8_t tecla) {
  if (tecla >= '1' && tecla <= '4') {
    uint8_t indice = tecla - '1';
    if (indice < medicamentoEmEdicao.qtdHorarios[diaEmEdicao]) {
      selecionarHorarioEdicao(indice);
    }
  } else if (tecla == '5') {
    iniciarNovoHorarioEdicao();
  } else if (tecla == '0' || tecla == PS2_BACKSPACE ||
             tecla == PS2_DELETE || tecla == PS2_ENTER) {
    telaAtual = TELA_EDITAR_DIAS;
    redesenharTela = true;
  }
}

// Seleciona uma dose para editar pelo matricial.
// Permite escolher um horario ou adicionar novo.
void tratarEditarDiaHorariosMatricial(char tecla) {
  if (tecla >= '1' && tecla <= '4') {
    uint8_t indice = tecla - '1';
    if (indice < medicamentoEmEdicao.qtdHorarios[diaEmEdicao]) {
      selecionarHorarioEdicao(indice);
    }
  } else if (tecla == '5') {
    iniciarNovoHorarioEdicao();
  } else if (tecla == '0' || tecla == '#' || tecla == '*') {
    telaAtual = TELA_EDITAR_DIAS;
    redesenharTela = true;
  }
}

// Define novo dia ou remove a dose escolhida via PS/2.
// A exclusao modifica apenas a copia do medicamento.
void tratarEditarHorarioDestinoPS2(uint8_t tecla) {
  if (tecla >= '1' && tecla <= '7') {
    escolherDiaDestinoEdicao(tecla - '1');
  } else if (tecla == '0') {
    if (indiceHorarioOriginal >= 0 &&
        diaHorarioOriginal <= 6 &&
        indiceHorarioOriginal <
          medicamentoEmEdicao.qtdHorarios[diaHorarioOriginal]) {
      removerHorarioMedicamento(medicamentoEmEdicao, diaHorarioOriginal,
                                (uint8_t)indiceHorarioOriginal);
      diaEmEdicao = diaHorarioOriginal;
      indiceHorarioOriginal = -1;
      telaAtual = TELA_EDITAR_DIA_HORARIOS;
      mostrarMensagem(F("Horario excluido da copia"));
      redesenharTela = true;
    }
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE ||
             tecla == PS2_ENTER) {
    telaAtual = TELA_EDITAR_DIA_HORARIOS;
    redesenharTela = true;
  }
}

// Define novo dia ou remove a dose pelo matricial.
// Mantem as alteracoes na copia ate salvar.
void tratarEditarHorarioDestinoMatricial(char tecla) {
  if (tecla >= '1' && tecla <= '7') {
    escolherDiaDestinoEdicao(tecla - '1');
  } else if (tecla == '0') {
    if (indiceHorarioOriginal >= 0 &&
        diaHorarioOriginal <= 6 &&
        indiceHorarioOriginal <
          medicamentoEmEdicao.qtdHorarios[diaHorarioOriginal]) {
      removerHorarioMedicamento(medicamentoEmEdicao, diaHorarioOriginal,
                                (uint8_t)indiceHorarioOriginal);
      diaEmEdicao = diaHorarioOriginal;
      indiceHorarioOriginal = -1;
      telaAtual = TELA_EDITAR_DIA_HORARIOS;
      mostrarMensagem(F("Horario excluido da copia"));
      redesenharTela = true;
    }
  } else if (tecla == '#' || tecla == '*') {
    telaAtual = TELA_EDITAR_DIA_HORARIOS;
    redesenharTela = true;
  }
}

// Recebe os digitos do novo horario pelo PS/2.
// Permite confirmar, apagar e voltar a etapa anterior.
void tratarEditarHorarioValorPS2(uint8_t tecla) {
  if (tecla == PS2_ENTER) {
    confirmarHorarioEdicao();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    if (tamanhoHorario > 0) {
      tamanhoHorario--;
      horarioEmEdicao[tamanhoHorario] = '\0';
      redesenharTela = true;
    } else {
      telaAtual = (indiceHorarioOriginal >= 0)
                  ? TELA_EDITAR_HORARIO_DESTINO
                  : TELA_EDITAR_DIA_HORARIOS;
      redesenharTela = true;
    }
  } else if (tecla >= '0' && tecla <= '9') {
    adicionarDigitoHorario((char)tecla);
  }
}

// Recebe os digitos do novo horario pelo teclado 3x4.
// Executa a mesma validacao da entrada convencional.
void tratarEditarHorarioValorMatricial(char tecla) {
  if (tecla == '*') {
    confirmarHorarioEdicao();
  } else if (tecla == '#') {
    if (tamanhoHorario > 0) {
      tamanhoHorario--;
      horarioEmEdicao[tamanhoHorario] = '\0';
      redesenharTela = true;
    } else {
      telaAtual = (indiceHorarioOriginal >= 0)
                  ? TELA_EDITAR_HORARIO_DESTINO
                  : TELA_EDITAR_DIA_HORARIOS;
      redesenharTela = true;
    }
  } else if (tecla >= '0' && tecla <= '9') {
    adicionarDigitoHorario(tecla);
  }
}

// Confirma ou cancela a exclusao pelo PS/2.
// A remocao so ocorre quando o usuario usa ENTER.
void tratarConfirmacaoExclusaoPS2(uint8_t tecla) {
  if (tecla == PS2_ENTER && indiceExclusaoPendente >= 0) {
    excluirMedicamento((uint8_t)indiceExclusaoPendente);
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    voltarAoGerenciador();
  }
}

// Confirma ou cancela a exclusao pelo matricial.
// A remocao so ocorre quando o usuario pressiona *.
void tratarConfirmacaoExclusaoMatricial(char tecla) {
  if (tecla == '*' && indiceExclusaoPendente >= 0) {
    excluirMedicamento((uint8_t)indiceExclusaoPendente);
  } else if (tecla == '#') {
    voltarAoGerenciador();
  }
}

// Confirma ou cancela a limpeza completa via PS/2.
// Encaminha para a rotina destrutiva apenas com ENTER.
void tratarConfirmacaoLimparTudoPS2(uint8_t tecla) {
  if (tecla == PS2_ENTER) {
    excluirTodosMedicamentos();
  } else if (tecla == PS2_BACKSPACE || tecla == PS2_DELETE) {
    voltarAoGerenciador();
  }
}

// Confirma ou cancela a limpeza completa no matricial.
// Usa * para confirmar ou # para desistir.
void tratarConfirmacaoLimparTudoMatricial(char tecla) {
  if (tecla == '*') {
    excluirTodosMedicamentos();
  } else if (tecla == '#') {
    voltarAoGerenciador();
  }
}

// Abre a confirmacao antes de apagar um medicamento.
// Guarda o indice solicitado sem remover o registro.
void solicitarExclusao(uint8_t indice) {
  if (indice >= dados.quantidade) return;
  indiceExclusaoPendente = indice;
  telaAtual = TELA_CONFIRMAR_EXCLUSAO;
  redesenharTela = true;
}

// Remove um medicamento confirmado e reorganiza o banco.
// Desloca os registros seguintes, atualiza contador e salva na EEPROM.
void excluirMedicamento(uint8_t indice) {
  if (indice >= dados.quantidade) {
    voltarAoGerenciador();
    return;
  }

  for (uint8_t i = indice; i + 1 < dados.quantidade; i++) {
    dados.medicamentos[i] = dados.medicamentos[i + 1];
  }

  if (dados.quantidade > 0) {
    dados.quantidade--;
    memset(&dados.medicamentos[dados.quantidade], 0, sizeof(Medicamento));
  }

  dados.reabastecimentoPendente = 0;
  salvarDados();

  uint8_t total = totalPaginasGerenciador();
  if (paginaGerenciador >= total) paginaGerenciador = total - 1;

  indiceExclusaoPendente = -1;
  indiceMedicamentoSelecionado = -1;
  telaAtual = TELA_GERENCIADOR;
  mostrarMensagem(F("Medicamento excluido"));
  redesenharTela = true;
}

// Abre a confirmacao de exclusao geral.
// Evita exibir confirmacao quando nao existem registros.
void solicitarLimparTodos() {
  if (dados.quantidade == 0) {
    mostrarMensagem(F("Nao ha medicamentos cadastrados"));
    telaAtual = TELA_GERENCIADOR;
    return;
  }

  telaAtual = TELA_CONFIRMAR_LIMPAR_TUDO;
  redesenharTela = true;
}

// Apaga todos os registros apos a confirmacao.
// Zera banco, alarmes e controles temporarios, depois salva.
void excluirTodosMedicamentos() {
  memset(dados.medicamentos, 0, sizeof(dados.medicamentos));
  dados.quantidade = 0;
  dados.reabastecimentoPendente = 0;
  salvarDados();

  alarmeMedicamentoAtivo = false;
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
  buzzerLigado = false;
  alarmeEhUltimaDoseDaSemana = false;
  ultimaAssinaturaMinuto = 0xFFFFFFFFUL;
  paginaGerenciador = 0;
  indiceExclusaoPendente = -1;
  indiceMedicamentoSelecionado = -1;
  modoEdicaoMedicamento = false;
  indiceMedicamentoEdicao = -1;
  telaAtual = TELA_PRINCIPAL;
  mostrarMensagem(F("Todos os medicamentos foram apagados"), 4000UL);
  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 17 | VALIDACAO DE CALENDARIO
// Confere intervalos de ano, mes, dia, hora e minuto; considera
// anos bissextos para validar corretamente o mes de fevereiro.
// ---------------------------------------------------------------------------

// Confere todos os campos de uma data e hora digitadas.
// Valida o dia conforme o mes e rejeita horas/minutos fora do intervalo.
bool dataHoraValida(uint16_t ano, uint8_t mes, uint8_t dia,
                    uint8_t hora, uint8_t minuto) {
  if (ano < 2000 || ano > 2099) return false;
  if (mes < 1 || mes > 12) return false;
  if (dia < 1 || dia > diasNoMes(ano, mes)) return false;
  if (hora > 23 || minuto > 59) return false;
  return true;
}

// Informa a quantidade de dias de cada mes.
// Aplica a regra de ano bissexto especificamente a fevereiro.
uint8_t diasNoMes(uint16_t ano, uint8_t mes) {
  static const uint8_t tabela[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };

  if (mes == 2) {
    bool bissexto = (ano % 400 == 0) || ((ano % 4 == 0) && (ano % 100 != 0));
    return bissexto ? 29 : 28;
  }

  return tabela[mes - 1];
}

// ---------------------------------------------------------------------------
// 18 | IDENTIFICACAO DOS HORARIOS PROGRAMADOS
// Compara data/hora do RTC aos horarios cadastrados, uma vez por minuto.
// Detecta tambem a ultima dose na sequencia semanal cadastrada.
// ---------------------------------------------------------------------------

// Converte o dia da semana do RTC para o indice interno.
// Transforma domingo=0 em segunda=0, deixando domingo=6.
uint8_t indiceDiaSegundaPrimeiro(const DateTime& dataHora) {
  return (dataHora.dayOfTheWeek() + 6) % 7;
}

// Verifica se um medicamento deve alertar no instante consultado.
// Compara dia, hora e minuto apenas nos horarios ativos.
bool medicamentoPossuiHorario(const Medicamento& med, uint8_t dia,
                              uint8_t hora, uint8_t minuto) {
  if (!med.ativo || dia > 6) return false;

  for (uint8_t h = 0; h < med.qtdHorarios[dia]; h++) {
    if (med.horarios[dia][h].hora == hora &&
        med.horarios[dia][h].minuto == minuto) {
      return true;
    }
  }
  return false;
}

// Procura doses programadas para o minuto atual.
// Uma assinatura de minuto impede disparos duplicados no mesmo instante.
void verificarAlarmes(const DateTime& agora) {
  uint32_t assinaturaMinuto = agora.unixtime() / 60UL;

  if (assinaturaMinuto == ultimaAssinaturaMinuto) return;
  ultimaAssinaturaMinuto = assinaturaMinuto;

  if (dados.reabastecimentoPendente || alarmeMedicamentoAtivo) return;

  uint8_t dia = indiceDiaSegundaPrimeiro(agora);
  if (existeMedicamentoAgora(dia, agora.hour(), agora.minute())) {
    iniciarAlarmeMedicamento(dia, agora.hour(), agora.minute());
  }
}

// Busca medicamentos com dose no horario consultado.
// Interrompe a busca quando encontra o primeiro valido.
bool existeMedicamentoAgora(uint8_t dia, uint8_t hora, uint8_t minuto) {
  for (uint8_t i = 0; i < dados.quantidade; i++) {
    if (medicamentoPossuiHorario(dados.medicamentos[i], dia, hora, minuto)) {
      return true;
    }
  }
  return false;
}

// Verifica se a dose atual e a ultima da programacao semanal.
// Compara a posicao minuto-da-semana com a maior cadastrada.
bool ehUltimoHorarioDaSemana(uint8_t dia, uint8_t hora, uint8_t minuto) {
  int32_t maiorPosicao = -1;

  for (uint8_t i = 0; i < dados.quantidade; i++) {
    const Medicamento& med = dados.medicamentos[i];
    if (!med.ativo) continue;

    for (uint8_t d = 0; d < 7; d++) {
      for (uint8_t h = 0; h < med.qtdHorarios[d]; h++) {
        int32_t posicao = (int32_t)d * 1440L +
                          (int32_t)med.horarios[d][h].hora * 60L +
                          med.horarios[d][h].minuto;
        if (posicao > maiorPosicao) maiorPosicao = posicao;
      }
    }
  }

  int32_t posicaoAtual = (int32_t)dia * 1440L +
                         (int32_t)hora * 60L + minuto;
  return maiorPosicao >= 0 && posicaoAtual == maiorPosicao;
}

// Registra os dados do novo aviso de administracao.
// Guarda seu instante inicial e identifica a ultima dose semanal.
void iniciarAlarmeMedicamento(uint8_t dia, uint8_t hora, uint8_t minuto) {
  alarmeMedicamentoAtivo = true;
  inicioAlarmeMedicamento = millis();
  diaAlarme = dia;
  horaAlarme = hora;
  minutoAlarme = minuto;
  alarmeEhUltimaDoseDaSemana = ehUltimoHorarioDaSemana(dia, hora, minuto);

  telaAtual = TELA_PRINCIPAL;
  finalizarMultiClique();
  redesenharTela = true;
}

// Finaliza o aviso de administracao confirmado ou expirado.
// Silencia o buzzer e ativa reabastecimento se foi a ultima dose.
void encerrarAlarmeMedicamento() {
  if (!alarmeMedicamentoAtivo) return;

  alarmeMedicamentoAtivo = false;

  // Para o som e deixa o pino em alta impedancia quando inativo.
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BUZZER, INPUT);
  buzzerLigado = false;

  if (alarmeEhUltimaDoseDaSemana) {
    dados.reabastecimentoPendente = 1;
    salvarDados();
  } else {
    mostrarMensagem(F("Alarme do medicamento confirmado"));
  }

  alarmeEhUltimaDoseDaSemana = false;
  redesenharTela = true;
}

// Reconhece que o usuario confirmou o reabastecimento.
// Desfaz o estado pendente e atualiza a EEPROM.
void confirmarReabastecimento() {
  dados.reabastecimentoPendente = 0;
  salvarDados();
  mostrarMensagem(F("Reabastecimento confirmado"));
  telaAtual = TELA_PRINCIPAL;
  redesenharTela = true;
}

// ---------------------------------------------------------------------------
// 19 | SINALIZACAO LUMINOSA E SONORA
// Na operacao comum, acende apenas o LED do dia; no aviso de reposicao,
// acende os sete LEDs. O buzzer alterna entre estados com millis().
// O pino A8 fica como INPUT nos intervalos de silencio para reduzir ruido.
// ---------------------------------------------------------------------------

// Define quais indicadores luminosos ficam acesos.
// Normalmente apenas o do dia; no reabastecimento, todos os sete.
void atualizarLeds(uint8_t diaAtual) {
  if (dados.reabastecimentoPendente) {
    for (uint8_t i = 0; i < 7; i++) digitalWrite(PINOS_LEDS[i], HIGH);
    return;
  }

  for (uint8_t i = 0; i < 7; i++) {
    digitalWrite(PINOS_LEDS[i], i == diaAtual ? HIGH : LOW);
  }
}

// Executa os padroes sonoros de alerta e reabastecimento.
// Alterna som por millis() e devolve A8 a INPUT quando silencioso.
void atualizarBuzzer() {
  bool deveTocar = false;

  if (dados.reabastecimentoPendente) {
    deveTocar = ((millis() / 350UL) % 2UL) == 0;
  } else if (alarmeMedicamentoAtivo) {
    deveTocar = ((millis() / 500UL) % 2UL) == 0;
  }

  if (deveTocar) {
    if (!buzzerLigado) {
      // O A8 vira saida somente no instante em que precisa tocar.
      pinMode(PIN_BUZZER, OUTPUT);
      digitalWrite(PIN_BUZZER, LOW);
      tone(PIN_BUZZER, dados.reabastecimentoPendente ? 2600 : 2100);
      buzzerLigado = true;
    }
  } else {
    if (buzzerLigado) {
      noTone(PIN_BUZZER);
      digitalWrite(PIN_BUZZER, LOW);
      buzzerLigado = false;
    }

    // Buzzer inativo: A8 em alta impedancia.
    // Nao usar INPUT_PULLUP.
    pinMode(PIN_BUZZER, INPUT);
  }
}

// ---------------------------------------------------------------------------
// 20 | GERENCIAMENTO DA INTERFACE TFT
// Decide qual tela desenhar conforme o estado atual; alertas possuem
// prioridade sobre menus. No relogio, atualiza somente a area do horario.
// ---------------------------------------------------------------------------

// Escolhe a tela a desenhar conforme a prioridade de estados.
// Reabastecimento e alarme sobrepoem menus; a hora atualiza a cada segundo.
void atualizarInterface(const DateTime& agora) {
  if (redesenharTela) {
    redesenharTela = false;
    ultimaAtualizacaoTela = millis();

    if (dados.reabastecimentoPendente) {
      desenharAlertaReabastecimento();
      return;
    }

    if (alarmeMedicamentoAtivo) {
      desenharAlarmeMedicamento();
      return;
    }

    switch (telaAtual) {
      case TELA_PRINCIPAL: desenharTelaPrincipal(agora); break;
      case TELA_CADASTRO_NOME: desenharTelaCadastroNome(); break;
      case TELA_CADASTRO_DIA: desenharTelaCadastroDia(); break;
      case TELA_CADASTRO_HORARIO: desenharTelaCadastroHorario(); break;
      case TELA_CADASTRO_ACAO: desenharTelaCadastroAcao(); break;
      case TELA_AJUSTE_RTC: desenharTelaAjusteRtc(); break;
      case TELA_GERENCIADOR: desenharTelaGerenciador(); break;
      case TELA_GERENCIADOR_ITEM: desenharTelaGerenciadorItem(); break;
      case TELA_EDITAR_MENU: desenharTelaEditarMenu(); break;
      case TELA_EDITAR_DIAS: desenharTelaEditarDias(); break;
      case TELA_EDITAR_DIA_HORARIOS: desenharTelaEditarDiaHorarios(); break;
      case TELA_EDITAR_HORARIO_DESTINO: desenharTelaEditarHorarioDestino(); break;
      case TELA_EDITAR_HORARIO_VALOR: desenharTelaEditarHorarioValor(); break;
      case TELA_CONFIRMAR_EXCLUSAO: desenharConfirmacaoExclusao(); break;
      case TELA_CONFIRMAR_LIMPAR_TUDO: desenharConfirmacaoLimparTudo(); break;
    }
    return;
  }

  if (!dados.reabastecimentoPendente && !alarmeMedicamentoAtivo &&
      telaAtual == TELA_PRINCIPAL && rtcDisponivel && agora.isValid() &&
      millis() - ultimaAtualizacaoTela >= 1000UL) {
    ultimaAtualizacaoTela = millis();

    if (agora.day() != ultimoDiaPrincipal ||
        agora.month() != ultimoMesPrincipal ||
        agora.year() != ultimoAnoPrincipal) {
      redesenharTela = true;
    } else {
      atualizarRelogioTelaPrincipal(agora);
    }
  }
}

// Padroniza os cabecalhos das telas com fundo claro.
// Centraliza o titulo e utiliza a cor informada apenas como contorno.
void desenharCabecalho(const char* titulo, uint16_t cor) {
  tft.fillScreen(BRANCO);
  tft.fillRect(0, 0, tft.width(), 34, CINZA_CLARO);
  tft.drawRect(0, 0, tft.width(), 34, cor);
  tft.drawRect(1, 1, tft.width() - 2, 32, cor);

  uint16_t larguraTam2 = (uint16_t)(strlen(titulo) * 12U);
  uint8_t tamanho = (larguraTam2 <= (uint16_t)(tft.width() - 8)) ? 2 : 1;
  int16_t y = (tamanho == 2) ? 9 : 13;
  imprimirCentralizado(titulo, y, tamanho, PRETO);
}

// Imprime texto centralizado horizontalmente no TFT.
// Calcula a largura do texto e posiciona o cursor no centro.
void imprimirCentralizado(const char* texto, int16_t y, uint8_t tamanho,
                          uint16_t cor) {
  int16_t x1, y1;
  uint16_t largura, altura;
  tft.setTextSize(tamanho);
  tft.getTextBounds(texto, 0, y, &x1, &y1, &largura, &altura);
  int16_t x = (tft.width() - largura) / 2;
  if (x < 0) x = 0;
  tft.setTextColor(cor);
  tft.setCursor(x, y);
  tft.print(texto);
}

// Sobrecarga para titulo armazenado na memoria Flash (F()).
// Copia o texto para um buffer temporario e reutiliza a rotina principal.
void desenharCabecalho(const __FlashStringHelper* titulo, uint16_t cor) {
  char buffer[32];
  strncpy_P(buffer, (PGM_P)titulo, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  desenharCabecalho(buffer, cor);
}

// Sobrecarga para textos constantes armazenados em Flash.
// Converte para buffer antes do calculo de centralizacao.
void imprimirCentralizado(const __FlashStringHelper* texto, int16_t y,
                          uint8_t tamanho, uint16_t cor) {
  char buffer[40];
  strncpy_P(buffer, (PGM_P)texto, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  imprimirCentralizado(buffer, y, tamanho, cor);
}

// Desenha um campo retangular reutilizado na interface.
// Preenche o interior e aplica uma borda de destaque.
void desenharCaixaPadrao(int16_t x, int16_t y, int16_t largura, int16_t altura,
                         uint16_t fundo, uint16_t borda) {
  tft.fillRect(x, y, largura, altura, fundo);
  tft.drawRect(x, y, largura, altura, borda);
}

// Desenha instrucoes fixas na parte inferior do display.
// Mostra ate duas linhas de comandos sobre fundo claro.
void desenharRodapePadrao(const __FlashStringHelper* linha1,
                          const __FlashStringHelper* linha2) {
  tft.fillRect(0, 282, tft.width(), 38, CINZA_CLARO);
  tft.drawRect(0, 282, tft.width(), 38, AZUL_ESCURO);
  imprimirCentralizado(linha1, 288, 1, PRETO);
  if (linha2 != nullptr) {
    imprimirCentralizado(linha2, 304, 1, PRETO);
  }
}

// Escolhe entre rodape de instrucoes e aviso temporario.
// Exibe mensagem de status quando ela estiver ativa.
void desenharRodapeOuMensagem(const __FlashStringHelper* linha1,
                              const __FlashStringHelper* linha2) {
  if (mensagemStatus[0] != '\0') {
    tft.fillRect(0, 286, tft.width(), 34, AMARELO);
    imprimirCentralizado(mensagemStatus, 298, 1, PRETO);
  } else {
    desenharRodapePadrao(linha1, linha2);
  }
}

// ---------------------------------------------------------------------------
// 21 | TELA PRINCIPAL
// Mostra o dia, a data, o relogio e ate cinco doses do dia em ordem.
// A busca ordenada evita alocar um grande vetor auxiliar na memoria SRAM.
// ---------------------------------------------------------------------------

struct DoseTela {
  uint8_t indiceMedicamento;
  uint8_t indiceHorario;
};

// Monta a pagina inicial do dispenser.
// Mostra relogio e ate cinco programacoes ordenadas do dia atual.
void desenharTelaPrincipal(const DateTime& agora) {
  desenharCabecalho(F("DISPENSER ELETRONICO"), AZUL_ESCURO);

  if (!rtcDisponivel || !agora.isValid()) {
    imprimirCentralizado(F("RTC NAO DISPONIVEL"), 85, 2, VERMELHO);
    imprimirCentralizado(F("Confira SDA=20 e SCL=21"), 120, 1, PRETO);
    return;
  }

  ultimoDiaPrincipal = agora.day();
  ultimoMesPrincipal = agora.month();
  ultimoAnoPrincipal = agora.year();

  uint8_t diaAtual = indiceDiaSegundaPrimeiro(agora);
  char texto[40];

  imprimirCentralizado(NOMES_DIAS[diaAtual], 42, 2, PRETO);

  snprintf_P(texto, sizeof(texto), PSTR("%02u/%02u/%04u"),
           agora.day(), agora.month(), agora.year());
  imprimirCentralizado(texto, 65, 2, PRETO);

  tft.fillRect(8, 88, tft.width() - 16, 43, CINZA_CLARO);
  tft.drawRect(8, 88, tft.width() - 16, 43, AZUL_ESCURO);
  snprintf_P(texto, sizeof(texto), PSTR("%02u:%02u:%02u"),
           agora.hour(), agora.minute(), agora.second());
  imprimirCentralizado(texto, 98, 3, AZUL_ESCURO);

  tft.fillRect(0, 139, tft.width(), 24, CINZA_CLARO);
  tft.drawRect(0, 139, tft.width(), 24, AZUL_ESCURO);
  imprimirCentralizado(F("HORARIOS DE HOJE"), 147, 1, PRETO);

  // Conta as entradas programadas para o dia selecionado.
  uint8_t quantidadeDoses = 0;
  for (uint8_t i = 0; i < dados.quantidade; i++) {
    if (dados.medicamentos[i].ativo) {
      quantidadeDoses += dados.medicamentos[i].qtdHorarios[diaAtual];
    }
  }

  tft.setTextColor(PRETO);
  tft.setTextSize(1);

  if (quantidadeDoses == 0) {
    imprimirCentralizado(F("Nenhum horario"), 202, 2, PRETO);
    imprimirCentralizado(F("programado para hoje"), 226, 1, PRETO);
  } else {
    uint8_t limite = quantidadeDoses > 5 ? 5 : quantidadeDoses;
    uint32_t ultimaChave = 0;
    bool primeiraDose = true;

    // Seleciona ate cinco entradas ordenadas, sem vetor auxiliar em SRAM.
    for (uint8_t linha = 0; linha < limite; linha++) {
      bool encontrou = false;
      uint32_t melhorChave = 0xFFFFFFFFUL;
      uint8_t melhorMedicamento = 0;
      uint8_t melhorHorario = 0;

      for (uint8_t i = 0; i < dados.quantidade; i++) {
        const Medicamento& medBusca = dados.medicamentos[i];
        if (!medBusca.ativo) continue;

        for (uint8_t h = 0; h < medBusca.qtdHorarios[diaAtual]; h++) {
          const HorarioDose& horBusca = medBusca.horarios[diaAtual][h];
          uint16_t minutoDia = horBusca.hora * 60U + horBusca.minuto;
          uint32_t chave = ((uint32_t)minutoDia << 16) |
                           ((uint32_t)i << 8) | h;

          if ((primeiraDose || chave > ultimaChave) && chave < melhorChave) {
            melhorChave = chave;
            melhorMedicamento = i;
            melhorHorario = h;
            encontrou = true;
          }
        }
      }

      if (!encontrou) break;

      const Medicamento& med = dados.medicamentos[melhorMedicamento];
      const HorarioDose& hor = med.horarios[diaAtual][melhorHorario];
      int16_t y = 172 + linha * 21;

      tft.setCursor(8, y);
      snprintf_P(texto, sizeof(texto), PSTR("%02u:%02u  %.20s"),
                 hor.hora, hor.minuto, med.nome);
      tft.print(texto);

      ultimaChave = melhorChave;
      primeiraDose = false;
    }

    if (quantidadeDoses > 5) {
      snprintf_P(texto, sizeof(texto), PSTR("+%u horario(s)"), quantidadeDoses - 5);
      imprimirCentralizado(texto, 278, 1, PRETO);
    }
  }

  if (mensagemStatus[0] != '\0') {
    tft.fillRect(0, 288, tft.width(), 32, AMARELO);
    imprimirCentralizado(mensagemStatus, 300, 1, PRETO);
  } else {
    tft.fillRect(0, 288, tft.width(), 32, CINZA_CLARO);
    tft.drawRect(0, 288, tft.width(), 32, AZUL_ESCURO);
    imprimirCentralizado(F("PS2: F1 cadastro | F3 data | F5 gerenc."), 293, 1, PRETO);
    imprimirCentralizado(F("Segure: * cadastro | 0 data | # gerenc."), 307, 1, PRETO);
  }
}

// Atualiza somente o retangulo do horario principal.
// Evita redesenhar todas as informacoes a cada segundo.
void atualizarRelogioTelaPrincipal(const DateTime& agora) {
  char texto[12];
  tft.fillRect(9, 89, tft.width() - 18, 41, CINZA_CLARO);
  tft.drawRect(8, 88, tft.width() - 16, 43, AZUL_ESCURO);
  snprintf_P(texto, sizeof(texto), PSTR("%02u:%02u:%02u"),
           agora.hour(), agora.minute(), agora.second());
  imprimirCentralizado(texto, 98, 3, AZUL_ESCURO);
}

// ---------------------------------------------------------------------------
// 22 | TELA DE INSERCAO DO NOME
// Desenha a caixa de entrada, contador de caracteres e instrucoes
// para os dois teclados, usando fundo claro e texto escuro.
// ---------------------------------------------------------------------------

// Apresenta campo de nome e mapa de uso dos teclados.
// Ajusta fonte conforme comprimento e informa limite de 20 caracteres.
void desenharTelaCadastroNome() {
  if (modoEdicaoMedicamento) desenharCabecalho(F("EDITAR: NOME"), LARANJA);
  else desenharCabecalho(F("CADASTRO: NOME"), VERDE);

  tft.setTextColor(PRETO);
  tft.setTextSize(1);
  tft.setCursor(10, 42);
  tft.print(F("NOME DO MEDICAMENTO"));

  desenharCaixaPadrao(8, 56, tft.width() - 16, 43, BRANCO, AZUL_ESCURO);
  uint8_t tamNomeTela = (tamanhoNome <= 17) ? 2 : 1;
  imprimirCentralizado(nomeEmEdicao[0] ? nomeEmEdicao : "_", 69, tamNomeTela, AZUL_ESCURO);

  char contador[24];
  snprintf_P(contador, sizeof(contador), PSTR("%u/20 caracteres"), tamanhoNome);
  imprimirCentralizado(contador, 104, 1, CINZA);

  // Painel principal do teclado matricial.
  desenharCaixaPadrao(8, 121, tft.width() - 16, 104, BRANCO, AZUL_ESCURO);
  tft.fillRect(9, 122, tft.width() - 18, 24, CINZA_CLARO);
  tft.drawRect(9, 122, tft.width() - 18, 24, AZUL_ESCURO);
  imprimirCentralizado(F("TECLADO MATRICIAL"), 129, 1, PRETO);

  tft.setTextColor(PRETO);
  tft.setTextSize(1);
  tft.setCursor(15, 155); tft.print(F("2=ABC   3=DEF   4=GHI"));
  tft.setCursor(15, 173); tft.print(F("5=JKL   6=MNO   7=PQRS"));
  tft.setCursor(15, 191); tft.print(F("8=TUV   9=WXYZ  0=ESPACO"));
  tft.setCursor(15, 209); tft.print(F("* CONFIRMA       # APAGA"));

  desenharCaixaPadrao(8, 233, tft.width() - 16, 42, CINZA_CLARO, AZUL_ESCURO);
  tft.setTextColor(PRETO);
  tft.setTextSize(1);
  tft.setCursor(14, 241);
  if (modoEdicaoMedicamento) tft.print(F("1 = LIMPAR NOME"));
  else tft.print(F("Ate 3 s entre toques da mesma tecla"));
  tft.setCursor(14, 258);
  tft.print(F("PS/2: ENTER confirma | BACK apaga"));

  desenharRodapeOuMensagem(F("* confirma | # apaga"),
                            modoEdicaoMedicamento ? F("1 limpa nome | ESC cancela") : F("Cadastro do medicamento"));
}

// ---------------------------------------------------------------------------
// 23 | TELA DE SELECAO DOS DIAS
// Exibe os sete dias com o numero de horarios associados a cada um
// e informa quando o cadastro ja pode ser concluido.
// ---------------------------------------------------------------------------

// Apresenta dias da semana e horarios por dia.
// Sinaliza quando ja existem dados suficientes para salvar.
void desenharTelaCadastroDia() {
  desenharCabecalho(F("CADASTRO: DIA"), VERDE);

  desenharCaixaPadrao(8, 41, tft.width() - 16, 42, CINZA_CLARO, AZUL_ESCURO);
  tft.setTextColor(PRETO); tft.setTextSize(1); tft.setCursor(13, 47); tft.print(F("MEDICAMENTO"));
  uint8_t tam = strlen(medicamentoEmEdicao.nome) <= 17 ? 2 : 1;
  imprimirCentralizado(medicamentoEmEdicao.nome, 62, tam, AZUL_ESCURO);

  imprimirCentralizado(F("ESCOLHA O DIA"), 89, 1, PRETO);

  for (uint8_t d = 0; d < 7; d++) {
    int16_t y = 103 + d * 22;
    desenharCaixaPadrao(10, y, tft.width() - 20, 19,
                        d % 2 ? BRANCO : CINZA_CLARO, AZUL_ESCURO);
    char linha[32];
    snprintf_P(linha, sizeof(linha), PSTR("%u  %-3s       %u horario(s)"),
               d + 1, NOMES_DIAS_CURTOS[d], medicamentoEmEdicao.qtdHorarios[d]);
    tft.setTextColor(PRETO); tft.setTextSize(1); tft.setCursor(17, y + 6); tft.print(linha);
  }

  char total[34];
  snprintf_P(total, sizeof(total), PSTR("TOTAL SEMANAL: %u horario(s)"),
             contarHorariosSemana(medicamentoEmEdicao));
  imprimirCentralizado(total, 262, 1, AZUL_ESCURO);

  if (medicamentoTemHorarios(medicamentoEmEdicao))
    desenharRodapeOuMensagem(F("1-7 escolhe dia | 0/* salva"), F("# / BACK cancela"));
  else
    desenharRodapeOuMensagem(F("1-7 escolhe o dia"), F("# / BACK cancela"));
}

// ---------------------------------------------------------------------------
// 24 | TELA DE INSERCAO DOS HORARIOS
// Mostra os horarios existentes no dia e a digitacao gradual de HH:MM;
// os limites e os comandos de confirmacao aparecem no rodape.
// ---------------------------------------------------------------------------

// Apresenta as doses existentes e o campo HH:MM.
// Monta a lista do dia e orienta a digitacao de quatro numeros.
void desenharTelaCadastroHorario() {
  desenharCabecalho(F("CADASTRO: HORARIO"), VERDE);

  char texto[48];
  desenharCaixaPadrao(8, 41, tft.width() - 16, 53, CINZA_CLARO, AZUL_ESCURO);
  uint8_t tam = strlen(medicamentoEmEdicao.nome) <= 17 ? 2 : 1;
  imprimirCentralizado(medicamentoEmEdicao.nome, 49, tam, PRETO);
  snprintf_P(texto, sizeof(texto), PSTR("DIA: %s"), NOMES_DIAS[diaEmEdicao]);
  imprimirCentralizado(texto, 75, 1, AZUL_ESCURO);

  desenharCaixaPadrao(8, 101, tft.width() - 16, 39, BRANCO, AZUL_ESCURO);
  tft.setTextColor(PRETO); tft.setTextSize(1); tft.setCursor(13, 107); tft.print(F("HORARIOS JA CADASTRADOS"));
  char lista[40] = "";
  if (medicamentoEmEdicao.qtdHorarios[diaEmEdicao] == 0) {
    strncpy_P(lista, PSTR("nenhum"), sizeof(lista) - 1);
    lista[sizeof(lista)-1] = '\0';
  } else {
    for (uint8_t h = 0; h < medicamentoEmEdicao.qtdHorarios[diaEmEdicao]; h++) {
      char item[8];
      snprintf_P(item, sizeof(item), PSTR("%02u:%02u "),
                 medicamentoEmEdicao.horarios[diaEmEdicao][h].hora,
                 medicamentoEmEdicao.horarios[diaEmEdicao][h].minuto);
      strncat(lista, item, sizeof(lista) - strlen(lista) - 1);
    }
  }
  imprimirCentralizado(lista, 124, 1, PRETO);

  imprimirCentralizado(F("DIGITE O HORARIO - HHMM"), 150, 1, PRETO);
  char exibicao[6] = "__:__";
  if (tamanhoHorario > 0) exibicao[0] = horarioEmEdicao[0];
  if (tamanhoHorario > 1) exibicao[1] = horarioEmEdicao[1];
  if (tamanhoHorario > 2) exibicao[3] = horarioEmEdicao[2];
  if (tamanhoHorario > 3) exibicao[4] = horarioEmEdicao[3];

  desenharCaixaPadrao(25, 166, tft.width() - 50, 58, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(exibicao, 182, 3, AZUL_ESCURO);

  desenharCaixaPadrao(8, 233, tft.width() - 16, 42, BRANCO, AZUL_ESCURO);
  imprimirCentralizado(F("HORARIO VALIDO: 00:00 A 23:59"), 241, 1, PRETO);
  imprimirCentralizado(F("MAXIMO: 4 HORARIOS POR DIA"), 258, 1, PRETO);

  desenharRodapeOuMensagem(F("* / ENTER confirma"), F("# / BACK apaga ou volta"));
}

// ---------------------------------------------------------------------------
// 25 | TELA DE CONFIRMACAO DO HORARIO
// Resume o horario inserido e oferece acrescentar outra dose,
// selecionar outro dia ou salvar o medicamento.
// ---------------------------------------------------------------------------

// Confirma a inclusao de um horario na tela.
// Oferece adicionar outra dose, mudar de dia ou salvar.
void desenharTelaCadastroAcao() {
  desenharCabecalho(F("HORARIO ADICIONADO"), VERDE);

  char texto[48];
  desenharCaixaPadrao(8, 42, tft.width() - 16, 70, CINZA_CLARO, AZUL_ESCURO);
  uint8_t tam = strlen(medicamentoEmEdicao.nome) <= 17 ? 2 : 1;
  imprimirCentralizado(medicamentoEmEdicao.nome, 51, tam, PRETO);
  snprintf_P(texto, sizeof(texto), PSTR("%s  %02u:%02u"),
             NOMES_DIAS_CURTOS[diaEmEdicao], ultimaHoraAdicionada, ultimoMinutoAdicionado);
  imprimirCentralizado(texto, 77, 2, AZUL_ESCURO);
  snprintf_P(texto, sizeof(texto), PSTR("Neste dia: %u/4  |  Semana: %u"),
             medicamentoEmEdicao.qtdHorarios[diaEmEdicao], contarHorariosSemana(medicamentoEmEdicao));
  imprimirCentralizado(texto, 100, 1, PRETO);

  desenharCaixaPadrao(10, 126, tft.width() - 20, 42, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(F("1 - OUTRO HORARIO"), 140, 2, PRETO);
  imprimirCentralizado(F("NO MESMO DIA"), 157, 1, PRETO);

  desenharCaixaPadrao(10, 177, tft.width() - 20, 42, BRANCO, AZUL_ESCURO);
  imprimirCentralizado(F("2 - OUTRO DIA"), 191, 2, PRETO);
  imprimirCentralizado(F("Adicionar mais horarios"), 208, 1, PRETO);

  desenharCaixaPadrao(10, 228, tft.width() - 20, 47, CINZA_CLARO, VERDE);
  imprimirCentralizado(F("SALVAR CADASTRO"), 238, 2, PRETO);
  imprimirCentralizado(F("0 / * / ENTER"), 260, 1, PRETO);

  desenharRodapeOuMensagem(F("# / BACK = voltar aos dias"), F("Escolha uma opcao"));
}

// ---------------------------------------------------------------------------
// 26 | TELA DE AJUSTE DO RELOGIO
// Apresenta os 12 digitos no formato de data e hora e atualiza
// o campo de entrada sem redesenhar toda a interface.
// ---------------------------------------------------------------------------

// Exibe progressivamente o formato DD/MM/AAAA HH:MM.
// Insere os digitos nas posicoes corretas de uma mascara visual.
void desenharCampoDataHora() {
  char exibicao[18] = "__/__/____ __:__";
  const uint8_t posicoes[12] = {0, 1, 3, 4, 6, 7, 8, 9, 11, 12, 14, 15};
  for (uint8_t i = 0; i < tamanhoDataHora; i++) exibicao[posicoes[i]] = dataHoraEmEdicao[i];

  desenharCaixaPadrao(12, 110, tft.width() - 24, 56, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(exibicao, 127, 2, AZUL_ESCURO);

  char cont[24];
  snprintf_P(cont, sizeof(cont), PSTR("DIGITOS: %u/12"), tamanhoDataHora);
  imprimirCentralizado(cont, 173, 1, CINZA);
}

// Monta a pagina de ajuste manual do relogio.
// Exibe formato de entrada, exemplo e teclas de confirmacao.
void desenharTelaAjusteRtc() {
  desenharCabecalho(F("AJUSTE DE DATA E HORA"), LARANJA);

  desenharCaixaPadrao(8, 43, tft.width() - 16, 58, BRANCO, AZUL_ESCURO);
  imprimirCentralizado(F("DIGITE 12 NUMEROS"), 51, 2, PRETO);
  imprimirCentralizado(F("DDMMAAAAHHMM"), 75, 2, AZUL_ESCURO);
  imprimirCentralizado(F("Ex.: 150920261730"), 93, 1, PRETO);

  desenharCampoDataHora();

  desenharCaixaPadrao(8, 193, tft.width() - 16, 36, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(F("Exibicao: DD/MM/AAAA HH:MM"), 205, 1, PRETO);

  desenharCaixaPadrao(8, 237, tft.width() - 16, 38, BRANCO, AZUL_ESCURO);
  imprimirCentralizado(F("Upload tambem sincroniza o RTC"), 246, 1, PRETO);
  imprimirCentralizado(F("quando necessario"), 261, 1, PRETO);

  desenharRodapeOuMensagem(F("* / ENTER confirma"), F("# / BACK apaga | ESC sai"));
}

// ---------------------------------------------------------------------------
// 27 | TELAS DE CONSULTA E EDICAO
// Lista medicamentos por paginas e apresenta as etapas de edicao
// de nome, dias e horarios, alem das opcoes de salvar ou cancelar.
// ---------------------------------------------------------------------------

// Transforma os dias com horarios em uma lista curta de texto.
// Concatena abreviacoes sem ultrapassar o tamanho do buffer.
void montarDiasAtivos(const Medicamento& med, char* destino, size_t tamanho) {
  destino[0] = '\0';

  for (uint8_t d = 0; d < 7; d++) {
    if (med.qtdHorarios[d] > 0) {
      if (destino[0] != '\0') {
        size_t usado = strlen(destino);
        if (usado + 1 < tamanho) {
          destino[usado] = ' ';
          destino[usado + 1] = '\0';
        }
      }
      strncat(destino, NOMES_DIAS_CURTOS[d],
              tamanho - strlen(destino) - 1);
    }
  }

  if (destino[0] == '\0') {
    strncpy_P(destino, PSTR("SEM HORARIOS"), tamanho - 1);
    destino[tamanho - 1] = '\0';
  }
}

// Mostra a lista paginada de medicamentos cadastrados.
// Exibe tres registros por pagina, com nome, dias e total de doses.
void desenharTelaGerenciador() {
  desenharCabecalho(F("GERENCIADOR"), AZUL_ESCURO);

  uint8_t totalPaginas = totalPaginasGerenciador();
  if (paginaGerenciador >= totalPaginas) paginaGerenciador = totalPaginas - 1;

  char texto[48];
  snprintf_P(texto, sizeof(texto), PSTR("MEDICAMENTOS: %u/20     PAG. %u/%u"),
             dados.quantidade, paginaGerenciador + 1, totalPaginas);
  imprimirCentralizado(texto, 43, 1, AZUL_ESCURO);

  if (dados.quantidade == 0) {
    desenharCaixaPadrao(12, 91, tft.width() - 24, 82, CINZA_CLARO, AZUL_ESCURO);
    imprimirCentralizado(F("NENHUM MEDICAMENTO"), 111, 2, PRETO);
    imprimirCentralizado(F("CADASTRADO"), 135, 2, PRETO);
    imprimirCentralizado(F("F1 no PS/2 para cadastrar"), 159, 1, PRETO);
  } else {
    uint8_t inicio = paginaGerenciador * ITENS_POR_PAGINA;
    for (uint8_t item = 0; item < ITENS_POR_PAGINA; item++) {
      uint8_t indice = inicio + item;
      if (indice >= dados.quantidade) break;
      const Medicamento& med = dados.medicamentos[indice];
      int16_t y = 62 + item * 66;

      desenharCaixaPadrao(8, y, tft.width() - 16, 58,
                          item % 2 ? BRANCO : CINZA_CLARO, AZUL_ESCURO);
      tft.fillRect(9, y + 1, 28, 56, BRANCO);
      tft.drawRect(9, y + 1, 28, 56, AZUL_ESCURO);
      char numero[4]; snprintf_P(numero, sizeof(numero), PSTR("%u"), item + 1);
      tft.setTextColor(PRETO); tft.setTextSize(2); tft.setCursor(17, y + 20); tft.print(numero);

      tft.setTextColor(PRETO);
      uint8_t tamNome = strlen(med.nome) <= 15 ? 2 : 1;
      tft.setTextSize(tamNome); tft.setCursor(45, y + 7); tft.print(med.nome);

      char dias[34]; montarDiasAtivos(med, dias, sizeof(dias));
      tft.setTextSize(1); tft.setCursor(45, y + 30); tft.print(dias);
      snprintf_P(texto, sizeof(texto), PSTR("%u horario(s)/semana"), contarHorariosSemana(med));
      tft.setCursor(45, y + 44); tft.print(texto);
    }
  }

  desenharRodapeOuMensagem(F("1/2/3 seleciona | 0 prox. pagina"),
                            F("# anterior | * / ENTER sai"));
}

// Apresenta um medicamento selecionado.
// Exibe resumo dos dias e as opcoes de editar e excluir.
void desenharTelaGerenciadorItem() {
  desenharCabecalho(F("MEDICAMENTO"), AZUL_ESCURO);
  if (indiceMedicamentoSelecionado < 0 || indiceMedicamentoSelecionado >= dados.quantidade) {
    voltarAoGerenciador(); return;
  }
  const Medicamento& med = dados.medicamentos[indiceMedicamentoSelecionado];

  desenharCaixaPadrao(8, 44, tft.width() - 16, 76, CINZA_CLARO, AZUL_ESCURO);
  uint8_t tam = strlen(med.nome) <= 17 ? 2 : 1;
  imprimirCentralizado(med.nome, 55, tam, PRETO);
  char dias[40]; montarDiasAtivos(med, dias, sizeof(dias));
  imprimirCentralizado(dias, 83, 1, AZUL_ESCURO);
  char texto[42];
  snprintf_P(texto, sizeof(texto), PSTR("TOTAL: %u horario(s)/semana"), contarHorariosSemana(med));
  imprimirCentralizado(texto, 101, 1, PRETO);

  desenharCaixaPadrao(12, 139, tft.width() - 24, 52, CINZA_CLARO, VERDE);
  imprimirCentralizado(F("1 - EDITAR"), 156, 2, PRETO);
  desenharCaixaPadrao(12, 205, tft.width() - 24, 52, BRANCO, VERMELHO);
  imprimirCentralizado(F("2 - EXCLUIR"), 222, 2, PRETO);

  desenharRodapePadrao(F("# / * / BACK / ENTER"), F("VOLTAR AO GERENCIADOR"));
}

// Apresenta o menu de alteracoes ainda nao salvas.
// Mostra opcoes para nome, dias/horarios e confirmacao final.
void desenharTelaEditarMenu() {
  desenharCabecalho(F("EDITAR MEDICAMENTO"), LARANJA);

  desenharCaixaPadrao(8, 42, tft.width() - 16, 72, CINZA_CLARO, AZUL_ESCURO);
  uint8_t tam = strlen(medicamentoEmEdicao.nome) <= 17 ? 2 : 1;
  imprimirCentralizado(medicamentoEmEdicao.nome, 50, tam, PRETO);
  char dias[40]; montarDiasAtivos(medicamentoEmEdicao, dias, sizeof(dias));
  imprimirCentralizado(dias, 77, 1, AZUL_ESCURO);
  char texto[40];
  snprintf_P(texto, sizeof(texto), PSTR("TOTAL: %u horario(s)/semana"), contarHorariosSemana(medicamentoEmEdicao));
  imprimirCentralizado(texto, 96, 1, PRETO);

  desenharCaixaPadrao(10, 126, tft.width() - 20, 42, BRANCO, AZUL_ESCURO);
  imprimirCentralizado(F("1 - EDITAR NOME"), 140, 2, PRETO);

  desenharCaixaPadrao(10, 178, tft.width() - 20, 42, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(F("2 - DIAS/HORARIOS"), 192, 2, PRETO);

  desenharCaixaPadrao(10, 230, tft.width() - 20, 45, CINZA_CLARO, VERDE);
  imprimirCentralizado(F("SALVAR ALTERACOES"), 240, 2, PRETO);
  imprimirCentralizado(F("0 / * / ENTER"), 262, 1, PRETO);

  desenharRodapeOuMensagem(F("# / BACK = CANCELAR EDICAO"), F("Nada grava ate voce SALVAR"));
}

// Lista os sete dias para a edicao da programacao.
// Mostra quantos horarios existem em cada dia.
void desenharTelaEditarDias() {
  desenharCabecalho(F("EDITAR: DIAS"), LARANJA);

  desenharCaixaPadrao(8, 41, tft.width() - 16, 39, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(medicamentoEmEdicao.nome, 49,
                       strlen(medicamentoEmEdicao.nome) <= 17 ? 2 : 1, PRETO);
  imprimirCentralizado(F("ESCOLHA O DIA PARA ALTERAR"), 86, 1, PRETO);

  for (uint8_t d = 0; d < 7; d++) {
    int16_t y = 101 + d * 23;
    desenharCaixaPadrao(10, y, tft.width() - 20, 20,
                        d % 2 ? BRANCO : CINZA_CLARO, AZUL_ESCURO);
    char linha[34];
    snprintf_P(linha, sizeof(linha), PSTR("%u  %-3s       %u horario(s)"),
               d + 1, NOMES_DIAS_CURTOS[d], medicamentoEmEdicao.qtdHorarios[d]);
    tft.setTextColor(PRETO); tft.setTextSize(1); tft.setCursor(17, y + 6); tft.print(linha);
  }

  desenharRodapeOuMensagem(F("1-7 abre o dia"), F("0 / * / # / BACK = voltar"));
}

// Mostra ate quatro horarios do dia em edicao.
// Permite selecionar uma dose existente ou criar uma nova.
void desenharTelaEditarDiaHorarios() {
  desenharCabecalho(F("EDITAR: HORARIOS"), LARANJA);

  char texto[46];
  desenharCaixaPadrao(8, 41, tft.width() - 16, 49, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(medicamentoEmEdicao.nome, 48,
                       strlen(medicamentoEmEdicao.nome) <= 17 ? 2 : 1, PRETO);
  snprintf_P(texto, sizeof(texto), PSTR("DIA: %s"), NOMES_DIAS[diaEmEdicao]);
  imprimirCentralizado(texto, 72, 1, AZUL_ESCURO);

  uint8_t qtd = medicamentoEmEdicao.qtdHorarios[diaEmEdicao];
  imprimirCentralizado(F("HORARIOS CADASTRADOS"), 97, 1, PRETO);

  if (qtd == 0) {
    desenharCaixaPadrao(18, 115, tft.width() - 36, 51, BRANCO, AZUL_ESCURO);
    imprimirCentralizado(F("NENHUM HORARIO"), 126, 2, CINZA);
    imprimirCentralizado(F("neste dia"), 151, 1, CINZA);
  } else {
    for (uint8_t h = 0; h < qtd; h++) {
      int16_t y = 113 + h * 31;
      desenharCaixaPadrao(25, y, tft.width() - 50, 27,
                          h % 2 ? BRANCO : CINZA_CLARO, AZUL_ESCURO);
      snprintf_P(texto, sizeof(texto), PSTR("%u - %02u:%02u"), h + 1,
                 medicamentoEmEdicao.horarios[diaEmEdicao][h].hora,
                 medicamentoEmEdicao.horarios[diaEmEdicao][h].minuto);
      imprimirCentralizado(texto, y + 7, 2, PRETO);
    }
  }

  desenharCaixaPadrao(10, 244, tft.width() - 20, 32, CINZA_CLARO, VERDE);
  imprimirCentralizado(F("5 - NOVO HORARIO"), 253, 2, PRETO);

  desenharRodapeOuMensagem(F("1-4 edita horario | 5 adiciona"), F("0 / # / * / BACK = voltar"));
}

// Apresenta a escolha do novo dia de uma dose.
// Exibe o dia original e opcao para remover o horario.
void desenharTelaEditarHorarioDestino() {
  desenharCabecalho(F("EDITAR: DIA DO HORARIO"), LARANJA);

  if (indiceHorarioOriginal < 0 || diaHorarioOriginal > 6 ||
      indiceHorarioOriginal >= medicamentoEmEdicao.qtdHorarios[diaHorarioOriginal]) {
    telaAtual = TELA_EDITAR_DIA_HORARIOS; redesenharTela = true; return;
  }

  const HorarioDose& hor = medicamentoEmEdicao.horarios[diaHorarioOriginal][indiceHorarioOriginal];
  char texto[48];
  desenharCaixaPadrao(8, 42, tft.width() - 16, 48, CINZA_CLARO, AZUL_ESCURO);
  snprintf_P(texto, sizeof(texto), PSTR("ATUAL: %s  %02u:%02u"),
             NOMES_DIAS_CURTOS[diaHorarioOriginal], hor.hora, hor.minuto);
  imprimirCentralizado(texto, 56, 2, AZUL_ESCURO);
  imprimirCentralizado(F("ESCOLHA O NOVO DIA"), 96, 1, PRETO);

  for (uint8_t d = 0; d < 7; d++) {
    int16_t y = 111 + d * 19;
    desenharCaixaPadrao(16, y, tft.width() - 32, 17,
                        d == diaHorarioOriginal ? AMARELO : (d % 2 ? BRANCO : CINZA_CLARO),
                        AZUL_ESCURO);
    snprintf_P(texto, sizeof(texto), PSTR("%u - %s"), d + 1, NOMES_DIAS[d]);
    tft.setTextColor(PRETO); tft.setTextSize(1); tft.setCursor(25, y + 5); tft.print(texto);
  }

  desenharCaixaPadrao(16, 250, tft.width() - 32, 27, BRANCO, VERMELHO);
  imprimirCentralizado(F("0 - EXCLUIR HORARIO"), 258, 1, PRETO);

  desenharRodapeOuMensagem(F("1-7 escolhe o novo dia"), F("# / * / BACK = voltar"));
}

// Mostra o campo HH:MM de uma dose editada ou nova.
// Inclui dados do horario anterior quando se trata de alteracao.
void desenharTelaEditarHorarioValor() {
  if (indiceHorarioOriginal >= 0) desenharCabecalho(F("EDITAR: NOVO HORARIO"), LARANJA);
  else desenharCabecalho(F("ADICIONAR HORARIO"), LARANJA);

  char texto[46];
  desenharCaixaPadrao(8, 42, tft.width() - 16, 61, CINZA_CLARO, AZUL_ESCURO);
  if (indiceHorarioOriginal >= 0 && diaHorarioOriginal <= 6 &&
      indiceHorarioOriginal < medicamentoEmEdicao.qtdHorarios[diaHorarioOriginal]) {
    const HorarioDose& antigo = medicamentoEmEdicao.horarios[diaHorarioOriginal][indiceHorarioOriginal];
    snprintf_P(texto, sizeof(texto), PSTR("ANTES: %s %02u:%02u"),
               NOMES_DIAS_CURTOS[diaHorarioOriginal], antigo.hora, antigo.minuto);
    imprimirCentralizado(texto, 51, 1, PRETO);
  } else {
    imprimirCentralizado(F("NOVO HORARIO"), 51, 1, PRETO);
  }
  snprintf_P(texto, sizeof(texto), PSTR("NOVO DIA: %s"), NOMES_DIAS[diaHorarioDestino]);
  imprimirCentralizado(texto, 75, 1, AZUL_ESCURO);

  imprimirCentralizado(F("DIGITE HHMM"), 112, 1, PRETO);
  char exibicao[6] = "__:__";
  if (tamanhoHorario > 0) exibicao[0] = horarioEmEdicao[0];
  if (tamanhoHorario > 1) exibicao[1] = horarioEmEdicao[1];
  if (tamanhoHorario > 2) exibicao[3] = horarioEmEdicao[2];
  if (tamanhoHorario > 3) exibicao[4] = horarioEmEdicao[3];

  desenharCaixaPadrao(25, 130, tft.width() - 50, 62, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(exibicao, 148, 3, AZUL_ESCURO);

  desenharCaixaPadrao(8, 205, tft.width() - 16, 69, BRANCO, AZUL_ESCURO);
  imprimirCentralizado(F("HORARIO VALIDO: 00:00 A 23:59"), 215, 1, PRETO);
  imprimirCentralizado(F("* / ENTER = CONFIRMAR"), 236, 1, PRETO);
  imprimirCentralizado(F("# / BACK = APAGAR"), 256, 1, PRETO);

  desenharRodapeOuMensagem(F("Digite os 4 numeros do horario"), F("# vazio volta para a tela anterior"));
}

// ---------------------------------------------------------------------------
// 28 | TELAS DE CONFIRMACAO DE EXCLUSAO
// Mostra o registro selecionado e exige confirmacao explicita
// para excluir um medicamento ou limpar todos os cadastros.
// ---------------------------------------------------------------------------

// Apresenta as consequencias da exclusao de um cadastro.
// Exibe nome e opcoes explicitas de confirmar ou cancelar.
void desenharConfirmacaoExclusao() {
  desenharCabecalho(F("CONFIRMAR EXCLUSAO"), VERMELHO);
  if (indiceExclusaoPendente < 0 || indiceExclusaoPendente >= dados.quantidade) {
    voltarAoGerenciador(); return;
  }
  const Medicamento& med = dados.medicamentos[indiceExclusaoPendente];

  desenharCaixaPadrao(8, 46, tft.width() - 16, 94, CINZA_CLARO, VERMELHO);
  imprimirCentralizado(F("APAGAR MEDICAMENTO?"), 56, 2, VERMELHO);
  imprimirCentralizado(med.nome, 84, strlen(med.nome) <= 17 ? 2 : 1, PRETO);
  char texto[40];
  snprintf_P(texto, sizeof(texto), PSTR("%u horario(s) por semana"), contarHorariosSemana(med));
  imprimirCentralizado(texto, 113, 1, PRETO);

  desenharCaixaPadrao(12, 159, tft.width() - 24, 54, BRANCO, VERMELHO);
  imprimirCentralizado(F("CONFIRMAR EXCLUSAO"), 172, 2, PRETO);
  imprimirCentralizado(F("* / ENTER"), 197, 1, PRETO);

  desenharCaixaPadrao(12, 225, tft.width() - 24, 48, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(F("CANCELAR"), 236, 2, PRETO);
  imprimirCentralizado(F("# / BACK / ESC"), 258, 1, PRETO);

  desenharRodapePadrao(F("ATENCAO: esta acao apaga o cadastro"), F("Use CANCELAR para manter os dados"));
}

// Apresenta confirmacao da remocao de todos os registros.
// Mostra quantidade atual e comandos de seguranca.
void desenharConfirmacaoLimparTudo() {
  desenharCabecalho(F("APAGAR TODOS"), VERMELHO);

  desenharCaixaPadrao(8, 47, tft.width() - 16, 96, CINZA_CLARO, VERMELHO);
  imprimirCentralizado(F("ATENCAO"), 58, 2, VERMELHO);
  imprimirCentralizado(F("TODOS OS MEDICAMENTOS"), 84, 1, PRETO);
  imprimirCentralizado(F("SERAO APAGADOS"), 103, 2, PRETO);
  char texto[38];
  snprintf_P(texto, sizeof(texto), PSTR("Quantidade atual: %u"), dados.quantidade);
  imprimirCentralizado(texto, 128, 1, PRETO);

  desenharCaixaPadrao(12, 158, tft.width() - 24, 54, BRANCO, VERMELHO);
  imprimirCentralizado(F("APAGAR TODOS"), 172, 2, PRETO);
  imprimirCentralizado(F("* / ENTER"), 197, 1, PRETO);

  desenharCaixaPadrao(12, 225, tft.width() - 24, 48, CINZA_CLARO, AZUL_ESCURO);
  imprimirCentralizado(F("CANCELAR"), 236, 2, PRETO);
  imprimirCentralizado(F("# / BACK / ESC"), 258, 1, PRETO);

  desenharRodapePadrao(F("Esta acao nao pode ser desfeita"), F("Confirme somente se tiver certeza"));
}

// ---------------------------------------------------------------------------
// 29 | TELAS DOS ALERTAS
// Exibe os medicamentos do horario acionado e, quando aplicavel,
// a mensagem de reabastecimento com confirmacao obrigatoria.
// ---------------------------------------------------------------------------

// Exibe os medicamentos que possuem dose no minuto do alarme.
// Mostra nomes correspondentes e tecla de confirmacao.
void desenharAlarmeMedicamento() {
  tft.fillScreen(BRANCO);
  tft.fillRect(0, 0, tft.width(), 42, CINZA_CLARO);
  tft.drawRect(0, 0, tft.width(), 42, VERMELHO);
  imprimirCentralizado(F("HORA DO MEDICAMENTO"), 13, 2, PRETO);

  char texto[48];
  snprintf_P(texto, sizeof(texto), PSTR("%s  %02u:%02u"),
             NOMES_DIAS_CURTOS[diaAlarme], horaAlarme, minutoAlarme);
  imprimirCentralizado(texto, 54, 2, PRETO);

  desenharCaixaPadrao(8, 83, tft.width() - 16, 154, BRANCO, VERMELHO);
  imprimirCentralizado(F("TOMAR AGORA"), 92, 1, VERMELHO);
  uint8_t linha = 0;
  for (uint8_t i = 0; i < dados.quantidade && linha < 6; i++) {
    const Medicamento& med = dados.medicamentos[i];
    if (medicamentoPossuiHorario(med, diaAlarme, horaAlarme, minutoAlarme)) {
      tft.setTextColor(PRETO); tft.setTextSize(2); tft.setCursor(16, 114 + linha * 20);
      tft.print(F("- "));
      char nomeCurto[18]; strncpy(nomeCurto, med.nome, 17); nomeCurto[17] = '\0';
      tft.print(nomeCurto); linha++;
    }
  }

  desenharCaixaPadrao(12, 251, tft.width() - 24, 56, CINZA_CLARO, VERDE);
  imprimirCentralizado(F("CONFIRMAR"), 262, 2, PRETO);
  imprimirCentralizado(F("* matricial / ENTER PS2"), 288, 1, PRETO);
}

// Exibe o aviso prioritario de reabastecimento semanal.
// Mantem mensagem de reposicao ate a confirmacao.
void desenharAlertaReabastecimento() {
  tft.fillScreen(BRANCO);
  tft.fillRect(0, 0, tft.width(), 58, CINZA_CLARO);
  tft.drawRect(0, 0, tft.width(), 58, VERMELHO);
  imprimirCentralizado(F("REABASTECER"), 17, 3, PRETO);

  imprimirCentralizado(F("ULTIMA DOSE DA SEMANA"), 77, 1, PRETO);
  imprimirCentralizado(F("FINALIZADA"), 98, 2, PRETO);

  desenharCaixaPadrao(10, 129, tft.width() - 20, 79, BRANCO, PRETO);
  imprimirCentralizado(F("REPONHA OS MEDICAMENTOS"), 145, 1, PRETO);
  imprimirCentralizado(F("PARA A PROXIMA SEMANA"), 166, 1, PRETO);
  imprimirCentralizado(F("ANTES DE CONFIRMAR"), 188, 1, VERMELHO);

  desenharCaixaPadrao(10, 226, tft.width() - 20, 72, CINZA_CLARO, VERDE);
  imprimirCentralizado(F("CONFIRMAR"), 238, 2, PRETO);
  imprimirCentralizado(F("ENTER no PS/2"), 265, 1, PRETO);
  imprimirCentralizado(F("ou * no matricial"), 282, 1, PRETO);
}

// ---------------------------------------------------------------------------
// 30 | MENSAGENS TEMPORARIAS
// Armazena avisos curtos com prazo calculado por millis(); ao expirar,
// limpa a mensagem e solicita a atualizacao da tela.
// ---------------------------------------------------------------------------

// Registra mensagem recebida como texto comum em SRAM.
// Define o instante em que o aviso temporario sera removido.
void mostrarMensagem(const char* texto, unsigned long duracaoMs) {
  strncpy(mensagemStatus, texto, sizeof(mensagemStatus) - 1);
  mensagemStatus[sizeof(mensagemStatus) - 1] = '\0';
  fimMensagemStatus = millis() + duracaoMs;
  redesenharTela = true;
}

// Sobrecarga para texto constante armazenado na memoria Flash (F()).
// Copia-o para o buffer de status antes de programar sua expiracao.
void mostrarMensagem(const __FlashStringHelper* texto,
                     unsigned long duracaoMs) {
  strncpy_P(mensagemStatus, (PGM_P)texto, sizeof(mensagemStatus) - 1);
  mensagemStatus[sizeof(mensagemStatus) - 1] = '\0';
  fimMensagemStatus = millis() + duracaoMs;
  redesenharTela = true;
}

// Remove mensagens cujo prazo ja se encerrou.
// Compara millis() ao vencimento e solicita novo desenho.
void limparMensagemExpirada() {
  if (mensagemStatus[0] != '\0' &&
      (long)(millis() - fimMensagemStatus) >= 0) {
    mensagemStatus[0] = '\0';
    fimMensagemStatus = 0;
    redesenharTela = true;
  }
}
