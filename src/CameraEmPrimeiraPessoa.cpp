/* CameraEmPrimeiraPessoa - Tarefa - Adicionando uma câmera em primeira pessoa.
 *
 * Controles:
 *   W / S       - mover para frente / trás
 *   A / D       - mover para esquerda / direita
 *   Mouse       - rotacionar a câmera (yaw / pitch)
 *   ESC         - fechar janela
 *
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

// Enumeração de direções de movimento.
enum CameraDirection { FORWARD, BACKWARD, LEFT, RIGHT };

// Classe Camera.
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

    // Construtor com posição inicial, worldUp, yaw e pitch.
    Camera(vec3 startPosition        = vec3(0.0f, 0.0f,  3.0f),
           vec3 startWorldUp         = vec3(0.0f, 1.0f,  0.0f),
           float startYaw            = -90.0f,
           float startPitch          =   0.0f,
           float startMovementSpeed  =   5.0f,
           float startSensitivity    =   0.1f)
        : position(startPosition),
          worldUp(startWorldUp),
          yaw(startYaw),
          pitch(startPitch),
          movementSpeed(startMovementSpeed),
          mouseSensitivity(startSensitivity)
    {
        updateCameraVectors();
    }

    // Retorna a matriz view.
    mat4 getViewMatrix() const
    {
        return lookAt(position, position + front, up);
    }

    // Move a câmera na direção indicada.
    void processKeyboard(CameraDirection direction, float dt)
    {
        float velocity = movementSpeed * dt;
        switch (direction)
        {
            case FORWARD:  position += front * velocity; break;
            case BACKWARD: position -= front * velocity; break;
            case LEFT:     position -= right * velocity; break;
            case RIGHT:    position += right * velocity; break;
        }
    }

    // Rotação - Atualiza yaw e pitch a partir dos offsets do cursor do mouse.
    void processMouse(float xOffset, float yOffset, bool constrainPitch = true)
    {
        xOffset *= mouseSensitivity;
        yOffset *= mouseSensitivity;

        yaw   += xOffset;
        pitch += yOffset;

        if (constrainPitch)
        {
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }

        updateCameraVectors();
    }

private:
    // Reconstrói front, right e up a partir de yaw e pitch atuais.
    void updateCameraVectors()
    {
        vec3 newFront;
        newFront.x = cos(radians(yaw)) * cos(radians(pitch));
        newFront.y = sin(radians(pitch));
        newFront.z = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(newFront);

        right = normalize(cross(front, worldUp));
        up    = normalize(cross(right, front));
    }
};

// Estrutura de material (lida do .MTL).
struct Material
{
    vec3  Ka{0.2f, 0.2f, 0.2f};
    vec3  Kd{1.0f, 1.0f, 1.0f};
    vec3  Ks{0.5f, 0.5f, 0.5f};
    float Ns{32.0f};
    string textureName;
};

// Protótipos de funções.
void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void   mouse_callback(GLFWwindow* window, double xPos, double yPos);
int    setupShader();
GLuint loadSimpleOBJ(const string& filePath, int& nVertices, Material& mat);
GLuint loadTexture(const string& filePath);
Material loadMTL(const string& mtlPath);

// Constantes de janela.
const GLuint WIDTH = 1000, HEIGHT = 800;

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

out vec4 color;

void main()
{
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec4 texColor   = texture(texBuff, texCoord);

    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 R = normalize(reflect(-L, N));
    vec3 V = normalize(camPos - fragPos);

    vec3 ambient  = Ka * lightColor;

    float diff    = max(dot(N, L), 0.0);
    vec3 diffuse  = Kd * diff * lightColor;

    float spec    = pow(max(dot(R, V), 0.0), Ns);
    vec3 specular = Ks * spec * lightColor;

    vec3 result = (ambient + diffuse) * vec3(texColor) + specular;
    color = vec4(result, texColor.a);
}
)";

// Estado global da câmera e mouse.
Camera camera(vec3(0.0f, 0.0f, 5.0f));
float  lastX      = WIDTH  / 2.0f;
float  lastY      = HEIGHT / 2.0f;
bool   firstMouse = true;

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "Camera em Primeira Pessoa", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // Captura o cursor no modo FPS.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version: " << version << endl;

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    GLuint shaderID = setupShader();

    // Carrega o modelo OBJ e o material MTL.
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

    vec3 lightPos(3.0f, 4.0f, 3.0f);

    glUseProgram(shaderID);
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);
    glUniform3fv(glGetUniformLocation(shaderID, "Ka"),       1, value_ptr(mat.Ka));
    glUniform3fv(glGetUniformLocation(shaderID, "Kd"),       1, value_ptr(mat.Kd));
    glUniform3fv(glGetUniformLocation(shaderID, "Ks"),       1, value_ptr(mat.Ks));
    glUniform1f (glGetUniformLocation(shaderID, "Ns"),       mat.Ns);
    glUniform3fv(glGetUniformLocation(shaderID, "lightPos"), 1, value_ptr(lightPos));

    // Projeção perspectiva.
    mat4 projection = perspective(radians(45.0f),
                                  (float)WIDTH / (float)HEIGHT,
                                  0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"),
                       1, GL_FALSE, value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);

    // Posições das instâncias de Suzanne na cena.
    vector<vec3> objectPositions = {
        vec3( 0.0f,  0.0f,  0.0f),
        vec3( 3.0f,  0.0f, -2.0f),
        vec3(-3.0f,  0.5f, -4.0f),
        vec3( 1.5f, -1.0f, -6.0f),
        vec3(-1.5f,  1.0f, -8.0f),
    };

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        float deltaTime    = currentFrame - lastFrame;
        lastFrame          = currentFrame;

        glfwPollEvents();

        // Polling de teclado para movimento suave.
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.processKeyboard(FORWARD,  deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.processKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.processKeyboard(LEFT,     deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.processKeyboard(RIGHT,    deltaTime);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"),
                           1, GL_FALSE, value_ptr(view));
        glUniform3fv(glGetUniformLocation(shaderID, "camPos"),
                     1, value_ptr(camera.position));

        glBindTexture(GL_TEXTURE_2D, texID);
        glBindVertexArray(VAO);

        // Desenha todas as instâncias com model matrices distintas.
        float angle = currentFrame * 0.5f;
        for (size_t i = 0; i < objectPositions.size(); ++i)
        {
            mat4 model = mat4(1.0f);
            model = translate(model, objectPositions[i]);
            model = rotate(model, angle * (1.0f + 0.1f * (float)i),
                           vec3(0.0f, 1.0f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"),
                               1, GL_FALSE, value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, nVertices);
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

// Callback de teclado — apenas ESC.
void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mode*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

// Callback do mouse — delega para camera.processMouse().
void mouse_callback(GLFWwindow* /*window*/, double xPos, double yPos)
{
    if (firstMouse)
    {
        lastX      = (float)xPos;
        lastY      = (float)yPos;
        firstMouse = false;
    }

    float xOffset =  (float)xPos - lastX;
    float yOffset =  lastY - (float)yPos;
    lastX = (float)xPos;
    lastY = (float)yPos;

    camera.processMouse(xOffset, yOffset);
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

// Carrega arquivo .MTL e preenche a estrutura Material
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

    cout << "MTL carregado: Ka=(" << mat.Ka.r << "," << mat.Ka.g << "," << mat.Ka.b << ")"
         << " Kd=(" << mat.Kd.r << "," << mat.Kd.g << "," << mat.Kd.b << ")"
         << " Ks=(" << mat.Ks.r << "," << mat.Ks.g << "," << mat.Ks.b << ")"
         << " Ns=" << mat.Ns << " tex=" << mat.textureName << endl;

    return mat;
}

// Carrega um arquivo .OBJ simples (v / vt / vn / f) e cria VAO/VBO.
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

// Carrega uma textura com stb_image e retorna o ID OpenGL.
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
