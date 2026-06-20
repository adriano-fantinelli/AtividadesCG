# GrauB - Pipeline Grafico Completo

**Autor:** Adriano Fantinelli

**Disciplina:** Computação Gráfica

**Data:** Junho de 2026

Visualizador interativo 3D que integra todos os componentes do pipeline gráfico
programável estudados no semestre em uma única aplicacao OpenGL/C++.

---

## Setup

### Dependências

| Biblioteca   | Versão       | Obtenção                                        |
|--------------|--------------|-------------------------------------------------|
| GLFW         | 3.4          | Baixado automaticamente via CMake FetchContent.  |
| GLM          | 1.1.0        | Baixado automaticamente via CMake FetchContent.  |
| stb_image    | master       | Baixado automaticamente via CMake FetchContent.  |
| GLAD         | OpenGL 4.6   | Incluido em `include/glad/` e `common/glad.c`   |
| CMake        | >= 3.10      | https://cmake.org/download/                     |
| MinGW-w64    | 13.2+        | https://www.mingw-w64.org/ (apenas Windows)     |

> No Windows, o compilador MinGW deve estar no PATH. Verifique com `g++ --version`.

### Compilação (Windows)

```bash
# 1. Entre na raiz do repositorio
cd AtividadesCG

# 2. Configure o CMake (baixa GLFW, GLM e stb_image automaticamente)
cmake -S . -B build -G "MinGW Makefiles"

# 3. Compile apenas o GrauB
cmake --build build --target GrauB
```

### Compilacao (Linux / macOS)

```bash
cmake -S . -B build
cmake --build build --target GrauB
```

### Execução

```bash
cd build
.\GrauB.exe  # Windows
./GrauB      # Linux / macOS
```

---

## Controles - GrauB

| Tecla          | Acao                                                        |
|----------------|-------------------------------------------------------------|
| `M`            | Alternar modo **câmera FPS** e modo **objeto**            |
| `WASD` + mouse | Mover e rotacionar câmera (modo câmera)                     |
| `Tab`          | Selecionar próximo objeto (modo objeto)                     |
| `R`            | Modo Rotacao  ->  `X`/`Y`/`Z` togglam eixo de rotacao      |
| `T`            | Modo Translacao  ->  Setas e  `I`/`K`               |
| `S`            | Modo Escala uniforme  ->  `=` aumenta,  `-` diminui         |
| `1`            | Toggle Key light  (luz principal)                           |
| `2`            | Toggle Fill light (luz de preenchimento)                    |
| `3`            | Toggle Back light (rim light azul-fria)                     |
| `4`            | Toggle textura UV <-> material puro (Ka/Kd/Ks)              |
| `Space`        | Pausar / retomar animações Bezier                           |
| `ESC`          | Fechar janela                                               |

---

## Funcionalidades Implementadas

| Requisito                          | Implementação no código                                      |
|------------------------------------|--------------------------------------------------------------|
| Múltiplos OBJ com MTL e textura    | `loadSimpleOBJ()` + `loadMTL()` - le Ka, Kd, Ks, Ns, map_Kd |
| Iluminacao Phong com 3 luzes       | Loop no Fragment Shader; teclas 1/2/3 habilitam cada luz     |
| Atenuacao quadratica na difusa     | `att = 1 / (c + l*d + q*d^2)`                               |
| Back light / rim light             | `rim = (1 - dot(N,V))^2.5 * lightColor * (1 - ambientScale)`|
| Camera FPS                         | Classe `Camera` - lookAt, yaw/pitch via mouse, WASD          |
| Seleção e transformação de objetos | Tab seleciona; R/T/S + eixos aplicam rotação/translação/escala|
| Animacao por curva Bezier cubico   | `evalBezier()` - Bernstein grau 3; Space pausa/retoma        |
| Arquivo de configuração de cena    | `assets/scene.txt` - câmera, frustum, luzes, objetos, curvas |

---

## Arquivo de Configuracao de Cena (assets/scene.txt)

Gerado automaticamente se ausente. Sintaxe:

```
# Comentarios com #
camera   px py pz  yaw pitch
frustum  fov near far
light    id  px py pz  cr cg cb  ambientScale
object   name objPath  px py pz  rx ry rz  scale  bezierSpeed
bezier   name  p0x p0y p0z  p1x p1y p1z  p2x p2y p2z  p3x p3y p3z
```

Parámetros de `light`:
- `id` = 0 (key), 1 (fill), 2 (back/rim)
- `ambientScale` = 1.0 luz normal, 0.0 rim-only (back light)

---

## Assets

### Modelos 3D incluidos

| Arquivo              | Origem                                                                 | Processamento      |
|----------------------|------------------------------------------------------------------------|--------------------|
| `Suzanne.obj`        | Modelo padrão "Suzanne" do Blender (CC0), arquivo `SuzanneAula.blend` fornecido pelo professor | Blender 4.3 - exportado com normais e coords UV |
| `SuzanneSubdiv1.obj` | Mesmo modelo com 1 nivel de Subdivision Surface                        | Blender 4.3        |
| `Cube.obj`           | Cubo primitivo padrão do Blender                                       | Blender 4.3        |
| `Suzanne.png`        | Textura UV gerada no Blender, incluida no material de aula             | -                  |

> Os modelos e a textura foram fornecidos pelo professor como material de aula.
> A malha Suzanne e um modelo de domínio público (CC0) nativo do Blender.

---

## Referencias

### Tutoriais e Documentação Técnica

- **LearnOpenGL** (Joey de Vries) - https://learnopengl.com/

- **GLM Documentation** - https://glm.g-truc.net/

- **GLFW Documentation** - https://www.glfw.org/docs/latest/

- **stb_image** (Sean Barrett) - https://github.com/nothings/stb

- **Anton's OpenGL 4 Tutorials** (Anton Gerdelan) - https://antongerdelan.net/opengl/

- **OBJ / MTL File Format** - https://paulbourke.net/dataformats/obj/

- **Bezier Curves** - https://en.wikipedia.org/wiki/B%C3%A9zier_curve

- **The OpenGL Programming Guide** ("Red Book"), 9a edicao

### Material de Aula

- Slides e exercicios da disciplina de Computação Gráfica.
- Arquivos de exemplo fornecidos pelo professor.
