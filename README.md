# SpeedyCoders Contest Announcer

Divulgação de contests de **Codeforces e AtCoder**, em **C++20**, pelo Discord Webhook. Executado no GitHub Actions a cada **3 horas**, sem depender de um computador ligado.

## Ativar

1. No Discord, crie um webhook para o canal desejado em **Configurações do servidor → Integrações → Webhooks** e copie a URL padrão (sem `/github`).
2. Neste repositório, acesse **Settings → Secrets and variables → Actions → New repository secret**. Nome: `DISCORD_WEBHOOK_URL`; valor: a URL copiada. Nunca coloque essa URL em arquivos, issues ou logs.
3. Em **Actions → Contest announcer → Run workflow**, execute primeiro com `dry_run` marcado para conferir as mensagens nos logs. Depois desmarque para publicar.
4. O agendamento é `17 */3 * * *`, em UTC: 00:17, 03:17, …, 21:17. No horário de Brasília (UTC−3), corresponde ao mesmo conjunto de horários a cada três horas.

O workflow precisa de permissão de escrita em `contents` para salvar `data/state.json` na branch padrão. Se regras de proteção impedirem os commits do Actions, adapte essas regras para permitir a persistência antes de ativar os envios. Workflows agendados executam na branch padrão e podem atrasar ou ser descartados; em repositórios públicos, podem ser desativados após 60 dias sem atividade. Consulte a [documentação do GitHub](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#schedule).

## Mensagens e classificação

Cada mensagem contém nome, plataforma, **tipo/divisão**, início, tempo restante, duração e link. Os timestamps do Discord usam o fuso horário de quem lê. Menções automáticas, inclusive `@everyone`, ficam desativadas.

- Codeforces: Div. 1/2/3/4, divisões combinadas, Educational e Global Round; demais: `Outro`.
- AtCoder: ABC, ARC (inclui ARC++/ARC--), AGC, AHC e AWC, identificados pelo ID oficial; demais: `Outro`.
- Não há rótulos de dificuldade, recomendações de rating ou estimativas baseadas nos problemas.

Fontes: [Codeforces contest.list](https://codeforces.com/apiHelp/methods#contest.list), sem Gym, apenas fase `BEFORE`; [AtCoder](https://atcoder.jp/contests/?lang=en), tabelas de próximos contests e contests diários. Contests já iniciados não geram mensagens. O HTML do AtCoder é lido com libxml2; mudanças incompatíveis geram falha visível. Uma plataforma indisponível não impede a consulta à outra.

## Lembretes com consultas a cada 3 horas

Não é possível garantir um envio exatamente 1h antes consultando apenas a cada 3h. Para evitar perder essa janela em condições normais, o programa olha uma execução à frente:

| Evento | Quando pode enviar |
|---|---|
| Novo contest | Primeira vez que encontra um contest futuro |
| Lembrete de 24h | Quando faltam até 27h e mais de 4h |
| Lembrete de 1h | Quando faltam até 4h e mais de 0h |

Normalmente o aviso de 24h chega entre 24 e 27h antes; o aviso de 1h entre 1 e 4h antes. As janelas mais amplas permitem recuperação após atrasos. Com menos de 4h, o aviso de 24h é descartado para não ficar obsoleto. A mensagem identifica a janela de consulta e mostra **o tempo real restante**. Se a próxima execução só ocorrer após o início, o lembrete é perdido. Para precisão maior, seria necessário aumentar a frequência.

Na primeira execução, todos os contests futuros encontrados recebem aviso de novo contest; os que já estiverem numa janela recebem também o respectivo lembrete. O mesmo vale para contests descobertos tardiamente. Uma mudança no horário de início cria uma nova sequência de avisos.

## Estado e falhas

`data/state.json` registra cada tipo de mensagem por plataforma, ID e horário de início. O arquivo é atualizado atomicamente após cada envio confirmado com `wait=true` pelo [Discord](https://docs.discord.com/developers/resources/webhook#execute-webhook). Uma simulação não modifica o estado nem acessa o webhook. Não apague o estado: isso causa novos anúncios.

O workflow serializa execuções, persiste os envios já confirmados mesmo após uma falha parcial e guarda uma cópia do estado como artefato quando o job falha. Estado ausente ou corrompido interrompe a execução, sem reinicialização silenciosa. Os registros são mantidos, inclusive os antigos, para preservar o histórico de deduplicação.

HTTP 429 respeita `retry_after` com tentativas limitadas. Falhas de transporte e outros erros de POST não são repetidos imediatamente porque o Discord pode ter recebido a mensagem. Não há garantia transacional de envio exatamente uma vez: uma interrupção entre a aceitação pelo Discord e o commit do estado pode duplicar um aviso na execução seguinte. Se o push falhar, recupere `data/state.json` do artefato antes de executar novamente. Os logs não imprimem a URL secreta nem o corpo de erros do Discord.

## Compilar e testar localmente (Debian/Ubuntu)

```sh
sudo apt-get update
sudo apt-get install -y g++ cmake libcurl4-openssl-dev libxml2-dev nlohmann-json3-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/contest-announcer --dry-run
```

Execute a partir da raiz do repositório. Para envio real, disponibilize `DISCORD_WEBHOOK_URL` no ambiente e execute `./build/contest-announcer`. Não execute simultaneamente com o workflow: a exclusão mútua do GitHub cobre apenas as execuções do Actions.

Os testes são offline e cobrem classificação, parsing, fuso japonês, limites das janelas, deduplicação, reagendamento, estado inválido e falha parcial de envio. O CI executa compilação e testes em alterações de código, sem webhook. O programa usa libcurl, nlohmann/json e libxml2 fornecidos pelos pacotes do sistema; não há serviço residente nem comandos slash de Discord.
