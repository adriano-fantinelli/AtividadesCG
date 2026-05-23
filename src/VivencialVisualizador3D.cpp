/* VivencialVisualizador3D.cpp — Vivencial 2.
 *
 * Controles:
 *   X / Y / Z  — rotacionar em torno do eixo X / Y / Z.
 *   1          — habilitar/desabilitar luz principal.
 *   2          — habilitar/desabilitar luz de preenchimento.
 *   3          — habilitar/desabilitar luz de fundo.
 *   ESC        — fechar janela
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace glm;

// Estrutura de material.
struct Material
{
    vec3  Ka{0.2f, 0.2f, 0.2f};
    vec3  Kd{1.0f, 1.0f, 1.0f};
    vec3  Ks{0.5f, 0.5f, 0.5f};
    float Ns{32.0f};
    string textureName;
};

// Protótipos.
void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
int    setupShader();
GLuint loadSimpleOBJ(const string& filePath, int& nVertices, Material& mat,
                     vec3& outCenter, float& outScale);
GLuint loadTexture(const string& filePath);
Material loadMTL(const string& mtlPath);

// Constantes.
const GLuint WIDTH = 800, HEIGHT = 800;

const GLchar* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texc;
layout (location = 2) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 texCoord;
out vec3 vNormal;
out vec3 fragPos;

void main()
{
    vec4 worldPos = model * vec4(position, 1.0);
    gl_Position   = projection * view * worldPos;
    fragPos  = vec3(worldPos);
    texCoord = texc;
    vNormal  = mat3(transpose(inverse(model))) * normal;
}
)";

// Fragment shader — 3 luzes pontuais com atenuação quadrática na difusa.
const GLchar* fragmentShaderSource = R"(
#version 400
in vec2 texCoord;
in vec3 vNormal;
in vec3 fragPos;

uniform sampler2D texBuff;
uniform vec3  Ka;
uniform vec3  Kd;
uniform vec3  Ks;
uniform float Ns;
uniform vec3 camPos;

// 3 luzes pontuais.
uniform vec3  lightPos[3];     
uniform vec3  lightColor[3];
uniform bool  lightEnabled[3];
uniform float lightAmbientScale[3];

// Coeficientes de atenuação quadrática.
const float attC = 1.0;
const float attL = 0.09;
const float attQ = 0.032;

out vec4 color;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(camPos - fragPos);

    vec4 texColor = texture(texBuff, texCoord);

    vec3 result = vec3(0.0);

    for (int i = 0; i < 3; i++)
    {
        if (!lightEnabled[i])
            continue;

        vec3  L    = normalize(lightPos[i] - fragPos);
        vec3  R    = normalize(reflect(-L, N));
        float dist = length(lightPos[i] - fragPos);

        // Atenuação quadrática — aplicada SOMENTE à parcela difusa.
        float att  = 1.0 / (attC + attL * dist + attQ * dist * dist);

        // Parcela ambiente — escalada por lightAmbientScale.
        // (0.0 para rim light: evita iluminar o objeto todo como luz global)
        vec3 ambient  = Ka * lightColor[i] * lightAmbientScale[i];

        // Parcela difusa com atenuação.
        float diff    = max(dot(N, L), 0.0);
        vec3  diffuse = Kd * diff * lightColor[i] * att;

        // Parcela especular.
        float spec    = pow(max(dot(R, V), 0.0), Ns);
        vec3  specular = Ks * spec * lightColor[i];

        // Termo de rim light.
        float rimFactor = pow(clamp(1.0 - dot(N, V), 0.0, 1.0), 2.5);
        vec3  rim       = lightColor[i] * rimFactor * (1.0 - lightAmbientScale[i]);

        result += ambient + diffuse + specular + rim;
    }

    color = vec4(result * vec3(texColor), texColor.a);
}
)";

// Estado global de rotação e luzes.
bool rotateX = false, rotateY = false, rotateZ = false;
bool lightOn[3] = {true, true, true};

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "Visualizador 3D — Iluminacao de 3 Pontos", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    cout << "Renderer: "       << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL version: " << glGetString(GL_VERSION)  << endl;

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    GLuint shaderID = setupShader();

    // Carregamento do modelo.
    Material mat;
    int   nVertices = 0;
    vec3  objCenter;
    float objScale;

    GLuint VAO = loadSimpleOBJ("../assets/Modelos3D/SuzanneSubdiv1.obj",
                               nVertices, mat, objCenter, objScale);
    if (VAO == (GLuint)-1)
    {
        cerr << "Erro ao carregar OBJ." << endl;
        glfwTerminate();
        return -1;
    }

    string texPath = "../assets/Modelos3D/" + mat.textureName;
    GLuint texID   = loadTexture(texPath);

    float S = objScale * 1.5f;

    vec3 lightPos[3] = {
        objCenter + vec3(-1.5f * S,  1.5f * S,  2.0f * S),  // [0] key  — frente/esq/acima
        objCenter + vec3( 1.5f * S,  0.5f * S,  2.0f * S),  // [1] fill — frente/dir/leve-acima
        objCenter + vec3(-0.5f * S,  0.8f * S, -2.5f * S)   // [2] back — atrás/levemente esq/acima
    };

    // Intensidades calibradas:
    //   key  100% branca  — luz dominante.
    //   fill  50% branca  — suaviza sombras.
    //   back 160% azul-fria — rim light intensa.
    vec3 lightColor[3] = {
        vec3(1.0f, 1.0f, 1.0f), 
        vec3(0.5f, 0.5f, 0.5f),
        vec3(0.8f, 0.9f, 2.0f) 
    };

    float lightAmbientScale[3] = {1.0f, 1.0f, 0.0f};

    cout << "\n--- Posicoes das luzes (S = " << S << ") ---" << endl;
    cout << "[0] Key  light : (" << lightPos[0].x << ", " << lightPos[0].y << ", " << lightPos[0].z << ")" << endl;
    cout << "[1] Fill light : (" << lightPos[1].x << ", " << lightPos[1].y << ", " << lightPos[1].z << ")" << endl;
    cout << "[2] Back light : (" << lightPos[2].x << ", " << lightPos[2].y << ", " << lightPos[2].z << ")" << endl;
    cout << "\nControles: X/Y/Z = rotacao  |  1/2/3 = toggle key/fill/back  |  ESC = sair\n" << endl;

    // Câmera e projeção perspectiva.
    vec3 camPos(0.0f, 0.0f, 3.0f);
    mat4 view       = lookAt(camPos, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat4 projection = perspective(radians(45.0f),
                                  (float)WIDTH / (float)HEIGHT,
                                  0.1f, 100.0f);

    // Envio de uniforms estáticos.
    glUseProgram(shaderID);

    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);

    glUniform3fv(glGetUniformLocation(shaderID, "Ka"),  1, value_ptr(mat.Ka));
    glUniform3fv(glGetUniformLocation(shaderID, "Kd"),  1, value_ptr(mat.Kd));
    glUniform3fv(glGetUniformLocation(shaderID, "Ks"),  1, value_ptr(mat.Ks));
    glUniform1f (glGetUniformLocation(shaderID, "Ns"),  mat.Ns);

    glUniform3fv(glGetUniformLocation(shaderID, "camPos"),     1, value_ptr(camPos));
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"),       1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    // Posições, cores e escala ambiente das luzes.
    for (int i = 0; i < 3; i++)
    {
        string lp = "lightPos["          + to_string(i) + "]";
        string lc = "lightColor["        + to_string(i) + "]";
        string la = "lightAmbientScale[" + to_string(i) + "]";
        glUniform3fv(glGetUniformLocation(shaderID, lp.c_str()), 1, value_ptr(lightPos[i]));
        glUniform3fv(glGetUniformLocation(shaderID, lc.c_str()), 1, value_ptr(lightColor[i]));
        glUniform1f (glGetUniformLocation(shaderID, la.c_str()), lightAmbientScale[i]);
    }

    // Setup de renderização.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    glEnable(GL_DEPTH_TEST);

    // Loop principal.
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < 3; i++)
        {
            string le = "lightEnabled[" + to_string(i) + "]";
            glUniform1i(glGetUniformLocation(shaderID, le.c_str()), lightOn[i] ? 1 : 0);
        }

        float angle = (float)glfwGetTime();
        mat4 model = mat4(1.0f);
        if (rotateX)
            model = rotate(model, angle, vec3(1.0f, 0.0f, 0.0f));
        else if (rotateY)
            model = rotate(model, angle, vec3(0.0f, 1.0f, 0.0f));
        else if (rotateZ)
            model = rotate(model, angle, vec3(0.0f, 0.0f, 1.0f));

        glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, nVertices);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

// Callbacks e funções auxiliares.
void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mode*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    { rotateX = true;  rotateY = false; rotateZ = false; }
    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    { rotateX = false; rotateY = true;  rotateZ = false; }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    { rotateX = false; rotateY = false; rotateZ = true;  }

    if (key == GLFW_KEY_1 && action == GLFW_PRESS)
    {
        lightOn[0] = !lightOn[0];
        cout << "Key light  (1): " << (lightOn[0] ? "ON" : "OFF") << endl;
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS)
    {
        lightOn[1] = !lightOn[1];
        cout << "Fill light (2): " << (lightOn[1] ? "ON" : "OFF") << endl;
    }
    if (key == GLFW_KEY_3 && action == GLFW_PRESS)
    {
        lightOn[2] = !lightOn[2];
        cout << "Back light (3): " << (lightOn[2] ? "ON" : "OFF") << endl;
    }
}

int setupShader()
{
    GLint  success;
    GLchar infoLog[512];

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        cerr << "VERTEX SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        cerr << "FRAGMENT SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(prog, 512, nullptr, infoLog);
        cerr << "SHADER PROGRAM LINK ERROR:\n" << infoLog << endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

Material loadMTL(const string& mtlPath)
{
    Material mat;

    ifstream arq(mtlPath);
    if (!arq.is_open())
    {
        cerr << "MTL nao encontrado: " << mtlPath << endl;
        return mat;
    }

    string line;
    while (getline(arq, line))
    {
        istringstream ss(line);
        string token;
        ss >> token;

        if      (token == "Ka") ss >> mat.Ka.r >> mat.Ka.g >> mat.Ka.b;
        else if (token == "Kd") ss >> mat.Kd.r >> mat.Kd.g >> mat.Kd.b;
        else if (token == "Ks") ss >> mat.Ks.r >> mat.Ks.g >> mat.Ks.b;
        else if (token == "Ns") ss >> mat.Ns;
        else if (token == "map_Kd") ss >> mat.textureName;
    }

    cout << "MTL carregado:"
         << " Ka=(" << mat.Ka.r << "," << mat.Ka.g << "," << mat.Ka.b << ")"
         << " Kd=(" << mat.Kd.r << "," << mat.Kd.g << "," << mat.Kd.b << ")"
         << " Ks=(" << mat.Ks.r << "," << mat.Ks.g << "," << mat.Ks.b << ")"
         << " Ns=" << mat.Ns
         << " tex=" << mat.textureName << endl;

    return mat;
}

GLuint loadSimpleOBJ(const string& filePath, int& nVertices, Material& mat,
                     vec3& outCenter, float& outScale)
{
    vector<vec3>    positions;
    vector<vec2>    texCoords;
    vector<vec3>    normals;
    vector<GLfloat> vBuffer;

    string baseDir;
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != string::npos)
        baseDir = filePath.substr(0, lastSlash + 1);

    ifstream arq(filePath);
    if (!arq.is_open())
    {
        cerr << "Erro ao abrir: " << filePath << endl;
        return (GLuint)-1;
    }

    string line;
    while (getline(arq, line))
    {
        istringstream ssline(line);
        string token;
        ssline >> token;

        if (token == "mtllib")
        {
            string mtlFile;
            ssline >> mtlFile;
            mat = loadMTL(baseDir + mtlFile);
        }
        else if (token == "v")
        {
            vec3 pos;
            ssline >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (token == "vt")
        {
            vec2 tc;
            ssline >> tc.s >> tc.t;
            texCoords.push_back(tc);
        }
        else if (token == "vn")
        {
            vec3 n;
            ssline >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (token == "f")
        {
            string faceToken;
            while (ssline >> faceToken)
            {
                int vi = 0, ti = 0, ni = 0;
                istringstream ss(faceToken);
                string idx;

                if (getline(ss, idx, '/')) vi = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(ss, idx, '/')) ti = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(ss, idx))      ni = !idx.empty() ? stoi(idx) - 1 : 0;

                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);

                if (!texCoords.empty())
                {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                }

                if (!normals.empty())
                {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(1.0f);
                }
            }
        }
    }
    arq.close();

    // Calcula bounding box para derivar centro e escala do objeto.
    if (!positions.empty())
    {
        vec3 bmin = positions[0], bmax = positions[0];
        for (const auto& p : positions)
        {
            bmin = glm::min(bmin, p);
            bmax = glm::max(bmax, p);
        }
        outCenter = (bmin + bmax) * 0.5f;
        vec3 extent = bmax - bmin;
        outScale = glm::max(glm::max(extent.x, extent.y), extent.z);
        cout << "Bounding box: min=(" << bmin.x << "," << bmin.y << "," << bmin.z << ")"
             << "  max=(" << bmax.x << "," << bmax.y << "," << bmax.z << ")" << endl;
        cout << "Centro: (" << outCenter.x << "," << outCenter.y << "," << outCenter.z << ")"
             << "  Escala max: " << outScale << endl;
    }
    else
    {
        outCenter = vec3(0.0f);
        outScale  = 1.0f;
    }

    cout << "OBJ carregado. Gerando VAO..." << endl;

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vBuffer.size() * sizeof(GLfloat),
                 vBuffer.data(),
                 GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    const GLsizei stride = 8 * sizeof(GLfloat);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 8);
    cout << "Vertices: " << nVertices << endl;

    return VAO;
}

GLuint loadTexture(const string& filePath)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nChannels, 0);

    if (data)
    {
        GLenum fmt = (nChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        cout << "Textura carregada: " << filePath
             << " (" << width << "x" << height << ", " << nChannels << "ch)" << endl;
    }
    else
    {
        cerr << "Falha ao carregar textura: " << filePath << endl;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}
