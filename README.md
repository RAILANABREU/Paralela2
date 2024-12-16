# Relatório de Funcionamento dos Códigos

---

## Introdução

Os dois códigos apresentados implementam um sistema cliente-servidor para transferência de arquivos. 

- O **servidor** aguarda conexões de clientes, aceita o envio de arquivos e salva os dados recebidos no disco. 
- O **cliente** conecta-se ao servidor, envia o nome do arquivo e seus dados, permitindo retomadas de transferências interrompidas.

A seguir, descreve-se o funcionamento detalhado de cada código.

---
## 1. Compilar o Código

### **Servidor**
Use o seguinte comando para compilar o código do servidor:

```bash
gcc Servidor.c -o serv
```

### **Cliente**
Use o seguinte comando para compilar o código do cliente:

```bash
gcc Cliente.c -o client
```

---

## 1.2 Executar o Servidor

### **Passo 1: Iniciar o Servidor**
Execute o servidor no terminal:

```bash
./serv
```

**Saída esperada no terminal do servidor:**
```plaintext
Servidor aguardando conexões na porta 9000...
```

---

## 1.3 Executar o Cliente para Transferência de Arquivos

### **Passo 1: Executar o Cliente**
Use o comando abaixo para enviar um arquivo ao servidor:

```bash
./client <ip_do_servidor> <arquivo>
```

Por exemplo, se o servidor estiver rodando no mesmo dispositivo (localhost) e o arquivo for `BigBig.txt`:

```bash
./client 127.0.0.1 teste.txt
```

---

## 1.4 Exemplos de Execução

### **Exemplo: Erro ao Conectar**
Se o servidor não estiver rodando, o cliente exibirá a seguinte mensagem de erro:

```bash
./client 127.0.0.1 teste.txt
Erro na conexão: Connection refused
```

---

### **Exemplo: Envio Bem-Sucedido**
Com o servidor rodando, o cliente exibirá:

```bash
./client 127.0.0.1 teste.txt
Conexão com o servidor 127.0.0.1 realizada com sucesso.
Iniciando envio do arquivo...
[========================>                         ] 48.03%
```

Se a transferência for interrompida com `Ctrl+C` e o cliente for reiniciado, ele retomará do ponto onde parou.

---

## 1.5 Logs do Servidor

### **Servidor Aceitando Conexões**
Quando o cliente se conecta, o servidor exibe:

```plaintext
Servidor aguardando conexões na porta 9000...
Conexão aceita de 127.0.0.1:59086
Recebendo arquivo: teste.txt
```

### **Servidor Salvando o Arquivo**
Ao finalizar o envio, o servidor exibirá:

```plaintext
Arquivo salvo com sucesso em: /"HOME"/Downloads/teste.txt
```

---

## 1.6 Comportamento em Caso de Desconexão

- Se o cliente perder a conexão, ele encera o processo Cliente.
- Caso o serividor volte, devemos iniciar o cliente novamente e ele retoma o envio do ponto onde parou.

## 2. Código do Servidor

### 2.1. Propósito

O servidor:
- Escuta conexões na porta especificada.
- Aceita conexões de múltiplos clientes, criando um processo separado para cada um.
- Recebe o nome e o conteúdo de arquivos enviados pelos clientes.
- Salva os arquivos recebidos no diretório `Downloads` do usuário.
- Garante continuidade no envio de arquivos usando offsets.

---

### 2.2. Funcionamento

1. **Criação do Socket**
   ```c
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   ```
   - Configura um socket TCP para comunicação.

2. **Configuração do Endereço**
   ```c
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(PORTA);
   ```
   - O servidor escuta conexões em qualquer endereço local (`INADDR_ANY`) na porta `9000`.

3. **Vinculação e Escuta**
   ```c
   bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
   listen(sockfd, 5);
   ```
   - O socket é vinculado ao endereço configurado, permitindo até 5 conexões pendentes.

4. **Aceitação de Conexões**
   ```c
   newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
   ```
   - Aceita conexões de clientes, retornando um descritor de socket para comunicação com o cliente.

5. **Criação de Processo Filho**
   ```c
   pid = fork();
   ```
   - Um novo processo é criado para tratar o cliente. O processo pai continua a aceitar novas conexões.

6. **Recepção de Dados**
   - O servidor lê:
     - **Nome do Arquivo**: Recebido inicialmente.
     - **Conteúdo do Arquivo**: Recebido em chunks de tamanho fixo (`128 bytes`).

7. **Offset e Continuidade**
   ```c
   fseeko(f, 0, SEEK_END);
   off_t offset = ftello(f);
   ```
   - O offset é enviado ao cliente para que a transmissão continue de onde parou.

8. **Salvamento**
   ```c
   snprintf(final_path, sizeof(final_path), "%s/Downloads/%s", home_dir, filename);
   ```
   - O arquivo é salvo no diretório `Downloads` do usuário.

9. **Logs Adicionados**
   - Mensagens para indicar:
     - Conexão aceita.
     - Nome do arquivo recebido.
     - Local onde o arquivo foi salvo.

---

## 3. Código do Cliente

### 3.1. Propósito

O cliente:
- Conecta-se ao servidor especificado.
- Envia o nome e o conteúdo de um arquivo local.
- Retoma o envio a partir de um offset indicado pelo servidor.

---

### 3.2. Funcionamento

1. **Conexão ao Servidor**
   ```c
   connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
   ```
   - Conecta-se ao servidor na porta `9000` e IP fornecido como argumento.

2. **Envio do Nome do Arquivo**
   ```c
   robust_write(sockfd, filename, fname_len);
   ```
   - O cliente envia o nome do arquivo ao servidor.

3. **Recepção do Offset**
   ```c
   robust_read(sockfd, &offset_servidor, sizeof(offset_servidor));
   ```
   - O cliente recebe o offset do servidor, indicando o ponto de continuação.

4. **Envio do Arquivo**
   ```c
   while ((r = fread(buffer, 1, BUFSZ, f)) > 0)
   {
       robust_write(sockfd, buffer, r);
   }
   ```
   - O cliente lê o arquivo em chunks e envia ao servidor.

5. **Logs Adicionados**
   - Mensagens para indicar:
     - Conexão bem-sucedida.
     - Progresso da transferência com barra de progresso.
     - Conclusão da transferência.

---

## 4. Características Principais

1. **Confiabilidade na Transmissão**
   - As funções `robust_read` e `robust_write` garantem leitura e escrita completas, mesmo que sinais ou interrupções ocorram.

2. **Continuidade**
   - O servidor informa ao cliente o offset para reiniciar a transmissão de onde parou, evitando reenvio desnecessário.

3. **Mensagens de Log**
   - Ambos os programas fornecem mensagens claras para facilitar o monitoramento das operações.

---

## 5. Casos de Uso

1. **Servidor**
   - Hospedar o servidor e aceitar conexões de múltiplos clientes para transferências simultâneas.

2. **Cliente**
   - Enviar arquivos para o servidor especificado.
   - Permitir retomada de transferências em caso de interrupção.

---

## 6. Conclusão

Os códigos apresentados fornecem uma base funcional para transferências de arquivos em um ambiente cliente-servidor. Eles demonstram conceitos importantes, como manipulação de sockets, escrita robusta e tratamento de processos paralelos. Com pequenos ajustes, podem ser facilmente adaptados para aplicações práticas em redes confiáveis.

Durante os testes, utilizamos um arquivo chamado `BigBig.txt` com 580 MB de dados. O sistema foi capaz de criar o arquivo `.part` diretamente no diretório `Downloads` da máquina enquanto a transferência estava em andamento. Também implementamos uma barra de progresso para acompanhar visualmente o envio dos dados.

Um ponto interessante observado foi a capacidade do sistema de retomar a transferência do ponto exato onde foi interrompida. Ao interromper o servidor durante a transferência, o cliente parou de enviar os dados. Quando o servidor foi reiniciado e o cliente foi reexecutado, a transferência continuou do ponto onde havia parado, graças ao uso de offsets. 

Embora tenhamos considerado implementar um timeout no cliente para aguardar automaticamente o retorno do servidor, acabamos não seguindo adiante devido à complexidade que isso acrescentaria ao projeto, considerando nosso nível de experiência. Atualmente, o cliente precisa ser reiniciado manualmente após o servidor voltar, mas o sistema funciona bem dentro dessa limitação.

Com isso, conseguimos validar que o sistema é robusto, confiável e atende ao propósito proposto, mesmo em cenários de interrupção da transferência. Há espaço para melhorias, mas o projeto cumpre seu objetivo de maneira funcional e clara.
