# VivencialObjetos3D — Selecionando e aplicando transformações em objetos 3D.

Disciplina: Computação Gráfica.

## Grupo

Nome Completo: Adriano Fantinelli

## Descrição

Exibe três modelos 3D (Cubo, Suzanne e Suzanne Subdividida) em uma cena OpenGL.
O usuário pode selecionar individualmente cada objeto e aplicar transformações
geométricas (rotação, translação e escala) em tempo real via teclado.

Cada objeto possui estado independente, então selecionar outro objeto não interrompe
a rotação ou a posição do anterior.

## Como compilar e executar

```bash
# Na raiz do repositório
cd build
cmake --build .
./VivencialObjetos3D.exe
```

## Controles

### Seleção de objeto

| Tecla | Ação |
|-------|------|
| `Tab` | Seleciona o próximo objeto na lista (objeto ativo fica mais brilhante). |

### Troca de modo

| Tecla | Modo ativado |
|-------|-------------|
| `R` | **Rotação** |
| `T` | **Translação** |
| `S` | **Escala** |

### Modo Rotação

Pressione para alternar rotação contínua no eixo (pressione novamente para parar):

| Tecla | Eixo |
|-------|------|
| `X` | Eixo X |
| `Y` | Eixo Y |
| `Z` | Eixo Z |

### Modo Translação

Mantenha pressionado para mover o objeto selecionado:

| Tecla | Movimento |
|-------|-----------|
| `↑` | +Y (cima) |
| `↓` | −Y (baixo) |
| `→` | +X (direita) |
| `←` | −X (esquerda) |
| `I` | −Z (frente) |
| `J` | +Z (trás) |

### Modo Escala

Mantenha pressionado (escala uniforme, limitada entre 0.1× e 5.0×):

| Tecla | Efeito |
|-------|--------|
| `+` | Aumenta escala |
| `-` | Diminui escala |

### Geral

| Tecla | Ação |
|-------|------|
| `Esc` | Fecha a janela |

---