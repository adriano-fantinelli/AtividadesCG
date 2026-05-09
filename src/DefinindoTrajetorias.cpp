/* DefinindoTrajetorias - Tarefa - Definindo trajetórias para alguns objetos.
 *
 * Controles:
 *   W / S / A / D  - mover camera.
 *   Mouse          - rotacionar camera.
 *   Tab            - selecionar próximo objeto.
 *   P              - adicionar posição da camera como waypoint do objeto selecionado.
 *   C              - limpar waypoints do objeto selecionado.
 *   Espaço         - pausar / retomar animações.
 *   F              - salvar waypoints em "trajetorias.txt".
 *   ESC            - fechar janela.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace glm;

// Câmera em primeira pessoa.
enum CameraDirection { FORWARD, BACKWARD, LEFT, RIGHT };

class Camera
{
public:
    vec3  position;
    vec3  front;
    vec3  up;
    vec3  right;
    vec3  worldUp;
    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;

    Camera(vec3  pos   = vec3(0.0f, 2.0f, 12.0f),
           vec3  wUp   = vec3(0.0f, 1.0f,  0.0f),
           float y     = -90.0f,
           float p     =   0.0f,
           float speed =   8.0f,
           float sens  =   0.1f)
        : position(pos), worldUp(wUp), yaw(y), pitch(p),
          movementSpeed(speed), mouseSensitivity(sens)
    {
        updateCameraVectors();
    }

    mat4 getViewMatrix() const
    {
        return lookAt(position, position + front, up);
    }

    void processKeyboard(CameraDirection dir, float dt)
    {
        float v = movementSpeed * dt;
        if (dir == FORWARD)  position += front * v;
        if (dir == BACKWARD) position -= front * v;
        if (dir == LEFT)     position -= right * v;
        if (dir == RIGHT)    position += right * v;
    }

    void processMouse(float xOff, float yOff, bool constrainPitch = true)
    {
        xOff *= mouseSensitivity;
        yOff *= mouseSensitivity;
        yaw   += xOff;
        pitch += yOff;
        if (constrainPitch)
        {
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }
        updateCameraVectors();
    }

private:
    void updateCameraVectors()
    {
        vec3 f;
        f.x   = cos(radians(yaw)) * cos(radians(pitch));
        f.y   = sin(radians(pitch));
        f.z   = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(f);
        right = normalize(cross(front, worldUp));
        up    = normalize(cross(right, front));
    }
};

// Material (lido do .MTL).
struct Material
{
    vec3  Ka{0.2f, 0.2f, 0.2f};
    vec3  Kd{1.0f, 1.0f, 1.0f};
    vec3  Ks{0.5f, 0.5f, 0.5f};
    float Ns{32.0f};
    string textureName;
};

// Objeto de cena com trajetória própria.
struct SceneObject
{
    vec3         basePosition;
    float        scale;
    vector<vec3> waypoints;
    float        trajT;
    float        speed;
};

// Interpolação linear cíclica entre waypoints.
vec3 getTrajectoryPosition(const SceneObject& obj)
{
    if (obj.waypoints.size() < 2)
        return obj.basePosition;

    int   N     = (int)obj.waypoints.size();
    float t     = obj.trajT;
    int   seg   = (int)floor(t) % N;
    float alpha = t - floor(t);

    vec3 a = obj.waypoints[seg];
    vec3 b = obj.waypoints[(seg + 1) % N];
    return mix(a, b, alpha);
}

void     key_callback(GLFWwindow* w, int key, int sc, int action, int mode);
void     mouse_callback(GLFWwindow* w, double xPos, double yPos);
int      setupShader();
GLuint   loadSimpleOBJ(const string& path, int& nVerts, Material& mat);
GLuint   loadTexture(const string& path);
Material loadMTL(const string& path);
void     saveWaypoints(const vector<SceneObject>& objs, const string& file);
void     loadWaypoints(vector<SceneObject>& objs, const string& file);

// Dimensoes da janela.
const GLuint WIDTH = 1200, HEIGHT = 800;

// Shaders.
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
uniform vec3  lightPos;
uniform vec3  camPos;
uniform float selectedBoost;

out vec4 color;

void main()
{
    vec3 lightColor = vec3(1.0);
    vec4 texColor   = texture(texBuff, texCoord);

    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 R = normalize(reflect(-L, N));
    vec3 V = normalize(camPos - fragPos);

    vec3 ambient  = Ka * lightColor * (1.0 + selectedBoost);
    float diff    = max(dot(N, L), 0.0);
    vec3 diffuse  = Kd * diff * lightColor;
    float spec    = pow(max(dot(R, V), 0.0), Ns);
    vec3 specular = Ks * spec * lightColor;

    vec3 result = (ambient + diffuse) * vec3(texColor) + specular;
    color = vec4(result, texColor.a);
}
)";

// Estado global.
Camera              camera;
float               lastX      = WIDTH  / 2.0f;
float               lastY      = HEIGHT / 2.0f;
bool                firstMouse = true;
vector<SceneObject> objects;
int                 selectedIdx = 0;
bool                paused      = false;

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "Definindo Trajetorias", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    cout << "Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL:   " << glGetString(GL_VERSION)  << endl;

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shaderID = setupShader();

    Material mat;
    int nVertices = 0;
    GLuint VAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, mat);
    if (VAO == (GLuint)-1)
    {
        cerr << "Erro ao carregar OBJ." << endl;
        glfwTerminate();
        return -1;
    }

    string texPath = "../assets/Modelos3D/" + mat.textureName;
    GLuint texID = loadTexture(texPath);

    // Três objetos com trajetórias pre-definidas.
    objects.resize(3);

    // Objeto 0.
    objects[0].basePosition = vec3( 0.0f, 0.0f, 0.0f);
    objects[0].scale        = 0.8f;
    objects[0].trajT        = 0.0f;
    objects[0].speed        = 0.4f;
    objects[0].waypoints    = {
        vec3( 3.0f, 0.0f,  3.0f),
        vec3( 3.0f, 0.0f, -3.0f),
        vec3(-3.0f, 0.0f, -3.0f),
        vec3(-3.0f, 0.0f,  3.0f),
    };

    // Objeto 1.
    objects[1].basePosition = vec3( 5.0f, 0.0f, 0.0f);
    objects[1].scale        = 0.7f;
    objects[1].trajT        = 0.5f;
    objects[1].speed        = 0.5f;
    objects[1].waypoints    = {
        vec3( 6.0f,  1.5f,  0.0f),
        vec3( 8.0f,  0.0f,  0.0f),
        vec3( 6.0f, -1.5f,  0.0f),
        vec3( 4.0f,  0.0f,  0.0f),
    };

    // Objeto 2.
    objects[2].basePosition = vec3(-5.0f, 0.0f, 0.0f);
    objects[2].scale        = 0.9f;
    objects[2].trajT        = 0.0f;
    objects[2].speed        = 0.35f;
    objects[2].waypoints    = {
        vec3(-3.0f, 2.0f,  0.0f),
        vec3(-7.0f, 2.0f,  3.0f),
        vec3(-7.0f, 2.0f, -3.0f),
    };

    // Tenta carregar waypoints salvos anteriormente e sobrescreve os pré-definidos.
    loadWaypoints(objects, "trajetorias.txt");

    cout << "\n=== Controles ===" << endl;
    cout << "W/A/S/D + Mouse : camera FPS"                              << endl;
    cout << "Tab             : selecionar proximo objeto [atual: "
         << selectedIdx << "]"                                           << endl;
    cout << "P               : adicionar posicao da camera como waypoint" << endl;
    cout << "C               : limpar waypoints do objeto selecionado"   << endl;
    cout << "Espaco          : pausar / retomar animacoes"               << endl;
    cout << "F               : salvar em trajetorias.txt"               << endl;
    cout << "ESC             : sair\n"                                  << endl;

    vec3 lightPos(4.0f, 8.0f, 4.0f);

    glUseProgram(shaderID);
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);
    glUniform3fv(glGetUniformLocation(shaderID, "Ka"),       1, value_ptr(mat.Ka));
    glUniform3fv(glGetUniformLocation(shaderID, "Kd"),       1, value_ptr(mat.Kd));
    glUniform3fv(glGetUniformLocation(shaderID, "Ks"),       1, value_ptr(mat.Ks));
    glUniform1f (glGetUniformLocation(shaderID, "Ns"),       mat.Ns);
    glUniform3fv(glGetUniformLocation(shaderID, "lightPos"), 1, value_ptr(lightPos));

    mat4 projection = perspective(radians(45.0f),
                                  (float)WIDTH / (float)HEIGHT,
                                  0.1f, 200.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"),
                       1, GL_FALSE, value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    glEnable(GL_DEPTH_TEST);

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        float deltaTime    = currentFrame - lastFrame;
        lastFrame          = currentFrame;

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.processKeyboard(FORWARD,  deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.processKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.processKeyboard(LEFT,     deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.processKeyboard(RIGHT,    deltaTime);

        if (!paused)
        {
            for (auto& obj : objects)
            {
                if (obj.waypoints.size() >= 2)
                    obj.trajT += obj.speed * deltaTime;
            }
        }

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"),
                           1, GL_FALSE, value_ptr(view));
        glUniform3fv(glGetUniformLocation(shaderID, "camPos"),
                     1, value_ptr(camera.position));

        glBindVertexArray(VAO);
        for (int i = 0; i < (int)objects.size(); ++i)
        {
            vec3 pos = getTrajectoryPosition(objects[i]);

            mat4 model = mat4(1.0f);
            model = translate(model, pos);
            model = glm::scale(model, vec3(objects[i].scale));

            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"),
                               1, GL_FALSE, value_ptr(model));

            // Objeto selecionado recebe destaque de ambiente.
            float boost = (i == selectedIdx) ? 1.2f : 0.0f;
            glUniform1f(glGetUniformLocation(shaderID, "selectedBoost"), boost);

            glDrawArrays(GL_TRIANGLES, 0, nVertices);
        }
        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

// Callback de teclado.
void key_callback(GLFWwindow* window, int key, int /*sc*/, int action, int /*mode*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // Seleciona próximo objeto.
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
    {
        selectedIdx = (selectedIdx + 1) % (int)objects.size();
        cout << "Objeto selecionado: " << selectedIdx
             << " | Waypoints: " << objects[selectedIdx].waypoints.size() << endl;
    }

    // Adiciona posição atual da câmera como waypoint.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        objects[selectedIdx].waypoints.push_back(camera.position);
        cout << "Waypoint adicionado ao objeto " << selectedIdx
             << ": (" << camera.position.x << ", "
                      << camera.position.y << ", "
                      << camera.position.z << ")"
             << " | total: " << objects[selectedIdx].waypoints.size() << endl;
    }

    // Limpa waypoints do objeto selecionado.
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        objects[selectedIdx].waypoints.clear();
        objects[selectedIdx].trajT = 0.0f;
        cout << "Waypoints do objeto " << selectedIdx << " limpos." << endl;
    }

    // Pausa ou retoma a animação.
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        paused = !paused;
        cout << (paused ? "Animacao pausada." : "Animacao retomada.") << endl;
    }

    // Salva em arquivo.
    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        saveWaypoints(objects, "trajetorias.txt");
        cout << "Waypoints salvos em trajetorias.txt" << endl;
    }
}

// Callback do mouse.
void mouse_callback(GLFWwindow* /*window*/, double xPos, double yPos)
{
    if (firstMouse)
    {
        lastX = (float)xPos;
        lastY = (float)yPos;
        firstMouse = false;
    }
    float xOff =  (float)xPos - lastX;
    float yOff =  lastY - (float)yPos;
    lastX = (float)xPos;
    lastY = (float)yPos;
    camera.processMouse(xOff, yOff);
}

// Salva waypoints de todos os objetos em arquivo de texto.
void saveWaypoints(const vector<SceneObject>& objs, const string& filename)
{
    ofstream f(filename);
    if (!f.is_open())
    {
        cerr << "Erro ao abrir " << filename << " para escrita." << endl;
        return;
    }
    f << "# DefinindoTrajetorias - waypoints\n";
    f << "# Formato: OBJECT <id>  seguido de linhas  x y z\n\n";
    for (int i = 0; i < (int)objs.size(); ++i)
    {
        f << "OBJECT " << i << "\n";
        for (const auto& p : objs[i].waypoints)
            f << p.x << " " << p.y << " " << p.z << "\n";
        f << "\n";
    }
}

// Carrega waypoints de arquivo e sobrescreve apenas os objetos mencionados.
void loadWaypoints(vector<SceneObject>& objs, const string& filename)
{
    ifstream f(filename);
    if (!f.is_open()) return;

    int    currentObj = -1;
    string line;
    while (getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;

        istringstream ss(line);
        string token;
        ss >> token;

        if (token == "OBJECT")
        {
            ss >> currentObj;
            if (currentObj >= 0 && currentObj < (int)objs.size())
                objs[currentObj].waypoints.clear();
            else
                currentObj = -1;
        }
        else if (currentObj >= 0)
        {
            float x, y, z;
            istringstream ss2(line);
            if (ss2 >> x >> y >> z)
                objs[currentObj].waypoints.push_back(vec3(x, y, z));
        }
    }
    cout << "Waypoints carregados de " << filename << endl;
}

// Compila e linka os shaders.
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

// Carrega arquivo .MTL.
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
        if      (token == "Ka")     ss >> mat.Ka.r >> mat.Ka.g >> mat.Ka.b;
        else if (token == "Kd")     ss >> mat.Kd.r >> mat.Kd.g >> mat.Kd.b;
        else if (token == "Ks")     ss >> mat.Ks.r >> mat.Ks.g >> mat.Ks.b;
        else if (token == "Ns")     ss >> mat.Ns;
        else if (token == "map_Kd") ss >> mat.textureName;
    }
    return mat;
}

// Carrega OBJ simples (v / vt / vn / f) e cria VAO/VBO.
GLuint loadSimpleOBJ(const string& filePath, int& nVertices, Material& mat)
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
            string mtlFile; ssline >> mtlFile;
            mat = loadMTL(baseDir + mtlFile);
        }
        else if (token == "v")
        {
            vec3 pos; ssline >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (token == "vt")
        {
            vec2 tc; ssline >> tc.s >> tc.t;
            texCoords.push_back(tc);
        }
        else if (token == "vn")
        {
            vec3 n; ssline >> n.x >> n.y >> n.z;
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
                if (getline(ss, idx, '/')) vi = !idx.empty() ? stoi(idx)-1 : 0;
                if (getline(ss, idx, '/')) ti = !idx.empty() ? stoi(idx)-1 : 0;
                if (getline(ss, idx))      ni = !idx.empty() ? stoi(idx)-1 : 0;

                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);
                if (!texCoords.empty()) {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                } else {
                    vBuffer.push_back(0.0f); vBuffer.push_back(0.0f);
                }
                if (!normals.empty()) {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                } else {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(1.0f);
                }
            }
        }
    }
    arq.close();

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vBuffer.size() * sizeof(GLfloat),
                 vBuffer.data(), GL_STATIC_DRAW);

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
    cout << "OBJ carregado: " << nVertices << " vertices." << endl;
    return VAO;
}

// Carrega textura com stb_image.
GLuint loadTexture(const string& filePath)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
        cout << "Textura: " << filePath << " (" << width << "x" << height << ")" << endl;
    }
    else cerr << "Falha ao carregar textura: " << filePath << endl;

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}
